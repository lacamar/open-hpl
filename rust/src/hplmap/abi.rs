//! C ABI of `HPL2/core/include/resources/HplMapBuilder.h`: mirrors of its structs, the builder and
//! element-tree adapters over its callback tables, and the exported entry points.

use super::cache::{read_cache, write_cache};
use super::parse::parse_map;
use super::{
    BodyOut, Builder, CacheArray, CacheBody, CacheMesh, CacheShape, CacheSource, CacheSummary,
    Decal, Entity, Fog, MeshOut, NodeKind, Options, Parsed, Phase, Plane, ShapeBody, StaticObject,
    Xml,
};
use crate::binary_buffer::BinaryBuffer;
use crate::types::{Color, Matrixf, Vector2f, Vector3f};
use std::ffi::{CStr, c_char, c_int, c_void};

type Node = *const c_void;
type NodeSink = extern "C" fn(*mut c_void, Node);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    p: *const u8,
    n: usize,
}

impl Str {
    fn new(s: &[u8]) -> Self {
        Self {
            p: s.as_ptr(),
            n: s.len(),
        }
    }

    /// # Safety
    /// `p` is valid for `n` bytes (or `n` is 0) for the returned lifetime.
    unsafe fn bytes<'a>(self) -> &'a [u8] {
        // SAFETY: caller contract.
        unsafe { slice(self.p, self.n) }
    }
}

/// # Safety
/// `p` is valid for `n` elements for the returned lifetime, or null / `n` is 0.
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

#[repr(C)]
pub struct XmlView {
    ctx: *mut c_void,
    first_child: extern "C" fn(*mut c_void, Node, Str) -> Node,
    children: extern "C" fn(*mut c_void, Node, NodeSink, *mut c_void),
    attr: extern "C" fn(*mut c_void, Node, Str) -> *const c_char,
    value: extern "C" fn(*mut c_void, Node) -> *const c_char,
}

#[repr(C)]
pub struct MapOptions {
    flags: u32,
    log_cache: bool,
    file: Str,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct RawShape {
    kind: c_int,
    size: Vector3f,
    offset: Matrixf,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct RawArray {
    kind: c_int,
    format: c_int,
    prog_var: c_int,
    elem_num: c_int,
    compression: c_int,
    data: *const c_void,
    count: usize,
}

#[repr(C)]
pub struct MapBuilder {
    ctx: *mut c_void,
    log: extern "C" fn(*mut c_void, Str),
    warning: extern "C" fn(*mut c_void, Str),
    error: extern "C" fn(*mut c_void, Str),
    phase: extern "C" fn(*mut c_void, c_int, bool),
    set_fog: extern "C" fn(*mut c_void, bool, Color, f32, f32, f32, bool),
    set_skybox: extern "C" fn(*mut c_void, bool, Color, Str),
    static_begin: extern "C" fn(*mut c_void),
    static_object:
        extern "C" fn(*mut c_void, Str, Str, Vector3f, Vector3f, Vector3f, bool, bool, c_int),
    static_plane: extern "C" fn(
        *mut c_void,
        Str,
        Str,
        bool,
        bool,
        c_int,
        Vector3f,
        Vector3f,
        Vector3f,
        Vector3f,
        Vector3f,
        *const Vector2f,
    ),
    static_decal: extern "C" fn(*mut c_void, Str, Color, c_int, Str, Node),
    static_loading_done: extern "C" fn(*mut c_void),
    static_combo: extern "C" fn(*mut c_void, c_int, *const c_int, usize),
    static_compile: extern "C" fn(*mut c_void),
    entities_begin: extern "C" fn(*mut c_void),
    entities_end: extern "C" fn(*mut c_void),
    entity: extern "C" fn(*mut c_void, Str, c_int, bool, Vector3f, Vector3f, Vector3f, Str, Node),
    area: extern "C" fn(*mut c_void, Str, c_int, bool, Vector3f, Vector3f, Vector3f, Str, Node),
    fog_area: extern "C" fn(*mut c_void, Node),
    particle_system: extern "C" fn(*mut c_void, Node),
    sound: extern "C" fn(*mut c_void, Node),
    billboard: extern "C" fn(*mut c_void, Node),
    light: extern "C" fn(*mut c_void, Node),
    cache_header: extern "C" fn(*mut c_void, c_int, c_int, c_int),
    cache_mesh_body: extern "C" fn(*mut c_void, Str, Str, bool, bool),
    cache_shape_body:
        extern "C" fn(*mut c_void, Str, *const Matrixf, bool, bool, *const RawShape, usize),
    cache_mesh: extern "C" fn(
        *mut c_void,
        Str,
        Str,
        bool,
        c_int,
        *const RawArray,
        usize,
        *const u32,
        usize,
    ),
}

#[repr(C)]
struct RawBodyOut {
    name: Str,
    material: Str,
    blocks_light: bool,
    char_collider: bool,
}

#[repr(C)]
struct RawShapeBodyOut {
    material: Str,
    transform: Matrixf,
    char_collider: bool,
    blocks_light: bool,
    colliders: *const RawShape,
    n_colliders: usize,
}

#[repr(C)]
struct RawMeshOut {
    name: Str,
    material: Str,
    cast_shadows: bool,
    vertex_num: c_int,
    arrays: *const RawArray,
    n_arrays: usize,
    indices: *const u32,
    n_indices: usize,
}

#[repr(C)]
pub struct MapCacheSource {
    ctx: *mut c_void,
    error: extern "C" fn(*mut c_void, Str),
    counts: extern "C" fn(*mut c_void, *mut c_int, *mut c_int, *mut c_int),
    next_body: extern "C" fn(*mut c_void, *mut RawBodyOut),
    write_body_shape: extern "C" fn(*mut c_void),
    next_shape_body: extern "C" fn(*mut c_void, *mut RawShapeBodyOut),
    next_mesh: extern "C" fn(*mut c_void, *mut RawMeshOut),
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Str>() == 16);
    assert!(size_of::<MapOptions>() == 24 && offset_of!(MapOptions, file) == 8);
    assert!(size_of::<RawShape>() == 80);
    assert!(size_of::<RawArray>() == 40 && offset_of!(RawArray, data) == 24);
    assert!(size_of::<RawBodyOut>() == 40);
    assert!(size_of::<RawShapeBodyOut>() == 16 + 64 + 8 + 16);
    assert!(size_of::<RawMeshOut>() == 16 + 16 + 8 + 8 + 8 + 8 + 8);
};

struct CXml<'a>(&'a XmlView);

fn name_str(name: &[u8]) -> Str {
    Str::new(name)
}

impl Xml for CXml<'_> {
    type Node = Node;

    fn first_child(&self, node: Node, name: &[u8]) -> Option<Node> {
        let child = (self.0.first_child)(self.0.ctx, node, name_str(name));
        (!child.is_null()).then_some(child)
    }

    fn children(&self, node: Node) -> Vec<Node> {
        extern "C" fn push(ctx: *mut c_void, node: Node) {
            // SAFETY: `ctx` is the `Vec` passed below, alive for the whole call.
            unsafe { &mut *ctx.cast::<Vec<Node>>() }.push(node);
        }
        let mut out = Vec::new();
        (self.0.children)(self.0.ctx, node, push, (&raw mut out).cast());
        out
    }

    fn attr(&self, node: Node, name: &[u8]) -> Option<&[u8]> {
        let p = (self.0.attr)(self.0.ctx, node, name_str(name));
        // SAFETY: a non-null result is a NUL-terminated string owned by the tree, which outlives `self`.
        (!p.is_null()).then(|| unsafe { CStr::from_ptr(p) }.to_bytes())
    }

    fn value(&self, node: Node) -> &[u8] {
        // SAFETY: the result is a NUL-terminated string owned by the tree, which outlives `self`.
        unsafe { CStr::from_ptr((self.0.value)(self.0.ctx, node)) }.to_bytes()
    }
}

fn opt(node: Option<Node>) -> Node {
    node.unwrap_or(std::ptr::null())
}

struct CBuilder<'a>(&'a MapBuilder);

impl Builder<Node> for CBuilder<'_> {
    fn log(&mut self, msg: &[u8]) {
        (self.0.log)(self.0.ctx, Str::new(msg));
    }

    fn warning(&mut self, msg: &[u8]) {
        (self.0.warning)(self.0.ctx, Str::new(msg));
    }

    fn error(&mut self, msg: &[u8]) {
        (self.0.error)(self.0.ctx, Str::new(msg));
    }

    fn phase(&mut self, phase: Phase, begin: bool) {
        (self.0.phase)(self.0.ctx, phase as c_int, begin);
    }

    fn set_fog(&mut self, fog: Fog) {
        (self.0.set_fog)(
            self.0.ctx,
            fog.active,
            fog.color,
            fog.falloff_exp,
            fog.start,
            fog.end,
            fog.culling,
        );
    }

    fn set_skybox(&mut self, active: bool, color: Color, texture: &[u8]) {
        (self.0.set_skybox)(self.0.ctx, active, color, Str::new(texture));
    }

    fn static_begin(&mut self) {
        (self.0.static_begin)(self.0.ctx);
    }

    fn static_object(&mut self, o: &StaticObject) {
        (self.0.static_object)(
            self.0.ctx,
            Str::new(o.name),
            Str::new(o.file),
            o.at.pos,
            o.at.rot,
            o.at.scale,
            o.collides,
            o.cast_shadows,
            o.id,
        );
    }

    fn static_plane(&mut self, p: &Plane) {
        (self.0.static_plane)(
            self.0.ctx,
            Str::new(p.name),
            Str::new(p.material),
            p.cast_shadows,
            p.collides,
            p.id,
            p.at.pos,
            p.at.scale,
            p.at.rot,
            p.start_corner,
            p.end_corner,
            p.uv.as_ptr(),
        );
    }

    fn static_decal(&mut self, d: &Decal<Node>) {
        (self.0.static_decal)(
            self.0.ctx,
            Str::new(d.name),
            d.color,
            d.id,
            Str::new(d.material),
            opt(d.mesh),
        );
    }

    fn static_loading_done(&mut self) {
        (self.0.static_loading_done)(self.0.ctx);
    }

    fn static_combo(&mut self, group_id: i32, ids: &[i32]) {
        (self.0.static_combo)(self.0.ctx, group_id, ids.as_ptr(), ids.len());
    }

    fn static_compile(&mut self) {
        (self.0.static_compile)(self.0.ctx);
    }

    fn entities_begin(&mut self) {
        (self.0.entities_begin)(self.0.ctx);
    }

    fn entities_end(&mut self) {
        (self.0.entities_end)(self.0.ctx);
    }

    fn entity(&mut self, e: &Entity<Node>) {
        (self.0.entity)(
            self.0.ctx,
            Str::new(e.name),
            e.id,
            e.active,
            e.at.pos,
            e.at.rot,
            e.at.scale,
            Str::new(e.file),
            opt(e.user_vars),
        );
    }

    fn area(&mut self, a: &Entity<Node>) {
        (self.0.area)(
            self.0.ctx,
            Str::new(a.name),
            a.id,
            a.active,
            a.at.pos,
            a.at.rot,
            a.at.scale,
            Str::new(a.file),
            opt(a.user_vars),
        );
    }

    fn node(&mut self, kind: NodeKind, node: Node) {
        let f = match kind {
            NodeKind::FogArea => self.0.fog_area,
            NodeKind::ParticleSystem => self.0.particle_system,
            NodeKind::Sound => self.0.sound,
            NodeKind::Billboard => self.0.billboard,
            NodeKind::Light => self.0.light,
        };
        f(self.0.ctx, node);
    }

    fn cache_header(&mut self, s: CacheSummary) {
        (self.0.cache_header)(self.0.ctx, s.mesh_bodies, s.shape_bodies, s.mesh_entities);
    }

    fn cache_mesh_body(&mut self, b: &CacheBody, _buffer: *mut BinaryBuffer) {
        (self.0.cache_mesh_body)(
            self.0.ctx,
            Str::new(b.name),
            Str::new(b.material),
            b.blocks_light,
            b.char_collider,
        );
    }

    fn cache_shape_body(&mut self, b: &ShapeBody) {
        let shapes: Vec<RawShape> = b
            .colliders
            .iter()
            .map(|s| RawShape {
                kind: s.kind,
                size: s.size,
                offset: s.offset,
            })
            .collect();
        (self.0.cache_shape_body)(
            self.0.ctx,
            Str::new(b.material),
            &raw const b.transform,
            b.char_collider,
            b.blocks_light,
            shapes.as_ptr(),
            shapes.len(),
        );
    }

    fn cache_mesh(&mut self, m: &CacheMesh) {
        let arrays: Vec<RawArray> = m
            .arrays
            .iter()
            .map(|a| RawArray {
                kind: a.kind,
                format: a.format,
                prog_var: a.prog_var,
                elem_num: a.elem_num,
                compression: a.compression,
                data: a.data.as_ptr().cast(),
                count: a.count,
            })
            .collect();
        (self.0.cache_mesh)(
            self.0.ctx,
            Str::new(m.name),
            Str::new(m.material),
            m.cast_shadows,
            m.vertex_num,
            arrays.as_ptr(),
            arrays.len(),
            m.indices.as_ptr(),
            m.indices.len(),
        );
    }
}

struct CSource<'a> {
    raw: &'a MapCacheSource,
    shapes: Vec<CacheShape>,
    arrays: Vec<CacheArray<'static>>,
}

fn data_width(array: &RawArray) -> usize {
    if super::cache::compression_of(array.kind) != 0 || array.format != 2 {
        4
    } else {
        1
    }
}

impl CacheSource for CSource<'_> {
    fn error(&mut self, msg: &[u8]) {
        (self.raw.error)(self.raw.ctx, Str::new(msg));
    }

    fn counts(&mut self) -> CacheSummary {
        let (mut bodies, mut shapes, mut meshes) = (0, 0, 0);
        (self.raw.counts)(self.raw.ctx, &mut bodies, &mut shapes, &mut meshes);
        CacheSummary {
            mesh_bodies: bodies,
            shape_bodies: shapes,
            mesh_entities: meshes,
        }
    }

    fn next_body(&mut self) -> BodyOut<'_> {
        let mut out = std::mem::MaybeUninit::<RawBodyOut>::uninit();
        (self.raw.next_body)(self.raw.ctx, out.as_mut_ptr());
        // SAFETY: the callback fills every field; strings stay valid until the next call.
        let out = unsafe { out.assume_init() };
        BodyOut {
            // SAFETY: as above.
            name: unsafe { out.name.bytes() },
            // SAFETY: as above.
            material: unsafe { out.material.bytes() },
            blocks_light: out.blocks_light,
            char_collider: out.char_collider,
        }
    }

    fn write_body_shape(&mut self) {
        (self.raw.write_body_shape)(self.raw.ctx);
    }

    fn next_shape_body(&mut self) -> ShapeBody<'_> {
        let mut out = std::mem::MaybeUninit::<RawShapeBodyOut>::uninit();
        (self.raw.next_shape_body)(self.raw.ctx, out.as_mut_ptr());
        // SAFETY: the callback fills every field; pointers stay valid until the next call.
        let out = unsafe { out.assume_init() };
        // SAFETY: as above.
        let raw = unsafe { slice(out.colliders, out.n_colliders) };
        self.shapes = raw
            .iter()
            .map(|s| CacheShape {
                kind: s.kind,
                size: s.size,
                offset: s.offset,
            })
            .collect();
        ShapeBody {
            // SAFETY: as above.
            material: unsafe { out.material.bytes() },
            transform: out.transform,
            char_collider: out.char_collider,
            blocks_light: out.blocks_light,
            colliders: &self.shapes,
        }
    }

    fn next_mesh(&mut self) -> MeshOut<'_> {
        let mut out = std::mem::MaybeUninit::<RawMeshOut>::uninit();
        (self.raw.next_mesh)(self.raw.ctx, out.as_mut_ptr());
        // SAFETY: the callback fills every field; pointers stay valid until the next call.
        let out = unsafe { out.assume_init() };
        // SAFETY: as above.
        let raw = unsafe { slice(out.arrays, out.n_arrays) };
        self.arrays = raw
            .iter()
            .map(|a| CacheArray {
                kind: a.kind,
                format: a.format,
                prog_var: a.prog_var,
                elem_num: a.elem_num,
                compression: a.compression,
                count: a.count,
                // SAFETY: the source sizes `data` to `count` elements of the width the writer reads.
                data: unsafe { slice(a.data.cast::<u8>(), a.count.saturating_mul(data_width(a))) },
            })
            .collect();
        MeshOut {
            // SAFETY: as above.
            name: unsafe { out.name.bytes() },
            // SAFETY: as above.
            material: unsafe { out.material.bytes() },
            cast_shadows: out.cast_shadows,
            vertex_num: out.vertex_num,
            arrays: &self.arrays,
            // SAFETY: as above.
            indices: unsafe { slice(out.indices, out.n_indices) },
        }
    }
}

/// # Safety
/// `view`, `builder` and `options` are live for the call; `doc` is a node of the view's tree;
/// every callback honours the header's contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_map_parse(
    view: *const XmlView,
    doc: Node,
    builder: *const MapBuilder,
    options: *const MapOptions,
    cache_loaded: bool,
) -> c_int {
    // SAFETY: caller contract.
    let (view, builder, options) = unsafe { (&*view, &*builder, &*options) };
    let options = Options {
        flags: options.flags,
        log_cache: options.log_cache,
        // SAFETY: caller contract.
        file: unsafe { options.file.bytes() },
    };
    match parse_map(
        &CXml(view),
        doc,
        &mut CBuilder(builder),
        &options,
        cache_loaded,
    ) {
        Parsed::Ok => 0,
        Parsed::NoContents => 1,
        Parsed::Abort => 2,
    }
}

/// # Safety
/// `buffer` is live; `builder` and `options` are live for the call; the builder's
/// `cache_mesh_body` reads the mesh shape from `buffer` itself.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_map_cache_read(
    buffer: *mut BinaryBuffer,
    builder: *const MapBuilder,
    options: *const MapOptions,
) -> bool {
    // SAFETY: caller contract.
    let (builder, options) = unsafe { (&*builder, &*options) };
    let options = Options {
        flags: options.flags,
        log_cache: options.log_cache,
        // SAFETY: caller contract.
        file: unsafe { options.file.bytes() },
    };
    // SAFETY: caller contract.
    unsafe { read_cache(buffer, &mut CBuilder(builder), &options) }
}

/// # Safety
/// `buffer` is live; `source` is live for the call and its `write_body_shape` appends to `buffer`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_map_cache_write(
    buffer: *mut BinaryBuffer,
    source: *const MapCacheSource,
) {
    // SAFETY: caller contract.
    let raw = unsafe { &*source };
    let mut source = CSource {
        raw,
        shapes: Vec::new(),
        arrays: Vec::new(),
    };
    // SAFETY: caller contract.
    unsafe { write_cache(buffer, &mut source) };
}
