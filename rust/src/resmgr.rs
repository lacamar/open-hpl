//! Resource-manager decisions (R1): name normalisation, lookup order, destroy decisions, load settings.
//! Object creation and the loader virtual calls stay in C++; resources are opaque pointers.

use crate::resources::ResMeta;
use crate::string;
use std::collections::BTreeMap;
use std::ffi::c_void;

pub trait Env {
    /// `cFileSearcher::GetFilePath`; the count is only meaningful when `want_count`.
    fn file_path(&mut self, name: &[u8], want_count: bool) -> (Vec<u32>, i32);
    /// `iResourceManager::GetResource`.
    fn get_resource(&mut self, path: &[u32]) -> *mut c_void;
}

pub struct Lookup {
    pub res: *mut c_void,
    pub path: Vec<u32>,
    pub count: i32,
}

fn null() -> *mut c_void {
    std::ptr::null_mut()
}

fn empty_lookup() -> Lookup {
    Lookup {
        res: null(),
        path: Vec::new(),
        count: 0,
    }
}

fn ascii(s: &[u8]) -> Vec<u32> {
    s.iter().copied().map(u32::from).collect()
}

/// `iResourceManager::FindLoadedResource`: the path is cleared when the resource is already loaded.
pub fn find_loaded(env: &mut dyn Env, name: &[u8], want_count: bool) -> Lookup {
    let (mut path, count) = env.file_path(name, want_count);
    let res = env.get_resource(&path);
    if !res.is_null() {
        path.clear();
    }
    Lookup { res, path, count }
}

/// `cTextureManager::FindTexture2D` / `cImageManager::FindImage`: with no extension the format
/// with the most matching directories wins, the first on ties.
pub fn find_with_formats(env: &mut dyn Env, name: &[u8], exts: &[&[u8]]) -> Lookup {
    if !string::file_ext(name).is_empty() {
        return find_loaded(env, name, false);
    }
    let mut best = empty_lookup();
    let mut max = -1;
    for ext in exts {
        let lk = find_loaded(env, &string::set_file_ext(name, ext), true);
        if (!lk.res.is_null() || !lk.path.is_empty()) && lk.count > max {
            max = lk.count;
            best = lk;
        }
    }
    best
}

/// `cSoundManager::FindSampleData`. The path is whatever the last probed format left.
pub fn find_sample(env: &mut dyn Env, name: &[u8], formats: &[&[u8]]) -> Lookup {
    if !string::file_ext(name).is_empty() {
        return find_loaded(env, name, false);
    }
    let mut last = empty_lookup();
    for ext in formats {
        last = find_loaded(env, &string::set_file_ext(name, ext), false);
        if !last.res.is_null() || !last.path.is_empty() {
            break;
        }
    }
    last
}

/// `cSoundManager::FindStreamPath`.
pub fn find_stream_path(env: &mut dyn Env, name: &[u8], formats: &[&[u8]]) -> Vec<u32> {
    if !string::file_ext(name).is_empty() {
        return env.file_path(name, false).0;
    }
    let mut path = Vec::new();
    for ext in formats {
        path = env.file_path(&string::set_file_ext(name, ext), false).0;
        if !path.is_empty() {
            break;
        }
    }
    path
}

pub enum Model {
    NoFormat,
    Found(Vec<u8>, Lookup),
}

/// `cMeshManager::CreateMesh` / `cAnimationManager::CreateAnimation` up to the loader call.
pub fn resolve_model(env: &mut dyn Env, name: &[u8], exts: &[&[u8]]) -> Model {
    let mut new_name = name.to_vec();
    if string::file_ext(&new_name).is_empty() {
        let mut found = false;
        for ext in exts {
            new_name = string::set_file_ext(&new_name, ext);
            if !env.file_path(&new_name, false).0.is_empty() {
                found = true;
                break;
            }
        }
        if !found {
            return Model::NoFormat;
        }
    }
    let lk = find_loaded(env, &new_name, false);
    Model::Found(new_name, lk)
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Simple {
    EntFile,
    Font,
    Script,
    SoundEntity,
    Material,
    PhysicsMaterial,
}

impl Simple {
    pub fn from_i32(k: i32) -> Option<Self> {
        Some(match k {
            0 => Self::EntFile,
            1 => Self::Font,
            2 => Self::Script,
            3 => Self::SoundEntity,
            4 => Self::Material,
            5 => Self::PhysicsMaterial,
            _ => return None,
        })
    }
}

pub fn simple_name(kind: Simple, name: &[u8]) -> Vec<u8> {
    match kind {
        Simple::EntFile => string::set_file_ext(&string::to_lower(name), b"ent"),
        Simple::Font => string::to_lower(name),
        Simple::Script if string::file_ext(name) != b"chps" => string::set_file_ext(name, b"hps"),
        Simple::Script => string::set_file_ext(name, b"chps"),
        Simple::SoundEntity => string::set_file_ext(name, b"snt"),
        Simple::Material | Simple::PhysicsMaterial => string::set_file_ext(name, b"mat"),
    }
}

/// `None`: the manager returns NULL before `BeginLoad` (empty material name).
pub fn resolve_simple(env: &mut dyn Env, kind: Simple, name: &[u8]) -> Option<(Vec<u8>, Lookup)> {
    if kind == Simple::Material && name.is_empty() {
        return None;
    }
    let new_name = simple_name(kind, name);
    let lk = find_loaded(env, &new_name, false);
    Some((new_name, lk))
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum FontKind {
    Unknown,
    TrueType,
    Bitmap,
}

pub fn font_kind(name: &[u8]) -> FontKind {
    match string::to_lower(string::file_ext(name)).as_slice() {
        b"ttf" => FontKind::TrueType,
        b"fnt" => FontKind::Bitmap,
        _ => FontKind::Unknown,
    }
}

/// Custom-entity registry of `cSoundEntityManager`: `std::map<lowercase name, data>`.
#[derive(Default)]
pub struct CustomMap(BTreeMap<Vec<u8>, usize>);

impl CustomMap {
    pub fn get(&self, name: &[u8]) -> Option<*mut c_void> {
        self.0
            .get(&string::to_lower(name))
            .map(|p| std::ptr::with_exposed_provenance_mut(*p))
    }

    pub fn set(&mut self, name: &[u8], data: *mut c_void) {
        self.0
            .insert(string::to_lower(name), data.expose_provenance());
    }
}

/// The custom resolver runs for a bare name with no `.snt` file.
pub fn sound_entity_needs_resolver(env: &mut dyn Env, name: &[u8], has_resolver: bool) -> bool {
    has_resolver
        && string::file_ext(name).is_empty()
        && env
            .file_path(&string::set_file_ext(name, b"snt"), false)
            .0
            .is_empty()
}

/// `cParticleManager::CreatePS(name, type, size)`: lookup by file path, which may be empty.
pub fn particle_by_file(env: &mut dyn Env, ty: &[u8]) -> (Vec<u8>, Vec<u32>, *mut c_void) {
    let file = string::set_file_ext(ty, b"ps");
    let path = env.file_path(&file, false).0;
    let res = env.get_resource(&path);
    (file, path, res)
}

pub fn particle_by_data_name(env: &mut dyn Env, name: &[u8]) -> *mut c_void {
    env.get_resource(&string::utf8_to_wide(name))
}

pub fn destroy_standard(m: &mut ResMeta) -> bool {
    m.dec_user();
    m.user_count == 0
}

pub fn destroy_particle(m: &mut ResMeta) {
    if m.user_count > 0 {
        m.dec_user();
    }
}

/// True when a stream's last user went away and it leaves the stream list.
pub fn destroy_sound(m: &mut ResMeta, is_stream: bool) -> bool {
    m.dec_user();
    is_stream && m.user_count == 0
}

/// Frame bookkeeping after an image's frame pic counts were lowered.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub struct ImageAfter {
    pub delete_frame_bitmap: bool,
    pub delete_frame: bool,
}

pub fn destroy_image_frame(
    frame_empty: bool,
    frame_custom: bool,
    has_bitmap_frame: bool,
) -> ImageAfter {
    ImageAfter {
        delete_frame_bitmap: frame_empty && !frame_custom && has_bitmap_frame,
        delete_frame: frame_empty,
    }
}

/// `cImageManager::AddToFrame` size for a bitmap that fit no frame. A zero or overflowing size
/// (an endless/UB loop in C++) stops growing.
pub fn image_frame_size(frame: (i32, i32), bmp: (i32, i32)) -> (i32, i32) {
    let grow = |mut v: i32, need: i32| {
        let need = need.wrapping_add(2);
        while v < need && v > 0 && v <= 0x3fff_ffff {
            v *= 2;
        }
        v
    };
    (grow(frame.0, bmp.0), grow(frame.1, bmp.1))
}

pub const CUBE_SUFFIXES: [&[u8]; 6] = [
    b"_pos_x", b"_neg_x", b"_pos_y", b"_neg_y", b"_pos_z", b"_neg_z",
];

/// `CreateAnim` first-frame name: a bare name takes the first bitmap format that exists.
pub fn anim_first_frame(env: &mut dyn Env, name: &[u8], exts: &[&[u8]]) -> Vec<u8> {
    if string::file_ext(name).is_empty() {
        for ext in exts {
            let mut cand = name.to_vec();
            cand.push(b'.');
            cand.extend_from_slice(ext);
            if !env.file_path(&cand, false).0.is_empty() {
                return cand;
            }
        }
    }
    name.to_vec()
}

#[derive(Debug, PartialEq, Eq)]
pub enum AnimErr {
    No01,
    No01BeforeExt,
    FirstFrameMissing,
    NoFrames(Vec<u8>),
}

pub enum AnimPlan {
    Loaded(*mut c_void),
    Frames {
        base_name: Vec<u8>,
        fake_path: Vec<u32>,
        paths: Vec<Vec<u32>>,
    },
}

/// `cTextureManager::CreateAnim` after `BeginLoad`: base name, cache key and frame list.
pub fn anim_plan(env: &mut dyn Env, first: &[u8]) -> Result<AnimPlan, AnimErr> {
    let pos = string::first_pos(first, b"01");
    if pos < 0 {
        return Err(AnimErr::No01);
    }
    let sub1 = string::sub(first, 0, pos, false).unwrap_or_default();
    let sub2 = string::sub(first, pos.wrapping_add(2), -1, false).unwrap_or_default();
    let mut base = sub1.to_vec();
    base.extend_from_slice(sub2);
    if sub2.first() != Some(&b'.') {
        return Err(AnimErr::No01BeforeExt);
    }
    let first_path = env.file_path(first, false).0;
    if first_path.is_empty() {
        return Err(AnimErr::FirstFrameMissing);
    }
    let mut fake = string::file_path(&first_path).to_vec();
    fake.extend(string::utf8_to_wide(string::file_name(&base)));
    let res = env.get_resource(&fake);
    if !res.is_null() {
        return Ok(AnimPlan::Loaded(res));
    }
    let ext = string::file_ext(&base);
    let stem = string::set_file_ext(string::file_name(&base), b"");
    let mut test = stem.clone();
    test.extend_from_slice(b"01.");
    test.extend_from_slice(ext);
    let mut paths = Vec::new();
    let mut num = 2;
    loop {
        let p = env.file_path(&test, false).0;
        if p.is_empty() {
            break;
        }
        paths.push(p);
        test = stem.clone();
        if num < 10 {
            test.push(b'0');
        }
        test.extend(string::int_to_string(num, 0));
        test.push(b'.');
        test.extend_from_slice(ext);
        num += 1;
    }
    if paths.is_empty() {
        return Err(AnimErr::NoFrames(base));
    }
    Ok(AnimPlan::Frames {
        base_name: base,
        fake_path: fake,
        paths,
    })
}

pub enum CubePre {
    SingleFile,
    Faces {
        name: Vec<u8>,
        fake_path: Vec<u32>,
        res: *mut c_void,
    },
}

/// `CreateCubeMap` before `BeginLoad`.
pub fn cube_pre(env: &mut dyn Env, path_name: &[u8]) -> CubePre {
    if string::to_lower(string::file_ext(path_name)) == b"dds" {
        return CubePre::SingleFile;
    }
    let name = string::set_file_ext(path_name, b"");
    let fake_path = string::utf8_to_wide(&name);
    let res = env.get_resource(&fake_path);
    CubePre::Faces {
        name,
        fake_path,
        res,
    }
}

/// The six face files, or the index of the first face with no file. With no formats a face
/// reuses the previous face's path.
pub fn cube_faces(env: &mut dyn Env, name: &[u8], exts: &[&[u8]]) -> Result<Vec<Vec<u32>>, usize> {
    let mut paths = Vec::new();
    let mut path: Vec<u32> = Vec::new();
    for (i, suffix) in CUBE_SUFFIXES.iter().enumerate() {
        for ext in exts {
            let mut cand = name.to_vec();
            cand.extend_from_slice(suffix);
            cand.push(b'.');
            cand.extend_from_slice(ext);
            path = env.file_path(&cand, false).0;
            if !path.is_empty() {
                break;
            }
        }
        if path.is_empty() {
            return Err(i);
        }
        paths.push(path.clone());
    }
    Ok(paths)
}

pub mod mat {
    //! `cMaterialManager` string tables and load-setting decisions.
    use crate::string;

    pub const TEX_1D: i32 = 0;
    pub const TEX_2D: i32 = 1;
    pub const TEX_CUBE: i32 = 3;
    pub const TEX_3D: i32 = 4;

    pub fn texture_type(s: &[u8]) -> i32 {
        match string::to_lower(s).as_slice() {
            b"cube" | b"rect" => TEX_CUBE,
            b"1d" => TEX_1D,
            b"3d" => TEX_3D,
            _ => TEX_2D,
        }
    }

    pub fn wrap(s: &[u8]) -> i32 {
        match string::to_lower(s).as_slice() {
            b"clamp" => 1,
            b"clamptoedge" => 2,
            _ => 0,
        }
    }

    pub fn anim_mode(s: &[u8]) -> i32 {
        match string::to_lower(s).as_slice() {
            b"loop" => 1,
            b"oscillate" => 2,
            _ => 0,
        }
    }

    /// Value and whether C++ logs the unknown-mode warning.
    pub fn blend_mode(s: &[u8]) -> (i32, bool) {
        match string::to_lower(s).as_slice() {
            b"add" => (1, false),
            b"mul" => (2, false),
            b"mulx2" => (3, false),
            b"alpha" => (4, false),
            b"premulalpha" => (5, false),
            _ => (1, true),
        }
    }

    /// `None` is the C++ NULL pointer; the bool is "log an error".
    pub fn uv_anim_type(s: Option<&[u8]>) -> (i32, bool) {
        match s.map(string::to_lower).as_deref() {
            Some(b"translate") => (0, false),
            Some(b"sin") => (1, false),
            Some(b"rotate") => (2, false),
            _ => (3, true),
        }
    }

    pub fn anim_axis(s: Option<&[u8]>) -> (i32, bool) {
        match s.map(string::to_lower).as_deref() {
            Some(b"x") => (0, false),
            Some(b"y") => (1, false),
            Some(b"z") => (2, false),
            _ => (3, true),
        }
    }

    const NAMES: [&str; 22] = [
        "Diffuse",
        "NMap",
        "Specular",
        "Alpha",
        "Height",
        "Illumination",
        "CubeMap",
        "DissolveAlpha",
        "CubeMapAlpha",
        "DetailDiffuse",
        "DetailNMap",
        "Translucency",
        "DiffuseSide",
        "DiffuseTop",
        "DiffuseBottom",
        "NMapSide",
        "NMapTop",
        "NMapBottom",
        "SpecularSide",
        "SpecularTop",
        "SpecularBottom",
        "Liquid",
    ];

    /// `GetTextureString`: the XML element name of a material texture slot.
    pub fn texture_string(t: i32) -> Vec<u8> {
        if (21..=32).contains(&t) {
            let idx = t - 21;
            let kinds: [&[u8]; 4] = [b"Diffuse_", b"Specular_", b"NMap_", b"Height_"];
            let mut out = kinds
                .get((idx & 3) as usize)
                .copied()
                .unwrap_or_default()
                .to_vec();
            out.push(b"RGB".get((idx >> 2) as usize).copied().unwrap_or(b'R'));
            return out;
        }
        let i = match t {
            0..=20 => t,
            33 => 21,
            _ => return Vec::new(),
        };
        NAMES
            .get(i as usize)
            .map_or_else(Vec::new, |n| n.as_bytes().to_vec())
    }

    pub fn texture_is_srgb(t: i32) -> bool {
        matches!(
            t,
            0 | 2 | 5 | 6 | 8 | 9 | 12..=14 | 18..=22 | 25 | 26 | 29 | 30
        )
    }

    #[derive(Clone, Copy, PartialEq, Eq, Debug)]
    pub enum Create {
        Anim,
        Tex1D,
        Tex2D,
        Tex3D,
        Cube,
        /// Unknown texture type: no call, the texture stays NULL.
        None,
    }

    pub fn create_kind(anim_mode: i32, ty: i32) -> Create {
        if anim_mode != 0 {
            return Create::Anim;
        }
        match ty {
            TEX_1D => Create::Tex1D,
            TEX_2D => Create::Tex2D,
            TEX_3D => Create::Tex3D,
            TEX_CUBE => Create::Cube,
            _ => Create::None,
        }
    }

    /// A texture file with no directory part is looked up next to the material.
    pub fn texture_file(file: &[u8], material_path: &[u32]) -> Vec<u8> {
        if string::file_path(file).len() <= 1 {
            let dir = string::wide_to_utf8(string::file_path(material_path));
            string::set_file_path(file, &dir)
        } else {
            file.to_vec()
        }
    }

    /// `SetTextureAnisotropy` early-outs; true when the new value is applied.
    pub fn anisotropy_accepted(x: f32, caps: i32, max: i32, current: f32) -> bool {
        !(x < 1.0 || caps == 0 || x > max as f32 || current == x)
    }
}

pub mod world {
    //! `cEngineFileLoading` parsing decisions.
    use crate::string;

    pub fn billboard_type(s: &[u8]) -> i32 {
        match s {
            b"Axis" => 1,
            b"FixedAxis" => 2,
            _ => 0,
        }
    }

    pub fn shadow_resolution(s: &[u8]) -> i32 {
        match string::to_lower(s).as_slice() {
            b"medium" => 1,
            b"low" => 0,
            _ => 2,
        }
    }

    #[derive(Clone, Copy, PartialEq, Eq, Debug)]
    pub enum Light {
        Box,
        Spot,
        Point,
    }

    pub fn light_kind(element: &[u8]) -> Option<Light> {
        match element {
            b"BoxLight" => Some(Light::Box),
            b"SpotLight" => Some(Light::Spot),
            b"PointLight" => Some(Light::Point),
            _ => None,
        }
    }

    #[derive(Clone, Copy, PartialEq, Eq, Debug)]
    pub enum Gobo {
        Tex2D,
        Anim2D,
        Cube,
        AnimCube,
    }

    pub fn gobo_kind(spot: bool, anim_mode: i32) -> Gobo {
        match (spot, anim_mode == 0) {
            (true, true) => Gobo::Tex2D,
            (true, false) => Gobo::Anim2D,
            (false, true) => Gobo::Cube,
            (false, false) => Gobo::AnimCube,
        }
    }

    pub fn shadow_casters(dynamic: bool, stat: bool) -> u32 {
        u32::from(stat) | (u32::from(dynamic) << 1)
    }

    pub fn sound_use_default(use_default: bool, custom_props: Option<bool>) -> bool {
        custom_props.map_or(use_default, |c| !c)
    }

    /// Complete triples in `values` floats.
    pub fn band_count(values: usize) -> usize {
        let mut n = 0;
        let mut i = 0;
        while i + 2 < values {
            n += 1;
            i += 3;
        }
        n
    }

    /// `strtoul(s, NULL, 10)` truncated to `unsigned int`.
    pub fn light_mask_id(s: &[u8]) -> u32 {
        let mut it = s
            .iter()
            .copied()
            .skip_while(|c| matches!(c, b' ' | b'\t'..=b'\r'))
            .peekable();
        let neg = matches!(it.peek(), Some(b'-'));
        if matches!(it.peek(), Some(b'-' | b'+')) {
            it.next();
        }
        let mut v: u64 = 0;
        let mut over = false;
        for c in it.take_while(u8::is_ascii_digit) {
            match v
                .checked_mul(10)
                .and_then(|x| x.checked_add(u64::from(c - b'0')))
            {
                Some(x) => v = x,
                None => over = true,
            }
        }
        if over {
            return u32::MAX;
        }
        (if neg { v.wrapping_neg() } else { v }) as u32
    }

    pub const DECAL_ELEMENTS: [usize; 4] = [4, 3, 3, 4];

    /// Per vertex and array one `[x, y, z, w]` (missing components 0).
    /// `None` where C++ reads past an array.
    pub fn decal_vertices(arrays: [&[f32]; 4], verts: usize) -> Option<Vec<[f32; 4]>> {
        let mut out = Vec::with_capacity(verts.min(1 << 20).saturating_mul(4));
        for v in 0..verts {
            for (a, n) in arrays.iter().zip(DECAL_ELEMENTS) {
                let base = v.checked_mul(n)?;
                let d = a.get(base..base.checked_add(n)?)?;
                out.push([
                    d.first().copied().unwrap_or(0.0),
                    d.get(1).copied().unwrap_or(0.0),
                    d.get(2).copied().unwrap_or(0.0),
                    d.get(3).copied().unwrap_or(0.0),
                ]);
            }
        }
        Some(out)
    }
}

/// `std::map<name, loader>` with insert-keeps-first and a default fallback (`cResources` entity and area loaders).
#[derive(Default)]
pub struct LoaderMap {
    map: BTreeMap<Vec<u8>, usize>,
    default: usize,
}

impl LoaderMap {
    pub fn add(&mut self, name: &[u8], loader: *mut c_void, set_default: bool) {
        self.map
            .entry(name.to_vec())
            .or_insert(loader.expose_provenance());
        if set_default {
            self.default = loader.expose_provenance();
        }
    }

    /// Loader (null when none) and whether the name was missing (C++ warns, and logs again when a default is used).
    pub fn get(&self, name: &[u8]) -> (*mut c_void, bool) {
        match self.map.get(name) {
            Some(p) => (std::ptr::with_exposed_provenance_mut(*p), false),
            None => (std::ptr::with_exposed_provenance_mut(self.default), true),
        }
    }

    /// Values in key order, as `STLMapDeleteAll` walks them.
    pub fn values(&self) -> impl Iterator<Item = *mut c_void> {
        self.map
            .values()
            .map(|p| std::ptr::with_exposed_provenance_mut(*p))
    }
}

/// `cResources::GetCacheFile`: file path plus the lengths of the directory prefixes to create.
/// An all-slash or empty `file` throws in C++ (`substr(npos)`); here it adds nothing.
pub fn cache_file(cache_home: &[u32], file: &[u32], ext: &[u32]) -> (Vec<u32>, Vec<usize>) {
    let slash = u32::from(b'/');
    let start = file.iter().position(|c| *c != slash).unwrap_or(file.len());
    let mut p = cache_home.to_vec();
    p.extend(ascii(b"open-hpl/files/"));
    p.extend_from_slice(file.get(start..).unwrap_or_default());
    let p = string::set_file_ext(&p, ext);
    let dirs = p
        .iter()
        .enumerate()
        .skip(1)
        .filter(|(_, c)| **c == slash)
        .map(|(i, _)| i)
        .collect();
    (p, dirs)
}

/// `LoadResourceDirsFile` entry: directories to add in order, none for an empty path.
pub fn resource_dirs(path: &[u8], alt: &[u32]) -> Vec<Vec<u32>> {
    if path.is_empty() {
        return Vec::new();
    }
    let path = if matches!(path.first(), Some(b'/' | b'\\')) {
        string::sub(path, 1, -1, false).unwrap_or_default()
    } else {
        path
    };
    let wide = string::utf8_to_wide(path);
    let mut out = Vec::new();
    if !alt.is_empty() {
        let mut a = alt.to_vec();
        a.extend_from_slice(&wide);
        out.push(a);
    }
    out.push(wide);
    out
}
