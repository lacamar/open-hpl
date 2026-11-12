//! `Main.cpp`: init file choice, directory plan, config file choice, language plan, game exe and
//! the arguments it starts with.

use crate::luxbase::cfg::Cfg;
use crate::string;

pub const DEFAULT_INIT: &[u8] = b"config/main_init.cfg";
const GAME_PARENT: &[u8] = b"open-hpl/Amnesia/";

/// `hplMain` ignores the command line (it breaks Epic launches), so callers pass empty params.
pub fn init_cfg_name(params: &[u8]) -> Vec<u8> {
    if params == b"ptest" {
        b"config/ptest_main_init.cfg".to_vec()
    } else if params.is_empty() {
        DEFAULT_INIT.to_vec()
    } else {
        params.to_vec()
    }
}

pub fn user_resource_dir(personal: &[u8]) -> Vec<u8> {
    [personal, GAME_PARENT, b"local_resources/"].concat()
}

/// The init file read: the user resource copy when it exists.
pub fn init_cfg_path(user_dir: &[u8], init: &[u8], user_exists: bool) -> Vec<u8> {
    if user_exists {
        [user_dir, init].concat()
    } else {
        init.to_vec()
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct InitVars {
    pub save_folder: Vec<u8>,
    pub default_main: Vec<u8>,
    pub low: Vec<u8>,
    pub medium: Vec<u8>,
    pub high: Vec<u8>,
    pub default_base_language: Vec<u8>,
    pub base_language_folder: Vec<u8>,
    pub game_language_folder: Vec<u8>,
    pub custom_stories: Vec<u8>,
    pub game_name: Vec<u8>,
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct InitResult {
    pub vars: InitVars,
    pub messages: Vec<Vec<u8>>,
    pub failed: bool,
}

const SDL2_KEY: &[u8] = b"DefaultMainSettingsSDL2";

pub fn message_init_load(init: &[u8]) -> Vec<u8> {
    [b"Could not load main init file: ".as_slice(), init].concat()
}

/// `InitPaths` reads. `load_defaults` loads the default init file on demand and returns its
/// `DefaultMainSettingsSDL2`, or `None` when it fails to load.
pub fn read_init(
    init: &mut dyn Cfg,
    init_file: &[u8],
    default_file: &[u8],
    load_defaults: &mut dyn FnMut() -> Option<Vec<u8>>,
) -> InitResult {
    let mut r = InitResult::default();
    r.vars.save_folder = init.string(b"Directories", b"MainSaveFolder", b"");
    r.vars.default_main = init.string(b"ConfigFiles", SDL2_KEY, b"");
    if r.vars.default_main.is_empty() && init_file != default_file {
        r.messages.push(
            [
                b"No DefaultMainSettingsSDL2 path defined in: ".as_slice(),
                init_file,
                b" - Using default",
            ]
            .concat(),
        );
        let Some(v) = load_defaults() else {
            r.messages.push(
                [
                    b"Could not load default main init file: ".as_slice(),
                    default_file,
                ]
                .concat(),
            );
            r.failed = true;
            return r;
        };
        r.vars.default_main = v;
        if r.vars.default_main.is_empty() {
            r.messages.push(
                b"Settings file for entry DefaultMainSettingsSDL2 in default config is empty"
                    .to_vec(),
            );
            r.failed = true;
            return r;
        }
    }
    let v = &mut r.vars;
    v.low = init.string(b"ConfigFiles", b"DefaultMainSettingsLow", b"");
    v.medium = init.string(b"ConfigFiles", b"DefaultMainSettingsMedium", b"");
    v.high = init.string(b"ConfigFiles", b"DefaultMainSettingsHigh", b"");
    v.default_base_language = init.string(b"ConfigFiles", b"DefaultBaseLanguage", b"");
    v.base_language_folder = init.string(b"Directories", b"BaseLanguageFolder", b"");
    v.game_language_folder = init.string(b"Directories", b"GameLanguageFolder", b"");
    v.custom_stories = init.string(b"Directories", b"CustomStoryPath", b"");
    v.game_name = init.string(b"Variables", b"GameName", b"");
    r
}

/// Directory work, in execution order.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum PathOp {
    /// Create below the personal dir unless it exists.
    CreateIfMissing(Vec<u8>),
    /// Create (the XDG config and state dirs).
    Mkdir(Vec<u8>),
    /// Rename unless the target exists.
    MoveIfMissing(Vec<u8>, Vec<u8>),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PathPlan {
    pub ops: Vec<PathOp>,
    pub base_save: Vec<u8>,
    pub base_config: Vec<u8>,
    pub crash_flag: Vec<u8>,
    pub log_file: Vec<u8>,
}

/// `InitPaths` below `$XDG_DATA_HOME` (`personal`), `$XDG_CONFIG_HOME` and `$XDG_STATE_HOME`;
/// all three end in a slash.
pub fn plan_paths(
    personal: &[u8],
    config_home: &[u8],
    state_home: &[u8],
    save_folder: &[u8],
    custom_story_path: &[u8],
) -> PathPlan {
    let game: &[u8] = GAME_PARENT;
    let resources = [game, b"local_resources/"].concat();
    let mut dirs: Vec<Vec<u8>> = vec![b"open-hpl".to_vec(), game.to_vec()];
    if !save_folder.is_empty() {
        dirs.push([game, save_folder, b"/"].concat());
    }
    dirs.push(game.to_vec());
    dirs.push(resources.clone());
    if !custom_story_path.is_empty() {
        dirs.push([resources.as_slice(), custom_story_path, b"/"].concat());
    }
    let mut ops: Vec<PathOp> = dirs
        .iter()
        .map(|d| PathOp::CreateIfMissing([personal, d].concat()))
        .collect();
    let base_save = [personal, game, save_folder, b"/"].concat();
    let base_config = [config_home, game, save_folder, b"/"].concat();
    let base_state = [state_home, game, save_folder, b"/"].concat();
    ops.push(PathOp::Mkdir(base_config.clone()));
    ops.push(PathOp::Mkdir(base_state.clone()));
    ops.push(PathOp::MoveIfMissing(
        [base_save.as_slice(), b"main_settings.cfg"].concat(),
        [base_config.as_slice(), b"main_settings.cfg"].concat(),
    ));
    PathPlan {
        ops,
        crash_flag: [base_state.as_slice(), b"crash_flag"].concat(),
        log_file: [base_state.as_slice(), b"launcher.log"].concat(),
        base_save,
        base_config,
    }
}

pub fn settings_path(base_config: &[u8]) -> Vec<u8> {
    [base_config, b"main_settings.cfg"].concat()
}

/// `LoadConfigFile`: the wanted file when it exists and the default is not forced; otherwise
/// the default, saved to the wanted path.
pub fn use_wanted_config(force_default: bool, wanted_exists: bool) -> bool {
    !force_default && wanted_exists
}

/// Message when the config fails to load: only for an empty default path.
pub fn config_load_message(loaded_wanted: bool, default_empty: bool) -> Option<&'static [u8]> {
    (!loaded_wanted && default_empty).then_some(b"Empty default config file in startup config file")
}

pub fn sound_device(main: &mut dyn Cfg) -> i32 {
    main.int(b"Sound", b"Device", -1)
}

pub fn show_launcher(main: &mut dyn Cfg) -> bool {
    main.bool(b"Main", b"ShowLauncher", true)
}

pub fn start_language(main: &mut dyn Cfg) -> Vec<u8> {
    main.string(b"Main", b"StartLanguage", b"english.lang")
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct LangOp {
    pub file: Vec<u8>,
    /// Required file, looked up outside the user resource dir; fallbacks may come from it.
    pub forced: bool,
}

/// `LoadLanguage`: translations are cleared, then these files are added in order. The launcher
/// never sets a default game language, so the empty name is still added as a fallback.
pub fn language_plan(
    name: &[u8],
    base_folder: &[u8],
    game_folder: &[u8],
    default_base: &[u8],
    default_game: &[u8],
) -> Vec<LangOp> {
    let game_file = string::set_file_ext(name, b"lang");
    let base_file = [b"base_".as_slice(), &game_file].concat();
    let mut ops = vec![LangOp {
        file: [base_folder, &base_file].concat(),
        forced: true,
    }];
    if game_file != default_game {
        ops.push(LangOp {
            file: [game_folder, default_game].concat(),
            forced: false,
        });
    }
    if base_file != default_base {
        ops.push(LangOp {
            file: [base_folder, default_base].concat(),
            forced: false,
        });
    }
    ops
}

#[cfg(target_arch = "aarch64")]
pub const GAME_BIN: &[u8] = b"Amnesia.bin.aarch64";
#[cfg(target_arch = "x86")]
pub const GAME_BIN: &[u8] = b"Amnesia.bin.x86";
#[cfg(not(any(target_arch = "aarch64", target_arch = "x86")))]
pub const GAME_BIN: &[u8] = b"Amnesia.bin.x86_64";

/// Window class (matches the game binary name for `StartupWMClass`).
pub fn window_class() -> &'static [u8] {
    GAME_BIN
}

/// Game binary next to the launcher.
pub fn game_exe(data_dir: &[u8]) -> Vec<u8> {
    [data_dir, b"/", GAME_BIN].concat()
}

/// After the dialog loop: whether to start the game (`exitflag` 0 is cancel) and its arguments;
/// `exitflag` 2 starts the ptest build.
pub fn run_plan(exitflag: i32, params: &[u8]) -> (bool, Vec<u8>) {
    let params: &[u8] = if exitflag == 2 { b"ptest" } else { params };
    (exitflag != 0, [b"-cwd ".as_slice(), params].concat())
}
