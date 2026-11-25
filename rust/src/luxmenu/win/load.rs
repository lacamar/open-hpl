//! `cLuxMainMenu_LoadGame`.

use super::host::{self, Arrow, Kind, Opts, W, WStr, lux_op, lux_path, msg, set_op, tr, ws};
use super::{Base, UI_PRIMARY, UI_SECONDARY, go, popup_ok, ui_switch};
use crate::luxmenu2::pages;
use std::cell::{Cell, RefCell};

const WINDOW_ON_UPDATE: u8 = 0;
const PRESS_OK: u8 = 1;
const PRESS_CANCEL: u8 = 2;
const UI_PRESS_LIST: u8 = 3;
const UI_PRESS: u8 = 4;
const UI_PRESS_CANCEL: u8 = 5;
const LOAD_POPUP: u8 = 6;
const CLICK_CHANGE: u8 = 7;
const LOCK_LIST: u8 = 8;

const WINDOW: [f32; 2] = [600.0, 440.0];

pub struct Load {
    pub base: Base,
    list: Cell<W>,
    load: Cell<W>,
    files: RefCell<Vec<WStr>>,
}

impl Load {
    pub fn new(base: Base) -> Load {
        Load {
            base,
            list: Cell::default(),
            load: Cell::default(),
            files: RefCell::new(Vec::new()),
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
            &tr("LoadGame", "LoadGameTitle"),
        );
        b.set_window(window);
        window.add_callback(msg::ON_UPDATE, b.tok(WINDOW_ON_UPDATE));

        let border = 15.0f32;
        let (px, py) = (border, 60.0 + border);

        let list = host::create(
            set,
            Kind::ListBox,
            window,
            [px, py, 0.1 + 1.0],
            [WINDOW[0] - border * 2.0, 300.0],
            &[],
        );
        list.add_callback(msg::SELECTION_DOUBLE_CLICK, b.tok(PRESS_OK));
        list.add_callback(msg::CHECK_CHANGE, b.tok(CLICK_CHANGE));
        list.add_callback(msg::GET_UI_NAV_FOCUS, b.tok(LOCK_LIST));
        list.add_callback(msg::UI_BUTTON_PRESS, b.tok(UI_PRESS_LIST));
        self.list.set(list);

        let shot = list.size()[0] + 10.0;
        host::create(set, Kind::Image, list, [shot; 3], [200.0, 200.0], &[]);

        let btn_w = 80.0f32;
        let sepp = 3.0f32;
        let mut x = window.size()[0] - btn_w * 2.0 - sepp - 5.0;
        let y = window.size()[1] - 25.0 - 10.0;

        let ok = host::create(
            set,
            Kind::Button,
            window,
            [x, y, 0.1],
            [btn_w, 30.0],
            &tr("Global", "OK"),
        );
        ok.add_callback(msg::BUTTON_PRESSED, b.tok(PRESS_OK));
        ok.add_callback(msg::UI_BUTTON_PRESS, b.tok(UI_PRESS));
        self.load.set(ok);

        x += btn_w + sepp;
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

        list.nav(Arrow::Down, ok);
        ok.nav(Arrow::Right, cancel);
        cancel.nav(Arrow::Left, ok);
        for button in [ok, cancel] {
            button.nav(Arrow::Up, list);
        }
    }

    pub fn set_active(&self, on: bool) {
        if !on {
            return;
        }
        let (set, list) = (self.base.set, self.list.get());
        self.populate();
        set.set_op(set_op::DEFAULT_FOCUS, list, 0);
        set.set_op(set_op::FOCUSED, list, 0);
        if list.list_len() > 0 {
            list.set_locked(true);
        }
    }

    fn populate(&self) {
        let list = self.list.get();
        list.list_clear();
        self.files.borrow_mut().clear();

        let dir = host::lux_str(lux_path::PROFILE_SAVE);
        let found = host::list_files(&dir, &ws("*.sav"));
        let dates: Vec<_> = found
            .iter()
            .map(|f| {
                let mut path = dir.clone();
                path.extend_from_slice(f);
                host::file_date(host::DateKind::Modified, &path)
            })
            .collect();
        let mut names = Vec::with_capacity(found.len());
        for i in pages::sort_saves(&dates) {
            let Some(file) = found.get(i) else { continue };
            list.list_add(&host::proper_save_name(file), W::NULL);
            names.push(file.clone());
        }
        *self.files.borrow_mut() = names;
    }

    fn load_game(&self, idx: i32) {
        self.exit_callback(0);
        host::lux(lux_op::INPUT_GAME, 0);
        host::lux(lux_op::UPDATER_RESET, 0);
        host::lux(lux_op::PROGRESS_LOG_RESET, 0);
        let file = {
            let files = self.files.borrow();
            pages::load_index(idx, files.len()).and_then(|i| files.get(i).cloned())
        };
        if let Some(file) = file {
            let mut path = host::lux_str(lux_path::PROFILE_SAVE);
            path.extend_from_slice(&file);
            host::lux_with(host::lux_with_op::LOAD_GAME_FROM_FILE, &path);
        }
    }

    fn exit_callback(&self, val: i32) {
        let custom = host::lux_flag(lux_op::CUSTOM_STORY_ACTIVE);
        if let Some(target) = pages::exit_target(custom, popup_ok(val)) {
            go(target as usize);
        }
    }

    fn press_ok(&self) -> bool {
        let set = self.base.set;
        let warning = tr("Global", "Warning");
        let ok = tr("Global", "OK");
        if pages::press_ok(self.list.get().list_selected()) == pages::PRESS_NO_SELECTION {
            host::popup(
                set,
                &warning,
                &tr("LoadGame", "NoGame"),
                &ok,
                &[],
                0,
                Opts::PLAIN,
            );
        } else {
            host::popup(
                set,
                &warning,
                &tr("LoadGame", "LoadGameMessage"),
                &ok,
                &tr("Global", "Cancel"),
                self.base.tok(LOAD_POPUP),
                Opts::PLAIN,
            );
        }
        true
    }

    fn press_cancel(&self) -> bool {
        self.exit_callback(0);
        true
    }

    pub fn callback(&self, id: u8, _w: W, val: i32) -> bool {
        match id {
            WINDOW_ON_UPDATE => true,
            PRESS_OK => self.press_ok(),
            PRESS_CANCEL => self.press_cancel(),
            UI_PRESS_LIST => match ui_switch(val) {
                UI_SECONDARY => self.press_cancel(),
                _ => false,
            },
            UI_PRESS => match ui_switch(val) {
                UI_PRIMARY => self.press_ok(),
                UI_SECONDARY => self.press_cancel(),
                _ => false,
            },
            UI_PRESS_CANCEL => match ui_switch(val) {
                UI_PRIMARY | UI_SECONDARY => self.press_cancel(),
                _ => false,
            },
            LOAD_POPUP => {
                if popup_ok(val) {
                    self.load_game(self.list.get().list_selected());
                }
                true
            }
            CLICK_CHANGE => {
                self.base.set.set_op(set_op::FOCUSED, self.load.get(), 0);
                true
            }
            LOCK_LIST => {
                self.list.get().set_locked(true);
                true
            }
            _ => false,
        }
    }
}
