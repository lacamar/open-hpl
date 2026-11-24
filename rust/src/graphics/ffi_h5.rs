//! C ABI of the H5 graphics state, declared in `graphics/RustGraphicsH5.h`.

use std::ffi::c_void;

use super::atlas::Atlas;
use crate::math::Rect2l;
use crate::types::{Vector2f, Vector3l};

#[repr(C)]
pub struct OhplH5Surface {
    size: Vector3l,
    format: i32,
    bytes_per_pixel: i32,
}

#[repr(C)]
pub struct OhplH5Added {
    rect: Rect2l,
    found_empty: bool,
}

macro_rules! atlas_flags {
    ($($get:ident $set:ident $f:ident;)*) => {$(
        /// # Safety
        /// `a` is a live atlas.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $get(a: *const c_void) -> bool {
            // SAFETY: caller contract.
            unsafe { a.cast::<Atlas>().as_ref() }.is_some_and(|a| a.$f)
        }

        /// # Safety
        /// `a` is a live atlas.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $set(a: *mut c_void, v: bool) {
            // SAFETY: caller contract.
            if let Some(a) = unsafe { a.cast::<Atlas>().as_mut() } {
                a.$f = v;
            }
        }
    )*};
}

atlas_flags! {
    ohpl_h5_atlas_full ohpl_h5_atlas_set_full full;
    ohpl_h5_atlas_updated ohpl_h5_atlas_set_updated updated;
    ohpl_h5_atlas_need_reorg ohpl_h5_atlas_set_need_reorg need_reorg;
    ohpl_h5_atlas_locked ohpl_h5_atlas_set_locked locked;
}

const _: () = {
    assert!(std::mem::size_of::<OhplH5Surface>() == 20);
    assert!(std::mem::size_of::<OhplH5Added>() == 20);
    assert!(std::mem::size_of::<Vector2f>() == 8);
};
