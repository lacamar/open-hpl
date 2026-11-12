//! `LuxBase.cpp` and `Main.cpp`: command line, init file, config file selection, XDG path plan,
//! start-game resolution, language file plan, profile paths, custom stories and small helpers.

use super::cfg::Cfg;
use crate::string;

pub fn w(s: &str) -> Vec<u32> {
    s.bytes().map(u32::from).collect()
}

fn cat(parts: &[&[u32]]) -> Vec<u32> {
    parts.concat()
}

fn wide(s: &[u8]) -> Vec<u32> {
    string::utf8_to_wide(s)
}

fn narrow(s: &[u32]) -> Vec<u8> {
    string::wide_to_utf8(s)
}

const KEY: [u8; 8] = [0x28, 0x25, 0xaf, 0xc3, 0x61, 0x27, 0x15, 0xd4];

/// `DecryptString`. The key position resets after every byte from the eighth on because
/// `lBuffPos` is never cleared, so bytes 8.. all use `KEY[0]`.
pub fn decrypt(enc: &[u8]) -> Vec<u8> {
    let end = enc.iter().position(|&b| b == 0).unwrap_or(enc.len());
    let mut pos = 0usize;
    let mut counted = 0usize;
    enc.iter()
        .take(end)
        .map(|&b| {
            let k = KEY.get(pos).copied().unwrap_or(0);
            pos += 1;
            counted += 1;
            if counted >= KEY.len() {
                pos = 0;
            }
            b ^ k
        })
        .collect()
}

pub const MAIN_INIT_STR: [u8; 27] = [
    0x4B, 0x4A, 0xC1, 0xA5, 0x8, 0x40, 0x3A, 0xA4, 0x5C, 0x4D, 0x5B, 0x5C, 0x77, 0x45, 0x49, 0x41,
    0x46, 0x77, 0x41, 0x46, 0x41, 0x5C, 0x6, 0x4B, 0x4E, 0x4F, 0,
];
pub const START_MAP_STR: [u8; 13] = [
    0x18, 0x14, 0xF0, 0xA0, 0x4, 0x4B, 0x79, 0xA7, 0x6, 0x45, 0x49, 0x58, 0,
];
pub const START_FOLDER_STR: [u8; 12] = [
    0x45, 0x44, 0xDF, 0xB0, 0x4E, 0x57, 0x61, 0xB1, 0x5B, 0x5C, 0x7, 0,
];
pub const START_POS_STR: [u8; 18] = [
    0x78, 0x49, 0xCE, 0xBA, 0x4, 0x55, 0x46, 0xA0, 0x49, 0x5A, 0x5C, 0x69, 0x5A, 0x4D, 0x49, 0x77,
    0x19, 0,
];

#[derive(Clone, Debug, PartialEq)]
pub struct CmdLine {
    pub ptest: bool,
    pub hard_mode: bool,
    pub default_init: Vec<u32>,
    pub init_file: Vec<u32>,
}

/// `ParseCommandLine`.
pub fn parse_command_line(cmd: &[u8]) -> CmdLine {
    let default_init = w("config/main_init.cfg");
    if cmd == b"ptest" {
        return CmdLine {
            ptest: true,
            hard_mode: false,
            init_file: wide(&decrypt(&MAIN_INIT_STR)),
            default_init,
        };
    }
    if cmd == b"hardmode" {
        return CmdLine {
            ptest: false,
            hard_mode: true,
            init_file: default_init.clone(),
            default_init,
        };
    }
    let mut init_file = wide(cmd);
    if init_file.is_empty() {
        init_file = default_init.clone();
    }
    CmdLine {
        ptest: false,
        hard_mode: false,
        init_file,
        default_init,
    }
}

/// `Main.cpp`: the message box text when `Init` fails.
pub fn error_text(msg: &[u32]) -> Vec<u32> {
    if msg.is_empty() {
        w("Error occured")
    } else {
        msg.to_vec()
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct AppVars {
    pub main_save_folder: Vec<u32>,
    pub default_user_config: Vec<u32>,
    pub default_user_key_config: Vec<u32>,
    pub default_main_config: Vec<u32>,
    pub game_config: Vec<u32>,
    pub menu_config: Vec<u32>,
    pub pre_menu_config: Vec<u32>,
    pub demo_config: Vec<u32>,
    pub resource_config: Vec<u8>,
    pub material_config: Vec<u8>,
    pub default_base_language: Vec<u8>,
    pub default_game_language: Vec<u8>,
    pub base_language_folder: Vec<u8>,
    pub game_language_folder: Vec<u8>,
    pub custom_story_path: Vec<u8>,
    pub game_name: Vec<u8>,
    pub allow_hardmode: bool,
    pub start_map_file: Vec<u8>,
    pub start_map_folder: Vec<u8>,
    pub start_map_pos: Vec<u8>,
}

/// The init file and the fallback init file; each is loaded on demand.
pub trait InitEnv {
    fn load_init(&mut self) -> bool;
    fn init(&mut self) -> &mut dyn Cfg;
    fn load_defaults(&mut self) -> bool;
    fn defaults(&mut self) -> &mut dyn Cfg;
    fn warn(&mut self, text: &[u32]);
}

fn wstr(c: &mut dyn Cfg, lvl: &[u8], key: &[u8]) -> Vec<u32> {
    c.string_w(lvl, key, &[])
}

/// The config half of `InitApp`: the variables read so far (all on success) and, on failure,
/// `msErrorMessage`.
pub fn init_app(e: &mut dyn InitEnv, cmd: &CmdLine) -> (AppVars, Option<Vec<u32>>) {
    let mut v = AppVars::default();
    if !e.load_init() {
        return (
            v,
            Some(cat(&[
                &w("Could not load main init file: "),
                &cmd.init_file,
            ])),
        );
    }
    let c = e.init();
    v.main_save_folder = wstr(c, b"Directories", b"MainSaveFolder");
    v.default_user_config = wstr(c, b"ConfigFiles", b"DefaultUserSettings");
    v.default_user_key_config = wstr(c, b"ConfigFiles", b"DefaultUserKeysSDL2");
    v.default_main_config = wstr(c, b"ConfigFiles", b"DefaultMainSettingsSDL2");

    if v.default_user_key_config.is_empty() || v.default_main_config.is_empty() {
        if cmd.init_file != cmd.default_init {
            if !e.load_defaults() {
                let m = cat(&[
                    &w("Could not load default main init file: "),
                    &cmd.default_init,
                ]);
                return (v, Some(m));
            }
            let mut warn = cat(&[
                &w("The following 'ConfigFiles' path entries were not defined in "),
                &cmd.init_file,
            ]);
            if v.default_main_config.is_empty() {
                warn.extend(w("\n - DefaultMainSettingsSDL2"));
                v.default_main_config =
                    wstr(e.defaults(), b"ConfigFiles", b"DefaultMainSettingsSDL2");
            }
            if v.default_user_key_config.is_empty() {
                warn.extend(w("\n - DefaultUserKeysSDL2"));
                v.default_user_key_config =
                    wstr(e.defaults(), b"ConfigFiles", b"DefaultUserKeysSDL2");
            }
            warn.extend(w("\nGame defaults will be used"));
            if !v.default_main_config.is_empty() && !v.default_user_key_config.is_empty() {
                e.warn(&warn);
            }
        }
        if v.default_main_config.is_empty() || v.default_user_key_config.is_empty() {
            let mut m = w("Could not load default settings files:");
            if v.default_main_config.is_empty() {
                m.extend(w("\n - DefaultMainSettingsSDL2"));
            }
            if v.default_user_key_config.is_empty() {
                m.extend(w("\n - DefaultUserKeysSDL2"));
            }
            return (v, Some(m));
        }
    }

    let c = e.init();
    v.game_config = wstr(c, b"ConfigFiles", b"Game");
    v.menu_config = wstr(c, b"ConfigFiles", b"Menu");
    v.pre_menu_config = wstr(c, b"ConfigFiles", b"PreMenu");
    v.demo_config = wstr(c, b"ConfigFiles", b"Demo");
    v.resource_config = c.string(b"ConfigFiles", b"Resources", b"");
    v.material_config = c.string(b"ConfigFiles", b"Materials", b"");
    v.default_base_language = c.string(b"ConfigFiles", b"DefaultBaseLanguage", b"");
    v.default_game_language = c.string(b"ConfigFiles", b"DefaultGameLanguage", b"");
    v.base_language_folder = c.string(b"Directories", b"BaseLanguageFolder", b"");
    v.game_language_folder = c.string(b"Directories", b"GameLanguageFolder", b"");
    v.custom_story_path = c.string(b"Directories", b"CustomStoryPath", b"");
    v.game_name = c.string(b"Variables", b"GameName", b"");
    v.allow_hardmode = c.bool(b"Variables", b"AllowHardMode", false);
    v.start_map_file = c.string(b"StartMap", b"File", b"");
    v.start_map_folder = c.string(b"StartMap", b"Folder", b"");
    v.start_map_pos = c.string(b"StartMap", b"Pos", b"");
    (v, None)
}

#[derive(Clone, Debug, PartialEq)]
pub enum PathOp {
    Migrate(Vec<u32>),
    CreateIfMissing(Vec<u32>),
    Mkdir(Vec<u32>),
    MoveIfMissing(Vec<u32>, Vec<u32>),
}

#[derive(Clone, Debug, PartialEq)]
pub struct PathPlan {
    pub ops: Vec<PathOp>,
    pub game_relative: Vec<u32>,
    pub base_save: Vec<u32>,
    pub base_config: Vec<u32>,
    pub base_state: Vec<u32>,
    pub crash_flag: Vec<u32>,
    pub first_start_flag: Vec<u32>,
    pub log_file: Vec<u32>,
    pub update_log_file: Vec<u32>,
}

/// The directory plan of `InitApp` below `$XDG_DATA_HOME`, `$XDG_CONFIG_HOME` and
/// `$XDG_STATE_HOME`; the engine performs every op in order.
pub fn plan_paths(
    personal: &[u32],
    config_home: &[u32],
    state_home: &[u32],
    main_save_folder: &[u32],
    custom_story_path: &[u32],
    headless_pid: Option<i32>,
) -> PathPlan {
    let game = w("open-hpl/Amnesia/");
    let slash = w("/");
    let mut dirs = vec![w("open-hpl"), game.clone()];
    if !main_save_folder.is_empty() {
        dirs.push(cat(&[&game, main_save_folder, &slash]));
    }
    dirs.push(game.clone());
    let resources = cat(&[&game, &w("local_resources/")]);
    dirs.push(resources.clone());
    if !custom_story_path.is_empty() {
        dirs.push(cat(&[&resources, custom_story_path, &slash]));
    }
    let mut ops = vec![PathOp::Migrate(cat(&[personal, &game]))];
    ops.extend(
        dirs.iter()
            .map(|d| PathOp::CreateIfMissing(cat(&[personal, d]))),
    );
    let base_save = cat(&[personal, &game, main_save_folder, &slash]);
    let xdg = |home: &[u32]| cat(&[home, &game, main_save_folder, &slash]);
    let base_config = xdg(config_home);
    let base_state = xdg(state_home);
    ops.push(PathOp::Mkdir(base_config.clone()));
    ops.push(PathOp::Mkdir(base_state.clone()));
    for (file, to) in [
        ("main_settings.cfg", &base_config),
        ("first_start_flag", &base_state),
    ] {
        ops.push(PathOp::MoveIfMissing(
            cat(&[&base_save, &w(file)]),
            cat(&[to, &w(file)]),
        ));
    }
    let suffix = headless_pid.map_or_else(Vec::new, |p| w(&format!("-{p}")));
    PathPlan {
        ops,
        crash_flag: cat(&[&base_state, &w("crash_flag")]),
        first_start_flag: cat(&[&base_state, &w("first_start_flag")]),
        log_file: cat(&[&base_state, &w("hpl"), &suffix, &w(".log")]),
        update_log_file: cat(&[&base_state, &w("hpl_update"), &suffix, &w(".log")]),
        game_relative: game,
        base_save,
        base_config,
        base_state,
    }
}

/// `LoadConfigFile`: the wanted file when it exists and the default is not forced.
pub fn use_wanted_config(force_default: bool, wanted_exists: bool) -> bool {
    !force_default && wanted_exists
}

#[derive(Clone, Debug, PartialEq)]
pub struct MainVars {
    pub save_config_at_exit: bool,
    pub exit_menu_directly: bool,
    pub default_profile_name: Vec<u32>,
    pub show_pre_menu: bool,
    pub show_menu: bool,
    pub update_log_active: bool,
}

/// The basic variables of `InitMainConfig`.
pub fn main_vars(c: &mut dyn Cfg) -> MainVars {
    MainVars {
        save_config_at_exit: c.bool(b"Main", b"SaveConfig", true),
        exit_menu_directly: c.bool(b"Main", b"ExitMenuDirectly", false),
        default_profile_name: c.string_w(b"Main", b"DefaultProfileName", &[]),
        show_pre_menu: c.bool(b"Main", b"ShowPreMenu", true),
        show_menu: c.bool(b"Main", b"ShowMenu", true),
        update_log_active: c.bool(b"Main", b"UpdateLogActive", true),
    }
}

/// `CheckFeatureSupport`: the error for the first missing capability.
pub fn feature_error(
    shader_model_2: i32,
    max_draw_buffers: i32,
    packed_depth_stencil: i32,
) -> Option<&'static str> {
    if shader_model_2 == 0 {
        Some("Shader model 2 not supported! Make sure your graphic card drivers are up to date!\n")
    } else if max_draw_buffers < 4 {
        Some(
            "Not enough drawbuffers supported! Make sure your graphic card drivers are up to date!\n",
        )
    } else if packed_depth_stencil == 0 {
        Some(
            "Packed Depth and Stencil not supported! Make sure your graphic card drivers are up to date!\n",
        )
    } else {
        None
    }
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum StartMode {
    PreMenu,
    Menu,
    Direct,
}

/// `Init`: what to start once the game data exists.
pub fn start_mode(show_pre_menu: bool, show_menu: bool) -> StartMode {
    if show_pre_menu && show_menu {
        StartMode::PreMenu
    } else if show_menu {
        StartMode::Menu
    } else {
        StartMode::Direct
    }
}

/// `Init` direct start: `Some(lock_input)` when the debug window is not active.
pub fn direct_start_input(debug_window_active: bool, fullscreen: bool) -> Option<bool> {
    (!debug_window_active).then_some(!fullscreen)
}

#[derive(Clone, Debug, PartialEq)]
pub struct CustomStory {
    pub maps_folder: Vec<u8>,
    pub start_map: Vec<u8>,
    pub start_pos: Vec<u8>,
    pub name: Vec<u32>,
    pub author: Vec<u32>,
    pub img_file: Vec<u8>,
    pub extra_lang_prefix: Vec<u8>,
    pub default_extra_lang: Vec<u8>,
}

/// `CreateFromPath` once `custom_story_settings.cfg` loaded; the second value is the log reason
/// when invalid. Empty name and author are for the caller to translate.
pub fn custom_story(
    c: &mut dyn Cfg,
    root: &[u32],
    default_lang: &[u8],
    file_exists: &mut dyn FnMut(&[u32]) -> bool,
) -> (CustomStory, Option<&'static str>) {
    let mut maps = narrow(root);
    maps.extend(c.string(b"Main", b"MapsFolder", b"maps"));
    let maps_folder = string::add_slash_at_end(&maps, b'/');
    let start_map = c.string(b"Main", b"StartMap", b"");
    let start_pos = c.string(b"Main", b"StartPos", b"");
    let path = cat(&[&wide(&maps_folder), &wide(&start_map)]);
    let err = (start_map.is_empty() || !file_exists(&path)).then_some("could not find start map");
    let name = wide(&c.string(b"Main", b"Name", b""));
    let author = wide(&c.string(b"Main", b"Author", b""));
    let img_file = c.string(b"Main", b"ImgFile", b"");
    let extra_lang_prefix = c.string(b"Main", b"ExtraLangFilePrefix", b"extra_");
    let default_extra_lang = c.string(b"Main", b"DefaultExtraLangFile", default_lang);
    (
        CustomStory {
            maps_folder,
            start_map,
            start_pos,
            name,
            author,
            img_file,
            extra_lang_prefix,
            default_extra_lang,
        },
        err,
    )
}

/// `SetCustomStory`: the profile save path below a story root. An empty root has no last folder;
/// the C++ indexed `size()-1` of an empty vector (undefined), here the folder is empty.
pub fn custom_story_save_path(main_profile_save: &[u32], story_root: &[u32]) -> Vec<u32> {
    let sep = w("/");
    let slash = u32::from(b'/');
    let folder = string::split(story_root, Some(&sep), true)
        .last()
        .map_or_else(Vec::new, <[u32]>::to_vec);
    let base = string::add_slash_at_end(&cat(&[main_profile_save, &w("custom")]), slash);
    cat(&[&base, &string::add_slash_at_end(&folder, slash)])
}

#[derive(Clone, Debug, PartialEq)]
pub struct StartTarget {
    pub file: Vec<u8>,
    pub folder: Vec<u8>,
    pub pos: Vec<u8>,
}

pub struct StartDefaults<'a> {
    pub file: &'a [u8],
    pub folder: &'a [u8],
    pub pos: &'a [u8],
}

pub struct StartStory<'a> {
    pub start_map: &'a [u8],
    pub maps_folder: &'a [u8],
    pub start_pos: &'a [u8],
}

fn pick(
    given: &[u8],
    from_story: Option<&[u8]>,
    user: &mut dyn Cfg,
    key: &[u8],
    default: &[u8],
) -> Vec<u8> {
    if !given.is_empty() {
        return given.to_vec();
    }
    let v = match from_story {
        Some(s) => s.to_vec(),
        None => user.string(b"Map", key, b""),
    };
    if v.is_empty() { default.to_vec() } else { v }
}

/// `StartGame`: file, folder and start position, filled from the custom story or the user
/// settings, then the game defaults; ptest overrides all three. `set_map_folder` gets the
/// resolved folder (before the ptest override) between the folder and the position.
pub fn resolve_start(
    given: &StartTarget,
    story: Option<StartStory>,
    user: &mut dyn Cfg,
    def: &StartDefaults,
    ptest: bool,
    set_map_folder: &mut dyn FnMut(&[u8]),
) -> StartTarget {
    let file = pick(
        &given.file,
        story.as_ref().map(|s| s.start_map),
        user,
        b"File",
        def.file,
    );
    let folder = pick(
        &given.folder,
        story.as_ref().map(|s| s.maps_folder),
        user,
        b"Folder",
        def.folder,
    );
    set_map_folder(&folder);
    let pos = pick(
        &given.pos,
        story.as_ref().map(|s| s.start_pos),
        user,
        b"StartPos",
        def.pos,
    );
    if ptest {
        return StartTarget {
            file: decrypt(&START_MAP_STR),
            folder: decrypt(&START_FOLDER_STR),
            pos: decrypt(&START_POS_STR),
        };
    }
    StartTarget { file, folder, pos }
}

pub struct LangStory<'a> {
    pub root: &'a [u8],
    pub extra_prefix: &'a [u8],
    pub default_extra: &'a [u8],
}

pub struct LangFolders<'a> {
    pub game_folder: &'a [u8],
    pub base_folder: &'a [u8],
    pub default_game: &'a [u8],
    pub default_base: &'a [u8],
}

pub trait LangEnv {
    fn clear_translations(&mut self);
    fn file_found(&mut self, name: &[u8]) -> bool;
    fn add_language_file(&mut self, name: &[u8], force: bool, user_dir: bool);
}

/// `LoadLanguage`: the new current language, or `None` when it was already loaded.
pub fn load_language(
    e: &mut dyn LangEnv,
    name: &[u8],
    force_reload: bool,
    current: &[u8],
    story: Option<LangStory>,
    f: &LangFolders,
) -> Option<Vec<u8>> {
    let low = string::to_lower(name);
    if current == low.as_slice() && !force_reload {
        return None;
    }
    if !current.is_empty() {
        e.clear_translations();
    }
    let game_file = string::set_file_ext(name, b"lang");
    let base_file = [b"base_".as_slice(), &game_file].concat();
    if let Some(s) = story.as_ref().filter(|s| !s.extra_prefix.is_empty()) {
        let extra = [s.root, s.extra_prefix, &game_file].concat();
        if e.file_found(&extra) {
            e.add_language_file(&extra, true, false);
        }
    }
    e.add_language_file(&[f.game_folder, &game_file].concat(), true, true);
    e.add_language_file(&[f.base_folder, &base_file].concat(), true, true);
    if let Some(s) = story.as_ref().filter(|s| game_file != s.default_extra) {
        e.add_language_file(
            &[s.root, s.extra_prefix, s.default_extra].concat(),
            false,
            false,
        );
    }
    if game_file != f.default_game {
        e.add_language_file(&[f.game_folder, f.default_game].concat(), false, true);
    }
    if base_file != f.default_base {
        e.add_language_file(&[f.base_folder, f.default_base].concat(), false, true);
    }
    Some(low)
}

#[derive(Clone, Debug, PartialEq)]
pub struct ProfilePlan {
    pub save_previous: bool,
    pub main_profile_save: Vec<u32>,
    pub main_profile_config: Vec<u32>,
    pub profile_save: Vec<u32>,
    pub ops: Vec<PathOp>,
}

/// `SetProfile`.
pub fn set_profile(
    prev_profile: &[u32],
    have_user_config: bool,
    name: &[u32],
    base_save: &[u32],
    base_config: &[u32],
) -> ProfilePlan {
    let save_previous = !prev_profile.is_empty() && have_user_config;
    if name.is_empty() {
        return ProfilePlan {
            save_previous,
            main_profile_save: vec![],
            main_profile_config: vec![],
            profile_save: vec![],
            ops: vec![],
        };
    }
    let slash = u32::from(b'/');
    let save = string::add_slash_at_end(&cat(&[base_save, name]), slash);
    let config = string::add_slash_at_end(&cat(&[base_config, name]), slash);
    let moved =
        |file: &str| PathOp::MoveIfMissing(cat(&[&save, &w(file)]), cat(&[&config, &w(file)]));
    let ops = vec![
        PathOp::Mkdir(config.clone()),
        moved("user_settings.cfg"),
        moved("user_keys.cfg"),
    ];
    ProfilePlan {
        save_previous,
        profile_save: save.clone(),
        main_profile_save: save,
        main_profile_config: config,
        ops,
    }
}

/// `CreateProfile`: the folder to create, or `None` when it exists.
pub fn profile_folder(base_save: &[u32], name: &[u32], exists: bool) -> Option<Vec<u32>> {
    (!exists).then(|| cat(&[base_save, name, &w("/")]))
}

/// `LuxCalcGuiSetOffset`: (size, offset).
pub fn calc_gui_set_offset(virtual_size: [f32; 2], screen: [f32; 2]) -> ([f32; 2], [f32; 2]) {
    let wanted = virtual_size[0] / virtual_size[1];
    let ratio = screen[0] / screen[1];
    if ratio >= (4.0f32 / 3.0f32) - 0.001 {
        let add_x = virtual_size[0] * (ratio - wanted);
        (
            [virtual_size[0] + add_x, virtual_size[1]],
            [add_x * 0.5, 0.0],
        )
    } else {
        (virtual_size, [0.0, 0.0])
    }
}

/// `InitGame` font file for the current language.
pub fn default_font(current_language: &[u8]) -> &'static str {
    if current_language == b"chinese.lang" {
        "game_default.fnt"
    } else {
        "font_default.fnt"
    }
}

/// `InitAchievements`: (`eLuxAchievement`, name) in registration order.
pub const ACHIEVEMENTS: [(i32, &str); 18] = [
    (0, "Insanity"),
    (1, "Alchemist"),
    (2, "EscapeArtist"),
    (3, "Descendant"),
    (4, "Pipeworker"),
    (5, "Restorer"),
    (6, "Survivor"),
    (7, "Sacrifice"),
    (8, "Benefactor"),
    (9, "Illuminatus"),
    (10, "NOPE"),
    (11, "Quitter"),
    (13, "Egotist"),
    (14, "Altruist"),
    (15, "Vacillator"),
    (16, "StillAlive"),
    (12, "Masochist"),
    (17, "MasterArchivist"),
];

/// `StringToAxis`: x/y/z as 0..2; the flag is false when the C++ logs the error and answers x.
pub fn axis_from_name(name: &[u8]) -> (i32, bool) {
    match string::to_lower(name).as_slice() {
        b"x" => (0, true),
        b"y" => (1, true),
        b"z" => (2, true),
        _ => (0, false),
    }
}
