//! C ABI for `imageload`, declared in `HPL2/core/include/impl/RustImpl.h`.

use super::dds::{self, BlockPlan};
use super::theora::{self};
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

impl OhplDdsBlockPlan {}

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
