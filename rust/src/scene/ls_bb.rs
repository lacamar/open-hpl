//! LS: `cBillboard`, `cBeam` and `cBeamEnd` shells. The pods are the `sl.rs` ones; the engine
//! calls around them (vertex buffer, materials, occlusion queries, XML) go through `ls_host`.

use super::billboard;
use super::h10::EntPub;
use super::h12b_ent;
use super::ls_host::{self as lh, Ent, Vb, Xml, XmlError};
use super::sl::{self, BbPod, BeamEndPod, BeamObj, BeamPod};
use crate::math::bv::{Bv, Frustum};
use crate::math::geom::{FrustumView, normalized_clip_rect_from_bv};
use crate::math::{matrix_mul, matrix_scale, normalize3};
use crate::types::{Color, Matrixf, Vector2f, Vector3f};
use std::ffi::c_void;

const POINT_AXIS: i32 = 1;
const FIXED_AXIS: i32 = 2;
const HALO_SIZE: u32 = 1;
const HALO_TRANSFORM: u32 = 2;
const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

#[repr(C)]
struct BbBox {
    pod: BbPod,
    ent: Ent,
}

fn bb<'a>(p: *mut BbPod) -> &'a mut BbBox {
    // SAFETY: every `BbPod` handed to C++ is the first field of a `BbBox`.
    unsafe { &mut *p.cast() }
}

impl BbBox {
    fn vb(&self) -> Vb {
        Vb(self.pod.vtx as *mut c_void)
    }

    fn update_colors(&self) {
        let vb = self.vb();
        // SAFETY: the colour array holds four vertices of four floats.
        let out = unsafe { vb.floats(lh::E_COLOR0, 16) };
        if let Ok(out) = <&mut [f32; 16]>::try_from(out) {
            billboard::vertex_colors(
                self.pod.color,
                self.pod.brightness,
                self.pod.halo_alpha,
                lh::hdr(),
                out,
            );
        }
        vb.update(lh::F_COLOR0);
    }

    fn set_halo_alpha(&mut self, a: f32) {
        // SAFETY: `pod` is valid.
        if unsafe { sl::ohpl_sl_bb_set_halo_alpha(&mut self.pod, a) } {
            self.update_colors();
        }
    }

    fn set_axis(&mut self, axis: Vector3f) {
        let mut axis = axis;
        normalize3(&mut axis);
        let mut size = 0.0;
        // SAFETY: `pod`, `axis`, `size` valid.
        if unsafe { sl::ohpl_sl_bb_set_axis(&mut self.pod, &axis, &mut size) } {
            self.ent.bv().set_size(Vector3f {
                x: size,
                y: size,
                z: size,
            });
            self.ent.updated();
        }
    }
}

/// # Safety
/// `ctx` is the shell, `e` its entity pod, `bv` its bounding volume, `res`/`gfx` the engine services.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_new(
    ctx: *mut c_void,
    e: *mut EntPub,
    bv: *mut Bv,
    res: usize,
    gfx: usize,
    kind: i32,
    size: *const Vector2f,
) -> *mut BbPod {
    // SAFETY: `ohpl_sl_bb_new` returns a live pod that is copied out and released.
    let mut pod = unsafe {
        let p = sl::ohpl_sl_bb_new(kind, size);
        let pod = *p;
        sl::ohpl_sl_bb_free(p);
        pod
    };
    pod.mat_mgr = res;
    let (hx, hy) = (pod.size.x / 2.0, pod.size.y / 2.0);

    let vb = Vb::create(gfx as *mut c_void, lh::USAGE_DYNAMIC, 4, 6);
    pod.vtx = vb.0 as usize;
    vb.elem(lh::E_POS, 4);
    vb.elem(lh::E_NORMAL, 3);
    vb.elem(lh::E_COLOR0, 4);
    vb.elem(lh::E_TEX0, 3);

    let coords = [(hx, -hy), (-hx, -hy), (-hx, hy), (hx, hy)];
    let mut tex = [(1.0f32, -1.0f32), (-1.0, -1.0), (-1.0, 1.0), (1.0, 1.0)];
    if lh::hdr() {
        for t in &mut tex {
            t.1 = -t.1;
        }
    }
    for ((x, y), (u, v)) in coords.into_iter().zip(tex) {
        vb.add(lh::E_POS, &[x, y, 0.0]);
        vb.color(lh::E_COLOR0, WHITE);
        vb.add(lh::E_TEX0, &[(u + 1.0) / 2.0, (v + 1.0) / 2.0, 0.0]);
        vb.add(lh::E_NORMAL, &[0.0, 0.0, 1.0]);
    }
    for i in [0, 1, 2, 2, 3, 0] {
        vb.index(i);
    }
    if kind == FIXED_AXIS {
        for i in [2, 1, 0, 0, 3, 2] {
            vb.index(i);
        }
    }
    vb.compile(1);

    let ent = Ent { ctx, e, bv };
    ent.bv().set_size(Vector3f {
        x: pod.size.x,
        y: pod.size.y,
        z: pod.size.x,
    });
    Box::into_raw(Box::new(BbBox { pod, ent })).cast()
}

/// # Safety
/// `p` from `ohpl_ls_bb_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_free(p: *mut BbPod) {
    // SAFETY: caller contract.
    let b = unsafe { Box::from_raw(p.cast::<BbBox>()) };
    lh::mat_destroy(b.pod.mat_mgr, b.pod.material);
    b.vb().free();
    if b.pod.halo_bv != 0 {
        // SAFETY: the halo volume is a `Box<Bv>` made by `set_is_halo`.
        drop(unsafe { Box::from_raw(b.pod.halo_bv as *mut Bv) });
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_set_size(p: *mut BbPod, size: *const Vector2f) {
    let b = bb(p);
    // SAFETY: caller contract.
    b.pod.size = unsafe { *size };
    b.ent.bv().set_size(Vector3f {
        x: b.pod.size.x,
        y: b.pod.size.y,
        z: b.pod.size.x,
    });
    let vb = b.vb();
    // SAFETY: the position array holds four vertices of four floats.
    if let Ok(out) = <&mut [f32; 16]>::try_from(unsafe { vb.floats(lh::E_POS, 16) }) {
        billboard::vertex_positions(b.pod.size, out);
    }
    vb.update(lh::F_POS);
    if b.pod.kind == POINT_AXIS {
        b.set_axis(b.pod.axis);
    }
    b.ent.updated();
}

/// # Safety
/// `p` and `axis` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_set_axis(p: *mut BbPod, axis: *const Vector3f) {
    // SAFETY: caller contract.
    bb(p).set_axis(unsafe { *axis });
}

/// # Safety
/// `p` and `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_set_color(p: *mut BbPod, c: *const Color) {
    let b = bb(p);
    // SAFETY: caller contract.
    if unsafe { sl::ohpl_sl_bb_set_color(&mut b.pod, c) } {
        b.update_colors();
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_set_halo_alpha(p: *mut BbPod, a: f32) {
    bb(p).set_halo_alpha(a);
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_set_brightness(p: *mut BbPod, v: f32) {
    let b = bb(p);
    // SAFETY: caller contract.
    if unsafe { sl::ohpl_sl_bb_set_brightness(&mut b.pod, v) } {
        b.update_colors();
    }
}

/// # Safety
/// `p` valid; `frustum` null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_model(p: *mut BbPod, frustum: *const Frustum) -> *mut Matrixf {
    let b = bb(p);
    let world = b.ent.world();
    if frustum.is_null() || b.pod.kind == FIXED_AXIS {
        return world.cast_mut();
    }
    // SAFETY: caller contract; `world` is the live entity matrix.
    let (f, world) = unsafe { (&*frustum, &*world) };
    let view_up = Vector3f {
        x: f.view.m[1][0],
        y: f.view.m[1][1],
        z: f.view.m[1][2],
    };
    let mut warn = false;
    // SAFETY: `pod`, `world`, the two vectors and `warn` are valid.
    let m = unsafe { sl::ohpl_sl_bb_model(&mut b.pod, world, &f.origin, &view_up, &mut warn) };
    if warn {
        lh::log(
            false,
            "Billboard Right vector is not correct! Contact programmer!\n",
        );
    }
    m.cast_mut()
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_set_is_halo(p: *mut BbPod, halo: bool) {
    let b = bb(p);
    b.pod.is_halo = halo;
    if halo {
        b.pod.halo_alpha = 1.0;
        b.set_halo_alpha(0.0);
        if b.pod.halo_bv == 0 {
            b.pod.halo_bv = Box::into_raw(Box::new(Bv::new())) as usize;
        }
    } else if b.pod.halo_bv != 0 {
        // SAFETY: the halo volume is a `Box<Bv>` made above.
        drop(unsafe { Box::from_raw(b.pod.halo_bv as *mut Bv) });
        b.pod.halo_bv = 0;
    }
}

/// # Safety
/// `p` valid; `renderer` is the `iRenderer` that queries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_assign_occlusion(p: *mut BbPod, renderer: *mut c_void) {
    let b = bb(p);
    if !b.pod.is_halo {
        return;
    }
    let world = b.ent.world();
    // SAFETY: `world` is the live entity matrix.
    b.pod.halo_occlusion = matrix_mul(unsafe { &*world }, &matrix_scale(b.pod.halo_source_size));
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        unsafe {
            (h.occ_assign)(renderer, b.ent.ctx, 0, &b.pod.halo_occlusion, true);
            (h.occ_assign)(renderer, b.ent.ctx, 1, &b.pod.halo_occlusion, false);
        }
    }
}

/// # Safety
/// `p` valid; `renderer` is the `iRenderer` that queries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_bb_retrieve_occlusion(
    p: *mut BbPod,
    renderer: *mut c_void,
) -> bool {
    let b = bb(p);
    if !b.pod.is_halo {
        return true;
    }
    let Some(h) = lh::host() else { return true };
    // SAFETY: table contract.
    let (samples, max_samples) = unsafe {
        (
            (h.occ_samples)(renderer, b.ent.ctx, 0),
            (h.occ_samples)(renderer, b.ent.ctx, 1),
        )
    };
    if max_samples <= 0 {
        b.set_halo_alpha(0.0);
        return false;
    }
    // SAFETY: table contract; the halo volume exists while `is_halo`.
    let (frustum, halo) = unsafe {
        (
            &*(h.renderer_frustum)(renderer),
            &mut *(b.pod.halo_bv as *mut Bv),
        )
    };
    // SAFETY: `pod` is valid.
    let gates = unsafe { sl::ohpl_sl_bb_halo_gates(&mut b.pod, b.ent.count()) };
    if gates & HALO_SIZE != 0 {
        halo.set_size(b.pod.halo_source_size);
    }
    if gates & HALO_TRANSFORM != 0 {
        // SAFETY: the entity matrix is live.
        halo.set_transform(unsafe { &*b.ent.world() });
    }
    halo.update_size();
    let view = FrustumView {
        view: &frustum.view,
        proj: &frustum.proj,
        perspective: frustum.projection == crate::math::bv::PERSPECTIVE,
        fov: frustum.fov,
        near: frustum.near_plane,
        aspect: frustum.aspect,
    };
    let clip = normalized_clip_rect_from_bv(halo.world_min, halo.world_max, &view, 0.0);
    b.set_halo_alpha(h12b_ent::bb_halo_alpha(samples, max_samples, clip));
    true
}

#[repr(C)]
struct BeamBox {
    obj: BeamObj,
    ent: Ent,
    end_ent: usize,
    end: *mut BeamEndPod,
}

fn beam<'a>(p: *mut BeamPod) -> &'a mut BeamBox {
    // SAFETY: every `BeamPod` handed to C++ is the first field of a `BeamBox`.
    unsafe { &mut *p.cast() }
}

impl BeamBox {
    fn vb(&self) -> Vb {
        Vb(self.obj.pod.vtx as *mut c_void)
    }

    fn set_size(&mut self, size: Vector2f) {
        self.obj.pod.size = size;
        self.ent.bv().set_size(Vector3f {
            x: size.x,
            y: size.y,
            z: size.x,
        });
        self.ent.updated();
    }

    fn set_color(&mut self, c: &Color) {
        let vb = self.vb();
        // SAFETY: the colour array holds eight floats per edge.
        let colors = unsafe { vb.floats(lh::E_COLOR0, 16) };
        // SAFETY: `pod` and `c` valid; `colors` holds 16 floats.
        if unsafe { sl::ohpl_sl_beam_set_color(&mut self.obj.pod, c, colors.as_mut_ptr()) } {
            vb.update(lh::F_COLOR0);
        }
    }

    fn set_end_color(&mut self, c: &Color) {
        let vb = self.vb();
        // SAFETY: as `set_color`.
        let colors = unsafe { vb.floats(lh::E_COLOR0, 16) };
        // SAFETY: the end pod is the one registered by `ohpl_ls_beam_set_end`.
        if unsafe {
            sl::ohpl_sl_beam_set_end_color(&mut self.obj.pod, self.end, c, colors.as_mut_ptr())
        } {
            vb.update(lh::F_COLOR0);
        }
    }
}

/// # Safety
/// As `ohpl_ls_bb_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_new(
    ctx: *mut c_void,
    e: *mut EntPub,
    bv: *mut Bv,
    res: usize,
    gfx: usize,
) -> *mut BeamPod {
    // SAFETY: `ohpl_sl_beam_new` returns a boxed `BeamObj` that is moved out.
    let mut obj = unsafe { *Box::from_raw(sl::ohpl_sl_beam_new().cast::<BeamObj>()) };
    obj.pod.mat_mgr = res;
    obj.pod.file_searcher = res;
    let (hx, hy) = (obj.pod.size.x / 2.0, obj.pod.size.y / 2.0);

    let vb = Vb::create(gfx as *mut c_void, lh::USAGE_DYNAMIC, 4, 6);
    obj.pod.vtx = vb.0 as usize;
    vb.elem(lh::E_POS, 4);
    vb.elem(lh::E_NORMAL, 3);
    vb.elem(lh::E_COLOR0, 4);
    vb.elem(lh::E_TEX0, 3);
    let coords = [(hx, -hy), (-hx, -hy), (-hx, hy), (hx, hy)];
    let tex = [(1.0f32, 1.0f32), (-1.0, 1.0), (-1.0, -1.0), (1.0, -1.0)];
    for ((x, y), (u, v)) in coords.into_iter().zip(tex) {
        vb.add(lh::E_POS, &[x, y, 0.0]);
        vb.color(lh::E_COLOR0, WHITE);
        vb.add(lh::E_TEX0, &[(u + 1.0) / 2.0, (v + 1.0) / 2.0, 0.0]);
        vb.add(lh::E_NORMAL, &[0.0, 0.0, 1.0]);
    }
    for i in [0, 1, 2, 2, 3, 0] {
        vb.index(i);
    }
    vb.compile(1);

    let ent = Ent { ctx, e, bv };
    ent.bv().set_size(Vector3f {
        x: obj.pod.size.x,
        y: obj.pod.size.y,
        z: obj.pod.size.x,
    });
    Box::into_raw(Box::new(BeamBox {
        obj,
        ent,
        end_ent: 0,
        end: std::ptr::null_mut(),
    }))
    .cast()
}

/// Registers the end entity (`end_ent`, its pod `end`) the shell created.
///
/// # Safety
/// `p` valid; `end` is the `OhplSLBeamEnd` of the end entity `end_ent`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_set_end(
    p: *mut BeamPod,
    end_ent: usize,
    end: *mut BeamEndPod,
) {
    let b = beam(p);
    b.end_ent = end_ent;
    b.end = end;
}

/// Frees the pod and the vertex buffer; the C++ shell deletes the end entity first.
///
/// # Safety
/// `p` from `ohpl_ls_beam_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_free(p: *mut BeamPod) {
    // SAFETY: caller contract.
    let b = unsafe { Box::from_raw(p.cast::<BeamBox>()) };
    lh::mat_destroy(b.obj.pod.mat_mgr, b.obj.pod.material);
    b.vb().free();
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_set_size(p: *mut BeamPod, size: *const Vector2f) {
    // SAFETY: caller contract.
    beam(p).set_size(unsafe { *size });
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_set_tile_height(p: *mut BeamPod, tile: bool) {
    let b = beam(p);
    if b.obj.pod.tile_height == tile {
        return;
    }
    b.obj.pod.tile_height = tile;
    b.ent.updated();
}

/// # Safety
/// `p` and `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_set_color(p: *mut BeamPod, c: *const Color) {
    // SAFETY: caller contract.
    beam(p).set_color(unsafe { &*c });
}

/// # Safety
/// `p` and `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_set_end_color(p: *mut BeamPod, c: *const Color) {
    // SAFETY: caller contract.
    beam(p).set_end_color(unsafe { &*c });
}

/// `GetBoundingVolume`: refreshes the local box from the two end positions when stale.
///
/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_bounds(p: *mut BeamPod) {
    let b = beam(p);
    // SAFETY: the entity pod outlives the shell.
    let e = unsafe { &mut *b.ent.e };
    if !e.update_bv {
        return;
    }
    let (start, end) = (b.ent.pos(), lh::pos_of(b.end_ent as *mut c_void));
    let (mut min, mut max) = (Vector3f::default(), Vector3f::default());
    // SAFETY: all pointers valid.
    unsafe { sl::ohpl_sl_beam_bounds(&b.obj.pod, &start, &end, &mut min, &mut max) };
    b.ent.bv().set_local_min_max(min, max);
    e.update_bv = false;
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_update_frame(p: *mut BeamPod) {
    let b = beam(p);
    let count = b.ent.count();
    let pod = &b.obj.pod;
    if !h12b_ent::beam_needs_geometry(pod.start_transform_count, pod.end_transform_count, count) {
        return;
    }
    let (start, end) = (b.ent.pos(), lh::pos_of(b.end_ent as *mut c_void));
    let vb = b.vb();
    let (pos, tex) = (vb.array(lh::E_POS), vb.array(lh::E_TEX0));
    // SAFETY: all pointers valid; the arrays hold four vertices.
    unsafe { sl::ohpl_sl_beam_geometry(&mut b.obj.pod, count, &start, &end, pos, tex) };
    vb.update(lh::F_POS | lh::F_TEX0);
}

/// # Safety
/// `p` valid; `frustum` null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_model(
    p: *mut BeamPod,
    frustum: *const Frustum,
) -> *mut Matrixf {
    let b = beam(p);
    if frustum.is_null() {
        return b.ent.world().cast_mut();
    }
    let pos = b.ent.pos();
    // SAFETY: caller contract; the entity matrix is live.
    unsafe {
        sl::ohpl_sl_beam_model(&mut b.obj.pod, b.ent.world(), &pos, &(*frustum).origin).cast_mut()
    }
}

/// `LoadXMLProperties`: file name, size, colours and material from the `.beam` file.
///
/// # Safety
/// `p` valid; `file` addresses `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_beam_load(p: *mut BeamPod, file: *const u8, len: usize) -> bool {
    let b = beam(p);
    // SAFETY: caller contract.
    let file = unsafe { std::slice::from_raw_parts(file, len) };
    // SAFETY: `pod` valid; `file` outlives the call.
    unsafe { sl::ohpl_sl_beam_set_file(&mut b.obj.pod, file.as_ptr(), file.len()) };
    let new_file = crate::string::set_file_ext(file, b"beam");
    let name = String::from_utf8_lossy(&new_file).into_owned();
    let res = b.obj.pod.mat_mgr;
    let xml = match Xml::open(res, &new_file) {
        Ok(x) => x,
        Err(XmlError::Unreadable) => return false,
        Err(XmlError::Load) => {
            lh::log(true, &format!("Couldn't load file '{name}'\n"));
            return true;
        }
        Err(XmlError::NoMain) => {
            lh::log(true, &format!("Cannot find main element in {name}\n"));
            return false;
        }
        Err(XmlError::NotFound) => {
            lh::log(true, &format!("Couldn't find file '{name}'\n"));
            return false;
        }
    };
    let material = xml.string("Material");
    let [sx, sy] = xml.floats("Size", [1.0, 1.0]);
    let tile = xml.boolean("TileHeight", true);
    let multiply = xml.boolean("MultiplyAlphaWithColor", false);
    let [r, g, bl, a] = xml.floats("StartColor", [1.0; 4]);
    let start = Color { r, g, b: bl, a };
    let [r, g, bl, a] = xml.floats("EndColor", [1.0; 4]);
    let end = Color { r, g, b: bl, a };
    drop(xml);

    b.set_size(Vector2f { x: sx, y: sy });
    // SAFETY: `p` valid.
    unsafe { ohpl_ls_beam_set_tile_height(p, tile) };
    b.obj.pod.multiply_alpha = multiply;
    b.set_color(&start);
    b.set_end_color(&end);
    let mat = lh::mat_create(res, &material);
    if mat == 0 {
        lh::log(
            true,
            &format!(
                "Couldn't load material '{}' in Beam file '{name}'",
                String::from_utf8_lossy(&material)
            ),
        );
        return false;
    }
    b.obj.pod.material = mat;
    true
}
