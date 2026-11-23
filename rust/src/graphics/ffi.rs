//! C ABI for the graphics data code, declared in `graphics/RustGraphics.h`.
//!
//! Unless noted, pointers are non-null, valid and aligned for the call and outputs do not alias
//! inputs. Buffers cross as pointer + length.

use std::ffi::c_void;

use super::bitmap::{self, Surface};
use super::packer::RectPacker;
use super::{formats, uv};
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
