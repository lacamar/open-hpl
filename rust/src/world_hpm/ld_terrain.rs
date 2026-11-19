//! SOMA terrain geometry and undergrowth scatter (`cHpmWorldBuilder::Terrain`, `CreateTerrainUndergrowth`).
//! Float expressions follow gcc's FMA contraction of the C++ they replace.

#![expect(
    clippy::indexing_slicing,
    clippy::integer_division_remainder_used,
    reason = "grid indices are clamped to the heightmap and cell sizes"
)]

use crate::math::{
    cross3, fast_random_float, matrix_mul, matrix_mul_3x3, matrix_unit_vectors, rand_rectf,
    rotate_y,
};
use crate::types::Vector3f;
use std::collections::BTreeMap;

const KPIF: f32 = std::f32::consts::PI;

fn fma(a: f32, b: f32, c: f32) -> f32 {
    a.mul_add(b, c)
}

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

fn normalize3(v: Vector3f) -> Vector3f {
    let length = fma(v.z, v.z, fma(v.x, v.x, v.y * v.y)).sqrt();
    if f64::from(length) > 1e-08 {
        let inv = 1.0f32 / length;
        v3(v.x * inv, v.y * inv, v.z * inv)
    } else {
        v
    }
}

fn clamp_i(v: i32, lo: i32, hi: i32) -> i32 {
    v.max(lo).min(hi)
}

fn clamp_f(v: f32, lo: f32, hi: f32) -> f32 {
    if v < lo {
        lo
    } else if v > hi {
        hi
    } else {
        v
    }
}

#[derive(Default)]
struct Hole {
    data: Vec<u8>,
    scale: i32,
    channel: i32,
}

#[derive(Default)]
pub struct Patch {
    pos: Vec<f32>,
    normal: Vec<f32>,
    uv0: Vec<f32>,
    blend_uv: Vec<f32>,
    idx: Vec<u32>,
}

pub struct Terrain {
    size: i32,
    unit: f32,
    height: Vec<f32>,
    elev: Vec<u16>,
    hole: Hole,
    patch: Patch,
}

impl Terrain {
    /// `data` is the whole heightmap .dds; `None` when it is too short.
    pub fn new(data: &[u8], size: i32, unit: f32, max_height: f32) -> Option<Self> {
        let n = usize::try_from(size).ok().filter(|n| *n > 0)?;
        if data.len() < 128 + n * n * 3 {
            return None;
        }
        let mut height = vec![0.0f32; n * n];
        let mut elev = vec![0u16; (n + 1) * (n + 1)];
        for (i, h) in height.iter_mut().enumerate() {
            let p = data.get(128 + i * 3..128 + i * 3 + 3)?;
            let val = u32::from(p[0]) | (u32::from(p[1]) << 8) | (u32::from(p[2]) << 16);
            *h = if val != 0 {
                val as f32 * (max_height / 16777215.0f32)
            } else {
                f32::NAN
            };
            elev[i / n * (n + 1) + i % n] =
                (val as f32 / 16777215.0f32 * 65535.0f32).round() as u16;
        }
        for i in 0..n {
            elev[i * (n + 1) + n] = elev[i * (n + 1) + n - 1];
        }
        let (head, tail) = elev.split_at_mut(n * (n + 1));
        tail[..n + 1].copy_from_slice(&head[(n - 1) * (n + 1)..(n - 1) * (n + 1) + n + 1]);
        Some(Self {
            size,
            unit,
            height,
            elev,
            hole: Hole::default(),
            patch: Patch::default(),
        })
    }

    /// Accepts a raw 32-bit blend layer `file` as the hole mask.
    pub fn set_hole(&mut self, file: Vec<u8>, channel: i32) -> bool {
        if file.len() < 128 {
            return false;
        }
        let w = i32::from(file[16]) | (i32::from(file[17]) << 8) | (i32::from(file[18]) << 16);
        let raw32 = file[84] == 0 && file[88] == 32 && file[92] == 0xFF;
        if !raw32 || w < self.size || file.len() < 128 + (w as usize) * (w as usize) * 4 {
            return false;
        }
        self.hole = Hole {
            scale: w / self.size,
            channel,
            data: file,
        };
        true
    }

    pub fn has_hole(&self) -> bool {
        !self.hole.data.is_empty()
    }

    fn height_at(&self, x: i32, z: i32) -> f32 {
        let s = self.size;
        self.height[clamp_i(z, 0, s - 1) as usize * s as usize + clamp_i(x, 0, s - 1) as usize]
    }

    fn solid(&self, x: i32, z: i32, fallback: f32) -> f32 {
        let h = self.height_at(x, z);
        if h.is_nan() { fallback } else { h }
    }

    fn is_hole(&self, x: i32, z: i32) -> bool {
        let hole = &self.hole;
        if hole.data.is_empty() {
            return false;
        }
        let w = (hole.scale * self.size) as usize;
        let mut sum = 0i32;
        for tz in z * hole.scale..(z + 1) * hole.scale {
            for tx in x * hole.scale..(x + 1) * hole.scale {
                sum += i32::from(
                    hole.data[128 + (tz as usize * w + tx as usize) * 4 + hole.channel as usize],
                );
            }
        }
        sum * 2 > 255 * hole.scale * hole.scale
    }

    /// Builds the patch whose lower corner is (`x0`, `z0`) with `w` x `h` vertices.
    pub fn build_patch(&mut self, x0: i32, z0: i32, w: i32, h: i32, tile: f32) -> &Patch {
        let size = self.size;
        let unit = self.unit;
        let offset = size as f32 * unit * 0.5f32;
        let mut p = Patch::default();
        for z in z0..z0 + h {
            for x in x0..x0 + w {
                let y = self.solid(x, z, 0.0);
                let pos = v3(
                    fma(x as f32, unit, -offset),
                    y,
                    fma(z as f32, unit, -offset),
                );
                let n = normalize3(v3(
                    self.solid(x - 1, z, y) - self.solid(x + 1, z, y),
                    2.0 * unit,
                    self.solid(x, z - 1, y) - self.solid(x, z + 1, y),
                ));
                p.pos.extend([pos.x, pos.y, pos.z]);
                p.normal.extend([n.x, n.y, n.z]);
                p.uv0.extend([pos.x * tile, pos.z * tile, 0.0 * tile]);
                let s = size as f32;
                p.blend_uv.extend([x as f32 / s, z as f32 / s, 0.0 / s]);
            }
        }
        for z in 0..h - 1 {
            for x in 0..w - 1 {
                let (gx, gz) = (x0 + x, z0 + z);
                if self.height_at(gx, gz).is_nan()
                    || self.height_at(gx + 1, gz).is_nan()
                    || self.height_at(gx, gz + 1).is_nan()
                    || self.height_at(gx + 1, gz + 1).is_nan()
                    || self.is_hole(gx, gz)
                {
                    continue;
                }
                let i = (z * w + x) as u32;
                let w = w as u32;
                p.idx.extend([i, i + 1, i + w, i + 1, i + w + 1, i + w]);
            }
        }
        self.patch = p;
        &self.patch
    }

    /// Newton heightfields have no holes: sink vertices inside one.
    pub fn sink_holes(&mut self) {
        if !self.has_hole() {
            return;
        }
        let s = self.size;
        for z in 1..s {
            for x in 1..s {
                if self.is_hole(x - 1, z - 1)
                    && self.is_hole(x, z - 1)
                    && self.is_hole(x - 1, z)
                    && self.is_hole(x, z)
                {
                    self.elev[z as usize * (s as usize + 1) + x as usize] = 0;
                }
            }
        }
    }

    pub fn elev(&self) -> &[u16] {
        &self.elev
    }

    pub fn heights(&self) -> &[f32] {
        &self.height
    }
}

impl Patch {
    pub fn out(&self) -> LdPatchOut {
        LdPatchOut {
            num_verts: (self.pos.len() / 3) as u32,
            pos: self.pos.as_ptr(),
            normal: self.normal.as_ptr(),
            uv0: self.uv0.as_ptr(),
            blend_uv: self.blend_uv.as_ptr(),
            num_idx: self.idx.len() as u32,
            idx: self.idx.as_ptr(),
        }
    }
}

#[repr(C)]
pub struct LdPatchOut {
    pub num_verts: u32,
    pub pos: *const f32,
    pub normal: *const f32,
    pub uv0: *const f32,
    pub blend_uv: *const f32,
    pub num_idx: u32,
    pub idx: *const u32,
}

pub struct Mat {
    pub pos: Vec<Vector3f>,
    pub uv: Vec<Vector3f>,
    pub idx: Vec<u32>,
    pub sub_div: [f32; 2],
    pub min_color: [f32; 4],
    pub max_color: [f32; 4],
    pub density: f32,
    pub align: f32,
    pub base_size: [f32; 3],
    pub min_size_mul: [f32; 3],
    pub max_size_mul: [f32; 3],
}

pub struct AreaIn {
    pub mat: i32,
    pub sub: bool,
    pub max_influence: f32,
    pub fade_border: f32,
    pub circle: bool,
    pub center: [f32; 2],
    pub radius: f32,
    pub points: Vec<f32>,
}

struct Area {
    mat: i32,
    sub: bool,
    max_influence: f32,
    fade_border: f32,
    radius: f32,
    center: [f32; 2],
    min: [f32; 2],
    max: [f32; 2],
    planes: Vec<[f32; 3]>,
}

impl Area {
    fn new(src: &AreaIn) -> Option<Self> {
        let mut area = Area {
            mat: src.mat,
            sub: src.sub,
            max_influence: src.max_influence,
            fade_border: src.fade_border,
            radius: 0.0,
            center: [0.0; 2],
            min: [0.0; 2],
            max: [0.0; 2],
            planes: Vec::new(),
        };
        if src.circle {
            area.center = src.center;
            area.radius = src.radius;
            area.min = [src.center[0] - src.radius, src.center[1] - src.radius];
            area.max = [src.center[0] + src.radius, src.center[1] + src.radius];
            return Some(area);
        }
        let pts: Vec<[f32; 2]> = src.points.chunks_exact(2).map(|c| [c[0], c[1]]).collect();
        if pts.len() < 3 {
            return None;
        }
        let n = pts.len() as f32;
        let mut centroid = [0.0f32; 2];
        area.min = pts[0];
        area.max = pts[0];
        for v in &pts {
            centroid[0] += v[0] / n;
            centroid[1] += v[1] / n;
            area.min = [area.min[0].min(v[0]), area.min[1].min(v[1])];
            area.max = [area.max[0].max(v[0]), area.max[1].max(v[1])];
        }
        for i in 0..pts.len() {
            let a = pts[i];
            let b = pts[(i + 1) % pts.len()];
            let mut nx = b[1] - a[1];
            let mut ny = a[0] - b[0];
            let length = fma(ny, ny, nx * nx).sqrt();
            if f64::from(length) > 1e-08 {
                let inv = 1.0f32 / length;
                nx *= inv;
                ny *= inv;
            }
            if fma(ny, centroid[1] - a[1], nx * (centroid[0] - a[0])) < 0.0 {
                nx *= -1.0;
                ny *= -1.0;
            }
            area.planes.push([nx, ny, -fma(ny, a[1], nx * a[0])]);
        }
        Some(area)
    }

    fn influence(&self, p: [f32; 2]) -> f32 {
        let mut e = 1e8f32;
        if self.planes.is_empty() {
            let dx = p[0] - self.center[0];
            let dy = p[1] - self.center[1];
            let d2 = fma(dy, dy, dx * dx);
            if d2 > self.radius * self.radius {
                return 0.0;
            }
            e = self.radius - d2.sqrt();
        } else {
            for pl in &self.planes {
                let d = fma(pl[1], p[1], pl[0] * p[0]) + pl[2];
                if d < 0.0 {
                    return 0.0;
                }
                e = e.min(d);
            }
        }
        if e > self.fade_border {
            self.max_influence
        } else {
            e / self.fade_border * self.max_influence
        }
    }
}

#[derive(Default)]
pub struct Group {
    mat: i32,
    cell: i32,
    pos: Vec<f32>,
    normal: Vec<f32>,
    color: Vec<f32>,
    uv0: Vec<f32>,
    uv1: Vec<f32>,
    idx: Vec<u32>,
}

#[repr(C)]
pub struct GroupOut {
    pub mat: i32,
    pub cell: i32,
    pub num_verts: u32,
    pub pos: *const f32,
    pub normal: *const f32,
    pub color: *const f32,
    pub uv0: *const f32,
    pub uv1: *const f32,
    pub num_idx: u32,
    pub idx: *const u32,
}

impl Group {
    pub fn out(&self) -> GroupOut {
        GroupOut {
            mat: self.mat,
            cell: self.cell,
            num_verts: (self.pos.len() / 3) as u32,
            pos: self.pos.as_ptr(),
            normal: self.normal.as_ptr(),
            color: self.color.as_ptr(),
            uv0: self.uv0.as_ptr(),
            uv1: self.uv1.as_ptr(),
            num_idx: self.idx.len() as u32,
            idx: self.idx.as_ptr(),
        }
    }
}

fn lerp3(a: [f32; 3], b: [f32; 3], t: f32) -> [f32; 3] {
    let s = 1.0f32 - t;
    [
        fma(a[0], s, b[0] * t),
        fma(a[1], s, b[1] * t),
        fma(a[2], s, b[2] * t),
    ]
}

/// Bakes every (cell, material) plant batch in `std::map` key order; `None` when no area survives.
pub fn build_undergrowth(
    terr: &Terrain,
    grid: f32,
    mats: &[Mat],
    areas_in: &[AreaIn],
) -> Option<Vec<Group>> {
    let size = terr.size;
    let unit = terr.unit;
    let fw = size as f32 * unit;
    let cells = (fw / grid + 1.0f32) as i32;
    let nmats = mats.len() as i32;
    let areas: Vec<Area> = areas_in.iter().filter_map(Area::new).collect();
    if areas.is_empty() {
        return None;
    }

    let cell_of = |f: f32| clamp_i(((f + fw * 0.5f32) / grid) as i32, 0, cells - 1);
    let mut cell_areas: BTreeMap<i32, Vec<usize>> = BTreeMap::new();
    for (a, area) in areas.iter().enumerate() {
        for cy in cell_of(area.min[1])..=cell_of(area.max[1]) {
            for cx in cell_of(area.min[0])..=cell_of(area.max[0]) {
                cell_areas
                    .entry((cy * cells + cx) * nmats + area.mat)
                    .or_default()
                    .push(a);
            }
        }
    }

    let mut groups = Vec::new();
    for (key, ids) in &cell_areas {
        let lmat = key % nmats;
        let lcell = key / nmats;
        let (cx, cy) = (lcell % cells, lcell / cells);
        let mat = &mats[lmat as usize];
        let res = ((fma(grid, mat.density, 0.5f32)) as i32).max(1);
        let half = fw * 0.5f32;
        let cache = [fma(cx as f32, grid, -half), fma(cy as f32, grid, -half)];

        let mut vgrid = vec![0.0f32; (res * res) as usize];
        let index = |f: f32| fma(clamp_f(f, 0.0, grid) / grid, (res - 1) as f32, 0.5f32) as i32;
        for &a in ids {
            let area = &areas[a];
            for y in index(area.min[1] - cache[1])..=index(area.max[1] - cache[1]) {
                for x in index(area.min[0] - cache[0])..=index(area.max[0] - cache[0]) {
                    let infl = area.influence([
                        fma(x as f32 / res as f32, grid, cache[0]),
                        fma(y as f32 / res as f32, grid, cache[1]),
                    ]);
                    vgrid[(y * res + x) as usize] += if area.sub { -infl } else { infl };
                }
            }
        }

        let step = grid / res as f32;
        let sub_mul = [1.0f32 / mat.sub_div[0], 1.0f32 / mat.sub_div[1]];
        let sub_x = (mat.sub_div[0] as i32).max(1);
        let sub_num = sub_x * (mat.sub_div[1] as i32).max(1);
        let mut g = Group {
            mat: lmat,
            cell: lcell,
            ..Group::default()
        };
        let mut planted = false;
        for i in 0..res * res {
            let v = clamp_f(vgrid[i as usize], 0.0, 1.0);
            if v < 1.0
                && v.partial_cmp(&fast_random_float(cx + cy + i).abs())
                    != Some(std::cmp::Ordering::Greater)
            {
                continue;
            }
            let jitter = fast_random_float(cx * 13 + cy + i) * step * 0.5f32;
            let px = fma((i % res) as f32, step, cache[0]) + jitter;
            let pz = fma((i / res) as f32, step, cache[1]) + jitter;
            let u = (px + fw * 0.5f32) / unit;
            let w = (pz + fw * 0.5f32) / unit;
            if u < 0.0 || w < 0.0 || u > (size - 1) as f32 || w > (size - 1) as f32 {
                continue;
            }
            let x0 = (u as i32).min(size - 2);
            let z0 = (w as i32).min(size - 2);
            let fu = u - x0 as f32;
            let fwt = w - z0 as f32;
            let a = fma(
                terr.height_at(x0, z0),
                1.0 - fu,
                terr.height_at(x0 + 1, z0) * fu,
            );
            let b = fma(
                terr.height_at(x0, z0 + 1),
                1.0 - fu,
                terr.height_at(x0 + 1, z0 + 1) * fu,
            );
            let py = fma(a, 1.0 - fwt, b * fwt);
            if py.is_nan() {
                continue;
            }
            let ppos = v3(px, py, pz);
            planted = true;

            let nx = (u + 0.5f32) as i32;
            let nz = (w + 0.5f32) as i32;
            let h = terr.solid(nx, nz, py);
            let normal = normalize3(v3(
                terr.solid(nx - 1, nz, h) - terr.solid(nx + 1, nz, h),
                2.0 * unit,
                terr.solid(nx, nz - 1, h) - terr.solid(nx, nz + 1, h),
            ));
            let mut rot = rotate_y(fast_random_float(cx * 17 + cy + i) * KPIF);
            if mat.align > 0.0 {
                let n = if mat.align >= 1.0 {
                    normal
                } else {
                    let s = 1.0f32 - mat.align;
                    normalize3(v3(
                        fma(0.0, s, normal.x * mat.align),
                        fma(1.0, s, normal.y * mat.align),
                        fma(0.0, s, normal.z * mat.align),
                    ))
                };
                let tan = normalize3(cross3(n, v3(0.0, 0.0, 1.0)));
                let m = matrix_unit_vectors(
                    v3(-tan.x, -tan.y, -tan.z),
                    n,
                    cross3(n, tan),
                    v3(0.0, 0.0, 0.0),
                );
                rot = matrix_mul(&m, &rot);
            }
            let size_t = fast_random_float(cx * 23 + cy + i).abs();
            let mul = lerp3(mat.min_size_mul, mat.max_size_mul, size_t);
            let vsize = v3(
                mat.base_size[0] * mul[0],
                mat.base_size[1] * mul[1],
                mat.base_size[2] * mul[2],
            );
            let color_t = fast_random_float(cx * 31 + cy + i).abs();
            let s = 1.0f32 - color_t;
            let col = [
                mat.min_color[0] * s + mat.max_color[0] * color_t,
                mat.min_color[1] * s + mat.max_color[1] * color_t,
                mat.min_color[2] * s + mat.max_color[2] * color_t,
                mat.min_color[3] * s + mat.max_color[3] * color_t,
            ];
            let tex = (i * 13) % sub_num;
            let uv_add = [
                (tex % sub_x) as f32 * sub_mul[0],
                (tex / sub_x) as f32 * sub_mul[1],
                0.0f32 * 1.0,
            ];
            let rand = rand_rectf(0.0, 1.0);

            let base = (g.pos.len() / 3) as u32;
            for (p, uv) in mat.pos.iter().zip(&mat.uv) {
                let r = matrix_mul_3x3(&rot, *p);
                g.pos.extend([
                    fma(r.x, vsize.x, ppos.x),
                    fma(r.y, vsize.y, ppos.y),
                    fma(r.z, vsize.z, ppos.z),
                ]);
                g.normal.extend([normal.x, normal.y, normal.z]);
                g.color.extend(col);
                g.uv0.extend([
                    fma(uv.x, sub_mul[0], uv_add[0]),
                    fma(uv.y, sub_mul[1], uv_add[1]),
                    fma(uv.z, 1.0, uv_add[2]),
                ]);
                g.uv1.extend([p.y, rand, 0.0]);
            }
            g.idx.extend(mat.idx.iter().map(|k| base + k));
            for tri in mat.idx.chunks_exact(3) {
                g.idx.extend([base + tri[0], base + tri[2], base + tri[1]]);
            }
        }
        if planted {
            groups.push(g);
        }
    }
    Some(groups)
}
