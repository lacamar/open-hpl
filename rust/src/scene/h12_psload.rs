//! `cParticleEmitterData_UserData::LoadFromElement`: attribute strings to emitter parameters.
//! Attributes arrive through a getter callback so a missing attribute stays distinct from an
//! empty one, exactly like `cXmlElement::GetAttribute`.

use super::particles::{
    COORD_LOCAL, DRAW_AXIS, DRAW_DYNAMIC_POINT, DRAW_LINE, GRAVITY_CENTER, GRAVITY_VECTOR, PE_BEAM,
    Params, SPIN_MOVEMENT, START_BOX, START_SPHERE, SUBDIV_ANIMATION,
};
use crate::math::to_rad;
use crate::string::{floats_exact, ints_exact, to_bool, to_float, to_int, to_lower};
use crate::types::{Color, Vector2f, Vector2l, Vector3f};
use std::ffi::{CStr, CString, c_char, c_void};
use std::ptr::null;

pub type AttrFn = unsafe extern "C" fn(ctx: *mut c_void, name: *const c_char) -> *const c_char;

#[repr(C)]
pub struct PsDesc {
    pub params: Params,
    pub max_particle_num: i32,
    pub respawn: bool,
    pub warm_up_time: f32,
    pub warm_up_steps_per_sec: f32,
    pub pos_offset: Vector3f,
    pub angle_offset: Vector3f,
    pub material_num: i32,
    pub animation_length: f32,
    pub sub_div: Vector2l,
    pub has_sub_div: bool,
    pub use_beam_noise: bool,
    pub low_freq_points: i32,
    pub high_freq_points: i32,
    pub min_low_freq_noise: Vector3f,
    pub max_low_freq_noise: Vector3f,
    pub min_high_freq_noise: Vector3f,
    pub max_high_freq_noise: Vector3f,
    pub name: *const c_char,
    pub material: *const c_char,
    pub death_ps: *const c_char,
}

pub struct PsBox {
    pub desc: PsDesc,
    _strings: [CString; 3],
}

struct Attrs {
    get: AttrFn,
    ctx: *mut c_void,
}

impl Attrs {
    fn raw(&self, name: &'static [u8]) -> Option<&CStr> {
        // SAFETY: `name` is a NUL-terminated literal; the getter returns NULL or a string that
        // outlives the parse.
        let p = unsafe { (self.get)(self.ctx, name.as_ptr().cast()) };
        // SAFETY: non-null results are NUL-terminated.
        (!p.is_null()).then(|| unsafe { CStr::from_ptr(p) })
    }
    fn string(&self, name: &'static [u8]) -> Vec<u8> {
        self.raw(name)
            .map(|s| s.to_bytes().to_vec())
            .unwrap_or_default()
    }
    fn f(&self, name: &'static [u8], d: f32) -> f32 {
        // SAFETY: NUL-terminated.
        self.raw(name)
            .map_or(d, |s| unsafe { to_float(s.as_ptr()) })
    }
    fn i(&self, name: &'static [u8], d: i32) -> i32 {
        // SAFETY: NUL-terminated.
        self.raw(name).map_or(d, |s| unsafe { to_int(s.as_ptr()) })
    }
    fn b(&self, name: &'static [u8], d: bool) -> bool {
        self.raw(name).map_or(d, |s| to_bool(s.to_bytes()))
    }
    fn nums<const N: usize>(&self, name: &'static [u8]) -> Option<[f32; N]> {
        let mut v = [0.0f32; N];
        floats_exact(self.raw(name)?.to_bytes(), &mut v).then_some(v)
    }
    fn v3(&self, name: &'static [u8], d: Vector3f) -> Vector3f {
        self.nums::<3>(name)
            .map_or(d, |[x, y, z]| Vector3f { x, y, z })
    }
    fn v2(&self, name: &'static [u8], d: Vector2f) -> Vector2f {
        self.nums::<2>(name).map_or(d, |[x, y]| Vector2f { x, y })
    }
    fn col(&self, name: &'static [u8], d: Color) -> Color {
        self.nums::<4>(name)
            .map_or(d, |[r, g, b, a]| Color { r, g, b, a })
    }
    fn v2l(&self, name: &'static [u8], d: Vector2l) -> Vector2l {
        let mut v = [0i32; 2];
        match self.raw(name) {
            Some(s) if ints_exact(s.to_bytes(), &mut v) => Vector2l { x: v[0], y: v[1] },
            _ => d,
        }
    }
    fn kind(&self, name: &'static [u8], table: &[(&[u8], i32)], d: i32) -> i32 {
        self.raw(name)
            .map(|s| to_lower(s.to_bytes()))
            .and_then(|w| table.iter().find(|(k, _)| *k == w.as_slice()).map(|e| e.1))
            .unwrap_or(d)
    }
}

fn splat3(v: f32) -> Vector3f {
    Vector3f { x: v, y: v, z: v }
}

fn rad3(v: Vector3f) -> Vector3f {
    Vector3f {
        x: to_rad(v.x),
        y: to_rad(v.y),
        z: to_rad(v.z),
    }
}

fn rad2(v: Vector2f) -> Vector2f {
    Vector2f {
        x: to_rad(v.x),
        y: to_rad(v.y),
    }
}

fn cstring(v: Vec<u8>) -> CString {
    CString::new(v).unwrap_or_default()
}

pub fn parse(get: AttrFn, ctx: *mut c_void) -> PsBox {
    let a = Attrs { get, ctx };
    let white = Color {
        r: 1.0,
        g: 1.0,
        b: 1.0,
        a: 1.0,
    };
    let zero3 = splat3(0.0);
    let zero2 = Vector2f::default();
    let one2 = Vector2f { x: 1.0, y: 1.0 };
    let start_kinds: &[(&[u8], i32)] = &[(b"box", START_BOX), (b"sphere", START_SPHERE)];
    let angle3 = rad3(a.v3(b"AngleOffset\0", zero3));

    let mut p = Params {
        pe_type: a.kind(b"PEType\0", &[(b"beam", PE_BEAM), (b"normal", 0)], 0),
        draw_type: a.kind(
            b"DrawType\0",
            &[
                (b"point", DRAW_DYNAMIC_POINT),
                (b"line", DRAW_LINE),
                (b"axis", DRAW_AXIS),
            ],
            DRAW_DYNAMIC_POINT,
        ),
        coord_system: a.kind(
            b"CoordSystem\0",
            &[(b"world", 0), (b"local", COORD_LOCAL)],
            0,
        ),
        sub_div_type: a.kind(
            b"SubDivType\0",
            &[(b"random", 0), (b"animation", SUBDIV_ANIMATION)],
            0,
        ),
        start_pos_type: a.kind(b"StartPosType\0", start_kinds, START_BOX),
        start_vel_type: a.kind(b"StartVelType\0", start_kinds, START_BOX),
        gravity_type: a.kind(
            b"GravityType\0",
            &[
                (b"none", 0),
                (b"vector", GRAVITY_VECTOR),
                (b"center", GRAVITY_CENTER),
            ],
            0,
        ),
        part_spin_type: a.kind(
            b"PartSpinType\0",
            &[(b"constant", 0), (b"movement", SPIN_MOVEMENT)],
            0,
        ),
        ..Params::default()
    };
    p.particles_per_second = a.f(b"ParticlesPerSecond\0", 1.0);
    p.start_time_offset = a.f(b"StartTimeOffset\0", 0.0);
    p.min_pause_length = a.f(b"MinPauseLength\0", 0.0);
    p.max_pause_length = a.f(b"MaxPauseLength\0", 0.0);
    p.min_pause_interval = a.f(b"MinPauseInterval\0", 0.0);
    p.max_pause_interval = a.f(b"MaxPauseInterval\0", 0.0);
    p.min_start_pos = a.v3(b"MinStartPos\0", zero3);
    p.max_start_pos = a.v3(b"MaxStartPos\0", zero3);
    p.min_start_angles = rad2(a.v2(b"MinStartAngles\0", zero2));
    p.max_start_angles = rad2(a.v2(b"MaxStartAngles\0", zero2));
    p.min_start_radius = a.f(b"MinStartRadius\0", 0.0);
    p.max_start_radius = a.f(b"MaxStartRadius\0", 0.0);
    p.min_start_vel = a.v3(b"MinStartVel\0", zero3);
    p.max_start_vel = a.v3(b"MaxStartVel\0", zero3);
    p.min_start_vel_angles = rad2(a.v2(b"MinStartVelAngles\0", zero2));
    p.max_start_vel_angles = rad2(a.v2(b"MaxStartVelAngles\0", zero2));
    p.min_start_vel_speed = a.f(b"MinStartVelSpeed\0", 0.0);
    p.max_start_vel_speed = a.f(b"MaxStartVelSpeed\0", 0.0);
    p.min_speed_multiply = a.f(b"MinSpeedMultiply\0", 0.0);
    p.max_speed_multiply = a.f(b"MaxSpeedMultiply\0", 0.0);
    p.min_start_acc = a.v3(b"MinStartAcc\0", zero3);
    p.max_start_acc = a.v3(b"MaxStartAcc\0", zero3);
    p.min_vel_maximum = a.f(b"MinVelMaximum\0", 0.0);
    p.max_vel_maximum = a.f(b"MaxVelMaximum\0", 0.0);
    p.uses_direction = a.b(b"UsesDirection\0", false);
    p.gravity_acc = a.v3(b"GravityAcc\0", zero3);
    p.use_part_spin = a.b(b"UsePartSpin\0", false);
    p.min_spin_range = a.f(b"MinSpinRange\0", 0.0);
    p.max_spin_range = a.f(b"MaxSpinRange\0", 0.0);
    p.use_revolution = a.b(b"UseRevolution\0", false);
    p.min_rev_vel = a.v3(b"MinRevVel\0", zero3);
    p.max_rev_vel = a.v3(b"MaxRevVel\0", zero3);
    p.min_life_span = a.f(b"MinLifeSpan\0", 0.0);
    p.max_life_span = a.f(b"MaxLifeSpan\0", 0.0);
    p.min_start_size = a.v2(b"MinStartSize\0", one2);
    p.max_start_size = a.v2(b"MaxStartSize\0", one2);
    p.start_rel_size = a.f(b"StartRelSize\0", 0.0);
    p.middle_rel_size = a.f(b"MiddleRelSize\0", 0.0);
    p.middle_rel_size_time = a.f(b"MiddleRelSizeTime\0", 0.0);
    p.middle_rel_size_length = a.f(b"MiddleRelSizeLength\0", 0.0);
    p.end_rel_size = a.f(b"EndRelSize\0", 0.0);
    p.multiply_rgb_with_alpha = a.b(b"MultiplyRGBWithAlpha\0", false);
    p.min_start_color = a.col(b"MinStartColor\0", white);
    p.max_start_color = a.col(b"MaxStartColor\0", white);
    p.start_rel_color = a.col(b"StartRelColor\0", white);
    p.middle_rel_color = a.col(b"MiddleRelColor\0", white);
    p.middle_rel_color_time = a.f(b"MiddleRelColorTime\0", 0.0);
    p.middle_rel_color_length = a.f(b"MiddleRelColorLength\0", 0.0);
    p.end_rel_color = a.col(b"EndRelColor\0", white);
    p.collides = a.b(b"Collides\0", false);
    p.min_bounce_amount = a.f(b"MinBounceAmount\0", 0.0);
    p.max_bounce_amount = a.f(b"MaxBounceAmount\0", 0.0);
    p.min_collision_max = a.i(b"MinCollisionMax\0", 0);
    p.max_collision_max = a.i(b"MaxCollisionMax\0", 0);
    p.collision_update_rate = a.i(b"CollisionUpdateRate\0", 0);

    let strings = [
        cstring(a.string(b"Name\0")),
        cstring(a.string(b"Material\0")),
        cstring(a.string(b"DeathPS\0")),
    ];
    let ptr = |i: usize| strings.get(i).map_or(null(), |s| s.as_ptr());
    let desc = PsDesc {
        params: p,
        max_particle_num: a.i(b"MaxParticleNum\0", 1),
        respawn: a.b(b"Respawn\0", false),
        warm_up_time: a.f(b"WarmUpTime\0", 0.0),
        warm_up_steps_per_sec: a.f(b"WarmUpStepsPerSec\0", 60.0),
        pos_offset: a.v3(b"PosOffset\0", zero3),
        angle_offset: angle3,
        material_num: a.i(b"MaterialNum\0", 1),
        animation_length: a.f(b"AnimationLength\0", 1.0),
        sub_div: a.v2l(b"SubDiv\0", Vector2l { x: 1, y: 1 }),
        has_sub_div: a.raw(b"SubDiv\0").is_some(),
        use_beam_noise: a.b(b"UseBeamNoise\0", false),
        low_freq_points: a.i(b"LowFreqPoints\0", 4),
        high_freq_points: a.i(b"HighFreqPoints\0", 5),
        min_low_freq_noise: a.v3(b"MinLowFreqNoise\0", zero3),
        max_low_freq_noise: a.v3(b"MaxLowFreqNoise\0", zero3),
        min_high_freq_noise: a.v3(b"MinHighFreqNoise\0", zero3),
        max_high_freq_noise: a.v3(b"MaxHighFreqNoise\0", zero3),
        name: ptr(0),
        material: ptr(1),
        death_ps: ptr(2),
    };
    PsBox {
        desc,
        _strings: strings,
    }
}

/// `1/(animationLength/materialCount)`, as in the C++ expression.
pub fn frame_step(animation_length: f32, materials: usize) -> f32 {
    1.0 / (animation_length / materials as f32)
}

pub fn max_frame_time(materials: usize) -> f32 {
    materials as f32 - 0.0001
}

/// # Safety
/// `get` returns NULL or a NUL-terminated string valid until the call returns; `ctx` is its context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12_ps_parse(get: AttrFn, ctx: *mut c_void) -> *mut PsBox {
    Box::into_raw(Box::new(parse(get, ctx)))
}

/// # Safety
/// `b` comes from `ohpl_h12_ps_parse`, or is null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12_ps_desc(b: *const PsBox) -> *const PsDesc {
    // SAFETY: caller contract.
    unsafe { b.as_ref() }.map_or(null(), |b| &raw const b.desc)
}

/// # Safety
/// `b` comes from `ohpl_h12_ps_parse` and is freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12_ps_free(b: *mut PsBox) {
    if !b.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(b) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12_ps_frame_step(animation_length: f32, materials: u32) -> f32 {
    frame_step(animation_length, materials as usize)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12_ps_max_frame_time(materials: u32) -> f32 {
    max_frame_time(materials as usize)
}

#[cfg(test)]
#[test]
fn desc_layout_matches_header() {
    assert_eq!(std::mem::size_of::<PsDesc>(), 544);
}
