//! `.msh` / `.anm` reader (`hpl::cMeshLoaderMSH`), versions 7 (HPL2) and 8 (HPL3).
//!
//! The C++ interleaves parsing with object creation and has no failure path after the header;
//! this builds the whole file as data. Inputs that make the C++ crash, throw or allocate without
//! bound (counts out of range, unknown element types, duplicate arrays) are `Unsupported`.

use crate::binary_buffer::BinaryBuffer;
use crate::math::{cross3, dot3, normalize3, v3};
use crate::types::Vector3f;
use std::cmp::Ordering;
use std::ffi::CStr;

pub const MAGIC: i32 = 0x7603_4569;
pub const VERSION: i32 = 7;
pub const VERSION_HPL3: i32 = 8;

const MAX_ITEMS: usize = 1 << 22;
const MAX_BYTES: usize = 1 << 28;
const ELEMENT_LAST: i32 = 14;
const ELEMENT_NORMAL: i32 = 0;
const ELEMENT_POSITION: i32 = 1;
const ELEMENT_TANGENT: i32 = 4;
const ELEMENT_TEXTURE0: i32 = 5;
const HPL3_UV_SET: i32 = 7;
const FORMAT_FLOAT: i32 = 0;
const FORMAT_BYTE: i32 = 2;

/// Why an input is refused: each case makes the C++ loader crash, throw or allocate without bound.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Reason {
    Budget,
    Count,
    NegativeSize,
    SizeRange,
    TooManyNodes,
    ElementType,
    ElementCount,
    RepeatOverflow,
    ElementFormat,
    TangentFix,
}

impl Reason {
    pub const ALL: [(Reason, &'static CStr); 10] = [
        (Reason::Budget, c"allocation budget exceeded"),
        (Reason::Count, c"count out of range"),
        (Reason::NegativeSize, c"negative size"),
        (Reason::SizeRange, c"size out of range"),
        (Reason::TooManyNodes, c"too many nodes"),
        (Reason::ElementType, c"element type out of range"),
        (Reason::ElementCount, c"element count out of range"),
        (Reason::RepeatOverflow, c"repeated element array overflows"),
        (Reason::ElementFormat, c"element format out of range"),
        (Reason::TangentFix, c"tangent fix on malformed arrays"),
    ];
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MshError {
    BadMagic,
    BadVersion,
    Unsupported(Reason),
}

#[derive(Debug, Clone, PartialEq)]
pub struct Node {
    pub name: Vec<u8>,
    pub matrix: [f32; 16],
    pub flags: i32,
    pub parent: i32,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Bone {
    pub name: Vec<u8>,
    pub sid: Vec<u8>,
    pub matrix: [f32; 16],
    pub unit_scale: f32,
    pub parent: i32,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Collider {
    pub kind: i16,
    pub matrix: [f32; 16],
    pub size: [f32; 3],
    pub char_collider: bool,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BonePair {
    pub vtx: i32,
    pub bone: i32,
    pub weight: f32,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Array {
    pub kind: i32,
    pub format: i32,
    pub prog_var: i32,
    pub elem_num: i32,
    /// Times the file named this element type again, which the C++ logs as an error.
    pub repeats: i32,
    pub count: usize,
    pub data: Vec<u8>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct SubMesh {
    pub name: Vec<u8>,
    pub material: Vec<u8>,
    pub matrix: [f32; 16],
    pub scale: [f32; 3],
    pub collide_shape: bool,
    pub colliders: Vec<Collider>,
    pub bone_pairs: Vec<BonePair>,
    pub arrays: Vec<Array>,
    /// Element types of HPL3 arrays that have no HPL2 equivalent; the C++ warns for each.
    pub skipped: Vec<i32>,
    pub indices: Vec<u32>,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Key {
    pub time: f32,
    pub trans: [f32; 3],
    pub rot: [f32; 4],
}

#[derive(Debug, Clone, PartialEq)]
pub struct Track {
    pub name: Vec<u8>,
    pub flags: i16,
    pub keys: Vec<Key>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Anim {
    pub length: f32,
    pub tracks: Vec<Track>,
}

#[derive(Debug, Clone, PartialEq, Default)]
pub struct Mesh {
    pub hpl3: bool,
    pub bones: Option<Vec<Bone>>,
    pub nodes: Vec<Node>,
    pub submeshes: Vec<SubMesh>,
    pub animations: Vec<Anim>,
}

struct Reader {
    bb: BinaryBuffer,
    used: usize,
}

type Res<T> = Result<T, MshError>;

impl Reader {
    fn new(data: &[u8]) -> Self {
        let mut bb = BinaryBuffer::new();
        bb.assign(data);
        Self { bb, used: 0 }
    }

    fn at_eof(&self) -> bool {
        self.bb.pos() >= self.bb.size()
    }

    fn charge(&mut self, bytes: usize) -> Res<()> {
        self.used = self.used.saturating_add(bytes);
        if self.used > MAX_BYTES {
            return Err(MshError::Unsupported(Reason::Budget));
        }
        Ok(())
    }

    fn i32(&mut self) -> i32 {
        self.bb.get_i32()
    }

    fn i16(&mut self) -> i16 {
        let mut b = [0; 2];
        self.bb.get_array(&mut b, 2);
        i16::from_le_bytes(b)
    }

    fn bool(&mut self) -> bool {
        self.bb.get_u8() != 0
    }

    fn f32(&mut self) -> f32 {
        let mut b = [0; 4];
        self.bb.get_array(&mut b, 4);
        f32::from_le_bytes(b)
    }

    fn floats<const N: usize>(&mut self) -> [f32; N] {
        let mut out = [0.0; N];
        for v in &mut out {
            *v = self.f32();
        }
        out
    }

    fn string(&mut self) -> Vec<u8> {
        self.bb.get_string().to_vec()
    }

    /// A signed count the C++ uses as a loop bound; negative runs zero times.
    fn count(&mut self) -> Res<usize> {
        let n = usize::try_from(self.i32()).unwrap_or(0);
        if n > MAX_ITEMS {
            return Err(MshError::Unsupported(Reason::Count));
        }
        Ok(n)
    }

    /// `ResizeX(n)`-style count: negative makes the C++ throw.
    fn size(&mut self) -> Res<usize> {
        let n =
            usize::try_from(self.i32()).map_err(|_| MshError::Unsupported(Reason::NegativeSize))?;
        if n > MAX_ITEMS {
            return Err(MshError::Unsupported(Reason::SizeRange));
        }
        Ok(n)
    }
}

fn read_tree<T>(r: &mut Reader, mut item: impl FnMut(&mut Reader, i32) -> (T, i32)) -> Res<Vec<T>> {
    let mut out = Vec::new();
    let mut stack = vec![(r.count()?, -1)];
    while let Some(top) = stack.last_mut() {
        if top.0 == 0 {
            stack.pop();
            continue;
        }
        top.0 -= 1;
        let parent = top.1;
        r.charge(192)?;
        let (value, children) = item(r, parent);
        out.push(value);
        if out.len() > MAX_ITEMS {
            return Err(MshError::Unsupported(Reason::TooManyNodes));
        }
        let index = i32::try_from(out.len().saturating_sub(1)).unwrap_or(i32::MAX);
        let children = usize::try_from(children).unwrap_or(0);
        if children > MAX_ITEMS {
            return Err(MshError::Unsupported(Reason::Count));
        }
        stack.push((children, index));
    }
    Ok(out)
}

fn read_bones(r: &mut Reader, unit_scale: bool) -> Res<Vec<Bone>> {
    read_tree(r, |r, parent| {
        let name = r.string();
        let sid = r.string();
        let matrix = r.floats();
        let unit = if unit_scale { r.f32() } else { 1.0 };
        let children = r.i32();
        let bone = Bone {
            name,
            sid,
            matrix,
            unit_scale: unit,
            parent,
        };
        (bone, children)
    })
}

fn read_nodes(r: &mut Reader) -> Res<Vec<Node>> {
    read_tree(r, |r, parent| {
        let name = r.string();
        let matrix = r.floats();
        let flags = r.i32();
        let children = r.i32();
        let node = Node {
            name,
            matrix,
            flags,
            parent,
        };
        (node, children)
    })
}

fn element_bytes(format: i32) -> usize {
    if format == FORMAT_BYTE { 1 } else { 4 }
}

fn read_array(
    r: &mut Reader,
    hpl3: bool,
    vtx_num: i32,
    arrays: &mut Vec<Array>,
    skipped: &mut Vec<i32>,
) -> Res<()> {
    let mut kind = i32::from(r.i16());
    let format = i32::from(r.i16());
    let prog_var = r.i32();
    let elem_num = r.i32();
    if hpl3 {
        if kind == HPL3_UV_SET {
            kind = ELEMENT_TEXTURE0;
        } else if !(0..=ELEMENT_TANGENT).contains(&kind) {
            let skip = (i64::from(vtx_num) as u64)
                .wrapping_mul(i64::from(elem_num) as u64)
                .wrapping_mul(element_bytes(format) as u64);
            skipped.push(kind);
            r.bb.add_pos(usize::try_from(skip).unwrap_or(usize::MAX));
            return Ok(());
        }
    }
    if !(0..ELEMENT_LAST).contains(&kind) {
        return Err(MshError::Unsupported(Reason::ElementType));
    }
    let n = vtx_num
        .checked_mul(elem_num)
        .and_then(|n| usize::try_from(n).ok())
        .ok_or(MshError::Unsupported(Reason::ElementCount))?;
    let valid_format = (0..=FORMAT_BYTE).contains(&format);
    // A repeated element type keeps the first array's layout: the C++ resizes it and reads the
    // new data over it.
    if let Some(old) = arrays.iter_mut().find(|a| a.kind == kind) {
        let old_bytes = element_bytes(old.format);
        if valid_format && element_bytes(format) > old_bytes {
            return Err(MshError::Unsupported(Reason::RepeatOverflow));
        }
        let total = n * old_bytes;
        r.charge(total)?;
        old.data.resize(total, 0);
        old.count = n;
        old.repeats += 1;
        if valid_format && let Some(dst) = old.data.get_mut(..n * element_bytes(format)) {
            if format == FORMAT_BYTE {
                r.bb.get_data(dst);
            } else {
                r.bb.get_array(dst, 4);
            }
        }
        return Ok(());
    }
    if !valid_format {
        return Err(MshError::Unsupported(Reason::ElementFormat));
    }
    let bytes = n * element_bytes(format);
    r.charge(bytes)?;
    let mut data = vec![0; bytes];
    if format == FORMAT_BYTE {
        r.bb.get_data(&mut data);
    } else {
        r.bb.get_array(&mut data, 4);
    }
    arrays.push(Array {
        kind,
        format,
        prog_var,
        elem_num,
        repeats: 0,
        count: n,
        data,
    });
    Ok(())
}

fn f32_at(data: &[u8], i: usize) -> Option<f32> {
    let b = data.get(i.checked_mul(4)?..)?.get(..4)?;
    Some(f32::from_le_bytes(b.try_into().ok()?))
}

fn set_f32(data: &mut [u8], i: usize, v: f32) -> Option<()> {
    let start = i.checked_mul(4)?;
    data.get_mut(start..start.checked_add(4)?)?
        .copy_from_slice(&v.to_le_bytes());
    Some(())
}

fn vec3_at(data: &[u8], base: usize) -> Option<Vector3f> {
    Some(v3(
        f32_at(data, base)?,
        f32_at(data, base.checked_add(1)?)?,
        f32_at(data, base.checked_add(2)?)?,
    ))
}

/// Shipped HPL3 meshes carry zero tangents on uv-degenerate triangles; the C++ patches them.
fn fix_tangents(arrays: &mut [Array]) -> Res<()> {
    let find = |kind: i32| arrays.iter().position(|a| a.kind == kind);
    let (Some(ti), Some(ni)) = (find(ELEMENT_TANGENT), find(ELEMENT_NORMAL)) else {
        return Ok(());
    };
    let float = |i: usize| arrays.get(i).is_some_and(|a| a.format == FORMAT_FLOAT);
    if !float(ti) || !float(ni) {
        return Ok(());
    }
    let bad = MshError::Unsupported(Reason::TangentFix);
    let pos = find(ELEMENT_POSITION)
        .and_then(|i| arrays.get(i))
        .ok_or(bad)?;
    let vtx = match pos.elem_num {
        0 => return Err(bad),
        e if e < 0 => 0,
        e => pos.count.checked_div(e as usize).ok_or(bad)?,
    };
    let stride = |i: usize| usize::try_from(arrays.get(i).map_or(0, |a| a.elem_num)).unwrap_or(0);
    let (ts, ns) = (stride(ti), stride(ni));
    if vtx > 0 && (ts < 3 || ns < 3) {
        return Err(bad);
    }
    let normals = arrays.get(ni).map(|a| a.data.clone()).ok_or(bad)?;
    let tan = arrays.get_mut(ti).ok_or(bad)?;
    for i in 0..vtx {
        let tb = i.checked_mul(ts).ok_or(bad)?;
        let nb = i.checked_mul(ns).ok_or(bad)?;
        let mut vtan = vec3_at(&tan.data, tb).ok_or(bad)?;
        let vnrm = vec3_at(&normals, nb).ok_or(bad)?;
        let cross = cross3(vtan, vnrm);
        if dot3(cross, cross).partial_cmp(&1e-12f32) != Some(Ordering::Greater) {
            let axis = if vnrm.x.abs() < 0.9f32 {
                v3(1.0, 0.0, 0.0)
            } else {
                v3(0.0, 1.0, 0.0)
            };
            vtan = cross3(vnrm, axis);
            normalize3(&mut vtan);
            set_f32(&mut tan.data, tb, vtan.x).ok_or(bad)?;
            set_f32(&mut tan.data, tb + 1, vtan.y).ok_or(bad)?;
            set_f32(&mut tan.data, tb + 2, vtan.z).ok_or(bad)?;
        }
        if ts > 3 && f32_at(&tan.data, tb + 3).ok_or(bad)? == 0.0 {
            set_f32(&mut tan.data, tb + 3, 1.0).ok_or(bad)?;
        }
    }
    Ok(())
}

fn read_indices(r: &mut Reader, hpl3: bool) -> Res<Vec<u32>> {
    let mut indices = Vec::new();
    let lods = if hpl3 { r.i32() } else { 1 };
    let mut lod = 0;
    // At EOF every read is zero and consumes nothing, so the remaining LODs change nothing.
    while lod < lods && !r.at_eof() {
        let n = r.i32();
        if hpl3 {
            r.f32();
        }
        // only LOD 0 is read; the others leave their index bytes in the stream
        if lod == 0 {
            let n = usize::try_from(n).map_err(|_| MshError::Unsupported(Reason::NegativeSize))?;
            let bytes = n
                .checked_mul(4)
                .ok_or(MshError::Unsupported(Reason::SizeRange))?;
            r.charge(bytes)?;
            let mut raw = vec![0; bytes];
            r.bb.get_array(&mut raw, 4);
            indices = raw
                .chunks_exact(4)
                .map(|c| u32::from_le_bytes(c.try_into().unwrap_or_default()))
                .collect();
        }
        lod += 1;
    }
    Ok(indices)
}

fn read_submesh(r: &mut Reader, hpl3: bool) -> Res<SubMesh> {
    r.charge(1024)?;
    let name = r.string();
    let material = r.string();
    let matrix = r.floats();
    let scale = r.floats();
    let collide_shape = r.bool();

    let mut colliders = Vec::new();
    for _ in 0..r.count()? {
        r.charge(128)?;
        let kind = r.i16();
        let matrix = r.floats();
        let size = r.floats();
        let char_collider = r.bool();
        colliders.push(Collider {
            kind,
            matrix,
            size,
            char_collider,
        });
    }

    let pairs = r.size()?;
    r.charge(pairs.saturating_mul(12))?;
    let mut bone_pairs = Vec::new();
    for _ in 0..pairs {
        let vtx = r.i32();
        let bone = r.i32();
        let weight = r.f32();
        bone_pairs.push(BonePair { vtx, bone, weight });
    }

    let vtx_num = r.i32();
    let mut arrays = Vec::new();
    let mut skipped = Vec::new();
    for _ in 0..r.count()? {
        read_array(r, hpl3, vtx_num, &mut arrays, &mut skipped)?;
    }
    let indices = read_indices(r, hpl3)?;
    fix_tangents(&mut arrays)?;
    Ok(SubMesh {
        name,
        material,
        matrix,
        scale,
        collide_shape,
        colliders,
        bone_pairs,
        arrays,
        skipped,
        indices,
    })
}

fn read_anim(r: &mut Reader, hpl3: bool) -> Res<Anim> {
    r.string();
    let length = r.f32();
    let tracks_n = r.size()?;
    let extra = if hpl3 { r.i32() } else { 0 };
    let mut tracks = Vec::new();
    for _ in 0..tracks_n {
        r.charge(64)?;
        let name = r.string();
        let flags = r.i16();
        let frames = r.count()?;
        r.charge(frames.saturating_mul(32))?;
        let mut keys = Vec::new();
        for _ in 0..frames {
            let time = r.f32();
            let trans = r.floats();
            let rot = r.floats();
            keys.push(Key { time, trans, rot });
        }
        tracks.push(Track { name, flags, keys });
    }
    let mut i = 0;
    while i < extra && !r.at_eof() {
        r.f32();
        r.floats::<6>();
        i += 1;
    }
    Ok(Anim { length, tracks })
}

fn header(r: &mut Reader) -> Res<i32> {
    if r.i32() != MAGIC {
        return Err(MshError::BadMagic);
    }
    Ok(r.i32())
}

/// `bone_unit_scale` is `mbBoneUnitScale`, `keep_hpl3_anims` is `mbKeepHpl3Animations`.
pub fn parse_mesh(data: &[u8], bone_unit_scale: bool, keep_hpl3_anims: bool) -> Res<Mesh> {
    let mut r = Reader::new(data);
    let version = header(&mut r)?;
    let hpl3 = version == VERSION_HPL3;
    if version != VERSION && !hpl3 {
        return Err(MshError::BadVersion);
    }
    if hpl3 {
        r.i32();
    }
    let submesh_num = r.count()?;
    r.i32();
    let bones = if r.bool() {
        Some(read_bones(&mut r, bone_unit_scale && !hpl3)?)
    } else {
        None
    };
    let nodes = read_nodes(&mut r)?;
    let mut submeshes = Vec::new();
    for _ in 0..submesh_num {
        submeshes.push(read_submesh(&mut r, hpl3)?);
    }
    let mut animations = Vec::new();
    let anim_num = r.i32();
    if !hpl3 || keep_hpl3_anims {
        for _ in 0..usize::try_from(anim_num).unwrap_or(0) {
            animations.push(read_anim(&mut r, hpl3)?);
            if animations.len() > MAX_ITEMS {
                return Err(MshError::Unsupported(Reason::Count));
            }
        }
    }
    Ok(Mesh {
        hpl3,
        bones,
        nodes,
        submeshes,
        animations,
    })
}

pub fn parse_animation(data: &[u8]) -> Res<Anim> {
    let mut r = Reader::new(data);
    if header(&mut r)? != VERSION {
        return Err(MshError::BadVersion);
    }
    read_anim(&mut r, false)
}
