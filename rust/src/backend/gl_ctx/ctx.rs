//! `cLowLevelGraphicsSDL` GL calls. Every method replays the `gl*` sequence the C++ member made;
//! the filters and enum tables come from `crate::gl`.

use core::ffi::{CStr, c_char, c_void};
use std::ffi::CString;
use std::sync::OnceLock;

use super::caps::Caps;
use crate::gl::state::{self, GlState};
use crate::gl::tables;
use crate::gl::vbuf::{self, MAX_OPS, Op};
use crate::sys::gl::{self as g, GlFns};
use crate::sys::sdl2 as sdl;
use crate::types::Vector2l;

static TABLE: OnceLock<GlFns> = OnceLock::new();
static EMPTY: OnceLock<GlFns> = OnceLock::new();

/// The process-wide GL entry points; `None` until a context is loaded.
pub fn table() -> Option<&'static GlFns> {
    TABLE.get()
}

fn empty() -> &'static GlFns {
    EMPTY.get_or_init(GlFns::default)
}

pub const LOG_INFO: i32 = 0;
pub const LOG_ERROR: i32 = 1;
pub const LOG_FATAL: i32 = 2;
pub const LOG_WARN_BOX: i32 = 3;

pub type HostLog = unsafe extern "C" fn(kind: i32, msg: *const c_char);

static HOST_LOG: OnceLock<HostLog> = OnceLock::new();

pub fn set_host_log(f: HostLog) {
    let _ = HOST_LOG.set(f);
}

/// Routes a message to the engine log (`Log`, `Error`, `FatalError`, a warning box).
pub fn host_log(kind: i32, msg: &str) {
    let Some(f) = HOST_LOG.get() else {
        return;
    };
    let text = CString::new(msg.replace('\0', "?")).unwrap_or_default();
    // SAFETY: the registered host function takes a NUL-terminated string.
    unsafe { f(kind, text.as_ptr()) }
}

macro_rules! gl {
    ($s:expr, $f:ident($($a:expr),*)) => {
        if let Some(f) = $s.gl.$f {
            // SAFETY: entry point resolved for the current context with its C signature.
            unsafe { f($($a),*) }
        }
    };
}

macro_rules! gl_of {
    ($gl:expr, $f:ident($($a:expr),*)) => {
        if let Some(f) = $gl.$f {
            // SAFETY: entry point resolved for the current context with its C signature.
            unsafe { f($($a),*) }
        }
    };
}

macro_rules! sdl_call {
    ($s:expr, $f:ident($($a:expr),*)) => {{
        let _ = &$s;
        // SAFETY: SDL entry point; `window` is null or the window this context created.
        unsafe { sdl::$f($($a),*) }
    }};
}

fn attr(a: i32, v: i32) -> i32 {
    // SAFETY: plain SDL call.
    unsafe { sdl::SDL_GL_SetAttribute(a, v) }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct WindowCfg {
    pub width: i32,
    pub height: i32,
    pub display: i32,
    pub bpp: i32,
    pub fullscreen: bool,
    pub multisampling: i32,
    pub headless: bool,
    pub caption: *const c_char,
    /// Outputs: size obtained, 1 when the 640x480 fallback was used.
    pub out_w: i32,
    pub out_h: i32,
    pub out_fallback: bool,
}

pub struct Sources {
    pub element: *const *const c_void,
    pub batch_vertex: *const f32,
    pub batch_texcoord: *const *mut f32,
}

pub struct Ctx {
    gl: &'static GlFns,
    pub caps: Caps,
    state: GlState,
    double_sided_stencil: bool,
    screen: Vector2l,
    fb_pos: Vector2l,
    fb_size: Vector2l,
    fb_total: Vector2l,
    gamma_program: u32,
    gamma_texture: u32,
    gamma_tex_size: Vector2l,
    window: *mut sdl::SDL_Window,
    multisampling: i32,
}

const ZERO: Vector2l = Vector2l { x: 0, y: 0 };

impl Ctx {
    pub fn new() -> Self {
        Self::with_table(empty())
    }

    pub fn with_table(gl: &'static GlFns) -> Self {
        Self {
            gl,
            caps: Caps::default(),
            state: GlState::new(),
            double_sided_stencil: false,
            screen: ZERO,
            fb_pos: ZERO,
            fb_size: ZERO,
            fb_total: ZERO,
            gamma_program: 0,
            gamma_texture: 0,
            gamma_tex_size: ZERO,
            window: core::ptr::null_mut(),
            multisampling: 0,
        }
    }

    pub fn window(&self) -> *mut c_void {
        self.window.cast()
    }

    pub fn screen(&self) -> Vector2l {
        self.screen
    }

    // ---- window and context ----

    /// `Init` up to and including `SDL_GL_CreateContext`. Returns false after a fatal error.
    ///
    /// # Safety
    /// `cfg.caption` is a NUL-terminated string.
    pub unsafe fn create_window(&mut self, cfg: &mut WindowCfg) -> bool {
        self.screen = Vector2l {
            x: cfg.width,
            y: cfg.height,
        };
        self.multisampling = cfg.multisampling;
        attr(sdl::SDL_GL_DOUBLEBUFFER, 1);
        attr(sdl::SDL_GL_RED_SIZE, 8);
        attr(sdl::SDL_GL_GREEN_SIZE, 8);
        attr(sdl::SDL_GL_BLUE_SIZE, 8);
        attr(sdl::SDL_GL_ALPHA_SIZE, 8);
        attr(sdl::SDL_GL_DEPTH_SIZE, 24);
        attr(sdl::SDL_GL_STENCIL_SIZE, 8);
        if self.multisampling > 0 {
            if attr(sdl::SDL_GL_MULTISAMPLEBUFFERS, 1) == -1 {
                host_log(LOG_ERROR, "Multisample buffers not supported!\n");
            } else if attr(sdl::SDL_GL_MULTISAMPLESAMPLES, self.multisampling) == -1 {
                host_log(
                    LOG_ERROR,
                    &format!(
                        "Couldn't set multisampling samples to {}\n",
                        self.multisampling
                    ),
                );
            }
        }
        let mut flags = sdl::SDL_WINDOW_OPENGL;
        if cfg.width == 0 && cfg.height == 0 {
            self.screen = Vector2l { x: 800, y: 600 };
            flags |= sdl::SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else if cfg.fullscreen {
            // SAFETY: SDL returns null or a static string.
            let driver = unsafe { sdl::SDL_GetCurrentVideoDriver() };
            let wayland = !driver.is_null()
                // SAFETY: non-null NUL-terminated driver name.
                && unsafe { CStr::from_ptr(driver) }
                    .to_string_lossy()
                    .to_ascii_lowercase()
                    == "wayland";
            flags |= if wayland {
                sdl::SDL_WINDOW_FULLSCREEN_DESKTOP
            } else {
                sdl::SDL_WINDOW_FULLSCREEN
            };
        }
        if cfg.headless {
            flags |= sdl::SDL_WINDOW_HIDDEN;
        }
        host_log(
            LOG_INFO,
            &format!(
                " Setting video mode: {} x {} - {} bpp\n",
                cfg.width, cfg.height, cfg.bpp
            ),
        );
        let pos = sdl::SDL_WINDOWPOS_CENTERED_DISPLAY(u32::try_from(cfg.display).unwrap_or(0));
        let pos = i32::try_from(pos).unwrap_or(i32::MAX);
        let caption = cfg.caption;
        // SAFETY: caption is NUL-terminated by the caller contract.
        let create = |s: &Self| unsafe {
            sdl::SDL_CreateWindow(caption, pos, pos, s.screen.x, s.screen.y, flags)
        };
        self.window = create(self);
        cfg.out_fallback = false;
        if self.window.is_null() {
            host_log(
                LOG_ERROR,
                &format!(
                    "Could not set display mode setting a lower one! {}\n",
                    sdl_error()
                ),
            );
            self.screen = Vector2l { x: 640, y: 480 };
            attr(sdl::SDL_GL_MULTISAMPLEBUFFERS, 0);
            attr(sdl::SDL_GL_MULTISAMPLESAMPLES, 0);
            self.window = create(self);
            if self.window.is_null() {
                host_log(
                    LOG_FATAL,
                    &format!("Unable to initialize display! {}\n", sdl_error()),
                );
                return false;
            }
            cfg.out_fallback = true;
            host_log(
                LOG_WARN_BOX,
                "Could not set displaymode and 640x480 is used instead!\n",
            );
        }
        let (mut w, mut h) = (0, 0);
        // SAFETY: live window, two writable ints.
        unsafe { sdl::SDL_GetWindowSize(self.window, &mut w, &mut h) };
        self.screen = Vector2l { x: w, y: h };
        cfg.out_w = w;
        cfg.out_h = h;
        // SAFETY: live window.
        unsafe { sdl::SDL_GL_CreateContext(self.window) };
        true
    }

    /// Resolves every entry point and derives the capability flags; needs a current context.
    pub fn load(&mut self) {
        // SAFETY: SDL_GL_GetProcAddress takes NUL-terminated names and returns null or the entry point.
        let table = TABLE.get_or_init(|| unsafe { GlFns::load(sdl::SDL_GL_GetProcAddress) });
        self.gl = table;
        self.derive_caps(|name| {
            CString::new(name)
                // SAFETY: NUL-terminated name.
                .is_ok_and(|n| !unsafe { sdl::SDL_GL_GetProcAddress(n.as_ptr()) }.is_null())
        });
    }

    pub fn derive_caps(&mut self, has_proc: impl Fn(&str) -> bool) {
        let ext = self.get_string(g::GL_EXTENSIONS);
        let ext = if ext.is_null() {
            String::new()
        } else {
            // SAFETY: glGetString returns a NUL-terminated string.
            unsafe { CStr::from_ptr(ext.cast()) }
                .to_string_lossy()
                .into_owned()
        };
        self.caps = Caps::from_extensions(&ext, &has_proc);
        crate::backend::gl_res::set_caps(
            self.caps.framebuffer_object,
            self.caps.packed_depth_stencil,
        );
    }

    pub fn debug_from_env(&self) {
        let Some(mode) = std::env::var_os("OPENHPL_GL_DEBUG") else {
            return;
        };
        if self.gl.glDebugMessageCallback.is_none() {
            return;
        }
        gl!(self, glEnable(g::GL_DEBUG_OUTPUT));
        gl!(self, glEnable(g::GL_DEBUG_OUTPUT_SYNCHRONOUS));
        let user = if mode == "all" {
            core::ptr::NonNull::<c_void>::dangling()
                .as_ptr()
                .cast_const()
        } else {
            core::ptr::null()
        };
        gl!(self, glDebugMessageCallback(Some(debug_callback), user));
    }

    pub fn show_cursor(&self, on: bool) {
        sdl_call!(
            self,
            SDL_ShowCursor(if on {
                sdl::SDL_ENABLE
            } else {
                sdl::SDL_DISABLE
            })
        );
    }

    pub fn window_grab(&self, on: bool, headless: bool) {
        if headless || self.window.is_null() {
            return;
        }
        sdl_call!(
            self,
            SDL_SetWindowGrab(self.window, if on { sdl::SDL_TRUE } else { sdl::SDL_FALSE })
        );
    }

    pub fn relative_mouse(&self, on: bool, headless: bool) {
        if headless {
            return;
        }
        sdl_call!(
            self,
            SDL_SetRelativeMouseMode(if on { sdl::SDL_TRUE } else { sdl::SDL_FALSE })
        );
    }

    /// # Safety
    /// `caption` is NUL-terminated.
    pub unsafe fn set_caption(&self, caption: *const c_char) {
        sdl_call!(self, SDL_SetWindowTitle(self.window, caption));
    }

    fn window_flag(&self, flag: u32) -> bool {
        sdl_call!(self, SDL_GetWindowFlags(self.window)) & flag != 0
    }

    pub fn mouse_focus(&self) -> bool {
        self.window_flag(sdl::SDL_WINDOW_MOUSE_FOCUS)
    }

    pub fn input_focus(&self) -> bool {
        self.window_flag(sdl::SDL_WINDOW_INPUT_FOCUS)
    }

    pub fn is_visible(&self) -> bool {
        self.window_flag(sdl::SDL_WINDOW_SHOWN)
    }

    pub fn set_vsync(&self, on: bool, adaptive: bool) {
        let interval = if on { if adaptive { -1 } else { 1 } } else { 0 };
        sdl_call!(self, SDL_GL_SetSwapInterval(interval));
    }

    pub fn force_window_size(&self, w: i32, h: i32) -> bool {
        if self.window.is_null() {
            return false;
        }
        sdl_call!(self, SDL_SetWindowFullscreen(self.window, 0));
        sdl_call!(self, SDL_SetWindowSize(self.window, w, h));
        true
    }

    /// `CheckAndUpdateScreenSize`: the new size when it changed.
    pub fn check_screen(&mut self) -> Option<Vector2l> {
        if self.window.is_null() {
            return None;
        }
        let (mut w, mut h) = (0, 0);
        // SAFETY: live window, two writable ints.
        unsafe { sdl::SDL_GetWindowSize(self.window, &mut w, &mut h) };
        if w <= 0 || h <= 0 || (w == self.screen.x && h == self.screen.y) {
            return None;
        }
        self.screen = Vector2l { x: w, y: h };
        Some(self.screen)
    }

    pub fn destroy_window(&mut self) {
        if !self.window.is_null() {
            sdl_call!(self, SDL_DestroyWindow(self.window));
            self.window = core::ptr::null_mut();
        }
    }

    // ---- setup and queries ----

    pub fn setup_gl(&mut self) {
        self.state.setup();
        let (pos, size) = (ZERO, self.screen);
        self.fb_pos = ZERO;
        self.fb_size = self.screen;
        self.fb_total = self.screen;
        gl!(self, glShadeModel(g::GL_SMOOTH));
        gl!(self, glClearColor(0.0, 0.0, 0.0, 0.0));
        gl!(self, glClearDepth(1.0));
        gl!(self, glEnable(g::GL_DEPTH_TEST));
        gl!(self, glDepthFunc(g::GL_LEQUAL));
        gl!(self, glDisable(g::GL_ALPHA_TEST));
        gl!(self, glDepthMask(g::GL_TRUE));
        gl!(self, glEnable(g::GL_DEPTH_TEST));
        gl!(self, glDepthFunc(tables::compare(self.state.depth_func)));
        gl!(
            self,
            glAlphaFunc(tables::compare(self.state.alpha_func), self.state.alpha_ref)
        );
        gl!(self, glEnable(g::GL_CULL_FACE));
        gl!(self, glCullFace(g::GL_BACK));
        gl!(self, glFrontFace(g::GL_CW));
        gl!(self, glDisable(g::GL_SCISSOR_TEST));
        gl!(
            self,
            glScissor(
                pos.x,
                size.y
                    .wrapping_sub(pos.y)
                    .wrapping_sub(1)
                    .wrapping_sub(size.y),
                size.x,
                size.y
            )
        );
        gl!(self, glDisable(g::GL_BLEND));
        gl!(
            self,
            glHint(g::GL_PERSPECTIVE_CORRECTION_HINT, g::GL_NICEST)
        );
        gl!(self, glClearStencil(0));
        gl!(
            self,
            glClear(g::GL_COLOR_BUFFER_BIT | g::GL_DEPTH_BUFFER_BIT | g::GL_STENCIL_BUFFER_BIT)
        );
        gl!(self, glMatrixMode(g::GL_MODELVIEW));
        gl!(self, glLoadIdentity());
        gl!(self, glMatrixMode(g::GL_PROJECTION));
        gl!(self, glLoadIdentity());
        gl!(self, glEnableClientState(g::GL_VERTEX_ARRAY));
        gl!(self, glEnableClientState(g::GL_COLOR_ARRAY));
        gl!(self, glEnableClientState(g::GL_TEXTURE_COORD_ARRAY));
        gl!(self, glDisableClientState(g::GL_NORMAL_ARRAY));
        gl!(self, glDisableClientState(g::GL_INDEX_ARRAY));
        gl!(self, glDisableClientState(g::GL_EDGE_FLAG_ARRAY));
    }

    pub fn get_string(&self, name: u32) -> *const u8 {
        match self.gl.glGetString {
            // SAFETY: entry point resolved for the current context.
            Some(f) => unsafe { f(name) },
            None => core::ptr::null(),
        }
    }

    fn get_int(&self, pname: u32) -> i32 {
        let mut v = 0;
        gl!(self, glGetIntegerv(pname, &mut v));
        v
    }

    /// `GetCaps`; `kind` is `eGraphicCaps`.
    pub fn get_caps(&self, kind: i32, force_sm34_off: bool) -> i32 {
        let c = &self.caps;
        let flag = |b: bool| i32::from(b);
        match kind {
            0 | 16 => 1,
            1 => flag(c.vertex_buffer_object),
            2 => flag(c.stencil_two_side || c.separate_stencil),
            3 => self.get_int(g::GL_MAX_TEXTURE_IMAGE_UNITS_ARB),
            4 => self.get_int(g::GL_MAX_TEXTURE_COORDS_ARB),
            5 => self.get_int(g::GL_MAX_CLIP_PLANES),
            6 => flag(c.anisotropic),
            7 => {
                if !c.anisotropic {
                    return 0;
                }
                let mut f = 0.0f32;
                gl!(
                    self,
                    glGetFloatv(g::GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &mut f)
                );
                f as i32
            }
            8 => flag(c.multisample),
            9 => flag(c.texture_compression),
            10 => flag(c.s3tc),
            11 => flag(c.generate_mipmap),
            12 => flag(c.framebuffer_object),
            13 => self.get_int(g::GL_MAX_DRAW_BUFFERS),
            14 => flag(c.packed_depth_stencil),
            15 => flag(c.texture_float),
            17 => flag(c.fragment_program || c.fragment_shader),
            18 => flag(!force_sm34_off && (c.nv_vertex_program3 || c.ati_shader_texture_lod)),
            19 => flag(!force_sm34_off && c.gpu_shader4),
            20 => flag(c.ati_fragment_shader),
            21 => self.get_int(g::GL_MAX_COLOR_ATTACHMENTS_EXT),
            _ => 0,
        }
    }

    pub fn set_multisampling_active(&self, on: bool) {
        if !self.caps.multisample || self.multisampling <= 0 {
            return;
        }
        if on {
            gl!(self, glEnable(g::GL_MULTISAMPLE_ARB));
        } else {
            gl!(self, glDisable(g::GL_MULTISAMPLE_ARB));
        }
    }

    // ---- frame buffer ----

    pub fn clear(&self, flags: u32) {
        let mut mask = 0;
        if flags & 1 != 0 {
            mask |= g::GL_COLOR_BUFFER_BIT;
        }
        if flags & 2 != 0 {
            mask |= g::GL_DEPTH_BUFFER_BIT;
        }
        if flags & 4 != 0 {
            mask |= g::GL_STENCIL_BUFFER_BIT;
        }
        gl!(self, glClear(mask));
    }

    pub fn set_clear_color(&self, r: f32, gr: f32, b: f32, a: f32) {
        gl!(self, glClearColor(r, gr, b, a));
    }

    pub fn set_clear_depth(&self, d: f32) {
        gl!(self, glClearDepth(f64::from(d)));
    }

    pub fn set_clear_stencil(&self, v: i32) {
        gl!(self, glClearStencil(v));
    }

    pub fn copy_to_texture(
        &self,
        pos: Vector2l,
        size: Vector2l,
        tex_off: Vector2l,
        tex_type: i32,
        tex_height: i32,
    ) {
        let r = state::copy_to_texture(
            self.fb_pos,
            self.fb_size,
            self.fb_total,
            pos,
            size,
            tex_off,
            tex_height,
        );
        let [a, b, c, d, e, f] = r;
        gl!(
            self,
            glCopyTexSubImage2D(tables::texture_target(tex_type), 0, a, b, c, d, e, f)
        );
    }

    pub fn read_rect(&self, pos: Vector2l, size: Vector2l) -> [i32; 4] {
        state::read_rect(self.fb_pos, self.fb_size, self.fb_total, pos, size)
    }

    /// # Safety
    /// `data` has room for `w * h` pixels of `format`.
    pub unsafe fn read_pixels(&self, rect: [i32; 4], format: i32, data: *mut c_void) {
        let [x, y, w, h] = rect;
        gl!(
            self,
            glReadPixels(
                x,
                y,
                w,
                h,
                tables::pixel_format(format),
                g::GL_UNSIGNED_BYTE,
                data
            )
        );
    }

    pub fn bind_framebuffer(&self, handle: u32) {
        gl!(self, glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, handle));
    }

    pub fn set_viewport(&mut self, target: Vector2l, pos: Vector2l, size: Vector2l) {
        let vp = state::viewport(target, pos, size);
        self.fb_total = vp.total;
        self.fb_size = vp.size;
        self.fb_pos = vp.pos;
        let [x, y, w, h] = vp.gl;
        gl!(self, glViewport(x, y, w, h));
    }

    pub fn set_draw_targets(&self, targets: &[i32], count: i32) {
        let mut attachments = [0u32; 64];
        state::draw_targets(targets, &mut attachments);
        let num = count.min(64);
        let ptr = if num > 0 {
            attachments.as_ptr()
        } else {
            core::ptr::null()
        };
        gl!(self, glDrawBuffers(num, ptr));
    }

    pub fn finish(&self) {
        gl!(self, glFinish());
    }

    pub fn flush(&self) {
        gl!(self, glFlush());
    }

    pub fn swap(&mut self, gamma: f32) {
        if (gamma - 1.0).abs() > 0.001 && gamma > 0.0 {
            self.apply_shader_gamma(gamma);
        }
        sdl_call!(self, SDL_GL_SwapWindow(self.window));
    }

    fn apply_shader_gamma(&mut self, gamma: f32) {
        if self.gamma_program == 0 {
            let vtx = c"#version 120\nvarying vec2 uv;\nvoid main(){ uv = gl_Vertex.xy*0.5+0.5; gl_Position = gl_Vertex; }\n";
            let frag = c"#version 120\nuniform sampler2D tex; uniform float invGamma; varying vec2 uv;\nvoid main(){ vec4 c = texture2D(tex, uv); gl_FragColor = vec4(pow(c.rgb, vec3(invGamma)), c.a); }\n";
            let mut vs = 0;
            let mut fs = 0;
            if let Some(f) = self.gl.glCreateShader {
                // SAFETY: entry point resolved for the current context.
                vs = unsafe { f(g::GL_VERTEX_SHADER) };
                // SAFETY: as above.
                fs = unsafe { f(g::GL_FRAGMENT_SHADER) };
            }
            let (pv, pf) = (vtx.as_ptr().cast::<i8>(), frag.as_ptr().cast::<i8>());
            gl!(self, glShaderSource(vs, 1, &pv, core::ptr::null()));
            gl!(self, glCompileShader(vs));
            gl!(self, glShaderSource(fs, 1, &pf, core::ptr::null()));
            gl!(self, glCompileShader(fs));
            if let Some(f) = self.gl.glCreateProgram {
                // SAFETY: entry point resolved for the current context.
                self.gamma_program = unsafe { f() };
            }
            gl!(self, glAttachShader(self.gamma_program, vs));
            gl!(self, glAttachShader(self.gamma_program, fs));
            gl!(self, glLinkProgram(self.gamma_program));
            gl!(self, glDeleteShader(vs));
            gl!(self, glDeleteShader(fs));
            gl!(self, glGenTextures(1, &mut self.gamma_texture));
        }
        let (mut program, mut fbo, mut active, mut tex, mut array) = (0, 0, 0, 0, 0);
        gl!(self, glGetIntegerv(g::GL_CURRENT_PROGRAM, &mut program));
        gl!(self, glGetIntegerv(g::GL_FRAMEBUFFER_BINDING, &mut fbo));
        gl!(self, glGetIntegerv(g::GL_ACTIVE_TEXTURE, &mut active));
        gl!(self, glGetIntegerv(g::GL_ARRAY_BUFFER_BINDING, &mut array));
        gl!(self, glActiveTexture(g::GL_TEXTURE0));
        gl!(self, glGetIntegerv(g::GL_TEXTURE_BINDING_2D, &mut tex));
        gl!(self, glPushAttrib(g::GL_ALL_ATTRIB_BITS));
        gl!(self, glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, 0));
        gl!(self, glBindBuffer(g::GL_ARRAY_BUFFER, 0));
        gl!(self, glBindTexture(g::GL_TEXTURE_2D, self.gamma_texture));
        let s = self.screen;
        if self.gamma_tex_size != s {
            self.gamma_tex_size = s;
            gl!(
                self,
                glTexImage2D(
                    g::GL_TEXTURE_2D,
                    0,
                    g::GL_RGBA8 as i32,
                    s.x,
                    s.y,
                    0,
                    g::GL_RGBA,
                    g::GL_UNSIGNED_BYTE,
                    core::ptr::null()
                )
            );
            gl!(
                self,
                glTexParameteri(
                    g::GL_TEXTURE_2D,
                    g::GL_TEXTURE_MIN_FILTER,
                    g::GL_NEAREST as i32
                )
            );
            gl!(
                self,
                glTexParameteri(
                    g::GL_TEXTURE_2D,
                    g::GL_TEXTURE_MAG_FILTER,
                    g::GL_NEAREST as i32
                )
            );
        }
        gl!(
            self,
            glCopyTexSubImage2D(g::GL_TEXTURE_2D, 0, 0, 0, 0, 0, s.x, s.y)
        );
        gl!(self, glViewport(0, 0, s.x, s.y));
        gl!(self, glDisable(g::GL_DEPTH_TEST));
        gl!(self, glDisable(g::GL_BLEND));
        gl!(self, glDisable(g::GL_CULL_FACE));
        gl!(self, glDisable(g::GL_SCISSOR_TEST));
        gl!(self, glDisable(g::GL_STENCIL_TEST));
        gl!(self, glDisable(g::GL_ALPHA_TEST));
        gl!(
            self,
            glColorMask(g::GL_TRUE, g::GL_TRUE, g::GL_TRUE, g::GL_TRUE)
        );
        gl!(self, glUseProgram(self.gamma_program));
        let loc = |name: &CStr| -> i32 {
            match self.gl.glGetUniformLocation {
                // SAFETY: entry point resolved; `name` is NUL-terminated.
                Some(f) => unsafe { f(self.gamma_program, name.as_ptr().cast()) },
                None => 0,
            }
        };
        gl!(self, glUniform1i(loc(c"tex"), 0));
        gl!(self, glUniform1f(loc(c"invGamma"), 1.0 / gamma));
        gl!(self, glBegin(g::GL_TRIANGLE_STRIP));
        gl!(self, glVertex2f(-1.0, -1.0));
        gl!(self, glVertex2f(1.0, -1.0));
        gl!(self, glVertex2f(-1.0, 1.0));
        gl!(self, glVertex2f(1.0, 1.0));
        gl!(self, glEnd());
        gl!(self, glPopAttrib());
        gl!(self, glUseProgram(program as u32));
        gl!(self, glBindTexture(g::GL_TEXTURE_2D, tex as u32));
        gl!(self, glActiveTexture(active as u32));
        gl!(self, glBindBuffer(g::GL_ARRAY_BUFFER, array as u32));
        gl!(
            self,
            glBindFramebufferEXT(g::GL_FRAMEBUFFER_EXT, fbo as u32)
        );
    }

    // ---- render state ----

    pub fn set_color_write(&mut self, r: bool, gr: bool, b: bool, a: bool) {
        if self.state.set_color_write(r, gr, b, a) {
            gl!(
                self,
                glColorMask(u8::from(r), u8::from(gr), u8::from(b), u8::from(a))
            );
        }
    }

    pub fn set_depth_write(&mut self, on: bool) {
        if self.state.set_depth_write(on) {
            gl!(self, glDepthMask(u8::from(on)));
        }
    }

    fn toggle(&self, cap: u32, on: bool) {
        if on {
            gl!(self, glEnable(cap));
        } else {
            gl!(self, glDisable(cap));
        }
    }

    pub fn set_depth_test(&mut self, on: bool) {
        if self.state.set_depth_test(on) {
            self.toggle(g::GL_DEPTH_TEST, on);
        }
    }

    pub fn set_depth_func(&mut self, f: i32) {
        if self.state.set_depth_func(f) {
            gl!(self, glDepthFunc(tables::compare(f)));
        }
    }

    pub fn set_alpha_test(&mut self, on: bool) {
        if self.state.set_alpha_test(on) {
            self.toggle(g::GL_ALPHA_TEST, on);
        }
    }

    pub fn set_alpha_func(&mut self, f: i32, reference: f32) {
        if self.state.set_alpha_func(f, reference) {
            gl!(self, glAlphaFunc(tables::compare(f), reference));
        }
    }

    pub fn set_stencil_active(&self, on: bool) {
        if on {
            if self.double_sided_stencil && self.caps.stencil_two_side {
                gl!(self, glEnable(g::GL_STENCIL_TEST_TWO_SIDE_EXT));
            }
            gl!(self, glEnable(g::GL_STENCIL_TEST));
        } else {
            gl!(self, glDisable(g::GL_STENCIL_TEST_TWO_SIDE_EXT));
            gl!(self, glDisable(g::GL_STENCIL_TEST));
        }
    }

    pub fn set_stencil_write_mask(&self, mask: u32) {
        gl!(self, glStencilMask(mask));
    }

    pub fn set_stencil(
        &mut self,
        func: i32,
        reference: i32,
        mask: u32,
        fail: i32,
        zfail: i32,
        zpass: i32,
    ) {
        self.double_sided_stencil = false;
        if self.caps.stencil_two_side {
            gl!(self, glDisable(g::GL_STENCIL_TEST_TWO_SIDE_EXT));
            gl!(self, glActiveStencilFaceEXT(g::GL_FRONT));
        }
        gl!(self, glStencilFunc(tables::compare(func), reference, mask));
        gl!(
            self,
            glStencilOp(
                tables::stencil_op(fail),
                tables::stencil_op(zfail),
                tables::stencil_op(zpass)
            )
        );
    }

    /// `ops` is `[front fail, front zfail, front zpass, back fail, back zfail, back zpass]`.
    pub fn set_stencil_two_side(
        &mut self,
        funcs: [i32; 2],
        reference: i32,
        mask: u32,
        ops: [i32; 6],
    ) {
        self.double_sided_stencil = true;
        let [front_func, back_func] = funcs;
        let [ff, fz, fp, bf, bz, bp] = ops.map(tables::stencil_op);
        if self.caps.stencil_two_side {
            gl!(self, glEnable(g::GL_STENCIL_TEST_TWO_SIDE_EXT));
            gl!(self, glActiveStencilFaceEXT(g::GL_FRONT));
            gl!(
                self,
                glStencilFunc(tables::compare(front_func), reference, mask)
            );
            gl!(self, glStencilOp(ff, fz, fp));
            gl!(self, glActiveStencilFaceEXT(g::GL_BACK));
            gl!(
                self,
                glStencilFunc(tables::compare(back_func), reference, mask)
            );
            gl!(self, glStencilOp(bf, bz, bp));
        } else if self.caps.separate_stencil {
            gl!(self, glStencilOpSeparateATI(g::GL_FRONT, ff, fz, fp));
            gl!(self, glStencilOpSeparateATI(g::GL_BACK, bf, bz, bp));
            gl!(
                self,
                glStencilFuncSeparateATI(
                    tables::compare(front_func),
                    tables::compare(back_func),
                    reference,
                    mask
                )
            );
        } else {
            host_log(LOG_FATAL, "Only single sided stencil supported!\n");
        }
    }

    pub fn set_cull_active(&mut self, on: bool) {
        self.state.cull_active = on;
        self.toggle(g::GL_CULL_FACE, on);
        gl!(self, glCullFace(g::GL_BACK));
    }

    /// `mode` is `eCullMode`: 0 is clockwise.
    pub fn set_cull_mode(&mut self, mode: i32) {
        self.state.cull_mode = mode;
        gl!(self, glCullFace(g::GL_BACK));
        gl!(
            self,
            glFrontFace(if mode == 0 { g::GL_CCW } else { g::GL_CW })
        );
    }

    pub fn set_scissor_active(&mut self, on: bool) {
        if self.state.set_scissor(on) {
            self.toggle(g::GL_SCISSOR_TEST, on);
        }
    }

    pub fn set_scissor_rect(&self, target_h: i32, pos: Vector2l, size: Vector2l) {
        let [x, y, w, h] = state::scissor_rect(target_h, pos, size);
        gl!(self, glScissor(x, y, w, h));
    }

    pub fn set_clip_plane(&self, idx: i32, plane: [f32; 4]) {
        let eq = plane.map(f64::from);
        gl!(
            self,
            glClipPlane(g::GL_CLIP_PLANE0.wrapping_add_signed(idx), eq.as_ptr())
        );
    }

    pub fn set_clip_plane_active(&self, idx: i32, on: bool) {
        self.toggle(g::GL_CLIP_PLANE0.wrapping_add_signed(idx), on);
    }

    pub fn set_blend_active(&mut self, on: bool) {
        if self.state.set_blend(on) {
            self.toggle(g::GL_BLEND, on);
        }
    }

    pub fn set_blend_func(&self, src: i32, dst: i32) {
        gl!(self, glBlendFunc(tables::blend(src), tables::blend(dst)));
    }

    pub fn set_blend_func_separate(&self, sc: i32, dc: i32, sa: i32, da: i32) {
        if self.caps.blend_func_separate {
            gl!(
                self,
                glBlendFuncSeparateEXT(
                    tables::blend(sc),
                    tables::blend(dc),
                    tables::blend(sa),
                    tables::blend(da)
                )
            );
        } else {
            gl!(self, glBlendFunc(tables::blend(sc), tables::blend(dc)));
        }
    }

    pub fn set_depth_clamp_active(&self, on: bool) {
        self.toggle(g::GL_DEPTH_CLAMP, on);
    }

    pub fn set_polygon_offset_active(&self, on: bool) {
        self.toggle(g::GL_POLYGON_OFFSET_FILL, on);
    }

    pub fn set_polygon_offset(&self, bias: f32, slope: f32) {
        gl!(self, glPolygonOffset(slope, bias));
    }

    // ---- matrices ----

    /// `eMatrix`: model view, projection, texture; other values leave the mode alone.
    pub fn set_matrix_mode(&self, kind: i32) {
        match kind {
            0 => gl!(self, glMatrixMode(g::GL_MODELVIEW)),
            1 => gl!(self, glMatrixMode(g::GL_PROJECTION)),
            2 => gl!(self, glMatrixMode(g::GL_TEXTURE)),
            _ => {}
        }
    }

    pub fn push_matrix(&self, kind: i32) {
        self.set_matrix_mode(kind);
        gl!(self, glPushMatrix());
    }

    pub fn pop_matrix(&self, kind: i32) {
        self.set_matrix_mode(kind);
        gl!(self, glPopMatrix());
    }

    pub fn set_matrix(&self, kind: i32, m: &[f32; 16]) {
        self.set_matrix_mode(kind);
        gl!(self, glLoadIdentity());
        let mut t = [0.0f32; 16];
        for (r, row) in t.chunks_exact_mut(4).enumerate() {
            for (c, slot) in row.iter_mut().enumerate() {
                *slot = m.get(c * 4 + r).copied().unwrap_or(0.0);
            }
        }
        gl!(self, glLoadMatrixf(t.as_ptr()));
    }

    pub fn set_identity_matrix(&self, kind: i32) {
        self.set_matrix_mode(kind);
        gl!(self, glLoadIdentity());
    }

    pub fn set_ortho(&self, left: f32, right: f32, bottom: f32, top: f32, near: f32, far: f32) {
        gl!(self, glMatrixMode(g::GL_PROJECTION));
        gl!(self, glLoadIdentity());
        gl!(
            self,
            glOrtho(
                f64::from(left),
                f64::from(right),
                f64::from(bottom),
                f64::from(top),
                f64::from(near),
                f64::from(far)
            )
        );
    }

    // ---- textures ----

    /// `tex_type` is `eTextureType`, ignored without a texture; `handle` the GL texture name.
    pub fn set_texture(&mut self, unit: u32, tex_type: i32, has_texture: bool, handle: u32) {
        let target = if has_texture {
            tables::texture_target(tex_type)
        } else {
            0
        };
        let plan = self
            .state
            .set_texture(unit, target, has_texture, self.caps.multitexture);
        if plan.active_unit >= 0 {
            gl!(
                self,
                glActiveTextureARB(g::GL_TEXTURE0_ARB.wrapping_add_signed(plan.active_unit))
            );
        }
        if plan.disable != 0 {
            gl!(self, glDisable(plan.disable));
        }
        if plan.bind {
            gl!(self, glBindTexture(target, handle));
        }
        if plan.enable != 0 {
            gl!(self, glEnable(plan.enable));
        }
    }

    pub fn set_active_texture_unit(&mut self, unit: u32) {
        if self.state.set_active_unit(unit) {
            gl!(
                self,
                glActiveTextureARB(g::GL_TEXTURE0_ARB.wrapping_add(unit))
            );
        }
    }

    /// `param` is `eTextureParam`; `val` the matching function, source, operand or raw value.
    pub fn set_texture_env(&self, param: i32, val: i32) {
        let p = tables::texture_param(param);
        gl!(
            self,
            glTexEnvi(
                g::GL_TEXTURE_ENV,
                g::GL_TEXTURE_ENV_MODE,
                g::GL_COMBINE_ARB as i32
            )
        );
        let v = if param == 0 || param == 1 {
            tables::texture_func(val) as i32
        } else if (2..=7).contains(&param) {
            tables::texture_source(val) as i32
        } else if (8..=13).contains(&param) {
            tables::texture_op(val) as i32
        } else {
            val
        };
        gl!(self, glTexEnvi(g::GL_TEXTURE_ENV, p, v));
    }

    pub fn set_texture_constant_color(&self, rgba: [f32; 4]) {
        gl!(
            self,
            glTexEnvfv(g::GL_TEXTURE_ENV, g::GL_TEXTURE_ENV_COLOR, rgba.as_ptr())
        );
    }

    pub fn set_color(&self, r: f32, gr: f32, b: f32, a: f32) {
        gl!(self, glColor4f(r, gr, b, a));
    }

    // ---- immediate mode ----

    pub fn begin(&self, mode: u32) {
        gl!(self, glBegin(mode));
    }

    pub fn end(&self) {
        gl!(self, glEnd());
    }

    pub fn vertex3f(&self, x: f32, y: f32, z: f32) {
        gl!(self, glVertex3f(x, y, z));
    }

    pub fn color4f(&self, r: f32, gr: f32, b: f32, a: f32) {
        gl!(self, glColor4f(r, gr, b, a));
    }

    pub fn texcoord2f(&self, s: f32, t: f32) {
        gl!(self, glTexCoord2f(s, t));
    }

    pub fn texcoord3f(&self, s: f32, t: f32, r: f32) {
        gl!(self, glTexCoord3f(s, t, r));
    }

    pub fn multitexcoord2f(&self, unit: u32, s: f32, t: f32) {
        gl!(self, glMultiTexCoord2fARB(unit, s, t));
    }

    pub fn multitexcoord3f(&self, unit: u32, s: f32, t: f32, r: f32) {
        gl!(self, glMultiTexCoord3fARB(unit, s, t, r));
    }

    // ---- batches ----

    /// The Rust form of `RunGlOps` (`GlOps.h`).
    ///
    /// # Safety
    /// Pointer sources in `src` are valid for the ops' indices; they are only handed to GL.
    pub unsafe fn run_ops(&self, ops: &[Op], src: &Sources) {
        // SAFETY: caller contract.
        unsafe { run_ops_with(self.gl, ops, src) }
    }

    /// # Safety
    /// See `run_ops`.
    pub unsafe fn batch_texture_unit(
        &self,
        unit: u32,
        active: bool,
        stride_bytes: i32,
        src: &Sources,
    ) {
        let mut ops = [Op::default(); MAX_OPS];
        let n = vbuf::batch_texture_unit(unit, active, stride_bytes, &mut ops);
        // SAFETY: caller contract.
        unsafe { self.run_ops(ops.get(..n).unwrap_or(&[]), src) }
    }

    /// `FlushTriBatch` / `FlushQuadBatch` up to the draw call.
    ///
    /// # Safety
    /// See `run_ops`; `indices` holds `index_count` entries.
    pub unsafe fn flush_batch(
        &self,
        quads: bool,
        flags: u32,
        stride_bytes: i32,
        index_count: i32,
        indices: *const c_void,
        src: &Sources,
    ) {
        let mut ops = [Op::default(); MAX_OPS];
        let n = vbuf::batch_states(flags, &mut ops);
        // SAFETY: caller contract.
        unsafe { self.run_ops(ops.get(..n).unwrap_or(&[]), src) };
        let n = vbuf::batch_arrays(stride_bytes, &mut ops);
        // SAFETY: caller contract.
        unsafe { self.run_ops(ops.get(..n).unwrap_or(&[]), src) };
        let mode = if quads { g::GL_QUADS } else { g::GL_TRIANGLES };
        gl!(
            self,
            glDrawElements(mode, index_count, g::GL_UNSIGNED_INT, indices)
        );
    }
}

/// Executes plan ops against `gl`; shared by the context and the vertex buffers.
///
/// # Safety
/// Pointer sources in `src` are valid for the ops' indices; they are only handed to GL.
pub unsafe fn run_ops_with(gl: &GlFns, ops: &[Op], src: &Sources) {
    for op in ops {
        match op.kind {
            vbuf::OP_CLIENT_ACTIVE => gl_of!(gl, glClientActiveTextureARB(op.array)),
            vbuf::OP_ENABLE => gl_of!(gl, glEnableClientState(op.array)),
            vbuf::OP_DISABLE => gl_of!(gl, glDisableClientState(op.array)),
            vbuf::OP_BIND_BUFFER => gl_of!(gl, glBindBufferARB(g::GL_ARRAY_BUFFER_ARB, op.array)),
            vbuf::OP_POINTER => {
                let idx = op.index as usize;
                let ptr: *const c_void = match op.src {
                    // SAFETY: caller contract on the source arrays.
                    vbuf::SRC_ELEMENT if !src.element.is_null() => unsafe { *src.element.add(idx) },
                    vbuf::SRC_BATCH_VERTEX => src.batch_vertex.wrapping_add(idx).cast(),
                    // SAFETY: caller contract on the source arrays.
                    vbuf::SRC_BATCH_TEXCOORD if !src.batch_texcoord.is_null() => unsafe {
                        (*src.batch_texcoord.add(idx)).cast_const().cast()
                    },
                    _ => core::ptr::null(),
                };
                pointer(gl, op, ptr);
            }
            _ => {}
        }
    }
}

fn pointer(gl: &GlFns, op: &Op, ptr: *const c_void) {
    match op.array {
        g::GL_VERTEX_ARRAY => gl_of!(gl, glVertexPointer(op.size, op.gl_type, op.stride, ptr)),
        g::GL_COLOR_ARRAY => gl_of!(gl, glColorPointer(op.size, op.gl_type, op.stride, ptr)),
        g::GL_SECONDARY_COLOR_ARRAY => {
            gl_of!(
                gl,
                glSecondaryColorPointerEXT(op.size, op.gl_type, op.stride, ptr)
            );
        }
        g::GL_NORMAL_ARRAY => gl_of!(gl, glNormalPointer(op.gl_type, op.stride, ptr)),
        g::GL_TEXTURE_COORD_ARRAY => {
            gl_of!(gl, glTexCoordPointer(op.size, op.gl_type, op.stride, ptr));
        }
        _ => {}
    }
}

impl Default for Ctx {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for Ctx {
    fn drop(&mut self) {
        self.destroy_window();
    }
}

fn sdl_error() -> String {
    // SAFETY: SDL_GetError returns a NUL-terminated string.
    unsafe { CStr::from_ptr(sdl::SDL_GetError()) }
        .to_string_lossy()
        .into_owned()
}

unsafe extern "C" fn debug_callback(
    _source: u32,
    type_: u32,
    _id: u32,
    _severity: u32,
    _length: i32,
    message: *const i8,
    user: *const c_void,
) {
    if type_ == g::GL_DEBUG_TYPE_ERROR || !user.is_null() {
        // SAFETY: GL passes a NUL-terminated message.
        let text = unsafe { CStr::from_ptr(message.cast()) }.to_string_lossy();
        host_log(LOG_ERROR, &format!("GL: {text}\n"));
    }
}
