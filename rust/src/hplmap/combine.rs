//! Static object combining of the `.map` loader: copies each object's vertex arrays into the
//! merged buffer, moved by the object's world matrix, with its indices offset by the vertices before.

use crate::gl::vbuf;
use crate::types::Matrixf;

const STRIDE: [usize; 5] = [4, 3, 4, 3, 4];

#[repr(C)]
pub struct Src {
    vtx_num: i32,
    idx_num: i32,
    matrix: Matrixf,
    arrays: [*const f32; 5],
    indices: *const u32,
}

/// Array pointers of the merged buffer: position, normal, colour, texture 0, tangent. Only
/// position is used when `all` is false.
#[repr(C)]
pub struct Dst {
    arrays: [*mut f32; 5],
    indices: *mut u32,
    vtx_total: usize,
    idx_total: usize,
}

/// Merges `n` sources into `dst`; false when the sources overflow it.
///
/// # Safety
/// Each source array holds `vtx_num` vertices and `indices` `idx_num` entries; the destination
/// arrays hold `vtx_total` vertices and `indices` `idx_total` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_map_combine(
    srcs: *const Src,
    n: usize,
    all: bool,
    dst: *const Dst,
) -> bool {
    // SAFETY: caller contract.
    let (srcs, dst) = unsafe { (std::slice::from_raw_parts(srcs, n), &*dst) };
    let kinds = if all { 5 } else { 1 };
    // SAFETY: the destination arrays hold `vtx_total` vertices of their stride.
    let mut arrays: Vec<&mut [f32]> = dst
        .arrays
        .iter()
        .zip(STRIDE)
        .take(kinds)
        .map(|(&p, w)| unsafe { std::slice::from_raw_parts_mut(p, dst.vtx_total * w) })
        .collect();
    // SAFETY: as above.
    let indices = unsafe { std::slice::from_raw_parts_mut(dst.indices, dst.idx_total) };
    let (mut voff, mut ioff) = (0usize, 0usize);
    for s in srcs {
        let (Ok(vn), Ok(inn)) = (usize::try_from(s.vtx_num), usize::try_from(s.idx_num)) else {
            return false;
        };
        let mut parts = Vec::with_capacity(kinds);
        for ((a, w), &sp) in arrays.iter_mut().zip(STRIDE).zip(&s.arrays) {
            let Some(d) = a.get_mut(voff * w..(voff + vn) * w) else {
                return false;
            };
            // SAFETY: caller contract.
            d.copy_from_slice(unsafe { std::slice::from_raw_parts(sp, vn * w) });
            parts.push(d);
        }
        let mut parts = parts.into_iter();
        let Some(pos) = parts.next() else {
            return false;
        };
        let (normal, tangent) = (parts.next(), parts.nth(2));
        vbuf::transform(&s.matrix, pos, normal, tangent, s.vtx_num, 4, false);
        let Some(d) = indices.get_mut(ioff..ioff + inn) else {
            return false;
        };
        // SAFETY: caller contract.
        let src = unsafe { std::slice::from_raw_parts(s.indices, inn) };
        let base = voff as u32;
        for (d, &i) in d.iter_mut().zip(src) {
            *d = i.wrapping_add(base);
        }
        voff += vn;
        ioff += inn;
    }
    voff == dst.vtx_total && ioff == dst.idx_total
}

#[cfg(test)]
mod tests {
    use super::*;

    const POS: usize = 0;
    const NORMAL: usize = 1;
    const COLOR: usize = 2;
    const TEX0: usize = 3;
    const TANGENT: usize = 4;

    #[test]
    fn merges_and_offsets() {
        let mut id = Matrixf::default();
        for (i, row) in id.m.iter_mut().enumerate() {
            row[i] = 1.0;
        }
        let mut mv = id;
        mv.m[0][3] = 5.0;
        let pos = [[0.0f32, 0.0, 0.0, 1.0]; 2].concat();
        let z = [0.0f32; 8];
        let idx = [0u32, 1, 0];
        let src = |m| Src {
            vtx_num: 2,
            idx_num: 3,
            matrix: m,
            arrays: [pos.as_ptr(), z.as_ptr(), z.as_ptr(), z.as_ptr(), z.as_ptr()],
            indices: idx.as_ptr(),
        };
        let srcs = [src(id), src(mv)];
        let mut d: Vec<Vec<f32>> = STRIDE.iter().map(|s| vec![9.0; 4 * s]).collect();
        let mut di = vec![0u32; 6];
        let dst = Dst {
            arrays: [0, 1, 2, 3, 4].map(|k| d[k].as_mut_ptr()),
            indices: di.as_mut_ptr(),
            vtx_total: 4,
            idx_total: 6,
        };
        assert!(unsafe { ohpl_map_combine(srcs.as_ptr(), 2, true, &dst) });
        assert_eq!(d[POS][12], 5.0);
        assert_eq!(d[POS][0], 0.0);
        assert_eq!(d[COLOR][0], 0.0);
        assert_eq!(d[TEX0].len(), 12);
        assert_eq!(d[NORMAL].len(), 12);
        assert_eq!(d[TANGENT].len(), 16);
        assert_eq!(di, [0, 1, 0, 2, 3, 2]);
    }
}
