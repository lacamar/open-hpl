//! `ePixelFormat` / `eVertexBuffer*` lookups from `GraphicsTypes.cpp`. Values outside the enum
//! take the C++ switch default.

pub const UNKNOWN: i32 = 0;
pub const ALPHA: i32 = 1;
pub const LUMINANCE: i32 = 2;
pub const LUMINANCE_ALPHA: i32 = 3;
pub const RGB: i32 = 4;
pub const RGBA: i32 = 5;
pub const BGR: i32 = 6;
pub const BGRA: i32 = 7;

const CHANNELS: [u8; 30] = [
    0, 1, 1, 2, 3, 4, 3, 4, // Unknown .. BGRA
    4, 4, 4, 4, 4, 4, // DXT1 .. RGTC2
    1, 1, 1, // Depth16/24/32
    1, 1, 2, 3, 4, // Alpha16 .. RGBA16
    1, 1, 2, 3, 4, // Alpha32 .. RGBA32
    1, 3, 4, // RGTC1, BC6H, BC7
];

const BYTES: [u8; 30] = [
    0, 1, 1, 2, 3, 4, 3, 4, //
    1, 1, 1, 1, 1, 1, //
    2, 3, 4, //
    2, 2, 4, 6, 8, //
    4, 4, 8, 12, 16, //
    1, 1, 1,
];

fn lookup(table: &[u8; 30], f: i32) -> i32 {
    usize::try_from(f)
        .ok()
        .and_then(|i| table.get(i))
        .map_or(0, |&v| i32::from(v))
}

pub fn channels(f: i32) -> i32 {
    lookup(&CHANNELS, f)
}

pub fn bytes_per_pixel(f: i32) -> i32 {
    lookup(&BYTES, f)
}

pub fn is_compressed(f: i32) -> bool {
    matches!(f, 8..=13 | 27..=29)
}

pub fn is_depth(f: i32) -> bool {
    matches!(f, 14..=16)
}

pub fn is_floating_point(f: i32) -> bool {
    matches!(f, 17..=26)
}

pub fn vertex_element_flag(e: i32) -> u32 {
    match e {
        0 => 0x1,
        1 => 0x2,
        2 => 0x4,
        3 => 0x8,
        4 | 6 => 0x20,
        5 => 0x10,
        7 => 0x40,
        8 => 0x80,
        9 => 0x100,
        10 => 0x200,
        11 => 0x400,
        12 => 0x800,
        13 => 0x1000,
        _ => 0,
    }
}

pub fn vertex_format_byte_size(f: i32) -> i32 {
    match f {
        0 | 1 => 4,
        2 => 1,
        _ => 0,
    }
}

pub fn vertex_element_texture_unit(e: i32) -> i32 {
    match e {
        4 | 6 => 1,
        5 => 0,
        7 => 2,
        8 => 3,
        9 => 4,
        _ => -1,
    }
}
