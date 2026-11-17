//! `cLuxPlayer` input routing, state switching, map transitions and HUD (A2c); see `a2d_disp`.

use super::a2_core::Core;
use super::a2d_disp::{
    DHost, Disp, MS_LAST, ONE, ST_GRAB, ST_LEVER, ST_NORMAL, ST_PUSH, ST_SLIDE, ST_USE_ITEM,
    ST_WHEEL, k,
};
use std::ffi::c_void;

const ACTION_LANTERN: i32 = 4;
const MSG_MAP_ENTER: i32 = 0;
const MSG_MAP_LEAVE: i32 = 1;

fn flag(b: bool) -> f32 {
    f32::from(u8::from(b))
}

/// # Safety
/// All pointers valid; the host may call back into the player.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_on_draw(c: *mut Core, d: *mut Disp, dh: *const DHost, dt: f32) {
    // SAFETY: caller contract; no reference lives across a host call.
    unsafe {
        let dh = &*dh;
        dh.c(k::MS_ON_DRAW, (*d).move_state, dt, 0.0);
        dh.c(k::ST_ON_DRAW, (*d).state, dt, 0.0);
        dh.c(k::HELPERS_DRAW, 0, dt, 0.0);
        if can_draw_crosshair(d, dh) {
            dh.c(k::DRAW_CROSSHAIR, 0, 0.0, 0.0);
        }
        let alpha = (*c).focus_alpha;
        if dh.cb(k::FOCUS_TEXT_STATE, 0, 0.0, 0.0) {
            dh.c(k::DRAW_FOCUS_TEXT, 0, alpha, 0.0);
        } else if alpha > 0.0 {
            dh.c(k::DRAW_FOCUS_TEXT, 1, alpha, 0.0);
        }
    }
}

/// # Safety
/// `d` valid.
unsafe fn can_draw_crosshair(d: *mut Disp, dh: &DHost) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if dh.cb(k::PAUSE_ACTIVE, 0, 0.0, 0.0) || !(*d).script_show_focus {
            return false;
        }
        if !dh.cb(k::SHOW_CROSSHAIR, 0, 0.0, 0.0) && (*d).state != ST_USE_ITEM {
            return dh.cb(k::FOCUS_CAN_INTERACT, 0, 0.0, 0.0);
        }
        true
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_move(d: *mut Disp, dh: *const DHost, dir: i32, mul: f32) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if (*d).free_cam {
            dh.c(k::FREE_MOVE, dir, (*d).free_cam_speed * mul, 0.0);
            return;
        }
        (*d).pressed_move = true;
        if dh.cb(k::ST_ON_MOVE, (*d).state, dir as f32, mul)
            && dh.cb(k::MS_ON_MOVE, (*d).move_state, dir as f32, mul)
        {
            dh.c(k::BODY_MOVE, dir, mul, 0.0);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
unsafe fn look(d: *mut Disp, dh: &DHost, amount: f32, ks: [u32; 4]) {
    // SAFETY: caller contract.
    unsafe {
        if (*d).free_cam {
            dh.c(ks[0], 0, amount, 0.0);
            return;
        }
        let amount = amount * (*d).look_speed_mul;
        if dh.cb(ks[1], (*d).state, amount, 0.0) && dh.cb(ks[2], (*d).move_state, amount, 0.0) {
            dh.c(ks[3], 0, amount, 0.0);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_add_yaw(d: *mut Disp, dh: *const DHost, amount: f32) {
    // SAFETY: caller contract.
    unsafe {
        look(
            d,
            &*dh,
            amount,
            [k::FREE_YAW, k::ST_ON_YAW, k::MS_ON_YAW, k::YAW],
        )
    };
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_add_pitch(d: *mut Disp, dh: *const DHost, amount: f32) {
    // SAFETY: caller contract.
    unsafe {
        look(
            d,
            &*dh,
            amount,
            [k::FREE_PITCH, k::ST_ON_PITCH, k::MS_ON_PITCH, k::PITCH],
        )
    };
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_scroll(d: *mut Disp, dh: *const DHost, amount: f32) {
    // SAFETY: caller contract.
    unsafe { (*dh).c(k::ST_ON_SCROLL, (*d).state, amount, 0.0) };
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_do_action(
    d: *mut Disp,
    dh: *const DHost,
    action: i32,
    pressed: bool,
) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if dh.cb(k::ST_ON_ACTION, (*d).state, action as f32, flag(pressed))
            && action == ACTION_LANTERN
            && pressed
        {
            dh.c(k::LANTERN_TOGGLE, 0, 0.0, 0.0);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_run(d: *mut Disp, dh: *const DHost, pressed: bool) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        (*d).pressing_run = pressed;
        if dh.cb(k::ST_ON_RUN, (*d).state, flag(pressed), 0.0) {
            dh.c(k::MS_ON_RUN, (*d).move_state, flag(pressed), 0.0);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_jump(d: *mut Disp, dh: *const DHost, pressed: bool) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if pressed && ((*d).jump_disabled || dh.cb(k::COLLAPSE_ACTIVE, 0, 0.0, 0.0)) {
            return;
        }
        (*d).pressing_jump = pressed;
        if dh.cb(k::ST_ON_JUMP, (*d).state, flag(pressed), 0.0) {
            dh.c(k::MS_ON_JUMP, (*d).move_state, flag(pressed), 0.0);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_crouch(d: *mut Disp, dh: *const DHost, pressed: bool) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if pressed && ((*d).crouch_disabled || dh.cb(k::COLLAPSE_ACTIVE, 0, 0.0, 0.0)) {
            return;
        }
        if dh.cb(k::ST_ON_CROUCH, (*d).state, flag(pressed), 0.0) {
            dh.c(k::MS_ON_CROUCH, (*d).move_state, flag(pressed), 0.0);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_change_state(d: *mut Disp, dh: *const DHost, state: i32) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if (*d).state == state {
            return;
        }
        dh.c(k::ST_LEAVE, (*d).state, state as f32, 0.0);
        let prev = (*d).state;
        (*d).state = state;
        dh.c(k::ST_SET_PREV, state, prev as f32, 0.0);
        dh.c(k::ST_ENTER, state, prev as f32, 0.0);
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_change_move_state(d: *mut Disp, dh: *const DHost, state: i32) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if (*d).move_state == state {
            return;
        }
        dh.c(k::MS_LEAVE, (*d).move_state, state as f32, 0.0);
        let prev = (*d).move_state;
        (*d).move_state = state;
        dh.c(k::MS_ENTER, state, prev as f32, 0.0);
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_on_leave_container(d: *mut Disp, dh: *const DHost) {
    // SAFETY: caller contract.
    unsafe {
        (*d).pressing_run = false;
        (*d).pressing_jump = false;
        if matches!(
            (*d).state,
            ST_GRAB | ST_SLIDE | ST_PUSH | ST_WHEEL | ST_LEVER
        ) {
            ohpl_a2d_change_state(d, dh, ST_NORMAL);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_set_free_cam(d: *mut Disp, dh: *const DHost, active: bool) {
    // SAFETY: caller contract.
    unsafe {
        (*d).free_cam = active;
        (*dh).c(k::FREECAM_CAMERA, i32::from(active), 0.0, 0.0);
    }
}

/// # Safety
/// As `ohpl_a2d_on_draw`; `map` is only passed through to the host.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_map_enter(
    c: *mut Core,
    d: *mut Disp,
    dh: *const DHost,
    map: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        for i in 0..MS_LAST {
            dh.c(k::MS_MAP_ENTER, i, 0.0, 0.0);
        }
        let ms = (*d).move_state;
        dh.c(k::MS_ENTER, ms, ms as f32, 0.0);
        dh.cp(k::ST_MAP_ENTER, (*d).state, map);
        dh.cp(k::HELPERS_LUX_MSG, MSG_MAP_ENTER, map);
        dh.c(k::CLEAR_FOCUS_TEXT, 0, 0.0, 0.0);
        (*c).focus_alpha = 0.0;
        (*d).is_in_water = false;
        dh.c(k::NULL_FOCUS, 0, 0.0, 0.0);
    }
}

/// # Safety
/// As `ohpl_a2d_map_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_map_leave(
    c: *mut Core,
    d: *mut Disp,
    dh: *const DHost,
    map: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        ohpl_a2d_change_state(d, dh, ST_NORMAL);
        dh.cp(k::ST_MAP_LEAVE, (*d).state, map);
        dh.c(k::DESTROY_CB, 0, 0.0, 0.0);
        dh.cp(k::HELPERS_LUX_MSG, MSG_MAP_LEAVE, map);
        dh.c(k::CLEAR_TERROR, 0, 0.0, 0.0);
        let m = &mut (*c).cam;
        m.aspect_mul_goal = ONE;
        m.aspect_mul = ONE;
        m.fov_mul_goal = ONE;
        m.fov_mul = ONE;
        m.roll_goal = 0.0;
        m.roll = 0.0;
        m.fading_pitch = 0;
        m.pitch_goal = 0.0;
        m.pitch_speed_mul = 0.0;
        m.pitch_max_speed = 0.0;
        m.lean_roll = 0.0;
        m.lean_roll_goal = 0.0;
        m.lean_roll_speed_mul = 0.0;
        m.lean_roll_max_speed = 0.0;
        dh.c(k::CAM_MAP_LEAVE, 0, 0.0, 0.0);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    thread_local! { static LOG: RefCell<Vec<(u32, i32)>> = const { RefCell::new(Vec::new()) }; }

    extern "C" fn rec(_: *mut c_void, kind: u32, i: i32, _: f32, _: f32, _: *mut c_void) -> f32 {
        LOG.with(|l| l.borrow_mut().push((kind, i)));
        1.0
    }

    fn disp() -> Disp {
        // SAFETY: all-zero is a valid Disp (null ring, zero numbers, false flags).
        unsafe { std::mem::zeroed() }
    }

    fn host() -> DHost {
        LOG.with(|l| l.borrow_mut().clear());
        DHost {
            ctx: std::ptr::null_mut(),
            call: rec,
        }
    }

    fn kinds() -> Vec<u32> {
        LOG.with(|l| l.borrow().iter().map(|e| e.0).collect())
    }

    #[test]
    fn change_state_order_and_noop() {
        let mut d = disp();
        let dh = host();
        // SAFETY: valid pointers.
        unsafe {
            ohpl_a2d_change_state(&mut d, &dh, ST_NORMAL);
            assert!(kinds().is_empty());
            ohpl_a2d_change_state(&mut d, &dh, ST_GRAB);
        }
        assert_eq!(d.state, ST_GRAB);
        assert_eq!(
            LOG.with(|l| l.borrow().clone()),
            [(k::ST_LEAVE, 0), (k::ST_SET_PREV, 3), (k::ST_ENTER, 3)]
        );
    }

    #[test]
    fn jump_blocked_and_move_chain() {
        let mut d = disp();
        d.jump_disabled = true;
        let dh = host();
        // SAFETY: valid pointers.
        unsafe {
            ohpl_a2d_jump(&mut d, &dh, true);
            assert!(!d.pressing_jump && kinds().is_empty());
            ohpl_a2d_move(&mut d, &dh, 0, 1.0);
        }
        assert!(d.pressed_move);
        assert_eq!(kinds(), [k::ST_ON_MOVE, k::MS_ON_MOVE, k::BODY_MOVE]);
    }

    #[test]
    fn leave_container_resets_interaction() {
        let mut d = disp();
        d.state = ST_PUSH;
        d.pressing_run = true;
        let dh = host();
        // SAFETY: valid pointers.
        unsafe { ohpl_a2d_on_leave_container(&mut d, &dh) };
        assert_eq!((d.state, d.pressing_run), (ST_NORMAL, false));
    }
}
