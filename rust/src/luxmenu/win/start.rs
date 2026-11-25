//! `cLuxMainMenu_StartGame`.

use super::host::{self, Arrow, Kind, Opts, W, lux_op, msg, set_op, tr};
use super::{Base, EXIT_START_GAME, UI_PRIMARY, UI_SECONDARY, WIN_NONE, go, popup_ok, ui_switch};
use crate::luxmenu2::pages;
use std::cell::Cell;

const WINDOW_ON_UPDATE: u8 = 0;
const PRESS_START: u8 = 1;
const PRESS_CANCEL: u8 = 2;
const UI_PRESS_START: u8 = 3;
const UI_PRESS_CANCEL: u8 = 4;
const PRESS_NORMAL: u8 = 5;
const UI_PRESS_NORMAL: u8 = 6;
const PRESS_HARD: u8 = 7;
const UI_PRESS_HARD: u8 = 8;
const START_POPUP: u8 = 9;

const GOLD: [f32; 4] = [232.0 / 255.0, 201.0 / 255.0, 28.0 / 255.0, 1.0];
const WHITE: [f32; 4] = [1.0, 1.0, 1.0, 1.0];
const WINDOW: [f32; 2] = [400.0, 220.0];

pub struct Start {
    pub base: Base,
    normal: Cell<W>,
    hard: Cell<W>,
    start: Cell<W>,
    desc: Cell<W>,
}

impl Start {
    pub fn new(base: Base) -> Start {
        Start {
            base,
            normal: Cell::default(),
            hard: Cell::default(),
            start: Cell::default(),
            desc: Cell::default(),
        }
    }

    pub fn create_gui(&self) {
        let b = &self.base;
        let set = b.set;
        let window = host::create_window(
            set,
            false,
            [0.0, 0.0, 5.0],
            WINDOW,
            &tr("MainMenu", "Start Game"),
        );
        b.set_window(window);
        window.add_callback(msg::ON_UPDATE, b.tok(WINDOW_ON_UPDATE));

        let border = 15.0f32;
        let btn_w = 120.0f32;
        let sepp = 3.0f32;
        let mut x = WINDOW[0] - (btn_w * 2.0 + sepp * 4.0);
        let y = window.size()[1] - 35.0;

        let start = host::create(
            set,
            Kind::Button,
            window,
            [x, y, 0.1],
            [(f64::from(btn_w) * 2.0 + f64::from(sepp)) as f32, 30.0],
            &tr("MainMenu", "Start Game"),
        );
        start.add_callback(msg::BUTTON_PRESSED, b.tok(PRESS_START));
        start.add_callback(msg::UI_BUTTON_PRESS, b.tok(UI_PRESS_START));
        self.start.set(start);

        x = (x / 2.0) - (btn_w / 2.0);
        let cancel = host::create(
            set,
            Kind::Button,
            window,
            [x, y, 0.1],
            [btn_w, 30.0],
            &tr("Global", "Cancel"),
        );
        cancel.add_callback(msg::BUTTON_PRESSED, b.tok(PRESS_CANCEL));
        cancel.add_callback(msg::UI_BUTTON_PRESS, b.tok(UI_PRESS_CANCEL));

        let btn_h = 30.0f32;
        x = WINDOW[0] - (btn_w * 2.0 + sepp * 4.0);
        let bx = (x / 2.0) - (btn_w / 2.0);
        let mut by =
            (f64::from(border) + f64::from(WINDOW[1]) / 4.0 - f64::from(btn_h) / 2.0) as f32;

        let normal = host::create(
            set,
            Kind::Button,
            window,
            [bx, by, 0.1],
            [btn_w, 30.0],
            &tr("MainMenu", "NormalMode"),
        );
        normal.add_callback(msg::BUTTON_PRESSED, b.tok(PRESS_NORMAL));
        normal.add_callback(msg::UI_BUTTON_PRESS, b.tok(UI_PRESS_NORMAL));
        normal.set_font_color(GOLD);
        self.normal.set(normal);

        by += 35.0 + sepp;
        let hard = host::create(
            set,
            Kind::Button,
            window,
            [bx, by, 0.1],
            [btn_w, 30.0],
            &tr("MainMenu", "HardMode"),
        );
        hard.add_callback(msg::BUTTON_PRESSED, b.tok(PRESS_HARD));
        hard.add_callback(msg::UI_BUTTON_PRESS, b.tok(UI_PRESS_HARD));
        hard.set_font_color(WHITE);
        self.hard.set(hard);

        start.nav(Arrow::Up, hard);
        start.nav(Arrow::Left, cancel);
        cancel.nav(Arrow::Up, hard);
        cancel.nav(Arrow::Right, start);
        normal.nav(Arrow::Down, hard);
        hard.nav(Arrow::Up, normal);
        hard.nav(Arrow::Down, start);

        let dpos = [WINDOW[0] - (btn_w * 2.0 + sepp * 4.0), 30.0 + border, 0.1];
        let mut dsize = [
            WINDOW[0] - (dpos[0] + sepp),
            WINDOW[1] - (border * 2.0) - 60.0,
        ];
        dsize[0] -= sepp * 3.0;
        let desc = host::create(
            set,
            Kind::Label,
            window,
            dpos,
            dsize,
            &tr("MainMenu", "NormalModeDescription"),
        );
        desc.set_font_color(WHITE);
        desc.set_word_wrap(true);
        desc.align_center();
        desc.set_clip_active(true);
        desc.set_f(host::f::SCROLL_SPEED, &[4.0]);
        desc.set_draw_background(true);
        desc.set_f(host::f::BG_COLOR, &[0.0, 0.0, 0.0, 0.5]);
        self.desc.set(desc);
    }

    pub fn set_active(&self, on: bool) {
        if on {
            let set = self.base.set;
            set.set_op(set_op::DEFAULT_FOCUS, self.start.get(), 0);
            set.set_op(set_op::FOCUSED, self.start.get(), 0);
        }
    }

    fn mode_colors(&self, hard: bool) {
        let (normal, hard_gold) = pages::mode_gold(hard);
        self.normal
            .get()
            .set_font_color(if normal { GOLD } else { WHITE });
        self.hard
            .get()
            .set_font_color(if hard_gold { GOLD } else { WHITE });
    }

    fn press_normal(&self) -> bool {
        self.mode_colors(false);
        self.desc
            .get()
            .set_text(&tr("MainMenu", "NormalModeDescription"));
        host::lux(lux_op::HARD_SET, 0);
        true
    }

    fn press_hard(&self) -> bool {
        self.mode_colors(true);
        self.desc
            .get()
            .set_text(&tr("MainMenu", "HardModeDescription"));
        host::lux(lux_op::HARD_SET, 1);
        true
    }

    fn press_start(&self) -> bool {
        let set = self.base.set;
        set.set_draw_focus(host::lux_flag(lux_op::GAMEPAD));
        let desc = if host::lux_flag(lux_op::HARD_GET) {
            tr("MainMenu", "HardModeStartNewGame")
        } else {
            tr("MainMenu", "Start a new game?")
        };
        host::popup(
            set,
            &[],
            &desc,
            &tr("MainMenu", "Yes"),
            &tr("MainMenu", "No"),
            self.base.tok(START_POPUP),
            Opts::NO_ESCAPE,
        );
        true
    }

    fn press_cancel(&self) -> bool {
        self.exit_callback(0);
        true
    }

    fn exit_callback(&self, val: i32) {
        let custom = host::lux_flag(lux_op::CUSTOM_STORY_ACTIVE);
        if let Some(target) = pages::exit_target(custom, popup_ok(val)) {
            go(target as usize);
        }
    }

    fn start_popup(&self, val: i32) -> bool {
        self.base.set.set_draw_focus(false);
        if popup_ok(val) {
            host::lux(lux_op::CUSTOM_STORY_CLEAR, 0);
            host::lux(lux_op::MENU_EXIT, EXIT_START_GAME);
            go(WIN_NONE);
        }
        true
    }

    pub fn callback(&self, id: u8, _w: W, val: i32) -> bool {
        match id {
            WINDOW_ON_UPDATE => true,
            PRESS_START => self.press_start(),
            PRESS_CANCEL => self.press_cancel(),
            UI_PRESS_START => match ui_switch(val) {
                UI_PRIMARY => self.press_start(),
                UI_SECONDARY => self.press_cancel(),
                _ => false,
            },
            UI_PRESS_CANCEL => match ui_switch(val) {
                UI_PRIMARY | UI_SECONDARY => self.press_cancel(),
                _ => false,
            },
            PRESS_NORMAL => self.press_normal(),
            UI_PRESS_NORMAL => match ui_switch(val) {
                UI_PRIMARY => self.press_normal(),
                UI_SECONDARY => self.press_cancel(),
                _ => false,
            },
            PRESS_HARD => self.press_hard(),
            UI_PRESS_HARD => match ui_switch(val) {
                UI_PRIMARY => self.press_hard(),
                UI_SECONDARY => self.press_cancel(),
                _ => false,
            },
            START_POPUP => self.start_popup(val),
            _ => false,
        }
    }
}
