//! DevIL enum mappings and image-series layout decisions of `BitmapLoaderDevil*`.
//! The IL_* values are checked against il.h by static_asserts in `RustImpl.h`.

use super::dds::{FMT_ALPHA, FMT_BGRA, FMT_DXT1, FMT_DXT3, FMT_DXT5, FMT_RGBA, FMT_UNKNOWN};

pub const IL_RGB: u32 = 0x1907;
pub const IL_RGBA: u32 = 0x1908;
pub const IL_BGR: u32 = 0x80E0;
pub const IL_BGRA: u32 = 0x80E1;
pub const IL_LUMINANCE: u32 = 0x1909;
pub const IL_BMP: u32 = 0x0420;
pub const IL_JPG: u32 = 0x0425;
pub const IL_PCX: u32 = 0x0428;
pub const IL_PNG: u32 = 0x042A;
pub const IL_SGI: u32 = 0x042C;
pub const IL_TGA: u32 = 0x042D;
pub const IL_TIF: u32 = 0x042E;
pub const IL_DDS: u32 = 0x0437;
pub const IL_PSD: u32 = 0x0439;
pub const IL_DXT1: u32 = 0x0706;
pub const IL_DXT3: u32 = 0x0708;
pub const IL_DXT5: u32 = 0x070A;
pub const IL_DXT_NO_COMP: u32 = 0x070B;

const PF_RGB: u32 = 4;
const PF_BGR: u32 = 6;
const PF_LUMINANCE: u32 = 2;

pub fn pixel_format_to_hpl(il: u32) -> u32 {
    match il {
        IL_RGB => PF_RGB,
        IL_RGBA => FMT_RGBA,
        IL_BGR => PF_BGR,
        IL_BGRA => FMT_BGRA,
        IL_LUMINANCE => FMT_ALPHA,
        _ => FMT_UNKNOWN,
    }
}

pub fn pixel_format_from_hpl(format: u32) -> u32 {
    match format {
        PF_RGB => IL_RGB,
        FMT_RGBA => IL_RGBA,
        PF_BGR => IL_BGR,
        FMT_BGRA => IL_BGRA,
        PF_LUMINANCE => IL_LUMINANCE,
        _ => 0,
    }
}

pub fn dxt_to_hpl(il: u32) -> u32 {
    match il {
        IL_DXT1 => FMT_DXT1,
        IL_DXT3 => FMT_DXT3,
        IL_DXT5 => FMT_DXT5,
        _ => FMT_UNKNOWN,
    }
}

/// `ext` is the lower-cased extension as code points.
pub fn type_from_extension(ext: &[u32]) -> u32 {
    const TABLE: [(&[u8], u32); 10] = [
        (b"bmp", IL_BMP),
        (b"jpeg", IL_JPG),
        (b"jpg", IL_JPG),
        (b"pcx", IL_PCX),
        (b"png", IL_PNG),
        (b"tga", IL_TGA),
        (b"tif", IL_TIF),
        (b"dds", IL_DDS),
        (b"psd", IL_PSD),
        (b"sgi", IL_SGI),
    ];
    TABLE
        .iter()
        .find(|(name, _)| {
            name.len() == ext.len() && name.iter().zip(ext).all(|(&a, &b)| u32::from(a) == b)
        })
        .map_or(0, |&(_, t)| t)
}

/// `images`/`mips` are IL_NUM_IMAGES / IL_NUM_MIPMAPS (both count minus one).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DdsLayout {
    pub images: i32,
    pub mips: i32,
    pub setup_data: bool,
}

pub fn dds_layout(il_images: i32, il_mips: i32, cube_flags: i32) -> DdsLayout {
    let mut images = il_images.wrapping_add(1);
    let mips = il_mips.wrapping_add(1);
    if cube_flags != 0 {
        images = 6;
    }
    DdsLayout {
        images,
        mips,
        setup_data: images > 1 || mips > 1,
    }
}

/// Misc loader: IL_NUM_IMAGES of 0 still means one image.
pub fn misc_image_count(il_images: i32) -> i32 {
    if il_images > 0 { il_images } else { 1 }
}
