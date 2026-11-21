//! LS: `cEnvironmentParticles` state and logic. The C++ class is a shell around one `EnvPod`
//! (`RustLs.h` mirrors the layout); GPU programs, the vertex buffer, the texture, clip entities and
//! the spot light are C++ objects reached through `ls_host`.

use super::ls_host::{self as lh, ClipInfo, SpotInfo, Var, Vb};
use super::particles::{
    ENV_MAX_ITER, EnvVecs, EnvVelIn, env_setup, env_update, env_velocity_vectors, rand_v3,
};
use crate::math::bv::Frustum;
use crate::math::{
    clamp, fma, len3, matrix_inverse, matrix_mul, matrix_rotate, matrix_scale, matrix_translate,
    mul3, sub3, v3,
};
use crate::types::{Color, Matrixf, Vector2f, Vector2l, Vector3f};
use std::ffi::c_void;

const _: () = assert!(size_of::<EnvPod>() == 1992);

const VAR_NAMES: [&str; 15] = [
    "avCameraUp",
    "avCameraRight",
    "avOffset",
    "avBoxSize",
    "avBoxWorldPos",
    "a_mtxRotation",
    "avColor",
    "avFadeProp",
    "avLightProbeLevels",
    "avDepthOfFieldParams",
    "a_mtxLightViewProj",
    "avLightPosScale",
    "avLightColor",
    "afLightFalloff",
    "afSpotFalloffPow",
];
const V_CAMERA_UP: i32 = 0;
const V_CAMERA_RIGHT: i32 = 1;
const V_OFFSET: i32 = 2;
const V_BOX_SIZE: i32 = 3;
const V_BOX_WORLD_POS: i32 = 4;
const V_ROTATION: i32 = 5;
const V_COLOR: i32 = 6;
const V_FADE_PROP: i32 = 7;
const V_PROBE_LEVELS: i32 = 8;
const V_DOF: i32 = 9;
const V_LIGHT_VIEW_PROJ: i32 = 10;
const V_LIGHT_POS_SCALE: i32 = 11;
const V_LIGHT_COLOR: i32 = 12;
const V_LIGHT_FALLOFF: i32 = 13;
const V_SPOT_FALLOFF: i32 = 14;
const V_PROBE_POS: i32 = 15;
const V_INTERSECT: i32 = V_PROBE_POS + 3;
const V_SUBTRACT: i32 = V_INTERSECT + 64;
const MAX_CLIPS: i32 = 64;

#[repr(C)]
pub struct EnvPod {
    res: usize,
    gfx: usize,
    vtx: usize,
    texture: usize,
    program: [usize; 2],
    spot: usize,
    program_key: [i32; 2],
    box_size: f32,
    particle_size: Vector2f,
    box_distance: f32,
    fade_in_start: f32,
    fade_in_end: f32,
    fade_out_start: f32,
    fade_out_end: f32,
    iteration_num: f32,
    iteration_int: i32,
    color: Color,
    brightness: f32,
    spot_mul: f32,
    gravity_vel: Vector3f,
    wind_vel: Vector3f,
    rotate_vel: Vector3f,
    gravity_speed_rand: f32,
    wind_speed_rand: f32,
    wind_dir_rand: f32,
    rotate_speed_rand: f32,
    iter_gravity: EnvVecs,
    iter_wind: EnvVecs,
    iter_rotate: EnvVecs,
    iter_offset: EnvVecs,
    iter_rotation: EnvVecs,
    visible: bool,
    affected_by_light: bool,
    rotate_both_dirs: bool,
    vel_dirty: bool,
    clip_active: bool,
}

struct Clip {
    ent: usize,
    subtractive: bool,
    mtx: Matrixf,
}

#[repr(C)]
struct EnvBox {
    pod: EnvPod,
    clips: Vec<Clip>,
}

fn one() -> Color {
    Color {
        r: 1.0,
        g: 1.0,
        b: 1.0,
        a: 1.0,
    }
}

impl EnvPod {
    fn vb(&self) -> Vb {
        Vb(self.vtx as *mut c_void)
    }

    fn update_velocity_vectors(&mut self) {
        if !self.vel_dirty {
            return;
        }
        self.vel_dirty = false;
        let input = EnvVelIn {
            gravity_vel: self.gravity_vel,
            wind_vel: self.wind_vel,
            rotate_vel: self.rotate_vel,
            gravity_speed_rand: self.gravity_speed_rand,
            wind_speed_rand: self.wind_speed_rand,
            wind_dir_rand: self.wind_dir_rand,
            rotate_speed_rand: self.rotate_speed_rand,
            rotate_both_dirs: self.rotate_both_dirs,
        };
        env_velocity_vectors(
            &input,
            &mut self.iter_gravity,
            &mut self.iter_wind,
            &mut self.iter_rotate,
        );
    }
}

fn clip_info(ent: usize) -> ClipInfo {
    let mut info = ClipInfo {
        active: false,
        bv: std::ptr::null_mut(),
    };
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        unsafe { (h.ent_clip)(ent as *mut c_void, &mut info) }
    }
    info
}

fn prog_set(prog: usize, id: i32, v: &[f32]) {
    if let Some(h) = lh::host() {
        // SAFETY: `v` holds the component count.
        unsafe { (h.prog_set)(prog as *mut c_void, id, v.len() as i32, v.as_ptr()) }
    }
}

fn prog_vec3(prog: usize, id: i32, v: Vector3f) {
    prog_set(prog, id, &[v.x, v.y, v.z]);
}

fn prog_destroy(gfx: usize, prog: usize) {
    if let (Some(h), true) = (lh::host(), prog != 0) {
        // SAFETY: table contract.
        unsafe { (h.prog_destroy)(gfx as *mut c_void, prog as *mut c_void) }
    }
}

fn prog_var(prog: usize, name: &str, id: i32) {
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        unsafe { (h.prog_var)(prog as *mut c_void, name.as_ptr(), name.len(), id) }
    }
}

fn spot_info(spot: usize) -> SpotInfo {
    let mut info = SpotInfo::default();
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        unsafe { (h.spot_info)(spot as *mut c_void, &mut info) }
    }
    info
}

fn light_level(pos: Vector3f) -> Option<f32> {
    let mut level = 0.0;
    let h = lh::host()?;
    // SAFETY: table contract.
    unsafe { (h.light_level)(&pos, &mut level) }.then_some(level)
}

fn has_light_level() -> bool {
    // SAFETY: table contract; a null position asks whether the callback is set.
    lh::host().is_some_and(|h| unsafe { (h.light_level)(std::ptr::null(), std::ptr::null_mut()) })
}

/// # Safety
/// `res` and `gfx` are the engine objects the C++ shell was given.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_new(res: usize, gfx: usize) -> *mut EnvPod {
    let zero = Vector3f::default();
    let mut pod = EnvPod {
        res,
        gfx,
        vtx: 0,
        texture: 0,
        program: [0; 2],
        spot: 0,
        program_key: [-1; 2],
        box_size: 1.0,
        particle_size: Vector2f { x: 1.0, y: 1.0 },
        box_distance: 2.0,
        fade_in_start: 0.2,
        fade_in_end: 1.0,
        fade_out_start: 10.0,
        fade_out_end: 20.0,
        iteration_num: 1.0,
        iteration_int: 1,
        color: one(),
        brightness: 1.0,
        spot_mul: 1.0,
        gravity_vel: v3(0.0, -0.1, 0.0),
        wind_vel: zero,
        rotate_vel: zero,
        gravity_speed_rand: 0.0,
        wind_speed_rand: 0.0,
        wind_dir_rand: 0.0,
        rotate_speed_rand: 0.0,
        iter_gravity: [zero; ENV_MAX_ITER],
        iter_wind: [zero; ENV_MAX_ITER],
        iter_rotate: [zero; ENV_MAX_ITER],
        iter_offset: [zero; ENV_MAX_ITER],
        iter_rotation: [zero; ENV_MAX_ITER],
        visible: true,
        affected_by_light: false,
        rotate_both_dirs: false,
        vel_dirty: true,
        clip_active: true,
    };
    let hundred = v3(100.0, 100.0, 100.0);
    for (offset, rotation) in pod.iter_offset.iter_mut().zip(pod.iter_rotation.iter_mut()) {
        *offset = rand_v3(zero, hundred);
        *rotation = rand_v3(zero, hundred);
    }
    Box::into_raw(Box::new(EnvBox {
        pod,
        clips: Vec::new(),
    }))
    .cast()
}

/// # Safety
/// `p` from `ohpl_ls_env_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_free(p: *mut EnvPod) {
    if p.is_null() {
        return;
    }
    // SAFETY: caller contract; `EnvPod` is the first field of `EnvBox`.
    let b = unsafe { Box::from_raw(p.cast::<EnvBox>()) };
    b.pod.vb().free();
    if let (Some(h), true) = (lh::host(), b.pod.texture != 0) {
        // SAFETY: table contract.
        unsafe { (h.tex_destroy)(b.pod.res as *mut c_void, b.pod.texture as *mut c_void) }
    }
    for prog in b.pod.program {
        prog_destroy(b.pod.gfx, prog);
    }
}

/// # Safety
/// `p`, `particle_size` and `sub_div` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_setup(
    p: *mut EnvPod,
    box_size: f32,
    num: i32,
    particle_size: *const Vector2f,
    sub_div: *const Vector2l,
    affected_by_light: bool,
    texture: usize,
) {
    // SAFETY: caller contract.
    let (pod, size, sub_div) = unsafe { (&mut *p, *particle_size, *sub_div) };
    pod.box_size = box_size;
    pod.particle_size = size;
    pod.affected_by_light = affected_by_light;
    pod.texture = texture;

    pod.vb().free();
    let n = usize::try_from(num).unwrap_or(0);
    let vb = Vb::create(
        pod.gfx as *mut c_void,
        lh::USAGE_STATIC,
        num.wrapping_mul(4),
        num.wrapping_mul(6),
    );
    pod.vtx = vb.0 as usize;
    vb.elem(lh::E_POS, 4);
    vb.elem(lh::E_COLOR0, 4);
    vb.elem(lh::E_TEX0, 3);
    vb.elem(lh::E_TEX1, 3);

    let (mut pos, mut uv0, mut uv1) = (vec![0.0; n * 16], vec![0.0; n * 12], vec![0.0; n * 12]);
    env_setup(box_size, size, sub_div, &mut pos, &mut uv0, &mut uv1);
    let quads = pos
        .chunks_exact(16)
        .zip(uv0.chunks_exact(12))
        .zip(uv1.chunks_exact(12));
    for (i, ((p, u0), u1)) in quads.enumerate() {
        let verts = p
            .chunks_exact(4)
            .zip(u0.chunks_exact(3))
            .zip(u1.chunks_exact(3));
        for ((pv, a), b) in verts {
            vb.add(lh::E_POS, pv);
            vb.color(lh::E_COLOR0, one());
            vb.add(lh::E_TEX0, a);
            vb.add(lh::E_TEX1, b);
        }
        for j in [0, 1, 2, 2, 3, 0] {
            vb.index(i32::try_from(i * 4).unwrap_or(0) + j);
        }
    }
    vb.compile(0);
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_set_iteration_num(p: *mut EnvPod, num: f32) {
    // SAFETY: caller contract.
    let pod = unsafe { &mut *p };
    pod.iteration_num = clamp(num, 0.0, ENV_MAX_ITER as f32);
    pod.iteration_int = pod.iteration_num.ceil() as i32;
    pod.vel_dirty = true;
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_update(p: *mut EnvPod, dt: f32) {
    // SAFETY: caller contract.
    let b = unsafe { &mut *p.cast::<EnvBox>() };
    let pod = &mut b.pod;
    pod.update_velocity_vectors();
    env_update(
        usize::try_from(pod.iteration_int).unwrap_or(0),
        dt,
        [&pod.iter_gravity, &pod.iter_wind, &pod.iter_rotate],
        &mut pod.iter_offset,
        &mut pod.iter_rotation,
    );

    let shrink = fma(
        pod.particle_size.x,
        pod.particle_size.x,
        pod.particle_size.y * pod.particle_size.y,
    )
    .sqrt()
        * 0.5;
    for clip in &mut b.clips {
        let info = clip_info(clip.ent);
        if !info.active {
            clip.mtx = matrix_scale(v3(10000.0, 10000.0, 10000.0));
            continue;
        }
        // SAFETY: the host returns the live bounding volume of the clip entity.
        let bv = unsafe { &*info.bv };
        let (min, max) = (bv.local_min, bv.local_max);
        let d = mul3(sub3(max, min), 0.5);
        let half =
            [d.x - shrink, d.y - shrink, d.z - shrink].map(|c| if c > 0.0001 { c } else { 0.0001 });
        let center = mul3(v3(min.x + max.x, min.y + max.y, min.z + max.z), 0.5);
        let mtx_box = matrix_mul(
            &matrix_translate(center),
            &matrix_scale(v3(half[0], half[1], half[2])),
        );
        clip.mtx = matrix_inverse(&matrix_mul(&bv.mtx, &mtx_box));
    }
}

/// # Safety
/// `p` and `frustum` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_is_visible(p: *mut EnvPod, frustum: *const Frustum) -> bool {
    // SAFETY: caller contract.
    let (b, frustum) = unsafe { (&*p.cast::<EnvBox>(), &*frustum) };
    let pod = &b.pod;
    if !pod.visible || pod.texture == 0 || pod.vtx == 0 {
        return false;
    }
    if !pod.clip_active {
        return true;
    }
    let half = pod.box_size * 0.5;
    let max_dist = if pod.fade_out_end < half {
        pod.fade_out_end
    } else {
        half
    };
    let mut intersect = 0;
    for clip in &b.clips {
        let info = clip_info(clip.ent);
        if clip.subtractive || !info.active {
            continue;
        }
        intersect += 1;
        // SAFETY: the host returns the live bounding volume of the clip entity.
        let bv = unsafe { &mut *info.bv };
        bv.update_size();
        let (min, max) = (bv.world_min, bv.world_max);
        let o = frustum.origin;
        let closest = v3(
            clamp(o.x, min.x, max.x),
            clamp(o.y, min.y, max.y),
            clamp(o.z, min.z, max.z),
        );
        if len3(sub3(closest, o)) < max_dist && frustum.collide_bv(bv) != 1 {
            return true;
        }
    }
    intersect == 0
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_add_clip_area(p: *mut EnvPod, ent: usize, subtractive: bool) {
    if ent == 0 {
        return;
    }
    // SAFETY: caller contract.
    let b = unsafe { &mut *p.cast::<EnvBox>() };
    b.clips.retain(|c| c.ent != ent);
    b.clips.push(Clip {
        ent,
        subtractive,
        mtx: crate::math::IDENTITY,
    });
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_remove_clip_area(p: *mut EnvPod, ent: usize) {
    // SAFETY: caller contract.
    let b = unsafe { &mut *p.cast::<EnvBox>() };
    b.clips.retain(|c| c.ent != ent);
}

/// `GetProgram`: the shader variant for the current clip and light setup, built on a key change.
///
/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_program(p: *mut EnvPod, dof: bool) -> usize {
    // SAFETY: caller contract.
    let b = unsafe { &mut *p.cast::<EnvBox>() };
    let pod = &mut b.pod;
    let (mut intersect, mut subtract) = (0, 0);
    if pod.clip_active {
        for clip in &b.clips {
            if clip.subtractive {
                subtract += 1;
            } else {
                intersect += 1;
            }
        }
    }
    let (intersect, subtract) = (intersect.min(MAX_CLIPS), subtract.min(MAX_CLIPS));
    let probes = pod.affected_by_light && has_light_level();

    let key = i32::from(probes)
        | (if pod.spot != 0 { 2 } else { 0 })
        | (intersect << 2)
        | (subtract << 9);
    let d = usize::from(dof);
    let (Some(&old_key), Some(&old_prog)) = (pod.program_key.get(d), pod.program.get(d)) else {
        return 0;
    };
    if old_key == key {
        return old_prog;
    }
    prog_destroy(pod.gfx, old_prog);

    let flag = |name: &'static str| Var {
        name: name.as_ptr(),
        len: name.len(),
        value: 0,
        has_value: false,
    };
    let valued = |name: &'static str, value: i32| Var {
        name: name.as_ptr(),
        len: name.len(),
        value,
        has_value: true,
    };
    let mut vars = vec![flag("UseDiffuse"), flag("UseUv"), flag("UseColor")];
    if probes {
        vars.push(flag("UseLightProbes"));
    }
    if dof {
        vars.push(flag("UseDepthOfField"));
    }
    if pod.spot != 0 {
        vars.push(flag("UseSpotLight"));
    }
    if intersect + subtract > 0 {
        vars.push(flag("UseClipping"));
        if intersect > 0 {
            vars.push(flag("IntersectClip"));
            vars.push(valued("kNumIntersect", intersect));
        }
        if subtract > 0 {
            vars.push(flag("SubtractClip"));
            vars.push(valued("kNumSubtract", subtract));
        }
    }
    let name = format!("EnvironmentParticles_{}", key * 2 + i32::from(dof));
    let prog = lh::host().map_or(0, |h| {
        // SAFETY: table contract; `vars` and `name` outlive the call.
        unsafe {
            (h.prog_create)(
                pod.gfx as *mut c_void,
                name.as_ptr(),
                name.len(),
                vars.as_ptr(),
                vars.len(),
            ) as usize
        }
    });
    if let (Some(slot), Some(slot_key)) = (pod.program.get_mut(d), pod.program_key.get_mut(d)) {
        *slot = prog;
        *slot_key = key;
    }
    if prog == 0 {
        return 0;
    }

    for (i, name) in VAR_NAMES.iter().enumerate() {
        prog_var(prog, name, i as i32);
    }
    for i in 0..3 {
        prog_var(
            prog,
            &format!("avLightProbePositions[{i}]"),
            V_PROBE_POS + i,
        );
    }
    for i in 0..intersect {
        prog_var(prog, &format!("a_mtxIntersectClip[{i}]"), V_INTERSECT + i);
    }
    for i in 0..subtract {
        prog_var(prog, &format!("a_mtxSubtractClip[{i}]"), V_SUBTRACT + i);
    }
    prog
}

/// `SetupProgramBase`.
///
/// # Safety
/// `p`, `frustum` valid; `dof` holds four floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_setup_base(
    p: *mut EnvPod,
    prog: usize,
    frustum: *const Frustum,
    dof: *const f32,
) {
    // SAFETY: caller contract.
    let (b, frustum, dof) = unsafe {
        (
            &*p.cast::<EnvBox>(),
            &*frustum,
            std::slice::from_raw_parts(dof, 4),
        )
    };
    let pod = &b.pod;
    let view = &frustum.view;
    let origin = frustum.origin;
    let [right, up, fwd, _] = view.m.map(|r| v3(r[0], r[1], r[2]));
    prog_vec3(prog, V_CAMERA_UP, up);
    prog_vec3(prog, V_CAMERA_RIGHT, right);
    let half = pod.box_size * 0.5;
    prog_vec3(
        prog,
        V_BOX_WORLD_POS,
        v3(
            fma(fwd.x, pod.box_distance, origin.x) - half,
            fma(fwd.y, pod.box_distance, origin.y) - half,
            fma(fwd.z, pod.box_distance, origin.z) - half,
        ),
    );
    prog_vec3(
        prog,
        V_BOX_SIZE,
        v3(pod.box_size, pod.box_size, pod.box_size),
    );
    let c = pod.color;
    let b2 = pod.brightness;
    prog_set(
        prog,
        V_COLOR,
        &[c.r * c.r * b2, c.g * c.g * b2, c.b * c.b * b2, c.a],
    );
    prog_set(
        prog,
        V_FADE_PROP,
        &[
            pod.fade_in_start,
            pod.fade_in_end - pod.fade_in_start,
            pod.fade_out_start,
            pod.fade_out_end - pod.fade_out_start,
        ],
    );
    prog_set(prog, V_DOF, dof);

    if pod.affected_by_light && has_light_level() {
        let step = |k: f32| {
            v3(
                fma(-fwd.x, k, origin.x),
                fma(-fwd.y, k, origin.y),
                fma(-fwd.z, k, origin.z),
            )
        };
        let probes = [origin, step(pod.box_size * 0.3), step(pod.box_size * 0.75)];
        let mut levels = [0.0; 3];
        for (i, (pos, level)) in probes.iter().zip(levels.iter_mut()).enumerate() {
            prog_vec3(prog, V_PROBE_POS + i as i32, *pos);
            *level = light_level(*pos).unwrap_or(0.0);
        }
        prog_set(prog, V_PROBE_LEVELS, &levels);
    }

    if pod.spot != 0 {
        let s = spot_info(pod.spot);
        let mul = s.brightness * pod.spot_mul * if s.visible { 1.0 } else { 0.0 };
        prog_set(prog, V_LIGHT_VIEW_PROJ, s.view_proj.m.as_flattened());
        prog_set(
            prog,
            V_LIGHT_POS_SCALE,
            &[s.pos.x, s.pos.y, s.pos.z, 1.0 / s.radius],
        );
        prog_set(
            prog,
            V_LIGHT_COLOR,
            &[
                s.color.r * s.color.r * mul,
                s.color.g * s.color.g * mul,
                s.color.b * s.color.b * mul,
                s.color.a,
            ],
        );
        prog_set(prog, V_LIGHT_FALLOFF, &[s.falloff_pow * 2.0]);
        prog_set(prog, V_SPOT_FALLOFF, &[s.spot_falloff_pow * 2.0]);
    }

    if pod.clip_active {
        let (mut intersect, mut subtract) = (0, 0);
        for clip in &b.clips {
            let id = if clip.subtractive {
                subtract += 1;
                V_SUBTRACT + subtract - 1
            } else {
                intersect += 1;
                V_INTERSECT + intersect - 1
            };
            prog_set(prog, id, clip.mtx.m.as_flattened());
        }
    }
}

/// `SetupProgramIteration`.
///
/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_env_setup_iteration(p: *mut EnvPod, prog: usize, idx: i32) {
    // SAFETY: caller contract.
    let pod = unsafe { &*p };
    let Ok(i) = usize::try_from(idx) else { return };
    let (Some(offset), Some(rotation)) = (pod.iter_offset.get(i), pod.iter_rotation.get(i)) else {
        return;
    };
    prog_vec3(prog, V_OFFSET, *offset);
    prog_set(
        prog,
        V_ROTATION,
        matrix_rotate(*rotation, 0).m.as_flattened(),
    );

    let frac = pod.iteration_num - pod.iteration_num.floor();
    if idx == pod.iteration_int - 1 && frac > 0.001 {
        let (c, b) = (pod.color, pod.brightness);
        prog_set(
            prog,
            V_COLOR,
            &[c.r * c.r * b, c.g * c.g * b, c.b * c.b * b, frac],
        );
    }
}
