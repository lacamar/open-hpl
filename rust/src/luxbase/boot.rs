//! `cLuxBase` boot and shutdown order: `Init`, `InitEngine`, `InitGame`, `Exit`. Rust owns the
//! order of every step, every config read and the module / loader / container tables; the engine
//! calls (statics, engine creation, module construction) come back as host ops (`bop`).

use super::base::{self, StartMode};
use super::cfg::Cfg;
use super::ffi::{CfgHost, Host, MainNums, af, ai, astr};
use std::ffi::c_void;

pub mod bop {
    pub const INIT_BEGIN: u32 = 200;
    pub const STEP_PARSE: u32 = 201;
    pub const STEP_APP: u32 = 202;
    pub const STEP_MAIN_CONFIG: u32 = 203;
    pub const LOG_VERSION: u32 = 204;
    pub const SERIAL: u32 = 205;
    pub const LOG_NEWLINE: u32 = 206;
    pub const STEP_ENGINE: u32 = 207;
    pub const STEP_FEATURE: u32 = 208;
    pub const STEP_GAME: u32 = 209;
    pub const HEADLESS: u32 = 210;
    pub const FLAGS: u32 = 211;
    pub const START_PRE: u32 = 212;
    pub const START_MENU: u32 = 213;
    pub const START_DIRECT: u32 = 214;
    pub const INIT_USER_CFG: u32 = 215;
    pub const LOCK_INPUT: u32 = 216;
    pub const RELATIVE_MOUSE: u32 = 217;
    pub const START_GAME: u32 = 218;
    pub const INIT_OVER: u32 = 219;
    pub const EXIT_BEGIN: u32 = 220;
    pub const SAVE_CONFIG: u32 = 221;
    pub const EXIT_GAME: u32 = 222;
    pub const MODULE_COUNT: u32 = 223;
    pub const DELETE_MODULE: u32 = 224;
    pub const EXIT_CONFIG: u32 = 225;
    pub const EXIT_END: u32 = 226;

    pub const E_VARS: u32 = 240;
    pub const E_SOUND: u32 = 241;
    pub const E_STATIC_A: u32 = 242;
    pub const E_STATIC_SSAO: u32 = 243;
    pub const E_GBUFFER: u32 = 244;
    pub const E_STATIC_MISC: u32 = 245;
    pub const E_CREATE: u32 = 246;
    pub const E_VSYNC: u32 = 247;
    pub const E_GAMMA: u32 = 248;
    pub const E_LIMIT_FPS: u32 = 249;
    pub const E_WAIT_FOCUS: u32 = 250;
    pub const E_MATERIAL: u32 = 251;
    pub const E_VOLUME: u32 = 252;
    pub const E_RESOURCES: u32 = 253;
    pub const E_SURFACE: u32 = 254;
    pub const E_LANGUAGE_BEGIN: u32 = 255;
    pub const E_LANGUAGE: u32 = 256;
    pub const E_FASTLOAD: u32 = 257;
    pub const E_SOUND_DEVICE: u32 = 258;
    pub const Q_DEFAULT_LANG: u32 = 259;
    pub const Q_GAME_NAME: u32 = 260;
    pub const Q_CURRENT_LANG: u32 = 261;
    pub const Q_RENDERER: u32 = 262;
    pub const Q_COLOR: u32 = 263;

    pub const G_HUD: u32 = 270;
    pub const G_FONT: u32 = 271;
    pub const G_CONTAINERS: u32 = 272;
    pub const G_MODULE: u32 = 273;
    pub const G_ACHIEVEMENTS: u32 = 274;
    pub const G_ACHIEVEMENT: u32 = 275;
    pub const G_ACHIEVEMENTS_END: u32 = 276;
    pub const G_RESET: u32 = 277;
    pub const G_LOADER: u32 = 278;
    pub const G_MESSAGE: u32 = 279;
}

pub const FLAG_PRE_MENU: i64 = 1;
pub const FLAG_MENU: i64 = 2;
pub const FLAG_DEBUG_WINDOW: i64 = 4;
pub const FLAG_FULLSCREEN: i64 = 8;
pub const FLAG_SAVE_AT_EXIT: i64 = 16;

pub const CONTAINERS: [&[u8]; 6] = [
    b"PreMenu",
    b"MainMenu",
    b"Inventory",
    b"Journal",
    b"Credits",
    b"LoadScreen",
];

const GLOBAL: &[u8] = b"";
const DEFAULT: &[u8] = b"Default";

/// Modules in creation order: (module id of the host switch, updater container; empty = global).
pub const MODULES_BEFORE_ACHIEVEMENTS: [(i64, &[u8]); 19] = [
    (0, GLOBAL),
    (1, GLOBAL),
    (2, GLOBAL),
    (3, GLOBAL),
    (4, GLOBAL),
    (5, DEFAULT),
    (6, DEFAULT),
    (7, DEFAULT),
    (8, DEFAULT),
    (9, DEFAULT),
    (10, DEFAULT),
    (11, DEFAULT),
    (12, DEFAULT),
    (13, DEFAULT),
    (14, DEFAULT),
    (15, DEFAULT),
    (16, DEFAULT),
    (17, DEFAULT),
    (18, DEFAULT),
];

pub const MODULES_AFTER_ACHIEVEMENTS: [(i64, &[u8]); 7] = [
    (19, b"PreMenu"),
    (20, b"MainMenu"),
    (21, b"Inventory"),
    (22, b"Journal"),
    (23, b"Credits"),
    (24, b"DemoEnd"),
    (25, b"LoadScreen"),
];

pub const ENTITY_LOADERS: [&[u8]; 22] = [
    b"StaticProp",
    b"Object",
    b"SwingDoor",
    b"Lever",
    b"Wheel",
    b"Lamp",
    b"Photocell",
    b"MoveObject",
    b"Item",
    b"Chest",
    b"LevelDoor",
    b"Button",
    b"OilBarrel",
    b"EmotionStone",
    b"NPC",
    b"MultiSlider",
    b"CritterBug",
    b"CritterSpider",
    b"Enemy_Grunt",
    b"Enemy_WaterLurker",
    b"Enemy_ManPig",
    b"CommentaryIcon",
];

pub const AREA_LOADERS: [&[u8]; 13] = [
    b"PlayerStart",
    b"PathNode",
    b"PosNode",
    b"Script",
    b"Flashback",
    b"Ladder",
    b"Liquid",
    b"Sticky",
    b"Insanity",
    b"Examine",
    b"Sign",
    b"SlimeDamage",
    b"Rope",
];

pub const MSG_LOAD_FONTS: i64 = 0;
pub const MSG_LOAD_MAIN_CONFIG: i64 = 1;

/// `eDeferredGBuffer_32Bit`.
const GBUFFER_32BIT: i32 = 0;

fn b(v: bool) -> i64 {
    i64::from(v)
}

/// `Init`.
pub(super) fn init(h: Host) -> bool {
    h.go(bop::INIT_BEGIN, &[]);
    for step in [bop::STEP_PARSE, bop::STEP_APP, bop::STEP_MAIN_CONFIG] {
        if !h.b(step, &[]) {
            return false;
        }
    }
    h.go(bop::LOG_VERSION, &[]);
    if !h.b(bop::SERIAL, &[]) {
        return false;
    }
    h.go(bop::LOG_NEWLINE, &[]);
    for step in [bop::STEP_ENGINE, bop::STEP_FEATURE, bop::STEP_GAME] {
        if !h.b(step, &[]) {
            return false;
        }
    }
    h.go(bop::HEADLESS, &[]);
    let flags = h.i(bop::FLAGS, &[]);
    match base::start_mode(flags & FLAG_PRE_MENU != 0, flags & FLAG_MENU != 0) {
        StartMode::PreMenu => {
            h.go(bop::START_PRE, &[]);
        }
        StartMode::Menu => {
            h.go(bop::START_MENU, &[]);
        }
        StartMode::Direct => {
            h.go(bop::START_DIRECT, &[]);
            if !h.b(bop::INIT_USER_CFG, &[]) {
                return false;
            }
            if let Some(lock) = base::direct_start_input(
                flags & FLAG_DEBUG_WINDOW != 0,
                flags & FLAG_FULLSCREEN != 0,
            ) {
                if lock {
                    h.go(bop::LOCK_INPUT, &[]);
                }
                h.go(bop::RELATIVE_MOUSE, &[]);
            }
            h.go(bop::START_GAME, &[]);
        }
    }
    h.go(bop::INIT_OVER, &[]);
    true
}

/// `InitEngine`; `n` are the config handler's numbers.
pub(super) fn init_engine(h: Host, n: &MainNums) -> bool {
    let mut main = CfgHost { h, base: 0 };
    let name = h.go(bop::Q_GAME_NAME, &[]).bytes;
    let renderer = h.go(bop::Q_RENDERER, &[]).bytes;
    let color = h.go(bop::Q_COLOR, &[]).bytes;
    h.go(
        bop::E_VARS,
        &[
            ai(i64::from(n.screen_width)),
            ai(i64::from(n.screen_height)),
            ai(i64::from(n.display)),
            ai(b(n.fullscreen)),
            af(n.paper_white),
            af(n.peak_nits),
            astr(&renderer),
            astr(&color),
            astr(&name),
        ],
    );
    h.go(
        bop::E_SOUND,
        &[
            ai(i64::from(n.sound_dev_id)),
            ai(i64::from(n.max_sound_channels)),
            ai(i64::from(n.sound_stream_buffers)),
            ai(i64::from(n.sound_stream_buffer_size)),
        ],
    );
    h.go(
        bop::E_STATIC_A,
        &[
            ai(i64::from(n.shadow_quality)),
            ai(i64::from(n.shadow_res)),
            ai(i64::from(n.parallax_quality)),
            ai(b(n.parallax_enabled)),
            ai(i64::from(n.gui_scale)),
            ai(b(n.refraction)),
        ],
    );
    h.go(
        bop::E_STATIC_SSAO,
        &[
            ai(if n.ssao_resolution == 0 { 2 } else { 1 }),
            ai(i64::from(n.ssao_samples)),
            ai(b(n.ssao_active)),
        ],
    );
    let gbuffer = main.int(b"Graphics", b"GBufferType", GBUFFER_32BIT);
    let textures = main.int(b"Graphics", b"NumOfGBufferTextures", 3);
    h.go(
        bop::E_GBUFFER,
        &[ai(i64::from(gbuffer)), ai(i64::from(textures))],
    );
    h.go(
        bop::E_STATIC_MISC,
        &[
            ai(b(n.edge_smooth)),
            ai(b(n.occlusion_test_lights)),
            ai(b(n.force_sm34_off)),
            ai(b(n.force_cache)),
        ],
    );
    h.go(bop::E_CREATE, &[]);
    h.go(bop::E_VSYNC, &[ai(b(n.vsync)), ai(b(n.adaptive_vsync))]);
    let gamma = main.float(b"Graphics", b"Gamma", 1.0);
    h.go(bop::E_GAMMA, &[af(gamma)]);
    let limit = main.bool(b"Engine", b"LimitFPS", false);
    h.go(bop::E_LIMIT_FPS, &[ai(b(limit))]);
    let wait = main.bool(b"Engine", b"SleepWhenOutOfFocus", true);
    h.go(bop::E_WAIT_FOCUS, &[ai(b(wait))]);
    h.go(
        bop::E_MATERIAL,
        &[
            ai(i64::from(n.texture_quality)),
            ai(i64::from(n.texture_filter)),
            af(n.texture_anisotropy),
        ],
    );
    let volume = main.float(b"Sound", b"Volume", 1.0);
    h.go(bop::E_VOLUME, &[af(volume)]);
    h.go(bop::E_RESOURCES, &[]);
    h.go(bop::E_SURFACE, &[]);
    h.go(bop::E_LANGUAGE_BEGIN, &[]);
    let default_lang = h.go(bop::Q_DEFAULT_LANG, &[]).bytes;
    let lang = main.string(b"Main", b"StartLanguage", &default_lang);
    h.go(bop::E_LANGUAGE, &[astr(&lang)]);
    h.go(bop::E_FASTLOAD, &[]);
    h.go(bop::E_SOUND_DEVICE, &[]);
    true
}

/// `InitGame`.
pub(super) fn init_game(h: Host) -> bool {
    h.go(bop::G_HUD, &[]);
    let lang = h.go(bop::Q_CURRENT_LANG, &[]).bytes;
    h.go(bop::G_FONT, &[astr(base::default_font(&lang).as_bytes())]);
    let names: Vec<_> = CONTAINERS.iter().map(|c| astr(c)).collect();
    h.go(bop::G_CONTAINERS, &names);
    for (id, container) in MODULES_BEFORE_ACHIEVEMENTS {
        h.go(bop::G_MODULE, &[ai(id), astr(container)]);
    }
    if h.b(bop::G_ACHIEVEMENTS, &[]) {
        for (id, name) in base::ACHIEVEMENTS {
            h.go(
                bop::G_ACHIEVEMENT,
                &[ai(i64::from(id)), astr(name.as_bytes())],
            );
        }
        h.go(bop::G_ACHIEVEMENTS_END, &[]);
    }
    for (id, container) in MODULES_AFTER_ACHIEVEMENTS {
        h.go(bop::G_MODULE, &[ai(id), astr(container)]);
    }
    h.go(bop::G_RESET, &[]);
    for (i, key) in (0..).zip(ENTITY_LOADERS) {
        h.go(bop::G_LOADER, &[ai(0), ai(i), astr(key)]);
    }
    for (i, key) in (0..).zip(AREA_LOADERS) {
        h.go(bop::G_LOADER, &[ai(1), ai(i), astr(key)]);
    }
    h.go(bop::G_MESSAGE, &[ai(MSG_LOAD_FONTS)]);
    h.go(bop::G_MESSAGE, &[ai(MSG_LOAD_MAIN_CONFIG)]);
    true
}

/// `Exit`.
pub(super) fn exit(h: Host) {
    let save = h.i(bop::EXIT_BEGIN, &[]) & FLAG_SAVE_AT_EXIT != 0;
    if save {
        h.go(bop::SAVE_CONFIG, &[]);
    }
    h.go(bop::EXIT_GAME, &[]);
    for i in 0..h.i(bop::MODULE_COUNT, &[]) {
        h.go(bop::DELETE_MODULE, &[ai(i)]);
    }
    h.go(bop::EXIT_CONFIG, &[]);
    h.go(bop::EXIT_END, &[]);
}

type Ctx = *mut c_void;

/// # Safety
/// `ctx`/`call` as in the `ffi` module docs; every `bop` op is handled.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga13b_init(ctx: Ctx, call: super::ffi::Call) -> bool {
    init(Host { ctx, call })
}

/// # Safety
/// As `ohpl_ga13b_init`; `nums` is the live config handler's numbers (or null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga13b_init_engine(
    ctx: Ctx,
    call: super::ffi::Call,
    nums: *const MainNums,
) -> bool {
    // SAFETY: `nums` is null or points to the config handler's numbers for the whole call.
    unsafe { nums.as_ref() }.is_some_and(|n| init_engine(Host { ctx, call }, n))
}

/// # Safety
/// As `ohpl_ga13b_init`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga13b_init_game(ctx: Ctx, call: super::ffi::Call) -> bool {
    init_game(Host { ctx, call })
}

/// # Safety
/// As `ohpl_ga13b_init`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga13b_exit(ctx: Ctx, call: super::ffi::Call) {
    exit(Host { ctx, call });
}
