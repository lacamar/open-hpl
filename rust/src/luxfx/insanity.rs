//! `LuxInsanityHandler.cpp`: event selection and timing, and the per-event state machines. The
//! XML data, sounds, particles and gui drawing stay C++; random numbers are drawn in the original
//! order.

use super::hw::{madd, msub};
use crate::math::{
    angle_from_points_2d, cos, fma, len3, matrix_mul_vec, normalize3, rand_rectf, rand_rectl, sin,
    v3,
};
use crate::types::{Matrixf, Vector2f};

const TWO_PI: f32 = std::f32::consts::TAU;
const EPSILON: f32 = 0.0001;

/// `cMatrixf` rotation about Y as `cMath::MatrixRotateY` builds it.
fn rotate_y(a: f32) -> Matrixf {
    let (s, c) = (sin(a), cos(a));
    Matrixf {
        m: [
            [c, 0.0, s, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [-s, 0.0, c, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

fn len2(x: f32, y: f32) -> f32 {
    fma(x, x, y * y).sqrt()
}

/// `cVector2f::Normalize` in place.
fn normalize2(v: &mut [f32; 2]) {
    let length = len2(v[0], v[1]);
    if f64::from(length) > 1e-08 {
        let inv = 1.0f32 / length;
        v[0] *= inv;
        v[1] *= inv;
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Bug {
    pub active: u8,
    pub pad: [u8; 3],
    pub pos: [f32; 2],
    pub vel: [f32; 2],
    pub size_mul: f32,
    pub angle: f32,
    pub alpha: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BugsParams {
    pub min_size_mul: f32,
    pub max_size_mul: f32,
    pub wander_dist: f32,
    pub wander_radius: f32,
    pub swarm_mul: f32,
    pub max_speed: f32,
}

/// `OnStart`: scatters the bugs along the gui edges.
pub fn bugs_start(bugs: &mut [Bug], p: &BugsParams, min: [f32; 2], max: [f32; 2]) {
    for b in bugs.iter_mut() {
        let vertical = f64::from(rand_rectf(0.0, 1.0)) > 0.7;
        let pos = if vertical {
            let x = if rand_rectl(0, 1) == 0 {
                min[0]
            } else {
                max[0]
            };
            [x, rand_rectf(min[1], max[1])]
        } else {
            let x = rand_rectf(min[0], max[0]);
            [
                x,
                if rand_rectl(0, 1) == 0 {
                    min[1]
                } else {
                    max[1]
                },
            ]
        };
        b.active = 0;
        b.pad = [0; 3];
        b.alpha = 0.0;
        b.angle = rand_rectf(0.0, TWO_PI);
        b.pos = pos;
        b.vel = [0.0, 0.0];
        b.size_mul = rand_rectf(p.min_size_mul, p.max_size_mul);
    }
}

/// `Update`: returns true when the event is over.
pub fn bugs_update(
    bugs: &mut [Bug],
    count: usize,
    p: &BugsParams,
    dt: f32,
    time: &mut f32,
) -> bool {
    let swarm = [400.0f32, 300.0f32];
    let mut active = true;
    let mut disabled = 0usize;
    *time -= dt;
    if *time < 0.0 {
        *time = 0.0;
        active = false;
    }
    let total = bugs.len();
    for b in bugs.iter_mut().take(count) {
        if active && b.alpha < 1.0 {
            b.alpha = madd(rand_rectf(0.2, 1.0), dt, b.alpha);
            if b.alpha > 1.0 {
                b.alpha = 1.0;
            }
        } else if !active {
            b.alpha = msub(rand_rectf(0.2, 1.0), dt, b.alpha);
            if b.alpha < 0.0 {
                b.alpha = 0.0;
                disabled += 1;
            }
        }
        if len2(b.vel[0], b.vel[1]) > EPSILON {
            let mut dir = b.vel;
            normalize2(&mut dir);
            let angle = rand_rectf(0.0, TWO_PI);
            let force = matrix_mul_vec(&rotate_y(angle), v3(p.wander_radius, 0.0, 0.0));
            let ax = fma(dir[0], p.wander_dist, force.x);
            let ay = fma(dir[1], p.wander_dist, force.z);
            b.vel[0] = fma(ax, dt, b.vel[0]);
            b.vel[1] = fma(ay, dt, b.vel[1]);
        }
        {
            let dist = {
                let dx = swarm[0] - b.pos[0];
                let dy = swarm[1] - b.pos[1];
                fma(dx, dx, dy * dy).sqrt()
            };
            let mut wanted = [swarm[0] - b.pos[0], swarm[1] - b.pos[1]];
            normalize2(&mut wanted);
            wanted[0] *= p.swarm_mul;
            wanted[1] *= p.swarm_mul;
            let m = dist * 0.025f32;
            let m = if m < 1.0 { m } else { 1.0 };
            let acc = [wanted[0] * m, wanted[1] * m];
            b.vel[0] = fma(acc[0], dt, b.vel[0]);
            b.vel[1] = fma(acc[1], dt, b.vel[1]);
        }
        let speed = len2(b.vel[0], b.vel[1]);
        if speed > p.max_speed {
            b.vel = [
                (b.vel[0] / speed) * p.max_speed,
                (b.vel[1] / speed) * p.max_speed,
            ];
        }
        b.pos[0] = fma(b.vel[0], dt, b.pos[0]);
        b.pos[1] = fma(b.vel[1], dt, b.pos[1]);
        if len2(b.vel[0], b.vel[1]) > EPSILON {
            b.angle = angle_from_points_2d(
                Vector2f { x: 0.0, y: 0.0 },
                Vector2f {
                    x: b.vel[0],
                    y: b.vel[1],
                },
            );
        }
    }
    !active && disabled == total
}

/// Countdown shared by the particle event: true once the time ran below zero.
pub fn timer_expired(time: &mut f32, dt: f32) -> bool {
    *time -= dt;
    *time < 0.0
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Stream {
    pub sound_count: f32,
    pub fade_alpha: f32,
    pub fade_alpha_speed: f32,
    pub fade_out_speed: f32,
    pub fade_screen: u32,
}

/// `OnLoadData` derived values: (delay, fade in speed, fade out speed).
pub fn stream_load(delay: f32, fade_in: f32, fade_out: f32) -> (f32, f32, f32) {
    let d = if delay <= 0.0 { 0.001f32 } else { delay };
    let i = if fade_in <= 0.0 { 0.001f32 } else { fade_in };
    let o = if fade_out <= 0.0 { 0.001f32 } else { fade_out };
    (d, 1.0f32 / i, 1.0f32 / o)
}

/// First half of `Update`: true when the stream should start now.
pub fn stream_a(s: &mut Stream, entry_null: bool, dt: f32) -> bool {
    if entry_null && s.sound_count > 0.0 {
        s.sound_count -= dt;
        if s.sound_count <= 0.0 {
            return true;
        }
    }
    false
}

/// Second half of `Update`: the fade and the end test. `entry_gone` is "no entry or it stopped
/// being valid". Returns true when the event is over.
pub fn stream_b(s: &mut Stream, entry_gone: bool, dt: f32) -> bool {
    let mut done = false;
    if s.fade_screen != 0 {
        s.fade_alpha = madd(s.fade_alpha_speed, dt, s.fade_alpha);
        if s.fade_alpha_speed > 0.0 && s.fade_alpha > 1.0 {
            s.fade_alpha = 1.0;
        }
        if s.fade_alpha_speed < 0.0 && s.fade_alpha < 0.0 {
            s.fade_alpha = 0.0;
            done = true;
        }
    }
    if s.sound_count <= 0.0 && entry_gone {
        if s.fade_screen == 0 {
            done = true;
        }
        if s.fade_alpha_speed > 0.0 {
            s.fade_alpha_speed = -s.fade_out_speed;
        }
    }
    done
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Steps {
    pub sound_count: f32,
    pub sound_count_max: f32,
    pub count: i32,
    pub pos: [f32; 3],
    pub time_mul: f32,
    pub dist_mul: f32,
}

/// `OnStart`: a random direction at the configured distance.
pub fn steps_start(s: &mut Steps, step_time: f32, step_num: i32, distance: f32) {
    let m = rotate_y(rand_rectf(0.0, TWO_PI));
    let v = matrix_mul_vec(&m, v3(distance, 0.0, 0.0));
    s.pos = [v.x, v.y, v.z];
    s.sound_count_max = step_time;
    s.sound_count = 0.0;
    s.count = step_num;
}

/// `Update`: returns (create a step sound at the old offset, event over).
pub fn steps_update(s: &mut Steps, dt: f32) -> (bool, bool) {
    if s.sound_count <= 0.0 {
        s.sound_count = s.sound_count_max;
        s.sound_count_max *= s.time_mul;
        let mut n = v3(s.pos[0], s.pos[1], s.pos[2]);
        let length = len3(v3(s.pos[0], s.pos[1], s.pos[2]));
        normalize3(&mut n);
        s.pos = [
            (n.x * length) * s.dist_mul,
            (n.y * length) * s.dist_mul,
            (n.z * length) * s.dist_mul,
        ];
        s.count = s.count.wrapping_sub(1);
        (true, s.count <= 0)
    } else {
        s.sound_count -= dt;
        (false, false)
    }
}

/// Result of picking an event to start.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Pick {
    pub index: i32,
    pub reset_used: u32,
}

/// `StartEvent()` selection. `set_disabled`, `used` and `max_sanity` hold one entry per event; the
/// used flags are all cleared when no enabled event is left unused.
pub fn pick(sanity: f32, set_disabled: &[u32], used: &mut [u32], max_sanity: &[f32]) -> Pick {
    let n = set_disabled.len().min(used.len()).min(max_sanity.len());
    let disabled = |i: usize| {
        set_disabled.get(i).is_some_and(|d| *d != 0)
            || max_sanity.get(i).is_some_and(|m| sanity > *m)
    };
    let mut total = 0i32;
    let mut unused = 0i32;
    for i in 0..n {
        let u = used.get(i).is_some_and(|u| *u != 0);
        let d = disabled(i);
        if !u && !d {
            unused += 1;
        }
        if !d {
            total += 1;
        }
    }
    let none = Pick {
        index: -1,
        reset_used: 0,
    };
    if total == 0 {
        return none;
    }
    let mut reset = 0;
    if unused == 0 {
        for u in used.iter_mut().take(n) {
            *u = 0;
        }
        unused = total;
        reset = 1;
    }
    let idx = rand_rectl(0, unused);
    let mut cur = 0i32;
    for i in 0..n {
        if disabled(i) || used.get(i).is_some_and(|u| *u != 0) {
            continue;
        }
        if cur == idx {
            return Pick {
                index: i as i32,
                reset_used: reset,
            };
        }
        cur += 1;
    }
    Pick {
        index: -1,
        reset_used: reset,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Timing {
    pub max_sanity_long: f32,
    pub max_sanity_med: f32,
    pub max_sanity_short: f32,
    pub time_long: f32,
    pub time_med: f32,
    pub time_short: f32,
}

/// Wait before the next event after one ended, by sanity band.
pub fn wait_after(t: &Timing, sanity: f32) -> f32 {
    if sanity <= t.max_sanity_short {
        t.time_short
    } else if sanity <= t.max_sanity_med {
        t.time_med
    } else {
        t.time_long
    }
}

/// Counts the wait down while no event runs.
pub fn count_down(count: f32, current: i32, dt: f32) -> f32 {
    if count > 0.0 && current < 0 {
        count - dt
    } else {
        count
    }
}

pub fn new_event_possible(t: &Timing, count: f32, sanity: f32, current: i32) -> bool {
    count <= 0.0 && sanity <= t.max_sanity_long && current < 0
}
