//! C ABI for the Collada extractor, mirrored in
//! `HPL2/core/include/impl/RustMeshLoadCollada.h`. The result is one little-endian byte stream
//! the C++ side decodes into its `cCollada*` structures.

use super::collada::{
    Animation, Bytes, Collada, Controller, Fail, Geometry, Light, Node, Opts, ParseError, parse,
};
use std::slice;

pub struct Blob(Vec<u8>);

#[derive(Default)]
struct W(Vec<u8>);

impl W {
    fn u8(&mut self, v: u8) {
        self.0.push(v);
    }
    fn u32(&mut self, v: usize) {
        self.0.extend((v as u32).to_le_bytes());
    }
    fn i32(&mut self, v: i32) {
        self.0.extend(v.to_le_bytes());
    }
    fn f32(&mut self, v: f32) {
        self.0.extend(v.to_le_bytes());
    }
    fn f32s(&mut self, v: &[f32]) {
        v.iter().for_each(|&x| self.f32(x));
    }
    fn str(&mut self, s: &[u8]) {
        self.u32(s.len());
        self.0.extend(s);
    }
    fn list<T>(&mut self, v: &[T], f: impl Fn(&mut Self, &T)) {
        self.u32(v.len());
        v.iter().for_each(|x| f(self, x));
    }
}

fn light(w: &mut W, l: &Light) {
    w.str(&l.id);
    w.str(&l.name);
    w.str(&l.kind);
    w.f32s(&l.color);
    w.f32(l.angle);
    w.u8(l.angle_uninit.into());
}

fn geometry(w: &mut W, g: &Geometry) {
    w.str(&g.id);
    w.str(&g.name);
    w.str(&g.material);
    w.list(&g.arrays, |w, a| {
        w.str(&a.id);
        w.str(&a.kind);
        w.u8(a.in_vertex.into());
        w.list(&a.data, |w, v| w.f32s(v));
    });
    for v in [
        g.pos_idx_num,
        g.norm_idx_num,
        g.tex_idx_num,
        g.pos_array_idx,
        g.norm_array_idx,
        g.tex_array_idx,
    ] {
        w.i32(v);
    }
    w.list(&g.vertices, |w, v| {
        w.f32s(&v.pos);
        w.f32s(&v.tex);
        w.f32s(&v.norm);
        w.f32s(&v.col);
    });
    w.list(&g.indices, |w, &i| w.0.extend(i.to_le_bytes()));
    w.list(&g.tangents, |w, &f| w.f32(f));
}

fn controller(w: &mut W, c: &Controller) {
    w.str(&c.target);
    w.str(&c.id);
    w.u8(c.bind.is_some().into());
    w.f32s(&c.bind.unwrap_or_default());
    w.list(&c.joints, |w, j| w.str(j));
    w.list(&c.weights, |w, &f| w.f32(f));
    w.list(&c.matrices, |w, m| w.f32s(m));
    w.list(&c.pairs, |w, p| {
        w.list(p, |w, &(j, k)| {
            w.i32(j);
            w.i32(k);
        });
    });
}

fn animation(w: &mut W, a: &Animation) {
    w.str(&a.id);
    w.str(&a.target_node);
    w.list(&a.channels, |w, c| {
        w.str(&c.id);
        w.str(&c.target);
        w.str(&c.source);
    });
    w.list(&a.samplers, |w, s| {
        w.str(&s.id);
        w.str(&s.time_array);
        w.str(&s.value_array);
        w.str(&s.target);
    });
    w.list(&a.sources, |w, s| {
        w.str(&s.id);
        w.list(&s.values, |w, &f| w.f32(f));
    });
}

fn node(w: &mut W, n: &Node) {
    for s in [
        &n.id,
        &n.name,
        &n.sid,
        &n.kind,
        &n.instance_material,
        &n.source,
    ] {
        w.str(s);
    }
    w.u8(n.source_is_file.into());
    w.u8(n.source_is_light.into());
    w.f32s(&super::collada::flat16(&n.mtx));
    w.f32s(&super::collada::flat16(&n.mtx_world));
    w.f32s(&n.scale);
    w.i32(n.parent);
    w.list(&n.transforms, |w, t| {
        w.str(&t.sid);
        w.str(&t.kind);
        w.list(&t.values, |w, &f| w.f32(f));
    });
}

fn encode(c: &Collada) -> Vec<u8> {
    let mut w = W::default();
    w.u8(c.z_to_y.into());
    w.f32(c.unit_scale);
    w.list(&c.images, |w, i| {
        w.str(&i.id);
        w.str(&i.name);
        w.str(&i.source);
    });
    w.list(&c.textures, |w, t| {
        w.str(&t.id);
        w.str(&t.image);
    });
    w.list(&c.materials, |w, m| {
        w.str(&m.id);
        w.str(&m.name);
        w.str(&m.texture);
    });
    w.list(&c.lights, light);
    w.list(&c.geometries, geometry);
    w.list(&c.controllers, controller);
    w.list(&c.animations, animation);
    w.f32(c.scene.start);
    w.f32(c.scene.end);
    w.f32(c.scene.delta);
    w.list(&c.scene.nodes, node);
    w.list(&c.log, |w, (level, msg)| {
        w.u8(*level);
        w.str(msg);
    });
    w.0
}

pub const OK: i32 = 0;
pub const XML_ERROR: i32 = 1;
pub const FAILED: i32 = 2;

pub const F_IMAGES: u32 = 1;
pub const F_TEXTURES: u32 = 2;
pub const F_MATERIALS: u32 = 4;
pub const F_LIGHTS: u32 = 8;
pub const F_GEOMETRY: u32 = 16;
pub const F_CONTROLLERS: u32 = 32;
pub const F_ANIMS: u32 = 64;
pub const F_SCENE: u32 = 128;
pub const F_CONVERT_UNIT: u32 = 256;
pub const F_CONVERT_UNIT_ANY: u32 = 512;

pub fn opts(flags: u32) -> Opts {
    let f = |b| flags & b != 0;
    Opts {
        convert_unit: f(F_CONVERT_UNIT),
        convert_unit_any: f(F_CONVERT_UNIT_ANY),
        images: f(F_IMAGES),
        textures: f(F_TEXTURES),
        materials: f(F_MATERIALS),
        lights: f(F_LIGHTS),
        geometry: f(F_GEOMETRY),
        controllers: f(F_CONTROLLERS),
        anims: f(F_ANIMS),
        scene: f(F_SCENE),
    }
}

/// `blob` receives the encoded result (`OK`), or the failure reason text (`FAILED`).
///
/// # Safety
/// `data` is valid for `len` bytes; `blob` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_collada_parse(
    data: *const u8,
    len: usize,
    read_ok: bool,
    flags: u32,
    blob: *mut *mut Blob,
) -> i32 {
    let data = if data.is_null() || len == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { slice::from_raw_parts(data, len) }
    };
    let (code, bytes): (i32, Bytes) = match parse(data, read_ok, opts(flags)) {
        Ok(c) => (OK, encode(&c)),
        Err(ParseError::Xml) => (XML_ERROR, Bytes::new()),
        Err(ParseError::Fail(Fail(why))) => (FAILED, why.as_bytes().to_vec()),
    };
    // SAFETY: caller contract.
    unsafe { *blob = Box::into_raw(Box::new(Blob(bytes))) };
    code
}

/// # Safety
/// `blob` came from `ohpl_collada_parse` and is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_collada_blob_data(blob: *const Blob) -> *const u8 {
    // SAFETY: caller contract.
    unsafe { (*blob).0.as_ptr() }
}

/// # Safety
/// `blob` came from `ohpl_collada_parse` and is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_collada_blob_len(blob: *const Blob) -> usize {
    // SAFETY: caller contract.
    unsafe { (*blob).0.len() }
}

/// # Safety
/// `blob` came from `ohpl_collada_parse`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_collada_free(blob: *mut Blob) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(blob) });
}
