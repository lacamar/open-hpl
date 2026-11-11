//! `cRendererDeferred` part C: the light passes (`RenderLights` and the `RenderLights_*` lists).
//!
//! Same plan pattern as `deferred`; light indices refer to `mvTempDeferredLights`.

use std::collections::VecDeque;

use super::deferred::{DlOp, Feed, Op, Out, P3Op, Plan, Source, State, Step, st, state};
use super::deferred_light::{
    MAX_GBUFFER_TEXTURES, feature, light_shape, linear_light_color, res, slot, var,
};

pub mod pslot {
    pub const STENCIL_PROG: i32 = 8;
    pub const BOX_PROG0: i32 = 9;
    pub const BOX_PROG1: i32 = 10;
    pub const PROG_COUNT: usize = 11;
}

/// `SetShape` targets beyond the sphere qualities and the pyramid.
pub mod pshape {
    pub const BOX: i32 = 4;
    pub const FULLSCREEN_QUAD: i32 = 5;
}

pub mod pass {
    pub const STENCIL_BACK_SCREEN_QUAD: i32 = 0;
    pub const STENCIL_FRONT_RENDER_BACK: i32 = 1;
    pub const RENDER_BACK: i32 = 2;
    pub const BOX_STENCIL_FRONT_RENDER_BACK: i32 = 3;
    pub const BOX_RENDER_BACK: i32 = 4;
}

pub mod stencil {
    pub const FUNC_EQUAL: i32 = 0;
    pub const FUNC_ALWAYS: i32 = 1;
    pub const OP_KEEP: i32 = 0;
    pub const OP_REPLACE: i32 = 1;
}

/// `eLightBoxBlendFunc_*`.
pub mod blend {
    pub const REPLACE: i32 = 0;
    pub const ADD: i32 = 1;
}

pub mod logkind {
    pub const BEGIN: i32 = 0;
    pub const STENCIL: i32 = 1;
    pub const RENDER: i32 = 2;
    pub const END: i32 = 3;
}

const MAX_STENCIL_BITS: usize = 8;

/// One entry of a sorted light list with the `iLight` values the passes read.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct PassLight {
    pub idx: i32,
    pub kind: i32,
    pub cast_shadows: bool,
    pub inside_near: bool,
    pub blend_func: i32,
    pub diffuse: [f32; 4],
    pub brightness: f32,
    pub distance_fade: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct PassIn {
    pub pass: i32,
    pub stencil_dirty: bool,
    pub depth_cull: bool,
    pub occlusion_test: bool,
    pub box_ssao: bool,
    pub hdr: bool,
    pub far_left: f32,
    pub far_bottom: f32,
    pub far_right: f32,
    pub far_top: f32,
    pub far_plane: f32,
}

fn p3(o: P3Op) -> Op {
    Op::P3(o)
}

fn dl(o: DlOp) -> Op {
    Op::Dl(o)
}

fn depth_test(on: bool) -> Op {
    state(State::DepthTest, i32::from(on))
}

fn channel(v: i32) -> Op {
    state(State::Channel, v)
}

fn blend_state(v: i32) -> Op {
    state(State::Blend, v)
}

fn bit(i: usize) -> i32 {
    1 << (i & 31)
}

fn log(kind: i32, idx: i32) -> Op {
    p3(P3Op::Log { kind, idx })
}

/// `cRendererDeferred::RenderBoxLight`.
fn box_light(out: &mut Vec<Item>, i: &PassIn, l: &PassLight) {
    out.push(Item::Op(log(logkind::RENDER, l.idx)));
    let prog = if i.box_ssao {
        out.push(Item::Op(Op::Tex {
            unit: 5,
            tex: slot::tex::SSAO,
        }));
        pslot::BOX_PROG1
    } else {
        pslot::BOX_PROG0
    };
    out.push(Item::Op(Op::Program { prog }));
    let f = l.brightness * l.distance_fade;
    let mut c = linear_light_color(l.diffuse, i.hdr);
    c[0] *= f;
    c[1] *= f;
    c[2] *= f;
    out.push(Item::Op(Op::Vec4 {
        prog,
        id: var::AV_LIGHT_COLOR,
        v: c,
    }));
    match l.blend_func {
        blend::ADD => out.push(Item::Op(blend_state(st::BLEND_ADD))),
        blend::REPLACE => out.push(Item::Op(blend_state(st::BLEND_NONE))),
        _ => {}
    }
    out.push(Item::Op(dl(DlOp::SetModelView(l.idx))));
    out.push(Item::Op(dl(DlOp::DrawCurrent)));
}

fn flat_min_max(i: &PassIn) -> Op {
    Op::FlatProjectionMinMax {
        min: [i.far_left, i.far_bottom, -i.far_plane * 1.5],
        max: [i.far_right, i.far_top, i.far_plane * 1.5],
    }
}

/// Ops, with questions whose answer picks the ops that follow.
enum Item {
    Op(Op),
    Branch { ask: Op, yes: Vec<Op>, no: Vec<Op> },
}

struct Script {
    items: VecDeque<Item>,
    pending: Option<(Vec<Op>, Vec<Op>)>,
}

impl Script {
    fn new(items: Vec<Item>) -> Self {
        Self {
            items: items.into(),
            pending: None,
        }
    }
}

impl Source for Script {
    fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool {
        if let Some((yes, no)) = self.pending.take() {
            out.extend(if feed.is_some_and(Feed::flag) {
                yes
            } else {
                no
            });
        }
        while let Some(item) = self.items.pop_front() {
            match item {
                Item::Op(o) => out.push(o),
                Item::Branch { ask, yes, no } => {
                    out.push(ask);
                    self.pending = Some((yes, no));
                    return true;
                }
            }
        }
        false
    }
}

fn ops(v: impl IntoIterator<Item = Op>) -> Vec<Item> {
    v.into_iter().map(Item::Op).collect()
}

fn light_draw(l: &PassLight, flags: u32, quality: i32, occlusion: bool, items: &mut Vec<Item>) {
    items.extend(ops([
        dl(DlOp::SetModelView(l.idx)),
        dl(DlOp::SetShape(light_shape(l.kind, quality))),
        p3(P3Op::LightProgram { idx: l.idx, flags }),
    ]));
    if occlusion {
        items.extend(ops([
            p3(P3Op::PairBegin(l.idx)),
            dl(DlOp::DrawCurrent),
            p3(P3Op::PairEnd),
        ]));
    } else {
        items.push(Item::Op(dl(DlOp::DrawCurrent)));
    }
}

fn stencil_op(func: i32, mask: i32, zfail: i32, zpass: i32) -> Op {
    p3(P3Op::Stencil {
        func,
        reference: 0xFF,
        mask,
        fail: stencil::OP_KEEP,
        zfail,
        zpass,
    })
}

/// `RenderLights_StencilBack_ScreenQuad`, `_StencilFront_RenderBack`, `_RenderBack` and the box
/// variants over the sorted list `lights`.
pub fn plan_light_pass(i: PassIn, lights: &[PassLight]) -> Plan {
    let mut items = Vec::new();
    if !lights.is_empty() {
        build_pass(&i, lights, &mut items);
    }
    Plan::new(Script::new(items))
}

fn build_pass(i: &PassIn, lights: &[PassLight], items: &mut Vec<Item>) {
    let back_screen = i.pass == pass::STENCIL_BACK_SCREEN_QUAD;
    let front = matches!(
        i.pass,
        pass::STENCIL_FRONT_RENDER_BACK | pass::BOX_STENCIL_FRONT_RENDER_BACK
    );
    let is_box = matches!(
        i.pass,
        pass::BOX_STENCIL_FRONT_RENDER_BACK | pass::BOX_RENDER_BACK
    );
    items.push(Item::Op(log(logkind::BEGIN, 0)));
    if (back_screen || front) && i.stencil_dirty {
        items.extend(ops([p3(P3Op::ClearStencil), p3(P3Op::StencilDirty(false))]));
    }
    if back_screen {
        if i.depth_cull {
            items.push(Item::Op(p3(P3Op::StencilActive(true))));
        }
    } else if front {
        if i.depth_cull {
            items.extend(ops([p3(P3Op::StencilActive(true)), depth_test(true)]));
        } else {
            items.push(Item::Op(depth_test(false)));
        }
    } else {
        if i.depth_cull {
            items.extend(ops([p3(P3Op::StencilActive(false)), depth_test(true)]));
        } else {
            items.push(Item::Op(depth_test(false)));
        }
        items.push(Item::Op(channel(st::CHANNEL_RGBA)));
    }
    if is_box {
        items.push(Item::Op(dl(DlOp::SetShape(pshape::BOX))));
    }

    if !(back_screen || front) {
        for l in lights {
            if is_box {
                box_light(items, i, l);
                continue;
            }
            if l.cast_shadows {
                items.push(Item::Branch {
                    ask: p3(P3Op::ShadowSetup(l.idx)),
                    yes: vec![
                        p3(P3Op::Cull(true)),
                        p3(P3Op::ShadowMap(l.idx)),
                        p3(P3Op::Cull(false)),
                    ],
                    no: Vec::new(),
                });
            }
            items.push(Item::Op(log(logkind::RENDER, l.idx)));
            let mut flags = feature::LIGHT_SHAPES;
            if l.inside_near {
                flags |= feature::DIVIDE_IN_FRAG;
            }
            light_draw(l, flags, res::MEDIUM, false, items);
        }
        if is_box {
            items.push(Item::Op(blend_state(st::BLEND_ADD)));
        }
        items.push(Item::Op(log(logkind::END, 0)));
        return;
    }

    let mut start = 0;
    while start < lights.len() {
        let rest = lights.get(start..).unwrap_or_default();
        let batch = rest
            .get(..rest.len().min(MAX_STENCIL_BITS))
            .unwrap_or_default();
        if back_screen {
            items.extend(ops([
                depth_test(i.depth_cull),
                channel(st::CHANNEL_NONE),
                stencil_op(
                    stencil::FUNC_ALWAYS,
                    0xFF,
                    stencil::OP_KEEP,
                    stencil::OP_REPLACE,
                ),
            ]));
        } else {
            items.extend(ops([
                channel(st::CHANNEL_NONE),
                p3(P3Op::Cull(true)),
                stencil_op(
                    stencil::FUNC_ALWAYS,
                    0xFF,
                    stencil::OP_REPLACE,
                    stencil::OP_KEEP,
                ),
            ]));
        }
        for (n, l) in batch.iter().enumerate() {
            items.extend(ops([
                log(logkind::STENCIL, l.idx),
                p3(P3Op::StencilWriteMask(bit(n))),
            ]));
            if !is_box {
                items.push(Item::Op(Op::Tex {
                    unit: 5,
                    tex: super::deferred::NONE,
                }));
            }
            items.push(Item::Op(dl(DlOp::SetModelView(l.idx))));
            if !is_box {
                items.push(Item::Op(dl(DlOp::SetShape(light_shape(
                    l.kind,
                    res::MEDIUM,
                )))));
            }
            items.extend(ops([
                Op::Program {
                    prog: pslot::STENCIL_PROG,
                },
                dl(DlOp::DrawCurrent),
            ]));
        }
        if back_screen {
            items.extend(ops([
                depth_test(false),
                channel(st::CHANNEL_RGBA),
                flat_min_max(i),
                p3(P3Op::Cull(true)),
                p3(P3Op::StencilWriteMask(0xFF)),
            ]));
        } else {
            items.extend(ops([
                channel(st::CHANNEL_RGBA),
                p3(P3Op::Cull(false)),
                p3(P3Op::StencilWriteMask(0xFF)),
            ]));
        }
        for (n, l) in batch.iter().enumerate() {
            if !is_box && l.cast_shadows {
                items.push(Item::Branch {
                    ask: p3(P3Op::ShadowSetup(l.idx)),
                    yes: shadow_ops(i, l, back_screen),
                    no: Vec::new(),
                });
            }
            items.push(Item::Op(log(logkind::RENDER, l.idx)));
            items.push(Item::Op(stencil_op(
                stencil::FUNC_EQUAL,
                bit(n),
                stencil::OP_KEEP,
                stencil::OP_KEEP,
            )));
            if is_box {
                box_light(items, i, l);
            } else if back_screen {
                items.extend(ops([
                    dl(DlOp::SetShape(pshape::FULLSCREEN_QUAD)),
                    p3(P3Op::Scissor(l.idx)),
                    p3(P3Op::LightProgram {
                        idx: l.idx,
                        flags: 0,
                    }),
                    dl(DlOp::DrawCurrent),
                ]));
            } else {
                light_draw(
                    l,
                    feature::LIGHT_SHAPES | feature::DIVIDE_IN_FRAG,
                    res::HIGH,
                    i.occlusion_test,
                    items,
                );
            }
        }
        if back_screen {
            items.extend(ops([
                Op::NormalFrustumProjection,
                p3(P3Op::Cull(false)),
                p3(P3Op::ScissorOff),
            ]));
        }
        start += batch.len();
        if start < lights.len() {
            items.push(Item::Op(p3(P3Op::ClearStencil)));
        }
    }
    items.push(Item::Op(p3(P3Op::StencilDirty(true))));
    if is_box {
        items.push(Item::Op(blend_state(st::BLEND_ADD)));
    }
    items.push(Item::Op(log(logkind::END, 0)));
}

fn shadow_ops(i: &PassIn, l: &PassLight, back_screen: bool) -> Vec<Op> {
    if back_screen {
        let mut v = Vec::new();
        if i.depth_cull {
            v.extend([p3(P3Op::StencilActive(false)), depth_test(true)]);
        } else {
            v.push(depth_test(false));
        }
        v.push(p3(P3Op::ShadowMap(l.idx)));
        if i.depth_cull {
            v.extend([depth_test(false), p3(P3Op::StencilActive(true))]);
        } else {
            v.push(depth_test(false));
        }
        v.push(flat_min_max(i));
        v
    } else {
        vec![
            p3(P3Op::Cull(true)),
            p3(P3Op::StencilActive(false)),
            p3(P3Op::ShadowMap(l.idx)),
            p3(P3Op::StencilActive(true)),
            p3(P3Op::Cull(false)),
        ]
    }
}

/// `cRendererDeferred::RenderLights`: the state around the light passes.
pub fn plan_render_lights(gbuffer_textures: i32) -> Plan {
    let s = |x| Item::Op(Op::Step(x));
    let mut items = vec![
        s(Step::InitLightRendering),
        s(Step::RenderSsao),
        s(Step::RenderHpl3Ssao),
    ];
    items.extend(ops([
        depth_test(true),
        state(State::DepthWrite, 0),
        blend_state(st::BLEND_ADD),
        state(State::Alpha, st::ALPHA_SOLID),
        channel(st::CHANNEL_RGBA),
    ]));
    for n in 0..gbuffer_textures.clamp(0, MAX_GBUFFER_TEXTURES) {
        items.push(Item::Op(Op::Tex {
            unit: n,
            tex: slot::tex::GBUFFER0 + n,
        }));
    }
    items.extend(ops([
        Op::TexRangeNull {
            first: gbuffer_textures,
        },
        Op::SetAccumBuffer,
        p3(P3Op::ClearColorStencil),
        p3(P3Op::Cull(false)),
        state(State::DepthFunc, st::FUNC_GREATER_OR_EQUAL),
    ]));
    items.push(Item::Branch {
        ask: Op::Step(Step::RenderBoxWeighted),
        yes: Vec::new(),
        no: vec![
            Op::Step(Step::LightPass(pass::BOX_STENCIL_FRONT_RENDER_BACK)),
            Op::Step(Step::LightPass(pass::BOX_RENDER_BACK)),
        ],
    });
    items.extend(ops([
        Op::Step(Step::RenderDirectional),
        Op::Step(Step::LightPass(pass::STENCIL_BACK_SCREEN_QUAD)),
        Op::Step(Step::LightPass(pass::STENCIL_FRONT_RENDER_BACK)),
        Op::Step(Step::LightPass(pass::RENDER_BACK)),
        p3(P3Op::StencilActive(false)),
        state(State::DepthFunc, st::FUNC_LESS_OR_EQUAL),
        p3(P3Op::Cull(true)),
        Op::Step(Step::RenderNightVision),
        Op::Step(Step::ApplyHpl3Ssao),
    ]));
    Plan::new(Script::new(items))
}
