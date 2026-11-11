//! Effect fades (lights, billboards, flares, illumination), the mesh coverage fade, the effect
//! base-colour fade and the liquid-amount fade of a prop.

use super::motion::cmin;
use crate::math::{clamp, fma};
use crate::types::Color;

/// `SetEffectsActive`: the fade speed for `fade_time` (the var lookup stays C++); sets the alpha
/// at once when there is no fade.
pub fn fade_setup(on: bool, fade_time: f32, off_alpha: f32, alpha: &mut f32) -> f32 {
    let speed = if fade_time > 0.0 {
        (if on { 1.0f32 } else { -1.0f32 }) / fade_time
    } else {
        0.0
    };
    if speed == 0.0 {
        *alpha = if on { 1.0 } else { off_alpha };
    }
    speed
}

pub fn effect_color(def: Color, base: Color, alpha: f32) -> Color {
    Color {
        r: def.r * base.r * alpha,
        g: def.g * base.g * alpha,
        b: def.b * base.b * alpha,
        a: def.a * base.a * alpha,
    }
}

pub fn effects_on(alpha: f32, active: bool) -> bool {
    alpha > 0.0 && active
}

pub fn light_lit(col: Color, on: bool) -> bool {
    on && col.r + col.g + col.b > 0.0
}

pub const UPDATE_MESH: u32 = 1;
pub const UPDATE_EFFECTS: u32 = 2;

/// `UpdateEffectColor`, mesh coverage and effect alpha parts. Returns which of the two C++ has
/// to apply (`SetCoverageAmount`, `ApplyEffectsAlpha`).
pub fn update_fades(
    mesh_alpha: &mut f32,
    mesh_speed: f32,
    has_mesh: bool,
    alpha: &mut f32,
    speed: &mut f32,
    off_alpha: f32,
    dt: f32,
) -> u32 {
    let mut flags = 0;
    if *mesh_alpha < 1.0 && has_mesh {
        *mesh_alpha = cmin(fma(mesh_speed, dt, *mesh_alpha), 1.0);
        flags |= UPDATE_MESH;
    }
    if *speed != 0.0 {
        let min = if *speed < 0.0 { off_alpha } else { 0.0 };
        *alpha = clamp(fma(*speed, dt, *alpha), min, 1.0);
        if *alpha == min || *alpha == 1.0 {
            *speed = 0.0;
        }
        flags |= UPDATE_EFFECTS;
    }
    flags
}

/// `FadeEffectBaseColor`: true when the colour is to be set at once.
pub fn color_fade_begin(time: f32, color_time: &mut f32, color_t: &mut f32) -> bool {
    if time <= 0.0 {
        *color_time = 0.0;
        return true;
    }
    *color_time = time;
    *color_t = 0.0;
    false
}

fn lerp(a: Color, b: Color, t: f32) -> Color {
    let k = 1.0f32 - t;
    Color {
        r: a.r * k + b.r * t,
        g: a.g * k + b.g * t,
        b: a.b * k + b.b * t,
        a: a.a * k + b.a * t,
    }
}

/// `UpdateEffectColor`, colour part: the new base colour when a fade runs.
pub fn color_fade_step(
    from: Color,
    to: Color,
    color_time: &mut f32,
    color_t: &mut f32,
    dt: f32,
) -> Option<Color> {
    if *color_time <= 0.0 {
        return None;
    }
    let t = *color_t + dt / *color_time;
    *color_t = if 1.0f32 < t { 1.0 } else { t };
    let col = lerp(from, to, *color_t);
    if *color_t >= 1.0 {
        *color_time = 0.0;
    }
    Some(col)
}

/// `FadeLiquidAmountTo`: true when the amount is to be set at once.
pub fn liquid_fade(
    target: f32,
    time: f32,
    amount: f32,
    cur_target: &mut f32,
    speed: &mut f32,
) -> bool {
    if time <= 0.0 {
        return true;
    }
    *cur_target = target;
    *speed = (target - amount).abs() / time;
    false
}

pub const LIQUID_IDLE: i32 = 0;
pub const LIQUID_SNAP: i32 = 1;
pub const LIQUID_MOVED: i32 = 2;

/// `UpdateLiquidAmount`: `SNAP` sets the target at once, `MOVED` writes the new amount.
pub fn liquid_update(dt: f32, amount: &mut f32, target: f32, speed: f32) -> i32 {
    if speed == 0.0 {
        return LIQUID_IDLE;
    }
    let step = speed * dt;
    if (target - *amount).abs() <= step {
        return LIQUID_SNAP;
    }
    *amount += if target > *amount { step } else { -step };
    LIQUID_MOVED
}
