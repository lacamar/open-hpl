//! S5: the timers and faders of `cSomaLuxScriptable` (`SomaLuxScriptable.cpp`) and their script
//! natives. C++ keeps the object as a shell; Rust owns its table in an arena, found by object address.

use super::scriptable::{self, Fader, Timer, TimerAdd};
use crate::obj::{Arena, Handle};
use crate::somascript::s2_as::{Engine, Frame, Soma};
use crate::string_ffi::{Sink, emit};
use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::{CStr, c_char, c_void};

#[derive(Default)]
pub struct Table {
    timers: Vec<(Timer, Vec<u8>)>,
    faders: Vec<Fader>,
}

pub type Due = Vec<(u64, Vec<u8>)>;

impl Table {
    fn timer(&self, id: u64) -> Option<&Timer> {
        self.timers.iter().map(|(t, _)| t).find(|t| t.id == id)
    }

    fn timer_mut(&mut self, id: u64) -> Option<&mut Timer> {
        self.timers.iter_mut().map(|(t, _)| t).find(|t| t.id == id)
    }

    fn lengths(&self, id: u64) -> Option<(f32, f32)> {
        self.timer(id).map(|t| (t.length, t.time_left))
    }

    pub fn add(&mut self, id: u64, time: f32, func: &[u8], create_if_exist: bool, repeat: bool) {
        let pos = self.timers.iter().position(|(t, _)| t.id == id);
        let fresh = (
            Timer {
                id,
                length: time,
                time_left: time,
                repeat,
                paused: false,
            },
            func.to_vec(),
        );
        match (scriptable::timer_add(pos.is_some(), create_if_exist), pos) {
            (TimerAdd::Skip, _) => {}
            (TimerAdd::Overwrite, Some(i)) => {
                if let Some(slot) = self.timers.get_mut(i) {
                    *slot = fresh;
                }
            }
            _ => self.timers.push(fresh),
        }
    }

    pub fn remove(&mut self, id: u64) {
        self.timers.retain(|(t, _)| t.id != id);
    }

    pub fn exists(&self, id: u64) -> bool {
        self.timer(id).is_some()
    }

    pub fn time_left(&self, id: u64) -> f32 {
        self.timer(id).map_or(0.0, |t| t.time_left)
    }

    pub fn set_paused(&mut self, id: u64, paused: bool) {
        if let Some(t) = self.timer_mut(id) {
            t.paused = paused;
        }
    }

    pub fn has_passed(&self, id: u64, length: f32) -> bool {
        scriptable::timer_time_has_passed(self.lengths(id), length)
    }

    pub fn value(&self, id: u64, min: f32, max: f32, kind: i32, abs: bool) -> f32 {
        scriptable::timer_value(self.lengths(id), min, max, kind, abs)
    }

    pub fn clear_timers(&mut self) {
        self.timers.clear();
    }

    pub fn clear_faders(&mut self) {
        self.faders.clear();
    }

    pub fn idle(&self) -> bool {
        self.timers.is_empty() && self.faders.is_empty()
    }

    pub fn describe(&self) -> Vec<u8> {
        let mut out = Vec::new();
        for (t, f) in &self.timers {
            out.extend_from_slice(
                format!(
                    "t[{} {:08x} {} {} ",
                    t.id,
                    t.length.to_bits(),
                    u8::from(t.repeat),
                    u8::from(t.paused)
                )
                .as_bytes(),
            );
            out.extend_from_slice(f);
            out.push(b']');
        }
        for f in &self.faders {
            out.extend_from_slice(
                format!(
                    "f[{} {:08x} {} {}]",
                    f.id,
                    f.goal.to_bits(),
                    u8::from(f.reverse_at_end),
                    u8::from(f.paused)
                )
                .as_bytes(),
            );
        }
        out
    }

    pub fn step_timers(&mut self, dt: f32) -> Due {
        let mut due = Vec::new();
        let mut keep = Vec::with_capacity(self.timers.len());
        for (t, f) in &mut self.timers {
            let (is_due, kept) = scriptable::step_timer(t, dt);
            if is_due {
                due.push((t.id, f.clone()));
            }
            keep.push(kept);
        }
        let mut it = keep.into_iter();
        self.timers.retain(|_| it.next().unwrap_or(true));
        due
    }

    pub fn step_faders(&mut self, dt: f32) {
        scriptable::update_faders(&mut self.faders, dt);
    }

    fn fader_slot(&mut self, id: u64, skip_if_exists: bool) -> Option<&mut Fader> {
        if let Some(i) = self.faders.iter().position(|f| f.id == id) {
            return if skip_if_exists {
                None
            } else {
                self.faders.get_mut(i)
            };
        }
        self.faders.push(Fader {
            id,
            ..Fader::default()
        });
        self.faders.last_mut()
    }

    pub fn fade_to(&mut self, id: u64, goal: f32, time: f32, reverse: bool, skip: bool) {
        if let Some(f) = self.fader_slot(id, skip) {
            f.start = f.value;
            f.goal = goal;
            f.speed = scriptable::fade_speed(f.value, goal, time);
            f.reverse_at_end = reverse;
        }
    }

    pub fn set_fader(&mut self, id: u64, x: f32, skip: bool) {
        if let Some(f) = self.fader_slot(id, skip) {
            f.value = x;
            f.start = x;
            f.goal = x;
        }
    }

    pub fn fader_paused(&mut self, id: u64, paused: bool) {
        if let Some(f) = self.faders.iter_mut().find(|f| f.id == id) {
            f.paused = paused;
        }
    }

    pub fn fader_value(&self, id: u64, min: f32, max: f32, kind: i32, abs: bool) -> f32 {
        let v = self.faders.iter().find(|f| f.id == id).map(|f| f.value);
        scriptable::fader_value(v, min, max, kind, abs)
    }
}

#[derive(Default)]
struct Registry {
    tables: Arena<Table>,
    by_addr: HashMap<usize, Handle<Table>>,
}

thread_local! {
    static REG: RefCell<Registry> = RefCell::new(Registry::default());
}

fn with<R>(this: usize, f: impl FnOnce(&mut Table) -> R) -> Option<R> {
    REG.try_with(|r| {
        let mut r = r.try_borrow_mut().ok()?;
        let h = *r.by_addr.get(&this)?;
        r.tables.get_mut(h).map(f)
    })
    .ok()
    .flatten()
}

fn on<R: Default>(fr: &Frame, f: impl FnOnce(&mut Table) -> R) -> R {
    with(fr.this as usize, f).unwrap_or_default()
}

/// # Safety
/// `this` is the address of the scriptable the natives will be called on.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_new(this: *mut c_void) {
    let _ = REG.try_with(|r| {
        if let Ok(mut r) = r.try_borrow_mut()
            && let Some(h) = r.tables.insert(Table::default())
        {
            r.by_addr.insert(this as usize, h);
        }
    });
}

/// # Safety
/// `this` was passed to `ohpl_s5_ts_new`; the C++ object dies after this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_free(this: *mut c_void) {
    let _ = REG.try_with(|r| {
        if let Ok(mut r) = r.try_borrow_mut()
            && let Some(h) = r.by_addr.remove(&(this as usize))
        {
            r.tables.remove(h);
        }
    });
}

/// # Safety
/// `this` was passed to `ohpl_s5_ts_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_idle(this: *mut c_void) -> bool {
    with(this as usize, |t| t.idle()).unwrap_or(true)
}

/// A timer as the save code sees it; the script function goes through the sink.
#[repr(C)]
pub struct TimerOut {
    id: u64,
    length: f32,
    time_left: f32,
    repeat: bool,
    paused: bool,
}

/// # Safety
/// `this` was passed to `ohpl_s5_ts_new`; `out` is writable; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_timer_at(
    this: *mut c_void,
    i: usize,
    out: *mut TimerOut,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    let Some(Some((t, f))) = with(this as usize, |t| t.timers.get(i).cloned()) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        out.write(TimerOut {
            id: t.id,
            length: t.length,
            time_left: t.time_left,
            repeat: t.repeat,
            paused: t.paused,
        });
        emit(sink, ctx, &f);
    }
    true
}

/// # Safety
/// `this` was passed to `ohpl_s5_ts_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_fader_at(this: *mut c_void, i: usize, out: *mut Fader) -> bool {
    let Some(Some(f)) = with(this as usize, |t| t.faders.get(i).copied()) else {
        return false;
    };
    // SAFETY: caller contract; `Fader` is `repr(C)` like `OhplGs3Fader`.
    unsafe { out.write(f) };
    true
}

/// # Safety
/// `this` was passed to `ohpl_s5_ts_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_clear(this: *mut c_void) {
    with(this as usize, |t| {
        t.timers.clear();
        t.faders.clear();
    });
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Fader>() == 32 && offset_of!(Fader, reverse_at_end) == 24);
    assert!(size_of::<TimerOut>() == 24 && offset_of!(TimerOut, repeat) == 16);
};

/// One table operation by number, for the `OHPL_S5_DUMP` self-test; queries return 0 or 1 for booleans.
///
/// # Safety
/// `this` was passed to `ohpl_s5_ts_new`; `func` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_op(
    this: *mut c_void,
    op: u32,
    id: u64,
    f0: f32,
    f1: f32,
    i0: i32,
    flags: u32,
    func: *const u8,
    n: usize,
) -> f32 {
    // SAFETY: caller contract.
    let func = unsafe { crate::string_ffi::units(func, n) };
    let (b0, b1) = (flags & 1 != 0, flags & 2 != 0);
    with(this as usize, |t| match op {
        0 => {
            t.add(id, f0, func, b0, b1);
            0.0
        }
        1 => {
            t.remove(id);
            0.0
        }
        2 => f32::from(u8::from(t.exists(id))),
        3 => t.time_left(id),
        4 => {
            t.set_paused(id, b0);
            0.0
        }
        5 => f32::from(u8::from(t.has_passed(id, f0))),
        6 => t.value(id, f0, f1, i0, b0),
        7 => {
            t.fade_to(id, f0, f1, b0, b1);
            0.0
        }
        8 => {
            t.set_fader(id, f0, b0);
            0.0
        }
        9 => {
            t.fader_paused(id, b0);
            0.0
        }
        10 => t.fader_value(id, f0, f1, i0, b0),
        _ => {
            t.clear_timers();
            t.clear_faders();
            0.0
        }
    })
    .unwrap_or(0.0)
}

/// # Safety
/// `this` was passed to `ohpl_s5_ts_new`; `func` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_push_timer(
    this: *mut c_void,
    id: u64,
    length: f32,
    time_left: f32,
    repeat: bool,
    paused: bool,
    func: *const u8,
    n: usize,
) {
    // SAFETY: caller contract.
    let func = unsafe { crate::string_ffi::units(func, n) }.to_vec();
    let t = Timer {
        id,
        length,
        time_left,
        repeat,
        paused,
    };
    with(this as usize, |tb| tb.timers.push((t, func)));
}

/// # Safety
/// `this` was passed to `ohpl_s5_ts_new`; `f` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_push_fader(this: *mut c_void, f: *const Fader) {
    // SAFETY: caller contract.
    if let Some(f) = unsafe { f.as_ref() } {
        let f = *f;
        with(this as usize, |t| t.faders.push(f));
    }
}

/// The dump line body of the table: timers then faders, floats as hex bits.
///
/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_describe(this: *mut c_void, sink: Sink<u8>, ctx: *mut c_void) {
    let d = with(this as usize, |t| t.describe()).unwrap_or_default();
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &d) };
}

pub type DueCb = unsafe extern "C" fn(*mut c_void, u64, *const u8, usize);

/// Steps the timers, calls `cb` for each due one with a script function, then steps the faders.
///
/// # Safety
/// `cb` may re-enter the natives of `this`; `ctx` is whatever `cb` expects.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_ts_update(
    this: *mut c_void,
    dt: f32,
    cb: DueCb,
    ctx: *mut c_void,
) {
    let due = with(this as usize, |t| t.step_timers(dt)).unwrap_or_default();
    for (id, func) in &due {
        if func.is_empty() {
            continue;
        }
        // SAFETY: caller contract; no table borrow is held across the call.
        unsafe { cb(ctx, *id, func.as_ptr(), func.len()) };
    }
    with(this as usize, |t| t.step_faders(dt));
}

/// # Safety
/// `engine` is a live `asIScriptEngine`; `ty` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_register_timer_natives(engine: *mut c_void, ty: *const c_char) {
    // SAFETY: caller contract.
    let ty = unsafe { CStr::from_ptr(ty) }.to_string_lossy();
    register(Engine(engine), &ty);
}

const OPT: &str =
    "float afMin=0, float afMax=1, eEasing aEasing=eEasing_Linear, bool abAbsValue=false";

#[derive(Clone, Copy)]
enum Id {
    U64,
    Hash,
    U32,
}

fn id(k: Id, f: &Frame) -> u64 {
    match k {
        Id::U64 => f.arg::<u64>(0),
        Id::Hash => scriptable::hash64(f.str_arg(0)),
        Id::U32 => u64::from(f.arg::<u32>(0)),
    }
}

fn timers(s: &Soma, ty: &str, d: &str, k: Id) {
    s.method(
        ty,
        &format!("void Timer_Add({d}, float afTime, const tString&in asFunc=\"\", bool abCreateIfExist=true, bool abRepeat=false)"),
        move |f| {
            let func = f.str_arg(2);
            on(f, |t| t.add(id(k, f), f.arg(1), func, f.arg(3), f.arg(4)));
        },
    );
    s.method(ty, &format!("void Timer_Remove({d})"), move |f| {
        on(f, |t| t.remove(id(k, f)))
    });
    s.method(ty, &format!("bool Timer_Exists({d})"), move |f| {
        f.ret(on(f, |t| t.exists(id(k, f))))
    });
    s.method(ty, &format!("float Timer_GetTimeLeft({d})"), move |f| {
        f.ret(on(f, |t| t.time_left(id(k, f))));
    });
    s.method(
        ty,
        &format!("void Timer_SetPaused({d}, bool abX)"),
        move |f| {
            on(f, |t| t.set_paused(id(k, f), f.arg(1)));
        },
    );
    s.method(
        ty,
        &format!("bool Timer_TimeHasPassed({d}, float afLength)"),
        move |f| {
            f.ret(on(f, |t| t.has_passed(id(k, f), f.arg(1))));
        },
    );
    s.method(ty, &format!("float Timer_GetValue({d}, {OPT})"), move |f| {
        f.ret(on(f, |t| {
            t.value(id(k, f), f.arg(1), f.arg(2), f.arg(3), f.arg(4))
        }));
    });
}

fn faders(s: &Soma, ty: &str, d: &str, k: Id) {
    s.method(
        ty,
        &format!("void Fader_FadeTo({d}, float afGoal, float afTime, bool abReverseAtEnd=false, bool abSkipIfExists=false)"),
        move |f| on(f, |t| t.fade_to(id(k, f), f.arg(1), f.arg(2), f.arg(3), f.arg(4))),
    );
    s.method(
        ty,
        &format!("void Fader_Set({d}, float afX, bool abSkipIfExists=false)"),
        move |f| {
            on(f, |t| t.set_fader(id(k, f), f.arg(1), f.arg(2)));
        },
    );
    s.method(
        ty,
        &format!("void Fader_SetPaused({d}, bool abPaused)"),
        move |f| {
            on(f, |t| t.fader_paused(id(k, f), f.arg(1)));
        },
    );
    s.method(ty, &format!("float Fader_GetValue({d}, {OPT})"), move |f| {
        f.ret(on(f, |t| {
            t.fader_value(id(k, f), f.arg(1), f.arg(2), f.arg(3), f.arg(4))
        }));
    });
}

pub fn register(e: Engine, ty: &str) {
    let s = Soma(e);
    s.method(ty, "void Timer_ClearAll()", |f| on(f, Table::clear_timers));
    timers(&s, ty, "uint64 alID", Id::U64);
    timers(&s, ty, "const tString&in asID", Id::Hash);
    s.method(ty, "void Fader_ClearAll()", |f| on(f, Table::clear_faders));
    faders(&s, ty, "uint alID", Id::U32);
    faders(&s, ty, "const tString&in asName", Id::Hash);
}
