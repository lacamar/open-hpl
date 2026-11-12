//! `cMeshLoaderAssimp`: the scene comes from `cshim/assimp_shim.cpp` through a sink; Rust decides
//! what becomes bones, sub meshes, bone pairs and materials and drives the builder.

use super::abi::{self, Array, Bone, Cfg, Pair, Sink, VbCfg};
use super::msh::text;
use crate::meshload::Key;
use crate::string;
use std::collections::{BTreeMap, HashMap};
use std::ffi::OsStr;
use std::ffi::{CString, c_char, c_void};
use std::os::unix::ffi::OsStrExt;

type Mat = [f32; 16];

const IDENTITY: Mat = [
    1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
];

const PRIMITIVE_TRIANGLE: u32 = 0x4;
const PRIMITIVE_NGON_FLAG: u32 = 0x10;

const ELEMENT_NORMAL: i32 = 0;
const ELEMENT_POSITION: i32 = 1;
const ELEMENT_COLOR0: i32 = 2;
const ELEMENT_TANGENT: i32 = 4;
const ELEMENT_TEXTURE0: i32 = 5;

#[repr(C)]
#[derive(Clone, Copy)]
struct AiWeight {
    vertex: u32,
    weight: f32,
}

#[repr(C)]
struct AiBone {
    name: *const c_char,
    name_len: usize,
    offset: Mat,
    weight_num: u32,
    weights: *const AiWeight,
}

#[repr(C)]
struct AiMesh {
    primitive_types: u32,
    vertex_num: u32,
    vertices: *const f32,
    normals: *const f32,
    uv0: *const f32,
    tangents: *const f32,
    bitangents: *const f32,
    face_num: u32,
    indices: *const u32,
    material: u32,
    bone_num: u32,
    bones: *const AiBone,
}

#[repr(C)]
struct AiSink {
    ctx: *mut c_void,
    scene: extern "C" fn(*mut c_void, f32),
    node: extern "C" fn(*mut c_void, i32, *const c_char, usize, *const f32, *const u32, usize),
    mesh: extern "C" fn(*mut c_void, *const AiMesh),
    material: extern "C" fn(*mut c_void, *const c_char, usize, *const c_char, usize),
}

unsafe extern "C" {
    fn ohpl_ai_read(
        path: *const c_char,
        mode: i32,
        sink: *const AiSink,
        err: *mut c_char,
        err_len: usize,
    ) -> i32;
    fn ohpl_ai_mul(a: *const f32, b: *const f32, out: *mut f32);
    fn ohpl_ai_inverse(a: *const f32, out: *mut f32);
    fn ohpl_ai_rigid(a: *const f32, out: *mut f32);
    fn ohpl_ai_scale_x(a: *const f32) -> f32;
    fn ohpl_ai_vertices(
        world: *const f32,
        vertex_num: u32,
        vertices: *const f32,
        normals: *const f32,
        uv0: *const f32,
        tangents: *const f32,
        bitangents: *const f32,
        out_pos: *mut f32,
        out_nrm: *mut f32,
        out_uv: *mut f32,
        out_col: *mut f32,
        out_tan: *mut f32,
    );
}

fn mul(a: &Mat, b: &Mat) -> Mat {
    let mut out = [0.0; 16];
    // SAFETY: 16-float arrays; the shim only reads `a` and `b` and writes `out`.
    unsafe { ohpl_ai_mul(a.as_ptr(), b.as_ptr(), out.as_mut_ptr()) };
    out
}

fn inverse(a: &Mat) -> Mat {
    let mut out = [0.0; 16];
    // SAFETY: as `mul`.
    unsafe { ohpl_ai_inverse(a.as_ptr(), out.as_mut_ptr()) };
    out
}

fn rigid(a: &Mat) -> Mat {
    let mut out = [0.0; 16];
    // SAFETY: as `mul`.
    unsafe { ohpl_ai_rigid(a.as_ptr(), out.as_mut_ptr()) };
    out
}

fn scale_x(a: &Mat) -> f32 {
    // SAFETY: as `mul`.
    unsafe { ohpl_ai_scale_x(a.as_ptr()) }
}

struct Node {
    name: Vec<u8>,
    m: Mat,
    meshes: Vec<u32>,
    children: Vec<usize>,
}

struct SrcBone {
    name: Vec<u8>,
    offset: Mat,
    weights: Vec<(u32, f32)>,
}

struct SrcMesh {
    primitive_types: u32,
    vertex_num: usize,
    vertices: Vec<f32>,
    normals: Option<Vec<f32>>,
    uv0: Option<Vec<f32>>,
    tangents: Option<Vec<f32>>,
    bitangents: Option<Vec<f32>>,
    indices: Vec<u32>,
    material: usize,
    bones: Vec<SrcBone>,
}

struct SrcMaterial {
    texture: Option<Vec<u8>>,
    name: Option<Vec<u8>>,
}

#[derive(Default)]
struct Scene {
    unit_scale: f32,
    nodes: Vec<Node>,
    meshes: Vec<SrcMesh>,
    materials: Vec<SrcMaterial>,
}

/// # Safety
/// `p` is valid for `n` elements, or null.
unsafe fn copy<T: Copy>(p: *const T, n: usize) -> Vec<T> {
    if p.is_null() || n == 0 {
        Vec::new()
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    }
}

/// # Safety
/// `p` is valid for `n` bytes, or null.
unsafe fn cstr(p: *const c_char, n: usize) -> Vec<u8> {
    // SAFETY: caller contract.
    unsafe { copy(p.cast::<u8>(), n) }
}

fn opt<T: Copy>(p: *const T, n: usize) -> Option<Vec<T>> {
    // SAFETY: the shim passes null or `n` elements.
    (!p.is_null()).then(|| unsafe { copy(p, n) })
}

extern "C" fn on_scene(ctx: *mut c_void, unit: f32) {
    // SAFETY: `ctx` is the `Scene` `read` passes.
    let s = unsafe { &mut *ctx.cast::<Scene>() };
    s.unit_scale = unit;
}

extern "C" fn on_node(
    ctx: *mut c_void,
    parent: i32,
    name: *const c_char,
    name_len: usize,
    m: *const f32,
    meshes: *const u32,
    mesh_num: usize,
) {
    // SAFETY: `ctx` is the `Scene`; the shim passes a 16-float matrix and valid arrays.
    let (s, name, m, meshes) = unsafe {
        (
            &mut *ctx.cast::<Scene>(),
            cstr(name, name_len),
            copy(m, 16),
            copy(meshes, mesh_num),
        )
    };
    let mut mat = IDENTITY;
    mat.iter_mut().zip(&m).for_each(|(d, s)| *d = *s);
    let at = s.nodes.len();
    if let Ok(p) = usize::try_from(parent)
        && let Some(n) = s.nodes.get_mut(p)
    {
        n.children.push(at);
    }
    s.nodes.push(Node {
        name,
        m: mat,
        meshes,
        children: Vec::new(),
    });
}

extern "C" fn on_mesh(ctx: *mut c_void, mesh: *const AiMesh) {
    // SAFETY: `ctx` is the `Scene`; `mesh` and its arrays are valid for the callback.
    let (s, m) = unsafe { (&mut *ctx.cast::<Scene>(), &*mesh) };
    let n = m.vertex_num as usize;
    // SAFETY: the shim passes `bone_num` bones, each with `weight_num` weights.
    let bones = unsafe { std::slice::from_raw_parts(m.bones, m.bone_num as usize) }
        .iter()
        .map(|b| SrcBone {
            // SAFETY: as above.
            name: unsafe { cstr(b.name, b.name_len) },
            offset: b.offset,
            weights: unsafe { copy(b.weights, b.weight_num as usize) }
                .iter()
                .map(|w: &AiWeight| (w.vertex, w.weight))
                .collect(),
        })
        .collect();
    s.meshes.push(SrcMesh {
        primitive_types: m.primitive_types,
        vertex_num: n,
        // SAFETY: arrays of 3 floats per vertex, or null.
        vertices: unsafe { copy(m.vertices, n * 3) },
        normals: opt(m.normals, n * 3),
        uv0: opt(m.uv0, n * 3),
        tangents: opt(m.tangents, n * 3),
        bitangents: opt(m.bitangents, n * 3),
        // SAFETY: 3 indices per face.
        indices: unsafe { copy(m.indices, m.face_num as usize * 3) },
        material: m.material as usize,
        bones,
    });
}

extern "C" fn on_material(
    ctx: *mut c_void,
    tex: *const c_char,
    tex_len: usize,
    name: *const c_char,
    name_len: usize,
) {
    // SAFETY: `ctx` is the `Scene`; strings are valid or null.
    let s = unsafe { &mut *ctx.cast::<Scene>() };
    s.materials.push(SrcMaterial {
        // SAFETY: as above.
        texture: (!tex.is_null()).then(|| unsafe { cstr(tex, tex_len) }),
        name: (!name.is_null()).then(|| unsafe { cstr(name, name_len) }),
    });
}

/// Imports `path` through the shim; the error text on failure.
fn read(path: &[u8], mode: i32) -> Result<Scene, Vec<u8>> {
    let Ok(cpath) = CString::new(path) else {
        return Err(Vec::new());
    };
    let mut scene = Scene::default();
    let sink = AiSink {
        ctx: std::ptr::from_mut(&mut scene).cast(),
        scene: on_scene,
        node: on_node,
        mesh: on_mesh,
        material: on_material,
    };
    let mut err = vec![0u8; 1024];
    // SAFETY: `cpath` is NUL-terminated, `err` is writable for its length, `sink` outlives the call.
    let rc = unsafe {
        ohpl_ai_read(
            cpath.as_ptr(),
            mode,
            &sink,
            err.as_mut_ptr().cast(),
            err.len(),
        )
    };
    if rc == 0 {
        return Ok(scene);
    }
    err.truncate(err.iter().position(|&c| c == 0).unwrap_or(err.len()));
    Err(err)
}

fn is_old_binary_fbx(path: &[u8]) -> bool {
    use std::io::Read;
    let Ok(mut f) = std::fs::File::open(OsStr::from_bytes(path)) else {
        return false;
    };
    let mut h = [0u8; 27];
    let mut n = 0;
    while n < h.len() {
        match f.read(h.get_mut(n..).unwrap_or_default()) {
            Ok(0) | Err(_) => break,
            Ok(k) => n += k,
        }
    }
    let version = h
        .get(23..27)
        .and_then(|v| v.try_into().ok())
        .map_or(0, u32::from_ne_bytes);
    n == h.len() && h.starts_with(b"Kaydara FBX Binary") && version < 7000
}

fn scene_has_bones(s: &Scene) -> bool {
    s.meshes.iter().any(|m| !m.bones.is_empty())
}

fn node_has_skin(s: &Scene, n: &Node) -> bool {
    n.meshes.iter().any(|&m| {
        s.meshes
            .get(m as usize)
            .is_some_and(|m| !m.bones.is_empty())
    })
}

fn collect_bind_poses(s: &Scene, at: usize, parent: &Mat, out: &mut BTreeMap<Vec<u8>, Mat>) {
    let Some(node) = s.nodes.get(at) else { return };
    let world = mul(parent, &node.m);
    for &mi in &node.meshes {
        let Some(src) = s.meshes.get(mi as usize) else {
            continue;
        };
        for b in &src.bones {
            out.insert(b.name.clone(), mul(&world, &inverse(&b.offset)));
        }
    }
    for &c in &node.children {
        collect_bind_poses(s, c, &world, out);
    }
}

struct Skeleton<'a> {
    scene: &'a Scene,
    unit_scale: f32,
    bind: BTreeMap<Vec<u8>, Mat>,
    count: i32,
    by_name: HashMap<Vec<u8>, i32>,
}

impl Skeleton<'_> {
    fn create_bones(
        &mut self,
        sink: Sink,
        at: usize,
        parent_node_world: &Mat,
        parent_bone_world: &Mat,
        parent: i32,
    ) {
        let Some(node) = self.scene.nodes.get(at) else {
            return;
        };
        let world = mul(parent_node_world, &node.m);
        let bind = self.bind.get(&node.name).copied();
        let mut bone_world = rigid(&world);
        if bind.is_none() && node_has_skin(self.scene, node) {
            bone_world = *parent_bone_world;
        }
        let local = mul(&inverse(parent_bone_world), &bone_world);
        let me = self.count;
        self.count += 1;
        self.by_name.entry(node.name.clone()).or_insert(me);
        sink.bone(&Bone {
            name: abi::Str::new(&node.name),
            sid: abi::Str::new(&node.name),
            m: local,
            inv_bind: bind.map_or(IDENTITY, |b| inverse(&rigid(&b))),
            unit_scale: scale_x(parent_node_world) * self.unit_scale,
            parent,
            value: 0,
            has_unit_scale: 1,
            has_inv_bind: bind.is_some().into(),
        });
        for &c in &node.children {
            self.create_bones(sink, c, &world, &bone_world, me);
        }
    }
}

fn material_file(m: Option<&SrcMaterial>) -> Vec<u8> {
    let Some(m) = m else { return Vec::new() };
    if let Some(t) = &m.texture {
        let path = string::replace_char_to(t, b"\\", b"/");
        return string::set_file_ext(string::file_name(&path), b"mat");
    }
    m.name
        .as_ref()
        .map_or_else(Vec::new, |n| string::set_file_ext(n, b"mat"))
}

fn element(sink: Sink, kind: i32, elem_num: i32, data: &[f32]) {
    let bytes: Vec<u8> = data.iter().flat_map(|f| f.to_ne_bytes()).collect();
    sink.sub_vb_array(&Array {
        kind,
        format: 0,
        prog_var: 0,
        elem_num,
        repeats: 0,
        count: data.len(),
        data: bytes.as_ptr(),
        bytes: bytes.len(),
    });
}

struct Build<'a> {
    scene: &'a Scene,
    sink: Sink<'a>,
    fallback: Vec<u8>,
    skeleton: Option<&'a Skeleton<'a>>,
    subs: usize,
}

impl Build<'_> {
    fn add_node_meshes(&mut self, at: usize, parent: &Mat) {
        let Some(node) = self.scene.nodes.get(at) else {
            return;
        };
        let world = mul(parent, &node.m);
        for (m, &mi) in node.meshes.iter().enumerate() {
            let Some(src) = self.scene.meshes.get(mi as usize) else {
                continue;
            };
            let Some(normals) = &src.normals else {
                continue;
            };
            if src.primitive_types & !PRIMITIVE_NGON_FLAG != PRIMITIVE_TRIANGLE
                || src.vertex_num == 0
            {
                continue;
            }
            self.add_sub(node, &world, m, src, normals);
        }
        for &c in &node.children {
            self.add_node_meshes(c, &world);
        }
    }

    fn add_sub(&mut self, node: &Node, world: &Mat, m: usize, src: &SrcMesh, normals: &[f32]) {
        let sink = self.sink;
        let n = src.vertex_num;
        sink.sub_vb_new(&VbCfg {
            draw_type: 0,
            usage_type: 0,
            reserve_vtx: i32::try_from(n).unwrap_or(i32::MAX),
            reserve_idx: i32::try_from(src.indices.len()).unwrap_or(i32::MAX),
        });
        let (mut pos, mut nrm, mut uv, mut col, mut tan) = (
            vec![0.0f32; n * 4],
            vec![0.0f32; n * 3],
            vec![0.0f32; n * 3],
            vec![0.0f32; n * 4],
            vec![0.0f32; n * 4],
        );
        let none = std::ptr::null();
        let ptr = |o: &Option<Vec<f32>>| o.as_ref().map_or(none, |v| v.as_ptr());
        // SAFETY: every input has 3 floats per vertex (or is null) and every output has room for `n` vertices.
        unsafe {
            ohpl_ai_vertices(
                world.as_ptr(),
                u32::try_from(n).unwrap_or(0),
                src.vertices.as_ptr(),
                normals.as_ptr(),
                ptr(&src.uv0),
                ptr(&src.tangents),
                ptr(&src.bitangents),
                pos.as_mut_ptr(),
                nrm.as_mut_ptr(),
                uv.as_mut_ptr(),
                col.as_mut_ptr(),
                tan.as_mut_ptr(),
            );
        }
        element(sink, ELEMENT_POSITION, 4, &pos);
        element(sink, ELEMENT_NORMAL, 3, &nrm);
        element(sink, ELEMENT_TEXTURE0, 3, &uv);
        element(sink, ELEMENT_COLOR0, 4, &col);
        element(sink, ELEMENT_TANGENT, 4, &tan);
        let idx: Vec<u32> = src
            .indices
            .chunks_exact(3)
            .flat_map(|f| f.iter().rev().copied())
            .collect();
        sink.sub_vb_indices(&idx);
        sink.sub_vb_compile();

        let mut name = node.name.clone();
        if node.meshes.len() > 1 {
            name.push(b'_');
            name.extend(m.to_string().as_bytes());
        }
        sink.sub_new(&name, None, None, None);
        self.subs += 1;
        sink.sub_vb_attach();

        if let Some(sk) = self.skeleton {
            let mut pairs = Vec::new();
            if src.bones.is_empty()
                && let Some(&b) = sk.by_name.get(&node.name)
            {
                pairs.extend((0..n).map(|v| Pair {
                    vtx: i32::try_from(v).unwrap_or(i32::MAX),
                    bone: b,
                    weight: 1.0,
                }));
            }
            for b in &src.bones {
                let Some(&bone) = sk.by_name.get(&b.name) else {
                    continue;
                };
                pairs.extend(b.weights.iter().map(|&(v, w)| Pair {
                    vtx: v as i32,
                    bone,
                    weight: w,
                }));
            }
            if !pairs.is_empty() {
                sink.sub_pairs(&pairs);
            }
        }

        let mut material = material_file(self.scene.materials.get(src.material));
        let mut handle = if material.is_empty() {
            0
        } else {
            sink.material_create(&material)
        };
        if handle == 0 && !self.fallback.is_empty() {
            material.clone_from(&self.fallback);
            handle = sink.material_create(&material);
        }
        sink.sub_material(&material, handle);
        sink.sub_compile();
    }
}

pub fn load_mesh(_cfg: &Cfg, sink: Sink, path: &[u8]) -> i32 {
    if is_old_binary_fbx(path) {
        sink.log(
            abi::LOG,
            &text(&[b"'", path, b"' is binary FBX older than 7.0\n"]),
        );
        return 1;
    }
    let scene = match read(path, 0) {
        Ok(s) if !s.nodes.is_empty() => s,
        Ok(_) => return 1,
        Err(e) => {
            sink.log(
                abi::ERROR,
                &text(&[b"Could not load '", path, b"': ", &e, b"\n"]),
            );
            return 1;
        }
    };
    let mesh_name = string::file_name(path);
    sink.mesh_new(mesh_name, path);
    let fallback = string::set_file_ext(mesh_name, b"mat");

    let mut skeleton = None;
    if scene_has_bones(&scene) {
        let mut bind = BTreeMap::new();
        collect_bind_poses(&scene, 0, &IDENTITY, &mut bind);
        let mut sk = Skeleton {
            scene: &scene,
            unit_scale: scene.unit_scale,
            bind,
            count: 0,
            by_name: HashMap::new(),
        };
        sink.skeleton_begin();
        let root_world = scene.nodes.first().map_or(IDENTITY, |n| n.m);
        for &c in scene.nodes.first().map_or(&[][..], |n| &n.children) {
            sk.create_bones(sink, c, &root_world, &IDENTITY, -1);
        }
        sink.skeleton_end(false);
        skeleton = Some(sk);
    }

    let mut build = Build {
        scene: &scene,
        sink,
        fallback,
        skeleton: skeleton.as_ref(),
        subs: 0,
    };
    build.add_node_meshes(0, &IDENTITY);
    let subs = build.subs;
    if skeleton.is_some() {
        sink.mesh_compile();
    }
    if subs == 0 {
        sink.log(
            abi::ERROR,
            &text(&[b"No triangle geometry in '", path, b"'\n"]),
        );
        sink.mesh_drop();
        return 1;
    }
    0
}

struct Reader<'a> {
    data: &'a [u8],
    pos: usize,
    ok: bool,
}

impl Reader<'_> {
    fn bytes<const N: usize>(&mut self) -> [u8; N] {
        let end = self.pos + N;
        match self.data.get(self.pos..end).and_then(|b| b.try_into().ok()) {
            Some(b) => {
                self.pos = end;
                b
            }
            None => {
                self.ok = false;
                [0; N]
            }
        }
    }

    fn f32(&mut self) -> f32 {
        f32::from_le_bytes(self.bytes())
    }

    fn u32(&mut self) -> u32 {
        u32::from_le_bytes(self.bytes())
    }

    fn string(&mut self) -> Vec<u8> {
        let rest = self.data.get(self.pos..).unwrap_or_default();
        let len = rest.iter().position(|&c| c == 0).unwrap_or(rest.len());
        if len == rest.len() {
            self.ok = false;
        }
        self.pos += len + 1;
        rest.get(..len).unwrap_or_default().to_vec()
    }
}

type Anm = (f32, Vec<(Vec<u8>, Vec<Key>)>);

/// `cMeshLoaderAssimp::LoadHpl3Anm`: `Ok(None)` if the file is missing or not an `.anm`, `Err` if corrupt.
pub fn read_hpl3_anm(anm: &[u8]) -> Result<Option<Anm>, ()> {
    let Ok(data) = std::fs::read(OsStr::from_bytes(anm)) else {
        return Ok(None);
    };
    let mut r = Reader {
        data: &data,
        pos: 0,
        ok: true,
    };
    let magic: [u8; 4] = r.bytes();
    let _version = r.u32();
    if !r.ok || &magic != b"iE\x03v" {
        return Ok(None);
    }
    r.string();
    let length = r.f32();
    let track_num = r.u32() as usize;
    let _extra = r.u32();
    if !r.ok {
        return Ok(None);
    }
    let mut tracks = Vec::new();
    let mut t = 0;
    while t < track_num && r.ok {
        let name = r.string();
        let _flags: [u8; 2] = r.bytes();
        let key_num = r.u32() as usize;
        let mut keys = Vec::new();
        let mut k = 0;
        while k < key_num && r.ok {
            let f: [f32; 8] = std::array::from_fn(|_| r.f32());
            keys.push(Key {
                time: f[0],
                trans: [f[1], f[2], f[3]],
                rot: [f[4], f[5], f[6], f[7]],
            });
            k += 1;
        }
        tracks.push((name, keys));
        t += 1;
    }
    if r.ok {
        Ok(Some((length, tracks)))
    } else {
        Err(())
    }
}

const TRANSLATE_ROTATE: i32 = 3;

pub fn load_anim(cfg: &Cfg, sink: Sink, path: &[u8]) -> i32 {
    let anm = string::set_file_ext(path, b"anm");
    let (length, tracks) = match read_hpl3_anm(&anm) {
        Ok(Some(a)) => a,
        Ok(None) => return 1,
        Err(()) => {
            sink.log(abi::ERROR, &text(&[b"Corrupt animation '", &anm, b"'\n"]));
            return 1;
        }
    };
    let file = string::file_name(path);
    let skip_scale =
        cfg.force_cache_skip_saving != 0 && sink.file_exists(&string::set_file_ext(path, b"msh"));
    let top: Option<(f32, Vec<Vec<u8>>)> = if skip_scale {
        None
    } else {
        read(path, 1).ok().and_then(|s| {
            let top = s.nodes.first().map_or_else(Vec::new, |r| {
                r.children
                    .iter()
                    .filter_map(|&c| s.nodes.get(c).map(|n| n.name.clone()))
                    .collect()
            });
            (s.unit_scale != 1.0).then_some((s.unit_scale, top))
        })
    };
    sink.anim_new(file, file, Some(b"Default"), length, tracks.len());
    for (name, keys) in &tracks {
        sink.anim_track(name, TRANSLATE_ROTATE, keys);
        if let Some((unit, top)) = &top
            && !top.contains(name)
        {
            sink.anim_unit_scale(*unit);
        }
    }
    sink.anim_end(false);
    0
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<AiWeight>() == 8 && size_of::<AiBone>() == 96 && size_of::<AiMesh>() == 80);
};
