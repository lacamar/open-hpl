//! `cLuxMainMenu_CustomStory` and `cLuxMainMenu_CustomStoryList`.

use super::host::{self, Kind, Opts, W, lux_op, lux_path, lux_with_op, msg, story_str_op, tr, ws};
use super::{Base, WIN_LOAD, WIN_NONE, WIN_STORY, WIN_STORY_LIST, Win, go, lookup, popup_ok};
use crate::luxmenu2::pages;
use std::cell::{Cell, RefCell};

const WINDOW: [f32; 2] = [600.0, 440.0];

const PRESS_CONTINUE: u8 = 0;
const PRESS_START: u8 = 1;
const PRESS_LOAD: u8 = 2;
const PRESS_BACK: u8 = 3;

pub struct Story {
    pub base: Base,
    story: Cell<W>,
    window: Cell<W>,
    picture: Cell<W>,
    author: Cell<W>,
    desc: Cell<W>,
    buttons: RefCell<Vec<W>>,
}

impl Story {
    pub fn new(base: Base) -> Story {
        Story {
            base,
            story: Cell::default(),
            window: Cell::default(),
            picture: Cell::default(),
            author: Cell::default(),
            desc: Cell::default(),
            buttons: RefCell::new(Vec::new()),
        }
    }

    pub fn create_gui(&self) {
        let b = &self.base;
        let set = b.set;
        let window = host::create_window(set, false, [0.0, 0.0, 5.0], WINDOW, &[]);
        self.window.set(window);
        b.set_window(window);

        let border = 15.0f32;
        let mut pos = [border, 60.0 + border, 0.1];

        let picture = host::create(
            set,
            Kind::Image,
            window,
            [6.0, 75.0, 0.05],
            [588.0, 360.0],
            &[],
        );
        picture.set_f(host::f::COLOR_MUL, &[1.0, 1.0, 1.0, 0.65]);
        self.picture.set(picture);

        let author = host::create(
            set,
            Kind::Label,
            window,
            [pos[0] + 50.0, pos[1] - 50.0, pos[2] + 0.0],
            [0.0, 0.0],
            &ws(" "),
        );
        self.author.set(author);
        let frame = host::create(
            set,
            Kind::Frame,
            window,
            [pos[0], pos[1] + 10.0, pos[2] + 0.0],
            [420.0, 345.0],
            &[],
        );
        let fsize = frame.size();
        let desc = host::create(
            set,
            Kind::Label,
            frame,
            [0.0, 0.0, 0.0],
            [fsize[0] - 20.0, fsize[1] - 10.0],
            &[],
        );
        desc.set_word_wrap(true);
        self.desc.set(desc);

        let labels = [
            tr("MainMenu", "Continue"),
            tr("CustomStory", "Start"),
            tr("MainMenu", "Load Game"),
            tr("Global", "Cancel"),
        ];
        let first = host::create(set, Kind::Button, window, [0.0; 3], [0.0, 30.0], &[]);
        let lengths: Vec<f32> = labels.iter().map(|l| first.text_width(l)).collect();
        let btn_w = pages::button_width(&lengths);
        let (btn_h, sepp) = (30.0f32, 3.0f32);

        pos[0] = window.size()[0] - btn_w - sepp - 5.0;
        pos[1] = window.size()[1] * 0.5 - 25.0 - 10.0;

        first.set_position(pos);
        first.set_size([btn_w, btn_h]);
        first.set_text(&labels[0]);
        first.add_callback(msg::BUTTON_PRESSED, b.tok(PRESS_CONTINUE));
        let mut buttons = vec![first];

        for (label, cb) in [
            (&labels[1], PRESS_START),
            (&labels[2], PRESS_LOAD),
            (&labels[3], PRESS_BACK),
        ] {
            pos[1] += btn_h + sepp * 2.0;
            let button = host::create(set, Kind::Button, window, pos, [btn_w, btn_h], label);
            button.add_callback(msg::BUTTON_PRESSED, b.tok(cb));
            buttons.push(button);
        }
        *self.buttons.borrow_mut() = buttons;
    }

    pub fn set_current_story(&self, story: W) {
        if self.story.get() == story {
            return;
        }
        if story.is_null() {
            host::lux(lux_op::CUSTOM_STORY_CLEAR, 0);
            return;
        }

        self.story.set(story);
        host::story_activate(story);

        let by = tr("CustomStory", "By");
        let mut name = host::story_str(story, story_str_op::NAME);
        let mut author = host::story_str(story, story_str_op::AUTHOR);
        let mut desc = tr("CustomStoryMain", "Description");
        let fb = pages::story_fallbacks(name.is_empty(), author.is_empty(), desc.is_empty());
        if fb & pages::FB_NAME != 0 {
            name = tr("CustomStory", "NoName");
        }
        if fb & pages::FB_AUTHOR != 0 {
            author = tr("CustomStory", "NoAuthor");
        }
        if fb & pages::FB_DESC != 0 {
            desc = tr("CustomStory", "NoDescription");
        }

        let img = host::story_str(story, story_str_op::IMG_FILE);
        if !img.is_empty() {
            self.picture.get().set_image_file(&img);
        }

        self.window.get().set_text(&name);
        let mut line = by;
        line.push(u32::from(' '));
        line.extend_from_slice(&author);
        self.author.get().set_text(&line);
        self.desc.get().set_text(&desc);

        let buttons = self.buttons.borrow().clone();
        if buttons.is_empty() {
            return;
        }

        let save_exists = host::lux_flag(lux_op::SAVE_EXISTS);
        let mut pos = buttons.first().map_or([0.0; 3], |b| b.local_position());
        let heights: Vec<f32> = buttons.iter().map(|b| b.size()[1]).collect();
        let mut active = vec![false; heights.len()];
        let mut ys = vec![0.0f32; heights.len()];
        pages::story_buttons(save_exists, pos[1], &heights, &mut active, &mut ys);

        for ((button, on), y) in buttons.iter().zip(active).zip(ys) {
            button.set_enabled(on);
            button.set_visible(on);
            if !on {
                continue;
            }
            pos[1] = y;
            button.set_position(pos);
        }
    }

    fn press_continue(&self) -> bool {
        host::lux(lux_op::INPUT_GAME, 0);
        if !host::lux_flag(lux_op::USER_CONFIG_LOADED) {
            let name = host::lux_str(lux_path::DEFAULT_PROFILE);
            host::lux_with(lux_with_op::CREATE_PROFILE, &name);
            host::lux_with(lux_with_op::SET_PROFILE, &name);
            host::lux(lux_op::INIT_USER_CONFIG, 0);
        }
        host::lux(lux_op::UPDATER_RESET, 0);
        host::lux(lux_op::AUTO_LOAD, 0);
        true
    }

    fn press_start(&self) -> bool {
        let mut folder = host::lux_str(lux_path::MAIN_PROFILE_SAVE);
        folder.extend(ws("custom"));
        let exists = host::folder_exists(&folder);
        let profile = host::lux_str(lux_path::PROFILE_SAVE);
        let make = pages::start_folders(exists, exists && host::folder_exists(&profile));
        if make & pages::FOLDER_CUSTOM != 0 {
            host::create_folder(&folder);
        }
        if make & pages::FOLDER_PROFILE != 0 {
            host::create_folder(&profile);
        }
        host::lux(lux_op::START_CUSTOM_STORY, 0);
        true
    }

    pub fn callback(&self, id: u8, _w: W, _val: i32) -> bool {
        match id {
            PRESS_CONTINUE => self.press_continue(),
            PRESS_START => self.press_start(),
            PRESS_LOAD => {
                go(WIN_LOAD as usize);
                true
            }
            PRESS_BACK => {
                self.set_current_story(W::NULL);
                go(WIN_STORY_LIST as usize);
                true
            }
            _ => false,
        }
    }
}

const WINDOW_ON_UPDATE: u8 = 0;
const SELECT_STORY: u8 = 1;
const PRESS_OK: u8 = 2;
const PRESS_CANCEL: u8 = 3;

pub struct List {
    pub base: Base,
    story_win: u64,
    list: Cell<W>,
}

impl List {
    pub fn new(base: Base, story_win: u64) -> List {
        List {
            base,
            story_win,
            list: Cell::default(),
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
            &tr("MainMenu", "Custom Map"),
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
        list.add_callback(msg::SELECTION_CHANGE, b.tok(SELECT_STORY));
        list.add_callback(msg::SELECTION_DOUBLE_CLICK, b.tok(PRESS_OK));
        self.list.set(list);

        let (btn_w, sepp) = (80.0f32, 3.0f32);
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
    }

    pub fn set_active(&self, on: bool) {
        if on {
            self.populate();
        } else {
            self.clear();
        }
    }

    fn populate(&self) {
        self.clear();
        if host::lux_flag(lux_op::USERDIR_RESOURCES) {
            self.add_stories(&host::lux_str(lux_path::USER_STORY_ROOT));
        }
        self.add_stories(&host::lux_str(lux_path::STORY_ROOT));
    }

    fn add_stories(&self, root: &[u32]) {
        let list = self.list.get();
        for dir in host::list_folders(root) {
            let story = host::story_new(&[root, &dir].concat());
            if story.is_null() {
                continue;
            }
            list.list_add(&host::story_str(story, story_str_op::NAME), story);
        }
    }

    fn clear(&self) {
        let list = self.list.get();
        for i in 0..list.list_len() {
            host::story_free(list.list_item_user(i));
        }
        list.list_clear();
    }

    fn press_ok(&self) -> bool {
        let list = self.list.get();
        let selected = list.list_selected();
        if pages::press_ok(selected) == pages::PRESS_NO_SELECTION {
            host::popup(
                self.base.set,
                &tr("Global", "Warning"),
                &tr("CustomStory", "NoValidStory"),
                &tr("Global", "OK"),
                &[],
                0,
                Opts::PLAIN,
            );
        } else {
            let story = list.list_item_user(selected);
            if let Some(win) = lookup(self.story_win)
                && let Win::Story(w) = &*win
            {
                w.set_current_story(story);
            }
            go(WIN_STORY as usize);
        }
        true
    }

    pub fn callback(&self, id: u8, _w: W, _val: i32) -> bool {
        match id {
            WINDOW_ON_UPDATE | SELECT_STORY => true,
            PRESS_OK => self.press_ok(),
            PRESS_CANCEL => {
                self.exit_callback(0);
                true
            }
            _ => false,
        }
    }

    fn exit_callback(&self, val: i32) {
        if !popup_ok(val) {
            host::lux(lux_op::CUSTOM_STORY_CLEAR, 0);
            return;
        }
        go(WIN_NONE);
    }
}
