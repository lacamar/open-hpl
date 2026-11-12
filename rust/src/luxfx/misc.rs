//! Progress log lines and timing, completion counter, achievement gates.

use crate::string::{float_to_string, int_to_string};

/// `cLuxProgressLogHandler::Update`: the counter ticks once per update while logging.
pub fn proglog_tick(active: bool, counter: i32) -> i32 {
    if active {
        counter.wrapping_add(1)
    } else {
        counter
    }
}

fn level_name(level: i32) -> &'static [u8] {
    match level {
        0 => b"Low",
        1 => b"Medium",
        2 => b"High",
        _ => b"",
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vitals {
    pub health: f32,
    pub sanity: f32,
    pub oil: f32,
    pub tinderboxes: i32,
    pub coins: i32,
}

fn clock(counter: i32) -> Vec<u8> {
    let sec = counter.wrapping_div(60).wrapping_rem(60);
    let min = counter.wrapping_div(3600).wrapping_rem(60);
    let hour = counter.wrapping_div(216_000);
    format!("{hour:02}:{min:02}:{sec:02}").into_bytes()
}

fn cut_nul(mut v: Vec<u8>) -> Vec<u8> {
    if let Some(p) = v.iter().position(|b| *b == 0) {
        v.truncate(p);
    }
    v
}

/// One `AddLog` entry as written to the file: header with clock, map and vitals, then the message
/// with a continuation marker after every newline. The C++ printed it with `%s`, which stops at a
/// NUL byte.
pub fn proglog_line(level: i32, counter: i32, map: &[u8], v: &Vitals, msg: &[u8]) -> Vec<u8> {
    let mut o = Vec::with_capacity(msg.len() + 128);
    o.extend_from_slice(b"# ");
    o.extend_from_slice(level_name(level));
    o.extend_from_slice(b" # ");
    o.extend(clock(counter));
    o.extend_from_slice(b" | ");
    o.extend_from_slice(map);
    o.extend_from_slice(b" | H:");
    o.extend(float_to_string(v.health, 0, false));
    o.extend_from_slice(b" | S:");
    o.extend(float_to_string(v.sanity, 0, false));
    o.extend_from_slice(b" | O:");
    o.extend(float_to_string(v.oil, 0, false));
    o.extend_from_slice(b" | T:");
    o.extend(int_to_string(v.tinderboxes, 0));
    o.extend_from_slice(b" | C:");
    o.extend(int_to_string(v.coins, 0));
    o.extend_from_slice(b" \n > ");
    for c in msg {
        o.push(*c);
        if *c == b'\n' {
            o.extend_from_slice(b" > ");
        }
    }
    o.push(b'\n');
    cut_nul(o)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Date {
    pub year: i32,
    pub month: i32,
    pub month_day: i32,
    pub hours: i32,
    pub minutes: i32,
    pub seconds: i32,
}

/// `ProgLog_<profile>_YYYY_MM_DD_hh_mm_ss_NN.log` as wide characters.
pub fn proglog_file_name(profile: &[u32], d: &Date, file_count: i32) -> Vec<u32> {
    let mut o: Vec<u32> = b"ProgLog_".iter().map(|c| u32::from(*c)).collect();
    o.extend_from_slice(profile);
    let num = |v: i32, pad: i32, o: &mut Vec<u32>| {
        o.push(u32::from(b'_'));
        o.extend(int_to_string(v, pad).iter().map(|c| u32::from(*c)));
    };
    num(d.year, 4, &mut o);
    num(d.month.wrapping_add(1), 2, &mut o);
    num(d.month_day, 2, &mut o);
    num(d.hours, 2, &mut o);
    num(d.minutes, 2, &mut o);
    num(d.seconds, 2, &mut o);
    num(file_count, 2, &mut o);
    o.extend(b".log".iter().map(|c| u32::from(*c)));
    o
}

/// The three header lines of a new log file; the profile prints through `%s`.
pub fn proglog_header(profile: &[u8], d: &Date) -> Vec<u8> {
    let name = cut_nul(profile.to_vec());
    let mut o = b"--- Amnesia Progress Logger ----\nPlayer: ".to_vec();
    o.extend(name);
    o.extend(
        format!(
            " Date: {}/{} - {}\n",
            d.month_day,
            d.month.wrapping_add(1),
            d.year
        )
        .into_bytes(),
    );
    o.extend_from_slice(b"---------------------------------\n");
    o
}

/// Both completion counter entry points start with `return;` in the shipped code; the state
/// machine below is kept for when that is removed.
pub const COMPLETION_LIVE: bool = false;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Completion {
    pub active: u32,
    pub alpha: f32,
    pub delay: f32,
    pub new_amount: f32,
    pub displayed: f32,
    pub disable_delay: f32,
}

pub fn completion_reset(c: &mut Completion) {
    c.active = 0;
    c.alpha = 0.0;
    c.delay = 0.0;
    c.new_amount = 0.0;
    c.displayed = 0.0;
}

pub fn completion_update(c: &mut Completion, dt: f32) {
    if COMPLETION_LIVE {
        completion_step(c, dt);
    }
}

pub fn completion_show(c: &mut Completion, previous: f32, new: f32, delay: f32) {
    if COMPLETION_LIVE {
        completion_show_inner(c, previous, new, delay);
    }
}

fn completion_step(c: &mut Completion, dt: f32) {
    let dt64 = f64::from(dt);
    if c.active == 0 {
        c.alpha = (f64::from(c.alpha) - 0.5 * dt64) as f32;
        if c.alpha < 0.0 {
            c.alpha = 0.0;
        }
        return;
    }
    if c.delay > 0.0 {
        c.delay -= dt;
        return;
    }
    c.alpha = 1.2f32.mul_add(dt, c.alpha);
    if c.alpha > 1.0 {
        c.alpha = 1.0;
    }
    if c.alpha > 0.2 {
        if c.displayed < c.new_amount {
            let add = (f64::from(0.0f32) + 0.001) * 30.0 * dt64;
            c.displayed = (f64::from(c.displayed) + add) as f32;
            if c.displayed > c.new_amount {
                c.displayed = c.new_amount;
            }
        } else {
            c.disable_delay -= dt;
            if c.disable_delay <= 0.0 {
                c.active = 0;
            }
        }
    }
}

fn completion_show_inner(c: &mut Completion, previous: f32, new: f32, delay: f32) {
    if previous == new {
        return;
    }
    c.active = 1;
    if c.alpha <= 0.0 {
        c.delay = delay;
        c.displayed = previous;
    }
    c.new_amount = new;
    c.disable_delay = 2.0;
}

pub const UNLOCK_OK: i32 = 0;
pub const UNLOCK_NOT_REGISTERED: i32 = 1;
pub const UNLOCK_UNKNOWN: i32 = 2;

/// `UnlockAchievement`: which error, if any, the call ends with.
pub fn unlock_gate(registered: bool, known: bool) -> i32 {
    if !registered {
        UNLOCK_NOT_REGISTERED
    } else if !known {
        UNLOCK_UNKNOWN
    } else {
        UNLOCK_OK
    }
}

#[cfg(test)]
pub fn completion_step_for_test(c: &mut Completion, dt: f32) {
    completion_step(c, dt);
}

#[cfg(test)]
pub fn completion_show_for_test(c: &mut Completion, p: f32, n: f32, d: f32) {
    completion_show_inner(c, p, n, d);
}
