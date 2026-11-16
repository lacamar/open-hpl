//! Lean and HUD effect against the per-frame lines of the C++ `OHPL_A2B_DUMP` run on Amnesia 02_library
//! (`tests/golden/A2b2/lean_hud.txt`, fixed 1/60 step, frames 1..700), and the spawn particle systems
//! against hand-worked recycle cases.

use super::a2b_helpers::{HostB, bq};
use super::a2c_helpers::{
    Hud, Lean, Sps, SpsCfg, SpsHost, cf, cq, hud_add_splash, hud_flash, hud_update, lean_add,
    lean_set, lean_update,
};
use crate::types::Vector3f;
use std::cell::Cell;
use std::ffi::c_void;

const DT: f32 = 1.0 / 60.0;

struct Fake {
    dead: Cell<bool>,
}

extern "C" fn fake_query(ctx: *mut c_void, k: u32, a: f32, _b: f32) -> f32 {
    // SAFETY: `ctx` is the `Fake` of the running test.
    let f = unsafe { &*ctx.cast::<Fake>() };
    match k {
        bq::DEAD => f32::from(u8::from(f.dead.get())),
        cq::HUD_IMAGES => 1.0,
        bq::RAND => a,
        _ => 0.0,
    }
}

extern "C" fn fake_op(_: *mut c_void, _: u32, _: i32, _: f32, _: f32, _: f32, _: f32) {}

extern "C" fn fake_fill(_: *mut c_void, k: u32, buf: *mut f32) {
    let v = if k == cf::HUD_IMAGE_SIZE {
        [64.0, 64.0]
    } else {
        [800.0, 600.0]
    };
    // SAFETY: the driver passes a two-float buffer.
    unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), buf, 2) };
}

fn hex(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

#[test]
fn lean_and_hud_follow_the_cpp_dump() {
    let golden = include_str!("../../tests/golden/A2b2/lean_hud.txt");
    let mut lines = golden.lines();
    let fake = Fake {
        dead: Cell::new(false),
    };
    let host = HostB {
        ctx: (&raw const fake).cast_mut().cast(),
        op: fake_op,
        query: fake_query,
        fill: fake_fill,
    };
    let mut lean = Lean {
        max_movement: 0.5,
        max_rotation: crate::math::to_rad(15.0),
        ..Lean::default()
    };
    let mut hud = Hud::default();
    for frame in 1..=700 {
        fake.dead.set(frame >= 300);
        if (200..230).contains(&frame) {
            lean_set(&mut lean, 1.0);
        }
        if (235..250).contains(&frame) {
            lean_add(&mut lean, -0.1);
        }
        if frame == 90 {
            hud_flash(&mut hud, 0.2, 0.5);
        }
        if let Some(ty) = [(140, 0), (160, 1), (175, 2), (300, 0)]
            .iter()
            .find_map(|&(f, ty)| (f == frame).then_some(ty))
        {
            // SAFETY: `hud` is exclusively ours and the host is the fake above.
            unsafe { hud_add_splash(&raw mut hud, &host, ty) };
            hud_flash(&mut hud, 0.0, 0.25);
        }
        // SAFETY: as above.
        unsafe { lean_update(&raw mut lean, &host, DT) };
        hud_update(&mut hud, DT);

        let want_lean = lines.next().unwrap_or_default();
        let got_lean = format!(
            "lean dir={} add={} mov={} rot={} press={}",
            hex(lean.dir),
            hex(lean.dir_add),
            hex(lean.movement),
            hex(lean.rotation),
            lean.pressed
        );
        assert_eq!(got_lean, want_lean, "frame {frame}");

        let want_hud = lines.next().unwrap_or_default();
        let mut got_hud = format!("hud splashes={}", hud.count);
        for s in hud.splashes.iter().take(hud.count as usize) {
            got_hud += &format!(" a={}", hex(s.alpha));
        }
        got_hud += &format!(
            " fa={} fact={} fsp={}",
            hex(hud.flash_alpha),
            hud.flash_active,
            hex(hud.flash_alpha_speed)
        );
        assert_eq!(got_hud, want_hud, "frame {frame}");
    }
}

struct SpsFake {
    next: usize,
    feet: Vector3f,
    killed: usize,
}

fn sps_fake(ctx: *mut c_void) -> &'static mut SpsFake {
    // SAFETY: `ctx` is the `SpsFake` of the running test, used from one thread.
    unsafe { &mut *ctx.cast::<SpsFake>() }
}

extern "C" fn sps_create(ctx: *mut c_void, _: *const Vector3f) -> usize {
    let f = sps_fake(ctx);
    f.next += 1;
    f.next
}

extern "C" fn sps_exists(_: *mut c_void, _: usize) -> bool {
    true
}

extern "C" fn sps_kill(ctx: *mut c_void, _: usize) {
    sps_fake(ctx).killed += 1;
}

extern "C" fn sps_rand(_: *mut c_void, min: f32, _: f32) -> f32 {
    min
}

extern "C" fn sps_feet(ctx: *mut c_void, out: *mut Vector3f) {
    // SAFETY: `out` is writable.
    unsafe { *out = sps_fake(ctx).feet };
}

#[test]
fn spawn_ps_recycles_to_the_far_side() {
    let mut fake = SpsFake {
        next: 0,
        feet: Vector3f::default(),
        killed: 0,
    };
    let host = SpsHost {
        ctx: (&raw mut fake).cast(),
        create: sps_create,
        exists: sps_exists,
        kill: sps_kill,
        destroy: sps_kill,
        rand: sps_rand,
        feet: sps_feet,
    };
    let mut sps = Sps::default();
    sps.start(
        &host,
        SpsCfg {
            radius: 2.0,
            density: 1.0,
            height_from_feet: 1.0,
            height_add_min: 0.0,
            height_add_max: 0.0,
        },
    );
    assert_eq!(sps.count(), 16);
    sps.update(&host);
    assert_eq!(fake.killed, 0);

    fake.feet.x = 1.5;
    sps.update(&host);
    assert_eq!(sps.count(), 16);
    assert_eq!(fake.killed, 8);
    sps.update(&host);
    assert_eq!(fake.killed, 8);

    sps.set_active(false);
    fake.feet.x = 30.0;
    sps.update(&host);
    assert_eq!(fake.killed, 8);
}
