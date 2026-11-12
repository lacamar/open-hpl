//! `LuxInputHandler`: the action table, default bindings, user key config, smoothing and the
//! decisions of the per-state input updates (USE_GAMEPAD with SDL2 gamepad names).

use super::cfg::Cfg;
use crate::input as key;
use crate::string;
use std::collections::VecDeque;

pub mod act {
    pub const EXIT: i32 = 0;
    pub const EXIT_DIRECT: i32 = 1;
    pub const SCREENSHOT: i32 = 2;
    pub const PRINT_INFO: i32 = 3;
    pub const LEFT_CLICK: i32 = 4;
    pub const MIDDLE_CLICK: i32 = 5;
    pub const RIGHT_CLICK: i32 = 6;
    pub const SCROLL_UP: i32 = 7;
    pub const SCROLL_DOWN: i32 = 8;
    pub const MOUSE6: i32 = 9;
    pub const MOUSE7: i32 = 10;
    pub const MOUSE8: i32 = 11;
    pub const MOUSE9: i32 = 12;
    pub const UI_UP: i32 = 13;
    pub const UI_DOWN: i32 = 14;
    pub const UI_LEFT: i32 = 15;
    pub const UI_RIGHT: i32 = 16;
    pub const UI_PRIMARY: i32 = 17;
    pub const UI_SECONDARY: i32 = 18;
    pub const UI_PREV: i32 = 19;
    pub const UI_NEXT: i32 = 20;
    pub const UI_DELETE: i32 = 21;
    pub const UI_CLEAR: i32 = 22;
    pub const OPEN_DEBUG: i32 = 23;
    pub const RELOAD_MAP: i32 = 24;
    pub const QUICK_SAVE: i32 = 25;
    pub const QUICK_LOAD: i32 = 26;
    pub const FAST_FORWARD: i32 = 27;
    pub const INVENTORY: i32 = 28;
    pub const JOURNAL: i32 = 29;
    pub const QUEST_LOG: i32 = 30;
    pub const RECENT_TEXT: i32 = 31;
    pub const CROSSHAIR: i32 = 32;
    pub const FORWARD: i32 = 33;
    pub const BACKWARD: i32 = 34;
    pub const RIGHT: i32 = 35;
    pub const LEFT: i32 = 36;
    pub const LEAN_RIGHT: i32 = 37;
    pub const LEAN_LEFT: i32 = 38;
    pub const LEAN: i32 = 39;
    pub const ATTACK: i32 = 40;
    pub const INTERACT: i32 = 41;
    pub const IGNITE: i32 = 42;
    pub const ROTATE: i32 = 43;
    pub const HOLSTER: i32 = 44;
    pub const LANTERN: i32 = 45;
    pub const RUN: i32 = 46;
    pub const JUMP: i32 = 47;
    pub const CROUCH: i32 = 48;
    pub const ZOOM_IN: i32 = 49;
    pub const ZOOM_OUT: i32 = 50;
}

use act::*;

pub const CAT_SYSTEM: i32 = 0;
pub const CAT_MOVEMENT: i32 = 1;
pub const CAT_ACTION: i32 = 2;
pub const CAT_MISC: i32 = 3;
pub const CAT_LAST: i32 = 4;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Action {
    pub name: &'static str,
    pub id: i32,
    pub configurable: bool,
    pub cat: i32,
}

const fn a(name: &'static str, id: i32, configurable: bool, cat: i32) -> Action {
    Action {
        name,
        id,
        configurable,
        cat,
    }
}

pub const ACTIONS: [Action; 51] = [
    a("Exit", EXIT, false, CAT_SYSTEM),
    a("ExitDirect", EXIT_DIRECT, false, CAT_SYSTEM),
    a("ScreenShot", SCREENSHOT, false, CAT_SYSTEM),
    a("PrintInfo", PRINT_INFO, false, CAT_SYSTEM),
    a("LeftClick", LEFT_CLICK, false, CAT_SYSTEM),
    a("MiddleClick", MIDDLE_CLICK, false, CAT_SYSTEM),
    a("RightClick", RIGHT_CLICK, false, CAT_SYSTEM),
    a("ScrollUp", SCROLL_UP, false, CAT_SYSTEM),
    a("ScrollDown", SCROLL_DOWN, false, CAT_SYSTEM),
    a("MouseButton6", MOUSE6, false, CAT_SYSTEM),
    a("MouseButton7", MOUSE7, false, CAT_SYSTEM),
    a("MouseButton8", MOUSE8, false, CAT_SYSTEM),
    a("MouseButton9", MOUSE9, false, CAT_SYSTEM),
    a("UIArrowUp", UI_UP, false, CAT_SYSTEM),
    a("UIArrowLeft", UI_LEFT, false, CAT_SYSTEM),
    a("UIArrowDown", UI_DOWN, false, CAT_SYSTEM),
    a("UIArrowRight", UI_RIGHT, false, CAT_SYSTEM),
    a("UIPrimary", UI_PRIMARY, false, CAT_SYSTEM),
    a("UISecondary", UI_SECONDARY, false, CAT_SYSTEM),
    a("UIPrevPage", UI_PREV, false, CAT_SYSTEM),
    a("UINextPage", UI_NEXT, false, CAT_SYSTEM),
    a("UIDelete", UI_DELETE, false, CAT_SYSTEM),
    a("UIClear", UI_CLEAR, false, CAT_SYSTEM),
    a("OpenDebug", OPEN_DEBUG, false, CAT_SYSTEM),
    a("ReloadMap", RELOAD_MAP, false, CAT_SYSTEM),
    a("QuickSave", QUICK_SAVE, false, CAT_SYSTEM),
    a("QuickLoad", QUICK_LOAD, false, CAT_SYSTEM),
    a("FastForward", FAST_FORWARD, false, CAT_SYSTEM),
    a("Inventory", INVENTORY, true, CAT_MISC),
    a("Journal", JOURNAL, true, CAT_MISC),
    a("QuestLog", QUEST_LOG, true, CAT_MISC),
    a("RecentText", RECENT_TEXT, true, CAT_MISC),
    a("CrosshairToggle", CROSSHAIR, true, CAT_MISC),
    a("Forward", FORWARD, true, CAT_MOVEMENT),
    a("Backward", BACKWARD, true, CAT_MOVEMENT),
    a("Right", RIGHT, true, CAT_MOVEMENT),
    a("Left", LEFT, true, CAT_MOVEMENT),
    a("LeanRight", LEAN_RIGHT, true, CAT_MOVEMENT),
    a("LeanLeft", LEAN_LEFT, true, CAT_MOVEMENT),
    a("Lean", LEAN, true, CAT_MOVEMENT),
    a("ZoomOut", ZOOM_OUT, false, CAT_ACTION),
    a("ZoomIn", ZOOM_IN, false, CAT_ACTION),
    a("Attack", ATTACK, true, CAT_ACTION),
    a("Interact", INTERACT, true, CAT_ACTION),
    a("Ignite", IGNITE, false, CAT_ACTION),
    a("Rotate", ROTATE, true, CAT_ACTION),
    a("Holster", HOLSTER, false, CAT_ACTION),
    a("Lantern", LANTERN, true, CAT_ACTION),
    a("Run", RUN, true, CAT_MOVEMENT),
    a("Crouch", CROUCH, true, CAT_MOVEMENT),
    a("Jump", JUMP, true, CAT_MOVEMENT),
];

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Input {
    pub ty: &'static str,
    pub value: i32,
    pub action: i32,
}

const fn i(ty: &'static str, value: i32, action: i32) -> Input {
    Input { ty, value, action }
}

const KB: &str = "Keyboard";
const MB: &str = "MouseButton";
const PB: &str = "GamepadButton";
const AXIS_LY: &str = "GamepadAxis.Axis LeftY";
const AXIS_LX: &str = "GamepadAxis.Axis LeftX";
const AXIS_LT: &str = "GamepadAxis.Axis LeftTrigger";
const AXIS_RT: &str = "GamepadAxis.Axis RightTrigger";
const POSITIVE: i32 = 1;
const NEGATIVE: i32 = 2;

pub const INPUTS: [Input; 76] = [
    i(KB, 5, EXIT),
    i(KB, 108, EXIT_DIRECT),
    i(KB, 104, SCREENSHOT),
    i(KB, 59, PRINT_INFO),
    i(MB, 0, LEFT_CLICK),
    i(MB, 1, MIDDLE_CLICK),
    i(MB, 2, RIGHT_CLICK),
    i(MB, 3, SCROLL_UP),
    i(MB, 4, SCROLL_DOWN),
    i(MB, 5, MOUSE6),
    i(MB, 6, MOUSE7),
    i(MB, 7, MOUSE8),
    i(MB, 8, MOUSE9),
    i(PB, 11, UI_UP),
    i(PB, 12, UI_DOWN),
    i(PB, 13, UI_LEFT),
    i(PB, 14, UI_RIGHT),
    i(AXIS_LY, NEGATIVE, UI_UP),
    i(AXIS_LX, POSITIVE, UI_RIGHT),
    i(AXIS_LY, POSITIVE, UI_DOWN),
    i(AXIS_LX, NEGATIVE, UI_LEFT),
    i(PB, 0, UI_PRIMARY),
    i(PB, 1, UI_SECONDARY),
    i(PB, 9, UI_PREV),
    i(PB, 10, UI_NEXT),
    i(AXIS_LT, POSITIVE, UI_PREV),
    i(AXIS_RT, POSITIVE, UI_NEXT),
    i(PB, 2, UI_DELETE),
    i(PB, 3, UI_CLEAR),
    i(KB, 3, UI_PRIMARY),
    i(KB, 97, OPEN_DEBUG),
    i(KB, 98, RELOAD_MAP),
    i(KB, 100, QUICK_SAVE),
    i(KB, 101, QUICK_LOAD),
    i(KB, 99, FAST_FORWARD),
    i(KB, 1, INVENTORY),
    i(KB, 53, JOURNAL),
    i(KB, 56, QUEST_LOG),
    i(KB, 57, RECENT_TEXT),
    i(KB, 67, CROSSHAIR),
    i(KB, 66, FORWARD),
    i(KB, 62, BACKWARD),
    i(KB, 47, RIGHT),
    i(KB, 44, LEFT),
    i(KB, 48, LEAN_RIGHT),
    i(KB, 60, LEAN_LEFT),
    i(KB, 120, LEAN),
    i(MB, 2, ATTACK),
    i(MB, 0, INTERACT),
    i(KB, 46, IGNITE),
    i(KB, 61, ROTATE),
    i(KB, 51, HOLSTER),
    i(KB, 49, LANTERN),
    i(KB, 116, RUN),
    i(KB, 118, CROUCH),
    i(KB, 6, JUMP),
    i(AXIS_LY, NEGATIVE, FORWARD),
    i(AXIS_LX, POSITIVE, RIGHT),
    i(AXIS_LY, POSITIVE, BACKWARD),
    i(AXIS_LX, NEGATIVE, LEFT),
    i(PB, 0, JUMP),
    i(PB, 1, CROUCH),
    i(PB, 2, LANTERN),
    i(PB, 3, JOURNAL),
    i(PB, 14, QUEST_LOG),
    i(PB, 13, RECENT_TEXT),
    i(PB, 4, INVENTORY),
    i(PB, 9, ATTACK),
    i(PB, 10, INTERACT),
    i(PB, 6, EXIT),
    i(PB, 7, CROSSHAIR),
    i(PB, 8, ROTATE),
    i(AXIS_LT, POSITIVE, RUN),
    i(AXIS_RT, POSITIVE, LEAN),
    i(PB, 11, ZOOM_OUT),
    i(PB, 12, ZOOM_IN),
];

pub const INPUT_POS: [&str; 2] = ["Primary", "Secondary"];

pub fn action_by_name(name: &[u8]) -> Option<&'static Action> {
    ACTIONS.iter().find(|a| a.name.as_bytes() == name)
}

pub fn action_by_id(id: i32) -> Option<&'static Action> {
    ACTIONS.iter().find(|a| a.id == id)
}

/// `GetActionsByCategory`; `CAT_LAST` lists everything.
pub fn actions_by_category(cat: i32) -> Vec<&'static Action> {
    ACTIONS
        .iter()
        .filter(|a| cat == CAT_LAST || a.cat == cat)
        .collect()
}

pub fn default_inputs(action: i32) -> Vec<&'static Input> {
    INPUTS.iter().filter(|x| x.action == action).collect()
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum SubAction {
    Key(i32),
    Mouse(i32),
    PadButton(i32),
    PadHat(i32, i32),
    PadAxis(i32, i32),
}

pub const KEY_LAST: i32 = 230;
pub const MOUSE_LAST: i32 = 9;
pub const PAD_BUTTON_LAST: i32 = 30;
pub const PAD_HAT_LAST: i32 = 10;
pub const PAD_AXIS_LAST: i32 = 10;
pub const PAD_AXIS_RANGE_LAST: i32 = 3;

fn parts(s: &[u8]) -> Vec<Vec<u8>> {
    string::split(s, Some(b"."), true)
        .map(<[u8]>::to_vec)
        .collect()
}

fn part(p: &[Vec<u8>], i: usize) -> &[u8] {
    p.get(i).map_or(&[], Vec::as_slice)
}

/// `CreateSubAction`. A missing part reads as empty (the C++ indexed past the vector).
pub fn create_sub_action(p: &[Vec<u8>], value: i32) -> Option<SubAction> {
    let ty = string::to_lower(part(p, 0));
    match ty.as_slice() {
        b"keyboard" => Some(SubAction::Key(value)),
        b"mousebutton" => Some(SubAction::Mouse(value)),
        _ if string::first_pos(&ty, b"gamepad") == 0 => {
            if part(p, 0) == b"GamepadButton" {
                return Some(SubAction::PadButton(value));
            }
            let hat = key::pad_hat_from_name(part(p, 1));
            let axis = key::pad_axis_from_name(part(p, 1));
            if hat != PAD_HAT_LAST {
                Some(SubAction::PadHat(hat, value))
            } else if axis != PAD_AXIS_LAST {
                Some(SubAction::PadAxis(axis, value))
            } else {
                None
            }
        }
        _ => None,
    }
}

/// `CreateSubActionFromInputString`: whether it counts as a valid input, and the sub action
/// when one was created. A "gamepad" type that is not a prefix is valid yet creates nothing, and
/// an unknown hat state keeps the not-found code 16 because it is compared with the hat enum
/// end (10).
pub fn sub_action_from_string(s: &[u8]) -> (bool, Option<SubAction>) {
    let p = parts(s);
    if p.is_empty() {
        return (false, None);
    }
    let ty = string::to_lower(part(&p, 0));
    let mut value = -1;
    match ty.as_slice() {
        b"keyboard" => {
            value = key::key_from_name(part(&p, 1));
            if value == KEY_LAST {
                value = -1;
            }
        }
        b"mousebutton" => {
            value = key::mouse_button_from_name(part(&p, 1));
            if value == MOUSE_LAST {
                value = -1;
            }
        }
        _ if string::first_pos(&ty, b"gamepad") != -1 => {
            if part(&p, 0) == b"GamepadButton" {
                value = key::pad_button_from_name(part(&p, 1));
                if value == PAD_BUTTON_LAST {
                    value = -1;
                }
            } else {
                let hat = key::pad_hat_from_name(part(&p, 1));
                let axis = key::pad_axis_from_name(part(&p, 1));
                if hat != PAD_HAT_LAST {
                    value = key::pad_hat_state_from_name(part(&p, 2));
                    if value == PAD_HAT_LAST {
                        value = -1;
                    }
                } else if axis != PAD_AXIS_LAST {
                    value = key::pad_axis_range_from_name(part(&p, 2));
                    if value == PAD_AXIS_RANGE_LAST {
                        value = -1;
                    }
                }
            }
        }
        _ => {}
    }
    if value == -1 {
        (false, None)
    } else {
        (true, create_sub_action(&p, value))
    }
}

/// The input actions of the engine, as the handler drives them.
pub trait ActionEnv {
    fn create_action(&mut self, name: &[u8], id: i32);
    fn clear_sub_actions(&mut self, id: i32);
    fn add_sub_action(&mut self, id: i32, s: SubAction);
    fn sub_action_count(&mut self, id: i32) -> usize;
    fn sub_action_input(&mut self, id: i32, index: usize) -> (Vec<u8>, Vec<u8>);
}

/// `CreateActions`: every action, then every default input.
pub fn create_actions(e: &mut dyn ActionEnv) {
    for a in &ACTIONS {
        e.create_action(a.name.as_bytes(), a.id);
    }
    for x in &INPUTS {
        if let Some(s) = create_sub_action(&parts(x.ty.as_bytes()), x.value) {
            e.add_sub_action(x.action, s);
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Settings {
    pub invert_mouse: bool,
    pub smooth_mouse: bool,
    pub mouse_sensitivity: f32,
    pub gamepad_walk_sensitivity: f32,
    pub gamepad_look_sensitivity: f32,
    pub gamepad_look_invert: bool,
}

/// `LoadUserConfig`: the settings and the key bindings of every configurable action.
pub fn load_user_config(user: &mut dyn Cfg, keys: &mut dyn Cfg, e: &mut dyn ActionEnv) -> Settings {
    let invert_mouse = user.bool(b"Input", b"InvertMouse", false);
    let smooth_mouse = user.bool(b"Input", b"SmoothMouse", true);
    let mouse_sensitivity = user.float(b"Input", b"MouseSensitivity", 1.0);
    let gamepad_walk_sensitivity = user.float(b"Input", b"GamepadWalkSensitivity", 1.0);
    let gamepad_look_sensitivity = user.float(b"Input", b"GamepadLookSensitivity", 1.5);
    let gamepad_look_invert = user.bool(b"Input", b"InvertGamepadLook", false);
    for act in ACTIONS.iter().filter(|a| a.configurable) {
        e.clear_sub_actions(act.id);
        let mut user_defined = false;
        for pos in INPUT_POS {
            let s = keys.string(act.name.as_bytes(), pos.as_bytes(), b"");
            if s.is_empty() {
                continue;
            }
            let (valid, sub) = sub_action_from_string(&s);
            if let Some(sub) = sub {
                e.add_sub_action(act.id, sub);
            }
            user_defined = valid || user_defined;
        }
        if !user_defined {
            for d in default_inputs(act.id) {
                if let Some(s) = create_sub_action(&parts(d.ty.as_bytes()), d.value) {
                    e.add_sub_action(act.id, s);
                }
            }
        }
    }
    Settings {
        invert_mouse,
        smooth_mouse,
        mouse_sensitivity,
        gamepad_walk_sensitivity,
        gamepad_look_sensitivity,
        gamepad_look_invert,
    }
}

/// `SaveUserConfig`.
pub fn save_user_config(
    s: &Settings,
    user: &mut dyn Cfg,
    keys: &mut dyn Cfg,
    e: &mut dyn ActionEnv,
) {
    user.set_bool(b"Input", b"InvertMouse", s.invert_mouse);
    user.set_bool(b"Input", b"SmoothMouse", s.smooth_mouse);
    user.set_float(b"Input", b"MouseSensitivity", s.mouse_sensitivity);
    user.set_bool(b"Input", b"InvertGamepadLook", s.gamepad_look_invert);
    user.set_float(
        b"Input",
        b"GamepadWalkSensitivity",
        s.gamepad_walk_sensitivity,
    );
    user.set_float(
        b"Input",
        b"GamepadLookSensitivity",
        s.gamepad_look_sensitivity,
    );
    for act in ACTIONS.iter().filter(|a| a.configurable) {
        let name = act.name.as_bytes();
        let have = e.sub_action_count(act.id);
        let mut j = 0;
        while j < have && j < INPUT_POS.len() {
            let (ty, input) = e.sub_action_input(act.id, j);
            let value = [ty.as_slice(), b".", &input].concat();
            if let Some(pos) = INPUT_POS.get(j) {
                keys.set_string(name, pos.as_bytes(), &value);
            }
            j += 1;
        }
        for pos in INPUT_POS.iter().skip(j) {
            keys.set_string(name, pos.as_bytes(), b"");
        }
    }
}

/// `GetSmoothMousePos`'s ring of recent mouse positions.
#[derive(Clone, Debug, Default)]
pub struct Smooth {
    pub max: i32,
    pub prev_mul: f32,
    list: VecDeque<[f32; 2]>,
}

impl Smooth {
    pub fn new(max: i32, prev_mul: f32) -> Self {
        Smooth {
            max,
            prev_mul,
            list: VecDeque::new(),
        }
    }

    pub fn reset(&mut self) {
        self.list.clear();
    }

    pub fn len(&self) -> usize {
        self.list.len()
    }

    pub fn is_empty(&self) -> bool {
        self.list.is_empty()
    }

    pub fn push(&mut self, rel: [f32; 2]) -> [f32; 2] {
        self.list.push_front(rel);
        if i32::try_from(self.list.len()).unwrap_or(i32::MAX) > self.max {
            self.list.pop_back();
        }
        let mut weight = 1.0f32;
        let mut weight_sum = 0.0f32;
        let mut sum = [0.0f32; 2];
        for p in &self.list {
            sum[0] = p[0].mul_add(weight, sum[0]);
            sum[1] = p[1].mul_add(weight, sum[1]);
            weight_sum += weight;
            weight *= self.prev_mul;
        }
        [sum[0] / weight_sum, sum[1] / weight_sum]
    }
}

/// Engine queries of `ShowMouseOnMouseInput`.
pub trait MouseEnv {
    fn any_mouse_button_down(&mut self) -> bool;
    fn abs_position(&mut self) -> [i32; 2];
    fn game_time(&mut self) -> f64;
    fn gamepad_present(&mut self) -> bool;
    fn ui_direction_triggered(&mut self) -> bool;
    fn screen_size(&mut self) -> [i32; 2];
    fn hide_mouse_uis(&mut self);
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MouseActivity {
    pub last_abs: [i32; 2],
    pub active_at: f64,
}

impl Default for MouseActivity {
    fn default() -> Self {
        MouseActivity {
            last_abs: [0, 0],
            active_at: -1.0,
        }
    }
}

impl MouseActivity {
    /// `ChangeState`'s tail: remember the cursor position.
    pub fn set_last(&mut self, abs: [i32; 2]) {
        self.last_abs = abs;
    }

    /// `ShowMouseOnMouseInput`.
    pub fn step(&mut self, e: &mut dyn MouseEnv) -> bool {
        let mut active = e.any_mouse_button_down();
        let abs = e.abs_position();
        let dx = self.last_abs[0].wrapping_sub(abs[0]);
        let dy = self.last_abs[1].wrapping_sub(abs[1]);
        active = active || dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) > 0;
        self.last_abs = abs;
        if active {
            if self.active_at == -1.0 {
                active = false;
            }
            self.active_at = e.game_time();
        } else if e.gamepad_present() {
            let dir = e.ui_direction_triggered();
            if dir || (self.active_at + 5.0 < e.game_time() && self.active_at > 0.0) {
                e.hide_mouse_uis();
                let s = e.screen_size();
                self.last_abs = [
                    s[0].checked_div(2).unwrap_or(0),
                    s[1].checked_div(2).unwrap_or(0),
                ];
                self.active_at = -1.0;
            }
        }
        active
    }
}

pub const STATE_NULL: i32 = 0;
pub const STATE_GAME: i32 = 1;
pub const STATE_MAIN_MENU: i32 = 2;
pub const STATE_INVENTORY: i32 = 3;
pub const STATE_JOURNAL: i32 = 4;
pub const STATE_DEBUG: i32 = 5;
pub const STATE_PRE_MENU: i32 = 6;
pub const STATE_CREDITS: i32 = 7;
pub const STATE_DEMO_END: i32 = 8;
pub const STATE_LOAD_SCREEN: i32 = 9;

/// `CurrentStateSendsInputToGui`; an open inventory message blocks it.
pub fn state_sends_to_gui(state: i32, inventory_message_active: bool) -> bool {
    match state {
        STATE_INVENTORY => !inventory_message_active,
        STATE_DEBUG | STATE_JOURNAL | STATE_MAIN_MENU | STATE_PRE_MENU | STATE_DEMO_END => true,
        _ => false,
    }
}

/// `UpdateGamepadUIInput`: (action, `eUIArrow` / `eUIButton` flag) of the arrows and buttons.
pub const UI_ARROWS: [(i32, i32); 4] = [(UI_UP, 1), (UI_RIGHT, 2), (UI_DOWN, 4), (UI_LEFT, 8)];
pub const UI_BUTTONS: [(i32, i32); 7] = [
    (UI_PRIMARY, 1),
    (UI_SECONDARY, 2),
    (UI_PREV, 4),
    (UI_NEXT, 8),
    (INTERACT, 16),
    (UI_DELETE, 32),
    (UI_CLEAR, 64),
];

/// `UpdateGlobalInput` screenshot name: the first unused `Amnesia_Screenshot_NNN.<ext>` below
/// `dir`.
pub fn screenshot_name(
    dir: &[u32],
    ext: &[u8],
    exists: &mut dyn FnMut(&[u32]) -> bool,
) -> Vec<u32> {
    let slash = u32::from(b'/');
    let base = [
        string::add_slash_at_end(dir, slash),
        super::base::w("Amnesia_"),
    ]
    .concat();
    let mut count = 0;
    loop {
        let name = [
            base.clone(),
            super::base::w("Screenshot_"),
            string::utf8_to_wide(&string::int_to_string(count, 3)),
            super::base::w("."),
            string::utf8_to_wide(ext),
        ]
        .concat();
        count += 1;
        if !exists(&name) {
            return name;
        }
    }
}
