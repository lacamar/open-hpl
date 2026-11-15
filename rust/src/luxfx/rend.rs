//! `cLuxEffectRenderer` (flash, enemy glow and outline object lists, the flash oscillation and the
//! pass plans) and the insanity post effect clock. Programs, frame buffers and textures stay with
//! the C++ shells; plans are replayed through the host.

use super::host::Fx;
use super::hw::madd;
use super::postfx::{self, Clock};
use super::renderer::{self, Glow};

/// `cLinearOscillation`: a value ramping between `min` and `max` at fixed speeds.
#[derive(Clone, Copy, Debug)]
pub struct Oscillation {
    min: f32,
    max: f32,
    pub val: f32,
    up_speed: f32,
    down_speed: f32,
    add: f32,
}

impl Oscillation {
    pub fn new(min: f32, max: f32, val: f32, up: f32, down: f32) -> Self {
        let up_speed = up.abs();
        Self {
            min,
            max,
            val,
            up_speed,
            down_speed: -down.abs(),
            add: up_speed,
        }
    }

    pub fn update(&mut self, dt: f32) {
        self.val = madd(self.add, dt, self.val);
        if self.add > 0.0 {
            if self.val >= self.max {
                self.val = self.max;
                self.add = self.down_speed;
            }
        } else if self.val <= self.min {
            self.val = self.min;
            self.add = self.up_speed;
        }
    }
}

#[derive(Clone)]
pub struct Rend {
    pub flash: Vec<(usize, f32)>,
    pub glow: Vec<(usize, f32)>,
    pub outline: Vec<usize>,
    pub osc: Oscillation,
    /// Bit `n` set when program slot `n` was created (flash 0, glow 1, outline 2..5, blur 6, 7).
    pub programs: u32,
}

impl Rend {
    pub fn new(programs: u32) -> Self {
        Self {
            flash: Vec::new(),
            glow: Vec::new(),
            outline: Vec::new(),
            osc: Oscillation::new(0.0, 1.0, 0.0, 1.0, 1.0),
            programs,
        }
    }

    pub fn clear_lists(&mut self) {
        self.flash.clear();
        self.glow.clear();
    }

    pub fn reset(&mut self) {
        self.outline.clear();
        self.clear_lists();
    }
}

fn glow_pass(
    fx: &dyn Fx,
    fns: usize,
    list: &[(usize, f32)],
    plan: impl FnOnce(&[Glow]) -> renderer::Plan,
) {
    let objs: Vec<usize> = list.iter().map(|o| o.0).collect();
    let glow: Vec<Glow> = list
        .iter()
        .map(|o| Glow {
            collides: u32::from(fx.r_collides(fns, o.0)),
            alpha: o.1,
        })
        .collect();
    let p = plan(&glow);
    fx.r_replay(fns, &objs, p.ops(), [0; 4]);
}

/// `RenderTrans` after a snapshot of the lists, so no borrow is held across host calls.
pub fn render_trans(fx: &dyn Fx, fns: usize, r: &Rend) {
    if !r.flash.is_empty() && r.programs >> renderer::PROG_FLASH & 1 != 0 {
        glow_pass(fx, fns, &r.flash, |g| {
            renderer::plan_flash(g, r.osc.val, true)
        });
    }
    if !r.glow.is_empty() && r.programs >> renderer::PROG_GLOW & 1 != 0 {
        glow_pass(fx, fns, &r.glow, |g| renderer::plan_glow(g, true));
    }
    if fx.r_reflection(fns) || r.outline.is_empty() {
        return;
    }
    let outline: Vec<renderer::Outline> = r.outline.iter().map(|o| fx.r_outline(fns, *o)).collect();
    let plan = renderer::plan_outline(&outline, r.programs);
    let clip = fx.r_clip(fns, plan.clip_min, plan.clip_max);
    fx.r_replay(fns, &r.outline, plan.ops(), clip);
}

pub fn ins_update(c: &mut Clock, dt: f32) {
    postfx::update(c, dt);
}

pub fn ins_render(
    fx: &dyn Fx,
    c: &Clock,
    screen: [f32; 2],
    program_ok: bool,
    effect: usize,
    input: usize,
    fb: usize,
) {
    let plan = postfx::plan(c, screen, program_ok);
    fx.p_replay(effect, input, fb, plan.ops());
}
