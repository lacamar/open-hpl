//! Vulkan backend decisions that need no `vk*` call: enum tables, pixel formats and conversions,
//! mip plans, index conversion, uniform writes, sampler keys, ring allocation. Vulkan enum values
//! are spelled as the numbers from `vulkan_core.h`.

pub mod vkf {
    pub const UNDEFINED: u32 = 0;
    pub const R8_UNORM: u32 = 9;
    pub const R8G8_UNORM: u32 = 16;
    pub const R8G8B8A8_UNORM: u32 = 37;
    pub const R8G8B8A8_SRGB: u32 = 43;
    pub const B8G8R8A8_UNORM: u32 = 44;
    pub const B8G8R8A8_SRGB: u32 = 50;
    pub const R16_SFLOAT: u32 = 76;
    pub const R16G16_SFLOAT: u32 = 83;
    pub const R16G16B16A16_SFLOAT: u32 = 97;
    pub const R32_SFLOAT: u32 = 100;
    pub const R32G32_SFLOAT: u32 = 103;
    pub const R32G32B32A32_SFLOAT: u32 = 109;
    pub const D16_UNORM: u32 = 124;
    pub const X8_D24_UNORM_PACK32: u32 = 125;
    pub const D32_SFLOAT: u32 = 126;
    pub const D24_UNORM_S8_UINT: u32 = 129;
    pub const D32_SFLOAT_S8_UINT: u32 = 130;
    pub const BC1_RGBA_UNORM: u32 = 133;
    pub const BC1_RGBA_SRGB: u32 = 134;
    pub const BC2_UNORM: u32 = 135;
    pub const BC2_SRGB: u32 = 136;
    pub const BC3_UNORM: u32 = 137;
    pub const BC3_SRGB: u32 = 138;
    pub const BC4_UNORM: u32 = 139;
    pub const BC5_UNORM: u32 = 141;
    pub const BC6H_UFLOAT: u32 = 143;
    pub const BC7_UNORM: u32 = 145;
    pub const BC7_SRGB: u32 = 146;
}

const SW_IDENTITY: u32 = 0;
const SW_ZERO: u32 = 1;
const SW_ONE: u32 = 2;
const SW_R: u32 = 3;
const SW_G: u32 = 4;

pub fn stencil_op(op: i32) -> u32 {
    match op {
        0..=7 => op as u32,
        _ => 0,
    }
}

pub fn blend_factor(f: i32) -> u32 {
    match f {
        0..=9 => f as u32,
        10 => 14,
        _ => 1,
    }
}

pub fn wrap(mode: i32) -> u32 {
    match mode {
        0 => 0,
        3 => 3,
        _ => 2,
    }
}

pub const CONVERT_NONE: i32 = 0;
pub const CONVERT_RGB_TO_RGBA: i32 = 1;
pub const CONVERT_FLOAT_TO_HALF: i32 = 2;
pub const CONVERT_FLOAT3_TO_4: i32 = 3;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Format {
    pub format: u32,
    pub swizzle: [u32; 4],
    pub convert: i32,
    pub src_components: i32,
}

pub fn vk_format(pixel_format: i32, srgb: bool, depth24: u32) -> Format {
    let pick = |s: u32, u: u32| if srgb { s } else { u };
    let (format, swizzle, convert, src_components) = match pixel_format {
        1 => (vkf::R8_UNORM, [SW_ZERO, SW_ZERO, SW_ZERO, SW_R], 0, 0),
        2 => (vkf::R8_UNORM, [SW_R, SW_R, SW_R, SW_ONE], 0, 0),
        3 => (vkf::R8G8_UNORM, [SW_R, SW_R, SW_R, SW_G], 0, 0),
        4 => (
            pick(vkf::R8G8B8A8_SRGB, vkf::R8G8B8A8_UNORM),
            [0; 4],
            CONVERT_RGB_TO_RGBA,
            0,
        ),
        5 => (pick(vkf::R8G8B8A8_SRGB, vkf::R8G8B8A8_UNORM), [0; 4], 0, 0),
        6 => (
            pick(vkf::B8G8R8A8_SRGB, vkf::B8G8R8A8_UNORM),
            [0; 4],
            CONVERT_RGB_TO_RGBA,
            0,
        ),
        7 => (pick(vkf::B8G8R8A8_SRGB, vkf::B8G8R8A8_UNORM), [0; 4], 0, 0),
        8 => (pick(vkf::BC1_RGBA_SRGB, vkf::BC1_RGBA_UNORM), [0; 4], 0, 0),
        9 | 10 => (pick(vkf::BC2_SRGB, vkf::BC2_UNORM), [0; 4], 0, 0),
        11 | 12 => (pick(vkf::BC3_SRGB, vkf::BC3_UNORM), [0; 4], 0, 0),
        27 => (vkf::BC4_UNORM, [0; 4], 0, 0),
        13 => (vkf::BC5_UNORM, [SW_R, SW_R, SW_R, SW_G], 0, 0),
        28 => (vkf::BC6H_UFLOAT, [0; 4], 0, 0),
        29 => (pick(vkf::BC7_SRGB, vkf::BC7_UNORM), [0; 4], 0, 0),
        14 => (vkf::D16_UNORM, [0; 4], 0, 0),
        15 => (depth24, [0; 4], 0, 0),
        16 => (vkf::D32_SFLOAT, [0; 4], 0, 0),
        17 => (
            vkf::R16_SFLOAT,
            [SW_ZERO, SW_ZERO, SW_ZERO, SW_R],
            CONVERT_FLOAT_TO_HALF,
            1,
        ),
        18 => (
            vkf::R16_SFLOAT,
            [SW_R, SW_R, SW_R, SW_ONE],
            CONVERT_FLOAT_TO_HALF,
            1,
        ),
        19 => (
            vkf::R16G16_SFLOAT,
            [SW_R, SW_R, SW_R, SW_G],
            CONVERT_FLOAT_TO_HALF,
            2,
        ),
        20 => (vkf::R16G16B16A16_SFLOAT, [0; 4], CONVERT_FLOAT_TO_HALF, 3),
        21 => (vkf::R16G16B16A16_SFLOAT, [0; 4], CONVERT_FLOAT_TO_HALF, 4),
        22 => (vkf::R32_SFLOAT, [SW_ZERO, SW_ZERO, SW_ZERO, SW_R], 0, 0),
        23 => (vkf::R32_SFLOAT, [SW_R, SW_R, SW_R, SW_ONE], 0, 0),
        24 => (vkf::R32G32_SFLOAT, [SW_R, SW_R, SW_R, SW_G], 0, 0),
        25 => (vkf::R32G32B32A32_SFLOAT, [0; 4], CONVERT_FLOAT3_TO_4, 0),
        26 => (vkf::R32G32B32A32_SFLOAT, [0; 4], 0, 0),
        _ => (vkf::UNDEFINED, [SW_IDENTITY; 4], 0, 0),
    };
    Format {
        format,
        swizzle,
        convert,
        src_components,
    }
}

pub fn unorm_of_srgb(f: u32) -> u32 {
    match f {
        vkf::R8G8B8A8_SRGB => vkf::R8G8B8A8_UNORM,
        vkf::B8G8R8A8_SRGB => vkf::B8G8R8A8_UNORM,
        _ => f,
    }
}

pub fn format_size(f: u32) -> usize {
    match f {
        vkf::R8_UNORM => 1,
        vkf::R8G8_UNORM | vkf::R16_SFLOAT | vkf::D16_UNORM => 2,
        vkf::R16G16B16A16_SFLOAT => 8,
        vkf::R32G32B32A32_SFLOAT => 16,
        _ => 4,
    }
}

pub fn float_to_half(f: f32) -> u16 {
    let x = f.to_bits();
    let s = (x >> 16) & 0x8000;
    let e = ((x >> 23) & 0xFF) as i32 - 127 + 15;
    let mut m = x & 0x7F_FFFF;
    let half = |v: u32| (v & 0xFFFF) as u16;
    if (x >> 23) & 0xFF == 0xFF {
        return half(s | 0x7C00 | if m != 0 { 0x200 } else { 0 });
    }
    if e >= 31 {
        return half(s | 0x7C00);
    }
    if e <= 0 {
        if e < -10 {
            return half(s);
        }
        m |= 0x80_0000;
        return half(s | (m >> (14 - e)));
    }
    half(s | ((e as u32) << 10) | (m >> 13))
}

/// Bytes `convert_pixels` reads from the source.
pub fn convert_src_len(f: &Format, pixels: usize) -> Option<usize> {
    match f.convert {
        CONVERT_RGB_TO_RGBA => pixels.checked_mul(3),
        CONVERT_FLOAT_TO_HALF => pixels
            .checked_mul(usize::try_from(f.src_components).ok()?)?
            .checked_mul(4),
        CONVERT_FLOAT3_TO_4 => pixels.checked_mul(12),
        _ => Some(0),
    }
}

/// Bytes `convert_pixels` writes; 0 for `CONVERT_NONE`. `None` where C++ `resize` threw.
pub fn convert_dst_len(f: &Format, pixels: usize) -> Option<usize> {
    match f.convert {
        CONVERT_RGB_TO_RGBA => pixels.checked_mul(4),
        CONVERT_FLOAT_TO_HALF => {
            let dst = if f.src_components == 3 {
                4
            } else {
                f.src_components
            };
            pixels
                .checked_mul(usize::try_from(dst).ok()?)?
                .checked_mul(2)
        }
        CONVERT_FLOAT3_TO_4 => pixels.checked_mul(16),
        _ => Some(0),
    }
}

/// Converts `pixels` pixels; `dst` is exactly `convert_dst_len` long. A short source reads as zero
/// (C++ read past the end).
pub fn convert_pixels(f: &Format, src: &[u8], pixels: usize, dst: &mut [u8]) {
    let byte = |i: usize| src.get(i).copied().unwrap_or(0);
    let float = |i: usize| {
        f32::from_bits(u32::from_le_bytes([
            byte(i * 4),
            byte(i * 4 + 1),
            byte(i * 4 + 2),
            byte(i * 4 + 3),
        ]))
    };
    match f.convert {
        CONVERT_RGB_TO_RGBA => {
            for (i, d) in dst.chunks_exact_mut(4).take(pixels).enumerate() {
                for (c, o) in d.iter_mut().enumerate() {
                    *o = if c == 3 { 255 } else { byte(i * 3 + c) };
                }
            }
        }
        CONVERT_FLOAT_TO_HALF => {
            let sc = usize::try_from(f.src_components).unwrap_or(0);
            let dc = if sc == 3 { 4 } else { sc };
            if dc == 0 {
                return;
            }
            for (i, px) in dst.chunks_exact_mut(dc * 2).take(pixels).enumerate() {
                for (c, o) in px.chunks_exact_mut(2).enumerate() {
                    let h = if c < sc {
                        float_to_half(float(i * sc + c))
                    } else {
                        float_to_half(1.0)
                    };
                    o.copy_from_slice(&h.to_ne_bytes());
                }
            }
        }
        CONVERT_FLOAT3_TO_4 => {
            for (i, px) in dst.chunks_exact_mut(16).take(pixels).enumerate() {
                for (c, o) in px.chunks_exact_mut(4).enumerate() {
                    let v = if c == 3 { 1.0 } else { float(i * 3 + c) };
                    o.copy_from_slice(&v.to_ne_bytes());
                }
            }
        }
        _ => {}
    }
}

pub fn full_mip_count(x: i32, y: i32, z: i32) -> i32 {
    let mut max = x.max(y.max(z));
    let mut n = 1;
    while max > 1 {
        max >>= 1;
        n += 1;
    }
    n
}

fn half(v: i32) -> i32 {
    v.checked_div(2).unwrap_or(0)
}

/// Exact-halving 2x2 box filter on 8-bit data. Returns the new size; `dst` holds
/// `new.x * new.y * bpp` bytes. A short source reads as zero (C++ read past the end).
pub fn halve_size(x: i32, y: i32) -> (i32, i32) {
    (half(x).max(1), half(y).max(1))
}

pub fn halve_image(src: &[u8], size: (i32, i32), bpp: usize, dst: &mut [u8]) {
    let (nx, ny) = halve_size(size.0, size.1);
    let (nxu, nyu) = (
        usize::try_from(nx).unwrap_or(0),
        usize::try_from(ny).unwrap_or(0),
    );
    let sw = usize::try_from(size.0).unwrap_or(0);
    for y in 0..nyu {
        for x in 0..nxu {
            for c in 0..bpp {
                let mut sum = 0i32;
                for dy in 0..2usize {
                    for dx in 0..2usize {
                        let sx = (size.0 - 1).min(i32::try_from(x * 2 + dx).unwrap_or(i32::MAX));
                        let sy = (size.1 - 1).min(i32::try_from(y * 2 + dy).unwrap_or(i32::MAX));
                        let idx = usize::try_from(sy)
                            .unwrap_or(0)
                            .wrapping_mul(sw)
                            .wrapping_add(usize::try_from(sx).unwrap_or(0))
                            .wrapping_mul(bpp)
                            .wrapping_add(c);
                        sum += i32::from(src.get(idx).copied().unwrap_or(0));
                    }
                }
                if let Some(d) = dst.get_mut((y * nxu + x) * bpp + c) {
                    *d = ((sum + 2) >> 2) as u8;
                }
            }
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct MipIn {
    pub size: [i32; 3],
    pub num_mips: i32,
    pub downscale_level: u32,
    pub min_size: [i32; 3],
    pub use_mipmaps: bool,
    pub compressed: bool,
    pub depth: bool,
    pub floating: bool,
    pub cube: bool,
    pub tex2d: bool,
    pub rect: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct MipPlan {
    pub size: [i32; 3],
    pub use_mipmaps: bool,
    pub start: i32,
    pub cpu_resize: bool,
    pub halve_steps: i32,
    pub provided: bool,
    pub levels: i32,
}

/// `CreateFromBitmapToIndex` planning: how many mips to skip, whether the CPU halves the base,
/// the final base size and the level count.
pub fn mip_plan(i: &MipIn) -> MipPlan {
    let [mut x, mut y, mut z] = i.size;
    let mut use_mipmaps = i.use_mipmaps;
    if i.compressed && i.num_mips <= 1 {
        use_mipmaps = false;
    }
    if i.rect {
        use_mipmaps = false;
    }
    let mut start = 0;
    let mut cpu_resize = false;
    let level = i.downscale_level as i32;
    if i.downscale_level > 0 && (!i.compressed || i.num_mips > 1) && !i.depth {
        if i.num_mips > 1 {
            start = (i.num_mips - 1).min(level);
            let sh = |v: i32| {
                if start >= 32 {
                    if v < 0 { -1 } else { 0 }
                } else {
                    v >> start
                }
            };
            x = sh(x).max(1);
            y = sh(y).max(1);
            z = sh(z).max(1);
        } else if (i.tex2d || i.cube) && x > i.min_size[0] && y > i.min_size[1] && !i.floating {
            cpu_resize = true;
        }
    }
    let mut steps = 0;
    let (mut sx, mut sy) = (x, y);
    if cpu_resize {
        for _ in 0..level {
            if half(sx) < i.min_size[0] || half(sy) < i.min_size[1] {
                break;
            }
            (sx, sy) = halve_size(sx, sy);
            steps += 1;
        }
        if steps > 0 {
            (x, y, z) = (sx, sy, 1);
        }
    }
    let provided = use_mipmaps && i.num_mips > 1 && !cpu_resize;
    let levels = if !use_mipmaps {
        1
    } else if provided {
        i.num_mips - start
    } else {
        full_mip_count(x, y, z)
    };
    MipPlan {
        size: [x, y, z],
        use_mipmaps,
        start,
        cpu_resize,
        halve_steps: steps,
        provided,
        levels,
    }
}

pub const DRAW_TRI: i32 = 0;
pub const DRAW_QUAD: i32 = 3;
pub const DRAW_QUAD_STRIP: i32 = 4;
pub const DRAW_LINE_LOOP: i32 = 6;

pub fn prim(draw_type: i32) -> i32 {
    match draw_type {
        1 => 1,
        2 => 2,
        5 => 3,
        6 | 7 => 4,
        _ => 0,
    }
}

/// Indices produced for primitives Vulkan lacks; 0 where the source is used unchanged.
pub fn converted_len(draw_type: i32, count: i32) -> usize {
    let c = usize::try_from(count).unwrap_or(0);
    match draw_type {
        DRAW_LINE_LOOP => c + 1,
        DRAW_QUAD => (c >> 2) * 6,
        DRAW_QUAD_STRIP => (c.saturating_sub(2) >> 1) * 6,
        _ => 0,
    }
}

/// Fills `out` (exactly `converted_len`) and returns whether it replaces the source indices.
pub fn convert_indices(draw_type: i32, idx: &[u32], out: &mut [u32]) -> bool {
    let at = |i: usize| idx.get(i).copied().unwrap_or(0);
    match draw_type {
        DRAW_LINE_LOOP => {
            for (i, o) in out.iter_mut().enumerate() {
                *o = if i < idx.len() { at(i) } else { at(0) };
            }
            true
        }
        DRAW_QUAD => {
            for (q, quad) in out.chunks_exact_mut(6).enumerate() {
                for (o, k) in quad.iter_mut().zip([0usize, 1, 2, 0, 2, 3]) {
                    *o = at(q * 4 + k);
                }
            }
            true
        }
        DRAW_QUAD_STRIP => {
            for (q, quad) in out.chunks_exact_mut(6).enumerate() {
                for (o, k) in quad.iter_mut().zip([0usize, 1, 3, 0, 3, 2]) {
                    *o = at(q * 2 + k);
                }
            }
            true
        }
        _ => false,
    }
}

#[repr(C)]
pub struct SamplerKey {
    pub mag_filter: u32,
    pub min_filter: u32,
    pub mipmap_mode: u32,
    pub address_u: u32,
    pub address_v: u32,
    pub address_w: u32,
    pub compare_enable: u32,
    pub compare_op: u32,
    pub max_anisotropy: f32,
    pub anisotropy_enable: u32,
    pub max_lod: f32,
}

/// C++ cast negative/NaN floats to `uint64_t` (UB); saturates to 0 here, like the hardware did.
pub fn sampler_key(k: &SamplerKey) -> u64 {
    u64::from(k.mag_filter)
        | u64::from(k.min_filter) << 2
        | u64::from(k.mipmap_mode) << 4
        | u64::from(k.address_u) << 6
        | u64::from(k.address_v) << 9
        | u64::from(k.address_w) << 12
        | u64::from(k.compare_enable) << 15
        | u64::from(k.compare_op) << 16
        | ((k.max_anisotropy * 4.0) as u64) << 20
        | u64::from(k.anisotropy_enable) << 28
        | ((k.max_lod * 4.0) as u64) << 29
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Ring {
    pub offset: usize,
    pub grow: bool,
    pub new_size: usize,
    pub used: usize,
}

/// `AllocTransient`: where the next allocation lands and whether the ring buffer is replaced.
pub fn ring_alloc(used: usize, size: usize, alloc: usize, align: usize) -> Ring {
    let mut offset = used.wrapping_add(align).wrapping_sub(1) & !align.wrapping_sub(1);
    let mut grow = false;
    let mut new_size = size;
    if offset.wrapping_add(alloc) > size {
        grow = true;
        new_size = size.wrapping_mul(2).max(alloc.wrapping_mul(2));
        offset = 0;
    }
    Ring {
        offset,
        grow,
        new_size,
        used: offset.wrapping_add(alloc),
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Viewport {
    pub viewport: [f32; 4],
    pub scissor: [i32; 2],
    pub scissor_size: [u32; 2],
}

/// `ApplyState` viewport and scissor; `total` is the target size.
pub fn viewport(
    total: [i32; 2],
    pos: [i32; 2],
    size: [i32; 2],
    scissor: Option<([i32; 2], [i32; 2])>,
) -> Viewport {
    let mut v = Viewport {
        viewport: [
            pos[0] as f32,
            total[1].wrapping_sub(size[1]).wrapping_sub(pos[1]) as f32,
            size[0] as f32,
            size[1] as f32,
        ],
        scissor: [0, 0],
        scissor_size: [total[0] as u32, total[1] as u32],
    };
    if let Some((sp, ss)) = scissor {
        let x0 = sp[0].max(0);
        let y0 = sp[1].max(0);
        let x1 = x0.max(sp[0].wrapping_add(ss[0]));
        let y1 = y0.max(sp[1].wrapping_add(ss[1]));
        v.scissor = [x0, y0];
        v.scissor_size = [x1.wrapping_sub(x0) as u32, y1.wrapping_sub(y0) as u32];
    }
    v
}

/// `GlOrtho` as row-major `m[r][c]`.
pub fn gl_ortho(l: f32, r: f32, b: f32, t: f32, n: f32, f: f32) -> [f32; 16] {
    [
        2.0 / (r - l),
        0.0,
        0.0,
        -(r + l) / (r - l),
        0.0,
        2.0 / (t - b),
        0.0,
        -(t + b) / (t - b),
        0.0,
        0.0,
        -2.0 / (f - n),
        -(f + n) / (f - n),
        0.0,
        0.0,
        0.0,
        1.0,
    ]
}

/// `WriteMatrix`: column-major into std140 columns of 4 floats; `m` is row-major.
pub fn write_matrix(dst: &mut [f32], m: &[f32; 16], cols: i32) {
    let cols = usize::try_from(cols).unwrap_or(0);
    for c in 0..cols {
        for r in 0..4 {
            let v = if r < cols {
                m.get(r * 4 + c).copied().unwrap_or(0.0)
            } else {
                0.0
            };
            if let Some(d) = dst.get_mut(c * 4 + r) {
                *d = v;
            }
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Uniform {
    pub offset: i32,
    pub array_stride: i32,
    pub matrix_columns: i32,
    pub count: i32,
    pub is_int: bool,
    pub is_bool: bool,
    pub components: i32,
}

/// Reflected GL type, array size and stride to the stored uniform description.
pub fn reflect_uniform(offset: i32, gl_type: i32, size: i32, array_stride: i32) -> Uniform {
    let matrix_columns = match gl_type {
        0x8B5B => 3,
        0x8B5C => 4,
        _ => 0,
    };
    let components = match gl_type {
        0x8B50 | 0x8B53 | 0x8B57 => 2,
        0x8B51 | 0x8B54 | 0x8B58 => 3,
        0x8B52 | 0x8B55 | 0x8B59 => 4,
        _ if matrix_columns != 0 => matrix_columns * matrix_columns,
        _ => 1,
    };
    Uniform {
        offset,
        array_stride: if array_stride > 0 { array_stride } else { 16 },
        matrix_columns,
        count: size.max(1),
        is_int: gl_type == 0x1404 || gl_type == 0x1405 || (0x8B53..=0x8B59).contains(&gl_type),
        is_bool: (0x8B56..=0x8B59).contains(&gl_type),
        components,
    }
}

/// `"name"` or `"name[i]"`: (length of the base name, element).
pub fn split_uniform_name(name: &[u8]) -> (usize, i32) {
    match name.iter().position(|&b| b == b'[') {
        Some(p) => (
            p,
            super::glsl_rewrite::atoi(name.get(p + 1..).unwrap_or(&[])),
        ),
        None => (name.len(), 0),
    }
}

fn slot(u: &Uniform, element: i32) -> Option<usize> {
    let off = i64::from(u.offset) + i64::from(element) * i64::from(u.array_stride);
    usize::try_from(off).ok()
}

/// `cVulkanProgram::Write`; false where it returned false (earlier elements stay written).
pub fn write_uniform(
    data: &mut [u8],
    u: &Uniform,
    first: i32,
    comps: i32,
    count: i32,
    src: &[f32],
) -> bool {
    if comps != u.components || (u.is_int && !u.is_bool) {
        return false;
    }
    let n = count.min(u.count.wrapping_sub(first));
    let comps_u = usize::try_from(comps).unwrap_or(0);
    for e in 0..n.max(0) {
        let Some(base) = slot(u, first.wrapping_add(e)) else {
            return false;
        };
        if base.saturating_add(comps_u * 4) > data.len() {
            return false;
        }
        for c in 0..comps_u {
            let v = src
                .get(usize::try_from(e).unwrap_or(0) * comps_u + c)
                .copied()
                .unwrap_or(0.0);
            let bytes = if u.is_int {
                (v as i32).to_ne_bytes()
            } else {
                v.to_ne_bytes()
            };
            if let Some(d) = data.get_mut(base + c * 4..base + c * 4 + 4) {
                d.copy_from_slice(&bytes);
            }
        }
    }
    true
}

pub fn write_int(data: &mut [u8], u: &Uniform, element: i32, x: i32) -> bool {
    if !u.is_int || u.components != 1 {
        return false;
    }
    let Some(base) = slot(u, element) else {
        return false;
    };
    match data.get_mut(base..base.saturating_add(4)) {
        Some(d) if d.len() == 4 => {
            d.copy_from_slice(&x.to_ne_bytes());
            true
        }
        _ => true,
    }
}

pub fn write_mat4(data: &mut [u8], u: &Uniform, element: i32, m: &[f32; 16]) -> bool {
    if u.matrix_columns != 4 {
        return false;
    }
    let off = i64::from(u.offset) + i64::from(element) * i64::from(u.array_stride);
    if off + 64 > data.len() as i64 {
        return false;
    }
    let Ok(base) = usize::try_from(off) else {
        return false;
    };
    for c in 0..4 {
        for r in 0..4 {
            let v = m.get(r * 4 + c).copied().unwrap_or(0.0);
            let o = base + (c * 4 + r) * 4;
            if let Some(d) = data.get_mut(o..o + 4) {
                d.copy_from_slice(&v.to_ne_bytes());
            }
        }
    }
    true
}

/// `UnpackPixel`: RGBA floats of one texel; a short source reads as zero.
pub fn unpack_pixel(format: u32, p: &[u8]) -> [f32; 4] {
    let b = |i: usize| f32::from(p.get(i).copied().unwrap_or(0));
    let u16at = |i: usize| {
        u16::from(p.get(i * 2).copied().unwrap_or(0))
            | u16::from(p.get(i * 2 + 1).copied().unwrap_or(0)) << 8
    };
    let u32at = || u32::from(u16at(0)) | u32::from(u16at(1)) << 16;
    let f32at =
        |i: usize| f32::from_bits(u32::from(u16at(i * 2)) | u32::from(u16at(i * 2 + 1)) << 16);
    let half = |h: u16| -> f32 {
        let s = (h >> 15) & 1;
        let e = i32::from((h >> 10) & 0x1F);
        let m = i32::from(h & 0x3FF);
        let f = if e == 0 {
            m as f32 * 2f32.powi(-24)
        } else if e == 31 {
            if m != 0 { f32::NAN } else { f32::INFINITY }
        } else {
            (m | 0x400) as f32 * 2f32.powi(e - 25)
        };
        if s != 0 { -f } else { f }
    };
    match format {
        vkf::B8G8R8A8_UNORM | vkf::B8G8R8A8_SRGB => {
            [b(2) / 255.0, b(1) / 255.0, b(0) / 255.0, b(3) / 255.0]
        }
        vkf::R16G16B16A16_SFLOAT => [
            half(u16at(0)),
            half(u16at(1)),
            half(u16at(2)),
            half(u16at(3)),
        ],
        vkf::R16G16_SFLOAT => [half(u16at(0)), half(u16at(1)), 0.0, 1.0],
        vkf::R16_SFLOAT => {
            let h = half(u16at(0));
            [h, h, h, 1.0]
        }
        vkf::R32G32B32A32_SFLOAT => [f32at(0), f32at(1), f32at(2), f32at(3)],
        vkf::R32_SFLOAT | vkf::D32_SFLOAT | vkf::D32_SFLOAT_S8_UINT => {
            let v = f32at(0);
            [v, v, v, 1.0]
        }
        vkf::D24_UNORM_S8_UINT | vkf::X8_D24_UNORM_PACK32 => {
            let v = (u32at() & 0xFF_FFFF) as f32 / 16_777_215.0;
            [v, v, v, 1.0]
        }
        vkf::D16_UNORM => {
            let v = f32::from(u16at(0)) / 65535.0;
            [v, v, v, 1.0]
        }
        vkf::R8_UNORM => {
            let v = b(0) / 255.0;
            [v, v, v, 1.0]
        }
        vkf::R8G8_UNORM => [b(0) / 255.0, b(1) / 255.0, 0.0, 1.0],
        _ => [b(0) / 255.0, b(1) / 255.0, b(2) / 255.0, b(3) / 255.0],
    }
}
