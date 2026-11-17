//! The splash and the first-run gamma screen (S8). Rust owns both classes' state and flow; the
//! gui set, viewport, gfx elements, widgets, font and base callbacks stay C++ objects reached
//! through one `call` entry per class (`op` ids mirrored in `soma/src/game/RustSomaS8.h`).

use super::s8_trace::{self, Tracer};
use super::screens::{self, SplashIn};
use crate::luxmenu2::host::{Arg, Host, Res};
use crate::obj::{Arena, Handle};
use std::sync::{LazyLock, Mutex};

pub mod op {
    pub const SET_NEW: i32 = 1;
    pub const SET_DRAW_MOUSE: i32 = 2;
    pub const SET_ACTIVE: i32 = 3;
    pub const GUI_FOCUS: i32 = 4;
    pub const GUI_FOCUSED_IS: i32 = 5;
    pub const VIEWPORT_NEW: i32 = 6;
    pub const VIEWPORT_ADD_SET: i32 = 7;
    pub const VIEWPORT_ACTIVE: i32 = 8;
    pub const GFX_RECT: i32 = 9;
    pub const GFX_TEX: i32 = 10;
    pub const GFX_SIZE: i32 = 11;
    pub const GFX_DESTROY: i32 = 12;
    pub const DRAW_GFX: i32 = 13;
    pub const DRAW_FONT: i32 = 14;
    pub const FONT_NEW: i32 = 15;
    pub const FONT_WRAP: i32 = 16;
    pub const CLIP_NEW: i32 = 17;
    pub const CLIP_RECT: i32 = 18;
    pub const CLIP_GET: i32 = 19;
    pub const CLIP_SET: i32 = 20;
    pub const SLIDER_NEW: i32 = 21;
    pub const BUTTON_NEW: i32 = 22;
    pub const SLIDER_SET: i32 = 23;
    pub const SLIDER_GET: i32 = 24;
    pub const DEFAULT_FOCUS_NAV: i32 = 25;
    pub const FOCUSED_WIDGET: i32 = 26;
    pub const MUSIC: i32 = 27;
    pub const NOW: i32 = 28;
    pub const SCREEN: i32 = 29;
    pub const LOAD_MENU: i32 = 30;
    pub const HUD_IMGUI: i32 = 31;
    pub const BASE_SPLASH_DONE: i32 = 32;
    pub const BASE_GAMMA_DONE: i32 = 33;
    pub const SAVE_GAMMA: i32 = 34;
    pub const GAMMA_GET: i32 = 35;
    pub const GAMMA_SET: i32 = 36;
    pub const FRAME_BEGIN: i32 = 37;
    pub const IMGUI_RENDER: i32 = 38;
    pub const FRAME_END: i32 = 39;
    pub const MOUSE_POLL: i32 = 40;
    pub const GUI_MOUSE_POS: i32 = 41;
    pub const CLICK: i32 = 42;
    pub const KEY_POLL: i32 = 43;
    pub const STATE_DIR: i32 = 44;
    pub const CLIP_DELETE: i32 = 45;
}

/// `eGuiMaterial` values used here.
pub const MAT_DIFFUSE: i64 = 0;
pub const MAT_ALPHA: i64 = 1;
pub const MAT_ADDITIVE: i64 = 3;

/// `eKey` values that continue the gamma screen.
pub(super) const KEY_RETURN: i64 = 3;
const KEY_ESCAPE: i64 = 5;
const KEY_SPACE: i64 = 6;

const BRAIN_FRAMES: usize = 26;
const WHITE: [f32; 4] = [1.0; 4];
const GAMMA_TEXT: &str =
    "Adjust gamma so you can barely make out the details on the robot poster on the left.";

pub const TAG_SLIDER: i64 = 0;
pub const TAG_BUTTON: i64 = 1;

pub struct E<'a> {
    h: &'a dyn Host,
    tr: Option<&'a Tracer>,
}

impl<'a> E<'a> {
    pub fn new(h: &'a dyn Host) -> E<'a> {
        E {
            h,
            tr: s8_trace::global(),
        }
    }

    pub fn traced(h: &'a dyn Host, tr: &'a Tracer) -> E<'a> {
        E { h, tr: Some(tr) }
    }

    fn call(&self, op: i32, a: &[Arg]) -> Res {
        let before = self.tr.filter(|_| Tracer::before(op));
        if let Some(t) = before {
            t.call(op, a, &Res::default());
        }
        let r = super::s8_direct::call(op, a).unwrap_or_else(|| self.h.call(op, a));
        if let (Some(t), None) = (self.tr, before) {
            t.call(op, a, &r);
        }
        r
    }

    fn event(&self, name: &str) {
        if let Some(t) = self.tr {
            t.event(name, &[], "");
        }
    }

    fn p(&self, op: i32, a: &[Arg]) -> usize {
        self.call(op, a).p()
    }

    fn v(&self, op: i32, a: &[Arg]) {
        self.call(op, a);
    }

    fn f2(&self, op: i32, a: &[Arg]) -> [f32; 2] {
        let r = self.call(op, a);
        [r.f(0), r.f(1)]
    }

    fn set_new(&self, name: &[u8]) -> usize {
        self.p(op::SET_NEW, &[Arg::S(name)])
    }

    fn viewport_new(&self, flag: bool) -> usize {
        self.p(op::VIEWPORT_NEW, &[Arg::B(flag)])
    }

    fn gfx_tex(&self, file: &[u8], mat: i64) -> usize {
        self.p(op::GFX_TEX, &[Arg::S(file), Arg::I(mat)])
    }

    fn gfx_rect(&self, c: [f32; 4], mat: i64) -> usize {
        let a = [
            Arg::F(c[0]),
            Arg::F(c[1]),
            Arg::F(c[2]),
            Arg::F(c[3]),
            Arg::I(mat),
        ];
        self.p(op::GFX_RECT, &a)
    }

    fn gfx_size(&self, g: usize) -> [f32; 2] {
        self.f2(op::GFX_SIZE, &[Arg::P(g)])
    }

    fn screen(&self) -> [f32; 2] {
        self.f2(op::SCREEN, &[])
    }

    fn now(&self) -> u64 {
        match self.tr {
            Some(t) => t.now(),
            None => self.h.call(op::NOW, &[]).u[0],
        }
    }

    fn clock(&self, dt: f32) -> f32 {
        if self.tr.is_some() { 1.0 / 60.0 } else { dt }
    }

    fn gamma_get(&self) -> f32 {
        self.call(op::GAMMA_GET, &[]).f(0)
    }

    fn draw_gfx(&self, set: usize, g: usize, pos: [f32; 3], size: [f32; 2], c: [f32; 4]) {
        let a = [
            Arg::P(set),
            Arg::P(g),
            Arg::F(pos[0]),
            Arg::F(pos[1]),
            Arg::F(pos[2]),
            Arg::F(size[0]),
            Arg::F(size[1]),
            Arg::F(c[0]),
            Arg::F(c[1]),
            Arg::F(c[2]),
            Arg::F(c[3]),
        ];
        self.v(op::DRAW_GFX, &a);
    }

    fn gui_focus(&self, set: usize) {
        self.v(op::GUI_FOCUS, &[Arg::P(set)]);
    }

    fn viewport_active(&self, vp: usize, on: bool) {
        self.v(op::VIEWPORT_ACTIVE, &[Arg::P(vp), Arg::B(on)]);
    }

    fn set_active(&self, set: usize, on: bool) {
        self.v(op::SET_ACTIVE, &[Arg::P(set), Arg::B(on)]);
    }
}

struct Store<T>(Mutex<Arena<T>>);

impl<T> Store<T> {
    fn new() -> Store<T> {
        Store(Mutex::new(Arena::default()))
    }

    fn add(&self, v: T) -> u64 {
        let Ok(mut a) = self.0.lock() else { return 0 };
        a.insert(v).map_or(0, Handle::to_bits)
    }

    /// `f` must not call the host: callbacks re-enter while a class call is running.
    fn with<R>(&self, h: u64, f: impl FnOnce(&mut T) -> R) -> Option<R> {
        let hd = Handle::from_bits(h)?;
        let mut a = self.0.lock().ok()?;
        a.get_mut(hd).map(f)
    }

    fn remove(&self, h: u64) -> Option<T> {
        let hd = Handle::from_bits(h)?;
        self.0.lock().ok()?.remove(hd)
    }
}

// ---- splash -----------------------------------------------------------------------------------

#[derive(Clone, Default)]
struct Splash {
    set: usize,
    vp: usize,
    black: usize,
    white: usize,
    premenu: usize,
    bar: usize,
    frame: usize,
    brain: [usize; BRAIN_FRAMES],
    clip: usize,
    screen: [f32; 2],
    vw: f32,
    elapsed: f32,
    boot_start: u64,
    load_start: u64,
    last_draw: u64,
    load_bar_pos: [f32; 2],
    load_bar_size: [f32; 2],
    finished: bool,
    awaiting_draw: bool,
}

static SPLASH: LazyLock<Store<Splash>> = LazyLock::new(Store::new);

fn snap(h: u64) -> Option<Splash> {
    SPLASH.with(h, |s| s.clone())
}

/// `h_out` gets the handle before the first host call that can re-enter (the boot load ticks).
pub fn splash_new(e: &E, h_out: &dyn Fn(u64)) -> u64 {
    let screen = e.screen();
    let vw = screens::virtual_width(screen);
    let set = e.set_new(b"Splash");
    let black = e.gfx_rect([0.0, 0.0, 0.0, 1.0], MAT_ALPHA);
    let white = e.gfx_rect(WHITE, MAT_ALPHA);
    let premenu = e.gfx_tex(b"Premenu.png", MAT_ALPHA);
    let bar = e.gfx_tex(b"loading_bar.dds", MAT_ALPHA);
    let frame = e.gfx_tex(b"loading_frame.dds", MAT_ALPHA);
    let mut brain = [0usize; BRAIN_FRAMES];
    for (i, b) in brain.iter_mut().enumerate() {
        *b = e.gfx_tex(format!("brain_{:02}.dds", i + 1).as_bytes(), MAT_ADDITIVE);
    }
    let clip = e.p(op::CLIP_NEW, &[]);
    e.v(
        op::MUSIC,
        &[
            Arg::S(b"loadscreen_background.ogg"),
            Arg::F(0.15),
            Arg::F(0.3),
            Arg::B(true),
            Arg::B(false),
        ],
    );
    let boot_start = e.now();
    let h = SPLASH.add(Splash {
        set,
        black,
        white,
        premenu,
        bar,
        frame,
        brain,
        clip,
        screen,
        vw,
        boot_start,
        ..Splash::default()
    });
    h_out(h);
    e.event("load_begin");
    e.v(op::LOAD_MENU, &[]);
    e.event("load_end");
    e.v(op::HUD_IMGUI, &[]);
    let vp = e.viewport_new(false);
    e.v(op::VIEWPORT_ADD_SET, &[Arg::P(vp), Arg::P(set)]);
    SPLASH.with(h, |s| s.vp = vp);
    h
}

pub fn splash_delete(e: &E, h: u64) {
    if let Some(s) = SPLASH.remove(h) {
        e.v(op::CLIP_DELETE, &[Arg::P(s.clip)]);
    }
}

fn splash_finish(e: &E, h: u64) {
    let Some(s) = snap(h) else { return };
    if s.finished {
        return;
    }
    SPLASH.with(h, |s| {
        s.finished = true;
        s.premenu = 0;
        s.bar = 0;
        s.frame = 0;
    });
    for g in [s.premenu, s.bar, s.frame] {
        e.v(op::GFX_DESTROY, &[Arg::P(g)]);
    }
    e.viewport_active(s.vp, false);
    e.v(op::BASE_SPLASH_DONE, &[]);
}

/// A traced run does one update, then one draw, whatever the engine loop does.
fn gate(awaiting_draw: &mut bool, update: bool) -> bool {
    if *awaiting_draw == update {
        return false;
    }
    *awaiting_draw = update;
    true
}

pub fn splash_update(e: &E, h: u64, dt: f32) {
    let Some(s) = snap(h) else { return };
    if s.finished {
        return;
    }
    if e.tr.is_some()
        && !SPLASH
            .with(h, |s| gate(&mut s.awaiting_draw, true))
            .unwrap_or(false)
    {
        return;
    }
    let (elapsed, done) = screens::splash_update(s.elapsed, e.clock(dt));
    SPLASH.with(h, |s| s.elapsed = elapsed);
    if done {
        splash_finish(e, h);
    }
}

pub fn scripts_may_run(h: u64) -> bool {
    SPLASH
        .with(h, |s| screens::scripts_may_run(s.finished, s.elapsed))
        .unwrap_or(true)
}

pub fn start_load(e: &E, h: u64) {
    let now = e.now();
    SPLASH.with(h, |s| s.load_start = now);
}

pub fn set_load_bar(h: u64, pos: [f32; 2], size: [f32; 2]) {
    SPLASH.with(h, |s| {
        s.load_bar_pos = pos;
        s.load_bar_size = size;
    });
}

fn draw_brain(e: &E, s: &Splash, alpha: f32, time: f32) {
    if alpha <= 0.0 {
        return;
    }
    let Some(f) = screens::brain_frame(time) else {
        return;
    };
    let Some(&g) = s.brain.get(f).filter(|g| **g != 0) else {
        return;
    };
    let (pos, size) = screens::brain_rect(s.vw, s.screen);
    e.draw_gfx(s.set, g, pos, size, [alpha, alpha, alpha, 1.0]);
}

pub fn splash_draw(e: &E, h: u64) {
    let Some(s) = snap(h) else { return };
    if s.finished {
        return;
    }
    if e.tr.is_some()
        && !SPLASH
            .with(h, |s| gate(&mut s.awaiting_draw, false))
            .unwrap_or(false)
    {
        return;
    }
    draw_body(e, s);
}

fn draw_body(e: &E, s: Splash) {
    e.draw_gfx(s.set, s.black, [0.0; 3], s.screen, WHITE);
    let t = s.elapsed;
    let img = (s.premenu != 0).then(|| e.gfx_size(s.premenu));
    let plan = screens::splash_plan(&SplashIn {
        screen: s.screen,
        vw: s.vw,
        t,
        premenu_image: img,
    });
    let a = plan.alpha;
    if s.premenu != 0 {
        e.draw_gfx(
            s.set,
            s.premenu,
            plan.premenu_pos,
            plan.premenu_size,
            [1.0, 1.0, 1.0, a],
        );
    }
    if s.frame != 0 && s.bar != 0 {
        let c = [1.0, 1.0, 1.0, a];
        e.draw_gfx(s.set, s.frame, plan.bar_pos, plan.bar_size, c);
        let r = plan.bar_clip;
        let ra = [
            Arg::P(s.clip),
            Arg::F(r[0]),
            Arg::F(r[1]),
            Arg::F(r[2]),
            Arg::F(r[3]),
        ];
        e.v(op::CLIP_RECT, &ra);
        let prev = e.p(op::CLIP_GET, &[Arg::P(s.set)]);
        e.v(op::CLIP_SET, &[Arg::P(s.set), Arg::P(s.clip)]);
        e.draw_gfx(s.set, s.bar, plan.bar_pos, plan.bar_size, c);
        e.v(op::CLIP_SET, &[Arg::P(s.set), Arg::P(prev)]);
    }
    draw_brain(e, &s, plan.brain_alpha, t);
}

/// One frame of the boot or script loading screen, at most every 33 ms.
pub fn splash_load_draw(e: &E, h: u64, boot: bool) {
    if boot {
        e.event("load_tick");
    }
    let Some(s) = snap(h) else { return };
    let now = e.now();
    if !screens::load_draw_due(now, s.last_draw) {
        return;
    }
    SPLASH.with(h, |s| s.last_draw = now);
    e.v(op::FRAME_BEGIN, &[]);
    if boot {
        let elapsed = screens::boot_elapsed(now, s.boot_start);
        SPLASH.with(h, |s| s.elapsed = elapsed);
        if let Some(s) = snap(h).filter(|s| !s.finished) {
            draw_body(e, s);
        }
    } else {
        if s.load_bar_size[0] > 0.0 {
            e.v(op::IMGUI_RENDER, &[]);
            let frac = screens::load_fraction(now, s.load_start);
            let pos = screens::virtual_to_screen(s.load_bar_pos, 1.0, s.vw, s.screen);
            let size = [s.load_bar_size[0] * frac, s.load_bar_size[1]];
            let size = screens::virtual_size_to_screen(size, s.vw, s.screen);
            e.draw_gfx(s.set, s.white, pos, size, WHITE);
        }
        draw_brain(e, &s, 1.0, now as f32 / 1000.0);
    }
    e.v(op::FRAME_END, &[Arg::P(s.set)]);
}

// ---- gamma screen -----------------------------------------------------------------------------

#[derive(Clone, Default)]
struct Gamma {
    set: usize,
    vp: usize,
    bg: usize,
    checker: usize,
    checker_pos: [f32; 2],
    checker_size: [f32; 2],
    font: usize,
    rows: Vec<Vec<u32>>,
    instr_pos: [f32; 2],
    slider: usize,
    screen: [f32; 2],
    finished: bool,
    mouse_was_down: bool,
    awaiting_draw: bool,
}

const ROW_H: f32 = 30.0;
const FONT_H: f32 = 24.0;

static GAMMA: LazyLock<Store<Gamma>> = LazyLock::new(Store::new);

/// `<state home>/open-hpl/<game>/gamma_screen_seen`: the first call per game is true.
pub fn should_show_and_mark_seen(e: &E) -> bool {
    use crate::backend::sdl::fs;
    let dir: Vec<u8> = e
        .call(op::STATE_DIR, &[])
        .str0()
        .iter()
        .map(|&c| c as u8)
        .collect();
    if !fs::folder_exists(&dir) {
        fs::create_folder(&dir);
    }
    let mut marker = dir;
    marker.extend_from_slice(b"gamma_screen_seen");
    if fs::file_exists(&marker) {
        return false;
    }
    if let Ok(p) = std::str::from_utf8(&marker) {
        let _ = std::fs::write(p, b"1\n");
    }
    true
}

pub fn gamma_new(e: &E, h_out: &dyn Fn(u64)) -> u64 {
    let screen = e.screen();
    let set = e.set_new(b"GammaScreen");
    e.v(op::SET_DRAW_MOUSE, &[Arg::P(set), Arg::B(true)]);
    let vp = e.viewport_new(true);
    e.v(op::VIEWPORT_ADD_SET, &[Arg::P(vp), Arg::P(set)]);
    e.set_active(set, true);
    e.gui_focus(set);
    let bg = e.gfx_tex(b"gamma_background.tga", MAT_DIFFUSE);
    let checker = e.gfx_tex(b"gamma.tga", MAT_ALPHA);
    let checker_size = if checker != 0 {
        Some(e.gfx_size(checker))
    } else {
        None
    };
    let font = e.p(op::FONT_NEW, &[Arg::S(b"sansation_large_bold.fnt")]);
    let mut rows = Vec::new();
    let mut wrap_w = 0.0;
    if font != 0 {
        wrap_w = screens::gamma_wrap_width(screen[0]);
        let text: Vec<u32> = GAMMA_TEXT.chars().map(u32::from).collect();
        let a = [
            Arg::P(font),
            Arg::F(wrap_w),
            Arg::F(ROW_H),
            Arg::F(FONT_H),
            Arg::W(&text),
        ];
        rows = e.call(op::FONT_WRAP, &a).s;
    }
    let lay = screens::gamma_layout(screen, checker_size, wrap_w, rows.len());
    let mut g = Gamma {
        set,
        vp,
        bg,
        checker,
        font,
        screen,
        ..Gamma::default()
    };
    if let Some(sz) = checker_size {
        g.checker_size = sz;
        g.checker_pos = lay.checker_pos;
    }
    if font != 0 {
        g.instr_pos = lay.instructions_pos;
    }
    g.rows = rows;
    let h = GAMMA.add(g);
    h_out(h);

    let sp = lay.slider_pos;
    let ss = lay.slider_size;
    let sa = [
        Arg::P(set),
        Arg::F(sp[0]),
        Arg::F(sp[1]),
        Arg::F(sp[2]),
        Arg::F(ss[0]),
        Arg::F(ss[1]),
        Arg::I(100),
        Arg::I(TAG_SLIDER),
    ];
    let slider = e.p(op::SLIDER_NEW, &sa);
    let cur = e.gamma_get();
    e.v(
        op::SLIDER_SET,
        &[
            Arg::P(slider),
            Arg::I(i64::from(screens::gamma_to_slider(cur))),
        ],
    );
    let bp = lay.button_pos;
    let bs = lay.button_size;
    let label: Vec<u32> = "Continue".chars().map(u32::from).collect();
    let ba = [
        Arg::P(set),
        Arg::F(bp[0]),
        Arg::F(bp[1]),
        Arg::F(bp[2]),
        Arg::F(bs[0]),
        Arg::F(bs[1]),
        Arg::W(&label),
        Arg::I(TAG_BUTTON),
    ];
    let button = e.p(op::BUTTON_NEW, &ba);
    e.v(op::DEFAULT_FOCUS_NAV, &[Arg::P(set), Arg::P(button)]);
    e.v(op::FOCUSED_WIDGET, &[Arg::P(set), Arg::P(button)]);
    GAMMA.with(h, |g| g.slider = slider);
    h
}

pub fn gamma_delete(h: u64) {
    GAMMA.remove(h);
}

fn gamma_finish(e: &E, h: u64) {
    let Some(g) = GAMMA.with(h, |g| std::mem::replace(&mut g.finished, true)) else {
        return;
    };
    if g {
        return;
    }
    let Some(s) = GAMMA.with(h, |g| (g.vp, g.set)) else {
        return;
    };
    e.viewport_active(s.0, false);
    e.set_active(s.1, false);
    if e.call(op::GUI_FOCUSED_IS, &[Arg::P(s.1)]).b() {
        e.gui_focus(0);
    }
    let gamma = e.gamma_get();
    e.v(op::SAVE_GAMMA, &[Arg::F(gamma)]);
    e.v(op::BASE_GAMMA_DONE, &[]);
}

pub fn gamma_update(e: &E, h: u64) {
    let Some((finished, was_down)) = GAMMA.with(h, |g| (g.finished, g.mouse_was_down)) else {
        return;
    };
    if finished {
        return;
    }
    if e.tr.is_some()
        && !GAMMA
            .with(h, |g| gate(&mut g.awaiting_draw, true))
            .unwrap_or(false)
    {
        return;
    }
    let m = e.call(op::MOUSE_POLL, &[]);
    if m.u[0] != 0 {
        let a = [
            Arg::I(m.u[1] as i32 as i64),
            Arg::I(m.u[2] as i32 as i64),
            Arg::I(m.u[3] as i32 as i64),
            Arg::I(m.u[4] as i32 as i64),
        ];
        e.v(op::GUI_MOUSE_POS, &a);
        let down = m.u[5] != 0;
        match screens::mouse_edge(down, was_down) {
            1 => e.v(op::CLICK, &[Arg::B(true)]),
            2 => e.v(op::CLICK, &[Arg::B(false)]),
            _ => {}
        }
        GAMMA.with(h, |g| g.mouse_was_down = down);
    }
    let key = match e.tr.and_then(Tracer::forced_key) {
        Some(k) => k,
        None => e.call(op::KEY_POLL, &[]).u[0] as i32 as i64,
    };
    if let Some(t) = e.tr {
        t.key(key);
    }
    if matches!(key, KEY_RETURN | KEY_ESCAPE | KEY_SPACE) {
        gamma_finish(e, h);
    }
}

pub fn gamma_draw(e: &E, h: u64) {
    let Some(g) = GAMMA.with(h, |g| g.clone()) else {
        return;
    };
    if g.finished {
        return;
    }
    if e.tr.is_some()
        && !GAMMA
            .with(h, |g| gate(&mut g.awaiting_draw, false))
            .unwrap_or(false)
    {
        return;
    }
    if g.bg != 0 {
        e.draw_gfx(g.set, g.bg, [0.0; 3], g.screen, WHITE);
    }
    if g.checker != 0 {
        let pos = [g.checker_pos[0], g.checker_pos[1], 0.1];
        e.draw_gfx(g.set, g.checker, pos, g.checker_size, WHITE);
    }
    if g.font != 0 {
        for (i, row) in g.rows.iter().enumerate() {
            let a = [
                Arg::P(g.set),
                Arg::P(g.font),
                Arg::F(g.instr_pos[0]),
                Arg::F(ROW_H.mul_add(i as f32, g.instr_pos[1])),
                Arg::F(0.2),
                Arg::F(FONT_H),
                Arg::F(1.0),
                Arg::F(1.0),
                Arg::F(1.0),
                Arg::F(1.0),
                Arg::W(row),
            ];
            e.v(op::DRAW_FONT, &a);
        }
    }
}

/// Widget callback: `tag` is `TAG_SLIDER` or `TAG_BUTTON`.
pub fn gamma_callback(e: &E, h: u64, tag: i64) -> bool {
    match tag {
        TAG_SLIDER => {
            let Some(slider) = GAMMA.with(h, |g| g.slider) else {
                return true;
            };
            let r: Res = e.call(op::SLIDER_GET, &[Arg::P(slider)]);
            let gamma = screens::slider_to_gamma(r.u[0] as i32, r.u[1] as i32);
            e.v(op::GAMMA_SET, &[Arg::F(gamma)]);
        }
        _ => gamma_finish(e, h),
    }
    true
}
