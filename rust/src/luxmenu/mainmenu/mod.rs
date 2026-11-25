//! `cLuxMainMenu` (A9b): the top menu, fades, window stack, background world, blur render and exit
//! flow. Rust owns the menu and its windows; the C++ class is a shell holding the handle, and
//! every engine object (GUI set, viewport, textures, world, camera) is reached through `host`.

mod dump;
pub mod ffi;
pub mod host;
#[cfg(test)]
pub(crate) mod tests;

use super::host::{DOWN, UP, Wd, msg};
use super::store::{Cb, CbData, Kind, token};
use super::win;
use super::{keyconfig_win as kcw, options_win as ow};
use crate::luxmenu2::menu::{self as m, Menu as State, Times};
use crate::obj::{Arena, Handle};
use host as h;
use std::cell::{Cell, RefCell};
use std::rc::Rc;

pub const WIN_PROFILES: i32 = 0;
pub const WIN_OPTIONS: i32 = 1;
pub const WIN_KEY_CONFIG: i32 = 2;
pub const WIN_LOAD: i32 = 3;
pub const WIN_STORY_LIST: i32 = 4;
pub const WIN_STORY: i32 = 5;
pub const WIN_START: i32 = 6;

/// `glHardMode_SaveCost`.
const SAVE_COST: i64 = 4;

mod cb {
    pub const ENTER: u16 = 0;
    pub const LEAVE: u16 = 1;
    pub const PRESS: u16 = 2;
    pub const DRAW: u16 = 3;
    pub const HARD_DRAW: u16 = 4;
    pub const CONTINUE: u16 = 10;
    pub const CONTINUE_POPUP: u16 = 11;
    pub const START: u16 = 12;
    pub const START_POPUP: u16 = 13;
    pub const BACK: u16 = 14;
    pub const LOAD: u16 = 15;
    pub const CUSTOM: u16 = 16;
    pub const EXIT: u16 = 17;
    pub const EXIT_POPUP: u16 = 18;
    pub const TO_MENU: u16 = 19;
    pub const TO_MENU_POPUP: u16 = 20;
    pub const AND_SAVE: u16 = 21;
    pub const AND_SAVE_POPUP: u16 = 22;
    pub const PROFILE: u16 = 23;
    pub const OPTIONS: u16 = 24;
    pub const SAVE: u16 = 25;
    pub const SAVE_POPUP: u16 = 26;
}

enum WinKind {
    A9(u64),
    Opt(u64),
    Kc(u64),
}

struct Window {
    kind: WinKind,
    widget: Cell<usize>,
}

impl Window {
    fn create_gui(&self) {
        let w = match self.kind {
            WinKind::A9(b) => {
                win::create_gui(b);
                win::widget(b)
            }
            WinKind::Opt(b) => ow::create_gui(b),
            WinKind::Kc(b) => kcw::create_gui(b),
        };
        self.widget.set(w);
    }

    /// `iLuxMainMenuWindow::SetActive`: nothing until the window has widgets.
    fn set_active(&self, on: bool) {
        let w = Wd(self.widget.get());
        if w.is_null() {
            return;
        }
        w.set_visible(on);
        w.set_enabled(on);
        if on {
            h::center(w);
        }
        match self.kind {
            WinKind::A9(b) => win::set_active(b, on),
            WinKind::Opt(b) => ow::on_set_active(b, on),
            WinKind::Kc(b) => kcw::on_set_active(b, on),
        }
    }

    fn exit_pressed(&self) {
        match self.kind {
            WinKind::A9(b) => win::exit_pressed(b),
            WinKind::Opt(b) => ow::exit_pressed(b),
            WinKind::Kc(b) => kcw::exit_pressed(b),
        }
    }

    fn free(&self) {
        match self.kind {
            WinKind::A9(b) => win::free(b),
            WinKind::Opt(b) => ow::free(b),
            WinKind::Kc(b) => kcw::free(b),
        }
    }
}

struct Cfg {
    fade_in: f32,
    out_fast: f32,
    out_slow: f32,
    top_in: f32,
    top_out: f32,
    font_size: [f32; 2],
    start_pos: [f32; 3],
    start_pos_game: [f32; 3],
    music: String,
    zoom_sound: String,
    logo_pos: [f32; 3],
    fov: f32,
    zoomed_fov: f32,
    screen: [f32; 2],
}

pub struct Menu {
    bits: Cell<u64>,
    st: Cell<State>,
    cfg: Cfg,
    set: usize,
    vp: usize,
    wins: Vec<Window>,
    blur_prog: [usize; 2],
    black_fade: usize,
    top_bg: usize,
    logo_size: Cell<[f32; 2]>,
    font_mul: Cell<f32>,
    font: Cell<usize>,
    labels: RefCell<Vec<Wd>>,
    last_focused: Cell<Wd>,
    save_label: Cell<Wd>,
    save_cost: Cell<Wd>,
    num_tinder: Cell<Wd>,
    gui_created: Cell<bool>,
    recreate: Cell<bool>,
    screen_tex: Cell<usize>,
    screen_gfx: Cell<usize>,
    blur_tex: Cell<usize>,
    blur_gfx: Cell<usize>,
    logo_gfx: Cell<usize>,
    world: Cell<usize>,
    camera: Cell<usize>,
    cam_timer: Cell<f32>,
}

thread_local! {
    static REG: RefCell<Arena<Rc<Menu>>> = RefCell::new(Arena::default());
    static CURRENT: Cell<u64> = const { Cell::new(0) };
}

pub(crate) fn lookup(bits: u64) -> Option<Rc<Menu>> {
    let hd = Handle::<Rc<Menu>>::from_bits(bits)?;
    REG.with(|r| r.borrow().get(hd).cloned())
}

#[cfg_attr(test, allow(dead_code))]
fn current() -> Option<Rc<Menu>> {
    lookup(CURRENT.with(Cell::get))
}

/// Host ops 14-17 of the A9 windows and `SET_WINDOW_ACTIVE` of the options windows.
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn set_window(win: usize) {
    if let (Some(me), Ok(w)) = (current(), i32::try_from(win)) {
        me.set_window_active(w);
    }
}

#[cfg_attr(test, allow(dead_code))]
pub(crate) fn exit_menu(msg: usize) {
    if let (Some(me), Ok(v)) = (current(), i32::try_from(msg)) {
        me.exit_menu(v);
    }
}

#[cfg_attr(test, allow(dead_code))]
pub(crate) fn set_top_alpha(bits: usize) {
    if let Some(me) = current() {
        let mut s = me.st.get();
        s.top_alpha = f32::from_bits(bits as u32);
        me.st.set(s);
    }
}

#[cfg_attr(test, allow(dead_code))]
pub(crate) fn recreate_gui() {
    if let Some(me) = current() {
        me.recreate.set(true);
    }
}

pub fn new() -> u64 {
    let skin = h::skin_new("gui_main_menu.skin");
    let set = h::set_new(skin, "MainMenu");
    super::host::Set(set).set_draw_mouse(false);
    if h::is_chinese() {
        h::skin_font_mul(skin, 1.4);
    }
    let vp = h::vp_new();
    h::vp_active(vp, false);
    h::vp_visible(vp, false);
    h::set_active(set, false);
    h::vp_add_set(vp, set);

    let wins = new_windows(set);

    let screen = h::screen_size().0;
    let fade_in = h::cfg_float("Main", "MainFadeInTime", 0.0);
    let out_fast = h::cfg_float("Main", "MainFadeOutTimeFast", 0.0);
    let out_slow = h::cfg_float("Main", "MainFadeOutTimeSlow", 0.0);
    let top_in = h::cfg_float("Main", "TopMenuFadeInTime", 0.0);
    let top_out = h::cfg_float("Main", "TopMenuFadeOutTime", 0.0);
    let space = h::set_vsize(set);
    let mul2 = |v: [f32; 2]| [v[0] * space[0], v[1] * space[1]];
    let p = mul2(h::cfg_vec2("Main", "TopMenuStartRelativePos"));
    let start_pos = [p[0], p[1], 2.0];
    let mut font_size = mul2(h::cfg_vec2("Main", "TopMenuFontRelativeSize"));
    let p = mul2(h::cfg_vec2("Main", "TopMenuStartRelativePosInGame"));
    let start_pos_game = [p[0], p[1], 2.0];
    font_size[0] *= (screen[1] / screen[0]) / (3.0f32 / 4.0f32);
    let p = mul2(h::cfg_vec2("Main", "MainMenuLogoStartRelativePos"));
    let logo_pos = [p[0], p[1], 2.0];
    let logo_size = mul2(h::cfg_vec2("Main", "MainMenuLogoRelativeSize"));
    let music = h::cfg_string("Main", "Music", "");
    let zoom_sound = h::cfg_string("Main", "ZoomSound", "");
    let fov = crate::math::to_rad(h::cfg_float("Main", "BGCamera_FOV", 0.1));
    let zoomed_fov = crate::math::to_rad(h::cfg_float("Main", "BGCamera_ZoomedFOV", 1.0));

    let top_bg = h::gfx_rect([0.0, 0.0, 0.0, 1.0]);
    let black_fade = h::gfx_rect([0.0, 0.0, 0.0, 1.0]);
    let blur_prog = [0, 1].map(|i| {
        h::prog_new(
            &format!("MainMenuBlur{i}"),
            "mainmenu_screen_blur_vtx.glsl",
            "mainmenu_screen_blur_frag.glsl",
            i == 0,
        )
    });

    let me = Rc::new(Menu {
        bits: Cell::new(0),
        st: Cell::new(State::new()),
        cfg: Cfg {
            fade_in,
            out_fast,
            out_slow,
            top_in,
            top_out,
            font_size,
            start_pos,
            start_pos_game,
            music,
            zoom_sound,
            logo_pos,
            fov,
            zoomed_fov,
            screen,
        },
        set,
        vp,
        wins,
        blur_prog,
        black_fade,
        top_bg,
        logo_size: Cell::new(logo_size),
        font_mul: Cell::new(0.0),
        font: Cell::new(0),
        labels: RefCell::new(Vec::new()),
        last_focused: Cell::new(Wd(0)),
        save_label: Cell::new(Wd(0)),
        save_cost: Cell::new(Wd(0)),
        num_tinder: Cell::new(Wd(0)),
        gui_created: Cell::new(false),
        recreate: Cell::new(false),
        screen_tex: Cell::new(0),
        screen_gfx: Cell::new(0),
        blur_tex: Cell::new(0),
        blur_gfx: Cell::new(0),
        logo_gfx: Cell::new(0),
        world: Cell::new(0),
        camera: Cell::new(0),
        cam_timer: Cell::new(0.0),
    });
    let Some(hd) = REG.with(|r| r.borrow_mut().insert(Rc::clone(&me))) else {
        return 0;
    };
    let bits = hd.to_bits();
    me.bits.set(bits);
    CURRENT.with(|c| c.set(bits));
    me.reset();
    bits
}

fn new_windows(set: usize) -> Vec<Window> {
    let w9 = |kind, story| win::new(kind, super::win::host::W::from_addr(set), story);
    let story = w9(win::KIND_STORY, 0);
    let list = w9(win::KIND_STORY_LIST, story);
    let kinds = [
        WinKind::A9(w9(win::KIND_PROFILE, 0)),
        WinKind::Opt(ow::create(set)),
        WinKind::Kc(kcw::create(set)),
        WinKind::A9(w9(win::KIND_LOAD, 0)),
        WinKind::A9(list),
        WinKind::A9(story),
        WinKind::A9(w9(win::KIND_START, 0)),
    ];
    kinds
        .into_iter()
        .map(|kind| Window {
            kind,
            widget: Cell::new(0),
        })
        .collect()
}

pub fn free(bits: u64) {
    let Some(hd) = Handle::<Rc<Menu>>::from_bits(bits) else {
        return;
    };
    let me = REG.with(|r| r.borrow_mut().remove(hd));
    CURRENT.with(|c| {
        if c.get() == bits {
            c.set(0);
        }
    });
    if let Some(me) = me {
        for w in &me.wins {
            w.free();
        }
    }
}

fn wide(s: &str) -> Vec<u32> {
    super::host::w(s)
}

fn subst(text: Vec<u32>, num: i64) -> Vec<u32> {
    m::subst_hash(&text, num).unwrap_or(text)
}

impl Menu {
    fn state(&self) -> State {
        self.st.get()
    }

    fn upd<R>(&self, f: impl FnOnce(&mut State) -> R) -> R {
        let mut s = self.st.get();
        let r = f(&mut s);
        self.st.set(s);
        r
    }

    fn token(&self, id: u16) -> usize {
        token(Cb {
            kind: Kind::Main,
            owner: self.bits.get(),
            aux: 0,
            id,
        })
    }

    fn set(&self) -> super::host::Set {
        super::host::Set(self.set)
    }

    pub fn load_fonts(&self) {
        self.font
            .set(h::font_load(&h::cfg_string("Main", "TopMenuFont", "")));
    }

    pub fn reset(&self) {
        let (world, camera) = (self.world.get(), self.camera.get());
        if world != 0 {
            h::world_destroy(world);
        }
        if camera != 0 {
            h::camera_destroy(camera);
        }
        h::vp_camera(self.vp, 0);
        h::vp_world(self.vp, 0);
        self.world.set(0);
        self.camera.set(0);
    }

    pub fn on_quit(&self) {
        if h::has_map() {
            h::save(h::save::AUTO_SAVE);
            h::destroy_map();
            h::updater(h::updater::RESET_ALL);
            h::custom_clear();
        }
        self.upd(|s| s.exit_msg = m::EXIT_QUIT_GAME);
        self.on_menu_exit();
    }

    pub fn enter(&self) {
        if !h::flag(h::flag::FULLSCREEN) {
            h::lock_input(false);
        }
        h::relative_mouse(false);
        h::input_state(h::input::MAIN_MENU);
        h::vp_active(self.vp, true);
        h::vp_visible(self.vp, true);
        h::set_active(self.set, true);
        h::gui_focus(self.set);

        if !h::map_loaded() {
            h::music_play(&self.cfg.music);
        } else {
            h::proglog("Enter main menu.");
            h::pause_map();
        }
        if h::flag(h::flag::BUILD_GAMEPAD) {
            let pad = h::flag(h::flag::GAMEPAD);
            self.set().set_draw_mouse(!pad);
            self.set().set_mouse_move(!pad);
        } else {
            self.set().set_draw_mouse(true);
        }

        self.upd(State::enter);
        self.create_gui();
        self.create_background();

        if h::flag(h::flag::NO_USER_CONFIG) {
            self.set_window_active(WIN_PROFILES);
        }
        self.upd(|s| s.top_alpha = 0.01);
        self.update_top_menu(1.0f32 / 60.0f32);
        self.cam_timer.set(0.0);
    }

    pub fn leave(&self) {
        if !h::flag(h::flag::DEBUG_WINDOW) {
            if !h::flag(h::flag::FULLSCREEN) {
                h::lock_input(true);
            }
            h::relative_mouse(true);
        }
        if !h::map_loaded() {
            h::music_op(h::music::STOP);
        }
        h::vp_active(self.vp, false);
        h::vp_visible(self.vp, false);
        h::set_active(self.set, false);
        self.destroy_background();
    }

    pub fn update(&self, dt: f32) {
        if self.recreate.get() {
            let current = self.state().window;
            self.upd(|s| s.window = m::WIN_LAST);
            self.create_gui();
            self.set_window_active(current);
            self.recreate.set(false);
        }
        self.update_base(dt);
        self.update_top_menu(dt);
    }

    pub fn draw(&self) {
        let space = h::set_vsize(self.set);
        let s = self.state();
        let set = self.set;
        let origin = [0.0, 0.0, 0.0];
        if self.screen_gfx.get() != 0 {
            if s.fade_alpha > 0.0 {
                h::draw_gfx(set, self.screen_gfx.get(), origin, space, None);
            }
            if self.blur_gfx.get() != 0 {
                h::draw_gfx(
                    set,
                    self.blur_gfx.get(),
                    [0.0, 0.0, 0.2],
                    space,
                    Some(1.0 - s.fade_alpha),
                );
            }
            if s.fade_alpha > 0.0 && s.exiting && s.exit_msg != m::EXIT_RETURN {
                h::draw_gfx(
                    set,
                    self.black_fade,
                    [0.0, 0.0, 50.0],
                    space,
                    Some(s.fade_alpha),
                );
            }
            let y = self.cfg.start_pos[1];
            h::draw_gfx(
                set,
                self.top_bg,
                [0.0, y, 0.5],
                [space[0], space[1] - y],
                Some(0.5 * s.top_alpha),
            );
        } else {
            if s.fade_alpha > 0.0 {
                h::draw_gfx(
                    set,
                    self.black_fade,
                    [0.0, 0.0, 50.0],
                    space,
                    Some(s.fade_alpha),
                );
            }
            if self.logo_gfx.get() != 0 {
                h::draw_gfx(
                    set,
                    self.logo_gfx.get(),
                    self.cfg.logo_pos,
                    self.logo_size.get(),
                    Some(s.top_alpha),
                );
            }
        }
    }

    pub fn set_window_active(&self, win: i32) {
        let prev = m::win_begin(&self.state(), win);
        if prev == -2 {
            return;
        }
        if h::flag(h::flag::BUILD_GAMEPAD) {
            let pad = h::flag(h::flag::GAMEPAD);
            h::set_draw_focus(
                self.set,
                if m::draw_focus_window(win) {
                    pad
                } else {
                    false
                },
            );
        }
        if prev != m::WIN_LAST {
            self.window(prev).inspect(|w| w.set_active(false));
        }
        let top = self.upd(|s| m::win_commit(s, win));
        if top {
            self.set_top_visible(true);
        } else {
            self.set_top_visible(false);
            self.window(self.state().window)
                .inspect(|w| w.set_active(true));
        }
    }

    fn window(&self, i: i32) -> Option<&Window> {
        usize::try_from(i).ok().and_then(|i| self.wins.get(i))
    }

    pub fn exit_pressed(&self) {
        match m::exit_pressed(&self.state(), h::map_loaded()) {
            m::EXIT_RETURN_TO_GAME => self.exit_menu(m::EXIT_RETURN),
            m::EXIT_WINDOW => {
                self.window(self.state().window)
                    .inspect(|w| w.exit_pressed());
            }
            _ => {}
        }
    }

    pub fn focus(&self, got: bool) {
        if !h::map_loaded() {
            h::music_op(if got {
                h::music::RESUME
            } else {
                h::music::PAUSE
            });
        }
    }

    pub fn device_changed(&self) {
        h::set_draw_focus(self.set, h::flag(h::flag::GAMEPAD));
    }

    pub fn exit_menu(&self, msg: i32) {
        self.set_top_visible(false);
        let flags = self.upd(|s| m::exit_menu(s, msg));
        h::input_state(h::input::NULL);
        self.set().set_draw_mouse(false);
        h::gui_focus(0);
        if flags & m::XM_ZOOM_SOUND != 0 && !self.cfg.zoom_sound.is_empty() {
            h::zoom_sound(&self.cfg.zoom_sound);
        }
        if flags & m::XM_ACHIEVEMENT != 0 {
            let name = h::map_name();
            if name == "00_rainy_hall" || name == "01_old_archives" {
                h::unlock_nope();
            }
        }
    }

    fn on_menu_exit(&self) {
        let msg = self.state().exit_msg;
        match m::on_menu_exit(msg, h::flag(h::flag::DEMO_ON_ALL_EXIT)) {
            m::OME_DEMO_END => h::updater(h::updater::DEMO_END),
            m::OME_ENGINE_EXIT => h::updater(h::updater::ENGINE_EXIT),
            m::OME_RETURN => {
                h::proglog("Return to game from menu");
                h::input_state(h::input::GAME);
                h::updater(h::updater::DEFAULT);
            }
            m::OME_START => {
                h::input_state(h::input::GAME);
                self.ensure_profile();
                h::updater(h::updater::DEFAULT);
                h::start_game();
            }
            m::OME_CONTINUE => {
                h::input_state(h::input::GAME);
                self.ensure_profile();
                h::updater(h::updater::DEFAULT);
                h::updater(h::updater::RESET_ALL);
                h::save(h::save::AUTO_LOAD);
            }
            m::OME_TO_MENU => {
                h::draw_menu_screen();
                h::updater(h::updater::RESET_ALL);
                h::custom_clear();
                self.leave();
                self.enter();
            }
            m::OME_AND_SAVE => {
                h::draw_menu_screen();
                h::save(h::save::AUTO_SAVE);
                if h::has_map() {
                    h::destroy_map();
                }
                h::updater(h::updater::RESET_ALL);
                h::custom_clear();
                self.leave();
                self.enter();
            }
            _ => {}
        }
    }

    fn ensure_profile(&self) {
        if h::flag(h::flag::NO_USER_CONFIG) {
            h::profile(h::profile::CREATE_DEFAULT);
            h::profile(h::profile::SET_DEFAULT);
            h::profile(h::profile::INIT_CONFIG);
        }
    }

    fn update_base(&self, dt: f32) {
        let t = Times {
            fade_in: self.cfg.fade_in,
            out_fast: self.cfg.out_fast,
            out_slow: self.cfg.out_slow,
            fov: self.cfg.fov,
            zoomed_fov: self.cfg.zoomed_fov,
        };
        let camera = self.camera.get();
        let base = self.upd(|s| m::update_base(s, dt, &t, camera != 0));
        if base.set_fov {
            h::camera_fov(camera, base.fov);
        }
        if base.exit {
            self.on_menu_exit();
        }
    }

    fn update_top_menu(&self, dt: f32) {
        let save = self.save_label.get();
        let save_focus = if save.is_null() {
            -1
        } else {
            i32::from(h::has_focus(save))
        };
        let (fade_in, fade_out) = (self.cfg.top_in, self.cfg.top_out);
        if self.upd(|s| m::update_top(s, dt, fade_in, fade_out, save_focus)) {
            let alpha = self.state().top_alpha;
            let labels = self.labels.borrow().clone();
            for l in labels {
                let c = l.font_color();
                l.set_font_color([c[0], c[1], c[2], alpha]);
            }
        }
    }

    fn set_top_visible(&self, visible: bool) {
        self.upd(|s| s.top_visible = visible);
        let labels = self.labels.borrow().clone();
        if visible {
            self.set()
                .set_default_nav(labels.first().copied().unwrap_or_default());
            self.set().set_focused(self.last_focused.get());
        } else if let Some(l) = labels.iter().find(|l| h::has_focus(**l)) {
            self.last_focused.set(*l);
        }
    }

    pub fn create_gui(&self) {
        if self.gui_created.get() {
            h::set_destroy_all(self.set);
        }
        self.create_top_menu_gui();
        for w in &self.wins {
            w.create_gui();
            w.set_active(false);
        }
        self.gui_created.set(true);
    }

    fn setup_label(&self, l: Wd) {
        let size = self.cfg.font_size;
        let mul = self.font_mul.get();
        l.set_center_align();
        l.set_font_color([1.0, 1.0, 1.0, 1.0]);
        l.set_font_size(size[0] * mul, size[1] * mul);
        h::set_font_type(l, self.font.get());
        l.on(msg::MOUSE_ENTER, self.token(cb::ENTER));
        l.on(msg::MOUSE_LEAVE, self.token(cb::LEAVE));
        l.on(msg::UI_BUTTON, self.token(cb::PRESS));
        l.on(msg::ON_DRAW, self.token(cb::DRAW));
        l.set_autosize(true);
        self.center_x(l);
        let mut labels = self.labels.borrow_mut();
        if let Some(prev) = labels.last().copied() {
            prev.set_nav(DOWN, l);
            l.set_nav(UP, prev);
        }
        labels.push(l);
    }

    /// `SetPosition(GetLocalPosition() - (GetSize().x * 0.5, 0, 0))`.
    fn center_x(&self, l: Wd) {
        let p = l.local_pos();
        l.set_pos([p[0] - l.size().0 * 0.5, p[1], p[2]]);
    }

    fn create_top_menu_gui(&self) {
        let cfg = &self.cfg;
        let mut pos = if h::map_loaded() {
            cfg.start_pos_game
        } else {
            cfg.start_pos
        };
        self.labels.borrow_mut().clear();
        self.upd(|s| {
            s.top_visible = true;
            s.top_alpha = 1.0;
        });

        let keys = [
            "Continue",
            "Back To Game",
            "Start Game",
            "Load Game",
            "Custom Map",
            "Options",
            "Change Profile",
            "Exit",
            "ExitToMainMenu",
            "ExitAndSave",
            "Save",
        ];
        let texts: Vec<Vec<u32>> = keys
            .iter()
            .map(|k| super::host::tr("MainMenu", k))
            .collect();
        let temp = self.set().label([0.0; 3], (0.0, 0.0), &[], Wd(0));
        let lengths: Vec<f32> = texts
            .iter()
            .map(|t| h::font_len(temp, cfg.font_size, t))
            .collect();
        let chars: Vec<i32> = texts
            .iter()
            .map(|t| i32::try_from(t.len()).unwrap_or(i32::MAX))
            .collect();
        h::widget_destroy(self.set, temp);
        let (mul, row_add) = m::font_fit(&lengths, &chars, cfg.screen[0], cfg.font_size);
        self.font_mul.set(mul);

        let mut flags = 0u32;
        let ptest = h::flag(h::flag::PTEST);
        let map = h::map_loaded();
        let exit_direct = h::flag(h::flag::EXIT_DIRECT);
        if ptest {
            flags |= m::P_PTEST;
        }
        if map {
            flags |= m::P_MAP;
        }
        if h::flag(h::flag::SAVE_EXISTS) {
            flags |= m::P_SAVE_EXISTS;
        }
        if h::flag(h::flag::HARD) {
            flags |= m::P_HARD;
        }
        if exit_direct {
            flags |= m::P_EXIT_DIRECT;
        }
        if map && !exit_direct && !ptest && h::flag(h::flag::PLAYER_DEAD) {
            flags |= m::P_DEAD;
        }
        if h::flag(h::flag::BUILD_DEMO) {
            flags |= m::P_NO_CUSTOM;
        }

        self.save_label.set(Wd(0));
        for kind in m::plan(flags) {
            let (key, id) = match kind {
                m::L_CONTINUE => ("Continue", cb::CONTINUE),
                m::L_BACK => ("Back To Game", cb::BACK),
                m::L_START => ("Start Game", cb::START),
                m::L_LOAD => ("Load Game", cb::LOAD),
                m::L_SAVE => ("Save", cb::SAVE),
                m::L_CUSTOM => ("Custom Map", cb::CUSTOM),
                m::L_OPTIONS => ("Options", cb::OPTIONS),
                m::L_PROFILE => ("Change Profile", cb::PROFILE),
                m::L_EXIT => ("Exit", cb::EXIT),
                m::L_TO_MENU => ("ExitToMainMenu", cb::TO_MENU),
                _ => ("ExitAndSave", cb::AND_SAVE),
            };
            let text = super::host::tr("MainMenu", key);
            if kind == m::L_SAVE {
                let l = h::label_named(self.set, pos, &text, &wide("Save"));
                l.on(msg::MOUSE_DOWN, self.token(id));
                self.save_label.set(l);
                self.setup_label(l);
                l.set_enabled(h::tinderboxes() >= SAVE_COST as i32);
                self.create_save_descriptions(l, row_add);
            } else {
                let l = self.set().label(pos, (0.0, 0.0), &text, Wd(0));
                l.on(msg::MOUSE_DOWN, self.token(id));
                self.setup_label(l);
            }
            pos[1] += row_add;
        }

        let labels = self.labels.borrow().clone();
        if let (Some(first), Some(last)) = (labels.first().copied(), labels.last().copied()) {
            first.set_nav(UP, last);
            last.set_nav(DOWN, first);
        }
        let first = labels.first().copied().unwrap_or_default();
        self.last_focused.set(first);
        self.set().set_default_nav(first);
        self.set().set_focused(first);

        let save = self.save_label.get();
        if !save.is_null() && !h::is_enabled(save) {
            self.bridge_nav(save);
        }
    }

    fn bridge_nav(&self, save: Wd) {
        let above = h::nav_get(save, UP);
        let below = h::nav_get(save, DOWN);
        if !above.is_null() {
            above.set_nav(DOWN, below);
        }
        if !below.is_null() {
            below.set_nav(UP, above);
        }
    }

    fn create_save_descriptions(&self, _save: Wd, row_add: f32) {
        let cfg = &self.cfg;
        let mul = self.font_mul.get();
        let mut pos = [
            cfg.start_pos_game[0],
            (-2.5f32).mul_add(row_add, cfg.start_pos_game[1]),
            2.0,
        ];
        let describe = |name: &str, text: Vec<u32>, pos: [f32; 3], scale: f32| {
            let l = h::label_named(self.set, pos, &text, &wide(name));
            l.set_center_align();
            l.set_font_color([0.5, 0.5, 0.5, 0.0]);
            l.set_font_size(
                cfg.font_size[0] * mul * scale,
                cfg.font_size[1] * mul * scale,
            );
            h::set_font_type(l, self.font.get());
            l.set_autosize(true);
            l.on(msg::ON_DRAW, self.token(cb::HARD_DRAW));
            l.set_autosize(true);
            self.center_x(l);
            l
        };
        let cost = subst(super::host::tr("MainMenu", "HardModeSaveCost"), SAVE_COST);
        self.save_cost
            .set(describe("SaveDescription", cost, pos, 0.85));
        pos[1] = row_add.mul_add(0.65, pos[1]);
        let tinder = subst(
            super::host::tr("MainMenu", "HardModeTinderboxInInventory"),
            i64::from(h::tinderboxes()),
        );
        self.num_tinder
            .set(describe("NumTinderboxes", tinder, pos, 0.75));
        self.upd(|s| {
            s.fade_desc = true;
            s.desc_alpha = 0.0;
        });
    }

    fn create_background(&self) {
        if h::map_loaded() {
            self.create_screen_textures();
            self.render_blur_texture();
            return;
        }
        let logo = h::gfx_image(&h::cfg_string("Main", "MenuLogo", "menu_logo.tga"));
        self.logo_gfx.set(logo);
        if logo != 0 {
            let size = h::gfx_size(logo);
            let aspect = size[0] / size[1];
            let mut ls = self.logo_size.get();
            ls[0] = ls[1] * aspect;
            self.logo_size.set(ls);
        }
        let file = h::cfg_string("Main", "BGScene", "");
        let world = if file.is_empty() {
            0
        } else {
            h::world_load(&file)
        };
        self.world.set(world);
        let camera = h::camera_new();
        self.camera.set(camera);
        let screen = h::screen_size().0;
        h::camera_fov(camera, self.cfg.fov);
        h::camera_aspect(camera, screen[0] / screen[1]);
        let found = h::camera_place(camera, world, "CameraPos");
        if world != 0 && !found {
            h::log_error(&format!(
                "Could not find start pos entity CameraPos in '{file}'\n"
            ));
        }
        if world != 0 {
            h::vp_world(self.vp, world);
        }
        h::vp_camera(self.vp, camera);
    }

    fn create_screen_textures(&self) {
        let size = h::screen_size().1;
        self.screen_tex.set(h::tex_new("Screen", size, true));
        self.blur_tex.set(h::tex_new("ScreenBlur", size, false));
        self.screen_gfx
            .set(h::gfx_tex(self.screen_tex.get(), false));
        self.blur_gfx.set(h::gfx_tex(self.blur_tex.get(), true));
    }

    fn render_blur(&self, input: usize, temp: usize, fbs: [usize; 2]) {
        let size = self.cfg.screen;
        h::prog_bind(self.blur_prog[0], true);
        h::gfx_set_fb(fbs[0]);
        h::gfx_set_tex(input);
        h::gfx_quad(size);
        h::prog_bind(self.blur_prog[0], false);
        h::prog_bind(self.blur_prog[1], true);
        h::gfx_set_fb(fbs[1]);
        h::gfx_set_tex(temp);
        h::gfx_quad(size);
        h::prog_bind(self.blur_prog[1], false);
    }

    fn render_blur_texture(&self) {
        let size = self.cfg.screen;
        let temp = h::tex_new("TempBlur", [size[0] as i32, size[1] as i32], true);
        let fbs = [0usize, 1].map(|i| {
            let tex = if i == 0 { temp } else { self.blur_tex.get() };
            h::fb_new(&format!("MainMenuBlurBuffer{i}"), tex)
        });
        h::gfx_state(h::gfx_state::BLEND, false);
        h::gfx_state(h::gfx_state::DEPTH_TEST, false);
        h::gfx_state(h::gfx_state::DEPTH_WRITE, false);
        h::gfx_ortho(size);
        h::gfx_copy_screen(self.screen_tex.get());
        self.render_blur(self.screen_tex.get(), temp, fbs);
        for _ in 0..6 {
            self.render_blur(self.blur_tex.get(), temp, fbs);
        }
        h::gfx_set_tex(0);
        h::gfx_set_fb(0);
        h::gfx_state(h::gfx_state::DEPTH_TEST, true);
        h::gfx_flush();
        h::tex_destroy(temp);
        for fb in fbs {
            h::fb_destroy(fb);
        }
    }

    fn destroy_background(&self) {
        let (world, camera) = (self.world.get(), self.camera.get());
        if world != 0 && camera != 0 {
            if self.logo_gfx.get() != 0 {
                h::gfx_destroy(self.logo_gfx.get());
            }
            h::camera_destroy(camera);
            h::world_destroy(world);
            h::vp_world(self.vp, 0);
            h::vp_camera(self.vp, 0);
            self.logo_gfx.set(0);
            self.camera.set(0);
            self.world.set(0);
        } else {
            if self.screen_gfx.get() != 0 {
                h::gfx_destroy(self.screen_gfx.get());
            }
            if self.screen_tex.get() != 0 {
                h::tex_destroy(self.screen_tex.get());
            }
            if self.blur_gfx.get() != 0 {
                h::gfx_destroy(self.blur_gfx.get());
            }
            if self.blur_tex.get() != 0 {
                h::tex_destroy(self.blur_tex.get());
            }
            self.screen_gfx.set(0);
            self.screen_tex.set(0);
            self.blur_tex.set(0);
            self.blur_gfx.set(0);
        }
    }

    /// Popup with the window set's draw-focus mirrored, the shape every top menu prompt has.
    fn popup(&self, text: Vec<u32>, ok: Vec<u32>, cancel: Vec<u32>, id: u16) {
        self.set_top_visible(false);
        h::set_draw_focus(self.set, h::flag(h::flag::GAMEPAD));
        let p = self
            .set()
            .msgbox([&[], &text, &ok, &cancel], self.token(id));
        h::popup_opts(p, self.set);
    }

    fn yes_no(&self, key: &str, id: u16) {
        let text = super::host::tr("MainMenu", key);
        let (yes, no) = (
            super::host::tr("MainMenu", "Yes"),
            super::host::tr("MainMenu", "No"),
        );
        self.popup(text, yes, no, id);
    }

    /// Result of a popup: draw focus off, then `ok` runs, else the top menu comes back.
    fn answered(&self, val: i32, ok: impl FnOnce()) {
        let yes = m::popup_ok(val);
        h::set_draw_focus(self.set, false);
        if yes {
            ok();
        } else {
            self.set_top_visible(true);
        }
    }

    fn top_press_guard(&self) -> bool {
        self.state().top_visible
    }

    pub fn callback(&self, id: u16, w: Wd, data: &CbData) -> bool {
        match id {
            cb::ENTER => {
                if self.state().top_visible {
                    self.set().set_focused(w);
                }
                true
            }
            cb::LEAVE => {
                if self.state().top_visible && h::focused(self.set) == w {
                    self.set().set_focused(Wd(0));
                }
                true
            }
            cb::PRESS => match m::top_press(data.lval) {
                1 => h::process_mouse_down(w),
                2 => {
                    self.exit_pressed();
                    true
                }
                _ => false,
            },
            cb::DRAW => {
                let alpha = self.state().top_alpha;
                if h::has_focus(w) {
                    w.set_font_color([
                        232.0f32 / 255.0f32,
                        201.0f32 / 255.0f32,
                        28.0f32 / 255.0f32,
                        alpha,
                    ]);
                } else {
                    w.set_font_color([1.0, 1.0, 1.0, alpha]);
                }
                true
            }
            cb::HARD_DRAW => {
                let s = self.state();
                w.set_font_color([s.desc_alpha, s.desc_alpha, s.desc_alpha, s.top_alpha]);
                false
            }
            cb::CONTINUE_POPUP => {
                self.answered(data.lval, || self.exit_menu(m::EXIT_CONTINUE));
                true
            }
            cb::START_POPUP => {
                self.answered(data.lval, || {
                    h::custom_clear();
                    self.exit_menu(m::EXIT_START);
                });
                true
            }
            cb::EXIT_POPUP => {
                self.answered(data.lval, || self.exit_menu(m::EXIT_QUIT_GAME));
                true
            }
            cb::TO_MENU_POPUP => {
                self.answered(data.lval, || self.exit_menu(m::EXIT_TO_MENU));
                true
            }
            cb::AND_SAVE_POPUP => {
                self.answered(data.lval, || self.exit_menu(m::EXIT_AND_SAVE));
                true
            }
            cb::SAVE_POPUP => {
                self.saved(data.lval);
                true
            }
            _ => {
                if !self.top_press_guard() {
                    return true;
                }
                self.press(id);
                true
            }
        }
    }

    fn press(&self, id: u16) {
        match id {
            cb::CONTINUE => self.yes_no("Continue old game?", cb::CONTINUE_POPUP),
            cb::START => {
                if h::flag(h::flag::ALLOW_HARD) && h::flag(h::flag::NO_CUSTOM_STORY) {
                    self.set_window_active(WIN_START);
                    h::set_hard(false);
                } else {
                    self.yes_no("Start a new game?", cb::START_POPUP);
                }
            }
            cb::BACK => self.exit_menu(m::EXIT_RETURN),
            cb::LOAD => self.set_window_active(WIN_LOAD),
            cb::CUSTOM => self.set_window_active(WIN_STORY_LIST),
            cb::EXIT => self.yes_no("Sure you want to quit?", cb::EXIT_POPUP),
            cb::TO_MENU => self.yes_no("Sure you want to exit to main menu?", cb::TO_MENU_POPUP),
            cb::AND_SAVE => self.yes_no("Sure you want to exit and save?", cb::AND_SAVE_POPUP),
            cb::PROFILE => self.set_window_active(WIN_PROFILES),
            cb::OPTIONS => self.set_window_active(WIN_OPTIONS),
            cb::SAVE => {
                let text = subst(super::host::tr("MainMenu", "HardModeSavePopup"), SAVE_COST);
                self.popup(
                    text,
                    super::host::tr("MainMenu", "Save"),
                    super::host::tr("Global", "Cancel"),
                    cb::SAVE_POPUP,
                );
            }
            _ => {}
        }
    }

    fn saved(&self, val: i32) {
        let save = m::popup_ok(val);
        h::set_draw_focus(self.set, false);
        self.set_top_visible(true);
        if !save {
            return;
        }
        let Some(left) = m::pay_save(h::tinderboxes()) else {
            return;
        };
        h::set_tinderboxes(left);
        h::save(h::save::HARD_SAVE);

        let text = subst(
            super::host::tr("MainMenu", "HardModeTinderboxInInventory"),
            i64::from(h::tinderboxes()),
        );
        let l = self.num_tinder.get();
        let mut g = l.global_pos();
        g[0] = self.cfg.start_pos_game[0];
        l.set_text(&text);
        h::set_global_pos(l, g);
        self.center_x(l);

        let save = self.save_label.get();
        if !save.is_null() {
            save.set_enabled(false);
            self.bridge_nav(save);
        }
    }

    pub fn dump_info(&self) -> DumpInfo {
        let s = self.state();
        DumpInfo {
            set: self.set,
            window: s.window,
            top_visible: s.top_visible,
            top_alpha: s.top_alpha,
            fade_alpha: s.fade_alpha,
            first_label: self.labels.borrow().first().map_or(0, |l| l.0),
            camera: self.camera.get(),
            world: self.world.get(),
            logo: self.logo_gfx.get(),
            logo_size: self.logo_size.get(),
        }
    }

    pub fn dump_background(&self) {
        self.create_background();
    }

    pub fn dump_end(&self) {
        self.destroy_background();
        h::set_destroy_all(self.set);
        self.gui_created.set(false);
    }

    pub fn set_ptr(&self) -> usize {
        self.set
    }

    pub fn window_dump(&self, win: i32, begin: bool) -> usize {
        let Some(w) = self.window(win) else {
            return 0;
        };
        if begin {
            w.create_gui();
            w.set_active(true);
        } else {
            w.set_active(false);
        }
        w.widget.get()
    }
}

#[derive(Clone, Copy, Default)]
pub struct DumpInfo {
    pub set: usize,
    pub window: i32,
    pub top_visible: bool,
    pub top_alpha: f32,
    pub fade_alpha: f32,
    pub first_label: usize,
    pub camera: usize,
    pub world: usize,
    pub logo: usize,
    pub logo_size: [f32; 2],
}

/// Widget callbacks of the top menu, routed from `ohpl_a10a_cb`.
pub fn callback(c: &Cb, w: Wd, data: &CbData) -> bool {
    let Some(me) = lookup(c.owner) else {
        return true;
    };
    me.callback(c.id, w, data)
}
