use super::a2_core::{Core, Host};
use super::a2d_disp::{
    DHost, Disp, k, ohpl_a2d_free, ohpl_a2d_init, ohpl_a2d_reset, ohpl_a2d_update,
};
use std::cell::RefCell;
use std::ffi::c_void;

thread_local! { static LOG: RefCell<Vec<u32>> = const { RefCell::new(Vec::new()) }; }

extern "C" fn rec(_: *mut c_void, kind: u32, _: i32, a: f32, _: f32, _: *mut c_void) -> f32 {
    LOG.with(|l| l.borrow_mut().push(kind));
    match kind {
        k::BODY_SPEED => a * 120.0,
        k::BODY_Y => 5.0,
        _ => 0.0,
    }
}

extern "C" fn aop(_: *mut c_void, _: u32, _: i32, _: f32, _: f32, _: f32, _: f32) {}

extern "C" fn aq(_: *mut c_void, _: u32) -> f32 {
    0.0
}

fn setup() -> (Core, Disp) {
    // SAFETY: all-zero is valid for both (plain numbers, null ring).
    let (c, mut d): (Core, Disp) = unsafe { (std::mem::zeroed(), std::mem::zeroed()) };
    // SAFETY: `d` valid.
    unsafe { ohpl_a2d_init(&mut d, 3, -10.0) };
    (c, d)
}

#[test]
fn update_order_and_speed_window() {
    let (mut c, mut d) = setup();
    let dh = DHost {
        ctx: std::ptr::null_mut(),
        call: rec,
    };
    let h = Host {
        ctx: std::ptr::null_mut(),
        op: aop,
        query: aq,
    };
    LOG.with(|l| l.borrow_mut().clear());
    // SAFETY: valid pointers; the ring is freed at the end.
    unsafe {
        ohpl_a2d_update(&mut c, &mut d, &h, &dh, 0.5);
        let kinds = LOG.with(|l| l.borrow().clone());
        assert_eq!(
            kinds,
            [
                k::MS_UPDATE,
                k::ST_UPDATE,
                k::HEAD_POS_ADD,
                k::HELPERS_UPDATE,
                k::BODY_Y,
                k::COLLISION_CB,
                k::BODY_SPEED
            ]
        );
        assert_eq!(d.avg_speed, 60.0);
        for dt in [1.0, 1.5, 2.0] {
            ohpl_a2d_update(&mut c, &mut d, &h, &dh, dt);
        }
        // max 3: the fourth sample drops the oldest, leaving 120, 180, 240
        assert_eq!(d.avg_speed, 180.0);
        ohpl_a2d_free(&mut d);
    }
}

#[test]
fn reset_values() {
    let (mut c, mut d) = setup();
    let dh = DHost {
        ctx: std::ptr::null_mut(),
        call: rec,
    };
    d.coins = 7;
    d.free_cam = true;
    // SAFETY: valid pointers.
    unsafe {
        ohpl_a2d_reset(&mut c, &mut d, &dh);
        ohpl_a2d_free(&mut d);
    }
    assert_eq!(
        (d.coins, d.free_cam, d.active, d.free_cam_speed),
        (0, false, true, 0.1)
    );
    assert_eq!(
        (c.health, c.sanity, c.lamp_oil, c.cam.fov_mul),
        (100.0, 100.0, 100.0, 1.0)
    );
    assert!(d.script_show_focus && d.look_speed_mul == 1.0);
}
