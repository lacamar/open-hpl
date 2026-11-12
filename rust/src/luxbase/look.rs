//! The head section of `UpdateGamePlayerInput`: mouse look, smoothing and gamepad look.

use super::input::{Smooth, act};

/// Gamepad and player queries and effects of the look update, in the order the C++ makes them.
pub trait LookEnv {
    fn is_triggered(&mut self, action: i32) -> bool;
    fn add_lean(&mut self, v: f32);
    fn set_lean(&mut self, v: f32);
    fn scroll(&mut self, v: f32);
    fn stop_movement(&mut self);
    fn button_down(&mut self, button: i32) -> bool;
    fn axis(&mut self, axis: i32) -> f32;
    fn dead_zone(&mut self) -> f32;
    fn frame_time(&mut self) -> f32;
    fn step_size(&mut self) -> f32;
}

pub const DPAD_UP: i32 = 11;
pub const DPAD_DOWN: i32 = 12;
pub const AXIS_LEFT_X: i32 = 0;
pub const AXIS_LEFT_Y: i32 = 1;
pub const AXIS_RIGHT_X: i32 = 2;
pub const AXIS_RIGHT_Y: i32 = 3;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LookIn {
    pub mouse_rel: [i32; 2],
    pub mouse_sensitivity: f32,
    pub screen_height: f32,
    pub smooth_mouse: bool,
    pub invert_mouse: bool,
    pub pad_present: bool,
    pub look_sensitivity: f32,
    pub look_invert: bool,
}

/// `cMath::RoundToInt`.
pub fn round_to_int(v: f32) -> i32 {
    (f64::from(v) + 0.5).floor() as i32
}

fn sign(v: f32) -> f32 {
    f32::from(i8::from(v > 0.0) - i8::from(v < 0.0))
}

/// Yaw and pitch input (before the C++ negates them for `AddYaw` and `AddPitch`).
pub fn look(smooth: &mut Smooth, i: &LookIn, e: &mut dyn LookEnv) -> [f32; 2] {
    let rel = [
        (i.mouse_rel[0] as f32) * i.mouse_sensitivity,
        (i.mouse_rel[1] as f32) * i.mouse_sensitivity,
    ];
    let abs = [round_to_int(rel[0]), round_to_int(rel[1])];
    let div = (1.7f64 * f64::from(i.screen_height)) as f32;
    let rel_pos = [(abs[0] as f32) / div, (abs[1] as f32) / div];
    let mut fin = if i.smooth_mouse {
        smooth.push(rel_pos)
    } else {
        rel_pos
    };
    if i.invert_mouse {
        fin[1] = -fin[1];
    }
    if e.is_triggered(act::LEAN) {
        e.add_lean(rel_pos[0]);
    }
    if i.pad_present {
        gamepad(i, e, &mut fin);
    }
    if e.is_triggered(act::LEAN) {
        fin = [0.0, 0.0];
    }
    fin
}

fn gamepad(i: &LookIn, e: &mut dyn LookEnv, fin: &mut [f32; 2]) {
    if e.button_down(DPAD_UP) || e.button_down(DPAD_DOWN) {
        if e.is_triggered(act::ZOOM_OUT) {
            let t = e.frame_time();
            e.scroll(t * 8.0);
        }
        if e.is_triggered(act::ZOOM_IN) {
            let t = e.frame_time();
            e.scroll(-t * 8.0);
        }
    }
    let mut look = [e.axis(AXIS_RIGHT_X), e.axis(AXIS_RIGHT_Y)];
    if e.is_triggered(act::ROTATE) {
        let t = e.frame_time();
        e.scroll(-t * 6.0 * look[1]);
        look = [e.axis(AXIS_LEFT_X), e.axis(AXIS_LEFT_Y)];
        let ex = look[0].abs().sqrt();
        let ey = look[1].abs().sqrt();
        look = [
            ex * look[0] * i.look_sensitivity / 1.25,
            ey * look[1] * i.look_sensitivity / 1.25,
        ];
        if (look[0] * look[0] + look[1] * look[1]).sqrt() > 0.0 {
            e.stop_movement();
        }
    }
    let dz = e.dead_zone();
    look[0] -= sign(look[0]) * dz;
    look[1] -= sign(look[1]) * dz;
    let k = (1.0f64 / f64::from(1.0f32 - dz)) as f32;
    look[0] *= k;
    look[1] *= k;
    let ex = look[0].abs().sqrt();
    let ey = look[1].abs().sqrt();
    let step = e.step_size();
    let px = look[0] * ex * i.look_sensitivity;
    let mut py = look[1] * ey * i.look_sensitivity;
    if i.look_invert {
        py = -py;
    }
    fin[0] += px * step;
    fin[1] += py * step;
    if e.is_triggered(act::LEAN) {
        if look[0].abs() > 0.05 {
            e.set_lean(look[0]);
        } else {
            e.set_lean(0.0);
        }
    }
}
