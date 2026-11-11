//! `cLuxProp_Lamp`: lit state, ignition, fades and the lamp-to-light colour connection.

use super::base::{BaseState, EffectsIn, LightData, effects_set_active};
use super::object::{GrabData, load_grab};
use super::ops::Ops;
use super::vars::{Vars, put_var};
use crate::math::fma;
use crate::types::Color;

pub const SLOT_CONNECTED_LIGHT: i32 = 70;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LampParams {
    pub can_be_lit_by_player: bool,
    pub can_be_grabbed: bool,
    pub grab: GrabData,
}

pub fn load(v: &dyn Vars, out: &mut LampParams) {
    out.can_be_lit_by_player = v.boolean("CanBeLitByPlayer", true);
    out.can_be_grabbed = v.boolean("CanBeGrabbed", false);
    load_grab(v, true, &mut out.grab);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LampState {
    pub lit: bool,
    pub light_connection_setup: bool,
    pub connection_light_amount: f32,
    pub connection_light_use_on_color: bool,
    pub connection_light_use_spec: bool,
}

impl LampState {
    pub fn new() -> Self {
        Self {
            lit: true,
            ..Self::default()
        }
    }
}

pub fn set_lit(
    s: &mut LampState,
    base: &mut BaseState,
    env: &EffectsIn,
    lights: &[LightData],
    lit: bool,
    use_effects: bool,
    ops: &mut Ops,
) {
    if s.lit == lit {
        return;
    }
    s.lit = lit;
    let a = EffectsIn {
        active: lit,
        fade: use_effects,
        ..*env
    };
    effects_set_active(base, &a, lights, ops);
}

/// `LoadInstanceVariables`.
pub fn load_instance(
    v: &dyn Vars,
    s: &mut LampState,
    base: &mut BaseState,
    env: &EffectsIn,
    lights: &[LightData],
    ops: &mut Ops,
) {
    set_lit(s, base, env, lights, v.boolean("Lit", true), false, ops);
    put_var(v, SLOT_CONNECTED_LIGHT, "ConnectedLight");
    s.connection_light_amount = v.float("ConnectionLightAmount", 0.0);
    s.connection_light_use_on_color = v.boolean("ConnectionLightUseOnColor", false);
    s.connection_light_use_spec = v.boolean("ConnectionLightUseSpec", false);
}

pub fn can_ignite_by_player(s: &LampState, p: &LampParams) -> bool {
    !(s.lit || !p.can_be_lit_by_player)
}

pub fn can_interact(s: &LampState, p: &LampParams, has_interact_callback: bool) -> bool {
    (can_ignite_by_player(s, p) && !s.lit) || has_interact_callback
}

/// 0 default, 1 the custom crosshair, 2 ignite.
pub fn focus_crosshair(
    s: &LampState,
    p: &LampParams,
    custom_is_default: bool,
    has_cb: bool,
) -> i32 {
    if !custom_is_default && s.lit {
        return 1;
    }
    if can_interact(s, p, has_cb) { 2 } else { 0 }
}

/// The sanity the player gains from lighting a lamp.
pub fn sanity_add(sanity: f32, min_increase: f32, max_increase: f32) -> f32 {
    let mut inc = 1.0f32 - sanity / 100.0f32;
    inc *= inc;
    fma(min_increase, 1.0f32 - inc, max_increase * inc)
}

pub fn on_connection_state(
    s: &mut LampState,
    base: &mut BaseState,
    env: &EffectsIn,
    lights: &[LightData],
    state: i32,
    ops: &mut Ops,
) {
    if state > 0 {
        set_lit(s, base, env, lights, true, true, ops);
    }
    if state < 0 {
        set_lit(s, base, env, lights, false, true, ops);
    }
}

/// `FadeTo` per light: a negative component or radius keeps the light's own value.
pub fn fade_target(light: Color, light_radius: f32, rgba: [f32; 4], radius: f32) -> (Color, f32) {
    let pick = |want: f32, cur: f32| if want >= 0.0 { want } else { cur };
    (
        Color {
            r: pick(rgba[0], light.r),
            g: pick(rgba[1], light.g),
            b: pick(rgba[2], light.b),
            a: pick(rgba[3], light.a),
        },
        pick(radius, light_radius),
    )
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LampLink {
    pub has_lights: bool,
    /// The lamp's first light colour: the `On` colour or the live one.
    pub color: Color,
    pub use_spec: bool,
    pub amount: f32,
    pub effects_alpha: f32,
}

/// `cLuxLampLightConnection::Update`.
pub fn light_connection_color(default: Color, links: &[LampLink]) -> Color {
    let mut out = default;
    for l in links {
        if !l.has_lights {
            continue;
        }
        let mut c = l.color;
        if !l.use_spec {
            c.a = 0.0;
        }
        out = Color {
            r: out.r + (c.r * l.amount) * l.effects_alpha,
            g: out.g + (c.g * l.amount) * l.effects_alpha,
            b: out.b + (c.b * l.amount) * l.effects_alpha,
            a: out.a + (c.a * l.amount) * l.effects_alpha,
        };
    }
    out
}
