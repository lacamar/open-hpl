//! Per-update orchestration of the liquid, sticky, slime, insanity and flashback areas (A7b): body scans,
//! buoyancy, attach and detach run here against the engine through `mhost` ops (`aop`); the state is the A7 pod block.
//! No reference to an area block or the map is held across a host op: scripts and entities re-enter.

use super::a7_area::{A7Area, InsanityS, LiquidS, Pod, SlimeS, StickyS};
use super::mhost::{self, A, S, body, op};
use super::mmap::M;
use super::{a7_ffi::Ctx, area};
use crate::math::rand_rectf;
use crate::types::{Matrixf, Vector3f};
use std::sync::atomic::{AtomicBool, Ordering};

pub mod aop {
    pub const ENGINE_STEP: i32 = 100;
    pub const ENV: i32 = 101;
    pub const BODIES: i32 = 102;
    pub const LQ_BODY_READ: i32 = 103;
    pub const LQ_BODY_SET: i32 = 104;
    pub const BODY_CENTER: i32 = 105;
    pub const LQ_CHAR_READ: i32 = 106;
    pub const LQ_CHAR_APPLY: i32 = 107;
    pub const BODY_NAME: i32 = 108;
    pub const BODY_IN_AREA: i32 = 109;
    pub const STR: i32 = 110;
    pub const ST_PRE_ATTACH: i32 = 111;
    pub const ST_ATTACH_APPLY: i32 = 112;
    pub const ST_DETACH_APPLY: i32 = 113;
    pub const FX: i32 = 114;
    pub const BODY_STATE: i32 = 115;
    pub const BODY_SET: i32 = 116;
    pub const COLLIDES: i32 = 117;
    pub const SET_ACTIVE: i32 = 118;
    pub const SLIME_DAMAGE: i32 = 119;
    pub const PLAY_SOUND: i32 = 120;
    pub const PS_AT: i32 = 121;
    pub const SHAKE: i32 = 122;
    pub const INSANITY: i32 = 123;
    pub const FLASHBACK: i32 = 124;
}

/// Names of the C++ shell strings the host returns for `aop::STR`.
pub mod string_of {
    pub const ATTACH_FUNCTION: i32 = 0;
    pub const DETACH_FUNCTION: i32 = 1;
    pub const ATTACH_SOUND: i32 = 2;
    pub const DETACH_SOUND: i32 = 3;
    pub const ATTACH_PS: i32 = 4;
    pub const DETACH_PS: i32 = 5;
    pub const ATTACHABLE_BODY: i32 = 6;
    pub const AREA_NAME: i32 = 7;
    pub const SLIME_CALLBACK: i32 = 8;
    pub const SLIME_SOUND: i32 = 9;
    pub const SLIME_PS: i32 = 10;
}

/// `cLuxArea_Sticky::mbAllowAttachment`: set by `ohpl_ga5_sticky_name` and by the script `SetAllowAttachment`.
static ALLOW_ATTACHMENT: AtomicBool = AtomicBool::new(false);

pub fn allow_attachment() -> bool {
    ALLOW_ATTACHMENT.load(Ordering::Relaxed)
}

pub fn set_allow_attachment(v: bool) {
    ALLOW_ATTACHMENT.store(v, Ordering::Relaxed);
}

fn host(o: i32, f: impl FnOnce(&mut A)) -> A {
    let mut a = A::new();
    f(&mut a);
    mhost::call(o, &mut a);
    a
}

fn text(a: &A) -> Vec<u8> {
    a.ret_bytes(0)
}

fn shell_string(shell: usize, which: i32) -> Vec<u8> {
    text(&host(aop::STR, |a| {
        a.p[0] = shell;
        a.i[0] = which;
    }))
}

fn vec_at(a: &A, second: bool) -> Vector3f {
    let [x0, y0, z0, x1, y1, z1, ..] = a.f;
    if second {
        Vector3f {
            x: x1,
            y: y1,
            z: z1,
        }
    } else {
        Vector3f {
            x: x0,
            y: y0,
            z: z0,
        }
    }
}

fn matrix(m: &[f32; 16]) -> Matrixf {
    let mut r = Matrixf::default();
    for (row, chunk) in r.m.iter_mut().zip(m.chunks_exact(4)) {
        row.copy_from_slice(chunk);
    }
    r
}

fn flat(m: &Matrixf) -> [f32; 16] {
    let mut r = [0.0; 16];
    for (chunk, row) in r.chunks_exact_mut(4).zip(m.m.iter()) {
        chunk.copy_from_slice(row);
    }
    r
}

fn pod<'a>(a: *mut A7Area) -> &'a mut Pod {
    // SAFETY: `a` is a live block (caller contract of every entry); the borrow is a statement-local temporary.
    unsafe { &mut (*a).pod }
}

fn liquid(a: *mut A7Area) -> LiquidS {
    match pod(a) {
        Pod::Liquid(s) => *s,
        _ => LiquidS::default(),
    }
}

fn sticky(a: *mut A7Area) -> StickyS {
    match pod(a) {
        Pod::Sticky(s) => *s,
        _ => StickyS::default(),
    }
}

fn with_sticky(a: *mut A7Area, f: impl FnOnce(&mut StickyS)) {
    if let Pod::Sticky(s) = pod(a) {
        f(s);
    }
}

struct Env {
    body: usize,
    world: usize,
    pos: Vector3f,
    size: Vector3f,
    mtx: Matrixf,
}

fn env(shell: usize) -> Env {
    let e = host(aop::ENV, |a| a.p[0] = shell);
    Env {
        body: e.p[1],
        world: e.p[2],
        pos: vec_at(&e, false),
        size: vec_at(&e, true),
        mtx: matrix(&e.m),
    }
}

fn bodies_in(world: usize, area_body: usize) -> Vec<usize> {
    let r = host(aop::BODIES, |a| {
        a.p[0] = world;
        a.p[1] = area_body;
    });
    let n = usize::try_from(r.i[0]).unwrap_or(0);
    mhost::slice_of(r.r as *const usize, n).to_vec()
}

struct BodyInfo {
    collide: bool,
    active: bool,
    is_char: bool,
    mass: f32,
}

fn body_info(b: usize) -> BodyInfo {
    let r = host(op::BODY_INFO, |a| a.p[0] = b);
    BodyInfo {
        collide: r.i[0] & body::COLLIDE != 0,
        active: r.i[0] & body::ACTIVE != 0,
        is_char: r.i[0] & body::IS_CHAR != 0,
        mass: r.f[0],
    }
}

fn skipped(i: &BodyInfo) -> bool {
    area::body_skipped(i.collide, i.active, i.mass, i.is_char)
}

fn fx(world_shell: usize, kind: i32, name: &[u8], label: i32) {
    host(aop::FX, |a| {
        a.p[0] = world_shell;
        a.i[0] = kind;
        a.i[1] = label;
        a.s[0] = S::of(name);
    });
}

/// Update of the area families whose logic runs against engine bodies; the family follows the pod.
///
/// # Safety
/// `a` is a live block; `shell` is its C++ area; `ctx` the map context.
pub unsafe fn update(a: *mut A7Area, shell: usize, ctx: *mut Ctx, step: f32) {
    match pod(a) {
        Pod::Liquid(_) => liquid_update(a, shell, step),
        Pod::Sticky(_) => {
            sticky_update_attach(a, shell, step);
            sticky_update_collision(a, shell, ctx);
        }
        Pod::Slime(_) => slime_update(a, shell, ctx, step),
        Pod::Insanity(_) => insanity_update(a, shell, step),
        Pod::Flashback(_) => flashback_update(a, shell, step),
        _ => {}
    }
}

fn tick(a: *mut A7Area, step: f32, engine_step: f32) -> bool {
    // SAFETY: caller contract of `update`.
    unsafe { (*a).tick(step, engine_step) }
}

fn liquid_update(a: *mut A7Area, shell: usize, step: f32) {
    let engine_step = host(aop::ENGINE_STEP, |_| {}).f[0];
    if !tick(a, step, engine_step) {
        return;
    }
    let e = env(shell);
    let surface_y = area::liquid_surface_y(e.pos.y, e.size.y);
    for b in bodies_in(e.world, e.body) {
        let info = body_info(b);
        if skipped(&info) {
            continue;
        }
        let inside = host(op::BODY_PAIR, |x| {
            x.p[0] = e.world;
            x.p[1] = b;
            x.p[2] = e.body;
        })
        .i[0]
            != 0;
        if info.is_char {
            liquid_char(a, shell, b, surface_y, inside);
        } else {
            liquid_body(a, shell, b, surface_y, inside);
        }
    }
}

fn liquid_body(a: *mut A7Area, shell: usize, b: usize, surface_y: f32, inside: bool) {
    let r = host(aop::LQ_BODY_READ, |x| {
        x.p[0] = b;
        x.i[0] = i32::from(inside);
    });
    let s = liquid(a);
    let out = area::liquid_body(
        area::BuoyIn {
            inside,
            allowed: r.i[1] != 0,
            active: r.i[2] != 0,
            has_waves: s.has_waves,
        },
        r.f[0],
        s.max_wave_distance_sqr,
    );
    if out.deactivate {
        host(aop::LQ_BODY_SET, |x| {
            x.p[0] = b;
            x.i[0] = 0;
        });
        return;
    }
    if out.setup {
        host(aop::LQ_BODY_SET, |x| {
            x.p[0] = b;
            x.p[1] = shell;
            x.i[0] = 1;
            x.f[0..4].copy_from_slice(&s.surface_plane);
            x.f[4] = s.density;
            x.f[5] = s.linear_viscosity;
            x.f[6] = s.angular_viscosity;
            x.f[7] = surface_y;
        });
    }
    if out.waves {
        let c = host(aop::BODY_CENTER, |x| x.p[0] = b);
        let wave_y = area::wave_surface_y(
            liquid(a).time_count,
            s.wave_freq,
            c.f[0],
            c.f[2],
            s.wave_amp,
            surface_y,
        );
        let p = area::wave_plane(wave_y);
        host(aop::LQ_BODY_SET, |x| {
            x.p[0] = b;
            x.i[0] = 2;
            x.f[0..4].copy_from_slice(&[p.a, p.b, p.c, p.d]);
        });
    }
}

fn liquid_char(a: *mut A7Area, shell: usize, b: usize, surface_y: f32, inside: bool) {
    let r = host(aop::LQ_CHAR_READ, |x| x.p[0] = b);
    let action = area::liquid_char(r.i[0] != 0, r.i[1] != 0, inside, r.i[2] != 0);
    if action == area::CHAR_NONE {
        return;
    }
    let speed_mul = liquid(a).player_speed_mul;
    host(aop::LQ_CHAR_APPLY, |x| {
        x.p[0] = shell;
        x.p[1] = b;
        x.i[0] = r.i[0];
        x.i[1] = action;
        x.f[0] = surface_y;
        x.f[1] = speed_mul;
    });
}

fn sticky_update_attach(a: *mut A7Area, shell: usize, step: f32) {
    let s = sticky(a);
    if s.attached_body == 0 || s.set_mtx_time >= 1.0 {
        return;
    }
    let e = env(shell);
    let st = host(aop::BODY_STATE, |x| x.p[0] = s.attached_body);
    // SAFETY: caller contract of `update`.
    let out =
        unsafe { (*a).sticky_step(e.mtx, e.pos, vec_at(&st, false), vec_at(&st, true), step) };
    let Some(out) = out else {
        return;
    };
    if out.mode == area::ATTACH_SET_MATRIX {
        host(aop::BODY_SET, |x| {
            x.p[0] = s.attached_body;
            x.i[0] = 1;
            x.m = flat(&out.mtx);
        });
    } else if out.mode == area::ATTACH_SET_POSITION {
        host(aop::BODY_SET, |x| {
            x.p[0] = s.attached_body;
            x.i[0] = 2;
            x.f[0..3].copy_from_slice(&[out.pos.x, out.pos.y, out.pos.z]);
        });
    }
    if out.done {
        let sound = shell_string(shell, string_of::ATTACH_SOUND);
        if !sound.is_empty() {
            fx(shell, 0, &sound, 0);
        }
        let ps = shell_string(shell, string_of::ATTACH_PS);
        if !ps.is_empty() {
            fx(shell, 1, &ps, 0);
        }
    }
}

fn body_name(b: usize) -> Vec<u8> {
    text(&host(aop::BODY_NAME, |x| x.p[0] = b))
}

fn callback(func: &[u8], area_name: &[u8], b: usize) -> Vec<u8> {
    area::call2(func, area_name, &body_name(b))
}

fn sticky_update_collision(a: *mut A7Area, shell: usize, ctx: *mut Ctx) {
    if sticky(a).attached_body != 0 {
        return;
    }
    let e = env(shell);
    let attach_name = shell_string(shell, string_of::ATTACHABLE_BODY);
    for b in bodies_in(e.world, e.body) {
        let info = body_info(b);
        if skipped(&info) {
            continue;
        }
        let in_bv = host(aop::BODY_IN_AREA, |x| {
            x.p[0] = b;
            x.p[1] = e.body;
            x.i[0] = i32::from(sticky(a).check_center_in_area);
        })
        .i[0]
            != 0;
        let last = sticky(a).last_attached;
        let inside = area::sticky_inside(in_bv, b == last);
        if inside == area::INSIDE_FORGET_LAST {
            with_sticky(a, |s| s.last_attached = 0);
        }
        if inside != area::INSIDE_PROCEED {
            continue;
        }
        let (skip, allow) = area::sticky_name(&attach_name, &body_name(b));
        set_allow_attachment(allow);
        if skip {
            continue;
        }
        let collides = host(op::BODY_PAIR, |x| {
            x.p[0] = e.world;
            x.p[1] = b;
            x.p[2] = e.body;
            x.i[1] = 1;
        })
        .i[0]
            != 0;
        if !collides {
            continue;
        }
        let func = shell_string(shell, string_of::ATTACH_FUNCTION);
        if !func.is_empty() {
            let name = shell_string(shell, string_of::AREA_NAME);
            // SAFETY: `ctx` is the live map context (caller contract of `update`).
            unsafe { M::new(ctx) }.run_script(&callback(&func, &name, b));
            if area::sticky_after_callback(true, allow_attachment()) {
                continue;
            }
        }
        // SAFETY: as above.
        unsafe { attach_body(a, b) };
    }
}

/// `cLuxArea_Sticky::AttachBody`.
///
/// # Safety
/// `a` is a live sticky block.
pub unsafe fn attach_body(a: *mut A7Area, b: usize) {
    if sticky(a).attached_body != 0 {
        return;
    }
    let r = host(aop::ST_PRE_ATTACH, |x| x.p[0] = b);
    let saved = (r.i[1] >= 0).then_some(r.i[1] != 0);
    // SAFETY: caller contract.
    unsafe { (*a).sticky_attach(matrix(&r.m), r.i[0] != 0, r.f[0], saved) };
    host(aop::ST_ATTACH_APPLY, |x| x.p[0] = b);
    with_sticky(a, |s| {
        s.attached_body = b;
        s.last_attached = b;
    });
}

/// `cLuxArea_Sticky::DetachBody`.
///
/// # Safety
/// `a` is a live sticky block; `shell` its C++ area; `ctx` the map context.
pub unsafe fn detach_body(a: *mut A7Area, shell: usize, ctx: *mut Ctx) {
    let attached = sticky(a).attached_body;
    if attached == 0 {
        return;
    }
    let name = shell_string(shell, string_of::AREA_NAME);
    let func = shell_string(shell, string_of::DETACH_FUNCTION);
    if !func.is_empty() {
        // SAFETY: caller contract.
        unsafe { M::new(ctx) }.run_script(&callback(&func, &name, attached));
    }
    let sound = shell_string(shell, string_of::DETACH_SOUND);
    if !sound.is_empty() {
        fx(shell, 0, &sound, 1);
    }
    let ps = shell_string(shell, string_of::DETACH_PS);
    if !ps.is_empty() {
        fx(shell, 1, &ps, 1);
    }
    let s = sticky(a);
    host(aop::ST_DETACH_APPLY, |x| {
        x.p[0] = attached;
        x.i[0] = i32::from(s.attached_body_gravity);
        x.i[1] = i32::from(s.attached_entity_full_game_saved);
        x.f[0] = s.attached_body_mass;
    });
    with_sticky(a, |s| s.attached_body = 0);
}

fn collides_player(shell: usize) -> bool {
    host(aop::COLLIDES, |x| x.p[0] = shell).i[0] != 0
}

fn deactivate(shell: usize) {
    host(aop::SET_ACTIVE, |x| {
        x.p[0] = shell;
        x.i[0] = 0;
    });
}

fn slime_update(a: *mut A7Area, shell: usize, ctx: *mut Ctx, step: f32) {
    if !tick(a, step, 0.0) || !collides_player(shell) {
        return;
    }
    let Pod::Slime(SlimeS {
        min_attack_damage,
        max_attack_damage,
        screen_shake_amount,
        ..
    }) = *pod(a)
    else {
        return;
    };
    let damage = rand_rectf(min_attack_damage, max_attack_damage);
    host(aop::SLIME_DAMAGE, |x| x.f[0] = damage);
    let sound = shell_string(shell, string_of::SLIME_SOUND);
    host(aop::PLAY_SOUND, |x| {
        x.p[0] = shell;
        x.s[0] = S::of(&sound);
    });
    let ps = shell_string(shell, string_of::SLIME_PS);
    if !ps.is_empty() {
        host(aop::PS_AT, |x| {
            x.p[0] = shell;
            x.s[0] = S::of(&ps);
        });
    }
    host(aop::SHAKE, |x| x.f[0] = screen_shake_amount);
    let cb = shell_string(shell, string_of::SLIME_CALLBACK);
    if !cb.is_empty() {
        let name = shell_string(shell, string_of::AREA_NAME);
        // SAFETY: `ctx` is the live map context.
        unsafe { M::new(ctx) }.run_script(&area::call1(&cb, &name));
    }
    if matches!(pod(a), Pod::Slime(s) if s.disable_after_attack) {
        deactivate(shell);
    }
}

fn insanity_update(a: *mut A7Area, shell: usize, step: f32) {
    if !tick(a, step, 0.0) || !collides_player(shell) {
        return;
    }
    if host(aop::INSANITY, |x| x.i[0] = 0).i[0] == 0 {
        return;
    }
    if matches!(
        pod(a),
        Pod::Insanity(InsanityS {
            auto_disable: true,
            ..
        })
    ) {
        deactivate(shell);
    }
    host(aop::INSANITY, |x| x.i[0] = 1);
}

fn flashback_update(a: *mut A7Area, shell: usize, step: f32) {
    let engine_step = host(aop::ENGINE_STEP, |_| {}).f[0];
    if !tick(a, step, engine_step) || !collides_player(shell) {
        return;
    }
    deactivate(shell);
    host(aop::FLASHBACK, |x| x.p[0] = shell);
}
