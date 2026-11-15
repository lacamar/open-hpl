//! `cLuxProgressLogHandler`.

use super::a10b::Store;
use super::host::{Arg, CFG_USER, Eng};
use crate::luxfx::misc::{
    Date, Vitals, proglog_file_name, proglog_header, proglog_line, proglog_tick,
};
use crate::string::wide_to_utf8;
use std::fs::File;
use std::io::Write;
use std::os::unix::ffi::OsStrExt;

#[derive(Default)]
pub struct Log {
    counter: i32,
    file_count: i32,
    active: bool,
    file: Option<File>,
}

pub static STORE: std::sync::LazyLock<Store<Log>> = std::sync::LazyLock::new(Store::new);

pub fn new() -> u64 {
    STORE.add(Log::default())
}

pub fn delete(h: u64) {
    STORE.remove(h);
}

pub fn load_user_config(e: &Eng, h: u64) {
    e.ev("log_load_user_config", &[]);
    let on = e.cfg_bool(CFG_USER, b"Debug", b"UseProgLog", false);
    STORE.with(h, |l| l.active = on);
}

pub fn save_user_config(e: &Eng, h: u64) {
    e.ev("log_save_user_config", &[]);
    if let Some(on) = STORE.with(h, |l| l.active) {
        e.cfg_set_bool(CFG_USER, b"Debug", b"UseProgLog", on);
    }
}

fn int(r: &super::host::Res, i: usize) -> i32 {
    r.u.get(i).copied().unwrap_or(0) as i32
}

pub fn create_and_reset(e: &Eng, h: u64) {
    e.ev("log_create", &[]);
    let Some((active, count)) = STORE.with(h, |l| {
        if l.active {
            l.file = None;
        }
        (l.active, l.file_count)
    }) else {
        return;
    };
    if !active {
        return;
    }
    let r = e.date();
    let date = Date {
        year: int(&r, 0),
        month: int(&r, 1),
        month_day: int(&r, 2),
        hours: int(&r, 3),
        minutes: int(&r, 4),
        seconds: int(&r, 5),
    };
    let profile = e.profile_name();
    let mut path = e.state_path();
    path.extend(proglog_file_name(&profile, &date, count));
    let path8 = wide_to_utf8(&path);
    match File::create(std::ffi::OsStr::from_bytes(&path8)) {
        Err(_) => {
            let mut m = b"Could not open progress log file '".to_vec();
            m.extend_from_slice(&path8);
            m.extend_from_slice(b"'!\n");
            e.log_error(&m);
            STORE.with(h, |l| l.counter = 0);
        }
        Ok(mut f) => {
            let header = proglog_header(&wide_to_utf8(&profile), &date);
            let _ = f.write_all(&header);
            STORE.with(h, |l| {
                l.file = Some(f);
                l.counter = 0;
            });
        }
    }
}

pub fn update(h: u64) {
    STORE.with(h, |l| l.counter = proglog_tick(l.active, l.counter));
}

pub fn add_log(e: &Eng, h: u64, level: i32, msg: &[u8]) {
    e.ev("log_add", &[Arg::I(i64::from(level)), Arg::S(msg)]);
    let Some(Some(counter)) =
        STORE.with(h, |l| (l.active && l.file.is_some()).then_some(l.counter))
    else {
        return;
    };
    let map = e.map_name();
    let v = e.player_vitals();
    let vitals = Vitals {
        health: v.f(0),
        sanity: v.f(1),
        oil: v.f(2),
        tinderboxes: int(&v, 3),
        coins: int(&v, 4),
    };
    let line = proglog_line(level, counter, &map, &vitals, msg);
    STORE.with(h, |l| {
        if let Some(f) = l.file.as_mut() {
            let _ = f.write_all(&line);
        }
    });
}
