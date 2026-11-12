//! `cLuxEffectRenderer` draw plans: the exact `iRenderFunctions` call sequences of the flash,
//! enemy glow and outline passes. Objects, programs, textures and frame buffers are indices into
//! arrays the C++ shim owns; the shim replays the plan.

use super::hw::{madd, msub};
use crate::math::{expand_aabb, matrix_mul, matrix_scale, matrix_translate};
use crate::types::{Matrixf, Vector3f};

pub mod op {
    pub const DEPTH_FUNC: u32 = 0;
    pub const DEPTH_TEST: u32 = 1;
    pub const DEPTH_WRITE: u32 = 2;
    pub const BLEND: u32 = 3;
    pub const ALPHA_MODE: u32 = 4;
    pub const CHANNEL: u32 = 5;
    pub const PROGRAM: u32 = 6;
    pub const PROG_FLOAT: u32 = 7;
    pub const PROG_COLOR3: u32 = 8;
    pub const TEX_DIFFUSE: u32 = 9;
    pub const TEX_ALPHA: u32 = 10;
    pub const TEX_NULL: u32 = 11;
    pub const TEX_SLOT: u32 = 12;
    pub const VERTEX_OBJ: u32 = 13;
    pub const MATRIX_OBJ: u32 = 14;
    pub const MATRIX_NULL: u32 = 15;
    pub const MATRIX_TEMP: u32 = 16;
    pub const DRAW: u32 = 17;
    pub const FRAME_BUFFER: u32 = 18;
    pub const CLEAR_STENCIL_COLOR: u32 = 19;
    pub const STENCIL_ACTIVE: u32 = 20;
    pub const STENCIL_ALWAYS_REPLACE: u32 = 21;
    pub const STENCIL_NOT_EQUAL_KEEP: u32 = 22;
    pub const FLAT_PROJECTION: u32 = 23;
    pub const NORMAL_PROJECTION: u32 = 24;
    pub const TEXTURE_RANGE_NULL: u32 = 25;
    pub const SCISSOR_ACTIVE: u32 = 26;
    pub const SCISSOR_RECT: u32 = 27;
    pub const QUAD_MAX_UV: u32 = 28;
    pub const QUAD_MIN_MAX: u32 = 29;
}

pub const DEPTH_EQUAL: i32 = 0;
pub const DEPTH_LESS_OR_EQUAL: i32 = 1;
pub const BLEND_NONE: i32 = 0;
pub const BLEND_ADD: i32 = 1;
pub const ALPHA_SOLID: i32 = 0;
pub const ALPHA_TRANS: i32 = 1;
pub const CHANNEL_RGBA: i32 = 0;
pub const CHANNEL_NONE: i32 = 1;

pub const PROG_FLASH: i32 = 0;
pub const PROG_GLOW: i32 = 1;
pub const PROG_OUTLINE_COLOR0: i32 = 2;
pub const PROG_OUTLINE_COLOR1: i32 = 3;
pub const PROG_OUTLINE_STENCIL: i32 = 4;
pub const PROG_OUTLINE_STENCIL_ALPHA: i32 = 5;
pub const PROG_BLUR0: i32 = 6;
pub const PROG_BLUR1: i32 = 7;
pub const NONE: i32 = -1;

pub const TEX_OUTLINE_COLOR: i32 = 0;
pub const TEX_BLUR0: i32 = 1;
pub const TEX_BLUR1: i32 = 2;

pub const FB_COLOR: i32 = 0;
pub const FB_BLUR0: i32 = 1;
pub const FB_BLUR1: i32 = 2;
pub const FB_ACCUM: i32 = 3;

const VAR_BLUR_SIZE: i32 = 0;
const VAR_COLOR_MUL: i32 = 1;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Op {
    pub kind: u32,
    pub i: i32,
    pub j: i32,
    pub f: [f32; 16],
}

pub struct Plan {
    ops: Vec<Op>,
    pos: usize,
    pub clip_min: Vector3f,
    pub clip_max: Vector3f,
}

impl Plan {
    fn new() -> Self {
        Self {
            ops: Vec::new(),
            pos: 0,
            clip_min: Vector3f::default(),
            clip_max: Vector3f::default(),
        }
    }

    fn p(&mut self, kind: u32, i: i32, j: i32) {
        self.ops.push(Op {
            kind,
            i,
            j,
            f: [0.0; 16],
        });
    }

    fn pf(&mut self, kind: u32, i: i32, j: i32, v: f32) {
        self.ops.push(Op {
            kind,
            i,
            j,
            f: [
                v, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            ],
        });
    }

    pub fn next_op(&mut self) -> Option<Op> {
        let op = self.ops.get(self.pos).copied();
        self.pos += 1;
        op
    }

    #[cfg(test)]
    pub fn ops(&self) -> &[Op] {
        &self.ops
    }
}

/// One glow object: culling result and alpha.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Glow {
    pub collides: u32,
    pub alpha: f32,
}

fn glow_pass(objs: &[Glow], program: i32, ok: bool, osc: Option<f32>, draws: usize) -> Plan {
    let mut p = Plan::new();
    if objs.is_empty() || !ok {
        return p;
    }
    p.p(op::DEPTH_FUNC, DEPTH_EQUAL, 0);
    p.p(op::DEPTH_TEST, 1, 0);
    p.p(op::DEPTH_WRITE, 0, 0);
    p.p(op::BLEND, BLEND_ADD, 0);
    p.p(op::ALPHA_MODE, ALPHA_SOLID, 0);
    p.p(op::CHANNEL, CHANNEL_RGBA, 0);
    p.p(op::PROGRAM, program, 0);
    let global = osc.map(|v| madd(v, 0.5f32, 0.5f32));
    for (n, o) in objs.iter().enumerate() {
        if o.collides == 0 {
            continue;
        }
        let v = match global {
            Some(g) => o.alpha * g,
            None => o.alpha,
        };
        p.pf(op::PROG_FLOAT, program, VAR_COLOR_MUL, v);
        p.p(op::TEX_DIFFUSE, n as i32, 0);
        p.p(op::VERTEX_OBJ, n as i32, 0);
        p.p(op::MATRIX_OBJ, n as i32, 0);
        for _ in 0..draws {
            p.p(op::DRAW, 0, 0);
        }
    }
    p.p(op::TEX_NULL, 0, 0);
    p.p(op::PROGRAM, NONE, 0);
    p.p(op::DEPTH_FUNC, DEPTH_LESS_OR_EQUAL, 0);
    p
}

pub fn plan_flash(objs: &[Glow], osc_val: f32, program_ok: bool) -> Plan {
    glow_pass(objs, PROG_FLASH, program_ok, Some(osc_val), 2)
}

pub fn plan_glow(objs: &[Glow], program_ok: bool) -> Plan {
    glow_pass(objs, PROG_GLOW, program_ok, None, 1)
}

/// One outline object: culling, alpha texture, bounds and world matrix.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Outline {
    pub collides: u32,
    pub has_alpha: u32,
    pub min: Vector3f,
    pub max: Vector3f,
    pub local_min: Vector3f,
    pub local_max: Vector3f,
    pub local_center: Vector3f,
    pub world: Matrixf,
}

fn blur(p: &mut Plan, input: i32) {
    p.p(op::FRAME_BUFFER, FB_BLUR0, 0);
    p.p(op::PROGRAM, PROG_BLUR0, 0);
    p.p(op::TEX_SLOT, input, 0);
    p.p(op::QUAD_MAX_UV, input, 0);
    p.p(op::FRAME_BUFFER, FB_BLUR1, 0);
    p.p(op::PROGRAM, PROG_BLUR1, 0);
    p.p(op::TEX_SLOT, TEX_BLUR0, 0);
    p.p(op::QUAD_MAX_UV, TEX_BLUR0, 0);
}

/// `programs_ok` bit `n` is set when program slot `n` exists. A missing outline color program is
/// skipped here where the C++ dereferenced null.
pub fn plan_outline(objs: &[Outline], programs_ok: u32) -> Plan {
    let mut p = Plan::new();
    if objs.is_empty() {
        return p;
    }
    let ok = |slot: i32| programs_ok >> slot & 1 != 0;
    let list: Vec<(usize, &Outline)> = objs
        .iter()
        .enumerate()
        .filter(|(_, o)| o.collides != 0)
        .collect();
    let mut tmin = Vector3f {
        x: 100000.0,
        y: 100000.0,
        z: 100000.0,
    };
    let mut tmax = Vector3f {
        x: -100000.0,
        y: -100000.0,
        z: -100000.0,
    };
    for (_, o) in &list {
        expand_aabb(&mut tmin, &mut tmax, o.min, o.max);
    }
    let scale_add = 0.02f32;
    let size = Vector3f {
        x: tmax.x - tmin.x,
        y: tmax.y - tmin.y,
        z: tmax.z - tmin.z,
    };
    let add = scale_add * 2.0f32;
    // gcc fuses `min - prod * add` and `max + prod * add`
    let prod = |s: f32| s * (1.0f32 / s);
    let lo = |m: f32, s: f32| msub(prod(s), add, m);
    let hi = |m: f32, s: f32| madd(prod(s), add, m);
    p.clip_min = Vector3f {
        x: lo(tmin.x, size.x),
        y: lo(tmin.y, size.y),
        z: lo(tmin.z, size.z),
    };
    p.clip_max = Vector3f {
        x: hi(tmax.x, size.x),
        y: hi(tmax.y, size.y),
        z: hi(tmax.z, size.z),
    };

    p.p(op::FRAME_BUFFER, FB_COLOR, 0);
    p.p(op::CLEAR_STENCIL_COLOR, 0, 0);
    p.p(op::DEPTH_TEST, 1, 0);
    p.p(op::DEPTH_WRITE, 0, 0);
    p.p(op::STENCIL_ACTIVE, 1, 0);
    p.p(op::BLEND, BLEND_NONE, 0);
    p.p(op::CHANNEL, CHANNEL_NONE, 0);
    p.p(op::STENCIL_ALWAYS_REPLACE, 0, 0);
    for (n, o) in &list {
        if o.has_alpha == 0 {
            p.p(op::ALPHA_MODE, ALPHA_SOLID, 0);
            p.p(op::PROGRAM, PROG_OUTLINE_STENCIL, 0);
            p.p(op::TEX_NULL, 0, 0);
        } else {
            p.p(op::ALPHA_MODE, ALPHA_TRANS, 0);
            p.p(op::PROGRAM, PROG_OUTLINE_STENCIL_ALPHA, 0);
            p.p(op::TEX_ALPHA, *n as i32, 0);
        }
        p.p(op::VERTEX_OBJ, *n as i32, 0);
        p.p(op::MATRIX_OBJ, *n as i32, 0);
        p.p(op::DRAW, 0, 0);
    }
    p.p(op::ALPHA_MODE, ALPHA_SOLID, 0);
    p.p(op::BLEND, BLEND_NONE, 0);
    p.p(op::CHANNEL, CHANNEL_RGBA, 0);
    p.p(op::STENCIL_NOT_EQUAL_KEEP, 0, 0);
    p.p(op::TEXTURE_RANGE_NULL, 0, 0);
    for (n, o) in &list {
        let prog = if o.has_alpha != 0 {
            p.p(op::TEX_ALPHA, *n as i32, 0);
            PROG_OUTLINE_COLOR1
        } else {
            p.p(op::TEX_NULL, 0, 0);
            PROG_OUTLINE_COLOR0
        };
        p.p(op::PROGRAM, prog, 0);
        if ok(prog) {
            p.ops.push(Op {
                kind: op::PROG_COLOR3,
                i: prog,
                j: 0,
                f: [
                    0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                ],
            });
        }
        let ls = Vector3f {
            x: o.local_max.x - o.local_min.x,
            y: o.local_max.y - o.local_min.y,
            z: o.local_max.z - o.local_min.z,
        };
        let sc = |s: f32| madd(1.0f32 / s, scale_add, 1.0f32);
        let c = o.local_center;
        let mut mtx = matrix_mul(
            &matrix_scale(Vector3f {
                x: sc(ls.x),
                y: sc(ls.y),
                z: sc(ls.z),
            }),
            &matrix_translate(Vector3f {
                x: -c.x,
                y: -c.y,
                z: -c.z,
            }),
        );
        mtx.m[0][3] += c.x;
        mtx.m[1][3] += c.y;
        mtx.m[2][3] += c.z;
        let tmp = matrix_mul(&o.world, &mtx);
        p.p(op::VERTEX_OBJ, *n as i32, 0);
        p.p(op::MATRIX_NULL, 0, 0);
        let mut f = [0.0f32; 16];
        for (r, row) in tmp.m.iter().enumerate() {
            for (cidx, v) in row.iter().enumerate() {
                if let Some(slot) = f.get_mut(r * 4 + cidx) {
                    *slot = *v;
                }
            }
        }
        p.ops.push(Op {
            kind: op::MATRIX_TEMP,
            i: 0,
            j: 0,
            f,
        });
        p.p(op::DRAW, 0, 0);
    }
    p.p(op::FLAT_PROJECTION, 0, 0);
    p.p(op::STENCIL_ACTIVE, 0, 0);
    p.p(op::DEPTH_TEST, 0, 0);
    for i in 0..2 {
        let prog = PROG_BLUR1 - i;
        if ok(prog) {
            p.pf(op::PROG_FLOAT, prog, VAR_BLUR_SIZE, 1.0);
        }
    }
    blur(&mut p, TEX_OUTLINE_COLOR);
    blur(&mut p, TEX_BLUR1);
    p.p(op::STENCIL_ACTIVE, 1, 0);
    p.p(op::BLEND, BLEND_ADD, 0);
    p.p(op::PROGRAM, NONE, 0);
    p.p(op::FRAME_BUFFER, FB_ACCUM, 0);
    p.p(op::SCISSOR_ACTIVE, 1, 0);
    p.p(op::SCISSOR_RECT, 0, 0);
    p.p(op::TEX_SLOT, TEX_BLUR1, 0);
    p.p(op::QUAD_MIN_MAX, TEX_BLUR1, 0);
    p.p(op::NORMAL_PROJECTION, 0, 0);
    p.p(op::STENCIL_ACTIVE, 0, 0);
    p.p(op::SCISSOR_ACTIVE, 0, 0);
    p.p(op::TEXTURE_RANGE_NULL, 0, 0);
    p
}
