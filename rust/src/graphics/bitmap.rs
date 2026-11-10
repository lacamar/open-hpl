//! `cBitmap::Clear`, `Blit` and pixel-format conversion.
//!
//! The C++ converts through two process-wide 4-byte scratch pixels, so formats the switch does
//! not handle (compressed, depth, 16/32-bit) read or write whatever the last conversion left
//! there. `TEMP_RGBA` and `TEMP_OUT` keep that state. Reads and writes past the buffers, which
//! the C++ performs as undefined behaviour, stop the operation instead.

use std::sync::atomic::{AtomicU32, Ordering};

use super::formats::{self, ALPHA, BGR, BGRA, LUMINANCE, LUMINANCE_ALPHA, RGB, RGBA};
use crate::types::Vector3l;

static TEMP_RGBA: AtomicU32 = AtomicU32::new(0);
static TEMP_OUT: AtomicU32 = AtomicU32::new(0);

struct Temps {
    rgba: [u8; 4],
    out: [u8; 4],
}

impl Temps {
    fn load() -> Self {
        Temps {
            rgba: TEMP_RGBA.load(Ordering::Relaxed).to_le_bytes(),
            out: TEMP_OUT.load(Ordering::Relaxed).to_le_bytes(),
        }
    }

    fn store(&self) {
        TEMP_RGBA.store(u32::from_le_bytes(self.rgba), Ordering::Relaxed);
        TEMP_OUT.store(u32::from_le_bytes(self.out), Ordering::Relaxed);
    }
}

fn src_reads(f: i32) -> usize {
    match f {
        ALPHA | LUMINANCE => 1,
        LUMINANCE_ALPHA => 2,
        RGB | BGR => 3,
        RGBA | BGRA => 4,
        _ => 0,
    }
}

fn to_rgba(t: &mut [u8; 4], f: i32, px: &[u8]) {
    let p = |i: usize| px.get(i).copied().unwrap_or(0);
    match f {
        ALPHA => t.copy_from_slice(&[255, 255, 255, p(0)]),
        LUMINANCE => t.copy_from_slice(&[p(0), p(0), p(0), 255]),
        LUMINANCE_ALPHA => t.copy_from_slice(&[p(0), p(0), p(0), p(1)]),
        RGB => t.copy_from_slice(&[p(0), p(1), p(2), 255]),
        RGBA => t.copy_from_slice(&[p(0), p(1), p(2), p(3)]),
        BGR => t.copy_from_slice(&[p(2), p(1), p(0), 255]),
        BGRA => t.copy_from_slice(&[p(2), p(1), p(0), p(3)]),
        _ => {}
    }
}

fn from_rgba(out: &mut [u8; 4], c: &[u8; 4], f: i32) {
    let [r, g, b, a] = *c;
    match f {
        ALPHA => out[0] = a,
        LUMINANCE => out[0] = r,
        LUMINANCE_ALPHA => {
            out[0] = r;
            out[1] = a;
        }
        RGB => out[..3].copy_from_slice(&[r, g, b]),
        RGBA => *out = *c,
        BGR => out[..3].copy_from_slice(&[b, g, r]),
        BGRA => *out = [b, g, r, a],
        _ => {}
    }
}

/// `ConvertDataToFormat`; the result is `src` itself when the formats match, else `t.out`.
fn convert<'a>(t: &'a mut Temps, src: &'a [u8], sf: i32, df: i32) -> Option<&'a [u8; 4]> {
    if sf == df {
        return None;
    }
    to_rgba(&mut t.rgba, sf, src);
    let rgba = t.rgba;
    from_rgba(&mut t.out, &rgba, df);
    Some(&t.out)
}

/// `cBitmap::CopyPixel`; false when a source or destination pixel runs past its buffer.
fn copy_pixel(t: &mut Temps, dst: &mut [u8], df: i32, src: &[u8], sf: i32) -> bool {
    let n = usize::try_from(formats::channels(df)).unwrap_or(0);
    if sf != df && src.len() < src_reads(sf) {
        return false;
    }
    let from = match convert(t, src, sf, df) {
        Some(out) => out.as_slice(),
        None => src,
    };
    match (dst.get_mut(..n), from.get(..n)) {
        (Some(d), Some(s)) => {
            d.copy_from_slice(s);
            true
        }
        _ => false,
    }
}

pub struct Surface {
    pub size: Vector3l,
    pub format: i32,
    pub bytes_per_pixel: i32,
}

/// Fills `count` pixels of `bpp` bytes with `color` (RGBA bytes) converted to `format`.
pub fn clear(data: &mut [u8], count: i64, bpp: usize, format: i32, color: [u8; 4]) {
    let mut t = Temps::load();
    let px = match convert(&mut t, &color, RGBA, format) {
        Some(out) => *out,
        None => color,
    };
    t.store();
    if bpp == 0 {
        return;
    }
    let n = bpp.min(4);
    let count = usize::try_from(count).unwrap_or(0);
    for chunk in data.chunks_mut(bpp).take(count) {
        let n = n.min(chunk.len());
        if let (Some(d), Some(s)) = (chunk.get_mut(..n), px.get(..n)) {
            d.copy_from_slice(s);
        }
    }
}

fn v3(v: &Vector3l) -> [i64; 3] {
    [i64::from(v.x), i64::from(v.y), i64::from(v.z)]
}

/// `cBitmap::Blit` after the image/mip lookup. Positions and sizes clip exactly as the C++ does.
pub fn blit(
    dst: &Surface,
    dst_data: &mut [u8],
    src: &Surface,
    src_data: &[u8],
    dest_position: Vector3l,
    src_size: Vector3l,
    src_position: Vector3l,
) {
    let [dsx, dsy, dsz] = v3(&dst.size);
    let [ssx, ssy, ssz] = v3(&src.size);
    let [dpx, dpy, dpz] = v3(&dest_position);
    let [spx0, spy0, spz0] = v3(&src_position);
    if dpx >= dsx || dpy >= dsy || dpz >= dsz || spx0 >= ssx || spy0 >= ssy || spz0 >= ssz {
        return;
    }

    let [rx, ry, rz] = v3(&src_size);
    let (mut w, mut h, mut d) = (rx.min(ssx), ry.min(ssy), rz.min(ssz));
    if w <= 0 {
        w = 1;
    }
    if h <= 0 {
        h = 1;
    }
    if d <= 0 {
        d = 1;
    }

    let (mut sx, mut sy, mut sz) = (spx0, spy0, spz0);
    if sx < 0 {
        w += sx;
        sx = 0;
    }
    if sy < 0 {
        h += sy;
        sy = 0;
    }
    if sz < 0 {
        d += sz;
        sz = 0;
    }
    if sx + w > ssx {
        w = ssx - sx;
    }
    if sy + h > ssy {
        h = ssy - sy;
    }
    if sz + d > ssz {
        d = ssz - sz;
    }

    let (mut dx, mut dy, mut dz) = (dpx, dpy, dpz);
    if dx < 0 {
        w += dx;
        sx -= dx;
        dx = 0;
    }
    if dy < 0 {
        h += dy;
        sy -= dy;
        dy = 0;
    }
    if dz < 0 {
        d += dz;
        sz -= dz;
        dz = 0;
    }
    if dx + w > dsx {
        w = dsx - dx;
    }
    if dy + h > dsy {
        h = dsy - dy;
    }
    if dz + d > dsz {
        d = dsz - dz;
    }
    if w <= 0 || h <= 0 || d <= 0 {
        return;
    }

    let spix = i64::from(src.bytes_per_pixel);
    let dpix = i64::from(dst.bytes_per_pixel);
    let addr = |x: i64, y: i64, z: i64, sx: i64, sy: i64, pix: i64| {
        z.wrapping_mul(sx)
            .wrapping_mul(sy)
            .wrapping_add(y * sx)
            .wrapping_add(x)
            .wrapping_mul(pix)
    };
    let mut s_off = addr(sx, sy, sz, ssx, ssy, spix);
    let mut d_off = addr(dx, dy, dz, dsx, dsy, dpix);
    let s_wadd = (ssx - w) * spix;
    let d_wadd = (dsx - w) * dpix;
    let s_hadd = ((ssy - h) * ssx).wrapping_mul(spix);
    // The C++ scales the destination slice skip by the source pixel size.
    let d_hadd = ((dsy - h) * dsx).wrapping_mul(spix);

    let mut t = Temps::load();
    let (mut wc, mut hc, mut dc) = (w, h, d);
    while dc != 0 {
        let s = usize::try_from(s_off).ok().and_then(|o| src_data.get(o..));
        let dpx = usize::try_from(d_off)
            .ok()
            .and_then(|o| dst_data.get_mut(o..));
        let ok = match (dpx, s) {
            (Some(dp), Some(sp)) => copy_pixel(&mut t, dp, dst.format, sp, src.format),
            _ => false,
        };
        if !ok {
            break;
        }
        s_off = s_off.wrapping_add(spix);
        d_off = d_off.wrapping_add(dpix);
        wc -= 1;
        if wc == 0 {
            hc -= 1;
            wc = w;
            s_off = s_off.wrapping_add(s_wadd);
            d_off = d_off.wrapping_add(d_wadd);
            if hc == 0 {
                dc -= 1;
                hc = h;
                s_off = s_off.wrapping_add(s_hadd);
                d_off = d_off.wrapping_add(d_hadd);
            }
        }
    }
    t.store();
}
