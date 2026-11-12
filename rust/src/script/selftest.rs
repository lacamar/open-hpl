//! Exercises the registration API end to end against the real library (run by the script check).

use super::engine::{
    AsResult, Behaviour, CallConv, EngineRef, ScriptEngine, new_script_str, obj, script_str,
};
use super::sqscript::Script;
use std::ffi::c_void;
use std::sync::atomic::{AtomicI32, AtomicU32, Ordering};

static PASSED: AtomicU32 = AtomicU32::new(0);
static LIVE: AtomicI32 = AtomicI32::new(0);
static mut PROP: i32 = 77;

#[repr(C)]
#[derive(Clone, Copy)]
struct V3 {
    x: f32,
    y: f32,
    z: f32,
}

#[repr(C)]
struct Handle {
    rc: i32,
    v: i32,
}

extern "C" fn st_report(bit: i32, ok: bool) {
    if ok {
        PASSED.fetch_or(1 << (bit & 31), Ordering::Relaxed);
    }
}
extern "C" fn st_add(a: i32, b: i32) -> i32 {
    a + b
}
extern "C" fn st_not(a: bool) -> bool {
    !a
}
extern "C" fn st_vadd(a: &V3, b: &V3) -> V3 {
    V3 {
        x: a.x + b.x,
        y: a.y + b.y,
        z: a.z + b.z,
    }
}
extern "C" fn st_vscale(this: &mut V3, k: f32) {
    this.x *= k;
    this.y *= k;
    this.z *= k;
}
extern "C" fn st_vdot(this: &V3, o: &V3) -> f32 {
    this.x * o.x + this.y * o.y + this.z * o.z
}
extern "C" fn st_vinit(this: &mut V3) {
    *this = V3 {
        x: 1.0,
        y: 2.0,
        z: 3.0,
    };
}
unsafe extern "C" fn st_slen(s: *const c_void) -> i32 {
    // SAFETY: the engine passes a live string.
    i32::try_from(unsafe { script_str(s) }.len()).unwrap_or(-1)
}
unsafe extern "C" fn st_up(s: *const c_void) -> *mut c_void {
    // SAFETY: as `st_slen`.
    new_script_str(&unsafe { script_str(s) }.to_ascii_uppercase())
}
extern "C" fn st_hnew() -> *mut Handle {
    LIVE.fetch_add(1, Ordering::Relaxed);
    Box::into_raw(Box::new(Handle { rc: 1, v: 40 }))
}
unsafe extern "C" fn st_hadd(h: *mut Handle) {
    // SAFETY: the engine passes a live object.
    unsafe { (*h).rc += 1 };
}
unsafe extern "C" fn st_hrel(h: *mut Handle) {
    // SAFETY: as `st_hadd`; freed when the last reference goes.
    unsafe {
        (*h).rc -= 1;
        if (*h).rc == 0 {
            LIVE.fetch_sub(1, Ordering::Relaxed);
            drop(Box::from_raw(h));
        }
    }
}
unsafe extern "C" fn st_hget(h: *mut Handle) -> i32 {
    // SAFETY: as `st_hadd`.
    unsafe { (*h).v }
}
unsafe extern "C" fn st_hset(h: *mut Handle, v: i32) {
    // SAFETY: as `st_hadd`.
    unsafe { (*h).v = v };
}
unsafe extern "C" fn st_hpass(h: *mut Handle) -> i32 {
    // SAFETY: the handle argument's reference belongs to the callee.
    unsafe {
        let v = (*h).v + 1;
        st_hrel(h);
        v
    }
}

const SCRIPT: &str = "
void main() {
 st_report(0, st_add(2, 3) == 5);
 st_report(1, st_not(false) && !st_not(true));
 st_report(2, A == 1 && B == 5);
 V3 a; st_report(3, a.x == 1 && a.z == 3);
 V3 c = st_vadd(a, a); c.scale(0.5f); st_report(4, c.x == 1 && c.y == 2 && c.dot(a) == 14.0f);
 st_prop = st_prop + 1;
 st_report(5, st_slen('hello') == 5);
 string u = st_up('abc'); st_report(6, u == 'ABC');
 H@ h = H(); h.set(h.get() + 2); st_report(7, h.get() == 42 && st_hpass(h) == 43);
}
";

pub fn register(e: EngineRef) -> AsResult {
    // SAFETY: every function matches its declaration and convention (the probe's rules); the property is a static.
    unsafe {
        e.global_function(
            "void st_report(int, bool)",
            st_report as *const c_void,
            CallConv::Cdecl,
        )?;
        e.global_function(
            "int st_add(int, int)",
            st_add as *const c_void,
            CallConv::Cdecl,
        )?;
        e.global_function(
            "bool st_not(bool)",
            st_not as *const c_void,
            CallConv::Cdecl,
        )?;
        e.enum_type("Mode")?;
        e.enum_value("Mode", "A", 1)?;
        e.enum_value("Mode", "B", 5)?;
        e.global_property("int st_prop", (&raw mut PROP).cast())?;
        e.object_type(
            "V3",
            size_of::<V3>(),
            obj::VALUE | obj::POD | obj::APP_CLASS | obj::APP_CLASS_ALLFLOATS,
        )?;
        e.object_property("V3", "float x", 0)?;
        e.object_property("V3", "float y", 4)?;
        e.object_property("V3", "float z", 8)?;
        e.object_behaviour(
            Behaviour::Construct,
            "V3",
            "void f()",
            st_vinit as *const c_void,
            CallConv::CdeclObjFirst,
        )?;
        e.global_function(
            "V3 st_vadd(const V3 &in, const V3 &in)",
            st_vadd as *const c_void,
            CallConv::Cdecl,
        )?;
        e.object_method(
            "V3",
            "void scale(float)",
            st_vscale as *const c_void,
            CallConv::CdeclObjFirst,
        )?;
        e.object_method(
            "V3",
            "float dot(const V3 &in) const",
            st_vdot as *const c_void,
            CallConv::CdeclObjFirst,
        )?;
        e.global_function(
            "int st_slen(const string &in)",
            st_slen as *const c_void,
            CallConv::Cdecl,
        )?;
        e.global_function(
            "string@ st_up(const string &in)",
            st_up as *const c_void,
            CallConv::Cdecl,
        )?;
        e.object_type("H", 0, obj::REF)?;
        e.object_behaviour(
            Behaviour::Factory,
            "H",
            "H@ f()",
            st_hnew as *const c_void,
            CallConv::Cdecl,
        )?;
        e.object_behaviour(
            Behaviour::AddRef,
            "H",
            "void f()",
            st_hadd as *const c_void,
            CallConv::CdeclObjFirst,
        )?;
        e.object_behaviour(
            Behaviour::Release,
            "H",
            "void f()",
            st_hrel as *const c_void,
            CallConv::CdeclObjFirst,
        )?;
        e.object_method(
            "H",
            "int get()",
            st_hget as *const c_void,
            CallConv::CdeclObjFirst,
        )?;
        e.object_method(
            "H",
            "void set(int)",
            st_hset as *const c_void,
            CallConv::CdeclObjFirst,
        )?;
        e.global_function(
            "int st_hpass(H@)",
            st_hpass as *const c_void,
            CallConv::Cdecl,
        )
    }
}

/// 0 on success, else a bitmask of failed checks (bit 8 prop write, 9 leaked handle, 10 setup, 11 build).
pub fn run() -> u32 {
    PASSED.store(0, Ordering::Relaxed);
    // SAFETY: the selftest is the only user of `PROP`.
    unsafe { PROP = 77 };
    let Some(engine) = ScriptEngine::new() else {
        return 1 << 10;
    };
    if register(engine.engine()).is_err() {
        return 1 << 10;
    }
    let mut s = {
        // SAFETY: `engine` outlives `s`.
        unsafe { Script::new(engine.engine(), std::ptr::from_ref(&*engine.output), 0, 0) }
    };
    if s.build(SCRIPT.as_bytes()).1.is_err() {
        return 1 << 11;
    }
    let h = s.func_handle("main");
    if h < 0 || !s.run_handle(h) {
        return 1 << 11;
    }
    drop(s);
    // SAFETY: single-threaded read after the run.
    let prop = unsafe { PROP };
    let mut ok = PASSED.load(Ordering::Relaxed);
    ok |= u32::from(prop == 78) << 8;
    ok |= u32::from(LIVE.load(Ordering::Relaxed) == 0) << 9;
    !ok & 0x3FF
}
