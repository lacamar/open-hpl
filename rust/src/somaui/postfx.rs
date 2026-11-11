//! `SomaPostEffects.cpp`: post-effect parameter maths and pass plans. The GPU programs, textures
//! and frame buffers stay C++, which replays the plans below.

use super::hw;
use crate::math::{clamp, matrix_mul, matrix_rotate, matrix_scale, rand_rectf, v3};
use crate::somasound::{fmodf, powf};
use crate::types::{Matrixf, Vector3f};

unsafe extern "C" {
    #[link_name = "expf"]
    fn c_expf(x: f32) -> f32;
}

fn exp(x: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { c_expf(x) }
}

fn smax(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}

pub const PARAMS: usize = 9;

/// Effect types in the order of C++'s `gvTypes`.
pub const KEYS: [i32; 8] = [0, 1, 0, 0, 0, -1, -1, 0];

/// `Set`: the parameter that decides activity is positive. Returns (active, set_clear).
pub fn set_activity(ty: usize, params: &[f32; PARAMS], was_active: bool) -> (bool, bool) {
    // C++ read `mfParams[-1]` for the key-less types; they never go through `Set`
    let key = KEYS.get(ty).copied().unwrap_or(-1);
    let active = usize::try_from(key)
        .ok()
        .and_then(|k| params.get(k))
        .is_some_and(|p| *p > 0.0);
    (active, active && !was_active)
}

/// Hue wheel colour `1 - max(0, min(k, 4 - k, 1))` for the three channels (alpha 1).
pub fn hue(deg: f32) -> [f32; 4] {
    let f = |n: f32| {
        let k = fmodf(n + deg / 60.0, 6.0);
        let mut m = k;
        for c in [4.0 - k, 1.0] {
            if c < m {
                m = c;
            }
        }
        1.0 - smax(0.0, m)
    };
    [f(5.0), f(3.0), f(1.0), 1.0]
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Chromatic {
    pub offset_a: [f32; 2],
    pub offset_b: [f32; 2],
    pub offset_c: [f32; 2],
    pub color_a: [f32; 4],
    pub color_b: [f32; 4],
    pub color_c: [f32; 4],
}

pub fn chromatic(p: &[f32; PARAMS], screen_h: f32, tex: [f32; 2]) -> Chromatic {
    let k = screen_h * p[0];
    let scale = [k / tex[0], k / tex[1]];
    let (sn, cs) = hw::sincos(p[1]);
    let dir = [sn, std::hint::black_box(-cs)];
    let off = [p[3], p[4]];
    let a = [
        hw::mul(hw::add(off[0], dir[0]), scale[0]),
        hw::mul(hw::add(off[1], dir[1]), scale[1]),
    ];
    let b = [hw::mul(scale[0], off[0]), hw::mul(scale[1], off[1])];
    let c = [
        hw::mul(hw::sub(off[0], dir[0]), scale[0]),
        hw::mul(hw::sub(off[1], dir[1]), scale[1]),
    ];
    let cols = [
        hue(fmodf(p[2], 360.0)),
        hue(fmodf(p[2] + 120.0, 360.0)),
        hue(fmodf(p[2] + 240.0, 360.0)),
    ];
    let sum = [
        cols[0][0] + cols[1][0] + cols[2][0],
        cols[0][1] + cols[1][1] + cols[2][1],
        cols[0][2] + cols[1][2] + cols[2][2],
        cols[0][3] + cols[1][3] + cols[2][3],
    ];
    let norm = |c: [f32; 4]| [c[0] / sum[0], c[1] / sum[1], c[2] / sum[2], c[3] / sum[3]];
    Chromatic {
        offset_a: a,
        offset_b: b,
        offset_c: c,
        color_a: norm(cols[0]),
        color_b: norm(cols[1]),
        color_c: norm(cols[2]),
    }
}

/// Image trail blend, applied next frame by the deferred renderer.
pub fn trail_alpha(amount: f32, frame_time: f32, clear: bool) -> f32 {
    if clear {
        return 1.0;
    }
    let e = exp(amount * -60.0 * 0.015);
    smax(0.05, 1.0 - powf(1.0 - e, frame_time * 60.0))
}

pub const BLUR_TAPS: usize = 7;
pub const MAX_LEVELS: usize = 24;
pub const MAX_OPS: usize = 4 * MAX_LEVELS + 8;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct LevelSize {
    pub w: i32,
    pub h: i32,
    pub div: i32,
}

/// Level 0 is the screen, each further level halves it until the width is 8 or less.
pub fn blur_levels(screen: [i32; 2]) -> Vec<LevelSize> {
    let mut l = LevelSize {
        w: screen[0],
        h: screen[1],
        div: 1,
    };
    let mut v = vec![l];
    while l.w > 8 && v.len() < MAX_LEVELS {
        l.div = l.div.wrapping_shl(1);
        l.w = l.w.wrapping_add(1).wrapping_div(2);
        l.h = l.h.wrapping_add(1).wrapping_div(2);
        v.push(l);
    }
    v
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BlurWeights {
    pub weights: [f32; BLUR_TAPS],
    pub off_h: [[f32; 2]; BLUR_TAPS],
    pub off_v: [[f32; 2]; BLUR_TAPS],
}

pub fn blur_weights(d: f32, w: i32, h: i32) -> BlurWeights {
    let mut r = BlurWeights::default();
    let mut sum = 0.0f32;
    for (i, ((wt, oh), ov)) in r
        .weights
        .iter_mut()
        .zip(r.off_h.iter_mut())
        .zip(r.off_v.iter_mut())
        .enumerate()
    {
        if i == 0 {
            *wt = 1.0;
            continue;
        }
        let x = 2.0 * i as f32 - 1.0;
        let w0 = exp(-x * x / d);
        let w1 = exp(-(x + 1.0) * (x + 1.0) / d);
        let total = w0 + w1;
        let off = if total > 0.0 { x + w1 / total } else { x };
        *wt = total;
        sum += total;
        *oh = [off / w as f32, 0.0];
        *ov = [0.0, off / h as f32];
    }
    let norm = 2.0f32.mul_add(sum, 1.0);
    for w in &mut r.weights {
        *w /= norm;
    }
    r
}

pub const OP_DOWNSAMPLE: i32 = 0;
pub const OP_BLUR: i32 = 1;
pub const OP_FINAL: i32 = 2;

pub const PROG_BLUR: i32 = 0;
pub const PROG_PLAIN: i32 = 1;
pub const PROG_COMBINE_MIP: i32 = 2;
pub const PROG_BLUR_COMBINE: i32 = 3;

/// One replayed step. `Downsample`: plain program, draw level `level - 1`'s A texture into level
/// `level`'s A buffer. `Blur`: blur program with the weights `slot`, horizontal reads A and writes
/// B, vertical reads B and writes A. `Final`: draw into the output.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BlurOp {
    pub kind: i32,
    pub level: i32,
    pub vertical: bool,
    pub program: i32,
    /// Weights slot (0 current, 1 second) the program's uniforms come from, -1 for none.
    pub slot: i32,
    pub alpha: f32,
    /// Final: source texture of level `tex0_level` (B when `tex0_b`), plus an optional second one.
    pub tex0_level: i32,
    pub tex0_b: bool,
    pub tex1_level: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct BlurPlan {
    pub n_ops: usize,
    pub ops: [BlurOp; MAX_OPS],
    pub weights: [BlurWeights; 2],
    /// Nothing to draw: the input is the result.
    pub passthrough: bool,
}

impl Default for BlurPlan {
    fn default() -> Self {
        Self {
            n_ops: 0,
            ops: [BlurOp::default(); MAX_OPS],
            weights: [BlurWeights::default(); 2],
            passthrough: true,
        }
    }
}

impl BlurPlan {
    fn push(&mut self, op: BlurOp) {
        if let Some(slot) = self.ops.get_mut(self.n_ops) {
            *slot = op;
            self.n_ops += 1;
        }
    }
}

struct Level {
    size: LevelSize,
    d: f32,
    t: f32,
}

pub fn blur_plan(amount: f32, screen_h: i32, sizes: &[LevelSize]) -> BlurPlan {
    let mut plan = BlurPlan::default();
    let size = screen_h as f32 * amount * 0.25;
    let mut levels: Vec<Level> = Vec::new();
    let mut cur: Option<usize> = None;
    let mut prev: Option<usize> = None;
    for (i, s) in sizes.iter().enumerate() {
        let d = size / s.div as f32;
        let t = clamp(d / 14.0, 0.0, 1.0).mul_add(2.0, -1.0);
        levels.push(Level { size: *s, d, t });
        if prev.is_some() {
            plan.push(BlurOp {
                kind: OP_DOWNSAMPLE,
                level: i as i32,
                program: PROG_PLAIN,
                slot: -1,
                tex1_level: -1,
                ..BlurOp::default()
            });
        }
        if t < 1.0 {
            cur = Some(i);
            break;
        }
        prev = Some(i);
    }
    let Some(mut c) = cur else {
        return plan;
    };
    plan.passthrough = false;
    const CUTOFF: f32 = 0.125;
    if let Some(l) = levels.get_mut(c) {
        l.t = (l.t - CUTOFF) / (1.0 - CUTOFF);
    }
    let mut second = prev;
    let c_t = levels.get(c).map_or(0.0, |l| l.t);
    if let Some(p) = prev
        && c_t <= 0.0
    {
        c = p;
        second = None;
    }
    let cur_t = levels.get(c).map_or(0.0, |l| l.t);
    if cur_t == 1.0 {
        second = None;
    }
    let weights = |i: usize| {
        levels
            .get(i)
            .map(|l| blur_weights(l.d, l.size.w, l.size.h))
            .unwrap_or_default()
    };
    if let Some(w) = plan.weights.get_mut(0) {
        *w = weights(c);
    }
    if let Some(s) = second
        && let Some(w) = plan.weights.get_mut(1)
    {
        *w = weights(s);
    }
    let blur = |plan: &mut BlurPlan, level: Option<usize>, slot: i32, vertical: bool| {
        let Some(l) = level else {
            return;
        };
        // level 0 has no A buffer to write its vertical pass to
        if vertical && l == 0 {
            return;
        }
        plan.push(BlurOp {
            kind: OP_BLUR,
            level: l as i32,
            vertical,
            program: PROG_BLUR,
            slot,
            tex1_level: -1,
            ..BlurOp::default()
        });
    };
    blur(&mut plan, Some(c), 0, false);
    blur(&mut plan, second, 1, false);
    blur(&mut plan, Some(c), 0, true);
    blur(&mut plan, second, 1, true);

    let fin = if let Some(s) = second {
        // level 0 combine uses the current level's offsets and weights, as official
        let level0 = s == 0;
        BlurOp {
            kind: OP_FINAL,
            level: s as i32,
            vertical: level0,
            program: if level0 {
                PROG_BLUR_COMBINE
            } else {
                PROG_COMBINE_MIP
            },
            slot: if level0 { 0 } else { -1 },
            alpha: 1.0 - levels.get(c).map_or(0.0, |l| l.t),
            tex0_level: s as i32,
            tex0_b: level0,
            tex1_level: c as i32,
        }
    } else if c == 0 {
        BlurOp {
            kind: OP_FINAL,
            level: 0,
            vertical: true,
            program: PROG_BLUR,
            slot: 0,
            tex0_level: 0,
            tex0_b: true,
            tex1_level: -1,
            ..BlurOp::default()
        }
    } else {
        BlurOp {
            kind: OP_FINAL,
            level: c as i32,
            program: PROG_PLAIN,
            slot: -1,
            tex0_level: c as i32,
            tex1_level: -1,
            ..BlurOp::default()
        }
    };
    plan.push(fin);
    plan
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TentacleFade {
    pub mat: i32,
    pub t: f32,
    pub speed: f32,
}

/// Pending material fades of the fear tentacles.
#[derive(Default)]
pub struct TentacleFades {
    pub fades: Vec<TentacleFade>,
    pub mat: i32,
}

impl TentacleFades {
    pub fn clear(&mut self) {
        self.fades.clear();
    }

    /// `SetMaterial`; `tentacles` and `materials` are the current counts.
    pub fn set_material(&mut self, tentacles: usize, materials: usize, idx: i32, time: f32) {
        if tentacles == 0 || idx < 0 || idx as usize >= materials {
            return;
        }
        if time <= 0.0 {
            self.mat = idx;
            self.fades.clear();
        } else if idx == self.mat {
            for f in &mut self.fades {
                f.speed = -1.0 / time;
            }
        } else if self.fades.last().is_some_and(|f| f.mat == idx) {
            if let Some(f) = self.fades.last_mut() {
                f.speed = 1.0 / time;
            }
        } else {
            self.fades.push(TentacleFade {
                mat: idx,
                t: 0.0,
                speed: 1.0 / time,
            });
        }
    }

    /// Advances the fades; finished fade-ins become the current material.
    pub fn advance(&mut self, dt: f32) {
        let mut i = 0;
        while let Some(f) = self.fades.get_mut(i) {
            f.t = dt.mul_add(f.speed, f.t);
            if f.speed > 0.0 && f.t >= 1.0 {
                self.mat = f.mat;
                self.fades.remove(i);
            } else if f.speed < 0.0 && f.t <= 0.0 {
                self.fades.remove(i);
            } else {
                i += 1;
            }
        }
    }

    /// Second material and its blend alpha.
    pub fn blend(&self) -> (Option<i32>, f32) {
        self.fades
            .first()
            .map_or((None, 0.0), |f| (Some(f.mat), f.t))
    }
}

/// Animation clock of the tentacles: wraps to zero past 10000.
pub fn tentacle_time(time: f32, dt: f32, speed: f32) -> f32 {
    let t = dt.mul_add(speed, time);
    if t > 10000.0 { 0.0 } else { t }
}

/// Playback speeds spread over the groups of a tentacle animation.
pub fn tentacle_speeds(groups: usize, min: f32, max: f32) -> Vec<f32> {
    let step = (max - min) / groups as f32;
    let mut speed = min + rand_rectf(0.0, step);
    (0..groups)
        .map(|_| {
            speed += step;
            speed
        })
        .collect()
}

/// Render target size: wider than 1920 scales down keeping the aspect.
pub fn tentacle_target(w: i32, h: i32) -> [i32; 2] {
    if w > 1920 {
        [1920, (h.wrapping_mul(1920)).checked_div(w).unwrap_or(0)]
    } else {
        [w, h]
    }
}

pub fn tentacle_model(size: [i32; 2], rot: Vector3f) -> Matrixf {
    let aspect = size[1] as f32 / size[0] as f32;
    matrix_mul(&matrix_scale(v3(aspect, 1.0, 1.0)), &matrix_rotate(rot, 3))
}

pub fn tentacle_pos(pos: Vector3f, spread: f32) -> Vector3f {
    let k = 1.0 - spread;
    v3(pos.x * k, pos.y * k, pos.z * k)
}

pub fn tentacle_aspect(size: [i32; 2]) -> f32 {
    size[0] as f32 / size[1] as f32
}
