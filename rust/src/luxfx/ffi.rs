//! C ABI of the effect, insanity, music and log ports, mirrored in
//! `amnesia/src/game/RustLuxFx.h`.

use super::effects::{Amount, Emotion, Fade, Flash, Shake};
use super::misc::{self, Completion, Date, Vitals};
use super::ops::{Op, Ops};
use super::postfx::{self, Clock};
use super::renderer::{self, Glow, Outline};
use super::voice::{self, AddIn, Row, TextsIn, UpdateIn};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Op>() == 24 && offset_of!(Op, a) == 8);
    assert!(size_of::<Ops>() == 4 + 24 * 24 && offset_of!(Ops, ops) == 4);
    assert!(size_of::<Amount>() == 16);
    assert!(size_of::<Fade>() == 16);
    assert!(size_of::<Flash>() == 28);
    assert!(size_of::<Emotion>() == 24);
    assert!(size_of::<Shake>() == 28);
    assert!(size_of::<AddIn>() == 28);
    assert!(size_of::<UpdateIn>() == 24);
    assert!(size_of::<TextsIn>() == 44);
    assert!(size_of::<Row>() == 20);
    assert!(size_of::<Glow>() == 8);
    assert!(size_of::<Outline>() == 132);
    assert!(size_of::<renderer::Op>() == 76);
    assert!(size_of::<postfx::Op>() == 20);
    assert!(size_of::<Clock>() == 20);
    assert!(size_of::<Vitals>() == 20);
    assert!(size_of::<Date>() == 24);
    assert!(size_of::<Completion>() == 24);
};

/// # Safety
/// `p` is valid and exclusively borrowed.
unsafe fn rw<'a, T>(p: *mut T) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut *p }
}

/// # Safety
/// `p` points to `n` writable elements, or `n` is 0.
unsafe fn slice_mut<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if n == 0 {
        return &mut [];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts_mut(p, n) }
}

fn copy_out<T: Copy>(src: &[T], dst: *mut T, cap: usize) -> usize {
    // SAFETY: caller contract: `dst` has room for `cap` elements.
    let d = unsafe { slice_mut(dst, cap) };
    for (to, from) in d.iter_mut().zip(src.iter()) {
        *to = *from;
    }
    src.len()
}

macro_rules! ffi {
    ($($(#[$m:meta])* fn $name:ident($($a:ident: $t:ty),*) $(-> $r:ty)? $body:block)*) => {
        $(
            $(#[$m])*
            #[unsafe(no_mangle)]
            pub unsafe extern "C" fn $name($($a: $t),*) $(-> $r)? $body
        )*
    };
}

ffi! {
    /// # Safety
    /// `out` has room for `rows` (at least 1) elements.
    fn ohpl_ga9b_stream_rows(rows: usize, out: *mut Row) -> usize {
        copy_out(&voice::stream_rows(rows), out, rows.max(1))
    }

    /// # Safety
    /// `c` valid.
    fn ohpl_ga9b_completion_reset(c: *mut Completion) {
        // SAFETY: caller contract.
        misc::completion_reset(unsafe { rw(c) });
    }

    /// # Safety
    /// `c` valid.
    fn ohpl_ga9b_completion_update(c: *mut Completion, dt: f32) {
        // SAFETY: caller contract.
        misc::completion_update(unsafe { rw(c) }, dt);
    }

    /// # Safety
    /// `c` valid.
    fn ohpl_ga9b_completion_show(c: *mut Completion, previous: f32, new: f32, delay: f32) {
        // SAFETY: caller contract.
        misc::completion_show(unsafe { rw(c) }, previous, new, delay);
    }

    /// # Safety
    /// Plain values.
    fn ohpl_ga9b_achievement_unlock_gate(registered: bool, known: bool) -> i32 {
        misc::unlock_gate(registered, known)
    }
}
