//! `iRenderable` state: the plain data every renderable carries, owned here and read in place by
//! the C++ class (`graphics/RustRenderable.h` mirrors the layout) and by the render list.

use std::ffi::c_void;
use std::ptr::null_mut;

use crate::types::{Color, Matrixf, Vector3f};

unsafe extern "C" {
    fn ohpl_math_rand_rectf(min: f32, max: f32) -> f32;
}

#[repr(C)]
pub struct Rnd {
    pub inv_model: Matrixf,
    pub previous: Matrixf,
    pub model: *mut Matrixf,
    pub callback: *mut c_void,
    pub is_one_sided: bool,
    pub one_sided_normal: Vector3f,
    pub last_matrix_count: i32,
    pub render_flags: u32,
    pub visible: bool,
    pub is_static: bool,
    pub force_shadow: bool,
    pub placement: i32,
    pub render_frame_count: i32,
    pub calc_scale_count: i32,
    pub calc_scale: Vector3f,
    pub view_space_z: f32,
    pub illumination: f32,
    pub illumination_color: Color,
    pub color_mul: Color,
    pub coverage: f32,
    pub liquid: f32,
    pub culled_by_distance: bool,
    pub culled_by_fog: bool,
    pub distance_cull_fade: f32,
    pub container_node: *mut c_void,
    pub user_data: *mut c_void,
}

const _: () = assert!(size_of::<Rnd>() == 272);

const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

impl Rnd {
    /// The `iRenderable` constructor; the one `rand()` draw keeps the engine's stream order.
    pub fn new() -> Self {
        // SAFETY: exported by crate::math, plain floats.
        let liquid = -unsafe { ohpl_math_rand_rectf(1.1, 300.0) };
        Self {
            inv_model: Matrixf::default(),
            previous: Matrixf::default(),
            model: null_mut(),
            callback: null_mut(),
            is_one_sided: false,
            one_sided_normal: Vector3f::default(),
            last_matrix_count: -1,
            render_flags: 0x6,
            visible: true,
            is_static: false,
            force_shadow: false,
            placement: 0,
            render_frame_count: -1,
            calc_scale_count: -1,
            calc_scale: Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
            view_space_z: 0.0,
            illumination: 1.0,
            illumination_color: WHITE,
            color_mul: WHITE,
            coverage: 1.0,
            liquid,
            culled_by_distance: true,
            culled_by_fog: true,
            distance_cull_fade: 1.0,
            container_node: null_mut(),
            user_data: null_mut(),
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rnd_new() -> *mut Rnd {
    Box::into_raw(Box::new(Rnd::new()))
}

/// # Safety
/// `r` came from `ohpl_rnd_new` and is freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rnd_free(r: *mut Rnd) {
    if !r.is_null() {
        // SAFETY: by the caller's contract.
        drop(unsafe { Box::from_raw(r) });
    }
}
