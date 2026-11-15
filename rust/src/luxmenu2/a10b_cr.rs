//! `cLuxCredits` and `cLuxDemoEnd`.

use super::a10b::{
    MSG_MOUSE_ENTER, MSG_MOUSE_LEAVE, MSG_MOUSE_UP, MSG_ON_DRAW, MSG_ON_UPDATE, Store, WHITE,
};
use super::credits::{self, Credits, DE_BROWSER, DE_EXIT, DemoEnd, Row, UP_FINISH, UP_STOP_MUSIC};
use super::host::{
    ALIGN_CENTER, ALIGN_LEFT, Arg, CFG_DEMO, CFG_MENU, Eng, FLAG_PTEST, INPUT_CREDITS,
    INPUT_DEMO_END, INPUT_NULL,
};
use super::premenu;
use crate::string::utf8_to_wide;

#[derive(Default)]
pub struct Cr {
    set: usize,
    vp: usize,
    black: usize,
    font_normal: usize,
    font_header: usize,
    gui_size: [f32; 2],
    start: [f32; 3],
    normal_fs: [f32; 2],
    header_fs: [f32; 2],
    end_fs: [f32; 2],
    scroll: f32,
    fade: f32,
    key: Vec<u8>,
    music: Vec<u8>,
    loop_music: bool,
    text_rows: Vec<Vec<u32>>,
    rows: Vec<Row>,
    s: Credits,
    upd: i64,
}

pub static CR_STORE: std::sync::LazyLock<Store<Cr>> = std::sync::LazyLock::new(Store::new);

fn cr<R>(h: u64, f: impl FnOnce(&mut Cr) -> R) -> Option<R> {
    CR_STORE.with(h, f)
}

pub fn cr_new(e: &Eng) -> u64 {
    e.ev("cr_new", &[]);
    let h = CR_STORE.add(Cr {
        s: Credits::new(),
        ..Cr::default()
    });
    let set = e.create_set(b"Credits", 0);
    let (gui_size, off) = e.calc_offset([800.0, 600.0]);
    e.set_virtual_size(set, gui_size, off);
    let vp = e.viewport_new();
    e.viewport_active(vp, false);
    e.viewport_visible(vp, false);
    e.set_active(set, false);
    e.set_draw_mouse(set, false);
    e.viewport_add_set(vp, set);
    let normal_fs = e.cfg_vec2(CFG_MENU, b"Credits", b"NormalFontSize", 0.0);
    let header_fs = e.cfg_vec2(CFG_MENU, b"Credits", b"HeaderFontSize", 0.0);
    let end_fs = e.cfg_vec2(CFG_MENU, b"Credits", b"TheEndFontSize", 0.0);
    let black = e.gfx_rect([0.0, 0.0, 0.0, 1.0], true);
    let scroll = e.cfg_float(CFG_MENU, b"Credits", b"ScrollSpeed", 0.0);
    let fade = e.cfg_float(CFG_MENU, b"Credits", b"FadeSpeed", 0.0);
    cr(h, |c| {
        c.set = set;
        c.vp = vp;
        c.black = black;
        c.gui_size = gui_size;
        c.start = [-off[0], -off[1], 0.0];
        c.normal_fs = normal_fs;
        c.header_fs = header_fs;
        c.end_fs = end_fs;
        c.scroll = scroll;
        c.fade = fade;
    });
    cr_reset_state(h);
    h
}

fn cr_reset_state(h: u64) {
    cr(h, |c| {
        c.music.clear();
        c.loop_music = false;
        c.text_rows.clear();
        c.rows.clear();
        c.s.reset();
        c.key.clear();
    });
}

pub fn cr_delete(h: u64) {
    CR_STORE.remove(h);
}

pub fn cr_load_fonts(e: &Eng, h: u64) {
    e.ev("cr_load_fonts", &[]);
    let n = e.cfg_string(CFG_MENU, b"Credits", b"NormalFont", b"");
    let hd = e.cfg_string(CFG_MENU, b"Credits", b"HeaderFont", b"");
    let fnormal = e.font_load(&n);
    let fheader = e.font_load(&hd);
    cr(h, |c| {
        c.font_normal = fnormal;
        c.font_header = fheader;
    });
}

pub fn cr_reset(e: &Eng, h: u64) {
    e.ev("cr_reset", &[]);
    cr_reset_state(h);
}

pub fn cr_on_enter(e: &Eng, h: u64) {
    e.ev("cr_enter", &[]);
    let Some((set, vp, music, looped)) = cr(h, |c| (c.set, c.vp, c.music.clone(), c.loop_music))
    else {
        return;
    };
    e.input_state(INPUT_CREDITS);
    e.viewport_active(vp, true);
    e.viewport_visible(vp, true);
    e.set_active(set, true);
    e.gui_focus(set);
    if !music.is_empty() {
        e.music_play(&music, 1.0, 1.0, looped);
    }
}

pub fn cr_on_leave(e: &Eng, h: u64) {
    e.ev("cr_leave", &[]);
    let Some((set, vp)) = cr(h, |c| (c.set, c.vp)) else {
        return;
    };
    e.viewport_active(vp, false);
    e.viewport_visible(vp, false);
    e.set_active(set, false);
}

pub fn cr_update(e: &Eng, h: u64, dt: f32) {
    e.ev("cr_update", &[Arg::F(dt)]);
    let Some(up) = cr(h, |c| {
        c.upd += 1;
        let (scroll, fade) = (c.scroll, c.fade);
        c.s.update(dt, &c.rows, scroll, fade)
    }) else {
        return;
    };
    if up & UP_STOP_MUSIC != 0 {
        e.music_stop(0.1);
    }
    if up & UP_FINISH != 0 {
        e.updater_reset_all();
        super::a10b_ls::draw_menu_screen_global(e);
        e.updater_container(b"MainMenu");
    }
}

pub fn cr_add_key(e: &Eng, h: u64, key: i32) {
    e.ev("cr_add_key", &[Arg::I(i64::from(key))]);
    cr(h, |c| c.key.extend(key.to_le_bytes()));
}

pub fn cr_setup(
    e: &Eng,
    h: u64,
    music: &[u8],
    looped: bool,
    cat: &[u8],
    entry: &[u8],
    end_num: i32,
) {
    e.ev(
        "cr_setup",
        &[
            Arg::S(music),
            Arg::B(looped),
            Arg::S(cat),
            Arg::S(entry),
            Arg::I(i64::from(end_num)),
        ],
    );
    let Some(font) = cr(h, |c| {
        c.music = music.to_vec();
        c.loop_music = looped;
        c.s.end_num = end_num;
        c.font_normal
    }) else {
        return;
    };
    let text = e.translate(cat, entry);
    let rows = e.font_wrap(font, 750.0, 19.0, [17.0, 17.0], &text);
    cr(h, |c| {
        c.text_rows.extend(rows);
        c.rows = c
            .text_rows
            .iter()
            .map(|r| Row {
                len: u32::try_from(r.len()).unwrap_or(u32::MAX),
                first: r.first().copied().unwrap_or(0),
            })
            .collect();
    });
}

pub fn cr_exit_pressed(e: &Eng, h: u64) {
    e.ev("cr_exit_pressed", &[]);
    if cr(h, |c| c.s.exit_pressed()) == Some(true) {
        e.music_stop(0.1);
    }
}

pub fn cr_on_draw(e: &Eng, h: u64) {
    let Some((set, black, size, start, fade_alpha, state, upd)) = cr(h, |c| {
        (
            c.set,
            c.black,
            c.gui_size,
            c.start,
            c.s.fade_alpha,
            c.s.state,
            c.upd,
        )
    }) else {
        return;
    };
    e.ev("cr_draw", &[Arg::I(upd)]);
    e.draw_gfx(set, black, start, size, WHITE);
    if fade_alpha > 0.0 {
        e.draw_gfx(
            set,
            black,
            [start[0] + 0.0, start[1] + 0.0, start[2] + 20.0],
            size,
            [1.0, 1.0, 1.0, fade_alpha],
        );
    }
    if state == 0 || state == 1 {
        let Some((y, rows, trows, nfs, hfs, fnormal, fheader)) = cr(h, |c| {
            (
                c.s.y,
                c.rows.clone(),
                c.text_rows.clone(),
                c.normal_fs,
                c.header_fs,
                c.font_normal,
                c.font_header,
            )
        }) else {
            return;
        };
        for r in credits::draw_rows(y, &rows, [nfs[1] + 2.0, hfs[1] + 2.0]) {
            let Some(text) = trows.get(r.row as usize) else {
                continue;
            };
            if !r.header {
                e.draw_font(
                    set,
                    text,
                    fnormal,
                    [400.0, r.y, 10.0],
                    nfs,
                    [1.0, 1.0, 1.0, r.alpha],
                    ALIGN_CENTER,
                );
            } else {
                e.draw_font(
                    set,
                    text.get(1..).unwrap_or(&[]),
                    fheader,
                    [400.0, r.y, 10.0],
                    hfs,
                    [0.8, 0.8, 0.8, r.alpha],
                    ALIGN_CENTER,
                );
            }
        }
    } else if state >= 2 {
        let Some((end_fs, fnormal, fheader, end_num)) =
            cr(h, |c| (c.end_fs, c.font_normal, c.font_header, c.s.end_num))
        else {
            return;
        };
        let end = e.translate(b"General", b"TheEnd");
        e.draw_font(
            set,
            &end,
            fheader,
            [400.0, 290.0, 10.0],
            end_fs,
            WHITE,
            ALIGN_CENTER,
        );
        let ptest = e.base_flag(FLAG_PTEST);
        let code = credits::end_code(end_num, ptest);
        if code >= 0 {
            let s: &[u8] = match code {
                0 => b"tyr299",
                1 => b"odn314",
                _ => b"lke271",
            };
            e.draw_font(
                set,
                &utf8_to_wide(s),
                fnormal,
                [400.0, 560.0, 10.0],
                [15.0, 15.0],
                [0.65, 0.65, 0.65, 1.0],
                ALIGN_CENTER,
            );
        }
    }
}

pub fn cr_focus_lost(e: &Eng) {
    e.ev("cr_focus_lost", &[]);
    e.music_pause();
}

pub fn cr_focus_got(e: &Eng) {
    e.ev("cr_focus_got", &[]);
    e.music_resume();
}

// ---- demo end ---------------------------------------------------------------------------------

pub const TAG_BUY: i64 = 1;
pub const TAG_EXIT: i64 = 2;
pub const TAG_UPDATE: i64 = 3;
pub const TAG_DRAW: i64 = 4;
pub const TAG_ENTER: i64 = 5;
pub const TAG_LEAVE: i64 = 6;

#[derive(Default)]
pub struct De {
    set: usize,
    vp: usize,
    black: usize,
    bg_gfx: usize,
    font_message: usize,
    font_button: usize,
    gui_size: [f32; 2],
    start: [f32; 3],
    bg_file: Vec<u8>,
    msg_fs: [f32; 2],
    msg_color: [f32; 4],
    avail_fs: [f32; 2],
    avail_color: [f32; 4],
    btn_fs: [f32; 2],
    btn_color: [f32; 4],
    text_w: f32,
    text_y: f32,
    avail_y: f32,
    show_on_all_exit: bool,
    url: Vec<u8>,
    buy: usize,
    exit: usize,
    buy_fade: f32,
    exit_fade: f32,
    buy_hover: bool,
    exit_hover: bool,
    rows: Vec<Vec<u32>>,
    s: DemoEnd,
    upd: i64,
}

pub static DE_STORE: std::sync::LazyLock<Store<De>> = std::sync::LazyLock::new(Store::new);

fn de<R>(h: u64, f: impl FnOnce(&mut De) -> R) -> Option<R> {
    DE_STORE.with(h, f)
}

fn setup_button(e: &Eng, w: usize, fs: [f32; 2], color: [f32; 4], pressed: i64) {
    e.widget_callback(w, MSG_ON_UPDATE, TAG_UPDATE);
    e.widget_callback(w, MSG_ON_DRAW, TAG_DRAW);
    e.widget_callback(w, MSG_MOUSE_ENTER, TAG_ENTER);
    e.widget_callback(w, MSG_MOUSE_LEAVE, TAG_LEAVE);
    e.widget_callback(w, MSG_MOUSE_UP, pressed);
    e.widget_font_size(w, fs);
    e.widget_font_color(w, color);
    e.widget_align(w, ALIGN_CENTER);
    e.widget_autosize(w, true);
    let lp = e.widget_get_local_pos(w);
    let sz = e.widget_get_size(w);
    e.widget_pos(w, [lp[0] - sz[0] * 0.5, lp[1] - 0.0, lp[2] - 0.0]);
}

pub fn de_new(e: &Eng) -> u64 {
    e.ev("de_new", &[]);
    let h = DE_STORE.add(De {
        s: DemoEnd::new(),
        ..De::default()
    });
    let set = e.create_set(b"DemoEnd", 0);
    let (gui_size, off) = e.calc_offset([800.0, 600.0]);
    e.set_virtual_size(set, gui_size, off);
    let vp = e.viewport_new();
    e.viewport_active(vp, false);
    e.viewport_visible(vp, false);
    e.set_active(set, false);
    e.set_draw_mouse(set, false);
    e.viewport_add_set(vp, set);
    let d = CFG_DEMO;
    let sec = b"DemoEnd";
    let skin_file = e.cfg_string(d, sec, b"Skin", b"gui_default.skin");
    let skin = e.create_skin(&skin_file);
    e.set_skin(set, skin);
    let bg_file = e.cfg_string(d, sec, b"BackgroundImage", b"");
    let msg_fs = e.cfg_vec2(d, sec, b"MessageFontSize", 0.0);
    let msg_color = e.cfg_color(d, sec, b"MessageFontColor", WHITE);
    let avail_fs = e.cfg_vec2(d, sec, b"AvailableAtFontSize", 0.0);
    let avail_color = e.cfg_color(d, sec, b"AvailableAtFontColor", WHITE);
    let btn_fs = e.cfg_vec2(d, sec, b"ButtonFontSize", 0.0);
    let btn_color = e.cfg_color(d, sec, b"ButtonFontColor", WHITE);
    let text_w = e.cfg_float(d, sec, b"TextWidth", 0.0);
    let text_y = e.cfg_float(d, sec, b"TextY", 0.0);
    let avail_y = e.cfg_float(d, sec, b"AvailableAtY", 0.0);
    let buy_y = e.cfg_float(d, sec, b"BuyButtonY", 0.0);
    let exit_y = e.cfg_float(d, sec, b"ExitButtonY", 0.0);
    let url = e.cfg_string(d, sec, b"DestinationURL", b"");
    let black = e.gfx_rect([0.0, 0.0, 0.0, 1.0], true);
    e.cfg_float(d, sec, b"FadeSpeed", 0.0);
    let show_on_all_exit = e.cfg_bool(d, sec, b"ShowOnAllExit", false);
    let bsize = [150.0, btn_fs[1]];
    let t = e.translate(b"Demo", b"BuyNow");
    let buy = e.widget_label(set, [400.0, buy_y, 10.0], bsize, &t, 0);
    let t = e.translate(b"Demo", b"Exit");
    let exit = e.widget_label(set, [400.0, exit_y, 10.0], bsize, &t, 0);
    setup_button(e, buy, btn_fs, btn_color, TAG_BUY);
    setup_button(e, exit, btn_fs, btn_color, TAG_EXIT);
    de(h, |d| {
        d.set = set;
        d.vp = vp;
        d.black = black;
        d.gui_size = gui_size;
        d.start = [-off[0], -off[1], 0.0];
        d.bg_file = bg_file;
        d.msg_fs = msg_fs;
        d.msg_color = msg_color;
        d.avail_fs = avail_fs;
        d.avail_color = avail_color;
        d.btn_fs = btn_fs;
        d.btn_color = btn_color;
        d.text_w = text_w;
        d.text_y = text_y;
        d.avail_y = avail_y;
        d.show_on_all_exit = show_on_all_exit;
        d.url = url;
        d.buy = buy;
        d.exit = exit;
        d.s.reset();
    });
    h
}

pub fn de_delete(h: u64) {
    DE_STORE.remove(h);
}

pub fn de_show_on_all_exit(h: u64) -> bool {
    de(h, |d| d.show_on_all_exit).unwrap_or(false)
}

pub fn de_load_fonts(e: &Eng, h: u64) {
    e.ev("de_load_fonts", &[]);
    let m = e.cfg_string(CFG_DEMO, b"DemoEnd", b"MessageFont", b"");
    let b = e.cfg_string(CFG_DEMO, b"DemoEnd", b"ButtonFont", b"");
    let fm = e.font_load(&m);
    let fb = e.font_load(&b);
    let Some((buy, exit)) = de(h, |d| {
        d.font_message = fm;
        d.font_button = fb;
        (d.buy, d.exit)
    }) else {
        return;
    };
    if fb != 0 {
        e.widget_font_type(buy, fb);
        e.widget_font_type(exit, fb);
    }
}

pub fn de_reset(e: &Eng, h: u64) {
    e.ev("de_reset", &[]);
    de(h, |d| d.s.reset());
}

pub fn de_on_enter(e: &Eng, h: u64) {
    e.ev("de_enter", &[]);
    let Some((set, vp, bg_file, fm, tw, mfs)) = de(h, |d| {
        (
            d.set,
            d.vp,
            d.bg_file.clone(),
            d.font_message,
            d.text_w,
            d.msg_fs,
        )
    }) else {
        return;
    };
    e.input_state(INPUT_DEMO_END);
    e.viewport_active(vp, true);
    e.viewport_visible(vp, true);
    e.set_active(set, true);
    e.set_draw_mouse(set, true);
    e.gui_focus(set);
    e.music_stop(0.3);
    let tex = e.tex_2d(&bg_file);
    let gfx = if tex != 0 { e.gfx_from_tex(tex) } else { 0 };
    if gfx != 0 {
        de(h, |d| d.bg_gfx = gfx);
    }
    let text = e.translate(b"Demo", b"EndMessage");
    de(h, |d| d.rows.clear());
    let rows = e.font_wrap(fm, tw, mfs[1] + 2.0, mfs, &text);
    de(h, |d| d.rows.extend(rows));
}

pub fn de_on_leave(e: &Eng, h: u64) {
    e.ev("de_leave", &[]);
    let Some((set, vp, bg)) = de(h, |d| (d.set, d.vp, std::mem::take(&mut d.bg_gfx))) else {
        return;
    };
    e.viewport_active(vp, false);
    e.viewport_visible(vp, false);
    e.set_active(set, false);
    if bg != 0 {
        e.gfx_destroy(bg);
    }
}

pub fn de_update(e: &Eng, h: u64, dt: f32) {
    e.ev("de_update", &[Arg::F(dt)]);
    let Some((up, url)) = de(h, |d| {
        d.upd += 1;
        (d.s.update(dt), d.url.clone())
    }) else {
        return;
    };
    if up & DE_EXIT != 0 {
        e.engine_exit();
        if up & DE_BROWSER != 0 {
            e.open_browser(&url);
        }
    }
}

pub fn de_exit(e: &Eng, h: u64, website: bool) {
    e.ev("de_exit", &[Arg::B(website)]);
    let Some(set) = de(h, |d| {
        d.s.exit(website);
        d.set
    }) else {
        return;
    };
    e.input_state(INPUT_NULL);
    e.set_draw_mouse(set, false);
}

pub fn de_on_draw(e: &Eng, h: u64) {
    let Some((set, black, bg, size, start, fade, fm, upd)) = de(h, |d| {
        (
            d.set,
            d.black,
            d.bg_gfx,
            d.gui_size,
            d.start,
            d.s.fade_alpha,
            d.font_message,
            d.upd,
        )
    }) else {
        return;
    };
    e.ev("de_draw", &[Arg::I(upd)]);
    e.draw_gfx(set, black, start, size, WHITE);
    if bg != 0 {
        let sz = e.gfx_size(bg);
        let (x, w) = premenu::background_rect(sz[0], sz[1]);
        e.draw_gfx(set, bg, [x, 0.0, 5.0], [w, 600.0], WHITE);
    }
    if fm != 0 {
        let Some((rows, ty, mfs, mcol, tw, afs, acol, ay)) = de(h, |d| {
            (
                d.rows.clone(),
                d.text_y,
                d.msg_fs,
                d.msg_color,
                d.text_w,
                d.avail_fs,
                d.avail_color,
                d.avail_y,
            )
        }) else {
            return;
        };
        let ys = credits::message_rows(rows.len(), ty, mfs[1]);
        for (r, y) in rows.iter().zip(ys) {
            if !r.is_empty() {
                e.draw_font(
                    set,
                    r,
                    fm,
                    [400.0 - tw / 2.0, y, 10.0],
                    mfs,
                    mcol,
                    ALIGN_LEFT,
                );
            }
        }
        let t = e.translate(b"Demo", b"AvailableAt");
        e.draw_font(set, &t, fm, [400.0, ay, 10.0], afs, acol, ALIGN_CENTER);
    }
    if fade > 0.0 {
        e.draw_gfx(
            set,
            black,
            [start[0] + 0.0, start[1] + 0.0, start[2] + 20.0],
            size,
            [1.0, 1.0, 1.0, fade],
        );
    }
}

pub fn de_focus_lost(e: &Eng) {
    e.ev("de_focus_lost", &[]);
    e.music_pause();
}

pub fn de_focus_got(e: &Eng) {
    e.ev("de_focus_got", &[]);
    e.music_resume();
}

pub fn de_callback(e: &Eng, h: u64, tag: i64, widget: usize, fval: f32) -> bool {
    match tag {
        TAG_BUY => de_exit(e, h, true),
        TAG_EXIT => de_exit(e, h, false),
        TAG_UPDATE => {
            let Some(color) = de(h, |d| {
                let (fade, hover) = if widget == d.buy {
                    (&mut d.buy_fade, d.buy_hover)
                } else {
                    (&mut d.exit_fade, d.exit_hover)
                };
                credits::button_color(fade, hover, fval, d.btn_color)
            }) else {
                return true;
            };
            e.widget_font_color(widget, color);
        }
        TAG_ENTER | TAG_LEAVE => {
            de(h, |d| {
                let on = tag == TAG_ENTER;
                if widget == d.buy {
                    d.buy_hover = on;
                } else {
                    d.exit_hover = on;
                }
            });
        }
        _ => {}
    }
    true
}
