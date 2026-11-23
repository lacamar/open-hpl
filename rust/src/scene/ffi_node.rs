//! C ABI for `node.rs` / `camera.rs`, declared in `HPL2/core/include/scene/RustNode.h`.
//!
//! Vectors cross as pointers (`cVector3` has a user-provided copy constructor). Pointers are
//! non-null, valid and aligned for the call; outputs do not alias inputs.

use super::node;
use crate::types::{Matrixf, Vector3f};

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_inverse(m: *const Matrixf, out: *mut Matrixf) {
    // SAFETY: caller contract.
    unsafe { *out = node::inverse(&*m) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_calc_scale(m: *const Matrixf, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { *out = node::calc_scale(&*m) }
}
