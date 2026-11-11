use std::collections::HashMap;

use super::boxtree_dyn::*;
use crate::math::v3;
use crate::render::renderer::{MatView, ObjCheck, RnHost};
use crate::types::Vector3f;

pub(crate) struct MockNode {
    pub base: NodeBase,
    pub d: NodeDyn,
    pub children: Vec<Handle>,
    pub objects: Vec<Handle>,
}

pub(crate) struct MockObj {
    pub bv: Bv,
    pub node: Handle,
    pub visible: bool,
    pub flags: u32,
    pub fade: f32,
    pub culled_by_distance: bool,
    pub material: MatView,
    pub z: f32,
}

#[derive(Default)]
pub(crate) struct Mock {
    pub nodes: HashMap<Handle, Box<MockNode>>,
    pub objs: HashMap<Handle, MockObj>,
    next_node: Handle,
    pub log: Vec<String>,
    pub next_query: Handle,
    pub samples: HashMap<Handle, u32>,
    pub query_node: HashMap<Handle, Handle>,
    pub rendered: Vec<Handle>,
    pub listed: Vec<Handle>,
    pub collision: i32,
}

impl Mock {
    pub fn new() -> Self {
        Self {
            next_node: 0x1000,
            next_query: 0x9000,
            ..Self::default()
        }
    }

    pub fn add_node(&mut self, uses_flags: bool) -> Handle {
        let h = self.next_node;
        self.next_node += 0x100;
        self.nodes.insert(
            h,
            Box::new(MockNode {
                base: NodeBase::new(uses_flags),
                d: NodeDyn::new(),
                children: Vec::new(),
                objects: Vec::new(),
            }),
        );
        h
    }

    pub fn add_obj(&mut self, id: Handle, min: Vector3f, max: Vector3f) {
        let center = v3(
            (min.x + max.x) * 0.5,
            (min.y + max.y) * 0.5,
            (min.z + max.z) * 0.5,
        );
        self.objs.insert(
            id,
            MockObj {
                bv: Bv {
                    min,
                    max,
                    center,
                    position: center,
                    radius: 1.0,
                },
                node: 0,
                visible: true,
                flags: 0,
                fade: 1.0,
                culled_by_distance: false,
                material: MatView {
                    present: true,
                    ..MatView::default()
                },
                z: 0.0,
            },
        );
    }

    pub fn move_obj(&mut self, id: Handle, to: Vector3f, half: f32) {
        let o = self.objs.get_mut(&id).unwrap();
        o.bv.min = v3(to.x - half, to.y - half, to.z - half);
        o.bv.max = v3(to.x + half, to.y + half, to.z + half);
        o.bv.center = to;
        o.bv.position = to;
    }

    fn n(&mut self, h: Handle) -> &mut MockNode {
        self.nodes.get_mut(&h).unwrap()
    }
}

impl Host for Mock {
    fn base(&mut self, node: Handle) -> &mut NodeBase {
        &mut self.n(node).base
    }
    fn dyn_state(&mut self, node: Handle) -> &mut NodeDyn {
        &mut self.n(node).d
    }
    fn child_count(&mut self, node: Handle) -> usize {
        self.n(node).children.len()
    }
    fn children(&mut self, node: Handle, out: &mut Vec<Handle>) {
        out.clone_from(&self.n(node).children);
    }
    fn object_count(&mut self, node: Handle) -> usize {
        self.n(node).objects.len()
    }
    fn objects(&mut self, node: Handle, out: &mut Vec<Handle>) {
        out.clone_from(&self.n(node).objects);
    }
    fn push_object(&mut self, node: Handle, obj: Handle) {
        self.n(node).objects.push(obj);
    }
    fn clear_objects(&mut self, node: Handle) {
        self.n(node).objects.clear();
    }
    fn remove_first_object(&mut self, node: Handle, obj: Handle) {
        let v = &mut self.n(node).objects;
        if let Some(i) = v.iter().position(|o| *o == obj) {
            v.remove(i);
        }
    }
    fn remove_all_objects(&mut self, node: Handle, obj: Handle) {
        self.n(node).objects.retain(|o| *o != obj);
    }
    fn new_child(&mut self, parent: Handle) -> Handle {
        let c = self.add_node(false);
        self.n(c).base.parent = parent;
        self.n(parent).children.push(c);
        c
    }
    fn delete_children(&mut self, node: Handle) {
        let kids = std::mem::take(&mut self.n(node).children);
        for k in kids {
            self.delete_node(k);
        }
    }
    fn delete_child(&mut self, parent: Handle, child: Handle) {
        self.n(parent).children.retain(|c| *c != child);
        self.delete_node(child);
    }
    fn clear_children(&mut self, node: Handle) {
        self.n(node).children.clear();
    }
    fn push_child(&mut self, node: Handle, child: Handle) {
        self.n(node).children.push(child);
    }
    fn delete_node(&mut self, node: Handle) {
        let kids = std::mem::take(&mut self.n(node).children);
        for k in kids {
            self.delete_node(k);
        }
        self.nodes.remove(&node);
    }
    fn object_node(&mut self, obj: Handle) -> Handle {
        self.objs[&obj].node
    }
    fn set_object_node(&mut self, obj: Handle, node: Handle) {
        self.objs.get_mut(&obj).unwrap().node = node;
    }
    fn object_bv(&mut self, obj: Handle) -> Bv {
        self.objs[&obj].bv
    }
    fn object_visible(&mut self, obj: Handle) -> bool {
        self.objs[&obj].visible
    }
    fn object_render_flags(&mut self, obj: Handle) -> u32 {
        self.objs[&obj].flags
    }
}

impl RnHost for Mock {
    fn update_node(&mut self, node: Handle) {
        if self.n(node).base.uses_flags_and_visibility {
            return;
        }
        // dyn nodes only
        let t = DynTree::new();
        update_before_use(self, &t, node);
    }
    fn collide_node(&mut self, _frustum: Handle, _node: Handle) -> i32 {
        self.collision
    }
    fn collide_object(&mut self, _frustum: Handle, _obj: Handle) -> i32 {
        self.collision
    }
    fn object_collides_frustum(&mut self, _frustum: Handle, _obj: Handle) -> bool {
        self.collision != 1
    }
    fn near_plane_aabb(&mut self, _frustum: Handle, _min: Vector3f, _max: Vector3f) -> bool {
        false
    }
    fn object_check(&mut self, obj: Handle) -> ObjCheck {
        let o = &self.objs[&obj];
        ObjCheck {
            visible: o.visible,
            culled_by_fog: false,
            culled_by_distance: o.culled_by_distance,
            has_bv: true,
            render_flags: o.flags,
            fade: o.fade,
            min: o.bv.min,
            max: o.bv.max,
        }
    }
    fn set_object_fade(&mut self, obj: Handle, fade: f32) {
        self.objs.get_mut(&obj).unwrap().fade = fade;
    }
    fn object_material(&mut self, obj: Handle) -> MatView {
        self.objs[&obj].material
    }
    fn set_view_space_z(&mut self, obj: Handle, z: f32) {
        self.objs.get_mut(&obj).unwrap().z = z;
    }
    fn view_space_z(&mut self, obj: Handle) -> f32 {
        self.objs[&obj].z
    }
    fn add_to_render_list(&mut self, obj: Handle) {
        self.listed.push(obj);
    }
    fn render_object(&mut self, obj: Handle) -> bool {
        self.rendered.push(obj);
        true
    }
    fn render_node_bbox(&mut self, node: Handle, query: Handle) {
        if query != 0 {
            self.query_node.insert(query, node);
        }
        self.log.push(format!("bbox {node:x} q{query:x}"));
    }
    fn get_query(&mut self) -> Handle {
        self.next_query += 1;
        self.next_query
    }
    fn release_query(&mut self, _query: Handle) {}
    fn query_fetch(&mut self, _query: Handle) -> bool {
        true
    }
    fn query_samples(&mut self, query: Handle) -> u32 {
        let node = self.query_node.get(&query).copied().unwrap_or(0);
        self.samples.get(&node).copied().unwrap_or(100)
    }
    fn flush(&mut self) {
        self.log.push("flush".into());
    }
}

struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u32 {
        self.0 = self
            .0
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        (self.0 >> 33) as u32
    }
    fn f(&mut self, a: f32, b: f32) -> f32 {
        a + (b - a) * (self.next() & 0xffff) as f32 / 65535.0
    }
}

/// Every object sits in exactly the node it points at, parents link back, and node bounds
/// hold their objects once `UpdateBeforeUse` has run over the tree.
fn check_tree(m: &mut Mock, root: Handle, objs: &[Handle]) {
    fn walk(m: &mut Mock, n: Handle, seen: &mut Vec<Handle>) {
        let (kids, os) = {
            let node = m.n(n);
            (node.children.clone(), node.objects.clone())
        };
        for o in os {
            assert_eq!(m.objs[&o].node, n, "object points at its node");
            seen.push(o);
        }
        for k in kids {
            assert_eq!(m.n(k).base.parent, n, "child links to parent");
            walk(m, k, seen);
        }
    }
    let mut seen = Vec::new();
    walk(m, root, &mut seen);
    seen.sort_unstable();
    let mut want = objs.to_vec();
    want.sort_unstable();
    assert_eq!(seen, want);
}

fn flush_tree(m: &mut Mock, t: &DynTree, n: Handle) {
    update_before_use(m, t, n);
    let kids = m.n(n).children.clone();
    for k in kids {
        flush_tree(m, t, k);
    }
}

fn contained(m: &mut Mock, n: Handle) {
    let (min, max, kids, os) = {
        let node = m.n(n);
        (
            node.base.min,
            node.base.max,
            node.children.clone(),
            node.objects.clone(),
        )
    };
    for o in os {
        let bv = m.objs[&o].bv;
        assert!(
            bv.min.x >= min.x && bv.max.x <= max.x && bv.min.y >= min.y && bv.max.y <= max.y,
            "node bounds hold object"
        );
    }
    for k in kids {
        contained(m, k);
    }
}

#[test]
fn dyn_tree_splits_and_stays_consistent() {
    let mut m = Mock::new();
    let mut t = DynTree::new();
    let root = m.add_node(false);
    let mut rng = Rng(7);
    let mut ids = Vec::new();
    for i in 0..200 {
        let id = 0x100000 + i * 0x10;
        let p = v3(rng.f(-50.0, 50.0), rng.f(-5.0, 5.0), rng.f(-50.0, 50.0));
        m.add_obj(
            id,
            v3(p.x - 0.5, p.y - 0.5, p.z - 0.5),
            v3(p.x + 0.5, p.y + 0.5, p.z + 0.5),
        );
        dyn_add(&mut m, &mut t, root, id);
        ids.push(id);
    }
    check_tree(&mut m, root, &ids);
    for _ in 0..4 {
        flush_tree(&mut m, &t, root);
    }
    assert!(m.n(root).children.len() == 2, "root split into two");
    check_tree(&mut m, root, &ids);
    contained(&mut m, root);

    for round in 0..30 {
        let mut moved = Vec::new();
        for &id in &ids {
            if rng.next().is_multiple_of(4) {
                let p = v3(rng.f(-60.0, 60.0), rng.f(-8.0, 8.0), rng.f(-60.0, 60.0));
                m.move_obj(id, p, 0.5);
                moved.push(id);
            }
        }
        moved.sort_unstable();
        dyn_specific_update(&mut m, &mut t, root, &moved);
        if round % 3 == 0 {
            let victim = ids.swap_remove((rng.next() as usize) % ids.len());
            assert!(dyn_remove(&mut m, &mut t, victim));
            assert!(!dyn_remove(&mut m, &mut t, victim));
        }
        update_before_rendering(&mut m, root);
        flush_tree(&mut m, &t, root);
        check_tree(&mut m, root, &ids);
    }
}

#[test]
fn dyn_remove_merges_lone_sibling() {
    let mut m = Mock::new();
    let mut t = DynTree::new();
    t.split_threshold = 4;
    let root = m.add_node(false);
    let mut ids = Vec::new();
    for i in 0..8 {
        let id = 0x100000 + i * 0x10;
        let x = if i < 4 { -10.0 } else { 10.0 } + i as f32 * 0.1;
        m.add_obj(id, v3(x - 0.5, -0.5, -0.5), v3(x + 0.5, 0.5, 0.5));
        dyn_add(&mut m, &mut t, root, id);
        ids.push(id);
    }
    flush_tree(&mut m, &t, root);
    flush_tree(&mut m, &t, root);
    assert_eq!(m.n(root).children.len(), 2);
    for &id in ids.iter().take(4) {
        dyn_remove(&mut m, &mut t, id);
    }
    assert_eq!(m.n(root).children.len(), 0, "emptied leaf merged away");
    assert_eq!(m.n(root).objects.len(), 4);
    assert!(!m.n(root).d.is_split);
    assert!(m.n(root).d.recalc_aabb);
    check_tree(&mut m, root, &ids[4..]);
}

#[test]
fn rebuild_flattens_tree() {
    let mut m = Mock::new();
    let mut t = DynTree::new();
    t.split_threshold = 4;
    let root = m.add_node(false);
    let mut ids = Vec::new();
    for i in 0..10 {
        let id = 0x100000 + i * 0x10;
        let x = i as f32 * 5.0;
        m.add_obj(id, v3(x, 0.0, 0.0), v3(x + 1.0, 1.0, 1.0));
        dyn_add(&mut m, &mut t, root, id);
        ids.push(id);
    }
    flush_tree(&mut m, &t, root);
    assert!(!m.n(root).children.is_empty());
    t.rebuild_count = 0;
    dyn_specific_update(&mut m, &mut t, root, &[]);
    assert!(m.n(root).children.is_empty());
    assert_eq!(t.rebuild_count, t.max_rebuild_count);
    assert!(m.n(root).d.recalc_split && m.n(root).d.recalc_split_axis);
    check_tree(&mut m, root, &ids);
}

#[test]
fn property_and_aabb_passes() {
    let mut m = Mock::new();
    let root = m.add_node(true);
    let leaf = m.add_node(true);
    m.n(root).children.push(leaf);
    m.n(leaf).base.parent = root;
    m.add_obj(0x100000, v3(1.0, 2.0, 3.0), v3(4.0, 5.0, 6.0));
    m.objs.get_mut(&0x100000).unwrap().flags = 0b101;
    m.objs.get_mut(&0x100000).unwrap().node = leaf;
    m.n(leaf).objects.push(0x100000);
    m.n(leaf).base.need_aabb_update = true;
    m.n(root).base.need_aabb_update = true;
    update_before_rendering(&mut m, root);
    assert_eq!(m.n(root).base.render_flags, 0b101);
    assert!(m.n(root).base.visible_objects);
    assert_eq!(m.n(root).base.min, v3(1.0, 2.0, 3.0));
    assert_eq!(m.n(root).base.center, v3(2.5, 3.5, 4.5));
    assert!(!m.n(leaf).base.need_property_update);

    m.objs.get_mut(&0x100000).unwrap().visible = false;
    object_properties_changed(&mut m, 0x100000);
    assert!(m.n(root).base.need_property_update);
    update_before_rendering(&mut m, root);
    assert!(!m.n(root).base.visible_objects);
}

#[test]
fn boxtree_remove_misses_aabb_update() {
    let mut m = Mock::new();
    let root = m.add_node(true);
    m.add_obj(0x100000, v3(0.0, 0.0, 0.0), v3(1.0, 1.0, 1.0));
    m.n(root).objects.push(0x100000);
    boxtree_remove(&mut m, root, 0x100000);
    assert!(m.n(root).objects.is_empty());
    assert!(!m.n(root).base.need_aabb_update);
    m.n(root).objects.extend([0x100000, 0x100010, 0x100000]);
    boxtree_remove_set(&mut m, root, &[0x100010, 0x100000]);
    assert!(m.n(root).objects.is_empty());
}
