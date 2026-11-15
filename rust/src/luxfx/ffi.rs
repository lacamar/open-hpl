//! C ABI of the effect, insanity, music and log ports, mirrored in
//! `amnesia/src/game/RustLuxFx.h`.

use super::effects::{self, Amount, Emotion, Fade, Flash, Shake};
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
/// `p` is valid for reads.
unsafe fn rd<'a, T>(p: *const T) -> &'a T {
    // SAFETY: caller contract.
    unsafe { &*p }
}

/// # Safety
/// `p` points to `n` readable elements, or `n` is 0.
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 {
        return &[];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts(p, n) }
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

fn fresh(o: *mut Ops) -> &'static mut Ops {
    // SAFETY: callers pass a valid writable `Ops`.
    let o = unsafe { rw(o) };
    *o = Ops::default();
    o
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
    /// `s` valid, `o` writable.
    fn ohpl_ga9b_amount_update(s: *mut Amount, dt: f32, o: *mut Ops) {
        // SAFETY: caller contract.
        effects::amount_update(unsafe { rw(s) }, dt, fresh(o));
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_fade_in(s: *mut Fade, t: f32) {
        // SAFETY: caller contract.
        effects::fade_in(unsafe { rw(s) }, t);
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_fade_out(s: *mut Fade, t: f32) {
        // SAFETY: caller contract.
        effects::fade_out(unsafe { rw(s) }, t);
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_fade_direct(s: *mut Fade, x: f32) {
        // SAFETY: caller contract.
        effects::fade_direct(unsafe { rw(s) }, x);
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_fade_is_fading(s: *const Fade) -> bool {
        // SAFETY: caller contract.
        effects::fade_is_fading(unsafe { rd(s) })
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_fade_update(s: *mut Fade, dt: f32) {
        // SAFETY: caller contract.
        effects::fade_update(unsafe { rw(s) }, dt);
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_fade_reset(s: *mut Fade) {
        // SAFETY: caller contract.
        effects::fade_reset(unsafe { rw(s) });
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_flash_start(s: *mut Flash, a: f32, b: f32, c: f32) {
        // SAFETY: caller contract.
        effects::flash_start(unsafe { rw(s) }, a, b, c);
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_sanity_flash_start(s: *mut Flash, fade_in: f32, fade_out: f32) {
        // SAFETY: caller contract.
        effects::sanity_flash_start(unsafe { rw(s) }, fade_in, fade_out);
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_flash_reset(s: *mut Flash) {
        // SAFETY: caller contract.
        effects::flash_reset(unsafe { rw(s) });
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_flash_update(s: *mut Flash, dt: f32) {
        // SAFETY: caller contract.
        effects::flash_update(unsafe { rw(s) }, dt);
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_emotion_start(s: *mut Emotion, len: usize) {
        // SAFETY: caller contract.
        effects::emotion_start(unsafe { rw(s) }, len);
    }

    /// # Safety
    /// `s` valid, `o` writable.
    fn ohpl_ga9b_emotion_update(s: *mut Emotion, dt: f32, o: *mut Ops) {
        // SAFETY: caller contract.
        effects::emotion_update(unsafe { rw(s) }, dt, fresh(o));
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_emotion_action(s: *mut Emotion, pressed: bool) {
        // SAFETY: caller contract.
        effects::emotion_action(unsafe { rw(s) }, pressed);
    }

    /// Updates `*n` shakes in place; `*n` receives the remaining count. Returns the active flag.
    ///
    /// # Safety
    /// `shakes` holds `*n` elements, `n` valid, `o` writable.
    fn ohpl_ga9b_shake_update(shakes: *mut Shake, n: *mut u32, dt: f32, o: *mut Ops) -> bool {
        // SAFETY: caller contract.
        let (count, s) = unsafe { (rw(n), slice_mut(shakes, *rd(n) as usize)) };
        let (left, active) = effects::shake_update(s, dt, fresh(o));
        *count = left as u32;
        active
    }

    /// # Safety
    /// `out` has room for `rows` (at least 1) elements.
    fn ohpl_ga9b_voice_rows(rows: usize, row_width: f32, font_y: f32, out: *mut Row) -> usize {
        copy_out(&voice::voice_rows(rows, row_width, font_y), out, rows.max(1))
    }

    /// # Safety
    /// `out` has room for `rows` (at least 1) elements.
    fn ohpl_ga9b_stream_rows(rows: usize, out: *mut Row) -> usize {
        copy_out(&voice::stream_rows(rows), out, rows.max(1))
    }

    /// # Safety
    /// `out` has room for `rows` (at least 1) elements.
    fn ohpl_ga9b_emotion_rows(rows: usize, font_y: f32, out: *mut Row) -> usize {
        copy_out(&voice::emotion_rows(rows, font_y), out, rows.max(1))
    }

    /// # Safety
    /// `i` valid, `o` writable.
    fn ohpl_ga9b_voice_add(i: *const AddIn, o: *mut Ops) {
        // SAFETY: caller contract.
        voice::add(unsafe { rd(i) }, fresh(o));
    }

    /// # Safety
    /// `delays` holds `n` floats.
    fn ohpl_ga9b_voice_text_tick(delays: *mut f32, n: usize, dt: f32) -> usize {
        // SAFETY: caller contract.
        voice::text_tick(unsafe { slice_mut(delays, n) }, dt)
    }

    /// # Safety
    /// `i` valid, `o` writable.
    fn ohpl_ga9b_voice_update_a(i: *const UpdateIn, o: *mut Ops) -> i32 {
        // SAFETY: caller contract.
        voice::update_a(unsafe { rd(i) }, fresh(o))
    }

    /// # Safety
    /// Plain values.
    fn ohpl_ga9b_voice_start_time(current: f64, interrupted_at: f64, game_time: f64) -> f64 {
        voice::start_time(current, interrupted_at, game_time)
    }

    /// # Safety
    /// `i` valid, `o` writable.
    fn ohpl_ga9b_voice_update_b(i: *const TextsIn, o: *mut Ops) -> bool {
        // SAFETY: caller contract.
        voice::update_b(unsafe { rd(i) }, fresh(o))
    }

    /// # Safety
    /// `objs` holds `n` elements.
    fn ohpl_ga9b_plan_flash(objs: *const Glow, n: usize, osc: f32, ok: bool) -> *mut renderer::Plan {
        // SAFETY: caller contract.
        Box::into_raw(Box::new(renderer::plan_flash(unsafe { slice(objs, n) }, osc, ok)))
    }

    /// # Safety
    /// `objs` holds `n` elements.
    fn ohpl_ga9b_plan_glow(objs: *const Glow, n: usize, ok: bool) -> *mut renderer::Plan {
        // SAFETY: caller contract.
        Box::into_raw(Box::new(renderer::plan_glow(unsafe { slice(objs, n) }, ok)))
    }

    /// # Safety
    /// `objs` holds `n` elements.
    fn ohpl_ga9b_plan_outline(objs: *const Outline, n: usize, programs_ok: u32) -> *mut renderer::Plan {
        // SAFETY: caller contract.
        Box::into_raw(Box::new(renderer::plan_outline(unsafe { slice(objs, n) }, programs_ok)))
    }

    /// # Safety
    /// `p` came from a plan constructor, `out` writable.
    fn ohpl_ga9b_rplan_next(p: *mut renderer::Plan, out: *mut renderer::Op) -> bool {
        // SAFETY: caller contract.
        match unsafe { rw(p) }.next_op() {
            Some(op) => {
                // SAFETY: caller contract.
                *unsafe { rw(out) } = op;
                true
            }
            None => false,
        }
    }

    /// # Safety
    /// `p` valid, `min` and `max` hold 3 floats.
    fn ohpl_ga9b_rplan_clip(p: *const renderer::Plan, min: *mut f32, max: *mut f32) {
        // SAFETY: caller contract.
        let (plan, mn, mx) = unsafe { (rd(p), slice_mut(min, 3), slice_mut(max, 3)) };
        for (d, s) in mn.iter_mut().zip([plan.clip_min.x, plan.clip_min.y, plan.clip_min.z]) {
            *d = s;
        }
        for (d, s) in mx.iter_mut().zip([plan.clip_max.x, plan.clip_max.y, plan.clip_max.z]) {
            *d = s;
        }
    }

    /// # Safety
    /// `p` came from a plan constructor and is not used again.
    fn ohpl_ga9b_rplan_free(p: *mut renderer::Plan) {
        if !p.is_null() {
            // SAFETY: caller contract.
            drop(unsafe { Box::from_raw(p) });
        }
    }

    /// # Safety
    /// `c` valid.
    fn ohpl_ga9b_post_update(c: *mut Clock, dt: f32) {
        // SAFETY: caller contract.
        postfx::update(unsafe { rw(c) }, dt);
    }

    /// # Safety
    /// `c` valid.
    fn ohpl_ga9b_post_plan(c: *const Clock, sx: f32, sy: f32, ok: bool) -> *mut postfx::Plan {
        // SAFETY: caller contract.
        Box::into_raw(Box::new(postfx::plan(unsafe { rd(c) }, [sx, sy], ok)))
    }

    /// # Safety
    /// `p` came from `ohpl_ga9b_post_plan`, `out` writable.
    fn ohpl_ga9b_pplan_next(p: *mut postfx::Plan, out: *mut postfx::Op) -> bool {
        // SAFETY: caller contract.
        match unsafe { rw(p) }.next_op() {
            Some(op) => {
                // SAFETY: caller contract.
                *unsafe { rw(out) } = op;
                true
            }
            None => false,
        }
    }

    /// # Safety
    /// `p` came from `ohpl_ga9b_post_plan` and is not used again.
    fn ohpl_ga9b_pplan_free(p: *mut postfx::Plan) {
        if !p.is_null() {
            // SAFETY: caller contract.
            drop(unsafe { Box::from_raw(p) });
        }
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
