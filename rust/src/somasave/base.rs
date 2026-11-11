//! Game detection, settings file, command line, path setup and boot decisions of `SomaConfig.cpp`,
//! `SomaBase.cpp` and `Main.cpp`.

use crate::string;

fn contains(hay: &[u8], needle: &[u8]) -> bool {
    hay.windows(needle.len()).any(|w| w == needle)
}

/// `SomaGameId`: "rebirth" / "bunker" / "soma" from the game's `main_init.cfg` text.
pub fn game_id(main_init: &[u8]) -> &'static str {
    if contains(main_init, b"Rebirth") {
        "rebirth"
    } else if contains(main_init, b"Bunker") {
        "bunker"
    } else {
        "soma"
    }
}

/// `ParseCommandLine`: the init config path, `config/main_init.cfg` by default.
pub fn init_config_file(cmdline: &[u8]) -> Vec<u32> {
    let w = string::utf8_to_wide(cmdline);
    if w.is_empty() {
        string::utf8_to_wide(b"config/main_init.cfg")
    } else {
        w
    }
}

/// Directories to create, in order, for `root` + `parts`, each extending the previous.
pub fn dir_chain(root: &[u32], parts: &[&[u32]]) -> Vec<Vec<u32>> {
    let mut d = root.to_vec();
    parts
        .iter()
        .map(|p| {
            d.extend_from_slice(p);
            d.clone()
        })
        .collect()
}

fn w(s: &str) -> Vec<u32> {
    s.bytes().map(u32::from).collect()
}

fn id_dir(id: &str) -> Vec<u32> {
    let mut v = w(id);
    v.push(u32::from(b'/'));
    v
}

/// `cSomaSaveHandler::GetSaveDir`: chain below `$XDG_DATA_HOME`.
pub fn save_dirs(data_home: &[u32], id: &str) -> Vec<Vec<u32>> {
    dir_chain(data_home, &[&w("open-hpl/"), &id_dir(id), &w("saves/")])
}

/// `cSomaConfig::GetConfigFilePath`: dirs below `$XDG_CONFIG_HOME`, then the settings file.
pub fn config_dirs(config_home: &[u32], id: &str) -> (Vec<Vec<u32>>, Vec<u32>) {
    let dirs = dir_chain(config_home, &[&w("open-hpl/"), &id_dir(id)]);
    let mut file = dirs.last().cloned().unwrap_or_default();
    file.extend(w("main_settings.cfg"));
    (dirs, file)
}

/// `cSomaBase::SetupLogFile`: dirs below `$XDG_STATE_HOME`, then the log file.
pub fn log_dirs(
    state_home: &[u32],
    id: &str,
    headless: bool,
    pid: i32,
) -> (Vec<Vec<u32>>, Vec<u32>) {
    let dirs = dir_chain(state_home, &[&w("open-hpl/"), &id_dir(id)]);
    let mut file = dirs.last().cloned().unwrap_or_default();
    if headless {
        file.extend(w(&format!("hpl-{pid}.log")));
    } else {
        file.extend(w("hpl.log"));
    }
    (dirs, file)
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub struct ConfigVals {
    pub master_volume: f32,
    pub gamma: f32,
    pub vsync: bool,
    pub fullscreen: bool,
    pub screen_width: i32,
    pub screen_height: i32,
    pub anti_aliasing: bool,
    pub show_subtitles: bool,
}

impl Default for ConfigVals {
    fn default() -> Self {
        ConfigVals {
            master_volume: 1.0,
            gamma: 1.0,
            vsync: false,
            fullscreen: false,
            screen_width: 1280,
            screen_height: 720,
            anti_aliasing: true,
            show_subtitles: true,
        }
    }
}

pub trait CfgHost {
    fn get_float(&mut self, level: &str, key: &str, def: f32) -> f32;
    fn get_bool(&mut self, level: &str, key: &str, def: bool) -> bool;
    fn get_int(&mut self, level: &str, key: &str, def: i32) -> i32;
}

/// `cSomaConfig::Load` after the file was read: each key falls back to the current value.
pub fn load_config(h: &mut dyn CfgHost, c: &mut ConfigVals) {
    c.master_volume = h.get_float("Sound", "Volume", c.master_volume);
    c.gamma = h.get_float("Graphics", "Gamma", c.gamma);
    c.vsync = h.get_bool("Screen", "Vsync", c.vsync);
    c.fullscreen = h.get_bool("Screen", "FullScreen", c.fullscreen);
    c.screen_width = h.get_int("Screen", "Width", c.screen_width);
    c.screen_height = h.get_int("Screen", "Height", c.screen_height);
    c.anti_aliasing = h.get_bool("Graphics", "AntiAliasing", c.anti_aliasing);
    c.show_subtitles = h.get_bool("Sound", "ShowSubtitles", c.show_subtitles);
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum Value {
    Float(f32),
    Bool(bool),
    Int(i32),
}

/// `cSomaConfig::Save`: (level, key, value) in write order.
pub fn save_config(c: &ConfigVals) -> [(&'static str, &'static str, Value); 8] {
    [
        ("Sound", "Volume", Value::Float(c.master_volume)),
        ("Graphics", "Gamma", Value::Float(c.gamma)),
        ("Screen", "Vsync", Value::Bool(c.vsync)),
        ("Screen", "FullScreen", Value::Bool(c.fullscreen)),
        ("Screen", "Width", Value::Int(c.screen_width)),
        ("Screen", "Height", Value::Int(c.screen_height)),
        ("Graphics", "AntiAliasing", Value::Bool(c.anti_aliasing)),
        ("Sound", "ShowSubtitles", Value::Bool(c.show_subtitles)),
    ]
}

/// Renderer settings `cSomaBase::Init` derives from the game id.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct EnginePlan {
    pub shadow_map_compare_off: bool,
    pub linear_depth_texture_idx: Option<i32>,
    pub gbuffer_textures: Option<i32>,
    pub depth_in_normal_alpha: bool,
    pub keep_hpl3_animations: bool,
    pub hardware_srgb: bool,
}

pub fn engine_plan(id: &str) -> EnginePlan {
    let (rebirth, bunker) = (id == "rebirth", id == "bunker");
    EnginePlan {
        shadow_map_compare_off: rebirth || bunker,
        linear_depth_texture_idx: if rebirth {
            Some(3)
        } else if bunker {
            Some(4)
        } else {
            None
        },
        gbuffer_textures: bunker.then_some(5),
        depth_in_normal_alpha: !rebirth && !bunker,
        keep_hpl3_animations: id == "soma",
        hardware_srgb: id != "soma",
    }
}

/// `OPENHPL_SOMA_FREECAM`: unset, empty or "0" keeps the real player.
pub fn use_real_player(freecam: Option<&[u8]>) -> bool {
    freecam.is_none_or(|f| f.is_empty() || f == b"0")
}

/// `OPENHPL_SOMA_MAP` / `_STARTPOS` after boot: the map to load, with its start position ("*" default).
pub fn boot_map(map: Option<&[u8]>, start_pos: Option<&[u8]>) -> Option<(Vec<u8>, Vec<u8>)> {
    let m = map.filter(|m| !m.is_empty())?;
    Some((m.to_vec(), start_pos.unwrap_or(b"*").to_vec()))
}

/// `LoadScriptMainMenu`: the configured menu map, `main_menu.hpm` when unset.
pub fn menu_map(configured: &[u8]) -> Vec<u8> {
    if configured.is_empty() {
        b"main_menu.hpm".to_vec()
    } else {
        configured.to_vec()
    }
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum StartArea {
    NoArea,
    First,
    Named,
}

/// `LoadMap` start area: "" none, "*" the first, anything else by name (first when missing).
pub fn start_area(name: &[u8]) -> StartArea {
    if name.is_empty() {
        StartArea::NoArea
    } else if name == b"*" {
        StartArea::First
    } else {
        StartArea::Named
    }
}

#[derive(Clone, PartialEq, Debug)]
pub enum MainPlan {
    ScriptCheck {
        report: Vec<u8>,
        game_dir: Vec<u8>,
        api: Vec<u8>,
    },
    Engine,
}

/// `hplMain`: `OPENHPL_SOMA_SCRIPT_CHECK=<report>` compiles the scripts instead of starting the engine.
pub fn main_plan(report: Option<&[u8]>, game_dir: Option<&[u8]>, api: Option<&[u8]>) -> MainPlan {
    match report {
        Some(r) => MainPlan::ScriptCheck {
            report: r.to_vec(),
            game_dir: game_dir.unwrap_or(b".").to_vec(),
            api: api.unwrap_or(b"script_api.txt").to_vec(),
        },
        None => MainPlan::Engine,
    }
}

/// `Main.cpp` error path: the message box text, "Error occured" when empty.
pub fn error_message(msg: &[u32]) -> Vec<u32> {
    if msg.is_empty() {
        w("Error occured")
    } else {
        msg.to_vec()
    }
}

/// `Init` failure message when the init config cannot be read.
pub fn init_config_error(file: &[u32]) -> Vec<u32> {
    let mut m = w("Could not load main init file: ");
    m.extend_from_slice(file);
    m
}
