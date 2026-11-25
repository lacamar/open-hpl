//! C ABI for `h11.rs`, declared in `HPL2/core/include/scene/RustSceneH11.h`.
//!
//! Pointers are non-null, valid and aligned for the call.

use super::h11::{self, Caster, MeshCtl, SpotCtl};
use crate::types::Matrixf;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<MeshCtl>() == 32 && offset_of!(MeshCtl, physics) == 20);
    assert!(size_of::<SpotCtl>() == 16 && offset_of!(SpotCtl, projection_updated) == 12);
    assert!(size_of::<Caster>() == 16 && offset_of!(Caster, is_static) == 12);
};

fn count(n: i32) -> usize {
    usize::try_from(n).unwrap_or(0)
}

/// # Safety
/// `p` valid for `n` reads when `n > 0`.
unsafe fn slice<'a, T>(p: *const T, n: i32) -> &'a [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts(p, n) },
        _ => &[],
    }
}

/// # Safety
/// `vals` holds `nvals` floats, `locals`/`unit`/`out` hold `n` entries; `out` does not alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_rest_pose_from_map(
    vals: *const f32,
    nvals: i32,
    locals: *const Matrixf,
    unit: *const f32,
    out: *mut Matrixf,
    n: i32,
) -> bool {
    let len = count(n);
    let out = if out.is_null() || len == 0 {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(out, len) }
    };
    // SAFETY: caller contract.
    let (vals, locals, unit) = unsafe { (slice(vals, nvals), slice(locals, n), slice(unit, n)) };
    h11::rest_pose_from_map(vals, locals, unit, out)
}
