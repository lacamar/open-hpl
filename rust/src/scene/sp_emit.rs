//! SP2: state and logic of `iParticleEmitter`. The C++ class is a shell around one `Pe`; the
//! vertex buffer, entity matrices, bounding volume, frustum and materials stay C++ objects reached
//! through the `Host` table. No reference into a `Pe` is held across a host call (the host may
//! re-enter the shell).

use super::h12_psload::PsDesc;
use super::h12b_psys::{GATE_DIRECTION, PeCtl, axis_gate, bv_gate, model_kind, update_gate};
use super::particles::{Emitter, Gfx, GfxIn};
use super::sp_sys::{Sys, hpl3_color};
use crate::math::{IDENTITY, matrix_inverse};
use crate::types::{Matrixf, Vector2f, Vector3f};
use std::ffi::c_void;
use std::slice;

const AXIS: i32 = 3;
const LOCAL: i32 = 1;

pub const VB_TEX: i32 = 0;
pub const VB_POS: i32 = 1;
pub const VB_COL: i32 = 2;
pub const VB_NORMAL: i32 = 3;
pub const VB_TANGENT: i32 = 4;

pub const OP_KILL: i32 = 3;

#[repr(C)]
pub struct FrustumInfo {
    pub inverts_cull: bool,
    pub origin: Vector3f,
    pub view: Matrixf,
}

impl FrustumInfo {
    fn blank() -> Self {
        Self {
            inverts_cull: false,
            origin: Vector3f::default(),
            view: IDENTITY,
        }
    }
}

/// C++ objects behind the emitter. `ctx` is the shell, `vb` a vertex buffer, `ps` the particle
/// system shell, `data`/`world` the opaque pointers given to `ohpl_spe_new`/`ohpl_spe_set_world`.
#[repr(C)]
pub struct Host {
    pub vb_create: unsafe extern "C" fn(gfx: *mut c_void, max: u32) -> *mut c_void,
    pub vb_index: unsafe extern "C" fn(vb: *mut c_void, i: i32),
    pub vb_add: unsafe extern "C" fn(vb: *mut c_void, kind: i32, x: f32, y: f32, z: f32, w: f32),
    pub vb_compile: unsafe extern "C" fn(vb: *mut c_void),
    pub vb_free: unsafe extern "C" fn(vb: *mut c_void),
    pub vb_array: unsafe extern "C" fn(vb: *mut c_void, kind: i32) -> *mut f32,
    pub vb_stride: unsafe extern "C" fn(vb: *mut c_void) -> i32,
    pub vb_set_count: unsafe extern "C" fn(vb: *mut c_void, n: i32),
    pub vb_update: unsafe extern "C" fn(vb: *mut c_void, with_tex: bool),
    pub active: unsafe extern "C" fn(ctx: *mut c_void) -> bool,
    pub same_frame: unsafe extern "C" fn(ctx: *mut c_void) -> bool,
    pub matrix_count: unsafe extern "C" fn(ctx: *mut c_void) -> i32,
    pub world_matrix: unsafe extern "C" fn(ctx: *mut c_void) -> *const Matrixf,
    pub local_matrix: unsafe extern "C" fn(ctx: *mut c_void) -> *const Matrixf,
    pub world_pos: unsafe extern "C" fn(ctx: *mut c_void, out: *mut Vector3f),
    pub transform_updated: unsafe extern "C" fn(ctx: *mut c_void),
    pub bv_set: unsafe extern "C" fn(ctx: *mut c_void, min: *const Vector3f, max: *const Vector3f),
    pub bv_transform: unsafe extern "C" fn(ctx: *mut c_void, m: *const Matrixf),
    pub apply: unsafe extern "C" fn(ctx: *mut c_void, op: i32, arg: bool),
    pub ps_world_matrix: unsafe extern "C" fn(ps: *mut c_void) -> *const Matrixf,
    pub ps_center: unsafe extern "C" fn(ps: *mut c_void, out: *mut Vector3f),
    pub frustum: unsafe extern "C" fn(f: *const c_void, out: *mut FrustumInfo),
    pub mat_at: unsafe extern "C" fn(mats: *mut c_void, i: i32) -> *mut c_void,
    pub mat_add_blend: unsafe extern "C" fn(mat: *mut c_void) -> bool,
    pub collide: unsafe extern "C" fn(
        data: *mut c_void,
        world: *mut c_void,
        start: *const Vector3f,
        end: *const Vector3f,
        normal: *mut Vector3f,
        point: *mut Vector3f,
    ) -> bool,
}

pub struct Pe {
    host: *const Host,
    ctx: *mut c_void,
    em: Emitter,
    ctl: PeCtl,
    vb: *mut c_void,
    pool: usize,
    materials: *mut c_void,
    data: *mut c_void,
    world: *mut c_void,
    ps: *mut c_void,
    sys: *const Sys,
    draw_type: i32,
    coord: i32,
    uses_direction: bool,
    collides: bool,
    warm_time: f32,
    warm_steps: f32,
    direction: Vector3f,
    right: Vector3f,
    forward: Vector3f,
    max_draw: Vector2f,
    tmp: Matrixf,
}

impl Pe {
    pub fn warm_time(&self) -> f32 {
        self.warm_time
    }

    pub fn warm_steps(&self) -> f32 {
        self.warm_steps
    }

    pub fn is_dead(&self) -> bool {
        self.em.is_dead()
    }

    pub fn is_dying(&self) -> bool {
        self.em.is_dying()
    }

    pub fn ctx(&self) -> *mut c_void {
        self.ctx
    }

    pub fn attach(&mut self, sys: *const Sys, ps: *mut c_void) {
        self.sys = sys;
        self.ps = ps;
    }
}

fn host<'a>(pe: *const Pe) -> &'a Host {
    // SAFETY: the host table outlives every emitter (a static in the C++ shell TU).
    unsafe { &*(*pe).host }
}

fn ctx(pe: *const Pe) -> *mut c_void {
    // SAFETY: `pe` live.
    unsafe { (*pe).ctx }
}

unsafe fn vb_fill(h: &Host, vb: *mut c_void, max: i32) {
    // SAFETY: host contract; the call order is the trace the C++ constructor produced.
    unsafe {
        for i in 0..max {
            let s = i * 4;
            for j in 0..3 {
                (h.vb_index)(vb, s + j);
            }
            for j in 2..5 {
                (h.vb_index)(vb, s + if j == 4 { 0 } else { j });
            }
        }
        for _ in 0..max {
            for (x, y) in [(1.0, 1.0), (0.0, 1.0), (0.0, 0.0), (1.0, 0.0)] {
                (h.vb_add)(vb, VB_TEX, x, y, 0.0, 0.0);
            }
        }
        for _ in 0..max * 4 {
            (h.vb_add)(vb, VB_POS, 0.0, 0.0, 0.0, 0.0);
            (h.vb_add)(vb, VB_COL, 1.0, 1.0, 1.0, 1.0);
            (h.vb_add)(vb, VB_NORMAL, 0.0, 0.0, 1.0, 0.0);
            (h.vb_add)(vb, VB_TANGENT, 1.0, 0.0, 0.0, 1.0);
        }
        (h.vb_compile)(vb);
    }
}

/// # Safety
/// `host` outlives the emitter, `desc` valid for the call; the pointers are kept opaque.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_new(
    host: *const Host,
    ctx: *mut c_void,
    gfx: *mut c_void,
    materials: *mut c_void,
    data: *mut c_void,
    desc: *const PsDesc,
    material_count: u32,
    warm_time: f32,
    warm_steps: f32,
) -> *mut Pe {
    // SAFETY: caller contract.
    let (h, d) = unsafe { (&*host, &*desc) };
    let p = d.params;
    let max = d.max_particle_num as u32;
    // SAFETY: host contract.
    let vb = unsafe {
        let vb = (h.vb_create)(gfx, max);
        vb_fill(h, vb, max as i32);
        vb
    };
    let mut m = p.start_rel_size;
    if m < p.middle_rel_size {
        m = p.middle_rel_size;
    }
    if m < p.end_rel_size {
        m = p.end_rel_size;
    }
    Box::into_raw(Box::new(Pe {
        host,
        ctx,
        em: Emitter::new(p, max, d.respawn, d.sub_div, material_count as usize),
        ctl: PeCtl::new(),
        vb,
        pool: max as usize,
        materials,
        data,
        world: std::ptr::null_mut(),
        ps: std::ptr::null_mut(),
        sys: std::ptr::null(),
        draw_type: p.draw_type,
        coord: p.coord_system,
        uses_direction: p.uses_direction,
        collides: p.collides,
        warm_time,
        warm_steps,
        direction: Vector3f::default(),
        right: Vector3f::default(),
        forward: Vector3f::default(),
        max_draw: Vector2f {
            x: p.max_start_size.x * m,
            y: p.max_start_size.y * m,
        },
        tmp: IDENTITY,
    }))
}

/// # Safety
/// `pe` from `ohpl_spe_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_free(pe: *mut Pe) {
    if pe.is_null() {
        return;
    }
    // SAFETY: caller contract.
    let pe = unsafe { Box::from_raw(pe) };
    // SAFETY: host contract.
    unsafe { (host(&raw const *pe).vb_free)(pe.vb) };
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_set_world(pe: *mut Pe, world: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { (*pe).world = world };
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_vertex_buffer(pe: *const Pe) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (*pe).vb }
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_num_particles(pe: *const Pe) -> u32 {
    // SAFETY: caller contract.
    unsafe { &(*pe).em }.num_particles()
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_is_dead(pe: *const Pe) -> bool {
    // SAFETY: caller contract.
    unsafe { &(*pe).em }.is_dead()
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_is_dying(pe: *const Pe) -> bool {
    // SAFETY: caller contract.
    unsafe { &(*pe).em }.is_dying()
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_kill(pe: *mut Pe) {
    // SAFETY: caller contract.
    unsafe { &mut (*pe).em }.kill();
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_kill_instantly(pe: *mut Pe) {
    // SAFETY: caller contract.
    unsafe { &mut (*pe).em }.kill_instantly();
}

/// Copies the gate counters out (probes only).
///
/// # Safety
/// `pe` live, `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_ctl(pe: *const Pe, out: *mut PeCtl) {
    // SAFETY: caller contract.
    unsafe { *out = (*pe).ctl };
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_material(pe: *const Pe) -> *mut c_void {
    // SAFETY: caller contract and host contract.
    unsafe { (host(pe).mat_at)((*pe).materials, (*pe).em.frame() as i32) }
}

/// Applies a visibility/culling op (0..=2) through the host, or kills.
///
/// # Safety
/// `pe` live.
pub unsafe fn apply(pe: *mut Pe, op: i32, arg: bool) {
    // SAFETY: caller contract and host contract.
    unsafe {
        match op {
            OP_KILL => (*pe).em.kill(),
            4 => (*pe).em.kill_instantly(),
            _ => (host(pe).apply)(ctx(pe), op, arg),
        }
    }
}

/// `iParticleEmitter::UpdateLogic` and `cParticleEmitter_UserData::UpdateMotion`.
///
/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_update_logic(pe: *mut Pe, dt: f32) {
    let (h, c) = (host(pe), ctx(pe));
    // SAFETY: caller contract and host contract; `pe` is only reborrowed between host calls.
    unsafe {
        if !(h.active)(c) {
            return;
        }
        let dying = (*pe).em.is_dying();
        let same = (h.same_frame)(c);
        let uses_dir = (*pe).uses_direction;
        let count = (h.matrix_count)(c);
        let gate = update_gate(&mut (*pe).ctl, true, dying, same, uses_dir, count);
        if gate == 0 {
            return;
        }
        if gate & GATE_DIRECTION != 0 {
            let inv = matrix_inverse(&*(h.world_matrix)(c));
            (*pe).direction = Vector3f {
                x: inv.m[1][0],
                y: inv.m[1][1],
                z: inv.m[1][2],
            };
        }
        let start = if (*pe).coord == LOCAL {
            (h.local_matrix)(c)
        } else {
            (h.world_matrix)(c)
        };
        let (collides, data) = ((*pe).collides, (*pe).data);
        let mut cb = |s: &Vector3f, e: &Vector3f| {
            if !collides {
                return None;
            }
            let (mut n, mut p) = (Vector3f::default(), Vector3f::default());
            let world = (*pe).world;
            (h.collide)(data, world, s, e, &mut n, &mut p).then_some((n, p))
        };
        (*pe).em.update_motion(dt, &*start, &mut cb);
        (h.transform_updated)(c);
    }
}

fn recompute_axes(pe: *mut Pe) {
    let (h, c) = (host(pe), ctx(pe));
    // SAFETY: host contract; `pe` live.
    unsafe {
        let inv = matrix_inverse(&*(h.world_matrix)(c));
        (*pe).right = Vector3f {
            x: inv.m[0][0],
            y: inv.m[0][1],
            z: inv.m[0][2],
        };
        (*pe).forward = Vector3f {
            x: inv.m[2][0],
            y: inv.m[2][1],
            z: inv.m[2][2],
        };
    }
}

/// # Safety
/// `pe` live; `frustum` is a C++ frustum.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_update_gfx(pe: *mut Pe, frustum: *const c_void) -> bool {
    let (h, c) = (host(pe), ctx(pe));
    let linear = hpl3_color();
    // SAFETY: caller and host contract; `pe` is only reborrowed between host calls.
    unsafe {
        let sys = &*(*pe).sys;
        let sp = sys.pubs();
        let mut f = FrustumInfo::blank();
        (h.frustum)(frustum, &mut f);
        let vb = (*pe).vb;
        let mut g = GfxIn {
            hpl3_color: linear,
            add_blend: false,
            fade_at_distance: sp.fade_at_distance,
            inverts_cull: f.inverts_cull,
            ps_color: sp.color,
            brightness: sp.brightness,
            light_color: sp.light_color,
            light_brightness: sp.light_brightness,
            fade_center: Vector3f::default(),
            origin: f.origin,
            min_fade_start: sp.min_fade_start,
            min_fade_end: sp.min_fade_end,
            max_fade_start: sp.max_fade_start,
            max_fade_end: sp.max_fade_end,
            draw_size: Vector2f { x: 1.0, y: 1.0 },
            world: IDENTITY,
            view: f.view,
            ps_world: IDENTITY,
            axis_right: Vector3f::default(),
            axis_forward: Vector3f::default(),
            pos_stride: (h.vb_stride)(vb),
        };
        if linear {
            let mat = ohpl_spe_material(pe);
            g.add_blend = !mat.is_null() && (h.mat_add_blend)(mat);
            g.world = *(h.world_matrix)(c);
        }
        if g.fade_at_distance {
            (h.ps_center)((*pe).ps, &mut g.fade_center);
        }
        if (*pe).coord == LOCAL {
            g.ps_world = *(h.ps_world_matrix)((*pe).ps);
        }
        if (*pe).draw_type == AXIS {
            let count = (h.matrix_count)(c);
            if axis_gate(&mut (*pe).ctl, true, count) {
                recompute_axes(pe);
            }
            g.axis_right = (*pe).right;
            g.axis_forward = (*pe).forward;
        }

        let quads = (*pe).pool;
        let stride = g.pos_stride.max(0) as usize;
        let sub_div = (*pe).em.sub_div_count() > 1;
        let arr = |kind: i32, n: usize| -> &mut [f32] {
            let p = (h.vb_array)(vb, kind);
            if p.is_null() || n == 0 {
                &mut []
            } else {
                slice::from_raw_parts_mut(p, n)
            }
        };
        let pos = arr(VB_POS, quads * 4 * stride);
        let col = arr(VB_COL, quads * 16);
        let tex = if sub_div {
            arr(VB_TEX, quads * 12)
        } else {
            &mut []
        };
        match (*pe).em.build_vertices(&g, pos, col, tex) {
            Gfx::Hidden => {
                (h.vb_set_count)(vb, 0);
                false
            }
            Gfx::Beam => true,
            Gfx::Drawn => {
                (h.vb_set_count)(vb, ((*pe).em.num_particles() * 6) as i32);
                (h.vb_update)(vb, sub_div);
                true
            }
        }
    }
}

/// # Safety
/// `pe` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_update_bv(pe: *mut Pe) {
    let (h, c) = (host(pe), ctx(pe));
    // SAFETY: caller and host contract; `pe` is only reborrowed between host calls.
    unsafe {
        let count = (h.matrix_count)(c);
        let is_axis = (*pe).draw_type == AXIS;
        let gate = bv_gate(&mut (*pe).ctl, is_axis, count);
        if gate & 1 == 0 {
            return;
        }
        if gate & 2 != 0 {
            recompute_axes(pe);
        }
        let mut pos = Vector3f::default();
        (h.world_pos)(c, &mut pos);
        let (mn, mx) = (*pe)
            .em
            .bounds(pos, (*pe).right, (*pe).forward, (*pe).max_draw);
        (h.bv_set)(c, &mn, &mx);
        if (*pe).coord == LOCAL {
            (h.bv_transform)(c, (h.ps_world_matrix)((*pe).ps));
        }
        (*pe).ctl.update_bv = false;
    }
}

/// # Safety
/// `pe` live; `frustum` null or a C++ frustum. The matrix is valid until the next call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_spe_model_matrix(
    pe: *mut Pe,
    frustum: *const c_void,
) -> *const Matrixf {
    let (h, c) = (host(pe), ctx(pe));
    // SAFETY: caller and host contract.
    unsafe {
        match model_kind(!frustum.is_null(), (*pe).draw_type == AXIS) {
            0 => return (h.world_matrix)(c),
            1 => (*pe).tmp = IDENTITY,
            _ => {
                let mut f = FrustumInfo::blank();
                (h.frustum)(frustum, &mut f);
                (*pe).tmp = matrix_inverse(&f.view);
            }
        }
        &raw const (*pe).tmp
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::scene::h12_psload::{AttrFn, ohpl_h12_ps_desc, ohpl_h12_ps_free, ohpl_h12_ps_parse};
    use crate::scene::sp_sys::{ohpl_sp_add_emitter, ohpl_sp_free, ohpl_sp_new};
    use std::ffi::c_char;
    use std::sync::atomic::{AtomicI32, Ordering::Relaxed};

    static ADDS: [AtomicI32; 5] = [const { AtomicI32::new(0) }; 5];
    static INDEX: AtomicI32 = AtomicI32::new(0);
    static FREED: AtomicI32 = AtomicI32::new(0);

    const NIL: *mut c_void = std::ptr::null_mut();

    unsafe extern "C" fn vb_create(_: *mut c_void, _: u32) -> *mut c_void {
        NIL
    }
    unsafe extern "C" fn vb_index(_: *mut c_void, _: i32) {
        INDEX.fetch_add(1, Relaxed);
    }
    unsafe extern "C" fn vb_add(_: *mut c_void, k: i32, _: f32, _: f32, _: f32, _: f32) {
        ADDS[k as usize].fetch_add(1, Relaxed);
    }
    unsafe extern "C" fn vb_nop(_: *mut c_void) {}
    unsafe extern "C" fn vb_free(_: *mut c_void) {
        FREED.fetch_add(1, Relaxed);
    }
    unsafe extern "C" fn vb_array(_: *mut c_void, _: i32) -> *mut f32 {
        std::ptr::null_mut()
    }
    unsafe extern "C" fn int0(_: *mut c_void) -> i32 {
        0
    }
    unsafe extern "C" fn set_count(_: *mut c_void, _: i32) {}
    unsafe extern "C" fn vb_update(_: *mut c_void, _: bool) {}
    unsafe extern "C" fn inactive(_: *mut c_void) -> bool {
        false
    }
    unsafe extern "C" fn mat(_: *mut c_void) -> *const Matrixf {
        std::ptr::null()
    }
    unsafe extern "C" fn pos(_: *mut c_void, _: *mut Vector3f) {}
    unsafe extern "C" fn nop(_: *mut c_void) {}
    unsafe extern "C" fn bv_set(_: *mut c_void, _: *const Vector3f, _: *const Vector3f) {}
    unsafe extern "C" fn bv_tr(_: *mut c_void, _: *const Matrixf) {}
    unsafe extern "C" fn apply(_: *mut c_void, _: i32, _: bool) {}
    unsafe extern "C" fn frustum(_: *const c_void, _: *mut FrustumInfo) {}
    unsafe extern "C" fn mat_at(_: *mut c_void, _: i32) -> *mut c_void {
        NIL
    }
    unsafe extern "C" fn blend(_: *mut c_void) -> bool {
        false
    }
    unsafe extern "C" fn collide(
        _: *mut c_void,
        _: *mut c_void,
        _: *const Vector3f,
        _: *const Vector3f,
        _: *mut Vector3f,
        _: *mut Vector3f,
    ) -> bool {
        false
    }
    unsafe extern "C" fn no_attr(_: *mut c_void, _: *const c_char) -> *const c_char {
        std::ptr::null()
    }

    static HOST: Host = Host {
        vb_create,
        vb_index,
        vb_add,
        vb_compile: vb_nop,
        vb_free,
        vb_array,
        vb_stride: int0,
        vb_set_count: set_count,
        vb_update,
        active: inactive,
        same_frame: inactive,
        matrix_count: int0,
        world_matrix: mat,
        local_matrix: mat,
        world_pos: pos,
        transform_updated: nop,
        bv_set,
        bv_transform: bv_tr,
        apply,
        ps_world_matrix: mat,
        ps_center: pos,
        frustum,
        mat_at,
        mat_add_blend: blend,
        collide,
    };

    #[test]
    fn vertex_buffer_trace_and_inactive_gate() {
        let get: AttrFn = no_attr;
        // SAFETY: fresh boxes; the host is static.
        unsafe {
            let b = ohpl_h12_ps_parse(get, NIL);
            let d = ohpl_h12_ps_desc(b);
            let max = (*d).max_particle_num;
            let pe = ohpl_spe_new(&HOST, NIL, NIL, NIL, NIL, d, 1, 0.0, 20.0);
            assert_eq!(INDEX.load(Relaxed), 6 * max);
            for a in &ADDS {
                assert_eq!(a.load(Relaxed), 4 * max);
            }
            ohpl_spe_update_logic(pe, 0.1);
            assert_eq!((*pe).ctl, PeCtl::new());
            let s = ohpl_sp_new(NIL, NIL);
            ohpl_sp_add_emitter(s, pe);
            assert!(!ohpl_spe_is_dead(pe));
            ohpl_sp_free(s);
            ohpl_spe_free(pe);
            assert_eq!(FREED.load(Relaxed), 1);
            ohpl_h12_ps_free(b);
        }
    }
}
