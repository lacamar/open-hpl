//! C ABI of the launcher port, mirrored in `amnesia/src/launcher/RustLauncher.h`.

use super::flow::{self, PathOp};
use super::modes::{self, Custom, Device, Label, Mode};
use super::preset;
use super::quality::Chooser;
use crate::luxbase::cfg::Cfg;
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, i32, *const u8, usize);

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn items<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live array of `n` items.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn emit(sink: Option<Sink>, ctx: Ctx, tag: i32, b: &[u8]) {
    if let Some(s) = sink {
        // SAFETY: the sink contract is documented in the header; the buffer is live.
        unsafe { s(ctx, tag, b.as_ptr(), b.len()) }
    }
}

#[repr(C)]
pub struct Str {
    p: *const u8,
    n: usize,
}

#[repr(C)]
pub struct CfgOps {
    ctx: Ctx,
    get_bool: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, i32) -> i32,
    get_int: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, i32) -> i32,
    get_float: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, f32) -> f32,
    get_string:
        unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, *const u8, usize, Sink, Ctx),
    set_bool: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, i32),
    set_int: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, i32),
    set_float: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, f32),
    set_string: unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize, *const u8, usize),
}

struct Host<'a>(&'a CfgOps);

unsafe extern "C" fn collect(ctx: Ctx, _tag: i32, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec<u8>` passed by `Host::string`.
    let v = unsafe { &mut *ctx.cast::<Vec<u8>>() };
    *v = bytes(p, n).to_vec();
}

impl Cfg for Host<'_> {
    fn bool(&mut self, l: &[u8], k: &[u8], d: bool) -> bool {
        let o = self.0;
        // SAFETY: the vtable is valid for the call (header contract).
        unsafe {
            (o.get_bool)(
                o.ctx,
                l.as_ptr(),
                l.len(),
                k.as_ptr(),
                k.len(),
                i32::from(d),
            ) != 0
        }
    }
    fn int(&mut self, l: &[u8], k: &[u8], d: i32) -> i32 {
        let o = self.0;
        // SAFETY: as above.
        unsafe { (o.get_int)(o.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), d) }
    }
    fn float(&mut self, l: &[u8], k: &[u8], d: f32) -> f32 {
        let o = self.0;
        // SAFETY: as above.
        unsafe { (o.get_float)(o.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), d) }
    }
    fn string(&mut self, l: &[u8], k: &[u8], d: &[u8]) -> Vec<u8> {
        let o = self.0;
        let mut out = d.to_vec();
        // SAFETY: as above; `collect` writes the `Vec` behind the pointer.
        unsafe {
            (o.get_string)(
                o.ctx,
                l.as_ptr(),
                l.len(),
                k.as_ptr(),
                k.len(),
                d.as_ptr(),
                d.len(),
                collect,
                (&raw mut out).cast(),
            );
        }
        out
    }
    fn string_w(&mut self, _l: &[u8], _k: &[u8], d: &[u32]) -> Vec<u32> {
        d.to_vec()
    }
    fn set_bool(&mut self, l: &[u8], k: &[u8], v: bool) {
        let o = self.0;
        // SAFETY: as above.
        unsafe {
            (o.set_bool)(
                o.ctx,
                l.as_ptr(),
                l.len(),
                k.as_ptr(),
                k.len(),
                i32::from(v),
            )
        }
    }
    fn set_int(&mut self, l: &[u8], k: &[u8], v: i32) {
        let o = self.0;
        // SAFETY: as above.
        unsafe { (o.set_int)(o.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), v) }
    }
    fn set_float(&mut self, l: &[u8], k: &[u8], v: f32) {
        let o = self.0;
        // SAFETY: as above.
        unsafe { (o.set_float)(o.ctx, l.as_ptr(), l.len(), k.as_ptr(), k.len(), v) }
    }
    fn set_string(&mut self, l: &[u8], k: &[u8], v: &[u8]) {
        let o = self.0;
        // SAFETY: as above.
        unsafe {
            (o.set_string)(
                o.ctx,
                l.as_ptr(),
                l.len(),
                k.as_ptr(),
                k.len(),
                v.as_ptr(),
                v.len(),
            )
        }
    }
}

fn host<'a>(p: *const CfgOps) -> Option<Host<'a>> {
    // SAFETY: null or a vtable that outlives the call.
    unsafe { p.as_ref() }.map(Host)
}

// quality chooser

/// Loads the card database; null when the file is missing or malformed.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_qc_load(path: *const u8, len: usize) -> *mut Chooser {
    Chooser::load(bytes(path, len)).map_or(std::ptr::null_mut(), |c| Box::into_raw(Box::new(c)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_qc_free(c: *mut Chooser) {
    if !c.is_null() {
        // SAFETY: allocated by `ohpl_launcher_qc_load` and freed once.
        drop(unsafe { Box::from_raw(c) });
    }
}

/// Rating of `card`; the closest-series text goes to `sink` (tag 0) when inexact.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_qc_rate(
    c: *const Chooser,
    card: *const u8,
    len: usize,
    sink: Option<Sink>,
    ctx: Ctx,
) -> i32 {
    // SAFETY: null or a live chooser.
    let Some(c) = (unsafe { c.as_ref() }) else {
        return super::quality::UNKNOWN;
    };
    let (rating, msg) = c.rate(bytes(card, len));
    if !msg.is_empty() {
        emit(sink, ctx, 0, &msg);
    }
    rating
}

// presets

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_equals_preset(file: *const CfgOps, preset: *const CfgOps) -> i32 {
    match (host(file), host(preset)) {
        (Some(mut f), Some(mut p)) => i32::from(preset::equals_preset(&mut f, &mut p)),
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_equal_preset_index(
    file: *const CfgOps,
    presets: *const *const CfgOps,
    n: usize,
) -> i32 {
    let mut hosts: Vec<Option<Host>> = items(presets, n).iter().map(|&p| host(p)).collect();
    let mut refs: Vec<Option<&mut dyn Cfg>> = hosts
        .iter_mut()
        .map(|h| h.as_mut().map(|h| h as &mut dyn Cfg))
        .collect();
    match host(file) {
        Some(mut f) => preset::equal_index(Some(&mut f), &mut refs),
        None => -1,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_apply_preset(file: *const CfgOps, preset: *const CfgOps) {
    if let (Some(mut f), Some(mut p)) = (host(file), host(preset)) {
        preset::apply(&mut f, &mut p);
    }
}

#[repr(C)]
pub struct Detect {
    show_closest: i32,
    show_unknown: i32,
    show_unsupported: i32,
    preset: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_detect(rating: i32, closest_nonempty: i32, out: *mut Detect) {
    let d = preset::detect(rating, closest_nonempty != 0);
    // SAFETY: null or a live struct.
    if let Some(o) = unsafe { out.as_mut() } {
        *o = Detect {
            show_closest: i32::from(d.show_closest),
            show_unknown: i32::from(d.show_unknown),
            show_unsupported: i32::from(d.show_unsupported),
            preset: d.preset.map_or(-1, |p| p as i32),
        };
    }
}

#[repr(C)]
pub struct Initial {
    fullscreen: i32,
    shadow_quality: i32,
    shadow_resolution: i32,
    reflection: i32,
    ssao: i32,
    ssao_resolution: i32,
    ssao_samples: i32,
    texture_quality: i32,
    texture_filter: i32,
    anisotropy: i32,
    edge_smooth: i32,
    parallax: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_initial(cfg: *const CfgOps, out: *mut Initial) {
    let (Some(mut c), Some(o)) = (host(cfg), unsafe { out.as_mut() }) else {
        return;
    };
    let i = preset::initial(&mut c);
    *o = Initial {
        fullscreen: i.fullscreen,
        shadow_quality: i.shadow_quality,
        shadow_resolution: i.shadow_resolution,
        reflection: i.reflection,
        ssao: i.ssao,
        ssao_resolution: i.ssao_resolution,
        ssao_samples: i.ssao_samples,
        texture_quality: i.texture_quality,
        texture_filter: i.texture_filter,
        anisotropy: i.anisotropy,
        edge_smooth: i.edge_smooth,
        parallax: i.parallax,
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_menu_index(values: *const i32, n: usize, wanted: i32) -> i32 {
    preset::menu_index(items(values, n), wanted)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_texture_quality_row(menu_size: i32, quality: i32) -> i32 {
    preset::texture_quality_row(menu_size, quality)
}

// flow

/// Tag 0: init file (`config/...`), tag 1: the default init file.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_init_cfg(
    params: *const u8,
    len: usize,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    emit(sink, ctx, 0, &flow::init_cfg_name(bytes(params, len)));
    emit(sink, ctx, 1, flow::DEFAULT_INIT);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_user_resource_dir(
    p: *const u8,
    n: usize,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    emit(sink, ctx, 0, &flow::user_resource_dir(bytes(p, n)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_init_cfg_path(
    user_dir: *const u8,
    user_len: usize,
    init: *const u8,
    init_len: usize,
    user_exists: i32,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    emit(
        sink,
        ctx,
        0,
        &flow::init_cfg_path(
            bytes(user_dir, user_len),
            bytes(init, init_len),
            user_exists != 0,
        ),
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_message_init_load(
    p: *const u8,
    n: usize,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    emit(sink, ctx, 0, &flow::message_init_load(bytes(p, n)));
}

pub const T_MESSAGE: i32 = 100;

type LoadDefaults = unsafe extern "C" fn(Ctx) -> *const CfgOps;

/// Tags 1..=10: save folder, default main, low, medium, high, default base language, base
/// language folder, game language folder, custom story path, game name. Tag 100: a message to
/// show. Returns 1 when the launcher must quit.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_read_init(
    init: *const CfgOps,
    init_file: *const u8,
    init_len: usize,
    default_file: *const u8,
    default_len: usize,
    load_defaults: Option<LoadDefaults>,
    load_ctx: Ctx,
    sink: Option<Sink>,
    ctx: Ctx,
) -> i32 {
    let Some(mut init) = host(init) else {
        return 1;
    };
    let mut loader = || {
        let ops = load_defaults.map(|f| {
            // SAFETY: the loader contract is documented in the header.
            unsafe { f(load_ctx) }
        })?;
        let mut h = host(ops)?;
        Some(h.string(b"ConfigFiles", b"DefaultMainSettingsSDL2", b""))
    };
    let r = flow::read_init(
        &mut init,
        bytes(init_file, init_len),
        bytes(default_file, default_len),
        &mut loader,
    );
    for m in &r.messages {
        emit(sink, ctx, T_MESSAGE, m);
    }
    let v = &r.vars;
    let fields: [&[u8]; 10] = [
        &v.save_folder,
        &v.default_main,
        &v.low,
        &v.medium,
        &v.high,
        &v.default_base_language,
        &v.base_language_folder,
        &v.game_language_folder,
        &v.custom_stories,
        &v.game_name,
    ];
    for (i, f) in fields.iter().enumerate() {
        emit(sink, ctx, i as i32 + 1, f);
    }
    i32::from(r.failed)
}

/// Executed as emitted. Tag 10: create below the personal dir if missing; 11: mkdir; 12 then 13:
/// move-if-missing (source, target); 20: base save path; 21: base config path; 22: crash flag;
/// 23: log file.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_plan_paths(
    personal: Str,
    config_home: Str,
    state_home: Str,
    save_folder: Str,
    custom: Str,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    let plan = flow::plan_paths(
        bytes(personal.p, personal.n),
        bytes(config_home.p, config_home.n),
        bytes(state_home.p, state_home.n),
        bytes(save_folder.p, save_folder.n),
        bytes(custom.p, custom.n),
    );
    emit(sink, ctx, 20, &plan.base_save);
    emit(sink, ctx, 21, &plan.base_config);
    emit(sink, ctx, 22, &plan.crash_flag);
    emit(sink, ctx, 23, &plan.log_file);
    for op in &plan.ops {
        match op {
            PathOp::CreateIfMissing(d) => emit(sink, ctx, 10, d),
            PathOp::Mkdir(d) => emit(sink, ctx, 11, d),
            PathOp::MoveIfMissing(a, b) => {
                emit(sink, ctx, 12, a);
                emit(sink, ctx, 13, b);
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_settings_path(
    p: *const u8,
    n: usize,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    emit(sink, ctx, 0, &flow::settings_path(bytes(p, n)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_use_wanted_config(force_default: i32, wanted_exists: i32) -> i32 {
    i32::from(flow::use_wanted_config(
        force_default != 0,
        wanted_exists != 0,
    ))
}

/// 1 and a message on `sink` when the failed load deserves one.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_config_load_message(
    loaded_wanted: i32,
    default_empty: i32,
    sink: Option<Sink>,
    ctx: Ctx,
) -> i32 {
    flow::config_load_message(loaded_wanted != 0, default_empty != 0).map_or(0, |m| {
        emit(sink, ctx, 0, m);
        1
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_sound_device(cfg: *const CfgOps) -> i32 {
    host(cfg).map_or(-1, |mut c| flow::sound_device(&mut c))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_show_launcher(cfg: *const CfgOps) -> i32 {
    host(cfg).map_or(1, |mut c| i32::from(flow::show_launcher(&mut c)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_start_language(cfg: *const CfgOps, sink: Option<Sink>, ctx: Ctx) {
    let v = host(cfg).map_or_else(
        || b"english.lang".to_vec(),
        |mut c| flow::start_language(&mut c),
    );
    emit(sink, ctx, 0, &v);
}

/// Tag 1: add a required file (no user dir); tag 2: add a fallback file (user dir first).
/// Translations are cleared before the first.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_language_plan(
    name: Str,
    base_folder: Str,
    game_folder: Str,
    default_base: Str,
    default_game: Str,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    let ops = flow::language_plan(
        bytes(name.p, name.n),
        bytes(base_folder.p, base_folder.n),
        bytes(game_folder.p, game_folder.n),
        bytes(default_base.p, default_base.n),
        bytes(default_game.p, default_game.n),
    );
    for op in &ops {
        emit(sink, ctx, if op.forced { 1 } else { 2 }, &op.file);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_window_class(sink: Option<Sink>, ctx: Ctx) {
    emit(sink, ctx, 0, flow::window_class());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_game_exe(dir: *const u8, n: usize, sink: Option<Sink>, ctx: Ctx) {
    emit(sink, ctx, 0, &flow::game_exe(bytes(dir, n)));
}

/// Returns 1 to start the game with the arguments on `sink`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_run_plan(
    exitflag: i32,
    params: *const u8,
    len: usize,
    sink: Option<Sink>,
    ctx: Ctx,
) -> i32 {
    let (run, args) = flow::run_plan(exitflag, bytes(params, len));
    emit(sink, ctx, 0, &args);
    i32::from(run)
}

// resolutions, languages, sound devices

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CMode {
    display: i32,
    width: i32,
    height: i32,
}

fn modes_of(p: *const CMode, n: usize) -> Vec<Mode> {
    items(p, n)
        .iter()
        .map(|m| Mode {
            display: m.display,
            width: m.width,
            height: m.height,
        })
        .collect()
}

/// Returns 1 when the configured custom size must be appended to the mode list.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_fix_config_size(
    cfg: *const CfgOps,
    modes: *const CMode,
    n: usize,
) -> i32 {
    host(cfg).map_or(0, |mut c| {
        i32::from(modes::fix_config_size(&mut c, &modes_of(modes, n)))
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_configured_mode(cfg: *const CfgOps, out: *mut CMode) {
    if let (Some(mut c), Some(o)) = (host(cfg), unsafe { out.as_mut() }) {
        let m = modes::configured(&mut c);
        *o = CMode {
            display: m.display,
            width: m.width,
            height: m.height,
        };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_last_match(modes: *const CMode, n: usize, cur: CMode) -> i32 {
    modes::last_match(
        &modes_of(modes, n),
        Mode {
            display: cur.display,
            width: cur.width,
            height: cur.height,
        },
    )
}

/// 0: rejected, 1: select row `*row`, 2: add the size as a custom mode.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_custom_resolution(
    modes: *const CMode,
    n: usize,
    w: i32,
    h: i32,
    row: *mut i32,
) -> i32 {
    match modes::custom_resolution(&modes_of(modes, n), w, h) {
        Custom::Reject => 0,
        Custom::Select(i) => {
            // SAFETY: null or a live int.
            if let Some(r) = unsafe { row.as_mut() } {
                *r = i;
            }
            1
        }
        Custom::Add => 2,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_populate_selection(last_match: i32, w: i32, h: i32) -> i32 {
    modes::populate_selection(last_match, w, h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_multi_display(modes: *const CMode, n: usize) -> i32 {
    i32::from(modes::multi_display(&modes_of(modes, n)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_aspect_ratio(w: i32, h: i32, out: *mut i32) {
    let (a, b) = modes::aspect_ratio(w, h);
    // SAFETY: null or two live ints.
    if let Some(o) = unsafe { out.cast::<[i32; 2]>().as_mut() } {
        *o = [a, b];
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_mode_label(
    fullscreen_desktop: i32,
    fullscreen_desktop_text: Str,
    display_name: Str,
    w: i32,
    h: i32,
    custom: i32,
    custom_text: Str,
    multi_display: i32,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    let text = modes::label(&Label {
        fullscreen_desktop: fullscreen_desktop != 0,
        fullscreen_desktop_text: bytes(fullscreen_desktop_text.p, fullscreen_desktop_text.n),
        display_name: bytes(display_name.p, display_name.n),
        width: w,
        height: h,
        custom: custom != 0,
        custom_text: bytes(custom_text.p, custom_text.n),
        multi_display: multi_display != 0,
    });
    emit(sink, ctx, 0, &text);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_language_name(
    p: *const u8,
    n: usize,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    emit(sink, ctx, 0, &modes::language_name(bytes(p, n)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_language_index(
    cfg: *const CfgOps,
    files: *const Str,
    n: usize,
    default: Str,
) -> i32 {
    let default = bytes(default.p, default.n);
    let current = host(cfg).map_or_else(
        || default.to_vec(),
        |mut c| modes::current_language(&mut c, default),
    );
    let files: Vec<&[u8]> = items(files, n).iter().map(|f| bytes(f.p, f.n)).collect();
    modes::language_index(&files, default, &current)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_languages_dir(
    folder: Str,
    user_dir: Str,
    user_exists: i32,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    emit(
        sink,
        ctx,
        0,
        &modes::languages_dir(
            bytes(folder.p, folder.n),
            bytes(user_dir.p, user_dir.n),
            user_exists != 0,
        ),
    );
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CDevice {
    is_default: i32,
    is_current: i32,
}

/// Row to select; `*append_unsupported` is 1 when the current device must be listed as
/// unsupported after the others.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_sound_selection(
    cfg: *const CfgOps,
    devs: *const CDevice,
    n: usize,
    append_unsupported: *mut i32,
) -> i32 {
    let hint = host(cfg).map_or(-1, |mut c| modes::sound_hint(&mut c));
    let devs: Vec<Device> = items(devs, n)
        .iter()
        .map(|d| Device {
            is_default: d.is_default != 0,
            is_current: d.is_current != 0,
        })
        .collect();
    let (row, append) = modes::sound_selection(hint, &devs);
    // SAFETY: null or a live int.
    if let Some(a) = unsafe { append_unsupported.as_mut() } {
        *a = i32::from(append);
    }
    row
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_launcher_escape_device(
    p: *const u8,
    n: usize,
    sink: Option<Sink>,
    ctx: Ctx,
) {
    emit(sink, ctx, 0, &modes::escape_device(bytes(p, n)));
}
