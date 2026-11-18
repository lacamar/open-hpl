//! `cLuxInputHandler`: the per-frame input updates of every input state. The handler lives in a
//! thread-local arena; the C++ shell holds the handle bits. Engine access goes through [`InEnv`],
//! which the C ABI backs with the host call of `LuxInputHost.cpp` (codes mirrored in
//! `LuxInputHost.h`). State sits in cells because the engine calls back into the handler
//! (`ChangeState` from the debug window, container switches) in the middle of an update.

use super::ffi::{ActionHost, CfgHost, Host, af, ai, astr, aw, op};
use super::input::{
    self, MouseActivity, MouseEnv, STATE_CREDITS, STATE_DEBUG, STATE_DEMO_END, STATE_GAME,
    STATE_INVENTORY, STATE_JOURNAL, STATE_LOAD_SCREEN, STATE_MAIN_MENU, STATE_PRE_MENU, Settings,
    Smooth, act,
};
use super::look::{self, LookEnv, LookIn};
use crate::obj::{Arena, Handle};
use std::cell::{Cell, RefCell};
use std::rc::Rc;
use std::sync::atomic::{AtomicBool, Ordering};

pub mod q {
    pub const DEBUG_MENU: i32 = 0;
    pub const PTEST: i32 = 1;
    pub const PLAYER_DEAD: i32 = 2;
    pub const PLAYER_ACTIVE: i32 = 3;
    pub const ALLOW_MENUS: i32 = 4;
    pub const INV_DISABLED: i32 = 5;
    pub const MSG_PAUSE: i32 = 6;
    pub const EFFECT_PAUSED: i32 = 7;
    pub const INV_MSG: i32 = 8;
    pub const DEMO_END: i32 = 9;
    pub const QUICK_SAVE: i32 = 10;
    pub const FAST_FORWARD: i32 = 11;
    pub const CROSSHAIR: i32 = 12;
    pub const PRE_CONTINUE: i32 = 13;
    pub const KEY_POP: i32 = 14;
    pub const PAD_SETUP: i32 = 15;
}

pub mod val {
    pub const MOUSE_ANY: i32 = 0;
    pub const MOUSE_ABS_X: i32 = 1;
    pub const MOUSE_ABS_Y: i32 = 2;
    pub const MOUSE_REL_X: i32 = 3;
    pub const MOUSE_REL_Y: i32 = 4;
    pub const SCREEN_W: i32 = 5;
    pub const SCREEN_H: i32 = 6;
    pub const GAME_TIME: i32 = 7;
    pub const PAD_PRESENT: i32 = 8;
    pub const PAD_BUTTON: i32 = 9;
    pub const PAD_AXIS: i32 = 10;
    pub const PAD_DEAD: i32 = 11;
    pub const FRAME_TIME: i32 = 12;
    pub const STEP_SIZE: i32 = 13;
}

pub mod fx {
    pub const RENDER_LOG: i32 = 0;
    pub const GUI_MOUSE: i32 = 1;
    pub const GUI_CLICK: i32 = 2;
    pub const PRE_MENU_BUTTON: i32 = 3;
    pub const PAD_DRAIN: i32 = 4;
    pub const UI_SEND: i32 = 5;
    pub const DEBUG_WINDOW: i32 = 6;
    pub const DEBUG_RELOAD: i32 = 7;
    pub const FAST_FORWARD: i32 = 8;
    pub const AUTO_SAVE: i32 = 9;
    pub const AUTO_LOAD: i32 = 10;
    pub const CONTAINER: i32 = 11;
    pub const CROSSHAIR: i32 = 12;
    pub const DEATH_PRESS: i32 = 13;
    pub const JOURNAL_FORCE: i32 = 14;
    pub const JOURNAL_QUEST_LOG: i32 = 15;
    pub const JOURNAL_RECENT: i32 = 16;
    pub const JOURNAL_EXIT: i32 = 17;
    pub const PL_MOVE: i32 = 18;
    pub const PL_LEAN_SET: i32 = 19;
    pub const PL_LEAN_ADD: i32 = 20;
    pub const PL_SCROLL: i32 = 21;
    pub const PL_ACTION: i32 = 22;
    pub const PL_RUN: i32 = 23;
    pub const PL_JUMP: i32 = 24;
    pub const PL_CROUCH: i32 = 25;
    pub const PL_HOLSTER: i32 = 26;
    pub const PL_YAW: i32 = 27;
    pub const PL_PITCH: i32 = 28;
    pub const PL_STOP: i32 = 29;
    pub const MSG_ACTION: i32 = 30;
    pub const EFFECT_ACTION: i32 = 31;
    pub const MENU_MOUSE: i32 = 32;
    pub const HIDE_MOUSE_UIS: i32 = 33;
    pub const INV_EXIT: i32 = 34;
    pub const INV_JOURNAL: i32 = 35;
    pub const MAIN_MENU_EXIT: i32 = 36;
    pub const CREDITS_EXIT: i32 = 37;
    pub const DEMO_END_EXIT: i32 = 38;
    pub const LOAD_SCREEN_EXIT: i32 = 39;
    pub const RESET_ACTIONS: i32 = 40;
}

const BECAME: i32 = 0;
const WAS: i32 = 1;
const IS: i32 = 2;
const DOUBLE: i32 = 3;

const CONTAINER_MAIN_MENU: i32 = 0;
const CONTAINER_INVENTORY: i32 = 1;
const CONTAINER_JOURNAL: i32 = 2;

const DIR_FORWARD: i32 = 0;
const DIR_RIGHT: i32 = 1;

const P_INTERACT: i32 = 0;
const P_ATTACK: i32 = 1;
const P_IGNITE: i32 = 2;
const P_LANTERN: i32 = 4;

const GUI_LEFT: i32 = 0x1;
const GUI_MIDDLE: i32 = 0x2;
const GUI_RIGHT: i32 = 0x4;
const GUI_WHEEL_UP: i32 = 0x8;
const GUI_WHEEL_DOWN: i32 = 0x10;
const GUI_BUTTON6: i32 = 0x20;
const GUI_BUTTON7: i32 = 0x40;
const GUI_BUTTON8: i32 = 0x80;
const GUI_BUTTON9: i32 = 0x100;

const SET_INVENTORY: i32 = 0;
const SET_PRE_MENU: i32 = 1;
const SET_MAIN_MENU: i32 = 2;
const SET_JOURNAL: i32 = 3;

const UI_ARROW_PRESS: i32 = 0;
const UI_ARROW_RELEASE: i32 = 1;
const UI_BUTTON_PRESS: i32 = 4;
const UI_BUTTON_RELEASE: i32 = 5;
const UI_BUTTON_DOUBLE: i32 = 6;

/// The engine calls of the input handler, in the order the C++ made them.
pub trait InEnv {
    fn trig(&self, kind: i32, action: i32) -> bool;
    fn val(&self, what: i32, idx: i32) -> f64;
    fn q(&self, q: i32) -> bool;
    fn fx(&self, op: i32, a: i32, b: i32, f: f32);
    fn shot_dir(&self) -> Vec<u32>;
    fn shot_ext(&self) -> Vec<u8>;
    fn file_exists(&self, path: &[u32]) -> bool;
    fn shot_save(&self, path: &[u32]);
    fn game_cfg_int(&self, section: &[u8], key: &[u8], default: i32) -> i32;
    fn game_cfg_float(&self, section: &[u8], key: &[u8], default: f32) -> f32;
}

pub struct InputHandler {
    state: Cell<i32>,
    invert_mouse: Cell<bool>,
    smooth_mouse: Cell<bool>,
    mouse_sensitivity: Cell<f32>,
    pad_walk_sensitivity: Cell<f32>,
    pad_look_sensitivity: Cell<f32>,
    pad_look_invert: Cell<bool>,
    pad_ui_input: Cell<bool>,
    mouse_act: Cell<MouseActivity>,
    smooth: RefCell<Smooth>,
}

struct Mouse<'a>(&'a dyn InEnv);

impl MouseEnv for Mouse<'_> {
    fn any_mouse_button_down(&mut self) -> bool {
        self.0.val(val::MOUSE_ANY, 0) != 0.0
    }
    fn abs_position(&mut self) -> [i32; 2] {
        [
            self.0.val(val::MOUSE_ABS_X, 0) as i32,
            self.0.val(val::MOUSE_ABS_Y, 0) as i32,
        ]
    }
    fn game_time(&mut self) -> f64 {
        self.0.val(val::GAME_TIME, 0)
    }
    fn gamepad_present(&mut self) -> bool {
        self.0.val(val::PAD_PRESENT, 0) != 0.0
    }
    fn ui_direction_triggered(&mut self) -> bool {
        self.0.trig(IS, act::UI_UP)
            || self.0.trig(IS, act::UI_DOWN)
            || self.0.trig(IS, act::UI_LEFT)
            || self.0.trig(IS, act::UI_RIGHT)
    }
    fn screen_size(&mut self) -> [i32; 2] {
        [
            self.0.val(val::SCREEN_W, 0) as i32,
            self.0.val(val::SCREEN_H, 0) as i32,
        ]
    }
    fn hide_mouse_uis(&mut self) {
        self.0.fx(fx::HIDE_MOUSE_UIS, 0, 0, 0.0);
    }
}

struct Look<'a>(&'a dyn InEnv);

impl LookEnv for Look<'_> {
    fn is_triggered(&mut self, action: i32) -> bool {
        self.0.trig(IS, action)
    }
    fn add_lean(&mut self, v: f32) {
        self.0.fx(fx::PL_LEAN_ADD, 0, 0, v);
    }
    fn set_lean(&mut self, v: f32) {
        self.0.fx(fx::PL_LEAN_SET, 0, 0, v);
    }
    fn scroll(&mut self, v: f32) {
        self.0.fx(fx::PL_SCROLL, 0, 0, v);
    }
    fn stop_movement(&mut self) {
        self.0.fx(fx::PL_STOP, 0, 0, 0.0);
    }
    fn button_down(&mut self, button: i32) -> bool {
        self.0.val(val::PAD_BUTTON, button) != 0.0
    }
    fn axis(&mut self, axis: i32) -> f32 {
        self.0.val(val::PAD_AXIS, axis) as f32
    }
    fn dead_zone(&mut self) -> f32 {
        self.0.val(val::PAD_DEAD, 0) as f32
    }
    fn frame_time(&mut self) -> f32 {
        self.0.val(val::FRAME_TIME, 0) as f32
    }
    fn step_size(&mut self) -> f32 {
        self.0.val(val::STEP_SIZE, 0) as f32
    }
}

impl InputHandler {
    pub fn new(e: &dyn InEnv) -> Self {
        let max = e.game_cfg_int(b"Input", b"MaxSmoothMousePos", 0);
        let mul = e.game_cfg_float(b"Input", b"PrevSmoothMousePosMul", 0.0);
        let h = InputHandler {
            state: Cell::new(STATE_GAME),
            invert_mouse: Cell::new(false),
            smooth_mouse: Cell::new(false),
            mouse_sensitivity: Cell::new(0.0),
            pad_walk_sensitivity: Cell::new(0.0),
            pad_look_sensitivity: Cell::new(0.0),
            pad_look_invert: Cell::new(false),
            pad_ui_input: Cell::new(false),
            mouse_act: Cell::new(MouseActivity::default()),
            smooth: RefCell::new(Smooth::new(max, mul)),
        };
        e.q(q::PAD_SETUP);
        h
    }

    pub fn state(&self) -> i32 {
        self.state.get()
    }

    pub fn settings(&self) -> Settings {
        Settings {
            invert_mouse: self.invert_mouse.get(),
            smooth_mouse: self.smooth_mouse.get(),
            mouse_sensitivity: self.mouse_sensitivity.get(),
            gamepad_walk_sensitivity: self.pad_walk_sensitivity.get(),
            gamepad_look_sensitivity: self.pad_look_sensitivity.get(),
            gamepad_look_invert: self.pad_look_invert.get(),
        }
    }

    pub fn apply(&self, s: &Settings) {
        self.invert_mouse.set(s.invert_mouse);
        self.smooth_mouse.set(s.smooth_mouse);
        self.mouse_sensitivity.set(s.mouse_sensitivity);
        self.pad_walk_sensitivity.set(s.gamepad_walk_sensitivity);
        self.pad_look_sensitivity.set(s.gamepad_look_sensitivity);
        self.pad_look_invert.set(s.gamepad_look_invert);
    }

    pub fn smooth_reset(&self) {
        if let Ok(mut s) = self.smooth.try_borrow_mut() {
            s.reset();
        }
    }

    pub fn smooth_push(&self, rel: [f32; 2]) -> [f32; 2] {
        self.smooth
            .try_borrow_mut()
            .map_or([0.0, 0.0], |mut s| s.push(rel))
    }

    pub fn change_state(&self, e: &dyn InEnv, state: i32) {
        self.state.set(state);
        e.fx(fx::RESET_ACTIONS, 0, 0, 0.0);
        self.smooth_reset();
        let mut m = self.mouse_act.get();
        m.set_last([
            e.val(val::MOUSE_ABS_X, 0) as i32,
            e.val(val::MOUSE_ABS_Y, 0) as i32,
        ]);
        self.mouse_act.set(m);
    }

    pub fn post_render(&self, e: &dyn InEnv) {
        e.fx(fx::RENDER_LOG, 0, 0, 0.0);
    }

    pub fn update(&self, e: &dyn InEnv) {
        self.global(e);
        match self.state.get() {
            STATE_GAME => self.game(e),
            STATE_MAIN_MENU => self.main_menu(e),
            STATE_PRE_MENU => self.pre_menu(e),
            STATE_INVENTORY => self.inventory(e),
            STATE_JOURNAL => self.journal(e),
            STATE_DEBUG => self.debug(e),
            STATE_CREDITS => self.credits(e),
            STATE_DEMO_END => self.demo_end(e),
            STATE_LOAD_SCREEN => self.load_screen(e),
            _ => {}
        }
    }

    fn global(&self, e: &dyn InEnv) {
        if e.trig(BECAME, act::SCREENSHOT) {
            let dir = e.shot_dir();
            let ext = e.shot_ext();
            let name = input::screenshot_name(&dir, &ext, &mut |p| e.file_exists(p));
            e.shot_save(&name);
        }
        if e.trig(BECAME, act::PRINT_INFO) {
            e.fx(fx::RENDER_LOG, 1, 0, 0.0);
        }
        let state = self.state.get();
        let inv_msg = state == STATE_INVENTORY && e.q(q::INV_MSG);
        if !input::state_sends_to_gui(state, inv_msg) {
            return;
        }
        while e.q(q::KEY_POP) {
            if self.state.get() == STATE_PRE_MENU && !e.q(q::PRE_CONTINUE) {
                e.fx(fx::PRE_MENU_BUTTON, 0, 0, 0.0);
            }
        }
        e.fx(fx::GUI_MOUSE, 0, 0, 0.0);
        for (a, b) in [
            (act::LEFT_CLICK, GUI_LEFT),
            (act::MIDDLE_CLICK, GUI_MIDDLE),
            (act::RIGHT_CLICK, GUI_RIGHT),
        ] {
            click(e, a, b, true);
        }
        for (a, b) in [
            (act::SCROLL_UP, GUI_WHEEL_UP),
            (act::SCROLL_DOWN, GUI_WHEEL_DOWN),
        ] {
            click(e, a, b, false);
        }
        for (a, b) in [
            (act::MOUSE6, GUI_BUTTON6),
            (act::MOUSE7, GUI_BUTTON7),
            (act::MOUSE8, GUI_BUTTON8),
            (act::MOUSE9, GUI_BUTTON9),
        ] {
            click(e, a, b, true);
        }
        self.pad_ui_input.set(false);
        self.pad_ui_input.set(self.gamepad_ui(e));
    }

    fn gamepad_ui(&self, e: &dyn InEnv) -> bool {
        let mut ret = false;
        if e.val(val::PAD_PRESENT, 0) != 0.0 {
            e.fx(fx::PAD_DRAIN, 0, 0, 0.0);
        }
        for (a, id) in input::UI_ARROWS {
            if e.trig(BECAME, a) {
                e.fx(fx::UI_SEND, UI_ARROW_PRESS, id, 0.0);
                ret = true;
            }
            if e.trig(WAS, a) {
                e.fx(fx::UI_SEND, UI_ARROW_RELEASE, id, 0.0);
                ret = true;
            }
        }
        for (a, id) in input::UI_BUTTONS {
            if e.trig(BECAME, a) {
                e.fx(fx::UI_SEND, UI_BUTTON_PRESS, id, 0.0);
                ret = true;
            }
            if e.trig(WAS, a) {
                e.fx(fx::UI_SEND, UI_BUTTON_RELEASE, id, 0.0);
                ret = true;
            }
            if e.trig(DOUBLE, a) {
                e.fx(fx::UI_SEND, UI_BUTTON_DOUBLE, id, 0.0);
                ret = true;
            }
        }
        ret
    }

    fn game(&self, e: &dyn InEnv) {
        if e.trig(BECAME, act::OPEN_DEBUG) {
            e.fx(fx::DEBUG_WINDOW, 1, 0, 0.0);
        }
        if e.trig(BECAME, act::RELOAD_MAP) && e.q(q::DEBUG_MENU) {
            e.fx(fx::DEBUG_RELOAD, 0, 0, 0.0);
        }
        if e.trig(BECAME, act::FAST_FORWARD) && e.q(q::DEBUG_MENU) {
            let activate = !e.q(q::FAST_FORWARD);
            e.fx(fx::FAST_FORWARD, i32::from(activate), 0, 0.0);
        }
        if !e.q(q::PLAYER_DEAD) && e.q(q::QUICK_SAVE) && !e.q(q::PTEST) {
            if e.trig(BECAME, act::QUICK_SAVE) {
                e.fx(fx::AUTO_SAVE, 0, 0, 0.0);
            }
            if e.trig(BECAME, act::QUICK_LOAD) {
                e.fx(fx::AUTO_LOAD, 0, 0, 0.0);
            }
        }
        if e.trig(BECAME, act::EXIT) {
            e.fx(fx::CONTAINER, CONTAINER_MAIN_MENU, 0, 0.0);
        }
        if e.trig(BECAME, act::CROSSHAIR) {
            let show = !e.q(q::CROSSHAIR);
            e.fx(fx::CROSSHAIR, i32::from(show), 0, 0.0);
        }
        if e.q(q::MSG_PAUSE) {
            Self::actions(e, fx::MSG_ACTION);
        } else if e.q(q::EFFECT_PAUSED) {
            Self::actions(e, fx::EFFECT_ACTION);
        } else {
            self.player(e);
        }
    }

    /// `UpdateGameMessageInput` / `UpdateGameEffectInput`: the effect variant sends the gamepad
    /// secondary press to the message handler, as the original does.
    fn actions(e: &dyn InEnv, op: i32) {
        for (a, p) in [(act::ATTACK, P_ATTACK), (act::INTERACT, P_INTERACT)] {
            if e.trig(BECAME, a) {
                e.fx(op, p, 1, 0.0);
            }
            if e.trig(WAS, a) {
                e.fx(op, p, 0, 0.0);
            }
        }
        if e.trig(BECAME, act::UI_PRIMARY) {
            e.fx(op, P_INTERACT, 1, 0.0);
        }
        if e.trig(WAS, act::UI_PRIMARY) {
            e.fx(op, P_INTERACT, 0, 0.0);
        }
        if e.trig(BECAME, act::UI_SECONDARY) {
            e.fx(fx::MSG_ACTION, P_ATTACK, 1, 0.0);
        }
        if e.trig(WAS, act::UI_SECONDARY) {
            e.fx(op, P_ATTACK, 0, 0.0);
        }
    }

    fn player(&self, e: &dyn InEnv) {
        if e.q(q::PLAYER_DEAD) {
            if e.trig(BECAME, act::ATTACK)
                || e.trig(BECAME, act::INTERACT)
                || e.trig(BECAME, act::IGNITE)
                || e.trig(BECAME, act::JUMP)
                || e.trig(BECAME, act::CROUCH)
            {
                e.fx(fx::DEATH_PRESS, 0, 0, 0.0);
            }
            return;
        }
        if !e.q(q::PLAYER_ACTIVE) {
            return;
        }
        if e.q(q::ALLOW_MENUS) {
            if e.trig(BECAME, act::INVENTORY) && !e.q(q::INV_DISABLED) {
                e.fx(fx::CONTAINER, CONTAINER_INVENTORY, 0, 0.0);
            }
            if e.trig(BECAME, act::JOURNAL) && !e.q(q::INV_DISABLED) {
                e.fx(fx::CONTAINER, CONTAINER_JOURNAL, 0, 0.0);
            }
            if e.trig(BECAME, act::QUEST_LOG) && !e.q(q::INV_DISABLED) {
                e.fx(fx::JOURNAL_FORCE, 0, 0, 0.0);
                e.fx(fx::CONTAINER, CONTAINER_JOURNAL, 0, 0.0);
                e.fx(fx::JOURNAL_QUEST_LOG, 0, 0, 0.0);
            }
            if e.trig(BECAME, act::RECENT_TEXT) && !e.q(q::INV_DISABLED) {
                e.fx(fx::JOURNAL_RECENT, 0, 0, 0.0);
            }
        }

        for (a, dir, sign) in [
            (act::FORWARD, DIR_FORWARD, 1),
            (act::BACKWARD, DIR_FORWARD, -1),
            (act::RIGHT, DIR_RIGHT, 1),
            (act::LEFT, DIR_RIGHT, -1),
        ] {
            if e.trig(IS, a) {
                e.fx(fx::PL_MOVE, dir, sign, 0.0);
            }
        }
        if e.trig(IS, act::LEAN_RIGHT) {
            e.fx(fx::PL_LEAN_SET, 0, 0, 1.0);
        }
        if e.trig(IS, act::LEAN_LEFT) {
            e.fx(fx::PL_LEAN_SET, 0, 0, -1.0);
        }

        if e.trig(BECAME, act::ATTACK) {
            e.fx(fx::PL_ACTION, P_ATTACK, 1, 0.0);
        }
        if e.trig(WAS, act::ATTACK) {
            e.fx(fx::PL_ACTION, P_ATTACK, 0, 0.0);
        }
        if e.trig(BECAME, act::INTERACT) {
            e.trig(BECAME, act::LEFT_CLICK);
            e.fx(fx::PL_ACTION, P_INTERACT, 1, 0.0);
        }
        if e.trig(WAS, act::INTERACT) {
            e.trig(WAS, act::LEFT_CLICK);
            e.fx(fx::PL_ACTION, P_INTERACT, 0, 0.0);
        }
        if e.trig(BECAME, act::IGNITE) {
            e.fx(fx::PL_ACTION, P_IGNITE, 1, 0.0);
        }
        if e.trig(WAS, act::IGNITE) {
            e.fx(fx::PL_ACTION, P_IGNITE, 0, 0.0);
        }
        if e.trig(BECAME, act::HOLSTER) && e.q(q::DEBUG_MENU) {
            e.fx(fx::PL_HOLSTER, 0, 0, 0.0);
        }
        if e.trig(BECAME, act::LANTERN) {
            e.fx(fx::PL_ACTION, P_LANTERN, 1, 0.0);
        }
        if e.trig(WAS, act::LANTERN) {
            e.fx(fx::PL_ACTION, P_LANTERN, 0, 0.0);
        }
        if e.trig(BECAME, act::SCROLL_UP) {
            e.fx(fx::PL_SCROLL, 0, 0, 1.0);
        }
        if e.trig(BECAME, act::SCROLL_DOWN) {
            e.fx(fx::PL_SCROLL, 0, 0, -1.0);
        }

        for (a, op) in [
            (act::RUN, fx::PL_RUN),
            (act::JUMP, fx::PL_JUMP),
            (act::CROUCH, fx::PL_CROUCH),
        ] {
            if e.trig(BECAME, a) {
                e.fx(op, 1, 0, 0.0);
            }
            if e.trig(WAS, a) {
                e.fx(op, 0, 0, 0.0);
            }
        }

        let rel = [
            e.val(val::MOUSE_REL_X, 0) as i32,
            e.val(val::MOUSE_REL_Y, 0) as i32,
        ];
        let look_in = LookIn {
            mouse_rel: rel,
            mouse_sensitivity: self.mouse_sensitivity.get(),
            screen_height: e.val(val::SCREEN_H, 0) as f32,
            smooth_mouse: self.smooth_mouse.get(),
            invert_mouse: self.invert_mouse.get(),
            pad_present: e.val(val::PAD_PRESENT, 0) != 0.0,
            look_sensitivity: self.pad_look_sensitivity.get(),
            look_invert: self.pad_look_invert.get(),
        };
        let l = match self.smooth.try_borrow_mut() {
            Ok(mut s) => look::look(&mut s, &look_in, &mut Look(e)),
            Err(_) => [0.0, 0.0],
        };
        e.fx(fx::PL_YAW, 0, 0, -l[0]);
        e.fx(fx::PL_PITCH, 0, 0, -l[1]);
    }

    fn show_mouse(&self, e: &dyn InEnv) -> bool {
        let mut m = self.mouse_act.get();
        let r = m.step(&mut Mouse(e));
        self.mouse_act.set(m);
        r
    }

    fn inventory(&self, e: &dyn InEnv) {
        if self.show_mouse(e) {
            e.fx(fx::MENU_MOUSE, SET_INVENTORY, 0, 0.0);
        }
        if e.trig(BECAME, act::EXIT) || e.trig(BECAME, act::INVENTORY) {
            e.fx(fx::INV_EXIT, 0, 0, 0.0);
        }
        if e.trig(BECAME, act::JOURNAL) {
            e.fx(fx::INV_JOURNAL, 0, 0, 0.0);
        }
        if e.q(q::INV_MSG) {
            e.fx(fx::GUI_MOUSE, 0, 0, 0.0);
            if e.trig(BECAME, act::LEFT_CLICK)
                || e.trig(BECAME, act::MIDDLE_CLICK)
                || e.trig(BECAME, act::RIGHT_CLICK)
                || e.trig(BECAME, act::UI_PRIMARY)
                || e.trig(BECAME, act::UI_SECONDARY)
            {
                e.fx(fx::INV_EXIT, 0, 0, 0.0);
            }
        }
    }

    fn pre_menu(&self, e: &dyn InEnv) {
        if self.show_mouse(e) {
            e.fx(fx::MENU_MOUSE, SET_PRE_MENU, 0, 0.0);
        }
        let primary = e.trig(BECAME, act::EXIT) || e.trig(BECAME, act::UI_PRIMARY);
        if primary
            || (!e.q(q::PRE_CONTINUE)
                && (e.trig(BECAME, act::LEFT_CLICK)
                    || e.trig(BECAME, act::MIDDLE_CLICK)
                    || e.trig(BECAME, act::RIGHT_CLICK)))
        {
            e.fx(fx::PRE_MENU_BUTTON, 0, 0, 0.0);
        }
    }

    fn main_menu(&self, e: &dyn InEnv) {
        if self.show_mouse(e) {
            e.fx(fx::MENU_MOUSE, SET_MAIN_MENU, 0, 0.0);
        }
        if e.trig(BECAME, act::EXIT) {
            e.fx(fx::MAIN_MENU_EXIT, 0, 0, 0.0);
        }
    }

    fn journal(&self, e: &dyn InEnv) {
        if self.show_mouse(e) {
            e.fx(fx::MENU_MOUSE, SET_JOURNAL, 0, 0.0);
        }
        if e.trig(BECAME, act::EXIT) {
            e.fx(fx::JOURNAL_EXIT, 0, 0, 0.0);
        } else if (e.trig(BECAME, act::JOURNAL)
            || e.trig(BECAME, act::QUEST_LOG)
            || e.trig(BECAME, act::INVENTORY)
            || e.trig(BECAME, act::RECENT_TEXT))
            && !self.pad_ui_input.get()
        {
            e.fx(fx::JOURNAL_EXIT, 1, 0, 0.0);
        }
    }

    fn debug(&self, e: &dyn InEnv) {
        if e.trig(BECAME, act::OPEN_DEBUG) {
            e.fx(fx::DEBUG_WINDOW, 0, 0, 0.0);
        }
    }

    fn credits(&self, e: &dyn InEnv) {
        if exit_keys(e) {
            e.fx(fx::CREDITS_EXIT, 0, 0, 0.0);
        }
    }

    fn demo_end(&self, e: &dyn InEnv) {
        if e.q(q::DEMO_END) && e.trig(BECAME, act::EXIT) {
            e.fx(fx::DEMO_END_EXIT, 0, 0, 0.0);
        }
    }

    fn load_screen(&self, e: &dyn InEnv) {
        if exit_keys(e) {
            e.fx(fx::LOAD_SCREEN_EXIT, 0, 0, 0.0);
        }
    }
}

fn exit_keys(e: &dyn InEnv) -> bool {
    e.trig(BECAME, act::EXIT)
        || e.trig(BECAME, act::JUMP)
        || e.trig(BECAME, act::ATTACK)
        || e.trig(BECAME, act::INTERACT)
        || e.trig(BECAME, act::INVENTORY)
}

fn click(e: &dyn InEnv, action: i32, button: i32, double: bool) {
    if e.trig(BECAME, action) {
        e.fx(fx::GUI_CLICK, 0, button, 0.0);
    }
    if e.trig(WAS, action) {
        e.fx(fx::GUI_CLICK, 1, button, 0.0);
    }
    if double && e.trig(DOUBLE, action) {
        e.fx(fx::GUI_CLICK, 2, button, 0.0);
    }
}

pub mod hop {
    pub const TRIG: u32 = 160;
    pub const VAL: u32 = 161;
    pub const Q: u32 = 162;
    pub const FX: u32 = 163;
    pub const SHOT_DIR: u32 = 164;
    pub const SHOT_EXT: u32 = 165;
    pub const FILE_EXISTS: u32 = 166;
    pub const SHOT_SAVE: u32 = 167;
    pub const GAME_INT: u32 = 168;
    pub const GAME_FLOAT: u32 = 169;
    pub const LOG_CFG: u32 = 170;
}

/// Set while the C++ dump tick scripts the handler: gui sends are only logged then.
static SCRIPTED: AtomicBool = AtomicBool::new(false);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_set_scripted(on: bool) {
    SCRIPTED.store(on, Ordering::Relaxed);
}

const MSG_MOUSE_DOWN: i32 = 1;
const MSG_UI_ARROW_PRESS: i32 = 17;

/// The gui sends of `fx`; false for every other op.
fn gui_fx(op: i32, a: i32, b: i32) -> bool {
    match op {
        fx::GUI_CLICK => {
            let msg = match a {
                0 => MSG_MOUSE_DOWN,
                1 => MSG_MOUSE_DOWN + 1,
                _ => MSG_MOUSE_DOWN + 2,
            };
            crate::gui::direct::send_mouse_button(msg, b);
            true
        }
        fx::UI_SEND => {
            let msg = match a {
                UI_ARROW_PRESS | UI_ARROW_RELEASE => MSG_UI_ARROW_PRESS + a - UI_ARROW_PRESS,
                UI_BUTTON_PRESS | UI_BUTTON_RELEASE | UI_BUTTON_DOUBLE => {
                    MSG_UI_ARROW_PRESS + 2 + a - UI_BUTTON_PRESS
                }
                _ => return true,
            };
            crate::gui::direct::send_value(msg, b);
            true
        }
        _ => false,
    }
}

/// The C host behind `InEnv`.
struct CEnv(Host);

impl InEnv for CEnv {
    fn trig(&self, kind: i32, action: i32) -> bool {
        self.0
            .b(hop::TRIG, &[ai(i64::from(kind)), ai(i64::from(action))])
    }
    fn val(&self, what: i32, idx: i32) -> f64 {
        self.0
            .go(hop::VAL, &[ai(i64::from(what)), ai(i64::from(idx))])
            .d
    }
    fn q(&self, q: i32) -> bool {
        self.0.b(hop::Q, &[ai(i64::from(q))])
    }
    fn fx(&self, op: i32, a: i32, b: i32, f: f32) {
        if !SCRIPTED.load(Ordering::Relaxed) && gui_fx(op, a, b) {
            return;
        }
        self.0.go(
            hop::FX,
            &[ai(i64::from(op)), ai(i64::from(a)), ai(i64::from(b)), af(f)],
        );
    }
    fn shot_dir(&self) -> Vec<u32> {
        self.0.go(hop::SHOT_DIR, &[]).wide
    }
    fn shot_ext(&self) -> Vec<u8> {
        self.0.go(hop::SHOT_EXT, &[]).bytes
    }
    fn file_exists(&self, path: &[u32]) -> bool {
        self.0.b(hop::FILE_EXISTS, &[aw(path)])
    }
    fn shot_save(&self, path: &[u32]) {
        self.0.go(hop::SHOT_SAVE, &[aw(path)]);
    }
    fn game_cfg_int(&self, section: &[u8], key: &[u8], default: i32) -> i32 {
        self.0.i(
            hop::GAME_INT,
            &[astr(section), astr(key), ai(i64::from(default))],
        ) as i32
    }
    fn game_cfg_float(&self, section: &[u8], key: &[u8], default: f32) -> f32 {
        self.0
            .f(hop::GAME_FLOAT, &[astr(section), astr(key), af(default)])
    }
}

impl InputHandler {
    /// `LoadUserConfig`: logged by the host as the config dump expects.
    pub(super) fn load_user_config(&self, h: Host) {
        h.go(hop::LOG_CFG, &[ai(1)]);
        let s = input::load_user_config(
            &mut CfgHost { h, base: 0 },
            &mut CfgHost { h, base: op::CFG2 },
            &mut ActionHost(h),
        );
        h.go(hop::LOG_CFG, &[ai(0)]);
        self.apply(&s);
    }

    pub(super) fn save_user_config(&self, h: Host) {
        h.go(hop::LOG_CFG, &[ai(1)]);
        input::save_user_config(
            &self.settings(),
            &mut CfgHost { h, base: 0 },
            &mut CfgHost { h, base: op::CFG2 },
            &mut ActionHost(h),
        );
        h.go(hop::LOG_CFG, &[ai(0)]);
    }
}

type Call = super::ffi::Call;

thread_local! {
    static HANDLERS: RefCell<Arena<Rc<InputHandler>>> = RefCell::new(Arena::default());
}

static HOST_CALL: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);

pub(super) fn host() -> Option<Host> {
    let raw = HOST_CALL.load(std::sync::atomic::Ordering::Acquire);
    if raw == 0 {
        return None;
    }
    // SAFETY: only `ohpl_in_set_host` stores here, and it stores a `Call`.
    let call = unsafe { std::mem::transmute::<usize, Call>(raw) };
    Some(Host {
        ctx: std::ptr::null_mut(),
        call,
    })
}

fn get(bits: u64) -> Option<Rc<InputHandler>> {
    let h = Handle::from_bits(bits)?;
    HANDLERS.with(|a| a.try_borrow().ok().and_then(|a| a.get(h).cloned()))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_set_host(call: Option<Call>) {
    HOST_CALL.store(
        call.map_or(0, |c| c as usize),
        std::sync::atomic::Ordering::Release,
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_new() -> u64 {
    let Some(h) = host() else {
        return 0;
    };
    let env = CEnv(h);
    let handler = InputHandler::new(&env);
    input::create_actions(&mut ActionHost(h));
    HANDLERS.with(|a| {
        a.try_borrow_mut()
            .ok()
            .and_then(|mut a| a.insert(Rc::new(handler)))
            .map_or(0, Handle::to_bits)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_free(bits: u64) {
    if let Some(h) = Handle::from_bits(bits) {
        let old = HANDLERS.with(|a| a.try_borrow_mut().ok().and_then(|mut a| a.remove(h)));
        drop(old);
    }
}

fn with_env(bits: u64, f: impl FnOnce(&InputHandler, &CEnv)) {
    if let (Some(h), Some(host)) = (get(bits), host()) {
        f(&h, &CEnv(host));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_update(bits: u64) {
    with_env(bits, |h, e| h.update(e));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_post_render(bits: u64) {
    with_env(bits, |h, e| h.post_render(e));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_change_state(bits: u64, state: i32) {
    with_env(bits, |h, e| h.change_state(e, state));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_state(bits: u64) -> i32 {
    get(bits).map_or(STATE_GAME, |h| h.state())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_load_user_config(bits: u64) {
    if let (Some(h), Some(host)) = (get(bits), host()) {
        h.load_user_config(host);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_save_user_config(bits: u64) {
    if let (Some(h), Some(host)) = (get(bits), host()) {
        h.save_user_config(host);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_smooth_reset(bits: u64) {
    if let Some(h) = get(bits) {
        h.smooth_reset();
    }
}

/// # Safety
/// `out` points to 2 writable floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_in_smooth_push(bits: u64, x: f32, y: f32, out: *mut f32) {
    let r = get(bits).map_or([0.0, 0.0], |h| h.smooth_push([x, y]));
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

/// Setting `which`: 0 invert mouse, 1 smooth mouse, 2 mouse sensitivity, 3 gamepad look
/// sensitivity, 4 gamepad look invert.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_get(bits: u64, which: i32) -> f32 {
    let Some(h) = get(bits) else {
        return 0.0;
    };
    let s = h.settings();
    match which {
        0 => f32::from(u8::from(s.invert_mouse)),
        1 => f32::from(u8::from(s.smooth_mouse)),
        2 => s.mouse_sensitivity,
        3 => s.gamepad_look_sensitivity,
        4 => f32::from(u8::from(s.gamepad_look_invert)),
        _ => 0.0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_in_set(bits: u64, which: i32, v: f32) {
    let Some(h) = get(bits) else {
        return;
    };
    match which {
        0 => h.invert_mouse.set(v != 0.0),
        1 => h.smooth_mouse.set(v != 0.0),
        2 => h.mouse_sensitivity.set(v),
        3 => h.pad_look_sensitivity.set(v),
        4 => h.pad_look_invert.set(v != 0.0),
        _ => {}
    }
}
