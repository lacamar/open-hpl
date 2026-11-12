//! C ABI of the game base port, mirrored in `amnesia/src/game/RustLuxBase.h`.
//!
//! Engine access goes through one callback `call(ctx, op, args, nargs, ret)`; the op numbers are
//! the `eOhplGa9Op_*` enum of the header. Rust owns the order of the calls. Strings come back
//! through `ohpl_ga9_ret_bytes` / `ohpl_ga9_ret_wide` or through an emit callback `(tag, p, n)`.

use super::cfg::Cfg;
use super::{amfp, base, config, debug, headless, input, look};
use std::ffi::c_void;

type Ctx = *mut c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Arg {
    pub i: i64,
    pub f: f32,
    pub p: *const u8,
    pub n: usize,
}

#[repr(C)]
pub struct Ret {
    pub i: i64,
    pub f: f32,
    pub d: f64,
    bytes: *mut Vec<u8>,
    wide: *mut Vec<u32>,
}

type Call = unsafe extern "C" fn(Ctx, u32, *const Arg, usize, *mut Ret);
type Emit = unsafe extern "C" fn(Ctx, u32, *const c_void, usize);

fn units<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the entry point's caller passes a live `n`-unit buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

/// # Safety
/// `r` is the `Ret` of the current call; `p` points to `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_ret_bytes(r: *mut Ret, p: *const u8, n: usize) {
    // SAFETY: `r` is live and its sink points to a `Vec` owned by the caller of the callback.
    if let Some(v) = unsafe { r.as_mut().and_then(|r| r.bytes.as_mut()) } {
        v.extend_from_slice(units(p, n));
    }
}

/// # Safety
/// `r` is the `Ret` of the current call; `p` points to `n` wide units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_ret_wide(r: *mut Ret, p: *const u32, n: usize) {
    // SAFETY: as `ohpl_ga9_ret_bytes`.
    if let Some(v) = unsafe { r.as_mut().and_then(|r| r.wide.as_mut()) } {
        v.extend_from_slice(units(p, n));
    }
}

fn ai(i: i64) -> Arg {
    Arg {
        i,
        f: 0.0,
        p: std::ptr::null(),
        n: 0,
    }
}

fn af(f: f32) -> Arg {
    Arg { f, ..ai(0) }
}

fn ab(b: bool) -> Arg {
    ai(i64::from(b))
}

fn astr(s: &[u8]) -> Arg {
    Arg {
        p: s.as_ptr(),
        n: s.len(),
        ..ai(0)
    }
}

fn aw(s: &[u32]) -> Arg {
    Arg {
        p: s.as_ptr().cast(),
        n: s.len(),
        ..ai(0)
    }
}

#[derive(Default)]
struct Reply {
    i: i64,
    f: f32,
    d: f64,
    bytes: Vec<u8>,
    wide: Vec<u32>,
}

#[derive(Clone, Copy)]
struct Host {
    ctx: Ctx,
    call: Call,
}

impl Host {
    fn go(&self, op: u32, args: &[Arg]) -> Reply {
        let mut rep = Reply::default();
        let mut ret = Ret {
            i: 0,
            f: 0.0,
            d: 0.0,
            bytes: &raw mut rep.bytes,
            wide: &raw mut rep.wide,
        };
        // SAFETY: the entry point's caller guarantees `call` accepts `ctx` and every op.
        unsafe { (self.call)(self.ctx, op, args.as_ptr(), args.len(), &raw mut ret) };
        rep.i = ret.i;
        rep.f = ret.f;
        rep.d = ret.d;
        rep
    }

    fn i(&self, op: u32, args: &[Arg]) -> i64 {
        self.go(op, args).i
    }

    fn b(&self, op: u32, args: &[Arg]) -> bool {
        self.go(op, args).i != 0
    }

    fn f(&self, op: u32, args: &[Arg]) -> f32 {
        self.go(op, args).f
    }
}

mod op {
    pub const C_BOOL: u32 = 0;
    pub const C_INT: u32 = 1;
    pub const C_FLOAT: u32 = 2;
    pub const C_STR: u32 = 3;
    pub const C_STRW: u32 = 4;
    pub const C_SET_BOOL: u32 = 5;
    pub const C_SET_INT: u32 = 6;
    pub const C_SET_FLOAT: u32 = 7;
    pub const C_SET_STR: u32 = 8;
    pub const CFG2: u32 = 16;
    pub const LOAD_INIT: u32 = 40;
    pub const LOAD_DEFAULTS: u32 = 41;
    pub const WARN: u32 = 42;
    pub const RAISE_FIRST_START: u32 = 44;
    pub const GAMMA: u32 = 45;
    pub const GBUFFER_TYPE: u32 = 46;
    pub const NUM_GBUFFER: u32 = 47;
    pub const VOLUME: u32 = 48;
    pub const LIMIT_FPS: u32 = 49;
    pub const WAIT_FOCUS: u32 = 50;
    pub const FIRST_START_FLAG: u32 = 51;
    pub const FILE_EXISTS_W: u32 = 52;
    pub const MAP_FOLDER: u32 = 53;
    pub const LANG_CLEAR: u32 = 54;
    pub const LANG_FOUND: u32 = 55;
    pub const LANG_ADD: u32 = 56;
    pub const A_CREATE: u32 = 60;
    pub const A_CLEAR: u32 = 61;
    pub const A_ADD: u32 = 62;
    pub const A_COUNT: u32 = 63;
    pub const A_TYPE: u32 = 64;
    pub const A_INPUT: u32 = 65;
    pub const M_ANY_BUTTON: u32 = 70;
    pub const M_ABS: u32 = 71;
    pub const M_TIME: u32 = 72;
    pub const M_PAD: u32 = 73;
    pub const M_UI_DIR: u32 = 74;
    pub const M_SCREEN: u32 = 75;
    pub const M_HIDE: u32 = 76;
    pub const K_TRIGGERED: u32 = 80;
    pub const K_ADD_LEAN: u32 = 81;
    pub const K_SET_LEAN: u32 = 82;
    pub const K_SCROLL: u32 = 83;
    pub const K_STOP: u32 = 84;
    pub const K_BUTTON: u32 = 85;
    pub const K_AXIS: u32 = 86;
    pub const K_DEAD: u32 = 87;
    pub const K_FRAME: u32 = 88;
    pub const K_STEP: u32 = 89;
    pub const Q_HAS: u32 = 100;
    pub const Q_STR: u32 = 101;
    pub const Q_INT: u32 = 102;
    pub const Q_FLOAT: u32 = 103;
    pub const Q_BOOL: u32 = 104;
    pub const P_ERROR: u32 = 110;
    pub const P_STR: u32 = 111;
    pub const P_INT: u32 = 112;
    pub const P_FLOAT: u32 = 113;
    pub const P_BOOL: u32 = 114;
    pub const P_RAW: u32 = 115;
    pub const H_MAP_LOADED: u32 = 120;
    pub const H_RUN_SCRIPT: u32 = 121;
    pub const H_SET_GBUFFER: u32 = 122;
    pub const H_DEFERRED: u32 = 123;
    pub const H_GBUFFER_SIZE: u32 = 124;
    pub const H_GBUFFER_PIXELS: u32 = 126;
    pub const H_MAP_NAME: u32 = 127;
    pub const H_MAP_FILE: u32 = 128;
    pub const H_BODY: u32 = 129;
    pub const H_BODY_VALUE: u32 = 130;
    pub const H_FPS: u32 = 131;
    pub const H_OCCLUSION: u32 = 132;
    pub const H_SET_OCCLUSION: u32 = 133;
    pub const H_GPU: u32 = 134;
    pub const H_RENDER_JSON: u32 = 135;
    pub const H_SAVE: u32 = 136;
    pub const H_FILE_EXISTS: u32 = 137;
    pub const H_LOAD: u32 = 138;
    pub const H_PLAYER: u32 = 139;
    pub const H_TELEPORT: u32 = 140;
    pub const H_HAVE_USER_CFG: u32 = 141;
    pub const H_DEFAULT_PROFILE: u32 = 142;
    pub const H_INIT_USER_CFG: u32 = 143;
    pub const H_ENTER_GAME: u32 = 144;
    pub const H_START_GAME: u32 = 145;
    pub const H_CAMERA: u32 = 146;
    pub const H_CAMERA_VALUE: u32 = 147;
    pub const H_SET_CAMERA_POS: u32 = 148;
    pub const H_SET_PITCH: u32 = 149;
    pub const H_SET_YAW: u32 = 150;
}

struct CfgHost {
    h: Host,
    base: u32,
}

impl Cfg for CfgHost {
    fn bool(&mut self, l: &[u8], k: &[u8], d: bool) -> bool {
        self.h.b(self.base + op::C_BOOL, &[astr(l), astr(k), ab(d)])
    }
    fn int(&mut self, l: &[u8], k: &[u8], d: i32) -> i32 {
        self.h
            .i(self.base + op::C_INT, &[astr(l), astr(k), ai(i64::from(d))]) as i32
    }
    fn float(&mut self, l: &[u8], k: &[u8], d: f32) -> f32 {
        self.h
            .f(self.base + op::C_FLOAT, &[astr(l), astr(k), af(d)])
    }
    fn string(&mut self, l: &[u8], k: &[u8], d: &[u8]) -> Vec<u8> {
        self.h
            .go(self.base + op::C_STR, &[astr(l), astr(k), astr(d)])
            .bytes
    }
    fn string_w(&mut self, l: &[u8], k: &[u8], d: &[u32]) -> Vec<u32> {
        self.h
            .go(self.base + op::C_STRW, &[astr(l), astr(k), aw(d)])
            .wide
    }
    fn set_bool(&mut self, l: &[u8], k: &[u8], v: bool) {
        self.h
            .go(self.base + op::C_SET_BOOL, &[astr(l), astr(k), ab(v)]);
    }
    fn set_int(&mut self, l: &[u8], k: &[u8], v: i32) {
        self.h.go(
            self.base + op::C_SET_INT,
            &[astr(l), astr(k), ai(i64::from(v))],
        );
    }
    fn set_float(&mut self, l: &[u8], k: &[u8], v: f32) {
        self.h
            .go(self.base + op::C_SET_FLOAT, &[astr(l), astr(k), af(v)]);
    }
    fn set_string(&mut self, l: &[u8], k: &[u8], v: &[u8]) {
        self.h
            .go(self.base + op::C_SET_STR, &[astr(l), astr(k), astr(v)]);
    }
}

fn cfg(ctx: Ctx, call: Call) -> CfgHost {
    CfgHost {
        h: Host { ctx, call },
        base: 0,
    }
}

#[derive(Clone, Copy)]
struct Out {
    emit: Emit,
    ctx: Ctx,
}

impl Out {
    fn bytes(&self, tag: u32, v: &[u8]) {
        // SAFETY: `emit` accepts `ctx` and any (tag, ptr, len).
        unsafe { (self.emit)(self.ctx, tag, v.as_ptr().cast(), v.len()) }
    }

    fn wide(&self, tag: u32, v: &[u32]) {
        // SAFETY: as `bytes`.
        unsafe { (self.emit)(self.ctx, tag, v.as_ptr().cast(), v.len()) }
    }

    fn num(&self, tag: u32, v: usize) {
        // SAFETY: as `bytes`; a null pointer carries a number.
        unsafe { (self.emit)(self.ctx, tag, std::ptr::null(), v) }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    p: *const u8,
    n: usize,
}

impl Str {
    fn get<'a>(&self) -> &'a [u8] {
        units(self.p, self.n)
    }
}

#[repr(C)]
pub struct MainCfgC {
    load_debug_menu: bool,
    first_start: bool,
    force_cache: bool,
    fullscreen: bool,
    vsync: bool,
    adaptive_vsync: bool,
    fast_physics: bool,
    fast_static: bool,
    fast_entity: bool,
    occlusion_test_lights: bool,
    shadows_active: bool,
    world_reflection: bool,
    refraction: bool,
    edge_smooth: bool,
    ssao_active: bool,
    parallax_enabled: bool,
    force_sm34_off: bool,
    screen_width: i32,
    screen_height: i32,
    display: i32,
    shadow_quality: i32,
    shadow_res: i32,
    gui_scale: i32,
    ssao_samples: i32,
    ssao_resolution: i32,
    parallax_quality: i32,
    texture_quality: i32,
    texture_filter: i32,
    sound_dev_id: i32,
    max_sound_channels: i32,
    sound_stream_buffers: i32,
    sound_stream_buffer_size: i32,
    paper_white: f32,
    peak_nits: f32,
    texture_anisotropy: f32,
    lang_file: Str,
    screenshot_ext: Str,
    renderer: Str,
    color_output: Str,
}

impl MainCfgC {
    fn from_rust(m: &config::MainCfg) -> MainCfgC {
        let none = Str {
            p: std::ptr::null(),
            n: 0,
        };
        MainCfgC {
            load_debug_menu: m.load_debug_menu,
            first_start: m.first_start,
            force_cache: m.force_cache_loading_and_skip_saving,
            fullscreen: m.fullscreen,
            vsync: m.vsync,
            adaptive_vsync: m.adaptive_vsync,
            fast_physics: m.fast_physics_load,
            fast_static: m.fast_static_load,
            fast_entity: m.fast_entity_load,
            occlusion_test_lights: m.occlusion_test_lights,
            shadows_active: m.shadows_active,
            world_reflection: m.world_reflection,
            refraction: m.refraction,
            edge_smooth: m.edge_smooth,
            ssao_active: m.ssao_active,
            parallax_enabled: m.parallax_enabled,
            force_sm34_off: m.force_shader_model_3_and_4_off,
            screen_width: m.screen_width,
            screen_height: m.screen_height,
            display: m.display,
            shadow_quality: m.shadow_quality,
            shadow_res: m.shadow_res,
            gui_scale: m.gui_scale,
            ssao_samples: m.ssao_samples,
            ssao_resolution: m.ssao_resolution,
            parallax_quality: m.parallax_quality,
            texture_quality: m.texture_quality,
            texture_filter: m.texture_filter,
            sound_dev_id: m.sound_dev_id,
            max_sound_channels: m.max_sound_channels,
            sound_stream_buffers: m.sound_stream_buffers,
            sound_stream_buffer_size: m.sound_stream_buffer_size,
            paper_white: m.paper_white,
            peak_nits: m.peak_nits,
            texture_anisotropy: m.texture_anisotropy,
            lang_file: none,
            screenshot_ext: none,
            renderer: none,
            color_output: none,
        }
    }

    fn to_rust(&self) -> config::MainCfg {
        config::MainCfg {
            load_debug_menu: self.load_debug_menu,
            first_start: self.first_start,
            lang_file: self.lang_file.get().to_vec(),
            screenshot_ext: self.screenshot_ext.get().to_vec(),
            force_cache_loading_and_skip_saving: self.force_cache,
            screen_width: self.screen_width,
            screen_height: self.screen_height,
            display: self.display,
            fullscreen: self.fullscreen,
            vsync: self.vsync,
            adaptive_vsync: self.adaptive_vsync,
            fast_physics_load: self.fast_physics,
            fast_static_load: self.fast_static,
            fast_entity_load: self.fast_entity,
            occlusion_test_lights: self.occlusion_test_lights,
            shadows_active: self.shadows_active,
            shadow_quality: self.shadow_quality,
            shadow_res: self.shadow_res,
            gui_scale: self.gui_scale,
            renderer: self.renderer.get().to_vec(),
            color_output: self.color_output.get().to_vec(),
            paper_white: self.paper_white,
            peak_nits: self.peak_nits,
            world_reflection: self.world_reflection,
            refraction: self.refraction,
            edge_smooth: self.edge_smooth,
            ssao_active: self.ssao_active,
            ssao_samples: self.ssao_samples,
            ssao_resolution: self.ssao_resolution,
            parallax_enabled: self.parallax_enabled,
            parallax_quality: self.parallax_quality,
            texture_quality: self.texture_quality,
            texture_filter: self.texture_filter,
            texture_anisotropy: self.texture_anisotropy,
            force_shader_model_3_and_4_off: self.force_sm34_off,
            sound_dev_id: self.sound_dev_id,
            max_sound_channels: self.max_sound_channels,
            sound_stream_buffers: self.sound_stream_buffers,
            sound_stream_buffer_size: self.sound_stream_buffer_size,
        }
    }
}

/// Tags of the four strings `ohpl_ga9_load_main_cfg` emits.
pub const MAIN_LANG: u32 = 1;
pub const MAIN_EXT: u32 = 2;
pub const MAIN_RENDERER: u32 = 3;
pub const MAIN_COLOR: u32 = 4;

/// # Safety
/// `ctx`/`call` as in the module docs; `out` is writable; `def_lang` is `n` bytes; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_load_main_cfg(
    ctx: Ctx,
    call: Call,
    def_lang: *const u8,
    n: usize,
    out: *mut MainCfgC,
    emit: Emit,
    ectx: Ctx,
) {
    let h = Host { ctx, call };
    let m = config::load_main(
        &mut cfg(ctx, call),
        &mut || h.b(op::FIRST_START_FLAG, &[]),
        units(def_lang, n),
    );
    let o = Out { emit, ctx: ectx };
    o.bytes(MAIN_LANG, &m.lang_file);
    o.bytes(MAIN_EXT, &m.screenshot_ext);
    o.bytes(MAIN_RENDERER, &m.renderer);
    o.bytes(MAIN_COLOR, &m.color_output);
    // SAFETY: `out` is writable per the contract.
    if let Some(out) = unsafe { out.as_mut() } {
        *out = MainCfgC::from_rust(&m);
    }
}

struct SaveHost(CfgHost);

impl Cfg for SaveHost {
    fn bool(&mut self, l: &[u8], k: &[u8], d: bool) -> bool {
        self.0.bool(l, k, d)
    }
    fn int(&mut self, l: &[u8], k: &[u8], d: i32) -> i32 {
        self.0.int(l, k, d)
    }
    fn float(&mut self, l: &[u8], k: &[u8], d: f32) -> f32 {
        self.0.float(l, k, d)
    }
    fn string(&mut self, l: &[u8], k: &[u8], d: &[u8]) -> Vec<u8> {
        self.0.string(l, k, d)
    }
    fn string_w(&mut self, l: &[u8], k: &[u8], d: &[u32]) -> Vec<u32> {
        self.0.string_w(l, k, d)
    }
    fn set_bool(&mut self, l: &[u8], k: &[u8], v: bool) {
        self.0.set_bool(l, k, v);
    }
    fn set_int(&mut self, l: &[u8], k: &[u8], v: i32) {
        self.0.set_int(l, k, v);
    }
    fn set_float(&mut self, l: &[u8], k: &[u8], v: f32) {
        self.0.set_float(l, k, v);
    }
    fn set_string(&mut self, l: &[u8], k: &[u8], v: &[u8]) {
        self.0.set_string(l, k, v);
    }
}

impl config::SaveEnv for SaveHost {
    fn raise_first_start_flag(&mut self) {
        self.0.h.go(op::RAISE_FIRST_START, &[]);
    }
    fn gamma(&mut self) -> f32 {
        self.0.h.f(op::GAMMA, &[])
    }
    fn gbuffer_type(&mut self) -> i32 {
        self.0.h.i(op::GBUFFER_TYPE, &[]) as i32
    }
    fn num_gbuffer_textures(&mut self) -> i32 {
        self.0.h.i(op::NUM_GBUFFER, &[]) as i32
    }
    fn volume(&mut self) -> f32 {
        self.0.h.f(op::VOLUME, &[])
    }
    fn limit_fps(&mut self) -> bool {
        self.0.h.b(op::LIMIT_FPS, &[])
    }
    fn wait_if_out_of_focus(&mut self) -> bool {
        self.0.h.b(op::WAIT_FOCUS, &[])
    }
}

/// # Safety
/// `ctx`/`call` as in the module docs; `m` is readable and its strings live for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_save_main_cfg(ctx: Ctx, call: Call, m: *const MainCfgC) {
    // SAFETY: `m` is readable per the contract.
    if let Some(m) = unsafe { m.as_ref() } {
        config::save_main(&mut SaveHost(cfg(ctx, call)), &m.to_rust());
    }
}

/// # Safety
/// `r` points to an `OhplGa9Restart`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_restart_set(r: *mut config::Restart) {
    // SAFETY: `r` is writable per the contract.
    if let Some(r) = unsafe { r.as_mut() } {
        r.set_needed();
    }
}

/// # Safety
/// `r` points to an `OhplGa9Restart`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_restart_due(r: *mut config::Restart) -> bool {
    // SAFETY: `r` is writable per the contract.
    unsafe { r.as_mut() }.is_some_and(config::Restart::show_due)
}

pub const T_INIT_FILE: u32 = 1;
pub const T_DEFAULT_INIT: u32 = 2;

/// # Safety
/// `cmd` is `n` bytes; `emit` accepts `ectx`; `out` is `[ptest, hardmode]` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_parse_cmdline(
    cmd: *const u8,
    n: usize,
    out: *mut bool,
    emit: Emit,
    ectx: Ctx,
) {
    let c = base::parse_command_line(units(cmd, n));
    let o = Out { emit, ctx: ectx };
    o.wide(T_INIT_FILE, &c.init_file);
    o.wide(T_DEFAULT_INIT, &c.default_init);
    // SAFETY: `out` holds two bools per the contract.
    unsafe {
        if let Some(p) = out.as_mut() {
            *p = c.ptest;
        }
        if let Some(p) = out.add(1).as_mut() {
            *p = c.hard_mode;
        }
    }
}

/// # Safety
/// `msg` is `n` wide units; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_error_text(msg: *const u32, n: usize, emit: Emit, ectx: Ctx) {
    Out { emit, ctx: ectx }.wide(1, &base::error_text(units(msg, n)));
}

struct InitHost {
    h: Host,
    init: CfgHost,
    defaults: CfgHost,
}

impl base::InitEnv for InitHost {
    fn load_init(&mut self) -> bool {
        self.h.b(op::LOAD_INIT, &[])
    }
    fn init(&mut self) -> &mut dyn Cfg {
        &mut self.init
    }
    fn load_defaults(&mut self) -> bool {
        self.h.b(op::LOAD_DEFAULTS, &[])
    }
    fn defaults(&mut self) -> &mut dyn Cfg {
        &mut self.defaults
    }
    fn warn(&mut self, text: &[u32]) {
        self.h.go(op::WARN, &[aw(text)]);
    }
}

pub const T_ERROR: u32 = 1;
pub const A_MAIN_SAVE_FOLDER: u32 = 10;
pub const A_DEFAULT_USER_CONFIG: u32 = 11;
pub const A_DEFAULT_USER_KEYS: u32 = 12;
pub const A_DEFAULT_MAIN: u32 = 13;
pub const A_GAME_CONFIG: u32 = 14;
pub const A_MENU_CONFIG: u32 = 15;
pub const A_PRE_MENU_CONFIG: u32 = 16;
pub const A_DEMO_CONFIG: u32 = 17;
pub const A_RESOURCES: u32 = 20;
pub const A_MATERIALS: u32 = 21;
pub const A_DEF_BASE_LANG: u32 = 22;
pub const A_DEF_GAME_LANG: u32 = 23;
pub const A_BASE_LANG_FOLDER: u32 = 24;
pub const A_GAME_LANG_FOLDER: u32 = 25;
pub const A_CUSTOM_STORY_PATH: u32 = 26;
pub const A_GAME_NAME: u32 = 27;
pub const A_ALLOW_HARDMODE: u32 = 28;
pub const A_START_FILE: u32 = 29;
pub const A_START_FOLDER: u32 = 30;
pub const A_START_POS: u32 = 31;

/// # Safety
/// `ctx`/`call` as in the module docs (cfg 0 is the init file, cfg 1 the default init file);
/// the wide buffers are live; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_init_app(
    ctx: Ctx,
    call: Call,
    init_file: *const u32,
    init_n: usize,
    default_init: *const u32,
    default_n: usize,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    let h = Host { ctx, call };
    let mut env = InitHost {
        h,
        init: CfgHost { h, base: 0 },
        defaults: CfgHost { h, base: op::CFG2 },
    };
    let cmd = base::CmdLine {
        ptest: false,
        hard_mode: false,
        default_init: units(default_init, default_n).to_vec(),
        init_file: units(init_file, init_n).to_vec(),
    };
    let o = Out { emit, ctx: ectx };
    let (v, err) = base::init_app(&mut env, &cmd);
    o.wide(A_MAIN_SAVE_FOLDER, &v.main_save_folder);
    o.wide(A_DEFAULT_USER_CONFIG, &v.default_user_config);
    o.wide(A_DEFAULT_USER_KEYS, &v.default_user_key_config);
    o.wide(A_DEFAULT_MAIN, &v.default_main_config);
    o.wide(A_GAME_CONFIG, &v.game_config);
    o.wide(A_MENU_CONFIG, &v.menu_config);
    o.wide(A_PRE_MENU_CONFIG, &v.pre_menu_config);
    o.wide(A_DEMO_CONFIG, &v.demo_config);
    o.bytes(A_RESOURCES, &v.resource_config);
    o.bytes(A_MATERIALS, &v.material_config);
    o.bytes(A_DEF_BASE_LANG, &v.default_base_language);
    o.bytes(A_DEF_GAME_LANG, &v.default_game_language);
    o.bytes(A_BASE_LANG_FOLDER, &v.base_language_folder);
    o.bytes(A_GAME_LANG_FOLDER, &v.game_language_folder);
    o.bytes(A_CUSTOM_STORY_PATH, &v.custom_story_path);
    o.bytes(A_GAME_NAME, &v.game_name);
    o.num(A_ALLOW_HARDMODE, usize::from(v.allow_hardmode));
    o.bytes(A_START_FILE, &v.start_map_file);
    o.bytes(A_START_FOLDER, &v.start_map_folder);
    o.bytes(A_START_POS, &v.start_map_pos);
    match err {
        Some(m) => {
            o.wide(T_ERROR, &m);
            false
        }
        None => true,
    }
}

pub const P_MIGRATE: u32 = 1;
pub const P_CREATE_IF_MISSING: u32 = 2;
pub const P_MKDIR: u32 = 3;
pub const P_MOVE_FROM: u32 = 4;
pub const P_MOVE_TO: u32 = 5;
pub const P_BASE_SAVE: u32 = 6;
pub const P_BASE_CONFIG: u32 = 7;
pub const P_BASE_STATE: u32 = 8;
pub const P_CRASH_FLAG: u32 = 9;
pub const P_FIRST_START_FLAG: u32 = 10;
pub const P_LOG_FILE: u32 = 11;
pub const P_UPDATE_LOG_FILE: u32 = 12;
pub const P_GAME_RELATIVE: u32 = 13;

fn emit_ops(o: &Out, ops: &[base::PathOp]) {
    for p in ops {
        match p {
            base::PathOp::Migrate(a) => o.wide(P_MIGRATE, a),
            base::PathOp::CreateIfMissing(a) => o.wide(P_CREATE_IF_MISSING, a),
            base::PathOp::Mkdir(a) => o.wide(P_MKDIR, a),
            base::PathOp::MoveIfMissing(a, b) => {
                o.wide(P_MOVE_FROM, a);
                o.wide(P_MOVE_TO, b);
            }
        }
    }
}

/// # Safety
/// Buffers are live for their lengths; `emit` accepts `ectx`. `pid` is used when `headless`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_plan_paths(
    personal: Str,
    config_home: Str,
    state_home: Str,
    save_folder: Str,
    custom_story: Str,
    headless: bool,
    pid: i32,
    emit: Emit,
    ectx: Ctx,
) {
    let w = |s: Str| units(s.p.cast::<u32>(), s.n);
    let plan = base::plan_paths(
        w(personal),
        w(config_home),
        w(state_home),
        w(save_folder),
        w(custom_story),
        headless.then_some(pid),
    );
    let o = Out { emit, ctx: ectx };
    emit_ops(&o, &plan.ops);
    o.wide(P_BASE_SAVE, &plan.base_save);
    o.wide(P_BASE_CONFIG, &plan.base_config);
    o.wide(P_BASE_STATE, &plan.base_state);
    o.wide(P_CRASH_FLAG, &plan.crash_flag);
    o.wide(P_FIRST_START_FLAG, &plan.first_start_flag);
    o.wide(P_LOG_FILE, &plan.log_file);
    o.wide(P_UPDATE_LOG_FILE, &plan.update_log_file);
    o.wide(P_GAME_RELATIVE, &plan.game_relative);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_use_wanted_config(force_default: bool, wanted_exists: bool) -> bool {
    base::use_wanted_config(force_default, wanted_exists)
}

pub const V_DEFAULT_PROFILE: u32 = 1;

/// # Safety
/// `ctx`/`call` as in the module docs; `out` points to 5 writable bools
/// (save at exit, exit menu directly, show pre menu, show menu, update log active).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_main_vars(
    ctx: Ctx,
    call: Call,
    out: *mut bool,
    emit: Emit,
    ectx: Ctx,
) {
    let v = base::main_vars(&mut cfg(ctx, call));
    Out { emit, ctx: ectx }.wide(V_DEFAULT_PROFILE, &v.default_profile_name);
    for (i, b) in [
        v.save_config_at_exit,
        v.exit_menu_directly,
        v.show_pre_menu,
        v.show_menu,
        v.update_log_active,
    ]
    .into_iter()
    .enumerate()
    {
        // SAFETY: `out` holds five bools per the contract.
        if let Some(p) = unsafe { out.add(i).as_mut() } {
            *p = b;
        }
    }
}

/// # Safety
/// `emit` accepts `ectx`. Returns whether a message was emitted.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_feature_error(
    sm2: i32,
    draw_buffers: i32,
    packed: i32,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    base::feature_error(sm2, draw_buffers, packed).is_some_and(|m| {
        Out { emit, ctx: ectx }.bytes(1, m.as_bytes());
        true
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_start_mode(show_pre_menu: bool, show_menu: bool) -> i32 {
    base::start_mode(show_pre_menu, show_menu) as i32
}

/// -1: nothing to do, else 1 when the input is locked and 0 when not.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_direct_start_input(debug_window_active: bool, fullscreen: bool) -> i32 {
    base::direct_start_input(debug_window_active, fullscreen).map_or(-1, i32::from)
}

pub const S_MAPS_FOLDER: u32 = 1;
pub const S_START_MAP: u32 = 2;
pub const S_START_POS: u32 = 3;
pub const S_NAME: u32 = 4;
pub const S_AUTHOR: u32 = 5;
pub const S_IMG_FILE: u32 = 6;
pub const S_EXTRA_PREFIX: u32 = 7;
pub const S_DEFAULT_EXTRA: u32 = 8;
pub const S_ERROR: u32 = 9;

/// # Safety
/// `ctx`/`call` as in the module docs (cfg 0 is the story settings, op `FILE_EXISTS_W`
/// asks the file system); buffers are live; `emit` accepts `ectx`. Returns validity.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_custom_story(
    ctx: Ctx,
    call: Call,
    root: *const u32,
    root_n: usize,
    default_lang: *const u8,
    lang_n: usize,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    let h = Host { ctx, call };
    let (s, err) = base::custom_story(
        &mut cfg(ctx, call),
        units(root, root_n),
        units(default_lang, lang_n),
        &mut |p| h.b(op::FILE_EXISTS_W, &[aw(p)]),
    );
    let o = Out { emit, ctx: ectx };
    o.bytes(S_MAPS_FOLDER, &s.maps_folder);
    o.bytes(S_START_MAP, &s.start_map);
    o.bytes(S_START_POS, &s.start_pos);
    o.wide(S_NAME, &s.name);
    o.wide(S_AUTHOR, &s.author);
    o.bytes(S_IMG_FILE, &s.img_file);
    o.bytes(S_EXTRA_PREFIX, &s.extra_lang_prefix);
    o.bytes(S_DEFAULT_EXTRA, &s.default_extra_lang);
    if let Some(e) = err {
        o.bytes(S_ERROR, e.as_bytes());
    }
    err.is_none()
}

/// # Safety
/// Buffers are live for their lengths; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_custom_story_save_path(
    profile: *const u32,
    profile_n: usize,
    root: *const u32,
    root_n: usize,
    emit: Emit,
    ectx: Ctx,
) {
    let p = base::custom_story_save_path(units(profile, profile_n), units(root, root_n));
    Out { emit, ctx: ectx }.wide(1, &p);
}

pub const R_FILE: u32 = 1;
pub const R_FOLDER: u32 = 2;
pub const R_POS: u32 = 3;

#[repr(C)]
pub struct StartArgs {
    file: Str,
    folder: Str,
    pos: Str,
    has_story: bool,
    story_map: Str,
    story_folder: Str,
    story_pos: Str,
    def_file: Str,
    def_folder: Str,
    def_pos: Str,
    ptest: bool,
}

/// # Safety
/// `ctx`/`call` as in the module docs (cfg 0 is the user config); `a` is readable with live
/// strings; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_resolve_start(
    ctx: Ctx,
    call: Call,
    a: *const StartArgs,
    emit: Emit,
    ectx: Ctx,
) {
    // SAFETY: `a` is readable per the contract.
    let Some(a) = (unsafe { a.as_ref() }) else {
        return;
    };
    let story = a.has_story.then(|| base::StartStory {
        start_map: a.story_map.get(),
        maps_folder: a.story_folder.get(),
        start_pos: a.story_pos.get(),
    });
    let h = Host { ctx, call };
    let t = base::resolve_start(
        &base::StartTarget {
            file: a.file.get().to_vec(),
            folder: a.folder.get().to_vec(),
            pos: a.pos.get().to_vec(),
        },
        story,
        &mut cfg(ctx, call),
        &base::StartDefaults {
            file: a.def_file.get(),
            folder: a.def_folder.get(),
            pos: a.def_pos.get(),
        },
        a.ptest,
        &mut |f| {
            h.go(op::MAP_FOLDER, &[astr(f)]);
        },
    );
    let o = Out { emit, ctx: ectx };
    o.bytes(R_FILE, &t.file);
    o.bytes(R_FOLDER, &t.folder);
    o.bytes(R_POS, &t.pos);
}

struct LangHost(Host);

impl base::LangEnv for LangHost {
    fn clear_translations(&mut self) {
        self.0.go(op::LANG_CLEAR, &[]);
    }
    fn file_found(&mut self, name: &[u8]) -> bool {
        self.0.b(op::LANG_FOUND, &[astr(name)])
    }
    fn add_language_file(&mut self, name: &[u8], force: bool, user_dir: bool) {
        self.0
            .go(op::LANG_ADD, &[astr(name), ab(force), ab(user_dir)]);
    }
}

#[repr(C)]
pub struct LangArgs {
    name: Str,
    force: bool,
    current: Str,
    has_story: bool,
    story_root: Str,
    story_prefix: Str,
    story_default: Str,
    game_folder: Str,
    base_folder: Str,
    default_game: Str,
    default_base: Str,
}

/// # Safety
/// `ctx`/`call` as in the module docs; `a` is readable with live strings; `emit` accepts
/// `ectx`. Returns whether a language was loaded (its lowercase name is emitted).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_load_language(
    ctx: Ctx,
    call: Call,
    a: *const LangArgs,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    // SAFETY: `a` is readable per the contract.
    let Some(a) = (unsafe { a.as_ref() }) else {
        return false;
    };
    let story = a.has_story.then(|| base::LangStory {
        root: a.story_root.get(),
        extra_prefix: a.story_prefix.get(),
        default_extra: a.story_default.get(),
    });
    let r = base::load_language(
        &mut LangHost(Host { ctx, call }),
        a.name.get(),
        a.force,
        a.current.get(),
        story,
        &base::LangFolders {
            game_folder: a.game_folder.get(),
            base_folder: a.base_folder.get(),
            default_game: a.default_game.get(),
            default_base: a.default_base.get(),
        },
    );
    r.is_some_and(|low| {
        Out { emit, ctx: ectx }.bytes(1, &low);
        true
    })
}

pub const PR_SAVE_PREVIOUS: u32 = 20;
pub const PR_MAIN_SAVE: u32 = 21;
pub const PR_MAIN_CONFIG: u32 = 22;
pub const PR_SAVE: u32 = 23;

/// # Safety
/// Buffers are live for their lengths; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_set_profile(
    prev: Str,
    have_user_config: bool,
    name: Str,
    base_save: Str,
    base_config: Str,
    emit: Emit,
    ectx: Ctx,
) {
    let w = |s: Str| units(s.p.cast::<u32>(), s.n);
    let p = base::set_profile(
        w(prev),
        have_user_config,
        w(name),
        w(base_save),
        w(base_config),
    );
    let o = Out { emit, ctx: ectx };
    o.num(PR_SAVE_PREVIOUS, usize::from(p.save_previous));
    o.wide(PR_MAIN_SAVE, &p.main_profile_save);
    o.wide(PR_MAIN_CONFIG, &p.main_profile_config);
    o.wide(PR_SAVE, &p.profile_save);
    emit_ops(&o, &p.ops);
}

/// # Safety
/// Buffers are live for their lengths; `emit` accepts `ectx`. Returns whether to create it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_profile_folder(
    base_save: Str,
    name: Str,
    exists: bool,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    let w = |s: Str| units(s.p.cast::<u32>(), s.n);
    base::profile_folder(w(base_save), w(name), exists).is_some_and(|d| {
        Out { emit, ctx: ectx }.wide(1, &d);
        true
    })
}

/// # Safety
/// `out` points to 4 writable floats (size x, y, offset x, y).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_calc_gui_set_offset(
    vx: f32,
    vy: f32,
    sx: f32,
    sy: f32,
    out: *mut f32,
) {
    let (s, o) = base::calc_gui_set_offset([vx, vy], [sx, sy]);
    for (i, v) in [s[0], s[1], o[0], o[1]].into_iter().enumerate() {
        // SAFETY: `out` holds four floats per the contract.
        if let Some(p) = unsafe { out.add(i).as_mut() } {
            *p = v;
        }
    }
}

/// # Safety
/// `cur` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_chinese_font(cur: *const u8, n: usize) -> bool {
    base::default_font(units(cur, n)) == "game_default.fnt"
}

/// # Safety
/// `id` is writable; `emit` accepts `ectx`. Returns false past the end.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_achievement(
    index: usize,
    id: *mut i32,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    let Some(&(i, name)) = base::ACHIEVEMENTS.get(index) else {
        return false;
    };
    // SAFETY: `id` is writable per the contract.
    if let Some(p) = unsafe { id.as_mut() } {
        *p = i;
    }
    Out { emit, ctx: ectx }.bytes(1, name.as_bytes());
    true
}

/// # Safety
/// `s` is `n` bytes; `ok` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_axis_from_name(s: *const u8, n: usize, ok: *mut bool) -> i32 {
    let (a, found) = base::axis_from_name(units(s, n));
    // SAFETY: `ok` is writable per the contract.
    if let Some(p) = unsafe { ok.as_mut() } {
        *p = found;
    }
    a
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_action_count() -> usize {
    input::ACTIONS.len()
}

#[repr(C)]
pub struct ActionC {
    id: i32,
    configurable: bool,
    cat: i32,
}

/// # Safety
/// `out` is writable; `emit` accepts `ectx`. Returns false past the end.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_action(
    index: usize,
    out: *mut ActionC,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    let Some(a) = input::ACTIONS.get(index) else {
        return false;
    };
    // SAFETY: `out` is writable per the contract.
    if let Some(p) = unsafe { out.as_mut() } {
        *p = ActionC {
            id: a.id,
            configurable: a.configurable,
            cat: a.cat,
        };
    }
    Out { emit, ctx: ectx }.bytes(1, a.name.as_bytes());
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_input_count() -> usize {
    input::INPUTS.len()
}

/// # Safety
/// `value` and `action` are writable; `emit` accepts `ectx`. Returns false past the end.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_input(
    index: usize,
    value: *mut i32,
    action: *mut i32,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    let Some(x) = input::INPUTS.get(index) else {
        return false;
    };
    // SAFETY: both are writable per the contract.
    unsafe {
        if let Some(p) = value.as_mut() {
            *p = x.value;
        }
        if let Some(p) = action.as_mut() {
            *p = x.action;
        }
    }
    Out { emit, ctx: ectx }.bytes(1, x.ty.as_bytes());
    true
}

fn index_of(found: Option<usize>) -> i32 {
    found.and_then(|i| i32::try_from(i).ok()).unwrap_or(-1)
}

/// # Safety
/// `s` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_action_by_name(s: *const u8, n: usize) -> i32 {
    let name = units(s, n);
    index_of(
        input::ACTIONS
            .iter()
            .position(|a| a.name.as_bytes() == name),
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_action_by_id(id: i32) -> i32 {
    index_of(input::ACTIONS.iter().position(|a| a.id == id))
}

/// Table indices of the actions of a category (`CAT_LAST` all), up to `cap`; returns the count.
///
/// # Safety
/// `out` holds `cap` writable ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_actions_by_category(
    cat: i32,
    out: *mut i32,
    cap: usize,
) -> usize {
    let idx = input::ACTIONS
        .iter()
        .enumerate()
        .filter(|(_, a)| cat == input::CAT_LAST || a.cat == cat)
        .map(|(i, _)| i);
    // SAFETY: `out` holds `cap` ints per the contract.
    unsafe { fill(idx, out, cap) }
}

/// Table indices of the default inputs of an action, up to `cap`; returns the count.
///
/// # Safety
/// `out` holds `cap` writable ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_default_inputs(id: i32, out: *mut i32, cap: usize) -> usize {
    let idx = input::INPUTS
        .iter()
        .enumerate()
        .filter(|(_, x)| x.action == id)
        .map(|(i, _)| i);
    // SAFETY: `out` holds `cap` ints per the contract.
    unsafe { fill(idx, out, cap) }
}

unsafe fn fill(idx: impl Iterator<Item = usize>, out: *mut i32, cap: usize) -> usize {
    let mut n = 0;
    for (k, i) in idx.enumerate() {
        if k < cap {
            // SAFETY: `k < cap` and `out` holds `cap` ints per the contract.
            if let Some(p) = unsafe { out.add(k).as_mut() } {
                *p = i32::try_from(i).unwrap_or(-1);
            }
        }
        n = k + 1;
    }
    n
}

struct ActionHost(Host);

impl input::ActionEnv for ActionHost {
    fn create_action(&mut self, name: &[u8], id: i32) {
        self.0.go(op::A_CREATE, &[astr(name), ai(i64::from(id))]);
    }
    fn clear_sub_actions(&mut self, id: i32) {
        self.0.go(op::A_CLEAR, &[ai(i64::from(id))]);
    }
    fn add_sub_action(&mut self, id: i32, s: input::SubAction) {
        let (kind, a, b) = match s {
            input::SubAction::Key(v) => (0, v, 0),
            input::SubAction::Mouse(v) => (1, v, 0),
            input::SubAction::PadButton(v) => (2, v, 0),
            input::SubAction::PadHat(h, v) => (3, h, v),
            input::SubAction::PadAxis(x, v) => (4, x, v),
        };
        self.0.go(
            op::A_ADD,
            &[
                ai(i64::from(id)),
                ai(kind),
                ai(i64::from(a)),
                ai(i64::from(b)),
            ],
        );
    }
    fn sub_action_count(&mut self, id: i32) -> usize {
        usize::try_from(self.0.i(op::A_COUNT, &[ai(i64::from(id))])).unwrap_or(0)
    }
    fn sub_action_input(&mut self, id: i32, index: usize) -> (Vec<u8>, Vec<u8>) {
        let a = [ai(i64::from(id)), ai(index as i64)];
        (
            self.0.go(op::A_TYPE, &a).bytes,
            self.0.go(op::A_INPUT, &a).bytes,
        )
    }
}

/// # Safety
/// `ctx`/`call` as in the module docs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_create_actions(ctx: Ctx, call: Call) {
    input::create_actions(&mut ActionHost(Host { ctx, call }));
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SettingsC {
    invert_mouse: bool,
    smooth_mouse: bool,
    gamepad_look_invert: bool,
    mouse_sensitivity: f32,
    gamepad_walk_sensitivity: f32,
    gamepad_look_sensitivity: f32,
}

/// # Safety
/// `ctx`/`call` as in the module docs (cfg 0 is the user config, cfg 1 the key config);
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_load_user_config(ctx: Ctx, call: Call, out: *mut SettingsC) {
    let h = Host { ctx, call };
    let s = input::load_user_config(
        &mut CfgHost { h, base: 0 },
        &mut CfgHost { h, base: op::CFG2 },
        &mut ActionHost(h),
    );
    // SAFETY: `out` is writable per the contract.
    if let Some(p) = unsafe { out.as_mut() } {
        *p = SettingsC {
            invert_mouse: s.invert_mouse,
            smooth_mouse: s.smooth_mouse,
            gamepad_look_invert: s.gamepad_look_invert,
            mouse_sensitivity: s.mouse_sensitivity,
            gamepad_walk_sensitivity: s.gamepad_walk_sensitivity,
            gamepad_look_sensitivity: s.gamepad_look_sensitivity,
        };
    }
}

/// # Safety
/// `ctx`/`call` as in the module docs (cfg 0 is the user config, cfg 1 the key config);
/// `s` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_save_user_config(ctx: Ctx, call: Call, s: *const SettingsC) {
    // SAFETY: `s` is readable per the contract.
    let Some(s) = (unsafe { s.as_ref() }) else {
        return;
    };
    let h = Host { ctx, call };
    input::save_user_config(
        &input::Settings {
            invert_mouse: s.invert_mouse,
            smooth_mouse: s.smooth_mouse,
            mouse_sensitivity: s.mouse_sensitivity,
            gamepad_walk_sensitivity: s.gamepad_walk_sensitivity,
            gamepad_look_sensitivity: s.gamepad_look_sensitivity,
            gamepad_look_invert: s.gamepad_look_invert,
        },
        &mut CfgHost { h, base: 0 },
        &mut CfgHost { h, base: op::CFG2 },
        &mut ActionHost(h),
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_smooth_new(max: i32, prev_mul: f32) -> *mut input::Smooth {
    Box::into_raw(Box::new(input::Smooth::new(max, prev_mul)))
}

/// # Safety
/// `s` comes from `ohpl_ga9_smooth_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_smooth_free(s: *mut input::Smooth) {
    if !s.is_null() {
        // SAFETY: per the contract `s` is a live box.
        drop(unsafe { Box::from_raw(s) });
    }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_smooth_reset(s: *mut input::Smooth) {
    // SAFETY: `s` is live per the contract.
    if let Some(s) = unsafe { s.as_mut() } {
        s.reset();
    }
}

/// # Safety
/// `s` is live; `out` points to 2 writable floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_smooth_push(
    s: *mut input::Smooth,
    x: f32,
    y: f32,
    out: *mut f32,
) {
    // SAFETY: `s` is live per the contract.
    let Some(s) = (unsafe { s.as_mut() }) else {
        return;
    };
    let r = s.push([x, y]);
    // SAFETY: `out` holds two floats per the contract.
    unsafe {
        if let Some(p) = out.as_mut() {
            *p = r[0];
        }
        if let Some(p) = out.add(1).as_mut() {
            *p = r[1];
        }
    }
}

#[repr(C)]
pub struct MouseActC {
    last_x: i32,
    last_y: i32,
    active_at: f64,
}

struct MouseHost(Host);

impl input::MouseEnv for MouseHost {
    fn any_mouse_button_down(&mut self) -> bool {
        self.0.b(op::M_ANY_BUTTON, &[])
    }
    fn abs_position(&mut self) -> [i32; 2] {
        unpack(self.0.i(op::M_ABS, &[]))
    }
    fn game_time(&mut self) -> f64 {
        self.0.go(op::M_TIME, &[]).d
    }
    fn gamepad_present(&mut self) -> bool {
        self.0.b(op::M_PAD, &[])
    }
    fn ui_direction_triggered(&mut self) -> bool {
        self.0.b(op::M_UI_DIR, &[])
    }
    fn screen_size(&mut self) -> [i32; 2] {
        unpack(self.0.i(op::M_SCREEN, &[]))
    }
    fn hide_mouse_uis(&mut self) {
        self.0.go(op::M_HIDE, &[]);
    }
}

fn unpack(v: i64) -> [i32; 2] {
    [v as i32, (v >> 32) as i32]
}

/// # Safety
/// `m` is live; `ctx`/`call` as in the module docs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_mouse_step(m: *mut MouseActC, ctx: Ctx, call: Call) -> bool {
    // SAFETY: `m` is live per the contract.
    let Some(m) = (unsafe { m.as_mut() }) else {
        return false;
    };
    let mut a = input::MouseActivity {
        last_abs: [m.last_x, m.last_y],
        active_at: m.active_at,
    };
    let r = a.step(&mut MouseHost(Host { ctx, call }));
    m.last_x = a.last_abs[0];
    m.last_y = a.last_abs[1];
    m.active_at = a.active_at;
    r
}

#[repr(C)]
pub struct LookInC {
    mouse_rel_x: i32,
    mouse_rel_y: i32,
    mouse_sensitivity: f32,
    screen_height: f32,
    smooth_mouse: bool,
    invert_mouse: bool,
    pad_present: bool,
    look_sensitivity: f32,
    look_invert: bool,
}

struct LookHost(Host);

impl look::LookEnv for LookHost {
    fn is_triggered(&mut self, action: i32) -> bool {
        self.0.b(op::K_TRIGGERED, &[ai(i64::from(action))])
    }
    fn add_lean(&mut self, v: f32) {
        self.0.go(op::K_ADD_LEAN, &[af(v)]);
    }
    fn set_lean(&mut self, v: f32) {
        self.0.go(op::K_SET_LEAN, &[af(v)]);
    }
    fn scroll(&mut self, v: f32) {
        self.0.go(op::K_SCROLL, &[af(v)]);
    }
    fn stop_movement(&mut self) {
        self.0.go(op::K_STOP, &[]);
    }
    fn button_down(&mut self, button: i32) -> bool {
        self.0.b(op::K_BUTTON, &[ai(i64::from(button))])
    }
    fn axis(&mut self, axis: i32) -> f32 {
        self.0.f(op::K_AXIS, &[ai(i64::from(axis))])
    }
    fn dead_zone(&mut self) -> f32 {
        self.0.f(op::K_DEAD, &[])
    }
    fn frame_time(&mut self) -> f32 {
        self.0.f(op::K_FRAME, &[])
    }
    fn step_size(&mut self) -> f32 {
        self.0.f(op::K_STEP, &[])
    }
}

/// # Safety
/// `s` is live; `i` readable; `ctx`/`call` as in the module docs; `out` holds 2 writable floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_look(
    s: *mut input::Smooth,
    i: *const LookInC,
    ctx: Ctx,
    call: Call,
    out: *mut f32,
) {
    // SAFETY: both are valid per the contract.
    let (Some(s), Some(i)) = (unsafe { s.as_mut() }, unsafe { i.as_ref() }) else {
        return;
    };
    let r = look::look(
        s,
        &look::LookIn {
            mouse_rel: [i.mouse_rel_x, i.mouse_rel_y],
            mouse_sensitivity: i.mouse_sensitivity,
            screen_height: i.screen_height,
            smooth_mouse: i.smooth_mouse,
            invert_mouse: i.invert_mouse,
            pad_present: i.pad_present,
            look_sensitivity: i.look_sensitivity,
            look_invert: i.look_invert,
        },
        &mut LookHost(Host { ctx, call }),
    );
    // SAFETY: `out` holds two floats per the contract.
    unsafe {
        if let Some(p) = out.as_mut() {
            *p = r[0];
        }
        if let Some(p) = out.add(1).as_mut() {
            *p = r[1];
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_state_sends_to_gui(state: i32, inventory_message_active: bool) -> bool {
    input::state_sends_to_gui(state, inventory_message_active)
}

/// # Safety
/// `dir` is `dir_n` wide units, `ext` `ext_n` bytes; `ctx`/`call` as in the module docs (op
/// `FILE_EXISTS_W`); `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_screenshot_name(
    dir: *const u32,
    dir_n: usize,
    ext: *const u8,
    ext_n: usize,
    ctx: Ctx,
    call: Call,
    emit: Emit,
    ectx: Ctx,
) {
    let h = Host { ctx, call };
    let name = input::screenshot_name(units(dir, dir_n), units(ext, ext_n), &mut |p| {
        h.b(op::FILE_EXISTS_W, &[aw(p)])
    });
    Out { emit, ctx: ectx }.wide(1, &name);
}

/// The gamepad UI map: kind 0 arrows, 1 buttons; writes (action, ui id).
///
/// # Safety
/// `action` and `id` are writable. Returns false past the end.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_ui_map(
    kind: i32,
    index: usize,
    action: *mut i32,
    id: *mut i32,
) -> bool {
    let e = if kind == 0 {
        input::UI_ARROWS.get(index)
    } else {
        input::UI_BUTTONS.get(index)
    };
    let Some(&(a, i)) = e else {
        return false;
    };
    // SAFETY: both are writable per the contract.
    unsafe {
        if let Some(p) = action.as_mut() {
            *p = a;
        }
        if let Some(p) = id.as_mut() {
            *p = i;
        }
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga9_debug_new() -> *mut debug::Debug {
    Box::into_raw(Box::new(debug::Debug::default()))
}

/// # Safety
/// `d` comes from `ohpl_ga9_debug_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_free(d: *mut debug::Debug) {
    if !d.is_null() {
        // SAFETY: per the contract `d` is a live box.
        drop(unsafe { Box::from_raw(d) });
    }
}

/// The flags block at the start of the handle (`OhplGa9DebugFlags`).
///
/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_flags(d: *mut debug::Debug) -> *mut debug::Flags {
    // SAFETY: `d` is live per the contract.
    unsafe { d.as_mut() }.map_or(std::ptr::null_mut(), |d| &raw mut d.f)
}

/// # Safety
/// `d` is live; `ctx`/`call` as in the module docs (cfg 0 is the user config).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_load_user_config(
    d: *mut debug::Debug,
    ptest: bool,
    ctx: Ctx,
    call: Call,
) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_mut() } {
        d.load_user_config(&mut cfg(ctx, call), ptest);
    }
}

/// # Safety
/// `d` is live; `ctx`/`call` as in the module docs (cfg 0 is the user config).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_save_user_config(
    d: *mut debug::Debug,
    ctx: Ctx,
    call: Call,
) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_ref() } {
        d.save_user_config(&mut cfg(ctx, call));
    }
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_reset(d: *mut debug::Debug) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_mut() } {
        d.reset();
    }
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_on_map_enter(d: *mut debug::Debug) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_mut() } {
        d.on_map_enter();
    }
}

/// # Safety
/// `d` is live; `p` is `n` wide units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_add_error(d: *mut debug::Debug, p: *const u32, n: usize) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_mut() } {
        d.add_error_or_warning(units(p, n));
    }
}

/// # Safety
/// `d` is live; `p` is `n` wide units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_add_message(
    d: *mut debug::Debug,
    p: *const u32,
    n: usize,
    check_duplicates: bool,
) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_mut() } {
        d.add_message(units(p, n), check_duplicates);
    }
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_update_messages(d: *mut debug::Debug, dt: f32) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_mut() } {
        d.update_messages(dt);
    }
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_message_count(d: *const debug::Debug) -> usize {
    // SAFETY: `d` is live per the contract.
    unsafe { d.as_ref() }.map_or(0, |d| d.messages.len())
}

/// Message `index` (0 is the newest): its text is emitted, its remaining time returned.
///
/// # Safety
/// `d` is live; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_message(
    d: *const debug::Debug,
    index: usize,
    emit: Emit,
    ectx: Ctx,
) -> f32 {
    // SAFETY: `d` is live per the contract.
    let Some(m) = (unsafe { d.as_ref() }).and_then(|d| d.messages.get(index)) else {
        return 0.0;
    };
    Out { emit, ctx: ectx }.wide(1, &m.text);
    m.count
}

/// Returns the new speed multiplier, or 0 when nothing changed.
///
/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_set_fast_forward(d: *mut debug::Debug, on: bool) -> f32 {
    // SAFETY: `d` is live per the contract.
    unsafe { d.as_mut() }
        .and_then(|d| d.set_fast_forward(on))
        .unwrap_or(0.0)
}

/// # Safety
/// `d` is live; `first` and `due` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_update_tick(
    d: *mut debug::Debug,
    first: *mut bool,
    due: *mut bool,
) {
    // SAFETY: all valid per the contract.
    unsafe {
        let Some(d) = d.as_mut() else {
            return;
        };
        let (f, b) = d.update_tick();
        if let Some(p) = first.as_mut() {
            *p = f;
        }
        if let Some(p) = due.as_mut() {
            *p = b;
        }
    }
}

/// # Safety
/// `d` is live; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_batch_pop(d: *mut debug::Debug, emit: Emit, ectx: Ctx) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_mut() } {
        Out { emit, ctx: ectx }.bytes(1, &d.batch_pop());
    }
}

/// # Safety
/// `d` is live; `p` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_load_batch(d: *mut debug::Debug, p: *const u8, n: usize) {
    // SAFETY: `d` is live per the contract.
    if let Some(d) = unsafe { d.as_mut() } {
        d.load_batch(units(p, n));
    }
}

/// `SetDebugWindowActive`: false when the debug menu is not loaded; else `lock` is true when
/// the cursor lock changes.
///
/// # Safety
/// `lock` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_window_change(
    load_debug_menu: bool,
    active: bool,
    fullscreen: bool,
    lock: *mut bool,
) -> bool {
    debug::Debug::window_change(load_debug_menu, active, fullscreen).is_some_and(|(_, l)| {
        // SAFETY: `lock` is writable per the contract.
        if let Some(p) = unsafe { lock.as_mut() } {
            *p = l;
        }
        true
    })
}

#[repr(C)]
pub struct ToggleC {
    kind: i32,
    on: bool,
    value: f32,
}

/// `ChangeDebugText`: the effect left for the engine (kind 0 none).
///
/// # Safety
/// `d` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_debug_change_text(
    d: *mut debug::Debug,
    num: i32,
    val: i32,
    out: *mut ToggleC,
) {
    // SAFETY: both valid per the contract.
    let (Some(d), Some(out)) = (unsafe { d.as_mut() }, unsafe { out.as_mut() }) else {
        return;
    };
    let (kind, on, value) = match d.change_text(num, val) {
        debug::Toggle::None => (0, false, 0.0),
        debug::Toggle::OcclusionCulling(b) => (1, b, 0.0),
        debug::Toggle::LogCreateAndDelete(b) => (2, b, 0.0),
        debug::Toggle::FastPhysicsLoad(b) => (3, b, 0.0),
        debug::Toggle::FreeCam(b) => (4, b, 0.0),
        debug::Toggle::FreeCamSpeed(v) => (5, false, v),
        debug::Toggle::FastForward(b) => (6, b, 0.0),
    };
    *out = ToggleC { kind, on, value };
}

/// # Safety
/// `start` and `fwd` hold 3 floats; `out` holds 9 writable floats (end, min, max).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_inspection_line(
    start: *const f32,
    fwd: *const f32,
    out: *mut f32,
) {
    let v3 = |p: *const f32| {
        let s = units(p, 3);
        [0, 1, 2].map(|i| s.get(i).copied().unwrap_or(0.0))
    };
    let (end, min, max) = debug::inspection_line(v3(start), v3(fwd));
    for (i, v) in end.into_iter().chain(min).chain(max).enumerate() {
        // SAFETY: `out` holds nine floats per the contract.
        if let Some(p) = unsafe { out.add(i).as_mut() } {
            *p = v;
        }
    }
}

struct HeadlessHost(Host);

impl headless::Req for HeadlessHost {
    fn has(&self, key: &[u8]) -> bool {
        self.0.b(op::Q_HAS, &[astr(key)])
    }
    fn string(&self, key: &[u8], def: &[u8]) -> Vec<u8> {
        self.0.go(op::Q_STR, &[astr(key), astr(def)]).bytes
    }
    fn int(&self, key: &[u8], def: i32) -> i32 {
        self.0.i(op::Q_INT, &[astr(key), ai(i64::from(def))]) as i32
    }
    fn float(&self, key: &[u8], def: f32) -> f32 {
        self.0.f(op::Q_FLOAT, &[astr(key), af(def)])
    }
    fn bool(&self, key: &[u8], def: bool) -> bool {
        self.0.b(op::Q_BOOL, &[astr(key), ab(def)])
    }
}

impl headless::Resp for HeadlessHost {
    fn error(&mut self, msg: &[u8]) {
        self.0.go(op::P_ERROR, &[astr(msg)]);
    }
    fn set_str(&mut self, key: &[u8], v: &[u8]) {
        self.0.go(op::P_STR, &[astr(key), astr(v)]);
    }
    fn set_int(&mut self, key: &[u8], v: i32) {
        self.0.go(op::P_INT, &[astr(key), ai(i64::from(v))]);
    }
    fn set_float(&mut self, key: &[u8], v: f32) {
        self.0.go(op::P_FLOAT, &[astr(key), af(v)]);
    }
    fn set_bool(&mut self, key: &[u8], v: bool) {
        self.0.go(op::P_BOOL, &[astr(key), ab(v)]);
    }
    fn set_raw(&mut self, key: &[u8], v: &[u8]) {
        self.0.go(op::P_RAW, &[astr(key), astr(v)]);
    }
}

impl headless::Env for HeadlessHost {
    fn map_loaded(&mut self) -> bool {
        self.0.b(op::H_MAP_LOADED, &[])
    }
    fn run_script(&mut self, line: &[u8]) {
        self.0.go(op::H_RUN_SCRIPT, &[astr(line)]);
    }
    fn set_debug_gbuffer(&mut self, on: bool) {
        self.0.go(op::H_SET_GBUFFER, &[ab(on)]);
    }
    fn deferred_available(&mut self) -> bool {
        self.0.b(op::H_DEFERRED, &[])
    }
    fn gbuffer_texture(&mut self, target: i32) -> Option<(i32, i32)> {
        let r = self.0.go(op::H_GBUFFER_SIZE, &[ai(i64::from(target))]);
        let [w, h] = unpack(r.i);
        (r.f != 0.0).then_some((w, h))
    }
    fn gbuffer_pixels(&mut self, target: i32) -> Option<Vec<f32>> {
        let r = self.0.go(op::H_GBUFFER_PIXELS, &[ai(i64::from(target))]);
        (r.i != 0).then(|| {
            r.bytes
                .chunks_exact(4)
                .filter_map(|c| c.try_into().ok().map(f32::from_ne_bytes))
                .collect()
        })
    }
    fn map_name(&mut self) -> Vec<u8> {
        self.0.go(op::H_MAP_NAME, &[]).bytes
    }
    fn map_file(&mut self) -> Vec<u8> {
        self.0.go(op::H_MAP_FILE, &[]).bytes
    }
    fn player_body(&mut self) -> Option<headless::Body> {
        if !self.0.b(op::H_BODY, &[]) {
            return None;
        }
        let v = |i: i64| self.0.f(op::H_BODY_VALUE, &[ai(i)]);
        Some(headless::Body {
            pos: [v(0), v(1), v(2)],
            vel: [v(3), v(4), v(5)],
            health: v(6),
            sanity: v(7),
        })
    }
    fn fps(&mut self) -> f32 {
        self.0.f(op::H_FPS, &[])
    }
    fn occlusion_culling(&mut self) -> bool {
        self.0.b(op::H_OCCLUSION, &[])
    }
    fn set_occlusion_culling(&mut self, on: bool) {
        self.0.go(op::H_SET_OCCLUSION, &[ab(on)]);
    }
    fn set_gpu_timing(&mut self, on: bool) {
        self.0.go(op::H_GPU, &[ab(on)]);
    }
    fn render_stats_json(&mut self) -> Vec<u8> {
        self.0.go(op::H_RENDER_JSON, &[]).bytes
    }
    fn save_game(&mut self, file: &[u8]) {
        self.0.go(op::H_SAVE, &[astr(file)]);
    }
    fn file_exists(&mut self, file: &[u8]) -> bool {
        self.0.b(op::H_FILE_EXISTS, &[astr(file)])
    }
    fn load_game(&mut self, file: &[u8]) {
        self.0.go(op::H_LOAD, &[astr(file)]);
    }
    fn has_player(&mut self) -> bool {
        self.0.b(op::H_PLAYER, &[])
    }
    fn teleport(&mut self, pos: [f32; 3]) {
        self.0
            .go(op::H_TELEPORT, &[af(pos[0]), af(pos[1]), af(pos[2])]);
    }
    fn have_user_config(&mut self) -> bool {
        self.0.b(op::H_HAVE_USER_CFG, &[])
    }
    fn create_and_set_default_profile(&mut self) {
        self.0.go(op::H_DEFAULT_PROFILE, &[]);
    }
    fn init_user_config(&mut self) -> bool {
        self.0.b(op::H_INIT_USER_CFG, &[])
    }
    fn enter_game_input(&mut self) {
        self.0.go(op::H_ENTER_GAME, &[]);
    }
    fn start_game(&mut self, map: &[u8], pos: &[u8]) -> bool {
        self.0.b(op::H_START_GAME, &[astr(map), astr(pos)])
    }
    fn camera_pose(&mut self) -> Option<([f32; 3], f32, f32)> {
        if !self.0.b(op::H_CAMERA, &[]) {
            return None;
        }
        let v = |i: i64| self.0.f(op::H_CAMERA_VALUE, &[ai(i)]);
        Some(([v(0), v(1), v(2)], v(3), v(4)))
    }
    fn set_camera_pos(&mut self, pos: [f32; 3]) {
        self.0
            .go(op::H_SET_CAMERA_POS, &[af(pos[0]), af(pos[1]), af(pos[2])]);
    }
    fn set_camera_pitch(&mut self, v: f32) {
        self.0.go(op::H_SET_PITCH, &[af(v)]);
    }
    fn set_camera_yaw(&mut self, v: f32) {
        self.0.go(op::H_SET_YAW, &[af(v)]);
    }
}

/// Runs headless command `cmd`: 0 run_script, 1 set_debug_gbuffer, 2 read_gbuffer_stats,
/// 3 state, 4 render_stats, 5 save_game, 6 load_game, 7 teleport, 8 start_map, 9 camera_state
/// (AMFP), 10 set_camera (AMFP).
///
/// # Safety
/// `ctx`/`call` as in the module docs (request, response and engine ops).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_headless(cmd: u32, ctx: Ctx, call: Call) {
    let mk = || HeadlessHost(Host { ctx, call });
    match cmd {
        0 => headless::run_script(&mk(), &mut mk(), &mut mk()),
        1 => headless::set_debug_gbuffer(&mk(), &mut mk()),
        2 => headless::read_gbuffer_stats(&mk(), &mut mk(), &mut mk()),
        3 => headless::state(&mk(), &mut mk(), &mut mk()),
        4 => headless::render_stats(&mk(), &mut mk(), &mut mk()),
        5 => headless::save_game(&mk(), &mut mk(), &mut mk()),
        6 => headless::load_game(&mk(), &mut mk(), &mut mk()),
        7 => headless::teleport(&mk(), &mut mk(), &mut mk()),
        8 => headless::start_map(&mk(), &mut mk(), &mut mk()),
        9 => headless::camera_state(&mut mk(), &mut mk()),
        10 => headless::set_camera(&mk(), &mut mk(), &mut mk()),
        _ => {}
    }
}

/// # Safety
/// `cmd` is `n` bytes; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_amfp_init_config_file(
    cmd: *const u8,
    n: usize,
    emit: Emit,
    ectx: Ctx,
) {
    Out { emit, ctx: ectx }.wide(1, &amfp::init_config_file(units(cmd, n)));
}

pub const AM_RESOURCES: u32 = 1;
pub const AM_MATERIALS: u32 = 2;
pub const AM_GAME_NAME: u32 = 3;
pub const AM_ERROR: u32 = 4;

/// # Safety
/// `ctx`/`call` as in the module docs (cfg 0 is the init file); `file` is `n` wide units;
/// `emit` accepts `ectx`. Returns false with the message under `AM_ERROR`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_amfp_load_vars(
    ctx: Ctx,
    call: Call,
    loaded: bool,
    file: *const u32,
    n: usize,
    emit: Emit,
    ectx: Ctx,
) -> bool {
    let o = Out { emit, ctx: ectx };
    match amfp::load_vars(loaded, units(file, n), &mut cfg(ctx, call)) {
        Err(m) => {
            o.wide(AM_ERROR, &m);
            false
        }
        Ok(v) => {
            o.bytes(AM_RESOURCES, &v.resource_config);
            o.bytes(AM_MATERIALS, &v.material_config);
            o.bytes(AM_GAME_NAME, &v.game_name);
            true
        }
    }
}

/// Window caption (kind 0) or startup banner (kind 1) for a game name.
///
/// # Safety
/// `name` is `n` bytes; `emit` accepts `ectx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga9_amfp_text(
    kind: i32,
    name: *const u8,
    n: usize,
    emit: Emit,
    ectx: Ctx,
) {
    let name = units(name, n);
    let t = if kind == 0 {
        amfp::window_caption(name)
    } else {
        amfp::banner(name)
    };
    Out { emit, ctx: ectx }.bytes(1, &t);
}
