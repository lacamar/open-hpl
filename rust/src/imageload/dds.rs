//! DDS header parsing for the block-compressed and raw-alpha fast paths of `BitmapLoaderDevilDDS`.
//! Offsets and sizes are validated against the file size, so a hostile header cannot make the
//! caller allocate more than the file holds.

const HEADER: usize = 128;
const DX10_HEADER: usize = 148;

pub const FMT_UNKNOWN: u32 = 0;
pub const FMT_ALPHA: u32 = 1;
pub const FMT_RGBA: u32 = 5;
pub const FMT_BGRA: u32 = 7;
pub const FMT_DXT1: u32 = 8;
pub const FMT_DXT3: u32 = 10;
pub const FMT_DXT5: u32 = 12;
pub const FMT_RGTC2: u32 = 13;
pub const FMT_RGTC1: u32 = 27;
pub const FMT_BC6H: u32 = 28;
pub const FMT_BC7: u32 = 29;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct BlockPlan {
    pub format: u32,
    pub block_size: u32,
    pub bytes_per_pixel: i32,
    pub cube: bool,
    pub width: u32,
    pub height: u32,
    pub mips: u32,
    pub images: u32,
    pub data_offset: u64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct AlphaPlan {
    pub width: u32,
    pub height: u32,
    pub row_bytes: u32,
}

fn u32_at(h: &[u8], off: usize) -> u32 {
    h.get(off..off + 4)
        .and_then(|b| b.try_into().ok())
        .map_or(0, u32::from_le_bytes)
}

fn fourcc_is(h: &[u8], tag: &[u8; 4]) -> bool {
    h.get(84..88) == Some(tag.as_slice())
}

/// `header` is the first up to 148 bytes of the file.
pub fn plan_block(header: &[u8], file_size: u64, no_compression: bool) -> Option<BlockPlan> {
    let ok = header.len() >= HEADER && header.starts_with(b"DDS ");
    let caps2 = u32_at(header, 112);
    let dxt = !no_compression && caps2 & 0x20_0000 == 0;
    let mut format = FMT_UNKNOWN;
    let mut block_size = 16;
    let mut bpp = 4;
    let mut data_offset = HEADER as u64;
    if fourcc_is(header, b"BC5U") || fourcc_is(header, b"ATI2") {
        format = FMT_RGTC2;
        bpp = 1;
    } else if dxt && fourcc_is(header, b"DXT1") {
        format = FMT_DXT1;
        block_size = 8;
    } else if dxt && fourcc_is(header, b"DXT3") {
        format = FMT_DXT3;
    } else if dxt && fourcc_is(header, b"DXT5") {
        format = FMT_DXT5;
    } else if fourcc_is(header, b"BC4U") || fourcc_is(header, b"ATI1") {
        format = FMT_RGTC1;
        block_size = 8;
        bpp = 1;
    } else if fourcc_is(header, b"DX10") && ok && header.len() >= DX10_HEADER {
        data_offset = DX10_HEADER as u64;
        match u32_at(header, HEADER) {
            71 | 72 if dxt => {
                format = FMT_DXT1;
                block_size = 8;
            }
            74 | 75 if dxt => format = FMT_DXT3,
            77 | 78 if dxt => format = FMT_DXT5,
            80 => {
                format = FMT_RGTC1;
                block_size = 8;
                bpp = 1;
            }
            83 => {
                format = FMT_RGTC2;
                bpp = 1;
            }
            95 => format = FMT_BC6H,
            98 | 99 => format = FMT_BC7,
            28 | 29 => {
                format = FMT_RGBA;
                block_size = 0;
            }
            87 | 91 => {
                format = FMT_BGRA;
                block_size = 0;
            }
            _ => {}
        }
    }
    let cube = caps2 & 0x200 != 0;
    if !ok || format == FMT_UNKNOWN || (cube && caps2 & 0xFE00 != 0xFE00) {
        return None;
    }
    let mips = match u32_at(header, 28) {
        0 => 1,
        m => m,
    };
    let plan = BlockPlan {
        format,
        block_size,
        bytes_per_pixel: bpp,
        cube,
        width: u32_at(header, 16),
        height: u32_at(header, 12),
        mips,
        images: if cube { 6 } else { 1 },
        data_offset,
    };
    plan.fits(file_size).then_some(plan)
}

impl BlockPlan {
    /// Wrapping 32-bit arithmetic as in the original; None when the size is negative as `int`.
    pub fn mip_size(&self, mip: u32) -> Option<i32> {
        let (mut w, mut h) = (self.width, self.height);
        for _ in 0..mip.min(32) {
            w = if w > 1 { w >> 1 } else { 1 };
            h = if h > 1 { h >> 1 } else { 1 };
        }
        let size = if self.block_size != 0 {
            (w.wrapping_add(3) >> 2)
                .wrapping_mul(h.wrapping_add(3) >> 2)
                .wrapping_mul(self.block_size)
        } else {
            w.wrapping_mul(h).wrapping_mul(4)
        };
        i32::try_from(size).ok()
    }

    fn fits(&self, file_size: u64) -> bool {
        let Ok(mips) = u32::try_from(i32::try_from(self.mips).unwrap_or(-1)) else {
            return false;
        };
        let mut left = file_size.saturating_sub(self.data_offset);
        for _ in 0..self.images {
            for mip in 0..mips {
                let Some(size) = self.mip_size(mip) else {
                    return false;
                };
                let Some(rest) = left.checked_sub(size.unsigned_abs().into()) else {
                    return false;
                };
                left = rest;
            }
        }
        true
    }
}

/// `header` is exactly the first 128 bytes.
pub fn plan_alpha(header: &[u8], file_size: u64) -> Option<AlphaPlan> {
    if header.len() != HEADER || !header.starts_with(b"DDS ") {
        return None;
    }
    let flags = u32_at(header, 8);
    let height = u32_at(header, 12);
    let width = u32_at(header, 16);
    let pitch = u32_at(header, 20);
    let pf_flags = u32_at(header, 80);
    let bits = u32_at(header, 88);
    if !matches!(pf_flags, 1 | 2) || bits != 8 || width == 0 || height == 0 {
        return None;
    }
    let row_bytes = if flags & 0x8 != 0 && pitch > 0 {
        pitch
    } else {
        width
    };
    let pixels = u64::from(width) * u64::from(height);
    if pixels > i32::MAX as u64 {
        return None;
    }
    let last_row_end = u64::from(height - 1)
        .checked_mul(u64::from(row_bytes))?
        .checked_add(HEADER as u64 + u64::from(width))?;
    (last_row_end <= file_size).then_some(AlphaPlan {
        width,
        height,
        row_bytes,
    })
}
