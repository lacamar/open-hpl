//! `cFrameBitmap` state and algorithms: border blits, atlas flags, `Reorganize`.

use super::bitmap::{self, Surface};
use super::formats;
use super::packer::{self, RectPacker};
use crate::math::Rect2l;
use crate::types::Vector3l;

pub struct Pix<'a> {
    pub surface: Surface,
    /// `None` when the bitmap has no data (blits and clears are skipped).
    pub data: Option<&'a mut [u8]>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Added {
    /// Placed rect without the 1-pixel border.
    pub rect: Rect2l,
    pub found_empty: bool,
}

pub struct Reorg {
    pub order: Vec<usize>,
    pub rects: Vec<Rect2l>,
    pub fit: Vec<bool>,
}

#[derive(Debug)]
pub struct Atlas {
    packer: RectPacker,
    pub pic_count: i32,
    pub additions: i32,
    pub full: bool,
    pub updated: bool,
    pub need_reorg: bool,
    pub locked: bool,
    pub handle: i32,
}

fn v3(x: i32, y: i32, z: i32) -> Vector3l {
    Vector3l { x, y, z }
}

fn blit(
    dst: &mut Pix,
    src: &Surface,
    src_data: Option<&[u8]>,
    to: Vector3l,
    size: Vector3l,
    from: Vector3l,
) {
    if let (Some(d), Some(src_data)) = (dst.data.as_deref_mut(), src_data) {
        bitmap::blit(&dst.surface, d, src, src_data, to, size, from);
    }
}

fn clear(p: &mut Pix) {
    let Some(d) = p.data.as_deref_mut() else {
        return;
    };
    let s = &p.surface.size;
    let count = i64::from(s.x) * i64::from(s.y) * i64::from(s.z);
    bitmap::clear(
        d,
        count,
        usize::try_from(p.surface.bytes_per_pixel).unwrap_or(0),
        p.surface.format,
        [255, 0, 255, 0],
    );
}

impl Atlas {
    pub fn new(w: i32, h: i32, handle: i32) -> Self {
        Atlas {
            packer: RectPacker::new(w, h),
            pic_count: 0,
            additions: 0,
            full: false,
            updated: false,
            need_reorg: false,
            locked: false,
            handle,
        }
    }

    pub fn clear_images(&mut self, dst: &mut Pix) {
        clear(dst);
        self.pic_count = 0;
        self.packer.clear(dst.surface.size.x, dst.surface.size.y);
    }

    /// `cFrameBitmap::AddBitmap`; `linked` is a subimage that already exists (reorganisation).
    pub fn add(
        &mut self,
        dst: &mut Pix,
        src: &Surface,
        src_data: Option<&[u8]>,
        linked: bool,
    ) -> Option<Added> {
        let (w, h) = (src.size.x, src.size.y);
        let ins = self.packer.insert(w, h);
        let r = ins.rect?;
        let (x, y) = (r.x, r.y);
        let (x1, y1) = (
            x.wrapping_add(w).wrapping_add(1),
            y.wrapping_add(h).wrapping_add(1),
        );
        let (lx, ly) = (w.wrapping_sub(1), h.wrapping_sub(1));
        let one = v3(1, 1, 1);
        let z = v3(0, 0, 0);
        let mut b =
            |to: Vector3l, size: Vector3l, from: Vector3l| blit(dst, src, src_data, to, size, from);
        b(v3(x, y, 0), one, z);
        b(v3(x1, y, 0), one, v3(lx, 0, 0));
        b(v3(x1, y1, 0), one, v3(lx, ly, 0));
        b(v3(x, y1, 0), one, v3(0, ly, 0));
        b(v3(x.wrapping_add(1), y, 0), v3(w, 1, 1), z);
        b(v3(x.wrapping_add(1), y1, 0), v3(w, 1, 1), v3(0, ly, 0));
        b(v3(x, y.wrapping_add(1), 0), v3(1, h, 1), z);
        b(v3(x1, y.wrapping_add(1), 0), v3(1, h, 1), v3(lx, 0, 0));
        b(v3(x.wrapping_add(1), y.wrapping_add(1), 0), src.size, z);
        self.additions = self.additions.wrapping_add(1);
        self.pic_count = self.pic_count.wrapping_add(1);
        if !linked {
            if !ins.found_empty {
                self.full = true;
            }
            self.updated = true;
        }
        Some(Added {
            rect: Rect2l {
                x: x.wrapping_add(1),
                y: y.wrapping_add(1),
                w,
                h,
            },
            found_empty: ins.found_empty,
        })
    }

    /// `cFrameBitmap::Reorganize` over the live images' inner rects.
    pub fn reorganize(&mut self, dst: &mut Pix, rects: &[Rect2l]) -> Reorg {
        let rgba = formats::RGBA;
        let bpp = formats::channels(rgba);
        let temps: Vec<(Surface, Vec<u8>)> = rects
            .iter()
            .map(|r| {
                let size = v3(r.w.max(1), r.h.max(1), 1);
                let bytes = i64::from(size.x) * i64::from(size.y) * i64::from(bpp);
                let mut buf = vec![0u8; usize::try_from(bytes).unwrap_or(0)];
                let mut t = Pix {
                    surface: Surface {
                        size,
                        format: rgba,
                        bytes_per_pixel: bpp,
                    },
                    data: Some(&mut buf),
                };
                if let Some(s) = dst.data.as_deref() {
                    blit(
                        &mut t,
                        &dst.surface,
                        Some(s),
                        v3(0, 0, 0),
                        size,
                        v3(r.x, r.y, 0),
                    );
                }
                (t.surface, buf)
            })
            .collect();

        self.clear_images(dst);

        let n = temps.len();
        let mut out = Reorg {
            order: Vec::new(),
            rects: vec![Rect2l::default(); n],
            fit: vec![false; n],
        };
        let sizes: Vec<(i32, i32)> = temps.iter().map(|(s, _)| (s.size.x, s.size.y)).collect();
        let mut all_fit = true;
        for i in packer::reorganize_order(&sizes) {
            let Some((s, d)) = temps.get(i) else { continue };
            let Some(a) = self.add(dst, s, Some(d), true) else {
                all_fit = false;
                break;
            };
            if let (Some(r), Some(f)) = (out.rects.get_mut(i), out.fit.get_mut(i)) {
                *r = a.rect;
                *f = true;
            }
            out.order.push(i);
        }
        if !all_fit {
            self.clear_images(dst);
            out.order.clear();
            for (i, (s, d)) in temps.iter().enumerate() {
                let a = self.add(dst, s, Some(d), true);
                if let (Some(r), Some(f)) = (out.rects.get_mut(i), out.fit.get_mut(i)) {
                    *f = a.is_some();
                    *r = a.map(|a| a.rect).unwrap_or_default();
                }
                if a.is_some() {
                    out.order.push(i);
                }
            }
        }
        self.additions = 0;
        self.updated = true;
        out
    }
}
