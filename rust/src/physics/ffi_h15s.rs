use super::host::Str;
use super::shell::{self, BodyState, JointStateC};
use super::wreg::WorldOp;
use crate::types::Vector3f;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15b_new(
    op: WorldOp,
    me: usize,
    out: *mut *mut BodyState,
    world: usize,
    shape: usize,
) -> u64 {
    unsafe { shell::body_new(op, me, out, world, shape) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_free(b: u64) {
    shell::body_free(b);
}

#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ohpl_h15j_new(
    op: WorldOp,
    me: usize,
    vt: usize,
    parent: usize,
    child: usize,
    world: usize,
    pivot: Vector3f,
    pin: Vector3f,
) -> *mut JointStateC {
    shell::joint_new(op, me, vt, parent, child, world, pivot, pin)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15j_drop(j: u64, me: usize) {
    shell::joint_drop(j, me);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15j_add_controller(j: u64, me: usize, ctrl: usize) {
    shell::joint_add_controller(j, me, ctrl);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_get_controller(j: u64, name: *const u8, n: usize) -> usize {
    shell::joint_get_controller(j, unsafe { Str { p: name, n }.bytes() })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_change_controller(j: u64, name: *const u8, n: usize) -> bool {
    shell::joint_change_controller(j, unsafe { Str { p: name, n }.bytes() })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15j_set_all_paused(j: u64, paused: bool) {
    shell::joint_set_all_paused(j, paused);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15j_controller_len(j: u64) -> usize {
    shell::joint_controller_len(j)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_controller_at(
    j: u64,
    i: usize,
    name: *mut Str,
    obj: *mut usize,
) -> bool {
    let Some((p, n, h)) = shell::joint_controller_at(j, i) else {
        return false;
    };
    unsafe {
        if let Some(o) = name.as_mut() {
            *o = Str { p, n };
        }
        if let Some(o) = obj.as_mut() {
            *o = h;
        }
    }
    true
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_update(j: u64, me: usize) -> bool {
    unsafe { shell::joint_update(j, me) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_check_breakage(j: u64, me: usize) -> bool {
    unsafe { shell::joint_check_breakage(j, me) }
}

#[repr(C)]
pub struct CtrlBox {
    params: super::controller::CtrlParams,
    pid: super::pid::Pid,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15c_new(me: usize) -> *mut super::controller::CtrlParams {
    let mut pid = super::pid::Pid::new();
    pid.set_error_num(10);
    Box::into_raw(Box::new(CtrlBox {
        params: super::controller::CtrlParams::new(me),
        pid,
    }))
    .cast()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15c_free(c: *mut super::controller::CtrlParams) {
    if !c.is_null() {
        drop(unsafe { Box::from_raw(c.cast::<CtrlBox>()) });
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15c_update(
    vt: *const super::host::Vt,
    c: *mut super::controller::CtrlParams,
    time_step: f32,
) {
    let (Some(vt), Some(p)) = (unsafe { vt.as_ref() }, unsafe { c.as_ref() }) else {
        return;
    };
    let p = *p;
    let pid = unsafe { &raw mut (*c.cast::<CtrlBox>()).pid };
    unsafe { super::controller::update(&vt, &p, pid, time_step) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15c_set_active(c: *mut super::controller::CtrlParams, active: bool) {
    if let Some(p) = unsafe { c.as_mut() } {
        if p.active == active {
            return;
        }
        p.active = active;
        unsafe { (*c.cast::<CtrlBox>()).pid.reset() };
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15c_set_error_num(c: *mut super::controller::CtrlParams, n: i32) {
    if let Some(b) = unsafe { c.cast::<CtrlBox>().as_mut() } {
        b.pid.set_error_num(n);
    }
}
