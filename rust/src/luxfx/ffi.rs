//! C ABI of the effect, insanity, music and log ports, mirrored in
//! `amnesia/src/game/RustLuxFx.h`.

use super::effects::{Amount, Emotion, Fade, Flash, Shake};
use super::insanity::{self, Bug, BugsParams, Pick, Steps, Stream, Timing};
use super::misc::{self, Completion, Date, Vitals};
use super::music::{self, Enemy, PlayOut, Slot, StopOut};
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
    assert!(size_of::<Bug>() == 32 && offset_of!(Bug, pos) == 4);
    assert!(size_of::<BugsParams>() == 24);
    assert!(size_of::<Stream>() == 20);
    assert!(size_of::<Steps>() == 32);
    assert!(size_of::<Timing>() == 24);
    assert!(size_of::<Pick>() == 8);
    assert!(size_of::<Slot>() == 16);
    assert!(size_of::<PlayOut>() == 16);
    assert!(size_of::<StopOut>() == 12);
    assert!(size_of::<Enemy>() == 36);
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
    /// `out` has room for `rows` (at least 1) elements.
    fn ohpl_ga9b_stream_rows(rows: usize, out: *mut Row) -> usize {
        copy_out(&voice::stream_rows(rows), out, rows.max(1))
    }

    /// # Safety
    /// `bugs` holds `n` elements, `p` valid.
    fn ohpl_ga9b_bugs_start(bugs: *mut Bug, n: usize, p: *const BugsParams, min_x: f32, min_y: f32, max_x: f32, max_y: f32) {
        // SAFETY: caller contract.
        insanity::bugs_start(unsafe { slice_mut(bugs, n) }, unsafe { rd(p) }, [min_x, min_y], [max_x, max_y]);
    }

    /// # Safety
    /// `bugs` holds `total` elements, `p` and `time` valid.
    fn ohpl_ga9b_bugs_update(bugs: *mut Bug, total: usize, count: i32, p: *const BugsParams, dt: f32, time: *mut f32) -> bool {
        let count = usize::try_from(count).unwrap_or(0);
        // SAFETY: caller contract.
        insanity::bugs_update(unsafe { slice_mut(bugs, total) }, count, unsafe { rd(p) }, dt, unsafe { rw(time) })
    }

    /// # Safety
    /// `time` valid.
    fn ohpl_ga9b_timer_expired(time: *mut f32, dt: f32) -> bool {
        // SAFETY: caller contract.
        insanity::timer_expired(unsafe { rw(time) }, dt)
    }

    /// # Safety
    /// `out` holds 3 floats: delay, fade in speed, fade out speed.
    fn ohpl_ga9b_stream_load(delay: f32, fade_in: f32, fade_out: f32, out: *mut f32) {
        let (d, i, o) = insanity::stream_load(delay, fade_in, fade_out);
        // SAFETY: caller contract.
        for (to, v) in unsafe { slice_mut(out, 3) }.iter_mut().zip([d, i, o]) {
            *to = v;
        }
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_stream_a(s: *mut Stream, entry_null: bool, dt: f32) -> bool {
        // SAFETY: caller contract.
        insanity::stream_a(unsafe { rw(s) }, entry_null, dt)
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_stream_b(s: *mut Stream, entry_gone: bool, dt: f32) -> bool {
        // SAFETY: caller contract.
        insanity::stream_b(unsafe { rw(s) }, entry_gone, dt)
    }

    /// # Safety
    /// `s` valid.
    fn ohpl_ga9b_steps_start(s: *mut Steps, step_time: f32, step_num: i32, distance: f32) {
        // SAFETY: caller contract.
        insanity::steps_start(unsafe { rw(s) }, step_time, step_num, distance);
    }

    /// Returns the event-over flag; `create` is set when a step sound is due.
    ///
    /// # Safety
    /// `s` and `create` valid.
    fn ohpl_ga9b_steps_update(s: *mut Steps, dt: f32, create: *mut bool) -> bool {
        // SAFETY: caller contract.
        let (made, done) = insanity::steps_update(unsafe { rw(s) }, dt);
        // SAFETY: caller contract.
        *unsafe { rw(create) } = made;
        done
    }

    /// # Safety
    /// All three arrays hold `n` elements; `used` is writable.
    fn ohpl_ga9b_insanity_pick(sanity: f32, n: usize, set_disabled: *const u32, used: *mut u32, max_sanity: *const f32) -> Pick {
        // SAFETY: caller contract.
        insanity::pick(sanity, unsafe { slice(set_disabled, n) }, unsafe { slice_mut(used, n) }, unsafe { slice(max_sanity, n) })
    }

    /// # Safety
    /// `t` valid.
    fn ohpl_ga9b_insanity_wait_after(t: *const Timing, sanity: f32) -> f32 {
        // SAFETY: caller contract.
        insanity::wait_after(unsafe { rd(t) }, sanity)
    }

    /// # Safety
    /// Plain values.
    fn ohpl_ga9b_insanity_count_down(count: f32, current: i32, dt: f32) -> f32 {
        insanity::count_down(count, current, dt)
    }

    /// # Safety
    /// `t` valid.
    fn ohpl_ga9b_insanity_possible(t: *const Timing, count: f32, sanity: f32, current: i32) -> bool {
        // SAFETY: caller contract.
        insanity::new_event_possible(unsafe { rd(t) }, count, sanity, current)
    }

    /// # Safety
    /// Plain values.
    fn ohpl_ga9b_music_prio(p: i32, max: i32) -> i32 {
        music::prio(p, max)
    }

    /// # Safety
    /// `out` valid.
    fn ohpl_ga9b_music_play(cur: i32, p: i32, same_file: bool, volume: f32, volume_mul: f32, fade_time: f32, out: *mut PlayOut) -> bool {
        // SAFETY: caller contract.
        music::play(cur, p, same_file, volume, volume_mul, fade_time, unsafe { rw(out) })
    }

    /// # Safety
    /// Plain values.
    fn ohpl_ga9b_music_stop(cur: i32, p: i32, has_file: bool, slot_volume: f32, fade_time: f32) -> StopOut {
        music::stop(cur, p, has_file, slot_volume, fade_time)
    }

    /// # Safety
    /// `slots` holds `n` elements, `o` writable.
    fn ohpl_ga9b_music_play_highest(slots: *const Slot, n: usize, max_prio: i32, o: *mut Ops) -> i32 {
        // SAFETY: caller contract.
        music::play_highest(unsafe { slice(slots, n) }, max_prio, fresh(o))
    }

    /// # Safety
    /// Plain values.
    fn ohpl_ga9b_music_song_over(cur: i32, no_song: bool) -> bool {
        music::song_over(cur, no_song)
    }

    /// # Safety
    /// Both arrays hold `n` elements.
    fn ohpl_ga9b_music_pick_danger(nonempty: *const u32, prios: *const i32, n: usize) -> i32 {
        // SAFETY: caller contract.
        music::pick_danger(unsafe { slice(nonempty, n) }, unsafe { slice(prios, n) })
    }

    /// # Safety
    /// Both arrays hold `n` elements.
    fn ohpl_ga9b_music_pick_enemy(nonempty: *const u32, prios: *const i32, n: usize) -> i32 {
        // SAFETY: caller contract.
        music::pick_enemy(unsafe { slice(nonempty, n) }, unsafe { slice(prios, n) })
    }

    /// # Safety
    /// `e` valid.
    fn ohpl_ga9b_music_danger_begin(hard: bool, e: *const Enemy, has_map: bool) -> i32 {
        // SAFETY: caller contract.
        music::danger_begin(hard, unsafe { rd(e) }, has_map)
    }

    /// # Safety
    /// `e` valid, `o` writable.
    fn ohpl_ga9b_music_danger_idle(e: *mut Enemy, found: bool, dt: f32, o: *mut Ops) {
        // SAFETY: caller contract.
        music::danger_idle(unsafe { rw(e) }, found, dt, fresh(o));
    }

    /// # Safety
    /// `e` valid, `o` writable.
    fn ohpl_ga9b_music_danger_playing(e: *mut Enemy, found: bool, dt: f32, o: *mut Ops) {
        // SAFETY: caller contract.
        music::danger_playing(unsafe { rw(e) }, found, dt, fresh(o));
    }

    /// # Safety
    /// `e` valid, `o` writable.
    fn ohpl_ga9b_music_enemy_begin(e: *mut Enemy, t: i32, has_map: bool, set_empty: bool, dt: f32, o: *mut Ops) -> i32 {
        let t = usize::try_from(t).unwrap_or(usize::MAX);
        // SAFETY: caller contract.
        music::enemy_begin(unsafe { rw(e) }, t, has_map, set_empty, dt, fresh(o))
    }

    /// # Safety
    /// `e` valid, `o` writable.
    fn ohpl_ga9b_music_enemy_found(e: *mut Enemy, t: i32, o: *mut Ops) {
        let t = usize::try_from(t).unwrap_or(usize::MAX);
        // SAFETY: caller contract.
        music::enemy_found(unsafe { rw(e) }, t, fresh(o));
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
