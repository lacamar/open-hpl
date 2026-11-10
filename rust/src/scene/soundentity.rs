//! `cSoundEntity` play/stop/fade state machine. Sound entries, the handler and the data stay in
//! C++ and are driven through `SoundHost` in the original call order, so the random number is
//! drawn at the same points.

use crate::math::rand_rectf;

pub const MAIN: i32 = 0;
pub const START: i32 = 1;
pub const STOP: i32 = 2;

/// `cSoundEntityData` values the machine branches on, read fresh by C++ for every call.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SoundCfg {
    pub interval: f32,
    pub random: f32,
    pub looping: bool,
    pub use_3d: bool,
    pub has_start: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SoundState {
    pub interval_count: f32,
    pub sleep_count: f32,
    pub stopped: bool,
    pub started: bool,
    pub skip_start_end: bool,
    pub prio_remove: bool,
    pub fading_out: bool,
    pub out_of_range: bool,
    pub remove_when_over: bool,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum PlayResult {
    Played,
    NoName,
    Failed { not_enough_channels: bool },
}

pub trait SoundHost {
    fn silent(&mut self) -> bool;
    fn has_entry(&mut self, t: i32) -> bool;
    fn is_valid(&mut self, t: i32) -> bool;
    fn clear_entry(&mut self, t: i32);
    fn stop_entry(&mut self, t: i32);
    fn fade_in(&mut self, t: i32, speed: f32);
    fn fade_out(&mut self, t: i32, speed: f32);
    fn update_position(&mut self, t: i32);
    fn play_sound(&mut self, t: i32, looping: bool) -> PlayResult;
    fn on_start(&mut self);
    fn emitter(&mut self) -> bool;
    fn listener_sqr_dist(&mut self) -> f32;
}

pub fn init(cfg: &SoundCfg, remove_when_over: bool, silent: bool) -> SoundState {
    let mut s = SoundState {
        interval_count: cfg.interval,
        remove_when_over,
        ..SoundState::default()
    };
    if silent {
        s.started = true;
        s.stopped = true;
        s.skip_start_end = true;
        s.remove_when_over = true;
    }
    s
}

pub fn priority_release(s: &mut SoundState) {
    if s.stopped || s.fading_out {
        return;
    }
    s.prio_remove = true;
}

pub fn is_stopped(s: &SoundState, stop_entry_present: bool) -> bool {
    s.stopped && !stop_entry_present
}

fn play_sound(s: &mut SoundState, h: &mut impl SoundHost, t: i32, looping: bool) -> bool {
    match h.play_sound(t, looping) {
        PlayResult::Played => true,
        PlayResult::NoName => false,
        PlayResult::Failed {
            not_enough_channels,
        } => {
            if (t == MAIN && !not_enough_channels) || (not_enough_channels && !looping) {
                s.stopped = true;
            }
            false
        }
    }
}

pub fn play(s: &mut SoundState, h: &mut impl SoundHost, cfg: &SoundCfg, play_start: bool) {
    if h.silent() {
        return;
    }
    s.stopped = false;
    s.out_of_range = false;
    s.fading_out = false;

    if play_start && !s.skip_start_end && cfg.looping && cfg.has_start {
        play_sound(s, h, START, false);
        s.started = false;
    }

    if !h.has_entry(MAIN) && !h.has_entry(START) && (!cfg.looping || cfg.interval == 0.0) {
        play_sound(s, h, MAIN, cfg.looping);
        s.started = true;
    }
}

pub fn stop(s: &mut SoundState, h: &mut impl SoundHost, cfg: &SoundCfg, play_end: bool) {
    if s.stopped {
        return;
    }
    s.stopped = true;
    s.out_of_range = false;

    if !h.has_entry(MAIN) {
        return;
    }

    if cfg.looping && play_end && !s.skip_start_end {
        play_sound(s, h, STOP, false);
    }
    if h.is_valid(MAIN) {
        h.stop_entry(MAIN);
    }
    h.clear_entry(MAIN);
}

pub fn fade_in(s: &mut SoundState, h: &mut impl SoundHost, cfg: &SoundCfg, speed: f32) {
    if h.silent() {
        return;
    }
    play(s, h, cfg, false);
    if h.has_entry(MAIN) {
        h.fade_in(MAIN, speed);
    }
}

pub fn fade_out(s: &mut SoundState, h: &mut impl SoundHost, speed: f32) {
    s.fading_out = true;
    if h.silent() {
        return;
    }
    if !h.has_entry(MAIN) {
        s.stopped = true;
        return;
    }
    if h.is_valid(MAIN) {
        h.fade_out(MAIN, speed);
    } else {
        h.clear_entry(MAIN);
        s.stopped = true;
    }
}

fn out_of_range(h: &mut impl SoundHost, max_distance: f32) -> bool {
    if !h.emitter() {
        return true;
    }
    h.listener_sqr_dist() >= max_distance * max_distance
}

pub fn update(
    s: &mut SoundState,
    h: &mut impl SoundHost,
    cfg: &SoundCfg,
    dt: f32,
    max_distance: f32,
) {
    if h.silent() {
        return;
    }
    if s.sleep_count > 0.0 {
        s.sleep_count -= dt;
        return;
    }

    if s.out_of_range && !s.stopped {
        if out_of_range(h, max_distance) {
            return;
        }
        play(s, h, cfg, false);
        s.out_of_range = false;
    }

    for t in MAIN..=STOP {
        if !h.has_entry(t) {
            continue;
        }
        if h.is_valid(t) {
            h.update_position(t);
            continue;
        }
        h.clear_entry(t);
        if s.prio_remove && t == MAIN {
            if !s.stopped {
                s.started = false;
                s.sleep_count = 0.3;
            }
        } else if (cfg.interval == 0.0 || !cfg.looping) && s.started {
            s.stopped = true;
        }
    }

    if s.stopped {
        return;
    }

    if !h.has_entry(MAIN) && !h.has_entry(START) && (!cfg.looping || cfg.interval == 0.0) {
        if play_sound(s, h, MAIN, cfg.looping) {
            s.started = true;
            s.prio_remove = false;
            h.on_start();
        } else if s.prio_remove {
            s.sleep_count = 0.3;
        }
    }

    if cfg.looping && cfg.interval > 0.0 && !h.has_entry(START) && !h.has_entry(MAIN) {
        s.interval_count += dt;
        if s.interval_count >= cfg.interval {
            if rand_rectf(0.0, 1.0) <= cfg.random || cfg.random == 0.0 {
                play_sound(s, h, MAIN, false);
            }
            s.interval_count = 0.0;
        }
    }

    if !h.has_entry(START) && cfg.looping && cfg.use_3d && out_of_range(h, max_distance) {
        if s.fading_out {
            stop(s, h, cfg, false);
        } else {
            if h.has_entry(MAIN) {
                if h.is_valid(MAIN) {
                    h.stop_entry(MAIN);
                }
                h.clear_entry(MAIN);
            }
            s.out_of_range = true;
        }
    }
}
