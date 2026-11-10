//! C ABI for `imageload`, declared in `HPL2/core/include/impl/RustImpl.h`.

use super::dds::{self, AlphaPlan, BlockPlan};
use super::theora::{self, Planes, TablesRef};
use super::{devil, fontdata};
use crate::string_ffi::units;

#[repr(C)]
pub struct OhplDdsBlockPlan {
    format: u32,
    block_size: u32,
    bytes_per_pixel: i32,
    cube: bool,
    width: u32,
    height: u32,
    mips: u32,
    images: u32,
    data_offset: u64,
}

#[repr(C)]
pub struct OhplDdsAlphaPlan {
    width: u32,
    height: u32,
    row_bytes: u32,
}

#[repr(C)]
pub struct OhplDdsLayout {
    images: i32,
    mips: i32,
    setup_data: bool,
}

#[repr(C)]
pub struct OhplFontHeader {
    line_height: i32,
    base: i32,
    height: f32,
    size_ratio_x: f32,
}

impl From<BlockPlan> for OhplDdsBlockPlan {
    fn from(p: BlockPlan) -> Self {
        Self {
            format: p.format,
            block_size: p.block_size,
            bytes_per_pixel: p.bytes_per_pixel,
            cube: p.cube,
            width: p.width,
            height: p.height,
            mips: p.mips,
            images: p.images,
            data_offset: p.data_offset,
        }
    }
}

impl OhplDdsBlockPlan {
    fn to_core(&self) -> BlockPlan {
        BlockPlan {
            format: self.format,
            block_size: self.block_size,
            bytes_per_pixel: self.bytes_per_pixel,
            cube: self.cube,
            width: self.width,
            height: self.height,
            mips: self.mips,
            images: self.images,
            data_offset: self.data_offset,
        }
    }
}

/// # Safety
/// `header` valid for `len` reads, `out` for a write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dds_plan_block(
    header: *const u8,
    len: usize,
    file_size: u64,
    no_compression: bool,
    out: *mut OhplDdsBlockPlan,
) -> bool {
    // SAFETY: caller contract.
    let header = unsafe { units(header, len) };
    let Some(plan) = dds::plan_block(header, file_size, no_compression) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { out.write(plan.into()) };
    true
}

/// Size in bytes of `mip` of one image; -1 for a size the format cannot hold.
///
/// # Safety
/// `plan` points to a plan from `ohpl_dds_plan_block`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dds_mip_size(plan: *const OhplDdsBlockPlan, mip: u32) -> i32 {
    // SAFETY: caller contract.
    unsafe { plan.as_ref() }
        .and_then(|p| p.to_core().mip_size(mip))
        .unwrap_or(-1)
}

/// # Safety
/// `header` valid for `len` reads, `out` for a write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dds_plan_alpha(
    header: *const u8,
    len: usize,
    file_size: u64,
    out: *mut OhplDdsAlphaPlan,
) -> bool {
    // SAFETY: caller contract.
    let header = unsafe { units(header, len) };
    let Some(AlphaPlan {
        width,
        height,
        row_bytes,
    }) = dds::plan_alpha(header, file_size)
    else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        out.write(OhplDdsAlphaPlan {
            width,
            height,
            row_bytes,
        })
    };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_devil_pixel_format_to_hpl(il: u32) -> u32 {
    devil::pixel_format_to_hpl(il)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_devil_pixel_format_from_hpl(format: u32) -> u32 {
    devil::pixel_format_from_hpl(format)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_devil_dxt_to_hpl(il: u32) -> u32 {
    devil::dxt_to_hpl(il)
}

/// # Safety
/// `ext` valid for `len` code points.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_devil_type_from_extension(ext: *const u32, len: usize) -> u32 {
    // SAFETY: caller contract.
    devil::type_from_extension(unsafe { units(ext, len) })
}

/// # Safety
/// `out` valid for a write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_devil_dds_layout(
    il_images: i32,
    il_mips: i32,
    cube_flags: i32,
    out: *mut OhplDdsLayout,
) {
    let l = devil::dds_layout(il_images, il_mips, cube_flags);
    // SAFETY: caller contract.
    unsafe {
        out.write(OhplDdsLayout {
            images: l.images,
            mips: l.mips,
            setup_data: l.setup_data,
        })
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_devil_misc_image_count(il_images: i32) -> i32 {
    devil::misc_image_count(il_images)
}

/// Fills the four YUV tables, of `R_LEN`, `R_LEN`, `G_UV_LEN` and `G_Y_UV_LEN` entries.
///
/// # Safety
/// Each pointer is valid for its table length in writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_theora_fill_tables(
    to_r: *mut u8,
    to_b: *mut u8,
    g_uv: *mut u16,
    g_y_uv: *mut u8,
) {
    let t = theora::build_tables();
    // SAFETY: caller contract; lengths match the generated tables.
    unsafe {
        to_r.copy_from_nonoverlapping(t.to_r.as_ptr(), t.to_r.len());
        to_b.copy_from_nonoverlapping(t.to_b.as_ptr(), t.to_b.len());
        g_uv.copy_from_nonoverlapping(t.g_uv.as_ptr(), t.g_uv.len());
        g_y_uv.copy_from_nonoverlapping(t.g_y_uv.as_ptr(), t.g_y_uv.len());
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_theora_frame_len(w: i32, h: i32) -> usize {
    theora::frame_buffer_len(w, h)
}

#[repr(C)]
pub struct OhplYuvPlanes {
    y: *const u8,
    y_len: usize,
    u: *const u8,
    u_len: usize,
    v: *const u8,
    v_len: usize,
    y_stride: i32,
    uv_stride: i32,
    offset_x: u32,
    offset_y: u32,
}

#[repr(C)]
pub struct OhplYuvTables {
    to_r: *const u8,
    to_b: *const u8,
    g_uv: *const u16,
    g_y_uv: *const u8,
}

/// # Safety
/// `out` valid for `out_len` writes; `planes` and `tables` point to valid structs whose pointers
/// are valid for their lengths (`tables` for the `ohpl_theora_fill_tables` sizes).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_theora_draw_frame(
    out: *mut u8,
    out_len: usize,
    planes: *const OhplYuvPlanes,
    tables: *const OhplYuvTables,
    w: i32,
    h: i32,
) -> bool {
    // SAFETY: caller contract.
    let (Some(p), Some(t)) = (unsafe { planes.as_ref() }, unsafe { tables.as_ref() }) else {
        return false;
    };
    if out.is_null() {
        return false;
    }
    let (Ok(y_stride), Ok(uv_stride)) = (usize::try_from(p.y_stride), usize::try_from(p.uv_stride))
    else {
        return false;
    };
    // SAFETY: caller contract.
    let (out, planes, tables) = unsafe {
        (
            std::slice::from_raw_parts_mut(out, out_len),
            Planes {
                y: units(p.y, p.y_len),
                u: units(p.u, p.u_len),
                v: units(p.v, p.v_len),
                y_stride,
                uv_stride,
                offset_x: p.offset_x as usize,
                offset_y: p.offset_y as usize,
            },
            TablesRef {
                to_r: units(t.to_r, theora::R_LEN),
                to_b: units(t.to_b, theora::R_LEN),
                g_uv: units(t.g_uv, theora::G_UV_LEN),
                g_y_uv: units(t.g_y_uv, theora::G_Y_UV_LEN),
            },
        )
    };
    theora::draw_frame(out, &planes, &tables, w, h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_font_header(outline: i32, line_height: i32, base: i32) -> OhplFontHeader {
    let h = fontdata::header(outline, line_height, base);
    OhplFontHeader {
        line_height: h.line_height,
        base: h.base,
        height: h.height,
        size_ratio_x: h.size_ratio_x,
    }
}

/// Glyph table size for the largest id, or -1 when refused.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_font_glyph_count(largest_id: i32) -> i64 {
    fontdata::glyph_count(largest_id).map_or(-1, |n| n as i64)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_font_glyph_id_valid(id: i32, glyph_count: usize) -> bool {
    fontdata::glyph_id_valid(id, glyph_count)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_font_glyph_advance(xadvance: i32, outline: i32) -> i32 {
    fontdata::glyph_advance(xadvance, outline)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_font_page_valid(page: i32, pages: usize) -> bool {
    fontdata::page_valid(page, pages)
}
