//! `CopyAccumTo`, `RenderImageTrail`, `RenderBloom`, tone mapping and film grain of `cRendererDeferred`.

use super::deferred::*;
use crate::types::{Vector2f, Vector2l};

unsafe extern "C" {
    fn fmodf(x: f32, y: f32) -> f32;
    fn sqrtf(x: f32) -> f32;
}

fn fmod(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { fmodf(x, y) }
}

fn sqrt(x: f32) -> f32 {
    // SAFETY: pure libm function on a plain float.
    unsafe { sqrtf(x) }
}

const PI: f32 = std::f32::consts::PI;

impl FilmGrain {
    /// `cRendererDeferred::RandomizeFilmGrain`.
    fn randomize(&mut self, screen_x: f32, noise_width: i32) {
        let [old_x, old_y] = [self.transform[0][0], self.transform[0][1]];
        self.transform[0] = self.transform[1];
        let scale = screen_x / noise_width as f32;
        let sign = |r: f32| if r >= 0.5f32 { 1.0f32 } else { -1.0f32 };
        let t1 = &mut self.transform[1];
        t1[2] = sign(rand_rectf(0.0, 1.0)) * scale;
        t1[3] = sign(rand_rectf(0.0, 1.0)) * scale;
        t1[0] = fmod(old_x + rand_rectf(0.25f32, 0.75f32), 1.0);
        t1[1] = fmod(old_y + rand_rectf(0.25f32, 0.75f32), 1.0);
    }
}

#[derive(Clone, Debug, Default)]
pub struct CopyIn {
    pub screen: Vector2l,
    pub screen_f: Vector2f,
    pub rt_pos: Vector2l,
    pub rt_size: Vector2l,
    pub rect_type: bool,
    pub use_fxaa: bool,
    pub fxaa_ok: bool,
    pub tonemap_ok: bool,
    pub trail_ok: bool,
    pub trail_alpha: f32,
    pub skip_passes: i32,
    pub hpl35: bool,
    pub layer_weights: Vec<f32>,
    pub grading: bool,
    pub grading_target: bool,
    pub noise: bool,
    pub noise_width: i32,
    pub bloom: bool,
    pub bloom_programs_ok: bool,
    pub srgb: bool,
    pub key: f32,
    pub exposure: f32,
    pub white_cut: f32,
    pub gamma: f32,
    pub bloom_bright: f32,
    pub bloom_width: f32,
    pub bloom_tint: [f32; 4],
    pub grading_blend: f32,
    pub film_intensity: f32,
    pub frame_time: f32,
    pub blur_samples: i32,
}

#[derive(Clone, Copy, PartialEq)]
enum Stage {
    Start,
    Headroom,
    ToneMapTry(u8),
    Copy,
}

struct CopyAccum {
    i: CopyIn,
    stage: Stage,
    combo: i32,
    fxaa: bool,
    headroom: f32,
    source: i32,
}

/// `CopyAccumTo`; `film_grain` is the renderer's current state, updated in `Plan::res`.
pub fn plan_copy_accum(i: CopyIn, film_grain: FilmGrain) -> Plan {
    let mut plan = Plan::new(CopyAccum {
        i,
        stage: Stage::Start,
        combo: 0,
        fxaa: false,
        headroom: 0.0,
        source: tex::ACCUM,
    });
    plan.res.film_grain = film_grain;
    plan
}

fn quad(i: &CopyIn) -> Op {
    let (min, max) = accum_quad_uv(i.rt_pos, i.rt_size, i.screen_f, i.rect_type);
    Op::Quad { min, max }
}

const UNIT_QUAD: Op = Op::Quad {
    min: [0.0, 0.0],
    max: [1.0, 1.0],
};

fn inv_screen(i: &CopyIn) -> [f32; 2] {
    [1.0f32 / i.screen_f.x, 1.0f32 / i.screen_f.y]
}

impl CopyAccum {
    fn image_trail(&self, out: &mut Out<'_>) {
        let i = &self.i;
        let alpha = c_min(i.trail_alpha, 1.0);
        out.extend([
            Op::AcquireTemp {
                fb: fb::TRAIL,
                tex: tex::TRAIL,
                size: i.screen,
                fmt: pf::RGBA16,
                idx: 8,
            },
            Op::SetFb {
                fb: fb::TRAIL,
                use_pos: false,
            },
            Op::FlatProjection,
            Op::Program { prog: prog::TRAIL },
            Op::Float {
                prog: prog::TRAIL,
                id: var::AF_ALPHA,
                v: alpha,
            },
            Op::State {
                which: State::Blend,
                val: if i.trail_alpha >= 1.0 {
                    st::BLEND_NONE
                } else {
                    st::BLEND_ALPHA
                },
            },
            Op::Tex {
                unit: 0,
                tex: tex::ACCUM,
            },
            Op::TexRangeNull { first: 1 },
            quad(i),
            Op::SetFb {
                fb: fb::ACCUM,
                use_pos: false,
            },
            Op::FlatProjection,
            Op::Float {
                prog: prog::TRAIL,
                id: var::AF_ALPHA,
                v: 1.0,
            },
            Op::State {
                which: State::Blend,
                val: st::BLEND_NONE,
            },
            Op::Tex {
                unit: 0,
                tex: tex::TRAIL,
            },
            quad(i),
        ]);
        out.res.trail_consumed = true;
    }

    fn bloom(&self, out: &mut Out<'_>) {
        let i = &self.i;
        let bloom_fb = |n: i32| fb::BLOOM0 + n;
        let bloom_tex = |n: i32| tex::BLOOM0 + n;
        for n in 0..6 {
            out.push(Op::AcquireTemp {
                fb: bloom_fb(n),
                tex: bloom_tex(n),
                size: div_size(i.screen, 4 << (n >> 1)),
                fmt: pf::RGBA16,
                idx: 10 + n,
            });
        }
        out.extend([
            Op::SetFb {
                fb: bloom_fb(1),
                use_pos: false,
            },
            Op::FlatProjection,
            Op::Program { prog: prog::BRIGHT },
            Op::Float {
                prog: prog::BRIGHT,
                id: var::AV_BRIGHT_PASS,
                v: i.white_cut * 8.0f32 * i.bloom_bright,
            },
            Op::Vec2 {
                prog: prog::BRIGHT,
                id: var::AV_INV_SCREEN_SIZE,
                v: inv_screen(i),
            },
            Op::Tex {
                unit: 0,
                tex: tex::ACCUM,
            },
            Op::TexRangeNull { first: 1 },
            UNIT_QUAD,
        ]);

        let n = usize::try_from(i.blur_samples).unwrap_or(0).max(1);
        let blur = |out: &mut Out<'_>, dir: i32, weights: &[f32], offsets: &[f32]| {
            let p = prog::BLUR0 + dir;
            let (wa, wl) = out.array(weights.to_vec());
            let (oa, ol) = out.array(offsets.to_vec());
            out.extend([
                Op::FloatArr {
                    prog: p,
                    id: var::AV_WEIGHTS,
                    arr: wa,
                    len: wl,
                },
                Op::FloatArr {
                    prog: p,
                    id: var::AV_OFFSETS,
                    arr: oa,
                    len: ol,
                },
            ]);
        };
        let mut weights = vec![0.0f32; n];
        for level in 0..3i32 {
            if level > 0 {
                out.extend([
                    Op::SetFb {
                        fb: bloom_fb(2 * level + 1),
                        use_pos: false,
                    },
                    Op::FlatProjection,
                    Op::Program { prog: prog::BLUR0 },
                ]);
                let mut identity = vec![0.0f32; n];
                if let Some(w) = identity.first_mut() {
                    *w = 1.0;
                }
                blur(out, 0, &identity, &vec![0.0f32; n]);
                out.extend([
                    Op::Tex {
                        unit: 0,
                        tex: bloom_tex(2 * level - 1),
                    },
                    UNIT_QUAD,
                ]);
            }

            let level_size = div_size(i.screen, 4 << level);
            let s = c_min(
                40.0f32,
                i.screen_f.x / 1280.0f32 * i.bloom_width / (4 << level) as f32,
            );
            let inv = [1.0f32 / level_size.x as f32, 1.0f32 / level_size.y as f32];
            let mut off_v = vec![0.0f32; n];
            let mut off_h = vec![0.0f32; n];
            if let Some(w) = weights.first_mut() {
                *w = 1.0;
            }
            let mut sum = 1.0f32;
            for k in 1..n {
                let x = 2.0f32 * k as f32 - 1.0f32;
                let a = exp(-x * x / s);
                let b = exp(-(x + 1.0f32) * (x + 1.0f32) / s);
                let w = a + b;
                let offset = b / (a + b) + x;
                if let (Some(slot), Some(o0), Some(o1)) =
                    (weights.get_mut(k), off_v.get_mut(k), off_h.get_mut(k))
                {
                    *slot = w;
                    *o0 = offset * inv[1];
                    *o1 = offset * inv[0];
                }
                sum = 2.0f32.mul_add(w, sum);
            }
            for w in &mut weights {
                *w /= sum;
            }

            for dir in 0..2i32 {
                out.extend([
                    Op::SetFb {
                        fb: bloom_fb(2 * level + dir),
                        use_pos: false,
                    },
                    Op::FlatProjection,
                    Op::Program {
                        prog: prog::BLUR0 + dir,
                    },
                ]);
                let off = if dir == 0 { &off_v } else { &off_h };
                blur(out, dir, &weights, off);
                out.extend([
                    Op::Tex {
                        unit: 0,
                        tex: bloom_tex(2 * level + 1 - dir),
                    },
                    UNIT_QUAD,
                ]);
            }
        }
    }

    fn tonemap(&mut self, out: &mut Out<'_>) {
        let combo = self.combo;
        let i = self.i.clone();
        let target = if self.fxaa { fb::TONEMAP } else { fb::TARGET };
        let set_target = Op::SetFb {
            fb: target,
            use_pos: !self.fxaa,
        };
        if combo & 2 != 0 {
            self.bloom(out);
            out.extend([set_target, Op::FlatProjection]);
        }

        let grading_unit = if i.hpl35 { 4 } else { 1 };
        let bloom_unit = if i.hpl35 { 1 } else { 4 };
        let p = prog::TONEMAP;
        out.extend([
            Op::Program { prog: p },
            Op::Float {
                prog: p,
                id: var::AF_KEY,
                v: i.key,
            },
            Op::Float {
                prog: p,
                id: var::AF_EXPOSURE,
                v: i.exposure,
            },
            Op::Float {
                prog: p,
                id: var::AF_WHITE_CUT,
                v: i.white_cut,
            },
            Op::Float {
                prog: p,
                id: var::AF_INV_GAMMA,
                v: 1.0f32 / i.gamma,
            },
        ]);
        if combo & 32 != 0 {
            out.push(Op::Float {
                prog: p,
                id: var::AF_HDR_HEADROOM,
                v: self.headroom,
            });
        }
        out.extend([
            Op::Tex {
                unit: 0,
                tex: tex::ACCUM,
            },
            Op::TexRangeNull { first: 1 },
        ]);
        if combo & 1 != 0 {
            out.extend([
                Op::Tex {
                    unit: grading_unit,
                    tex: tex::GRADING,
                },
                Op::Float {
                    prog: p,
                    id: var::AF_GRADING_ALPHA0,
                    v: 1.0,
                },
            ]);
            let mut num = 1i32;
            if combo & 16 != 0 && i.grading_target {
                out.extend([
                    Op::Tex {
                        unit: grading_unit + num,
                        tex: tex::GRADING_TARGET,
                    },
                    Op::Float {
                        prog: p,
                        id: var::AF_GRADING_BLEND,
                        v: i.grading_blend,
                    },
                ]);
                num += 1;
            }
            let layers = if i.hpl35 { i.layer_weights.len() } else { 0 };
            let mut k = 0usize;
            while combo & 16 != 0 && k < layers && num < 8 {
                let id = if num == 1 {
                    var::AF_GRADING_BLEND
                } else {
                    var::AF_GRADING_ALPHA2 + num - 2
                };
                out.extend([
                    Op::Tex {
                        unit: grading_unit + num,
                        tex: tex::LAYER0 + k as i32,
                    },
                    Op::Float {
                        prog: p,
                        id,
                        v: i.layer_weights.get(k).copied().unwrap_or(0.0),
                    },
                ]);
                k += 1;
                num += 1;
            }
            out.push(Op::Int {
                prog: p,
                id: var::AL_GRADING_NUM,
                v: num,
            });
        }
        if combo & 2 != 0 {
            for n in 0..3 {
                out.push(Op::Tex {
                    unit: bloom_unit + n,
                    tex: tex::BLOOM0 + 2 * n + 1,
                });
            }
            let t = i.bloom_tint;
            out.push(Op::Vec4 {
                prog: p,
                id: var::AV_BLOOM_TINT,
                v: [t[0] * t[0], t[1] * t[1], t[2] * t[2], t[3]],
            });
            let mut w = [0.0f32; 3];
            for (n, slot) in w.iter_mut().enumerate() {
                let d = (4 << n) as f32;
                let f = c_clamp((i.bloom_width - d) / d, 0.0, 1.0);
                *slot = f * f;
            }
            out.push(Op::Vec3 {
                prog: p,
                id: var::AV_SIZE_WEIGHT,
                v: w,
            });
        }
        if combo & 4 != 0 {
            let mut g = out.res.film_grain;
            if g.t == -1.0 {
                g.randomize(i.screen_f.x, i.noise_width);
                g.randomize(i.screen_f.x, i.noise_width);
                g.t = 0.0;
            }
            let ft = g.t + i.frame_time * 49.0f32 / PI;
            if ft as i32 != g.t as i32 {
                g.randomize(i.screen_f.x, i.noise_width);
            }
            g.t = fmod(ft, 128.0);
            let fi = i.film_intensity;
            if !i.hpl35 {
                out.push(Op::Tex {
                    unit: 3,
                    tex: tex::NOISE,
                });
            }
            let k = sqrt(i.exposure.mul_add(0.125f32, 0.375f32));
            let [t0, t1] = g.transform;
            out.res.film_grain = g;
            out.extend([
                Op::Float {
                    prog: p,
                    id: var::AF_T,
                    v: fmod(ft, 1.0),
                },
                Op::Float {
                    prog: p,
                    id: var::AF_INTENSITY,
                    v: c_max(
                        -1.0,
                        1.13f32
                            .mul_add(fi, -(3.51f32 * fi))
                            .mul_add(k, 3.51f32 * fi),
                    ),
                },
                Op::Vec4 {
                    prog: p,
                    id: var::AV_TRANSFORM0,
                    v: t0,
                },
                Op::Vec4 {
                    prog: p,
                    id: var::AV_TRANSFORM1,
                    v: t1,
                },
                Op::Vec2 {
                    prog: p,
                    id: var::AV_INV_SCREEN_SIZE,
                    v: inv_screen(&i),
                },
            ]);
        }
        if self.fxaa {
            out.push(UNIT_QUAD);
            self.source = tex::TONEMAP_OUT;
        } else {
            out.push(quad(&i));
            self.source = NONE;
        }
    }
}

impl Source for CopyAccum {
    fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool {
        match self.stage {
            Stage::Start => {
                out.extend([
                    Op::State {
                        which: State::DepthTest,
                        val: 0,
                    },
                    Op::State {
                        which: State::DepthWrite,
                        val: 0,
                    },
                    Op::State {
                        which: State::Blend,
                        val: st::BLEND_NONE,
                    },
                    Op::State {
                        which: State::Alpha,
                        val: st::ALPHA_SOLID,
                    },
                    Op::State {
                        which: State::Channel,
                        val: st::CHANNEL_RGBA,
                    },
                ]);
                if self.i.tonemap_ok {
                    if self.i.trail_alpha > 0.0 && self.i.trail_ok && self.i.skip_passes & 16 == 0 {
                        self.image_trail(out);
                    }
                    self.fxaa = self.i.use_fxaa && self.i.fxaa_ok;
                    out.push(Op::Ask(Query::Headroom));
                    self.stage = Stage::Headroom;
                } else {
                    self.stage = Stage::Copy;
                }
                true
            }
            Stage::Headroom => {
                let i = &self.i;
                self.headroom = feed.map_or(0.0, Feed::float);
                let h = self.headroom;
                if self.fxaa {
                    out.extend([
                        Op::AcquireTemp {
                            fb: fb::TONEMAP,
                            tex: tex::TONEMAP_OUT,
                            size: i.screen,
                            fmt: if h > 1.0 { pf::RGBA16 } else { pf::RGBA },
                            idx: 5,
                        },
                        Op::SetFb {
                            fb: fb::TONEMAP,
                            use_pos: false,
                        },
                    ]);
                } else {
                    out.push(Op::SetFb {
                        fb: fb::TARGET,
                        use_pos: true,
                    });
                }
                out.push(Op::FlatProjection);
                let layers = if i.hpl35 { i.layer_weights.len() } else { 0 };
                self.combo = i32::from(i.grading)
                    | if i.bloom && i.bloom_programs_ok { 2 } else { 0 }
                    | if i.noise { 4 } else { 0 }
                    | if i.srgb { 8 } else { 0 }
                    | if i.grading && (i.grading_target || layers != 0) {
                        16
                    } else {
                        0
                    }
                    | if h > 1.0 { 32 } else { 0 };
                out.push(Op::TryToneMap { combo: self.combo });
                self.stage = Stage::ToneMapTry(0);
                true
            }
            Stage::ToneMapTry(attempt) => {
                if feed.is_some_and(Feed::flag) {
                    self.tonemap(out);
                    self.stage = Stage::Copy;
                    return true;
                }
                match attempt {
                    0 => self.combo &= 1,
                    1 => self.combo = 0,
                    _ => {
                        // C++ dereferenced the null program; skip the tone map and copy the accumulation buffer.
                        self.stage = Stage::Copy;
                        return true;
                    }
                }
                out.push(Op::TryToneMap { combo: self.combo });
                self.stage = Stage::ToneMapTry(attempt + 1);
                true
            }
            Stage::Copy => {
                if self.source != NONE {
                    let i = &self.i;
                    out.extend([
                        Op::SetFb {
                            fb: fb::TARGET,
                            use_pos: true,
                        },
                        Op::FlatProjection,
                        Op::Program {
                            prog: if i.use_fxaa { prog::FXAA } else { NONE },
                        },
                    ]);
                    if i.use_fxaa && i.fxaa_ok {
                        out.push(Op::Vec2 {
                            prog: prog::FXAA,
                            id: var::AV_INV_SCREEN_SIZE,
                            v: inv_screen(i),
                        });
                    }
                    out.extend([
                        Op::Tex {
                            unit: 0,
                            tex: self.source,
                        },
                        Op::TexRangeNull { first: 1 },
                        quad(i),
                    ]);
                }
                out.push(Op::Program { prog: NONE });
                false
            }
        }
    }
}
