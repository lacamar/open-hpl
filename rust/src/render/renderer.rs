//! `iRenderer` culling decisions: object/node visibility, distance cull, the CHC occlusion
//! traversal with its node tracker, delayed occlusion, brute-force list building, shadow caster
//! selection and the fog-area maths.
//!
//! GL calls, queries and every renderable/node virtual stay C++ behind [`RnHost`]; this code
//! decides the order and the outcome. Handles are opaque addresses (0 = none).

use std::collections::{BTreeMap, BTreeSet, VecDeque};

use crate::graphics::stdsort;
use crate::math::{Matrixf, Planef};
use crate::math::{
    aabb_line_intersection, clamp, dot3, fast_random_float, fma, len3, matrix_mul_3x3,
    matrix_mul_vec, plane_aabb_collision, plane_aabb_sphere_collision, plane_dist, point_in_aabb,
    points_aabb_planes_collision, sub3, v3,
};
use crate::scene::boxtree_dyn::{Bv, Handle, Host};
use crate::types::Vector3f;

pub const INSIDE: i32 = 0;
pub const OUTSIDE: i32 = 1;

/// Renderable values `CheckObjectIsVisible` reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ObjCheck {
    pub visible: bool,
    pub culled_by_fog: bool,
    pub culled_by_distance: bool,
    pub has_bv: bool,
    pub render_flags: u32,
    pub fade: f32,
    pub min: Vector3f,
    pub max: Vector3f,
}

/// What the shadow caster sort and filter read of a material.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MatView {
    pub present: bool,
    pub translucent: bool,
    pub alpha_mode: i32,
    pub z_program: usize,
    pub diffuse_texture: usize,
}

/// The parts of a `cFrustum` the shadow caster test reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FrustumData {
    pub origin: Vector3f,
    pub forward: Vector3f,
    pub planes: [Planef; 6],
}

/// `iRenderer` state the visibility checks read.
#[derive(Clone, Copy, Debug)]
pub struct Checks<'a> {
    pub occlusion_planes_active: bool,
    pub planes: &'a [Planef],
    pub fog_cull_plane_active: bool,
    pub fog_cull_plane: Planef,
    pub distance_cull_active: bool,
    pub dc_origin: Vector3f,
    pub dc_min_range_sqr: f32,
    pub dc_screen_size_sqr: f32,
    pub dc_random_size: f32,
    pub dc_fade_time: f32,
    pub dc_fade_time_inv: f32,
    pub dc_max_range_sqr: f32,
    pub dc_time: f32,
}

pub trait RnHost: Host {
    fn update_node(&mut self, node: Handle);
    fn collide_node(&mut self, frustum: Handle, node: Handle) -> i32;
    fn collide_object(&mut self, frustum: Handle, obj: Handle) -> i32;
    fn object_collides_frustum(&mut self, frustum: Handle, obj: Handle) -> bool;
    fn near_plane_aabb(&mut self, frustum: Handle, min: Vector3f, max: Vector3f) -> bool;
    fn object_check(&mut self, obj: Handle) -> ObjCheck;
    fn set_object_fade(&mut self, obj: Handle, fade: f32);
    fn object_material(&mut self, obj: Handle) -> MatView;
    fn set_view_space_z(&mut self, obj: Handle, z: f32);
    fn view_space_z(&mut self, obj: Handle) -> f32;
    fn add_to_render_list(&mut self, obj: Handle);
    /// The CHC object callback: adds the object and renders it when solid; true if rendered.
    fn render_object(&mut self, obj: Handle) -> bool;
    fn render_node_bbox(&mut self, node: Handle, query: Handle);
    fn get_query(&mut self) -> Handle;
    fn release_query(&mut self, query: Handle);
    fn query_fetch(&mut self, query: Handle) -> bool;
    fn query_samples(&mut self, query: Handle) -> u32;
    fn flush(&mut self);
}

fn dist_sqr(a: Vector3f, b: Vector3f) -> f32 {
    let d = sub3(b, a);
    dot3(d, d)
}

/// `iRenderer::CheckObjectDistanceCull`; `v` was fetched by the caller.
fn distance_cull<H: RnHost>(h: &mut H, ck: &Checks, obj: Handle, v: &ObjCheck) -> bool {
    if !ck.distance_cull_active || !v.culled_by_distance || !v.has_bv {
        return false;
    }
    let (min, max) = (v.min, v.max);
    let closest = v3(
        clamp(ck.dc_origin.x, min.x, max.x),
        clamp(ck.dc_origin.y, min.y, max.y),
        clamp(ck.dc_origin.z, min.z, max.z),
    );
    let d_sqr = dist_sqr(closest, ck.dc_origin);

    // (int)(size_t)pointer
    let seed = obj as u32 as i32;
    let scale = fma(fast_random_float(seed) + 1.0, ck.dc_random_size, 1.0);
    let scale_sqr = scale * scale;

    let ext = sub3(max, min);
    let (mut a, mut b, mut c) = (ext.x, ext.y, ext.z);
    if a < b {
        std::mem::swap(&mut a, &mut b);
    }
    if b < c {
        std::mem::swap(&mut b, &mut c);
    }
    if a < b {
        std::mem::swap(&mut a, &mut b);
    }
    let size = (a + b) * 0.5;

    let small = size * size * scale_sqr < ck.dc_screen_size_sqr * d_sqr;
    let visible = !((small || ck.dc_max_range_sqr * scale_sqr < d_sqr)
        && ck.dc_min_range_sqr * scale_sqr <= d_sqr);

    let mut fade = v.fade;
    let t = ck.dc_time;
    let fade_time = ck.dc_fade_time;
    if visible {
        if fade < 0.0 {
            let elapsed = fade + t;
            fade = if fade_time > elapsed {
                t - fade_time + elapsed
            } else {
                t
            };
        }
    } else if fade > 0.0 {
        let elapsed = t - fade;
        fade = if fade_time > elapsed {
            fade_time - t - elapsed
        } else {
            -t
        };
    }
    h.set_object_fade(obj, fade);

    fade < 0.0 && fade + t > fade_time
}

/// `iRenderer::GetDistanceFadeAmount`.
pub fn distance_fade_amount(ck: &Checks, fade: f32) -> f32 {
    if !ck.distance_cull_active {
        return 1.0;
    }
    if fade < 0.0 {
        return clamp(
            fma(-(fade + ck.dc_time), ck.dc_fade_time_inv, 1.0),
            0.0,
            1.0,
        );
    }
    clamp((ck.dc_time - fade) * ck.dc_fade_time_inv, 0.0, 1.0)
}

/// `iRenderer::CheckObjectIsVisible`.
pub fn object_visible<H: RnHost>(h: &mut H, ck: &Checks, obj: Handle, needed: u32) -> bool {
    let v = h.object_check(obj);
    if !v.visible {
        return false;
    }
    if v.render_flags & needed != needed {
        return false;
    }
    // A null bounding volume was a null dereference in the C++; such an object skips the plane tests.
    if !ck.planes.is_empty() && ck.occlusion_planes_active && v.has_bv {
        for p in ck.planes {
            if plane_aabb_collision(p, v.min, v.max) == OUTSIDE {
                return false;
            }
        }
    }
    if ck.fog_cull_plane_active
        && ck.occlusion_planes_active
        && v.culled_by_fog
        && v.has_bv
        && plane_aabb_collision(&ck.fog_cull_plane, v.min, v.max) == OUTSIDE
    {
        return false;
    }
    !distance_cull(h, ck, obj, &v)
}

/// `iRenderer::CheckNodeIsVisible`.
pub fn node_visible<H: Host>(h: &mut H, ck: &Checks, node: Handle) -> bool {
    if !ck.occlusion_planes_active || ck.planes.is_empty() {
        return true;
    }
    let b = *h.base(node);
    for p in ck.planes {
        if plane_aabb_sphere_collision(p, b.min, b.max, b.center, b.radius) == OUTSIDE {
            return false;
        }
    }
    true
}

/// `cVisibleRCNodeTracker`: nodes visible last frame plus the delayed-occlusion bookkeeping.
#[derive(Debug, Default)]
pub struct Tracker {
    visible: [BTreeSet<Handle>; 2],
    current: usize,
    pub pending: BTreeMap<Handle, Handle>,
    occluded_frame: BTreeMap<Handle, i32>,
    query_nodes: Vec<Handle>,
    frame_counter: i32,
}

impl Tracker {
    pub fn switch_and_clear(&mut self) {
        self.current += 1;
        if self.current >= 2 {
            self.current = 0;
        }
        if let Some(s) = self.visible.get_mut(self.current) {
            s.clear();
        }
    }

    pub fn set_visible(&mut self, node: Handle) {
        if let Some(s) = self.visible.get_mut(self.current) {
            s.insert(node);
        }
    }

    pub fn was_visible(&self, node: Handle) -> bool {
        let prev = usize::from(self.current == 0);
        self.visible.get(prev).is_some_and(|s| s.contains(&node))
    }

    /// Also drops the pending queries; the caller deletes them first (see [`Self::pending_queries`]).
    pub fn reset(&mut self) {
        self.current = 0;
        self.frame_counter = 0;
        for s in &mut self.visible {
            s.clear();
        }
        self.pending.clear();
        self.occluded_frame.clear();
        self.query_nodes.clear();
    }

    pub fn pending_queries(&self) -> Vec<Handle> {
        self.pending.values().copied().collect()
    }

    fn push_up_visibility<H: Host>(&mut self, h: &mut H, mut node: Handle) {
        while node != 0 {
            self.set_visible(node);
            node = h.base(node).parent;
        }
    }
}

/// `iRenderer::FetchDelayedOcclusionResults`; returns the frame counter value used.
pub fn fetch_delayed_results<H: RnHost>(h: &mut H, t: &mut Tracker, sample_limit: i32) {
    t.frame_counter = t.frame_counter.wrapping_add(1);
    let frame = t.frame_counter;
    t.query_nodes.clear();

    let keys: Vec<Handle> = t.pending.keys().copied().collect();
    for node in keys {
        let Some(&query) = t.pending.get(&node) else {
            continue;
        };
        if !h.query_fetch(query) {
            continue;
        }
        if h.query_samples(query) as i32 > sample_limit {
            t.occluded_frame.remove(&node);
        } else {
            t.occluded_frame.insert(node, frame);
        }
        h.release_query(query);
        t.pending.remove(&node);
    }

    if frame & 255 == 0 {
        t.occluded_frame.retain(|_, f| *f >= frame.wrapping_sub(8));
    }
}

/// `iRenderer::DelayedOcclusionCulled`.
pub fn delayed_culled<H: RnHost>(
    h: &mut H,
    t: &mut Tracker,
    frustum: Handle,
    node: Handle,
) -> bool {
    if h.child_count(node) > 0 || h.object_count(node) == 0 {
        return false;
    }
    let b = *h.base(node);
    if h.near_plane_aabb(frustum, b.min, b.max) {
        return false;
    }
    let frame = t.frame_counter;
    let pending = t.pending.contains_key(&node);
    let occluded = t
        .occluded_frame
        .get(&node)
        .is_some_and(|f| *f >= frame.wrapping_sub(4));
    if !pending && (occluded || (frame.wrapping_add((node >> 6) as i32) & 3) == 0) {
        t.query_nodes.push(node);
    }
    occluded
}

/// `iRenderer::RenderDelayedOcclusionQueries` after the caller set the render states.
/// Returns false when there was nothing to render (the C++ returns before setting states).
pub fn has_delayed_queries(t: &Tracker) -> bool {
    !t.query_nodes.is_empty()
}

pub fn render_delayed_queries<H: RnHost>(h: &mut H, t: &mut Tracker) {
    for node in std::mem::take(&mut t.query_nodes) {
        let q = h.get_query();
        h.render_node_bbox(node, q);
        t.pending.insert(node, q);
    }
}

/// `iRenderer::CheckNodesAndAddToListIterative`.
pub fn add_visible_to_list<H: RnHost>(
    h: &mut H,
    ck: &Checks,
    t: &mut Tracker,
    frustum: Handle,
    use_delayed: bool,
    needed: u32,
    node: Handle,
) {
    h.update_node(node);
    let collision = h.collide_node(frustum, node);
    if h.base(node).parent != 0 {
        if collision == OUTSIDE {
            return;
        }
        if !node_visible(h, ck, node) {
            return;
        }
        if use_delayed && delayed_culled(h, t, frustum, node) {
            return;
        }
    }
    let mut kids = Vec::new();
    h.children(node, &mut kids);
    for c in kids {
        add_visible_to_list(h, ck, t, frustum, use_delayed, needed, c);
    }
    let mut objs = Vec::new();
    h.objects(node, &mut objs);
    for o in objs {
        if !object_visible(h, ck, o, needed) {
            continue;
        }
        if collision == INSIDE || h.object_collides_frustum(frustum, o) {
            h.add_to_render_list(o);
        }
    }
}

/// Inputs of one CHC traversal.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Chc {
    pub frustum: Handle,
    pub view: Matrixf,
    pub origin: Vector3f,
    pub needed_flags: u32,
    pub min_objects_before_occlusion: i32,
    pub sample_limit: i32,
    pub only_render_prev_visible: bool,
    pub only_prev_visible_frame_count: i32,
}

struct Pair {
    node: Handle,
    query: Handle,
    objects_rendered: bool,
}

/// `cRendererNodeSortFunc` over the node state.
fn node_before<H: Host>(h: &mut H, a: Handle, b: Handle) -> bool {
    let (ai, ad) = {
        let x = h.base(a);
        (x.inside_view, x.view_distance)
    };
    let (bi, bd) = {
        let x = h.base(b);
        (x.inside_view, x.view_distance)
    };
    if ai != bi {
        return ai && !bi;
    }
    ad > bd
}

/// `std::multiset::insert`: after every element not ordered after the new one.
fn stack_insert<H: Host>(h: &mut H, stack: &mut Vec<Handle>, node: Handle) {
    let mut at = stack.len();
    for (i, &e) in stack.iter().enumerate() {
        if node_before(h, node, e) {
            at = i;
            break;
        }
    }
    stack.insert(at, node);
}

fn push_children<H: RnHost>(
    h: &mut H,
    ck: &Checks,
    c: &Chc,
    stack: &mut Vec<Handle>,
    node: Handle,
) {
    if h.child_count(node) == 0 {
        return;
    }
    let mut kids = Vec::new();
    h.children(node, &mut kids);
    for child in kids {
        h.update_node(child);
        {
            let cb = h.base(child);
            if cb.uses_flags_and_visibility
                && (!cb.visible_objects || cb.render_flags & c.needed_flags != c.needed_flags)
            {
                continue;
            }
        }
        let collision = if h.base(node).prev_collision == INSIDE {
            INSIDE
        } else {
            h.collide_node(c.frustum, child)
        };
        if collision == OUTSIDE {
            continue;
        }
        if !node_visible(h, ck, child) {
            continue;
        }
        h.base(child).prev_collision = collision;

        let b = *h.base(child);
        if h.near_plane_aabb(c.frustum, b.min, b.max) {
            let view_space = matrix_mul_vec(&c.view, b.center);
            let cb = h.base(child);
            cb.view_distance = view_space.z;
            cb.inside_view = true;
        } else {
            let mut hit = None;
            aabb_line_intersection(b.min, b.max, c.origin, b.center, &mut hit);
            let view_space = matrix_mul_vec(&c.view, hit.unwrap_or_default());
            let cb = h.base(child);
            cb.view_distance = view_space.z;
            cb.inside_view = false;
        }
        stack_insert(h, stack, child);
    }
}

fn render_node_objects<H: RnHost>(h: &mut H, ck: &Checks, c: &Chc, node: Handle) -> i32 {
    if h.object_count(node) == 0 {
        return 0;
    }
    let mut rendered = 0;
    let mut objs = Vec::new();
    h.objects(node, &mut objs);
    let prev_inside = h.base(node).prev_collision == INSIDE;
    for o in objs {
        if !object_visible(h, ck, o, c.needed_flags) {
            continue;
        }
        if !prev_inside && !h.object_collides_frustum(c.frustum, o) {
            continue;
        }
        if h.render_object(o) {
            rendered += 1;
        }
    }
    rendered
}

/// `iRenderer::CheckForVisibleObjectsAddToListAndRenderZ` from the root insertion on.
/// `roots` are the roots of the container types asked for. Returns the new debug frame count.
pub fn chc_traverse<H: RnHost>(
    h: &mut H,
    ck: &Checks,
    t: &mut Tracker,
    c: &Chc,
    roots: &[Handle],
) -> i32 {
    let mut stack: Vec<Handle> = Vec::new();
    let mut pairs: VecDeque<Pair> = VecDeque::new();
    let mut frame_count = c.only_prev_visible_frame_count;

    for &root in roots {
        h.update_node(root);
        h.base(root).inside_view = true;
        stack_insert(h, &mut stack, root);
    }

    let mut min_rendered = c.min_objects_before_occlusion;
    while !stack.is_empty() || !pairs.is_empty() {
        if !stack.is_empty() {
            let node = stack.remove(0);
            let leaf = h.child_count(node) == 0;
            let near_plane_inside = h.base(node).inside_view;
            let was_visible = t.was_visible(node);

            if !near_plane_inside && min_rendered <= 0 && (!was_visible || leaf) {
                let render_objects = leaf && was_visible;
                if c.only_render_prev_visible && frame_count >= 1 {
                    h.render_node_bbox(node, 0);
                } else {
                    let query = h.get_query();
                    h.render_node_bbox(node, query);
                    pairs.push_back(Pair {
                        node,
                        query,
                        objects_rendered: render_objects,
                    });
                }
                if render_objects {
                    render_node_objects(h, ck, c, node);
                }
                if c.only_render_prev_visible && frame_count >= 1 && render_objects && was_visible {
                    t.push_up_visibility(h, node);
                }
            } else {
                push_children(h, ck, c, &mut stack, node);
                min_rendered -= render_node_objects(h, ck, c, node);
            }

            if stack.is_empty() {
                h.flush();
            }
        }

        if stack.is_empty() && !pairs.is_empty() {
            let Some(front) = pairs.front() else { continue };
            let (query, node, rendered) = (front.query, front.node, front.objects_rendered);
            if h.query_fetch(query) {
                let samples = h.query_samples(query) as i32;
                h.release_query(query);
                pairs.pop_front();
                if samples > c.sample_limit {
                    t.push_up_visibility(h, node);
                    push_children(h, ck, c, &mut stack, node);
                    if !rendered {
                        render_node_objects(h, ck, c, node);
                    }
                }
            }
        }
    }

    if c.only_render_prev_visible {
        frame_count = frame_count.wrapping_add(1);
    }
    frame_count
}

#[derive(Clone, Copy, Default)]
struct CasterKey {
    obj: Handle,
    mat: MatView,
    z: f32,
}

/// `SortFunc_ShadowCasters`.
fn caster_less(a: &CasterKey, b: &CasterKey) -> bool {
    if a.mat.alpha_mode != b.mat.alpha_mode {
        return a.mat.alpha_mode < b.mat.alpha_mode;
    }
    // eMaterialAlphaMode_Trans
    if a.mat.alpha_mode == 1 {
        if a.mat.z_program != b.mat.z_program {
            return a.mat.z_program < b.mat.z_program;
        }
        if a.mat.diffuse_texture != b.mat.diffuse_texture {
            return a.mat.diffuse_texture < b.mat.diffuse_texture;
        }
    }
    a.z < b.z
}

/// View-frustum side of the shadow caster test (`GetShadowCasters` with `abViewCheck`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ViewCheck {
    planes: [Planef; 6],
    beyond: [usize; 6],
    beyond_count: usize,
}

impl ViewCheck {
    /// `SetupBeyondLightAndViewPlanes`: the view planes facing the light's forward vector.
    pub fn new(view: &FrustumData, light: &FrustumData) -> Self {
        let mut s = Self {
            planes: view.planes,
            beyond: [0; 6],
            beyond_count: 0,
        };
        for (i, p) in view.planes.iter().enumerate() {
            let normal = v3(p.a, p.b, p.c);
            if dot3(normal, light.forward) > 0.0 {
                if let Some(b) = s.beyond.get_mut(s.beyond_count) {
                    *b = i;
                }
                s.beyond_count += 1;
            }
        }
        s
    }

    /// `CheckShadowCasterContributesToView` with the light known not to be behind the near plane.
    fn contributes(&self, bv: &Bv) -> bool {
        let mut inside = 0;
        let mut outside = 0;
        for &pi in self.beyond.iter().take(self.beyond_count) {
            let Some(plane) = self.planes.get(pi) else {
                continue;
            };
            let d = plane_dist(plane, bv.center);
            if d > bv.radius {
                inside += 1;
            } else if d < -bv.radius {
                outside += 1;
                break;
            }
        }
        if inside == self.beyond_count {
            return true;
        }
        if outside > 0 {
            return false;
        }
        let (max, min) = (bv.max, bv.min);
        let corners = [
            v3(max.x, max.y, max.z),
            v3(max.x, max.y, min.z),
            v3(max.x, min.y, max.z),
            v3(max.x, min.y, min.z),
            v3(min.x, max.y, max.z),
            v3(min.x, max.y, min.z),
            v3(min.x, min.y, max.z),
            v3(min.x, min.y, min.z),
        ];
        let mut contributes = false;
        for &pi in self.beyond.iter().take(self.beyond_count) {
            let Some(plane) = self.planes.get(pi) else {
                continue;
            };
            contributes = corners.iter().any(|c| plane_dist(plane, *c) > 0.0);
            if !contributes {
                break;
            }
        }
        contributes
    }
}

pub struct ShadowQuery<'a> {
    pub ck: &'a Checks<'a>,
    pub light: Handle,
    pub light_origin: Vector3f,
    pub min_radius: f32,
    pub caster_flag: u32,
    pub view_check: Option<ViewCheck>,
}

fn shadow_casters_rec<H: RnHost>(
    h: &mut H,
    q: &ShadowQuery,
    node: Handle,
    prev: i32,
    out: &mut Vec<Handle>,
) {
    h.update_node(node);
    let collision = if prev == INSIDE {
        prev
    } else {
        h.collide_node(q.light, node)
    };
    if h.base(node).parent != 0 {
        if collision == OUTSIDE {
            return;
        }
        if !node_visible(h, q.ck, node) {
            return;
        }
    }
    let mut kids = Vec::new();
    h.children(node, &mut kids);
    for c in kids {
        shadow_casters_rec(h, q, c, collision, out);
    }
    let mut objs = Vec::new();
    h.objects(node, &mut objs);
    for o in objs {
        if !object_visible(h, q.ck, o, q.caster_flag) {
            continue;
        }
        let m = h.object_material(o);
        if !m.present || m.translucent {
            continue;
        }
        let bv = h.object_bv(o);
        if bv.radius < q.min_radius {
            continue;
        }
        if collision != INSIDE && h.collide_object(q.light, o) == OUTSIDE {
            continue;
        }
        if let Some(v) = &q.view_check
            && !v.contributes(&bv)
        {
            continue;
        }
        h.set_view_space_z(o, dist_sqr(bv.center, q.light_origin));
        out.push(o);
    }
}

/// `iRenderer::GetShadowCasters`: appends the casters under `root` to `out`.
pub fn shadow_casters<H: RnHost>(h: &mut H, q: &ShadowQuery, root: Handle, out: &mut Vec<Handle>) {
    shadow_casters_rec(h, q, root, OUTSIDE, out);
}

/// The tail of `SetupShadowMapRendering`: orders `casters` for rendering by the view-space z
/// stored on each object.
pub fn sort_shadow_casters<H: RnHost>(h: &mut H, casters: &mut [Handle]) {
    let mut keys: Vec<CasterKey> = casters
        .iter()
        .map(|&obj| CasterKey {
            obj,
            mat: h.object_material(obj),
            z: h.view_space_z(obj),
        })
        .collect();
    stdsort::sort(&mut keys, caster_less);
    for (c, k) in casters.iter_mut().zip(&keys) {
        *c = k.obj;
    }
}

const FOG_BOX_NORMALS: [Vector3f; 6] = [
    v3(-1.0, 0.0, 0.0),
    v3(1.0, 0.0, 0.0),
    v3(0.0, -1.0, 0.0),
    v3(0.0, 1.0, 0.0),
    v3(0.0, 0.0, -1.0),
    v3(0.0, 0.0, 1.0),
];

/// `iRenderer::CheckFogAreaInsideNearPlane`; `verts` are the four near plane vertices.
pub fn fog_inside_near_plane(inv_box: &Matrixf, verts: &[Vector3f; 4]) -> bool {
    let box_verts = verts.map(|v| matrix_mul_vec(inv_box, v));
    let (min, max) = (v3(-0.5, -0.5, -0.5), v3(0.5, 0.5, 0.5));
    if box_verts.iter().any(|v| point_in_aabb(*v, min, max)) {
        return true;
    }
    points_aabb_planes_collision(&box_verts, min, max) != OUTSIDE
}

/// `iRenderer::CheckFogAreaRayIntersection`: `(entry, exit)` distances.
pub fn fog_ray_intersection(
    inv_box: &Matrixf,
    start: Vector3f,
    dir: Vector3f,
) -> Option<(f32, f32)> {
    let box_dir = matrix_mul_3x3(inv_box, dir);
    let mut found = false;
    let mut entry = 0.0f32;
    let mut exit = 0.0f32;
    for n in &FOG_BOX_NORMALS {
        let mul = dot3(*n, box_dir);
        if mul.abs() < 0.0001 {
            continue;
        }
        let neg_dist = -(dot3(*n, start) + 0.5);
        let t = neg_dist / mul;
        if t < 0.0 {
            continue;
        }
        let p = v3(
            fma(box_dir.x, t, start.x).abs(),
            fma(box_dir.y, t, start.y).abs(),
            fma(box_dir.z, t, start.z).abs(),
        );
        let limit = 0.5001f32;
        if p.x > limit || p.y > limit || p.z > limit {
            continue;
        }
        if !found {
            entry = t;
            exit = t;
            found = true;
        } else {
            entry = if entry < t { entry } else { t };
            exit = if exit > t { exit } else { t };
        }
    }
    if exit < 0.0 || !found {
        return None;
    }
    Some((entry, exit))
}

/// `cFogArea` values `GetFogAreaVisibilityForObject` reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FogParams {
    pub show_backside_inside: bool,
    pub show_backside_outside: bool,
    pub inside_near_plane: bool,
    pub start: f32,
    pub end: f32,
    pub color_a: f32,
    pub falloff_exp: f32,
}

unsafe extern "C" {
    fn powf(x: f32, y: f32) -> f32;
}

/// `iRenderer::GetFogAreaVisibilityForObject`.
pub fn fog_visibility(
    f: &FogParams,
    inv_box: &Matrixf,
    box_origin: Vector3f,
    frustum_origin: Vector3f,
    object_pos: Vector3f,
) -> f32 {
    let ray = sub3(object_pos, frustum_origin);
    let camera_dist = len3(ray);
    let dir = v3(
        ray.x / camera_dist,
        ray.y / camera_dist,
        ray.z / camera_dist,
    );

    let Some((entry, exit)) = fog_ray_intersection(inv_box, box_origin, dir) else {
        return 1.0;
    };
    if !f.inside_near_plane && camera_dist < entry {
        return 1.0;
    }
    let min = |a: f32, b: f32| if a < b { a } else { b };
    let fog_dist = if f.inside_near_plane {
        if f.show_backside_inside {
            min(exit, camera_dist)
        } else {
            camera_dist
        }
    } else if f.show_backside_outside {
        min(exit - entry, camera_dist - entry)
    } else {
        camera_dist - entry
    };
    if fog_dist <= 0.0 {
        return 1.0;
    }
    let alpha_floor = 1.0 - f.color_a;
    if fog_dist < f.start {
        return 1.0;
    }
    if fog_dist > f.end {
        return alpha_floor;
    }
    let mut alpha = (fog_dist - f.start) / (f.end - f.start);
    if f.falloff_exp != 1.0 {
        // SAFETY: pure libm function on plain floats.
        alpha = unsafe { powf(alpha, f.falloff_exp) };
    }
    fma(alpha_floor, alpha, 1.0 - alpha)
}

/// `SetupFogRenderDataArray`'s sort: order of `view_z` as `std::sort` with `<` leaves it.
pub fn fog_sort_order(view_z: &[f32], out: &mut [u32]) {
    let n = view_z.len().min(out.len());
    for (i, o) in (0u32..).zip(out.iter_mut()) {
        *o = i;
    }
    let Some(out) = out.get_mut(..n) else { return };
    stdsort::sort(out, |&a, &b| {
        match (view_z.get(a as usize), view_z.get(b as usize)) {
            (Some(x), Some(y)) => x < y,
            _ => false,
        }
    });
}

/// `iRenderer::GetShadowMapResolution`: `eShadowMapResolution` is Low 0, Medium 1, High 2.
pub fn shadow_map_resolution(wanted: i32, max: i32) -> i32 {
    if max == 2 {
        wanted
    } else if max == 1 {
        if wanted == 2 { 1 } else { wanted }
    } else {
        0
    }
}

/// A shadow map as `GetShadowMapData` sees it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ShadowSlot {
    pub light: Handle,
    pub frame_count: i32,
}

/// `iRenderer::GetShadowMapData`: the slot to use, and whether its frame count is to be updated
/// (a single slot is returned untouched).
pub fn pick_shadow_map(slots: &[ShadowSlot], light: Handle, frame: i32) -> (Option<usize>, bool) {
    if slots.len() == 1 {
        return (Some(0), false);
    }
    let mut best = None;
    let mut max_dist = -1i32;
    for (i, s) in slots.iter().enumerate() {
        if s.light == light {
            best = Some(i);
            break;
        }
        let dist = s.frame_count.wrapping_sub(frame).wrapping_abs();
        if dist > max_dist {
            max_dist = dist;
            best = Some(i);
        }
    }
    (best, best.is_some())
}

/// `cShadowMapLightCache`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ShadowCache {
    pub light: Handle,
    pub transform_count: i32,
    pub radius: f32,
    pub fov: f32,
    pub aspect: f32,
    pub near_clip: f32,
}

/// The light values `ShadowMapNeedsUpdate` compares the cache with.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LightKey {
    pub light: Handle,
    pub transform_count: i32,
    pub radius: f32,
    pub is_spot: bool,
    pub fov: f32,
    pub aspect: f32,
    pub near_clip: f32,
}

/// `ShadowMapNeedsUpdate` up to the shadow caster comparison.
pub fn shadow_cache_valid(c: &ShadowCache, l: &LightKey) -> bool {
    let valid =
        c.light == l.light && c.transform_count == l.transform_count && c.radius == l.radius;
    if valid && l.is_spot {
        return l.aspect == c.aspect && l.fov == c.fov && l.near_clip == c.near_clip;
    }
    valid
}
