//! Image frames owned by Rust: `cFrameTexture`, `cFrameBitmap`, `cFBitmapImage` and the state of
//! `cFrameSubImage`. The host only makes and deletes the GPU texture, the CPU bitmap and the
//! `cFrameSubImage` resource shell.

// Frame objects are handed to the host as raw pointers and linked to each other by raw pointers.
#![allow(clippy::not_unsafe_ptr_arg_deref)]

use super::h3_ops::{ERROR, Ops, Str, WStr};
use crate::graphics::atlas::{Atlas, Pix};
use crate::graphics::bitmap::Surface;
use crate::graphics::uv::subimage_uvs;
use crate::math::Rect2l;
use crate::string_ffi::units;
use crate::types::{Vector2l, Vector3l};
use std::ffi::c_void;

type Ptr = *mut c_void;

fn null() -> Ptr {
    std::ptr::null_mut()
}

#[repr(C)]
pub struct BmpInfo {
    pub size: Vector3l,
    pub format: i32,
    pub bytes_per_pixel: i32,
    pub data: *mut u8,
    pub len: usize,
    pub compressed: bool,
    pub name: Str,
}

impl BmpInfo {
    fn empty() -> Self {
        Self {
            size: Vector3l { x: 0, y: 0, z: 0 },
            format: 0,
            bytes_per_pixel: 0,
            data: std::ptr::null_mut(),
            len: 0,
            compressed: false,
            name: Str::empty(),
        }
    }

    fn surface(&self) -> Surface {
        Surface {
            size: Vector3l {
                x: self.size.x,
                y: self.size.y,
                z: self.size.z,
            },
            format: self.format,
            bytes_per_pixel: self.bytes_per_pixel,
        }
    }

    /// # Safety
    /// The host keeps `data` valid for `len` bytes while the result lives.
    unsafe fn bytes<'a>(&self, dest: bool) -> Option<&'a mut [u8]> {
        if self.data.is_null() || (dest && self.compressed) {
            None
        } else {
            // SAFETY: caller contract.
            Some(unsafe { std::slice::from_raw_parts_mut(self.data, self.len) })
        }
    }
}

#[repr(C)]
pub struct FrHost {
    pub ctx: Ptr,
    pub log: unsafe extern "C" fn(Ptr, i32, *const u8, usize),
    pub tex_new: unsafe extern "C" fn(Ptr) -> Ptr,
    pub tex_delete: unsafe extern "C" fn(Ptr, Ptr),
    /// `CreateFromBitmap` plus clamp-to-edge wrap on S and T.
    pub tex_upload: unsafe extern "C" fn(Ptr, Ptr, Ptr),
    pub tex_size: unsafe extern "C" fn(Ptr, Ptr, *mut Vector3l),
    pub bmp_new: unsafe extern "C" fn(Ptr, i32, i32) -> Ptr,
    pub bmp_delete: unsafe extern "C" fn(Ptr, Ptr),
    pub bmp_info: unsafe extern "C" fn(Ptr, Ptr, *mut BmpInfo),
    /// Clear to (1,0,1,0).
    pub bmp_clear: unsafe extern "C" fn(Ptr, Ptr),
    pub sub_new: unsafe extern "C" fn(Ptr, Str, WStr, Ptr, bool) -> Ptr,
    /// The `Sub` a `cFrameSubImage` shell holds.
    pub sub_rs: unsafe extern "C" fn(Ptr, Ptr) -> Ptr,
}

impl FrHost {
    fn info(&self, bmp: Ptr) -> BmpInfo {
        let mut i = BmpInfo::empty();
        // SAFETY: the host contract of `FrHost`.
        unsafe { (self.bmp_info)(self.ctx, bmp, &raw mut i) };
        i
    }

    fn error(&self, msg: &[u8]) {
        // SAFETY: the host contract of `FrHost`.
        unsafe { (self.log)(self.ctx, ERROR, msg.as_ptr(), msg.len()) }
    }
}

/// Read in place by `cFrameSubImage` (RustFrames.h).
#[repr(C)]
pub struct SubPrefix {
    pub rect: Rect2l,
    pub src: Vector2l,
    pub tex: [[f32; 3]; 4],
    pub update_count: i32,
}

#[repr(C)]
pub struct Sub {
    p: SubPrefix,
    ftex: *mut FrameTex,
    fbmp: *mut FrameBmp,
    img: *mut BmpImage,
    need_uv: bool,
    name: Vec<u8>,
    shell: Ptr,
    host: *const FrHost,
}

struct BmpImage {
    rect: Rect2l,
    sub: *mut Sub,
}

pub struct FrameTex {
    tex: Ptr,
    handle: i32,
    custom: bool,
    pics: i32,
    host: *const FrHost,
}

pub struct FrameBmp {
    bmp: Ptr,
    ftex: *mut FrameTex,
    atlas: Atlas,
    images: Vec<*mut BmpImage>,
    pics: i32,
    handle: i32,
    host: *const FrHost,
}

fn uvs(rect: &Rect2l, src: &Vector2l) -> [[f32; 3]; 4] {
    subimage_uvs(rect, src).map(|v| [v.x, v.y, 0.0])
}

impl Sub {
    fn new(
        host: *const FrHost,
        name: &[u8],
        ftex: *mut FrameTex,
        fbmp: *mut FrameBmp,
        rect: Rect2l,
        src: Vector2l,
        img: *mut BmpImage,
    ) -> *mut Sub {
        Box::into_raw(Box::new(Sub {
            p: SubPrefix {
                rect,
                src,
                tex: uvs(&rect, &src),
                update_count: 0,
            },
            ftex,
            fbmp,
            img,
            need_uv: false,
            name: name.to_vec(),
            shell: null(),
            host,
        }))
    }

    /// `cFrameSubImage::UpdateUvs`.
    fn update_uvs(&mut self) {
        // SAFETY: `img` is null or the live image that links back to this subimage.
        let Some(img) = (unsafe { self.img.as_ref() }) else {
            return;
        };
        self.p.update_count = self.p.update_count.wrapping_add(1);
        self.p.tex = uvs(&img.rect, &self.p.src);
    }

    /// `cFrameSubImage::Flush`.
    fn flush(&mut self) {
        // SAFETY: `fbmp` is null or the live frame bitmap this subimage was added to.
        let Some(f) = (unsafe { self.fbmp.as_mut() }) else {
            return;
        };
        f.update_before_draw();
        if self.need_uv {
            self.update_uvs();
            self.need_uv = false;
        }
    }
}

/// `~cFrameSubImage`.
///
/// # Safety
/// `s` is from `Sub::new`, freed once.
unsafe fn free_sub(s: *mut Sub) {
    // SAFETY: caller contract.
    let b = unsafe { Box::from_raw(s) };
    // SAFETY: the links are null or live (frames outlive their subimages).
    unsafe {
        if let Some(f) = b.fbmp.as_mut() {
            f.atlas.need_reorg = true;
        }
        if let Some(i) = b.img.as_mut() {
            i.sub = std::ptr::null_mut();
        }
    }
}

/// `~cFBitmapImage`.
///
/// # Safety
/// `i` is a boxed image, freed once.
unsafe fn free_image(i: *mut BmpImage) {
    // SAFETY: caller contract.
    let b = unsafe { Box::from_raw(i) };
    // SAFETY: `sub` is null or live.
    if let Some(s) = unsafe { b.sub.as_mut() } {
        s.img = std::ptr::null_mut();
    }
}

impl FrameBmp {
    fn host<'a>(&self) -> &'a FrHost {
        // SAFETY: the host table outlives its frames (it lives in the image manager).
        unsafe { &*self.host }
    }

    /// `cFrameBitmap::cFrameBitmap`.
    fn new(host: *const FrHost, bmp: Ptr, ftex: *mut FrameTex, handle: i32) -> *mut FrameBmp {
        // SAFETY: the host table is live.
        let h = unsafe { &*host };
        // SAFETY: the host contract of `FrHost`.
        unsafe { (h.bmp_clear)(h.ctx, bmp) };
        let info = h.info(bmp);
        Box::into_raw(Box::new(FrameBmp {
            bmp,
            ftex,
            atlas: Atlas::new(info.size.x, info.size.y, handle),
            images: Vec::new(),
            pics: 0,
            handle,
            host,
        }))
    }

    /// `cFrameBitmap::AddBitmap` (never with an existing subimage). Returns the new
    /// `cFrameSubImage` shell, or null when the bitmap does not fit.
    fn add_bitmap(&mut self, src: Ptr, path: &[u32]) -> Ptr {
        let h = self.host();
        let (dinfo, sinfo) = (h.info(self.bmp), h.info(src));
        // SAFETY: the host keeps both pixel buffers valid for this call.
        let (ddata, sdata) = unsafe { (dinfo.bytes(true), sinfo.bytes(false)) };
        let mut dst = Pix {
            surface: dinfo.surface(),
            data: ddata,
        };
        let added = self
            .atlas
            .add(&mut dst, &sinfo.surface(), sdata.as_deref(), false);
        let Some(added) = added else {
            return null();
        };
        let img = Box::into_raw(Box::new(BmpImage {
            rect: added.rect,
            sub: std::ptr::null_mut(),
        }));
        self.images.push(img);
        self.pics = self.atlas.pic_count;
        // SAFETY: `ftex` is the live frame texture of this bitmap.
        if let Some(t) = unsafe { self.ftex.as_mut() } {
            t.pics = self.pics;
        }
        // SAFETY: the host keeps the name valid for this call.
        let name = unsafe { units(sinfo.name.p, sinfo.name.n) };
        let sub = Sub::new(
            self.host,
            name,
            self.ftex,
            std::ptr::from_mut(self),
            added.rect,
            Vector2l {
                x: dinfo.size.x,
                y: dinfo.size.y,
            },
            img,
        );
        // SAFETY: `img` and `sub` were just boxed.
        unsafe {
            (*img).sub = sub;
            let shell = (h.sub_new)(h.ctx, Str::of(name), WStr::of(path), sub.cast(), false);
            (*sub).shell = shell;
            shell
        }
    }

    /// `cFrameBitmap::Reorganize`.
    fn reorganize(&mut self) {
        let live: Vec<(*mut Sub, Rect2l)> = self
            .images
            .iter()
            // SAFETY: the list holds live images.
            .filter_map(|&i| unsafe { i.as_ref() }.filter(|i| !i.sub.is_null()))
            .map(|i| (i.sub, i.rect))
            .collect();
        let rects: Vec<Rect2l> = live.iter().map(|l| l.1).collect();
        let h = self.host();
        let dinfo = h.info(self.bmp);
        // SAFETY: the host keeps the pixel buffer valid for this call.
        let mut dst = Pix {
            surface: dinfo.surface(),
            data: unsafe { dinfo.bytes(true) },
        };
        let out = self.atlas.reorganize(&mut dst, &rects);
        for i in self.images.drain(..) {
            // SAFETY: the list holds live images; deleting unlinks their subimages.
            unsafe { free_image(i) };
        }
        if out.fit.iter().any(|f| !f) {
            self.host()
                .error(b"When reorganizing images according to previous order an image still does not fit!!!\n");
        }
        for &idx in &out.order {
            let (Some(&(sub, _)), Some(&rect)) = (live.get(idx), out.rects.get(idx)) else {
                continue;
            };
            let img = Box::into_raw(Box::new(BmpImage { rect, sub }));
            // SAFETY: `sub` is a live subimage of this frame.
            unsafe { (*sub).img = img };
            self.images.push(img);
        }
        self.pics = self.atlas.pic_count;
        if !out.order.is_empty() {
            // SAFETY: `ftex` is the live frame texture of this bitmap.
            if let Some(t) = unsafe { self.ftex.as_mut() } {
                t.pics = self.pics;
            }
        }
        for &i in &self.images {
            // SAFETY: the list holds live images; `sub` is set for each.
            unsafe {
                if let Some(s) = (*i).sub.as_mut() {
                    s.need_uv = true;
                }
            }
        }
    }

    /// `cFrameBitmap::FlushToTexture`.
    fn flush_to_texture(&mut self) -> bool {
        if !std::mem::take(&mut self.atlas.updated) {
            return false;
        }
        let h = self.host();
        // SAFETY: `ftex` is the live frame texture; the host contract of `FrHost`.
        unsafe { (h.tex_upload)(h.ctx, (*self.ftex).tex, self.bmp) };
        true
    }

    /// `cFrameBitmap::UpdateBeforeDraw`.
    fn update_before_draw(&mut self) {
        if std::mem::take(&mut self.atlas.need_reorg) {
            self.reorganize();
        }
        self.flush_to_texture();
    }
}

/// `~cFrameBitmap`.
///
/// # Safety
/// `f` is from `FrameBmp::new`, freed once.
unsafe fn free_bmp_frame(f: *mut FrameBmp) {
    // SAFETY: caller contract.
    let b = unsafe { Box::from_raw(f) };
    for &i in &b.images {
        // SAFETY: the list holds live images.
        unsafe { free_image(i) };
    }
    let h = b.host();
    // SAFETY: the host contract of `FrHost`.
    unsafe { (h.bmp_delete)(h.ctx, b.bmp) };
}

impl FrameTex {
    fn new(host: *const FrHost, tex: Ptr, handle: i32, custom: bool) -> *mut FrameTex {
        Box::into_raw(Box::new(FrameTex {
            tex,
            handle,
            custom,
            pics: 0,
            host,
        }))
    }
}

/// `~cFrameTexture`.
///
/// # Safety
/// `f` is from `FrameTex::new`, freed once.
unsafe fn free_tex_frame(f: *mut FrameTex) {
    // SAFETY: caller contract.
    let b = unsafe { Box::from_raw(f) };
    if !b.tex.is_null() {
        // SAFETY: the host table outlives its frames; the host contract of `FrHost`.
        unsafe { ((*b.host).tex_delete)((*b.host).ctx, b.tex) };
    }
}

fn bmp_frame<'a>(p: Ptr) -> Option<&'a mut FrameBmp> {
    // SAFETY: the flows pass frame bitmaps made by `FrameApi::new_bitmap_frame`.
    unsafe { p.cast::<FrameBmp>().as_mut() }
}

fn tex_frame<'a>(p: Ptr) -> Option<&'a mut FrameTex> {
    // SAFETY: the flows pass frame textures made by this module.
    unsafe { p.cast::<FrameTex>().as_mut() }
}

fn image<'a>(host: &FrHost, shell: Ptr) -> Option<&'a Sub> {
    // SAFETY: the flows pass live `cFrameSubImage` shells; the host returns their Rust half.
    unsafe { (host.sub_rs)(host.ctx, shell).cast::<Sub>().as_ref() }
}

/// The frame calls of the image flows (`cImageManager`). Tests drive the flows with a stub host
/// through `OpsFrames`.
pub trait FrameApi {
    fn new_bitmap_frame(&self, ops: &Ops, w: i32, h: i32, handle: i32) -> Ptr;
    fn add_bitmap(&self, ops: &Ops, frame: Ptr, bmp: Ptr, path: &[u32]) -> Ptr;
    fn new_custom_frame(&self, ops: &Ops, tex: Ptr, handle: i32) -> Ptr;
    fn bmp_tex(&self, ops: &Ops, f: Ptr) -> usize;
    fn bmp_handle(&self, ops: &Ops, f: Ptr) -> i32;
    fn bmp_is_full(&self, ops: &Ops, f: Ptr) -> bool;
    fn bmp_is_locked(&self, ops: &Ops, f: Ptr) -> bool;
    fn bmp_additions(&self, ops: &Ops, f: Ptr) -> i64;
    fn bmp_reorganize(&self, ops: &Ops, f: Ptr);
    fn bmp_flush(&self, ops: &Ops, f: Ptr) -> bool;
    fn bmp_set_locked(&self, ops: &Ops, f: Ptr, locked: bool);
    fn bmp_dec_pic(&self, ops: &Ops, f: Ptr);
    fn bmp_delete(&self, ops: &Ops, f: Ptr);
    fn tex_dec_pic(&self, ops: &Ops, f: Ptr);
    fn tex_is_empty(&self, ops: &Ops, f: Ptr) -> bool;
    fn tex_is_custom(&self, ops: &Ops, f: Ptr) -> bool;
    fn tex_handle(&self, ops: &Ops, f: Ptr) -> i32;
    fn tex_delete(&self, ops: &Ops, f: Ptr);
    fn img_tex(&self, ops: &Ops, img: Ptr) -> usize;
    fn img_bmp(&self, ops: &Ops, img: Ptr) -> usize;
    fn bitmap_size(&self, ops: &Ops, bmp: Ptr) -> (i32, i32);
    fn bitmap_delete(&self, ops: &Ops, bmp: Ptr);
}

pub struct Real {
    pub host: *const FrHost,
}

impl Real {
    fn host(&self) -> &FrHost {
        // SAFETY: set by `ohpl_h3f_image_init` from the manager's own table.
        unsafe { &*self.host }
    }
}

impl FrameApi for Real {
    fn new_bitmap_frame(&self, _: &Ops, w: i32, h: i32, handle: i32) -> Ptr {
        let host = self.host();
        // SAFETY: the host contract of `FrHost`.
        let (tex, bmp) = unsafe { ((host.tex_new)(host.ctx), (host.bmp_new)(host.ctx, w, h)) };
        let ftex = FrameTex::new(self.host, tex, handle, false);
        FrameBmp::new(self.host, bmp, ftex, handle).cast()
    }

    fn add_bitmap(&self, _: &Ops, frame: Ptr, bmp: Ptr, path: &[u32]) -> Ptr {
        bmp_frame(frame).map_or(null(), |f| f.add_bitmap(bmp, path))
    }

    fn new_custom_frame(&self, _: &Ops, tex: Ptr, handle: i32) -> Ptr {
        FrameTex::new(self.host, tex, handle, true).cast()
    }

    fn bmp_tex(&self, _: &Ops, f: Ptr) -> usize {
        bmp_frame(f).map_or(0, |f| f.ftex.expose_provenance())
    }

    fn bmp_handle(&self, _: &Ops, f: Ptr) -> i32 {
        bmp_frame(f).map_or(0, |f| f.handle)
    }

    fn bmp_is_full(&self, _: &Ops, f: Ptr) -> bool {
        bmp_frame(f).is_some_and(|f| f.atlas.full)
    }

    fn bmp_is_locked(&self, _: &Ops, f: Ptr) -> bool {
        bmp_frame(f).is_some_and(|f| f.atlas.locked)
    }

    fn bmp_additions(&self, _: &Ops, f: Ptr) -> i64 {
        bmp_frame(f).map_or(0, |f| i64::from(f.atlas.additions))
    }

    fn bmp_reorganize(&self, _: &Ops, f: Ptr) {
        if let Some(f) = bmp_frame(f) {
            f.reorganize();
        }
    }

    fn bmp_flush(&self, _: &Ops, f: Ptr) -> bool {
        bmp_frame(f).is_some_and(FrameBmp::flush_to_texture)
    }

    fn bmp_set_locked(&self, _: &Ops, f: Ptr, locked: bool) {
        if let Some(f) = bmp_frame(f) {
            f.atlas.locked = locked;
        }
    }

    fn bmp_dec_pic(&self, _: &Ops, f: Ptr) {
        if let Some(f) = bmp_frame(f)
            && f.pics > 0
        {
            f.pics -= 1;
        }
    }

    fn bmp_delete(&self, _: &Ops, f: Ptr) {
        if !f.is_null() {
            // SAFETY: the flows delete each frame bitmap once.
            unsafe { free_bmp_frame(f.cast()) };
        }
    }

    fn tex_dec_pic(&self, _: &Ops, f: Ptr) {
        if let Some(f) = tex_frame(f)
            && f.pics > 0
        {
            f.pics -= 1;
        }
    }

    fn tex_is_empty(&self, _: &Ops, f: Ptr) -> bool {
        tex_frame(f).is_some_and(|f| f.pics <= 0)
    }

    fn tex_is_custom(&self, _: &Ops, f: Ptr) -> bool {
        tex_frame(f).is_some_and(|f| f.custom)
    }

    fn tex_handle(&self, _: &Ops, f: Ptr) -> i32 {
        tex_frame(f).map_or(0, |f| f.handle)
    }

    fn tex_delete(&self, _: &Ops, f: Ptr) {
        if !f.is_null() {
            // SAFETY: the flows delete each frame texture once.
            unsafe { free_tex_frame(f.cast()) };
        }
    }

    fn img_tex(&self, _: &Ops, img: Ptr) -> usize {
        image(self.host(), img).map_or(0, |s| s.ftex.expose_provenance())
    }

    fn img_bmp(&self, _: &Ops, img: Ptr) -> usize {
        image(self.host(), img).map_or(0, |s| s.fbmp.expose_provenance())
    }

    fn bitmap_size(&self, _: &Ops, bmp: Ptr) -> (i32, i32) {
        let i = self.host().info(bmp);
        (i.size.x, i.size.y)
    }

    fn bitmap_delete(&self, _: &Ops, bmp: Ptr) {
        let h = self.host();
        // SAFETY: the host contract of `FrHost`.
        unsafe { (h.bmp_delete)(h.ctx, bmp) };
    }
}

/// Bookkeeping of `cImageManager`: the frame lists, handle counter and frame size.
pub struct Frames {
    pub bitmap: Vec<usize>,
    pub textures: std::collections::BTreeMap<i32, usize>,
    pub handle: i32,
    pub size: (i32, i32),
    pub api: Box<dyn FrameApi>,
}

impl Default for Frames {
    fn default() -> Self {
        Self {
            bitmap: Vec::new(),
            textures: std::collections::BTreeMap::new(),
            handle: 0,
            size: (512, 512),
            #[cfg(test)]
            api: Box::new(OpsFrames),
            #[cfg(not(test))]
            api: Box::new(Real {
                host: std::ptr::null(),
            }),
        }
    }
}

/// # Safety
/// `s` is a live `Sub`, `shell` its `cFrameSubImage`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fr_sub_free(s: Ptr, _users: u32) {
    if !s.is_null() {
        // SAFETY: caller contract.
        unsafe { free_sub(s.cast()) };
    }
}

/// # Safety
/// `s` is a live `Sub`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fr_sub_texture(s: *const c_void) -> Ptr {
    // SAFETY: caller contract; `ftex` is null or live.
    unsafe { s.cast::<Sub>().as_ref().and_then(|s| s.ftex.as_ref()) }.map_or(null(), |t| t.tex)
}

/// # Safety
/// `s` is a live `Sub`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fr_sub_flush(s: Ptr) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { s.cast::<Sub>().as_mut() } {
        s.flush();
    }
}

/// # Safety
/// `f` is a live frame texture; `pos` and `size` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fr_ftex_custom_image(
    f: Ptr,
    pos: *const Vector2l,
    size: *const Vector2l,
) -> Ptr {
    // SAFETY: caller contract.
    let (Some(f), Some(pos), Some(size)) = (
        unsafe { f.cast::<FrameTex>().as_mut() },
        unsafe { pos.as_ref() },
        unsafe { size.as_ref() },
    ) else {
        return null();
    };
    if !f.custom {
        return null();
    }
    f.pics = f.pics.wrapping_add(1);
    // SAFETY: the host table outlives its frames; the host contract of `FrHost`.
    let h = unsafe { &*f.host };
    let mut tsize = Vector3l { x: 0, y: 0, z: 0 };
    // SAFETY: the host contract of `FrHost`.
    unsafe { (h.tex_size)(h.ctx, f.tex, &raw mut tsize) };
    let rect = Rect2l {
        x: pos.x,
        y: pos.y,
        w: size.x,
        h: size.y,
    };
    let sub = Sub::new(
        f.host,
        b"",
        f,
        std::ptr::null_mut(),
        rect,
        Vector2l {
            x: tsize.x,
            y: tsize.y,
        },
        std::ptr::null_mut(),
    );
    // SAFETY: `sub` was just boxed.
    unsafe {
        let shell = (h.sub_new)(h.ctx, Str::empty(), WStr::empty(), sub.cast(), true);
        (*sub).shell = shell;
        shell
    }
}

/// # Safety
/// `f` is a live frame texture.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fr_ftex_texture(f: *const c_void) -> Ptr {
    // SAFETY: caller contract.
    unsafe { f.cast::<FrameTex>().as_ref() }.map_or(null(), |f| f.tex)
}

/// The frame calls routed through the host `Ops`, for the flow tests' stub host.
#[cfg(test)]
pub struct OpsFrames;

#[cfg(test)]
impl FrameApi for OpsFrames {
    fn new_bitmap_frame(&self, ops: &Ops, w: i32, h: i32, handle: i32) -> Ptr {
        let mut cr = super::h3_ops::Create::new(super::h3_ops::k::IMG_BITMAP_FRAME);
        cr.i = [w, h, handle, 0];
        ops.create(&mut cr)
    }
    fn add_bitmap(&self, ops: &Ops, frame: Ptr, bmp: Ptr, path: &[u32]) -> Ptr {
        let mut cr = super::h3_ops::Create::new(super::h3_ops::k::IMG_ADD_BITMAP);
        cr.ptr = frame;
        cr.ptr2 = bmp;
        cr.path = WStr::of(path);
        ops.create(&mut cr)
    }
    fn new_custom_frame(&self, ops: &Ops, tex: Ptr, handle: i32) -> Ptr {
        let mut cr = super::h3_ops::Create::new(super::h3_ops::k::IMG_CUSTOM_FRAME);
        cr.ptr = tex;
        cr.i[0] = handle;
        ops.create(&mut cr)
    }
    fn bmp_tex(&self, ops: &Ops, f: Ptr) -> usize {
        ops.call(super::h3_ops::c::BMPFRAME_TEX, f, 0) as usize
    }
    fn bmp_handle(&self, ops: &Ops, f: Ptr) -> i32 {
        ops.call(super::h3_ops::c::BMPFRAME_HANDLE, f, 0) as i32
    }
    fn bmp_is_full(&self, ops: &Ops, f: Ptr) -> bool {
        ops.call(super::h3_ops::c::BMPFRAME_IS_FULL, f, 0) != 0
    }
    fn bmp_is_locked(&self, ops: &Ops, f: Ptr) -> bool {
        ops.call(super::h3_ops::c::BMPFRAME_IS_LOCKED, f, 0) != 0
    }
    fn bmp_additions(&self, ops: &Ops, f: Ptr) -> i64 {
        ops.call(super::h3_ops::c::BMPFRAME_ADDITIONS, f, 0)
    }
    fn bmp_reorganize(&self, ops: &Ops, f: Ptr) {
        ops.call(super::h3_ops::c::BMPFRAME_REORGANIZE, f, 0);
    }
    fn bmp_flush(&self, ops: &Ops, f: Ptr) -> bool {
        ops.call(super::h3_ops::c::BMPFRAME_FLUSH, f, 0) != 0
    }
    fn bmp_set_locked(&self, ops: &Ops, f: Ptr, locked: bool) {
        ops.call(super::h3_ops::c::BMPFRAME_SET_LOCKED, f, i64::from(locked));
    }
    fn bmp_dec_pic(&self, ops: &Ops, f: Ptr) {
        ops.call(super::h3_ops::c::BMPFRAME_DEC_PIC, f, 0);
    }
    fn bmp_delete(&self, ops: &Ops, f: Ptr) {
        ops.call(super::h3_ops::c::BMPFRAME_DELETE, f, 0);
    }
    fn tex_dec_pic(&self, ops: &Ops, f: Ptr) {
        ops.call(super::h3_ops::c::FRAME_DEC_PIC, f, 0);
    }
    fn tex_is_empty(&self, ops: &Ops, f: Ptr) -> bool {
        ops.call(super::h3_ops::c::FRAME_IS_EMPTY, f, 0) != 0
    }
    fn tex_is_custom(&self, ops: &Ops, f: Ptr) -> bool {
        ops.call(super::h3_ops::c::FRAME_IS_CUSTOM, f, 0) != 0
    }
    fn tex_handle(&self, ops: &Ops, f: Ptr) -> i32 {
        ops.call(super::h3_ops::c::FRAME_HANDLE, f, 0) as i32
    }
    fn tex_delete(&self, ops: &Ops, f: Ptr) {
        ops.call(super::h3_ops::c::FRAME_DELETE, f, 0);
    }
    fn img_tex(&self, ops: &Ops, img: Ptr) -> usize {
        ops.call(super::h3_ops::c::IMG_FRAME_TEX, img, 0) as usize
    }
    fn img_bmp(&self, ops: &Ops, img: Ptr) -> usize {
        ops.call(super::h3_ops::c::IMG_FRAME_BMP, img, 0) as usize
    }
    fn bitmap_size(&self, ops: &Ops, bmp: Ptr) -> (i32, i32) {
        (
            ops.call(super::h3_ops::c::BMP_WIDTH, bmp, 0) as i32,
            ops.call(super::h3_ops::c::BMP_HEIGHT, bmp, 0) as i32,
        )
    }
    fn bitmap_delete(&self, ops: &Ops, bmp: Ptr) {
        ops.call(super::h3_ops::c::BMP_DELETE, bmp, 0);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    #[derive(Default)]
    struct Stub {
        log: RefCell<Vec<String>>,
        bmps: RefCell<Vec<Vec<u8>>>,
        names: RefCell<Vec<Vec<u8>>>,
    }

    fn stub(c: Ptr) -> &'static Stub {
        // SAFETY: the tests pass a live `Stub` as ctx.
        unsafe { &*c.cast::<Stub>() }
    }

    unsafe extern "C" fn log(c: Ptr, _: i32, m: *const u8, n: usize) {
        // SAFETY: the host contract of `FrHost`.
        let m = unsafe { units(m, n) };
        stub(c)
            .log
            .borrow_mut()
            .push(String::from_utf8_lossy(m).into());
    }
    unsafe extern "C" fn tex_new(_: Ptr) -> Ptr {
        Box::into_raw(Box::new(0u32)).cast()
    }
    unsafe extern "C" fn tex_delete(c: Ptr, t: Ptr) {
        stub(c).log.borrow_mut().push("tex-delete".into());
        // SAFETY: from `tex_new`.
        drop(unsafe { Box::from_raw(t.cast::<u32>()) });
    }
    unsafe extern "C" fn tex_upload(_: Ptr, t: Ptr, _: Ptr) {
        // SAFETY: from `tex_new`.
        unsafe { *t.cast::<u32>() += 1 };
    }
    unsafe extern "C" fn tex_size(_: Ptr, _: Ptr, s: *mut Vector3l) {
        // SAFETY: the caller passes a writable size.
        unsafe {
            *s = Vector3l {
                x: 100,
                y: 50,
                z: 1,
            }
        };
    }
    unsafe extern "C" fn bmp_new(c: Ptr, w: i32, h: i32) -> Ptr {
        let s = stub(c);
        s.bmps.borrow_mut().push(vec![0; (w * h * 4) as usize]);
        s.names.borrow_mut().push(format!("{w}x{h}").into_bytes());
        // 1-based index as the handle.
        s.bmps.borrow().len() as Ptr
    }
    unsafe extern "C" fn bmp_delete(c: Ptr, _: Ptr) {
        stub(c).log.borrow_mut().push("bmp-delete".into());
    }
    unsafe extern "C" fn bmp_info(c: Ptr, b: Ptr, i: *mut BmpInfo) {
        let s = stub(c);
        let idx = b as usize - 1;
        let mut bm = s.bmps.borrow_mut();
        let Some(data) = bm.get_mut(idx) else { return };
        let side = ((data.len() / 4) as f64).sqrt() as i32;
        // SAFETY: the caller passes a writable info.
        let i = unsafe { &mut *i };
        i.size = Vector3l {
            x: side,
            y: side,
            z: 1,
        };
        i.format = crate::graphics::formats::RGBA;
        i.bytes_per_pixel = 4;
        i.data = data.as_mut_ptr();
        i.len = data.len();
        i.compressed = false;
        let names = s.names.borrow();
        i.name = Str::of(names.get(idx).map_or(b"", Vec::as_slice));
    }
    unsafe extern "C" fn bmp_clear(c: Ptr, b: Ptr) {
        let s = stub(c);
        if let Some(d) = s.bmps.borrow_mut().get_mut(b as usize - 1) {
            d.fill(0xff);
        }
    }
    unsafe extern "C" fn sub_new(_: Ptr, _: Str, _: WStr, rs: Ptr, _: bool) -> Ptr {
        rs
    }
    unsafe extern "C" fn sub_rs(_: Ptr, shell: Ptr) -> Ptr {
        shell
    }

    fn host(s: &Stub) -> FrHost {
        FrHost {
            ctx: std::ptr::from_ref(s).cast_mut().cast(),
            log,
            tex_new,
            tex_delete,
            tex_upload,
            tex_size,
            bmp_new,
            bmp_delete,
            bmp_info,
            bmp_clear,
            sub_new,
            sub_rs,
        }
    }

    unsafe extern "C" fn n_log(_: Ptr, _: i32, _: *const u8, _: usize) {}
    unsafe extern "C" fn n_now(_: Ptr) -> u64 {
        0
    }
    unsafe extern "C" fn n_path(
        _: Ptr,
        _: *const u8,
        _: usize,
        _: *mut i32,
        _: *mut usize,
    ) -> *const u32 {
        std::ptr::null()
    }
    unsafe extern "C" fn n_info(_: Ptr, _: Ptr, _: *mut super::super::h3_ops::Info) {}
    unsafe extern "C" fn n_destroy(_: Ptr, _: Ptr) {}
    unsafe extern "C" fn n_done(_: Ptr, _: *const u8, _: usize, _: u64) {}
    unsafe extern "C" fn n_create(_: Ptr, _: *mut super::super::h3_ops::Create) -> Ptr {
        null()
    }
    unsafe extern "C" fn n_call(_: Ptr, _: i32, _: Ptr, _: i64) -> i64 {
        0
    }
    unsafe extern "C" fn n_text(
        _: Ptr,
        _: i32,
        _: Ptr,
        _: WStr,
        _: crate::string_ffi::Sink<u8>,
        _: Ptr,
    ) {
    }

    fn noop_ops() -> Ops {
        Ops {
            ctx: null(),
            log: n_log,
            now: n_now,
            file_path: n_path,
            info: n_info,
            destroy: n_destroy,
            load_done: n_done,
            create: n_create,
            call: n_call,
            text: n_text,
        }
    }

    fn sub(shell: Ptr) -> &'static Sub {
        // SAFETY: the stub host returns the `Sub` itself as the shell.
        unsafe { &*shell.cast::<Sub>() }
    }

    #[test]
    fn frames_add_reorganize_and_free() {
        let stub_state = Stub::default();
        let h = host(&stub_state);
        let api = Real { host: &h };
        let ops: &Ops = &noop_ops();
        let f = api.new_bitmap_frame(ops, 64, 64, 7);
        assert_eq!(api.bmp_handle(ops, f), 7);
        assert!(!api.bmp_is_full(ops, f));
        let src = bmp_new_at(&stub_state, 10);
        let a = api.add_bitmap(ops, f, src, &[]);
        let b = api.add_bitmap(ops, f, src, &[]);
        assert!(!a.is_null() && !b.is_null());
        assert_eq!(sub(a).p.rect.w, 10);
        assert_ne!(sub(a).p.rect.x, sub(b).p.rect.x);
        assert!(api.bmp_flush(ops, f));
        assert!(!api.bmp_flush(ops, f));
        assert_eq!(api.img_bmp(ops, a), f.expose_provenance());

        // SAFETY: both shells are live subimages made above.
        unsafe { ohpl_fr_sub_free(a, 0) };
        // The freed subimage asks the frame to repack; the survivor then moves and its uv updates.
        // SAFETY: `b` is the live subimage.
        unsafe { ohpl_fr_sub_flush(b) };
        assert_eq!(sub(b).p.update_count, 1);
        assert_eq!(sub(b).p.rect.w, 10);
        // SAFETY: `b` is the live subimage.
        unsafe { ohpl_fr_sub_free(b, 0) };
        api.bmp_delete(ops, f);
        assert!(stub_state.log.borrow().iter().any(|l| l == "bmp-delete"));
    }

    fn bmp_new_at(s: &Stub, side: i32) -> Ptr {
        // SAFETY: the stub host.
        unsafe { bmp_new(std::ptr::from_ref(s).cast_mut().cast(), side, side) }
    }

    #[test]
    fn custom_frame_images() {
        let stub_state = Stub::default();
        let h = host(&stub_state);
        let api = Real { host: &h };
        let ops: &Ops = &noop_ops();
        // SAFETY: the stub host.
        let tex = unsafe { tex_new(std::ptr::null_mut()) };
        let ft = api.new_custom_frame(ops, tex, 3);
        assert!(api.tex_is_custom(ops, ft));
        assert!(api.tex_is_empty(ops, ft));
        let (p, s) = (Vector2l { x: 5, y: 6 }, Vector2l { x: 20, y: 10 });
        // SAFETY: live frame texture and arguments.
        let img = unsafe { ohpl_fr_ftex_custom_image(ft, &raw const p, &raw const s) };
        assert!(!api.tex_is_empty(ops, ft));
        assert_eq!((sub(img).p.src.x, sub(img).p.src.y), (100, 50));
        // SAFETY: `img` is live.
        unsafe { ohpl_fr_sub_free(img, 1) };
        api.tex_dec_pic(ops, ft);
        assert!(api.tex_is_empty(ops, ft));
        api.tex_delete(ops, ft);
        assert!(stub_state.log.borrow().iter().any(|l| l == "tex-delete"));
    }
}
