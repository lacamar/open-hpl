//! C ABI for the system layer (`HPL2/core/include/system/RustSystem.h`). Byte arguments are
//! (pointer, length) pairs; results go through sink callbacks, so Rust never hands out memory.

use super::diag;
use super::headless::{self, LineSplitter};
use super::logic_timer::LogicTimer;
use super::serialize::{self, Class, CreateFn, Field, Host, Level, Obj, T_NULL, Walker};
use crate::string_ffi::{Sink, emit, units};
use crate::xml::Doc;
use std::ffi::{c_char, c_void};
use std::sync::Arc;

type Fp<A, R> = unsafe extern "C" fn(A) -> R;

/// Callbacks into the C++ object model; every pointer is a live object or element.
#[repr(C)]
pub struct OhplSerHost {
    pub log: unsafe extern "C" fn(i32, *const u8, usize),
    pub top_class: unsafe extern "C" fn(Obj, Sink<u8>, *mut c_void),
    pub destroy: Fp<Obj, ()>,
    pub tmp_new: unsafe extern "C" fn(u32, usize) -> Obj,
    pub tmp_free: unsafe extern "C" fn(u32, Obj),
    pub str_get: unsafe extern "C" fn(Obj, *mut usize) -> *const u8,
    pub str_set: unsafe extern "C" fn(Obj, *const u8, usize),
    pub wstr_get: unsafe extern "C" fn(Obj, *mut usize) -> *const u32,
    pub wstr_set: unsafe extern "C" fn(Obj, *const u32, usize),
    pub string_size: usize,
    pub wstring_size: usize,
    pub cont_size: Fp<Obj, usize>,
    pub cont_clear: Fp<Obj, ()>,
    pub cont_add_ptr: unsafe extern "C" fn(Obj, Obj),
    pub cont_add_class: unsafe extern "C" fn(Obj, Obj),
    pub cont_iter: Fp<Obj, Obj>,
    pub iter_has_next: Fp<Obj, bool>,
    pub iter_next: Fp<Obj, Obj>,
    pub iter_free: Fp<Obj, ()>,
}

struct CHost<'a>(&'a OhplSerHost);

unsafe extern "C" fn collect(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec<u8>` passed by `top_class`; `p` is valid for `n` bytes.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) }
}

impl Host for CHost<'_> {
    fn log(&self, level: Level, msg: &[u8]) {
        let l = match level {
            Level::Log => 0,
            Level::Warning => 1,
            Level::Error => 2,
        };
        // SAFETY: `Walker::new` contract of the host callbacks.
        unsafe { (self.0.log)(l, msg.as_ptr(), msg.len()) }
    }

    fn top_class(&self, obj: Obj) -> Vec<u8> {
        let mut out = Vec::new();
        // SAFETY: `obj` is live; `collect` appends to the `Vec` passed as context.
        unsafe { (self.0.top_class)(obj, collect, (&raw mut out).cast()) };
        out
    }

    fn destroy(&self, obj: Obj) {
        // SAFETY: `obj` is an owned `iSerializable`.
        unsafe { (self.0.destroy)(obj) }
    }

    fn tmp_new(&self, ty: u32, size: usize) -> Obj {
        // SAFETY: host callback.
        unsafe { (self.0.tmp_new)(ty, size) }
    }

    fn tmp_free(&self, ty: u32, p: Obj) {
        // SAFETY: `p` came from `tmp_new` with the same type.
        unsafe { (self.0.tmp_free)(ty, p) }
    }

    fn str_get(&self, p: Obj) -> Vec<u8> {
        let mut n = 0;
        // SAFETY: `p` is a live `std::string`; the data stays valid until the next host call.
        unsafe {
            let d = (self.0.str_get)(p, &raw mut n);
            units(d, n).to_vec()
        }
    }

    fn str_set(&self, p: Obj, v: &[u8]) {
        // SAFETY: `p` is a live `std::string`.
        unsafe { (self.0.str_set)(p, v.as_ptr(), v.len()) }
    }

    fn wstr_get(&self, p: Obj) -> Vec<u32> {
        let mut n = 0;
        // SAFETY: `p` is a live `std::wstring`; the data stays valid until the next host call.
        unsafe {
            let d = (self.0.wstr_get)(p, &raw mut n);
            units(d, n).to_vec()
        }
    }

    fn wstr_set(&self, p: Obj, v: &[u32]) {
        // SAFETY: `p` is a live `std::wstring`.
        unsafe { (self.0.wstr_set)(p, v.as_ptr(), v.len()) }
    }

    fn string_size(&self) -> usize {
        self.0.string_size
    }

    fn wstring_size(&self) -> usize {
        self.0.wstring_size
    }

    fn cont_size(&self, cont: Obj) -> usize {
        // SAFETY: `cont` is a live `iContainer`.
        unsafe { (self.0.cont_size)(cont) }
    }

    fn cont_clear(&self, cont: Obj) {
        // SAFETY: `cont` is a live `iContainer`.
        unsafe { (self.0.cont_clear)(cont) }
    }

    fn cont_add_ptr(&self, cont: Obj, ptr: Obj) {
        // SAFETY: `cont` is a live `iContainer`.
        unsafe { (self.0.cont_add_ptr)(cont, ptr) }
    }

    fn cont_add_class(&self, cont: Obj, elem: Obj) {
        // SAFETY: `cont` is a live `iContainer`; `elem` holds one element.
        unsafe { (self.0.cont_add_class)(cont, elem) }
    }

    fn cont_iter(&self, cont: Obj) -> Obj {
        // SAFETY: `cont` is a live `iContainer`.
        unsafe { (self.0.cont_iter)(cont) }
    }

    fn iter_next(&self, it: Obj) -> Option<Obj> {
        // SAFETY: `it` is a live iterator from `cont_iter`.
        unsafe { (self.0.iter_has_next)(it).then(|| (self.0.iter_next)(it)) }
    }

    fn iter_free(&self, it: Obj) {
        // SAFETY: `it` came from `cont_iter`.
        unsafe { (self.0.iter_free)(it) }
    }
}

#[repr(C)]
pub struct OhplSerField {
    pub name: *const c_char,
    pub class_name: *const c_char,
    pub offset: usize,
    pub size: usize,
    pub ty: u32,
    pub main: u32,
    pub array_size: usize,
}

fn cstr<'a>(p: *const c_char) -> &'a [u8] {
    if p.is_null() {
        return &[];
    }
    // SAFETY: caller passes NUL-terminated strings.
    unsafe { std::ffi::CStr::from_ptr(p) }.to_bytes()
}

/// `SetUpData` for one class; the field table ends before its first `T_NULL` entry.
///
/// # Safety
/// `name` and `parent` are NUL-terminated; `fields` is valid for `n` entries with NUL-terminated
/// names; `create` is the class's `iSerializable` factory.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ser_register(
    name: *const c_char,
    parent: *const c_char,
    fields: *const OhplSerField,
    n: usize,
    size: usize,
    create: Option<CreateFn>,
) {
    // SAFETY: caller contract.
    let raw = unsafe { units(fields, n) };
    let fields = raw
        .iter()
        .take_while(|f| f.ty != T_NULL)
        .map(|f| {
            Arc::new(Field {
                name: cstr(f.name).to_vec(),
                class_name: cstr(f.class_name).to_vec(),
                offset: f.offset,
                size: f.size,
                ty: f.ty,
                main: f.main,
                array_size: f.array_size,
            })
        })
        .collect();
    serialize::register(Class {
        name: cstr(name).to_vec(),
        parent: cstr(parent).to_vec(),
        fields,
        size,
        create,
    });
}

/// Fills `doc` with `<root>` holding the serialised `obj`.
///
/// # Safety
/// `host` is fully populated; `doc` is live; `obj` is a live registered object; `root` is
/// NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ser_save(
    host: *const OhplSerHost,
    doc: *mut Doc,
    obj: Obj,
    root: *const c_char,
) {
    // SAFETY: caller contract.
    unsafe {
        let host = CHost(&*host);
        Walker::new(&host).save_document(&mut *doc, obj, cstr(root));
    }
}

/// Loads the first `class` element under the root of `doc` into `obj`.
///
/// # Safety
/// As `ohpl_ser_save`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ser_load(host: *const OhplSerHost, doc: *const Doc, obj: Obj) {
    // SAFETY: caller contract.
    unsafe {
        let host = CHost(&*host);
        Walker::new(&host).load_document(&*doc, obj);
    }
}

// ---- logic timer ----

/// # Safety
/// `t` is valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_lt_init(t: *mut LogicTimer, updates_per_sec: i32, now: u64) {
    // SAFETY: caller contract.
    unsafe { t.write(LogicTimer::new(updates_per_sec, now)) }
}

macro_rules! lt_fn {
    ($name:ident, $m:ident $(, $a:ident: $t:ty)* => $r:ty) => {
        /// # Safety
        /// `t` points to an initialised timer.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(t: *mut LogicTimer $(, $a: $t)*) -> $r {
            // SAFETY: caller contract.
            unsafe { (*t).$m($($a),*) }
        }
    };
}
lt_fn!(ohpl_lt_reset, reset, now: u64 => ());
lt_fn!(ohpl_lt_want_update, want_update, now: u64 => bool);
lt_fn!(ohpl_lt_end_update_loop, end_update_loop, now: u64 => ());
lt_fn!(ohpl_lt_set_updates_per_sec, set_updates_per_sec, ups: i32, now: u64 => ());
lt_fn!(ohpl_lt_updates_per_sec, updates_per_sec => i32);
lt_fn!(ohpl_lt_step_size, step_size => f32);

// ---- headless control ----

type PairSink = unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize);

/// Calls `sink(ctx, key, value)` per field of the request line, in key order.
///
/// # Safety
/// `s` is valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hc_parse(s: *const u8, n: usize, sink: PairSink, ctx: *mut c_void) {
    // SAFETY: caller contract.
    unsafe {
        for (k, v) in headless::parse_flat_json(units(s, n)) {
            sink(ctx, k.as_ptr(), k.len(), v.as_ptr(), v.len());
        }
    }
}

/// # Safety
/// `s` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hc_json_escape(
    s: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &headless::json_escape(units(s, n))) }
}

#[repr(C)]
pub struct OhplHcField {
    pub key: *const u8,
    pub key_n: usize,
    pub val: *const u8,
    pub val_n: usize,
}

/// # Safety
/// `err` valid for `err_n` bytes; `fields` for `n` entries with valid pointers; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hc_response_json(
    ok: bool,
    err: *const u8,
    err_n: usize,
    fields: *const OhplHcField,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let f: Vec<(&[u8], &[u8])> = units(fields, n)
            .iter()
            .map(|f| (units(f.key, f.key_n), units(f.val, f.val_n)))
            .collect();
        emit(
            sink,
            ctx,
            &headless::response_json(ok, units(err, err_n), &f),
        );
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_hc_splitter_new() -> *mut LineSplitter {
    Box::into_raw(Box::default())
}

/// # Safety
/// `s` comes from `ohpl_hc_splitter_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hc_splitter_free(s: *mut LineSplitter) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(s) });
}

/// Appends a received chunk and calls `sink` once per completed non-empty line.
///
/// # Safety
/// `s` is live; `data` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hc_splitter_push(
    s: *mut LineSplitter,
    data: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        for line in (*s).push(units(data, n)) {
            emit(sink, ctx, &line);
        }
    }
}

// ---- diagnostics ----

/// # Safety
/// `s` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_diag_json_escape(
    s: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &diag::json_escape(units(s, n))) }
}

/// `Num` and `cHeadlessResponse::Set(float)`.
///
/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_diag_num(x: f32, sink: Sink<u8>, ctx: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &diag::num(x)) }
}

/// # Safety
/// `px` valid for `4 * w * h` floats (or fewer when `w * h` is not positive); `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_diag_pixel_stats(
    px: *const f32,
    w: i32,
    h: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    let count = (w as usize).wrapping_mul(h as usize).saturating_mul(4);
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &diag::pixel_stats_json(units(px, count), w, h)) }
}

/// # Safety
/// `px` valid for `w * h * bpp` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_diag_frame_stats(
    px: *const u8,
    w: i32,
    h: i32,
    bpp: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    let count = (w as usize)
        .wrapping_mul(h as usize)
        .wrapping_mul(bpp as usize);
    // SAFETY: caller contract.
    unsafe {
        emit(
            sink,
            ctx,
            &diag::frame_stats_json(units(px, count), w, h, bpp),
        )
    }
}

/// # Safety
/// `out` valid for 256 bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_diag_gamma_lut(gamma: f32, out: *mut u8) {
    // SAFETY: caller contract.
    unsafe { out.copy_from_nonoverlapping(diag::gamma_lut(gamma).as_ptr(), 256) }
}
