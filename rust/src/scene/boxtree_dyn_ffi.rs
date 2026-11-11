//! C ABI of the container and renderer culling code, declared in
//! `HPL2/core/include/graphics/RustRenderer.h`.
//!
//! [`HostVt`] is the table of C++ callbacks both sides share; handles cross as `uintptr_t`. Every
//! `vt`/pointer argument is non-null, valid and aligned for the call; a null `vt` makes a call a
//! no-op. Entries a caller does not need may be null (the call then does nothing).

use std::ffi::c_void;

use super::boxtree_dyn::{
    Bv, DynTree, Handle, Host, NodeBase, NodeDyn, boxtree_remove, boxtree_remove_set,
    calculate_min_max_from_objects, dyn_add, dyn_remove, dyn_specific_update, get_add_node,
    object_properties_changed, push_up_need_aabb_update, update_before_rendering,
    update_before_use,
};
use crate::render::renderer::{MatView, ObjCheck, RnHost};
use crate::types::Vector3f;

type Fn0<R = ()> = Option<unsafe extern "C" fn(*mut c_void) -> R>;
type Fn1<A, R = ()> = Option<unsafe extern "C" fn(*mut c_void, A) -> R>;
type Fn2<A, B, R = ()> = Option<unsafe extern "C" fn(*mut c_void, A, B) -> R>;
type Fn3<A, B, C, R = ()> = Option<unsafe extern "C" fn(*mut c_void, A, B, C) -> R>;

#[repr(C)]
pub struct HostVt {
    pub base: Fn1<Handle, *mut NodeBase>,
    pub dyn_state: Fn1<Handle, *mut NodeDyn>,
    pub child_count: Fn1<Handle, usize>,
    pub children: Fn3<Handle, *mut Handle, usize, usize>,
    pub object_count: Fn1<Handle, usize>,
    pub objects: Fn3<Handle, *mut Handle, usize, usize>,
    pub push_object: Fn2<Handle, Handle>,
    pub clear_objects: Fn1<Handle>,
    pub remove_first_object: Fn2<Handle, Handle>,
    pub remove_all_objects: Fn2<Handle, Handle>,
    pub new_child: Fn1<Handle, Handle>,
    pub delete_children: Fn1<Handle>,
    pub delete_child: Fn2<Handle, Handle>,
    pub clear_children: Fn1<Handle>,
    pub push_child: Fn2<Handle, Handle>,
    pub delete_node: Fn1<Handle>,
    pub object_node: Fn1<Handle, Handle>,
    pub set_object_node: Fn2<Handle, Handle>,
    pub object_bv: Fn2<Handle, *mut Bv>,
    pub object_visible: Fn1<Handle, bool>,
    pub object_render_flags: Fn1<Handle, u32>,
    pub update_node: Fn1<Handle>,
    pub collide_node: Fn2<Handle, Handle, i32>,
    pub collide_object: Fn2<Handle, Handle, i32>,
    pub object_collides_frustum: Fn2<Handle, Handle, bool>,
    pub near_plane_aabb: Fn3<Handle, *const Vector3f, *const Vector3f, bool>,
    pub object_check: Fn2<Handle, *mut ObjCheck>,
    pub set_object_fade: Fn2<Handle, f32>,
    pub object_material: Fn2<Handle, *mut MatView>,
    pub set_view_space_z: Fn2<Handle, f32>,
    pub view_space_z: Fn1<Handle, f32>,
    pub add_to_render_list: Fn1<Handle>,
    pub render_object: Fn1<Handle, bool>,
    pub render_node_bbox: Fn2<Handle, Handle>,
    pub get_query: Fn0<Handle>,
    pub release_query: Fn1<Handle>,
    pub query_fetch: Fn1<Handle, bool>,
    pub query_samples: Fn1<Handle, u32>,
    pub flush: Fn0,
    pub caster: Fn1<Handle>,
}

pub struct FfiHost<'a> {
    vt: &'a HostVt,
    ctx: *mut c_void,
    scratch_base: NodeBase,
    scratch_dyn: NodeDyn,
}

impl<'a> FfiHost<'a> {
    /// # Safety
    /// `vt` is null or points at a table whose entries follow the declared signatures and accept
    /// `ctx`.
    pub unsafe fn new(vt: *const HostVt, ctx: *mut c_void) -> Option<Self> {
        // SAFETY: caller contract.
        let vt = unsafe { vt.as_ref() }?;
        Some(Self {
            vt,
            ctx,
            scratch_base: NodeBase::new(false),
            scratch_dyn: NodeDyn::new(),
        })
    }
}

macro_rules! call {
    ($s:expr, $f:ident $(, $a:expr)*) => {
        match $s.vt.$f {
            // SAFETY: the table's entries follow the declared signatures (see `FfiHost::new`).
            Some(f) => unsafe { f($s.ctx $(, $a)*) },
            None => Default::default(),
        }
    };
}

fn fill(out: &mut Vec<Handle>, mut fetch: impl FnMut(*mut Handle, usize) -> usize) {
    out.clear();
    out.resize(8, 0);
    let n = fetch(out.as_mut_ptr(), out.len());
    if n > out.len() {
        out.resize(n, 0);
        fetch(out.as_mut_ptr(), out.len());
    }
    out.truncate(n);
}

impl Host for FfiHost<'_> {
    fn base(&mut self, node: Handle) -> &mut NodeBase {
        let p = match self.vt.base {
            // SAFETY: the table's entries follow the declared signatures.
            Some(f) => unsafe { f(self.ctx, node) },
            None => std::ptr::null_mut(),
        };
        // SAFETY: the callback returns the node's embedded state, valid while the node lives; no
        // other reference to it exists while this one does.
        match unsafe { p.as_mut() } {
            Some(b) => b,
            None => &mut self.scratch_base,
        }
    }

    fn dyn_state(&mut self, node: Handle) -> &mut NodeDyn {
        let p = match self.vt.dyn_state {
            // SAFETY: the table's entries follow the declared signatures.
            Some(f) => unsafe { f(self.ctx, node) },
            None => std::ptr::null_mut(),
        };
        // SAFETY: as in `base`.
        match unsafe { p.as_mut() } {
            Some(b) => b,
            None => &mut self.scratch_dyn,
        }
    }

    fn child_count(&mut self, node: Handle) -> usize {
        call!(self, child_count, node)
    }

    fn children(&mut self, node: Handle, out: &mut Vec<Handle>) {
        let Some(f) = self.vt.children else {
            out.clear();
            return;
        };
        let ctx = self.ctx;
        // SAFETY: the callback writes at most `cap` handles to `ptr`.
        fill(out, |ptr, cap| unsafe { f(ctx, node, ptr, cap) });
    }

    fn object_count(&mut self, node: Handle) -> usize {
        call!(self, object_count, node)
    }

    fn objects(&mut self, node: Handle, out: &mut Vec<Handle>) {
        let Some(f) = self.vt.objects else {
            out.clear();
            return;
        };
        let ctx = self.ctx;
        // SAFETY: the callback writes at most `cap` handles to `ptr`.
        fill(out, |ptr, cap| unsafe { f(ctx, node, ptr, cap) });
    }

    fn push_object(&mut self, node: Handle, obj: Handle) {
        call!(self, push_object, node, obj)
    }

    fn clear_objects(&mut self, node: Handle) {
        call!(self, clear_objects, node)
    }

    fn remove_first_object(&mut self, node: Handle, obj: Handle) {
        call!(self, remove_first_object, node, obj)
    }

    fn remove_all_objects(&mut self, node: Handle, obj: Handle) {
        call!(self, remove_all_objects, node, obj)
    }

    fn new_child(&mut self, parent: Handle) -> Handle {
        call!(self, new_child, parent)
    }

    fn delete_children(&mut self, node: Handle) {
        call!(self, delete_children, node)
    }

    fn delete_child(&mut self, parent: Handle, child: Handle) {
        call!(self, delete_child, parent, child)
    }

    fn clear_children(&mut self, node: Handle) {
        call!(self, clear_children, node)
    }

    fn push_child(&mut self, node: Handle, child: Handle) {
        call!(self, push_child, node, child)
    }

    fn delete_node(&mut self, node: Handle) {
        call!(self, delete_node, node)
    }

    fn object_node(&mut self, obj: Handle) -> Handle {
        call!(self, object_node, obj)
    }

    fn set_object_node(&mut self, obj: Handle, node: Handle) {
        call!(self, set_object_node, obj, node)
    }

    fn object_bv(&mut self, obj: Handle) -> Bv {
        let mut bv = Bv::default();
        call!(self, object_bv, obj, &mut bv);
        bv
    }

    fn object_visible(&mut self, obj: Handle) -> bool {
        call!(self, object_visible, obj)
    }

    fn object_render_flags(&mut self, obj: Handle) -> u32 {
        call!(self, object_render_flags, obj)
    }
}

impl RnHost for FfiHost<'_> {
    fn update_node(&mut self, node: Handle) {
        call!(self, update_node, node)
    }

    fn collide_node(&mut self, frustum: Handle, node: Handle) -> i32 {
        call!(self, collide_node, frustum, node)
    }

    fn collide_object(&mut self, frustum: Handle, obj: Handle) -> i32 {
        call!(self, collide_object, frustum, obj)
    }

    fn object_collides_frustum(&mut self, frustum: Handle, obj: Handle) -> bool {
        call!(self, object_collides_frustum, frustum, obj)
    }

    fn near_plane_aabb(&mut self, frustum: Handle, min: Vector3f, max: Vector3f) -> bool {
        call!(self, near_plane_aabb, frustum, &min, &max)
    }

    fn object_check(&mut self, obj: Handle) -> ObjCheck {
        let mut v = ObjCheck::default();
        call!(self, object_check, obj, &mut v);
        v
    }

    fn set_object_fade(&mut self, obj: Handle, fade: f32) {
        call!(self, set_object_fade, obj, fade)
    }

    fn object_material(&mut self, obj: Handle) -> MatView {
        let mut v = MatView::default();
        call!(self, object_material, obj, &mut v);
        v
    }

    fn set_view_space_z(&mut self, obj: Handle, z: f32) {
        call!(self, set_view_space_z, obj, z)
    }

    fn view_space_z(&mut self, obj: Handle) -> f32 {
        call!(self, view_space_z, obj)
    }

    fn add_to_render_list(&mut self, obj: Handle) {
        call!(self, add_to_render_list, obj)
    }

    fn render_object(&mut self, obj: Handle) -> bool {
        call!(self, render_object, obj)
    }

    fn render_node_bbox(&mut self, node: Handle, query: Handle) {
        call!(self, render_node_bbox, node, query)
    }

    fn get_query(&mut self) -> Handle {
        call!(self, get_query)
    }

    fn release_query(&mut self, query: Handle) {
        call!(self, release_query, query)
    }

    fn query_fetch(&mut self, query: Handle) -> bool {
        call!(self, query_fetch, query)
    }

    fn query_samples(&mut self, query: Handle) -> u32 {
        call!(self, query_samples, query)
    }

    fn flush(&mut self) {
        call!(self, flush)
    }
}

impl FfiHost<'_> {
    pub fn caster(&mut self, obj: Handle) {
        call!(self, caster, obj)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_bt_node_base_init(out: *mut NodeBase, uses_flags_and_visibility: bool) {
    // SAFETY: `out` is null or valid for writes.
    if let Some(o) = unsafe { out.as_mut() } {
        *o = NodeBase::new(uses_flags_and_visibility);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_bt_node_dyn_init(out: *mut NodeDyn) {
    // SAFETY: `out` is null or valid for writes.
    if let Some(o) = unsafe { out.as_mut() } {
        *o = NodeDyn::new();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_bt_tree_init(out: *mut DynTree) {
    // SAFETY: `out` is null or valid for writes.
    if let Some(o) = unsafe { out.as_mut() } {
        *o = DynTree::new();
    }
}

macro_rules! host {
    ($vt:expr, $ctx:expr) => {
        // SAFETY: the caller's contract on `vt`.
        match unsafe { FfiHost::new($vt, $ctx) } {
            Some(h) => h,
            None => return Default::default(),
        }
    };
}

/// # Safety
/// `vt` follows the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_update_before_rendering(
    vt: *const HostVt,
    ctx: *mut c_void,
    root: Handle,
) {
    let mut h = host!(vt, ctx);
    update_before_rendering(&mut h, root);
}

/// # Safety
/// `vt` follows the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_object_properties_changed(
    vt: *const HostVt,
    ctx: *mut c_void,
    obj: Handle,
) {
    let mut h = host!(vt, ctx);
    object_properties_changed(&mut h, obj);
}

/// # Safety
/// `vt` follows the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_calculate_min_max(
    vt: *const HostVt,
    ctx: *mut c_void,
    node: Handle,
) {
    let mut h = host!(vt, ctx);
    calculate_min_max_from_objects(&mut h, node);
}

/// # Safety
/// `vt` follows the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_push_up_aabb_update(
    vt: *const HostVt,
    ctx: *mut c_void,
    node: Handle,
) {
    let mut h = host!(vt, ctx);
    push_up_need_aabb_update(&mut h, node);
}

/// # Safety
/// `vt` follows the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_boxtree_remove(
    vt: *const HostVt,
    ctx: *mut c_void,
    root: Handle,
    obj: Handle,
) {
    let mut h = host!(vt, ctx);
    boxtree_remove(&mut h, root, obj);
}

/// # Safety
/// `vt` follows the module contract; `set` points at `n` handles (or is null with `n` 0).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_boxtree_remove_set(
    vt: *const HostVt,
    ctx: *mut c_void,
    root: Handle,
    set: *const Handle,
    n: usize,
) {
    let mut h = host!(vt, ctx);
    let set: &[Handle] = if set.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(set, n) }
    };
    boxtree_remove_set(&mut h, root, set);
}

/// # Safety
/// `vt` follows the module contract; `tree` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_dyn_update_before_use(
    vt: *const HostVt,
    ctx: *mut c_void,
    tree: *const DynTree,
    node: Handle,
) {
    let mut h = host!(vt, ctx);
    // SAFETY: caller contract.
    if let Some(t) = unsafe { tree.as_ref() } {
        update_before_use(&mut h, t, node);
    }
}

/// Returns the node the object went into.
///
/// # Safety
/// `vt` follows the module contract; `tree` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_dyn_add(
    vt: *const HostVt,
    ctx: *mut c_void,
    tree: *mut DynTree,
    root: Handle,
    obj: Handle,
) -> Handle {
    let mut h = host!(vt, ctx);
    // SAFETY: caller contract.
    match unsafe { tree.as_mut() } {
        Some(t) => dyn_add(&mut h, t, root, obj),
        None => 0,
    }
}

/// # Safety
/// `vt` follows the module contract; `tree` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_dyn_remove(
    vt: *const HostVt,
    ctx: *mut c_void,
    tree: *mut DynTree,
    obj: Handle,
) -> bool {
    let mut h = host!(vt, ctx);
    // SAFETY: caller contract.
    match unsafe { tree.as_mut() } {
        Some(t) => dyn_remove(&mut h, t, obj),
        None => false,
    }
}

/// # Safety
/// `vt` follows the module contract; `tree` is valid; `moved` points at `n` handles.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_dyn_specific_update(
    vt: *const HostVt,
    ctx: *mut c_void,
    tree: *mut DynTree,
    root: Handle,
    moved: *const Handle,
    n: usize,
) {
    let mut h = host!(vt, ctx);
    let moved: &[Handle] = if moved.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(moved, n) }
    };
    // SAFETY: caller contract.
    if let Some(t) = unsafe { tree.as_mut() } {
        dyn_specific_update(&mut h, t, root, moved);
    }
}

/// The node `obj` would be added to, starting at `start` (expands bounds on the way).
///
/// # Safety
/// `vt` follows the module contract; `tree` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bt_dyn_get_add_node(
    vt: *const HostVt,
    ctx: *mut c_void,
    tree: *const DynTree,
    start: Handle,
    obj: Handle,
) -> Handle {
    let mut h = host!(vt, ctx);
    // SAFETY: caller contract.
    match unsafe { tree.as_ref() } {
        Some(t) => get_add_node(&mut h, t, start, obj),
        None => 0,
    }
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<NodeBase>() == 72 && offset_of!(NodeBase, parent) == 64);
    assert!(offset_of!(NodeBase, prev_collision) == 56);
    assert!(size_of::<NodeDyn>() == 24 && size_of::<DynTree>() == 24 && size_of::<Bv>() == 52);
};
