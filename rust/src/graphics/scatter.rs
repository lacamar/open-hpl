//! `cTextureCreator::GenerateScatterDiskMap2D/3D` sample data: `size * size` jittered, warped
//! unit-disk sample sets packed two samples per RGBA texel. Draws from the libc `rand()` stream
//! in the same order as the C++.

use super::stdsort;
use crate::math::{TWO_PI, cos, rand_rectf, sin};
use crate::types::Vector2f;

/// Byte length of the texture data for `size` disks of `samples` samples.
pub fn data_len(size: i32, samples: i32) -> Option<usize> {
    let size = usize::try_from(size).ok()?;
    let half = usize::try_from(samples >> 1).ok()?;
    size.checked_mul(size)?.checked_mul(half)?.checked_mul(4)
}

fn length(v: &Vector2f) -> f32 {
    v.x.mul_add(v.x, v.y * v.y).sqrt()
}

fn to_byte(f: f32) -> u8 {
    f as i32 as u8
}

/// Fills `out` (length `data_len`); false when the arguments are unusable.
pub fn generate(size: i32, samples: i32, sort: bool, out: &mut [u8]) -> bool {
    let (Some(len), Ok(n_samples)) = (data_len(size, samples), usize::try_from(samples)) else {
        return false;
    };
    if len != out.len() || size <= 0 || samples <= 0 {
        return false;
    }
    out.fill(0);
    let sets = (size as usize) * (size as usize);
    let grid = ((samples as f32).sqrt() + 0.5) as usize;
    let inv = 1.0f32 / grid as f32;
    let mut cells = vec![Vector2f::default(); grid * grid];
    let mut offsets = vec![Vector2f::default(); n_samples];
    let depth = n_samples >> 1;

    for set in 0..sets {
        for y in 0..grid {
            for x in 0..grid {
                let mut p = Vector2f {
                    x: x as f32 + 0.5,
                    y: y as f32 + 0.5,
                };
                p.x += rand_rectf(-0.5, 0.5);
                p.y += rand_rectf(-0.5, 0.5);
                p.x *= inv;
                p.y *= inv;
                if let Some(c) = cells.get_mut(y * grid + x) {
                    *c = p;
                }
            }
        }

        let step = cells.len() as f32 / samples as f32;
        let mut cur = 0.0f32;
        for o in offsets.iter_mut() {
            *o = cells.get(cur as usize).copied().unwrap_or_default();
            cur += step;
        }

        for o in offsets.iter_mut() {
            let p = *o;
            o.x = p.y.sqrt() * cos(TWO_PI * p.x);
            o.y = p.y.sqrt() * sin(TWO_PI * p.x);
        }

        if sort {
            stdsort::sort(&mut offsets, |a, b| length(a) > length(b));
        }

        for o in offsets.iter_mut() {
            o.x = (o.x + 1.0) * 0.5;
            o.y = (o.y + 1.0) * 0.5;
        }

        for d in 0..depth {
            let at = d * sets * 4 + set * 4;
            let (a, b) = (
                offsets.get(d * 2).copied().unwrap_or_default(),
                offsets.get(d * 2 + 1).copied().unwrap_or_default(),
            );
            if let Some(px) = out.get_mut(at..at + 4) {
                px.copy_from_slice(&[
                    to_byte(a.x * 255.0),
                    to_byte(a.y * 255.0),
                    to_byte(b.x * 255.0),
                    to_byte(b.y * 255.0),
                ]);
            }
        }
    }
    true
}
