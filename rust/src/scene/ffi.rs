//! C ABI for `scene/mod.rs`, declared in `HPL2/core/include/scene/RustRenderableContainer.h`.
//!
//! Vectors cross as pointers (`cVector3` has a user-provided copy constructor). Pointers are
//! non-null, valid and aligned for the call.

use super::{Node, Object, Params, Tree, compile};
use std::ptr;
use std::slice;

/// Result is freed with `ohpl_rc_boxtree_free`.
///
/// # Safety
/// `objects` points at `count` objects (or is null with `count` 0); `params` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rc_boxtree_compile(
    objects: *const Object,
    count: usize,
    params: *const Params,
) -> *mut Tree {
    let objs: &[Object] = if objects.is_null() {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { slice::from_raw_parts(objects, count) }
    };
    // SAFETY: caller contract.
    let tree = compile(objs, unsafe { &*params });
    Box::into_raw(Box::new(tree))
}

/// # Safety
/// `tree` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rc_boxtree_node_count(tree: *const Tree) -> usize {
    // SAFETY: caller contract.
    unsafe { tree.as_ref() }.map_or(0, |t| t.nodes.len())
}

/// Valid until `ohpl_rc_boxtree_free`.
///
/// # Safety
/// `tree` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rc_boxtree_nodes(tree: *const Tree) -> *const Node {
    // SAFETY: caller contract.
    unsafe { tree.as_ref() }.map_or(ptr::null(), |t| t.nodes.as_ptr())
}

/// Valid until `ohpl_rc_boxtree_free`; node `n` owns `[first_object, first_object + object_count)`.
///
/// # Safety
/// `tree` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rc_boxtree_objects(tree: *const Tree) -> *const u32 {
    // SAFETY: caller contract.
    unsafe { tree.as_ref() }.map_or(ptr::null(), |t| t.objects.as_ptr())
}

/// # Safety
/// `tree` from `ohpl_rc_boxtree_compile`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rc_boxtree_free(tree: *mut Tree) {
    if !tree.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(tree) });
    }
}
