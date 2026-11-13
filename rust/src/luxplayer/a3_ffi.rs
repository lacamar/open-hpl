//! C ABI of the A3 player states, mirrored in `amnesia/src/game/RustLuxPlayerA3.h`. Every entry takes the state's
//! `Base` (the first member of each concrete POD) and the host table; the kind in `Base` picks the behaviour.

use super::a3_host::{Host, Ptr};
use super::a3_state::{Base, Grab, Kind, Ladder, Plain, Push, Rotate, Slide, UseItem};
use super::{
    a3_default as default, a3_grab as grab, a3_ladder as ladder, a3_push as push,
    a3_rotate as rotate, a3_slide as slide,
};
use crate::types::{Vector2f, Vector3f};

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Base>() == 80);
    assert!(size_of::<Plain>() == 80);
    assert!(size_of::<UseItem>() == 120);
    assert!(size_of::<Grab>() == 952);
    assert!(size_of::<Push>() == 920);
    assert!(size_of::<Slide>() == 528);
    assert!(size_of::<Rotate>() == 592);
    assert!(size_of::<Ladder>() == 208);
};

fn kind_of(b: *mut Base) -> Option<Kind> {
    default::kind(b)
}

/// # Safety
/// `b` points to a zeroed POD of the concrete state for `kind`; `h` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_init(b: *mut Base, h: *const Host, kind: i32) {
    let Some(k) = Kind::from_i32(kind) else {
        return;
    };
    // SAFETY: caller contract.
    let h = unsafe { &*h };
    // SAFETY: caller contract.
    unsafe { (*b).kind = kind };
    match k {
        Kind::UseItem => {
            let u = b.cast::<UseItem>();
            // SAFETY: caller contract.
            unsafe {
                (*u).flash.set_up(0.0, 1.0, 0.0, 1.5, 1.5);
                (*u).min_use_distance = h.cfg(c"Player_Interaction", c"MinUseItemDistance", 0.0);
            }
        }
        // SAFETY: caller contract.
        Kind::Grab => grab::init(unsafe { &mut *b.cast::<Grab>() }, h),
        // SAFETY: caller contract.
        Kind::Push => push::init(unsafe { &mut *b.cast::<Push>() }, h),
        // SAFETY: caller contract.
        Kind::Slide => slide::init(unsafe { &mut *b.cast::<Slide>() }, h),
        // SAFETY: caller contract.
        Kind::SwingDoor | Kind::Lever | Kind::Wheel => {
            rotate::init(unsafe { &mut *b.cast::<Rotate>() }, h, k)
        }
        // SAFETY: caller contract.
        Kind::Ladder => ladder::init(unsafe { &mut *b.cast::<Ladder>() }, h),
        Kind::Normal | Kind::HandObject => {}
    }
}

/// # Safety
/// `b` was initialised by `ohpl_a3_init`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_free(b: *mut Base) {
    match kind_of(b) {
        Some(Kind::Grab) => grab::free(b),
        Some(Kind::Wheel) => rotate::free(b),
        _ => {}
    }
}

/// # Safety
/// As `ohpl_a3_init`; `prev` is the previous `eLuxPlayerState`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_enter(b: *mut Base, h: *const Host, prev: i32) {
    let Some(k) = kind_of(b) else { return };
    // SAFETY: caller contract.
    let h = unsafe { &*h };
    // SAFETY: caller contract.
    unsafe {
        match k {
            Kind::Normal | Kind::HandObject | Kind::UseItem => default::enter(b, h, k, prev),
            Kind::Grab => grab::enter(b, h),
            Kind::Push => push::enter(b, h),
            Kind::Slide => slide::enter(b, h),
            Kind::SwingDoor | Kind::Lever | Kind::Wheel => rotate::enter(b, h, k),
            Kind::Ladder => ladder::enter(b, h),
        }
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_leave(b: *mut Base, h: *const Host, new_state: i32) {
    let Some(k) = kind_of(b) else { return };
    // SAFETY: caller contract.
    let h = unsafe { &*h };
    // SAFETY: caller contract.
    unsafe {
        match k {
            Kind::Normal | Kind::HandObject | Kind::UseItem => default::leave(b, h, k, new_state),
            Kind::Grab => grab::leave(b, h),
            Kind::Push => push::leave(b, h),
            Kind::Slide => slide::leave(b, h),
            Kind::SwingDoor | Kind::Lever | Kind::Wheel => rotate::leave(b, h),
            Kind::Ladder => ladder::leave(b, h),
        }
    }
}

/// # Safety
/// As `ohpl_a3_init`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_map_leave(b: *mut Base) {
    if let Some(k) = kind_of(b) {
        // SAFETY: caller contract.
        unsafe { default::map_leave(b, k) };
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_update(b: *mut Base, h: *const Host, dt: f32) {
    let Some(k) = kind_of(b) else { return };
    // SAFETY: caller contract.
    let h = unsafe { &*h };
    // SAFETY: caller contract.
    unsafe {
        match k {
            Kind::Normal | Kind::HandObject | Kind::UseItem => default::update(b, h, k, dt),
            Kind::Grab => {}
            Kind::Push => push::update(b, h, dt),
            Kind::Slide => slide::update(b, h, dt),
            Kind::SwingDoor | Kind::Lever | Kind::Wheel => rotate::update(b, h, k, dt),
            Kind::Ladder => ladder::update(b, h, dt),
        }
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_post_update(b: *mut Base, h: *const Host, dt: f32) {
    let Some(k) = kind_of(b) else { return };
    // SAFETY: caller contract.
    let h = unsafe { &*h };
    // SAFETY: caller contract.
    unsafe {
        match k {
            Kind::Grab => grab::post_update(b, h, dt),
            Kind::Push => push::post_update(b),
            _ => {}
        }
    }
}

/// # Safety
/// As `ohpl_a3_enter`; `action` is the raw `eLuxPlayerAction`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_do_action(
    b: *mut Base,
    h: *const Host,
    action: i32,
    pressed: bool,
) -> bool {
    let Some(k) = kind_of(b) else { return true };
    // SAFETY: caller contract.
    let h = unsafe { &*h };
    // SAFETY: caller contract.
    unsafe {
        match k {
            Kind::Normal | Kind::HandObject | Kind::UseItem => {
                default::do_action(b, h, k, action, pressed)
            }
            Kind::Grab => grab::do_action(b, h, action, pressed),
            Kind::Push => push::do_action(b, h, action, pressed),
            Kind::Slide => slide::do_action(b, h, action, pressed),
            Kind::SwingDoor | Kind::Lever | Kind::Wheel => {
                rotate::do_action(b, h, k, action, pressed)
            }
            Kind::Ladder => ladder::do_action(h, action, pressed),
        }
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_scroll(b: *mut Base, h: *const Host, amount: f32) {
    if kind_of(b) == Some(Kind::Grab) {
        // SAFETY: caller contract.
        unsafe { grab::scroll(b, &*h, amount) };
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_on_move(
    b: *mut Base,
    _h: *const Host,
    dir: i32,
    mul: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match kind_of(b) {
            Some(Kind::Push) => {
                push::on_move(b, dir, mul);
                false
            }
            Some(Kind::Slide) => {
                slide::on_move(b, dir, mul);
                true
            }
            Some(Kind::SwingDoor | Kind::Lever | Kind::Wheel) => {
                rotate::on_move(b, dir, mul);
                true
            }
            Some(Kind::Ladder) => {
                ladder::on_move(b, dir, mul);
                false
            }
            _ => true,
        }
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_add_yaw(b: *mut Base, h: *const Host, amount: f32) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match kind_of(b) {
            Some(Kind::Grab) => grab::add_rotation(b, &*h, amount, false),
            Some(Kind::Slide) => {
                slide::add_yaw(b, amount);
                false
            }
            Some(Kind::SwingDoor | Kind::Lever | Kind::Wheel) => {
                rotate::add_yaw(b, amount);
                false
            }
            Some(Kind::Ladder) => ladder::camera_free(b),
            _ => true,
        }
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_add_pitch(b: *mut Base, h: *const Host, amount: f32) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match kind_of(b) {
            Some(Kind::Grab) => grab::add_rotation(b, &*h, amount, true),
            Some(Kind::Slide) => {
                slide::add_pitch(b, &*h, amount);
                false
            }
            Some(Kind::SwingDoor | Kind::Lever | Kind::Wheel) => {
                rotate::add_pitch(b, &*h, amount);
                false
            }
            Some(Kind::Ladder) => ladder::camera_free(b),
            _ => true,
        }
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_on_run(b: *mut Base, _h: *const Host, _pressed: bool) -> bool {
    kind_of(b) != Some(Kind::Ladder)
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_on_crouch(b: *mut Base, _h: *const Host, _pressed: bool) -> bool {
    kind_of(b) != Some(Kind::Ladder)
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_on_jump(b: *mut Base, h: *const Host, pressed: bool) -> bool {
    match kind_of(b) {
        Some(Kind::Ladder) => {
            // SAFETY: caller contract.
            unsafe { ladder::on_jump(&*h, pressed) };
            false
        }
        Some(Kind::Push | Kind::SwingDoor | Kind::Lever | Kind::Wheel) => false,
        _ => true,
    }
}

/// # Safety
/// As `ohpl_a3_enter`. The result is a `kOhplA3Crosshair*` code or a focus gfx index.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_crosshair(b: *mut Base, h: *const Host) -> i32 {
    let Some(k) = kind_of(b) else {
        return default::CROSSHAIR_NONE;
    };
    // SAFETY: caller contract.
    unsafe { default::crosshair(b, &*h, k) }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_draw(b: *mut Base, h: *const Host, gui_set: Ptr, frame_time: f32) {
    if let Some(k) = kind_of(b) {
        // SAFETY: caller contract.
        unsafe { default::draw(b, &*h, k, gui_set, frame_time) };
    }
}

/// # Safety
/// As `ohpl_a3_enter`; `pos` and `size` are valid for reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_draw_crosshair(
    b: *mut Base,
    h: *const Host,
    gfx: Ptr,
    pos: *const Vector3f,
    size: *const Vector2f,
) -> bool {
    let Some(k) = kind_of(b) else { return true };
    // SAFETY: caller contract.
    unsafe { default::draw_crosshair(b, &*h, k, gfx, *pos, *size) }
}

/// # Safety
/// As `ohpl_a3_enter`; `mass` and `collide` are valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_save_body(
    b: *mut Base,
    body: Ptr,
    mass: *mut f32,
    collide: *mut bool,
) {
    if kind_of(b) != Some(Kind::Grab) {
        return;
    }
    if let Some((m, c)) = grab::save_body(b, body) {
        // SAFETY: caller contract.
        unsafe {
            *mass = m;
            *collide = c;
        }
    }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_allow_buoyancy(b: *mut Base, h: *const Host, body: Ptr) -> bool {
    if kind_of(b) != Some(Kind::Grab) {
        return true;
    }
    // SAFETY: caller contract.
    unsafe { grab::allow_buoyancy(b, &*h, body) }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_destroy_entity(b: *mut Base, h: *const Host, entity: Ptr) {
    if kind_of(b).is_some_and(Kind::is_interact) {
        // SAFETY: caller contract.
        unsafe { default::on_destroy_entity(b, &*h, entity) };
    }
}

/// # Safety
/// As `ohpl_a3_enter`; the result is the y of the next debug row.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_draw_debug(
    b: *mut Base,
    h: *const Host,
    gui_set: Ptr,
    font: Ptr,
    y: f32,
) -> f32 {
    let Some(k) = kind_of(b) else { return y };
    // SAFETY: caller contract.
    unsafe { super::a3_debug::draw_debug(b, &*h, k, gui_set, font, y) }
}

/// # Safety
/// As `ohpl_a3_enter`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a3_attach_sticky(b: *mut Base, h: *const Host, body: Ptr) {
    if kind_of(b).is_some_and(Kind::is_interact) {
        // SAFETY: caller contract.
        unsafe { default::on_attach_sticky(b, &*h, body) };
    }
}
