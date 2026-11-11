//! YUV -> RGB conversion tables and frame conversion of `VideoStreamTheora`. libogg/libtheora
//! calls and stream state stay in C++.

pub const R_LEN: usize = 256 * 256;
pub const G_UV_LEN: usize = 256 * 256;
pub const G_Y_UV_LEN: usize = 256 * 1024;

fn clamp(x: f32, min: f32, max: f32) -> f32 {
    if x > max {
        max
    } else if x < min {
        min
    } else {
        x
    }
}

pub struct Tables {
    pub to_r: Vec<u8>,
    pub to_b: Vec<u8>,
    pub g_uv: Vec<u16>,
    pub g_y_uv: Vec<u8>,
}

/// gcc on aarch64 fuses the red sum (into the luma multiply) and the green terms; blue stays unfused.
pub fn build_tables() -> Tables {
    let mut to_r = Vec::with_capacity(R_LEN);
    let mut to_b = Vec::with_capacity(R_LEN);
    for i in 0..256 {
        let fi = i as f32;
        for y in 0..256 {
            let fy = 1.164f32 * (y as f32 - 16.0);
            to_r.push(clamp(
                1.164f32.mul_add(y as f32 - 16.0, 1.596f32 * (fi - 128.0)),
                0.0,
                255.0,
            ) as u8);
            to_b.push(clamp(fy + 2.018f32 * (fi - 128.0), 0.0, 255.0) as u8);
        }
    }
    let min_val = 0.813f32.mul_add(-128.0, -(0.391f32 * 127.0));
    let max_val = 0.813f32.mul_add(127.0, 0.391f32 * 128.0);
    let mut g_uv = Vec::with_capacity(G_UV_LEN);
    for i in 0..256 {
        let fi = i as f32;
        for j in 0..256 {
            let fj = j as f32;
            let uv = 0.813f32.mul_add(fj - 128.0, -(0.391f32 * (fi - 128.0)));
            g_uv.push(clamp(((uv - min_val) / (max_val - min_val)) * 1023.0, 0.0, 1023.0) as u16);
        }
    }
    let mut g_y_uv = Vec::with_capacity(G_Y_UV_LEN);
    for i in 0..1024 {
        let uv = ((i as f32) / 1023.0).mul_add(max_val - min_val, min_val);
        for y in 0..256 {
            let fy = 1.164f32 * (y as f32 - 16.0);
            g_y_uv.push(clamp(fy - uv, 0.0, 255.0) as u8);
        }
    }
    Tables {
        to_r,
        to_b,
        g_uv,
        g_y_uv,
    }
}

pub struct TablesRef<'a> {
    pub to_r: &'a [u8],
    pub to_b: &'a [u8],
    pub g_uv: &'a [u16],
    pub g_y_uv: &'a [u8],
}

impl Tables {
    pub fn as_ref(&self) -> TablesRef<'_> {
        TablesRef {
            to_r: &self.to_r,
            to_b: &self.to_b,
            g_uv: &self.g_uv,
            g_y_uv: &self.g_y_uv,
        }
    }
}

pub struct Planes<'a> {
    pub y: &'a [u8],
    pub u: &'a [u8],
    pub v: &'a [u8],
    pub y_stride: usize,
    pub uv_stride: usize,
    pub offset_x: usize,
    pub offset_y: usize,
}

/// RGB buffer size; 0 where `w * h * 3` does not fit an `int` (the original overflowed).
pub fn frame_buffer_len(w: i32, h: i32) -> usize {
    if w <= 0 || h <= 0 {
        return 0;
    }
    w.checked_mul(h)
        .and_then(|n| n.checked_mul(3))
        .map_or(0, |n| n as usize)
}

/// Converts into `out` (`w * h * 3` bytes). Returns false when a plane or the buffer is too small
/// for the frame, after having written the part that fit. Frames with a zero-width chroma row
/// (width 1) skip chroma: the original looped past the buffer there.
pub fn draw_frame(
    out: &mut [u8],
    planes: &Planes<'_>,
    tables: &TablesRef<'_>,
    w: i32,
    h: i32,
) -> bool {
    let (Ok(w), Ok(h)) = (usize::try_from(w), usize::try_from(h)) else {
        return false;
    };
    if w == 0 || h == 0 {
        return true;
    }
    let crop_y = planes
        .offset_x
        .wrapping_add(planes.y_stride.wrapping_mul(planes.offset_y));
    for r in 0..h {
        let src = crop_y.wrapping_add(r.wrapping_mul(planes.y_stride));
        let Some(row) = src.checked_add(w).and_then(|e| planes.y.get(src..e)) else {
            return false;
        };
        for (c, &px) in row.iter().enumerate() {
            let Some(o) = out.get_mut(r.wrapping_mul(w).wrapping_add(c).wrapping_mul(3)) else {
                return false;
            };
            *o = px;
        }
    }

    let (half_w, half_h) = (w >> 1, h >> 1);
    if half_w == 0 {
        return true;
    }
    let crop_uv =
        (planes.offset_x >> 1).wrapping_add(planes.uv_stride.wrapping_mul(planes.offset_y >> 1));
    let row_size = w.wrapping_mul(3);
    let mut base = 0usize;
    for r in 0..half_h {
        for c in 0..half_w {
            let at = crop_uv
                .wrapping_add(r.wrapping_mul(planes.uv_stride))
                .wrapping_add(c);
            let (Some(&u), Some(&v)) = (planes.u.get(at), planes.v.get(at)) else {
                return false;
            };
            let u_add = usize::from(u) << 8;
            let v_add = usize::from(v) << 8;
            let Some(&g) = tables.g_uv.get(u_add + usize::from(v)) else {
                return false;
            };
            let g_add = usize::from(g) << 8;
            for p in [base, base + 3, base + row_size, base + row_size + 3] {
                let Some(px) = out.get_mut(p..p + 3) else {
                    return false;
                };
                let y = usize::from(px.first().copied().unwrap_or(0));
                let (Some(&r), Some(&g), Some(&b)) = (
                    tables.to_r.get(y + v_add),
                    tables.g_y_uv.get(y + g_add),
                    tables.to_b.get(y + u_add),
                ) else {
                    return false;
                };
                px.copy_from_slice(&[r, g, b]);
            }
            base += 6;
        }
        base += row_size;
    }
    true
}
