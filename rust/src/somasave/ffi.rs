//! C ABI of the SOMA save/base layer, mirrored in `soma/src/game/RustSomaSave.h`.

use super::base::{self, CfgHost, ConfigVals, Value};
use super::codec::{In, Out};
use super::header::{self, HeaderIn, HeaderOut, LoadStatus};
use super::names::{self, Date, NameState, NamesHost};
use super::rec::{
    Agent, Billboard, CamTex, Camera, Colliding, EntityHead, EntityTail, Fog, FogArea, Light,
    MapTimer, OwnedLight, Particle, Player, Slice, Sound, Spawned, Str, Timers, Tone, World,
};
use super::script::{self, Addr, Kind, ScriptHost};
use super::world::{self, ReadHost, WriteHost};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink8 = unsafe extern "C" fn(Ctx, *const u8, usize);
type Sink32 = unsafe extern "C" fn(Ctx, *const u32, usize);

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller of the entry point passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn units<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller of the entry point passes a live `n`-unit buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn push8(sink: Sink8, ctx: Ctx, b: &[u8]) {
    // SAFETY: `sink` accepts `ctx`.
    unsafe { sink(ctx, b.as_ptr(), b.len()) }
}

fn push32(sink: Sink32, ctx: Ctx, b: &[u32]) {
    // SAFETY: `sink` accepts `ctx`.
    unsafe { sink(ctx, b.as_ptr(), b.len()) }
}

/// # Safety
/// `data` is `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_game_id(data: *const u8, n: usize) -> i32 {
    match base::game_id(bytes(data, n)) {
        "rebirth" => 1,
        "bunker" => 2,
        _ => 0,
    }
}

#[repr(C)]
pub struct OhplGs4CfgHost {
    pub ctx: Ctx,
    pub get_float: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, f32) -> f32,
    pub get_bool: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, bool) -> bool,
    pub get_int: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, i32) -> i32,
    pub set_float: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, f32),
    pub set_bool: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, bool),
    pub set_int: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, i32),
}

struct CCfg<'a>(&'a OhplGs4CfgHost);

// SAFETY (all calls): the entry point's caller guarantees the table accepts its `ctx`.
impl CfgHost for CCfg<'_> {
    fn get_float(&mut self, l: &str, k: &str, d: f32) -> f32 {
        unsafe { (self.0.get_float)(self.0.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), d) }
    }
    fn get_bool(&mut self, l: &str, k: &str, d: bool) -> bool {
        unsafe { (self.0.get_bool)(self.0.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), d) }
    }
    fn get_int(&mut self, l: &str, k: &str, d: i32) -> i32 {
        unsafe { (self.0.get_int)(self.0.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), d) }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OhplGs4Config {
    pub master_volume: f32,
    pub gamma: f32,
    pub screen_width: i32,
    pub screen_height: i32,
    pub vsync: bool,
    pub fullscreen: bool,
    pub anti_aliasing: bool,
    pub show_subtitles: bool,
}

impl From<OhplGs4Config> for ConfigVals {
    fn from(c: OhplGs4Config) -> Self {
        ConfigVals {
            master_volume: c.master_volume,
            gamma: c.gamma,
            vsync: c.vsync,
            fullscreen: c.fullscreen,
            screen_width: c.screen_width,
            screen_height: c.screen_height,
            anti_aliasing: c.anti_aliasing,
            show_subtitles: c.show_subtitles,
        }
    }
}

impl From<ConfigVals> for OhplGs4Config {
    fn from(c: ConfigVals) -> Self {
        OhplGs4Config {
            master_volume: c.master_volume,
            gamma: c.gamma,
            screen_width: c.screen_width,
            screen_height: c.screen_height,
            vsync: c.vsync,
            fullscreen: c.fullscreen,
            anti_aliasing: c.anti_aliasing,
            show_subtitles: c.show_subtitles,
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs4_cfg_default() -> OhplGs4Config {
    ConfigVals::default().into()
}

/// # Safety
/// `host` and `cfg` are valid; the table accepts its `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_cfg_load(host: *const OhplGs4CfgHost, cfg: *mut OhplGs4Config) {
    // SAFETY: valid pointers per the contract.
    let (Some(host), Some(cfg)) = (unsafe { host.as_ref() }, unsafe { cfg.as_mut() }) else {
        return;
    };
    let mut v = ConfigVals::from(*cfg);
    base::load_config(&mut CCfg(host), &mut v);
    *cfg = v.into();
}

/// # Safety
/// `host` and `cfg` are valid; the table accepts its `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_cfg_save(host: *const OhplGs4CfgHost, cfg: *const OhplGs4Config) {
    // SAFETY: valid pointers per the contract.
    let (Some(host), Some(cfg)) = (unsafe { host.as_ref() }, unsafe { cfg.as_ref() }) else {
        return;
    };
    for (l, k, v) in base::save_config(&ConfigVals::from(*cfg)) {
        // SAFETY: the table accepts its `ctx`.
        unsafe {
            match v {
                Value::Float(f) => {
                    (host.set_float)(host.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), f)
                }
                Value::Bool(b) => {
                    (host.set_bool)(host.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), b)
                }
                Value::Int(x) => {
                    (host.set_int)(host.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), x)
                }
            }
        }
    }
}

/// Directories to create in order (`is_file` false), then the file path (`is_file` true).
/// `kind`: 0 saves below the data home, 1 settings below the config home, 2 log below the state home.
///
/// # Safety
/// `root` is `root_n` units, `id` is `id_n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_paths(
    kind: i32,
    root: *const u32,
    root_n: usize,
    id: *const u8,
    id_n: usize,
    headless: bool,
    pid: i32,
    sink: unsafe extern "C" fn(Ctx, *const u32, usize, bool),
    ctx: Ctx,
) {
    let root = units(root, root_n);
    let id = match bytes(id, id_n) {
        b"rebirth" => "rebirth",
        b"bunker" => "bunker",
        _ => "soma",
    };
    let (dirs, file) = match kind {
        0 => (base::save_dirs(root, id), None),
        1 => {
            let (d, f) = base::config_dirs(root, id);
            (d, Some(f))
        }
        _ => {
            let (d, f) = base::log_dirs(root, id, headless, pid);
            (d, Some(f))
        }
    };
    for d in &dirs {
        // SAFETY: `sink` accepts `ctx`.
        unsafe { sink(ctx, d.as_ptr(), d.len(), false) };
    }
    if let Some(f) = file {
        // SAFETY: as above.
        unsafe { sink(ctx, f.as_ptr(), f.len(), true) };
    }
}

/// # Safety
/// `cmd` is `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_init_config_file(
    cmd: *const u8,
    n: usize,
    sink: Sink32,
    ctx: Ctx,
) {
    push32(sink, ctx, &base::init_config_file(bytes(cmd, n)));
}

/// # Safety
/// `s` is `n` units; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_init_config_error(
    s: *const u32,
    n: usize,
    sink: Sink32,
    ctx: Ctx,
) {
    push32(sink, ctx, &base::init_config_error(units(s, n)));
}

/// # Safety
/// `s` is `n` units; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_error_message(s: *const u32, n: usize, sink: Sink32, ctx: Ctx) {
    push32(sink, ctx, &base::error_message(units(s, n)));
}

#[repr(C)]
pub struct OhplGs4EnginePlan {
    pub linear_depth_texture_idx: i32,
    pub gbuffer_textures: i32,
    pub shadow_map_compare_off: bool,
    pub depth_in_normal_alpha: bool,
    pub keep_hpl3_animations: bool,
    pub hardware_srgb: bool,
}

/// `id`: 0 soma, 1 rebirth, 2 bunker. Indices are -1 when the game leaves the default.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs4_engine_plan(id: i32) -> OhplGs4EnginePlan {
    let p = base::engine_plan(match id {
        1 => "rebirth",
        2 => "bunker",
        _ => "soma",
    });
    OhplGs4EnginePlan {
        linear_depth_texture_idx: p.linear_depth_texture_idx.unwrap_or(-1),
        gbuffer_textures: p.gbuffer_textures.unwrap_or(-1),
        shadow_map_compare_off: p.shadow_map_compare_off,
        depth_in_normal_alpha: p.depth_in_normal_alpha,
        keep_hpl3_animations: p.keep_hpl3_animations,
        hardware_srgb: p.hardware_srgb,
    }
}

/// `freecam` is NULL when the variable is unset.
///
/// # Safety
/// `freecam` is NULL or a NUL-terminated string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_use_real_player(freecam: *const u8) -> bool {
    let f = if freecam.is_null() {
        None
    } else {
        // SAFETY: NUL-terminated per the contract.
        Some(unsafe { std::ffi::CStr::from_ptr(freecam.cast()) }.to_bytes())
    };
    base::use_real_player(f)
}

/// Returns false when no test map is configured; otherwise sinks the map then the start position.
///
/// # Safety
/// Each of `map` / `pos` is NULL or NUL-terminated; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_boot_map(
    map: *const u8,
    pos: *const u8,
    sink: Sink8,
    ctx: Ctx,
) -> bool {
    let c = |p: *const u8| {
        // SAFETY: NUL-terminated per the contract.
        (!p.is_null()).then(|| unsafe { std::ffi::CStr::from_ptr(p.cast()) }.to_bytes())
    };
    let Some((m, p)) = base::boot_map(c(map), c(pos)) else {
        return false;
    };
    push8(sink, ctx, &m);
    push8(sink, ctx, &p);
    true
}

/// # Safety
/// `s` is `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_menu_map(s: *const u8, n: usize, sink: Sink8, ctx: Ctx) {
    push8(sink, ctx, &base::menu_map(bytes(s, n)));
}

/// 0 no area, 1 the first, 2 by name.
///
/// # Safety
/// `s` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_start_area(s: *const u8, n: usize) -> i32 {
    match base::start_area(bytes(s, n)) {
        base::StartArea::NoArea => 0,
        base::StartArea::First => 1,
        base::StartArea::Named => 2,
    }
}

/// Returns true for a script check; then `sink` gets report, game dir, api path.
///
/// # Safety
/// Each argument is NULL or NUL-terminated; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_main_plan(
    report: *const u8,
    game_dir: *const u8,
    api: *const u8,
    sink: Sink8,
    ctx: Ctx,
) -> bool {
    let c = |p: *const u8| {
        // SAFETY: NUL-terminated per the contract.
        (!p.is_null()).then(|| unsafe { std::ffi::CStr::from_ptr(p.cast()) }.to_bytes())
    };
    match base::main_plan(c(report), c(game_dir), c(api)) {
        base::MainPlan::Engine => false,
        base::MainPlan::ScriptCheck {
            report,
            game_dir,
            api,
        } => {
            push8(sink, ctx, &report);
            push8(sink, ctx, &game_dir);
            push8(sink, ctx, &api);
            true
        }
    }
}

#[repr(C)]
pub struct OhplGs4NamesHost {
    pub ctx: Ctx,
    pub translate_level: unsafe extern "C" fn(Ctx, *const u8, usize, *mut *const u32, *mut usize),
    pub local_mtime: unsafe extern "C" fn(Ctx, *const u32, usize, *mut i32),
}

struct CNames<'a>(&'a OhplGs4NamesHost);

impl NamesHost for CNames<'_> {
    fn translate_level(&mut self, key: &[u8]) -> Vec<u32> {
        let (mut p, mut n) = (std::ptr::null(), 0usize);
        // SAFETY: the table accepts its `ctx`; it returns a buffer valid until its next call.
        unsafe { (self.0.translate_level)(self.0.ctx, key.as_ptr(), key.len(), &mut p, &mut n) };
        units(p, n).to_vec()
    }

    fn local_mtime(&mut self, file: &[u32]) -> [i32; 6] {
        let mut t = [0i32; 6];
        // SAFETY: as above; `t` holds the six fields.
        unsafe { (self.0.local_mtime)(self.0.ctx, file.as_ptr(), file.len(), t.as_mut_ptr()) };
        t
    }
}

/// # Safety
/// `st` is valid; `prefix`, `map` and `suffix` are live buffers; `sink` accepts `ctx`.
/// An empty `map` stands for a missing display name.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_save_name(
    st: *mut NameState,
    prefix: *const u32,
    prefix_n: usize,
    map: *const u8,
    map_n: usize,
    date: *const Date,
    suffix: *const u32,
    suffix_n: usize,
    sink: Sink32,
    ctx: Ctx,
) {
    // SAFETY: valid per the contract.
    let (Some(st), Some(date)) = (unsafe { st.as_mut() }, unsafe { date.as_ref() }) else {
        return;
    };
    let n = names::save_name(
        st,
        units(prefix, prefix_n),
        bytes(map, map_n),
        date,
        units(suffix, suffix_n),
    );
    push32(sink, ctx, &n);
}

#[repr(C)]
pub struct OhplGs4SaveFile {
    pub file: *const u32,
    pub file_n: usize,
    pub date: Date,
}

/// Sorted newest first: `sink(ctx, source index, name, date, suffix)`.
///
/// # Safety
/// `files` is `n` records with live buffers; the tables accept their `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_list_saves(
    host: *const OhplGs4NamesHost,
    files: *const OhplGs4SaveFile,
    n: usize,
    sink: unsafe extern "C" fn(Ctx, usize, *const u32, usize, *const u8, usize, *const u32, usize),
    ctx: Ctx,
) {
    // SAFETY: valid per the contract.
    let Some(host) = (unsafe { host.as_ref() }) else {
        return;
    };
    let recs: &[OhplGs4SaveFile] = if n == 0 || files.is_null() {
        &[]
    } else {
        // SAFETY: `n` records per the contract.
        unsafe { std::slice::from_raw_parts(files, n) }
    };
    let list: Vec<(Vec<u32>, Date)> = recs
        .iter()
        .map(|r| (units(r.file, r.file_n).to_vec(), r.date))
        .collect();
    for s in names::list_saves(&mut CNames(host), &list) {
        // SAFETY: `sink` accepts `ctx`.
        unsafe {
            sink(
                ctx,
                s.src,
                s.name.as_ptr(),
                s.name.len(),
                s.date_str.as_ptr(),
                s.date_str.len(),
                s.suffix.as_ptr(),
                s.suffix.len(),
            );
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OhplGs4W {
    pub p: *const u32,
    pub n: usize,
}

/// `out[i]` is set for each list entry the rotation removes (max 1: the first match).
///
/// # Safety
/// `suffixes` is `n` records, `out` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_rotation(
    suffixes: *const OhplGs4W,
    n: usize,
    suffix: *const u32,
    suffix_n: usize,
    max: i32,
    out: *mut u8,
) {
    if n == 0 || suffixes.is_null() || out.is_null() {
        return;
    }
    // SAFETY: `n` records and `n` bytes per the contract.
    let (recs, out) = unsafe {
        (
            std::slice::from_raw_parts(suffixes, n),
            std::slice::from_raw_parts_mut(out, n),
        )
    };
    let sufs: Vec<&[u32]> = recs.iter().map(|r| units(r.p, r.n)).collect();
    out.fill(0);
    for i in names::rotation_victims(&sufs, units(suffix, suffix_n), max) {
        if let Some(o) = out.get_mut(i) {
            *o = 1;
        }
    }
}

#[repr(C)]
pub struct OhplGs4ScriptHost {
    pub ctx: Ctx,
    pub handle_obj: unsafe extern "C" fn(Ctx, usize, i32, *mut usize) -> bool,
    pub resolve_handle: unsafe extern "C" fn(Ctx, usize, i32, *const u8, usize, *mut usize) -> bool,
    pub obj_class: unsafe extern "C" fn(Ctx, usize, *mut *const u8, *mut usize),
    pub prim_size: unsafe extern "C" fn(Ctx, i32) -> usize,
    pub kind: unsafe extern "C" fn(Ctx, i32, *mut usize) -> i32,
    pub string_get: unsafe extern "C" fn(Ctx, usize, *mut *const u8, *mut usize),
    pub string_set: unsafe extern "C" fn(Ctx, usize, *const u8, usize),
    pub wstring_get: unsafe extern "C" fn(Ctx, usize, *mut *const u32, *mut usize),
    pub wstring_set: unsafe extern "C" fn(Ctx, usize, *const u32, usize),
    pub array_len: unsafe extern "C" fn(Ctx, usize) -> u32,
    pub array_elem_type: unsafe extern "C" fn(Ctx, usize) -> i32,
    pub array_at: unsafe extern "C" fn(Ctx, usize, u32) -> usize,
    pub array_resize: unsafe extern "C" fn(Ctx, usize, u32),
    pub set_items: unsafe extern "C" fn(Ctx, usize) -> usize,
    pub set_item: unsafe extern "C" fn(Ctx, usize, *mut *const u8, *mut usize),
    pub set_clear: unsafe extern "C" fn(Ctx, usize),
    pub set_insert: unsafe extern "C" fn(Ctx, usize, *const u8, usize),
    pub prop_count: unsafe extern "C" fn(Ctx, usize) -> u32,
    pub prop_saved: unsafe extern "C" fn(Ctx, usize, u32) -> bool,
    pub prop_name: unsafe extern "C" fn(Ctx, usize, u32, *mut *const u8, *mut usize),
    pub prop_type_id: unsafe extern "C" fn(Ctx, usize, u32) -> i32,
    pub prop_addr: unsafe extern "C" fn(Ctx, usize, u32) -> usize,
    pub type_decl: unsafe extern "C" fn(Ctx, usize, i32, *mut *const u8, *mut usize),
}

struct CScript<'a>(&'a OhplGs4ScriptHost);

// SAFETY (all calls): the entry point's caller guarantees the table accepts its `ctx` and that the
// buffers it hands back stay valid until its next call; `Addr` values are live script memory of
// the size the walker asks for.
impl ScriptHost for CScript<'_> {
    fn handle_obj(&mut self, addr: Addr, type_id: i32) -> Option<Addr> {
        let mut o = 0usize;
        unsafe { (self.0.handle_obj)(self.0.ctx, addr, type_id, &mut o) }.then_some(o)
    }

    fn resolve_handle(&mut self, addr: Addr, type_id: i32, class: &[u8]) -> Option<Addr> {
        let mut o = 0usize;
        unsafe {
            (self.0.resolve_handle)(
                self.0.ctx,
                addr,
                type_id,
                class.as_ptr(),
                class.len(),
                &mut o,
            )
        }
        .then_some(o)
    }

    fn obj_class(&mut self, obj: Addr) -> Vec<u8> {
        let (mut p, mut n) = (std::ptr::null(), 0usize);
        unsafe { (self.0.obj_class)(self.0.ctx, obj, &mut p, &mut n) };
        bytes(p, n).to_vec()
    }

    fn prim_size(&mut self, type_id: i32) -> usize {
        unsafe { (self.0.prim_size)(self.0.ctx, type_id) }
    }

    fn kind(&mut self, type_id: i32) -> Kind {
        let mut raw = 0usize;
        match unsafe { (self.0.kind)(self.0.ctx, type_id, &mut raw) } {
            0 => Kind::None,
            1 => Kind::Enum,
            2 => Kind::ScriptObj,
            3 => Kind::Str,
            4 => Kind::WStr,
            5 => Kind::Array,
            6 => Kind::StrSet,
            7 => Kind::Raw(raw),
            _ => Kind::Other,
        }
    }

    fn read_mem(&mut self, addr: Addr, n: usize) -> Vec<u8> {
        bytes(addr as *const u8, n).to_vec()
    }

    fn write_mem(&mut self, addr: Addr, data: &[u8]) {
        if !data.is_empty() {
            // SAFETY: `addr` is live script memory of `data.len()` bytes (host contract).
            unsafe { std::ptr::copy_nonoverlapping(data.as_ptr(), addr as *mut u8, data.len()) };
        }
    }

    fn string_get(&mut self, addr: Addr) -> Vec<u8> {
        let (mut p, mut n) = (std::ptr::null(), 0usize);
        unsafe { (self.0.string_get)(self.0.ctx, addr, &mut p, &mut n) };
        bytes(p, n).to_vec()
    }

    fn string_set(&mut self, addr: Addr, s: &[u8]) {
        unsafe { (self.0.string_set)(self.0.ctx, addr, s.as_ptr(), s.len()) }
    }

    fn wstring_get(&mut self, addr: Addr) -> Vec<u32> {
        let (mut p, mut n) = (std::ptr::null(), 0usize);
        unsafe { (self.0.wstring_get)(self.0.ctx, addr, &mut p, &mut n) };
        units(p, n).to_vec()
    }

    fn wstring_set(&mut self, addr: Addr, s: &[u32]) {
        unsafe { (self.0.wstring_set)(self.0.ctx, addr, s.as_ptr(), s.len()) }
    }

    fn array_len(&mut self, addr: Addr) -> u32 {
        unsafe { (self.0.array_len)(self.0.ctx, addr) }
    }

    fn array_elem_type(&mut self, addr: Addr) -> i32 {
        unsafe { (self.0.array_elem_type)(self.0.ctx, addr) }
    }

    fn array_at(&mut self, addr: Addr, i: u32) -> Addr {
        unsafe { (self.0.array_at)(self.0.ctx, addr, i) }
    }

    fn array_resize(&mut self, addr: Addr, n: u32) {
        unsafe { (self.0.array_resize)(self.0.ctx, addr, n) }
    }

    fn set_items(&mut self, addr: Addr) -> Vec<Vec<u8>> {
        let n = unsafe { (self.0.set_items)(self.0.ctx, addr) };
        (0..n)
            .map(|i| {
                let (mut p, mut l) = (std::ptr::null(), 0usize);
                unsafe { (self.0.set_item)(self.0.ctx, i, &mut p, &mut l) };
                bytes(p, l).to_vec()
            })
            .collect()
    }

    fn set_clear(&mut self, addr: Addr) {
        unsafe { (self.0.set_clear)(self.0.ctx, addr) }
    }

    fn set_insert(&mut self, addr: Addr, s: &[u8]) {
        unsafe { (self.0.set_insert)(self.0.ctx, addr, s.as_ptr(), s.len()) }
    }

    fn prop_count(&mut self, obj: Addr) -> u32 {
        unsafe { (self.0.prop_count)(self.0.ctx, obj) }
    }

    fn prop_saved(&mut self, obj: Addr, i: u32) -> bool {
        unsafe { (self.0.prop_saved)(self.0.ctx, obj, i) }
    }

    fn prop_name(&mut self, obj: Addr, i: u32) -> Vec<u8> {
        let (mut p, mut n) = (std::ptr::null(), 0usize);
        unsafe { (self.0.prop_name)(self.0.ctx, obj, i, &mut p, &mut n) };
        bytes(p, n).to_vec()
    }

    fn prop_type_id(&mut self, obj: Addr, i: u32) -> i32 {
        unsafe { (self.0.prop_type_id)(self.0.ctx, obj, i) }
    }

    fn prop_addr(&mut self, obj: Addr, i: u32) -> Addr {
        unsafe { (self.0.prop_addr)(self.0.ctx, obj, i) }
    }

    fn type_decl(&mut self, obj: Addr, type_id: i32) -> Vec<u8> {
        let (mut p, mut n) = (std::ptr::null(), 0usize);
        unsafe { (self.0.type_decl)(self.0.ctx, obj, type_id, &mut p, &mut n) };
        bytes(p, n).to_vec()
    }
}

/// `WriteScript` into `out` (handed to the write host's script callbacks); `obj` 0 = no script.
///
/// # Safety
/// `out` is the stream a write callback received; `host` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_write_script(
    out: *mut Out,
    host: *const OhplGs4ScriptHost,
    obj: usize,
) {
    // SAFETY: valid per the contract.
    let (Some(out), Some(host)) = (unsafe { out.as_mut() }, unsafe { host.as_ref() }) else {
        return;
    };
    script::write_script(&mut CScript(host), out, (obj != 0).then_some(obj));
}

/// Stream for tools and tests that drive `ohpl_gs4_write_script` directly.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs4_out_new() -> *mut Out {
    Box::into_raw(Box::new(Out::default()))
}

/// # Safety
/// `o` came from `ohpl_gs4_out_new`; the bytes are valid until `o` is freed or written to.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_out_bytes(o: *const Out, p: *mut *const u8, n: *mut usize) {
    // SAFETY: valid per the contract.
    let Some(o) = (unsafe { o.as_ref() }) else {
        return;
    };
    // SAFETY: out pointers valid per the contract.
    unsafe {
        *p = o.s.as_ptr();
        *n = o.s.len();
    }
}

/// # Safety
/// `o` came from `ohpl_gs4_out_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_out_free(o: *mut Out) {
    if !o.is_null() {
        // SAFETY: allocated by `ohpl_gs4_out_new`.
        drop(unsafe { Box::from_raw(o) });
    }
}

#[repr(C)]
pub struct OhplGs4WriteHost {
    pub ctx: Ctx,
    pub map_script: unsafe extern "C" fn(Ctx, *mut Out),
    pub entity_head: unsafe extern "C" fn(Ctx, usize, *mut EntityHead),
    pub entity_script: unsafe extern "C" fn(Ctx, usize, *mut Out),
    pub entity_tail: unsafe extern "C" fn(Ctx, usize, *mut EntityTail),
    pub other_key: unsafe extern "C" fn(Ctx, usize, *mut Str),
    pub other_timers: unsafe extern "C" fn(Ctx, usize, *mut Timers),
    pub other_script: unsafe extern "C" fn(Ctx, usize, *mut Out),
}

struct CWrite<'a>(&'a OhplGs4WriteHost);

// SAFETY (all calls): the table accepts its `ctx`; the records it fills stay valid until its next call.
impl WriteHost for CWrite<'_> {
    fn map_script(&mut self, o: &mut Out) {
        unsafe { (self.0.map_script)(self.0.ctx, o) }
    }

    fn entity_head(&mut self, i: usize) -> EntityHead {
        let mut e = std::mem::MaybeUninit::<EntityHead>::zeroed();
        unsafe {
            (self.0.entity_head)(self.0.ctx, i, e.as_mut_ptr());
            e.assume_init()
        }
    }

    fn entity_script(&mut self, i: usize, o: &mut Out) {
        unsafe { (self.0.entity_script)(self.0.ctx, i, o) }
    }

    fn entity_tail(&mut self, i: usize) -> EntityTail {
        let mut e = std::mem::MaybeUninit::<EntityTail>::zeroed();
        unsafe {
            (self.0.entity_tail)(self.0.ctx, i, e.as_mut_ptr());
            e.assume_init()
        }
    }

    fn other_key(&mut self, i: usize) -> Str {
        let mut s = Str::EMPTY;
        unsafe { (self.0.other_key)(self.0.ctx, i, &mut s) };
        s
    }

    fn other_timers(&mut self, i: usize) -> Timers {
        let mut t = Timers {
            timers: Slice::EMPTY,
            faders: Slice::EMPTY,
        };
        unsafe { (self.0.other_timers)(self.0.ctx, i, &mut t) };
        t
    }

    fn other_script(&mut self, i: usize, o: &mut Out) {
        unsafe { (self.0.other_script)(self.0.ctx, i, o) }
    }
}

#[repr(C)]
pub struct OhplGs4HeaderOut {
    pub map: Str,
    pub start_pos: Str,
    pub visited: Slice<Str>,
    pub vars: Str,
    pub preload: Str,
    pub played: Slice<Str>,
    pub music: Str,
    pub exploration: bool,
}

/// Whole save file: header, world body, music. The bytes go to `sink` once.
///
/// # Safety
/// All pointers are valid; the host table accepts its `ctx`; `sink` accepts `sctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_encode_save(
    h: *const OhplGs4HeaderOut,
    w: *const World,
    host: *const OhplGs4WriteHost,
    sink: Sink8,
    sctx: Ctx,
) {
    // SAFETY: valid per the contract.
    let (Some(h), Some(w), Some(host)) = (unsafe { h.as_ref() }, unsafe { w.as_ref() }, unsafe {
        host.as_ref()
    }) else {
        return;
    };
    let visited: Vec<&[u8]> = h.visited.get().iter().map(Str::get).collect();
    let played: Vec<&[u8]> = h.played.get().iter().map(Str::get).collect();
    let mut o = Out::default();
    header::write_header(
        &mut o,
        &HeaderOut {
            map: h.map.get(),
            start_pos: h.start_pos.get(),
            visited: &visited,
            vars: h.vars.get(),
            exploration: h.exploration,
            preload: h.preload.get(),
            played: &played,
        },
    );
    world::write_world(&mut o, w, &mut CWrite(host));
    o.str(h.music.get());
    push8(sink, sctx, &o.s);
}

pub struct OhplGs4Header(HeaderIn);

/// Status: 0 ok, 1 unreadable (logged by the caller), 2 rejected silently. `out` is set for 0 and 2
/// (2 keeps the fields parsed so far); free it with `ohpl_gs4_header_free`.
///
/// # Safety
/// `data` is `n` bytes; `out` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_header_read(
    data: *const u8,
    n: usize,
    file_open: bool,
    out: *mut *mut OhplGs4Header,
) -> i32 {
    let (st, h) = header::read_header(bytes(data, n), file_open);
    // SAFETY: `out` is valid.
    unsafe { *out = Box::into_raw(Box::new(OhplGs4Header(h))) };
    match st {
        LoadStatus::Ok => 0,
        LoadStatus::Unreadable => 1,
        LoadStatus::Rejected => 2,
    }
}

/// # Safety
/// `h` came from `ohpl_gs4_header_read`; `which`: 0 map, 1 start pos, 2 vars, 3 preload; or
/// 4 + i for visited[i], 0x10000 + i for played[i]. Returns false past the end.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_header_str(
    h: *const OhplGs4Header,
    which: usize,
    p: *mut *const u8,
    n: *mut usize,
) -> bool {
    // SAFETY: valid per the contract.
    let Some(h) = (unsafe { h.as_ref() }) else {
        return false;
    };
    let h = &h.0;
    let s = match which {
        0 => Some(&h.map),
        1 => Some(&h.start_pos),
        2 => Some(&h.vars),
        3 => Some(&h.preload),
        w if w >= 0x10000 => h.played.get(w - 0x10000),
        w => h.visited.get(w - 4),
    };
    let Some(s) = s else {
        return false;
    };
    // SAFETY: out pointers valid per the contract.
    unsafe {
        *p = s.as_ptr();
        *n = s.len();
    }
    true
}

#[repr(C)]
pub struct OhplGs4HeaderInfo {
    pub version: i32,
    pub body: usize,
    pub visited: usize,
    pub played: usize,
    pub exploration: bool,
}

/// # Safety
/// `h` came from `ohpl_gs4_header_read`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_header_info(h: *const OhplGs4Header) -> OhplGs4HeaderInfo {
    // SAFETY: valid per the contract.
    let h = unsafe { h.as_ref() }.map(|h| &h.0);
    OhplGs4HeaderInfo {
        version: h.map_or(0, |h| h.version),
        body: h.map_or(0, |h| h.body),
        visited: h.map_or(0, |h| h.visited.len()),
        played: h.map_or(0, |h| h.played.len()),
        exploration: h.is_some_and(|h| h.exploration),
    }
}

/// # Safety
/// `h` came from `ohpl_gs4_header_read` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_header_free(h: *mut OhplGs4Header) {
    if !h.is_null() {
        // SAFETY: allocated by `ohpl_gs4_header_read`.
        drop(unsafe { Box::from_raw(h) });
    }
}

pub struct OhplGs4In {
    data: Vec<u8>,
    p: usize,
    ok: bool,
}

/// Stream over a copy of the saved world body.
///
/// # Safety
/// `data` is `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_in_new(data: *const u8, n: usize) -> *mut OhplGs4In {
    Box::into_raw(Box::new(OhplGs4In {
        data: bytes(data, n).to_vec(),
        p: 0,
        ok: true,
    }))
}

/// # Safety
/// `i` came from `ohpl_gs4_in_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_in_ok(i: *const OhplGs4In) -> bool {
    // SAFETY: valid per the contract.
    unsafe { i.as_ref() }.is_some_and(|i| i.ok)
}

/// `cIn::Str` for the trailing music state.
///
/// # Safety
/// `i` came from `ohpl_gs4_in_new`; the string is valid until `i` is freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_in_str(i: *mut OhplGs4In, p: *mut *const u8, n: *mut usize) {
    // SAFETY: valid per the contract.
    let Some(i) = (unsafe { i.as_mut() }) else {
        return;
    };
    let mut r = In {
        s: &i.data,
        p: i.p,
        ok: i.ok,
    };
    let s = r.str();
    i.p = r.p;
    i.ok = r.ok;
    // SAFETY: out pointers valid per the contract.
    unsafe {
        *p = s.as_ptr();
        *n = s.len();
    }
}

/// # Safety
/// `i` came from `ohpl_gs4_in_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_in_free(i: *mut OhplGs4In) {
    if !i.is_null() {
        // SAFETY: allocated by `ohpl_gs4_in_new`.
        drop(unsafe { Box::from_raw(i) });
    }
}

#[repr(C)]
pub struct OhplGs4ReadHost {
    pub ctx: Ctx,
    pub script: *const OhplGs4ScriptHost,
    pub has_player: unsafe extern "C" fn(Ctx) -> bool,
    pub has_camera: unsafe extern "C" fn(Ctx) -> bool,
    pub map_timers_begin: unsafe extern "C" fn(Ctx),
    pub map_timer: unsafe extern "C" fn(Ctx, *const MapTimer),
    pub map_script: unsafe extern "C" fn(Ctx) -> usize,
    pub spawned: unsafe extern "C" fn(Ctx, *const Spawned),
    pub entity_begin: unsafe extern "C" fn(Ctx, Str) -> usize,
    pub entity_head: unsafe extern "C" fn(Ctx, usize, *const EntityHead),
    pub entity_script: unsafe extern "C" fn(Ctx, usize) -> usize,
    pub entity_tail: unsafe extern "C" fn(Ctx, usize, *const EntityHead, *const EntityTail),
    pub entities_done: unsafe extern "C" fn(Ctx, *const Str, usize),
    pub other_begin: unsafe extern "C" fn(Ctx, Str) -> usize,
    pub other_timers: unsafe extern "C" fn(Ctx, usize, *const Timers),
    pub other_reset: unsafe extern "C" fn(Ctx, usize),
    pub other_script: unsafe extern "C" fn(Ctx, usize) -> usize,
    pub light: unsafe extern "C" fn(Ctx, *const Light),
    pub particles_begin: unsafe extern "C" fn(Ctx),
    pub particle: unsafe extern "C" fn(Ctx, *const Particle),
    pub particles_end: unsafe extern "C" fn(Ctx, bool),
    pub sounds_begin: unsafe extern "C" fn(Ctx),
    pub sound: unsafe extern "C" fn(Ctx, *const Sound),
    pub sounds_end: unsafe extern "C" fn(Ctx, bool),
    pub billboard: unsafe extern "C" fn(Ctx, *const Billboard),
    pub player_apply: unsafe extern "C" fn(Ctx, *const Player),
    pub health: unsafe extern "C" fn(Ctx, f32),
    pub agent: unsafe extern "C" fn(Ctx, *const Agent),
    pub underwater: unsafe extern "C" fn(Ctx, bool),
    pub fog: unsafe extern "C" fn(Ctx, *const Fog),
    pub cam_texture: unsafe extern "C" fn(Ctx, *const CamTex),
    pub fog_area: unsafe extern "C" fn(Ctx, *const FogArea),
    pub colliding: unsafe extern "C" fn(Ctx, *const Colliding),
    pub display_name: unsafe extern "C" fn(Ctx, Str),
    pub camera: unsafe extern "C" fn(Ctx, *const Camera),
    pub owned_light: unsafe extern "C" fn(Ctx, *const OwnedLight),
    pub tone: unsafe extern "C" fn(Ctx, *const Tone),
}

struct CRead<'a> {
    t: &'a OhplGs4ReadHost,
    s: CScript<'a>,
}

fn obj(a: usize) -> Option<Addr> {
    (a != 0).then_some(a)
}

// SAFETY (all calls): the table accepts its `ctx`; records passed are live for the call.
impl ReadHost for CRead<'_> {
    fn script(&mut self) -> &mut dyn ScriptHost {
        &mut self.s
    }
    fn has_player(&mut self) -> bool {
        unsafe { (self.t.has_player)(self.t.ctx) }
    }
    fn has_camera(&mut self) -> bool {
        unsafe { (self.t.has_camera)(self.t.ctx) }
    }
    fn map_timers_begin(&mut self) {
        unsafe { (self.t.map_timers_begin)(self.t.ctx) }
    }
    fn map_timer(&mut self, t: &MapTimer) {
        unsafe { (self.t.map_timer)(self.t.ctx, t) }
    }
    fn map_script(&mut self) -> Option<Addr> {
        obj(unsafe { (self.t.map_script)(self.t.ctx) })
    }
    fn spawned(&mut self, s: &Spawned) {
        unsafe { (self.t.spawned)(self.t.ctx, s) }
    }
    fn entity_begin(&mut self, name: &[u8]) -> usize {
        unsafe { (self.t.entity_begin)(self.t.ctx, Str::new(name)) }
    }
    fn entity_head(&mut self, h: usize, e: &EntityHead) {
        unsafe { (self.t.entity_head)(self.t.ctx, h, e) }
    }
    fn entity_script(&mut self, h: usize) -> Option<Addr> {
        obj(unsafe { (self.t.entity_script)(self.t.ctx, h) })
    }
    fn entity_tail(&mut self, h: usize, e: &EntityHead, t: &EntityTail) {
        unsafe { (self.t.entity_tail)(self.t.ctx, h, e, t) }
    }
    fn entities_done(&mut self, saved: &[Str]) {
        unsafe { (self.t.entities_done)(self.t.ctx, saved.as_ptr(), saved.len()) }
    }
    fn other_begin(&mut self, key: &[u8]) -> usize {
        unsafe { (self.t.other_begin)(self.t.ctx, Str::new(key)) }
    }
    fn other_timers(&mut self, h: usize, t: &Timers) {
        unsafe { (self.t.other_timers)(self.t.ctx, h, t) }
    }
    fn other_reset(&mut self, h: usize) {
        unsafe { (self.t.other_reset)(self.t.ctx, h) }
    }
    fn other_script(&mut self, h: usize) -> Option<Addr> {
        obj(unsafe { (self.t.other_script)(self.t.ctx, h) })
    }
    fn light(&mut self, l: &Light) {
        unsafe { (self.t.light)(self.t.ctx, l) }
    }
    fn particles_begin(&mut self) {
        unsafe { (self.t.particles_begin)(self.t.ctx) }
    }
    fn particle(&mut self, p: &Particle) {
        unsafe { (self.t.particle)(self.t.ctx, p) }
    }
    fn particles_end(&mut self, has_list: bool) {
        unsafe { (self.t.particles_end)(self.t.ctx, has_list) }
    }
    fn sounds_begin(&mut self) {
        unsafe { (self.t.sounds_begin)(self.t.ctx) }
    }
    fn sound(&mut self, s: &Sound) {
        unsafe { (self.t.sound)(self.t.ctx, s) }
    }
    fn sounds_end(&mut self, has_list: bool) {
        unsafe { (self.t.sounds_end)(self.t.ctx, has_list) }
    }
    fn billboard(&mut self, b: &Billboard) {
        unsafe { (self.t.billboard)(self.t.ctx, b) }
    }
    fn player_apply(&mut self, p: &Player) {
        unsafe { (self.t.player_apply)(self.t.ctx, p) }
    }
    fn health(&mut self, h: f32) {
        unsafe { (self.t.health)(self.t.ctx, h) }
    }
    fn agent(&mut self, a: &Agent) {
        unsafe { (self.t.agent)(self.t.ctx, a) }
    }
    fn underwater(&mut self, b: bool) {
        unsafe { (self.t.underwater)(self.t.ctx, b) }
    }
    fn fog(&mut self, f: &Fog) {
        unsafe { (self.t.fog)(self.t.ctx, f) }
    }
    fn cam_texture(&mut self, c: &CamTex) {
        unsafe { (self.t.cam_texture)(self.t.ctx, c) }
    }
    fn fog_area(&mut self, f: &FogArea) {
        unsafe { (self.t.fog_area)(self.t.ctx, f) }
    }
    fn colliding(&mut self, c: &Colliding) {
        unsafe { (self.t.colliding)(self.t.ctx, c) }
    }
    fn display_name(&mut self, n: &[u8]) {
        unsafe { (self.t.display_name)(self.t.ctx, Str::new(n)) }
    }
    fn camera(&mut self, c: &Camera) {
        unsafe { (self.t.camera)(self.t.ctx, c) }
    }
    fn owned_light(&mut self, l: &OwnedLight) {
        unsafe { (self.t.owned_light)(self.t.ctx, l) }
    }
    fn tone(&mut self, t: &Tone) {
        unsafe { (self.t.tone)(self.t.ctx, t) }
    }
}

/// `cSomaSaveState::ReadWorld` over the stream, applying through `host`.
///
/// # Safety
/// `i` came from `ohpl_gs4_in_new`; `host` and its script table are valid and accept their `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_read_world(
    i: *mut OhplGs4In,
    version: i32,
    host: *const OhplGs4ReadHost,
) {
    // SAFETY: valid per the contract.
    let (Some(i), Some(host)) = (unsafe { i.as_mut() }, unsafe { host.as_ref() }) else {
        return;
    };
    // SAFETY: the script table is valid per the contract.
    let Some(script) = (unsafe { host.script.as_ref() }) else {
        return;
    };
    let mut r = In {
        s: &i.data,
        p: i.p,
        ok: i.ok,
    };
    world::read_world(
        &mut r,
        version,
        &mut CRead {
            t: host,
            s: CScript(script),
        },
    );
    i.p = r.p;
    i.ok = r.ok;
}

/// `ReadScript` / `WriteScript` helpers for callers outside the world (none today).
///
/// # Safety
/// `host` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs4_read_script(
    i: *mut OhplGs4In,
    version: i32,
    host: *const OhplGs4ScriptHost,
    obj: usize,
) {
    // SAFETY: valid per the contract.
    let (Some(i), Some(host)) = (unsafe { i.as_mut() }, unsafe { host.as_ref() }) else {
        return;
    };
    let mut r = In {
        s: &i.data,
        p: i.p,
        ok: i.ok,
    };
    script::read_script(
        &mut CScript(host),
        &mut r,
        version,
        (obj != 0).then_some(obj),
    );
    i.p = r.p;
    i.ok = r.ok;
}
