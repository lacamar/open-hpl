//! `LuxDebugHandler` window, overlays, inspection and reload paths. Widgets, the renderable
//! containers and every other still-C++ engine object are reached through the host op table
//! (`dop`, shared with the input handler's `OhplInHostCall`); widgets and container nodes are ids
//! or opaque pointers.

use super::debug::{Debug, Toggle, inspection_line};
use super::ffi::{Host, af, ai, astr, aw};
use super::input_h::host;
use crate::math::{aabb_intersection, aabb_line_intersection, point_in_aabb};
use crate::string;
use crate::types::Vector3f;

pub mod dop {
    pub const GUI_INIT: u32 = 200;
    pub const GUI_START: u32 = 201;
    pub const W_NEW: u32 = 202;
    pub const W_CHECKED: u32 = 203;
    pub const W_UV: u32 = 204;
    pub const W_CB: u32 = 205;
    pub const W_SIZE: u32 = 206;
    pub const W_SLIDER: u32 = 207;
    pub const W_COMBO_ADD: u32 = 208;
    pub const W_COMBO_CLEAR: u32 = 209;
    pub const W_COMBO_SEL: u32 = 210;
    pub const W_COMBO_GET: u32 = 211;
    pub const W_COMBO_TEXT: u32 = 212;
    pub const W_TEXT: u32 = 213;
    pub const W_VIS: u32 = 214;
    pub const W_EN: u32 = 215;
    pub const W_DESTROY: u32 = 216;
    pub const W_FONT: u32 = 217;
    pub const GUI_VIRT_W: u32 = 218;
    pub const FONT_LEN: u32 = 219;
    pub const PICKER: u32 = 220;
    pub const PICKED: u32 = 221;
    pub const SET_ACTIVE: u32 = 222;
    pub const GUI_FOCUS: u32 = 223;
    pub const CFG_DEBUG: u32 = 225;
    pub const IN_STATE: u32 = 230;
    pub const LOCK: u32 = 231;
    pub const SPEED: u32 = 232;
    pub const FREECAM: u32 = 233;
    pub const FREECAM_SPEED: u32 = 234;
    pub const OCCL_SET: u32 = 235;
    pub const OCCL_GET: u32 = 236;
    pub const RESLOG_SET: u32 = 237;
    pub const RESLOG_GET: u32 = 238;
    pub const FASTPHYS_SET: u32 = 239;
    pub const FASTPHYS_GET: u32 = 240;
    pub const INS_COUNT: u32 = 241;
    pub const INS_NAME: u32 = 242;
    pub const INS_RELOAD: u32 = 243;
    pub const INS_START: u32 = 244;
    pub const START_COUNT: u32 = 245;
    pub const START_NAME: u32 = 246;
    pub const PLACE: u32 = 247;
    pub const RES_RELOAD: u32 = 250;
    pub const TRANSL: u32 = 251;
    pub const RECOMPILE: u32 = 252;
    pub const MUSIC_STOP: u32 = 253;
    pub const PLAYER_SET: u32 = 255;
    pub const MAP_FILE: u32 = 256;
    pub const BASE_RESET: u32 = 257;
    pub const DESTROY_UNUSED: u32 = 258;
    pub const START_GAME: u32 = 259;
    pub const CACHE_NEW: u32 = 260;
    pub const CACHE_FREE: u32 = 261;
    pub const MENU_SCREEN: u32 = 262;
    pub const MAP_SAVE: u32 = 263;
    pub const MAP_DESTROY: u32 = 264;
    pub const SAVED_ROUNDTRIP: u32 = 265;
    pub const MAP_LOAD: u32 = 266;
    pub const MAP_SETCUR: u32 = 267;
    pub const MAP_LOADSAVED: u32 = 268;
    pub const MAP_RUN: u32 = 269;
    pub const READ_FILE: u32 = 270;
    pub const CONT_ROOT: u32 = 280;
    pub const DYN_REBUILD: u32 = 281;
    pub const NODE_UPDATE: u32 = 282;
    pub const NODE_PARENT: u32 = 283;
    pub const NODE_BOUND: u32 = 284;
    pub const NODE_NCHILD: u32 = 285;
    pub const NODE_CHILD: u32 = 286;
    pub const NODE_NOBJ: u32 = 287;
    pub const NODE_OBJ: u32 = 288;
    pub const OBJ_IS_SUB: u32 = 290;
    pub const OBJ_NAME: u32 = 291;
    pub const OBJ_ETYPE: u32 = 292;
    pub const OBJ_BOUND: u32 = 293;
    pub const LINE_BV: u32 = 294;
    pub const OBJ_BV_HIT: u32 = 295;
    pub const OBJ_LINE_TRI: u32 = 296;
    pub const MESH_NAME: u32 = 297;
    pub const MESH_MAT: u32 = 298;
    pub const MESH_TEX: u32 = 299;
    pub const RENDER_WIRE: u32 = 300;
    pub const RENDER_PHYS: u32 = 301;
    pub const TEX_PATH: u32 = 302;
    pub const DRAW_FMT: u32 = 305;
    pub const DRAW_TEXT: u32 = 306;
    pub const SCREEN_H: u32 = 307;
    pub const Q: u32 = 310;
    pub const QS: u32 = 311;
    pub const STATE_DRAW: u32 = 312;
    pub const ENT_SNAP: u32 = 313;
    pub const ENT_ACTIVE: u32 = 314;
    pub const ENT_DRAW: u32 = 315;
    pub const SND_SNAP: u32 = 316;
    pub const SND_V: u32 = 317;
    pub const SND_S: u32 = 318;
    pub const LOG: u32 = 320;
    pub const LOG_CB: u32 = 321;
}

/// `Q` / `QS` selectors; the host mirrors them in `LuxDebugHost.cpp`.
mod q {
    pub const ON_GROUND: i32 = 0;
    pub const MOVED: i32 = 1;
    pub const MASS: i32 = 2;
    pub const ACTIVE_SIZE: i32 = 3;
    pub const SPEED_FWD: i32 = 4;
    pub const SPEED_RIGHT: i32 = 5;
    pub const VEL_LEN: i32 = 6;
    pub const INTERACT_MUL: i32 = 7;
    pub const AVG_SPEED: i32 = 8;
    pub const CLIMBING: i32 = 9;
    pub const MOVE_NORMAL: i32 = 10;
    pub const CROUCH: i32 = 11;
    pub const RUN: i32 = 12;
    pub const MOVE_MUL: i32 = 13;
    pub const RUN_MUL: i32 = 14;
    pub const EXT_LIGHT: i32 = 15;
    pub const NORM_LIGHT: i32 = 16;
    pub const HEALTH: i32 = 17;
    pub const TERROR: i32 = 18;
    pub const OIL: i32 = 19;
    pub const SANITY: i32 = 20;
    pub const TINDER: i32 = 21;
    pub const STATE: i32 = 22;
    pub const ANIM: i32 = 23;
    pub const ANIM_TIME: i32 = 24;
    pub const ANIM_REL: i32 = 25;
    pub const HEADBOB: i32 = 26;
    pub const NEW_EVENT: i32 = 27;
    pub const LOW_SANITY: i32 = 28;
    pub const HARD: i32 = 29;
    pub const FRAME_MS: i32 = 40;
    pub const FPS: i32 = 41;
    pub const MUSIC: i32 = 50;
    pub const MUSIC_VOL: i32 = 51;
    pub const MUSIC_PLAYING: i32 = 52;
    pub const MUSIC_PRIO: i32 = 53;
    pub const MUSIC_ELAPSED: i32 = 54;
    pub const MUSIC_TOTAL: i32 = 55;
    pub const MUSIC_UNDERRUN: i32 = 56;
    pub const CAM_POS: i32 = 60;
    pub const CAM_FWD: i32 = 63;
    pub const POS: i32 = 70;
    pub const PITCH: i32 = 73;
    pub const YAW: i32 = 74;
    pub const S_POS: i32 = 0;
    pub const S_FORCE_VEL: i32 = 1;
    pub const S_HAND_ANIM: i32 = 2;
}

const W_WINDOW: i32 = 0;
const W_FRAME: i32 = 1;
const W_GROUP: i32 = 2;
const W_CHECK: i32 = 3;
const W_BUTTON: i32 = 4;
const W_COMBO: i32 = 5;
const W_SLIDER: i32 = 6;
const W_LABEL: i32 = 7;

const MSG_CHECK: i32 = 0;
const MSG_BUTTON: i32 = 1;
const MSG_SLIDER: i32 = 2;

pub const PICK_WORLD: i32 = -1;
pub const PICK_BATCH: i32 = -2;

#[derive(Clone, Copy, Debug, PartialEq)]
enum Act {
    Change,
    PrintCont,
    RebuildDyn,
    LevelReload,
    QuickReload,
    TestSave,
    LoadWorld,
    Teleport,
    ReloadInsanity,
    StartInsanity,
    Recompile,
    CloseScript,
    BatchLoad,
}

/// Widget ids, the callback table and what the C++ held in the handler members.
#[derive(Clone, Debug)]
pub struct Ui {
    window: i32,
    script_window: i32,
    script_frame: i32,
    cb_starts: i32,
    cb_insanity: i32,
    cb_ff: i32,
    script_labels: Vec<i32>,
    inspect: u64,
    cur_path: Vec<u32>,
    acts: Vec<(i32, Act)>,
}

impl Default for Ui {
    fn default() -> Self {
        Self {
            window: -1,
            script_window: -1,
            script_frame: -1,
            cb_starts: -1,
            cb_insanity: -1,
            cb_ff: -1,
            script_labels: Vec::new(),
            inspect: 0,
            cur_path: Vec::new(),
            acts: Vec::new(),
        }
    }
}

fn st<R>(d: *mut Debug, f: impl FnOnce(&mut Debug) -> R) -> Option<R> {
    // SAFETY: `d` is the live handle of `ohpl_ga9_debug_new`; the borrow ends with `f`, before
    // any host call that can come back in.
    unsafe { d.as_mut() }.map(f)
}

fn wide(s: &str) -> Vec<u32> {
    string::utf8_to_wide(s.as_bytes())
}

/// C `%f`-style text of a double.
fn cf(x: f64, prec: usize) -> String {
    if x.is_nan() {
        return if x.is_sign_negative() { "-nan" } else { "nan" }.into();
    }
    if x.is_infinite() {
        return if x < 0.0 { "-inf" } else { "inf" }.into();
    }
    format!("{x:.prec$}")
}

fn vec3_str(v: [f32; 3]) -> String {
    format!(
        "{} : {} : {}",
        cf(f64::from(v[0]), 6),
        cf(f64::from(v[1]), 6),
        cf(f64::from(v[2]), 6)
    )
}

fn pixel_format(f: i32) -> &'static str {
    const NAMES: [&str; 27] = [
        "Unknown",
        "Alpha",
        "Luminance",
        "LuminanceALpha",
        "RGB",
        "RGBA",
        "BGR",
        "BGRA",
        "DXT1",
        "DXT2",
        "DXT3",
        "DXT4",
        "DXT5",
        "Unknown",
        "Depth16",
        "Depth24",
        "Depth32",
        "Alpha16",
        "Luminance16",
        "LuminanceAlpha16",
        "RGB16",
        "RGBA16",
        "Alpha32",
        "Luminance32",
        "LuminanceAlpha32",
        "RGB32",
        "RGBA32",
    ];
    usize::try_from(f)
        .ok()
        .and_then(|i| NAMES.get(i))
        .copied()
        .unwrap_or("Unknown")
}

fn cam3(h: &Host, base: i32) -> [f32; 3] {
    [0, 1, 2].map(|i| h.go(dop::Q, &[ai(i64::from(base + i))]).d as f32)
}

fn qd(h: &Host, k: i32) -> f64 {
    h.go(dop::Q, &[ai(i64::from(k))]).d
}

fn qs(h: &Host, k: i32) -> Vec<u32> {
    h.go(dop::QS, &[ai(i64::from(k))]).wide
}

fn log(h: &Host, text: &str) {
    h.go(dop::LOG, &[astr(text.as_bytes())]);
}

struct Maker<'a> {
    h: &'a Host,
    acts: Vec<(i32, Act)>,
    size: [f32; 2],
}

impl Maker<'_> {
    fn new_widget(
        &self,
        kind: i32,
        p: [f32; 3],
        s: [f32; 2],
        parent: i32,
        text: &[u32],
        extra: i32,
    ) -> i32 {
        self.h.i(
            dop::W_NEW,
            &[
                ai(i64::from(kind)),
                af(p[0]),
                af(p[1]),
                af(p[2]),
                af(s[0]),
                af(s[1]),
                ai(i64::from(parent)),
                aw(text),
                ai(i64::from(extra)),
            ],
        ) as i32
    }

    fn on(&mut self, id: i32, msg: i32, act: Act) {
        self.h
            .go(dop::W_CB, &[ai(i64::from(id)), ai(i64::from(msg))]);
        self.acts.push((id, act));
    }

    fn check(
        &mut self,
        p: [f32; 3],
        group: i32,
        text: &str,
        checked: bool,
        gen_cb: bool,
        uv: i32,
    ) -> i32 {
        let id = self.new_widget(W_CHECK, p, self.size, group, &wide(text), 0);
        self.h.go(
            dop::W_CHECKED,
            &[
                ai(i64::from(id)),
                ai(i64::from(checked)),
                ai(i64::from(gen_cb)),
            ],
        );
        self.h
            .go(dop::W_UV, &[ai(i64::from(id)), ai(i64::from(uv))]);
        self.on(id, MSG_CHECK, Act::Change);
        id
    }

    fn button(&mut self, p: [f32; 3], group: i32, text: &str, act: Act) -> i32 {
        let id = self.new_widget(W_BUTTON, p, self.size, group, &wide(text), 0);
        self.on(id, MSG_BUTTON, act);
        id
    }

    fn group(&self, pos: [f32; 3], text: &str, parent: i32) -> i32 {
        self.new_widget(W_GROUP, pos, [100.0, 100.0], parent, &wide(text), 0)
    }

    fn close_group(&self, group: i32, size_x: f32, group_y: f32, pos_y: &mut f32) {
        let gy = group_y + 15.0;
        self.h
            .go(dop::W_SIZE, &[ai(i64::from(group)), af(size_x), af(gy)]);
        *pos_y += gy + 15.0;
    }
}

const STEP: f32 = 22.0;

/// `CreateGuiWindow`.
fn create_gui_window(d: *mut Debug, h: &Host) {
    if !debug_menu(h) {
        return;
    }
    let f = st(d, |d| d.f).unwrap_or_default();
    let virt_w = h.f(dop::GUI_VIRT_W, &[]);
    let mut m = Maker {
        h,
        acts: Vec::new(),
        size: [0.0; 2],
    };

    let win_size = [250.0f32, 780.0];
    let group_x = win_size[0] - 20.0;
    let win_pos = [virt_w - win_size[0] - 10.0, 10.0, 0.0];
    let window = m.new_widget(W_WINDOW, win_pos, win_size, -1, &wide("Debug Toolbar"), 0);

    let size = [win_size[0] - 30.0, 18.0];
    m.size = size;
    let mut pos = [10.0f32, 30.0, 0.1];

    // Debug texts
    let mut gp = [5.0f32, 10.0, 0.1];
    let group = m.group(pos, "Debug texts", window);
    let occl = h.b(dop::OCCL_GET, &[]);
    let reslog = h.b(dop::RESLOG_GET, &[]);
    let rows: [(&str, bool, bool, i32); 10] = [
        ("Show FPS", f.show_fps, true, 0),
        ("Show player info", f.show_player_info, true, 1),
        ("Show entity info", f.show_entity_info, true, 2),
        ("Show sounds playing", f.show_sound_playing, true, 3),
        ("Show debug messages", f.show_debug_messages, true, 4),
        ("Show errors and warnings", f.show_error_messages, true, 12),
        ("Inspection mode", f.inspection_mode, true, 5),
        ("Occlusion Culling", occl, true, 6),
        ("Draw physics debug", f.draw_physics, true, 11),
        ("Resource Logging", reslog, false, 7),
    ];
    for (text, on, gen_cb, uv) in rows {
        m.check(gp, group, text, on, gen_cb, uv);
        gp[1] += STEP;
    }
    m.button(gp, group, "Print Container Debug Info", Act::PrintCont);
    gp[1] += STEP;
    m.button(gp, group, "Rebuild Dyn Container", Act::RebuildDyn);
    gp[1] += STEP;
    m.close_group(group, group_x, gp[1], &mut pos[1]);

    // Map
    gp = [5.0, 10.0, 0.1];
    let group = m.group(pos, "Map", window);
    m.button(gp, group, "Load Map", Act::LoadWorld);
    gp[1] += STEP;
    m.button(gp, group, "Recompile Script and Lang", Act::Recompile);
    gp[1] += STEP;
    m.check(
        gp,
        group,
        "Reload from current pos",
        f.reload_from_current_position,
        false,
        8,
    );
    gp[1] += STEP;
    let fast = h.b(dop::FASTPHYS_GET, &[]);
    m.check(gp, group, "Fast Physics Load", fast, false, 9);
    gp[1] += STEP;
    m.button(gp, group, "Reload Map", Act::LevelReload);
    gp[1] += STEP;
    m.button(gp, group, "Quick Map Reload (F2)", Act::QuickReload);
    gp[1] += STEP;
    m.button(gp, group, "Test Change Map Save", Act::TestSave);
    gp[1] += STEP;
    m.button(gp, group, "Teleport to start pos", Act::Teleport);
    gp[1] += STEP;
    let cb_starts = m.new_widget(W_COMBO, gp, size, group, &wide("None"), 0);
    h.go(dop::W_COMBO_SEL, &[ai(i64::from(cb_starts)), ai(-1)]);
    gp[1] += STEP;
    m.button(gp, group, "Map Batch", Act::BatchLoad);
    gp[1] += STEP;
    m.close_group(group, group_x, gp[1], &mut pos[1]);

    // Game
    gp = [5.0, 10.0, 0.1];
    let group = m.group(pos, "Game", window);
    m.check(
        gp,
        group,
        "Disable Flashbacks",
        f.disable_flashbacks,
        false,
        10,
    );
    gp[1] += STEP;
    m.button(gp, group, "Reload Insanity Effects", Act::ReloadInsanity);
    gp[1] += STEP;
    m.button(gp, group, "Start Insanity Effect", Act::StartInsanity);
    gp[1] += STEP;
    let cb_insanity = m.new_widget(W_COMBO, gp, size, group, &[], 0);
    fill_insanity(h, cb_insanity);
    gp[1] += STEP;
    let cb_ff = m.check(gp, group, "Fast Forward (F3)", f.fast_forward, false, 17);
    gp[1] += STEP;
    m.check(gp, group, "Fly camera", false, true, 13);
    gp[1] += STEP;
    let slider = m.new_widget(W_SLIDER, gp, size, group, &wide("Fly camera speed"), 100);
    h.go(dop::W_SLIDER, &[ai(i64::from(slider)), ai(10), ai(0)]);
    h.go(dop::W_UV, &[ai(i64::from(slider)), ai(14)]);
    m.on(slider, MSG_SLIDER, Act::Change);
    gp[1] += STEP;
    m.close_group(group, group_x, gp[1], &mut pos[1]);

    let acts = std::mem::take(&mut m.acts);
    st(d, |d| {
        d.ui.window = window;
        d.ui.cb_starts = cb_starts;
        d.ui.cb_insanity = cb_insanity;
        d.ui.cb_ff = cb_ff;
        d.ui.acts.extend(acts);
    });
    let (script_window, script_frame, script_acts) = create_script_window(h);
    st(d, |d| {
        d.ui.script_window = script_window;
        d.ui.script_frame = script_frame;
        d.ui.acts.extend(script_acts);
    });
}

/// Bit 0: `mbLoadDebugMenu`, bit 1: `mbFullscreen`.
fn cfg_debug(h: &Host) -> i64 {
    h.i(dop::CFG_DEBUG, &[])
}

fn debug_menu(h: &Host) -> bool {
    cfg_debug(h) & 1 != 0
}

fn fill_insanity(h: &Host, combo: i32) {
    let n = h.i(dop::INS_COUNT, &[]);
    for i in 0..n {
        let name = string::utf8_to_wide(&h.go(dop::INS_NAME, &[ai(i)]).bytes);
        h.go(dop::W_COMBO_ADD, &[ai(i64::from(combo)), aw(&name)]);
    }
    if n > 0 {
        h.go(dop::W_COMBO_SEL, &[ai(i64::from(combo)), ai(0)]);
    }
}

/// `CreateScriptOutputWindow`.
fn create_script_window(h: &Host) -> (i32, i32, Vec<(i32, Act)>) {
    let mut m = Maker {
        h,
        acts: Vec::new(),
        size: [0.0; 2],
    };
    let size = [700.0f32, 550.0];
    let virt_w = h.f(dop::GUI_VIRT_W, &[]);
    let pos = [virt_w / 2.0 - size[0] / 2.0, 50.0, 10.0];
    let window = m.new_widget(W_WINDOW, pos, size, -1, &wide("Script output"), 0);
    let frame = m.new_widget(
        W_FRAME,
        [10.0, 30.0, 1.0],
        [size[0] - 20.0, size[1] - 80.0],
        window,
        &[],
        0b111,
    );
    let gsize = [110.0f32, 30.0];
    let gpos = [
        size[0] / 2.0 - gsize[0] / 2.0,
        size[1] - gsize[1] - 10.0,
        1.0,
    ];
    let close = m.new_widget(W_BUTTON, gpos, gsize, window, &wide("Close Window"), 0);
    m.on(close, MSG_BUTTON, Act::CloseScript);
    h.go(dop::W_VIS, &[ai(i64::from(window)), ai(0)]);
    h.go(dop::W_EN, &[ai(i64::from(window)), ai(0)]);
    (window, frame, m.acts)
}

/// `CreateScriptOutputWindowText`.
fn script_text(d: *mut Debug, h: &Host, text: &[u32]) {
    if !debug_menu(h) {
        return;
    }
    let (labels, frame) = st(d, |d| {
        (std::mem::take(&mut d.ui.script_labels), d.ui.script_frame)
    })
    .unwrap_or_default();
    for id in labels {
        h.go(dop::W_DESTROY, &[ai(i64::from(id))]);
    }
    let font = 14.0f32;
    let mut y = 10.0f32;
    let mut made = Vec::new();
    for row in string::split(text, Some(&[u32::from(b'\n')]), true) {
        let width = h.f(dop::FONT_LEN, &[af(font), aw(row)]);
        let id = h.i(
            dop::W_NEW,
            &[
                ai(i64::from(W_LABEL)),
                af(5.0),
                af(y),
                af(1.0),
                af(width),
                af(16.0),
                ai(i64::from(frame)),
                aw(row),
                ai(0),
            ],
        ) as i32;
        h.go(dop::W_FONT, &[ai(i64::from(id)), af(font)]);
        made.push(id);
        y += font + 2.0;
    }
    st(d, |d| d.ui.script_labels = made);
}

fn show_script_output(d: *mut Debug, h: &Host, name: &str, text: &[u8]) {
    script_text(d, h, &string::utf8_to_wide(text));
    let win = st(d, |d| d.ui.script_window).unwrap_or(-1);
    let mut title = wide("Script output for ");
    title.extend(wide(name));
    h.go(dop::W_TEXT, &[ai(i64::from(win)), aw(&title)]);
    h.go(dop::W_EN, &[ai(i64::from(win)), ai(1)]);
    h.go(dop::W_VIS, &[ai(i64::from(win)), ai(1)]);
}

fn recompile(d: *mut Debug, h: &Host) -> bool {
    const STAGES: [&str; 3] = ["Map", "Inventory", "Global"];
    for (kind, name) in STAGES.iter().enumerate() {
        let r = h.go(dop::RECOMPILE, &[ai(kind as i64)]);
        if r.i == 0 {
            show_script_output(d, h, name, &r.bytes);
            return false;
        }
    }
    true
}

fn reload_translations(h: &Host) {
    h.go(dop::TRANSL, &[]);
}

fn restore_player(d: *mut Debug, h: &Host, pos: [f32; 3], pitch: f32, yaw: f32) {
    if st(d, |d| d.f.reload_from_current_position) == Some(true) {
        h.go(
            dop::PLAYER_SET,
            &[
                af(pitch),
                af(yaw),
                af(0.0),
                af(pos[0]),
                af(pos[1]),
                af(pos[2]),
            ],
        );
    }
}

fn player_pose(h: &Host) -> ([f32; 3], f32, f32) {
    (
        cam3(h, q::POS),
        qd(h, q::PITCH) as f32,
        qd(h, q::YAW) as f32,
    )
}

fn reload_map(d: *mut Debug, h: &Host) {
    h.go(dop::RES_RELOAD, &[]);
    reload_translations(h);
    if !recompile(d, h) {
        return;
    }
    h.go(dop::MUSIC_STOP, &[ai(10)]);
    let (pos, pitch, yaw) = player_pose(h);
    let file = h.go(dop::MAP_FILE, &[]).bytes;
    h.go(dop::BASE_RESET, &[]);
    h.go(dop::DESTROY_UNUSED, &[]);
    h.go(dop::START_GAME, &[astr(&file)]);
    restore_player(d, h, pos, pitch, yaw);
}

fn quick_reload(d: *mut Debug, h: &Host) {
    h.go(dop::CACHE_NEW, &[]);
    reload_map(d, h);
    h.go(dop::CACHE_FREE, &[]);
}

fn test_change_map_save(d: *mut Debug, h: &Host) {
    h.go(dop::MENU_SCREEN, &[]);
    let (pos, pitch, yaw) = player_pose(h);
    h.go(dop::CACHE_NEW, &[]);
    let file = h.go(dop::MAP_FILE, &[]).bytes;
    h.go(dop::MAP_SAVE, &[]);
    h.go(dop::MAP_DESTROY, &[]);
    h.go(
        dop::SAVED_ROUNDTRIP,
        &[aw(&wide("_debug_test_change_map.save"))],
    );
    h.go(dop::MAP_LOAD, &[astr(&file)]);
    h.go(dop::MAP_SETCUR, &[]);
    h.go(dop::MAP_LOADSAVED, &[]);
    h.go(dop::MAP_RUN, &[astr(b"OnEnter()")]);
    h.go(dop::CACHE_FREE, &[]);
    restore_player(d, h, pos, pitch, yaw);
}

fn load_batch_file(d: *mut Debug, h: &Host, path: &[u32]) {
    let data = h.go(dop::READ_FILE, &[aw(path)]).bytes;
    st(d, |d| d.load_batch(&data));
}

fn set_fast_forward(d: *mut Debug, h: &Host, on: bool) {
    let Some(speed) = st(d, |d| d.set_fast_forward(on)).flatten() else {
        return;
    };
    let (cb, checked) = st(d, |d| (d.ui.cb_ff, d.f.fast_forward)).unwrap_or((-1, false));
    if cb >= 0 {
        h.go(
            dop::W_CHECKED,
            &[ai(i64::from(cb)), ai(i64::from(checked)), ai(0)],
        );
    }
    h.go(dop::SPEED, &[af(speed)]);
}

fn change_text(d: *mut Debug, h: &Host, uv: i32, val: i32) {
    match st(d, |d| d.change_text(uv, val)).unwrap_or(Toggle::None) {
        Toggle::None => {}
        Toggle::OcclusionCulling(b) => {
            h.go(dop::OCCL_SET, &[ai(i64::from(b))]);
        }
        Toggle::LogCreateAndDelete(b) => {
            h.go(dop::RESLOG_SET, &[ai(i64::from(b))]);
        }
        Toggle::FastPhysicsLoad(b) => {
            h.go(dop::FASTPHYS_SET, &[ai(i64::from(b))]);
        }
        Toggle::FreeCam(b) => {
            h.go(dop::FREECAM, &[ai(i64::from(b))]);
        }
        Toggle::FreeCamSpeed(v) => {
            h.go(dop::FREECAM_SPEED, &[af(v)]);
        }
        Toggle::FastForward(b) => set_fast_forward(d, h, b),
    }
}

fn picker(d: *mut Debug, h: &Host, kind: i32) {
    let path = st(d, |d| d.ui.cur_path.clone()).unwrap_or_default();
    h.go(dop::PICKER, &[ai(i64::from(kind)), aw(&path)]);
}

fn picked(h: &Host) -> Option<Vec<u32>> {
    let r = h.go(dop::PICKED, &[]);
    (r.i > 0).then_some(r.wide)
}

fn dyn_root(h: &Host, which: i64) -> i64 {
    h.i(dop::CONT_ROOT, &[ai(which)])
}

fn node_bound(h: &Host, n: i64, k: i64) -> f32 {
    h.f(dop::NODE_BOUND, &[ai(n), ai(k)])
}

fn node_box(h: &Host, n: i64) -> ([f32; 3], [f32; 3]) {
    (
        [0, 1, 2].map(|k| node_bound(h, n, k)),
        [3, 4, 5].map(|k| node_bound(h, n, k)),
    )
}

fn obj_box(h: &Host, o: i64) -> ([f32; 3], [f32; 3]) {
    let b = |k: i64| h.f(dop::OBJ_BOUND, &[ai(o), ai(k)]);
    ([b(0), b(1), b(2)], [b(3), b(4), b(5)])
}

fn children(h: &Host, n: i64) -> Vec<i64> {
    let c = h.i(dop::NODE_NCHILD, &[ai(n)]);
    (0..c)
        .map(|i| h.i(dop::NODE_CHILD, &[ai(n), ai(i)]))
        .collect()
}

fn objects(h: &Host, n: i64) -> Vec<i64> {
    let c = h.i(dop::NODE_NOBJ, &[ai(n)]);
    (0..c)
        .map(|i| h.i(dop::NODE_OBJ, &[ai(n), ai(i)]))
        .collect()
}

fn tab(level: usize) -> String {
    "\t".repeat(level)
}

fn output_container(h: &Host, node: i64, level: usize) {
    let (mn, mx) = node_box(h, node);
    log(
        h,
        &format!(
            "{}-- Node {} AABB: ({})-({})  ------\n",
            tab(level),
            node as i32,
            vec3_str(mn),
            vec3_str(mx)
        ),
    );
    let objs = objects(h, node);
    if !objs.is_empty() {
        log(h, &format!("{}Objects:\n", tab(level)));
        for o in objs {
            let name = h.go(dop::OBJ_NAME, &[ai(o)]).bytes;
            let ty = h.go(dop::OBJ_ETYPE, &[ai(o)]).bytes;
            let (bmin, bmax) = obj_box(h, o);
            log(
                h,
                &format!(
                    "{} {} ({}) AABB: ({})-({})\n",
                    tab(level),
                    String::from_utf8_lossy(&name),
                    String::from_utf8_lossy(&ty),
                    vec3_str(bmin),
                    vec3_str(bmax)
                ),
            );
        }
    }
    for c in children(h, node) {
        output_container(h, c, level + 1);
    }
    log(h, &format!("{}--------\n", tab(level)));
}

// NaN boxes pass, as in the C++.
#[allow(clippy::neg_cmp_op_on_partial_ord)]
fn inside_box(o: ([f32; 3], [f32; 3]), mn: [f32; 3], mx: [f32; 3]) -> bool {
    // The C++ reads the object's min for both ends of the box; kept.
    let off = 0.001f32;
    o.0.iter().zip(&mn).all(|(e, m)| !(e + off < *m))
        && o.0.iter().zip(&mx).all(|(e, m)| !(e - off > *m))
}

fn outside_amount(o: ([f32; 3], [f32; 3]), mn: [f32; 3], mx: [f32; 3]) -> [f32; 3] {
    let mut amount = [0.0f32; 3];
    for ((a, e), m) in amount.iter_mut().zip(&o.0).zip(&mn) {
        if e - 0.001 < *m {
            *a = e - m;
        }
    }
    for ((a, e), m) in amount.iter_mut().zip(&o.0).zip(&mx) {
        if e + 0.001 > *m {
            *a = e - m;
        }
    }
    amount
}

fn check_container_bugs(h: &Host, node: i64) {
    h.go(dop::NODE_UPDATE, &[ai(node)]);
    for o in objects(h, node) {
        let ob = obj_box(h, o);
        let mut check = node;
        let mut level = 0;
        while check != 0 && h.i(dop::NODE_PARENT, &[ai(check)]) != 0 {
            let (mn, mx) = node_box(h, check);
            if !inside_box(ob, mn, mx) {
                let name = h.go(dop::OBJ_NAME, &[ai(o)]).bytes;
                log(
                    h,
                    &format!(
                        " Object: '{}' is outside node {} AABB Amount: {}!\n",
                        String::from_utf8_lossy(&name),
                        level,
                        vec3_str(outside_amount(ob, mn, mx))
                    ),
                );
                break;
            }
            level += 1;
            check = h.i(dop::NODE_PARENT, &[ai(check)]);
        }
    }
    for c in children(h, node) {
        check_container_bugs(h, c);
    }
}

fn container_debug_info(h: &Host) {
    let root = dyn_root(h, 0);
    log(
        h,
        "---------- BEGIN DYNAMIC CONTAINER OUTPUT ---------------\n",
    );
    output_container(h, root, 0);
    log(
        h,
        "---------- END DYNAMIC CONTAINER OUTPUT ---------------\n",
    );
    log(h, "---------- BEGIN CHECK DYNAMIC BUGS ---------------\n");
    check_container_bugs(h, root);
    log(h, "---------- STOP CHECK DYNAMIC BUGS ---------------\n");
}

fn v3(a: [f32; 3]) -> Vector3f {
    Vector3f {
        x: a[0],
        y: a[1],
        z: a[2],
    }
}

struct Line {
    start: [f32; 3],
    end: [f32; 3],
    bmin: [f32; 3],
    bmax: [f32; 3],
    min_t: f32,
    mesh: u64,
}

fn line_object(h: &Host, l: &mut Line, o: i64) {
    if !h.b(dop::OBJ_BV_HIT, &[ai(o)]) {
        return;
    }
    let (mn, mx) = obj_box(h, o);
    let mut t = 0.0f32;
    if !point_in_aabb(v3(l.start), v3(mn), v3(mx)) {
        let (hit, shortest) =
            aabb_line_intersection(v3(mn), v3(mx), v3(l.start), v3(l.end), &mut None);
        t = shortest;
        if !hit || t > l.min_t {
            return;
        }
    }
    let r = h.go(
        dop::OBJ_LINE_TRI,
        &[
            ai(o),
            af(l.start[0]),
            af(l.start[1]),
            af(l.start[2]),
            af(l.end[0]),
            af(l.end[1]),
            af(l.end[2]),
            af(t),
        ],
    );
    if r.i == 0 || r.f > l.min_t {
        return;
    }
    l.min_t = r.f;
    l.mesh = o as u64;
}

fn line_node(h: &Host, l: &mut Line, node: i64) {
    h.go(dop::NODE_UPDATE, &[ai(node)]);
    if h.i(dop::NODE_PARENT, &[ai(node)]) != 0 {
        let (nmin, nmax) = node_box(h, node);
        if !aabb_intersection(v3(nmin), v3(nmax), v3(l.bmin), v3(l.bmax)) {
            return;
        }
        if !point_in_aabb(v3(l.start), v3(nmin), v3(nmax)) {
            let (hit, t) =
                aabb_line_intersection(v3(nmin), v3(nmax), v3(l.start), v3(l.end), &mut None);
            if !hit || t > l.min_t {
                return;
            }
        }
    }
    for o in objects(h, node) {
        if h.b(dop::OBJ_IS_SUB, &[ai(o)]) {
            line_object(h, l, o);
        }
    }
    for c in children(h, node) {
        line_node(h, l, c);
    }
}

fn update_inspection(d: *mut Debug, h: &Host) {
    if st(d, |d| d.f.inspection_mode) != Some(true) {
        return;
    }
    let start = cam3(h, q::CAM_POS);
    let fwd = cam3(h, q::CAM_FWD);
    let (end, mn, mx) = inspection_line(start, fwd);
    h.go(
        dop::LINE_BV,
        &[
            af(mn[0]),
            af(mn[1]),
            af(mn[2]),
            af(mx[0]),
            af(mx[1]),
            af(mx[2]),
        ],
    );
    let mut l = Line {
        start,
        end,
        bmin: mn,
        bmax: mx,
        min_t: 9999.0,
        mesh: 0,
    };
    for which in 0..2 {
        let root = dyn_root(h, which);
        line_node(h, &mut l, root);
    }
    st(d, |d| d.ui.inspect = l.mesh);
}

fn draw(h: &Host, x: f32, y: f32, z: f32, text: &str) {
    h.go(dop::DRAW_FMT, &[af(x), af(y), af(z), aw(&wide(text))]);
}

fn draw_w(h: &Host, x: f32, y: f32, z: f32, text: &[u32]) {
    h.go(dop::DRAW_FMT, &[af(x), af(y), af(z), aw(text)]);
}

fn lines(s: &[u32]) -> String {
    String::from_utf8_lossy(&string::wide_to_utf8(s)).into_owned()
}

fn draw_player_info(h: &Host, mut y: f32) -> f32 {
    let g = |k: i32| qd(h, k);
    let pos = lines(&qs(h, q::S_POS));
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!(
            "Position: {} OnGround: {} Moved: {} Mass: {} ActiveSize: {}\n",
            pos,
            g(q::ON_GROUND) as i32,
            g(q::MOVED) as i32,
            cf(g(q::MASS), 6),
            g(q::ACTIVE_SIZE) as i32
        ),
    );
    y += 15.0;
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!(
            "MoveSpeed: {}, {} ({}) Mul: {} AvgSpeed: {}\n",
            cf(g(q::SPEED_FWD), 6),
            cf(g(q::SPEED_RIGHT), 6),
            cf(g(q::VEL_LEN), 6),
            cf(g(q::INTERACT_MUL), 6),
            cf(g(q::AVG_SPEED), 6)
        ),
    );
    y += 15.0;
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!("Climbing: {}\n", g(q::CLIMBING) as i32),
    );
    y += 15.0;
    let normal = g(q::MOVE_NORMAL) != 0.0;
    if normal {
        draw(
            h,
            5.0,
            y,
            10.0,
            &format!(
                "Crouching: {} Running: {} MoveSpeedMul {} RunSpeedMul {}\n",
                g(q::CROUCH) as i32,
                g(q::RUN) as i32,
                cf(g(q::MOVE_MUL), 6),
                cf(g(q::RUN_MUL), 6)
            ),
        );
        y += 15.0;
    }
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!("ForceVel: {}\n", lines(&qs(h, q::S_FORCE_VEL))),
    );
    y += 15.0;
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!(
            "ExtLightLevel: {} NormalLightlevel: {}\n",
            cf(g(q::EXT_LIGHT), 6),
            cf(g(q::NORM_LIGHT), 6)
        ),
    );
    y += 15.0;
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!(
            "Health: {} Terror: {}\n",
            cf(g(q::HEALTH), 6),
            cf(g(q::TERROR), 6)
        ),
    );
    y += 15.0;
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!(
            "Oil: {} Sanity: {} Tinderboxes: {}",
            cf(g(q::OIL), 6),
            cf(g(q::SANITY), 6),
            g(q::TINDER) as i32
        ),
    );
    y += 15.0;
    draw(h, 5.0, y, 10.0, &format!("State: {}\n", g(q::STATE) as i32));
    y += 15.0;
    let anim = qs(h, q::S_HAND_ANIM);
    if g(q::ANIM) != 0.0 {
        draw(
            h,
            5.0,
            y,
            10.0,
            &format!(
                "HandAnim: '{}' Time: {}/{}\n",
                lines(&anim),
                cf(g(q::ANIM_TIME), 6),
                cf(g(q::ANIM_REL), 6)
            ),
        );
        y += 15.0;
    }
    if normal {
        let bob = g(q::HEADBOB) as f32;
        draw(
            h,
            5.0,
            y,
            10.0,
            &format!(
                "HeadBob: {} cos: {} sin: {}\n",
                cf(f64::from(bob), 6),
                cf(f64::from(bob.cos()), 6),
                cf(f64::from(bob.sin()), 6)
            ),
        );
        y += 15.0;
    }
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!(
            "Sanity Between event time: {} AtLowSanityCount: {}\n",
            cf(g(q::NEW_EVENT), 6),
            cf(g(q::LOW_SANITY), 6)
        ),
    );
    y += 15.0;
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!("Hardcore mode: {} ", g(q::HARD) as i32),
    );
    y += 15.0;
    h.f(dop::STATE_DRAW, &[af(y)])
}

fn draw_entities(h: &Host, mut y: f32) -> f32 {
    let n = h.i(dop::ENT_SNAP, &[]);
    for i in 0..n {
        if h.b(dop::ENT_ACTIVE, &[ai(i)]) {
            y = h.f(dop::ENT_DRAW, &[ai(i), af(y)]);
        }
    }
    y
}

fn draw_sounds(h: &Host, mut y: f32) -> f32 {
    let g = |k: i32| qd(h, k);
    if g(q::MUSIC) != 0.0 {
        y += 5.0;
        let under = if g(q::MUSIC_UNDERRUN) != 0.0 {
            "*BUFFER UNDERRUN!!*"
        } else {
            ""
        };
        draw(
            h,
            5.0,
            y,
            10.0,
            &format!(
                "Music: '{}' vol: {} playing: {} prio: {} elapsed: {} total time: {} {}",
                lines(&qs(h, q::MUSIC)),
                cf(g(q::MUSIC_VOL), 2),
                g(q::MUSIC_PLAYING) as i32,
                g(q::MUSIC_PRIO) as i32,
                cf(g(q::MUSIC_ELAPSED), 2),
                cf(g(q::MUSIC_TOTAL), 2),
                under
            ),
        );
        y += 15.0;
    }
    let n = h.i(dop::SND_SNAP, &[]);
    draw(h, 5.0, y, 10.0, &format!("Num of sounds: {}", n as i32));
    y += 15.0;
    let (mut row, mut col) = (0i32, 0i32);
    for i in 0..n {
        let v = |k: i64| h.go(dop::SND_V, &[ai(i), ai(k)]).d;
        let name = h.go(dop::SND_S, &[ai(i)]).wide;
        let mut text = lines(&name);
        if v(0) != 0.0 {
            text.push_str("*st*");
        }
        if v(1) != 0.0 {
            text.push_str("BUFFER UNDERRUN!!");
        }
        text.push_str(&format!(
            "({})({}) ({}/{})",
            cf(v(2), 2),
            v(3) as i32,
            cf(v(4), 2),
            cf(v(5), 2)
        ));
        draw(
            h,
            (col * 250) as f32,
            (row as f32).mul_add(15.0, y),
            10.0,
            &text,
        );
        col += 1;
        if col == 3 {
            col = 0;
            row += 1;
        }
    }
    if n > 0 {
        y = 15.0f32.mul_add((row + 1) as f32, y);
    }
    y
}

fn draw_inspect(h: &Host, mesh: u64, mut y: f32) {
    let m = mesh as i64;
    let name = h.go(dop::MESH_NAME, &[ai(m)]).bytes;
    draw(
        h,
        5.0,
        y,
        10.0,
        &format!("EntityName: '{}'", String::from_utf8_lossy(&name)),
    );
    y += 15.0;
    let mat = h.go(dop::MESH_MAT, &[ai(m)]).wide;
    let mut t = wide("MaterialFile: '");
    t.extend(mat);
    t.push(u32::from(b'\''));
    draw_w(h, 5.0, y, 10.0, &t);
    y += 15.0;
    draw(h, 5.0, y, 10.0, "---------------------");
    y += 15.0;
    for i in 0..h.i(dop::MESH_TEX, &[ai(m), ai(-1), ai(0)]) {
        let r = |k: i64| h.go(dop::MESH_TEX, &[ai(m), ai(i), ai(k)]);
        if r(0).i == 0 {
            continue;
        }
        let mem = r(4).f;
        let mut t = wide(&format!(
            " {} size: {} x {} x {} mem: {}mb type: {} file: '",
            i,
            r(1).i as i32,
            r(2).i as i32,
            r(3).i as i32,
            cf(f64::from(mem), 2),
            pixel_format(r(5).i as i32)
        ));
        t.extend(h.go(dop::TEX_PATH, &[ai(m), ai(i)]).wide);
        t.push(u32::from(b'\''));
        draw_w(h, 5.0, y, 10.0, &t);
        y += 15.0;
    }
}

fn on_draw(d: *mut Debug, h: &Host) {
    let Some(f) = st(d, |d| d.f) else {
        return;
    };
    let mut y = 5.0f32;
    if f.show_fps {
        draw(
            h,
            5.0,
            y,
            10.0,
            &format!(
                "FrameTime: {}ms FPS: {}\n",
                cf(qd(h, q::FRAME_MS), 1),
                cf(qd(h, q::FPS), 1)
            ),
        );
        y += 13.0;
    }
    if f.show_debug_messages || f.show_error_messages {
        let mut my = h.f(dop::SCREEN_H, &[]) - 40.0;
        let msgs = st(d, |d| {
            d.messages
                .iter()
                .map(|m| m.text.clone())
                .collect::<Vec<_>>()
        })
        .unwrap_or_default();
        for text in msgs {
            h.go(dop::DRAW_TEXT, &[af(5.0), af(my), af(10.0), aw(&text)]);
            my -= 17.0;
        }
    }
    if f.show_player_info {
        y = draw_player_info(h, y);
    }
    if f.show_entity_info {
        y = draw_entities(h, y);
    }
    if f.show_sound_playing {
        y = draw_sounds(h, y);
    }
    let mesh = st(d, |d| d.ui.inspect).unwrap_or(0);
    if f.inspection_mode && mesh != 0 {
        draw_inspect(h, mesh, y);
    }
}

fn render_solid(d: *mut Debug, h: &Host) {
    let Some(f) = st(d, |d| d.f) else {
        return;
    };
    let mesh = st(d, |d| d.ui.inspect).unwrap_or(0);
    if f.inspection_mode && mesh != 0 {
        h.go(dop::RENDER_WIRE, &[ai(mesh as i64)]);
    }
    if f.draw_physics {
        h.go(dop::RENDER_PHYS, &[]);
    }
}

fn set_window_active(h: &Host, active: bool) {
    let cfg = cfg_debug(h);
    let Some((_, lock)) = Debug::window_change(cfg & 1 != 0, active, cfg & 2 != 0) else {
        return;
    };
    if active {
        log(h, "-------- Debug window open!\n");
        h.go(dop::GUI_FOCUS, &[ai(1)]);
        h.go(dop::SET_ACTIVE, &[ai(1)]);
        h.go(dop::IN_STATE, &[ai(0)]);
        if lock {
            h.go(dop::LOCK, &[ai(0)]);
        }
    } else {
        log(h, "-------- Debug window closed!\n");
        h.go(dop::GUI_FOCUS, &[ai(0)]);
        h.go(dop::SET_ACTIVE, &[ai(0)]);
        h.go(dop::IN_STATE, &[ai(1)]);
        if lock {
            h.go(dop::LOCK, &[ai(1)]);
        }
    }
}

fn act(d: *mut Debug, h: &Host, a: Act, uv: i32, val: i32) {
    match a {
        Act::Change => change_text(d, h, uv, val),
        Act::PrintCont => container_debug_info(h),
        Act::RebuildDyn => {
            log(
                h,
                "---------------- REBUILDING DYNAMIC --------------------\n",
            );
            h.go(dop::DYN_REBUILD, &[]);
        }
        Act::LevelReload => reload_map(d, h),
        Act::QuickReload => quick_reload(d, h),
        Act::TestSave => test_change_map_save(d, h),
        Act::LoadWorld => picker(d, h, 0),
        Act::BatchLoad => picker(d, h, 1),
        Act::Teleport => {
            let cb = st(d, |d| d.ui.cb_starts).unwrap_or(-1);
            let sel = h.i(dop::W_COMBO_GET, &[ai(i64::from(cb))]);
            if sel < 0 {
                return;
            }
            let name = h.go(dop::W_COMBO_TEXT, &[ai(i64::from(cb)), ai(sel)]).wide;
            h.go(dop::PLACE, &[aw(&name)]);
        }
        Act::Recompile => {
            if recompile(d, h) {
                reload_translations(h);
            }
        }
        Act::ReloadInsanity => {
            let cb = st(d, |d| d.ui.cb_insanity).unwrap_or(-1);
            h.go(dop::W_COMBO_CLEAR, &[ai(i64::from(cb))]);
            h.go(dop::W_COMBO_SEL, &[ai(i64::from(cb)), ai(-1)]);
            h.go(dop::INS_RELOAD, &[]);
            fill_insanity(h, cb);
        }
        Act::StartInsanity => {
            let cb = st(d, |d| d.ui.cb_insanity).unwrap_or(-1);
            let sel = h.i(dop::W_COMBO_GET, &[ai(i64::from(cb))]);
            if sel >= 0 {
                h.go(dop::INS_START, &[ai(sel)]);
            }
        }
        Act::CloseScript => {
            let w = st(d, |d| d.ui.script_window).unwrap_or(-1);
            h.go(dop::W_VIS, &[ai(i64::from(w)), ai(0)]);
            h.go(dop::W_EN, &[ai(i64::from(w)), ai(0)]);
        }
    }
}

fn picked_world(d: *mut Debug, h: &Host) {
    let Some(p) = picked(h) else {
        return;
    };
    st(d, |d| d.ui.cur_path = string::file_path(&p).to_vec());
    let file = string::wide_to_utf8(string::file_name(&p));
    h.go(dop::BASE_RESET, &[]);
    h.go(dop::START_GAME, &[astr(&file)]);
}

fn picked_batch(d: *mut Debug, h: &Host) {
    if let Some(p) = picked(h) {
        load_batch_file(d, h, &p);
    }
}

fn map_enter(d: *mut Debug, h: &Host) {
    st(d, |d| {
        d.on_map_enter();
        d.ui.inspect = 0;
    });
    let cb = st(d, |d| d.ui.cb_starts).unwrap_or(-1);
    if cb >= 0 {
        h.go(dop::W_COMBO_CLEAR, &[ai(i64::from(cb))]);
        h.go(dop::W_COMBO_SEL, &[ai(i64::from(cb)), ai(-1)]);
        let n = h.i(dop::START_COUNT, &[]);
        for i in 0..n {
            let name = string::utf8_to_wide(&h.go(dop::START_NAME, &[ai(i)]).bytes);
            h.go(dop::W_COMBO_ADD, &[ai(i64::from(cb)), aw(&name)]);
        }
        if n > 0 {
            h.go(dop::W_COMBO_SEL, &[ai(i64::from(cb)), ai(0)]);
        }
    }
}

fn map_leave(d: *mut Debug, h: &Host) {
    st(d, |d| d.ui.inspect = 0);
    let cb = st(d, |d| d.ui.cb_starts).unwrap_or(-1);
    if cb >= 0 {
        h.go(dop::W_COMBO_CLEAR, &[ai(i64::from(cb))]);
        h.go(dop::W_COMBO_SEL, &[ai(i64::from(cb)), ai(-1)]);
    }
}

fn update(d: *mut Debug, h: &Host, dt: f32) {
    let Some((_, due)) = st(d, |d| d.update_tick()) else {
        return;
    };
    if due {
        h.go(dop::CACHE_NEW, &[]);
        let map = st(d, |d| d.batch_pop()).unwrap_or_default();
        h.go(dop::BASE_RESET, &[]);
        h.go(dop::START_GAME, &[astr(&map)]);
        h.go(dop::CACHE_FREE, &[]);
    }
    update_inspection(d, h);
    st(d, |d| d.update_messages(dt));
}

fn with_host(f: impl FnOnce(Host)) {
    if let Some(h) = host() {
        f(h);
    }
}

pub fn after_load_user_config(d: *mut Debug) {
    with_host(|h| {
        h.go(dop::LOG_CB, &[ai(1)]);
        create_gui_window(d, &h);
    });
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_init(_d: *mut Debug) {
    with_host(|h| {
        h.go(dop::GUI_INIT, &[ai(cfg_debug(&h) & 1)]);
    });
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_shutdown(_d: *mut Debug) {
    with_host(|h| {
        h.go(dop::LOG_CB, &[ai(0)]);
    });
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_on_start(_d: *mut Debug) {
    with_host(|h| {
        h.go(dop::GUI_START, &[]);
    });
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_update(d: *mut Debug, dt: f32) {
    with_host(|h| update(d, &h, dt));
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_reset(d: *mut Debug) {
    st(d, |d| {
        d.reset();
        d.ui.inspect = 0;
    });
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_on_map_enter(d: *mut Debug) {
    with_host(|h| map_enter(d, &h));
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_on_map_leave(d: *mut Debug) {
    with_host(|h| map_leave(d, &h));
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_set_window_active(_d: *mut Debug, active: bool) {
    with_host(|h| set_window_active(&h, active));
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_on_draw(d: *mut Debug) {
    with_host(|h| on_draw(d, &h));
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_render_solid(d: *mut Debug) {
    with_host(|h| render_solid(d, &h));
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_set_fast_forward(d: *mut Debug, on: bool) {
    with_host(|h| set_fast_forward(d, &h, on));
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_quick_reload(d: *mut Debug) {
    with_host(|h| quick_reload(d, &h));
}

/// Widget callback (`id` >= 0) or a file picker result (`PICK_*`).
///
/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_event(d: *mut Debug, id: i32, uv: i32, val: i32) {
    with_host(|h| match id {
        PICK_WORLD => picked_world(d, &h),
        PICK_BATCH => picked_batch(d, &h),
        _ => {
            let a = st(d, |d| {
                d.ui.acts.iter().find(|(i, _)| *i == id).map(|(_, a)| *a)
            })
            .flatten();
            if let Some(a) = a {
                act(d, &h, a, uv, val);
            }
        }
    });
}

/// Widget id of the debug window (0) or the script output window (1).
///
/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_root(d: *mut Debug, which: i32) -> i32 {
    st(d, |d| {
        if which == 0 {
            d.ui.window
        } else {
            d.ui.script_window
        }
    })
    .unwrap_or(-1)
}

/// The inspected mesh entity (host pointer, 0 for none).
///
/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dbg_inspect(d: *mut Debug) -> u64 {
    st(d, |d| d.ui.inspect).unwrap_or(0)
}

#[cfg(test)]
#[path = "tests_debug_ui.rs"]
mod tests;
