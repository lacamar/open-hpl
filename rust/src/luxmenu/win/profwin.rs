//! `cLuxMainMenu_Profile`.

use super::host::{
    self, Arrow, Kind, Opts, W, WStr, lux_op, lux_path, lux_with_op, msg, set_op, tr,
};
use super::{Base, UI_SECONDARY, WIN_NONE, go, popup_ok};
use crate::luxmenu::profile::{self, Activate, Date};
use std::cell::{Cell, RefCell};

const WINDOW_CLOSES: u8 = 0;
const UI_PRESS_LIST: u8 = 1;
const WINDOW_UI_PRESS: u8 = 2;
const EXIT_POPUP: u8 = 3;
const SELECTED_CHANGE: u8 = 4;
const DOUBLE_CLICK: u8 = 5;
const PRESS_SELECT: u8 = 6;
const PRESS_CREATE: u8 = 7;
const PRESS_DELETE: u8 = 8;
const DELETE_POPUP: u8 = 9;
const ENTER_CREATE: u8 = 10;
const ENTER_CANCEL: u8 = 11;
const UI_ENTER_CANCEL: u8 = 12;
const CLICK_CHANGE: u8 = 13;
const LOCK_LIST: u8 = 14;

const TOP_ALPHA: f32 = 0.01;

pub struct Profile {
    pub base: Base,
    last_picked: Cell<i32>,
    default_name: WStr,
    window_size: [f32; 2],
    info_width: f32,
    list_font_size: [f32; 2],
    enter_size: [f32; 2],
    enter_button_len: f32,
    names: RefCell<Vec<WStr>>,
    list: Cell<W>,
    select: Cell<W>,
    window: Cell<W>,
    enter_window: Cell<W>,
    enter_text: Cell<W>,
}

impl Profile {
    pub fn new(base: Base) -> Profile {
        Profile {
            base,
            last_picked: Cell::new(-1),
            default_name: tr("MainMenu", "New Player"),
            window_size: host::cfg_vec2("Profiles", "WindowSize"),
            info_width: host::cfg_float("Profiles", "InformationWidth"),
            list_font_size: host::cfg_vec2("Profiles", "ListFontSize"),
            enter_size: host::cfg_vec2("Profiles", "EnterNameWindowSize"),
            enter_button_len: host::cfg_float("Profiles", "EnterNameButtonLength"),
            names: RefCell::new(Vec::new()),
            list: Cell::default(),
            select: Cell::default(),
            window: Cell::default(),
            enter_window: Cell::default(),
            enter_text: Cell::default(),
        }
    }

    pub fn create_gui(&self) {
        self.create_main_gui();
        self.create_enter_name_gui();
    }

    pub fn set_active(&self, on: bool) {
        if !on {
            return;
        }
        let (set, list) = (self.base.set, self.list.get());
        set.set_op(set_op::DEFAULT_FOCUS, list, 0);
        set.set_op(set_op::FOCUSED, list, 0);
        match profile::activate(list.list_len()) {
            Activate::CreateWindow => {
                self.press_create();
            }
            Activate::LockList => list.set_locked(true),
        }
    }

    fn create_main_gui(&self) {
        let b = &self.base;
        let set = b.set;
        let (header, border) = (40.0f32, 35.0f32);
        let mut pos = [border, header + border, 1.0];
        let win = self.window_size;

        let window =
            host::create_window(set, true, [0.0, 0.0, 5.0], win, &tr("MainMenu", "Profiles"));
        self.window.set(window);
        b.set_window(window);
        window.set_close_disables_window(false);
        window.add_callback(msg::WINDOW_CLOSE, b.tok(WINDOW_CLOSES));
        window.add_callback(msg::UI_BUTTON_PRESS, b.tok(WINDOW_UI_PRESS));
        window.set_global_ui_listener(true);

        let list = host::create(
            set,
            Kind::ListBox,
            window,
            pos,
            [
                win[0] - self.info_width - border * 2.0,
                win[1] - pos[1] - 40.0 - border * 2.0,
            ],
            &[],
        );
        self.list.set(list);
        list.add_callback(msg::SELECTION_CHANGE, b.tok(SELECTED_CHANGE));
        list.add_callback(msg::SELECTION_DOUBLE_CLICK, b.tok(DOUBLE_CLICK));
        list.add_callback(msg::CHECK_CHANGE, b.tok(CLICK_CHANGE));
        list.add_callback(msg::GET_UI_NAV_FOCUS, b.tok(LOCK_LIST));
        list.add_callback(msg::UI_BUTTON_PRESS, b.tok(UI_PRESS_LIST));
        list.set_f(host::f::FONT_SIZE, &self.list_font_size);
        pos[1] += list.size()[1] + border;

        self.add_profiles();

        if list.list_len() > 0 {
            list.list_select(self.last_picked.get());
            if list.list_selected() == -1 {
                list.list_select(0);
            }
        }

        let sepp = 3.0f32;
        let len = list.size()[0] / 3.0 - sepp;
        let mut buttons = Vec::with_capacity(3);
        for (text, cb) in [
            ("Select", PRESS_SELECT),
            ("Create", PRESS_CREATE),
            ("Delete", PRESS_DELETE),
        ] {
            let button = host::create(
                set,
                Kind::Button,
                window,
                pos,
                [len, 30.0],
                &tr("MainMenu", text),
            );
            button.add_callback(msg::BUTTON_PRESSED, b.tok(cb));
            if cb == PRESS_SELECT {
                self.select.set(button);
            }
            pos[0] += len + sepp;
            buttons.push(button);
        }

        if let Some(first) = buttons.first() {
            list.nav(Arrow::Down, *first);
        }
        for (i, button) in buttons.iter().enumerate() {
            button.nav(Arrow::Up, list);
            if let Some(prev) = i.checked_sub(1).and_then(|p| buttons.get(p)) {
                button.nav(Arrow::Left, *prev);
            }
            if let Some(next) = buttons.get(i + 1) {
                button.nav(Arrow::Right, *next);
            }
        }
    }

    fn create_enter_name_gui(&self) {
        let b = &self.base;
        let set = b.set;
        let border = 5.0f32;
        let enter = self.enter_size;
        let mut pos = [border, 25.0 + border, 1.0];
        pos[1] += 5.0;

        let window = host::create_window(
            set,
            true,
            [0.0, 0.0, 10.0],
            enter,
            &tr("MainMenu", "Create Profile"),
        );
        self.enter_window.set(window);

        let label = host::create(
            set,
            Kind::Label,
            window,
            pos,
            [-1.0, -1.0],
            &tr("MainMenu", "Enter name of Profile"),
        );
        label.set_autogenerate_size(true);
        pos[1] += label.size()[1] + border;

        let text = host::create(
            set,
            Kind::TextBox,
            window,
            pos,
            [enter[0] - border * 2.0, 20.0],
            &[],
        );
        self.enter_text.set(text);
        text.set_force_callback_on_enter(true);
        text.set_callback_on_lost_focus(false);
        text.set_illegal_chars(&profile::ILLEGAL_CHARS);
        text.add_callback(msg::TEXT_BOX_ENTER, b.tok(ENTER_CREATE));
        text.set_max_text_length(profile::NAME_MAX);
        pos[1] += text.size()[1] + border;

        let len = self.enter_button_len;
        let sepp = enter[0] - len * 2.0 - border * 2.0;
        pos[1] = enter[1] - 30.0 - border;
        let create = host::create(
            set,
            Kind::Button,
            window,
            pos,
            [len, 30.0],
            &tr("MainMenu", "Create"),
        );
        create.add_callback(msg::BUTTON_PRESSED, b.tok(ENTER_CREATE));
        text.set_user_data(create);

        pos[0] += len + sepp;
        let cancel = host::create(
            set,
            Kind::Button,
            window,
            pos,
            [len, 30.0],
            &tr("MainMenu", "Cancel"),
        );
        cancel.add_callback(msg::BUTTON_PRESSED, b.tok(ENTER_CANCEL));
        cancel.add_callback(msg::UI_BUTTON_PRESS, b.tok(UI_ENTER_CANCEL));
        cancel.set_global_ui_listener(true);

        window.set_visible(false);
        window.set_enabled(false);
        window.center_in_set();
        window.set_static(true);

        text.nav(Arrow::Down, create);
        create.nav(Arrow::Right, cancel);
        cancel.nav(Arrow::Left, create);
        for button in [create, cancel] {
            button.nav(Arrow::Up, text);
        }
    }

    fn add_profiles(&self) {
        let list = self.list.get();
        list.list_clear();

        let base = host::lux_str(lux_path::BASE_SAVE);
        let folders = host::list_folders(&base);
        let dates: Vec<Date> = folders
            .iter()
            .map(|f| {
                let mut path = base.clone();
                path.extend_from_slice(f);
                host::file_date(host::DateKind::Created, &path)
            })
            .collect();
        let order = if folders.is_empty() {
            Vec::new()
        } else {
            profile::sort_order(&dates)
        };
        let mut names = Vec::with_capacity(folders.len());
        for i in order {
            if let Some(name) = folders.get(i as usize) {
                names.push(name.clone());
            }
        }
        for name in &names {
            list.list_add(name, W::NULL);
        }
        *self.names.borrow_mut() = names;
    }

    fn window_closes(&self) -> bool {
        let set = self.base.set;
        if host::lux_flag(lux_op::USER_CONFIG_LOADED) {
            go(WIN_NONE);
            host::lux(lux_op::MENU_TOP_ALPHA, TOP_ALPHA.to_bits() as usize);
            let window = self.window.get();
            window.set_enabled(false);
            window.set_visible(false);
            if set.attention() == window {
                set.set_op(set_op::ATTENTION, W::NULL, 0);
            }
        } else {
            host::popup(
                set,
                &[],
                &tr("MainMenu", "Sure you want to quit?"),
                &tr("MainMenu", "Yes"),
                &tr("MainMenu", "No"),
                self.base.tok(EXIT_POPUP),
                Opts::FOCUS,
            );
        }
        true
    }

    fn select_profile(&self, num: i32) {
        let name = {
            let names = self.names.borrow();
            let Some(i) = profile::select_target(num, names.len()) else {
                return;
            };
            self.last_picked.set(i as i32);
            names.get(i).cloned()
        };
        let Some(name) = name else { return };
        host::lux_with(lux_with_op::SET_PROFILE, &name);
        host::lux(lux_op::INIT_USER_CONFIG, 0);
        host::lux(lux_op::MENU_RECREATE, 0);
        go(WIN_NONE);
        host::lux(lux_op::MENU_TOP_ALPHA, TOP_ALPHA.to_bits() as usize);
    }

    fn press_create(&self) -> bool {
        let (set, window, text) = (
            self.base.set,
            self.enter_window.get(),
            self.enter_text.get(),
        );
        window.set_visible(true);
        window.set_enabled(true);
        window.center_in_set();

        set.set_op(set_op::PUSH_ATTENTION, W::NULL, 0);
        set.set_op(set_op::PUSH_DEFAULT, W::NULL, 0);
        set.set_op(set_op::PUSH_FOCUSED, W::NULL, 0);
        set.set_op(set_op::ATTENTION, window, 0);

        text.set_text(&self.default_name);
        text.select_text();

        set.set_op(set_op::FOCUSED, text, 0);
        set.set_op(set_op::DEFAULT_FOCUS, text, 0);
        true
    }

    fn message(&self, text: &str) {
        host::popup(
            self.base.set,
            &[],
            &tr("MainMenu", text),
            &tr("MainMenu", "OK"),
            &[],
            0,
            Opts::FOCUS,
        );
    }

    fn press_delete(&self) -> bool {
        if self.list.get().list_selected() < 0 {
            self.message("No profile to be deleted!");
            return true;
        }
        host::popup(
            self.base.set,
            &[],
            &tr("MainMenu", "Sure you want to delete profile"),
            &tr("MainMenu", "Yes"),
            &tr("MainMenu", "No"),
            self.base.tok(DELETE_POPUP),
            Opts::FOCUS,
        );
        true
    }

    fn delete_popup(&self, val: i32) -> bool {
        if val != 0 {
            return true;
        }
        let list = self.list.get();
        let selected = list.list_selected();
        let folder = list.list_item_text(selected);

        let in_use = host::lux_flag(lux_op::USER_CONFIG_LOADED)
            && host::lux_with(lux_with_op::PROFILE_IS, &folder) != 0;
        let config = host::lux_str(lux_path::BASE_CONFIG);
        let save = host::lux_str(lux_path::BASE_SAVE);
        let plan = profile::delete_plan(selected, in_use, config != save);
        if plan.reset_profile {
            host::lux_with(lux_with_op::SET_PROFILE, &[]);
            host::lux(lux_op::INIT_USER_CONFIG, 0);
        }
        if plan.remove_config {
            host::remove_folder(&[config.as_slice(), &folder].concat());
        }
        if !host::remove_folder(&[save.as_slice(), &folder].concat()) {
            self.message("Could not remove profile");
            return true;
        }

        self.add_profiles();
        list.list_select(plan.new_selection);
        true
    }

    fn close_enter_name(&self) {
        let set = self.base.set;
        let window = self.enter_window.get();
        window.set_visible(false);
        window.set_enabled(false);
        set.set_op(set_op::POP_ATTENTION, W::NULL, 0);
        set.set_op(set_op::POP_DEFAULT, W::NULL, 0);
        set.set_op(set_op::POP_FOCUSED, W::NULL, 0);
    }

    fn enter_create(&self) -> bool {
        let typed = self.enter_text.get().text();
        let (start, end) = profile::trim(&typed);
        let name = typed.get(start..end).unwrap_or(&[]).to_vec();

        if name.is_empty() || host::lux_with(lux_with_op::CREATE_PROFILE, &name) == 0 {
            self.message("The profile name already exists");
            return true;
        }

        self.close_enter_name();
        self.add_profiles();
        let list = self.list.get();
        list.list_select(list.list_len() - 1);
        self.base.set.set_op(set_op::FOCUSED, self.select.get(), 0);
        true
    }

    pub fn callback(&self, id: u8, _w: W, val: i32) -> bool {
        match id {
            WINDOW_CLOSES => self.window_closes(),
            UI_PRESS_LIST => val == UI_SECONDARY && self.window_closes(),
            WINDOW_UI_PRESS => {
                let secondary = val == UI_SECONDARY;
                let locked = secondary && self.list.get().locked();
                let name_visible = secondary && self.enter_text.get().is_visible();
                profile::window_ui_press(secondary, locked, name_visible) && self.window_closes()
            }
            EXIT_POPUP => {
                if popup_ok(val) {
                    host::lux(lux_op::ENGINE_EXIT, 0);
                } else {
                    self.list.get().set_locked(true);
                }
                true
            }
            SELECTED_CHANGE => true,
            DOUBLE_CLICK | PRESS_SELECT => {
                self.select_profile(self.list.get().list_selected());
                true
            }
            PRESS_CREATE => self.press_create(),
            PRESS_DELETE => self.press_delete(),
            DELETE_POPUP => self.delete_popup(val),
            ENTER_CREATE => self.enter_create(),
            ENTER_CANCEL => {
                self.close_enter_name();
                true
            }
            UI_ENTER_CANCEL => {
                if val == UI_SECONDARY {
                    self.close_enter_name();
                    return true;
                }
                false
            }
            CLICK_CHANGE => {
                self.base.set.set_op(set_op::FOCUSED, self.select.get(), 0);
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
