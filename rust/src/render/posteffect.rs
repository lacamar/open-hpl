//! Post effect chain scheduling, quad UV math and per-effect draw plans.
//!
//! An effect plan is the exact sequence of `iPostEffect`/`iRenderFunctions` calls the C++
//! `RenderEffect` bodies made; the C++ shim replays it against the real objects. Textures, frame
//! buffers and programs are slot indices into arrays the shim owns (`NONE` is a null pointer).

use crate::types::{Vector2f, Vector2l};

pub const NONE: i32 = -1;

unsafe extern "C" {
    fn expf(x: f32) -> f32;
}

fn exp(x: f32) -> f32 {
    // SAFETY: pure libm function on a plain float.
    unsafe { expf(x) }
}

/// `cMath::Max(float, float)`: the second argument wins ties and NaNs.
fn c_max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Op {
    FlatProjection,
    BlendNone,
    BlendAlpha,
    ChannelRgba,
    TextureRangeNull {
        first: i32,
    },
    CompFb {
        fb: i32,
    },
    CompFbPos {
        fb: i32,
    },
    FinalFb,
    BaseFb {
        fb: i32,
    },
    Program {
        prog: i32,
    },
    ClearColor,
    Tex {
        unit: i32,
        tex: i32,
    },
    Float {
        prog: i32,
        id: i32,
        v: f32,
    },
    Vec2 {
        prog: i32,
        id: i32,
        v: [f32; 2],
    },
    Vec3 {
        prog: i32,
        id: i32,
        v: [f32; 3],
    },
    Quad1 {
        tex: i32,
        flip: bool,
    },
    Quad2 {
        t0: i32,
        t1: i32,
        flip0: bool,
        flip1: bool,
    },
}

pub struct Plan {
    pub result: i32,
    ops: Box<dyn Iterator<Item = Op>>,
}

impl Plan {
    fn new(result: i32, ops: impl Iterator<Item = Op> + 'static) -> Self {
        Self {
            result,
            ops: Box::new(ops),
        }
    }

    pub fn next_op(&mut self) -> Option<Op> {
        self.ops.next()
    }
}

pub fn uv_pos_size(
    tex_size: Vector2f,
    screen: Vector2f,
    rt_pos: Vector2l,
    rt_size: Vector2l,
) -> (Vector2f, Vector2f) {
    let rel_pos = Vector2f {
        x: rt_pos.x as f32 / screen.x,
        y: rt_pos.y as f32 / screen.y,
    };
    let rel_size = Vector2f {
        x: rt_size.x as f32 / screen.x,
        y: rt_size.y as f32 / screen.y,
    };
    (
        Vector2f {
            x: rel_pos.x * tex_size.x,
            y: rel_pos.y * tex_size.y,
        },
        Vector2f {
            x: rel_size.x * tex_size.x,
            y: rel_size.y * tex_size.y,
        },
    )
}

/// `(int)(v + 0.5f)` for both vectors.
pub fn round_target(uv_pos: Vector2f, uv_size: Vector2f) -> (Vector2l, Vector2l) {
    let r = |v: f32| (v + 0.5f32) as i32;
    (
        Vector2l {
            x: r(uv_pos.x),
            y: r(uv_pos.y),
        },
        Vector2l {
            x: r(uv_size.x),
            y: r(uv_size.y),
        },
    )
}

pub fn quad_uv(
    tex_size: Vector2f,
    rect: bool,
    uv_pos: Vector2f,
    uv_size: Vector2f,
) -> (Vector2f, Vector2f) {
    let scale = if rect {
        Vector2f { x: 1.0, y: 1.0 }
    } else {
        Vector2f {
            x: 1.0f32 / tex_size.x,
            y: 1.0f32 / tex_size.y,
        }
    };
    (
        Vector2f {
            x: uv_pos.x * scale.x,
            y: ((tex_size.y - uv_size.y) - uv_pos.y) * scale.y,
        },
        Vector2f {
            x: (uv_pos.x + uv_size.x) * scale.x,
            y: (tex_size.y - uv_pos.y) * scale.y,
        },
    )
}

/// Index at which `cPostEffectComposite::AddPostEffect` inserts: after equal priorities, highest first.
pub fn insert_pos(prios: &[i32], prio: i32) -> usize {
    prios.iter().position(|&e| prio > e).unwrap_or(prios.len())
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Step {
    pub index: u32,
    pub buffer: u32,
}

/// The active effects in chain order, alternating the final temp buffer; the last step is the last effect.
pub fn chain(active: &[bool]) -> Vec<Step> {
    let steps: Vec<Step> = active
        .iter()
        .enumerate()
        .filter(|&(_, &a)| a)
        .enumerate()
        .map(|(n, (i, _))| Step {
            index: i as u32,
            buffer: (n & 1) as u32,
        })
        .collect();
    steps
}

pub mod bloom {
    pub const TEX_INPUT: i32 = 0;
    pub const TEX_BLUR0: i32 = 1;
    pub const TEX_BLUR1: i32 = 2;
    pub const TEX_FINAL: i32 = 3;
    pub const FB_BLUR0: i32 = 0;
    pub const FB_BLUR1: i32 = 1;
    pub const PROG_BLUR0: i32 = 0;
    pub const PROG_BLUR1: i32 = 1;
    pub const PROG_BLOOM: i32 = 2;
    pub const VAR_RGB_TO_INTENSITY: i32 = 0;
    pub const VAR_BLUR_SIZE: i32 = 2;
}

pub mod image_trail {
    pub const TEX_INPUT: i32 = 0;
    pub const TEX_ACCUM: i32 = 1;
    pub const FB_ACCUM: i32 = 0;
    pub const PROG: i32 = 0;
    pub const VAR_ALPHA: i32 = 0;
}

pub mod radial_blur {
    pub const TEX_INPUT: i32 = 0;
    pub const TEX_FINAL: i32 = 1;
    pub const PROG: i32 = 0;
    pub const VAR_SIZE: i32 = 0;
    pub const VAR_HALF_SCREEN_SIZE: i32 = 1;
    pub const VAR_BLUR_START_DIST: i32 = 2;
}

pub mod color_conv {
    pub const TEX_INPUT: i32 = 0;
    pub const TEX_LUT: i32 = 1;
    pub const TEX_FINAL: i32 = 2;
    pub const PROG_PLAIN: i32 = 0;
    pub const PROG_FADE: i32 = 1;
    pub const VAR_FADE_ALPHA: i32 = 0;
}

pub mod final_copy {
    pub const TEX_OUT: i32 = 0;
    pub const FB_RENDER_TARGET: i32 = 0;
}

pub struct BloomIn {
    pub blur_size: f32,
    pub iterations: i32,
    pub rgb_to_intensity: [f32; 3],
    pub programs_ok: bool,
}

fn bloom_blur(t: i32) -> [Op; 8] {
    use bloom::*;
    [
        Op::BaseFb { fb: FB_BLUR0 },
        Op::Program { prog: PROG_BLUR0 },
        Op::Tex { unit: 0, tex: t },
        Op::Quad1 { tex: t, flip: true },
        Op::BaseFb { fb: FB_BLUR1 },
        Op::Program { prog: PROG_BLUR1 },
        Op::Tex {
            unit: 0,
            tex: TEX_BLUR0,
        },
        Op::Quad1 {
            tex: TEX_BLUR0,
            flip: true,
        },
    ]
}

pub fn plan_bloom(p: &BloomIn) -> Plan {
    use bloom::*;
    if !p.programs_ok {
        return Plan::new(TEX_INPUT, std::iter::empty());
    }
    let pre = [
        Op::FlatProjection,
        Op::BlendNone,
        Op::ChannelRgba,
        Op::Float {
            prog: PROG_BLUR1,
            id: VAR_BLUR_SIZE,
            v: p.blur_size,
        },
        Op::Float {
            prog: PROG_BLUR0,
            id: VAR_BLUR_SIZE,
            v: p.blur_size,
        },
    ];
    let post = [
        Op::FinalFb,
        Op::Tex {
            unit: 0,
            tex: TEX_BLUR1,
        },
        Op::Tex {
            unit: 1,
            tex: TEX_INPUT,
        },
        Op::Program { prog: PROG_BLOOM },
        Op::Vec3 {
            prog: PROG_BLOOM,
            id: VAR_RGB_TO_INTENSITY,
            v: p.rgb_to_intensity,
        },
        Op::Quad2 {
            t0: TEX_BLUR1,
            t1: TEX_INPUT,
            flip0: true,
            flip1: true,
        },
    ];
    let ops = pre
        .into_iter()
        .chain(bloom_blur(TEX_INPUT))
        .chain((1..p.iterations).flat_map(|_| bloom_blur(TEX_BLUR1)))
        .chain(post);
    Plan::new(TEX_FINAL, ops)
}

pub struct ImageTrailIn {
    pub amount: f32,
    pub frame_time: f32,
    pub program_ok: bool,
}

/// `clear` is the effect's `mbClearFrameBuffer`, consumed by the plan.
pub fn plan_image_trail(p: &ImageTrailIn, clear: &mut bool) -> Plan {
    use image_trail::*;
    let prog = if p.program_ok { PROG } else { NONE };
    let alpha = if *clear {
        1.0f32
    } else {
        let pow = (1.0f32 / p.frame_time) * p.amount;
        // gcc negates the product (fnmul), flipping a NaN's sign; LLVM would fold the negation into a multiply.
        exp(std::hint::black_box(-std::hint::black_box(pow)) * 0.015f32)
    };
    let mut ops = vec![
        Op::FlatProjection,
        Op::BlendAlpha,
        Op::ChannelRgba,
        Op::TextureRangeNull { first: 0 },
        Op::CompFb { fb: FB_ACCUM },
        Op::Program { prog },
    ];
    if *clear {
        ops.push(Op::ClearColor);
    }
    if p.program_ok {
        ops.push(Op::Float {
            prog,
            id: VAR_ALPHA,
            v: alpha,
        });
    }
    *clear = false;
    ops.extend([
        Op::Tex {
            unit: 0,
            tex: TEX_INPUT,
        },
        Op::Quad1 {
            tex: TEX_INPUT,
            flip: true,
        },
        Op::Program { prog: NONE },
        Op::BlendNone,
    ]);
    Plan::new(TEX_ACCUM, ops.into_iter())
}

pub struct RadialBlurIn {
    pub size: f32,
    pub blur_start_dist: f32,
    pub target_size: Vector2l,
    pub program_ok: bool,
}

pub fn plan_radial_blur(p: &RadialBlurIn) -> Plan {
    use radial_blur::*;
    let prog = if p.program_ok { PROG } else { NONE };
    let w = p.target_size.x as f32;
    let h = p.target_size.y as f32;
    let mut ops = vec![
        Op::FlatProjection,
        Op::BlendNone,
        Op::ChannelRgba,
        Op::TextureRangeNull { first: 1 },
        Op::FinalFb,
        Op::Program { prog },
    ];
    if p.program_ok {
        ops.extend([
            Op::Float {
                prog,
                id: VAR_SIZE,
                v: p.size * w,
            },
            Op::Float {
                prog,
                id: VAR_BLUR_START_DIST,
                v: p.blur_start_dist,
            },
            Op::Vec2 {
                prog,
                id: VAR_HALF_SCREEN_SIZE,
                v: [w * 0.5f32, h * 0.5f32],
            },
        ]);
    }
    ops.extend([
        Op::Tex {
            unit: 0,
            tex: TEX_INPUT,
        },
        Op::Quad1 {
            tex: TEX_INPUT,
            flip: true,
        },
        Op::Program { prog: NONE },
        Op::BlendNone,
    ]);
    Plan::new(TEX_FINAL, ops.into_iter())
}

pub struct ColorConvIn {
    pub fade_alpha: f32,
    pub programs_ok: [bool; 2],
}

pub fn plan_color_conv(p: &ColorConvIn) -> Plan {
    use color_conv::*;
    let slot = |i: i32, ok: bool| if ok { i } else { NONE };
    let mut ops = vec![
        Op::FlatProjection,
        Op::BlendNone,
        Op::ChannelRgba,
        Op::FinalFb,
        Op::Tex {
            unit: 0,
            tex: TEX_INPUT,
        },
        Op::Tex {
            unit: 1,
            tex: TEX_LUT,
        },
    ];
    if p.fade_alpha >= 1.0 {
        ops.push(Op::Program {
            prog: slot(PROG_PLAIN, p.programs_ok[0]),
        });
    } else {
        let fade = slot(PROG_FADE, p.programs_ok[1]);
        ops.push(Op::Program { prog: fade });
        if p.programs_ok[1] {
            ops.push(Op::Float {
                prog: fade,
                id: VAR_FADE_ALPHA,
                v: c_max(p.fade_alpha, 0.0f32),
            });
        }
    }
    ops.push(Op::Quad1 {
        tex: TEX_INPUT,
        flip: true,
    });
    Plan::new(TEX_FINAL, ops.into_iter())
}

/// Copy of the last effect's output onto the render target when it never set the final buffer.
pub fn plan_final_copy() -> Plan {
    use final_copy::*;
    Plan::new(
        TEX_OUT,
        [
            Op::Program { prog: NONE },
            Op::BlendNone,
            Op::CompFbPos {
                fb: FB_RENDER_TARGET,
            },
            Op::Tex {
                unit: 0,
                tex: TEX_OUT,
            },
            Op::Quad1 {
                tex: TEX_OUT,
                flip: true,
            },
        ]
        .into_iter(),
    )
}
