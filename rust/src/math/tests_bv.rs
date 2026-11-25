//! Replay of `tests/golden/BV/dump.txt`, recorded from the C++ `BvFrustumTests` harness
//! (`HPL2/tests/BvFrustumTests.cpp`) before the port. Same generator, same call order.

#![allow(clippy::needless_range_loop)]

use crate::math::{sub3, translation};
use std::fmt::Write;

use super::bv::{Bv, Frustum};
use super::geom::{INSIDE, INTERSECT, OUTSIDE};
use super::matrix::{
    IDENTITY, matrix_inverse, matrix_mul, matrix_rotate, matrix_scale, orthographic_projection,
    perspective_projection,
};
use super::{Matrixf, Planef, Vector2f, Vector3f, fma, v3};

const GOLDEN: &str = include_str!("../../tests/golden/BV/dump.txt");

struct Rng(u32);

impl Rng {
    fn rnd(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        self.0 >> 8
    }
    fn i(&mut self, n: u32) -> i32 {
        (self.rnd() % n) as i32
    }
    fn f(&mut self, lo: f32, hi: f32) -> f32 {
        lo + (hi - lo) * (self.rnd() & 0xFFFF) as f32 / 65535.0f32
    }
    fn v3(&mut self, lo: f32, hi: f32) -> Vector3f {
        let x = self.f(lo, hi);
        let y = self.f(lo, hi);
        let z = self.f(lo, hi);
        v3(x, y, z)
    }
    fn vec(&mut self, r: f32) -> Vector3f {
        self.v3(-r, r)
    }
    fn matrix(&mut self, scale: bool) -> Matrixf {
        let rot = self.v3(-3.0, 3.0);
        let order = self.i(6);
        let mut m = matrix_rotate(rot, order);
        if scale {
            let s = self.v3(0.5, 2.0);
            m = matrix_mul(&m, &matrix_scale(s));
        }
        let t = self.vec(100.0);
        m.m[0][3] = t.x;
        m.m[1][3] = t.y;
        m.m[2][3] = t.z;
        m
    }
}

fn add(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

struct Out(String);

impl Out {
    fn f(&mut self, f: f32) {
        let _ = write!(self.0, " {:08x}", f.to_bits());
    }
    fn v(&mut self, v: Vector3f) {
        self.f(v.x);
        self.f(v.y);
        self.f(v.z);
    }
    fn m(&mut self, m: &Matrixf) {
        for r in 0..4 {
            for c in 0..4 {
                self.f(m.m[r][c]);
            }
        }
    }
    fn p(&mut self, p: &Planef) {
        self.f(p.a);
        self.f(p.b);
        self.f(p.c);
        self.f(p.d);
    }
    fn n(&mut self, n: i32) {
        let _ = write!(self.0, " {n}");
    }
    fn end(&mut self) {
        self.0.push('\n');
    }
    fn bv(&mut self, b: &Bv) {
        self.m(&b.mtx);
        for v in [
            b.local_max,
            b.local_min,
            b.max,
            b.min,
            b.pivot,
            b.world_max,
            b.world_min,
            b.position,
            b.size,
        ] {
            self.v(v);
        }
        self.f(b.radius);
    }
}

fn run_bv(seed: i32, rng: &mut Rng, o: &mut Out) {
    let mut b = Bv::new();
    let mut arr = [[0.0f32; 40]; 2];
    for step in 0..40 {
        let op = if step == 0 { 0 } else { rng.i(11) };
        let _ = write!(o.0, "b {seed} {step} {op}");
        match op {
            0 => {
                let lo = rng.vec(20.0);
                let hi = add(lo, rng.v3(0.0, 30.0));
                b.set_local_min_max(lo, hi);
                b.update_size();
                o.v(b.world_max);
                o.v(b.world_min);
            }
            1 => {
                let s = rng.v3(0.1, 20.0);
                b.set_size(s);
            }
            2 => {
                let p = rng.vec(100.0);
                b.set_position(p);
            }
            3 => {
                let sc = rng.i(2) == 0;
                let m = rng.matrix(sc);
                b.set_transform(&m);
            }
            4 => {
                b.update_size();
                o.v(b.world_max);
                o.v(b.world_min);
            }
            5 => {
                o.v(b.world_center());
                o.f(b.radius());
            }
            6 => {
                b.update_size();
                o.v(b.size);
                o.v(b.local_center());
                o.v(b.local_max);
                o.v(b.local_min);
            }
            7 => {
                o.v(translation(&b.mtx));
                o.m(&b.mtx);
            }
            8 | 9 => {
                let stride = 3 + rng.i(2);
                let arrays = 1 + rng.i(2);
                for a in 0..arrays as usize {
                    let n = 1 + rng.i(8);
                    for i in 0..(n * stride) as usize {
                        arr[a][i] = rng.f(-30.0, 30.0);
                    }
                    // SAFETY: `arr` outlives the matching create_from_points below.
                    unsafe { b.add_array_points(arr[a].as_ptr(), n) };
                }
                if op == 8 {
                    let mut c = b.clone();
                    // SAFETY: pending arrays are `n` vectors of `stride` floats inside `arr`.
                    unsafe {
                        b.create_from_points(stride);
                        c.create_from_points(stride);
                    }
                    o.bv(&c);
                } else {
                    // SAFETY: as above.
                    unsafe { b.create_from_points(stride) };
                }
            }
            _ => {
                let mut c = b.clone();
                let p = rng.vec(10.0);
                c.set_position(p);
                c.update_size();
                o.v(c.world_max);
                o.f(c.radius());
                b.update_size();
                o.v(b.world_max);
            }
        }
        o.bv(&b);
        o.end();
    }
}

fn make_frustum(rng: &mut Rng) -> Box<Frustum> {
    // SAFETY: Frustum is plain data; every field is set by init or the setup call.
    let mut f: Box<Frustum> = unsafe { Box::new(std::mem::zeroed()) };
    f.origin_bv.bv = Bv::new();
    f.bounding_volume.bv = Bv::new();
    f.init();
    let cam = rng.matrix(false);
    let view = matrix_inverse(&cam);
    let origin = translation(&cam);
    let near = rng.f(0.05, 0.5);
    let far = rng.f(20.0, 200.0);
    if rng.i(4) != 0 {
        let fov = rng.f(0.5, 2.0);
        let aspect = rng.f(0.7, 2.0);
        let inf = rng.i(5) == 0;
        let oblique = rng.i(4) == 0;
        let custom = rng.i(4) == 0;
        let proj = perspective_projection(near, far, fov, aspect, inf);
        let custom_proj = perspective_projection(near, far * 0.5, fov, aspect, false);
        f.setup_perspective(
            &proj,
            &view,
            far,
            near,
            fov,
            aspect,
            origin,
            inf,
            custom.then_some(&custom_proj),
            oblique,
        );
        f.inverts_cull = rng.i(2) == 0;
    } else {
        let x = rng.f(5.0, 40.0);
        let y = rng.f(5.0, 40.0);
        let vs = Vector2f { x, y };
        let proj = orthographic_projection(near, far, vs);
        let inf = rng.i(5) == 0;
        f.setup_ortho(&proj, &view, far, near, vs, origin, inf);
    }
    f
}

fn bv_at(mn: Vector3f, mx: Vector3f, c: Vector3f) -> Bv {
    let mut b = Bv::new();
    b.set_local_min_max(sub3(mn, c), sub3(mx, c));
    b.set_position(c);
    b
}

fn run_frustum(seed: i32, rng: &mut Rng, o: &mut Out) {
    let mut fs = [make_frustum(rng), make_frustum(rng), make_frustum(rng)];
    for (i, f) in fs.iter_mut().enumerate() {
        let _ = write!(o.0, "f {seed} {i}");
        o.m(&f.proj);
        o.m(&f.view);
        o.f(f.far_plane);
        o.f(f.near_plane);
        o.n(f.inf_far as i32);
        o.n(f.inverts_cull as i32);
        o.n(f.projection as i32);
        if f.projection == 0 {
            o.f(f.fov);
            o.f(f.aspect);
        } else {
            o.f(f.view_size.x);
            o.f(f.view_size.y);
        }
        for p in &f.plane {
            o.p(p);
        }
        for v in &f.vertices {
            o.v(*v);
        }
        o.v(f.sphere.center);
        o.f(f.sphere.r);
        let (mn, mx, r) = {
            let b = &mut f.bounding_volume.bv;
            (b.world_min_max().0, b.world_min_max().1, b.radius())
        };
        o.v(mn);
        o.v(mx);
        o.f(r);
        o.v(f.origin);
        o.v(v3(f.view.m[2][0], f.view.m[2][1], f.view.m[2][2]));
        let b = &mut f.origin_bv.bv;
        let (mn, mx) = b.world_min_max();
        o.v(mn);
        o.v(mx);
        o.f(b.radius());
        o.end();
    }
    for q in 0..40 {
        let i = rng.i(3) as usize;
        let s = fs[i].sphere;
        let op = rng.i(15);
        let mode = rng.i(3);
        let c = if mode == 0 {
            rng.vec(80.0)
        } else {
            let nv = if (4..=7).contains(&op) { 4 } else { 8 };
            let mut w = [0.0f32; 8];
            let mut sum = 0.0f32;
            for x in w.iter_mut().take(nv) {
                *x = rng.f(0.01, 1.0);
                sum += *x;
            }
            let mut c = v3(0.0, 0.0, 0.0);
            for k in 0..nv {
                let t = w[k] / sum;
                let v = fs[i].vertices[k];
                c = v3(fma(v.x, t, c.x), fma(v.y, t, c.y), fma(v.z, t, c.z));
            }
            let j = rng.vec(if mode == 1 { 0.3 } else { 3.0 });
            add(c, j)
        };
        let r = rng.f(0.05, 8.0);
        let mn = c;
        let mx = add(c, rng.v3(0.1, 12.0));
        let _ = write!(o.0, "q {seed} {q} {i} {op}");
        let next = |rng: &mut Rng| ((i as i32 + 1 + rng.i(2)) % 3) as usize;
        let planes = [6, 4, 2];
        fs[i].sync();
        match op {
            0 => o.n(fs[i].collide_point(c) as i32),
            1 => {
                let mut b = bv_at(mn, mx, c);
                if rng.i(2) != 0 {
                    let m = rng.matrix(false);
                    b.set_transform(&m);
                }
                o.n(fs[i].collide_bv(&mut b));
            }
            2 => o.n(fs[i].collide_node(c, r, mn, mx)),
            3 => {
                let j = next(rng);
                fs[j].sync();
                o.n(fs[i].collide_frustum(&fs[j]));
            }
            4 => o.n(fs[i].check_sphere_near_plane(c, r) as i32),
            5 => o.n(fs[i].check_aabb_near_plane(mn, mx) as i32),
            6 => {
                let mut b = bv_at(mn, mx, c);
                o.n(fs[i].check_bv_near_plane(&mut b) as i32);
            }
            7 => {
                let j = next(rng);
                o.n(fs[i].check_frustum_near_plane(&fs[j]) as i32);
            }
            8 | 12 | 13 => {
                let (a, b2, mut out) = match op {
                    8 => {
                        let b2 = add(c, rng.vec(60.0));
                        (c, b2, rng.vec(1.0))
                    }
                    12 => {
                        let b2 = add(s.center, rng.vec(150.0));
                        (s.center, b2, v3(0.0, 0.0, 0.0))
                    }
                    _ => {
                        let a = add(s.center, rng.vec(30.0));
                        let b2 = add(s.center, rng.vec(30.0));
                        (a, b2, v3(0.0, 0.0, 0.0))
                    }
                };
                let hit = fs[i].check_line(a, b2, &mut out);
                o.n(hit as i32);
                o.v(out);
            }
            9 => o.n(fs[i].collide_frustum_sphere(c, r)),
            10 => {
                let mp = planes[rng.i(3) as usize];
                o.n(fs[i].collide_sphere(c, r, mp));
            }
            11 => {
                let _ = planes[rng.i(3) as usize];
                o.n(fs[i].collide_aabb(mn, mx));
            }
            _ => {
                let j = rng.vec(s.r * 0.5);
                o.n(fs[i].collide_point(add(s.center, j)) as i32);
            }
        }
        o.end();
    }
}

#[test]
fn replay_golden() {
    let mut o = Out(String::new());
    for seed in 0..24 {
        let mut rng = Rng((seed as u32).wrapping_mul(7919).wrapping_add(1));
        run_bv(seed, &mut rng, &mut o);
        run_frustum(seed, &mut rng, &mut o);
    }
    let (got, want): (Vec<&str>, Vec<&str>) = (o.0.lines().collect(), GOLDEN.lines().collect());
    assert_eq!(got.len(), want.len(), "line count");
    for (n, (g, w)) in got.iter().zip(&want).enumerate() {
        let canon = |t: &str| match u32::from_str_radix(t, 16) {
            Ok(v) if t.len() == 8 && v & 0x7fff_ffff > 0x7f80_0000 => "nan".to_string(),
            _ => t.to_string(),
        };
        let tok = g
            .split(' ')
            .zip(w.split(' '))
            .position(|(a, b)| canon(a) != canon(b));
        if tok.is_some() || g.split(' ').count() != w.split(' ').count() {
            panic!("line {} token {:?}\n got {g}\nwant {w}", n + 1, tok);
        }
    }
    let _ = (IDENTITY, INSIDE, INTERSECT, OUTSIDE);
}
