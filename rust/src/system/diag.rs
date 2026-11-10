//! Pure parts of `cEngineDiagnostics`: JSON helpers and pixel statistics.

use crate::string::float_to_string;

/// `cEngineDiagnostics::JsonEscape`: control characters other than newline become spaces.
pub fn json_escape(s: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(s.len());
    for &c in s {
        match c {
            b'"' | b'\\' => out.extend_from_slice(&[b'\\', c]),
            b'\n' => out.extend_from_slice(b"\\n"),
            0..0x20 => out.push(b' '),
            _ => out.push(c),
        }
    }
    out
}

/// `Num`: six decimals with trailing zeros dropped, `null` for NaN and infinities.
pub fn num(x: f32) -> Vec<u8> {
    if x.is_finite() {
        float_to_string(x, 6, true)
    } else {
        b"null".to_vec()
    }
}

fn int(x: i64) -> Vec<u8> {
    x.to_string().into_bytes()
}

/// `GetPixelStatsJson` over `pixels` (RGBA floats, `width * height` of them).
pub fn pixel_stats_json(pixels: &[f32], width: i32, height: i32) -> Vec<u8> {
    let mut out = b"{\"width\":".to_vec();
    out.extend(int(width.into()));
    out.extend_from_slice(b",\"height\":");
    out.extend(int(height.into()));
    let count = (width as usize).wrapping_mul(height as usize);
    for (c, name) in (*b"rgba").into_iter().enumerate() {
        let (mut min, mut max, mut sum) = (1e30_f32, -1e30_f32, 0.0_f64);
        let (mut nan, mut zero) = (0_i32, 0_i32);
        for &v in pixels.iter().skip(c).step_by(4).take(count) {
            if v.is_nan() {
                nan += 1;
                continue;
            }
            if v == 0.0 {
                zero += 1;
            }
            if v < min {
                min = v;
            }
            if v > max {
                max = v;
            }
            sum += f64::from(v);
        }
        let valid = count.wrapping_sub(nan as usize);
        let has = valid != 0;
        let mean = if has {
            (sum / valid as f64) as f32
        } else {
            0.0
        };
        out.extend_from_slice(b",\"");
        out.push(name);
        out.extend_from_slice(b"\":{\"min\":");
        out.extend(num(if has { min } else { 0.0 }));
        out.extend_from_slice(b",\"max\":");
        out.extend(num(if has { max } else { 0.0 }));
        out.extend_from_slice(b",\"mean\":");
        out.extend(num(mean));
        out.extend_from_slice(b",\"nan\":");
        out.extend(int(nan.into()));
        out.extend_from_slice(b",\"zero\":");
        out.extend(int(zero.into()));
        out.push(b'}');
    }
    out.push(b'}');
    out
}

/// `GetFrameStatsJson` over `pixels` (`bpp` bytes per pixel, RGB first). Missing bytes read as 0.
pub fn frame_stats_json(pixels: &[u8], width: i32, height: i32, bpp: i32) -> Vec<u8> {
    let count = (width as usize).wrapping_mul(height as usize);
    let step = bpp as usize;
    let mut hist = [0_i32; 16];
    let (mut black, mut white, mut magenta) = (0_usize, 0_usize, 0_usize);
    let mut lum_sum = 0.0_f64;
    let (mut lum_min, mut lum_max) = (255_i32, 0_i32);
    for i in 0..count {
        let at = |k: usize| {
            i.wrapping_mul(step)
                .checked_add(k)
                .and_then(|p| pixels.get(p))
                .map_or(0, |&b| i32::from(b))
        };
        let (r, g, b) = (at(0), at(1), at(2));
        let lum = (r * 54 + g * 183 + b * 19) >> 8;
        lum_sum += f64::from(lum);
        lum_min = lum_min.min(lum);
        lum_max = lum_max.max(lum);
        if let Some(h) = hist.get_mut((lum >> 4) as usize) {
            *h += 1;
        }
        if r < 4 && g < 4 && b < 4 {
            black += 1;
        }
        if r > 250 && g > 250 && b > 250 {
            white += 1;
        }
        if r > 200 && b > 200 && g < 60 {
            magenta += 1;
        }
    }
    let inv = if count != 0 { 1.0 / count as f64 } else { 0.0 };
    let mut out = b"{\"width\":".to_vec();
    out.extend(int(width.into()));
    out.extend_from_slice(b",\"height\":");
    out.extend(int(height.into()));
    out.extend_from_slice(b",\"lum_mean\":");
    out.extend(num((lum_sum * inv) as f32));
    out.extend_from_slice(b",\"lum_min\":");
    out.extend(int(lum_min.into()));
    out.extend_from_slice(b",\"lum_max\":");
    out.extend(int(lum_max.into()));
    for (key, v) in [
        (&b",\"black_frac\":"[..], black),
        (b",\"white_frac\":", white),
        (b",\"magenta_frac\":", magenta),
    ] {
        out.extend_from_slice(key);
        out.extend(num((v as f64 * inv) as f32));
    }
    out.extend_from_slice(b",\"lum_hist16\":[");
    for (i, &h) in hist.iter().enumerate() {
        if i > 0 {
            out.push(b',');
        }
        out.extend(num((f64::from(h) * inv) as f32));
    }
    out.extend_from_slice(b"]}");
    out
}

/// `ApplyDisplayGamma` lookup table.
pub fn gamma_lut(gamma: f32) -> [u8; 256] {
    let mut lut = [0_u8; 256];
    for (i, v) in lut.iter_mut().enumerate() {
        *v = ((i as f32 / 255.0).powf(1.0 / gamma) * 255.0 + 0.5) as u8;
    }
    lut
}
