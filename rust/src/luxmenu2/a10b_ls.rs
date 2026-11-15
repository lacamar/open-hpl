//! `cLuxLoadScreenHandler`.

use super::a10b::{Store, WHITE, v3};
use super::host::{ALIGN_CENTER, Arg, CFG_MENU, Eng, INPUT_GAME, INPUT_LOAD_SCREEN};
use super::loadscreen::{self, Load, STATE_GAME};
use std::sync::atomic::{AtomicU64, Ordering};

#[derive(Default)]
pub struct Ls {
    set: usize,
    vp: usize,
    white: usize,
    image: usize,
    image_name: Vec<u8>,
    gui_size: [f32; 2],
    start: [f32; 3],
    font: usize,
    loading_fs: [f32; 2],
    text_fs: [f32; 2],
    loading_y: f32,
    text_with_image_y: f32,
    text_alone_y: f32,
    image_y: f32,
    text_max_w: f32,
    fade_out: f32,
    dur_mul: f32,
    cat: Vec<u8>,
    entry: Vec<u8>,
    load_image: Vec<u8>,
    end_sound: Vec<u8>,
    cur_entry: Vec<u8>,
    cur_image: Vec<u8>,
    s: Load,
    upd: i64,
}

pub static STORE: std::sync::LazyLock<Store<Ls>> = std::sync::LazyLock::new(Store::new);
static GLOBAL: AtomicU64 = AtomicU64::new(0);

fn with<R>(h: u64, f: impl FnOnce(&mut Ls) -> R) -> Option<R> {
    STORE.with(h, f)
}

pub fn new(e: &Eng) -> u64 {
    e.ev("ls_new", &[]);
    let h = STORE.add(Ls {
        s: Load::new(),
        ..Ls::default()
    });
    let skin = e.create_skin(b"gui_main_menu.skin");
    let set = e.create_set(b"LoadScreen", skin);
    let (gui_size, off) = e.calc_offset([800.0, 600.0]);
    e.set_virtual_size(set, gui_size, off);
    e.set_active(set, false);
    let vp = e.viewport_new();
    e.viewport_active(vp, false);
    e.viewport_visible(vp, false);
    e.viewport_add_set(vp, set);
    let white = e.gfx_rect(WHITE, true);
    let m = CFG_MENU;
    let s = b"LoadScreen";
    let loading_fs = e.cfg_vec2(m, s, b"LoadingFontSize", 0.0);
    let text_fs = e.cfg_vec2(m, s, b"TextFontSize", 0.0);
    let loading_y = e.cfg_float(m, s, b"LoadingY", 0.0);
    let text_with_image_y = e.cfg_float(m, s, b"TextWithImageY", 0.0);
    let text_alone_y = e.cfg_float(m, s, b"TextAloneY", 0.0);
    let image_y = e.cfg_float(m, s, b"ImageY", 0.0);
    let text_max_w = e.cfg_float(m, s, b"TextMaxWidth", 0.0);
    let fade_out = e.cfg_float(m, s, b"FadeOutTime", 0.0);
    let dur_mul = e.cfg_float(m, s, b"TextDurationMul", 0.0);
    with(h, |l| {
        l.set = set;
        l.vp = vp;
        l.white = white;
        l.gui_size = gui_size;
        l.start = [-off[0], -off[1], 0.0];
        l.loading_fs = loading_fs;
        l.text_fs = text_fs;
        l.loading_y = loading_y;
        l.text_with_image_y = text_with_image_y;
        l.text_alone_y = text_alone_y;
        l.image_y = image_y;
        l.text_max_w = text_max_w;
        l.fade_out = fade_out;
        l.dur_mul = dur_mul;
    });
    GLOBAL.store(h, Ordering::Relaxed);
    h
}

pub fn delete(h: u64) {
    let _ = GLOBAL.compare_exchange(h, 0, Ordering::Relaxed, Ordering::Relaxed);
    STORE.remove(h);
}

pub fn load_fonts(e: &Eng, h: u64) {
    e.ev("ls_load_fonts", &[]);
    let file = e.cfg_string(CFG_MENU, b"LoadScreen", b"DefaultFont", b"");
    let f = e.font_load(&file);
    with(h, |l| l.font = f);
}

pub fn reset(e: &Eng, h: u64) {
    e.ev("ls_reset", &[]);
    let img = with(h, |l| {
        l.cat.clear();
        l.entry.clear();
        l.load_image.clear();
        l.s.reset();
        l.cur_entry.clear();
        l.end_sound.clear();
        l.cur_image.clear();
        std::mem::take(&mut l.image)
    })
    .unwrap_or(0);
    if img != 0 {
        e.gfx_destroy(img);
        with(h, |l| l.image_name.clear());
    }
}

pub fn update(e: &Eng, h: u64, dt: f32) {
    e.ev("ls_update", &[Arg::F(dt)]);
    let Some(done) = with(h, |l| {
        l.upd += 1;
        if l.s.state == STATE_GAME {
            let fo = l.fade_out;
            l.s.update_game(dt, fo)
        } else {
            false
        }
    }) else {
        return;
    };
    if done {
        exit(e, h);
    }
}

pub fn on_enter(e: &Eng, h: u64) {
    e.ev("ls_enter", &[]);
    let Some((set, vp)) = with(h, |l| (l.set, l.vp)) else {
        return;
    };
    e.input_state(INPUT_LOAD_SCREEN);
    e.viewport_active(vp, true);
    e.viewport_visible(vp, true);
    e.set_active(set, true);
}

pub fn on_leave(e: &Eng, h: u64) {
    e.ev("ls_leave", &[]);
    let Some((set, vp)) = with(h, |l| (l.set, l.vp)) else {
        return;
    };
    e.viewport_active(vp, false);
    e.viewport_visible(vp, false);
    e.set_active(set, false);
}

pub fn on_draw(e: &Eng, h: u64) {
    let Some((set, white, size, start, alpha, game, upd)) = with(h, |l| {
        (
            l.set,
            l.white,
            l.gui_size,
            l.start,
            l.s.alpha,
            l.s.state == STATE_GAME,
            l.upd,
        )
    }) else {
        return;
    };
    e.ev("ls_draw", &[Arg::I(upd)]);
    if !game {
        return;
    }
    e.draw_gfx(
        set,
        white,
        [start[0], start[1], -1.0],
        size,
        [0.0, 0.0, 0.0, 1.0],
    );
    draw_game_screen(e, h, set);
    e.draw_gfx(
        set,
        white,
        [start[0], start[1], 40.0],
        size,
        [0.0, 0.0, 0.0, 1.0 - alpha],
    );
}

pub fn game_screen_load_done(e: &Eng, h: u64, end_sound: &[u8], load_time: f32) {
    e.ev("ls_load_done", &[Arg::S(end_sound), Arg::F(load_time)]);
    let Some((has_text, cat, entry, mul)) = with(h, |l| {
        (
            !l.cur_entry.is_empty(),
            l.cat.clone(),
            l.cur_entry.clone(),
            l.dur_mul,
        )
    }) else {
        return;
    };
    let mut wanted = 0.0f32;
    if has_text {
        let text = e.translate(&cat, &entry);
        wanted = e.string_duration(&text) * mul;
    }
    with(h, |l| {
        l.s.load_done(has_text.then_some(wanted), load_time);
        l.end_sound = end_sound.to_vec();
    });
    e.updater_container(b"LoadScreen");
}

pub fn setup_load_text(e: &Eng, h: u64, cat: &[u8], entry: &[u8], rand_num: i32, image: &[u8]) {
    e.ev(
        "ls_setup",
        &[
            Arg::S(cat),
            Arg::S(entry),
            Arg::I(i64::from(rand_num)),
            Arg::S(image),
        ],
    );
    with(h, |l| {
        let same =
            l.cat == cat && l.entry == entry && l.load_image == image && l.s.rand_num == rand_num;
        l.s.setup(same, rand_num);
        if !same {
            l.cat = cat.to_vec();
            l.entry = entry.to_vec();
            l.load_image = image.to_vec();
        }
    });
}

pub fn draw_game_screen_public(e: &Eng, h: u64) {
    e.ev("ls_draw_game_screen", &[]);
    let entry = with(h, |l| {
        let entry = if l.cat.is_empty() || l.entry.is_empty() {
            Vec::new()
        } else {
            let mut x = l.entry.clone();
            x.extend(l.s.text_suffix());
            x
        };
        l.cur_entry = entry;
        l.cur_image = l.load_image.clone();
        l.s.loading_alpha = 1.0;
    });
    if entry.is_none() {
        return;
    }
    let set = e.help_set();
    draw_game_screen(e, h, set);
    e.draw_set_to_screen();
}

pub fn draw_menu_screen(e: &Eng, h: u64) {
    e.ev("ls_draw_menu_screen", &[]);
    let Some(font) = with(h, |l| l.font) else {
        return;
    };
    let loading = e.translate(b"General", b"Loading");
    let set = e.help_set();
    let img = e.gfx_tex_file(b"menu_loading_screen.jpg");
    let sz = e.gfx_size(img);
    e.draw_gfx_pos(
        set,
        img,
        [
            loadscreen::centered(sz[0], 400.0),
            loadscreen::centered(sz[1], 300.0),
            0.0,
        ],
    );
    e.draw_font(
        set,
        &loading,
        font,
        [400.0, 530.0, 1.0],
        [20.0, 20.0],
        WHITE,
        ALIGN_CENTER,
    );
    e.draw_set_to_screen();
    e.gfx_destroy(img);
}

pub fn draw_menu_screen_global(e: &Eng) {
    let h = GLOBAL.load(Ordering::Relaxed);
    if h != 0 {
        draw_menu_screen(e, h);
    }
}

pub fn exit_pressed(e: &Eng, h: u64) {
    e.ev("ls_exit_pressed", &[]);
    with(h, |l| l.s.exit_pressed());
}

fn exit(e: &Eng, h: u64) {
    let Some(img) = with(h, |l| std::mem::take(&mut l.image)) else {
        return;
    };
    if img != 0 {
        e.gfx_destroy(img);
        with(h, |l| l.image_name.clear());
    }
    e.updater_container(b"Default");
    e.input_state(INPUT_GAME);
    if let Some((game, snd)) = with(h, |l| (l.s.state == STATE_GAME, l.end_sound.clone()))
        && game
    {
        e.play_gui_sound(&snd, 0);
    }
}

fn draw_game_screen(e: &Eng, h: u64, set: usize) {
    let Some((name, have, font)) = with(h, |l| {
        (
            l.cur_image.clone(),
            l.image != 0 && l.image_name == l.cur_image,
            l.font,
        )
    }) else {
        return;
    };
    if !have {
        let img = e.gfx_tex_file(&name);
        with(h, |l| {
            l.image_name = name;
            l.image = img;
        });
    }
    let Some((img, image_y, alpha)) = with(h, |l| (l.image, l.image_y, l.s.loading_alpha)) else {
        return;
    };
    if img != 0 {
        let sz = e.gfx_size(img);
        e.draw_gfx(
            set,
            img,
            [loadscreen::centered(sz[0], 400.0), image_y, 0.5],
            [-1.0, -1.0],
            [1.0, 1.0, 1.0, alpha],
        );
    }
    let Some((entry, cat, text_fs, loading_fs, max_w, with_img, alone, loading_y)) = with(h, |l| {
        (
            l.cur_entry.clone(),
            l.cat.clone(),
            l.text_fs,
            l.loading_fs,
            l.text_max_w,
            l.text_with_image_y,
            l.text_alone_y,
            l.loading_y,
        )
    }) else {
        return;
    };
    if !entry.is_empty() {
        let text = e.translate(&cat, &entry);
        let rows = e.font_wrap(font, max_w, text_fs[1], text_fs, &text);
        let fy = loadscreen::text_y(alpha, with_img, alone);
        let mut pos = [400.0 - max_w / 2.0, fy, 1.0];
        for r in &rows {
            e.draw_font(set, r, font, pos, text_fs, WHITE, 0);
            pos[1] += text_fs[1] + 2.0;
        }
        let loading = e.translate(b"General", b"Loading");
        e.draw_font(
            set,
            &loading,
            font,
            [400.0, loading_y, 1.0],
            loading_fs,
            [1.0, 0.0, 0.0, alpha],
            ALIGN_CENTER,
        );
    } else {
        let loading = e.translate(b"General", b"Loading");
        e.draw_font(
            set,
            &loading,
            font,
            v3([400.0, loadscreen::centered(loading_fs[1], 300.0)], 1.0),
            loading_fs,
            WHITE,
            ALIGN_CENTER,
        );
    }
}

pub fn text_field(h: u64, which: i32) -> Vec<u8> {
    with(h, |l| match which {
        0 => l.cat.clone(),
        1 => l.entry.clone(),
        _ => l.load_image.clone(),
    })
    .unwrap_or_default()
}

pub fn rand_num(h: u64) -> i32 {
    with(h, |l| l.s.rand_num).unwrap_or(0)
}

pub fn prev_bits(h: u64) -> i32 {
    with(h, |l| l.s.prev_bits).unwrap_or(0)
}

pub fn set_prev_bits(h: u64, v: i32) {
    with(h, |l| l.s.prev_bits = v);
}
