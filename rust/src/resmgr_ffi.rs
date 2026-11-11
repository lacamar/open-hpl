//! C ABI for `resmgr.rs`, mirrored in `HPL2/core/include/resources/RustResMgr.h`.

use crate::resmgr::{
    self, AnimErr, AnimPlan, CubePre, CustomMap, Env, FontKind, LoaderMap, Lookup, Model, Simple,
};
use crate::resources::ResMeta;
use crate::string_ffi::{Sink, emit, units};
use std::ffi::c_void;

type FilePathFn =
    unsafe extern "C" fn(*mut c_void, *const u8, usize, *mut i32, *mut usize) -> *const u32;
type GetResFn = unsafe extern "C" fn(*mut c_void, *const u32, usize) -> *mut c_void;

#[repr(C)]
pub struct RmEnv {
    ctx: *mut c_void,
    file_path: FilePathFn,
    get_resource: GetResFn,
}

#[repr(C)]
pub struct RmBytes {
    p: *const u8,
    n: usize,
}

#[repr(C)]
pub struct RmOut {
    ctx: *mut c_void,
    name: Sink<u8>,
    path: Sink<u32>,
    item: Sink<u32>,
    res: *mut c_void,
}

struct CEnv<'a>(&'a RmEnv);

impl Env for CEnv<'_> {
    fn file_path(&mut self, name: &[u8], want_count: bool) -> (Vec<u32>, i32) {
        let mut count = 0;
        let mut len = 0;
        let counter = if want_count {
            &raw mut count
        } else {
            std::ptr::null_mut()
        };
        // SAFETY: the C++ callback returns a path valid for `len` units.
        let path = unsafe {
            let p =
                (self.0.file_path)(self.0.ctx, name.as_ptr(), name.len(), counter, &raw mut len);
            units(p, len).to_vec()
        };
        (path, count)
    }

    fn get_resource(&mut self, path: &[u32]) -> *mut c_void {
        // SAFETY: the C++ callback only reads `path`.
        unsafe { (self.0.get_resource)(self.0.ctx, path.as_ptr(), path.len()) }
    }
}

/// # Safety
/// `p` points to `n` valid `RmBytes` whose pointers are valid for their lengths.
unsafe fn byte_list<'a>(p: *const RmBytes, n: usize) -> Vec<&'a [u8]> {
    // SAFETY: caller contract.
    unsafe { units(p, n) }
        .iter()
        // SAFETY: caller contract.
        .map(|b| unsafe { units(b.p, b.n) })
        .collect()
}

/// # Safety
/// `out` is valid and its sinks accept its ctx.
unsafe fn put_lookup(out: *mut RmOut, lk: Lookup) {
    // SAFETY: caller contract.
    unsafe {
        (*out).res = lk.res;
        emit((*out).path, (*out).ctx, &lk.path);
    }
}

macro_rules! env_of {
    ($e:expr) => {
        &mut CEnv(&*$e)
    };
}

/// # Safety
/// All pointers valid; `exts` has `ne` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_find_with_formats(
    env: *const RmEnv,
    name: *const u8,
    n: usize,
    exts: *const RmBytes,
    ne: usize,
    out: *mut RmOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let lk = resmgr::find_with_formats(env_of!(env), units(name, n), &byte_list(exts, ne));
        put_lookup(out, lk);
    }
}

/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_find_sample(
    env: *const RmEnv,
    name: *const u8,
    n: usize,
    formats: *const RmBytes,
    nf: usize,
    out: *mut RmOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let lk = resmgr::find_sample(env_of!(env), units(name, n), &byte_list(formats, nf));
        put_lookup(out, lk);
    }
}

/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_find_stream_path(
    env: *const RmEnv,
    name: *const u8,
    n: usize,
    formats: *const RmBytes,
    nf: usize,
    out: *mut RmOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let p = resmgr::find_stream_path(env_of!(env), units(name, n), &byte_list(formats, nf));
        emit((*out).path, (*out).ctx, &p);
    }
}

/// 0: no file in any format; 1: `out.name`, `out.path`, `out.res` set.
///
/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_resolve_model(
    env: *const RmEnv,
    name: *const u8,
    n: usize,
    exts: *const RmBytes,
    ne: usize,
    out: *mut RmOut,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        match resmgr::resolve_model(env_of!(env), units(name, n), &byte_list(exts, ne)) {
            Model::NoFormat => 0,
            Model::Found(new_name, lk) => {
                emit((*out).name, (*out).ctx, &new_name);
                put_lookup(out, lk);
                1
            }
        }
    }
}

/// `kind`: 0 ent file, 1 font, 2 script, 3 sound entity, 4 material, 5 physics material.
/// 0 when the manager rejects the name before loading; otherwise as `ohpl_rm_resolve_model`.
///
/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_resolve_simple(
    env: *const RmEnv,
    kind: i32,
    name: *const u8,
    n: usize,
    out: *mut RmOut,
) -> i32 {
    let Some(kind) = Simple::from_i32(kind) else {
        return 0;
    };
    // SAFETY: caller contract.
    unsafe {
        match resmgr::resolve_simple(env_of!(env), kind, units(name, n)) {
            None => 0,
            Some((new_name, lk)) => {
                emit((*out).name, (*out).ctx, &new_name);
                put_lookup(out, lk);
                1
            }
        }
    }
}

/// 0 unknown, 1 true type, 2 bitmap.
///
/// # Safety
/// `name` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_font_kind(name: *const u8, n: usize) -> i32 {
    // SAFETY: caller contract.
    match resmgr::font_kind(unsafe { units(name, n) }) {
        FontKind::Unknown => 0,
        FontKind::TrueType => 1,
        FontKind::Bitmap => 2,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_custom_new() -> *mut CustomMap {
    Box::into_raw(Box::default())
}

/// # Safety
/// `m` from `ohpl_rm_custom_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_custom_free(m: *mut CustomMap) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(m) });
}

/// `*found` tells a stored NULL from a missing name.
///
/// # Safety
/// `m` live, `name` valid for `n`, `found` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_custom_get(
    m: *const CustomMap,
    name: *const u8,
    n: usize,
    found: *mut bool,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe {
        let r = (*m).get(units(name, n));
        *found = r.is_some();
        r.unwrap_or(std::ptr::null_mut())
    }
}

/// # Safety
/// `m` live, `name` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_custom_set(
    m: *mut CustomMap,
    name: *const u8,
    n: usize,
    data: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { (*m).set(units(name, n), data) };
}

/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_snd_needs_resolver(
    env: *const RmEnv,
    name: *const u8,
    n: usize,
    has_resolver: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe { resmgr::sound_entity_needs_resolver(env_of!(env), units(name, n), has_resolver) }
}

/// Sets `out.name` (file), `out.path` and `out.res`.
///
/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_particle_by_file(
    env: *const RmEnv,
    ty: *const u8,
    n: usize,
    out: *mut RmOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let (file, path, res) = resmgr::particle_by_file(env_of!(env), units(ty, n));
        emit((*out).name, (*out).ctx, &file);
        emit((*out).path, (*out).ctx, &path);
        (*out).res = res;
    }
}

/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_particle_by_data(
    env: *const RmEnv,
    name: *const u8,
    n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { resmgr::particle_by_data_name(env_of!(env), units(name, n)) }
}

/// True when the resource has no users left and is removed and deleted.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_destroy_standard(m: *mut ResMeta) -> bool {
    // SAFETY: caller contract.
    resmgr::destroy_standard(unsafe { &mut *m })
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_destroy_particle(m: *mut ResMeta) {
    // SAFETY: caller contract.
    resmgr::destroy_particle(unsafe { &mut *m });
}

/// True when the stream leaves the stream list.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_destroy_sound(m: *mut ResMeta, is_stream: bool) -> bool {
    // SAFETY: caller contract.
    resmgr::destroy_sound(unsafe { &mut *m }, is_stream)
}

/// Bit 0: delete the bitmap frame, bit 1: delete the texture frame.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_destroy_image_frame(
    frame_empty: bool,
    frame_custom: bool,
    has_bitmap_frame: bool,
) -> u32 {
    let a = resmgr::destroy_image_frame(frame_empty, frame_custom, has_bitmap_frame);
    u32::from(a.delete_frame_bitmap) | (u32::from(a.delete_frame) << 1)
}

/// # Safety
/// `w` and `h` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_image_frame_size(
    fw: i32,
    fh: i32,
    bw: i32,
    bh: i32,
    w: *mut i32,
    h: *mut i32,
) {
    let (rw, rh) = resmgr::image_frame_size((fw, fh), (bw, bh));
    // SAFETY: caller contract.
    unsafe {
        *w = rw;
        *h = rh;
    }
}

/// Sets `out.name` to the first-frame name.
///
/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_anim_first_frame(
    env: *const RmEnv,
    name: *const u8,
    n: usize,
    exts: *const RmBytes,
    ne: usize,
    out: *mut RmOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = resmgr::anim_first_frame(env_of!(env), units(name, n), &byte_list(exts, ne));
        emit((*out).name, (*out).ctx, &r);
    }
}

/// 0: loaded (`out.res`); 1: frames (`out.name` base, `out.path` cache key, `out.item` per frame);
/// -1/-2/-3: error, tab stays open; -4: no frames, `EndLoad` runs.
///
/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_anim_plan(
    env: *const RmEnv,
    first: *const u8,
    n: usize,
    out: *mut RmOut,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        match resmgr::anim_plan(env_of!(env), units(first, n)) {
            Err(AnimErr::No01) => -1,
            Err(AnimErr::No01BeforeExt) => -2,
            Err(AnimErr::FirstFrameMissing) => -3,
            Err(AnimErr::NoFrames(base)) => {
                emit((*out).name, (*out).ctx, &base);
                -4
            }
            Ok(AnimPlan::Loaded(r)) => {
                (*out).res = r;
                0
            }
            Ok(AnimPlan::Frames {
                base_name,
                fake_path,
                paths,
            }) => {
                emit((*out).name, (*out).ctx, &base_name);
                emit((*out).path, (*out).ctx, &fake_path);
                for p in &paths {
                    emit((*out).item, (*out).ctx, p);
                }
                1
            }
        }
    }
}

/// 0: single-file cube map; 1: `out.name`, `out.path` (key) and `out.res` set.
///
/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_cube_pre(
    env: *const RmEnv,
    path: *const u8,
    n: usize,
    out: *mut RmOut,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        match resmgr::cube_pre(env_of!(env), units(path, n)) {
            CubePre::SingleFile => 0,
            CubePre::Faces {
                name,
                fake_path,
                res,
            } => {
                emit((*out).name, (*out).ctx, &name);
                emit((*out).path, (*out).ctx, &fake_path);
                (*out).res = res;
                1
            }
        }
    }
}

/// 0: six paths via `out.item`; `1 + i`: face `i` has no file.
///
/// # Safety
/// As `ohpl_rm_find_with_formats`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_cube_faces(
    env: *const RmEnv,
    name: *const u8,
    n: usize,
    exts: *const RmBytes,
    ne: usize,
    out: *mut RmOut,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        match resmgr::cube_faces(env_of!(env), units(name, n), &byte_list(exts, ne)) {
            Err(i) => 1 + i as i32,
            Ok(paths) => {
                for p in &paths {
                    emit((*out).item, (*out).ctx, p);
                }
                0
            }
        }
    }
}

macro_rules! parse_fn {
    ($name:ident, $f:path) => {
        /// # Safety
        /// `s` valid for `n`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *const u8, n: usize) -> i32 {
            // SAFETY: caller contract.
            $f(unsafe { units(s, n) })
        }
    };
}

parse_fn!(ohpl_rm_mat_texture_type, resmgr::mat::texture_type);
parse_fn!(ohpl_rm_mat_wrap, resmgr::mat::wrap);
parse_fn!(ohpl_rm_mat_anim_mode, resmgr::mat::anim_mode);
parse_fn!(ohpl_rm_world_billboard_type, resmgr::world::billboard_type);
parse_fn!(
    ohpl_rm_world_shadow_resolution,
    resmgr::world::shadow_resolution
);

/// `*warn` set when C++ logs the unknown-mode warning.
///
/// # Safety
/// `s` valid for `n`, `warn` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_mat_blend_mode(s: *const u8, n: usize, warn: *mut bool) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        let (v, w) = resmgr::mat::blend_mode(units(s, n));
        *warn = w;
        v
    }
}

/// `s` NULL is a missing attribute; `*err` set when C++ logs an error.
///
/// # Safety
/// `s` NULL or valid for `n`, `err` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_mat_uv_anim_type(s: *const u8, n: usize, err: *mut bool) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        let (v, e) = resmgr::mat::uv_anim_type((!s.is_null()).then(|| units(s, n)));
        *err = e;
        v
    }
}

/// # Safety
/// As `ohpl_rm_mat_uv_anim_type`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_mat_anim_axis(s: *const u8, n: usize, err: *mut bool) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        let (v, e) = resmgr::mat::anim_axis((!s.is_null()).then(|| units(s, n)));
        *err = e;
        v
    }
}

/// # Safety
/// `ctx` accepted by `sink`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_mat_texture_string(t: i32, sink: Sink<u8>, ctx: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &resmgr::mat::texture_string(t)) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_mat_texture_is_srgb(t: i32) -> bool {
    resmgr::mat::texture_is_srgb(t)
}

/// 0 anim, 1 1D, 2 2D, 3 3D, 4 cube, 5 none.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_mat_create_kind(anim_mode: i32, ty: i32) -> i32 {
    use resmgr::mat::Create;
    match resmgr::mat::create_kind(anim_mode, ty) {
        Create::Anim => 0,
        Create::Tex1D => 1,
        Create::Tex2D => 2,
        Create::Tex3D => 3,
        Create::Cube => 4,
        Create::None => 5,
    }
}

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_mat_texture_file(
    file: *const u8,
    n: usize,
    mat_path: *const u32,
    pn: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = resmgr::mat::texture_file(units(file, n), units(mat_path, pn));
        emit(sink, ctx, &r);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_mat_anisotropy_accepted(
    x: f32,
    caps: i32,
    max: i32,
    current: f32,
) -> bool {
    resmgr::mat::anisotropy_accepted(x, caps, max, current)
}

/// -1 unknown element, else 0 box, 1 spot, 2 point.
///
/// # Safety
/// `s` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_world_light_kind(s: *const u8, n: usize) -> i32 {
    use resmgr::world::Light;
    // SAFETY: caller contract.
    match resmgr::world::light_kind(unsafe { units(s, n) }) {
        None => -1,
        Some(Light::Box) => 0,
        Some(Light::Spot) => 1,
        Some(Light::Point) => 2,
    }
}

/// 0 2D, 1 animated 2D, 2 cube, 3 animated cube.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_world_gobo_kind(spot: bool, anim_mode: i32) -> i32 {
    use resmgr::world::Gobo;
    match resmgr::world::gobo_kind(spot, anim_mode) {
        Gobo::Tex2D => 0,
        Gobo::Anim2D => 1,
        Gobo::Cube => 2,
        Gobo::AnimCube => 3,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_world_shadow_casters(dynamic: bool, stat: bool) -> u32 {
    resmgr::world::shadow_casters(dynamic, stat)
}

/// `custom`: -1 attribute absent, else its boolean value.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_world_sound_use_default(use_default: bool, custom: i32) -> bool {
    resmgr::world::sound_use_default(use_default, (custom >= 0).then_some(custom != 0))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_world_band_count(values: usize) -> usize {
    resmgr::world::band_count(values)
}

/// # Safety
/// `s` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_world_light_mask_id(s: *const u8, n: usize) -> u32 {
    // SAFETY: caller contract.
    resmgr::world::light_mask_id(unsafe { units(s, n) })
}

/// Packs decal vertex data into `verts * 4` `[x, y, z, w]` rows (one per array).
/// False when an array is shorter than the vertex count needs; nothing is written. A NULL `out` only checks.
///
/// # Safety
/// Each `arrays[i]` valid for `lens[i]` floats; `out` writable for `verts * 16` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_world_decal_vertices(
    arrays: *const *const f32,
    lens: *const usize,
    verts: usize,
    out: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let a = |i: usize| units(*arrays.add(i), *lens.add(i));
        let Some(rows) = resmgr::world::decal_vertices([a(0), a(1), a(2), a(3)], verts) else {
            return false;
        };
        if !out.is_null() {
            for (i, r) in rows.iter().enumerate() {
                std::ptr::copy_nonoverlapping(r.as_ptr(), out.add(i * 4), 4);
            }
        }
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_loaders_new() -> *mut LoaderMap {
    Box::into_raw(Box::default())
}

/// # Safety
/// `m` from `ohpl_rm_loaders_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_loaders_free(m: *mut LoaderMap) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(m) });
}

/// # Safety
/// `m` live, `name` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_loaders_add(
    m: *mut LoaderMap,
    name: *const u8,
    n: usize,
    loader: *mut c_void,
    set_default: bool,
) {
    // SAFETY: caller contract.
    unsafe { (*m).add(units(name, n), loader, set_default) };
}

/// `*missing`: the name was absent (warn; a non-NULL result is then the default).
///
/// # Safety
/// `m` live, `name` valid for `n`, `missing` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_loaders_get(
    m: *const LoaderMap,
    name: *const u8,
    n: usize,
    missing: *mut bool,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe {
        let (p, miss) = (*m).get(units(name, n));
        *missing = miss;
        p
    }
}

/// Calls `f(ctx, loader)` for every stored loader in key order.
///
/// # Safety
/// `m` live; `f` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_loaders_each(
    m: *const LoaderMap,
    f: unsafe extern "C" fn(*mut c_void, *mut c_void),
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        for p in (*m).values() {
            f(ctx, p);
        }
    }
}

/// `out.path` is the file path; `out.item` each directory prefix to create, shortest first.
///
/// # Safety
/// Pointers valid for their lengths; `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_cache_file(
    home: *const u32,
    hn: usize,
    file: *const u32,
    fnn: usize,
    ext: *const u32,
    en: usize,
    out: *mut RmOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let (p, dirs) = resmgr::cache_file(units(home, hn), units(file, fnn), units(ext, en));
        emit((*out).path, (*out).ctx, &p);
        for d in dirs {
            emit((*out).item, (*out).ctx, p.get(..d).unwrap_or_default());
        }
    }
}

/// `out.item` receives each directory to add, in order.
///
/// # Safety
/// Pointers valid for their lengths; `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_resource_dirs(
    path: *const u8,
    n: usize,
    alt: *const u32,
    an: usize,
    out: *mut RmOut,
) {
    // SAFETY: caller contract.
    unsafe {
        for d in resmgr::resource_dirs(units(path, n), units(alt, an)) {
            emit((*out).item, (*out).ctx, &d);
        }
    }
}
