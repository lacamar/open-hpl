//! Recording fakes shared by the GL table and the SDL entry points. The formatting and the return
//! values mirror `rust/tests/golden/L2/harness/fakecommon.cpp` so traces compare byte for byte.
#![allow(non_snake_case, clippy::missing_safety_doc)]

use core::ffi::{c_char, c_int, c_void};
use std::cell::{Cell, RefCell};
use std::ffi::CString;
use std::thread::LocalKey;

use crate::sys::gl::GLDEBUGPROC;
use crate::sys::sdl2::SDL_Window;

thread_local! {
    pub static TRACE: RefCell<String> = const { RefCell::new(String::new()) };
    pub static EXT: RefCell<CString> = RefCell::new(CString::default());
    pub static RANGES: RefCell<Vec<(usize, usize, String)>> = const { RefCell::new(Vec::new()) };
    pub static GEN: Cell<u32> = const { Cell::new(0) };
    pub static SHADERS: Cell<u32> = const { Cell::new(0) };
    pub static PROGRAMS: Cell<u32> = const { Cell::new(0) };
    pub static WIN: Cell<(i32, i32)> = const { Cell::new((0, 0)) };
    pub static ERRORS: Cell<i32> = const { Cell::new(0) };
    pub static FAIL_COMPILE: Cell<i32> = const { Cell::new(0) };
    pub static FAIL_LINK: Cell<i32> = const { Cell::new(0) };
    pub static FB_STATUS: Cell<u32> = const { Cell::new(0x8CD5) };
}

/// `glGetShaderiv` / `glGetProgramiv` of the fake: a failing status counts down, log length is fixed.
pub fn status_param(
    fail: &'static LocalKey<Cell<i32>>,
    pname: u32,
    status: u32,
    len: u32,
    len_value: i32,
    extra: Option<(u32, i32)>,
) -> i32 {
    if pname == status {
        fail.with(|f| {
            if f.get() > 0 {
                f.set(f.get() - 1);
                0
            } else {
                1
            }
        })
    } else if pname == len {
        len_value
    } else {
        extra.filter(|(p, _)| *p == pname).map_or(1, |(_, v)| v)
    }
}

const FULL: &str = "GL_ARB_multitexture GL_ARB_vertex_buffer_object GL_EXT_stencil_two_side GL_EXT_texture_filter_anisotropic GL_ARB_multisample GL_ARB_texture_compression GL_EXT_texture_compression_s3tc GL_SGIS_generate_mipmap GL_EXT_framebuffer_object GL_EXT_packed_depth_stencil GL_ARB_texture_float GL_ARB_fragment_program GL_ARB_fragment_shader GL_EXT_gpu_shader4 GL_EXT_blend_func_separate GL_ARB_occlusion_query GL_ARB_texture_rectangle GL_ARB_shading_language_100";
const LIMITED: &str = "GL_ARB_multitexture GL_ARB_vertex_buffer_object GL_ATI_separate_stencil GL_EXT_framebuffer_object GL_ARB_texture_compression GL_ARB_fragment_shader GL_ATI_shader_texture_lod GL_ARB_occlusion_query";

pub fn reset(set: &str) {
    TRACE.with(|t| t.borrow_mut().clear());
    RANGES.with(|r| r.borrow_mut().clear());
    for k in [&GEN, &SHADERS, &PROGRAMS] {
        k.with(|c| c.set(0));
    }
    WIN.with(|w| w.set((0, 0)));
    ERRORS.with(|c| c.set(0));
    FAIL_COMPILE.with(|c| c.set(0));
    FAIL_LINK.with(|c| c.set(0));
    FB_STATUS.with(|c| c.set(0x8CD5));
    let ext = match set {
        "B" => LIMITED.to_string(),
        "C" => format!("{FULL} GL_EXT_secondary_color"),
        _ => FULL.to_string(),
    };
    EXT.with(|e| *e.borrow_mut() = CString::new(ext).unwrap_or_default());
}

pub fn take_trace() -> String {
    TRACE.with(|t| std::mem::take(&mut *t.borrow_mut()))
}

pub fn add_range(base: usize, bytes: usize, label: &str) {
    RANGES.with(|r| r.borrow_mut().push((base, bytes, label.to_string())));
}

pub fn bump(k: &'static LocalKey<Cell<u32>>) -> u32 {
    k.with(|c| {
        c.set(c.get() + 1);
        c.get()
    })
}

pub unsafe fn gen_ids(n: i32, p: *mut u32) {
    for i in 0..usize::try_from(n).unwrap_or(0) {
        unsafe { *p.add(i) = bump(&GEN) };
    }
}

pub fn fnv(bytes: &[u8]) -> u64 {
    bytes.iter().fold(1_469_598_103_934_665_603u64, |h, b| {
        (h ^ u64::from(*b)).wrapping_mul(1_099_511_628_211)
    })
}

pub trait FmtArg {
    fn fmt_arg(self) -> String;
}

macro_rules! int_arg {
    ($($t:ty),*) => {$(impl FmtArg for $t {
        fn fmt_arg(self) -> String { self.to_string() }
    })*};
}
int_arg!(u32, i32, u8, isize);

impl FmtArg for f32 {
    fn fmt_arg(self) -> String {
        format!("f{:08x}", self.to_bits())
    }
}

impl FmtArg for f64 {
    fn fmt_arg(self) -> String {
        format!("d{:016x}", self.to_bits())
    }
}

fn ptr_arg(p: usize) -> String {
    if p == 0 {
        return "null".into();
    }
    RANGES.with(|r| {
        r.borrow()
            .iter()
            .find(|(base, bytes, _)| p >= *base && p < base + bytes)
            .map_or_else(
                || "p".to_string(),
                |(base, _, l)| format!("{l}+{}", p - base),
            )
    })
}

impl<T> FmtArg for *const T {
    fn fmt_arg(self) -> String {
        ptr_arg(self as usize)
    }
}

impl<T> FmtArg for *mut T {
    fn fmt_arg(self) -> String {
        ptr_arg(self as usize)
    }
}

impl FmtArg for Option<GLDEBUGPROC> {
    fn fmt_arg(self) -> String {
        ptr_arg(self.map_or(0, |f| f as usize))
    }
}

pub fn rec(name: &str, args: &[String], extra: &str) {
    TRACE.with(|t| {
        let mut t = t.borrow_mut();
        t.push_str(name);
        t.push('(');
        t.push_str(&args.join(","));
        t.push(')');
        t.push_str(extra);
        t.push('\n');
    });
}

unsafe extern "C" fn unknown() -> i64 {
    rec("unknown_gl", &[], "");
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GL_GetProcAddress(name: *const c_char) -> *mut c_void {
    let name = unsafe { std::ffi::CStr::from_ptr(name) }.to_string_lossy();
    let f = super::fakegl::lookup(&name);
    if !f.is_null() {
        f
    } else if name.starts_with("gl") && !name.starts_with("glX") {
        unknown as *const () as *mut c_void
    } else {
        core::ptr::null_mut()
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn gluScaleImage(
    format: u32,
    w_in: i32,
    h_in: i32,
    type_in: u32,
    data_in: *const c_void,
    w_out: i32,
    h_out: i32,
    type_out: u32,
    data_out: *mut c_void,
) -> i32 {
    rec(
        "gluScaleImage",
        &[
            format.fmt_arg(),
            w_in.fmt_arg(),
            h_in.fmt_arg(),
            type_in.fmt_arg(),
            data_in.fmt_arg(),
            w_out.fmt_arg(),
            h_out.fmt_arg(),
            type_out.fmt_arg(),
            data_out.fmt_arg(),
        ],
        "",
    );
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn gluBuild1DMipmaps(
    target: u32,
    internal: i32,
    width: i32,
    format: u32,
    type_: u32,
    data: *const c_void,
) -> i32 {
    rec(
        "gluBuild1DMipmaps",
        &[
            target.fmt_arg(),
            internal.fmt_arg(),
            width.fmt_arg(),
            format.fmt_arg(),
            type_.fmt_arg(),
            data.fmt_arg(),
        ],
        "",
    );
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn gluBuild2DMipmaps(
    target: u32,
    internal: i32,
    width: i32,
    height: i32,
    format: u32,
    type_: u32,
    data: *const c_void,
) -> i32 {
    rec(
        "gluBuild2DMipmaps",
        &[
            target.fmt_arg(),
            internal.fmt_arg(),
            width.fmt_arg(),
            height.fmt_arg(),
            format.fmt_arg(),
            type_.fmt_arg(),
            data.fmt_arg(),
        ],
        "",
    );
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GL_SetAttribute(a: c_int, v: c_int) -> c_int {
    rec("SDL_GL_SetAttribute", &[a.fmt_arg(), v.fmt_arg()], "");
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GetCurrentVideoDriver() -> *const c_char {
    rec("SDL_GetCurrentVideoDriver", &[], "");
    c"wayland".as_ptr()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GetError() -> *const c_char {
    c"fake error".as_ptr()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_CreateWindow(
    title: *const c_char,
    x: c_int,
    y: c_int,
    w: c_int,
    h: c_int,
    flags: u32,
) -> *mut SDL_Window {
    let t = unsafe { std::ffi::CStr::from_ptr(title) }.to_string_lossy();
    rec(
        "SDL_CreateWindow",
        &[
            x.fmt_arg(),
            y.fmt_arg(),
            w.fmt_arg(),
            h.fmt_arg(),
            flags.fmt_arg(),
        ],
        &format!(" {t}"),
    );
    WIN.with(|c| c.set((w, h)));
    0x1000 as *mut SDL_Window
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GetWindowSize(win: *mut SDL_Window, w: *mut c_int, h: *mut c_int) {
    rec("SDL_GetWindowSize", &[win.fmt_arg()], "");
    let (ww, hh) = WIN.with(Cell::get);
    unsafe {
        *w = ww;
        *h = hh;
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GL_CreateContext(win: *mut SDL_Window) -> *mut c_void {
    rec("SDL_GL_CreateContext", &[win.fmt_arg()], "");
    0x2000 as *mut c_void
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_ShowCursor(t: c_int) -> c_int {
    rec("SDL_ShowCursor", &[t.fmt_arg()], "");
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_SetWindowGrab(win: *mut SDL_Window, g: c_int) {
    rec("SDL_SetWindowGrab", &[win.fmt_arg(), g.fmt_arg()], "");
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_SetRelativeMouseMode(e: c_int) -> c_int {
    rec("SDL_SetRelativeMouseMode", &[e.fmt_arg()], "");
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_SetWindowTitle(win: *mut SDL_Window, t: *const c_char) {
    let t = unsafe { std::ffi::CStr::from_ptr(t) }.to_string_lossy();
    rec("SDL_SetWindowTitle", &[win.fmt_arg()], &format!(" {t}"));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GetWindowFlags(win: *mut SDL_Window) -> u32 {
    rec("SDL_GetWindowFlags", &[win.fmt_arg()], "");
    0x4 | 0x400
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GL_SetSwapInterval(i: c_int) -> c_int {
    rec("SDL_GL_SetSwapInterval", &[i.fmt_arg()], "");
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_SetWindowFullscreen(win: *mut SDL_Window, f: u32) -> c_int {
    rec("SDL_SetWindowFullscreen", &[win.fmt_arg(), f.fmt_arg()], "");
    0
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_SetWindowSize(win: *mut SDL_Window, w: c_int, h: c_int) {
    rec(
        "SDL_SetWindowSize",
        &[win.fmt_arg(), w.fmt_arg(), h.fmt_arg()],
        "",
    );
    WIN.with(|c| c.set((w, h)));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_GL_SwapWindow(win: *mut SDL_Window) {
    rec("SDL_GL_SwapWindow", &[win.fmt_arg()], "");
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn SDL_DestroyWindow(win: *mut SDL_Window) {
    rec("SDL_DestroyWindow", &[win.fmt_arg()], "");
}
