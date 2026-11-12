//! C ABI of `HPL2/core/include/impl/RustL6.h`: the config, the builder callback table and the
//! wrapper the loaders drive.

use crate::meshload::Key;
use std::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    pub p: *const u8,
    pub n: usize,
}

impl Str {
    pub fn new(s: &[u8]) -> Self {
        Self {
            p: s.as_ptr(),
            n: s.len(),
        }
    }

    pub const NONE: Str = Str {
        p: std::ptr::null(),
        n: 0,
    };

    /// # Safety
    /// `p` is valid for `n` bytes for the returned lifetime, or `n` is 0.
    pub unsafe fn bytes<'a>(self) -> &'a [u8] {
        if self.p.is_null() || self.n == 0 {
            &[]
        } else {
            // SAFETY: caller contract.
            unsafe { std::slice::from_raw_parts(self.p, self.n) }
        }
    }
}

#[repr(C)]
pub struct Cfg {
    pub bone_unit_scale: u8,
    pub keep_hpl3_anims: u8,
    pub convert_unit: u8,
    pub convert_unit_any: u8,
    pub unscaled_skeleton: u8,
    pub load_and_save_msh: u8,
    pub force_cache_skip_saving: u8,
    pub use_fastload: u8,
    pub mesh_flags: u32,
    pub mesh_cache_dir: Str,
    pub working_dir: Str,
    pub fastload_material: Str,
}

#[repr(C)]
pub struct Bone {
    pub name: Str,
    pub sid: Str,
    pub m: [f32; 16],
    pub inv_bind: [f32; 16],
    pub unit_scale: f32,
    pub parent: i32,
    pub value: i32,
    pub has_unit_scale: i32,
    pub has_inv_bind: i32,
}

#[repr(C)]
pub struct Collider {
    pub m: [f32; 16],
    pub size: [f32; 3],
    pub kind: i32,
    pub char_collider: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Pair {
    pub vtx: i32,
    pub bone: i32,
    pub weight: f32,
}

#[repr(C)]
pub struct VbCfg {
    pub draw_type: i32,
    pub usage_type: i32,
    pub reserve_vtx: i32,
    pub reserve_idx: i32,
}

#[repr(C)]
pub struct Array {
    pub kind: i32,
    pub format: i32,
    pub prog_var: i32,
    pub elem_num: i32,
    pub repeats: i32,
    pub count: usize,
    pub data: *const u8,
    pub bytes: usize,
}

pub const LOG: i32 = 0;
pub const WARNING: i32 = 1;
pub const ERROR: i32 = 2;

type Ctx = *mut c_void;

#[repr(C)]
pub struct Builder {
    ctx: Ctx,
    log: extern "C" fn(Ctx, i32, Str),
    file_exists: extern "C" fn(Ctx, Str) -> bool,
    file_date: extern "C" fn(Ctx, Str) -> i64,
    cache_file: extern "C" fn(Ctx, Str, Str) -> Str,
    relative_dir: extern "C" fn(Ctx, Str) -> Str,
    save_mesh: extern "C" fn(Ctx, Str) -> bool,
    save_anim: extern "C" fn(Ctx, Str) -> bool,
    mesh_new: extern "C" fn(Ctx, Str, Str),
    mesh_set_path: extern "C" fn(Ctx, Str),
    mesh_drop: extern "C" fn(Ctx),
    mesh_compile: extern "C" fn(Ctx),
    skeleton_begin: extern "C" fn(Ctx),
    bone: extern "C" fn(Ctx, *const Bone),
    skeleton_end: extern "C" fn(Ctx, i32),
    node: extern "C" fn(Ctx, i32, Str, *const f32, i32),
    sub_select: extern "C" fn(Ctx, i32),
    sub_new: extern "C" fn(Ctx, Str, *const f32, *const f32, i32),
    material_create: extern "C" fn(Ctx, Str) -> usize,
    sub_material: extern "C" fn(Ctx, Str, usize),
    sub_collider: extern "C" fn(Ctx, *const Collider),
    sub_pairs: extern "C" fn(Ctx, *const Pair, usize),
    sub_vb_new: extern "C" fn(Ctx, *const VbCfg),
    sub_vb_array: extern "C" fn(Ctx, *const Array),
    sub_vb_indices: extern "C" fn(Ctx, *const u32, usize),
    sub_vb_transform: extern "C" fn(Ctx, *const f32),
    sub_vb_compile: extern "C" fn(Ctx),
    sub_vb_attach: extern "C" fn(Ctx),
    sub_compile: extern "C" fn(Ctx),
    anim_new: extern "C" fn(Ctx, Str, Str, Str, f32, usize),
    anim_track: extern "C" fn(Ctx, Str, i32, *const Key, usize),
    anim_unit_scale: extern "C" fn(Ctx, f32),
    anim_set_path: extern "C" fn(Ctx, Str),
    anim_end: extern "C" fn(Ctx, i32),
}

/// The builder callbacks with Rust types.
#[derive(Clone, Copy)]
pub struct Sink<'a>(pub &'a Builder);

impl Sink<'_> {
    pub fn log(&self, level: i32, msg: &[u8]) {
        (self.0.log)(self.0.ctx, level, Str::new(msg));
    }

    pub fn error(&self, msg: &str) {
        self.log(ERROR, msg.as_bytes());
    }

    pub fn file_exists(&self, path: &[u8]) -> bool {
        (self.0.file_exists)(self.0.ctx, Str::new(path))
    }

    pub fn file_date(&self, path: &[u8]) -> i64 {
        (self.0.file_date)(self.0.ctx, Str::new(path))
    }

    /// The builder keeps its answer only until the next call, so it is copied out.
    pub fn cache_file(&self, path: &[u8], ext: &[u8]) -> Vec<u8> {
        let s = (self.0.cache_file)(self.0.ctx, Str::new(path), Str::new(ext));
        // SAFETY: the builder returns a string it keeps until its next call.
        unsafe { s.bytes() }.to_vec()
    }

    pub fn relative_dir(&self, path: &[u8]) -> Vec<u8> {
        let s = (self.0.relative_dir)(self.0.ctx, Str::new(path));
        // SAFETY: as `cache_file`.
        unsafe { s.bytes() }.to_vec()
    }

    pub fn save_mesh(&self, cache: &[u8]) -> bool {
        (self.0.save_mesh)(self.0.ctx, Str::new(cache))
    }

    pub fn save_anim(&self, cache: &[u8]) -> bool {
        (self.0.save_anim)(self.0.ctx, Str::new(cache))
    }

    pub fn sub_select(&self, index: i32) {
        (self.0.sub_select)(self.0.ctx, index);
    }

    pub fn mesh_new(&self, name: &[u8], path: &[u8]) {
        (self.0.mesh_new)(self.0.ctx, Str::new(name), Str::new(path));
    }

    pub fn mesh_set_path(&self, path: &[u8]) {
        (self.0.mesh_set_path)(self.0.ctx, Str::new(path));
    }

    pub fn mesh_drop(&self) {
        (self.0.mesh_drop)(self.0.ctx);
    }

    pub fn mesh_compile(&self) {
        (self.0.mesh_compile)(self.0.ctx);
    }

    pub fn skeleton_begin(&self) {
        (self.0.skeleton_begin)(self.0.ctx);
    }

    pub fn bone(&self, b: &Bone) {
        (self.0.bone)(self.0.ctx, b);
    }

    pub fn skeleton_end(&self, file_units: bool) {
        (self.0.skeleton_end)(self.0.ctx, file_units.into());
    }

    pub fn node(&self, parent: i32, name: &[u8], m: &[f32; 16], flags: i32) {
        (self.0.node)(self.0.ctx, parent, Str::new(name), m.as_ptr(), flags);
    }

    /// `None` leaves the sub mesh default.
    pub fn sub_new(
        &self,
        name: &[u8],
        m: Option<&[f32; 16]>,
        scale: Option<&[f32; 3]>,
        collide: Option<bool>,
    ) {
        (self.0.sub_new)(
            self.0.ctx,
            Str::new(name),
            m.map_or(std::ptr::null(), |m| m.as_ptr()),
            scale.map_or(std::ptr::null(), |s| s.as_ptr()),
            collide.map_or(-1, i32::from),
        );
    }

    pub fn material_create(&self, name: &[u8]) -> usize {
        (self.0.material_create)(self.0.ctx, Str::new(name))
    }

    pub fn sub_material(&self, name: &[u8], material: usize) {
        (self.0.sub_material)(self.0.ctx, Str::new(name), material);
    }

    pub fn sub_collider(&self, c: &Collider) {
        (self.0.sub_collider)(self.0.ctx, c);
    }

    pub fn sub_pairs(&self, pairs: &[Pair]) {
        (self.0.sub_pairs)(self.0.ctx, pairs.as_ptr(), pairs.len());
    }

    pub fn sub_vb_new(&self, cfg: &VbCfg) {
        (self.0.sub_vb_new)(self.0.ctx, cfg);
    }

    pub fn sub_vb_array(&self, a: &Array) {
        (self.0.sub_vb_array)(self.0.ctx, a);
    }

    pub fn sub_vb_indices(&self, idx: &[u32]) {
        (self.0.sub_vb_indices)(self.0.ctx, idx.as_ptr(), idx.len());
    }

    pub fn sub_vb_transform(&self, m: &[f32; 16]) {
        (self.0.sub_vb_transform)(self.0.ctx, m.as_ptr());
    }

    pub fn sub_vb_compile(&self) {
        (self.0.sub_vb_compile)(self.0.ctx);
    }

    pub fn sub_vb_attach(&self) {
        (self.0.sub_vb_attach)(self.0.ctx);
    }

    pub fn sub_compile(&self) {
        (self.0.sub_compile)(self.0.ctx);
    }

    pub fn anim_new(&self, name: &[u8], file: &[u8], anim: Option<&[u8]>, len: f32, tracks: usize) {
        (self.0.anim_new)(
            self.0.ctx,
            Str::new(name),
            Str::new(file),
            anim.map_or(Str::NONE, Str::new),
            len,
            tracks,
        );
    }

    pub fn anim_track(&self, name: &[u8], flags: i32, keys: &[Key]) {
        (self.0.anim_track)(self.0.ctx, Str::new(name), flags, keys.as_ptr(), keys.len());
    }

    /// Sets the unit scale of the track just added.
    pub fn anim_unit_scale(&self, scale: f32) {
        (self.0.anim_unit_scale)(self.0.ctx, scale);
    }

    pub fn anim_set_path(&self, path: &[u8]) {
        (self.0.anim_set_path)(self.0.ctx, Str::new(path));
    }

    pub fn anim_end(&self, add_to_mesh: bool) {
        (self.0.anim_end)(self.0.ctx, add_to_mesh.into());
    }
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Str>() == 16 && size_of::<Cfg>() == 64);
    assert!(size_of::<Bone>() == 184 && size_of::<Collider>() == 84);
    assert!(size_of::<Pair>() == 12 && size_of::<VbCfg>() == 16 && size_of::<Array>() == 48);
};
