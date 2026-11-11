//! `cRendererDeferred` part A: data-load decisions, G-buffer setup, pass state and the frame step order.
//!
//! A plan is the exact sequence of `iRenderFunctions`/`iGpuProgram` calls the C++ member functions
//! made; the C++ shim replays it. Frame buffers, textures and programs are slot indices into arrays
//! the shim owns (`NONE` is a null pointer). Values only the shim can read (headroom, program
//! creation results, world state) are `Query` ops answered through `Plan::feed`.
//!
//! G6b (`deferred_light*`: SSAO, edge smooth, light setup and lists) added the `Dl`/`AskGeom` ops. G6c
//! (from `RenderLights_StencilBack_ScreenQuad`) adds more `Source`s and `Op`/`Step` variants; slot
//! and variable ids live in the `deferred*` modules so both sides stay in step.

use std::collections::VecDeque;

use crate::types::{Vector2f, Vector2l};

pub const NONE: i32 = -1;

unsafe extern "C" {
    fn expf(x: f32) -> f32;
    fn tanf(x: f32) -> f32;
    fn log2f(x: f32) -> f32;
    fn floorf(x: f32) -> f32;
    fn ohpl_math_rand_rectf(min: f32, max: f32) -> f32;
}

pub(super) fn exp(x: f32) -> f32 {
    // SAFETY: pure libm function on a plain float.
    unsafe { expf(x) }
}

fn tan(x: f32) -> f32 {
    // SAFETY: pure libm function on a plain float.
    unsafe { tanf(x) }
}

fn log2(x: f32) -> f32 {
    // SAFETY: pure libm function on a plain float.
    unsafe { log2f(x) }
}

fn floor(x: f32) -> f32 {
    // SAFETY: pure libm function on a plain float.
    unsafe { floorf(x) }
}

/// `cMath::RandRectf`: shares the engine's `rand()` stream with the C++ callers.
pub(super) fn rand_rectf(min: f32, max: f32) -> f32 {
    // SAFETY: exported by crate::math, plain floats.
    unsafe { ohpl_math_rand_rectf(min, max) }
}

/// `cMath::Min(float, float)`: the second argument wins ties and NaNs.
pub(super) fn c_min(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

/// `cMath::Max(float, float)`.
pub(super) fn c_max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

/// `cMath::Clamp`.
pub(super) fn c_clamp(x: f32, min: f32, max: f32) -> f32 {
    if x > max {
        max
    } else if x < min {
        min
    } else {
        x
    }
}

/// C `int / int` as gcc emits it on aarch64 (sdiv): a zero divisor yields 0, `INT_MIN / -1` wraps.
pub fn cdiv(a: i32, b: i32) -> i32 {
    if b == 0 { 0 } else { a.wrapping_div(b) }
}

pub fn div_size(v: Vector2l, d: i32) -> Vector2l {
    Vector2l {
        x: cdiv(v.x, d),
        y: cdiv(v.y, d),
    }
}

pub mod fb {
    pub const TARGET: i32 = 0;
    pub const ACCUM: i32 = 1;
    pub const TONEMAP: i32 = 2;
    pub const TRAIL: i32 = 3;
    pub const BLOOM0: i32 = 4;
    pub const COUNT: usize = 10;
}

pub mod tex {
    pub const ACCUM: i32 = 0;
    pub const TONEMAP_OUT: i32 = 1;
    pub const TRAIL: i32 = 2;
    pub const BLOOM0: i32 = 3;
    pub const GRADING: i32 = 9;
    pub const GRADING_TARGET: i32 = 10;
    pub const NOISE: i32 = 11;
    pub const LAYER0: i32 = 12;
    pub const COUNT: usize = 20;
}

pub mod prog {
    pub const TONEMAP: i32 = 0;
    pub const FXAA: i32 = 1;
    pub const TRAIL: i32 = 2;
    pub const BRIGHT: i32 = 3;
    pub const BLUR0: i32 = 4;
    pub const BLUR1: i32 = 5;
    pub const COUNT: usize = 6;
}

/// `kVar_*` ids of `RendererDeferred.cpp`; the shim static-asserts them against its macros.
pub mod var {
    pub const AF_KEY: i32 = 28;
    pub const AF_EXPOSURE: i32 = 29;
    pub const AF_WHITE_CUT: i32 = 30;
    pub const AF_INV_GAMMA: i32 = 31;
    pub const AV_INV_SCREEN_SIZE: i32 = 24;
    pub const AV_SCREEN_TO_FAR_PLANE: i32 = 23;
    pub const AF_T: i32 = 57;
    pub const AV_BRIGHT_PASS: i32 = 79;
    pub const AV_WEIGHTS: i32 = 80;
    pub const AV_OFFSETS: i32 = 81;
    pub const AF_INTENSITY: i32 = 82;
    pub const AV_TRANSFORM0: i32 = 83;
    pub const AV_TRANSFORM1: i32 = 84;
    pub const AV_BLOOM_TINT: i32 = 85;
    pub const AV_SIZE_WEIGHT: i32 = 86;
    pub const AF_GRADING_BLEND: i32 = 95;
    pub const AF_ALPHA: i32 = 96;
    pub const AF_GRADING_ALPHA0: i32 = 102;
    pub const AL_GRADING_NUM: i32 = 103;
    pub const AF_GRADING_ALPHA2: i32 = 104;
    pub const AF_HDR_HEADROOM: i32 = 119;
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum State {
    DepthTest,
    DepthWrite,
    DepthFunc,
    Blend,
    Alpha,
    Channel,
}

pub mod st {
    pub const BLEND_NONE: i32 = 0;
    pub const BLEND_ALPHA: i32 = 1;
    pub const BLEND_ADD: i32 = 2;
    pub const BLEND_MUL: i32 = 3;
    pub const ALPHA_SOLID: i32 = 0;
    pub const CHANNEL_RGBA: i32 = 0;
    pub const CHANNEL_NONE: i32 = 1;
    pub const CHANNEL_RGB: i32 = 2;
    pub const FUNC_EQUAL: i32 = 0;
    pub const FUNC_LESS_OR_EQUAL: i32 = 1;
    pub const FUNC_GREATER_OR_EQUAL: i32 = 2;
}

pub mod pf {
    pub const RGBA: i32 = 0;
    pub const RGBA16: i32 = 1;
}

pub mod comp {
    pub const FULL: i32 = 0;
    pub const COLOR: i32 = 2;
    pub const LINEAR_DEPTH: i32 = 5;
}

pub mod clear {
    pub const DEPTH: i32 = 1;
    pub const COLOR: i32 = 2;
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Query {
    Headroom,
    FogAfterAreas,
    FogIsSkybox,
    DepthOfField,
}

/// Answer to a `Query`.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Feed {
    Bool(bool),
    Float(f32),
    Int(i32),
    Vec3([f32; 3]),
}

impl Feed {
    pub fn flag(self) -> bool {
        matches!(self, Feed::Bool(true))
    }

    pub fn float(self) -> f32 {
        match self {
            Feed::Float(v) => v,
            _ => 0.0,
        }
    }

    pub fn int(self) -> i32 {
        match self {
            Feed::Int(v) => v,
            _ => 0,
        }
    }

    pub fn vec3(self) -> [f32; 3] {
        match self {
            Feed::Vec3(v) => v,
            _ => [0.0; 3],
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Step {
    SetupRenderVariables,
    SetupGBuffer,
    CheckVisibleZ,
    CheckVisibleStatic,
    CheckVisibleDynamic,
    FetchDelayedOcclusion,
    AssignOcclusionQueries,
    SetupLights,
    CompileOcclusion,
    CompileBruteForce,
    RenderZ,
    RenderDelayedOcclusionQueries,
    RenderGbuffer,
    RenderGbufferContent,
    RenderDecals,
    CallbackPostGBuffer,
    RenderLights,
    RenderIllumination,
    RenderFog,
    RenderFullScreenFog,
    RenderEdgeSmooth,
    RenderBasicSkyBox,
    CallbackPostSolid,
    UpdateLensFlares,
    RenderEnvironmentParticles(bool),
    RenderTranslucent(i32),
    RenderDepthOfField,
    RenderMultiIrisAndGlare,
    CallbackPostTranslucent,
    RetrieveLightOcclusion,
    CopyAccumToPostEffects,
    InitLightRendering,
    RenderSsao,
    RenderHpl3Ssao,
    /// Answered with whether the weighted box pass handled the box lights.
    RenderBoxWeighted,
    RenderDirectional,
    RenderNightVision,
    ApplyHpl3Ssao,
    LightPass(i32),
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Op {
    State {
        which: State,
        val: i32,
    },
    FlatProjection,
    SetFb {
        fb: i32,
        use_pos: bool,
    },
    TexRangeNull {
        first: i32,
    },
    Program {
        prog: i32,
    },
    Tex {
        unit: i32,
        tex: i32,
    },
    AcquireTemp {
        fb: i32,
        tex: i32,
        size: Vector2l,
        fmt: i32,
        idx: i32,
    },
    Float {
        prog: i32,
        id: i32,
        v: f32,
    },
    Int {
        prog: i32,
        id: i32,
        v: i32,
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
    Vec4 {
        prog: i32,
        id: i32,
        v: [f32; 4],
    },
    FloatArr {
        prog: i32,
        id: i32,
        arr: i32,
        len: i32,
    },
    Quad {
        min: [f32; 2],
        max: [f32; 2],
    },
    SetGBuffer {
        comp: i32,
    },
    ClearDepthValue {
        v: f32,
    },
    ClearColorValue {
        rgba: [f32; 4],
    },
    ClearColorSettings,
    ClearFb {
        flags: i32,
        use_pos: bool,
    },
    Ask(Query),
    TryToneMap {
        combo: i32,
    },
    Step(Step),
    FlatProjectionMinMax {
        min: [f32; 3],
        max: [f32; 3],
    },
    NormalFrustumProjection,
    QuadAt {
        pos: [f32; 3],
        size: [f32; 2],
        min: [f32; 2],
        max: [f32; 2],
    },
    CopyFbToTex {
        tex: i32,
        size: Vector2l,
    },
    SetAccumBuffer,
    /// `SetMatrixf` of the 16 floats in plan array `arr`.
    Matrix {
        prog: i32,
        id: i32,
        arr: i32,
    },
    InvViewMatrix {
        prog: i32,
        id: i32,
    },
    ScreenPositionVars {
        prog: i32,
    },
    /// `mpProgramManager->GenerateProgram`; stores into program slot `slot`, answered with a flag.
    GenProgram {
        slot: i32,
        mode: i32,
        flags: u32,
    },
    AskGeom {
        q: Geom,
        idx: i32,
        f: [f32; 4],
    },
    Dl(DlOp),
    P3(P3Op),
}

/// Frustum and query questions only the shim can answer.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Geom {
    /// `CheckSphereNearPlaneIntersection(f[0..3], f[3])`; flag.
    SphereNearPlane,
    /// Light `idx`'s spot frustum against the camera near plane; flag.
    FrustumNearPlane,
    /// Spot frustum intersection with the line to the light's volume center; `Feed::Vec3`.
    LineIntersection,
    /// Light `idx`'s bounding volume against the near plane; flag.
    BoxNearPlane,
    QueryFetch,
    QuerySamples,
    ShadowNeedsUpdate,
}

/// Light list bookkeeping the shim performs on its own objects.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum DlOp {
    ClearPairs,
    ClearTempLights,
    SetMinLargeArea(i32),
    Light(i32),
    AcquireQuery(i32),
    SetModelView(i32),
    SetShape(i32),
    QueryBegin(i32),
    QueryEnd(i32),
    DrawCurrent,
    Flush,
    InvView,
    UpdateFullscreenQuad {
        pos: [f32; 3],
        size: [f32; 2],
    },
    LightMatrix(i32),
    QueryLog {
        idx: i32,
        samples: i32,
        visible: bool,
    },
    ReleaseQuery(i32),
    Lists,
    ClearShadowTexture,
    ShadowData,
    RenderShadowMap,
}

/// Light pass operations (G6c) on renderer state and light objects.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum P3Op {
    StencilActive(bool),
    ClearStencil,
    ClearColorStencil,
    Stencil {
        func: i32,
        reference: i32,
        mask: i32,
        fail: i32,
        zfail: i32,
        zpass: i32,
    },
    StencilWriteMask(i32),
    /// Counter-clockwise when true.
    Cull(bool),
    Scissor(i32),
    ScissorOff,
    /// `SetupProgramAndTextures` plus `SetupLightProgramVariables` for a light.
    LightProgram {
        idx: i32,
        flags: u32,
    },
    /// `SetupShadowMapRendering`; flag.
    ShadowSetup(i32),
    /// `RenderLightShadowMap`.
    ShadowMap(i32),
    PairBegin(i32),
    PairEnd,
    StencilDirty(bool),
    Log {
        kind: i32,
        idx: i32,
    },
}

/// Appends ops when the queue runs dry; returns false once nothing more will come.
pub trait Source {
    fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool;
}

/// `mfFilmGrainT` and `mvFilmGrainTransform`; `t == -1` means not yet seeded.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FilmGrain {
    pub t: f32,
    pub transform: [[f32; 4]; 2],
}

impl FilmGrain {
    pub const fn new() -> Self {
        Self {
            t: -1.0,
            transform: [[0.0; 4]; 2],
        }
    }
}

impl Default for FilmGrain {
    fn default() -> Self {
        Self::new()
    }
}

/// Renderer state a plan mutates; the shim copies it back after the replay.
#[derive(Clone, Debug, Default)]
pub struct Results {
    pub film_grain: FilmGrain,
    pub trail_consumed: bool,
    pub lights: super::deferred_light::LightResults,
}

pub struct Out<'a> {
    pub q: &'a mut VecDeque<Op>,
    pub arrays: &'a mut Vec<Vec<f32>>,
    pub res: &'a mut Results,
}

impl Out<'_> {
    pub fn push(&mut self, op: Op) {
        self.q.push_back(op);
    }

    pub fn extend(&mut self, ops: impl IntoIterator<Item = Op>) {
        self.q.extend(ops);
    }

    pub fn array(&mut self, v: Vec<f32>) -> (i32, i32) {
        let len = v.len() as i32;
        self.arrays.push(v);
        ((self.arrays.len() - 1) as i32, len)
    }
}

pub struct Plan {
    pub res: Results,
    q: VecDeque<Op>,
    arrays: Vec<Vec<f32>>,
    src: Box<dyn Source>,
    feed: Option<Feed>,
    done: bool,
}

impl Plan {
    pub fn new(src: impl Source + 'static) -> Self {
        Self {
            res: Results::default(),
            q: VecDeque::new(),
            arrays: Vec::new(),
            src: Box::new(src),
            feed: None,
            done: false,
        }
    }

    pub fn next_op(&mut self) -> Option<Op> {
        loop {
            if let Some(o) = self.q.pop_front() {
                return Some(o);
            }
            if self.done {
                return None;
            }
            let mut out = Out {
                q: &mut self.q,
                arrays: &mut self.arrays,
                res: &mut self.res,
            };
            self.done = !self.src.refill(&mut out, self.feed.take());
        }
    }

    /// Answers the last `Ask` op; call before the next `next_op`.
    pub fn feed(&mut self, f: Feed) {
        self.feed = Some(f);
    }

    pub fn array(&self, idx: i32) -> &[f32] {
        usize::try_from(idx)
            .ok()
            .and_then(|i| self.arrays.get(i))
            .map_or(&[], Vec::as_slice)
    }
}

/// A fixed op list.
pub struct Fixed(Option<Vec<Op>>);

impl Source for Fixed {
    fn refill(&mut self, out: &mut Out<'_>, _: Option<Feed>) -> bool {
        if let Some(v) = self.0.take() {
            out.extend(v);
        }
        false
    }
}

pub fn fixed(ops: Vec<Op>) -> Plan {
    Plan::new(Fixed(Some(ops)))
}

/// A fixed op list whose `Matrix`/`FloatArr` ops index `arrays`.
pub fn fixed_with_arrays(ops: Vec<Op>, arrays: Vec<Vec<f32>>) -> Plan {
    let mut p = fixed(ops);
    p.arrays = arrays;
    p
}

pub(super) fn state(which: State, val: i32) -> Op {
    Op::State { which, val }
}

/// `cRendererDeferred::SetupGBuffer`.
pub fn plan_setup_gbuffer(gbuffer_64bit: bool, linear_depth_idx: i32) -> Plan {
    let mut ops = vec![
        Op::SetGBuffer { comp: comp::FULL },
        Op::ClearDepthValue { v: 1.0 },
    ];
    if gbuffer_64bit {
        ops.extend([
            Op::ClearColorValue {
                rgba: [0.0, 0.0, 1.0, 1.0],
            },
            Op::ClearFb {
                flags: clear::DEPTH | clear::COLOR,
                use_pos: true,
            },
        ]);
        if linear_depth_idx != 2 {
            ops.extend([
                Op::SetGBuffer {
                    comp: comp::LINEAR_DEPTH,
                },
                Op::ClearColorValue {
                    rgba: [1.0, 1.0, 1.0, 1.0],
                },
                Op::ClearFb {
                    flags: clear::COLOR,
                    use_pos: true,
                },
                Op::SetGBuffer { comp: comp::FULL },
            ]);
        }
        ops.push(Op::ClearColorSettings);
    } else {
        ops.push(Op::ClearFb {
            flags: clear::DEPTH,
            use_pos: true,
        });
    }
    fixed(ops)
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Pass {
    ZBegin,
    GbufferBegin,
    GbufferEnd,
}

/// The state set around the per-object loops of `RenderZ` and `RenderGbuffer`.
pub fn plan_pass(p: Pass) -> Plan {
    use st::*;
    let ops = match p {
        Pass::ZBegin => vec![
            state(State::DepthTest, 1),
            state(State::DepthWrite, 1),
            state(State::Blend, BLEND_NONE),
            state(State::Alpha, ALPHA_SOLID),
            state(State::Channel, CHANNEL_NONE),
            Op::TexRangeNull { first: 0 },
        ],
        Pass::GbufferBegin => vec![
            state(State::DepthFunc, FUNC_EQUAL),
            state(State::DepthTest, 1),
            state(State::DepthWrite, 0),
            state(State::Blend, BLEND_NONE),
            state(State::Alpha, ALPHA_SOLID),
            state(State::Channel, CHANNEL_RGBA),
        ],
        Pass::GbufferEnd => vec![state(State::DepthFunc, FUNC_LESS_OR_EQUAL)],
    };
    fixed(ops)
}

#[derive(Clone, Copy, Debug, Default)]
pub struct ObjectsIn {
    pub use_occlusion_culling: bool,
    pub use_delayed_occlusion: bool,
    pub debug_render_buffers: bool,
    pub skip_passes: i32,
    pub occlusion_test_large_lights: bool,
    pub send_to_post_effects: bool,
    pub tonemap_ok: bool,
    pub is_reflection: bool,
}

struct Objects {
    i: ObjectsIn,
    stage: u8,
}

impl Objects {
    fn skip(&self, bit: i32) -> bool {
        self.i.skip_passes & bit != 0
    }
}

impl Source for Objects {
    fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool {
        use Step::*;
        let s = |x| Op::Step(x);
        match self.stage {
            0 => {
                out.extend([s(SetupRenderVariables), s(SetupGBuffer)]);
                if self.i.use_occlusion_culling {
                    out.extend([
                        s(CheckVisibleZ),
                        s(AssignOcclusionQueries),
                        s(SetupLights),
                        s(CompileOcclusion),
                    ]);
                } else {
                    if self.i.use_delayed_occlusion {
                        out.push(s(FetchDelayedOcclusion));
                    }
                    out.extend([
                        s(CheckVisibleStatic),
                        s(CheckVisibleDynamic),
                        s(CompileBruteForce),
                        s(RenderZ),
                    ]);
                    if self.i.use_delayed_occlusion {
                        out.push(s(RenderDelayedOcclusionQueries));
                    }
                    out.extend([s(AssignOcclusionQueries), s(SetupLights)]);
                }
                out.push(s(RenderGbuffer));
                if self.i.debug_render_buffers {
                    out.push(s(RenderGbufferContent));
                    return false;
                }
                if !self.skip(1) {
                    out.push(s(RenderDecals));
                }
                out.extend([s(CallbackPostGBuffer), s(RenderLights)]);
                if !self.skip(2) {
                    out.push(s(RenderIllumination));
                }
                out.push(Op::Ask(Query::FogAfterAreas));
                self.stage = 1;
                true
            }
            1 => {
                if feed.is_some_and(Feed::flag) {
                    out.extend([s(RenderFog), s(RenderFullScreenFog)]);
                } else {
                    out.extend([s(RenderFullScreenFog), s(RenderFog)]);
                }
                out.extend([s(RenderEdgeSmooth), Op::Ask(Query::FogIsSkybox)]);
                self.stage = 2;
                true
            }
            2 => {
                let fog_is_skybox = feed.is_some_and(Feed::flag);
                if !self.skip(4) && !fog_is_skybox {
                    out.push(s(RenderBasicSkyBox));
                }
                out.extend([
                    s(CallbackPostSolid),
                    s(UpdateLensFlares),
                    Op::Ask(Query::DepthOfField),
                ]);
                self.stage = 3;
                true
            }
            _ => {
                let dof = feed.is_some_and(Feed::flag);
                let particles = !self.skip(8);
                if particles {
                    out.push(s(RenderEnvironmentParticles(true)));
                    if dof {
                        out.push(s(RenderTranslucent(1)));
                    }
                }
                if !self.skip(32) {
                    out.push(s(RenderDepthOfField));
                }
                if particles {
                    out.push(s(RenderTranslucent(if dof { 2 } else { 0 })));
                    if dof {
                        out.push(s(RenderEnvironmentParticles(false)));
                    }
                }
                out.extend([s(RenderMultiIrisAndGlare), s(CallbackPostTranslucent)]);
                if self.i.occlusion_test_large_lights {
                    out.push(s(RetrieveLightOcclusion));
                }
                if self.i.send_to_post_effects && self.i.tonemap_ok && !self.i.is_reflection {
                    out.push(s(CopyAccumToPostEffects));
                }
                false
            }
        }
    }
}

/// `cRendererDeferred::RenderObjects`: the pass order of one frame.
pub fn plan_objects(i: ObjectsIn) -> Plan {
    Plan::new(Objects { i, stage: 0 })
}

/// `cRendererDeferred::SetupRenderVariables`: far plane and its corners.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FarPlane {
    pub plane: f32,
    pub top: f32,
    pub bottom: f32,
    pub right: f32,
    pub left: f32,
}

pub fn far_plane(fov: f32, aspect: f32, far: f32) -> FarPlane {
    // gcc negates the product (fnmul), which flips a NaN's sign.
    let top = -(std::hint::black_box(tan(fov * 0.5f32)) * far);
    let bottom = -top;
    let right = bottom * aspect;
    FarPlane {
        plane: far,
        top,
        bottom,
        right,
        left: -right,
    }
}

/// `cRendererDeferred::DrawAccumulationQuad` UV window.
pub fn accum_quad_uv(
    rt_pos: Vector2l,
    rt_size: Vector2l,
    screen: Vector2f,
    rect: bool,
) -> ([f32; 2], [f32; 2]) {
    let (px, py) = (rt_pos.x as f32, rt_pos.y as f32);
    let (sx, sy) = (rt_size.x as f32, rt_size.y as f32);
    let mut min = [px, (screen.y - sy) - py];
    let mut max = [px + sx, screen.y - py];
    if !rect {
        min = [min[0] / screen.x, min[1] / screen.y];
        max = [max[0] / screen.x, max[1] / screen.y];
    }
    (min, max)
}

/// `cRendererDeferred::SetScreenPositionVars`: `avScreenToFarPlane` and `avInvScreenSize`.
pub fn screen_position_vars(
    far: &FarPlane,
    rt_size: Vector2l,
    rt_pos: Vector2l,
    gbuffer_size: Vector2l,
) -> ([f32; 4], [f32; 2]) {
    let scale = [
        (far.right - far.left) / rt_size.x as f32,
        (far.bottom - far.top) / rt_size.y as f32,
    ];
    let origin = [
        rt_pos.x as f32,
        gbuffer_size
            .y
            .wrapping_sub(rt_size.y)
            .wrapping_sub(rt_pos.y) as f32,
    ];
    (
        [
            scale[0],
            scale[1],
            (-origin[0]).mul_add(scale[0], far.left),
            (-origin[1]).mul_add(scale[1], far.top),
        ],
        [
            1.0f32 / gbuffer_size.x as f32,
            1.0f32 / gbuffer_size.y as f32,
        ],
    )
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ShadowCfg {
    pub high: Vector2l,
    pub medium: Vector2l,
    pub low: Vector2l,
    pub jitter_size: i32,
    pub jitter_samples: i32,
}

/// Shadow map sizes and jitter settings of `LoadData`.
///
/// `resolution` and `quality` are 0 low, 1 medium, 2 high; counts are fixed at 1 high, 4 medium, 6 low.
pub fn shadow_cfg(resolution: i32, quality: i32, shader_model_4: bool) -> ShadowCfg {
    const SIZES: [i32; 5] = [128, 256, 256, 512, 1024];
    let start = match resolution {
        1 => 1usize,
        0 => 0,
        _ => 2,
    };
    let size = |res: usize| {
        let v = SIZES.get(start + res).copied().unwrap_or(0);
        Vector2l { x: v, y: v }
    };
    let (jitter_size, jitter_samples) = match (shader_model_4, quality) {
        (true, 2) => (32, 32),
        (true, 1) => (32, 16),
        (false, 2) => (32, 16),
        (false, 1) => (32, 4),
        _ => (0, 0),
    };
    ShadowCfg {
        low: size(0),
        medium: size(1),
        high: size(2),
        jitter_size,
        jitter_samples,
    }
}

/// Rows of the depth of field gauss table.
pub fn dof_rows(samples: i32) -> i32 {
    samples.wrapping_div(8).max(1)
}

/// `64 * rows * 4` floats of the depth of field gauss lookup.
pub fn dof_table(samples: i32, screen_x: f32) -> Vec<f32> {
    let rows = dof_rows(samples);
    let scale = if samples == 4 {
        256.0f32
    } else {
        samples as f32 * std::f32::consts::SQRT_2 * screen_x / 1280.0f32
    };
    let mut table = vec![0.0f32; 64usize.saturating_mul(rows as usize).saturating_mul(4)];
    for j in 0..rows {
        for x in 1..64i32 {
            for c in 0..4i32 {
                let k = (4 * j + 1 + c) as f32;
                let at = ((j * 64 + x) * 4 + c) as usize;
                let denom = (x.wrapping_mul(x) as f32) * scale;
                if let Some(slot) = table.get_mut(at) {
                    *slot = exp(k * k * -64.0f32 * 64.0f32 / denom);
                }
            }
        }
    }
    table
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SsaoMips {
    pub mip_size: Vector2l,
    pub mips: i32,
    pub levels: i32,
}

/// Mip chain sizes of the HPL3 SSAO texture.
pub fn ssao_mips(size: Vector2l) -> SsaoMips {
    let pot = |v: i32| 1i32.wrapping_shl((floor(log2(v as f32) + 0.5f32) as i32) as u32);
    let mip_size = Vector2l {
        x: pot(size.x),
        y: pot(size.y),
    };
    let big = mip_size.x.max(mip_size.y) as f32;
    SsaoMips {
        mip_size,
        mips: ((log2(big) as i32).wrapping_sub(4)).max(1),
        levels: (log2(big) as i32).wrapping_add(1),
    }
}

pub const GLARE_GRID: i32 = 20;
pub const GLARE_VERTS: usize = 400;
pub const GLARE_INDICES: usize = 19 * 19 * 6;

/// Positions (xyz per vertex) of the glare grid; interior points are jittered with the engine RNG.
pub fn glare_positions() -> Vec<f32> {
    let mut v = Vec::with_capacity(GLARE_VERTS * 3);
    for y in 0..GLARE_GRID {
        for x in 0..GLARE_GRID {
            let mut p = [x as f32 / 19.0f32, y as f32 / 19.0f32, 0.0f32];
            if x.wrapping_mul(y).wrapping_mul(19 - x).wrapping_mul(19 - y) != 0 {
                let min = [-1.0f32 / 76.0f32, -1.0f32 / 76.0f32, 0.0];
                let max = [1.0f32 / 76.0f32, 1.0f32 / 76.0f32, 0.0];
                p[0] += rand_rectf(min[0], max[0]);
                p[1] += rand_rectf(min[1], max[1]);
                p[2] += rand_rectf(min[2], max[2]);
            }
            v.extend(p);
        }
    }
    v
}

pub fn glare_indices() -> Vec<u32> {
    let mut v = Vec::with_capacity(GLARE_INDICES);
    let mut i = 21i32;
    while i < 400 {
        for j in i..i + 19 {
            v.extend([j - 1, j - 21, j, j, j - 21, j - 20].map(|k| k as u32));
        }
        i += 20;
    }
    v
}

/// Vertex and index capacity of the light batch buffer.
pub fn batch_sizes(vertices: i32, indices: i32, lights: i32) -> (i32, i32) {
    (vertices.wrapping_mul(lights), indices.wrapping_mul(lights))
}

pub mod dbg {
    pub const NULL: i32 = 0;
    pub const SHADOW: i32 = 1;
    pub const TEMP: i32 = 2;
    pub const DIR_SHADOW: i32 = 3;
    pub const REFRACTION: i32 = 4;
    pub const REFLECTION: i32 = 5;
    pub const BOX_WEIGHT: i32 = 6;
    pub const H3_SSAO: i32 = 7;
    pub const H3_MIP: i32 = 8;
    pub const ACCUM: i32 = 9;
    pub const BUFFER: i32 = 10;
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DebugTex {
    pub kind: i32,
    pub a: i32,
    pub b: i32,
}

/// `cRendererDeferred::GetDebugGBufferTexture`: which texture a debug index names.
///
/// `shadow_counts` are the sizes of the three shadow map lists; `Shadow` carries (list, index).
pub fn debug_gbuffer_texture(idx: i32, shadow_counts: [i32; 3]) -> DebugTex {
    let t = |kind, a, b| DebugTex { kind, a, b };
    let count = |r: i32| {
        usize::try_from(r)
            .ok()
            .and_then(|r| shadow_counts.get(r))
            .copied()
    };
    if idx >= 100 {
        let r = (idx - 100).wrapping_div(16);
        let i = (idx - 100).wrapping_rem(16);
        return match count(r) {
            Some(n) if i < n => t(dbg::SHADOW, r, i),
            _ => t(dbg::NULL, 0, 0),
        };
    }
    if (20..26).contains(&idx) {
        return t(dbg::TEMP, (idx - 20).wrapping_div(2), idx - 10);
    }
    if idx == 13 {
        return t(dbg::DIR_SHADOW, 0, 0);
    }
    if idx == 14 {
        return t(dbg::REFRACTION, 0, 0);
    }
    if idx == 15 {
        return t(dbg::REFLECTION, 0, 0);
    }
    if idx >= 10 {
        return match count(idx - 10) {
            Some(n) if n > 0 => t(dbg::SHADOW, idx - 10, 0),
            _ => t(dbg::NULL, 0, 0),
        };
    }
    if idx == 5 {
        return t(dbg::BOX_WEIGHT, 0, 0);
    }
    if (6..=8).contains(&idx) {
        return t(dbg::H3_SSAO, idx - 6, 0);
    }
    if idx == 9 {
        return t(dbg::H3_MIP, 0, 0);
    }
    if idx == 4 {
        return t(dbg::ACCUM, 0, 0);
    }
    t(dbg::BUFFER, idx, 0)
}

/// Tone map program defines of `GetToneMapProgram`, in the order the C++ added them.
pub mod define {
    pub const USE_UV: i32 = 0;
    pub const COLOR_GRADING: i32 = 1;
    pub const BLOOM: i32 = 2;
    pub const NOISE: i32 = 3;
    pub const FILM_GRAIN: i32 = 4;
    pub const SRGB: i32 = 5;
    pub const LAYERED_GRADING: i32 = 6;
    pub const BLEND_GRADING: i32 = 7;
    pub const HDR_OUTPUT: i32 = 8;
}

pub fn tonemap_defines(combo: i32, hpl35: bool) -> Vec<i32> {
    use define::*;
    let mut v = vec![USE_UV];
    if combo & 1 != 0 {
        v.push(COLOR_GRADING);
    }
    if combo & 2 != 0 {
        v.push(BLOOM);
    }
    if combo & 4 != 0 {
        v.push(if hpl35 { NOISE } else { FILM_GRAIN });
    }
    if combo & 8 != 0 {
        v.push(SRGB);
    }
    if combo & 16 != 0 {
        v.push(if hpl35 {
            LAYERED_GRADING
        } else {
            BLEND_GRADING
        });
    }
    if combo & 32 != 0 {
        v.push(HDR_OUTPUT);
    }
    v
}
