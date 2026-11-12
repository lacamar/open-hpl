//! `cLuxPostEffect_Insanity`: wave animation clock and the draw plan of `RenderEffect`.
//! Textures and programs are slot indices; `NONE` is null.

use super::hw::madd;

pub const NONE: i32 = -1;
pub const TEX_INPUT: i32 = 0;
pub const TEX_AMP0: i32 = 1;
pub const TEX_ZOOM: i32 = 4;
pub const PROG: i32 = 0;
pub const AMP_MAPS: i32 = 3;

pub mod op {
    pub const FLAT_PROJECTION: u32 = 0;
    pub const BLEND_NONE: u32 = 1;
    pub const CHANNEL_RGBA: u32 = 2;
    pub const FINAL_FB: u32 = 3;
    pub const TEX: u32 = 4;
    pub const PROGRAM: u32 = 5;
    pub const FLOAT: u32 = 6;
    pub const VEC2: u32 = 7;
    pub const QUAD1: u32 = 8;
    pub const TEXTURE_RANGE_NULL: u32 = 9;
}

const VAR_ALPHA: i32 = 0;
const VAR_T: i32 = 1;
const VAR_SCREEN_SIZE: i32 = 2;
const VAR_AMP_T: i32 = 3;
const VAR_WAVE_ALPHA: i32 = 4;
const VAR_ZOOM_ALPHA: i32 = 5;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Clock {
    pub t: f32,
    pub anim_count: f32,
    pub wave_alpha: f32,
    pub zoom_alpha: f32,
    pub wave_speed: f32,
}

pub fn update(c: &mut Clock, dt: f32) {
    c.t = madd(dt, c.wave_speed, c.t);
    c.anim_count = madd(dt, 0.15f32, c.anim_count);
    let max = AMP_MAPS as f32;
    if c.anim_count >= max {
        c.anim_count -= max;
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Op {
    pub kind: u32,
    pub a: i32,
    pub b: i32,
    pub f: [f32; 2],
}

pub struct Plan {
    ops: Vec<Op>,
    pos: usize,
}

impl Plan {
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

/// A fractional part as `cMath::GetFraction`.
fn fraction(v: f32) -> f32 {
    v - v.floor()
}

/// Plan of `RenderEffect`. An amp map index outside the three maps (the animation count after a
/// huge step) reads as no texture where the C++ read past the vector.
pub fn plan(c: &Clock, screen: [f32; 2], program_ok: bool) -> Plan {
    let amp0 = c.anim_count as i32;
    let mut amp1 = (c.anim_count + 1.0f32) as i32;
    if amp1 >= AMP_MAPS {
        amp1 = 0;
    }
    let amp_t = fraction(c.anim_count);
    let amp = |i: i32| {
        if (0..AMP_MAPS).contains(&i) {
            TEX_AMP0 + i
        } else {
            NONE
        }
    };
    let prog = if program_ok { PROG } else { NONE };
    let mut ops = vec![
        Op {
            kind: op::FLAT_PROJECTION,
            ..Op::default()
        },
        Op {
            kind: op::BLEND_NONE,
            ..Op::default()
        },
        Op {
            kind: op::CHANNEL_RGBA,
            ..Op::default()
        },
        Op {
            kind: op::FINAL_FB,
            ..Op::default()
        },
    ];
    let tex = |unit: i32, t: i32| Op {
        kind: op::TEX,
        a: unit,
        b: t,
        f: [0.0; 2],
    };
    ops.push(tex(0, TEX_INPUT));
    ops.push(tex(1, amp(amp0)));
    ops.push(tex(2, amp(amp1)));
    ops.push(tex(3, TEX_ZOOM));
    ops.push(Op {
        kind: op::PROGRAM,
        a: prog,
        ..Op::default()
    });
    if program_ok {
        let fl = |id: i32, v: f32| Op {
            kind: op::FLOAT,
            a: prog,
            b: id,
            f: [v, 0.0],
        };
        ops.push(fl(VAR_ALPHA, 1.0));
        ops.push(fl(VAR_T, c.t));
        ops.push(Op {
            kind: op::VEC2,
            a: prog,
            b: VAR_SCREEN_SIZE,
            f: screen,
        });
        ops.push(fl(VAR_AMP_T, amp_t));
        ops.push(fl(VAR_WAVE_ALPHA, c.wave_alpha));
        ops.push(fl(VAR_ZOOM_ALPHA, c.zoom_alpha));
    }
    ops.push(Op {
        kind: op::QUAD1,
        a: TEX_INPUT,
        b: 1,
        f: [0.0; 2],
    });
    ops.push(Op {
        kind: op::TEXTURE_RANGE_NULL,
        a: 1,
        ..Op::default()
    });
    Plan { ops, pos: 0 }
}
