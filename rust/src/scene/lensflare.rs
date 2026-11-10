//! `cLensFlare` / `cLensFlareType`: visibility smoothing, per-type colour and size, glare, the
//! billboard matrix and the multi-iris quad generator. Entities, materials and vertex buffers stay
//! in C++ and cross as plain values.

use crate::math::{
    IDENTITY, Matrixf, PI, TWO_PI, Vector2f, Vector2l, Vector3f, atan2, clamp, cos, cross3, dot3,
    fma, len3, matrix_mul, matrix_mul_divide_w, matrix_mul_vec, matrix_scale, rotate_z, sin, sub3,
    v3,
};
use crate::types::Color;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Kind {
    Anamorphic,
    Halo,
    MultiIris,
}

impl Kind {
    pub fn from_i32(v: i32) -> Option<Self> {
        match v {
            0 => Some(Self::Anamorphic),
            1 => Some(Self::Halo),
            2 => Some(Self::MultiIris),
            _ => None,
        }
    }
}

fn sel<T: Copy>(a: &[T; 3], k: Kind) -> T {
    let [anamorphic, halo, iris] = *a;
    match k {
        Kind::Anamorphic => anamorphic,
        Kind::Halo => halo,
        Kind::MultiIris => iris,
    }
}

/// Scalar members of `cLensFlare`; `active` already includes the material check.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FlareIn {
    pub active: [bool; 3],
    pub color: [Color; 3],
    pub size: [Vector2f; 3],
    pub range_max: Vector2f,
    pub range_min: Vector2f,
    pub glare_range: Vector2f,
    pub atlas_grid: Vector2l,
    pub multi_iris_seed: i32,
    pub multi_iris_count: i32,
    pub size_by_dist: f32,
    pub outer_cos: f32,
    pub inner_cos: f32,
    pub glare_cos: f32,
    pub glare_brightness: f32,
    pub glare_stare_at: f32,
    pub fade: f32,
    pub brightness: f32,
    pub shrink_when_occluded: bool,
    pub mul_iris_with_glare: bool,
}

/// What the flare reads from its entity: world position and `FacingAxis`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EntIn {
    pub pos: Vector3f,
    pub axis: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ViewIn {
    pub origin: Vector3f,
    pub view: Matrixf,
    pub proj: Matrixf,
    pub aspect: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct VisState {
    pub target: f32,
    pub vis: f32,
    pub vel: f32,
    pub frame: i32,
}

fn max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

fn min(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

fn col_mul(c: Color, s: f32) -> Color {
    Color {
        r: c.r * s,
        g: c.g * s,
        b: c.b * s,
        a: c.a * s,
    }
}

fn col_mul_col(c: Color, o: Color) -> Color {
    Color {
        r: c.r * o.r,
        g: c.g * o.g,
        b: c.b * o.b,
        a: c.a * o.a,
    }
}

fn len2(v: Vector2f) -> f32 {
    fma(v.x, v.x, v.y * v.y).sqrt()
}

pub fn fade_in_rate(fade: f32, time: f32) -> f32 {
    (1.0 - fade) / (time + 1e-4)
}

pub fn fade_out_rate(fade: f32, time: f32) -> f32 {
    -fade / (time + 1e-4)
}

pub fn update_fade(fade: f32, rate: f32, dt: f32) -> f32 {
    clamp(fma(dt, rate, fade), 0.0, 1.0)
}

pub fn outer_cos(angle: f32, inner_cos: f32) -> f32 {
    min(cos(angle * 0.5), inner_cos)
}

/// Returns `(inner_cos, outer_cos)`.
pub fn set_inner_fov(angle: f32, outer_cos: f32) -> (f32, f32) {
    let inner = cos(angle * 0.5);
    (inner, min(outer_cos, inner))
}

pub fn clamp01(x: f32) -> f32 {
    clamp(x, 0.0, 1.0)
}

pub fn update_visibility(s: &mut VisState, dt: f32, frame: i32) {
    if frame == s.frame {
        return;
    }
    if s.frame < 0 {
        s.vis = s.target;
        s.vel = 0.0;
    } else {
        if frame.wrapping_sub(s.frame) > 1 {
            s.vis = 0.0;
            s.vel = 0.0;
        }
        let t = min(1.0, dt * 15.0);
        let diff = s.target - s.vis;
        if s.vel * diff <= 0.0 {
            s.vel = 0.0;
        }
        s.vel = fma(t, diff, s.vel);
        s.vis = clamp(fma(s.vel, t, s.vis), 0.0, 1.0);
    }
    s.frame = frame;
}

pub fn bv_radius(active: &[bool; 3], size: &[Vector2f; 3]) -> f32 {
    let mut r = 0.125f32;
    for (on, s) in active.iter().zip(size) {
        if *on {
            r = max(r, len2(*s));
        }
    }
    r
}

fn first_active(f: &FlareIn) -> Kind {
    let [anamorphic, halo, iris] = f.active;
    if halo {
        Kind::Halo
    } else if anamorphic {
        Kind::Anamorphic
    } else if iris {
        Kind::MultiIris
    } else {
        Kind::Halo
    }
}

fn scale_from_distance(f: &FlareIn, d: f32) -> f32 {
    max(0.0, 1.5 * fma(1.0 - f.size_by_dist, d - 1.0, 1.5))
}

fn dir_to_camera(e: &EntIn, v: &ViewIn) -> Vector3f {
    let d = sub3(v.origin, e.pos);
    let len = len3(d);
    if len > 1e-8 {
        v3(d.x / len, d.y / len, d.z / len)
    } else {
        d
    }
}

fn fov_factor(f: &FlareIn, e: &EntIn, v: &ViewIn) -> f32 {
    let neg = v3(-e.axis.x, -e.axis.y, -e.axis.z);
    let c = dot3(dir_to_camera(e, v), neg);
    (clamp(c, f.outer_cos - 1e-4, f.inner_cos) - f.outer_cos + 1e-4)
        / (f.inner_cos - f.outer_cos + 1e-4)
}

fn screen_edge(ndc: Vector3f) -> f32 {
    min(1.0 - ndc.x.abs(), 1.0 - ndc.y.abs())
}

fn to_ndc(e: &EntIn, v: &ViewIn) -> (Vector3f, Vector3f) {
    let view = matrix_mul_vec(&v.view, e.pos);
    (view, matrix_mul_divide_w(&v.proj, view))
}

pub fn color_and_brightness(f: &FlareIn, e: &EntIn, v: &ViewIn, kind: Kind, vis: f32) -> Color {
    if !sel(&f.active, kind) {
        return Color::default();
    }
    let mut fov = fov_factor(f, e, v);
    let (view, ndc) = to_ndc(e, v);
    let d = -view.z;
    let screen_size = scale_from_distance(f, d) * len2(sel(&f.size, first_active(f))) / (8.0 * d);
    let edge = screen_edge(ndc);
    if edge - 0.5 * screen_size < 0.0 {
        fov *= fma(0.5, screen_size, edge) / screen_size;
    }
    let dist = len3(view);
    if f.range_max.y > 0.0 {
        fov *= clamp(
            (f.range_max.y - dist) / (f.range_max.y - f.range_max.x),
            0.0,
            1.0,
        );
    }
    if f.range_min.y > 0.0 {
        fov *= clamp(
            (dist - f.range_min.x) / (f.range_min.y - f.range_min.x),
            0.0,
            1.0,
        );
    }
    let mut value = max(fov, 0.0);
    if kind == Kind::Anamorphic {
        value *= value;
    }
    let shown = if f.shrink_when_occluded {
        fma(0.5, vis, 0.5)
    } else {
        vis
    };
    col_mul(col_mul(sel(&f.color, kind), value * f.fade), shown)
}

/// Returns the glare colour and the screen position it is drawn at.
pub fn glare(f: &FlareIn, e: &EntIn, v: &ViewIn, scale: f32) -> (Color, Vector3f) {
    let to_cam = dir_to_camera(e, v);
    let axis = e.axis;
    let c = -dot3(to_cam, axis);
    let mut g = (max(c, f.glare_cos) - f.glare_cos) / (1.0 - f.glare_cos + 1e-4);
    let forward = v3(v.view.m[2][0], v.view.m[2][1], v.view.m[2][2]);
    let dv = dot3(to_cam, forward);
    if dv > 0.9125 {
        g = fma((dv - 0.9125) / 0.087_599_98, f.glare_stare_at, g);
    }
    g *= fma(0.33, dot3(to_cam, axis), 0.67);
    if f.glare_range.y > 0.0 {
        let d = sub3(e.pos, v.origin);
        let dist = fma(d.z, d.z, fma(d.x, d.x, d.y * d.y)).sqrt();
        g *= clamp(
            (f.glare_range.y - dist) / (f.glare_range.y - f.glare_range.x),
            0.0,
            1.0,
        );
    }
    let mut screen = v3(0.0, 0.0, 0.0);
    if g > 0.0 {
        let (_, ndc) = to_ndc(e, v);
        screen = v3(fma(ndc.x, 0.5, 0.5), fma(ndc.y, 0.5, 0.5), 0.0);
    }
    g *= f.glare_brightness;
    let col = color_and_brightness(f, e, v, first_active(f), 1.0);
    let out = Color {
        r: col.r * col.r * g,
        g: col.g * col.g * g,
        b: col.b * col.b * g,
        a: col.a,
    };
    (col_mul(out, scale), screen)
}

pub fn size_and_rotation(f: &FlareIn, e: &EntIn, v: &ViewIn, kind: Kind, vis: f32) -> Matrixf {
    if !sel(&f.active, kind) || kind == Kind::MultiIris {
        return IDENTITY;
    }
    let (view, ndc) = to_ndc(e, v);
    let d = -view.z;
    let shrink = if f.shrink_when_occluded { vis } else { 1.0 };
    let size = sel(&f.size, kind);
    let mut scale = v3(size.x * shrink, size.y * shrink, shrink);
    let screen_size = scale_from_distance(f, d) * len2(size) / (4.0 * d);
    let edge = screen_edge(ndc);
    if edge - 0.5 * screen_size < 0.0 {
        let k = (edge - 0.5 * screen_size + screen_size) / screen_size;
        scale = v3(scale.x * k, scale.y * k, scale.z * k);
    }
    if kind == Kind::Anamorphic {
        let mut vec = v3(size.x, size.y, 1.0);
        if size.x > size.y {
            vec.x = fma(2.0 * ndc.x, ndc.x, 1.0) * scale.x;
        }
        if size.y > vec.x {
            vec.y = fma(2.0 * ndc.y, ndc.y, 1.0) * scale.y;
        }
        matrix_scale(vec)
    } else {
        let rot = -(0.5 * atan2(0.4, ndc.x) - std::f32::consts::FRAC_PI_4);
        matrix_mul(&rotate_z(rot), &matrix_scale(scale))
    }
}

pub fn billboard_matrix(f: &FlareIn, e: &EntIn, v: &ViewIn) -> Matrixf {
    let to_cam = dir_to_camera(e, v);
    let cam_z = v3(v.view.m[2][0], v.view.m[2][1], v.view.m[2][2]);
    let up = v3(v.view.m[1][0], v.view.m[1][1], v.view.m[1][2]);
    let sign = if dot3(to_cam, cam_z) > 0.0 { 1.0 } else { -1.0 };
    let fwd = v3(cam_z.x * sign, cam_z.y * sign, cam_z.z * sign);
    let right = cross3(up, fwd);
    let c = -dot3(to_cam, e.axis);
    let fov = (clamp(c, f.outer_cos - 1e-4, f.inner_cos) - f.outer_cos + 1e-4) * 0.5
        / (f.inner_cos - f.outer_cos + 1e-4)
        + 0.5;
    let scale = 2.0 * scale_from_distance(f, dot3(cam_z, sub3(v.origin, e.pos))) * fov;
    let mut m = IDENTITY;
    let rows = [
        (right.x, up.x, fwd.x, e.pos.x),
        (right.y, up.y, fwd.y, e.pos.y),
        (right.z, up.z, fwd.z, e.pos.z),
    ];
    for (row, (r, u, w, p)) in m.m.iter_mut().zip(rows) {
        row.copy_from_slice(&[r * scale, u * scale, w * scale, p]);
    }
    m
}

/// `cLensFlareType::UpdateGraphicsForViewport` colour; `None` when the flare is fully occluded.
pub fn type_color(f: &FlareIn, e: &EntIn, v: &ViewIn, kind: Kind, vis: f32) -> Option<Color> {
    if vis <= 0.0 {
        return None;
    }
    let c = color_and_brightness(f, e, v, kind, 1.0);
    let b = f.brightness;
    let c = col_mul(
        Color {
            r: c.r * c.r * b,
            g: c.g * c.g * b,
            b: c.b * c.b * b,
            a: c.a,
        },
        fma(0.5, vis, 0.5),
    );
    Some(if kind == Kind::Anamorphic {
        col_mul(c, vis)
    } else {
        c
    })
}

/// Occlusion-queried visibility target from the sample counts and the clipped screen rectangle.
pub fn occlusion_target(vis: f32, total: f32, mut vmin: Vector3f, mut vmax: Vector3f) -> f32 {
    let raw_area = (vmax.x - vmin.x) * (vmax.y - vmin.y);
    vmin.x = max(vmin.x, -1.0);
    vmin.y = max(vmin.y, -1.0);
    vmax.x = min(vmax.x, 1.0);
    vmax.y = min(vmax.y, 1.0);
    let clamp_area = (vmax.x - vmin.x) * (vmax.y - vmin.y);
    if raw_area > 0.0 {
        clamp(
            (clamp_area / raw_area).abs() * vis / (total + 1e-4),
            0.0,
            1.0,
        )
    } else {
        0.0
    }
}

/// Culling box for `CollidesWithFrustum`: `None` if out of range, else the box scaled by distance.
pub fn collide_box(
    f: &FlareIn,
    cam: &ViewIn,
    world_center: Vector3f,
    local_min: Vector3f,
    local_max: Vector3f,
    local_center: Vector3f,
) -> Option<(Vector3f, Vector3f)> {
    let d = sub3(cam.origin, world_center);
    let d2 = fma(d.z, d.z, fma(d.x, d.x, d.y * d.y));
    if f.range_max.y > 0.0 && d2 > f.range_max.y * f.range_max.y {
        return None;
    }
    if f.range_min.y > 0.0 && f.range_min.x * f.range_min.x > d2 {
        return None;
    }
    let s = scale_from_distance(f, -matrix_mul_vec(&cam.view, world_center).z);
    let scaled = |p: Vector3f, c: Vector3f| {
        let o = sub3(p, c);
        v3(c.x + o.x * s, c.y + o.y * s, c.z + o.z * s)
    };
    Some((
        scaled(local_min, local_center),
        scaled(local_max, local_center),
    ))
}

struct MsvcRand(u32);

impl MsvcRand {
    fn next(&mut self) -> i32 {
        self.0 = self.0.wrapping_mul(214_013).wrapping_add(2_531_011);
        ((self.0 >> 16) & 0x7fff) as i32
    }

    fn float(&mut self, a: f32, b: f32) -> f32 {
        fma(self.next() as f32 * 3.051_851e-5, b - a, a)
    }

    fn long(&mut self, a: i32, b: i32) -> i32 {
        let r = self.next();
        let m = b.wrapping_sub(a).wrapping_add(1);
        a.wrapping_add(if m == 0 { r } else { r.wrapping_rem(m) })
    }
}

/// Fills `count` quads (4 vertices each) of positions (4 floats), colours (4) and uvs (3).
/// Returns the number of quads written.
pub fn multi_iris(
    f: &FlareIn,
    e: &EntIn,
    v: &ViewIn,
    vis: f32,
    pos: &mut [f32],
    col: &mut [f32],
    uv: &mut [f32],
) -> usize {
    let (_, ndc) = to_ndc(e, v);
    let sx = fma(ndc.x, 0.5, 0.5);
    let sy = 1.0 - fma(ndc.y, 0.5, 0.5);
    let z = fma(ndc.z, 0.5, 0.5);

    let base = color_and_brightness(f, e, v, Kind::MultiIris, 1.0);
    let b = f.brightness;
    let mut c = col_mul(
        Color {
            r: base.r * base.r * b,
            g: base.g * base.g * b,
            b: base.b * base.b * b,
            a: base.a,
        },
        vis,
    );
    if f.mul_iris_with_glare {
        let (g, _) = glare(f, e, v, 1.0);
        c = col_mul_col(c, col_mul(g, 2.0));
    }

    let mut rng = MsvcRand(f.multi_iris_seed as u32);
    rng.next();
    let ry = rng.float(0.495, 0.505);
    let rx = rng.float(0.495, 0.505);
    let dir = Vector2f {
        x: rx - sx,
        y: ry - sy,
    };
    let l = len2(dir);
    let size_scale = fma(vis, 0.25, 0.25);
    let iris = sel(&f.size, Kind::MultiIris);
    let half_base = Vector2f {
        x: iris.x * size_scale,
        y: iris.y * v.aspect * size_scale,
    };
    let denom = fma(3.0, l, 1.0);
    let (gx, gy) = (f.atlas_grid.x, f.atlas_grid.y);
    let duv = Vector2f {
        x: 1.0 / gx as f32,
        y: 1.0 / gy as f32,
    };
    const CORNERS: [(f32, f32); 4] = [(-1.0, -1.0), (1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)];

    let quads = pos
        .chunks_exact_mut(16)
        .zip(col.chunks_exact_mut(16))
        .zip(uv.chunks_exact_mut(12))
        .take(usize::try_from(f.multi_iris_count).unwrap_or(0));
    let mut written = 0;
    for ((qp, qc), qu) in quads {
        let r = rng.float(-0.5, 2.25);
        let s1 = rng.float(0.33, 1.0);
        let s2 = rng.float(0.33, 1.0);
        let rot = rng.float(0.0, TWO_PI);
        let amp = rng.float(0.5, 1.0);
        let iy = rng.long(0, gy);
        let ix = rng.long(0, gx);

        let cx = fma(dir.x, r, sx);
        let cy = fma(dir.y, r, sy);
        let k = s1 * s2 / denom * ((0.25 * r).abs() + 0.5);
        let (hx, hy) = (half_base.x * k, half_base.y * k);
        let m = sin(fma(l * r, PI, rot)) * amp;
        let ci = col_mul(c, m * m);
        let (u0, v0) = (ix as f32 * duv.x, iy as f32 * duv.y);

        let verts = qp
            .chunks_exact_mut(4)
            .zip(qc.chunks_exact_mut(4))
            .zip(qu.chunks_exact_mut(3))
            .zip(CORNERS);
        for (((p, cc), t), (kx, ky)) in verts {
            p.copy_from_slice(&[fma(kx, hx, cx), fma(ky, hy, cy), z, 1.0]);
            cc.copy_from_slice(&[ci.r, ci.g, ci.b, ci.a]);
            t.copy_from_slice(&[
                u0 + if kx > 0.0 { duv.x } else { 0.0 },
                v0 + if ky > 0.0 { duv.y } else { 0.0 },
                0.0,
            ]);
        }
        written += 1;
    }
    written
}
