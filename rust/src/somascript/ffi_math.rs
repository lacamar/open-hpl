//! C ABI for `math.rs`. Vectors are passed as pointers to their float members.

use super::math;
use super::math::Pid;

macro_rules! scalar {
    ($name:ident, $f:path; ($($a:ident),*)) => {
        #[unsafe(no_mangle)]
        pub extern "C" fn $name($($a: f32),*) -> f32 {
            $f($($a),*)
        }
    };
}

scalar!(ohpl_gs6_sigmoid_curve, math::sigmoid_curve; (x));
scalar!(ohpl_gs6_interpolate_sigmoid, math::interpolate_sigmoid; (a, b, t));
scalar!(ohpl_gs6_interpolate_linear, math::interpolate_linear; (a, b, t));
scalar!(ohpl_gs6_interpolate_cosine, math::interpolate_cosine; (a, b, t));
scalar!(ohpl_gs6_interpolate_cubic, math::interpolate_cubic; (x0, x1, x2, x3, t));
scalar!(ohpl_gs6_noise_perlin_1d, math::noise_perlin_1d; (x));
scalar!(ohpl_gs6_round, math::round; (x));

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs6_get_bit(n: i32) -> i32 {
    math::get_bit(n)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs6_set_bit_flag(flags: i32, bit: i32, set: bool) -> i32 {
    math::set_bit_flag(flags, bit, set)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs6_get_bit_flag(flags: i32, bit: i32) -> bool {
    math::get_bit_flag(flags, bit)
}

/// # Safety
/// `p` points to `N` floats.
unsafe fn arr<const N: usize>(p: *const f32) -> [f32; N] {
    // SAFETY: caller contract.
    unsafe { p.cast::<[f32; N]>().read_unaligned() }
}

/// # Safety
/// `p` points to room for `N` floats.
unsafe fn put<const N: usize>(p: *mut f32, v: [f32; N]) {
    // SAFETY: caller contract.
    unsafe { p.cast::<[f32; N]>().write_unaligned(v) }
}

/// # Safety
/// Both point to two floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_vector2_dot(a: *const f32, b: *const f32) -> f32 {
    // SAFETY: caller contract.
    unsafe { math::vector2_dot(arr(a), arr(b)) }
}

/// # Safety
/// `v` points to two floats, `out` has room for two.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_vector2_rotate(v: *const f32, angle: f32, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe { put(out, math::vector2_rotate(arr(v), angle)) }
}

/// # Safety
/// `v` points to three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_vector3_max_element(v: *const f32) -> f32 {
    // SAFETY: caller contract.
    unsafe { math::vector3_max_element(arr(v)) }
}

/// # Safety
/// Inputs point to two floats each, `out` has room for two.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_catmull_rom2(
    p0: *const f32,
    p1: *const f32,
    p2: *const f32,
    p3: *const f32,
    t: f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        put(
            out,
            math::catmull_rom::<2>(arr(p0), arr(p1), arr(p2), arr(p3), t),
        )
    }
}

/// # Safety
/// Inputs point to three floats each, `out` has room for three.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_catmull_rom3(
    p0: *const f32,
    p1: *const f32,
    p2: *const f32,
    p3: *const f32,
    t: f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        put(
            out,
            math::catmull_rom::<3>(arr(p0), arr(p1), arr(p2), arr(p3), t),
        )
    }
}

/// # Safety
/// `seed` points to two floats, `out` has room for three.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_sphere_surface_point(
    seed: *const f32,
    radius: f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe { put(out, math::sphere_surface_point(arr(seed), radius)) }
}

/// # Safety
/// `out` has room for three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_random_sphere_surface_point(radius: f32, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe { put(out, math::random_sphere_surface_point(radius)) }
}

/// # Safety
/// `out` has room for two floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_random_circle_edge_point(radius: f32, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe { put(out, math::random_circle_edge_point(radius)) }
}

/// # Safety
/// Inputs point to three floats, `out` has room for three.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_project_on_plane(n: *const f32, v: *const f32, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe { put(out, math::project_on_plane(arr(n), arr(v))) }
}

/// # Safety
/// `c` points to three floats, `out` has room for six bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_rgb_to_hex(c: *const f32, out: *mut u8) {
    // SAFETY: caller contract.
    unsafe {
        out.cast::<[u8; 6]>()
            .write_unaligned(math::rgb_to_hex(arr(c)))
    }
}

/// # Safety
/// Inputs point to three floats each.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_aabb_sphere_collision(
    mn: *const f32,
    mx: *const f32,
    c: *const f32,
    r: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { math::aabb_sphere_collision(arr(mn), arr(mx), arr(c), r) }
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Pid<1>>() == 12 + 12 + 3 * 4 + 64 * 4 + 64 * 4);
    assert!(size_of::<Pid<3>>() == 12 + 12 + 3 * 12 + 64 * 12 + 64 * 4);
};

macro_rules! pid_api {
    ($n:literal, $set:ident, $reset:ident, $out:ident) => {
        /// # Safety
        /// `obj` is a script struct block holding a `Pid` at offset 16.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $set(obj: *mut u8, num: i32) {
            // SAFETY: caller contract.
            unsafe { (*obj.add(16).cast::<Pid<$n>>()).set_error_num(num) }
        }

        /// # Safety
        /// `obj` is a script struct block holding a `Pid` at offset 16.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $reset(obj: *mut u8) {
            // SAFETY: caller contract.
            unsafe { (*obj.add(16).cast::<Pid<$n>>()).reset() }
        }

        /// # Safety
        /// `obj` as above; `error` and `out` point to $n floats.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $out(obj: *mut u8, error: *const f32, dt: f32, out: *mut f32) {
            // SAFETY: caller contract.
            unsafe {
                let r = (*obj.add(16).cast::<Pid<$n>>()).output(arr(error), dt);
                put(out, r);
            }
        }
    };
}

pid_api!(
    1,
    ohpl_gs6_pid1_set_error_num,
    ohpl_gs6_pid1_reset,
    ohpl_gs6_pid1_output
);
pid_api!(
    3,
    ohpl_gs6_pid3_set_error_num,
    ohpl_gs6_pid3_reset,
    ohpl_gs6_pid3_output
);
