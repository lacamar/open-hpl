use super::ffi::{HostVt, VarsVt};
use super::mt_host::{HostCtx, KV, MtHost, P};
use super::pcm::Pcm;
use super::types::{Globals, ObjView, RenderView, Uniform};
use core::sync::atomic::{AtomicUsize, Ordering};
use std::collections::BTreeMap;

static PROGS: AtomicUsize = AtomicUsize::new(0);
static SHADERS: AtomicUsize = AtomicUsize::new(0);

struct Prog {
    user: u32,
    sh: [P; 2],
}

unsafe extern "C" fn prog_create(_: P, _: *const u8, _: usize) -> P {
    PROGS.fetch_add(1, Ordering::Relaxed);
    Box::into_raw(Box::new(Prog {
        user: 0,
        sh: [core::ptr::null_mut(); 2],
    }))
    .cast()
}
unsafe extern "C" fn prog_link(_: P) -> bool {
    true
}
unsafe extern "C" fn prog_delete(p: P) {
    PROGS.fetch_sub(1, Ordering::Relaxed);
    drop(unsafe { Box::from_raw(p.cast::<Prog>()) });
}
unsafe extern "C" fn prog_set_shaders(p: P, v: P, f: P) {
    unsafe { (*p.cast::<Prog>()).sh = [v, f] };
}
unsafe extern "C" fn prog_shader(p: P, i: i32) -> P {
    unsafe { (*p.cast::<Prog>()).sh[usize::from(i != 0)] }
}
unsafe extern "C" fn prog_set_user_id(p: P, id: u32) {
    unsafe { (*p.cast::<Prog>()).user = id };
}
unsafe extern "C" fn prog_user_id(p: P) -> u32 {
    unsafe { (*p.cast::<Prog>()).user }
}
unsafe extern "C" fn shader_create(
    _: P,
    _: *const u8,
    _: usize,
    _: i32,
    _: *const KV,
    _: usize,
    _: bool,
) -> P {
    SHADERS.fetch_add(1, Ordering::Relaxed);
    Box::into_raw(Box::new(0u32)).cast()
}
unsafe extern "C" fn shader_destroy(_: P, s: P) {
    SHADERS.fetch_sub(1, Ordering::Relaxed);
    drop(unsafe { Box::from_raw(s.cast::<u32>()) });
}
unsafe extern "C" fn shader_set_user_id(s: P, id: u32) {
    unsafe { *s.cast::<u32>() = id };
}
unsafe extern "C" fn shader_user_id(s: P) -> u32 {
    unsafe { *s.cast::<u32>() }
}
unsafe extern "C" fn name(_: P, p: *mut *const u8, n: *mut usize) {
    unsafe {
        *p = core::ptr::null();
        *n = 0;
    }
}
unsafe extern "C" fn nop_p(_: P, _: P) {}
unsafe extern "C" fn nop_pp(_: P, _: *const u8, _: usize, _: i32) {}
unsafe extern "C" fn nop_apply(_: P, _: *const Uniform, _: usize) {}
unsafe extern "C" fn nop_err(_: *const u8, _: usize) {}
unsafe extern "C" fn nop_globals(_: P, _: f32, _: *mut Globals) {}
unsafe extern "C" fn nop_rv(_: P, _: *mut RenderView) {}
unsafe extern "C" fn nop_ov(_: P, _: *mut ObjView) -> P {
    core::ptr::null_mut()
}
unsafe extern "C" fn nop_hvt(_: *mut HostCtx, _: *mut HostVt) {}
unsafe extern "C" fn nop_vvt(_: P, _: *mut VarsVt) {}
unsafe extern "C" fn null_i(_: i32) -> P {
    core::ptr::null_mut()
}
unsafe extern "C" fn null_ip(_: i32, _: P) -> P {
    core::ptr::null_mut()
}
unsafe extern "C" fn null_v() -> P {
    core::ptr::null_mut()
}
unsafe extern "C" fn tex_create(_: P, _: *const u8, _: usize, _: bool) -> P {
    core::ptr::null_mut()
}
unsafe extern "C" fn null_p(_: P) -> P {
    core::ptr::null_mut()
}
unsafe extern "C" fn tex_info(_: P, _: *mut i32, _: *mut i32, _: *mut bool) {}
unsafe extern "C" fn special(_: P, _: i32) -> P {
    core::ptr::null_mut()
}
unsafe extern "C" fn exists(_: P, _: *const u8, _: usize) -> bool {
    false
}

static HOST: MtHost = MtHost {
    prog_create,
    prog_link,
    prog_delete,
    prog_set_shaders,
    prog_set_owned: nop_p,
    prog_shader,
    prog_set_user_id,
    prog_user_id,
    prog_name: name,
    prog_var_id: nop_pp,
    prog_apply: nop_apply,
    shader_create,
    shader_destroy,
    shader_set_user_id,
    shader_user_id,
    shader_name: name,
    error: nop_err,
    globals: nop_globals,
    render_view: nop_rv,
    obj_view: nop_ov,
    host_vt: nop_hvt,
    vars_vt: nop_vvt,
    vars_new: null_i,
    vars_pod: null_ip,
    vars_object_new: null_v,
    tex_create_2d: tex_create,
    tex_destroy: nop_p,
    tex_white_new: null_p,
    tex_white_free: nop_p,
    tex_info,
    special_tex: special,
    file_exists: exists,
};

fn counts() -> (usize, usize) {
    (
        PROGS.load(Ordering::Relaxed),
        SHADERS.load(Ordering::Relaxed),
    )
}

#[test]
fn programs_share_shaders_and_free_in_order() {
    let null = core::ptr::null_mut();
    let mut m = Pcm::new(&HOST, b"T", null, null, 1);
    let feats = vec![(3u32, b"A".to_vec()), (2u32, b"B".to_vec())];
    m.setup_mode(0, b"m", b"v", b"f", &feats, &BTreeMap::new());
    assert_eq!(m.combination_num(0), 4);
    let p0 = m.generate_program(0, 0);
    assert_eq!(m.generate_program(0, 0), p0);
    assert_eq!(counts(), (1, 2));
    // feature B reaches the fragment stage only: the vertex shader is shared
    let p2 = m.generate_program(0, 2);
    assert_ne!(p2, p0);
    assert_eq!(counts(), (2, 3));
    m.destroy_generated_program(0, p2);
    assert_eq!(counts(), (1, 2));
    m.destroy_shaders_and_programs();
    assert_eq!(counts(), (0, 0));
}
