//! S5c: `cSomaLuxGame` (`SomaLuxGame.cpp`): the handler, user-module and effect lists, the
//! per-frame dispatch order, action routing, `Load` (game.cfg, Effects/Modules/EntityTypes/
//! PlayerStates) and the game-layer script natives. The scriptables, the configs, the XML
//! documents, ImGui and the engine stay C++ behind one host table (`OhplS5cOp`).

use super::game;
use super::s5_input_ffi::{Host, Io};
use crate::somascript::s2_as::{Engine, Frame, Soma, ohpl_as2_str_assign, str_bytes};
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::OnceLock;

pub mod op {
    pub const DUMP_ENABLED: u32 = 1;
    pub const DUMP_LOG: u32 = 2;
    pub const SELF_TEST: u32 = 3;
    pub const NEW: u32 = 4;
    pub const SET_SCRIPT_NAME: u32 = 5;
    pub const PLAYER_CAMERA: u32 = 6;
    pub const LOAD_SCRIPT: u32 = 7;
    pub const DELETE: u32 = 8;
    pub const SC: u32 = 9;
    pub const SCRIPT_OBJECT: u32 = 10;
    pub const CFG_STRING: u32 = 11;
    pub const CFG_FLOAT: u32 = 12;
    pub const LOAD_CONFIGS: u32 = 13;
    pub const LOAD_LANGUAGE: u32 = 14;
    pub const ADD_VOICE: u32 = 15;
    pub const PLAYER_EXISTS: u32 = 16;
    pub const INPUT_EXISTS: u32 = 17;
    pub const XML_FLAT: u32 = 18;
    pub const PLAYER_ADD_STATE: u32 = 19;
    pub const MOUSE_REL: u32 = 20;
    pub const DIALOG_UPDATE: u32 = 21;
    pub const VOICE_UPDATE: u32 = 22;
    pub const PAUSED: u32 = 23;
    pub const MAP_ACTION: u32 = 24;
    pub const UPDATE_CAMERA_TEXTURES: u32 = 25;
    pub const LEAVE_MAP: u32 = 26;
    pub const GUI_FOCUS_HISTORY: u32 = 27;
    pub const GUI_FOCUS: u32 = 28;
    pub const GUI_POINTER_MODE: u32 = 29;
    pub const GUI_SEND_MOUSE: u32 = 30;
    pub const GUI_HUD_BEGIN: u32 = 31;
    pub const GUI_MAP_ON_GUI: u32 = 32;
    pub const GUI_HUD_END: u32 = 33;
    pub const HUD_FOCUS_SET: u32 = 34;
    pub const HUD_FOCUS_GET: u32 = 35;
    pub const LOG: u32 = 36;
    pub const DUMP_ANALOG: u32 = 37;
    pub const SET_SAVED: u32 = 38;
}

pub mod sc {
    pub const UPDATE: i32 = 0;
    pub const POST_UPDATE: i32 = 1;
    pub const VARIABLE_UPDATE: i32 = 2;
    pub const GUI: i32 = 3;
    pub const DRAW: i32 = 4;
    pub const ACTION: i32 = 5;
    pub const ANALOG: i32 = 6;
    pub const MAP_MESSAGE: i32 = 7;
    pub const MESSAGE: i32 = 8;
    pub const CALL_OBJECT: i32 = 9;
}

pub mod kind {
    pub const HANDLER: i32 = 0;
    pub const PLAYER: i32 = 1;
    pub const INPUT: i32 = 2;
    pub const EFFECT: i32 = 3;
    pub const MODULE: i32 = 4;
}

/// The engine side of the game layer.
pub trait Ops {
    fn go(&self, op: u32, io: Io) -> (i32, Io);
}

struct CHost(&'static Host);

impl Ops for CHost {
    fn go(&self, op: u32, mut io: Io) -> (i32, Io) {
        // SAFETY: the table was installed by the host and `io` outlives the call.
        let r = unsafe { (self.0.call)(self.0.ctx, op, &mut io) };
        (r, io)
    }
}

static HOST: OnceLock<&'static Host> = OnceLock::new();

#[cfg(test)]
thread_local! {
    pub(super) static TEST_OPS: RefCell<Option<std::rc::Rc<dyn Ops>>> = const { RefCell::new(None) };
}

pub(super) fn with_ops<R>(f: impl FnOnce(&dyn Ops) -> R) -> Option<R> {
    #[cfg(test)]
    if let Some(h) = TEST_OPS.with(|t| t.borrow().clone()) {
        return Some(f(&*h));
    }
    HOST.get().map(|h| f(&CHost(h)))
}

fn io_s(s: &[u8]) -> Io {
    let mut io = Io::new();
    io.s = s.as_ptr();
    io.n = s.len();
    io
}

fn join(parts: &[&[u8]]) -> Vec<u8> {
    parts.join(&0u8)
}

fn call(o: &dyn Ops, op: u32) -> i32 {
    o.go(op, Io::new()).0
}

fn call_s(o: &dyn Ops, op: u32, s: &[u8]) -> i32 {
    o.go(op, io_s(s)).0
}

fn dump_on(o: &dyn Ops) -> bool {
    call(o, op::DUMP_ENABLED) != 0
}

fn dump(o: &dyn Ops, line: &str) {
    call_s(o, op::DUMP_LOG, line.as_bytes());
}

fn sc_call(o: &dyn Ops, p: usize, sub: i32, f: impl FnOnce(&mut Io)) {
    let mut io = Io::new();
    io.p[0] = p as *mut c_void;
    io.i[0] = sub;
    f(&mut io);
    o.go(op::SC, io);
}

fn sc_float(o: &dyn Ops, p: usize, sub: i32, x: f32) {
    sc_call(o, p, sub, |io| io.f[0] = x);
}

fn sc_msg(o: &dyn Ops, p: usize, decl: &[u8]) {
    sc_call(o, p, sc::MESSAGE, |io| {
        io.s = decl.as_ptr();
        io.n = decl.len();
    });
}

fn sc_map_msg(o: &dyn Ops, p: usize, decl: &[u8], map: usize) {
    sc_call(o, p, sc::MAP_MESSAGE, |io| {
        io.s = decl.as_ptr();
        io.n = decl.len();
        io.p[1] = map as *mut c_void;
    });
}

fn sc_action(o: &dyn Ops, p: usize, action: i32, pressed: bool) {
    sc_call(o, p, sc::ACTION, |io| {
        io.i[1] = action;
        io.i[2] = i32::from(pressed);
    });
}

fn sc_analog(o: &dyn Ops, p: usize, id: i32, v: [f32; 3]) {
    sc_call(o, p, sc::ANALOG, |io| {
        io.i[1] = id;
        io.f[..3].copy_from_slice(&v);
    });
}

const GAME_CFG: i32 = 2;

fn cfg_string(o: &dyn Ops, level: &[u8], key: &[u8], def: &[u8]) -> Vec<u8> {
    let s = join(&[level, key, def]);
    let mut io = io_s(&s);
    io.i[0] = GAME_CFG;
    o.go(op::CFG_STRING, io).1.text()
}

fn cfg_float(o: &dyn Ops, level: &[u8], key: &[u8], def: f32) -> f32 {
    let s = join(&[level, key]);
    let mut io = io_s(&s);
    io.i[0] = GAME_CFG;
    io.f[0] = def;
    o.go(op::CFG_FLOAT, io).1.f[0]
}

#[derive(Clone, Default)]
struct Entry {
    p: usize,
    name: Vec<u8>,
    id: i32,
    active: bool,
}

struct Game {
    handlers: Vec<Entry>,
    modules: Vec<Entry>,
    effects: Vec<Entry>,
    scripts: BTreeMap<Vec<u8>, (Vec<u8>, Vec<u8>)>,
    prop_distance: f32,
    critter_distance: f32,
    game_input: bool,
    mouse: (i32, i32),
    frame: i32,
    pointer: i32,
    mouse_moved: bool,
}

impl Game {
    fn new() -> Self {
        Self {
            handlers: Vec::new(),
            modules: Vec::new(),
            effects: Vec::new(),
            scripts: BTreeMap::new(),
            prop_distance: 2.0,
            critter_distance: 2.0,
            game_input: true,
            mouse: (0, 0),
            frame: 0,
            pointer: -1,
            mouse_moved: false,
        }
    }
}

thread_local! {
    static GAME: RefCell<Option<Game>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Game) -> R) -> Option<R> {
    GAME.with(|g| g.try_borrow_mut().ok().and_then(|mut g| g.as_mut().map(f)))
}

/// `ForEach`: handlers, effects (only active ones when `active_only`, tested as each is reached),
/// then modules. The lists are copied so a callback may call back into the game.
fn for_each(o: &dyn Ops, active_only: bool, mut f: impl FnMut(&dyn Ops, usize)) {
    let (handlers, effects, modules) = with(|g| {
        (
            g.handlers.iter().map(|e| e.p).collect::<Vec<_>>(),
            g.effects.iter().map(|e| e.p).collect::<Vec<_>>(),
            g.modules.iter().map(|e| e.p).collect::<Vec<_>>(),
        )
    })
    .unwrap_or_default();
    for p in handlers {
        f(o, p);
    }
    for p in effects {
        let active = with(|g| g.effects.iter().any(|e| e.p == p && e.active)).unwrap_or(false);
        if game::visits_effect(active, active_only) {
            f(o, p);
        }
    }
    for p in modules {
        f(o, p);
    }
}

fn new_scriptable(o: &dyn Ops, k: i32, id: i32, name: &[u8], script_name: &[u8]) -> usize {
    let mut io = io_s(name);
    io.i[0] = k;
    io.i[1] = id;
    let p = o.go(op::NEW, io).1.p[0] as usize;
    call_s_p(o, op::SET_SCRIPT_NAME, p, script_name);
    p
}

fn call_s_p(o: &dyn Ops, op: u32, p: usize, s: &[u8]) -> i32 {
    let mut io = io_s(s);
    io.p[0] = p as *mut c_void;
    o.go(op, io).0
}

fn load_script(o: &dyn Ops, p: usize, file: &[u8], class: &[u8], base: &[u8]) -> bool {
    call_s_p(o, op::LOAD_SCRIPT, p, &join(&[file, class, base])) != 0
}

fn delete(o: &dyn Ops, p: usize) {
    let mut io = Io::new();
    io.p[0] = p as *mut c_void;
    o.go(op::DELETE, io);
}

/// One element of the flat XML dump: depth, element value, attributes.
struct Elem {
    depth: u8,
    value: Vec<u8>,
    attrs: Vec<(Vec<u8>, Vec<u8>)>,
}

impl Elem {
    fn attr(&self, name: &[u8]) -> Option<&[u8]> {
        self.attrs
            .iter()
            .find(|(k, _)| k == name)
            .map(|(_, v)| v.as_slice())
    }

    fn string(&self, name: &[u8]) -> Vec<u8> {
        self.attr(name).unwrap_or_default().to_vec()
    }

    fn int(&self, name: &[u8], def: i32) -> i32 {
        self.attr(name).map_or(def, crate::string::token_to_int)
    }
}

/// `\x1e` ends a record; fields are `\x1f`: depth, value, then name/value pairs.
fn parse_flat(b: &[u8]) -> Vec<Elem> {
    b.split(|&c| c == 0x1e)
        .filter(|r| !r.is_empty())
        .filter_map(|r| {
            let mut f = r.split(|&c| c == 0x1f);
            let depth = f.next()?.first().map(|d| d - b'0')?;
            let value = f.next()?.to_vec();
            let rest: Vec<&[u8]> = f.collect();
            let attrs = rest
                .as_chunks::<2>()
                .0
                .iter()
                .map(|[k, v]| (k.to_vec(), v.to_vec()))
                .collect();
            Some(Elem {
                depth,
                value,
                attrs,
            })
        })
        .collect()
}

fn xml_flat(o: &dyn Ops, path: &[u8], require_file: bool) -> Option<Vec<Elem>> {
    let mut io = io_s(path);
    io.i[0] = i32::from(require_file);
    let (r, io) = o.go(op::XML_FLAT, io);
    (r > 0).then(|| parse_flat(&io.text()))
}

/// Top-level elements with their child elements.
fn groups(elems: &[Elem]) -> Vec<(&Elem, Vec<&Elem>)> {
    let mut v: Vec<(&Elem, Vec<&Elem>)> = Vec::new();
    for e in elems {
        if e.depth == 0 {
            v.push((e, Vec::new()));
        } else if let Some(g) = v.last_mut() {
            g.1.push(e);
        }
    }
    v
}

const HANDLERS: [(&str, &str, &str); 7] = [
    ("InputHandler", "cScrInputHandler", "cLuxInputHandler"),
    (
        "EventDatabaseHandler",
        "cScrEventDatabaseHandler",
        "cLuxEventDatabaseHandler",
    ),
    ("GuiHandler", "cScrGuiHandler", "cLuxGuiHandler"),
    (
        "AchievementHandler",
        "cScrAchievementHandler",
        "iLuxAchievementHandler",
    ),
    (
        "HeroStatsHandler",
        "cScrHeroStatsHandler",
        "iLuxHeroStatsHandler",
    ),
    (
        "RichPresenceHandler",
        "cScrRichPresenceHandler",
        "iLuxRichPresenceHandler",
    ),
    ("Player", "cScrPlayer", "cLuxPlayer"),
];

fn load_handlers(o: &dyn Ops) {
    for (attr, class, base) in HANDLERS {
        let file = cfg_string(o, b"ScriptFiles", attr.as_bytes(), b"");
        if file.is_empty() {
            continue;
        }
        let k = match attr {
            "Player" => kind::PLAYER,
            "InputHandler" => kind::INPUT,
            _ => kind::HANDLER,
        };
        let script_name = if k == kind::PLAYER { "LuxPlayer" } else { attr };
        let p = new_scriptable(o, k, -1, attr.as_bytes(), script_name.as_bytes());
        if k == kind::PLAYER {
            call_s_p(o, op::PLAYER_CAMERA, p, b"");
        }
        let cls = cfg_string(
            o,
            b"ScriptFiles",
            format!("{attr}Class").as_bytes(),
            class.as_bytes(),
        );
        if load_script(o, p, &file, &cls, base.as_bytes()) {
            let e = Entry {
                p,
                name: attr.as_bytes().to_vec(),
                id: -1,
                active: false,
            };
            with(|g| g.handlers.push(e));
        } else {
            delete(o, p);
        }
    }
}

fn load_player_states(o: &dyn Ops) {
    if call(o, op::PLAYER_EXISTS) == 0 {
        return;
    }
    let Some(rows) = xml_flat(o, b"config/PlayerStates.cfg", true) else {
        return;
    };
    for (group, elems) in groups(&rows) {
        for e in elems {
            let s = join(&[
                &e.string(b"Name"),
                &e.string(b"ScriptFile"),
                &e.string(b"ScriptClass"),
            ]);
            let mut io = io_s(&s);
            io.i[0] = e.int(b"ID", -1);
            io.i[1] = i32::from(group.value == b"MoveStates");
            o.go(op::PLAYER_ADD_STATE, io);
        }
    }
}

fn load_list(o: &dyn Ops, path: &[u8], k: i32, base: &[u8], only: Option<&[u8]>) {
    let rows = xml_flat(o, path, false).unwrap_or_default();
    for (elem, _) in groups(&rows) {
        if only.is_some_and(|v| elem.value != v) {
            continue;
        }
        let name = elem.string(b"Name");
        let id = elem.int(b"ID", -1);
        let p = new_scriptable(o, k, id, &name, &name);
        if load_script(
            o,
            p,
            &elem.string(b"ScriptFile"),
            &elem.string(b"ScriptClass"),
            base,
        ) {
            let e = Entry {
                p,
                name,
                id,
                active: false,
            };
            with(|g| {
                if k == kind::EFFECT {
                    g.effects.push(e);
                } else {
                    g.modules.push(e);
                }
            });
        } else {
            delete(o, p);
        }
    }
}

fn load_entity_types(o: &dyn Ops) {
    let rows = xml_flat(o, b"config/EntityTypes.cfg", false).unwrap_or_default();
    for (group, types) in groups(&rows) {
        for t in types {
            let mut key = group.value.clone();
            key.push(b':');
            key.extend_from_slice(&t.string(b"Name"));
            let v = (t.string(b"ScriptFile"), t.string(b"ScriptClass"));
            with(|g| g.scripts.insert(key, v));
        }
    }
}

fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

fn dump_lists(o: &dyn Ops) {
    let (h, e, m, s) = with(|g| {
        (
            g.handlers.clone(),
            g.effects.clone(),
            g.modules.clone(),
            g.scripts.clone(),
        )
    })
    .unwrap_or_default();
    for x in h {
        dump(o, &format!("gh {}", lossy(&x.name)));
    }
    for x in e {
        dump(o, &format!("ge {} {}", x.id, lossy(&x.name)));
    }
    for x in m {
        dump(o, &format!("gm {} {}", x.id, lossy(&x.name)));
    }
    for (k, (f, c)) in s {
        dump(o, &format!("gs {} {} {}", lossy(&k), lossy(&f), lossy(&c)));
    }
}

fn load_with(o: &dyn Ops) {
    call(o, op::LOAD_CONFIGS);
    call(o, op::LOAD_LANGUAGE);
    call(o, op::ADD_VOICE);
    load_handlers(o);
    load_player_states(o);
    let prop = cfg_float(o, b"Prop", b"DefaultMaxInteractDistance", 2.0);
    let critter = cfg_float(o, b"Critter", b"DefaultMaxInteractDistance", 2.0);
    with(|g| {
        g.prop_distance = prop;
        g.critter_distance = critter;
    });
    load_list(o, b"config/Effects.cfg", kind::EFFECT, b"cLuxEffect", None);
    load_list(
        o,
        b"config/Modules.cfg",
        kind::MODULE,
        b"cLuxUserModule",
        Some(b"Module"),
    );
    load_entity_types(o);
    let (h, m, e) =
        with(|g| (g.handlers.len(), g.modules.len(), g.effects.len())).unwrap_or_default();
    call_s(
        o,
        op::LOG,
        format!("SOMA script: {h} handlers, {m} user modules, {e} effects\n").as_bytes(),
    );
    if call(o, op::INPUT_EXISTS) != 0 {
        super::s5_input_ffi::ohpl_s5_in_load_user_config();
        super::s5_input_ffi::ohpl_s5_in_load_script();
        if dump_on(o) {
            super::s5_input_ffi::ohpl_s5_in_dump();
        }
    }
    reload_user_config(o);
    if dump_on(o) {
        dump_lists(o);
    }
    for_each(o, false, |o, p| sc_msg(o, p, b"void OnStart()"));
    for_each(o, false, |o, p| sc_msg(o, p, b"void Reset()"));
}

fn reload_user_config(o: &dyn Ops) {
    for_each(o, false, |o, p| sc_msg(o, p, b"void LoadUserConfig()"));
}

fn handler_ptr(name: &[u8]) -> usize {
    with(|g| {
        g.handlers
            .iter()
            .find(|e| e.name == name)
            .map_or(0, |e| e.p)
    })
    .unwrap_or(0)
}

fn update_gui(o: &dyn Ops, dt: f32) {
    call(o, op::GUI_FOCUS_HISTORY);
    let focus = o.go(op::GUI_FOCUS, Io::new()).1;
    let (present, is3d) = (focus.i[0] != 0, focus.i[1] != 0);
    let cur = with(|g| g.pointer).unwrap_or(-1);
    if let Some(rel) = game::pointer_mode(cur, present, present && is3d) {
        with(|g| g.pointer = i32::from(rel));
        let mut io = Io::new();
        io.i[0] = i32::from(rel);
        o.go(op::GUI_POINTER_MODE, io);
    }
    let focus = o.go(op::GUI_FOCUS, Io::new()).1;
    if focus.i[0] != 0 {
        let gui = handler_ptr(b"GuiHandler");
        if gui != 0 {
            sc_call(o, gui, sc::CALL_OBJECT, |io| {
                const DECL: &[u8] = b"void UpdateDefaultInput(cImGui @apImGui)";
                io.s = DECL.as_ptr();
                io.n = DECL.len();
                io.p[1] = focus.p[0];
            });
        }
        let moved = with(|g| {
            g.mouse_moved = game::pointer_moved(g.mouse_moved, g.mouse.0, g.mouse.1);
            (g.mouse_moved, g.mouse)
        })
        .unwrap_or_default();
        let mut io = Io::new();
        io.p[0] = focus.p[0];
        io.i[0] = i32::from(moved.0);
        io.i[1] = moved.1.0;
        io.i[2] = moved.1.1;
        o.go(op::GUI_SEND_MOUSE, io);
    }
    let mut io = Io::new();
    io.f[0] = dt;
    o.go(op::GUI_HUD_BEGIN, io);
    for_each(o, true, |o, p| sc_float(o, p, sc::GUI, dt));
    let mut io = Io::new();
    io.f[0] = dt;
    o.go(op::GUI_MAP_ON_GUI, io);
    call(o, op::GUI_HUD_END);
}

fn update_with(o: &dyn Ops, dt: f32, paused: bool) {
    let (mut mx, mut my) = {
        let io = o.go(op::MOUSE_REL, Io::new()).1;
        (io.i[0], io.i[1])
    };
    if dump_on(o) {
        let frame = with(|g| {
            g.frame += 1;
            g.frame
        })
        .unwrap_or(0);
        if (200..260).contains(&frame) {
            mx = if frame.rem_euclid(7) == 0 {
                0
            } else {
                (frame * 5).rem_euclid(23) - 11
            };
            my = if frame.rem_euclid(5) == 0 {
                0
            } else {
                (frame * 3).rem_euclid(17) - 8
            };
        }
    }
    let input = with(|g| {
        g.mouse = (mx, my);
        g.game_input
    })
    .unwrap_or(true);
    if call(o, op::INPUT_EXISTS) != 0 {
        super::s5_input_ffi::ohpl_s5_in_latch();
        super::s5_input_ffi::ohpl_s5_in_update(input);
    }
    if !paused {
        for_each(o, true, |o, p| sc_float(o, p, sc::UPDATE, dt));
    }
    let mut io = Io::new();
    io.f[0] = dt;
    o.go(op::VOICE_UPDATE, io);
    let mut io = Io::new();
    io.f[0] = dt;
    o.go(op::DIALOG_UPDATE, io);
    update_gui(o, dt);
}

fn post_update_with(o: &dyn Ops, dt: f32) {
    for_each(o, true, |o, p| sc_float(o, p, sc::POST_UPDATE, dt));
    for_each(o, true, |o, p| sc_float(o, p, sc::VARIABLE_UPDATE, dt));
    call(o, op::UPDATE_CAMERA_TEXTURES);
}

fn broadcast_action_with(o: &dyn Ops, action: i32, pressed: bool) {
    if dump_on(o) {
        dump(o, &format!("act {action} {}", i32::from(pressed)));
    }
    let paused = call(o, op::PAUSED) != 0;
    if game::route_action(paused) == game::Route::ModulesOnly {
        let modules =
            with(|g| g.modules.iter().map(|e| e.p).collect::<Vec<_>>()).unwrap_or_default();
        for p in modules {
            sc_action(o, p, action, pressed);
        }
        return;
    }
    for_each(o, true, |o, p| sc_action(o, p, action, pressed));
    let mut io = Io::new();
    io.i[0] = action;
    io.i[1] = i32::from(pressed);
    o.go(op::MAP_ACTION, io);
}

fn broadcast_analog_with(o: &dyn Ops, id: i32, v: [f32; 3]) {
    if dump_on(o) {
        let mut io = Io::new();
        io.i[0] = id;
        io.f[..3].copy_from_slice(&v);
        o.go(op::DUMP_ANALOG, io);
    }
    if !game::analog_allowed(call(o, op::PAUSED) != 0) {
        return;
    }
    for_each(o, true, |o, p| sc_analog(o, p, id, v));
}

fn leave_map_with(o: &dyn Ops, map: usize) {
    call(o, op::LEAVE_MAP);
    for_each(o, false, |o, p| {
        sc_map_msg(o, p, b"void OnMapLeave(cLuxMap @apMap)", map)
    });
    for_each(o, false, |o, p| {
        sc_map_msg(o, p, b"void DestroyWorldEntities(cLuxMap @apMap)", map)
    });
}

fn free_with(o: &dyn Ops) {
    let g = GAME.with(|g| g.try_borrow_mut().ok().and_then(|mut g| g.take()));
    if let Some(g) = g {
        for e in g.modules.iter().chain(&g.effects).chain(&g.handlers) {
            delete(o, e.p);
        }
    }
}

// C ABI

/// # Safety
/// `host` points at a table that lives for the whole process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5c_game_install_host(host: *const Host) {
    // SAFETY: caller contract.
    if let Some(h) = unsafe { host.as_ref() } {
        let _ = HOST.set(h);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_new() {
    GAME.with(|g| {
        if let Ok(mut g) = g.try_borrow_mut() {
            *g = Some(Game::new());
        }
    });
    with_ops(|o| {
        if dump_on(o) {
            dump(o, "begin");
            call(o, op::SELF_TEST);
        }
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_free() {
    with_ops(free_with);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_load() {
    with_ops(load_with);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_update(dt: f32, paused: bool) {
    with_ops(|o| update_with(o, dt, paused));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_post_update(dt: f32) {
    with_ops(|o| post_update_with(o, dt));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_draw(dt: f32) {
    with_ops(|o| for_each(o, true, |o, p| sc_float(o, p, sc::DRAW, dt)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_reset_scriptables() {
    with_ops(|o| for_each(o, false, |o, p| sc_msg(o, p, b"void Reset()")));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_reload_user_config() {
    with_ops(reload_user_config);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_preload(map: *mut c_void) {
    with_ops(|o| {
        for_each(o, false, |o, p| {
            sc_map_msg(o, p, b"void PreloadData(cLuxMap @apMap)", map as usize)
        })
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_enter_map(map: *mut c_void) {
    with_ops(|o| {
        for_each(o, false, |o, p| {
            sc_map_msg(
                o,
                p,
                b"void CreateWorldEntities(cLuxMap @apMap)",
                map as usize,
            )
        });
        for_each(o, false, |o, p| {
            sc_map_msg(o, p, b"void OnMapEnter(cLuxMap @apMap)", map as usize)
        });
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_modules_map_enter(map: *mut c_void) {
    with_ops(|o| {
        let modules =
            with(|g| g.modules.iter().map(|e| e.p).collect::<Vec<_>>()).unwrap_or_default();
        for p in modules {
            sc_map_msg(o, p, b"void OnMapEnter(cLuxMap @apMap)", map as usize);
        }
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_leave_map(map: *mut c_void) {
    with_ops(|o| leave_map_with(o, map as usize));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_broadcast_action(action: i32, pressed: bool) {
    with_ops(|o| broadcast_action_with(o, action, pressed));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_broadcast_analog(id: i32, x: f32, y: f32, z: f32) {
    with_ops(|o| broadcast_analog_with(o, id, [x, y, z]));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_module_by_id(id: i32) -> *mut c_void {
    module_by_id(id) as *mut c_void
}

fn module_by_id(id: i32) -> usize {
    with(|g| g.modules.iter().find(|e| e.id == id).map_or(0, |e| e.p)).unwrap_or(0)
}

fn module_by_name(name: &[u8]) -> usize {
    with(|g| g.modules.iter().find(|e| e.name == name).map_or(0, |e| e.p)).unwrap_or(0)
}

fn effect_by_id(id: i32) -> usize {
    with(|g| g.effects.iter().find(|e| e.id == id).map_or(0, |e| e.p)).unwrap_or(0)
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5c_game_module_by_name(name: *const u8, n: usize) -> *mut c_void {
    // SAFETY: caller contract.
    module_by_name(unsafe { crate::string_ffi::units(name, n) }) as *mut c_void
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_effect_by_id(id: i32) -> *mut c_void {
    effect_by_id(id) as *mut c_void
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5c_game_handler(name: *const u8, n: usize) -> *mut c_void {
    // SAFETY: caller contract.
    handler_ptr(unsafe { crate::string_ffi::units(name, n) }) as *mut c_void
}

type ScriptSink = unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize);

/// # Safety
/// The strings are readable for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5c_game_entity_script(
    group: *const u8,
    gn: usize,
    ty: *const u8,
    tn: usize,
    sink: ScriptSink,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let mut key = unsafe { crate::string_ffi::units(group, gn) }.to_vec();
    key.push(b':');
    // SAFETY: caller contract.
    key.extend_from_slice(unsafe { crate::string_ffi::units(ty, tn) });
    let Some((f, c)) = with(|g| g.scripts.get(&key).cloned()).flatten() else {
        return false;
    };
    // SAFETY: caller contract; no borrow is held.
    unsafe { sink(ctx, f.as_ptr(), f.len(), c.as_ptr(), c.len()) };
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_interact_distance(entity_type: i32) -> f32 {
    with(|g| {
        if entity_type == 4 {
            g.critter_distance
        } else {
            g.prop_distance
        }
    })
    .unwrap_or(2.0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5c_game_set_input(on: bool) {
    with(|g| g.game_input = on);
}

/// # Safety
/// `x` and `y` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5c_game_mouse_rel(x: *mut i32, y: *mut i32) {
    let m = with(|g| g.mouse).unwrap_or_default();
    // SAFETY: caller contract.
    unsafe {
        *x = m.0;
        *y = m.1;
    }
}

// script natives

fn text<'a>(f: &Frame, i: usize) -> &'a [u8] {
    // SAFETY: the declaration passes a `const tString &in` in slot `i`.
    unsafe { str_bytes(f.ptr_arg(i)) }
}

fn put_string(f: &Frame, i: usize, b: &[u8]) {
    // SAFETY: the declaration passes a `tString &out` in slot `i`.
    unsafe { ohpl_as2_str_assign(f.ptr_arg(i), b.as_ptr(), b.len()) }
}

/// Script objects returned as interface handles need a reference for the caller.
fn return_script(f: &Frame, p: usize) {
    let obj = if p == 0 {
        std::ptr::null_mut()
    } else {
        let mut io = Io::new();
        io.p[0] = p as *mut c_void;
        with_ops(|o| o.go(op::SCRIPT_OBJECT, io).1.p[1]).unwrap_or(std::ptr::null_mut())
    };
    f.ret_object(obj);
}

static EFFECT_HANDLER_TAG: u8 = 0;

fn handler_getter(s: &Soma, decl: &'static str, name: &'static str) {
    s.func(decl, move |f| {
        f.ret_ref(handler_ptr(name.as_bytes()) as *mut c_void);
    });
}

/// Handler and module lookups, up to `cLux_GetPlayer`.
pub fn register1(e: Engine) {
    let s = Soma(e);
    s.func(
        "iScrUserModule_Interface@ cLux_GetUserModuleFromID(int alID)",
        |f| return_script(f, module_by_id(f.arg::<i32>(0))),
    );
    s.func(
        "iScrUserModule_Interface@ cLux_GetUserModuleFromName(const tString&in asName)",
        |f| return_script(f, module_by_name(text(f, 0))),
    );
    s.method(
        "cLuxEffectHandler",
        "iScrEffect_Interface@ GetEffect(int alId)",
        |f| return_script(f, effect_by_id(f.arg::<i32>(0))),
    );
    s.func(
        "void cLux_GetTextCatAndEntryFromString(const tString&in asData, tString &out asOutCat, tString &out asOutEntry)",
        |f| {
            let mut v = crate::string::split(text(f, 0), None, true);
            put_string(f, 1, v.next().unwrap_or_default());
            put_string(f, 2, v.next().unwrap_or_default());
        },
    );
    s.func("cLuxEffectHandler@ cLux_GetEffectHandler()", |f| {
        f.ret_ref(std::ptr::addr_of!(EFFECT_HANDLER_TAG).cast_mut());
    });
    handler_getter(
        &s,
        "cLuxInputHandler@ cLux_GetInputHandler()",
        "InputHandler",
    );
    handler_getter(
        &s,
        "cLuxEventDatabaseHandler@ cLux_GetEventDatabaseHandler()",
        "EventDatabaseHandler",
    );
    handler_getter(&s, "cLuxGuiHandler@ cLux_GetGuiHandler()", "GuiHandler");
    s.method(
        "cLuxGuiHandler",
        "void SetGameHudInputFocus(bool abX)",
        |f| {
            let mut io = Io::new();
            io.i[0] = i32::from(f.arg::<bool>(0));
            with_ops(|o| o.go(op::HUD_FOCUS_SET, io));
        },
    );
    s.method("cLuxGuiHandler", "bool GetGameHudInputFocus()", |f| {
        f.ret(with_ops(|o| call(o, op::HUD_FOCUS_GET) != 0).unwrap_or(false));
    });
    handler_getter(
        &s,
        "iLuxAchievementHandler@ cLux_GetAchievementHandler()",
        "AchievementHandler",
    );
    s.method(
        "iLuxAchievementHandler",
        "bool GetAchievementUnlocked(int alID)",
        |f| f.ret(false),
    );
    handler_getter(
        &s,
        "iLuxHeroStatsHandler@ cLux_GetHeroStatsHandler()",
        "HeroStatsHandler",
    );
    handler_getter(
        &s,
        "iLuxRichPresenceHandler@ cLux_GetRichPresenceHandler()",
        "RichPresenceHandler",
    );
    handler_getter(&s, "cLuxPlayer@ cLux_GetPlayer()", "Player");
}

const TIMER_TYPES: [&str; 6] = [
    "cLuxUserModule",
    "cLuxEffect",
    "cLuxPlayer",
    "cLuxInputHandler",
    "cLuxGuiHandler",
    "cLuxEventDatabaseHandler",
];

/// Timer natives on the handler types and the id properties; the offsets are those of the C++ shells.
pub fn register2(e: Engine, module_id_off: i32, effect_id_off: i32) {
    for ty in TIMER_TYPES {
        if let Some(t) = e.type_by_name(ty.as_bytes()) {
            if t.has_method_name(b"Timer_Add") {
                continue;
            }
            super::s5_scriptable::register(e, ty);
        }
    }
    e.reg_property(b"cLuxUserModule", b"int mlId", module_id_off, 0, false);
    let s = Soma(e);
    s.method(
        "cLuxUserModule",
        "void SetScriptableIsSaved(bool abX)",
        |f| {
            let mut io = Io::new();
            io.p[0] = f.this;
            io.i[0] = i32::from(f.arg::<bool>(0));
            with_ops(|o| o.go(op::SET_SAVED, io));
        },
    );
    e.reg_property(b"cLuxEffect", b"int mlId", effect_id_off, 0, false);
}

fn effect_active(p: usize) -> bool {
    with(|g| g.effects.iter().any(|e| e.p == p && e.active)).unwrap_or(false)
}

pub fn register3(e: Engine) {
    let s = Soma(e);
    s.method("cLuxEffect", "void SetActive(bool abX)", |f| {
        let (p, on) = (f.this as usize, f.arg::<bool>(0));
        with(|g| {
            g.effects
                .iter_mut()
                .filter(|e| e.p == p)
                .for_each(|e| e.active = on)
        });
    });
    s.method("cLuxEffect", "bool IsActive()", |f| {
        f.ret(effect_active(f.this as usize));
    });
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5c_game_register1(engine: *mut c_void) {
    register1(Engine(engine));
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5c_game_register2(engine: *mut c_void, module: i32, effect: i32) {
    register2(Engine(engine), module, effect);
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5c_game_register3(engine: *mut c_void) {
    register3(Engine(engine));
}

#[cfg(test)]
#[path = "s5c_tests.rs"]
mod tests;
