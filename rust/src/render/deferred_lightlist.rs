//! `SetupLightsAndRenderQueries` and `InitLightRendering`: per-light classification, the occlusion
//! query draws and the sorted render lists.

use super::deferred::{DlOp, Feed, Geom, Op, Out, Plan, Source, State, st, state};
use super::deferred_light::{
    LightIn, LightOut, RADIUS_MUL_LOW, RADIUS_MUL_MEDIUM, SortKey, kind, light_shape, list,
    list_less, res, shadow_map_resolution, slot,
};
use crate::graphics::stdsort;
use crate::math::{Matrixf, Rect2l, matrix_inverse, matrix_mul, matrix_scale};
use crate::types::{Vector2l, Vector3f};

unsafe extern "C" {
    fn ohpl_math_get_clip_rect_from_sphere(
        pos: *const Vector3f,
        radius: f32,
        view: *const Matrixf,
        fov: f32,
        near: f32,
        aspect: f32,
        screen: *const Vector2l,
        position_is_screen_space: bool,
        tan_half_fov: f32,
        out: *mut Rect2l,
    );
    fn ohpl_math_get_normalized_clip_rect_from_bv(
        bv_min: *const Vector3f,
        bv_max: *const Vector3f,
        view: *const Matrixf,
        proj: *const Matrixf,
        perspective: bool,
        fov: f32,
        near: f32,
        aspect: f32,
        tan_half_fov: f32,
        dest_min: *mut Vector3f,
        dest_max: *mut Vector3f,
    ) -> bool;
    fn ohpl_math_get_clip_rect_from_normalized_min_max(
        min: *const Vector3f,
        max: *const Vector3f,
        screen: *const Vector2l,
        out: *mut Rect2l,
    );
}

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

/// The camera: `cFrustum` values the light math reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FrustumIn {
    pub view: Matrixf,
    pub proj: Matrixf,
    pub perspective: bool,
    pub fov: f32,
    pub near: f32,
    pub aspect: f32,
    pub origin: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SetupIn {
    pub test_large_lights: bool,
    pub use_occlusion_culling: bool,
    pub render_shadows: bool,
    pub rt_size: Vector2l,
    pub min_large_normalized_area: f32,
    pub max_shadow_res: i32,
    pub shadow_dist_none: f32,
    pub shadow_dist_low: f32,
    pub shadow_dist_medium: f32,
    pub tan_half_fov: f32,
    pub sample_limit: i32,
    pub frustum: FrustumIn,
}

/// A light occlusion pair from last frame.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct PairIn {
    pub light: u64,
    pub samples: i32,
}

fn translation(m: &Matrixf) -> Vector3f {
    let c = |i: usize| m.m.get(i).map_or(0.0, |r| r[3]);
    v3(c(0), c(1), c(2))
}

/// `SetupLightMatrix`: view-space render and transform matrices of a point or spot light.
fn light_matrices(l: &LightIn, view: &Matrixf, radius_mul: f32) -> (Matrixf, Matrixf) {
    let scale = if l.kind == kind::POINT {
        let r = l.radius * radius_mul;
        v3(r, r, r)
    } else if l.kind == kind::SPOT {
        let far_height = l.tan_half_fov * l.radius * 2.0;
        let far_width = far_height * l.aspect;
        v3(far_width, far_height, l.radius)
    } else {
        return (Matrixf::default(), Matrixf::default());
    };
    let transform = matrix_mul(view, &l.world);
    let render = matrix_mul(&transform, &matrix_scale(scale));
    (render, transform)
}

fn point_clip(i: &SetupIn, l: &LightIn, render: &Matrixf) -> Rect2l {
    let f = &i.frustum;
    let pos = translation(render);
    let mut rect = Rect2l::default();
    // SAFETY: exported by crate::math; every pointer is a live local.
    unsafe {
        ohpl_math_get_clip_rect_from_sphere(
            &pos,
            l.radius,
            &f.view,
            f.fov,
            f.near,
            f.aspect,
            &i.rt_size,
            true,
            i.tan_half_fov,
            &mut rect,
        );
    }
    rect
}

fn spot_clip(i: &SetupIn, l: &LightIn) -> Rect2l {
    let f = &i.frustum;
    let (mut min, mut max) = (Vector3f::default(), Vector3f::default());
    // SAFETY: exported by crate::math; every pointer is a live local.
    let visible = unsafe {
        ohpl_math_get_normalized_clip_rect_from_bv(
            &l.bv_min,
            &l.bv_max,
            &f.view,
            &f.proj,
            f.perspective,
            f.fov,
            f.near,
            f.aspect,
            i.tan_half_fov,
            &mut min,
            &mut max,
        )
    };
    if !visible {
        return Rect2l {
            x: 0,
            y: 0,
            w: i.rt_size.x,
            h: i.rt_size.y,
        };
    }
    let mut rect = Rect2l::default();
    // SAFETY: as above.
    unsafe { ohpl_math_get_clip_rect_from_normalized_min_max(&min, &max, &i.rt_size, &mut rect) };
    rect
}

/// `cMath::Vector3Dist` as gcc contracts it.
fn dist(a: Vector3f, b: Vector3f) -> f32 {
    let (dx, dy, dz) = (b.x - a.x, b.y - a.y, b.z - a.z);
    dz.mul_add(dz, dx.mul_add(dx, dy * dy)).sqrt()
}

#[derive(Clone, Copy, PartialEq)]
enum Stage {
    Start,
    Next,
    AfterNear,
    AfterLine,
}

struct SetupLights {
    i: SetupIn,
    lights: Vec<LightIn>,
    prev_visible: Vec<u64>,
    stage: Stage,
    idx: usize,
    min_area: i32,
    cur: LightOut,
}

impl SetupLights {
    fn finish_light(&mut self, out: &mut Out<'_>) {
        let idx = self.idx as i32;
        let cur = self.cur;
        out.res.lights.lights.push(cur);
        out.push(Op::Dl(DlOp::Light(idx)));
        self.idx += 1;
        self.stage = Stage::Next;
        let Some(l) = self.lights.get(self.idx - 1) else {
            return;
        };
        if !self.i.test_large_lights || !self.i.use_occlusion_culling {
            return;
        }
        if cur.inside_near_plane || cur.area < self.min_area {
            return;
        }
        if self.prev_visible.contains(&l.id) {
            return;
        }
        out.extend([
            Op::Dl(DlOp::AcquireQuery(idx)),
            Op::Dl(DlOp::SetModelView(idx)),
            Op::Dl(DlOp::SetShape(light_shape(l.kind, 1))),
            Op::Dl(DlOp::QueryBegin(idx)),
            Op::Dl(DlOp::DrawCurrent),
            Op::Dl(DlOp::QueryEnd(idx)),
        ]);
    }

    fn after_near(&mut self, out: &mut Out<'_>, inside: bool) {
        let Some(l) = self.lights.get(self.idx).copied() else {
            return;
        };
        let f = self.i.frustum;
        let (render, transform) = light_matrices(&l, &f.view, RADIUS_MUL_MEDIUM);
        let clip = if l.kind == kind::POINT {
            point_clip(&self.i, &l, &render)
        } else {
            spot_clip(&self.i, &l)
        };
        self.cur = LightOut {
            render,
            transform,
            clip,
            area: clip.w.wrapping_mul(clip.h),
            inside_near_plane: inside,
            cast_shadows: false,
            shadow_res: 0,
        };
        if l.kind == kind::SPOT && l.cast_shadows && self.i.render_shadows {
            self.cur.cast_shadows = true;
            self.cur.shadow_res = shadow_map_resolution(l.shadow_res, self.i.max_shadow_res);
            if !inside {
                out.push(Op::AskGeom {
                    q: Geom::LineIntersection,
                    idx: self.idx as i32,
                    f: [0.0; 4],
                });
                self.stage = Stage::AfterLine;
                return;
            }
        }
        self.finish_light(out);
    }

    fn after_line(&mut self, out: &mut Out<'_>, hit: [f32; 3]) {
        let d = dist(self.i.frustum.origin, v3(hit[0], hit[1], hit[2]));
        let c = &mut self.cur;
        if d > self.i.shadow_dist_none {
            c.cast_shadows = false;
        } else if d > self.i.shadow_dist_low {
            if c.shadow_res == res::LOW {
                c.cast_shadows = false;
            }
            c.shadow_res = res::LOW;
        } else if d > self.i.shadow_dist_medium {
            c.shadow_res = if c.shadow_res == res::HIGH {
                res::MEDIUM
            } else {
                res::LOW
            };
        }
        self.finish_light(out);
    }
}

impl Source for SetupLights {
    fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool {
        match self.stage {
            Stage::Start => {
                if self.i.test_large_lights {
                    out.push(Op::Dl(DlOp::ClearPairs));
                }
                let area = (self.i.rt_size.x.wrapping_mul(self.i.rt_size.y)) as f32;
                self.min_area = (self.i.min_large_normalized_area * area) as i32;
                out.extend([
                    Op::Dl(DlOp::ClearTempLights),
                    Op::Dl(DlOp::SetMinLargeArea(self.min_area)),
                    state(State::DepthTest, 1),
                    state(State::DepthWrite, 0),
                    state(State::Blend, st::BLEND_NONE),
                    state(State::Alpha, st::ALPHA_SOLID),
                    state(State::Channel, st::CHANNEL_NONE),
                    Op::TexRangeNull { first: 0 },
                    Op::Program {
                        prog: slot::prog::DEPTH_ONLY,
                    },
                ]);
                self.stage = Stage::Next;
                true
            }
            Stage::Next => {
                let Some(l) = self.lights.get(self.idx).copied() else {
                    if self.i.test_large_lights {
                        out.push(Op::Dl(DlOp::Flush));
                    }
                    return false;
                };
                self.cur = LightOut::default();
                let idx = self.idx as i32;
                if l.kind == kind::POINT {
                    out.push(Op::AskGeom {
                        q: Geom::SphereNearPlane,
                        idx,
                        f: [
                            l.world_pos.x,
                            l.world_pos.y,
                            l.world_pos.z,
                            l.radius * RADIUS_MUL_LOW,
                        ],
                    });
                    self.stage = Stage::AfterNear;
                } else if l.kind == kind::SPOT {
                    out.push(Op::AskGeom {
                        q: Geom::FrustumNearPlane,
                        idx,
                        f: [0.0; 4],
                    });
                    self.stage = Stage::AfterNear;
                } else {
                    out.res.lights.lights.push(LightOut::default());
                    out.push(Op::Dl(DlOp::Light(idx)));
                    self.idx += 1;
                }
                true
            }
            Stage::AfterNear => {
                self.after_near(out, feed.is_some_and(Feed::flag));
                true
            }
            Stage::AfterLine => {
                self.after_line(out, feed.map_or([0.0; 3], Feed::vec3));
                true
            }
        }
    }
}

/// `cRendererDeferred::SetupLightsAndRenderQueries`. `pairs` are last frame's light occlusion pairs.
pub fn plan_setup_lights(i: SetupIn, lights: Vec<LightIn>, pairs: &[PairIn]) -> Plan {
    let prev_visible = if i.test_large_lights {
        pairs
            .iter()
            .filter(|p| p.samples > i.sample_limit)
            .map(|p| p.light)
            .collect()
    } else {
        Vec::new()
    };
    Plan::new(SetupLights {
        i,
        lights,
        prev_visible,
        stage: Stage::Start,
        idx: 0,
        min_area: 0,
        cur: LightOut::default(),
    })
}

/// A light after `SetupLightsAndRenderQueries`, as `InitLightRendering` sees it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct InitLight {
    pub light: LightIn,
    pub inside_near: bool,
    pub has_query: bool,
    pub area: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct InitIn {
    pub view: Matrixf,
    pub last_fov: f32,
    pub last_far_plane: f32,
    pub fov: f32,
    pub far_plane: f32,
    pub far_left: f32,
    pub far_bottom: f32,
    pub far_right: f32,
    pub far_top: f32,
    pub min_large_area: i32,
    pub sample_limit: i32,
}

#[derive(Clone, Copy, PartialEq)]
enum InitStage {
    Start,
    Next,
    AfterBoxNear,
    AfterFetch,
    AfterSamples,
}

struct InitLights {
    i: InitIn,
    lights: Vec<InitLight>,
    stage: InitStage,
    idx: usize,
}

impl InitLights {
    fn cur(&self) -> Option<InitLight> {
        self.lights.get(self.idx).copied()
    }

    fn push_list(&self, out: &mut Out<'_>, list: usize) {
        if let Some(l) = out.res.lights.lists.get_mut(list) {
            l.push(self.idx as u32);
        }
    }

    fn next(&mut self) {
        self.idx += 1;
        self.stage = InitStage::Next;
    }

    fn classify(&mut self, out: &mut Out<'_>) {
        let Some(l) = self.cur() else { return };
        out.res.lights.rendered = out.res.lights.rendered.wrapping_add(1);
        let k = l.light.kind;
        if l.inside_near {
            self.push_list(out, list::RENDER_BACK);
        } else if k == kind::POINT {
            let which = if l.area >= self.i.min_large_area {
                list::STENCIL_FRONT_RENDER_BACK
            } else {
                list::RENDER_BACK
            };
            self.push_list(out, which);
        } else if k == kind::SPOT {
            self.push_list(out, list::STENCIL_FRONT_RENDER_BACK);
        }
        self.next();
    }

    fn release_and_classify(&mut self, out: &mut Out<'_>, invisible: bool) {
        out.push(Op::Dl(DlOp::ReleaseQuery(self.idx as i32)));
        if invisible {
            self.next();
        } else {
            self.classify(out);
        }
    }
}

impl Source for InitLights {
    fn refill(&mut self, out: &mut Out<'_>, feed: Option<Feed>) -> bool {
        match self.stage {
            InitStage::Start => {
                let inv = matrix_inverse(&self.i.view);
                out.res.lights.inv_view = inv;
                out.res.lights.lights = vec![LightOut::default(); self.lights.len()];
                out.push(Op::Dl(DlOp::InvView));
                if self.i.last_fov != self.i.fov || self.i.last_far_plane != self.i.far_plane {
                    out.push(Op::Dl(DlOp::UpdateFullscreenQuad {
                        pos: [self.i.far_left, self.i.far_bottom, -self.i.far_plane],
                        size: [self.i.far_right * 2.0, self.i.far_top * 2.0],
                    }));
                }
                self.stage = InitStage::Next;
                true
            }
            InitStage::Next => {
                let Some(l) = self.cur() else {
                    for (n, v) in out.res.lights.lists.iter_mut().enumerate() {
                        let keys = &self.lights;
                        let key = |i: u32| {
                            keys.get(i as usize)
                                .map(|k| SortKey::of(&k.light, k.inside_near))
                                .unwrap_or_default()
                        };
                        stdsort::sort(v, |&a, &b| list_less(n, &key(a), &key(b)));
                    }
                    out.push(Op::Dl(DlOp::Lists));
                    return false;
                };
                let idx = self.idx as i32;
                if l.light.kind == kind::BOX {
                    let mut m = matrix_scale(l.light.box_size);
                    m.m[0][3] = l.light.world_pos.x;
                    m.m[1][3] = l.light.world_pos.y;
                    m.m[2][3] = l.light.world_pos.z;
                    let m = matrix_mul(&self.i.view, &m);
                    if let Some(o) = out.res.lights.lights.get_mut(self.idx) {
                        o.render = m;
                    }
                    out.res.lights.rendered = out.res.lights.rendered.wrapping_add(1);
                    out.extend([
                        Op::Dl(DlOp::LightMatrix(idx)),
                        Op::AskGeom {
                            q: Geom::BoxNearPlane,
                            idx,
                            f: [0.0; 4],
                        },
                    ]);
                    self.stage = InitStage::AfterBoxNear;
                } else if l.has_query {
                    out.push(Op::AskGeom {
                        q: Geom::QueryFetch,
                        idx,
                        f: [0.0; 4],
                    });
                    self.stage = InitStage::AfterFetch;
                } else {
                    self.classify(out);
                }
                true
            }
            InitStage::AfterBoxNear => {
                let which = if feed.is_some_and(Feed::flag) {
                    list::BOX_RENDER_BACK
                } else {
                    list::BOX_STENCIL_FRONT_RENDER_BACK
                };
                self.push_list(out, which);
                self.next();
                true
            }
            InitStage::AfterFetch => {
                if feed.is_some_and(Feed::flag) {
                    out.push(Op::AskGeom {
                        q: Geom::QuerySamples,
                        idx: self.idx as i32,
                        f: [0.0; 4],
                    });
                    self.stage = InitStage::AfterSamples;
                } else {
                    self.release_and_classify(out, false);
                }
                true
            }
            InitStage::AfterSamples => {
                let samples = feed.map_or(0, Feed::int);
                let invisible = samples <= self.i.sample_limit;
                out.push(Op::Dl(DlOp::QueryLog {
                    idx: self.idx as i32,
                    samples,
                    visible: !invisible,
                }));
                self.release_and_classify(out, invisible);
                true
            }
        }
    }
}

/// `cRendererDeferred::InitLightRendering`.
pub fn plan_init_lights(i: InitIn, lights: Vec<InitLight>) -> Plan {
    Plan::new(InitLights {
        i,
        lights,
        stage: InitStage::Start,
        idx: 0,
    })
}
