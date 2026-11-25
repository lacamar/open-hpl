//! C ABI of the agent layer, mirrored in `soma/src/game/RustSomaAgentS4b.h`.

use super::api;
use super::core;
use super::io::{self, Io};
use super::model::{COMP_EDGE_GLOW, K};
use crate::math::{Vector3f, v3};
use crate::somascript::s2_as::Engine;
use std::ffi::c_void;

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);
type GlowFn = unsafe extern "C" fn(*mut c_void, usize, *const f32, f32, f32, f32);

fn vec3(p: *const f32) -> Vector3f {
    if p.is_null() {
        return v3(0.0, 0.0, 0.0);
    }
    // SAFETY: the caller passes three floats.
    unsafe { v3(*p, *p.add(1), *p.add(2)) }
}

/// # Safety
/// `f` is the host op entry, valid for the program's life.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_install(f: unsafe extern "C" fn(u32, *mut Io) -> i32) {
    io::install(Some(f));
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_register(engine: *mut c_void) {
    super::natives::register(Engine(engine));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_create(ent: usize, core: usize, names: usize) {
    super::pod::register(ent, core, names);
    api::create_agent(ent);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_destroy(ent: usize) {
    core::destroy_agent(ent);
    super::pod::forget(ent);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_update(ent: usize, dt: f32) {
    super::dump::begin(ent);
    core::update_agent(ent, dt);
    super::dump::end(ent);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_update_components(ent: usize, dt: f32) {
    core::update_components(ent, dt);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_set_active(ent: usize, on: bool) {
    core::set_active(ent, on);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_body(ent: usize) -> usize {
    api::body_of(ent)
}

/// # Safety
/// `out` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_get_pos(ent: usize, out: *mut f32) -> bool {
    match api::get_pos(ent) {
        Some(p) => {
            // SAFETY: caller contract.
            unsafe {
                *out = p.x;
                *out.add(1) = p.y;
                *out.add(2) = p.z;
            }
            true
        }
        None => false,
    }
}

/// # Safety
/// `p` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_set_pos(ent: usize, p: *const f32) -> bool {
    api::set_pos(ent, vec3(p))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_state(ent: usize) -> i32 {
    api::get_state(ent)
}

/// # Safety
/// The outputs are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_save_extra(
    ent: usize,
    yaw: *mut f32,
    senses: *mut bool,
    det: *mut bool,
) {
    let (y, s, d) = api::save_extra(ent);
    // SAFETY: caller contract.
    unsafe {
        *yaw = y;
        *senses = s;
        *det = d;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_load_extra(ent: usize, yaw: f32, senses: bool, det: bool) {
    api::load_extra(ent, yaw, senses, det);
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_save_path(ent: usize, ctx: *mut c_void, sink: Sink) {
    let b = api::save_path(ent);
    // SAFETY: sink contract.
    unsafe { sink(ctx, b.as_ptr(), b.len()) };
}

/// # Safety
/// `data` is valid for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_load_path(ent: usize, data: *const u8, len: usize) {
    api::load_path(ent, unsafe { crate::ffi::slice(data, len) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s4b_change_state(ent: usize, st: i32) {
    super::natives_c::change_state(ent, st);
}

/// # Safety
/// `v` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_send_message(ent: usize, msg: i32, v: *const f32, x: i32) {
    core::send_message(ent, msg, vec3(v), x);
}

/// # Safety
/// `pos` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_broadcast_sound(pos: *const f32, radius: f32, prio: i32) {
    api::broadcast_sound(vec3(pos), radius, prio);
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_debug(ent: usize, ctx: *mut c_void, sink: Sink) {
    let b = api::debug(ent);
    // SAFETY: sink contract.
    unsafe { sink(ctx, b.as_ptr(), b.len()) };
}

/// Every active edge-glow component that passes the alpha and light-limit tests.
///
/// # Safety
/// `cb` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4b_glow_each(ctx: *mut c_void, cb: GlowFn) {
    for a in core::agent_ptrs() {
        let c = core::comp_of(a, COMP_EDGE_GLOW);
        // SAFETY: registry agents and components.
        let (Some(c), ent) = (unsafe { c.as_ref() }, unsafe { (*a).ent }) else {
            continue;
        };
        let K::Glow(g) = &c.k else { continue };
        if !c.active || g.alpha <= 0.001 || g.light_limit <= 0.001 {
            continue;
        }
        // SAFETY: callback contract.
        unsafe {
            cb(
                ctx,
                ent,
                g.color.as_ptr(),
                g.alpha,
                g.thickness,
                g.light_limit,
            )
        };
    }
}
