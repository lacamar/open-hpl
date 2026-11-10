//! `m_setRenderObjects` (a `std::multiset` ordered by z, clip region, material, texture) and the
//! batching loop of `cGuiSet::RenderClipRegion`.

use std::ffi::c_void;
use std::ptr;

use super::sort::{Key, Sorter};
use crate::math::{matrix_mul_vec, rotate_z};
use crate::types::{Color, Vector2f, Vector3f};

/// Vertices per batch before the C++ flushes (`lIdxAdd + 4 <= 20000`).
const BATCH_LIMIT: usize = 20000;

pub const BEGIN_MATERIAL: u32 = 1;
pub const SET_CLIP: u32 = 2;
pub const END_CLIP: u32 = 4;
pub const END_MATERIAL: u32 = 8;

/// Mirror of `cVertex`; only `pos`, `tex` and `col` are read.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct GfxVertex {
    pub pos: Vector3f,
    pub tex: Vector3f,
    pub tan: Vector3f,
    pub norm: Vector3f,
    pub col: Color,
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<GfxVertex>() == 64);
    assert!(offset_of!(GfxVertex, tex) == 12 && offset_of!(GfxVertex, col) == 48);
};

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OutVertex {
    pub pos: Vector3f,
    pub col: Color,
    pub tex: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Batch {
    pub material: *const c_void,
    pub texture: *const c_void,
    pub clip: *const c_void,
    pub first: usize,
    pub count: usize,
    pub flags: u32,
}

#[derive(Clone, Copy)]
pub struct Rotation {
    pub angle: f32,
    pub pivot: Vector2f,
}

/// One `cGuiRenderObject`. The gfx element is held as the addresses of the fields the C++ reads
/// from it, so a material or texture swapped before rendering is picked up as in the original.
pub struct Object {
    pub vtx: *const GfxVertex,
    pub gfx_material: *const *const c_void,
    pub gfx_texture: *const *const c_void,
    pub custom_material: *const c_void,
    pub clip: *const c_void,
    pub pos: Vector3f,
    pub size: Vector2f,
    pub color: Color,
    pub rotation: Option<Rotation>,
}

impl Object {
    fn material(&self) -> *const c_void {
        if self.custom_material.is_null() {
            // SAFETY: `gfx_material` is the address of a live gfx element's material field (host contract).
            unsafe { ptr::read(self.gfx_material) }
        } else {
            self.custom_material
        }
    }

    fn texture(&self) -> *const c_void {
        // SAFETY: `gfx_texture` is the address of a live gfx element's first texture slot (host contract).
        unsafe { ptr::read(self.gfx_texture) }
    }
}

#[derive(Default)]
pub struct RenderList {
    objects: Vec<Object>,
    keys: Vec<Key>,
    sorter: Sorter,
    pub vertices: Vec<OutVertex>,
    pub batches: Vec<Batch>,
}

impl RenderList {
    pub fn len(&self) -> usize {
        self.objects.len()
    }

    pub fn is_empty(&self) -> bool {
        self.objects.is_empty()
    }

    pub fn clear(&mut self) {
        self.objects.clear();
    }

    pub fn push(&mut self, object: Object) {
        self.objects.push(object);
    }

    /// Sorts the objects and fills `batches`/`vertices` as the `RenderClipRegion` loop would emit them.
    pub fn build(&mut self) {
        self.vertices.clear();
        self.batches.clear();
        self.keys.clear();
        self.keys.extend(self.objects.iter().map(|o| Key {
            z: o.pos.z,
            clip: o.clip as usize,
            material: o.material() as usize,
            texture: o.texture() as usize,
        }));
        self.vertices.reserve(self.objects.len().saturating_mul(4));

        let mut prev_material: *const c_void = ptr::null();
        let mut prev_clip: *const c_void = ptr::null();
        let objects = &self.objects;
        let mut order = self
            .sorter
            .order(&self.keys)
            .filter_map(|i| objects.get(i))
            .peekable();

        while let Some(first) = order.peek() {
            let (material, texture, clip) = (first.material(), first.texture(), first.clip);
            let first_vertex = self.vertices.len();
            let mut idx_add = 0usize;
            let mut next_material = material;
            let mut next_clip = clip;
            let mut ended = false;
            while let Some(object) = order.next() {
                emit_quad(&mut self.vertices, object);
                idx_add += 4;
                let Some(next) = order.peek() else {
                    ended = true;
                    break;
                };
                next_material = next.material();
                next_clip = next.clip;
                if !(idx_add + 4 <= BATCH_LIMIT
                    && next.texture() == texture
                    && next_material == material
                    && next_clip == clip)
                {
                    break;
                }
            }
            let mut flags = 0;
            if prev_material != material {
                flags |= BEGIN_MATERIAL;
            }
            if prev_clip != clip {
                flags |= SET_CLIP;
            }
            if ended || next_clip != clip {
                flags |= END_CLIP;
            }
            if ended || next_material != material {
                flags |= END_MATERIAL;
            }
            self.batches.push(Batch {
                material,
                texture,
                clip,
                first: first_vertex,
                count: idx_add,
                flags,
            });
            prev_material = material;
            prev_clip = clip;
        }
    }
}

fn emit_quad(out: &mut Vec<OutVertex>, object: &Object) {
    // SAFETY: `vtx` points at the 4 vertices of a live gfx element (host contract).
    let vertices = unsafe { std::slice::from_raw_parts(object.vtx, 4) };
    let (p, s) = (object.pos, object.size);
    match object.rotation {
        Some(rot) => {
            let m = rotate_z(rot.angle);
            for v in vertices {
                let mut q = v.pos;
                q.x = q.x.mul_add(s.x, -rot.pivot.x);
                q.y = q.y.mul_add(s.y, -rot.pivot.y);
                q = matrix_mul_vec(&m, q);
                q.x += rot.pivot.x;
                q.y += rot.pivot.y;
                out.push(OutVertex {
                    pos: Vector3f {
                        x: q.x + p.x,
                        y: q.y + p.y,
                        z: p.z,
                    },
                    col: modulate(v.col, object.color),
                    tex: v.tex,
                });
            }
        }
        None => {
            for v in vertices {
                out.push(OutVertex {
                    pos: Vector3f {
                        x: v.pos.x.mul_add(s.x, p.x),
                        y: v.pos.y.mul_add(s.y, p.y),
                        z: p.z,
                    },
                    col: modulate(v.col, object.color),
                    tex: v.tex,
                });
            }
        }
    }
}

fn modulate(a: Color, b: Color) -> Color {
    Color {
        r: a.r * b.r,
        g: a.g * b.g,
        b: a.b * b.b,
        a: a.a * b.a,
    }
}
