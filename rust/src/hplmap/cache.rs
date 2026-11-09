//! The `.map_cache` binary: header, static bodies, shape bodies and combined meshes.

use super::parse::{message, note};
use super::{
    Builder, CacheArray, CacheBody, CacheMesh, CacheShape, CacheSource, CacheSummary, Options,
    ShapeBody,
};
use crate::binary_buffer::BinaryBuffer;
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_char;

const MAGIC: u32 = 0xF441_451F;
const VERSION: i32 = 13;
const ELEMENT_KINDS: i32 = 14;
const FORMAT_KINDS: i32 = 3;
const FORMAT_BYTE: i32 = 2;
const KIND_NORMAL: i32 = 0;
const KIND_COLOR0: i32 = 2;
const KIND_TANGENT: i32 = 4;

/// The buffer is shared with C++ code that moves its position during builder calls, so it is
/// reached through a pointer and borrowed only for the length of one operation.
struct Shared(*mut BinaryBuffer);

impl Shared {
    fn with<T>(&self, f: impl FnOnce(&mut BinaryBuffer) -> T) -> T {
        // SAFETY: the caller of `read_cache`/`write_cache` guarantees the buffer is live and is
        // not otherwise borrowed while this call runs; no reference outlives `f`.
        f(unsafe { &mut *self.0 })
    }

    fn i32(&self) -> i32 {
        self.with(BinaryBuffer::get_i32)
    }

    fn bool(&self) -> bool {
        self.array::<1>()[0] != 0
    }

    fn array<const N: usize>(&self) -> [u8; N] {
        let mut out = [0; N];
        self.with(|b| b.get_array(&mut out, 1));
        out
    }

    fn i16(&self) -> i16 {
        let mut out = [0; 2];
        self.with(|b| b.get_array(&mut out, 2));
        i16::from_ne_bytes(out)
    }

    fn string(&self) -> Vec<u8> {
        self.with(|b| b.get_string().to_vec())
    }

    fn floats(&self, out: &mut [f32]) {
        let mut raw = vec![0u8; out.len() * 4];
        self.with(|b| b.get_array(&mut raw, 4));
        for (f, c) in out.iter_mut().zip(raw.as_chunks::<4>().0) {
            *f = f32::from_ne_bytes(*c);
        }
    }

    fn vec3(&self) -> Vector3f {
        let mut v = [0.0; 3];
        self.floats(&mut v);
        Vector3f {
            x: v[0],
            y: v[1],
            z: v[2],
        }
    }

    fn matrix(&self) -> Matrixf {
        let mut v = [0.0; 16];
        self.floats(&mut v);
        let mut m = Matrixf::default();
        for (row, chunk) in m.m.iter_mut().zip(v.chunks_exact(4)) {
            row.copy_from_slice(chunk);
        }
        m
    }
}

/// `0 - 255 -> 0 - 1`
fn remaining(shared: &Shared) -> i64 {
    shared.with(|b| i64::try_from(b.size().saturating_sub(b.pos())).unwrap_or(i64::MAX))
}

fn byte_pos(x: u8) -> f32 {
    f32::from(x) / 255.0
}

/// `char` is the C type of the target, so on aarch64 Linux it is unsigned and negatives are lost.
fn byte_neg_pos(x: u8) -> f32 {
    f32::from(x as c_char) / 127.0
}

fn short_neg_pos(x: u16) -> f32 {
    f32::from(x as i16) / 32767.0
}

fn decode(shared: &Shared, array: &mut CacheArrayBuf) {
    let count = array.count;
    if array.compression == 0 {
        let width = if array.format == FORMAT_BYTE { 1 } else { 4 };
        array.data = vec![0; count * width];
        if width == 1 {
            let _ = shared.with(|b| b.get_data(&mut array.data));
        } else {
            shared.with(|b| b.get_array(&mut array.data, 4));
        }
        return;
    }
    let mut data = Vec::with_capacity(count * 4);
    match array.compression {
        c if c <= 2 => {
            let mut raw = vec![0; count];
            shared.with(|b| b.get_array(&mut raw, 1));
            let table = if array.compression == 1 {
                byte_pos
            } else {
                byte_neg_pos
            };
            raw.iter()
                .for_each(|&x| data.extend_from_slice(&table(x).to_ne_bytes()));
        }
        _ => {
            let mut raw = vec![0; count * 2];
            shared.with(|b| b.get_array(&mut raw, 2));
            raw.as_chunks::<2>().0.iter().for_each(|&c| {
                data.extend_from_slice(&short_neg_pos(u16::from_ne_bytes(c)).to_ne_bytes());
            });
        }
    }
    array.data = data;
}

struct CacheArrayBuf {
    kind: i32,
    format: i32,
    prog_var: i32,
    elem_num: i32,
    compression: i32,
    count: usize,
    data: Vec<u8>,
}

/// Reads a whole cache. False when the header was rejected before anything was built.
///
/// # Safety
/// `buffer` is live, not otherwise borrowed during the call except by the builder's own
/// positional reads of the mesh shapes.
pub unsafe fn read_cache<N: Copy>(
    buffer: *mut BinaryBuffer,
    builder: &mut impl Builder<N>,
    options: &Options,
) -> bool {
    let shared = Shared(buffer);
    let file = options.file;

    let magic = shared.i32();
    let version = shared.i32();

    if magic as u32 != MAGIC {
        builder.error(&note(&[b"File '", file, format!("' does not have right MAP_CACHE magic number ({:X} instead of {MAGIC:X})! Invalid header!\n",
                magic as u32).as_bytes()]));
        return false;
    }
    if version != VERSION {
        builder.error(&note(&[
            b"File '",
            file,
            format!("' does not have right MAP_CACHE version! Is {version}, newest is {VERSION}\n")
                .as_bytes(),
        ]));
        return false;
    }
    let expected = shared.i32();
    let size = shared.with(|b| b.size());
    if expected as usize != size {
        builder.error(&note(&[b"File '", file, format!("' is truncated or corrupt (expected {expected} bytes, file is {})! Rebuilding from source instead.\n",
                size as i32).as_bytes()]));
        return false;
    }

    let summary = CacheSummary {
        mesh_bodies: shared.i32(),
        shape_bodies: shared.i32(),
        mesh_entities: shared.i32(),
    };
    let limit = i64::try_from(size).unwrap_or(i64::MAX);
    if [
        summary.mesh_bodies,
        summary.shape_bodies,
        summary.mesh_entities,
    ]
    .iter()
    .any(|&n| !(0..=limit).contains(&i64::from(n)))
    {
        builder.error(&note(&[
            b"Map cache of '",
            file,
            format!(
                "' has invalid counts ({}, {}, {})! Rebuilding from source instead.\n",
                summary.mesh_bodies, summary.shape_bodies, summary.mesh_entities
            )
            .as_bytes(),
        ]));
        return false;
    }
    builder.cache_header(summary);

    for _ in 0..summary.mesh_bodies {
        let name = shared.string();
        let material = shared.string();
        let blocks_light = shared.bool();
        let char_collider = shared.bool();
        builder.cache_mesh_body(
            &CacheBody {
                name: &name,
                material: &material,
                blocks_light,
                char_collider,
            },
            buffer,
        );
    }

    for _ in 0..summary.shape_bodies {
        let material = shared.string();
        let transform = shared.matrix();
        let char_collider = shared.bool();
        let blocks_light = shared.bool();
        let n = shared.i32();
        if !(0..=limit).contains(&i64::from(n)) {
            builder.error(&message(
                format!("Shape body with {n} colliders is out of range in map cache!\n")
                    .into_bytes(),
            ));
            return true;
        }
        let colliders: Vec<CacheShape> = (0..n)
            .map(|_| CacheShape {
                kind: shared.i32(),
                size: shared.vec3(),
                offset: shared.matrix(),
            })
            .collect();
        builder.cache_shape_body(&ShapeBody {
            material: &material,
            transform,
            char_collider,
            blocks_light,
            colliders: &colliders,
        });
    }

    for mesh in 0..summary.mesh_entities {
        if !read_mesh(&shared, builder, options, mesh) {
            return true;
        }
    }
    true
}

fn read_mesh<N: Copy>(
    shared: &Shared,
    builder: &mut impl Builder<N>,
    options: &Options,
    mesh: i32,
) -> bool {
    let name = shared.string();
    let material = shared.string();
    let cast_shadows = shared.bool();

    if options.log_cache {
        builder.log(&note(&[
            format!("Mesh {mesh}: '").as_bytes(),
            &name,
            b"' '",
            &material,
            b"'
",
        ]));
    }

    let vertex_num = shared.i32();
    let type_num = shared.i32();
    if options.log_cache {
        builder.log(&message(
            format!(" VertexBuffers num: {vertex_num} typenum: {type_num}\n").into_bytes(),
        ));
    }

    if type_num < 0 || i64::from(type_num) * 20 > remaining(shared) {
        builder.error(&message(
            format!("Vertex type count {type_num} is out of range in map cache!\n").into_bytes(),
        ));
        return false;
    }

    let mut arrays = Vec::new();
    for i in 0..type_num {
        let mut array = CacheArrayBuf {
            kind: i32::from(shared.i16()),
            format: i32::from(shared.i16()),
            prog_var: shared.i32(),
            elem_num: shared.i32(),
            compression: shared.i32(),
            count: 0,
            data: Vec::new(),
        };
        if options.log_cache {
            builder.log(&message(
                format!(
                    "   Vtx {i}: {} {} {}\n",
                    array.kind, array.prog_var, array.elem_num
                )
                .into_bytes(),
            ));
        }

        let count = i64::from(vertex_num) * i64::from(array.elem_num);
        if !(0..=remaining(shared)).contains(&count) {
            builder.error(&message(
                format!(
                    "Vertex array of {vertex_num} x {} elements is out of range in map cache!\n",
                    array.elem_num
                )
                .into_bytes(),
            ));
            return false;
        }
        array.count = count as usize;

        if !(0..ELEMENT_KINDS).contains(&array.kind) || !(0..FORMAT_KINDS).contains(&array.format) {
            builder.error(
                b"Vertex buffer has incorrect format when getting vertexbuffer data during loading of MSH file!\n",
            );
            continue;
        }
        decode(shared, &mut array);
        arrays.push(array);
    }

    let index_num = shared.i32();
    if options.log_cache {
        builder.log(&message(format!("Indices: {index_num}\n").into_bytes()));
    }
    if index_num < 0 || i64::from(index_num) * 4 > remaining(shared) {
        builder.error(&message(
            format!("Index count {index_num} is out of range in map cache!\n").into_bytes(),
        ));
        return false;
    }
    let mut raw = vec![0u8; index_num as usize * 4];
    shared.with(|b| b.get_array(&mut raw, 4));
    let indices: Vec<u32> = raw
        .as_chunks::<4>()
        .0
        .iter()
        .map(|&c| u32::from_ne_bytes(c))
        .collect();

    let views: Vec<CacheArray> = arrays
        .iter()
        .map(|a| CacheArray {
            kind: a.kind,
            format: a.format,
            prog_var: a.prog_var,
            elem_num: a.elem_num,
            compression: a.compression,
            count: a.count,
            data: &a.data,
        })
        .collect();
    builder.cache_mesh(&CacheMesh {
        name: &name,
        material: &material,
        cast_shadows,
        vertex_num,
        arrays: &views,
        indices: &indices,
    });
    true
}

/// `FastPositiveFloatToInt`: the float magic-number trick, exact for the values written here.
pub(super) fn fast_positive(x: f32) -> i32 {
    let bias = f32::from_bits((23 + 127) << 23);
    ((x + bias).to_bits() as i32).wrapping_sub(bias.to_bits() as i32)
}

pub(super) fn fast_negative(x: f32) -> i32 {
    let bias = f32::from_bits(((23 + 127) << 23) + (1 << 22));
    ((x + bias).to_bits() as i32).wrapping_sub(bias.to_bits() as i32)
}

pub(super) fn fast_pos_and_neg(x: f32) -> i32 {
    if x >= 0.0 {
        fast_positive(x)
    } else {
        fast_negative(x)
    }
}

pub(super) fn compression_of(kind: i32) -> i32 {
    match kind {
        KIND_COLOR0 => 1,
        KIND_NORMAL | KIND_TANGENT => 2,
        _ => 0,
    }
}

/// Appends a whole cache, with the total size patched in.
///
/// # Safety
/// `buffer` is live and not otherwise borrowed during the call except by `source` appending
/// the mesh shapes in `write_body_shape`.
pub unsafe fn write_cache(buffer: *mut BinaryBuffer, source: &mut impl CacheSource) {
    let shared = Shared(buffer);
    let add_string = |s: &[u8]| {
        let mut bytes = s.to_vec();
        bytes.push(0);
        shared.with(|b| b.add_data(&bytes));
    };
    let add_i32 = |v: i32| shared.with(|b| b.add_i32(v));
    let add_bool = |v: bool| shared.with(|b| b.add_array(&[u8::from(v)], 1, false));
    let add_f32s = |v: &[f32]| {
        let bytes: Vec<u8> = v.iter().flat_map(|f| f.to_ne_bytes()).collect();
        shared.with(|b| b.add_array(&bytes, 4, true));
    };
    let add_matrix = |m: &Matrixf| add_f32s(&m.m.concat());

    add_i32(MAGIC as i32);
    add_i32(VERSION);
    let total_pos = shared.with(|b| b.pos());
    add_i32(0);

    let counts = source.counts();
    add_i32(counts.mesh_bodies);
    add_i32(counts.shape_bodies);
    add_i32(counts.mesh_entities);

    for _ in 0..counts.mesh_bodies {
        let body = source.next_body();
        add_string(body.name);
        add_string(body.material);
        add_bool(body.blocks_light);
        add_bool(body.char_collider);
        source.write_body_shape();
    }

    for _ in 0..counts.shape_bodies {
        let body = source.next_shape_body();
        add_string(body.material);
        add_matrix(&body.transform);
        add_bool(body.char_collider);
        add_bool(body.blocks_light);
        add_i32(body.colliders.len() as i32);
        for shape in body.colliders {
            add_i32(shape.kind);
            add_f32s(&[shape.size.x, shape.size.y, shape.size.z]);
            add_matrix(&shape.offset);
        }
    }

    for _ in 0..counts.mesh_entities {
        let mesh = source.next_mesh();
        add_string(mesh.name);
        add_string(mesh.material);
        add_bool(mesh.cast_shadows);
        add_i32(mesh.vertex_num);
        add_i32(mesh.arrays.len() as i32);

        let mut errors: Vec<&[u8]> = Vec::new();
        for array in mesh.arrays {
            let count = mesh.vertex_num.wrapping_mul(array.elem_num) as usize;
            shared.with(|b| {
                b.add_array(&(array.kind as i16).to_ne_bytes(), 2, true);
                b.add_array(&(array.format as i16).to_ne_bytes(), 2, true);
            });
            add_i32(array.prog_var);
            add_i32(array.elem_num);
            let compression = compression_of(array.kind);
            add_i32(compression);

            if compression == 0 {
                match array.format {
                    0 | 1 => write_elements(&shared, array.data, count, 4),
                    FORMAT_BYTE => write_elements(&shared, array.data, count, 1),
                    _ => {
                        errors.push(b"Vertex buffer has incorrect format when getting vertexbuffer data during loading of MSH file!\n");
                        errors.push(b"Vertex buffer has incorrect format when adding binary data during loading of MSH file!\n");
                    }
                }
            } else {
                write_compressed(&shared, array.data, count, compression);
            }
        }
        add_i32(mesh.indices.len() as i32);
        let bytes: Vec<u8> = mesh.indices.iter().flat_map(|i| i.to_ne_bytes()).collect();
        shared.with(|b| b.add_array(&bytes, 4, true));
        for e in errors {
            source.error(e);
        }
    }

    shared.with(|b| {
        let pos = b.pos() as i32;
        b.set_i32(pos, total_pos);
    });
}

fn write_elements(shared: &Shared, data: &[u8], count: usize, width: usize) {
    let Some(data) = data.get(..count * width) else {
        return;
    };
    shared.with(|b| b.add_array(data, width, width != 1));
}

fn write_compressed(shared: &Shared, data: &[u8], count: usize, compression: i32) {
    let Some(data) = data.get(..count * 4) else {
        return;
    };
    for &c in data.as_chunks::<4>().0 {
        let x = f32::from_ne_bytes(c);
        let byte = if compression == 1 {
            fast_positive(x * 255.0) as u8
        } else {
            fast_pos_and_neg(x * 127.0) as u8
        };
        shared.with(|b| b.add_array(&[byte], 1, false));
    }
}
