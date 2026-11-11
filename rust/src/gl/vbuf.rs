//! `iVertexBufferOpenGL` and its VBO / client-array variants: element layout, bind plans,
//! vertex-add counts, transform and shadow-double passes. Plans are `Op` lists the C++ executor
//! turns into `gl*` calls; nothing here allocates.

use super::c;
use super::tables;
use crate::graphics::formats;
use crate::math::{matrix_inverse, matrix_mul_3x3, matrix_mul_vec, normalize3};
use crate::types::{Matrixf, Vector3f};

pub const ELEMENT_COUNT: usize = 14;
pub const FORMAT_FLOAT: i32 = 0;
pub const FORMAT_INT: i32 = 1;
pub const FORMAT_BYTE: i32 = 2;
pub const ELEMENT_POSITION: i32 = 1;
pub const ELEMENT_COLOR0: i32 = 2;
pub const FLAG_NORMAL: u32 = 0x1;
pub const FLAG_POSITION: u32 = 0x2;
pub const FLAG_TEXTURE1: u32 = 0x20;
pub const FLAG_ALL: u32 = 0xFFFF_FFFF;
pub const MAX_OPS: usize = 128;

pub const OP_CLIENT_ACTIVE: u32 = 1;
pub const OP_ENABLE: u32 = 2;
pub const OP_DISABLE: u32 = 3;
pub const OP_BIND_BUFFER: u32 = 4;
pub const OP_POINTER: u32 = 5;

/// Pointer source of an `OP_POINTER`.
pub const SRC_OFFSET: u32 = 0;
pub const SRC_ELEMENT: u32 = 1;
pub const SRC_BATCH_VERTEX: u32 = 2;
pub const SRC_BATCH_TEXCOORD: u32 = 3;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Elem {
    pub kind: i32,
    pub format: i32,
    pub element_num: i32,
    pub handle: u32,
}

/// `array` is the GL client-array enum, which also selects the `gl*Pointer` call. For a client
/// array `index` is the element slot, for batch sources a float offset (vertex) or unit (texcoord).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Op {
    pub kind: u32,
    pub array: u32,
    pub size: i32,
    pub gl_type: u32,
    pub stride: i32,
    pub src: u32,
    pub index: u32,
}

struct Ops<'a> {
    out: &'a mut [Op],
    len: usize,
}

impl Ops<'_> {
    fn push(&mut self, op: Op) {
        if let Some(slot) = self.out.get_mut(self.len) {
            *slot = op;
            self.len += 1;
        }
    }

    fn simple(&mut self, kind: u32, array: u32) {
        self.push(Op {
            kind,
            array,
            ..Op::default()
        });
    }

    fn client_active(&mut self, unit: u32) {
        self.simple(OP_CLIENT_ACTIVE, c::TEXTURE0_ARB.wrapping_add(unit));
    }

    fn bind(&mut self, handle: u32) {
        self.simple(OP_BIND_BUFFER, handle);
    }
}

fn texture_unit(e: &Elem) -> Option<u32> {
    u32::try_from(formats::vertex_element_texture_unit(e.kind)).ok()
}

fn pointer(e: &Elem, src: u32, index: u32) -> Op {
    Op {
        kind: OP_POINTER,
        array: tables::vertex_array(e.kind),
        size: e.element_num,
        gl_type: tables::vertex_type(e.format),
        stride: 0,
        src,
        index,
    }
}

fn has_pointer(kind: i32) -> bool {
    matches!(kind, 0..=9)
}

/// `cVertexBufferOGL_VBO::SetVertexStates`.
pub fn bind_vbo(elems: &[Elem], out: &mut [Op]) -> usize {
    let mut o = Ops { out, len: 0 };
    let mut extra_unit = false;
    for e in elems.iter().filter(|e| e.kind != ELEMENT_POSITION) {
        if let Some(unit) = texture_unit(e) {
            extra_unit |= unit > 0;
            o.client_active(unit);
        }
        o.simple(OP_ENABLE, tables::vertex_array(e.kind));
        o.bind(e.handle);
        if has_pointer(e.kind) {
            o.push(pointer(e, SRC_OFFSET, 0));
        }
    }
    if extra_unit {
        o.client_active(0);
    }
    for e in elems.iter().filter(|e| e.kind == ELEMENT_POSITION) {
        o.simple(OP_ENABLE, tables::vertex_array(e.kind));
        o.bind(e.handle);
        o.push(pointer(e, SRC_OFFSET, 0));
    }
    o.bind(0);
    o.len
}

/// `cVertexBufferOGL_Array::SetVertexStates`.
pub fn bind_array(elems: &[Elem], out: &mut [Op]) -> usize {
    let mut o = Ops { out, len: 0 };
    for (i, e) in elems.iter().enumerate() {
        if let Some(unit) = texture_unit(e) {
            o.client_active(unit);
        }
        o.simple(OP_ENABLE, tables::vertex_array(e.kind));
        if has_pointer(e.kind) {
            o.push(pointer(e, SRC_ELEMENT, u32::try_from(i).unwrap_or(0)));
        }
    }
    o.client_active(0);
    o.len
}

/// `UnBind` of both variants.
pub fn unbind(elems: &[Elem], out: &mut [Op]) -> usize {
    let mut o = Ops { out, len: 0 };
    o.bind(0);
    for e in elems {
        if let Some(unit) = texture_unit(e) {
            o.client_active(unit);
        }
        o.simple(OP_DISABLE, tables::vertex_array(e.kind));
    }
    o.client_active(0);
    o.len
}

pub const BATCH_NORMAL: u32 = 0x1;
pub const BATCH_POSITION: u32 = 0x2;
pub const BATCH_COLOR0: u32 = 0x4;
pub const BATCH_TEXTURE0: u32 = 0x8;

/// `SetVtxBatchStates`.
pub fn batch_states(flags: u32, out: &mut [Op]) -> usize {
    let mut o = Ops { out, len: 0 };
    let toggle = |o: &mut Ops, on: bool, array: u32| {
        o.simple(if on { OP_ENABLE } else { OP_DISABLE }, array);
    };
    toggle(&mut o, flags & BATCH_POSITION != 0, c::VERTEX_ARRAY);
    toggle(&mut o, flags & BATCH_COLOR0 != 0, c::COLOR_ARRAY);
    toggle(&mut o, flags & BATCH_NORMAL != 0, c::NORMAL_ARRAY);
    for unit in 0..3u32 {
        o.client_active(unit);
        toggle(
            &mut o,
            flags & BATCH_TEXTURE0.wrapping_shl(unit) != 0,
            c::TEXTURE_COORD_ARRAY,
        );
    }
    o.len
}

/// `SetUpBatchArrays`; `stride` is `sizeof(float) * mlBatchStride`.
pub fn batch_arrays(stride: i32, out: &mut [Op]) -> usize {
    let mut o = Ops { out, len: 0 };
    let batch = |array, size, offset| Op {
        kind: OP_POINTER,
        array,
        size,
        gl_type: c::FLOAT,
        stride,
        src: SRC_BATCH_VERTEX,
        index: offset,
    };
    o.push(batch(c::VERTEX_ARRAY, 3, 0));
    o.push(batch(c::COLOR_ARRAY, 4, 3));
    o.push(batch(c::NORMAL_ARRAY, 0, 10));
    for unit in 0..3 {
        o.client_active(unit);
        o.push(batch(c::TEXTURE_COORD_ARRAY, 3, 7));
    }
    o.len
}

/// `SetBatchTextureUnitActive`.
pub fn batch_texture_unit(unit: u32, active: bool, stride: i32, out: &mut [Op]) -> usize {
    let mut o = Ops { out, len: 0 };
    o.client_active(unit);
    o.push(if active {
        Op {
            kind: OP_POINTER,
            array: c::TEXTURE_COORD_ARRAY,
            size: 3,
            gl_type: c::FLOAT,
            stride: 0,
            src: SRC_BATCH_TEXCOORD,
            index: unit,
        }
    } else {
        Op {
            kind: OP_POINTER,
            array: c::TEXTURE_COORD_ARRAY,
            size: 3,
            gl_type: c::FLOAT,
            stride,
            src: SRC_BATCH_VERTEX,
            index: 7,
        }
    });
    o.len
}

/// Draw mode: the override unless it is `LastEnum` (8).
pub fn draw_mode(own: i32, requested: i32) -> u32 {
    tables::draw_mode(if requested == 8 { own } else { requested })
}

/// Index count of a `Draw`: the explicit element count unless negative.
pub fn draw_count(element_num: i32, index_num: i32) -> i32 {
    if element_num < 0 {
        index_num
    } else {
        element_num
    }
}

/// Components `AddVertexVec3f` pushes: x, y, then z and an implicit 1 by element width.
pub fn vec3_components(element_num: i32, v: Vector3f, out: &mut [f32; 4]) -> usize {
    *out = [v.x, v.y, 0.0, 0.0];
    let mut n = 2;
    if element_num > 2 {
        out[2] = v.z;
        n = 3;
    }
    if element_num > 3 {
        out[3] = 1.0;
        n = 4;
    }
    n
}

/// Components `AddVertexColor` pushes: four floats for float elements, none otherwise.
pub fn color_components(format: i32) -> usize {
    if format == FORMAT_FLOAT { 4 } else { 0 }
}

/// Requested copy flags: everything present when the caller passed all bits.
pub fn copy_flags(requested: u32, present: u32) -> u32 {
    if requested == FLAG_ALL {
        present
    } else {
        requested
    }
}

/// `GetVertexNum`. Division by a zero element width yields 0 as the hardware divide did.
pub fn vertex_num(position_size: i32, element_num: i32, shadow_double: bool) -> i32 {
    let n = position_size.checked_div(element_num).unwrap_or(0);
    if shadow_double { n.wrapping_div(2) } else { n }
}

/// `CreateBoundingVolume` precheck: 0 ready, 1 no position element, 2 position is not float.
pub fn bounding_check(flags: u32, position_format: i32) -> i32 {
    if flags & FLAG_POSITION == 0 {
        1
    } else if position_format != FORMAT_FLOAT {
        2
    } else {
        0
    }
}

/// Elements appended by `CreateShadowDouble` for `size` position elements: groups of four.
pub fn shadow_double_extra(size: usize) -> usize {
    size.wrapping_shr(2).wrapping_mul(4)
}

/// Fills the appended half: `(x, y, z, 0)` per group read from stride-four positions. Byte
/// arrays are left zero (C++ read past the array).
pub fn shadow_double_fill(format: i32, data: &mut [u32], size: usize) {
    if format == FORMAT_BYTE {
        return;
    }
    let groups = size.wrapping_shr(2);
    for i in 0..groups {
        for k in 0..3 {
            let v = data
                .get(i.wrapping_mul(4).wrapping_add(k))
                .copied()
                .unwrap_or(0);
            if let Some(d) = data.get_mut(size.wrapping_add(i.wrapping_mul(4)).wrapping_add(k)) {
                *d = v;
            }
        }
        if let Some(d) = data.get_mut(size.wrapping_add(i.wrapping_mul(4)).wrapping_add(3)) {
            *d = 0;
        }
    }
}

fn transpose(a: &Matrixf) -> Matrixf {
    let mut t = Matrixf::default();
    for (i, row) in t.m.iter_mut().enumerate() {
        for (j, e) in row.iter_mut().enumerate() {
            *e = a.m.get(j).and_then(|r| r.get(i)).copied().unwrap_or(0.0);
        }
    }
    t
}

fn rotation(m: &Matrixf) -> Matrixf {
    let mut r = Matrixf::default();
    for (i, row) in r.m.iter_mut().enumerate() {
        for (j, e) in row.iter_mut().enumerate() {
            *e = if i < 3 && j < 3 {
                m.m.get(i).and_then(|r| r.get(j)).copied().unwrap_or(0.0)
            } else {
                0.0
            };
        }
    }
    r.m[3][3] = 1.0;
    r
}

fn load(a: &[f32], at: usize) -> Option<Vector3f> {
    let s = a.get(at..at.checked_add(3)?)?;
    Some(Vector3f {
        x: *s.first()?,
        y: *s.get(1)?,
        z: *s.get(2)?,
    })
}

fn store(a: &mut [f32], at: usize, v: Vector3f) {
    if let Some([x, y, z]) = at.checked_add(3).and_then(|e| a.get_mut(at..e)) {
        *x = v.x;
        *y = v.y;
        *z = v.z;
    }
}

fn index(i: i32, stride: i32) -> Option<usize> {
    usize::try_from(i.wrapping_mul(stride)).ok()
}

/// `Transform`: positions (and the shadow copy), normals by the inverse transpose of the
/// rotation, tangents by the rotation. Out-of-range slots are skipped. Returns the element flags
/// the caller re-uploads.
pub fn transform(
    m: &Matrixf,
    pos: &mut [f32],
    mut normal: Option<&mut [f32]>,
    mut tangent: Option<&mut [f32]>,
    vtx_num: i32,
    stride: i32,
    shadow_double: bool,
) -> u32 {
    let rot = rotation(m);
    let normal_rot = transpose(&matrix_inverse(&rot));
    let shadow_offset = vtx_num.wrapping_mul(4);
    for i in 0..vtx_num.max(0) {
        if let Some(at) = index(i, stride)
            && let Some(p) = load(pos, at)
        {
            let v = matrix_mul_vec(m, p);
            store(pos, at, v);
            if shadow_double
                && let Some(extra) = at.checked_add(usize::try_from(shadow_offset).unwrap_or(0))
            {
                store(pos, extra, v);
            }
        }
        if let Some(n) = normal.as_deref_mut()
            && let Some(at) = index(i, 3)
            && let Some(v) = load(n, at)
        {
            let mut v = matrix_mul_3x3(&normal_rot, v);
            normalize3(&mut v);
            store(n, at, v);
        }
        if let Some(t) = tangent.as_deref_mut()
            && let Some(at) = index(i, 4)
            && let Some(v) = load(t, at)
        {
            let mut v = matrix_mul_3x3(&rot, v);
            normalize3(&mut v);
            store(t, at, v);
        }
    }
    let mut flags = FLAG_POSITION;
    if normal.is_some() {
        flags |= FLAG_NORMAL;
    }
    if tangent.is_some() {
        flags |= FLAG_TEXTURE1;
    }
    flags
}
