//! Engine calls the Rust music and insanity handlers make: one dispatch function whose codes and
//! argument layout are mirrored in `amnesia/src/game/LuxFxHostA12b.cpp`. No handler state is
//! borrowed across a call (the handler is taken out of its arena slot meanwhile).

use std::ffi::c_void;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};

pub mod code {
    pub const CFG_F32: i32 = 1;
    pub const CFG_STR: i32 = 2;
    pub const LOG: i32 = 3;
    pub const WARN: i32 = 4;
    pub const ERR: i32 = 5;

    pub const M_PLAY: i32 = 10;
    pub const M_STOP: i32 = 11;
    pub const M_SONG_NULL: i32 = 12;
    pub const M_RESET_RESUME: i32 = 13;
    pub const M_PLAYER_DEAD: i32 = 14;
    pub const M_HARD_MODE: i32 = 15;
    pub const M_HAS_MAP: i32 = 16;
    pub const M_DANGER_SCAN: i32 = 17;
    pub const M_ENEMY_INFO: i32 = 18;

    pub const X_OPEN: i32 = 30;
    pub const X_TYPE: i32 = 31;
    pub const X_STR: i32 = 32;
    pub const X_F32: i32 = 33;
    pub const X_I32: i32 = 34;
    pub const X_BOOL: i32 = 35;
    pub const X_COLOR: i32 = 36;
    pub const X_CLOSE: i32 = 37;

    pub const G_IMAGE: i32 = 40;
    pub const G_RECT_WHITE: i32 = 41;
    pub const G_TEXTURE: i32 = 42;
    pub const G_DESTROY: i32 = 43;
    pub const G_BOUNDS: i32 = 45;
    pub const D_BUG: i32 = 50;
    pub const D_FADE: i32 = 51;
    pub const D_FONT: i32 = 52;

    pub const S_STREAM: i32 = 60;
    pub const S_FADE_IN: i32 = 61;
    pub const S_FADE_OUT: i32 = 62;
    pub const S_STOP: i32 = 63;
    pub const S_IS_VALID: i32 = 64;

    pub const P_CREATE: i32 = 70;
    pub const P_SET_CAMERA: i32 = 71;
    pub const P_EXISTS: i32 = 72;
    pub const P_KILL: i32 = 73;
    pub const E_STEP_SOUND: i32 = 80;
    pub const PL_ACTIVE: i32 = 90;
    pub const PL_SANITY: i32 = 91;
    pub const SUB_ROWS: i32 = 100;
    pub const SUBS_STATE: i32 = 101;
    pub const LOG_START: i32 = 110;
    pub const LOG_PROG: i32 = 111;
}

pub const TAG_STR: i32 = 0;
pub const TAG_ROW: i32 = 1;
pub const TAG_DANGER: i32 = 2;

pub type Sink = unsafe extern "C" fn(ctx: *mut c_void, tag: i32, val: i32, p: *const u8, n: usize);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Args {
    pub p: [usize; 4],
    pub f: [f32; 6],
    pub i: [i32; 4],
    pub s: [*const u8; 3],
    pub n: [usize; 3],
    pub out: [f32; 6],
    pub out_i: [i32; 2],
    pub sink: Option<Sink>,
    pub ctx: *mut c_void,
}

impl Default for Args {
    fn default() -> Self {
        Args {
            p: [0; 4],
            f: [0.0; 6],
            i: [0; 4],
            s: [std::ptr::null(); 3],
            n: [0; 3],
            out: [0.0; 6],
            out_i: [0; 2],
            sink: None,
            ctx: std::ptr::null_mut(),
        }
    }
}

pub type HostFn = unsafe extern "C" fn(code: i32, a: *mut Args) -> usize;

static HOST: AtomicUsize = AtomicUsize::new(0);
static LOGGING: AtomicBool = AtomicBool::new(false);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_set_host(f: Option<HostFn>, logging: i32) {
    HOST.store(f.map_or(0, |f| f as usize), Ordering::Release);
    LOGGING.store(logging != 0, Ordering::Release);
}

/// What a collected host answer carries.
#[derive(Default)]
pub struct Sunk {
    pub strs: Vec<Vec<u8>>,
    pub rows: Vec<Vec<u8>>,
    pub danger: Vec<(i32, Vec<u8>)>,
}

unsafe extern "C" fn collect(ctx: *mut c_void, tag: i32, val: i32, p: *const u8, n: usize) {
    if ctx.is_null() {
        return;
    }
    // SAFETY: `ctx` is the `Sunk` `Call::run` passed for the duration of the call.
    let s = unsafe { &mut *ctx.cast::<Sunk>() };
    let bytes = if p.is_null() || n == 0 {
        Vec::new()
    } else {
        // SAFETY: the host passes `n` readable bytes.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    };
    match tag {
        TAG_STR => s.strs.push(bytes),
        TAG_ROW => s.rows.push(bytes),
        TAG_DANGER => s.danger.push((val, bytes)),
        _ => {}
    }
}

/// The engine side: the C host, or a fake in tests.
pub trait Host {
    fn call(&self, code: i32, a: &mut Args) -> usize;
    fn logging(&self) -> bool;
}

pub struct CHost;

impl Host for CHost {
    fn call(&self, code: i32, a: &mut Args) -> usize {
        let raw = HOST.load(Ordering::Acquire);
        if raw == 0 {
            return 0;
        }
        // SAFETY: only `ohpl_a12b_set_host` stores here, and it stores a `HostFn`.
        let f: HostFn = unsafe { std::mem::transmute::<usize, HostFn>(raw) };
        // SAFETY: `a` is a live argument block that the host reads and writes in place.
        unsafe { f(code, a) }
    }

    fn logging(&self) -> bool {
        LOGGING.load(Ordering::Acquire)
    }
}

/// One host call with its string arguments and collected answers.
pub struct Call<'a> {
    pub a: Args,
    pub code: i32,
    host: &'a dyn Host,
    sunk: Sunk,
}

impl<'a> Call<'a> {
    pub fn new(host: &'a dyn Host, code: i32) -> Self {
        Call {
            a: Args::default(),
            code,
            host,
            sunk: Sunk::default(),
        }
    }

    pub fn s(mut self, slot: usize, v: &[u8]) -> Self {
        if let (Some(p), Some(n)) = (self.a.s.get_mut(slot), self.a.n.get_mut(slot)) {
            *p = v.as_ptr();
            *n = v.len();
        }
        self
    }

    pub fn p(mut self, slot: usize, v: usize) -> Self {
        if let Some(x) = self.a.p.get_mut(slot) {
            *x = v;
        }
        self
    }

    pub fn f(mut self, slot: usize, v: f32) -> Self {
        if let Some(x) = self.a.f.get_mut(slot) {
            *x = v;
        }
        self
    }

    pub fn i(mut self, slot: usize, v: i32) -> Self {
        if let Some(x) = self.a.i.get_mut(slot) {
            *x = v;
        }
        self
    }

    /// Runs the call; the borrowed strings must outlive it.
    pub fn run(mut self) -> Done {
        self.a.sink = Some(collect);
        self.a.ctx = (&mut self.sunk as *mut Sunk).cast();
        let ret = self.host.call(self.code, &mut self.a);
        self.a.ctx = std::ptr::null_mut();
        Done {
            ret,
            out: self.a.out,
            out_i: self.a.out_i,
            sunk: self.sunk,
        }
    }
}

pub struct Done {
    pub ret: usize,
    pub out: [f32; 6],
    pub out_i: [i32; 2],
    pub sunk: Sunk,
}

impl Done {
    pub fn flag(&self) -> bool {
        self.ret != 0
    }

    pub fn string(&self) -> Vec<u8> {
        self.sunk.strs.first().cloned().unwrap_or_default()
    }
}

pub fn log(h: &dyn Host, text: &str) {
    if h.logging() {
        Call::new(h, code::LOG).s(0, text.as_bytes()).run();
    }
}
