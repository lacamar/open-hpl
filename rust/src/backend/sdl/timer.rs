//! `cTimerSDL`: wall-clock microseconds from `gettimeofday`.

use super::clib::{gettimeofday, timeval};

/// `sec * 1e6 + usec` as the C++ double expression (gcc fuses it on aarch64).
pub fn micros(sec: i64, usec: i64) -> f64 {
    (sec as f64).mul_add(1_000_000.0, usec as f64)
}

fn now() -> (i64, i64) {
    let mut tv = timeval {
        tv_sec: 0,
        tv_usec: 0,
    };
    // SAFETY: `tv` is a valid out pointer; the timezone argument may be null.
    unsafe { gettimeofday(&mut tv, core::ptr::null_mut()) };
    (tv.tv_sec, tv.tv_usec)
}

#[derive(Default)]
pub struct Timer {
    start: (i64, i64),
    end: (i64, i64),
    stopped: bool,
}

impl Timer {
    pub fn start(&mut self) {
        self.stopped = false;
        self.start = now();
    }

    pub fn stop(&mut self) {
        self.end = now();
        self.stopped = true;
    }

    pub fn elapsed_micros(&mut self) -> f64 {
        if !self.stopped {
            self.end = now();
        }
        micros(self.end.0, self.end.1) - micros(self.start.0, self.start.1)
    }
}

/// # Safety
/// Pair with `ohpl_l1_timer_free`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_timer_new() -> *mut Timer {
    Box::into_raw(Box::default())
}

/// # Safety
/// `t` from `ohpl_l1_timer_new`, freed once (or null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_timer_free(t: *mut Timer) {
    if !t.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(t) });
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_timer_start(t: *mut Timer) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.start();
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_timer_stop(t: *mut Timer) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.stop();
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_timer_micros(t: *mut Timer) -> f64 {
    // SAFETY: caller contract.
    unsafe { t.as_mut() }.map_or(0.0, Timer::elapsed_micros)
}
