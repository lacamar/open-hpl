//! H12 pass 2: `cParticleSystem` lifecycle (warm-up, aggregate dead/dying, fan-out setters),
//! `cParticleSystemData::LoadFromElement` child selection and the `iParticleEmitter` shell state
//! (sleep count, update counters, bounding-volume and axis gates).
//!
//! The C++ keeps the emitters and the XML; this module decides the order of calls and owns the
//! counters. Every call into the system goes through `PsHost`, which may call back into C++, so no
//! reference into `PeCtl` or the caller's flags is held across a host call.

use std::ffi::c_void;

#[repr(C)]
pub struct PsHost {
    pub ctx: *mut c_void,
    pub emitter_count: unsafe extern "C" fn(*mut c_void) -> i32,
    pub warm_up_time: unsafe extern "C" fn(*mut c_void, i32) -> f32,
    pub warm_up_steps: unsafe extern "C" fn(*mut c_void, i32) -> f32,
    pub update: unsafe extern "C" fn(*mut c_void, i32, f32),
    pub apply: unsafe extern "C" fn(*mut c_void, i32, i32, bool),
    pub query: unsafe extern "C" fn(*mut c_void, i32, i32) -> bool,
}

impl PsHost {
    fn count(&self) -> i32 {
        // SAFETY: `ohpl_h12b_ps_*` contract: the host table is valid for the call.
        unsafe { (self.emitter_count)(self.ctx) }
    }

    fn each(&self, mut f: impl FnMut(i32)) {
        let mut i = 0;
        while i < self.count() {
            f(i);
            i += 1;
        }
    }

    pub fn all(&self, q: i32) -> bool {
        let mut hits = 0;
        self.each(|i| {
            // SAFETY: as `count`.
            if unsafe { (self.query)(self.ctx, i, q) } {
                hits += 1;
            }
        });
        hits == self.count()
    }
}

/// `cParticleSystem::UpdateLogic`. `first` is the C++ `mbFirstUpdate`.
pub fn update_logic(host: &PsHost, first: &mut bool, active: bool, dt: f32) {
    if !active {
        return;
    }
    host.each(|i| {
        if *first {
            // SAFETY: as `PsHost::count`.
            let t = unsafe { (host.warm_up_time)(host.ctx, i) };
            if t > 0.0 {
                let mut time = t;
                // SAFETY: as above.
                let step = 1.0 / unsafe { (host.warm_up_steps)(host.ctx, i) };
                // Negative steps never end, as in C++.
                while time > 0.0 {
                    // SAFETY: as above.
                    unsafe { (host.update)(host.ctx, i, step) };
                    time -= step;
                }
            }
        }
        // SAFETY: as above.
        unsafe { (host.update)(host.ctx, i, dt) };
    });
    *first = false;
}

pub fn set_visible(host: &PsHost, visible: &mut bool, new: bool) {
    if *visible == new {
        return;
    }
    *visible = new;
    apply_all(host, 0, new);
}

pub fn apply_all(host: &PsHost, op: i32, arg: bool) {
    host.each(|i| {
        // SAFETY: as `PsHost::count`.
        unsafe { (host.apply)(host.ctx, i, op, arg) };
    });
}

/// Which `<ParticleSystem>` children load as emitters; the result is `!mvEmitterData.empty()`.
pub fn data_load_plan(
    root_ok: bool,
    child_is_emitter: &[bool],
    existing: usize,
    load: &mut [bool],
) -> bool {
    let mut n = existing;
    for (i, slot) in load.iter_mut().enumerate() {
        *slot = root_ok && child_is_emitter.get(i).copied().unwrap_or(false);
        n += usize::from(*slot);
    }
    root_ok && n > 0
}

/// `PeCtl` of an `iParticleEmitter`: the members the shell logic reads and writes.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PeCtl {
    pub sleep_count: i32,
    pub direction_update_count: i32,
    pub axis_update_count: i32,
    pub update_gfx: bool,
    pub update_bv: bool,
}

impl PeCtl {
    pub const fn new() -> Self {
        Self {
            sleep_count: 60 * 5,
            direction_update_count: -1,
            axis_update_count: -1,
            update_gfx: true,
            update_bv: true,
        }
    }
}

impl Default for PeCtl {
    fn default() -> Self {
        Self::new()
    }
}

pub const GATE_RUN: u32 = 1;
pub const GATE_DIRECTION: u32 = 2;

/// `iParticleEmitter::UpdateLogic` up to the `UpdateMotion` call: 0 = return early.
pub fn update_gate(
    c: &mut PeCtl,
    active: bool,
    dying: bool,
    same_render_frame: bool,
    uses_direction: bool,
    matrix_count: i32,
) -> u32 {
    if !active {
        return 0;
    }
    if !dying && !same_render_frame {
        if c.sleep_count <= 0 {
            return 0;
        }
        c.sleep_count -= 1;
    } else if c.sleep_count < 10 {
        c.sleep_count = 10;
    }
    c.update_gfx = true;
    c.update_bv = true;
    let mut r = GATE_RUN;
    if uses_direction && c.direction_update_count != matrix_count {
        c.direction_update_count = matrix_count;
        r |= GATE_DIRECTION;
    }
    r
}

/// True when the axis vectors must be recomputed for `matrix_count`.
pub fn axis_gate(c: &mut PeCtl, is_axis: bool, matrix_count: i32) -> bool {
    if is_axis && c.axis_update_count != matrix_count {
        c.axis_update_count = matrix_count;
        return true;
    }
    false
}

/// `GetBoundingVolume` gate: bit 0 recompute the volume, bit 1 recompute the axis vectors first.
pub fn bv_gate(c: &mut PeCtl, is_axis: bool, matrix_count: i32) -> u32 {
    if !c.update_bv {
        return 0;
    }
    let r = 1 | (u32::from(axis_gate(c, is_axis, matrix_count)) << 1);
    c.update_bv = false;
    r
}

/// `GetModelMatrix`: 0 world matrix, 1 identity, 2 inverse view.
pub fn model_kind(has_frustum: bool, is_axis: bool) -> u32 {
    match (has_frustum, is_axis) {
        (false, _) => 0,
        (true, true) => 1,
        (true, false) => 2,
    }
}

/// # Safety
/// `host` is valid for the call, `first` is valid for reads and writes. Host calls may re-enter
/// the system, so `first` is copied out and back.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12b_ps_update_logic(
    host: *const PsHost,
    first: *mut bool,
    active: bool,
    dt: f32,
) {
    // SAFETY: caller contract.
    let (h, mut f) = unsafe { (&*host, *first) };
    update_logic(h, &mut f, active, dt);
    // SAFETY: caller contract.
    unsafe { *first = f };
}

/// # Safety
/// As `ohpl_h12b_ps_update_logic`, for `visible`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12b_ps_set_visible(
    host: *const PsHost,
    visible: *mut bool,
    new: bool,
) {
    // SAFETY: caller contract.
    let (h, mut v) = unsafe { (&*host, *visible) };
    set_visible(h, &mut v, new);
    // SAFETY: caller contract.
    unsafe { *visible = v };
}

/// # Safety
/// `host` is valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12b_ps_apply_all(host: *const PsHost, op: i32, arg: bool) {
    // SAFETY: caller contract.
    apply_all(unsafe { &*host }, op, arg);
}

/// # Safety
/// `host` is valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12b_ps_all(host: *const PsHost, query: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { &*host }.all(query)
}

/// # Safety
/// `child_is_emitter` and `load` hold `n` bools each, or are null with `n == 0`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12b_psdata_plan(
    root_ok: bool,
    child_is_emitter: *const bool,
    n: u32,
    existing: u32,
    load: *mut bool,
) -> bool {
    let n = n as usize;
    let (c, l): (&[bool], &mut [bool]) = if n == 0 {
        (&[], &mut [])
    } else {
        // SAFETY: caller contract.
        unsafe {
            (
                std::slice::from_raw_parts(child_is_emitter, n),
                std::slice::from_raw_parts_mut(load, n),
            )
        }
    };
    data_load_plan(root_ok, c, existing as usize, l)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_psdata_can_create(emitters: u32) -> bool {
    emitters > 0
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_pe_ctl_init(c: &mut PeCtl) {
    *c = PeCtl::new();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_pe_update_gate(
    c: &mut PeCtl,
    active: bool,
    dying: bool,
    same_render_frame: bool,
    uses_direction: bool,
    matrix_count: i32,
) -> u32 {
    update_gate(
        c,
        active,
        dying,
        same_render_frame,
        uses_direction,
        matrix_count,
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_pe_axis_gate(c: &mut PeCtl, is_axis: bool, matrix_count: i32) -> bool {
    axis_gate(c, is_axis, matrix_count)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_pe_bv_gate(c: &mut PeCtl, is_axis: bool, matrix_count: i32) -> u32 {
    bv_gate(c, is_axis, matrix_count)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_pe_model_kind(has_frustum: bool, is_axis: bool) -> u32 {
    model_kind(has_frustum, is_axis)
}

#[cfg(test)]
#[test]
fn ctl_layout() {
    assert_eq!(std::mem::size_of::<PeCtl>(), 16);
}
