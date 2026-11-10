//! C ABI for the graphics data code, declared in `graphics/RustGraphics.h`.
//!
//! Unless noted, pointers are non-null, valid and aligned for the call and outputs do not alias
//! inputs. Buffers cross as pointer + length.

use std::ffi::c_void;

use super::bitmap::{self, Surface};
use super::font::{self, Glyphs};
use super::packer::{self, RectPacker};
use super::{formats, scatter, uv};
use crate::math::Rect2l;
use crate::string_ffi::{Sink, emit, units};
use crate::types::{Vector2f, Vector2l, Vector3l};

macro_rules! format_fns {
    ($($name:ident => $f:path: $t:ty;)*) => {$(
        #[unsafe(no_mangle)]
        pub extern "C" fn $name(v: i32) -> $t { $f(v) }
    )*};
}

format_fns! {
    ohpl_gfx_channels => formats::channels: i32;
    ohpl_gfx_bytes_per_pixel => formats::bytes_per_pixel: i32;
    ohpl_gfx_is_compressed => formats::is_compressed: bool;
    ohpl_gfx_is_depth => formats::is_depth: bool;
    ohpl_gfx_is_floating_point => formats::is_floating_point: bool;
    ohpl_gfx_vertex_element_flag => formats::vertex_element_flag: u32;
    ohpl_gfx_vertex_format_byte_size => formats::vertex_format_byte_size: i32;
    ohpl_gfx_vertex_element_texture_unit => formats::vertex_element_texture_unit: i32;
}

#[repr(C)]
pub struct OhplSurface {
    size: Vector3l,
    format: i32,
    bytes_per_pixel: i32,
}

impl OhplSurface {
    fn surface(&self) -> Surface {
        Surface {
            size: self.size,
            format: self.format,
            bytes_per_pixel: self.bytes_per_pixel,
        }
    }
}

/// # Safety
/// `data` points to `len` writable bytes (or `len` is 0).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bitmap_clear(
    data: *mut u8,
    len: usize,
    count: i64,
    bpp: i32,
    format: i32,
    color: *const [u8; 4],
) {
    if data.is_null() {
        return;
    }
    // SAFETY: caller contract.
    let (data, color) = unsafe { (std::slice::from_raw_parts_mut(data, len), *color) };
    bitmap::clear(
        data,
        count,
        usize::try_from(bpp).unwrap_or(0),
        format,
        color,
    );
}

/// # Safety
/// `dst_data`/`src_data` point to their `*_len` bytes; the buffers may be the same allocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bitmap_blit(
    dst: *const OhplSurface,
    dst_data: *mut u8,
    dst_len: usize,
    src: *const OhplSurface,
    src_data: *const u8,
    src_len: usize,
    dest_position: *const Vector3l,
    src_size: *const Vector3l,
    src_position: *const Vector3l,
) {
    if dst_data.is_null() || src_data.is_null() {
        return;
    }
    // SAFETY: caller contract.
    let (dst, src, dp, ss, sp, d) = unsafe {
        (
            (*dst).surface(),
            (*src).surface(),
            *dest_position,
            *src_size,
            *src_position,
            std::slice::from_raw_parts_mut(dst_data, dst_len),
        )
    };
    let d_start = dst_data as usize;
    let s_start = src_data as usize;
    let overlaps =
        s_start < d_start.saturating_add(dst_len) && d_start < s_start.saturating_add(src_len);
    if overlaps {
        // SAFETY: caller contract; the copy ends the shared borrow before `d` is written.
        let snapshot = unsafe { std::slice::from_raw_parts(src_data, src_len) }.to_vec();
        bitmap::blit(&dst, d, &src, &snapshot, dp, ss, sp);
    } else {
        // SAFETY: caller contract; disjoint from `d`.
        let s = unsafe { std::slice::from_raw_parts(src_data, src_len) };
        bitmap::blit(&dst, d, &src, s, dp, ss, sp);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_packer_new(w: i32, h: i32) -> *mut c_void {
    Box::into_raw(Box::new(RectPacker::new(w, h))).cast()
}

/// # Safety
/// `p` is from `ohpl_packer_new`, freed once, or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_packer_free(p: *mut c_void) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p.cast::<RectPacker>()) });
    }
}

/// # Safety
/// `p` is a live packer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_packer_clear(p: *mut c_void, w: i32, h: i32) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { p.cast::<RectPacker>().as_mut() } {
        p.clear(w, h);
    }
}

/// Places a `w` x `h` bitmap (border added inside). Returns whether it fit; `rect` receives the
/// placed rect including the border.
///
/// # Safety
/// `p` is a live packer; `rect` and `found_empty` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_packer_insert(
    p: *mut c_void,
    w: i32,
    h: i32,
    rect: *mut Rect2l,
    found_empty: *mut bool,
) -> bool {
    // SAFETY: caller contract.
    let Some(p) = (unsafe { p.cast::<RectPacker>().as_mut() }) else {
        return false;
    };
    let r = p.insert(w, h);
    // SAFETY: caller contract.
    unsafe {
        *found_empty = r.found_empty;
        if let Some(rc) = r.rect {
            *rect = rc;
        }
    }
    r.rect.is_some()
}

/// `wh` holds `n` (width, height) pairs; `out` receives the `n` indices in insertion order.
///
/// # Safety
/// `wh` has `2 * n` ints and `out` room for `n` values.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_packer_reorganize_order(wh: *const i32, n: usize, out: *mut usize) {
    if n == 0 {
        return;
    }
    // SAFETY: caller contract.
    let (wh, out) = unsafe {
        (
            std::slice::from_raw_parts(wh, n * 2),
            std::slice::from_raw_parts_mut(out, n),
        )
    };
    let sizes: Vec<(i32, i32)> = wh
        .chunks_exact(2)
        .map(|c| {
            (
                c.first().copied().unwrap_or(0),
                c.get(1).copied().unwrap_or(0),
            )
        })
        .collect();
    for (o, i) in out.iter_mut().zip(packer::reorganize_order(&sizes)) {
        *o = i;
    }
}

/// # Safety
/// All pointers valid; `out` has room for four vectors.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_subimage_uvs(
    rect: *const Rect2l,
    src_size: *const Vector2l,
    out: *mut Vector2f,
) {
    // SAFETY: caller contract.
    unsafe {
        let uvs = uv::subimage_uvs(&*rect, &*src_size);
        std::slice::from_raw_parts_mut(out, 4).copy_from_slice(&uvs);
    }
}

/// `out` receives offset (2 floats), size (2 floats) and advance.
///
/// # Safety
/// All pointers valid; `out` has room for five floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_font_glyph_metrics(
    offset: *const Vector2l,
    size: *const Vector2l,
    font_size: *const Vector2l,
    advance: i32,
    ratio: *const Vector2f,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let m = uv::glyph_metrics(&*offset, &*size, &*font_size, advance, &*ratio);
        std::slice::from_raw_parts_mut(out, 5)
            .copy_from_slice(&[m.offset.x, m.offset.y, m.size.x, m.size.y, m.advance]);
    }
}

/// Calls `sink` once per row; returns false (emitting nothing) where the C++ `substr` throws.
///
/// # Safety
/// `g` describes live glyph pointers; `s` has `n` units; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_font_word_wrap(
    g: *const Glyphs,
    max_length: f32,
    size_x: f32,
    s: *const u32,
    n: usize,
    sink: Sink<u32>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let text = unsafe { units(s, n) };
    // SAFETY: caller contract.
    let Some(rows) = font::word_wrap_rows(unsafe { &*g }, max_length, size_x, text) else {
        return false;
    };
    for (start, len) in rows {
        if let Some(r) = text.get(start..start + len) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, r) };
        }
    }
    true
}

/// # Safety
/// `out` has `len` writable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scatter_disk(
    size: i32,
    samples: i32,
    sort: bool,
    out: *mut u8,
    len: usize,
) -> bool {
    // SAFETY: caller contract.
    let out = unsafe { units_mut(out, len) };
    scatter::generate(size, samples, sort, out)
}

/// # Safety
/// `p` points to `n` writable bytes, or `n` is 0.
unsafe fn units_mut<'a>(p: *mut u8, n: usize) -> &'a mut [u8] {
    if n == 0 {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(p, n) }
    }
}
