//! `cLuxMainMenu_Options`: the options window, built and driven from Rust over the host ops.

use super::host::{self as h, Args, DOWN, LEFT, RIGHT, Set, UP, Wd, call, msg, op, tr};
use super::options::{self as o, Range, Tip, TipAct};
use super::store::{Cb, CbData, Kind, lock, token};
use crate::obj::{Arena, Handle};
use std::sync::{LazyLock, Mutex};

const LAST_WINDOW: i32 = 7;
const KEY_CONFIG_WINDOW: i32 = 2;

pub const WINDOW_UPDATE: u16 = 1;
pub const PRESS_OK: u16 = 2;
pub const PRESS_CANCEL: u16 = 3;
pub const UI_PRESS_CANCEL: u16 = 4;
pub const TAB_CHANGE: u16 = 5;
pub const OPTION_OVER: u16 = 6;
pub const OPTION_CHANGE: u16 = 7;
pub const GAMMA_MOVE: u16 = 8;
pub const HDR_MOVE: u16 = 9;
pub const MOUSE_MOVE: u16 = 10;
pub const PAD_MOVE: u16 = 11;
pub const SOUND_MOVE: u16 = 12;
pub const TOGGLE_GFX: u16 = 13;
pub const PRESS_KEYCONFIG: u16 = 14;
pub const MSGBOX: u16 = 15;
pub const LANGUAGE: u16 = 16;

/// Fixed ASCII buffer shared with the C++ host.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    n: u32,
    b: [u8; 60],
}

impl Default for Str {
    fn default() -> Self {
        Str { n: 0, b: [0; 60] }
    }
}

impl Str {
    pub fn bytes(&self) -> &[u8] {
        self.b.get(..self.n as usize).unwrap_or(&[])
    }

    pub fn from_text(s: &str) -> Str {
        let mut r = Str::default();
        for (d, &c) in r.b.iter_mut().zip(s.as_bytes()) {
            *d = c;
            r.n += 1;
        }
        r
    }
}

/// `cResourceVarsObject` of the initial values and the engine's option block, as one record.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Vals {
    pub show_subtitles: i32,
    pub show_effect_subtitles: i32,
    pub show_hints: i32,
    pub show_death_hints: i32,
    pub show_crosshair: i32,
    pub focus_icon_style: i32,
    pub show_commentary: i32,
    pub res_x: i32,
    pub res_y: i32,
    pub display: i32,
    pub fullscreen: i32,
    pub vsync: i32,
    pub show_fps: i32,
    pub paper_white: f32,
    pub peak_nits: f32,
    pub gui_scale: i32,
    pub tex_quality: i32,
    pub tex_filter: i32,
    pub aniso: f32,
    pub edge_smooth: i32,
    pub shadows: i32,
    pub shadow_quality: i32,
    pub shadow_res: i32,
    pub parallax: i32,
    pub world_reflection: i32,
    pub refraction: i32,
    pub ssao: i32,
    pub ssao_samples: i32,
    pub ssao_res: i32,
    pub bloom: i32,
    pub image_trail: i32,
    pub sepia: i32,
    pub radial_blur: i32,
    pub insanity: i32,
    pub gamma: f32,
    pub invert_mouse: i32,
    pub smooth_mouse: i32,
    pub mouse_sens: f32,
    pub pad_invert: i32,
    pub pad_sens: f32,
    pub volume: f32,
    pub sound_dev: i32,
    pub lang: Str,
    pub renderer: Str,
    pub color: Str,
}

#[derive(Clone, Copy, Default)]
struct Ws {
    window: Wd,
    ok: Wd,
    cancel: Wd,
    tab_game: Wd,
    tab_gfx: Wd,
    tab_input: Wd,
    tab_sound: Wd,
    shoulder: [Wd; 2],
    tip: Wd,
    subtitles: Wd,
    effect_subtitles: Wd,
    language: Wd,
    hints: Wd,
    death_hints: Wd,
    crosshair: Wd,
    focus_style: Wd,
    commentary: Wd,
    basic: Wd,
    resolution: Wd,
    fullscreen: Wd,
    vsync: Wd,
    gui_scale: Wd,
    show_fps: Wd,
    renderer: Wd,
    color: Wd,
    l_paper: Wd,
    s_paper: Wd,
    l_peak: Wd,
    s_peak: Wd,
    tex_size: Wd,
    toggle: Wd,
    advanced: Wd,
    tex_filter: Wd,
    shadows: Wd,
    shadow_quality: Wd,
    shadow_res: Wd,
    world_reflection: Wd,
    refraction: Wd,
    aniso: Wd,
    parallax: Wd,
    edge: Wd,
    bloom: Wd,
    trail: Wd,
    sepia: Wd,
    radial: Wd,
    insanity: Wd,
    ssao: Wd,
    ssao_samples: Wd,
    ssao_res: Wd,
    l_gamma: Wd,
    s_gamma: Wd,
    invert: Wd,
    smooth: Wd,
    l_mouse: Wd,
    s_mouse: Wd,
    pad_invert: Wd,
    l_pad: Wd,
    s_pad: Wd,
    keyconfig: Wd,
    l_volume: Wd,
    s_volume: Wd,
    snd_dev: Wd,
}

struct Opt {
    set: Set,
    ws: Ws,
    show_commentary: bool,
    lang_files: Vec<h::S>,
    screen_sizes: Vec<(i32, i32, i32)>,
    sound_ids: Vec<i32>,
    data: Vec<(bool, h::S)>,
    tip: Tip,
    tip_updated: bool,
    tip_widget: Wd,
    setting_initial: bool,
    key_config_open: bool,
}

#[derive(Default)]
struct Store {
    opts: Arena<Opt>,
    initial: Vals,
    initial_set: bool,
}

static STORE: LazyLock<Mutex<Store>> = LazyLock::new(|| Mutex::new(Store::default()));

fn with<R>(me: u64, f: impl FnOnce(&mut Opt) -> R) -> Option<R> {
    let h = Handle::from_bits(me)?;
    lock(&STORE).opts.get_mut(h).map(f)
}

fn tok(me: u64, id: u16) -> usize {
    token(Cb {
        kind: Kind::Options,
        owner: me,
        aux: 0,
        id,
    })
}

fn p3(x: f32, y: f32, z: f32) -> [f32; 3] {
    [x, y, z]
}

fn add3(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
}

const NEG: (f32, f32) = (-1.0, -1.0);
const ZERO: (f32, f32) = (0.0, 0.0);

fn translate_or(cat: &str, name: &str, fallback: &str) -> h::S {
    let t = tr(cat, name);
    if t.is_empty() { h::w(fallback) } else { t }
}

fn b(v: i32) -> bool {
    v != 0
}

fn range(r: Range) -> (f32, f32, f32) {
    o::range(r)
}

pub fn create(set: usize) -> u64 {
    let show = call(op::SHOW_COMMENTARY_CFG, &mut Args::new()) != 0;
    let opt = Opt {
        set: Set(set),
        ws: Ws::default(),
        show_commentary: show,
        lang_files: Vec::new(),
        screen_sizes: Vec::new(),
        sound_ids: Vec::new(),
        data: Vec::new(),
        tip: Tip {
            restart: false,
            text_reset: false,
        },
        tip_updated: true,
        tip_widget: Wd(0),
        setting_initial: false,
        key_config_open: false,
    };
    lock(&STORE).opts.insert(opt).map_or(0, Handle::to_bits)
}

pub fn free(me: u64) {
    if let Some(h) = Handle::from_bits(me) {
        lock(&STORE).opts.remove(h);
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Input {
    Check,
    Combo,
    Slider,
    Button,
    Other,
}

fn set_up_input(me: u64, label: Wd, input: Wd, kind: Input, needs_restart: bool, text: h::S) {
    let ud = with(me, |o| {
        o.data.push((needs_restart, text));
        o.data.len()
    })
    .unwrap_or(0);
    if !label.is_null() {
        label.set_user_data(ud);
        label.on(msg::MOUSE_MOVE, tok(me, OPTION_OVER));
        label.on(msg::GET_NAV_FOCUS, tok(me, OPTION_OVER));
    }
    input.set_user_data(ud);
    input.on(msg::MOUSE_MOVE, tok(me, OPTION_OVER));
    input.on(msg::GET_NAV_FOCUS, tok(me, OPTION_OVER));
    let m = match kind {
        Input::Check => msg::CHECK,
        Input::Combo => msg::SELECTION,
        Input::Slider => msg::SLIDER_MOVE,
        Input::Button => msg::BUTTON_PRESSED,
        Input::Other => return,
    };
    input.on(m, tok(me, OPTION_CHANGE));
}

fn set_up_slider(me: u64, slider: Wd, r: Range, callback: Option<u16>, with_label: bool) -> Wd {
    let (min, step, max) = range(r);
    let (max_value, bar) = o::slider_setup(min, max, step);
    slider.slider_set_max(max_value);
    slider.slider_set_bar(bar);
    if let Some(cb) = callback {
        slider.on(msg::SLIDER_MOVE, tok(me, cb));
    }
    if !with_label {
        return Wd(0);
    }
    let set = with(me, |o| o.set).unwrap_or(Set(0));
    let label = set.label(p3(slider.size().0 * 0.5, 2.0, 1.0), NEG, &[], slider);
    label.set_center_align();
    label
}

fn slider_get(s: Wd, r: Range) -> f32 {
    let (min, _, max) = range(r);
    o::slider_to_value(s.slider_value(), s.slider_max(), min, max)
}

fn slider_put(s: Wd, v: f32, callback: bool, r: Range) {
    let (min, _, max) = range(r);
    s.slider_set(o::slider_to_int(v, min, max, s.slider_max()), callback);
}

fn slider_label(l: Wd, v: f32, r: Range) {
    let (min, _, max) = range(r);
    l.set_text(&o::slider_label(v, min, max));
}

fn volume_label(l: Wd, v: f32) {
    let (min, _, max) = range(Range::Volume);
    l.set_text(&o::slider_label(v * 10.0, min * 10.0, max * 10.0));
}

pub fn create_gui(me: u64) -> usize {
    let Some((set, show_commentary)) = with(me, |o| (o.set, o.show_commentary)) else {
        return 0;
    };
    let mut ws = Ws::default();
    let (left, upper) = (30.0f32, 10.0f32);
    let size = (620.0f32, 460.0f32);

    ws.window = set.window(p3(0.0, 0.0, 5.0), size, &tr("MainMenu", "Options"));
    ws.window.on(msg::ON_UPDATE, tok(me, WINDOW_UPDATE));
    ws.window.set_static(true);

    let (bw, sep) = (80.0f32, 3.0f32);
    let wsize = ws.window.size();
    let mut x = wsize.0 - bw * 2.0 - sep - 5.0;
    let y = wsize.1 - 25.0 - 10.0;
    ws.ok = set.button(p3(x, y, 1.0), (bw, 30.0), &tr("MainMenu", "OK"), ws.window);
    ws.ok.on(msg::BUTTON_PRESSED, tok(me, PRESS_OK));
    x += bw + sep;
    ws.cancel = set.button(
        p3(x, y, 1.0),
        (bw, 30.0),
        &tr("MainMenu", "Cancel"),
        ws.window,
    );
    ws.cancel.on(msg::BUTTON_PRESSED, tok(me, PRESS_CANCEL));
    ws.cancel.on(msg::UI_BUTTON, tok(me, UI_PRESS_CANCEL));
    ws.cancel.set_global_ui(true);
    ws.ok.set_nav(RIGHT, ws.cancel);
    ws.cancel.set_nav(LEFT, ws.ok);

    let mut pos = p3(left, 35.0 + upper, 1.0);
    let tab_size = (
        size.0 - left * 2.0,
        size.1 - upper * 3.0 - 30.0 - 25.0 - 20.0,
    );
    let frame = set.tabframe(pos, tab_size, ws.window);
    frame.set_global_ui(true);
    frame.on(msg::SELECTION, tok(me, TAB_CHANGE));

    ws.tab_game = frame.tab_add(&tr("MainMenu", "OptionsGame"));
    ws.tab_gfx = frame.tab_add(&tr("MainMenu", "OptionsGraphics"));
    ws.tab_input = frame.tab_add(&tr("MainMenu", "OptionsInput"));
    ws.tab_sound = frame.tab_add(&tr("MainMenu", "OptionsSound"));

    let mut image = (upper * 2.0, upper * 2.0);
    image.0 *= 1.5;
    pos[1] -= 1.0;
    pos[0] -= upper * 1.3;
    ws.shoulder[0] = set.image("gamepad_lb.tga", pos, image, ws.window, "LB Tip");
    pos[0] = ws.window.size().0 - upper * 5.0;
    ws.shoulder[1] = set.image("gamepad_rb.tga", pos, image, ws.window, "RB Tip");

    let tabs = frame.tab_num();
    for i in 0..tabs {
        let tab = frame.tab_get(i);
        if i > 0 {
            tab.tab_label()
                .set_nav(LEFT, frame.tab_get(i - 1).tab_label());
        }
        if i + 1 < tabs {
            tab.tab_label()
                .set_nav(RIGHT, frame.tab_get(i + 1).tab_label());
        }
    }

    add_graphics(me, set, &mut ws);
    add_sound(me, set, &mut ws);
    add_game(me, set, &mut ws, show_commentary);
    add_input(me, set, &mut ws);

    let tip_pos = p3(left, ws.window.size().1 - 25.0 - 10.0, 1.0);
    ws.tip = set.label(
        add3(tip_pos, p3(10.0, 0.0, 0.0)),
        (400.0, 30.0),
        &[],
        ws.window,
    );
    ws.tip.set_font_color([1.0, 1.0, 1.0, 1.0]);
    ws.tip.set_word_wrap(true);
    ws.tip.set_clip(true);
    ws.tip.set_scroll_speed(4.0);
    ws.tip.set_draw_bg(true);
    ws.tip.set_bg_color([0.0, 0.0, 0.0, 0.5]);

    let window = ws.window;
    with(me, |o| o.ws = ws);
    window.0
}

fn add_game(me: u64, set: Set, ws: &mut Ws, show_commentary: bool) {
    let border = 15.0f32;
    let mut pos = p3(border, 6.0 + border + 10.0, 0.1);
    let tab = ws.tab_game;
    let t = |n: &str| tr("OptionsMenu", n);

    let label = set.label(pos, NEG, &t("Language"), tab);
    ws.language = set.combo(
        add3(pos, p3(label.size().0 + 5.0, -2.0, 0.0)),
        (150.0, 25.0),
        tab,
    );
    set_up_input(
        me,
        label,
        ws.language,
        Input::Combo,
        false,
        t("LanguageTip"),
    );
    ws.language.on(msg::SELECTION, tok(me, LANGUAGE));
    pos[1] += ws.language.size().1 + 15.0;

    ws.subtitles = set.checkbox(pos, ZERO, &t("ShowSubtitles"), tab);
    set_up_input(
        me,
        Wd(0),
        ws.subtitles,
        Input::Check,
        false,
        t("ShowSubtitlesTip"),
    );

    ws.effect_subtitles = set.checkbox(
        add3(pos, p3(ws.subtitles.size().0 + 15.0, 0.0, 0.0)),
        ZERO,
        &t("ShowEffectSubtitles"),
        tab,
    );
    set_up_input(
        me,
        Wd(0),
        ws.effect_subtitles,
        Input::Check,
        false,
        t("ShowEffectSubtitlesTip"),
    );
    pos[1] += ws.subtitles.size().1 + 15.0;

    ws.hints = set.checkbox(pos, ZERO, &t("ShowHints"), tab);
    set_up_input(me, Wd(0), ws.hints, Input::Check, false, t("ShowHintsTip"));
    pos[1] += ws.hints.size().1 + 15.0;

    ws.death_hints = set.checkbox(pos, ZERO, &t("ShowDeathHints"), tab);
    set_up_input(
        me,
        Wd(0),
        ws.death_hints,
        Input::Check,
        false,
        t("ShowDeathHintsTip"),
    );
    pos[1] += ws.death_hints.size().1 + 15.0;

    ws.crosshair = set.checkbox(pos, ZERO, &t("ShowCrosshair"), tab);
    set_up_input(
        me,
        Wd(0),
        ws.crosshair,
        Input::Check,
        false,
        t("ShowCrosshairTip"),
    );
    pos[1] += ws.crosshair.size().1 + 15.0;

    let label = set.label(pos, NEG, &t("FocusIconStyle"), tab);
    ws.focus_style = set.combo(
        add3(pos, p3(label.size().0 + 5.0, -2.0, 0.0)),
        (150.0, 25.0),
        tab,
    );
    set_up_input(
        me,
        label,
        ws.focus_style,
        Input::Combo,
        false,
        t("FocusIconStyleTip"),
    );
    ws.focus_style.combo_add(&t("FocusIconStyleDefault"));
    ws.focus_style.combo_add(&t("FocusIconStyleSimple"));
    pos[1] += label.size().1 + 15.0;

    if show_commentary {
        ws.commentary = set.checkbox(pos, ZERO, &t("Commentary"), tab);
        set_up_input(
            me,
            Wd(0),
            ws.commentary,
            Input::Check,
            false,
            t("CommentaryTip"),
        );
    }

    populate_languages(me, ws.language);

    ws.language.set_nav(DOWN, ws.subtitles);
    ws.subtitles.set_nav(DOWN, ws.hints);
    ws.effect_subtitles.set_nav(DOWN, ws.hints);
    ws.hints.set_nav(DOWN, ws.death_hints);
    ws.death_hints.set_nav(DOWN, ws.crosshair);
    ws.crosshair.set_nav(DOWN, ws.focus_style);
    let last = if show_commentary {
        ws.focus_style.set_nav(DOWN, ws.commentary);
        ws.commentary.set_nav(DOWN, ws.ok);
        ws.commentary
    } else {
        ws.focus_style.set_nav(DOWN, ws.ok);
        ws.focus_style
    };
    tab.set_user_data(last.0);
    tab.tab_label().set_user_data(ws.language.0);

    ws.subtitles.set_nav(UP, ws.language);
    ws.effect_subtitles.set_nav(UP, ws.language);
    ws.hints.set_nav(UP, ws.subtitles);
    ws.death_hints.set_nav(UP, ws.hints);
    ws.crosshair.set_nav(UP, ws.death_hints);
    ws.focus_style.set_nav(UP, ws.crosshair);
    if show_commentary {
        ws.commentary.set_nav(UP, ws.focus_style);
    }
    ws.subtitles.set_nav(RIGHT, ws.effect_subtitles);
    ws.effect_subtitles.set_nav(LEFT, ws.subtitles);
}

fn populate_languages(me: u64, combo: Wd) {
    combo.combo_clear();
    let n = call(op::LANG_FILES, &mut Args::new()) as i32;
    let files: Vec<h::S> = (0..n).map(|i| h::host_list_str(0, i)).collect();
    for f in &files {
        let end = f
            .iter()
            .rposition(|&c| c == u32::from(b'.'))
            .unwrap_or(f.len());
        let stem = f.get(..end).unwrap_or(&[]);
        let lower = crate::string::wide_to_utf8(stem).to_ascii_lowercase();
        combo.combo_add(&tr("Languages", &String::from_utf8_lossy(&lower)));
    }
    with(me, |o| o.lang_files = files);
}

fn add_graphics(me: u64, set: Set, ws: &mut Ws) {
    let border = 15.0f32;
    let pos = p3(border - 5.0, 6.0 + border, 0.1);
    let tab = ws.tab_gfx;

    ws.basic = set.dummy(pos, tab);
    ws.basic.set_visible(true);
    ws.basic.set_enabled(true);
    add_basic_gfx(me, set, ws);

    ws.advanced = set.dummy(pos, tab);
    ws.advanced.set_visible(false);
    ws.advanced.set_enabled(false);
    add_advanced_gfx(me, set, ws);

    let advanced = tr("OptionsMenu", "AdvancedOptions");
    ws.toggle = set.button(p3(0.0, 0.0, 0.0), (0.0, 25.0), &advanced, tab);
    ws.toggle.on(msg::BUTTON_PRESSED, tok(me, TOGGLE_GFX));
    let mut width = 0.0f32;
    for _ in 0..2 {
        let len = ws.toggle.text_length() + 10.0;
        if width < len {
            width = len;
        }
    }
    ws.toggle.set_size(width, ws.toggle.size().1);
    let ts = tab.size();
    ws.toggle.set_pos(p3(ts.0 - width, ts.1 - 50.0, 0.0 + 2.0));
    set_up_input(me, Wd(0), ws.toggle, Input::Button, true, Vec::new());

    ws.toggle.set_nav(DOWN, ws.ok);
    ws.toggle.set_nav(UP, ws.s_gamma);
    ws.s_gamma.set_nav(DOWN, ws.toggle);
    ws.refraction.set_nav(DOWN, ws.toggle);

    tab.set_user_data(ws.toggle.0);
    tab.tab_label().set_user_data(ws.resolution.0);
}

fn add_basic_gfx(me: u64, set: Set, ws: &mut Ws) {
    let dummy = ws.basic;
    let mut pos = p3(0.0, 6.0, 0.1);
    let t = |n: &str| tr("OptionsMenu", n);

    let group = set.group(pos, ZERO, &t("Screen"), dummy);
    {
        let border = 15.0f32;
        group.set_size(dummy.parent().size().0 - border - border, 140.0);
        let mut inner = p3(border, border, 0.1);

        let label = set.label(inner, NEG, &t("Resolution"), group);
        ws.resolution = set.combo(
            add3(label.local_pos(), p3(0.0, label.size().1 + 5.0, 0.0)),
            (175.0, 25.0),
            group,
        );
        set_up_input(
            me,
            label,
            ws.resolution,
            Input::Combo,
            true,
            t("ResolutionTip"),
        );
        inner[0] += ws.resolution.size().0 + 100.0;

        ws.fullscreen = set.checkbox(add3(inner, p3(0.0, 2.0, 0.0)), NEG, &t("FullScreen"), group);
        set_up_input(
            me,
            Wd(0),
            ws.fullscreen,
            Input::Check,
            true,
            t("FullScreenTip"),
        );

        ws.vsync = set.checkbox(
            add3(inner, p3(0.0, ws.fullscreen.size().1 + 10.0, 0.0)),
            ZERO,
            &t("VSync"),
            group,
        );
        set_up_input(me, Wd(0), ws.vsync, Input::Check, false, t("VSyncTip"));

        let gui_scale_pos = p3(
            border,
            ws.vsync.local_pos()[1] + ws.vsync.size().1 + 15.0,
            0.1,
        );
        let label = set.label(gui_scale_pos, NEG, &t("GuiScale"), group);
        ws.gui_scale = set.combo(
            add3(label.local_pos(), p3(label.size().0 + 10.0, 0.0, 0.0)),
            (70.0, 25.0),
            group,
        );
        set_up_input(
            me,
            label,
            ws.gui_scale,
            Input::Combo,
            true,
            t("GuiScaleTip"),
        );

        let right_x = inner[0];
        let label = set.label(
            p3(right_x, gui_scale_pos[1], 0.1),
            NEG,
            &translate_or("OptionsMenu", "Renderer", "Renderer"),
            group,
        );
        ws.renderer = set.combo(
            add3(label.local_pos(), p3(label.size().0 + 10.0, 0.0, 0.0)),
            (110.0, 25.0),
            group,
        );
        set_up_input(
            me,
            label,
            ws.renderer,
            Input::Combo,
            true,
            translate_or(
                "OptionsMenu",
                "RendererTip",
                "Graphics API. OpenGL is the default; Vulkan is experimental.",
            ),
        );
        ws.renderer.combo_add(&h::w("OpenGL"));
        ws.renderer.combo_add(&h::w("Vulkan"));

        let fps_pos = p3(border, gui_scale_pos[1] + ws.gui_scale.size().1 + 10.0, 0.1);
        let label = set.label(
            p3(right_x, fps_pos[1], 0.1),
            NEG,
            &translate_or("OptionsMenu", "ColorOutput", "Color output"),
            group,
        );
        ws.color = set.combo(
            add3(label.local_pos(), p3(label.size().0 + 10.0, 0.0, 0.0)),
            (130.0, 25.0),
            group,
        );
        set_up_input(
            me,
            label,
            ws.color,
            Input::Combo,
            false,
            translate_or(
                "OptionsMenu",
                "ColorOutputTip",
                "SDR is standard. HDR10 and scRGB need an HDR display, P3 a wide-gamut one. Vulkan only.",
            ),
        );
        ws.color.combo_add(&h::w("SDR"));
        ws.color.combo_add(&h::w("HDR10"));
        ws.color.combo_add(&h::w("scRGB HDR"));
        ws.color.combo_add(&h::w("Wide color (P3)"));
        ws.show_fps = set.checkbox(
            fps_pos,
            ZERO,
            &translate_or("OptionsMenu", "ShowFPS", "Show FPS"),
            group,
        );
        set_up_input(
            me,
            Wd(0),
            ws.show_fps,
            Input::Check,
            false,
            translate_or(
                "OptionsMenu",
                "ShowFPSTip",
                "Show a frames-per-second counter during gameplay.",
            ),
        );
    }

    pos[1] += group.size().1 + 15.0;

    let label = set.label(pos, NEG, &t("TexQuality"), dummy);
    ws.tex_size = set.combo(
        add3(label.local_pos(), p3(0.0, label.size().1 + 5.0, 0.0)),
        (100.0, 25.0),
        dummy,
    );
    set_up_input(
        me,
        label,
        ws.tex_size,
        Input::Combo,
        true,
        t("TexQualityTip"),
    );
    ws.tex_size.combo_clear();
    for n in ["Low", "Medium", "High"] {
        ws.tex_size.combo_add(&t(n));
    }

    pos[0] += 140.0;
    let label = set.label(pos, NEG, &t("Gamma"), dummy);
    {
        let mut lp = p3(0.0, label.size().1 + 5.0, 0.0);
        let img = set.image("menu_gamma.tga", lp, NEG, label, "");
        set_up_input(me, Wd(0), img, Input::Other, false, t("GammaInstructions"));
        lp[1] += img.size().1 + 5.0;

        ws.s_gamma = set.slider(lp, (img.size().0, 20.0), label);
        set_up_input(
            me,
            label,
            ws.s_gamma,
            Input::Slider,
            false,
            t("GammaInstructions"),
        );
        ws.l_gamma = set_up_slider(me, ws.s_gamma, Range::Gamma, Some(GAMMA_MOVE), true);
        ws.s_gamma.slider_set_bar(4);
    }

    {
        let tp = ws.tex_size.local_pos();
        let slider_pos = add3(tp, p3(0.0, ws.tex_size.size().1 + 15.0, 0.0));
        let label = set.label(
            slider_pos,
            NEG,
            &translate_or("OptionsMenu", "HdrPaperWhite", "HDR white (nits)"),
            dummy,
        );
        ws.s_paper = set.slider(p3(0.0, label.size().1 + 5.0, 0.0), (120.0, 20.0), label);
        set_up_input(
            me,
            label,
            ws.s_paper,
            Input::Slider,
            false,
            translate_or(
                "OptionsMenu",
                "HdrPaperWhiteTip",
                "Brightness of white in nits when an HDR output is active.",
            ),
        );
        ws.l_paper = set_up_slider(me, ws.s_paper, Range::PaperWhite, Some(HDR_MOVE), true);

        let label = set.label(
            add3(slider_pos, p3(0.0, 55.0, 0.0)),
            NEG,
            &translate_or("OptionsMenu", "HdrPeak", "HDR peak (nits)"),
            dummy,
        );
        ws.s_peak = set.slider(p3(0.0, label.size().1 + 5.0, 0.0), (120.0, 20.0), label);
        set_up_input(
            me,
            label,
            ws.s_peak,
            Input::Slider,
            false,
            translate_or(
                "OptionsMenu",
                "HdrPeakTip",
                "Peak brightness of your HDR display in nits.",
            ),
        );
        ws.l_peak = set_up_slider(me, ws.s_peak, Range::Peak, Some(HDR_MOVE), true);
    }

    ws.renderer.set_nav(LEFT, ws.gui_scale);
    ws.renderer.set_nav(UP, ws.vsync);
    ws.renderer.set_nav(DOWN, ws.color);
    ws.color.set_nav(LEFT, ws.show_fps);
    ws.color.set_nav(UP, ws.renderer);
    ws.s_paper.set_nav(UP, ws.tex_size);
    ws.s_paper.set_nav(RIGHT, ws.s_gamma);
    ws.s_paper.set_nav(DOWN, ws.s_peak);
    ws.s_peak.set_nav(UP, ws.s_paper);
    ws.s_peak.set_nav(RIGHT, ws.s_gamma);
    ws.s_gamma.set_nav(LEFT, ws.s_paper);
    ws.gui_scale.set_nav(RIGHT, ws.renderer);
    ws.show_fps.set_nav(RIGHT, ws.color);
    ws.vsync.set_nav(RIGHT, ws.renderer);

    ws.resolution.set_nav(DOWN, ws.tex_size);
    ws.resolution.set_nav(RIGHT, ws.fullscreen);

    ws.fullscreen.set_nav(LEFT, ws.resolution);
    ws.fullscreen.set_nav(DOWN, ws.vsync);

    ws.vsync.set_nav(LEFT, ws.resolution);
    ws.vsync.set_nav(UP, ws.fullscreen);
    ws.vsync.set_nav(DOWN, ws.gui_scale);

    ws.gui_scale.set_nav(UP, ws.vsync);
    ws.gui_scale.set_nav(DOWN, ws.show_fps);

    ws.show_fps.set_nav(UP, ws.gui_scale);
    ws.show_fps.set_nav(DOWN, ws.tex_size);

    ws.tex_size.set_nav(UP, ws.show_fps);
    ws.tex_size.set_nav(DOWN, ws.s_gamma);

    ws.s_gamma.set_nav(UP, ws.tex_size);
}

struct Combo<'a> {
    label: (&'a str, &'a str),
    width: f32,
    restart: bool,
    tip: &'a str,
}

fn labeled_combo(me: u64, set: Set, group: Wd, at: [f32; 3], c: &Combo) -> Wd {
    let label = set.label(at, NEG, &tr(c.label.0, c.label.1), group);
    let combo = set.combo(p3(0.0, label.size().1 + 5.0, 0.0), (c.width, 25.0), label);
    set_up_input(
        me,
        label,
        combo,
        Input::Combo,
        c.restart,
        tr("OptionsMenu", c.tip),
    );
    combo
}

fn add_advanced_gfx(me: u64, set: Set, ws: &mut Ws) {
    let dummy = ws.advanced;
    let mut pos = p3(0.0, 6.0, 0.1);
    let item_sep = 180.0f32;
    let t = |n: &str| tr("OptionsMenu", n);

    let frame = set.frame(p3(0.0, 0.0, 1.0), (550.0, 275.0), dummy);
    frame.frame_draw_bg(false);
    let group_size = (520.0f32, 70.0f32);

    let group = set.group(pos, group_size, &t("Material"), frame);
    {
        let border = 15.0f32;
        let mut inner = p3(border, border, 0.1);
        ws.tex_filter = labeled_combo(
            me,
            set,
            group,
            inner,
            &Combo {
                label: ("OptionsMenu", "TexFilter"),
                width: 150.0,
                restart: false,
                tip: "TexFilterTip",
            },
        );
        inner[0] += item_sep;
        ws.aniso = labeled_combo(
            me,
            set,
            group,
            inner,
            &Combo {
                label: ("OptionsMenu", "Anisotropy"),
                width: 100.0,
                restart: false,
                tip: "AnisotropyTip",
            },
        );
        inner[0] += item_sep;
        ws.parallax = labeled_combo(
            me,
            set,
            group,
            inner,
            &Combo {
                label: ("Launcher", "Parallax"),
                width: 100.0,
                restart: true,
                tip: "ParallaxQualityTip",
            },
        );
    }
    pos[1] += group.size().1 + 10.0;

    let group = set.group(pos, group_size, &t("Shadows"), frame);
    {
        let border = 15.0f32;
        let mut inner = p3(border, border, 0.1);
        ws.shadows = set.checkbox(inner, ZERO, &t("Shadows"), group);
        set_up_input(me, Wd(0), ws.shadows, Input::Check, false, t("ShadowsTip"));
        inner[0] += item_sep;
        ws.shadow_quality = labeled_combo(
            me,
            set,
            group,
            inner,
            &Combo {
                label: ("OptionsMenu", "ShadowQuality"),
                width: 100.0,
                restart: true,
                tip: "ShadowQualityTip",
            },
        );
        inner[0] += item_sep;
        ws.shadow_res = labeled_combo(
            me,
            set,
            group,
            inner,
            &Combo {
                label: ("OptionsMenu", "ShadowRes"),
                width: 100.0,
                restart: true,
                tip: "ShadowResTip",
            },
        );

        ws.shadow_quality.combo_clear();
        ws.shadow_res.combo_clear();
        ws.parallax.combo_clear();
        for n in ["Low", "Medium", "High"] {
            ws.shadow_quality.combo_add(&t(n));
            ws.shadow_res.combo_add(&t(n));
        }
        ws.parallax.combo_add(&tr("Launcher", "Off"));
        ws.parallax.combo_add(&tr("Launcher", "On"));
    }
    pos[1] += group.size().1 + 10.0;

    let group = set.group(pos, group_size, &t("PostEffects"), frame);
    {
        let border = 15.0f32;
        let input_sep = 10.0f32;
        let mut inner = p3(border, border, 0.1);

        ws.bloom = set.checkbox(inner, ZERO, &t("Bloom"), group);
        set_up_input(me, Wd(0), ws.bloom, Input::Check, false, t("BloomTip"));
        inner[1] += ws.bloom.size().1 + input_sep;

        ws.trail = set.checkbox(inner, ZERO, &t("ImageTrail"), group);
        set_up_input(me, Wd(0), ws.trail, Input::Check, false, t("ImageTrailTip"));
        inner[1] = border;
        inner[0] += item_sep;

        ws.sepia = set.checkbox(inner, ZERO, &t("Sepia"), group);
        set_up_input(me, Wd(0), ws.sepia, Input::Check, false, t("SepiaTip"));
        inner[1] += ws.sepia.size().1 + input_sep;

        ws.radial = set.checkbox(inner, ZERO, &t("RadialBlur"), group);
        set_up_input(
            me,
            Wd(0),
            ws.radial,
            Input::Check,
            false,
            t("RadialBlurTip"),
        );
        inner[1] = border;
        inner[0] += item_sep;

        ws.insanity = set.checkbox(inner, ZERO, &t("Insanity"), group);
        set_up_input(
            me,
            Wd(0),
            ws.insanity,
            Input::Check,
            false,
            t("InsanityTip"),
        );
    }
    pos[1] += group.size().1 + 10.0;

    let group = set.group(pos, group_size, &t("SSAO"), frame);
    {
        let border = 15.0f32;
        let mut inner = p3(border, border, 0.1);
        ws.ssao = set.checkbox(inner, ZERO, &t("SSAO"), group);
        set_up_input(me, Wd(0), ws.ssao, Input::Check, true, t("SSAOTip"));
        inner[0] += item_sep;
        ws.ssao_samples = labeled_combo(
            me,
            set,
            group,
            inner,
            &Combo {
                label: ("OptionsMenu", "SSAOSamples"),
                width: 60.0,
                restart: true,
                tip: "SSAOSamplesTip",
            },
        );
        for i in 0..4 {
            ws.ssao_samples
                .combo_add(&h::w(&o::ssao_from_index(i).to_string()));
        }
        inner[0] += item_sep;
        ws.ssao_res = labeled_combo(
            me,
            set,
            group,
            inner,
            &Combo {
                label: ("OptionsMenu", "SSAOResolution"),
                width: 80.0,
                restart: true,
                tip: "SSAOResolutionTip",
            },
        );
        ws.ssao_res.combo_add(&t("Medium"));
        ws.ssao_res.combo_add(&t("High"));
    }
    pos[1] += group.size().1 + 10.0;

    let group = set.group(pos, group_size, &tr("KeyConfig", "Misc"), frame);
    {
        let border = 15.0f32;
        let mut inner = p3(border, border, 0.1);
        ws.edge = set.checkbox(inner, ZERO, &t("FullscreenSmooth"), group);
        set_up_input(me, Wd(0), ws.edge, Input::Check, true, t("EdgeSmoothTip"));
        inner[1] += ws.edge.size().1 + 10.0;

        ws.refraction = set.checkbox(inner, ZERO, &t("Refraction"), group);
        set_up_input(
            me,
            Wd(0),
            ws.refraction,
            Input::Check,
            true,
            t("RefractionTip"),
        );
        inner[0] += item_sep;
        inner[1] = border;

        let mut text = t("WorldReflection");
        text.extend(h::w(" ("));
        text.extend(t("Water"));
        text.extend(h::w(")"));
        ws.world_reflection = set.checkbox(inner, ZERO, &text, group);
        set_up_input(
            me,
            Wd(0),
            ws.world_reflection,
            Input::Check,
            false,
            t("WorldReflectionTip"),
        );
    }

    ws.tex_filter.set_nav(RIGHT, ws.aniso);
    ws.aniso.set_nav(RIGHT, ws.parallax);
    ws.aniso.set_nav(LEFT, ws.tex_filter);
    ws.parallax.set_nav(LEFT, ws.aniso);
    ws.tex_filter.set_nav(DOWN, ws.shadows);
    ws.aniso.set_nav(DOWN, ws.shadow_quality);
    ws.parallax.set_nav(DOWN, ws.shadow_res);

    ws.shadows.set_nav(UP, ws.tex_filter);
    ws.shadow_quality.set_nav(UP, ws.aniso);
    ws.shadow_res.set_nav(UP, ws.parallax);
    ws.shadows.set_nav(RIGHT, ws.shadow_quality);
    ws.shadow_quality.set_nav(RIGHT, ws.shadow_res);
    ws.shadow_quality.set_nav(LEFT, ws.shadows);
    ws.shadow_res.set_nav(LEFT, ws.shadow_quality);
    ws.shadows.set_nav(DOWN, ws.bloom);
    ws.shadow_quality.set_nav(DOWN, ws.sepia);
    ws.shadow_res.set_nav(DOWN, ws.insanity);

    ws.bloom.set_nav(UP, ws.shadows);
    ws.sepia.set_nav(UP, ws.shadow_quality);
    ws.insanity.set_nav(UP, ws.shadow_res);
    ws.bloom.set_nav(RIGHT, ws.sepia);
    ws.sepia.set_nav(RIGHT, ws.insanity);
    ws.sepia.set_nav(LEFT, ws.bloom);
    ws.insanity.set_nav(LEFT, ws.sepia);
    ws.bloom.set_nav(DOWN, ws.trail);
    ws.sepia.set_nav(DOWN, ws.radial);
    ws.insanity.set_nav(DOWN, ws.radial);
    ws.trail.set_nav(UP, ws.bloom);
    ws.radial.set_nav(UP, ws.sepia);
    ws.trail.set_nav(RIGHT, ws.radial);
    ws.radial.set_nav(LEFT, ws.trail);
    ws.trail.set_nav(DOWN, ws.ssao);
    ws.radial.set_nav(DOWN, ws.ssao_samples);

    ws.ssao.set_nav(UP, ws.trail);
    ws.ssao_samples.set_nav(UP, ws.radial);
    ws.ssao_res.set_nav(UP, ws.radial);
    ws.ssao.set_nav(RIGHT, ws.ssao_samples);
    ws.ssao_samples.set_nav(RIGHT, ws.ssao_res);
    ws.ssao_samples.set_nav(LEFT, ws.ssao);
    ws.ssao_res.set_nav(LEFT, ws.ssao_samples);
    ws.ssao.set_nav(DOWN, ws.edge);
    ws.ssao_samples.set_nav(DOWN, ws.world_reflection);
    ws.ssao_res.set_nav(DOWN, ws.world_reflection);

    ws.edge.set_nav(UP, ws.ssao);
    ws.world_reflection.set_nav(UP, ws.ssao_samples);
    ws.edge.set_nav(RIGHT, ws.world_reflection);
    ws.world_reflection.set_nav(LEFT, ws.edge);
    ws.edge.set_nav(DOWN, ws.refraction);
    ws.world_reflection.set_nav(DOWN, ws.refraction);
    ws.refraction.set_nav(UP, ws.edge);
}

fn add_input(me: u64, set: Set, ws: &mut Ws) {
    let border = 15.0f32;
    let mut pos = p3(border, 6.0 + border + 10.0, 0.1);
    let tab = ws.tab_input;
    let t = |n: &str| tr("OptionsMenu", n);

    ws.invert = set.checkbox(pos, ZERO, &t("InvertMouse"), tab);
    set_up_input(
        me,
        Wd(0),
        ws.invert,
        Input::Check,
        false,
        t("InvertMouseTip"),
    );
    pos[1] += ws.invert.size().1 + 15.0;

    ws.smooth = set.checkbox(pos, ZERO, &t("SmoothMouse"), tab);
    set_up_input(
        me,
        Wd(0),
        ws.smooth,
        Input::Check,
        false,
        t("SmoothMouseTip"),
    );
    pos[1] += ws.smooth.size().1 + 15.0;

    let label = set.label(pos, NEG, &t("MouseSensitivity"), tab);
    ws.s_mouse = set.slider(p3(0.0, label.size().1 + 5.0, 0.0), (100.0, 20.0), label);
    set_up_input(
        me,
        label,
        ws.s_mouse,
        Input::Slider,
        false,
        t("MouseSensitivityTip"),
    );
    ws.l_mouse = set_up_slider(me, ws.s_mouse, Range::Mouse, Some(MOUSE_MOVE), true);
    pos[1] += ws.s_mouse.local_pos()[1] + ws.s_mouse.size().1 + 15.0;

    ws.pad_invert = set.checkbox(pos, ZERO, &t("InvertGamepadLook"), tab);
    set_up_input(
        me,
        Wd(0),
        ws.pad_invert,
        Input::Check,
        false,
        t("InvertGamepadLookTip"),
    );
    pos[1] += ws.pad_invert.size().1 + 15.0;

    let label = set.label(pos, NEG, &t("GamepadLookSensitivity"), tab);
    ws.s_pad = set.slider(p3(0.0, label.size().1 + 5.0, 0.0), (100.0, 20.0), label);
    set_up_input(
        me,
        label,
        ws.s_mouse,
        Input::Slider,
        false,
        t("GamepadLookSensitivityTip"),
    );
    ws.l_pad = set_up_slider(me, ws.s_pad, Range::Gamepad, Some(PAD_MOVE), true);
    pos[1] += ws.s_pad.local_pos()[1] + ws.s_pad.size().1 + 15.0;

    ws.keyconfig = set.button(pos, (0.0, 25.0), &t("KeyConfigButton"), tab);
    let width = ws.keyconfig.text_length();
    ws.keyconfig.set_size(width + 20.0, ws.keyconfig.size().1);
    set_up_input(
        me,
        Wd(0),
        ws.keyconfig,
        Input::Button,
        false,
        t("KeyConfigButtonTip"),
    );
    ws.keyconfig
        .on(msg::BUTTON_PRESSED, tok(me, PRESS_KEYCONFIG));

    tab.set_user_data(ws.keyconfig.0);
    tab.tab_label().set_user_data(ws.invert.0);

    ws.invert.set_nav(DOWN, ws.smooth);
    ws.smooth.set_nav(DOWN, ws.s_mouse);
    ws.s_mouse.set_nav(DOWN, ws.keyconfig);
    ws.s_mouse.set_nav(DOWN, ws.pad_invert);
    ws.pad_invert.set_nav(DOWN, ws.s_pad);
    ws.s_pad.set_nav(DOWN, ws.keyconfig);
    ws.keyconfig.set_nav(DOWN, ws.ok);

    ws.smooth.set_nav(UP, ws.invert);
    ws.s_mouse.set_nav(UP, ws.smooth);
    ws.keyconfig.set_nav(UP, ws.s_mouse);
    ws.pad_invert.set_nav(UP, ws.s_mouse);
    ws.s_pad.set_nav(UP, ws.pad_invert);
    ws.keyconfig.set_nav(UP, ws.s_pad);
}

fn add_sound(me: u64, set: Set, ws: &mut Ws) {
    let border = 15.0f32;
    let mut pos = p3(border, 6.0 + border + 10.0, 0.1);
    let tab = ws.tab_sound;
    let t = |n: &str| tr("OptionsMenu", n);

    let label = set.label(pos, NEG, &t("SoundDevice"), tab);
    pos[1] += label.size().1 + 5.0;
    ws.snd_dev = set.combo(pos, (400.0, 25.0), tab);
    set_up_input(
        me,
        label,
        ws.snd_dev,
        Input::Combo,
        true,
        t("SoundDeviceTip"),
    );
    pos[1] += ws.snd_dev.size().1 + 15.0;

    let label = set.label(pos, NEG, &t("Volume"), tab);
    ws.s_volume = set.slider(
        add3(pos, p3(0.0, label.size().1 + 5.0, 0.0)),
        (100.0, 20.0),
        tab,
    );
    set_up_input(me, label, ws.s_volume, Input::Slider, false, t("VolumeTip"));
    ws.l_volume = set_up_slider(me, ws.s_volume, Range::Volume, Some(SOUND_MOVE), true);

    ws.snd_dev.set_nav(DOWN, ws.s_volume);
    ws.s_volume.set_nav(UP, ws.snd_dev);
    ws.s_volume.set_nav(DOWN, ws.ok);

    tab.set_user_data(ws.s_volume.0);
    tab.tab_label().set_user_data(ws.snd_dev.0);

    populate_sound_devices(me, ws.snd_dev);
}

fn populate_sound_devices(me: u64, combo: Wd) {
    combo.combo_clear();
    let n = call(op::SND_DEVICES, &mut Args::new()) as i32;
    let cur_name = h::host_list_str(5, 0);
    let mut ids = Vec::new();
    if n == 0 {
        combo.combo_add(&cur_name);
    } else {
        let mut found = false;
        for i in 0..n {
            let mut a = Args::new();
            a.i[0] = i;
            call(op::SND_DEVICE, &mut a);
            if !found && a.i[1] != 0 {
                found = true;
            }
            combo.combo_add(&h::host_list_str(1, i));
            ids.push(a.i[0]);
        }
        if !found {
            let mut name = h::w("(Unsupported) ");
            name.extend(cur_name);
            combo.combo_add(&name);
        }
    }
    with(me, |o| o.sound_ids = ids);
}

fn set_input_values(me: u64, v: &Vals) {
    let Some((ws, show_commentary, files)) = with(me, |o| {
        o.setting_initial = true;
        (o.ws, o.show_commentary, o.lang_files.clone())
    }) else {
        return;
    };
    ws.subtitles.set_checked(b(v.show_subtitles), false);
    ws.effect_subtitles
        .set_checked(b(v.show_effect_subtitles), false);
    ws.hints.set_checked(b(v.show_hints), false);
    ws.death_hints.set_checked(b(v.show_death_hints), false);
    ws.crosshair.set_checked(b(v.show_crosshair), false);
    ws.focus_style
        .combo_select(v.focus_icon_style, false, false);
    if ws.focus_style.combo_selected() == -1 {
        ws.focus_style.combo_select(0, false, true);
    }
    if show_commentary {
        ws.commentary.set_checked(b(v.show_commentary), false);
    }
    {
        let lang = v.lang.bytes();
        let refs: Vec<&[u32]> = files.iter().map(Vec::as_slice).collect();
        ws.language
            .combo_select(o::language_index(lang, &refs), false, false);
    }

    {
        let n = call(op::VIDEO_MODES, &mut Args::new());
        let raw: Vec<o::Mode> = (0..n)
            .map(|i| {
                let mut a = Args::new();
                a.i[0] = i as i32;
                call(op::VIDEO_MODE, &mut a);
                o::Mode {
                    display: a.i[0],
                    width: a.i[1],
                    height: a.i[2],
                    refresh: a.i[3],
                }
            })
            .collect();
        let multi = o::multi_display(&raw);
        let kept: Vec<(usize, i32)> = o::dedup_modes(&raw);
        let modes: Vec<o::Mode> = kept
            .iter()
            .filter_map(|&(src, refresh)| raw.get(src).map(|m| o::Mode { refresh, ..*m }))
            .collect();

        ws.resolution.combo_clear();
        let mut sizes = Vec::new();
        for m in &modes {
            let mut label;
            if m.width == 0 && m.height == 0 {
                label = tr("OptionsMenu", "FullScreenDesktop");
                label.extend(h::w(" "));
                label.extend(h::string_op(op::DISPLAY_NAME, m.display));
            } else {
                label = o::resolution_label(m.width, m.height, false);
                if multi {
                    label.extend(h::w(" ("));
                    label.extend(h::string_op(op::DISPLAY_NAME, m.display));
                    label.extend(h::w(")"));
                }
            }
            ws.resolution.combo_add(&label);
            sizes.push((m.display, m.width, m.height));
        }
        let mut current = o::current_mode(&modes, v.display, v.res_x, v.res_y);
        if current == -1 {
            ws.resolution
                .combo_add(&o::resolution_label(v.res_x, v.res_y, true));
            sizes.push((v.display, v.res_x, v.res_y));
            current = ws.resolution.combo_num() - 1;
        }
        with(me, |o| o.screen_sizes.extend(sizes));
        ws.resolution.combo_select(current, true, false);
    }

    ws.fullscreen.set_checked(b(v.fullscreen), false);
    ws.vsync.set_checked(b(v.vsync), false);
    ws.show_fps.set_checked(b(v.show_fps), false);
    ws.renderer
        .combo_select(o::renderer_index(v.renderer.bytes()), true, false);
    ws.color
        .combo_select(o::color_index(v.color.bytes()), true, false);
    slider_put(ws.s_paper, v.paper_white, false, Range::PaperWhite);
    slider_put(ws.s_peak, v.peak_nits, false, Range::Peak);
    hdr_labels(&ws);

    ws.gui_scale.combo_clear();
    for n in ["1x", "2x", "3x", "4x"] {
        ws.gui_scale.combo_add(&h::w(n));
    }
    let idx = o::gui_scale_index(v.gui_scale, ws.gui_scale.combo_num());
    ws.gui_scale.combo_select(idx, true, false);

    ws.tex_size.combo_select(
        o::flip_index(v.tex_quality, ws.tex_size.combo_num()),
        true,
        false,
    );

    ws.tex_filter.combo_clear();
    for n in ["Nearest", "Bilinear", "Trilinear"] {
        ws.tex_filter.combo_add(&tr("OptionsMenu", n));
    }
    ws.tex_filter.combo_select(v.tex_filter, true, false);

    ws.aniso.combo_clear();
    ws.aniso.combo_add(&tr("OptionsMenu", "Off"));
    let mut power = 2;
    for _ in 0..4 {
        ws.aniso.combo_add(&h::w(&format!("{power}X")));
        power *= 2;
    }
    ws.aniso.combo_select(o::aniso_index(v.aniso), false, true);

    ws.edge.set_checked(b(v.edge_smooth), false);
    ws.shadows.set_checked(b(v.shadows), false);

    ws.shadow_quality.combo_clear();
    ws.shadow_res.combo_clear();
    ws.parallax.combo_clear();
    for n in ["Low", "Medium", "High"] {
        ws.shadow_quality.combo_add(&tr("OptionsMenu", n));
        ws.shadow_res.combo_add(&tr("OptionsMenu", n));
    }
    ws.shadow_quality
        .combo_select(v.shadow_quality, true, false);
    ws.shadow_res.combo_select(v.shadow_res, true, false);
    ws.parallax.combo_add(&tr("Launcher", "Off"));
    ws.parallax.combo_add(&tr("Launcher", "On"));
    ws.parallax
        .combo_select(i32::from(b(v.parallax)), true, false);

    ws.world_reflection
        .set_checked(b(v.world_reflection), false);
    ws.refraction.set_checked(b(v.refraction), false);

    ws.ssao.set_checked(b(v.ssao), false);
    ws.ssao_samples
        .combo_select(o::ssao_index(v.ssao_samples), true, false);
    ws.ssao_res.combo_select(v.ssao_res, false, true);

    ws.bloom.set_checked(b(v.bloom), false);
    ws.trail.set_checked(b(v.image_trail), false);
    ws.sepia.set_checked(b(v.sepia), false);
    ws.radial.set_checked(b(v.radial_blur), false);
    ws.insanity.set_checked(b(v.insanity), false);

    slider_put(ws.s_gamma, v.gamma, false, Range::Gamma);
    slider_label(ws.l_gamma, v.gamma, Range::Gamma);

    ws.invert.set_checked(b(v.invert_mouse), false);
    ws.smooth.set_checked(b(v.smooth_mouse), false);
    slider_put(ws.s_mouse, v.mouse_sens, false, Range::Mouse);
    slider_label(ws.l_mouse, v.mouse_sens, Range::Mouse);
    ws.pad_invert.set_checked(b(v.pad_invert), false);
    slider_put(ws.s_pad, v.pad_sens, false, Range::Gamepad);
    slider_label(ws.l_pad, v.pad_sens, Range::Gamepad);

    slider_put(ws.s_volume, v.volume, false, Range::Volume);
    volume_label(ws.l_volume, v.volume);

    let ids = with(me, |o| o.sound_ids.clone()).unwrap_or_default();
    ws.snd_dev.combo_select(
        o::sound_device_index(v.sound_dev, &ids, ws.snd_dev.combo_num()),
        true,
        false,
    );

    with(me, |o| o.setting_initial = false);
}

fn hdr_labels(ws: &Ws) {
    ws.l_paper
        .set_text(&o::int_label(slider_get(ws.s_paper, Range::PaperWhite)));
    ws.l_peak
        .set_text(&o::int_label(slider_get(ws.s_peak, Range::Peak)));
}

fn gather(me: u64) -> Option<Vals> {
    let (ws, files, sizes, show_commentary) = with(me, |o| {
        (
            o.ws,
            o.lang_files.clone(),
            o.screen_sizes.clone(),
            o.show_commentary,
        )
    })?;
    let mut v = Vals::default();
    let lang = files
        .get(usize::try_from(ws.language.combo_selected()).unwrap_or(usize::MAX))
        .map(|f| h::narrow(f))
        .unwrap_or_default();
    v.lang = Str::from_text(&lang);
    v.show_hints = i32::from(ws.hints.checked());
    v.show_death_hints = i32::from(ws.death_hints.checked());
    v.show_subtitles = i32::from(ws.subtitles.checked());
    v.show_effect_subtitles = i32::from(ws.effect_subtitles.checked());
    v.show_crosshair = i32::from(ws.crosshair.checked());
    v.focus_icon_style = ws.focus_style.combo_selected();
    if show_commentary {
        v.show_commentary = i32::from(ws.commentary.checked());
    }
    let (display, x, y) = sizes
        .get(usize::try_from(ws.resolution.combo_selected()).unwrap_or(usize::MAX))
        .copied()
        .unwrap_or((0, 0, 0));
    v.display = display;
    v.res_x = x;
    v.res_y = y;
    v.fullscreen = i32::from(ws.fullscreen.checked());
    v.vsync = i32::from(ws.vsync.checked());
    v.show_fps = i32::from(ws.show_fps.checked());
    v.renderer = Str::from_text(o::renderer_name(ws.renderer.combo_selected()));
    v.color = Str::from_text(o::color_name(ws.color.combo_selected()));
    v.paper_white = slider_get(ws.s_paper, Range::PaperWhite);
    v.peak_nits = slider_get(ws.s_peak, Range::Peak);
    v.gui_scale = ws.gui_scale.combo_selected() + 1;
    v.tex_quality = o::flip_index(ws.tex_size.combo_selected(), ws.tex_size.combo_num());
    v.tex_filter = ws.tex_filter.combo_selected();
    v.aniso = o::aniso_from_index(ws.aniso.combo_selected());
    v.edge_smooth = i32::from(ws.edge.checked());
    v.shadows = i32::from(ws.shadows.checked());
    v.shadow_quality = ws.shadow_quality.combo_selected();
    v.shadow_res = ws.shadow_res.combo_selected();
    v.parallax = i32::from(ws.parallax.combo_selected() == 1);
    v.world_reflection = i32::from(ws.world_reflection.checked());
    v.refraction = i32::from(ws.refraction.checked());
    v.ssao = i32::from(ws.ssao.checked());
    v.ssao_samples = o::ssao_from_index(ws.ssao_samples.combo_selected());
    v.ssao_res = ws.ssao_res.combo_selected();
    v.bloom = i32::from(ws.bloom.checked());
    v.image_trail = i32::from(ws.trail.checked());
    v.sepia = i32::from(ws.sepia.checked());
    v.radial_blur = i32::from(ws.radial.checked());
    v.insanity = i32::from(ws.insanity.checked());
    v.gamma = slider_get(ws.s_gamma, Range::Gamma);
    v.invert_mouse = i32::from(ws.invert.checked());
    v.smooth_mouse = i32::from(ws.smooth.checked());
    v.mouse_sens = slider_get(ws.s_mouse, Range::Mouse);
    v.pad_invert = i32::from(ws.pad_invert.checked());
    v.pad_sens = slider_get(ws.s_pad, Range::Gamepad);
    v.volume = slider_get(ws.s_volume, Range::Volume);
    Some(v)
}

fn read_engine() -> Vals {
    let mut v = Vals::default();
    let mut a = Args::new();
    a.p[0] = std::ptr::addr_of_mut!(v) as usize;
    call(op::OPT_READ, &mut a);
    v
}

fn apply_changes(me: u64) {
    lock(&STORE).initial_set = false;
    let Some(v) = gather(me) else { return };
    let commentary = with(me, |o| o.show_commentary).unwrap_or(false);
    let mut a = Args::new();
    a.p[0] = std::ptr::addr_of!(v) as usize;
    a.i[0] = i32::from(commentary);
    call(op::OPT_APPLY, &mut a);
}

fn set_window_active(window: i32) {
    h::simple(op::SET_WINDOW_ACTIVE, window);
}

pub fn exit_pressed(me: u64) {
    let Some(set) = with(me, |o| o.set) else {
        return;
    };
    if set.popup_active() {
        return;
    }
    set_window_active(LAST_WINDOW);
}

fn set_tab_navigation(me: u64, tab: Wd, focus: bool) {
    let Some((set, ws)) = with(me, |o| (o.set, o.ws)) else {
        return;
    };
    let first = Wd(tab.tab_label().user_data());
    let last = Wd(tab.user_data());
    set.set_default_nav(first);
    if focus {
        set.set_focused(first);
    }
    ws.ok.set_nav(UP, last);
    ws.cancel.set_nav(UP, last);
}

pub fn on_set_active(me: u64, active: bool) {
    let Some(ws) = with(me, |o| o.ws) else { return };
    if active {
        set_tab_navigation(me, ws.tab_game.tab_frame().tab_top(), true);
        let open = with(me, |o| o.key_config_open).unwrap_or(false);
        let initial_set = lock(&STORE).initial_set;
        match o::activate(open, false, !open && initial_set) {
            o::Activate::KeepKeyConfig => {
                with(me, |o| o.key_config_open = false);
            }
            o::Activate::FromCurrent => {}
            o::Activate::DumpThenInitial => {
                let v = read_engine();
                lock(&STORE).initial = v;
                set_input_values(me, &v);
                lock(&STORE).initial_set = true;
            }
            o::Activate::FromInitial => {
                let v = lock(&STORE).initial;
                set_input_values(me, &v);
                lock(&STORE).initial_set = true;
            }
        }
    }
    if !ws.shoulder[0].is_null() {
        let present = call(op::GAMEPAD_PRESENT, &mut Args::new()) != 0;
        ws.shoulder[0].set_visible(present);
        ws.shoulder[1].set_visible(present);
    }
}

fn set_current_tip_widget(me: u64, w: Wd) {
    with(me, |o| {
        if o.tip_widget == w {
            o.tip.restart = true;
        } else {
            o.tip_updated = true;
            o.tip_widget = w;
        }
    });
}

fn window_update(me: u64, dt: f32) -> bool {
    let Some((set, ws, tip)) = with(me, |o| (o.set, o.ws, o.tip)) else {
        return true;
    };
    if set.popup_active() {
        return true;
    }
    let mut col = ws.tip.font_color();
    let step = o::tip_step(tip, col[3], dt);
    with(me, |o| o.tip = step.tip);
    if step.reset_scroll {
        ws.tip.set_scroll_offset(0.0);
    }
    match step.act {
        TipAct::None => {}
        TipAct::FadeIn { delta, clamp } => {
            col[3] += delta;
            if clamp {
                col[3] = 1.0;
            }
            ws.tip.set_font_color(col);
        }
        TipAct::FadeOut { delta } => {
            col[3] -= delta;
            ws.tip.set_font_color(col);
        }
    }

    let pending = with(me, |o| {
        let updated = std::mem::replace(&mut o.tip_updated, false);
        updated.then_some(o.tip_widget)
    })
    .flatten();
    let ud = pending
        .filter(|w| !w.is_null())
        .map_or(0, |w| w.user_data());
    let text = (ud != 0)
        .then(|| with(me, |o| o.data.get(ud - 1).cloned()).flatten())
        .flatten();
    if let Some((restart, mut msg_text)) = text {
        if restart {
            msg_text.extend(h::w(" ("));
            msg_text.extend(tr("OptionsMenu", "ReqRestart"));
            msg_text.extend(h::w(")"));
        }
        ws.tip.set_text(&msg_text);
    }
    true
}

fn option_over(me: u64, w: Wd, d: &CbData) -> bool {
    if w.user_data() == 0 {
        return true;
    }
    set_current_tip_widget(me, w);
    if d.msg == msg::GET_NAV_FOCUS {
        let parent = w.parent();
        if !parent.is_null() {
            let parent2 = parent.parent();
            let mut frame = parent2.frame_of();
            if frame.is_null() && !parent2.is_null() {
                frame = parent2.parent().frame_of();
            }
            if !frame.is_null() {
                let fp = frame.global_pos();
                let wp = w.global_pos();
                let sa = frame.frame_scroll();
                let x = wp[0] + sa[0] - fp[0];
                let mut y = wp[1] + sa[1] - fp[1];
                y -= 150.0;
                frame.frame_scroll_to(x, y);
            }
        }
    }
    true
}

fn option_change(me: u64, w: Wd) -> bool {
    let ud = w.user_data();
    let (setting, data) = with(me, |o| {
        (
            o.setting_initial,
            o.data.get(ud.wrapping_sub(1)).map(|d| d.0),
        )
    })
    .unwrap_or((false, None));
    if o::needs_restart(setting, data.is_some() && ud != 0, data.unwrap_or(false)) {
        call(op::NEEDS_RESTART, &mut Args::new());
    }
    true
}

fn press_cancel() -> bool {
    let v = lock(&STORE).initial;
    h::simple_f(op::SET_GAMMA, v.gamma);
    h::simple_f(op::SET_MOUSE_SENS, v.mouse_sens);
    h::simple_f(op::SET_PAD_SENS, v.pad_sens);
    h::simple_f(op::SET_VOLUME, v.volume);
    set_window_active(LAST_WINDOW);
    true
}

fn toggle_gfx(me: u64, w: Wd) -> bool {
    let Some(ws) = with(me, |o| o.ws) else {
        return true;
    };
    let tg = o::toggle_gfx(ws.basic.is_visible());
    ws.basic.set_visible(tg.show_basic);
    ws.basic.set_enabled(tg.show_basic);
    ws.advanced.set_visible(tg.show_advanced);
    ws.advanced.set_enabled(tg.show_advanced);
    if tg.toggle_first {
        ws.toggle.set_nav(UP, ws.refraction);
        ws.tab_gfx.tab_label().set_user_data(ws.toggle.0);
    } else {
        ws.toggle.set_nav(UP, ws.s_gamma);
        ws.tab_gfx.tab_label().set_user_data(ws.resolution.0);
    }
    let text = if tg.label == 0 {
        tr("OptionsMenu", "BasicOptions")
    } else {
        tr("OptionsMenu", "AdvancedOptions")
    };
    w.set_text(&text);
    true
}

pub fn callback(cb: &Cb, w: Wd, d: &CbData) -> bool {
    let me = cb.owner;
    let Some((set, ws)) = with(me, |o| (o.set, o.ws)) else {
        return true;
    };
    match cb.id {
        WINDOW_UPDATE => window_update(me, d.fval),
        OPTION_OVER => option_over(me, w, d),
        OPTION_CHANGE => option_change(me, w),
        HDR_MOVE => {
            hdr_labels(&ws);
            true
        }
        GAMMA_MOVE => {
            let g = slider_get(ws.s_gamma, Range::Gamma);
            slider_label(ws.l_gamma, g, Range::Gamma);
            h::simple_f(op::SET_GAMMA, g);
            true
        }
        MOUSE_MOVE => {
            let s = slider_get(ws.s_mouse, Range::Mouse);
            slider_label(ws.l_mouse, s, Range::Mouse);
            h::simple_f(op::SET_MOUSE_SENS, s);
            true
        }
        PAD_MOVE => {
            let s = slider_get(ws.s_pad, Range::Gamepad);
            slider_label(ws.l_pad, s, Range::Gamepad);
            h::simple_f(op::SET_PAD_SENS, s);
            true
        }
        SOUND_MOVE => {
            let v = slider_get(ws.s_volume, Range::Volume);
            volume_label(ws.l_volume, v);
            h::simple_f(op::SET_VOLUME, v);
            call(op::PLAY_SANITY, &mut Args::new());
            true
        }
        TOGGLE_GFX => toggle_gfx(me, w),
        PRESS_KEYCONFIG => {
            with(me, |o| o.key_config_open = true);
            set_window_active(KEY_CONFIG_WINDOW);
            true
        }
        PRESS_OK => {
            apply_changes(me);
            call(op::SAVE_CONFIG, &mut Args::new());
            if !set.restart_warning(tok(me, MSGBOX)) {
                set_window_active(LAST_WINDOW);
            }
            true
        }
        PRESS_CANCEL => press_cancel(),
        UI_PRESS_CANCEL => d.lval == h::UI_SECONDARY && press_cancel(),
        MSGBOX => {
            set_window_active(LAST_WINDOW);
            true
        }
        TAB_CHANGE => {
            set_tab_navigation(me, w.tab_get(d.lval), true);
            true
        }
        LANGUAGE => true,
        _ => true,
    }
}
