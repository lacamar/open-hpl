//! `iRenderableContainer` update passes, the BoxTree object removal and the whole of
//! `cRenderableContainer_DynBoxTree` (node split, garbage collection, object move, rebuild).
//!
//! Node and object memory stay C++. A node's scalar state is a [`NodeBase`]/[`NodeDyn`] the C++
//! node embeds and this code edits in place; lists, node creation/deletion and renderable
//! virtuals are reached through [`Host`]. Node/object handles are opaque addresses (0 = none).

use super::{at_edge, longest_axis, split_group};
use crate::math::{len3, sub3, v3};
use crate::types::Vector3f;

pub type Handle = usize;

/// `eCollision_Outside`, the value a fresh node starts with.
pub const COLLISION_OUTSIDE: i32 = 1;

const BIG: f32 = 100_000.0;

/// `iRenderableContainerNode` scalar state. `parent` is the `mpParent` pointer.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct NodeBase {
    pub min: Vector3f,
    pub max: Vector3f,
    pub center: Vector3f,
    pub radius: f32,
    pub render_flags: u32,
    pub visible_objects: bool,
    pub need_property_update: bool,
    pub need_aabb_update: bool,
    pub uses_flags_and_visibility: bool,
    pub view_distance: f32,
    pub inside_view: bool,
    pub prev_collision: i32,
    pub parent: Handle,
}

impl NodeBase {
    /// The `iRenderableContainerNode` constructor; the C++ left flags, visibility and view state
    /// uninitialised (recorded in bd), here they start zeroed.
    pub fn new(uses_flags_and_visibility: bool) -> Self {
        Self {
            min: v3(0.0, 0.0, 0.0),
            max: v3(0.0, 0.0, 0.0),
            center: v3(0.0, 0.0, 0.0),
            radius: 0.0,
            render_flags: 0,
            visible_objects: false,
            need_property_update: true,
            need_aabb_update: false,
            uses_flags_and_visibility,
            view_distance: 0.0,
            inside_view: false,
            prev_collision: COLLISION_OUTSIDE,
            parent: 0,
        }
    }
}

/// `cRCNode_DynBoxTree` scalar state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct NodeDyn {
    pub split_axis: i32,
    pub split_plane: f32,
    pub recalc_split: bool,
    pub recalc_split_axis: bool,
    pub recalc_aabb: bool,
    pub object_moved: bool,
    pub is_split: bool,
    pub ignore_split_count: i32,
    pub gc_count: i32,
}

impl NodeDyn {
    pub fn new() -> Self {
        Self {
            split_axis: -1,
            split_plane: 0.0,
            recalc_split: false,
            recalc_split_axis: true,
            recalc_aabb: false,
            object_moved: false,
            is_split: false,
            ignore_split_count: 0,
            gc_count: 0,
        }
    }
}

impl Default for NodeDyn {
    fn default() -> Self {
        Self::new()
    }
}

/// `cRenderableContainer_DynBoxTree` tuning and counters.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DynTree {
    pub split_threshold: i32,
    pub max_intersection_amount: f32,
    pub max_ignore_split_count: i32,
    pub max_garbage_collect_count: i32,
    pub rebuild_count: i32,
    pub max_rebuild_count: i32,
}

impl DynTree {
    pub fn new() -> Self {
        Self {
            split_threshold: 30,
            max_intersection_amount: 0.2,
            max_ignore_split_count: 30,
            max_garbage_collect_count: 60,
            rebuild_count: 600,
            max_rebuild_count: 600,
        }
    }
}

impl Default for DynTree {
    fn default() -> Self {
        Self::new()
    }
}

/// `cBoundingVolume` values the container code reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Bv {
    pub min: Vector3f,
    pub max: Vector3f,
    pub center: Vector3f,
    pub position: Vector3f,
    pub radius: f32,
}

/// What the container code needs from the C++ nodes and renderables.
pub trait Host {
    fn base(&mut self, node: Handle) -> &mut NodeBase;
    fn dyn_state(&mut self, node: Handle) -> &mut NodeDyn;
    fn child_count(&mut self, node: Handle) -> usize;
    /// Replaces `out` with the node's children in list order.
    fn children(&mut self, node: Handle, out: &mut Vec<Handle>);
    fn object_count(&mut self, node: Handle) -> usize;
    /// Replaces `out` with the node's objects in list order.
    fn objects(&mut self, node: Handle, out: &mut Vec<Handle>);
    fn push_object(&mut self, node: Handle, obj: Handle);
    fn clear_objects(&mut self, node: Handle);
    /// `STLFindAndRemove`: first occurrence.
    fn remove_first_object(&mut self, node: Handle, obj: Handle);
    /// `std::list::remove`: every occurrence.
    fn remove_all_objects(&mut self, node: Handle, obj: Handle);
    /// Appends a new, default-constructed child to `parent`'s list and returns it.
    fn new_child(&mut self, parent: Handle) -> Handle;
    /// `STLDeleteAll(children)`.
    fn delete_children(&mut self, node: Handle);
    /// `STLFindAndDelete(parent children, child)`: unlinks and deletes `child`.
    fn delete_child(&mut self, parent: Handle, child: Handle);
    /// `children.clear()` without deleting.
    fn clear_children(&mut self, node: Handle);
    fn push_child(&mut self, node: Handle, child: Handle);
    fn delete_node(&mut self, node: Handle);
    fn object_node(&mut self, obj: Handle) -> Handle;
    fn set_object_node(&mut self, obj: Handle, node: Handle);
    fn object_bv(&mut self, obj: Handle) -> Bv;
    fn object_visible(&mut self, obj: Handle) -> bool;
    fn object_render_flags(&mut self, obj: Handle) -> u32;
}

fn center_radius(b: &mut NodeBase) {
    b.center = v3(
        (b.max.x + b.min.x) * 0.5,
        (b.max.y + b.min.y) * 0.5,
        (b.max.z + b.min.z) * 0.5,
    );
    b.radius = len3(sub3(b.max, b.min)) * 0.5;
}

fn axis_value(v: Vector3f, axis: i32) -> f32 {
    match axis {
        0 => v.x,
        1 => v.y,
        2 => v.z,
        _ => 0.0,
    }
}

fn expand(min: &mut Vector3f, max: &mut Vector3f, add_min: Vector3f, add_max: Vector3f) {
    crate::math::expand_aabb(min, max, add_min, add_max);
}

fn aabb_inside(bv: &Bv, b: &NodeBase) -> bool {
    crate::math::aabb_inside(bv.min, bv.max, b.min, b.max)
}

/// `iRenderableContainerNode::PushUpNeedAABBUpdate`.
pub fn push_up_need_aabb_update<H: Host>(h: &mut H, mut node: Handle) {
    while node != 0 {
        let b = h.base(node);
        if b.need_aabb_update {
            return;
        }
        b.need_aabb_update = true;
        node = b.parent;
    }
}

/// `PushUpNeedPropertyUpdate` from an object whose visibility or flags changed.
pub fn object_properties_changed<H: Host>(h: &mut H, obj: Handle) {
    let mut node = h.object_node(obj);
    while node != 0 {
        let b = h.base(node);
        b.need_property_update = true;
        node = b.parent;
    }
}

/// `iRenderableContainerNode::CalculateMinMaxFromObjects`.
pub fn calculate_min_max_from_objects<H: Host>(h: &mut H, node: Handle) {
    let mut objs = Vec::new();
    h.objects(node, &mut objs);
    let mut min = v3(BIG, BIG, BIG);
    let mut max = v3(-BIG, -BIG, -BIG);
    for o in objs {
        let bv = h.object_bv(o);
        if min.x > bv.min.x {
            min.x = bv.min.x;
        }
        if min.y > bv.min.y {
            min.y = bv.min.y;
        }
        if min.z > bv.min.z {
            min.z = bv.min.z;
        }
        if max.x < bv.max.x {
            max.x = bv.max.x;
        }
        if max.y < bv.max.y {
            max.y = bv.max.y;
        }
        if max.z < bv.max.z {
            max.z = bv.max.z;
        }
    }
    let b = h.base(node);
    b.min = min;
    b.max = max;
}

fn check_property_update<H: Host>(h: &mut H, node: Handle) {
    {
        let b = h.base(node);
        b.render_flags = 0;
        b.visible_objects = false;
        b.need_property_update = false;
    }
    let mut kids = Vec::new();
    h.children(node, &mut kids);
    for c in kids {
        if h.base(c).need_property_update {
            check_property_update(h, c);
        }
        let (vis, flags) = {
            let cb = h.base(c);
            (cb.visible_objects, cb.render_flags)
        };
        let b = h.base(node);
        if vis {
            b.visible_objects = true;
        }
        b.render_flags |= flags;
    }
    let mut objs = Vec::new();
    h.objects(node, &mut objs);
    for o in objs {
        let vis = h.object_visible(o);
        let flags = h.object_render_flags(o);
        let b = h.base(node);
        if vis {
            b.visible_objects = true;
        }
        b.render_flags |= flags;
    }
}

fn check_aabb_update<H: Host>(h: &mut H, node: Handle) {
    h.base(node).need_aabb_update = false;
    if h.object_count(node) > 0 {
        calculate_min_max_from_objects(h, node);
    } else {
        let b = h.base(node);
        b.min = v3(BIG, BIG, BIG);
        b.max = v3(-BIG, -BIG, -BIG);
    }
    let mut kids = Vec::new();
    h.children(node, &mut kids);
    for c in kids {
        if h.base(c).need_aabb_update {
            check_aabb_update(h, c);
        }
        let (cmin, cmax) = {
            let cb = h.base(c);
            (cb.min, cb.max)
        };
        let b = h.base(node);
        expand(&mut b.min, &mut b.max, cmin, cmax);
    }
    center_radius(h.base(node));
}

/// `iRenderableContainer::UpdateBeforeRendering` up to the container specific part.
pub fn update_before_rendering<H: Host>(h: &mut H, root: Handle) {
    if h.base(root).need_property_update {
        check_property_update(h, root);
    }
    if h.base(root).need_aabb_update {
        check_aabb_update(h, root);
    }
}

fn remove_from_node<H: Host>(h: &mut H, node: Handle, obj: Handle) {
    h.remove_all_objects(node, obj);
    let mut kids = Vec::new();
    h.children(node, &mut kids);
    for c in kids {
        remove_from_node(h, c, obj);
    }
}

/// `cRenderableContainer_BoxTree::Remove` below the root. The C++ never marks the AABB or
/// properties dirty after a removal (kept).
pub fn boxtree_remove<H: Host>(h: &mut H, root: Handle, obj: Handle) {
    remove_from_node(h, root, obj);
}

fn remove_set_from_node<H: Host>(h: &mut H, node: Handle, set: &[Handle]) {
    let mut objs = Vec::new();
    h.objects(node, &mut objs);
    for o in objs {
        if set.binary_search(&o).is_ok() {
            h.remove_all_objects(node, o);
        }
    }
    let mut kids = Vec::new();
    h.children(node, &mut kids);
    for c in kids {
        remove_set_from_node(h, c, set);
    }
}

pub fn boxtree_remove_set<H: Host>(h: &mut H, root: Handle, set: &[Handle]) {
    let mut sorted = set.to_vec();
    sorted.sort_unstable();
    remove_set_from_node(h, root, &sorted);
}

fn object_moved<H: Host>(h: &mut H, mut node: Handle) {
    while node != 0 {
        if h.dyn_state(node).object_moved {
            return;
        }
        h.dyn_state(node).object_moved = true;
        node = h.base(node).parent;
    }
}

fn set_group_bounds<H: Host>(h: &mut H, node: Handle) {
    calculate_min_max_from_objects(h, node);
    center_radius(h.base(node));
}

/// `cRCNode_DynBoxTree::UpdateBeforeUse`.
pub fn update_before_use<H: Host>(h: &mut H, t: &DynTree, node: Handle) {
    let mut kids: Vec<Handle> = Vec::new();

    if h.dyn_state(node).object_moved {
        if h.dyn_state(node).gc_count <= 0 {
            let d = h.dyn_state(node);
            d.gc_count = t.max_garbage_collect_count;
            d.recalc_aabb = true;

            if h.child_count(node) > 0 {
                h.children(node, &mut kids);
                let (c1, c2) = (
                    kids.first().copied().unwrap_or(0),
                    kids.last().copied().unwrap_or(0),
                );
                let limit = t.split_threshold;
                let total = h
                    .object_count(c1)
                    .wrapping_add(h.object_count(c2))
                    .wrapping_add(h.object_count(node)) as i32;
                if h.child_count(c1) == 0 && h.child_count(c2) == 0 && total < limit {
                    let mut objs = Vec::new();
                    for &c in &kids {
                        h.objects(c, &mut objs);
                        for &o in &objs {
                            h.set_object_node(o, node);
                            h.push_object(node, o);
                        }
                    }
                    h.delete_children(node);
                    let d = h.dyn_state(node);
                    d.is_split = false;
                    d.recalc_split_axis = true;
                }
            }

            if h.object_count(node) as i32 >= t.split_threshold {
                h.dyn_state(node).recalc_split_axis = true;
            }
        } else {
            h.dyn_state(node).gc_count -= 1;
        }
    }

    if h.dyn_state(node).recalc_aabb {
        h.dyn_state(node).recalc_aabb = false;
        if h.object_count(node) > 0 {
            calculate_min_max_from_objects(h, node);
        }
        if h.child_count(node) > 0 {
            h.children(node, &mut kids);
            let a = kids.first().copied().unwrap_or(0);
            let b = kids.last().copied().unwrap_or(0);
            for c in [a, b] {
                let (cmin, cmax) = {
                    let cb = h.base(c);
                    (cb.min, cb.max)
                };
                let nb = h.base(node);
                expand(&mut nb.min, &mut nb.max, cmin, cmax);
            }
        }
        center_radius(h.base(node));
    }

    let object_num = h.object_count(node) as i32;
    let has_children = h.child_count(node) > 0;
    {
        let d = h.dyn_state(node);
        if d.recalc_split {
            if object_num < t.split_threshold && !has_children {
                d.recalc_split = false;
            }
        } else if !d.is_split && object_num >= t.split_threshold {
            d.recalc_split = true;
        }
    }

    if h.dyn_state(node).recalc_split {
        h.dyn_state(node).recalc_split = false;
        if h.dyn_state(node).recalc_split_axis {
            h.dyn_state(node).recalc_split_axis = false;
            let b = h.base(node);
            let size = sub3(b.max, b.min);
            h.dyn_state(node).split_axis = longest_axis(size);
        }
        let axis = h.dyn_state(node).split_axis;

        let mut pos_sum = 0.0f32;
        let mut weight_sum = 0.0f32;
        let mut objs = Vec::new();
        h.objects(node, &mut objs);
        for &o in &objs {
            pos_sum += axis_value(h.object_bv(o).position, axis);
            weight_sum += 1.0;
        }
        if h.child_count(node) > 0 {
            let mul = t.split_threshold as f32;
            h.children(node, &mut kids);
            for &c in &kids {
                let (c_axis, c_plane) = {
                    let cd = h.dyn_state(c);
                    (cd.split_axis, cd.split_plane)
                };
                let add = if c_axis == axis {
                    c_plane
                } else {
                    axis_value(h.base(c).center, axis)
                };
                pos_sum = add.mul_add(mul, pos_sum);
                weight_sum += mul;
            }
        }
        h.dyn_state(node).split_plane = pos_sum / weight_sum;
    }

    if h.object_count(node) as i32 >= t.split_threshold && h.child_count(node) == 0 {
        if h.dyn_state(node).is_split {
            if h.dyn_state(node).object_moved {
                let d = h.dyn_state(node);
                d.ignore_split_count = d.ignore_split_count.wrapping_sub(1);
                if d.ignore_split_count <= 0 {
                    d.is_split = false;
                }
            }
        } else {
            split_node(h, t, node);
        }
    }

    h.dyn_state(node).object_moved = false;
}

fn split_node<H: Host>(h: &mut H, t: &DynTree, node: Handle) {
    {
        let d = h.dyn_state(node);
        d.is_split = true;
        d.ignore_split_count = t.max_ignore_split_count;
    }
    let (size, plane, axis) = {
        let b = h.base(node);
        let size = sub3(b.max, b.min);
        let d = h.dyn_state(node);
        (size, d.split_plane, d.split_axis)
    };
    let mut temp = Vec::new();
    h.objects(node, &mut temp);
    h.clear_objects(node);

    let mut groups: [Vec<Handle>; 2] = [Vec::new(), Vec::new()];
    for &o in &temp {
        let bv = h.object_bv(o);
        let g = split_group(bv.min, bv.max, plane, axis, size, t.max_intersection_amount);
        match g {
            0 => groups[0].push(o),
            1 => groups[1].push(o),
            _ => h.push_object(node, o),
        }
    }

    if groups.iter().any(|g| g.is_empty()) {
        for g in &groups {
            for &o in g {
                h.push_object(node, o);
            }
        }
        return;
    }
    for g in &groups {
        let child = h.new_child(node);
        for &o in g {
            h.push_object(child, o);
            h.set_object_node(o, child);
        }
        {
            let d = h.dyn_state(child);
            d.recalc_split = true;
            d.recalc_aabb = true;
        }
        set_group_bounds(h, child);
    }
}

/// `cRenderableContainer_DynBoxTree::AddObjectToNodeIterative` from `start`; returns the node
/// that takes the object.
pub fn get_add_node<H: Host>(h: &mut H, t: &DynTree, start: Handle, obj: Handle) -> Handle {
    let bv = h.object_bv(obj);
    let mut node = start;
    let mut kids = Vec::new();
    loop {
        {
            let b = h.base(node);
            if !aabb_inside(&bv, b) {
                expand(&mut b.min, &mut b.max, bv.min, bv.max);
                center_radius(b);
                h.dyn_state(node).recalc_split = true;
            }
        }
        if h.child_count(node) == 0 {
            return node;
        }
        let (size, plane, axis) = {
            let b = h.base(node);
            let size = sub3(b.max, b.min);
            let d = h.dyn_state(node);
            (size, d.split_plane, d.split_axis)
        };
        let g = split_group(bv.min, bv.max, plane, axis, size, t.max_intersection_amount);
        h.children(node, &mut kids);
        match g {
            0 => node = kids.first().copied().unwrap_or(0),
            1 => node = kids.last().copied().unwrap_or(0),
            _ => return node,
        }
    }
}

/// `cRenderableContainer_DynBoxTree::Add`, minus the callback registration.
pub fn dyn_add<H: Host>(h: &mut H, t: &mut DynTree, root: Handle, obj: Handle) -> Handle {
    let node = get_add_node(h, t, root, obj);
    h.push_object(node, obj);
    h.set_object_node(obj, node);
    t.rebuild_count = t.rebuild_count.wrapping_sub(1);
    node
}

fn remove_node<H: Host>(h: &mut H, node: Handle) {
    let parent = h.base(node).parent;
    if parent == 0 {
        return;
    }
    h.delete_child(parent, node);
    let mut kids = Vec::new();
    h.children(parent, &mut kids);
    // Two children were always present; a lone parent leaves nothing to merge (the C++ read an
    // empty list's front()).
    let remaining = kids.first().copied();
    h.clear_children(parent);
    if let Some(child) = remaining {
        if h.object_count(child) > 0 {
            let mut objs = Vec::new();
            h.objects(child, &mut objs);
            for o in objs {
                h.set_object_node(o, parent);
                h.push_object(parent, o);
            }
        }
        if h.child_count(child) > 0 {
            let mut grand = Vec::new();
            h.children(child, &mut grand);
            for g in grand {
                h.base(g).parent = parent;
                h.push_child(parent, g);
            }
            h.clear_children(child);
        }
        h.delete_node(child);
    }
    h.dyn_state(parent).recalc_aabb = true;
    h.dyn_state(parent).is_split = false;
}

fn check_aabb_needs_update<H: Host>(h: &mut H, mut node: Handle, obj: Handle) {
    let bv = h.object_bv(obj);
    while node != 0 {
        let b = *h.base(node);
        if !at_edge(bv.min, bv.max, b.min, b.max) {
            return;
        }
        h.dyn_state(node).recalc_aabb = true;
        node = b.parent;
    }
}

/// `cRenderableContainer_DynBoxTree::Remove` minus the callback removal and the to-update set.
/// Returns false when the object was in no node (the C++ returns early there).
pub fn dyn_remove<H: Host>(h: &mut H, t: &mut DynTree, obj: Handle) -> bool {
    let node = h.object_node(obj);
    if node == 0 {
        return false;
    }
    h.remove_first_object(node, obj);
    if h.child_count(node) == 0 && h.object_count(node) == 0 && h.base(node).parent != 0 {
        remove_node(h, node);
    } else {
        check_aabb_needs_update(h, node, obj);
    }
    h.set_object_node(obj, 0);
    t.rebuild_count = t.rebuild_count.wrapping_sub(1);
    true
}

fn update_object<H: Host>(h: &mut H, t: &mut DynTree, obj: Handle) {
    let node = h.object_node(obj);
    if node == 0 {
        return;
    }
    object_moved(h, node);
    let bv = h.object_bv(obj);
    if aabb_inside(&bv, h.base(node)) {
        return;
    }

    let mut fit = node;
    let mut cur = h.base(node).parent;
    while cur != 0 {
        let b = *h.base(cur);
        if b.parent == 0 || aabb_inside(&bv, &b) {
            fit = cur;
            break;
        }
        cur = b.parent;
    }

    let new_node = get_add_node(h, t, fit, obj);
    if new_node != node {
        object_moved(h, new_node);
        t.rebuild_count = t.rebuild_count.wrapping_sub(1);
        let bv = h.object_bv(obj);
        if !aabb_inside(&bv, h.base(new_node)) {
            push_up_need_aabb_update(h, new_node);
        }
        h.set_object_node(obj, new_node);
        h.push_object(new_node, obj);
        h.remove_first_object(node, obj);
        if h.child_count(node) == 0 && h.object_count(node) == 0 {
            remove_node(h, node);
        } else {
            push_up_need_aabb_update(h, node);
        }
    } else {
        push_up_need_aabb_update(h, node);
    }
}

fn add_node_objects_to_root<H: Host>(h: &mut H, root: Handle, node: Handle) {
    let mut objs = Vec::new();
    h.objects(node, &mut objs);
    for o in objs {
        h.set_object_node(o, root);
        h.push_object(root, o);
    }
    let mut kids = Vec::new();
    h.children(node, &mut kids);
    for c in kids {
        add_node_objects_to_root(h, root, c);
    }
}

/// `cRenderableContainer_DynBoxTree::SpecificUpdateBeforeRendering`; `moved` is the
/// to-update set in iteration (address) order.
pub fn dyn_specific_update<H: Host>(h: &mut H, t: &mut DynTree, root: Handle, moved: &[Handle]) {
    for &o in moved {
        update_object(h, t, o);
    }
    if t.rebuild_count > 0 {
        return;
    }
    t.rebuild_count = t.max_rebuild_count;

    let mut kids = Vec::new();
    h.children(root, &mut kids);
    for c in kids {
        add_node_objects_to_root(h, root, c);
    }
    h.delete_children(root);
    let d = h.dyn_state(root);
    d.is_split = false;
    d.recalc_aabb = true;
    d.recalc_split_axis = true;
    d.recalc_split = true;
}
