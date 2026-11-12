//! C ABI for the player and move states, mirrored in `amnesia/src/game/RustLuxPlayer.h`.

use super::move_normal::{
    self as mv, BobCfg, BobIn, BobState, FallCfg, JumpState, LedgeCfg, MoveLimits, MoveMulIn,
    SpeedCfg, SpeedMuls,
};
use super::states_base::{self as sb, Action};
use super::states_grab::{
    self as sg, GrabEnterIn, GrabEnterOut, GrabPost, GrabPostOut, PushBegin, PushBeginOut,
    PushDrive, PushDriveOut, PushEnterIn, PushEnterOut, SlideIn, SlideOut,
};
use super::states_ladder::{
    self as sl, LadderAttachOut, LadderEnterIn, LadderEnterOut, LadderState,
};
use super::states_rotate::{self as sr, JointGeom, RotateTorqueIn};
use super::states_vec::Pid3;
use crate::types::{Matrixf, Vector2f, Vector3f};

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Pid3>() == 368);
    assert!(size_of::<MoveMulIn>() == 28);
    assert!(size_of::<SpeedCfg>() == 36);
    assert!(size_of::<SpeedMuls>() == 16);
    assert!(size_of::<MoveLimits>() == 16);
    assert!(size_of::<JumpState>() == 12);
    assert!(size_of::<FallCfg>() == 24);
    assert!(size_of::<BobCfg>() == 60);
    assert!(size_of::<BobState>() == 60);
    assert!(size_of::<BobIn>() == 28);
    assert!(size_of::<LedgeCfg>() == 12);
    assert!(size_of::<Action>() == 16);
    assert!(size_of::<GrabEnterIn>() == 156);
    assert!(size_of::<GrabEnterOut>() == 92);
    assert!(size_of::<GrabPost>() == 304);
    assert!(size_of::<GrabPostOut>() == 28);
    assert!(size_of::<PushEnterIn>() == 124);
    assert!(size_of::<PushEnterOut>() == 32);
    assert!(size_of::<PushBegin>() == 148);
    assert!(size_of::<PushBeginOut>() == 32);
    assert!(size_of::<PushDrive>() == 132);
    assert!(size_of::<PushDriveOut>() == 100);
    assert!(size_of::<SlideIn>() == 168);
    assert!(size_of::<SlideOut>() == 16);
    assert!(size_of::<RotateTorqueIn>() == 104);
    assert!(size_of::<JointGeom>() == 144);
    assert!(size_of::<LadderEnterIn>() == 40);
    assert!(size_of::<LadderEnterOut>() == 24);
    assert!(size_of::<LadderState>() == 56);
    assert!(size_of::<LadderAttachOut>() == 12);
};

/// # Safety
/// `p` is valid and exclusively borrowed.
unsafe fn rw<'a, T>(p: *mut T) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut *p }
}

/// # Safety
/// `p` is valid for reads.
unsafe fn rd<'a, T>(p: *const T) -> &'a T {
    // SAFETY: caller contract.
    unsafe { &*p }
}

macro_rules! ffi {
    ($(#[$m:meta])* fn $name:ident($($a:ident: $t:ty),* $(,)?) $(-> $r:ty)? $body:block) => {
        $(#[$m])*
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $t),*) $(-> $r)? $body
    };
}

ffi!(
    /// # Safety
    /// `i` is readable.
    fn ohpl_ga2b_move_speed_mul(i: *const MoveMulIn) -> f32 {
        // SAFETY: caller contract.
        mv::move_speed_mul(unsafe { rd(i) })
    }
);

ffi!(
    /// # Safety
    /// `c` is readable, `out` writable.
    fn ohpl_ga2b_speed_multipliers(
        c: *const SpeedCfg,
        move_mul: f32,
        run_mul: f32,
        on_ground: bool,
        crouching: bool,
        running: bool,
        out: *mut SpeedMuls,
    ) {
        // SAFETY: caller contract.
        unsafe {
            *rw(out) =
                mv::speed_multipliers(rd(c), move_mul, run_mul, on_ground, crouching, running);
        }
    }
);

ffi!(
    /// # Safety
    /// `m` is readable, `out` writable.
    fn ohpl_ga2b_move_limits(
        max_fwd: f32,
        max_back: f32,
        max_side: f32,
        m: *const SpeedMuls,
        out: *mut MoveLimits,
    ) {
        // SAFETY: caller contract.
        unsafe { *rw(out) = mv::move_limits(max_fwd, max_back, max_side, rd(m)) }
    }
);

ffi!(
    /// # Safety
    /// `was_moving` is valid and exclusively borrowed.
    fn ohpl_ga2b_movement_update(
        on_ground: bool,
        pressed_move: bool,
        was_moving: *mut bool,
        running: bool,
        crouch_disabled: bool,
        collapse_active: bool,
    ) -> bool {
        // SAFETY: caller contract.
        mv::movement_update(
            on_ground,
            pressed_move,
            unsafe { rw(was_moving) },
            running,
            crouch_disabled,
            collapse_active,
        )
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_run_pressed_stands(
        crouch_disabled: bool,
        collapse_active: bool,
        on_ground: bool,
        pressed_move: bool,
    ) -> bool {
        mv::run_pressed_stands(crouch_disabled, collapse_active, on_ground, pressed_move)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_jump_starts(pressed: bool, jumping: bool, on_ground: bool) -> bool {
        mv::jump_starts(pressed, jumping, on_ground)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_jump_force(
        crouching: bool,
        crouch_force: f32,
        force: f32,
        script_mul: f32,
        mass: f32,
    ) -> f32 {
        mv::jump_force(crouching, crouch_force, force, script_mul, mass)
    }
);

ffi!(
    /// # Safety
    /// `s` is valid and exclusively borrowed.
    fn ohpl_ga2b_jump_tick(s: *mut JumpState, dt: f32) -> bool {
        // SAFETY: caller contract.
        mv::jump_tick(unsafe { rw(s) }, dt)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_jump_gravity_force(count: f32, max: f32, gravity_y: f32, mass: f32) -> f32 {
        mv::jump_gravity_force(count, max, gravity_y, mass)
    }
);

ffi!(
    /// # Safety
    /// `was` is valid and exclusively borrowed.
    fn ohpl_ga2b_ground_check(on_ground: bool, force_vel_y: f32, was: *mut bool) {
        // SAFETY: caller contract.
        mv::ground_check(on_ground, force_vel_y, unsafe { rw(was) });
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_bounce_starts(
        normal_y: f32,
        force_vel_y: f32,
        min_speed: f32,
        active: bool,
    ) -> bool {
        mv::bounce_starts(normal_y, force_vel_y, min_speed, active)
    }
);

ffi!(
    /// # Safety
    /// `c` is readable, `damage` writable.
    fn ohpl_ga2b_fall_damage(c: *const FallCfg, y_speed: f32, damage: *mut f32) -> i32 {
        // SAFETY: caller contract.
        let (tier, d) = mv::fall_damage(unsafe { rd(c) }, y_speed);
        // SAFETY: caller contract.
        unsafe { *rw(damage) = d };
        tier
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid; `s` is exclusively borrowed.
    fn ohpl_ga2b_head_bob(s: *mut BobState, c: *const BobCfg, i: *const BobIn, dt: f32) -> bool {
        // SAFETY: caller contract.
        unsafe { mv::head_bob(rw(s), rd(c), rd(i), dt) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_ledge_update(
        c: *const LedgeCfg,
        head_add: *const Vector3f,
        pitch: f32,
        dt: f32,
        pitch_add: *mut f32,
    ) -> bool {
        // SAFETY: caller contract.
        unsafe {
            match mv::ledge_update(rd(c), *rd(head_add), pitch, dt) {
                Some(a) => {
                    *rw(pitch_add) = a;
                    true
                }
                None => false,
            }
        }
    }
);

ffi!(
    /// # Safety
    /// `out` is writable.
    fn ohpl_ga2b_action(which: i32, act: i32, pressed: bool, out: *mut Action) {
        let a = match which {
            0 => sb::normal_action(act, pressed),
            1 => sb::hand_object_action(act, pressed),
            _ => sb::use_item_action(act, pressed),
        };
        // SAFETY: caller contract.
        unsafe { *rw(out) = a };
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_hand_object_hides(new_state: i32) -> bool {
        sb::hand_object_hides(new_state)
    }
);

ffi!(
    /// # Safety
    /// `interact` is writable.
    fn ohpl_ga2b_default_base_interact(
        act: i32,
        pressed: bool,
        can_interact: bool,
        interact: *mut bool,
    ) -> bool {
        let (pass, i) = sb::default_base_interact(act, pressed, can_interact);
        // SAFETY: caller contract.
        unsafe { *rw(interact) = i };
        pass
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_interact_action(act: i32, pressed: bool) -> i32 {
        sb::interact_action(act, pressed)
    }
);

ffi!(
    /// # Safety
    /// `release` is writable.
    fn ohpl_ga2b_ladder_action(act: i32, pressed: bool, release: *mut bool) -> bool {
        let (r, pass) = sb::ladder_action(act, pressed);
        // SAFETY: caller contract.
        unsafe { *rw(release) = r };
        pass
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_focus_in_range(has_entity: bool, dist: f32, max_focus: f32) -> bool {
        sb::focus_in_range(has_entity, dist, max_focus)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_focus_can_interact(in_range: bool, can_interact: bool, disabled: bool) -> bool {
        sb::focus_can_interact(in_range, can_interact, disabled)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_crosshair_pick(
        crosshair: i32,
        default_crosshair: i32,
        last_enum: i32,
        disabled: bool,
        style_default: bool,
    ) -> i32 {
        sb::crosshair_pick(
            crosshair,
            default_crosshair,
            last_enum,
            disabled,
            style_default,
        )
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_outline_gate(has_entity: bool, is_prop: bool, dist: f32, max_focus: f32) -> bool {
        sb::outline_gate(has_entity, is_prop, dist, max_focus)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_show_outline(is_item: bool, can_interact: bool, disabled: bool) -> bool {
        sb::show_outline(is_item, can_interact, disabled)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_use_item_max_focus(max_focus: f32, min_use: f32) -> f32 {
        sb::use_item_max_focus(max_focus, min_use)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_use_item_focus(
        has_item: bool,
        has_entity: bool,
        has_map: bool,
        dist: f32,
        max: f32,
    ) -> i32 {
        sb::use_item_focus(has_item, has_entity, has_map, dist, max)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_use_item_plan(callback: bool) -> i32 {
        sb::use_item_plan(callback)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_use_item_draw(
        has_entity: bool,
        dist: f32,
        max: f32,
        is_prop: bool,
        is_item: bool,
    ) -> i32 {
        sb::use_item_draw(has_entity, dist, max, is_prop, is_item)
    }
);

ffi!(
    /// # Safety
    /// `p` is valid and exclusively borrowed.
    fn ohpl_ga2b_pid_init(p: *mut Pid3, n: i32, kp: f32, ki: f32, kd: f32) {
        // SAFETY: caller contract.
        unsafe { rw(p) }.init(n, kp, ki, kd);
    }
);

ffi!(
    /// # Safety
    /// `p` is valid and exclusively borrowed.
    fn ohpl_ga2b_pid_reset(p: *mut Pid3) {
        // SAFETY: caller contract.
        unsafe { rw(p) }.reset();
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid; `out` is writable.
    fn ohpl_ga2b_grab_enter(i: *const GrabEnterIn, o: *mut GrabEnterOut) {
        // SAFETY: caller contract.
        unsafe { sg::grab_enter(rd(i), rw(o)) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid; the pids and `o` are exclusively borrowed.
    fn ohpl_ga2b_grab_post(
        i: *const GrabPost,
        force_pid: *mut Pid3,
        torque_pid: *mut Pid3,
        dt: f32,
        o: *mut GrabPostOut,
    ) {
        // SAFETY: caller contract.
        unsafe { sg::grab_post(rd(i), rw(force_pid), rw(torque_pid), dt, rw(o)) }
    }
);

ffi!(
    /// # Safety
    /// `out` is writable.
    fn ohpl_ga2b_leave_limit(v: *const Vector3f, max: f32, out: *mut Vector3f) -> bool {
        // SAFETY: caller contract.
        unsafe {
            match sg::leave_limit(*rd(v), max) {
                Some(n) => {
                    *rw(out) = n;
                    true
                }
                None => false,
            }
        }
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_grab_scroll(depth: f32, amount: f32, inc: f32, min: f32, max: f32) -> f32 {
        sg::grab_scroll(depth, amount, inc, min, max)
    }
);

ffi!(
    /// # Safety
    /// `rot` is readable, `out` writable.
    fn ohpl_ga2b_grab_rotate(rot: *const Matrixf, pitch: bool, amount: f32, out: *mut Matrixf) {
        // SAFETY: caller contract.
        unsafe { *rw(out) = sg::grab_rotate(rd(rot), pitch, amount) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_push_enter(i: *const PushEnterIn, o: *mut PushEnterOut) {
        // SAFETY: caller contract.
        unsafe { sg::push_enter(rd(i), rw(o)) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_push_begin(i: *const PushBegin, o: *mut PushBeginOut) {
        // SAFETY: caller contract.
        unsafe { sg::push_begin(rd(i), rw(o)) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_push_stop(
        pid: *mut Pid3,
        vel: *const Vector3f,
        mass: f32,
        max_force: f32,
        dt: f32,
        out: *mut Vector3f,
    ) {
        // SAFETY: caller contract.
        unsafe { *rw(out) = sg::push_stop(rw(pid), *rd(vel), mass, max_force, dt) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_push_drive(pid: *mut Pid3, i: *const PushDrive, dt: f32, o: *mut PushDriveOut) {
        // SAFETY: caller contract.
        unsafe { sg::push_drive(rw(pid), rd(i), dt, rw(o)) }
    }
);

ffi!(
    /// # Safety
    /// `out` holds two floats.
    fn ohpl_ga2b_push_pick(
        normal_move: bool,
        running: bool,
        crouching: bool,
        walk: *const f32,
        run: *const f32,
        crouch: *const f32,
        out: *mut f32,
    ) {
        // SAFETY: each of `walk`, `run`, `crouch`, `out` holds two floats.
        unsafe {
            let pair = |p: *const f32| (*p, *p.add(1));
            let (s, f) = sg::push_pick(
                normal_move,
                running,
                crouching,
                pair(walk),
                pair(run),
                pair(crouch),
            );
            *out = s;
            *out.add(1) = f;
        }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_slide_update(
        i: *const SlideIn,
        speed: *mut f32,
        pid: *mut Pid3,
        dt: f32,
        o: *mut SlideOut,
    ) {
        // SAFETY: caller contract.
        unsafe { sg::slide_update(rd(i), rw(speed), rw(pid), dt, rw(o)) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_slide_throw(
        pin: *const Vector3f,
        cam_fwd: *const Vector3f,
        impulse: f32,
        out: *mut Vector3f,
    ) {
        // SAFETY: caller contract.
        unsafe { *rw(out) = sg::slide_throw(*rd(pin), *rd(cam_fwd), impulse) }
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_rotate_max_distance(feet: *const Vector3f, pivot: *const Vector3f) -> f32 {
        // SAFETY: caller contract.
        unsafe { sr::rotate_max_distance(*rd(feet), *rd(pivot)) }
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_rotate_far(feet: *const Vector3f, pivot: *const Vector3f, max: f32) -> bool {
        // SAFETY: caller contract.
        unsafe { sr::rotate_far(*rd(feet), *rd(pivot), max) }
    }
);

ffi!(
    /// # Safety
    /// `speed` is valid and exclusively borrowed.
    fn ohpl_ga2b_rotate_slow(speed: *mut f32, mouse: *const Vector2f, slow_factor: f32, dt: f32) {
        // SAFETY: caller contract.
        unsafe { sr::rotate_slow(rw(speed), *rd(mouse), slow_factor, dt) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_rotate_torque(
        i: *const RotateTorqueIn,
        speed: *mut f32,
        pid: *mut Pid3,
        dt: f32,
        out: *mut Vector3f,
    ) {
        // SAFETY: caller contract.
        unsafe { *rw(out) = sr::rotate_torque(rd(i), rw(speed), rw(pid), dt) }
    }
);

ffi!(
    /// # Safety
    /// `g` is readable.
    fn ohpl_ga2b_lever_speed_add(g: *const JointGeom) -> f32 {
        // SAFETY: caller contract.
        sr::lever_speed_add(unsafe { rd(g) })
    }
);

ffi!(
    /// # Safety
    /// `g` is readable.
    fn ohpl_ga2b_swing_door_speed_add(g: *const JointGeom) -> f32 {
        // SAFETY: caller contract.
        sr::swing_door_speed_add(unsafe { rd(g) })
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_swing_door_throw(
        g: *const JointGeom,
        impulse: f32,
        joint_forward: *mut Vector3f,
        out: *mut Vector3f,
    ) {
        // SAFETY: caller contract.
        unsafe {
            let (jf, v) = sr::swing_door_throw(rd(g), impulse);
            *rw(joint_forward) = jf;
            *rw(out) = v;
        }
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_wheel_pop(size: usize) -> bool {
        sr::wheel_pop(size)
    }
);

ffi!(
    /// # Safety
    /// `hist` holds `n` vectors.
    fn ohpl_ga2b_wheel_speed_add(
        hist: *const Vector2f,
        n: usize,
        mouse: *const Vector2f,
        pin: *const Vector3f,
        forward: *const Vector3f,
    ) -> f32 {
        let h: &[Vector2f] = if n == 0 {
            &[]
        } else {
            // SAFETY: caller contract.
            unsafe { std::slice::from_raw_parts(hist, n) }
        };
        // SAFETY: caller contract.
        unsafe { sr::wheel_speed_add(h, *rd(mouse), *rd(pin), *rd(forward)) }
    }
);

ffi!(
    /// # Safety
    /// `mouse` is valid and exclusively borrowed.
    fn ohpl_ga2b_move_to_mouse(forward: bool, mul: f32, factor: f32, mouse: *mut Vector2f) {
        // SAFETY: caller contract.
        unsafe { sr::move_to_mouse(forward, mul, factor, rw(mouse)) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_ladder_enter(i: *const LadderEnterIn, o: *mut LadderEnterOut) {
        // SAFETY: caller contract.
        unsafe { sl::ladder_enter(rd(i), rw(o)) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_ladder_attach(s: *mut LadderState, dt: f32, o: *mut LadderAttachOut) {
        // SAFETY: caller contract.
        unsafe { sl::ladder_attach(rw(s), dt, rw(o)) }
    }
);

ffi!(
    /// # Safety
    /// All pointers are valid.
    fn ohpl_ga2b_ladder_move_pre(
        s: *mut LadderState,
        pos: *const Vector3f,
        up: f32,
        down: f32,
        dt: f32,
    ) -> i32 {
        // SAFETY: caller contract.
        unsafe { sl::ladder_move_pre(rw(s), *rd(pos), up, down, dt) }
    }
);

ffi!(
    /// # Safety
    /// `s` is valid and exclusively borrowed.
    fn ohpl_ga2b_ladder_move_post(
        s: *mut LadderState,
        up: f32,
        down: f32,
        step_len: f32,
        dt: f32,
    ) -> i32 {
        // SAFETY: caller contract.
        unsafe { sl::ladder_move_post(rw(s), up, down, step_len, dt) }
    }
);

ffi!(
    /// # Safety
    /// `out` holds four vectors.
    fn ohpl_ga2b_ladder_ground_rays(
        feet: *const Vector3f,
        ladder_fwd: *const Vector3f,
        size_x: f32,
        out: *mut Vector3f,
    ) {
        // SAFETY: caller contract.
        unsafe {
            for (k, v) in sl::ladder_ground_rays(*rd(feet), *rd(ladder_fwd), size_x)
                .into_iter()
                .enumerate()
            {
                *out.add(k) = v;
            }
        }
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_ladder_at_top(move_mul: f32, y: f32, size_y: f32, max_y: f32) -> bool {
        sl::ladder_at_top(move_mul, y, size_y, max_y)
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_ladder_at_bottom(move_mul: f32, y: f32, size_y: f32, min_y: f32) -> i32 {
        sl::ladder_at_bottom(move_mul, y, size_y, min_y)
    }
);

ffi!(
    /// # Safety
    /// `out` holds two vectors.
    fn ohpl_ga2b_ladder_top_rays(feet: *const Vector3f, out: *mut Vector3f) {
        // SAFETY: caller contract.
        unsafe {
            for (k, v) in sl::ladder_top_rays(*rd(feet)).into_iter().enumerate() {
                *out.add(k) = v;
            }
        }
    }
);

ffi!(
    /// # Safety
    /// `count` is valid and exclusively borrowed.
    fn ohpl_ga2b_ladder_leave_tick(count: *mut f32, dt: f32) -> bool {
        // SAFETY: caller contract.
        sl::ladder_leave_tick(unsafe { rw(count) }, dt)
    }
);

ffi!(
    /// # Safety
    /// `move_mul` is valid and exclusively borrowed.
    fn ohpl_ga2b_ladder_on_move(state: i32, right: bool, mul: f32, move_mul: *mut f32) {
        // SAFETY: caller contract.
        sl::ladder_on_move(state, right, mul, unsafe { rw(move_mul) });
    }
);

ffi!(
    /// # Safety
    /// Plain values.
    fn ohpl_ga2b_ladder_camera_free(state: i32) -> bool {
        sl::ladder_camera_free(state)
    }
);

ffi!(
    /// # Safety
    /// `out` holds four floats.
    fn ohpl_ga2b_ladder_head_limits(yaw: f32, pitch: f32, out: *mut f32) {
        // SAFETY: caller contract.
        unsafe {
            for (k, v) in sl::ladder_head_limits(yaw, pitch).into_iter().enumerate() {
                *out.add(k) = v;
            }
        }
    }
);
