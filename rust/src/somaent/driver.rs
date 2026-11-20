//! Per-frame drivers of `cSomaLuxEntity` over its `Core`: move and rotate, liquid amount, effect
//! base colour fade. Engine objects are reached through the host table of the C++ shell.

use super::effects::{self, LIQUID_MOVED, LIQUID_SNAP};
use super::ent_core::Core;
use crate::types::{Color, Matrixf, Vector3f};
use std::ffi::c_void;

pub const CALLBACK_MOVE: i32 = 0;
pub const CALLBACK_ROTATE: i32 = 1;

/// Mirrored in `RustSomaCore.h`; every op gets the entity as `ctx`.
#[repr(C)]
pub struct Host {
    pub get_matrix: unsafe extern "C" fn(*mut c_void, *mut Matrixf),
    pub set_matrix_main: unsafe extern "C" fn(*mut c_void, *const Matrixf),
    pub run_callback: unsafe extern "C" fn(*mut c_void, i32),
    pub has_mesh: unsafe extern "C" fn(*mut c_void) -> bool,
    pub mesh_coverage: unsafe extern "C" fn(*mut c_void, f32),
    pub mesh_liquid: unsafe extern "C" fn(*mut c_void, f32),
    pub capture_effect_defaults: unsafe extern "C" fn(*mut c_void),
    pub apply_effects_alpha: unsafe extern "C" fn(*mut c_void),
}

struct Ent<'a> {
    c: &'a mut Core,
    ctx: *mut c_void,
    h: &'a Host,
}

impl Ent<'_> {
    fn matrix(&self) -> Matrixf {
        let mut m = Matrixf::default();
        // SAFETY: the host fills `m`.
        unsafe { (self.h.get_matrix)(self.ctx, &mut m) };
        m
    }

    fn set_matrix_main(&self, m: &Matrixf) {
        // SAFETY: `m` is valid for the call.
        unsafe { (self.h.set_matrix_main)(self.ctx, m) }
    }

    fn run_callback(&self, which: i32) {
        // SAFETY: plain value call.
        unsafe { (self.h.run_callback)(self.ctx, which) }
    }

    fn update_rotate(&mut self, dt: f32) {
        let m = self.matrix();
        let Some(out) = self.c.mRotate.step(dt, &m) else {
            return;
        };
        self.set_matrix_main(&out.matrix);
        if out.done {
            self.run_callback(CALLBACK_ROTATE);
        }
    }

    fn update_move(&mut self, dt: f32) {
        if self.c.mRotate.mode != 0 {
            self.update_rotate(dt);
        }
        if !self.c.mMove.moving {
            return;
        }
        let mut m = self.matrix();
        let pos = Vector3f {
            x: m.m[0][3],
            y: m.m[1][3],
            z: m.m[2][3],
        };
        let Some((to, done)) = self.c.mMove.step(dt, pos) else {
            return;
        };
        (m.m[0][3], m.m[1][3], m.m[2][3]) = (to.x, to.y, to.z);
        self.set_matrix_main(&m);
        if done {
            self.run_callback(CALLBACK_MOVE);
        }
    }

    fn set_liquid(&mut self, x: f32) {
        self.c.mfLiquidAmount = x;
        self.c.mfLiquidTarget = x;
        self.c.mfLiquidSpeed = 0.0;
        // SAFETY: plain value calls.
        unsafe {
            if (self.h.has_mesh)(self.ctx) {
                (self.h.mesh_liquid)(self.ctx, x)
            }
        }
    }

    fn set_effect_base_color(&mut self, col: Color) {
        // SAFETY: plain calls.
        unsafe { (self.h.capture_effect_defaults)(self.ctx) };
        self.c.mEffectBaseColor = col;
        // SAFETY: plain calls.
        unsafe { (self.h.apply_effects_alpha)(self.ctx) };
    }
}

/// # Safety
/// `c` is the entity's core, `h` the host table, `ctx` the entity.
unsafe fn ent<'a>(c: *mut Core, ctx: *mut c_void, h: *const Host) -> Ent<'a> {
    // SAFETY: caller contract.
    unsafe {
        Ent {
            c: &mut *c,
            ctx,
            h: &*h,
        }
    }
}

/// # Safety
/// `c` is a live core.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_stop_move(c: *mut Core) {
    // SAFETY: caller contract.
    let c = unsafe { &mut *c };
    c.mMove.stop();
    c.mRotate.stop();
}

/// # Safety
/// See `ent`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_update_move(
    c: *mut Core,
    ctx: *mut c_void,
    h: *const Host,
    dt: f32,
) {
    // SAFETY: caller contract.
    unsafe { ent(c, ctx, h) }.update_move(dt);
}

/// # Safety
/// See `ent`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_set_liquid(
    c: *mut Core,
    ctx: *mut c_void,
    h: *const Host,
    x: f32,
) {
    // SAFETY: caller contract.
    unsafe { ent(c, ctx, h) }.set_liquid(x);
}

/// # Safety
/// See `ent`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_fade_liquid(
    c: *mut Core,
    ctx: *mut c_void,
    h: *const Host,
    target: f32,
    time: f32,
) {
    // SAFETY: caller contract.
    let mut e = unsafe { ent(c, ctx, h) };
    let (amount, mut tgt, mut speed) = (e.c.mfLiquidAmount, e.c.mfLiquidTarget, e.c.mfLiquidSpeed);
    let snap = effects::liquid_fade(target, time, amount, &mut tgt, &mut speed);
    (e.c.mfLiquidTarget, e.c.mfLiquidSpeed) = (tgt, speed);
    if snap {
        e.set_liquid(target);
    }
}

/// # Safety
/// See `ent`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_update_liquid(
    c: *mut Core,
    ctx: *mut c_void,
    h: *const Host,
    dt: f32,
) {
    // SAFETY: caller contract.
    let mut e = unsafe { ent(c, ctx, h) };
    let mut amount = e.c.mfLiquidAmount;
    let r = effects::liquid_update(dt, &mut amount, e.c.mfLiquidTarget, e.c.mfLiquidSpeed);
    e.c.mfLiquidAmount = amount;
    if r == LIQUID_SNAP {
        let t = e.c.mfLiquidTarget;
        e.set_liquid(t);
    } else if r == LIQUID_MOVED {
        // SAFETY: plain value calls.
        unsafe {
            if (e.h.has_mesh)(e.ctx) {
                (e.h.mesh_liquid)(e.ctx, amount)
            }
        }
    }
}

/// # Safety
/// See `ent`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_set_effect_color(
    c: *mut Core,
    ctx: *mut c_void,
    h: *const Host,
    col: *const Color,
) {
    // SAFETY: caller contract.
    unsafe { ent(c, ctx, h).set_effect_base_color(*col) }
}

/// # Safety
/// See `ent`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_fade_effect_color(
    c: *mut Core,
    ctx: *mut c_void,
    h: *const Host,
    col: *const Color,
    time: f32,
) {
    // SAFETY: caller contract.
    let mut e = unsafe { ent(c, ctx, h) };
    let col = unsafe { *col };
    let (mut t0, mut t) = (e.c.mfEffectColorTime, e.c.mfEffectColorT);
    let now = effects::color_fade_begin(time, &mut t0, &mut t);
    (e.c.mfEffectColorTime, e.c.mfEffectColorT) = (t0, t);
    if now {
        e.set_effect_base_color(col);
        return;
    }
    e.c.mEffectColorFrom = e.c.mEffectBaseColor;
    e.c.mEffectColorTo = col;
}

/// # Safety
/// See `ent`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_update_effect_color(
    c: *mut Core,
    ctx: *mut c_void,
    h: *const Host,
    dt: f32,
) {
    // SAFETY: caller contract.
    let mut e = unsafe { ent(c, ctx, h) };
    let has_mesh = unsafe { (e.h.has_mesh)(ctx) };
    let k = &mut *e.c;
    let flags = effects::update_fades(
        &mut k.mfMeshFadeAlpha,
        k.mfMeshFadeSpeed,
        has_mesh,
        &mut k.mfEffectsAlpha,
        &mut k.mfEffectsFadeSpeed,
        k.mfEffectsOffAlpha,
        dt,
    );
    if flags & effects::UPDATE_MESH != 0 {
        // SAFETY: plain value call.
        unsafe { (e.h.mesh_coverage)(ctx, e.c.mfMeshFadeAlpha) };
    }
    if flags & effects::UPDATE_EFFECTS != 0 {
        // SAFETY: plain call.
        unsafe { (e.h.apply_effects_alpha)(ctx) };
    }
    let k = &mut *e.c;
    if let Some(col) = effects::color_fade_step(
        k.mEffectColorFrom,
        k.mEffectColorTo,
        &mut k.mfEffectColorTime,
        &mut k.mfEffectColorT,
        dt,
    ) {
        e.set_effect_base_color(col);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::math::IDENTITY;
    use std::cell::RefCell;

    thread_local! {
        static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    }

    fn log(s: String) {
        LOG.with(|l| l.borrow_mut().push(s));
    }

    unsafe extern "C" fn get_matrix(_: *mut c_void, m: *mut Matrixf) {
        unsafe { *m = IDENTITY };
    }
    unsafe extern "C" fn set_matrix_main(_: *mut c_void, m: *const Matrixf) {
        let m = unsafe { *m };
        log(format!("set {} {} {}", m.m[0][3], m.m[1][3], m.m[2][3]));
    }
    unsafe extern "C" fn run_callback(_: *mut c_void, w: i32) {
        log(format!("cb {w}"));
    }
    unsafe extern "C" fn has_mesh(_: *mut c_void) -> bool {
        true
    }
    unsafe extern "C" fn mesh_f(_: *mut c_void, a: f32) {
        log(format!("mesh {a}"));
    }
    unsafe extern "C" fn nop(_: *mut c_void) {
        log("fx".into());
    }

    const HOST: Host = Host {
        get_matrix,
        set_matrix_main,
        run_callback,
        has_mesh,
        mesh_coverage: mesh_f,
        mesh_liquid: mesh_f,
        capture_effect_defaults: nop,
        apply_effects_alpha: nop,
    };

    fn take() -> Vec<String> {
        LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
    }

    #[test]
    fn move_reaches_goal_and_runs_the_callback_once() {
        let mut c = Core::default();
        let p = &raw mut c;
        c.mMove.start(
            Vector3f {
                x: 1.0,
                y: 0.0,
                z: 0.0,
            },
            1000.0,
            1000.0,
            0.0,
            true,
        );
        for _ in 0..8 {
            unsafe { ohpl_se_update_move(p, std::ptr::null_mut(), &HOST, 0.1) };
        }
        let l = take();
        assert_eq!(l.iter().filter(|s| s.starts_with("cb 0")).count(), 1);
        assert!(!c.mMove.moving);
    }

    #[test]
    fn liquid_fade_snaps_without_time_and_steps_with_it() {
        let mut c = Core::default();
        let p = &raw mut c;
        unsafe { ohpl_se_fade_liquid(p, std::ptr::null_mut(), &HOST, 0.5, 0.0) };
        assert_eq!(
            (c.mfLiquidAmount, take()),
            (0.5, vec!["mesh 0.5".to_string()])
        );
        unsafe { ohpl_se_fade_liquid(p, std::ptr::null_mut(), &HOST, 1.0, 1.0) };
        assert_eq!((c.mfLiquidTarget, c.mfLiquidSpeed), (1.0, 0.5));
        unsafe { ohpl_se_update_liquid(p, std::ptr::null_mut(), &HOST, 0.5) };
        assert_eq!(c.mfLiquidAmount, 0.75);
        assert_eq!(take(), vec!["mesh 0.75".to_string()]);
    }

    #[test]
    fn effect_color_fade_ends_on_the_target() {
        let mut c = Core::default();
        let p = &raw mut c;
        let to = Color {
            r: 0.0,
            g: 0.0,
            b: 0.0,
            a: 1.0,
        };
        unsafe { ohpl_se_fade_effect_color(p, std::ptr::null_mut(), &HOST, &to, 1.0) };
        unsafe { ohpl_se_update_effect_color(p, std::ptr::null_mut(), &HOST, 2.0) };
        assert_eq!(c.mEffectBaseColor, to);
        assert!(take().iter().filter(|s| *s == "fx").count() >= 2);
    }
}
