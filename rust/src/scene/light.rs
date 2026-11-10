//! `iLight` fade/flicker/brightness animation, `cLightSpot` projection maths and the
//! `cLightBox` irradiance fade. Side effects stay in C++ and are called back in the original
//! order, so random numbers are drawn at the same points as before.

use crate::math::{
    Matrixf, Vector3f, cos, fma, matrix_mul, perspective_projection, rand_rectf, tan,
};
use crate::types::Color;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LightAnim {
    pub bright_fade_time: f32,
    pub bright_fade_count: f32,
    pub bright_fade_start: f32,
    pub bright_fade_dest: f32,
    pub col_add: Color,
    pub radius_add: f32,
    pub dest_col: Color,
    pub dest_radius: f32,
    pub fade_time: f32,
    pub flicker_on_min_length: f32,
    pub flicker_off_min_length: f32,
    pub flicker_on_max_length: f32,
    pub flicker_off_max_length: f32,
    pub flicker_off_color: Color,
    pub flicker_off_radius: f32,
    pub flicker_on_fade_min_length: f32,
    pub flicker_on_fade_max_length: f32,
    pub flicker_off_fade_min_length: f32,
    pub flicker_off_fade_max_length: f32,
    pub flicker_on_color: Color,
    pub flicker_on_radius: f32,
    pub flicker_time: f32,
    pub flicker_state_length: f32,
    pub flickering: bool,
    pub flicker_fade: bool,
    pub flicker_on: bool,
}

/// The light values `UpdateLight` reads back after its own setter calls.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LightLive {
    pub diffuse: Color,
    pub radius: f32,
    pub brightness: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FlickerCfg {
    pub off_color: Color,
    pub off_radius: f32,
    pub on_min_length: f32,
    pub on_max_length: f32,
    pub off_min_length: f32,
    pub off_max_length: f32,
    pub on_fade_min_length: f32,
    pub on_fade_max_length: f32,
    pub off_fade_min_length: f32,
    pub off_fade_max_length: f32,
    pub fade: bool,
}

/// C++ side effects of `UpdateLight`, each applied through the real (virtual) setter.
pub trait LightHost {
    fn ease(&mut self, t: f32) -> f32;
    fn set_brightness(&mut self, v: f32);
    fn set_radius(&mut self, v: f32);
    fn set_diffuse(&mut self, c: Color);
    fn assign_diffuse(&mut self, c: Color);
    fn raw_radius(&mut self, v: f32);
    fn flicker_sound(&mut self, on: bool);
    fn flicker_event(&mut self, on: bool);
}

fn min(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

pub fn fade_brightness_to(a: &mut LightAnim, live: &LightLive, dest: f32, time: f32) {
    a.bright_fade_time = if time > 0.0 { time } else { 0.0001 };
    a.bright_fade_count = 0.0;
    a.bright_fade_start = live.brightness;
    a.bright_fade_dest = dest;
}

pub fn fade_to(a: &mut LightAnim, live: &LightLive, col: Color, radius: f32, time: f32) {
    let time = if time <= 0.0 { 0.0001 } else { time };
    a.fade_time = time;
    a.col_add = Color {
        r: (col.r - live.diffuse.r) / time,
        g: (col.g - live.diffuse.g) / time,
        b: (col.b - live.diffuse.b) / time,
        a: (col.a - live.diffuse.a) / time,
    };
    a.radius_add = (radius - live.radius) / time;
    a.dest_radius = radius;
    a.dest_col = col;
}

pub fn set_flicker(a: &mut LightAnim, live: &LightLive, c: &FlickerCfg) {
    a.flicker_off_color = c.off_color;
    a.flicker_off_radius = c.off_radius;
    a.flicker_on_min_length = c.on_min_length;
    a.flicker_on_max_length = c.on_max_length;
    a.flicker_off_min_length = c.off_min_length;
    a.flicker_off_max_length = c.off_max_length;
    a.flicker_fade = c.fade;
    a.flicker_on_fade_min_length = c.on_fade_min_length;
    a.flicker_on_fade_max_length = c.on_fade_max_length;
    a.flicker_off_fade_min_length = c.off_fade_min_length;
    a.flicker_off_fade_max_length = c.off_fade_max_length;
    a.flicker_on_color = live.diffuse;
    a.flicker_on_radius = live.radius;
    a.flicker_on = true;
    a.flicker_time = 0.0;
    a.fade_time = 0.0;
    a.flicker_state_length = rand_rectf(a.flicker_on_min_length, a.flicker_on_max_length);
}

fn host_set_diffuse<H: LightHost>(live: &mut LightLive, host: &mut H, c: Color) {
    live.diffuse = c;
    host.set_diffuse(c);
}

fn host_set_radius<H: LightHost>(live: &mut LightLive, host: &mut H, r: f32) {
    live.radius = r;
    host.set_radius(r);
}

fn flicker_toggle<H: LightHost>(
    a: &mut LightAnim,
    live: &mut LightLive,
    host: &mut H,
    has_sound: bool,
    to_on: bool,
) {
    a.flicker_on = to_on;
    let (col, radius, fade_min, fade_max) = if to_on {
        (
            a.flicker_on_color,
            a.flicker_on_radius,
            a.flicker_on_fade_min_length,
            a.flicker_on_fade_max_length,
        )
    } else {
        (
            a.flicker_off_color,
            a.flicker_off_radius,
            a.flicker_off_fade_min_length,
            a.flicker_off_fade_max_length,
        )
    };
    if !a.flicker_fade {
        host_set_diffuse(live, host, col);
        host_set_radius(live, host, radius);
    } else {
        let time = rand_rectf(fade_min, fade_max);
        fade_to(a, live, col, radius, time);
    }
    if has_sound {
        host.flicker_sound(to_on);
    }
    host.flicker_event(to_on);
    a.flicker_time = 0.0;
    let (min_len, max_len) = if to_on {
        (a.flicker_on_min_length, a.flicker_on_max_length)
    } else {
        (a.flicker_off_min_length, a.flicker_off_max_length)
    };
    a.flicker_state_length = rand_rectf(min_len, max_len);
}

pub fn update_light<H: LightHost>(
    a: &mut LightAnim,
    live: &mut LightLive,
    has_on_sound: bool,
    has_off_sound: bool,
    host: &mut H,
    dt: f32,
) {
    if a.bright_fade_time > 0.0 {
        a.bright_fade_count = min(a.bright_fade_count + dt, a.bright_fade_time);
        let t = a.bright_fade_count / a.bright_fade_time;
        let t = host.ease(t);
        let v = fma(
            a.bright_fade_dest - a.bright_fade_start,
            t,
            a.bright_fade_start,
        );
        live.brightness = v;
        host.set_brightness(v);
        if a.bright_fade_count >= a.bright_fade_time {
            a.bright_fade_time = 0.0;
        }
    }

    if a.fade_time > 0.0 {
        let radius = fma(a.radius_add, dt, live.radius);
        host_set_radius(live, host, radius);
        let d = Color {
            r: fma(a.col_add.r, dt, live.diffuse.r),
            g: fma(a.col_add.g, dt, live.diffuse.g),
            b: fma(a.col_add.b, dt, live.diffuse.b),
            a: fma(a.col_add.a, dt, live.diffuse.a),
        };
        host.assign_diffuse(d);
        host_set_diffuse(live, host, d);
        a.fade_time -= dt;
        if a.fade_time <= 0.0 {
            a.fade_time = 0.0;
            host_set_diffuse(live, host, a.dest_col);
            live.radius = a.dest_radius;
            host.raw_radius(a.dest_radius);
        }
    }

    if a.flickering && a.fade_time <= 0.0 {
        if a.flicker_on {
            if a.flicker_time >= a.flicker_state_length {
                flicker_toggle(a, live, host, has_off_sound, false);
            }
        } else if a.flicker_time >= a.flicker_state_length {
            flicker_toggle(a, live, host, has_on_sound, true);
        }
        a.flicker_time += dt;
    }
}

pub fn spot_near(near: f32, shadow_near: f32) -> f32 {
    if shadow_near > 0.0 {
        min(near, shadow_near)
    } else {
        near
    }
}

/// Returns `(tan(fov/2), cos(fov/2))`.
pub fn spot_half_fov(fov: f32) -> (f32, f32) {
    (tan(fov * 0.5), cos(fov * 0.5))
}

pub fn spot_projection(fov: f32, aspect: f32, near: f32, shadow_near: f32, radius: f32) -> Matrixf {
    perspective_projection(spot_near(near, shadow_near), radius, fov, aspect, false)
}

const TEXTURE_UNIT_FIX: Matrixf = Matrixf {
    m: [
        [0.5, 0.0, 0.0, 0.5],
        [0.0, 0.5, 0.0, 0.5],
        [0.0, 0.0, 0.5, 0.5],
        [0.0, 0.0, 0.0, 1.0],
    ],
};

pub fn spot_view_proj(proj: &Matrixf, view: &Matrixf) -> Matrixf {
    matrix_mul(&TEXTURE_UNIT_FIX, &matrix_mul(proj, view))
}

pub fn box_fade_speed(time: f32) -> f32 {
    if time > 0.0 { 1.0 / time } else { 1e9 }
}

/// One `cLightBox::UpdateLogic` fade step; true once the fade has finished.
pub fn box_fade_step(
    bands: &mut [Vector3f; 9],
    from: &[Vector3f; 9],
    target: &[Vector3f; 9],
    t: &mut f32,
    speed: f32,
    dt: f32,
) -> bool {
    *t = min(fma(speed, dt, *t), 1.0);
    if speed >= 1e9 {
        *t = 1.0;
    }
    let t = *t;
    for ((b, f), g) in bands.iter_mut().zip(from).zip(target) {
        b.x = fma(g.x - f.x, t, f.x);
        b.y = fma(g.y - f.y, t, f.y);
        b.z = fma(g.z - f.z, t, f.z);
    }
    t >= 1.0
}
