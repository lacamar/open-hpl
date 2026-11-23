//! `cLuxPreMenu` with its sections and text elements.

use super::a10b::{
    MSG_BUTTON_PRESSED, MSG_SLIDER_MOVE, MSG_UI_ARROW, MSG_UI_BUTTON, Store, UI_PRIMARY, WHITE,
    wide,
};
use super::host::{
    ALIGN_LEFT, Arg, Eng, FLAG_DEBUG_WINDOW, FLAG_FIRST_START, FLAG_FULLSCREEN, FLAG_MAP_LOADED,
    FLAG_SHOW_MENU, INPUT_GAME, INPUT_PREMENU, MAT_ALPHA,
};
use super::premenu::{
    self, ACT_CHECK_SHOW, ACT_FINAL, ACT_SECTION, ACT_SET_TIMER, Pre, S_FADE_OUT,
};
use crate::string::{float_to_string, utf8_to_wide};

pub const TAG_CONTINUE: i64 = 1;
pub const TAG_CONTINUE_UI: i64 = 2;
pub const TAG_GAMMA_ARROW: i64 = 3;
pub const TAG_GAMMA_CHANGE: i64 = 4;

#[derive(Clone)]
struct TextEl {
    text: Vec<u32>,
    pos: [f32; 3],
    frame: [f32; 2],
    font: [f32; 2],
    color: [f32; 4],
    align: i64,
}

struct Section {
    bg_color: [f32; 4],
    bg_file: Vec<u8>,
    time: f32,
    first_only: bool,
    gamma: bool,
    music: Vec<u8>,
    music_vol: f32,
    music_fade: f32,
    allow_skip: bool,
    texts: Vec<TextEl>,
}

#[derive(Default)]
pub struct Pm {
    set: usize,
    vp: usize,
    black: usize,
    bg: usize,
    cur: Option<usize>,
    labels: Vec<usize>,
    cont: usize,
    gamma_group: usize,
    preview: usize,
    gamma_label: usize,
    gamma_slider: usize,
    gui_size: [f32; 2],
    start: [f32; 3],
    sections: Vec<Section>,
    s: Pre,
    upd: i64,
}

pub static STORE: std::sync::LazyLock<Store<Pm>> = std::sync::LazyLock::new(Store::new);

fn align_of(name: &[u8], e: &Eng) -> i64 {
    match premenu::parse_align(name) {
        Some(a) => i64::from(a),
        None => {
            let mut m = b"The font alignment '".to_vec();
            m.extend_from_slice(name);
            m.extend_from_slice(b"' does not exist!\n");
            e.log_error(&m);
            ALIGN_LEFT
        }
    }
}

fn load_text(e: &Eng, el: usize) -> Option<TextEl> {
    if el == 0 {
        return None;
    }
    let cat = e.xml_str(el, b"TextCat", b"");
    let entry = e.xml_str(el, b"TextEntry", b"");
    let text = e.translate(&cat, &entry);
    let p = e.xml_vec3(el, b"Pos");
    let frame = e.xml_vec2(el, b"FrameSize", 0.0);
    let font = e.xml_vec2(el, b"FontSize", 0.0);
    let color = e.xml_color(el, b"Color", [0.0; 4]);
    let align_name = e.xml_str(el, b"Align", b"Left");
    let align = align_of(&align_name, e);
    e.xml_float(el, b"time", 2.0);
    Some(TextEl {
        text,
        pos: [p[0] + 0.0, p[1] + 0.0, p[2] + 1.0],
        frame,
        font,
        color,
        align,
    })
}

fn load_section(e: &Eng, el: usize) -> Option<Section> {
    if el == 0 {
        return None;
    }
    let bg_color = e.xml_color(el, b"Color", [0.0, 0.0, 0.0, 1.0]);
    let bg_file = e.xml_str(el, b"Image", b"");
    let time = e.xml_float(el, b"Time", 2.0);
    let first_only = e.xml_bool(el, b"ShowFirstStartOnly", false);
    let gamma = e.xml_bool(el, b"HasGammaSettings", false);
    let music = e.xml_str(el, b"Music", b"");
    let music_vol = e.xml_float(el, b"MusicVolume", 0.0);
    let music_fade = e.xml_float(el, b"MusicFadeTime", 0.0);
    let allow_skip = e.xml_bool(el, b"AllowSkipping", true);
    let n = e.xml_child_count(el);
    let mut texts = Vec::new();
    for i in 0..n {
        let c = e.xml_child(el, i);
        if let Some(t) = load_text(e, c) {
            texts.push(t);
        }
    }
    Some(Section {
        bg_color,
        bg_file,
        time,
        first_only,
        gamma,
        music,
        music_vol,
        music_fade,
        allow_skip,
        texts,
    })
}

fn load_sections(e: &Eng, set: usize) -> Vec<Section> {
    let mut out = Vec::new();
    let path = e.premenu_path();
    let doc = e.xml_open(&path);
    if doc != 0 {
        let skin_name = e.xml_str(doc, b"skin", b"gui_default.skin");
        let skin = e.create_skin(&skin_name);
        e.set_skin(set, skin);
        let n = e.xml_child_count(doc);
        for i in 0..n {
            let c = e.xml_child(doc, i);
            if let Some(s) = load_section(e, c) {
                out.push(s);
            }
        }
        e.xml_close(doc);
    }
    out
}

fn with<R>(h: u64, f: impl FnOnce(&mut Pm) -> R) -> Option<R> {
    STORE.with(h, f)
}

pub fn new(e: &Eng, handle_out: &dyn Fn(u64)) -> u64 {
    e.ev("pm_new", &[]);
    let h = STORE.add(Pm::default());
    handle_out(h);
    let set = e.create_set(b"PreMenu", 0);
    e.set_draw_mouse(set, false);
    let center = [800.0f32, 600.0];
    let (gui_size, off) = e.calc_offset(center);
    let start = [-off[0], -off[1], 0.0];
    e.set_virtual_size(set, gui_size, off);
    let vp = e.viewport_new();
    e.viewport_active(vp, false);
    e.viewport_visible(vp, false);
    e.set_active(set, false);
    e.set_draw_mouse(set, false);
    e.viewport_add_set(vp, set);
    let black = e.gfx_rect([0.0, 0.0, 0.0, 1.0], true);
    with(h, |p| {
        p.set = set;
        p.vp = vp;
        p.black = black;
        p.gui_size = gui_size;
        p.start = start;
        p.s = Pre::new();
    });
    let sections = load_sections(e, set);
    with(h, |p| p.sections = sections);

    let label = e.translate(b"PreMenu", b"Continue");
    let cont = e.widget_button(set, [0.0; 3], [0.0; 2], &label, 0);
    let font = e.widget_default_font(cont);
    let fh = e.font_height(font);
    let length = e.font_len(font, [fh, fh], &label) + 20.0;
    let size = [length + 10.0, fh + 10.0];
    let mut pos = [
        ((center[0] - size[0]) + 0.0) * 0.5,
        ((center[1] - size[1]) + -10.0) * 1.0,
        (0.0 + 0.1) * 1.0,
    ];
    e.widget_pos(cont, pos);
    e.widget_size(cont, size);
    e.widget_callback(cont, MSG_BUTTON_PRESSED, TAG_CONTINUE);
    e.widget_callback(cont, MSG_UI_BUTTON, TAG_CONTINUE_UI);
    e.widget_callback(cont, MSG_UI_ARROW, TAG_GAMMA_ARROW);
    e.widget_global_ui_listener(cont, true);
    e.widget_visible(cont, false);
    with(h, |p| p.cont = cont);

    let vc = [center[0] * 0.5, center[1] * 0.5];
    let group = e.widget_dummy(set, [0.0; 3], 0);
    let preview = e.widget_image(
        set,
        b"menu_gamma.tga",
        [0.0; 3],
        [-1.0, -1.0],
        MAT_ALPHA,
        false,
        group,
    );
    let ps = e.widget_get_size(preview);
    let mut vsize = [ps[0] * 1.25, ps[1] * 1.25];
    e.widget_pos(preview, [0.0, 0.0, 0.1]);
    e.widget_size(preview, vsize);
    e.widget_visible(preview, false);
    e.widget_enabled(preview, false);
    let glabel = e.widget_label(set, [0.0; 3], [0.0; 2], &[], group);
    e.widget_visible(glabel, false);
    e.widget_enabled(glabel, false);
    vsize[1] = 25.0;
    let slider = e.widget_slider(set, pos, vsize, 10, group);
    e.widget_callback(slider, MSG_SLIDER_MOVE, TAG_GAMMA_CHANGE);
    let (smax, sbar) = premenu::gamma_slider();
    e.slider_max(slider, i64::from(smax));
    e.slider_bar(slider, i64::from(sbar));
    with(h, |p| {
        p.gamma_group = group;
        p.preview = preview;
        p.gamma_label = glabel;
        p.gamma_slider = slider;
    });
    let g = e.gamma_get();
    let mx = e.slider_get_max(slider);
    e.slider_set(
        slider,
        i64::from(premenu::gamma_to_slider(g, mx as i32)),
        true,
    );
    e.widget_visible(slider, false);
    e.widget_enabled(slider, false);
    pos = [0.0, 0.0, 0.1];
    e.widget_pos(preview, pos);
    let pz = e.widget_get_size(preview);
    pos[1] += pz[1] + 10.0;
    e.widget_pos(glabel, pos);
    pos[1] += 25.0;
    e.widget_pos(slider, pos);
    let gs = e.widget_get_size(group);
    let gp = [
        (vc[0] - gs[0] * 0.5) + 0.0,
        (vc[1] - gs[1] * 0.5) + 20.0,
        0.0 + 0.1,
    ];
    e.widget_pos(group, gp);
    e.set_default_focus_nav(set, cont);
    e.set_focused(set, cont);
    h
}

pub fn delete(h: u64) {
    STORE.remove(h);
}

pub fn on_enter(e: &Eng, h: u64) {
    e.ev("pm_enter", &[]);
    let Some((set, vp)) = with(h, |p| (p.set, p.vp)) else {
        return;
    };
    if !e.base_flag(FLAG_FULLSCREEN) {
        e.lock_input(false);
    }
    e.relative_mouse(false);
    e.input_state(INPUT_PREMENU);
    e.viewport_active(vp, true);
    e.viewport_visible(vp, true);
    e.set_active(set, true);
    e.gui_focus(set);
    with(h, |p| {
        p.s.enter();
        p.bg = 0;
        p.cur = None;
    });
}

pub fn on_leave(e: &Eng, h: u64) {
    e.ev("pm_leave", &[]);
    let Some((set, vp)) = with(h, |p| (p.set, p.vp)) else {
        return;
    };
    if !e.base_flag(FLAG_DEBUG_WINDOW) {
        if !e.base_flag(FLAG_FULLSCREEN) {
            e.lock_input(true);
        }
        e.relative_mouse(true);
    }
    e.viewport_active(vp, false);
    e.viewport_visible(vp, false);
    e.set_active(set, false);
    super::a10b_ls::draw_menu_screen_global(e);
}

fn update_state(e: &Eng, h: u64) {
    let first_start = e.base_flag(FLAG_FIRST_START);
    let Some((step, set)) = with(h, |p| {
        let first_only: Vec<bool> = p.sections.iter().map(|s| s.first_only).collect();
        (p.s.update_state(first_start, &first_only), p.set)
    }) else {
        return;
    };
    match step.act {
        ACT_SET_TIMER => {
            with(h, |p| {
                if let Some(s) = p.cur.and_then(|i| p.sections.get(i)) {
                    p.s.timer = s.time;
                }
            });
        }
        ACT_FINAL | ACT_SECTION => {
            let (bg, labels) = with(h, |p| {
                (std::mem::take(&mut p.bg), std::mem::take(&mut p.labels))
            })
            .unwrap_or_default();
            if bg != 0 {
                e.gfx_destroy(bg);
            }
            for l in labels {
                e.widget_destroy(set, l);
            }
            if step.act == ACT_FINAL {
                with(h, |p| p.cur = None);
            } else {
                enter_section(e, h, step.section);
            }
        }
        ACT_CHECK_SHOW => {
            with(h, |p| {
                if let Some(s) = p.cur.and_then(|i| p.sections.get(i)) {
                    let (t, g) = (!s.texts.is_empty(), s.gamma);
                    p.s.check_show(t, g);
                }
            });
        }
        _ => {}
    }
}

fn enter_section(e: &Eng, h: u64, index: i32) {
    let idx = usize::try_from(index).ok();
    let Some(info) = with(h, |p| {
        p.cur = idx.filter(|i| *i < p.sections.len());
        let s = p.cur.and_then(|i| p.sections.get(i))?;
        Some((
            p.set,
            p.cont,
            p.gamma_label,
            p.gamma_slider,
            p.preview,
            s.bg_color,
            s.bg_file.clone(),
            !s.texts.is_empty(),
            s.gamma,
            s.music.clone(),
            s.music_vol,
            s.music_fade,
        ))
    })
    .flatten() else {
        return;
    };
    let (
        set,
        cont,
        glabel,
        gslider,
        preview,
        bg_color,
        bg_file,
        has_text,
        has_gamma,
        music,
        vol,
        fade,
    ) = info;
    let gui_active = has_text || has_gamma;
    let bg = if bg_file.is_empty() {
        e.gfx_rect(bg_color, true)
    } else {
        let tex = e.tex_2d(&bg_file);
        if tex != 0 { e.gfx_from_tex(tex) } else { 0 }
    };
    e.widget_visible(glabel, has_gamma);
    e.widget_enabled(glabel, has_gamma);
    e.widget_visible(gslider, has_gamma);
    e.widget_enabled(gslider, has_gamma);
    e.widget_visible(preview, has_gamma);
    e.widget_enabled(preview, has_gamma);
    e.widget_visible(cont, gui_active);
    e.widget_enabled(cont, gui_active);
    e.set_draw_mouse(set, gui_active);
    let texts: Vec<TextEl> = with(h, |p| {
        p.bg = bg;
        p.cur
            .and_then(|i| p.sections.get(i))
            .map(|s| s.texts.clone())
            .unwrap_or_default()
    })
    .unwrap_or_default();
    let mut labels = Vec::new();
    for t in texts {
        let l = e.widget_label(set, t.pos, t.frame, &t.text, 0);
        e.widget_font_color(l, t.color);
        e.widget_font_size(l, t.font);
        e.widget_align(l, t.align);
        e.widget_wrap(l, true);
        labels.push(l);
    }
    with(h, |p| p.labels = labels);
    if !music.is_empty() {
        let speed = premenu::music_fade_speed(fade);
        e.music_play(&music, vol, speed, true);
    }
}

pub fn update(e: &Eng, h: u64, dt: f32) {
    e.ev("pm_update", &[Arg::F(dt)]);
    with(h, |p| {
        p.upd += 1;
        p.s.update_actions(dt)
    });
    update_state(e, h);
    let Some(exit) = with(h, |p| p.s.exit) else {
        return;
    };
    if !exit {
        return;
    }
    if e.base_flag(FLAG_SHOW_MENU) {
        e.updater_container(b"MainMenu");
    } else {
        e.input_state(INPUT_GAME);
        e.base_create_profile();
        e.base_set_profile();
        if !e.base_init_user_config() {
            e.base_exit();
            return;
        }
        e.updater_container(b"Default");
        e.base_start_game();
    }
}

pub fn button_pressed(e: &Eng, h: u64) {
    e.ev("pm_button", &[]);
    with(h, |p| {
        let sec = p.cur.and_then(|i| p.sections.get(i));
        let (has, allow) = (sec.is_some(), sec.is_none_or(|s| s.allow_skip));
        p.s.button_pressed(has, allow);
    });
}

pub fn on_draw(e: &Eng, h: u64) {
    let Some((set, black, bg, size, start, alpha, upd)) = with(h, |p| {
        (p.set, p.black, p.bg, p.gui_size, p.start, p.s.alpha, p.upd)
    }) else {
        return;
    };
    e.ev("pm_draw", &[Arg::I(upd)]);
    e.draw_gfx(
        set,
        black,
        [start[0] + 0.0, start[1] + 0.0, start[2] + 2.0],
        size,
        [1.0, 1.0, 1.0, alpha],
    );
    e.draw_gfx(
        set,
        black,
        [start[0] - 0.0, start[1] - 0.0, start[2] - 2.0],
        size,
        WHITE,
    );
    if bg != 0 {
        let sz = e.gfx_size(bg);
        let (x, w) = premenu::background_rect(sz[0], sz[1]);
        e.draw_gfx(set, bg, [x, 0.0, 0.0], [w, 600.0], WHITE);
    }
}

pub fn focus_lost(e: &Eng) {
    e.ev("pm_focus_lost", &[]);
    if !e.base_flag(FLAG_MAP_LOADED) {
        e.music_pause();
    }
}

pub fn focus_got(e: &Eng) {
    e.ev("pm_focus_got", &[]);
    if !e.base_flag(FLAG_MAP_LOADED) {
        e.music_resume();
    }
}

pub fn set_of(h: u64) -> usize {
    with(h, |p| p.set).unwrap_or(0)
}

pub fn continue_visible(e: &Eng, h: u64) -> bool {
    match with(h, |p| p.cont) {
        Some(c) => e.h.call(super::host::WIDGET_IS_VISIBLE, &[Arg::P(c)]).b(),
        None => false,
    }
}

pub fn callback(e: &Eng, h: u64, tag: i64, lval: i32) -> bool {
    match tag {
        TAG_CONTINUE => {
            with(h, |p| p.s.state = premenu::S_FADE_OUT);
            true
        }
        TAG_CONTINUE_UI => {
            if lval == UI_PRIMARY {
                with(h, |p| p.s.state = S_FADE_OUT);
                true
            } else {
                false
            }
        }
        TAG_GAMMA_ARROW => {
            let step = premenu::gamma_arrow(lval);
            if step != 0
                && let Some(sl) = with(h, |p| p.gamma_slider)
            {
                let v = e.slider_get(sl);
                e.slider_set(sl, v + i64::from(step), true);
            }
            true
        }
        TAG_GAMMA_CHANGE => {
            let Some((sl, lbl)) = with(h, |p| (p.gamma_slider, p.gamma_label)) else {
                return true;
            };
            let v = e.slider_get(sl);
            let mx = e.slider_get_max(sl);
            let g = premenu::slider_to_gamma(v as i32, mx as i32);
            let mut text = e.translate(b"OptionsMenu", b"Gamma");
            text.extend(wide(b": "));
            text.extend(utf8_to_wide(&float_to_string(g, 2, true)));
            e.widget_text(lbl, &text);
            e.gamma_set(g);
            true
        }
        _ => true,
    }
}
