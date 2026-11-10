//! `cRenderList::Compile`: per-list sort orders and the large-surface-plane placement, from plain
//! key arrays gathered by the C++ shim.

use crate::graphics::stdsort;
use crate::math::{
    Matrixf, Planef, Vector3f, dot3, fma, matrix_mul_3x3, matrix_mul_vec, normalize3, plane_dist,
    sub3,
};

pub const COMPILE_Z: u32 = 0x01;
pub const COMPILE_DIFFUSE: u32 = 0x02;
pub const COMPILE_TRANSLUCENT: u32 = 0x04;
pub const COMPILE_DECAL: u32 = 0x08;
pub const COMPILE_ILLUMINATION: u32 = 0x10;

const ALPHA_TRANS: i32 = 1;
pub const TEXTURE_UNITS: usize = 16;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SolidKey {
    pub alpha_mode: i32,
    pub view_z: f32,
    pub prog_z: usize,
    pub tex_diffuse: usize,
    pub prog_diffuse: usize,
    pub vtx: usize,
    pub mtx: usize,
    pub obj: usize,
    pub tex_units: [usize; TEXTURE_UNITS],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct TransKey {
    pub view_z: f32,
    pub placement: i32,
    /// Sub mesh, large transparent surface material and one sided: the C++ checks in original order.
    pub plane_candidate: u32,
    pub pad: u32,
    pub center: Vector3f,
    pub normal: Vector3f,
    pub point: Vector3f,
    pub world: Matrixf,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DecalKey {
    pub sort_order: i32,
    pub pad: u32,
    pub tex_illum: usize,
    pub vtx: usize,
    pub mtx: usize,
    pub pos: Vector3f,
    pub pad2: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct IllumKey {
    pub illum_amount: f32,
    pub pad: u32,
    pub tex_illum: usize,
    pub vtx: usize,
    pub mtx: usize,
}

/// `Some(a < b)` when the C++ `if(a != b) return a < b;` fires.
fn differs<T: PartialOrd>(a: T, b: T) -> Option<bool> {
    (a != b).then(|| a < b)
}

macro_rules! first_diff {
    ($($e:expr),+ $(,)?) => {
        $( if let Some(r) = $e { return r; } )+
    };
}

fn z_less(a: &SolidKey, b: &SolidKey) -> bool {
    first_diff!(differs(a.alpha_mode, b.alpha_mode));
    if a.alpha_mode == ALPHA_TRANS {
        first_diff!(
            differs(a.prog_z, b.prog_z),
            differs(a.tex_diffuse, b.tex_diffuse)
        );
    }
    a.view_z > b.view_z
}

fn diffuse_less(a: &SolidKey, b: &SolidKey) -> bool {
    first_diff!(differs(a.prog_diffuse, b.prog_diffuse));
    for (x, y) in a.tex_units.iter().zip(&b.tex_units) {
        first_diff!(differs(x, y));
    }
    first_diff!(differs(a.vtx, b.vtx), differs(a.mtx, b.mtx));
    a.obj < b.obj
}

fn trans_less(a: &TransKey, b: &TransKey) -> bool {
    first_diff!(differs(a.placement, b.placement));
    a.view_z < b.view_z
}

fn decal_less(a: &DecalKey, b: &DecalKey) -> bool {
    first_diff!(
        differs(a.sort_order, b.sort_order),
        differs(a.tex_illum, b.tex_illum),
        differs(a.vtx, b.vtx),
        differs(a.mtx, b.mtx),
        differs(a.pos.x, b.pos.x),
        differs(a.pos.y, b.pos.y),
    );
    a.pos.z < b.pos.z
}

fn illum_less(a: &IllumKey, b: &IllumKey) -> bool {
    first_diff!(
        differs(a.tex_illum, b.tex_illum),
        differs(a.vtx, b.vtx),
        differs(a.mtx, b.mtx)
    );
    a.illum_amount < b.illum_amount
}

/// Fills `out` with `0..n` ordered as libstdc++ `std::sort` with `less` over `keys` orders them.
fn sorted_indices<K>(keys: &[K], out: &mut [u32], less: impl Fn(&K, &K) -> bool) {
    for (i, o) in (0u32..).zip(out.iter_mut()) {
        *o = i;
    }
    let n = keys.len().min(out.len());
    let Some(out) = out.get_mut(..n) else { return };
    stdsort::sort(out, |&a, &b| {
        match (keys.get(a as usize), keys.get(b as usize)) {
            (Some(x), Some(y)) => less(x, y),
            _ => false,
        }
    });
}

pub fn sort_z(keys: &[SolidKey], out: &mut [u32]) {
    sorted_indices(keys, out, z_less);
}

pub fn sort_diffuse(keys: &[SolidKey], out: &mut [u32]) {
    sorted_indices(keys, out, diffuse_less);
}

pub fn sort_translucent(keys: &[TransKey], out: &mut [u32]) {
    sorted_indices(keys, out, trans_less);
}

pub fn sort_decal(keys: &[DecalKey], out: &mut [u32]) {
    sorted_indices(keys, out, decal_less);
}

pub fn sort_illumination(keys: &[IllumKey], out: &mut [u32]) {
    sorted_indices(keys, out, illum_less);
}

/// `FindNearestLargeSurfacePlane`: writes `placement` of every key.
pub fn find_large_surface_planes(keys: &mut [TransKey], origin: Vector3f) {
    let mut nearest: Option<(usize, Planef, f32)> = None;
    for (i, k) in keys.iter().enumerate() {
        if k.plane_candidate == 0 {
            continue;
        }
        let mut normal = matrix_mul_3x3(&k.world, k.normal);
        normalize3(&mut normal);
        let pos = matrix_mul_vec(&k.world, k.point);
        if dot3(sub3(pos, origin), normal) >= 0.0 {
            continue;
        }
        let plane = Planef {
            a: normal.x,
            b: normal.y,
            c: normal.z,
            d: -fma(normal.z, pos.z, fma(normal.x, pos.x, normal.y * pos.y)),
        };
        let dist = plane_dist(&plane, origin);
        if nearest.is_none_or(|(_, _, closest)| dist < closest) {
            nearest = Some((i, plane, dist));
        }
    }
    for (i, k) in keys.iter_mut().enumerate() {
        k.placement = match nearest {
            None => 0,
            Some((n, ..)) if n == i => 0,
            Some((_, plane, _)) => {
                if plane_dist(&plane, k.center) < 0.0 {
                    -1
                } else {
                    1
                }
            }
        };
    }
}

pub struct CompileIn<'a> {
    pub flags: u32,
    pub origin: Vector3f,
    pub solid: &'a [SolidKey],
    pub trans: &'a mut [TransKey],
    pub decal: &'a [DecalKey],
    pub illum: &'a [IllumKey],
}

pub struct CompileOut<'a> {
    pub z: &'a mut [u32],
    pub diffuse: &'a mut [u32],
    pub trans: &'a mut [u32],
    pub decal: &'a mut [u32],
    pub illum: &'a mut [u32],
}

pub fn compile(i: CompileIn<'_>, o: CompileOut<'_>) {
    if i.flags & COMPILE_Z != 0 {
        sort_z(i.solid, o.z);
    }
    if i.flags & COMPILE_DIFFUSE != 0 {
        sort_diffuse(i.solid, o.diffuse);
    }
    if i.flags & COMPILE_DECAL != 0 {
        sort_decal(i.decal, o.decal);
    }
    if i.flags & COMPILE_ILLUMINATION != 0 {
        sort_illumination(i.illum, o.illum);
    }
    if i.flags & COMPILE_TRANSLUCENT != 0 {
        find_large_surface_planes(i.trans, i.origin);
        sort_translucent(i.trans, o.trans);
    }
}
