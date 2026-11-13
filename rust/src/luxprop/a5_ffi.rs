//! C ABI of the A5 small props, mirrored in `amnesia/src/game/RustLuxPropA5.h`.
#![allow(clippy::missing_safety_doc)]

use super::a5_small::{self as s, A5Button, A5Door, A5Oil, A5Photocell, OilHost, PhotoHost};
use super::ffi::{Sink, VarsRef};
use super::ops::{Op, Ops, Pod};
use crate::types::Vector3f;
use std::ffi::c_void;

fn with_ops<R>(sink: &Sink, f: impl FnOnce(&mut Ops) -> R) -> R {
    let (ctx, push) = (sink.ctx, sink.push);
    let mut fwd = move |op: &Op| {
        // SAFETY: the sink stays valid for the call.
        unsafe { push(ctx, op) }
    };
    f(&mut Ops(&mut fwd))
}

fn rd<'a, T>(p: *const T) -> &'a T {
    // SAFETY: caller contract.
    unsafe { &*p }
}

fn rw<'a, T>(p: *mut T) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut *p }
}

fn pod<T>(p: *mut T) -> Pod<T> {
    // SAFETY: caller contract: `p` is live state.
    unsafe { Pod::from_ptr(p) }
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<A5Button>() == 3);
    assert!(size_of::<A5Photocell>() == 20);
    assert!(size_of::<A5Door>() == 8);
    assert!(size_of::<A5Oil>() == 8);
};

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_button_init(st: *mut A5Button) {
    *rw(st) = A5Button::new();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_button_load_instance(
    v: *const VarsRef,
    st: *mut A5Button,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        s::button_load_instance(rd::<VarsRef>(v), pod(st), ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_button_can_interact(st: *const A5Button) -> bool {
    s::button_can_interact(rd(st))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_button_interact(st: *mut A5Button, sink: *const Sink) {
    with_ops(rd(sink), |ops| s::button_interact(pod(st), ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_button_set_on(
    st: *mut A5Button,
    on: bool,
    effects: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| s::button_set_on(pod(st), on, effects, ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_button_conn(st: *mut A5Button, state: i32, sink: *const Sink) {
    with_ops(rd(sink), |ops| s::button_conn(pod(st), state, ops));
}

#[repr(C)]
pub struct PhotoHostC {
    pub ctx: *mut c_void,
    pub bounds: unsafe extern "C" fn(*mut c_void, *mut Vector3f, *mut Vector3f),
    pub light_at: unsafe extern "C" fn(*mut c_void, *const Vector3f) -> f32,
    pub illumination: unsafe extern "C" fn(*mut c_void, *mut f32) -> bool,
}

impl PhotoHost for &PhotoHostC {
    fn bounds(&mut self) -> (Vector3f, Vector3f) {
        let (mut size, mut center) = (Vector3f::default(), Vector3f::default());
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.bounds)(self.ctx, &mut size, &mut center) };
        (size, center)
    }

    fn light_at(&mut self, pos: Vector3f) -> f32 {
        // SAFETY: as above.
        unsafe { (self.light_at)(self.ctx, &pos) }
    }

    fn illumination(&mut self) -> Option<f32> {
        let mut cur = 0.0;
        // SAFETY: as above.
        unsafe { (self.illumination)(self.ctx, &mut cur) }.then_some(cur)
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_photocell_load_instance(v: *const VarsRef, st: *mut A5Photocell) {
    s::photocell_load_instance(rd::<VarsRef>(v), rw(st));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_photocell_update(
    st: *mut A5Photocell,
    dt: f32,
    host: *const PhotoHostC,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        s::photocell_update(pod(st), dt, &mut rd(host), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_door_load(v: *const VarsRef) {
    s::door_load(rd::<VarsRef>(v));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_door_load_instance(v: *const VarsRef, st: *mut A5Door) {
    s::door_load_instance(rd::<VarsRef>(v), rw(st));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_door_update(st: *mut A5Door, dt: f32) {
    s::door_update(rw(st), dt);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_door_interact(st: *mut A5Door, sink: *const Sink) {
    with_ops(rd(sink), |ops| s::door_interact(pod(st), ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_oil_load(v: *const VarsRef) {
    s::oil_load(rd::<VarsRef>(v));
}

#[repr(C)]
pub struct OilHostC {
    pub ctx: *mut c_void,
    pub hard_mode: unsafe extern "C" fn(*mut c_void) -> bool,
    pub lamp_oil: unsafe extern "C" fn(*mut c_void) -> f32,
    pub total_oil: unsafe extern "C" fn(*mut c_void) -> f32,
}

impl OilHost for &OilHostC {
    fn hard_mode(&mut self) -> bool {
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.hard_mode)(self.ctx) }
    }

    fn lamp_oil(&mut self) -> f32 {
        // SAFETY: as above.
        unsafe { (self.lamp_oil)(self.ctx) }
    }

    fn total_oil(&mut self) -> f32 {
        // SAFETY: as above.
        unsafe { (self.total_oil)(self.ctx) }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_oil_focus_tier(st: *mut A5Oil, host: *const OilHostC) -> i32 {
    s::oil_focus_tier(rw(st), &mut rd(host))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5_oil_interact(
    st: *mut A5Oil,
    host: *const OilHostC,
    sink: *const Sink,
) -> bool {
    with_ops(rd(sink), |ops| s::oil_interact(rw(st), &mut rd(host), ops))
}
