//! `LowLevelGraphicsImmediate.inl`: the immediate-mode draw helpers and the CPU vertex batch shared
//! by the GL and Vulkan backends, written against the `Imm` primitives.

use super::h5b_gfx::{Col, Imm, Rect, V2f, V3f, Vtx, prim};

const TWO_PI: f32 = std::f32::consts::TAU;
/// Floats per batch vertex (`mlBatchStride`).
pub const BATCH_STRIDE: usize = 13;
pub const MAX_TEXTURE_UNITS: usize = 16;

fn col<I: Imm + ?Sized>(g: &mut I, c: Col) {
    g.im_color4(c[0], c[1], c[2], c[3]);
}

fn vtx3<I: Imm + ?Sized>(g: &mut I, p: V3f) {
    g.im_vertex3(p[0], p[1], p[2]);
}

pub fn draw_triangle<I: Imm + ?Sized>(g: &mut I, v: &[Vtx]) {
    g.im_begin(prim::TRIANGLES);
    for p in v.iter().take(3) {
        g.im_texcoord3(p.tex[0], p.tex[1], p.tex[2]);
        col(g, p.col);
        vtx3(g, p.pos);
    }
    g.im_end();
}

pub fn draw_quad_pos<I: Imm + ?Sized>(g: &mut I, p: V3f, s: V2f, c: Col) {
    draw_quad_uv(g, p, s, [0.0, 0.0], [1.0, 1.0], c);
}

pub fn draw_quad_uv<I: Imm + ?Sized>(g: &mut I, p: V3f, s: V2f, min: V2f, max: V2f, c: Col) {
    g.im_begin(prim::QUADS);
    let corners = [
        ([min[0], min[1]], [p[0], p[1]]),
        ([max[0], min[1]], [p[0] + s[0], p[1]]),
        ([max[0], max[1]], [p[0] + s[0], p[1] + s[1]]),
        ([min[0], max[1]], [p[0], p[1] + s[1]]),
    ];
    for (uv, xy) in corners {
        g.im_texcoord2(uv[0], uv[1]);
        col(g, c);
        g.im_vertex3(xy[0], xy[1], p[2]);
    }
    g.im_end();
}

/// `uv` = min0, max0, min1, max1.
pub fn draw_quad_uv2<I: Imm + ?Sized>(g: &mut I, p: V3f, s: V2f, uv: [V2f; 4], c: Col) {
    let [min0, max0, min1, max1] = uv;
    g.im_begin(prim::QUADS);
    let corners = [
        ([min0[0], min0[1]], [min1[0], min1[1]], [p[0], p[1]]),
        ([max0[0], min0[1]], [max1[0], min1[1]], [p[0] + s[0], p[1]]),
        (
            [max0[0], max0[1]],
            [max1[0], max1[1]],
            [p[0] + s[0], p[1] + s[1]],
        ),
        ([min0[0], max0[1]], [min1[0], max1[1]], [p[0], p[1] + s[1]]),
    ];
    for (a, b, xy) in corners {
        g.im_multitexcoord2(prim::TEXTURE0, a[0], a[1]);
        g.im_multitexcoord2(prim::TEXTURE0 + 1, b[0], b[1]);
        col(g, c);
        g.im_vertex3(xy[0], xy[1], p[2]);
    }
    g.im_end();
}

fn quad_loop<I: Imm + ?Sized>(g: &mut I, v: &[Vtx], z: Option<f32>, c: Option<Col>) {
    g.im_begin(prim::QUADS);
    for p in v.iter().take(4) {
        g.im_texcoord3(p.tex[0], p.tex[1], z.unwrap_or(p.tex[2]));
        col(g, c.unwrap_or(p.col));
        vtx3(g, p.pos);
    }
    g.im_end();
}

pub fn draw_quad_vtx<I: Imm + ?Sized>(g: &mut I, v: &[Vtx]) {
    quad_loop(g, v, None, None);
}

pub fn draw_quad_vtx_col<I: Imm + ?Sized>(g: &mut I, v: &[Vtx], c: Col) {
    quad_loop(g, v, None, Some(c));
}

pub fn draw_quad_vtx_z<I: Imm + ?Sized>(g: &mut I, v: &[Vtx], z: f32) {
    quad_loop(g, v, Some(z), None);
}

pub fn draw_quad_vtx_z_col<I: Imm + ?Sized>(g: &mut I, v: &[Vtx], z: f32, c: Col) {
    quad_loop(g, v, Some(z), Some(c));
}

pub fn draw_quad_multitex<I: Imm + ?Sized>(g: &mut I, v: &[Vtx], extra: &[V3f]) {
    let units = extra.len() >> 2;
    g.im_begin(prim::QUADS);
    for (i, p) in v.iter().take(4).enumerate() {
        g.im_multitexcoord3(prim::TEXTURE0, p.tex[0], p.tex[1], p.tex[2]);
        for unit in 0..units {
            let e = extra.get(unit * 4 + i).copied().unwrap_or_default();
            g.im_multitexcoord3(prim::TEXTURE0 + 1 + unit as u32, e[0], e[1], e[2]);
        }
        col(g, p.col);
        vtx3(g, p.pos);
    }
    g.im_end();
}

pub fn draw_line<I: Imm + ?Sized>(g: &mut I, b: V3f, e: V3f, c: Col) {
    col(g, c);
    g.im_begin(prim::LINES);
    vtx3(g, b);
    vtx3(g, e);
    g.im_end();
}

pub fn draw_line_cols<I: Imm + ?Sized>(g: &mut I, b: V3f, bc: Col, e: V3f, ec: Col) {
    g.im_begin(prim::LINES);
    col(g, bc);
    vtx3(g, b);
    col(g, ec);
    vtx3(g, e);
    g.im_end();
}

pub fn draw_box_min_max<I: Imm + ?Sized>(g: &mut I, mn: V3f, mx: V3f, c: Col) {
    col(g, c);
    g.im_begin(prim::LINES);
    let (x0, y0, z0, x1, y1, z1) = (mn[0], mn[1], mn[2], mx[0], mx[1], mx[2]);
    let lines: [[f32; 6]; 12] = [
        [x1, y1, z1, x0, y1, z1],
        [x1, y1, z1, x1, y0, z1],
        [x0, y1, z1, x0, y0, z1],
        [x0, y0, z1, x1, y0, z1],
        [x1, y1, z0, x0, y1, z0],
        [x1, y1, z0, x1, y0, z0],
        [x0, y1, z0, x0, y0, z0],
        [x0, y0, z0, x1, y0, z0],
        [x1, y1, z1, x1, y1, z0],
        [x0, y1, z1, x0, y1, z0],
        [x0, y0, z1, x0, y0, z0],
        [x1, y0, z1, x1, y0, z0],
    ];
    for l in lines {
        g.im_vertex3(l[0], l[1], l[2]);
        g.im_vertex3(l[3], l[4], l[5]);
    }
    g.im_end();
}

/// The three great circles; gcc contracts `pos + trig * radius` into one fused multiply-add.
fn circles<I: Imm + ?Sized>(g: &mut I, p: V3f, r: f32, cols: Option<[Col; 3]>) {
    let step = TWO_PI / 32.0f32;
    let at = |a: f32| (a.sin(), a.cos());
    for axis in 0..3 {
        if let Some(c) = cols.as_ref().and_then(|c| c.get(axis)) {
            col(g, *c);
        }
        let mut a = 0.0f32;
        while a < TWO_PI {
            for t in [a, a + step] {
                let (s, c) = at(t);
                match axis {
                    0 => g.im_vertex3(p[0], s.mul_add(r, p[1]), c.mul_add(r, p[2])),
                    1 => g.im_vertex3(c.mul_add(r, p[0]), p[1], s.mul_add(r, p[2])),
                    _ => g.im_vertex3(c.mul_add(r, p[0]), s.mul_add(r, p[1]), p[2]),
                }
            }
            a += step;
        }
    }
}

pub fn draw_sphere<I: Imm + ?Sized>(g: &mut I, p: V3f, r: f32, c: Col) {
    col(g, c);
    g.im_begin(prim::LINES);
    circles(g, p, r, None);
    g.im_end();
}

pub fn draw_sphere_axes<I: Imm + ?Sized>(g: &mut I, p: V3f, r: f32, cols: [Col; 3]) {
    g.im_clear_texture0();
    g.im_blend_off();
    g.im_begin(prim::LINES);
    circles(g, p, r, Some(cols));
    g.im_end();
}

fn line_strip<I: Imm + ?Sized>(g: &mut I, x: f32, y: f32, w: f32, h: f32, z: f32, c: Col) {
    col(g, c);
    g.im_begin(prim::LINE_STRIP);
    g.im_vertex3(x, y, z);
    g.im_vertex3(x + w, y, z);
    g.im_vertex3(x + w, y + h, z);
    g.im_vertex3(x, y + h, z);
    g.im_vertex3(x, y, z);
    g.im_end();
}

pub fn draw_line_quad_rect<I: Imm + ?Sized>(g: &mut I, r: Rect, z: f32, c: Col) {
    line_strip(g, r[0], r[1], r[2], r[3], z, c);
}

pub fn draw_line_quad_pos<I: Imm + ?Sized>(g: &mut I, p: V3f, s: V2f, c: Col) {
    line_strip(g, p[0], p[1], s[0], s[1], p[2], c);
}

/// The CPU arrays behind `AddVertexToBatch*`: 13-float vertices, indices, per-unit texcoords.
#[derive(Debug, Default)]
pub struct Batch {
    /// Backing store; a clear only resets `count`, so slots a vertex does not write keep stale
    /// values from earlier vertices, as the C++ arrays do.
    store: Vec<f32>,
    count: usize,
    pub indices: Vec<u32>,
    pub texcoords: [Vec<f32>; MAX_TEXTURE_UNITS],
}

impl Batch {
    pub fn new() -> Self {
        Self::default()
    }

    /// The vertex floats in use (`mlVertexCount`).
    pub fn vertices(&self) -> &[f32] {
        self.store.get(..self.count).unwrap_or_default()
    }

    /// Start of the vertex store, for the GL array pointers.
    pub fn vertex_ptr(&self) -> *const f32 {
        self.store.as_ptr()
    }

    /// Per-unit texcoord arrays as the GL source table wants them.
    pub fn texcoord_ptrs(&mut self) -> [*mut f32; MAX_TEXTURE_UNITS] {
        let mut p = [core::ptr::null_mut(); MAX_TEXTURE_UNITS];
        for (d, s) in p.iter_mut().zip(self.texcoords.iter_mut()) {
            *d = s.as_mut_ptr();
        }
        p
    }

    fn slot(&mut self) -> &mut [f32] {
        let at = self.count;
        self.count += BATCH_STRIDE;
        if self.store.len() < self.count {
            self.store.resize(self.count, 0.0);
        }
        self.store.get_mut(at..self.count).unwrap_or_default()
    }

    fn write(&mut self, vals: &[(usize, &[f32])]) {
        let s = self.slot();
        for &(at, v) in vals {
            if let Some(d) = s.get_mut(at..at + v.len()) {
                d.copy_from_slice(v);
            }
        }
    }

    pub fn add_vertex(&mut self, v: &Vtx) {
        self.write(&[(0, &v.pos), (3, &v.col), (7, &v.tex), (10, &v.norm)]);
    }

    pub fn add_vertex_translated(&mut self, v: &Vtx, t: V3f) {
        let pos = [v.pos[0] + t[0], v.pos[1] + t[1], v.pos[2] + t[2]];
        self.write(&[(0, &pos), (3, &v.col), (7, &v.tex), (10, &v.norm)]);
    }

    pub fn add_vertex_size2d(&mut self, v: &Vtx, t: V3f, c: Option<Col>, w: f32, h: f32) {
        let pos = [t[0] + w, t[1] + h, t[2]];
        let c = c.unwrap_or_default();
        self.write(&[(0, &pos), (3, &c), (7, &v.tex)]);
    }

    pub fn add_vertex_raw(&mut self, pos: V3f, c: Col, tex: V3f) {
        self.write(&[(0, &pos), (3, &c), (7, &tex)]);
    }

    pub fn add_index(&mut self, i: i32) {
        self.indices.push(i as u32);
    }

    pub fn add_texcoord(&mut self, unit: u32, c: V3f) {
        if let Some(a) = self.texcoords.get_mut(unit as usize) {
            a.extend_from_slice(&c);
        }
    }

    /// `ClearBatch`: only the vertex and index counts; the texcoord counts stay.
    pub fn clear(&mut self) {
        self.count = 0;
        self.indices.clear();
    }

    /// The flush's auto-clear: also resets every texcoord count.
    pub fn clear_all(&mut self) {
        self.clear();
        self.texcoords.iter_mut().for_each(Vec::clear);
    }
}
