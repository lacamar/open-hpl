//! C ABI of the Newton glue logic, mirrored in `HPL2/core/include/impl/RustNewton.h`.
//! Every pointer is non-null and valid for the call unless noted; outputs do not alias inputs.

use super::newton_body::{self, BuoyIn, BuoyOut, SubShape};
use super::newton_material::SurfaceCalls;
use super::newton_material::{Contact, ContactAgg, GateBody, MatProps, PairParams};
use crate::types::Vector3f;

/// Returns false when the body gets no buoyancy force (`out` untouched).
///
/// # Safety
/// `input` and `out` are valid; `subs` points to `n` sub shapes whose `points` stay valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_newton_buoyancy(
    input: *const BuoyIn,
    subs: *const SubShape,
    n: usize,
    out: *mut BuoyOut,
) -> bool {
    let slice = if subs.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(subs, n) }
    };
    // SAFETY: caller contract.
    let (Some(input), Some(out)) = (unsafe { input.as_ref() }, unsafe { out.as_mut() }) else {
        return false;
    };
    match newton_body::hpl3_buoyancy(input, slice) {
        Some(o) => {
            *out = o;
            true
        }
        None => false,
    }
}

/// Writes up to `cap` points (at most 512 are ever produced) and returns the
/// count; `radius` receives the sphere radius.
///
/// # Safety
/// `size` and `radius` are valid; `out` has room for `cap` points.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_newton_buoyancy_points(
    shape: i32,
    size: *const Vector3f,
    out: *mut Vector3f,
    cap: usize,
    radius: *mut f32,
) -> usize {
    // SAFETY: caller contract.
    let (Some(size), Some(radius)) = (unsafe { size.as_ref() }, unsafe { radius.as_mut() }) else {
        return 0;
    };
    let mut pts = Vec::new();
    *radius = newton_body::buoyancy_points(shape, *size, &mut pts);
    let n = pts.len().min(cap);
    if !out.is_null() {
        // SAFETY: caller contract: `out` has room for `cap >= n` points.
        unsafe { std::ptr::copy_nonoverlapping(pts.as_ptr(), out, n) };
    }
    n
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Contact>() == 48 && size_of::<ContactAgg>() == 44);
    assert!(size_of::<BuoyOut>() == 56 && size_of::<SubShape>() == 24);
    assert!(size_of::<MatProps>() == 20 && size_of::<PairParams>() == 12);
    assert!(size_of::<GateBody>() == 6 && size_of::<SurfaceCalls>() == 4);
};
