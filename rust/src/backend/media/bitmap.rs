//! `iBitmapLoaderDevil` and its DDS / misc loaders: DevIL decoding into an owned `Bitmap` the C++
//! shell copies into a `cBitmap`. Format decisions live in `crate::imageload`.

use super::cfile::{self, File};
use super::log::{Level, Sink, lossy};
use crate::imageload::dds::{self, FMT_ALPHA};
use crate::imageload::devil::{
    self, IL_DXT_NO_COMP, dds_layout, misc_image_count, pixel_format_from_hpl, pixel_format_to_hpl,
    type_from_extension,
};
use crate::string::{file_ext, to_lower, wide_to_utf8};
use crate::sys::il::{
    IL_DXTC_DATA_FORMAT, IL_FLOAT, IL_IMAGE_BYTES_PER_PIXEL, IL_IMAGE_CUBEFLAGS, IL_IMAGE_DEPTH,
    IL_IMAGE_FORMAT, IL_IMAGE_HEIGHT, IL_IMAGE_SIZE_OF_DATA, IL_IMAGE_TYPE, IL_IMAGE_WIDTH,
    IL_KEEP_DXTC_DATA, IL_NUM_IMAGES, IL_NUM_MIPMAPS, IL_ORIGIN_MODE, IL_ORIGIN_SET,
    IL_ORIGIN_UPPER_LEFT, IL_SEEK_END, IL_SEEK_SET, IL_TRUE, IL_UNSIGNED_BYTE, IL_VENDOR,
    IL_VERSION, IL_VERSION_NUM, ilActiveFace, ilActiveImage, ilActiveMipmap, ilBindImage,
    ilDeleteImages, ilEnable, ilGenImages, ilGetDXTCData, ilGetData, ilGetInteger, ilGetString,
    ilInit, ilLoadF, ilSaveF, ilSetInteger, ilSetRead, ilSetWrite, ilTexImage,
};
use core::ffi::{CStr, c_char, c_void};
use std::sync::Once;

/// `ePixelFormat_RGBA32`.
const FMT_RGBA32: u32 = 26;
const FMT_RGBA: u32 = 5;

#[derive(Debug, Default, PartialEq)]
pub struct Bitmap {
    pub width: i32,
    pub height: i32,
    pub depth: i32,
    pub bytes_per_pixel: i32,
    pub format: u32,
    pub compressed: bool,
    pub images: i32,
    pub mips: i32,
    pub setup_data: bool,
    /// `images * mips` planes, image-major.
    pub planes: Vec<Vec<u8>>,
}

impl Bitmap {
    pub fn plane(&self, image: i32, mip: i32) -> Option<&[u8]> {
        let i = usize::try_from(image.checked_mul(self.mips)?.checked_add(mip)?).ok()?;
        self.planes.get(i).map(Vec::as_slice)
    }
}

unsafe extern "C" fn il_close(h: *mut c_void) {
    // SAFETY: handles given to DevIL are `FILE*` from `cfile::open`.
    unsafe { cfile::fclose(h.cast()) };
}

unsafe extern "C" fn il_open(path: *const c_char) -> *mut c_void {
    // SAFETY: DevIL passes a NUL-terminated path.
    unsafe { cfile::fopen(path, c"wb".as_ptr()) }.cast()
}

unsafe extern "C" fn il_putc(c: u8, h: *mut c_void) -> i32 {
    // SAFETY: as `il_close`.
    unsafe { cfile::fputc(i32::from(c), h.cast()) }
}

unsafe extern "C" fn il_seek(h: *mut c_void, off: i32, mode: i32) -> i32 {
    // SAFETY: as `il_close`.
    unsafe { cfile::fseek(h.cast(), off.into(), mode) }
}

unsafe extern "C" fn il_tell(h: *mut c_void) -> i32 {
    // SAFETY: as `il_close`.
    unsafe { cfile::ftell(h.cast()) as i32 }
}

unsafe extern "C" fn il_write(buf: *const c_void, size: u32, num: u32, h: *mut c_void) -> i32 {
    // SAFETY: as `il_close`; DevIL passes a buffer of `size * num` bytes.
    unsafe { cfile::fwrite(buf, size as usize, num as usize, h.cast()) as i32 }
}

unsafe extern "C" fn il_eof(h: *mut c_void) -> u8 {
    // SAFETY: as `il_close`.
    unsafe {
        let orig = il_tell(h) as u32;
        il_seek(h, 0, IL_SEEK_END);
        let size = il_tell(h) as u32;
        il_seek(h, orig as i32, IL_SEEK_SET);
        u8::from(il_tell(h) as u32 >= size)
    }
}

unsafe extern "C" fn il_getc(h: *mut c_void) -> i32 {
    // SAFETY: as `il_close`.
    unsafe { cfile::fgetc(h.cast()) }
}

unsafe extern "C" fn il_read(buf: *mut c_void, size: u32, num: u32, h: *mut c_void) -> i32 {
    // SAFETY: as `il_close`; DevIL passes a buffer of `size * num` bytes.
    unsafe { cfile::fread(buf, size as usize, num as usize, h.cast()) as i32 }
}

static INIT: Once = Once::new();

fn ostr(p: *const c_char) -> String {
    if p.is_null() {
        "(null)".into()
    } else {
        // SAFETY: DevIL returns NUL-terminated strings.
        lossy(unsafe { CStr::from_ptr(p) }.to_bytes())
    }
}

fn initialize(log: Sink) {
    INIT.call_once(|| {
        log(Level::Log, b" Initializing DevIL\n");
        // SAFETY: DevIL global setup; the callbacks match the DevIL signatures.
        unsafe {
            ilInit();
            log(
                Level::Log,
                format!("  Vendor String: {}\n", ostr(ilGetString(IL_VENDOR))).as_bytes(),
            );
            log(
                Level::Log,
                format!("  Version String: {}\n", ostr(ilGetString(IL_VERSION_NUM))).as_bytes(),
            );
            let version = ilGetInteger(IL_VERSION_NUM);
            log(Level::Log, format!("  Version Number: {version}\n").as_bytes());
            if i64::from(version) != i64::from(IL_VERSION) {
                for l in [
                    "  ***** WARNING *****\n".to_string(),
                    format!("  Expecting IL version {IL_VERSION} and found version {version}\n"),
                    "  Cube Maps may not load correctly in this version and will not render correctly in game\n".to_string(),
                    "  Please use the included version of libIL and not a custom build if you have issues\n".to_string(),
                    "  ***** WARNING *****\n".to_string(),
                ] {
                    log(Level::Log, l.as_bytes());
                }
            }
            ilSetWrite(Some(il_open), Some(il_close), Some(il_putc), Some(il_seek), Some(il_tell), Some(il_write));
            ilSetRead(Some(il_open), Some(il_close), Some(il_eof), Some(il_getc), Some(il_read), Some(il_seek), Some(il_tell));
            ilEnable(IL_ORIGIN_SET);
            ilSetInteger(IL_ORIGIN_MODE, IL_ORIGIN_UPPER_LEFT as i32);
        }
    });
}

fn il_type(path: &[u32]) -> u32 {
    type_from_extension(&to_lower(file_ext(path)))
}

fn message(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

fn load_il_image(path: &[u32], log: Sink) -> bool {
    let ty = il_type(path);
    let name = wide_to_utf8(path);
    if ty == 0 {
        log(
            Level::Error,
            &message(&[b"Filename ", &name, b" has not a valid image extension!\n"]),
        );
        return false;
    }
    let f = cfile::open(&name, c"rb");
    if f.is_null() {
        log(
            Level::Error,
            &message(&[b"Could not open file ", &name, b" for reading!\n"]),
        );
        return false;
    }
    // SAFETY: `f` is an open file handed to DevIL, then closed once.
    let ret = unsafe { ilLoadF(ty, f.cast()) };
    // SAFETY: `f` is open.
    unsafe { cfile::fclose(f) };
    ret != 0
}

fn il_int(mode: u32) -> i32 {
    // SAFETY: DevIL query on the bound image.
    unsafe { ilGetInteger(mode) }
}

fn il_data(size: i32) -> Vec<u8> {
    // SAFETY: DevIL returns the bound image's pixel buffer of `IL_IMAGE_SIZE_OF_DATA` bytes.
    let p = unsafe { ilGetData() };
    match usize::try_from(size) {
        Ok(n) if !p.is_null() => unsafe { std::slice::from_raw_parts(p, n) }.to_vec(),
        _ => Vec::new(),
    }
}

fn gen_image() -> u32 {
    let mut id = 0u32;
    // SAFETY: one id is written.
    unsafe {
        ilGenImages(1, &mut id);
        ilBindImage(id);
    }
    id
}

fn delete_image(id: u32) {
    // SAFETY: one id is read.
    unsafe { ilDeleteImages(1, &id) };
}

/// `cBitmapLoaderDevilMisc::LoadBitmap`.
pub fn load_misc(path: &[u32], log: Sink) -> Option<Bitmap> {
    initialize(log);
    let id = gen_image();
    if !load_il_image(path, log) {
        delete_image(id);
        return None;
    }
    let il_images = il_int(IL_NUM_IMAGES);
    let mut b = Bitmap {
        width: il_int(IL_IMAGE_WIDTH),
        height: il_int(IL_IMAGE_HEIGHT),
        depth: il_int(IL_IMAGE_DEPTH),
        bytes_per_pixel: il_int(IL_IMAGE_BYTES_PER_PIXEL),
        format: pixel_format_to_hpl(il_int(IL_IMAGE_FORMAT) as u32),
        compressed: false,
        images: misc_image_count(il_images),
        mips: 1,
        setup_data: il_images > 1,
        planes: Vec::new(),
    };
    for image in 0..b.images {
        if il_images > 1 {
            // SAFETY: DevIL state change.
            unsafe { ilActiveImage(image as u32) };
        }
        b.planes.push(il_data(il_int(IL_IMAGE_SIZE_OF_DATA)));
    }
    delete_image(id);
    Some(b)
}

fn read_exact(f: *mut File, buf: &mut [u8]) -> bool {
    // SAFETY: `f` is open.
    unsafe { cfile::read(f, buf) == buf.len() }
}

/// `TryLoadBlockCompressedDDS`.
fn try_block(path: &[u8], no_compression: bool) -> Option<Bitmap> {
    let f = cfile::open(path, c"rb");
    if f.is_null() {
        return None;
    }
    let r = block(f, no_compression);
    // SAFETY: `f` is open.
    unsafe { cfile::fclose(f) };
    r
}

fn block(f: *mut File, no_compression: bool) -> Option<Bitmap> {
    let mut header = [0u8; 148];
    // SAFETY: `f` is open.
    let read = unsafe { cfile::read(f, &mut header) };
    // SAFETY: `f` is open.
    let size = unsafe { cfile::size(f) };
    let plan = dds::plan_block(
        header.get(..read)?,
        u64::try_from(size).ok()?,
        no_compression,
    )?;
    // SAFETY: `f` is open.
    if !unsafe { cfile::seek_set(f, i64::try_from(plan.data_offset).ok()?) } {
        return None;
    }
    let mut b = Bitmap {
        width: plan.width as i32,
        height: plan.height as i32,
        depth: 1,
        bytes_per_pixel: plan.bytes_per_pixel,
        format: plan.format,
        compressed: plan.block_size != 0,
        images: plan.images as i32,
        mips: plan.mips as i32,
        setup_data: plan.images > 1 || plan.mips > 1,
        planes: Vec::new(),
    };
    for _ in 0..plan.images {
        for mip in 0..plan.mips {
            let n = usize::try_from(plan.mip_size(mip)?).ok()?;
            let mut buf = vec![0u8; n];
            if !read_exact(f, &mut buf) {
                return None;
            }
            b.planes.push(buf);
        }
    }
    Some(b)
}

/// `TryLoadUncompressedAlphaDDS`.
fn try_alpha(path: &[u8]) -> Option<Bitmap> {
    let f = cfile::open(path, c"rb");
    if f.is_null() {
        return None;
    }
    let r = alpha(f);
    // SAFETY: `f` is open.
    unsafe { cfile::fclose(f) };
    r
}

fn alpha(f: *mut File) -> Option<Bitmap> {
    let mut header = [0u8; 128];
    if !read_exact(f, &mut header) {
        return None;
    }
    // SAFETY: `f` is open.
    let size = unsafe { cfile::size(f) };
    let plan = dds::plan_alpha(&header, u64::try_from(size).ok()?)?;
    let (w, h) = (plan.width as usize, plan.height as usize);
    let mut pixels = vec![0u8; w.checked_mul(h)?];
    for (y, row) in pixels.chunks_exact_mut(w).enumerate() {
        // SAFETY: `f` is open.
        if !unsafe { cfile::seek_set(f, 128 + y as i64 * i64::from(plan.row_bytes)) }
            || !read_exact(f, row)
        {
            return None;
        }
    }
    Some(Bitmap {
        width: plan.width as i32,
        height: plan.height as i32,
        depth: 1,
        bytes_per_pixel: 1,
        format: FMT_ALPHA,
        compressed: false,
        images: 1,
        mips: 1,
        setup_data: false,
        planes: vec![pixels],
    })
}

/// `cBitmapLoaderDevilDDS::LoadBitmap`.
pub fn load_dds(path: &[u32], no_compression: bool, log: Sink) -> Option<Bitmap> {
    initialize(log);
    let name = wide_to_utf8(path);
    if let Some(b) = try_block(&name, no_compression) {
        return Some(b);
    }
    if let Some(b) = try_alpha(&name) {
        return Some(b);
    }

    let id = gen_image();
    // SAFETY: DevIL state change.
    unsafe { ilSetInteger(IL_KEEP_DXTC_DATA, i32::from(IL_TRUE)) };
    if !load_il_image(path, log) {
        delete_image(id);
        return None;
    }
    let cube_flags = il_int(IL_IMAGE_CUBEFLAGS);
    let layout = dds_layout(il_int(IL_NUM_IMAGES), il_int(IL_NUM_MIPMAPS), cube_flags);
    let mut format = pixel_format_to_hpl(il_int(IL_IMAGE_FORMAT) as u32);
    if il_int(IL_IMAGE_TYPE) as u32 == IL_FLOAT && format == FMT_RGBA {
        format = FMT_RGBA32;
    }
    let dxt = il_int(IL_DXTC_DATA_FORMAT) as u32;
    let compressed = dxt != IL_DXT_NO_COMP && !no_compression;
    let mut b = Bitmap {
        width: il_int(IL_IMAGE_WIDTH),
        height: il_int(IL_IMAGE_HEIGHT),
        depth: il_int(IL_IMAGE_DEPTH),
        bytes_per_pixel: il_int(IL_IMAGE_BYTES_PER_PIXEL),
        format: if compressed {
            devil::dxt_to_hpl(dxt)
        } else {
            format
        },
        compressed,
        images: layout.images,
        mips: layout.mips,
        setup_data: layout.setup_data,
        planes: Vec::new(),
    };
    for image in 0..layout.images {
        for mip in 0..layout.mips {
            if layout.images > 1 || layout.mips > 1 {
                // SAFETY: DevIL state changes.
                unsafe {
                    ilBindImage(id);
                    if layout.images > 1 {
                        if cube_flags != 0 {
                            ilActiveFace(image as u32);
                        } else {
                            ilActiveImage(image as u32);
                        }
                    }
                    if layout.mips > 1 {
                        ilActiveMipmap(mip as u32);
                    }
                }
            }
            if compressed {
                // SAFETY: the first call sizes the buffer the second fills.
                let n = unsafe { ilGetDXTCData(core::ptr::null_mut(), 0, dxt) };
                let mut buf = vec![0u8; n as usize];
                // SAFETY: `buf` is `n` bytes.
                unsafe { ilGetDXTCData(buf.as_mut_ptr().cast(), n, dxt) };
                b.planes.push(buf);
            } else {
                b.planes.push(il_data(il_int(IL_IMAGE_SIZE_OF_DATA)));
            }
        }
    }
    delete_image(id);
    Some(b)
}

pub struct SaveInput<'a> {
    pub width: u32,
    pub height: u32,
    pub depth: u32,
    pub bytes_per_pixel: u8,
    pub format: u32,
    pub data: *const u8,
    pub path: &'a [u32],
}

/// `iBitmapLoaderDevil::SaveBitmap`.
///
/// # Safety
/// `i.data` points to `width * height * depth * bytes_per_pixel` readable bytes.
pub unsafe fn save(i: &SaveInput, log: Sink) -> bool {
    initialize(log);
    let id = gen_image();
    let name = wide_to_utf8(i.path);
    let fmt = pixel_format_from_hpl(i.format);
    if fmt == 0 {
        log(
            Level::Error,
            &message(&[
                b"Could not save image to '",
                &name,
                b"'! Pixel format of bitmap was not valid!\n",
            ]),
        );
        delete_image(id);
        return false;
    }
    // SAFETY: caller contract on `data`; DevIL copies it.
    unsafe {
        ilTexImage(
            i.width,
            i.height,
            i.depth,
            i.bytes_per_pixel,
            fmt,
            IL_UNSIGNED_BYTE,
            i.data.cast_mut().cast(),
        );
    }
    let ty = il_type(i.path);
    if ty == 0 {
        log(
            Level::Error,
            &message(&[b"Filename ", &name, b" has not a valid image extension!\n"]),
        );
        delete_image(id);
        return false;
    }
    let f = cfile::open(&name, c"w+b");
    if f.is_null() {
        log(
            Level::Error,
            &message(&[b"Could not open file ", &name, b" for writing!\n"]),
        );
        delete_image(id);
        return false;
    }
    // SAFETY: `f` is an open file handed to DevIL, then closed once.
    let ret = unsafe { ilSaveF(ty, f.cast()) };
    if ret == 0 {
        log(
            Level::Error,
            &message(&[b"Error when saving image '", &name, b"' to file!\n"]),
        );
    }
    // SAFETY: `f` is open.
    unsafe { cfile::fclose(f) };
    delete_image(id);
    ret != 0
}
