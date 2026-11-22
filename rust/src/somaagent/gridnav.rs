//! Player-size grid A* over physics probes (`GridNavPath`, headless `nav_path`).
#![allow(clippy::indexing_slicing)] // node indices come from the arena built here

use super::dist3;
use crate::math::{Vector3f, fma, v3};
use crate::string::float_to_string;
use std::cmp::{Ordering, Reverse};
use std::collections::{BinaryHeap, HashMap};
use std::ffi::c_void;

const CELL: f32 = 0.4;
const STEP: f32 = 0.45;
const DROP: f32 = 1.5;

pub trait Probe {
    /// Distance of the first walkable hit along the ray, 1e9 when none.
    fn floor_dist(&mut self, from: Vector3f, to: Vector3f) -> f32;
    fn slab_hit(&mut self, at: Vector3f) -> bool;
    fn clear(&mut self, crouch: bool, at: Vector3f) -> bool;
}

struct Node {
    p: Vector3f,
    ix: i32,
    iz: i32,
    g: f32,
    parent: Option<usize>,
    crouch: bool,
}

#[derive(PartialEq)]
struct Key(f32, usize);

impl Eq for Key {}
impl PartialOrd for Key {
    fn partial_cmp(&self, o: &Self) -> Option<Ordering> {
        Some(self.cmp(o))
    }
}
impl Ord for Key {
    fn cmp(&self, o: &Self) -> Ordering {
        self.0.total_cmp(&o.0).then(self.1.cmp(&o.1))
    }
}

fn vector2_dist(ax: f32, ay: f32, bx: f32, by: f32) -> f32 {
    let (dx, dy) = (ax - bx, ay - by);
    fma(dx, dx, dy * dy).sqrt()
}

fn floor_at<P: Probe>(p: &mut P, x: f32, y: f32, z: f32) -> (bool, f32) {
    // rays exactly on a terrain patch edge miss both heightfields
    let x = x + 0.01;
    let z = z + 0.01;
    let top = y + STEP + 0.15;
    let d = p.floor_dist(v3(x, top, z), v3(x, y - DROP - 0.1, z));
    let fy = top - d;
    // rays fall between grating bars
    if fy < y - 0.05 && p.slab_hit(v3(x, y - 0.04, z)) {
        return (true, y);
    }
    (d < 1e8, fy)
}

fn seen_key(ix: i32, iz: i32, y: f32) -> (i32, i32, i32) {
    (ix, iz, (y / 0.5 + 0.5).floor() as i32)
}

/// `GridNavPath`: "x y z[ c]" corner lines, "partial" first line when the goal was not reached.
pub fn grid_nav_path<P: Probe>(
    p: &mut P,
    start: Vector3f,
    goal: Vector3f,
    max_expand: i32,
) -> Vec<u8> {
    let f20 = |x: f32| float_to_string(x, 20, false);
    let mut goal_p = goal;
    let mut d = 0.0f32;
    while d < 6.0 {
        let (ok, fy) = floor_at(p, goal_p.x, goal.y - d, goal_p.z);
        if ok {
            goal_p.y = fy;
            break;
        }
        d += DROP;
    }
    let sx = (start.x / CELL).round() as i32;
    let sz = (start.z / CELL).round() as i32;
    let mut vs = v3(sx as f32 * CELL, start.y, sz as f32 * CELL);
    let (ok, fy) = floor_at(p, vs.x, vs.y, vs.z);
    if ok {
        vs.y = fy;
    }
    let mut nodes = vec![Node {
        p: vs,
        ix: sx,
        iz: sz,
        g: 0.0,
        parent: None,
        crouch: false,
    }];
    let mut seen: HashMap<(i32, i32, i32), i32> = HashMap::new();
    seen.insert(seen_key(sx, sz, vs.y), 0);
    let mut open = BinaryHeap::new();
    open.push(Reverse(Key(dist3(vs, goal_p), 0)));
    let (mut best, mut found, mut best_h) = (0usize, None, 1e9f32);
    let mut expand = 0;
    while expand < max_expand {
        let Some(Reverse(Key(_, i))) = open.pop() else {
            break;
        };
        expand += 1;
        let (v, ix, iz) = (nodes[i].p, nodes[i].ix, nodes[i].iz);
        let h = dist3(v, goal_p);
        if h < best_h {
            best_h = h;
            best = i;
        }
        if vector2_dist(v.x, v.z, goal_p.x, goal_p.z) < CELL * 1.5 && (v.y - goal_p.y).abs() < 1.0 {
            found = Some(i);
            break;
        }
        for dx in -1..=1 {
            for dz in -1..=1 {
                if dx == 0 && dz == 0 {
                    continue;
                }
                let mut n = v3((ix + dx) as f32 * CELL, v.y, (iz + dz) as f32 * CELL);
                let (ok1, mid_y) = floor_at(p, (n.x + v.x) * 0.5, v.y, (n.z + v.z) * 0.5);
                if !ok1 {
                    continue;
                }
                let (ok2, fy) = floor_at(p, n.x, mid_y, n.z);
                if !ok2 || mid_y - v.y > STEP || fy - mid_y > STEP || v.y - fy > DROP {
                    continue;
                }
                n.y = fy;
                let k = seen_key(ix + dx, iz + dz, n.y);
                if seen.contains_key(&k) {
                    continue;
                }
                seen.insert(k, -1);
                let mid = v3(
                    (n.x + v.x) * 0.5,
                    mid_y.max(n.y.max(v.y)),
                    (n.z + v.z) * 0.5,
                );
                if !p.clear(true, n) || !p.clear(true, mid) {
                    continue;
                }
                let crouch = !p.clear(false, n);
                let g = nodes[i].g + dist3(v, n) * if crouch { 2.0 } else { 1.0 };
                seen.insert(k, nodes.len() as i32);
                nodes.push(Node {
                    p: n,
                    ix: ix + dx,
                    iz: iz + dz,
                    g,
                    parent: Some(i),
                    crouch,
                });
                open.push(Reverse(Key(g + dist3(n, goal_p), nodes.len() - 1)));
            }
        }
    }
    let mut chain = Vec::new();
    let mut cur = Some(found.unwrap_or(best));
    while let Some(i) = cur {
        chain.push(i);
        cur = nodes[i].parent;
    }
    chain.reverse();
    let mut out = if found.is_some() {
        Vec::new()
    } else {
        b"partial\n".to_vec()
    };
    for k in 1..chain.len() {
        let (a, b) = (&nodes[chain[k - 1]], &nodes[chain[k]]);
        let last = k + 1 == chain.len();
        let mut corner =
            last || (b.p.y - a.p.y).abs() > 0.05 || b.crouch != nodes[chain[k + 1]].crouch;
        if !corner {
            let c = &nodes[chain[k + 1]];
            corner = ((b.p.x - a.p.x) - (c.p.x - b.p.x)).abs() > 0.01
                || ((b.p.z - a.p.z) - (c.p.z - b.p.z)).abs() > 0.01;
        }
        if corner {
            for (j, f) in [b.p.x, b.p.y, b.p.z].into_iter().enumerate() {
                if j > 0 {
                    out.push(b' ');
                }
                out.extend(f20(f));
            }
            out.extend(if b.crouch { &b" c\n"[..] } else { &b"\n"[..] });
        }
    }
    out
}

#[repr(C)]
pub struct OhplSoGridHost {
    pub ctx: *mut c_void,
    pub floor_dist: unsafe extern "C" fn(*mut c_void, *const f32, *const f32) -> f32,
    pub slab_hit: unsafe extern "C" fn(*mut c_void, *const f32) -> bool,
    pub clear: unsafe extern "C" fn(*mut c_void, bool, *const f32) -> bool,
}

struct CProbe<'a>(&'a OhplSoGridHost);

impl Probe for CProbe<'_> {
    fn floor_dist(&mut self, from: Vector3f, to: Vector3f) -> f32 {
        // SAFETY: host callbacks contract (read 3 floats each).
        unsafe {
            (self.0.floor_dist)(
                self.0.ctx,
                [from.x, from.y, from.z].as_ptr(),
                [to.x, to.y, to.z].as_ptr(),
            )
        }
    }
    fn slab_hit(&mut self, at: Vector3f) -> bool {
        // SAFETY: as above.
        unsafe { (self.0.slab_hit)(self.0.ctx, [at.x, at.y, at.z].as_ptr()) }
    }
    fn clear(&mut self, crouch: bool, at: Vector3f) -> bool {
        // SAFETY: as above.
        unsafe { (self.0.clear)(self.0.ctx, crouch, [at.x, at.y, at.z].as_ptr()) }
    }
}

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

/// # Safety
/// `host` valid with working callbacks; `from`/`to` valid for 3 floats; sink accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_grid_nav_path(
    host: *const OhplSoGridHost,
    from: *const f32,
    to: *const f32,
    max_expand: i32,
    ctx: *mut c_void,
    sink: Sink,
) {
    // SAFETY: caller contract.
    let (host, a, b) = unsafe {
        (
            &*host,
            std::slice::from_raw_parts(from, 3),
            std::slice::from_raw_parts(to, 3),
        )
    };
    let out = grid_nav_path(
        &mut CProbe(host),
        v3(a[0], a[1], a[2]),
        v3(b[0], b[1], b[2]),
        max_expand,
    );
    // SAFETY: sink contract.
    unsafe { sink(ctx, out.as_ptr(), out.len()) };
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Flat;
    impl Probe for Flat {
        fn floor_dist(&mut self, from: Vector3f, to: Vector3f) -> f32 {
            if to.y <= 0.0 && from.y >= 0.0 {
                from.y
            } else {
                1e9
            }
        }
        fn slab_hit(&mut self, _: Vector3f) -> bool {
            false
        }
        fn clear(&mut self, _: bool, at: Vector3f) -> bool {
            !(at.x > 1.0 && at.x < 1.4 && at.z.abs() < 3.0)
        }
    }

    #[test]
    fn flat_floor_routes_around_wall() {
        let s = String::from_utf8(grid_nav_path(
            &mut Flat,
            v3(0.0, 0.0, 0.0),
            v3(3.0, 0.0, 0.0),
            20000,
        ))
        .unwrap();
        assert!(!s.starts_with("partial"), "{s}");
        assert!(s.lines().count() >= 2);
        assert!(s.lines().last().unwrap().starts_with("2."), "{s}");
    }

    #[test]
    fn unreachable_goal_is_partial() {
        let mut p = Flat;
        let s = grid_nav_path(&mut p, v3(0.0, 0.0, 0.0), v3(3.0, 0.0, 0.0), 3);
        assert!(s.starts_with(b"partial\n"));
    }
}
