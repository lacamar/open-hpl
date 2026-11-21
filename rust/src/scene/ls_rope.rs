//! LS: `cRopeEntity` state and vertex strip. The C++ class is a shell around one `RopePod`
//! (`RustLs.h` mirrors the layout); the vertex buffer, physics rope, frustum and bounding volume
//! are reached through `ls_host` and the Rust `Bv`/`Frustum` mirrors.

use super::h12b_ent;
use super::ls_host::{self as lh, RopeInfo, Vb};
use super::rope::{self, RopeOut, RopeParams};
use crate::math::bv::{Bv, Frustum};
use crate::types::{Color, Vector3f};
use std::ffi::c_void;

const _: () = assert!(size_of::<RopePod>() == 72);

#[repr(C)]
pub struct RopePod {
    res: usize,
    rope: usize,
    material: usize,
    vtx: usize,
    color: Color,
    radius: f32,
    tile_amount: f32,
    tile_size: f32,
    max_segments: i32,
    last_update_count: i32,
    multiply_alpha: bool,
}

#[repr(C)]
struct RopeBox {
    pod: RopePod,
    points: Vec<Vector3f>,
}

impl RopePod {
    fn vb(&self) -> Vb {
        Vb(self.vtx as *mut c_void)
    }

    fn info(&self) -> RopeInfo {
        let mut info = RopeInfo::default();
        if let Some(h) = lh::host() {
            // SAFETY: table contract.
            unsafe { (h.rope_info)(self.rope as *mut c_void, &mut info) }
        }
        info
    }

    fn collect(&self, points: &mut Vec<Vector3f>, take: i32, smooth: bool) {
        points.clear();
        let n = usize::try_from(take).unwrap_or(0);
        points.resize(n, Vector3f::default());
        let got = lh::host().map_or(0, |h| {
            // SAFETY: `points` holds `take` vectors.
            unsafe { (h.rope_points)(self.rope as *mut c_void, points.as_mut_ptr(), take, smooth) }
        });
        points.truncate(usize::try_from(got).unwrap_or(0));
    }
}

/// # Safety
/// `res`, `gfx` and `rope` are the engine objects the C++ shell was given.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_rope_new(
    res: usize,
    gfx: usize,
    rope: usize,
    max_segments: i32,
) -> *mut RopePod {
    let mut pod = RopePod {
        res,
        rope,
        material: 0,
        vtx: 0,
        color: Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        },
        radius: 0.0,
        tile_amount: 1.0,
        tile_size: 1.0,
        max_segments,
        last_update_count: -1,
        multiply_alpha: false,
    };
    pod.radius = pod.info().radius;

    let vb = Vb::create(
        gfx as *mut c_void,
        lh::USAGE_DYNAMIC,
        4 * max_segments,
        6 * max_segments,
    );
    pod.vtx = vb.0 as usize;
    vb.elem(lh::E_POS, 4);
    vb.elem(lh::E_NORMAL, 3);
    vb.elem(lh::E_COLOR0, 4);
    vb.elem(lh::E_TEX0, 3);
    for i in 0..max_segments {
        for _ in 0..4 {
            vb.add(lh::E_POS, &[0.0; 3]);
            vb.color(lh::E_COLOR0, pod.color);
            vb.add(lh::E_TEX0, &[0.0; 3]);
            vb.add(lh::E_NORMAL, &[0.0, 0.0, 1.0]);
        }
        for j in (0..3).chain(2..5).map(|j| if j == 4 { 0 } else { j }) {
            vb.index(j + i * 4);
        }
    }
    vb.compile(1);

    Box::into_raw(Box::new(RopeBox {
        pod,
        points: Vec::new(),
    }))
    .cast()
}

/// # Safety
/// `p` from `ohpl_ls_rope_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_rope_free(p: *mut RopePod) {
    if p.is_null() {
        return;
    }
    // SAFETY: caller contract; `RopePod` is the first field of `RopeBox`.
    let b = unsafe { Box::from_raw(p.cast::<RopeBox>()) };
    lh::mat_destroy(b.pod.res, b.pod.material);
    b.pod.vb().free();
}

/// # Safety
/// `p` and `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_rope_set_color(p: *mut RopePod, c: *const Color) {
    // SAFETY: caller contract.
    let (pod, c) = unsafe { (&mut *p, *c) };
    if pod.color == c {
        return;
    }
    pod.color = c;
    let vb = pod.vb();
    let n = usize::try_from(pod.max_segments).unwrap_or(0) * 16;
    // SAFETY: the colour array holds four floats per vertex.
    rope::set_colors(c, pod.multiply_alpha, unsafe { vb.floats(lh::E_COLOR0, n) });
    vb.update(lh::F_COLOR0);
}

/// `GetBoundingVolume`: refreshes the local box when the physics rope moved.
///
/// # Safety
/// `p` valid; `bv` is the entity's bounding volume.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_rope_bounds(p: *mut RopePod, bv: *mut Bv) {
    // SAFETY: caller contract.
    let b = unsafe { &mut *p.cast::<RopeBox>() };
    let info = b.pod.info();
    if b.pod.last_update_count == info.update_count {
        return;
    }
    let mut points = std::mem::take(&mut b.points);
    b.pod.collect(&mut points, info.particles, false);
    let (min, max) = rope::bounds(&points, b.pod.radius);
    // SAFETY: caller contract.
    unsafe { (*bv).set_local_min_max(min, max) };
    b.points = points;
    b.pod.last_update_count = info.update_count;
}

/// `UpdateGraphicsForViewport`: lays one camera-facing quad per segment.
///
/// # Safety
/// `p` valid; `frustum` is the viewport frustum.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_rope_viewport(p: *mut RopePod, frustum: *const Frustum) -> bool {
    // SAFETY: caller contract.
    let (b, view) = unsafe { (&mut *p.cast::<RopeBox>(), &(*frustum).view) };
    let vb = b.pod.vb();
    let info = b.pod.info();
    let take = h12b_ent::rope_points(info.particles, b.pod.max_segments);
    let mut points = std::mem::take(&mut b.points);
    b.pod.collect(&mut points, take, true);

    let params = RopeParams {
        forward: Vector3f {
            x: view.m[2][0],
            y: view.m[2][1],
            z: view.m[2][2],
        },
        radius: b.pod.radius,
        segment_length: info.segment_length,
        tile_amount: b.pod.tile_amount,
        tile_size: b.pod.tile_size,
    };
    let quads = usize::try_from(b.pod.max_segments).unwrap_or(0);
    // SAFETY: the arrays hold 16/12/12/16 floats per quad; they are distinct.
    unsafe {
        rope::quads(
            &points,
            &params,
            RopeOut {
                pos: vb.floats(lh::E_POS, quads * 16),
                uv: vb.floats(lh::E_TEX0, quads * 12),
                normal: vb.floats(lh::E_NORMAL, quads * 12),
                tangent: vb.floats(lh::E_TANGENT, quads * 16),
            },
        );
    }
    vb.set_count(h12b_ent::rope_elements(
        i32::try_from(points.len()).unwrap_or(0),
    ));
    vb.update(lh::F_POS | lh::F_TEX0 | lh::F_TEX1 | lh::F_NORMAL);
    b.points = points;
    true
}
