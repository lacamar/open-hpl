//! C ABI of the bitmap loaders, font reader, video stream and audio hash, declared in
//! `HPL2/core/include/impl/RustL5.h`.

use super::audio;
use super::bitmap::{self, Bitmap, SaveInput};
use super::font::{self, Font};
use super::log::{self, Level, c_bytes};
use super::video::{Loader, Video};
use crate::string::wide_to_utf8;
use crate::string_ffi::units;
use core::ffi::{CStr, c_char, c_void};
use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;

fn emit(level: Level, msg: &[u8]) {
    log::to_callback(level, msg);
}

/// # Safety
/// `p` is null or a live handle.
unsafe fn r<'a, T>(p: *mut T) -> Option<&'a mut T> {
    // SAFETY: caller contract.
    unsafe { p.as_mut() }
}

/// `kind` 0 is the misc loader (jpg, png, tif, bmp, tga), 1 the DDS loader.
///
/// # Safety
/// `path` holds `len` code points.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_bitmap_load(
    path: *const u32,
    len: usize,
    kind: u32,
    no_compression: bool,
) -> *mut Bitmap {
    // SAFETY: caller contract.
    let path = unsafe { units(path, len) };
    let b = if kind == 1 {
        bitmap::load_dds(path, no_compression, &mut emit)
    } else {
        bitmap::load_misc(path, &mut emit)
    };
    b.map_or(core::ptr::null_mut(), |b| Box::into_raw(Box::new(b)))
}

#[repr(C)]
pub struct OhplL5BitmapInfo {
    width: i32,
    height: i32,
    depth: i32,
    bytes_per_pixel: i32,
    format: u32,
    compressed: bool,
    images: i32,
    mips: i32,
    setup_data: bool,
}

/// # Safety
/// `b` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_bitmap_info(b: *const Bitmap, out: *mut OhplL5BitmapInfo) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(b), false) = (b.as_ref(), out.is_null()) {
            out.write(OhplL5BitmapInfo {
                width: b.width,
                height: b.height,
                depth: b.depth,
                bytes_per_pixel: b.bytes_per_pixel,
                format: b.format,
                compressed: b.compressed,
                images: b.images,
                mips: b.mips,
                setup_data: b.setup_data,
            });
        }
    }
}

/// The plane's bytes, valid until `ohpl_l5_bitmap_free`; null for an unknown plane.
///
/// # Safety
/// `b` is live; `size` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_bitmap_plane(
    b: *const Bitmap,
    image: i32,
    mip: i32,
    size: *mut i32,
) -> *const u8 {
    // SAFETY: caller contract.
    unsafe {
        let Some(p) = b.as_ref().and_then(|b| b.plane(image, mip)) else {
            return core::ptr::null();
        };
        if let Some(s) = size.as_mut() {
            *s = p.len() as i32;
        }
        p.as_ptr()
    }
}

/// # Safety
/// `b` from `ohpl_l5_bitmap_load`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_bitmap_free(b: *mut Bitmap) {
    if !b.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(b) });
    }
}

/// # Safety
/// `path` holds `len` code points; `data` holds `width * height * depth * bytes_per_pixel` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_bitmap_save(
    path: *const u32,
    len: usize,
    width: u32,
    height: u32,
    depth: u32,
    bytes_per_pixel: u8,
    format: u32,
    data: *const u8,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        bitmap::save(
            &SaveInput {
                width,
                height,
                depth,
                bytes_per_pixel,
                format,
                data,
                path: units(path, len),
            },
            &mut emit,
        )
    }
}

#[repr(C)]
pub struct OhplL5FontInfo {
    line_height: i32,
    base: i32,
    height: f32,
    size_ratio_x: f32,
    largest_id: i32,
    glyph_count: usize,
    pages: usize,
    chars: usize,
}

#[repr(C)]
pub struct OhplL5FontChar {
    id: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    x_offset: i32,
    y_offset: i32,
    advance: i32,
    page: i32,
    /// 0 usable, 1 invalid id, 2 invalid page.
    status: i32,
}

/// `status`: 0 loaded, 1 cannot open, 2 XML error, 3 lacks a section, 4 unreasonable glyph id
/// (`largest_id` set).
///
/// # Safety
/// `path` is NUL-terminated; `status` and `largest_id` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_font_load(
    path: *const c_char,
    status: *mut i32,
    largest_id: *mut i32,
) -> *mut Font {
    // SAFETY: caller contract.
    let data = std::fs::read(OsStr::from_bytes(unsafe { c_bytes(path) }));
    let (code, font) = match data {
        Err(_) => (1, None),
        Ok(d) => match font::parse(&d) {
            Ok(f) => (0, Some(f)),
            Err(font::Error::Parse) => (2, None),
            Err(font::Error::Lacks) => (3, None),
            Err(font::Error::Unreasonable(id)) => {
                // SAFETY: caller contract.
                if let Some(o) = unsafe { largest_id.as_mut() } {
                    *o = id;
                }
                (4, None)
            }
        },
    };
    // SAFETY: caller contract.
    if let Some(s) = unsafe { status.as_mut() } {
        *s = code;
    }
    font.map_or(core::ptr::null_mut(), |f| Box::into_raw(Box::new(f)))
}

/// # Safety
/// `f` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_font_info(f: *const Font, out: *mut OhplL5FontInfo) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(f), false) = (f.as_ref(), out.is_null()) {
            out.write(OhplL5FontInfo {
                line_height: f.header.line_height,
                base: f.header.base,
                height: f.header.height,
                size_ratio_x: f.header.size_ratio_x,
                largest_id: f.largest_id,
                glyph_count: f.glyph_count,
                pages: f.pages.len(),
                chars: f.glyphs.len(),
            });
        }
    }
}

type NameSink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

/// Hands page `i`'s file name to `sink`.
///
/// # Safety
/// `f` is live; `sink` accepts `ctx` and the name.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_font_page(
    f: *const Font,
    i: usize,
    sink: NameSink,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let Some(name) = f.as_ref().and_then(|f| f.pages.get(i)) else {
            return false;
        };
        sink(ctx, name.as_ptr(), name.len());
    }
    true
}

/// # Safety
/// `f` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_font_char(
    f: *const Font,
    i: usize,
    out: *mut OhplL5FontChar,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (Some(g), false) = (f.as_ref().and_then(|f| f.glyphs.get(i)), out.is_null()) else {
            return false;
        };
        out.write(OhplL5FontChar {
            id: g.id,
            x: g.x,
            y: g.y,
            width: g.width,
            height: g.height,
            x_offset: g.x_offset,
            y_offset: g.y_offset,
            advance: g.advance,
            page: g.page,
            status: match g.status {
                font::GlyphStatus::Ok => 0,
                font::GlyphStatus::InvalidId => 1,
                font::GlyphStatus::InvalidPage => 2,
            },
        });
    }
    true
}

/// # Safety
/// `f` from `ohpl_l5_font_load`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_font_free(f: *mut Font) {
    if !f.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(f) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l5_vloader_new() -> *mut Loader {
    Box::into_raw(Box::new(Loader::new()))
}

/// # Safety
/// `l` from `ohpl_l5_vloader_new`, freed once; streams made from it keep their own reference.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_vloader_free(l: *mut Loader) {
    if !l.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(l) });
    }
}

/// # Safety
/// `l` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_new(l: *const Loader) -> *mut Video {
    // SAFETY: caller contract.
    unsafe { l.as_ref() }
        .and_then(Video::new)
        .map_or(core::ptr::null_mut(), |v| Box::into_raw(Box::new(v)))
}

/// # Safety
/// `v` is live; `path` holds `len` code points.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_load(v: *mut Video, path: *const u32, len: usize) -> bool {
    // SAFETY: caller contract.
    let (Some(v), path) = (unsafe { r(v) }, unsafe { units(path, len) }) else {
        return false;
    };
    v.load_from_file(path, &wide_to_utf8(path), &mut emit)
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_update(v: *mut Video, dt: f32) {
    // SAFETY: caller contract.
    if let Some(v) = unsafe { r(v) } {
        v.update(dt, &mut emit);
    }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_play(v: *mut Video) {
    // SAFETY: caller contract.
    if let Some(v) = unsafe { r(v) } {
        v.play();
    }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_stop(v: *mut Video) {
    // SAFETY: caller contract.
    if let Some(v) = unsafe { r(v) } {
        v.stop(&mut emit);
    }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_pause(v: *mut Video, p: bool) {
    // SAFETY: caller contract.
    if let Some(v) = unsafe { r(v) } {
        v.pause(p);
    }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_set_loop(v: *mut Video, l: bool) {
    // SAFETY: caller contract.
    if let Some(v) = unsafe { r(v) } {
        v.set_loop(l);
    }
}

/// # Safety
/// `v` is live; `w` and `h` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_size(v: *const Video, w: *mut i32, h: *mut i32) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(v), Some(w), Some(h)) = (v.as_ref(), w.as_mut(), h.as_mut()) {
            (*w, *h) = v.size();
        }
    }
}

/// The RGB frame (`width * height * 3` bytes, valid until the next call) when a new frame is
/// ready, else null.
///
/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_take_frame(v: *mut Video) -> *const u8 {
    // SAFETY: caller contract.
    unsafe { r(v) }
        .and_then(Video::take_frame)
        .map_or(core::ptr::null(), <[u8]>::as_ptr)
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_flags(v: *const Video) -> u32 {
    // SAFETY: caller contract.
    unsafe { v.as_ref() }.map_or(0, |v| {
        u32::from(v.is_paused()) | u32::from(v.is_looping()) << 1
    })
}

/// # Safety
/// `v` from `ohpl_l5_video_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_video_free(v: *mut Video) {
    if !v.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(v) });
    }
}

/// Hash of the decoded samples of a `.wav` or `.ogg` file (by extension), as OALWrapper loads it.
///
/// # Safety
/// `path` is NUL-terminated; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_audio_hash(path: *const c_char, out: *mut u64) -> bool {
    // SAFETY: caller contract.
    let (p, out) = unsafe { (CStr::from_ptr(path), out.as_mut()) };
    let Some(out) = out else {
        return false;
    };
    let ext = p
        .to_bytes()
        .rsplit(|&b| b == b'.')
        .next()
        .unwrap_or_default()
        .to_ascii_lowercase();
    let pcm = match ext.as_slice() {
        b"wav" => std::fs::read(OsStr::from_bytes(p.to_bytes()))
            .ok()
            .and_then(|d| audio::decode_wav(&d)),
        b"ogg" => audio::decode_ogg(p),
        _ => None,
    };
    pcm.is_some_and(|pcm| {
        *out = audio::hash(&pcm);
        true
    })
}

/// Hash of the decoded samples of an in-memory WAV file.
///
/// # Safety
/// `data` holds `len` bytes; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l5_wav_hash(data: *const u8, len: usize, out: *mut u64) -> bool {
    // SAFETY: caller contract.
    let (Some(out), data) = (unsafe { out.as_mut() }, unsafe { units(data, len) }) else {
        return false;
    };
    audio::decode_wav(data).is_some_and(|pcm| {
        *out = audio::hash(&pcm);
        true
    })
}

const _: () = assert!(
    size_of::<OhplL5BitmapInfo>() == 36
        && core::mem::offset_of!(OhplL5BitmapInfo, compressed) == 20
        && core::mem::offset_of!(OhplL5BitmapInfo, setup_data) == 32
        && size_of::<OhplL5FontInfo>() == 48
        && core::mem::offset_of!(OhplL5FontInfo, glyph_count) == 24
        && size_of::<OhplL5FontChar>() == 40
);
