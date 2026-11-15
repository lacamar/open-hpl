//! GUI half of the inventory (A11a): set, widgets and slots creation, widget callbacks, drawing, the blurred
//! screen background. Engine objects are host pointers; every call goes through `invhost`.

use super::event::Out;
use super::inventory::{self as inv};
use super::invhost::{self as host, A, game, set, wd};
use super::invobj::{self as obj, ItemRef, g};
use super::itemtype;
use crate::math::rand_rectf;

pub const MAT_ALPHA: i32 = 0;
pub const MAT_ADD: i32 = 1;
pub const MAT_DIFFUSE: i32 = 2;
pub const MAT_MOD: i32 = 3;
pub const MAT_DEFAULT: i32 = -1;

pub const ALIGN_LEFT: i32 = 0;
pub const ALIGN_CENTER: i32 = 2;

const MSG_MOUSE_DOWN: i32 = 0;
const MSG_MOUSE_UP: i32 = 1;
const MSG_MOUSE_DBL: i32 = 2;
const MSG_MOUSE_ENTER: i32 = 3;
const MSG_MOUSE_LEAVE: i32 = 4;
const MSG_UPDATE: i32 = 5;
const MSG_DRAW: i32 = 6;
const MSG_GET_NAV: i32 = 7;
const MSG_LOSE_NAV: i32 = 8;
const MSG_UI_PRESS: i32 = 9;
const MSG_UI_DBL_PRESS: i32 = 10;
const MSG_UI_RELEASE: i32 = 11;

const ARROW_UP: i32 = 0;
const ARROW_RIGHT: i32 = 1;
const ARROW_DOWN: i32 = 2;
const ARROW_LEFT: i32 = 3;

/// Callback ids: `kind << 16 | slot`.
mod cb {
    pub const SLOT_DOWN: usize = 1;
    pub const SLOT_UP: usize = 2;
    pub const SLOT_DBL: usize = 3;
    pub const SLOT_ENTER: usize = 4;
    pub const SLOT_LEAVE: usize = 5;
    pub const SLOT_GET_NAV: usize = 6;
    pub const SLOT_LOSE_NAV: usize = 7;
    pub const SLOT_UI_PRESS: usize = 8;
    pub const SLOT_UI_DBL: usize = 9;
    pub const SLOT_UI_RELEASE: usize = 10;
    pub const SLOT_UPDATE: usize = 11;
    pub const SLOT_DRAW: usize = 12;
    pub const SANITY_DRAW: usize = 20;
    pub const SANITY_UPDATE: usize = 21;
    pub const HEALTH_DRAW: usize = 22;
    pub const HEALTH_UPDATE: usize = 23;
    pub const OIL_DRAW: usize = 24;
    pub const OIL_UPDATE: usize = 25;
    pub const OIL_UP: usize = 26;
    pub const TINDER_DRAW: usize = 27;
    pub const TINDER_UPDATE: usize = 28;
    pub const JOURNAL_DRAW: usize = 29;
    pub const JOURNAL_UPDATE: usize = 30;
    pub const JOURNAL_DOWN: usize = 31;
    pub const JOURNAL_UI_PRESS: usize = 32;
    pub const SLOTS_DRAW: usize = 33;
    pub const UI_BUTTON_PRESS: usize = 34;
    pub const WIDGET_ENTER: usize = 35;
    pub const WIDGET_GET_NAV: usize = 36;
    pub const WIDGET_LOSE_NAV: usize = 37;
    pub const FOCUS_DRAW: usize = 38;
}

fn token(kind: usize, slot: usize) -> usize {
    kind << 16 | slot
}

#[derive(Default, Clone, Copy)]
pub struct Frame {
    pub corners: [usize; 4],
    pub borders: [usize; 4],
}

#[derive(Default, Clone, Copy)]
pub struct Block {
    pub center: [f32; 3],
    pub frame_size: [f32; 2],
    pub hpad: [f32; 2],
    pub vpad: [f32; 2],
    pub offset: [f32; 3],
}

#[derive(Default, Clone, Copy)]
pub struct Slot {
    pub widget: usize,
    pub glow_gfx: usize,
    pub glow_alpha: f32,
}

#[derive(Default)]
pub struct Gui {
    pub set: usize,
    pub font_default: usize,
    pub font_header: usize,
    pub size: [f32; 2],
    pub offset: [f32; 2],
    pub start: [f32; 3],
    pub screen: [f32; 2],
    pub health_frame: Frame,
    pub sanity_frame: Frame,
    pub generic_frame: Frame,
    pub sanity_status: [usize; 4],
    pub sanity_glow: [usize; 4],
    pub health_status: [usize; 4],
    pub health_glow: [usize; 4],
    pub oil_fg: usize,
    pub oil_bubble: [usize; 3],
    pub oil_liquid: usize,
    pub journal_over: usize,
    pub white: usize,
    pub screen_tex: usize,
    pub screen_gfx: usize,
    pub bg_tex: usize,
    pub bg_gfx: usize,
    pub health_widget: usize,
    pub sanity_widget: usize,
    pub tinder_widget: usize,
    pub oil_widget: usize,
    pub journal_widget: usize,
    pub image_widgets: Vec<usize>,
    pub slots: Vec<Slot>,
}

#[derive(Default)]
pub struct Layout {
    pub popup_center: [f32; 3],
    pub journal_center: [f32; 3],
    pub journal: Block,
    pub slot_gfx: Vec<u8>,
    pub slot_size: [f32; 2],
    pub slot_color: [f32; 4],
    pub slot_over_gfx: Vec<u8>,
    pub slot_over_color: [f32; 4],
    pub pulse_min: f32,
    pub pulse_amp: f32,
    pub pulse_freq: f32,
    pub rows: i32,
    pub cols: i32,
    pub grid_color: [f32; 4],
    pub slots_start: [f32; 3],
    pub slots_sep: [f32; 3],
    pub draw_grid: bool,
    pub draw_frame: bool,
    pub slots_frame: Block,
    pub grid_hext: [f32; 2],
    pub grid_vext: [f32; 2],
    pub grid_offset: [f32; 3],
    pub sanity: Block,
    pub health: Block,
    pub tinder: Block,
    pub oil: Block,
    pub item_header_center: [f32; 3],
    pub item_desc_center: [f32; 3],
    pub text_frame_size: [f32; 2],
    pub text_frame_hpad: [f32; 2],
    pub text_frame_vpad: [f32; 2],
    pub text_frame_offset: [f32; 3],
    pub desc_width: f32,
    pub count_offset: [f32; 3],
    pub font_count: [f32; 2],
    pub font_label: [f32; 2],
    pub font_header: [f32; 2],
    pub font_desc: [f32; 2],
    pub font_message: [f32; 2],
}

fn block(prefix: &str, with_center: bool) -> Block {
    let k = |s: &str| format!("{prefix}{s}").into_bytes();
    Block {
        center: if with_center {
            host::cfg_v3(&k("Center"), [0.0; 3])
        } else {
            [0.0; 3]
        },
        frame_size: host::cfg_v2(&k("FrameSize"), [-1.0; 2]),
        hpad: host::cfg_v2(&k("FrameHPadding"), [0.0; 2]),
        vpad: host::cfg_v2(&k("FrameVPadding"), [0.0; 2]),
        offset: host::cfg_v3(&k("FrameOffset"), [0.0; 3]),
    }
}

fn load_layout() -> Layout {
    let mut l = Layout {
        popup_center: host::cfg_v3(b"PopUpMessageCenter", [0.0; 3]),
        journal_center: host::cfg_v3(b"JournalCenter", [0.0; 3]),
        journal: block("Journal", false),
        slot_gfx: host::cfg_string(b"SlotImage", b""),
        slot_size: host::cfg_v2(b"SlotSize", [-1.0; 2]),
        slot_color: host::cfg_color(b"SlotColor", [1.0, 1.0, 1.0, 1.0]),
        slot_over_gfx: host::cfg_string(b"SlotMouseOverImage", b""),
        slot_over_color: host::cfg_color(b"SlotMouseOverColor", [1.0, 1.0, 1.0, 1.0]),
        pulse_min: host::cfg_float(b"SlotPulsatingMin", 1.25),
        pulse_amp: host::cfg_float(b"SlotPulsatingAmp", 0.25),
        pulse_freq: host::cfg_float(b"SlotPulsatingFreq", 2.0),
        rows: host::cfg_int(b"SlotsRows", 3),
        cols: host::cfg_int(b"SlotsColumns", 6),
        grid_color: host::cfg_color(b"SlotsGridColor", [0.3, 0.3, 0.3, 0.5]),
        slots_start: host::cfg_v3(b"SlotsStart", [0.0; 3]),
        slots_sep: host::cfg_v3(b"SlotsSeparation", [0.0; 3]),
        slots_frame: Block {
            center: [0.0; 3],
            frame_size: host::cfg_v2(b"SlotsFrameSize", [-1.0; 2]),
            hpad: host::cfg_v2(b"SlotsFrameHPadding", [0.0; 2]),
            vpad: host::cfg_v2(b"SlotsFrameVPadding", [0.0; 2]),
            offset: host::cfg_v3(b"SlotsFrameOffset", [0.0; 3]),
        },
        draw_grid: host::cfg_bool(b"SlotsDrawGrid", true),
        draw_frame: host::cfg_bool(b"SlotsDrawFrame", true),
        grid_hext: host::cfg_v2(b"SlotsGridHExtension", [0.0; 2]),
        grid_vext: host::cfg_v2(b"SlotsGridVExtension", [0.0; 2]),
        grid_offset: host::cfg_v3(b"SlotsGridOffset", [0.0; 3]),
        health: block("Health", true),
        sanity: block("Sanity", true),
        tinder: block("Tinderboxes", true),
        oil: block("Oil", true),
        ..Layout::default()
    };
    l.journal.center = l.journal_center;
    l
}

fn load_text_layout(l: &mut Layout) {
    l.item_header_center = host::cfg_v3(b"ItemHeaderCenter", [0.0; 3]);
    l.item_desc_center = host::cfg_v3(b"ItemDescCenter", [0.0; 3]);
    l.text_frame_size = host::cfg_v2(b"ItemTextFrameSize", [-1.0; 2]);
    l.text_frame_hpad = host::cfg_v2(b"ItemTextFrameHPadding", [0.0; 2]);
    l.text_frame_vpad = host::cfg_v2(b"ItemTextFrameVPadding", [0.0; 2]);
    l.text_frame_offset = host::cfg_v3(b"ItemTextFrameOffset", [0.0; 3]);
    l.desc_width = host::cfg_float(b"ItemDescTextWidth", 400.0);
    if l.text_frame_size == [-1.0, -1.0] {
        l.text_frame_size = inv::item_text_frame_default(l.desc_width);
    }
    l.count_offset = host::cfg_v3(b"ItemCountOffset", [0.0; 3]);
    l.font_count = host::cfg_v2(b"FontSize_ItemCount", [0.0; 2]);
    l.font_label = host::cfg_v2(b"FontSize_Label", [0.0; 2]);
    l.font_header = host::cfg_v2(b"FontSize_Header", [0.0; 2]);
    l.font_desc = host::cfg_v2(b"FontSize_Description", [0.0; 2]);
    l.font_message = host::cfg_v2(b"FontSize_Message", [0.0; 2]);
}

fn add_cb(wd: usize, msg: i32, kind: usize, slot: usize) {
    host::widget_callback(wd, msg, token(kind, slot));
}

fn frame_images(kind: &str) -> Frame {
    let mut f = Frame::default();
    for (slot, s) in f.corners.iter_mut().zip(["lu", "ru", "rd", "ld"]) {
        *slot = host::gfx_image(
            format!("inventory_frame_{kind}_corner_{s}.tga").as_bytes(),
            MAT_ALPHA,
        );
    }
    for (slot, s) in f.borders.iter_mut().zip(["u", "r", "d", "l"]) {
        *slot = host::gfx_image(
            format!("inventory_frame_{kind}_border_{s}.tga").as_bytes(),
            MAT_ALPHA,
        );
    }
    f
}

/// The constructor: set, viewport, images, program and widgets.
pub fn build(inv: &mut obj::Inv) {
    let mut a = A::new();
    a.p[0] = token(cb::FOCUS_DRAW, 0);
    host::call(host::op::GUI_INIT, &mut a);
    inv.gui.set = a.r;
    inv.gui.size = [a.f[0], a.f[1]];
    inv.gui.offset = [a.f[2], a.f[3]];
    inv.gui.screen = [a.f[4], a.f[5]];
    inv.gui.start = [-a.f[2], -a.f[3], 0.0];
    inv.st = inv::InvState::default();
    inv.st.fade_in_time = host::cfg_float(b"FadeInTime", 10.0);
    inv.st.fade_out_time = host::cfg_float(b"FadeOutTime", 10.0);
    inv.st.picked_at = -1.0;
    inv.st.picked_moved = 0;
    for f in &mut inv.faders {
        f.fade_in = 1.0;
        f.fade_out = 1.0;
    }
    inv.gui.sanity_frame = frame_images("sanity");
    inv.gui.health_frame = frame_images("health");
    inv.gui.generic_frame = frame_images("generic");
    inv.gui.white = host::gfx_rect([1.0, 1.0, 1.0, 1.0], MAT_ALPHA);
    let mut p = A::new();
    host::ll(host::ll::PROGRAM_NEW, &mut p);
}

/// `CreateGui` after the constructor's images; called by the shell right after [`build`].
pub fn create_gui() {
    let mut l = load_layout();
    let center = l.oil.center;
    let mut bubbles = [[0.0f32; 3]; 3];
    for b in &mut bubbles {
        let base = [center[0], center[1], center[2] + 0.7];
        let r = [
            rand_rectf(-10.0, 10.0),
            rand_rectf(-10.0, 10.0),
            rand_rectf(0.0, 0.0),
        ];
        *b = [base[0] + r[0], base[1] + r[1], base[2] + r[2]];
    }
    let Some(i) = g() else { return };
    let (fi, fo) = (i.st.fade_in_time, i.st.fade_out_time);
    inv::init(&mut i.st, fi, fo, bubbles);
    load_text_layout(&mut l);
    i.layout = l;

    let suffix = ["25", "50", "75", "100"];
    let mut gi = std::mem::take(&mut i.gui);
    for (n, s) in suffix.iter().enumerate() {
        let img = |p: &str| host::gfx_image(format!("inventory/{p}{s}.tga").as_bytes(), MAT_ALPHA);
        let made = [
            img("inventory_sanity_"),
            img("inventory_sanity_glow_"),
            img("inventory_health_"),
            img("inventory_health_glow_"),
        ];
        let lists = [
            &mut gi.sanity_status,
            &mut gi.sanity_glow,
            &mut gi.health_status,
            &mut gi.health_glow,
        ];
        for (list, v) in lists.into_iter().zip(made) {
            if let Some(slot) = list.get_mut(n) {
                *slot = v;
            }
        }
    }
    let mut lay = std::mem::take(&mut i.layout);
    let _ = i;
    create_widgets(&mut gi, &mut lay);
    if let Some(i) = g() {
        i.gui = gi;
        i.layout = lay;
    }
}

fn status_widget(gi: &mut Gui, fader: i32, kind_draw: usize, kind_update: usize) -> usize {
    let w = host::widget_new_image(b"", [0.0; 3], 0);
    host::widget_set(w, wd::SET_USER_VALUE, &[], fader, 0);
    add_cb(w, MSG_DRAW, kind_draw, 0);
    add_cb(w, MSG_UPDATE, kind_update, 0);
    add_cb(w, MSG_MOUSE_ENTER, cb::WIDGET_ENTER, 0);
    add_cb(w, MSG_MOUSE_LEAVE, cb::WIDGET_ENTER, 0);
    gi.image_widgets.push(w);
    w
}

fn centered(w: usize, center: [f32; 3]) {
    let sz = host::widget_get(w, wd::GET_SIZE).f;
    host::widget_set(
        w,
        wd::SET_POSITION,
        &[center[0] - sz[0] * 0.5, center[1] - sz[1] * 0.5, center[2]],
        0,
        0,
    );
}

fn create_widgets(gi: &mut Gui, l: &mut Layout) {
    let sanity = status_widget(gi, obj_fader::SANITY, cb::SANITY_DRAW, cb::SANITY_UPDATE);
    add_cb(sanity, MSG_UI_PRESS, cb::UI_BUTTON_PRESS, 0);
    host::widget_set(sanity, wd::SET_GLOBAL_UI, &[], 1, 0);
    gi.sanity_widget = sanity;

    gi.health_widget = status_widget(gi, obj_fader::HEALTH, cb::HEALTH_DRAW, cb::HEALTH_UPDATE);

    gi.oil_fg = host::gfx_image(b"inventory/inventory_oil_fg.tga", MAT_ALPHA);
    for (n, slot) in gi.oil_bubble.iter_mut().enumerate() {
        *slot = host::gfx_image(
            format!("inventory/inventory_oil_bubble_{}.tga", n + 1).as_bytes(),
            MAT_ALPHA,
        );
    }
    gi.oil_liquid = host::gfx_image(b"inventory/inventory_oil_liquid.tga", MAT_ALPHA);

    let oil = host::widget_new_image(b"inventory/inventory_oil_bg.tga", [0.0; 3], 0);
    centered(oil, l.oil.center);
    host::widget_set(oil, wd::SET_USER_VALUE, &[], obj_fader::OIL, 0);
    add_cb(oil, MSG_DRAW, cb::OIL_DRAW, 0);
    add_cb(oil, MSG_UPDATE, cb::OIL_UPDATE, 0);
    add_cb(oil, MSG_MOUSE_ENTER, cb::WIDGET_ENTER, 0);
    add_cb(oil, MSG_MOUSE_LEAVE, cb::WIDGET_ENTER, 0);
    add_cb(oil, MSG_MOUSE_UP, cb::OIL_UP, 0);
    gi.image_widgets.push(oil);
    gi.oil_widget = oil;

    let tinder = host::widget_new_image(b"inventory/inventory_tinderboxes.tga", [0.0; 3], 0);
    host::widget_set(tinder, wd::SET_USER_VALUE, &[], obj_fader::TINDER, 0);
    centered(tinder, l.tinder.center);
    add_cb(tinder, MSG_DRAW, cb::TINDER_DRAW, 0);
    add_cb(tinder, MSG_UPDATE, cb::TINDER_UPDATE, 0);
    add_cb(tinder, MSG_MOUSE_ENTER, cb::WIDGET_ENTER, 0);
    add_cb(tinder, MSG_MOUSE_LEAVE, cb::WIDGET_ENTER, 0);
    gi.image_widgets.push(tinder);
    gi.tinder_widget = tinder;

    gi.journal_over = host::gfx_image(b"inventory/inventory_journal_mouse_over.tga", MAT_ALPHA);
    let journal = host::widget_new_image(b"inventory/inventory_journal.tga", [0.0; 3], 0);
    centered(journal, l.journal_center);
    host::widget_set(journal, wd::SET_USER_VALUE, &[], obj_fader::JOURNAL, 0);
    add_cb(journal, MSG_DRAW, cb::JOURNAL_DRAW, 0);
    add_cb(journal, MSG_UPDATE, cb::JOURNAL_UPDATE, 0);
    add_cb(journal, MSG_MOUSE_DOWN, cb::JOURNAL_DOWN, 0);
    add_cb(journal, MSG_UI_PRESS, cb::JOURNAL_UI_PRESS, 0);
    add_cb(journal, MSG_MOUSE_ENTER, cb::WIDGET_ENTER, 0);
    add_cb(journal, MSG_MOUSE_LEAVE, cb::WIDGET_ENTER, 0);
    gi.image_widgets.push(journal);

    for &w in &gi.image_widgets {
        add_cb(w, MSG_GET_NAV, cb::WIDGET_GET_NAV, 0);
        add_cb(w, MSG_LOSE_NAV, cb::WIDGET_LOSE_NAV, 0);
    }
    gi.journal_widget = journal;

    let group = host::widget_new_dummy(l.slots_start);
    add_cb(group, MSG_DRAW, cb::SLOTS_DRAW, 0);

    for y in 0..l.rows {
        for x in 0..l.cols {
            let w = host::widget_new_image(b"", [0.0; 3], group);
            let idx = gi.slots.len();
            let slot = build_slot(w, idx, l);
            let size = host::widget_get(w, wd::GET_SIZE).f;
            host::widget_set(
                w,
                wd::SET_POSITION,
                &[
                    x as f32 * (size[0] + l.slots_sep[0]),
                    y as f32 * (size[1] + l.slots_sep[1]),
                    0.0,
                ],
                0,
                0,
            );
            gi.slots.push(slot);
            gi.image_widgets.push(w);
        }
    }
    let cols = l.cols as usize;
    let sw = |n: usize| gi.slots.get(n).map_or(0, |s| s.widget);
    host::set_widget(set::DEFAULT_NAV, gi.health_widget);
    let nav = |w: usize, arrow: i32, to: usize| host::widget_set(w, wd::SET_NAV, &[], arrow, to);
    nav(gi.health_widget, ARROW_DOWN, gi.sanity_widget);
    nav(gi.health_widget, ARROW_RIGHT, sw(0));
    nav(gi.sanity_widget, ARROW_UP, gi.health_widget);
    nav(gi.sanity_widget, ARROW_RIGHT, sw(2 * cols));
    nav(gi.tinder_widget, ARROW_LEFT, sw(cols - 1));
    nav(gi.tinder_widget, ARROW_DOWN, gi.oil_widget);
    nav(gi.oil_widget, ARROW_UP, gi.tinder_widget);
    nav(gi.oil_widget, ARROW_LEFT, sw(cols + cols - 1));
    nav(gi.oil_widget, ARROW_DOWN, gi.journal_widget);
    nav(gi.journal_widget, ARROW_UP, gi.oil_widget);
    nav(gi.journal_widget, ARROW_LEFT, sw(2 * cols + cols - 1));

    let rows = l.rows as usize;
    for y in 0..rows {
        for x in 0..cols {
            let me = sw(y * cols + x);
            let left = if x >= 1 { sw(y * cols + x - 1) } else { 0 };
            let right = if x + 1 < cols {
                sw(y * cols + x + 1)
            } else {
                0
            };
            let above = if y >= 1 { sw((y - 1) * cols + x) } else { 0 };
            let below = if y + 1 < rows {
                sw((y + 1) * cols + x)
            } else {
                0
            };
            nav(me, ARROW_UP, above);
            nav(me, ARROW_RIGHT, right);
            nav(me, ARROW_DOWN, below);
            nav(me, ARROW_LEFT, left);
        }
    }
    nav(sw(0), ARROW_LEFT, gi.health_widget);
    nav(sw(cols), ARROW_LEFT, gi.health_widget);
    nav(sw(2 * cols), ARROW_LEFT, gi.sanity_widget);
    nav(sw(cols - 1), ARROW_RIGHT, gi.tinder_widget);
    nav(sw(cols + cols - 1), ARROW_RIGHT, gi.oil_widget);
    nav(sw(2 * cols + cols - 1), ARROW_RIGHT, gi.oil_widget);

    let first = sw(0);
    if let Some(i) = g() {
        i.last_current_widget = first;
    }
}

fn build_slot(w: usize, idx: usize, l: &mut Layout) -> Slot {
    if !l.slot_gfx.is_empty() {
        host::gfx_image(&l.slot_gfx, MAT_ALPHA);
    }
    let gfx = host::gfx_rect([1.0, 1.0, 1.0, 1.0], MAT_ALPHA);
    host::gfx_set_color(gfx, l.slot_color);
    host::widget_set(w, wd::SET_IMAGE, &[], 0, gfx);
    if l.slot_size == [-1.0, -1.0] {
        let s = host::widget_get(w, wd::GET_SIZE).f;
        l.slot_size = [s[0], s[1]];
    } else {
        host::widget_set(w, wd::SET_SIZE, &l.slot_size[..], 0, 0);
    }
    for (msg, kind) in [
        (MSG_MOUSE_DOWN, cb::SLOT_DOWN),
        (MSG_MOUSE_UP, cb::SLOT_UP),
        (MSG_MOUSE_DBL, cb::SLOT_DBL),
        (MSG_MOUSE_ENTER, cb::SLOT_ENTER),
        (MSG_MOUSE_LEAVE, cb::SLOT_LEAVE),
        (MSG_UPDATE, cb::SLOT_UPDATE),
        (MSG_DRAW, cb::SLOT_DRAW),
        (MSG_GET_NAV, cb::SLOT_GET_NAV),
        (MSG_LOSE_NAV, cb::SLOT_LOSE_NAV),
        (MSG_UI_PRESS, cb::SLOT_UI_PRESS),
        (MSG_UI_DBL_PRESS, cb::SLOT_UI_DBL),
        (MSG_UI_RELEASE, cb::SLOT_UI_RELEASE),
    ] {
        add_cb(w, msg, kind, idx);
    }
    let mut glow = 0;
    if !l.slot_over_gfx.is_empty() {
        glow = host::gfx_image(&l.slot_over_gfx, MAT_ALPHA);
    }
    if glow == 0 {
        glow = host::gfx_rect([1.0, 1.0, 1.0, 1.0], MAT_ALPHA);
        host::gfx_set_active_size(glow, l.slot_size);
    }
    host::gfx_set_color(glow, l.slot_over_color);
    Slot {
        widget: w,
        glow_gfx: glow,
        glow_alpha: 0.0,
    }
}

pub mod obj_fader {
    pub use super::inv::{
        FADER_HEALTH as HEALTH, FADER_JOURNAL as JOURNAL, FADER_OIL as OIL, FADER_SANITY as SANITY,
        FADER_TINDERBOX as TINDER,
    };
}

// ---- drawing helpers ------------------------------------------------------------------------

fn ev_color(e: &super::event::Ev) -> [f32; 4] {
    [e.v[5], e.v[5], e.v[5], e.v[8]]
}

fn ev_rgba(e: &super::event::Ev) -> [f32; 4] {
    [e.v[5], e.v[6], e.v[7], e.v[8]]
}

fn ev_pos(e: &super::event::Ev) -> [f32; 3] {
    [e.v[0], e.v[1], e.v[2]]
}

fn ev_size(e: &super::event::Ev) -> [f32; 2] {
    [e.v[3], e.v[4]]
}

fn draw_frame_events(o: &Out, f: &Frame) {
    for e in &o.evs {
        if e.kind == inv::DRAW_CORNER {
            let g = f.corners.get(e.a as usize).copied().unwrap_or(0);
            host::draw_gfx(g, ev_pos(e), [-1.0; 2], ev_color(e), MAT_DEFAULT);
        } else {
            let g = f.borders.get(e.a as usize).copied().unwrap_or(0);
            host::draw_gfx(g, ev_pos(e), ev_size(e), ev_color(e), MAT_DEFAULT);
        }
    }
}

fn corner_sizes(f: &Frame) -> ([f32; 2], [f32; 2], [f32; 2]) {
    (
        host::gfx_sizes(f.corners[0]).0,
        host::gfx_sizes(f.corners[1]).0,
        host::gfx_sizes(f.corners[3]).0,
    )
}

fn alpha() -> f32 {
    g().map_or(0.0, |i| i.st.alpha)
}

fn draw_frame_around_widget(f: Frame, widget: usize, b: &Block) {
    let (c0, c1, c3) = corner_sizes(&f);
    let pos = host::widget_get(widget, wd::GET_GLOBAL).f;
    let size = host::widget_get(widget, wd::GET_SIZE).f;
    let fr = inv::FrameIn {
        widget_pos: [pos[0], pos[1], pos[2]],
        widget_size: [size[0], size[1]],
        hpad: b.hpad,
        vpad: b.vpad,
        offset: b.offset,
        size: b.frame_size,
        c0,
        c1,
        c3,
        alpha: alpha(),
    };
    let mut o = Out::default();
    inv::draw_frame_around(&fr, &mut o);
    draw_frame_events(&o, &f);
}

fn draw_frame(f: Frame, pos: [f32; 3], size: [f32; 2]) {
    let (c0, c1, c3) = corner_sizes(&f);
    let mut o = Out::default();
    inv::draw_frame(pos, size, c0, c1, c3, alpha(), &mut o);
    draw_frame_events(&o, &f);
}

fn draw_element_at_center(center: [f32; 3], gfx: usize, a: f32, size: [f32; 2], mul: bool) {
    let image = host::gfx_sizes(gfx).1;
    let (pos, sz, c) = inv::element_at_center(center, image, size, a, mul);
    host::draw_gfx(gfx, pos, sz, c, MAT_DEFAULT);
}

fn white_color(a: f32) -> [f32; 4] {
    [1.0, 1.0, 1.0, a]
}

// ---- background ----------------------------------------------------------------------------

pub fn create_background() {
    let mut a = A::new();
    host::ll(host::ll::SCREEN_SIZE_INT, &mut a);
    let (wi, hi) = (a.i[1], a.i[2]);
    let new_tex = |name: &[u8], wrap: bool| {
        let mut t = A::new();
        t.s[0] = super::event::Str::of(name);
        t.i[1] = i32::from(wrap);
        t.i[2] = wi;
        t.i[3] = hi;
        host::ll(host::ll::TEXTURE_NEW, &mut t);
        t.r
    };
    let screen_tex = new_tex(b"Screen", true);
    let bg_tex = new_tex(b"ScreenBlur", false);
    let screen_gfx = host::gfx_texture(screen_tex, MAT_DIFFUSE);
    let bg_gfx = host::gfx_texture(bg_tex, MAT_ALPHA);
    let size = if let Some(i) = g() {
        i.gui.screen_tex = screen_tex;
        i.gui.bg_tex = bg_tex;
        i.gui.screen_gfx = screen_gfx;
        i.gui.bg_gfx = bg_gfx;
        i.gui.screen
    } else {
        return;
    };

    let step = |sub: i32, f: &dyn Fn(&mut A)| {
        let mut a = A::new();
        f(&mut a);
        host::ll(sub, &mut a);
        a.r
    };
    use host::ll as l;
    let fb = step(l::FB_NEW, &|a| a.p[0] = bg_tex);
    step(l::BG_SCREEN, &|a| a.i[1] = 0);
    step(l::BLEND, &|a| a.i[1] = 0);
    step(l::DEPTH_TEST, &|a| a.i[1] = 0);
    step(l::DEPTH_WRITE, &|a| a.i[1] = 0);
    step(l::ORTHO, &|a| a.f[..2].copy_from_slice(&size));
    step(l::IDENTITY, &|_| {});
    step(l::COPY_SCREEN, &|a| a.p[0] = screen_tex);
    step(l::PROGRAM, &|a| a.i[1] = 1);
    step(l::SET_FB, &|a| a.p[0] = fb);
    step(l::SET_TEXTURE, &|a| a.p[0] = screen_tex);
    step(l::DRAW_QUAD, &|a| a.f[..2].copy_from_slice(&size));
    step(l::PROGRAM, &|a| a.i[1] = 0);
    step(l::SET_FB, &|a| a.p[0] = 0);
    step(l::SET_TEXTURE, &|a| a.p[0] = 0);
    step(l::BG_SCREEN, &|a| a.i[1] = 1);
    step(l::COPY_SCREEN, &|a| a.p[0] = screen_tex);
    step(l::SET_TEXTURE, &|a| a.p[0] = 0);
    step(l::SET_FB, &|a| a.p[0] = 0);
    step(l::DEPTH_TEST, &|a| a.i[1] = 1);
    step(l::FLUSH, &|_| {});
    step(l::FB_DESTROY, &|a| a.p[0] = fb);
}

pub fn destroy_background() {
    let Some(i) = g() else { return };
    let (sg, st, bg, bt) = (
        std::mem::take(&mut i.gui.screen_gfx),
        std::mem::take(&mut i.gui.screen_tex),
        std::mem::take(&mut i.gui.bg_gfx),
        std::mem::take(&mut i.gui.bg_tex),
    );
    if sg != 0 {
        host::gfx_destroy(sg);
    }
    if st != 0 {
        let mut a = A::new();
        a.p[0] = st;
        host::ll(host::ll::TEXTURE_DESTROY, &mut a);
    }
    if bg != 0 {
        host::gfx_destroy(bg);
    }
    if bt != 0 {
        let mut a = A::new();
        a.p[0] = bt;
        host::ll(host::ll::TEXTURE_DESTROY, &mut a);
    }
}

pub fn load_fonts() {
    let mut a = A::new();
    a.s[0] = super::event::Str::of(b"DefaultFont");
    host::call(host::op::FONT_LOAD, &mut a);
    let d = a.r;
    let mut a = A::new();
    a.s[0] = super::event::Str::of(b"HeaderFont");
    host::call(host::op::FONT_LOAD, &mut a);
    let h = a.r;
    if let Some(i) = g() {
        i.gui.font_default = d;
        i.gui.font_header = h;
    }
}

// ---- OnDraw ---------------------------------------------------------------------------------

fn draw_desc_row(text: &[u32], col: [f32; 4]) {
    let Some(i) = g() else { return };
    let (center, width, font, a, fd) = (
        i.layout.item_desc_center,
        i.layout.desc_width,
        i.layout.font_desc,
        i.st.alpha,
        i.gui.font_default,
    );
    let mut w = A::new();
    w.p[0] = fd;
    w.f[0] = width;
    w.f[1] = inv::wrap_line_height(font);
    w.f[2] = font[1];
    w.w[0] = host::WStr::of(text);
    host::call(host::op::FONT_WRAP, &mut w);
    let n = w.i[0] as usize;
    let rows: Vec<Vec<u32>> = (0..n)
        .map(|r| {
            let mut a = A::new();
            a.i[0] = r as i32;
            host::call(host::op::FONT_ROW, &mut a);
            a.ret_wide()
        })
        .collect();
    let mut o = Out::default();
    inv::draw_desc_rows(center, width, font, n, col, a, &mut o);
    for e in &o.evs {
        let row = rows.get(e.a as usize).map_or(&[][..], |r| &r[..]);
        host::draw_font(
            fd,
            row,
            ev_pos(e),
            [font[1], font[1]],
            ev_rgba(e),
            ALIGN_LEFT,
        );
    }
}

fn clip_child(parent: usize, pos: [f32; 3], size: [f32; 2]) -> usize {
    let mut a = A::new();
    a.i[0] = set::CLIP_CHILD;
    a.p[0] = parent;
    a.f[..3].copy_from_slice(&pos);
    a.f[3..5].copy_from_slice(&size);
    host::call(host::op::SET_OP, &mut a);
    a.r
}

fn clip_set(c: usize) {
    host::set_widget(set::CLIP_SET, c);
}

pub fn on_draw(frame_time: f32) {
    let Some(i) = g() else { return };
    let (screen_gfx, bg_gfx, start, size, a) = (
        i.gui.screen_gfx,
        i.gui.bg_gfx,
        i.gui.start,
        i.gui.size,
        i.st.alpha,
    );
    if screen_gfx != 0 && a < 1.0 {
        host::draw_gfx(screen_gfx, start, size, white_color(1.0), MAT_DEFAULT);
    }
    if bg_gfx != 0 {
        host::draw_gfx(
            bg_gfx,
            [start[0], start[1], start[2] + 0.2],
            size,
            white_color(a),
            MAT_DEFAULT,
        );
    }
    let Some(i) = g() else { return };
    if i.st.exit_to_journal != 0 || i.st.enter_from_journal != 0 {
        let (white, fa) = (i.gui.white, super::journal::fade_alpha(i.st.alpha));
        host::draw_gfx(
            white,
            [start[0], start[1], start[2] + 20.0],
            size,
            [0.0, 0.0, 0.0, fa],
            MAT_DEFAULT,
        );
    }

    draw_picked();

    let Some(i) = g() else { return };
    if i.st.message_alpha > 0.0 && !i.message.is_empty() {
        let fa = inv::message_alpha(&i.st);
        let (text, fd, center, fs, fh, white) = (
            i.message.clone(),
            i.gui.font_default,
            i.layout.popup_center,
            i.layout.font_message,
            i.layout.font_message[1],
            i.gui.white,
        );
        host::draw_font(fd, &text, center, fs, white_color(fa), ALIGN_CENTER);
        let (bp, bs) = inv::message_bar(start[0], size[0], center, fh);
        let v = 1.0 - fa * 0.8;
        host::draw_gfx(white, bp, bs, [v, v, v, 1.0], MAT_MOD);
    }

    let Some(i) = g() else { return };
    let (hc, header, normal, fh_size, fhd, a) = (
        i.layout.item_header_center,
        i.header.clone(),
        i.normal.clone(),
        i.layout.font_header,
        i.gui.font_header,
        i.st.alpha,
    );
    if !header.is_empty() {
        host::draw_font(fhd, &header, hc, fh_size, white_color(a), ALIGN_CENTER);
    }
    if !normal.is_empty() {
        draw_desc_row(&normal, [1.0, 1.0, 1.0, 1.0]);
    }

    let Some(i) = g() else { return };
    let (gfr, tfs, hpad, vpad, off) = (
        i.gui.generic_frame,
        i.layout.text_frame_size,
        i.layout.text_frame_hpad,
        i.layout.text_frame_vpad,
        i.layout.text_frame_offset,
    );
    let c0 = host::gfx_sizes(gfr.corners[0]).0;
    let fpos = inv::text_frame_pos(hc, tfs[0], c0, hpad[0], vpad[1], off);
    draw_frame(gfr, fpos, tfs);

    let Some(i) = g() else { return };
    if i.st.old_desc_alpha > 0.0 {
        let (oh, on, oa, a) = (
            i.old_header.clone(),
            i.old_normal.clone(),
            i.st.old_desc_alpha,
            i.st.alpha,
        );
        if !oh.is_empty() {
            host::draw_font(fhd, &oh, hc, fh_size, white_color(a * oa), ALIGN_CENTER);
        }
        if !on.is_empty() {
            draw_desc_row(&on, [1.0, 1.0, 1.0, oa]);
        }
    }

    host::game_f(game::HINT_DRAW, frame_time);
    host::game_f(game::FLASH_DRAW, frame_time);
}

fn draw_picked() {
    let Some(i) = g() else { return };
    let Some(p) = i.picked else { return };
    let (img, off, cur, a) = (
        i.items.get(p).map_or(0, |it| it.image),
        i.picked_offset,
        i.current_widget,
        i.st.alpha,
    );
    let left = host::game_i(game::TRIGGERED, 0).r != 0;
    let m = host::set_query(set::MOUSE_POS).f;
    let (mut wpos, mut wsize) = ([0.0; 3], [0.0; 2]);
    if !left && cur != 0 {
        let gp = host::widget_get(cur, wd::GET_GLOBAL).f;
        let sz = host::widget_get(cur, wd::GET_SIZE).f;
        wpos = [gp[0], gp[1], gp[2]];
        wsize = [sz[0], sz[1]];
    }
    let pos = inv::picked_pos(off, left, [m[0], m[1]], wpos, wsize);
    host::draw_gfx(img, pos, [-1.0; 2], white_color(a * 0.8), MAT_DEFAULT);
}

// ---- callbacks ------------------------------------------------------------------------------

fn item_at(idx: usize) -> Option<ItemRef> {
    g()?.order.get(idx).copied()
}

fn picked() -> Option<ItemRef> {
    g()?.picked
}

fn slot_pick(w: usize, item: ItemRef, mouse: bool) {
    let image = g()
        .and_then(|i| i.items.get(item).map(|it| it.image))
        .map_or([0.0; 2], |gfx| host::gfx_sizes(gfx).1);
    let size = host::widget_get(w, wd::GET_SIZE).f;
    let gp = host::widget_get(w, wd::GET_GLOBAL).f;
    let off = if mouse {
        let m = host::set_query(set::MOUSE_POS).f;
        inv::pick_offset_mouse(
            [gp[0], gp[1], gp[2]],
            [size[0], size[1]],
            image,
            [m[0], m[1]],
        )
    } else {
        inv::pick_offset_button([gp[0], gp[1], gp[2]], [size[0], size[1]], image)
    };
    obj::set_picked(Some(item), off);
}

fn slot_mouse_down(w: usize, idx: usize) -> bool {
    if let Some(it) = item_at(idx) {
        slot_pick(w, it, true);
    }
    true
}

fn use_item(item: ItemRef) {
    let Some((ty, name, sval, extra, amount)) = g().and_then(|i| {
        i.items.get(item).map(|it| {
            (
                it.ty,
                it.name.clone(),
                it.val.clone(),
                it.extra.clone(),
                it.amount,
            )
        })
    }) else {
        return;
    };
    if matches!(ty, itemtype::COINS | itemtype::NOTE | itemtype::DIARY) {
        return;
    }
    let p = obj::player_info(ty);
    let it = itemtype::Item {
        name: &name,
        sval: &sval,
        extra: &extra,
        amount,
    };
    let mut o = Out::default();
    itemtype::on_use(ty, &it, &p, &mut o);
    obj::run_events(&o, Some(item), 0, 0);
}

fn special_combine(a: ItemRef, b: ItemRef) -> bool {
    let (ta, tb) = match g() {
        Some(i) => (
            i.items.get(a).map_or(-1, |it| it.ty),
            i.items.get(b).map_or(-1, |it| it.ty),
        ),
        None => return false,
    };
    let oil = inv::special_combine(ta, tb);
    if oil != 0 {
        use_item(if oil == 1 { a } else { b });
        return true;
    }
    false
}

fn slot_mouse_up(idx: usize) -> bool {
    let picked_item = picked();
    let item = item_at(idx);
    if inv::slot_mouse_up(item.is_some(), picked_item.is_some(), picked_item == item)
        == inv::UP_COMBINE
        && let (Some(it), Some(pk)) = (item, picked_item)
    {
        if !special_combine(it, pk) {
            combine_items(it, pk);
        }
        let cur = item_at(idx);
        obj::set_desc_text_from_item(cur);
    }
    true
}

fn combine_items(it: ItemRef, pk: ItemRef) {
    let names =
        g().and_then(|i| Some((i.items.get(it)?.name.clone(), i.items.get(pk)?.name.clone())));
    let Some((na, nb)) = names else { return };
    match obj::find_combine(&na, &nb) {
        Some(ci) => {
            let Some(c) = g().and_then(|i| i.combine.get(ci).cloned()) else {
                return;
            };
            let cmd = inv::run_script_command(&c.func, &c.a, &c.b);
            obj::run_script(&cmd);
            if c.auto_destroy
                && let Some(i) = g()
                && let Some(pos) = i
                    .combine
                    .iter()
                    .position(|x| x.name == c.name && x.a == c.a && x.b == c.b && x.func == c.func)
            {
                i.combine.remove(pos);
            }
        }
        None => {
            let t = host::translate(b"Inventory", b"CombinationDoesNotWork");
            obj::set_message_text(&t, 0.0);
        }
    }
}

fn slot_double_click(idx: usize) -> bool {
    if let Some(it) = item_at(idx) {
        use_item(it);
        match item_at(idx) {
            Some(cur) => obj::set_desc_text_from_item(Some(cur)),
            None => obj::set_desc_text(&[], &[]),
        }
    }
    true
}

fn slot_enter(w: usize, idx: usize) -> bool {
    let pk = picked();
    let item = item_at(idx);
    obj::set_current_widget(w);
    let msg_active = g().is_some_and(|i| i.st.message_active != 0);
    match inv::slot_mouse_enter(item.is_some(), msg_active, pk.is_some(), pk == item) {
        inv::ENTER_COMBINE => {
            let names = g().and_then(|i| {
                Some((
                    i.items.get(pk?)?.name_entry.clone(),
                    i.items.get(item?)?.name_entry.clone(),
                ))
            });
            if let Some((a, b)) = names {
                let sp = u32::from(b' ');
                let mut s = host::translate(b"Inventory", b"Combine");
                s.push(sp);
                s.extend(host::translate(b"Inventory", &a));
                s.push(sp);
                s.extend(host::translate(b"Inventory", b"with"));
                s.push(sp);
                s.extend(host::translate(b"Inventory", &b));
                s.push(u32::from(b'.'));
                obj::set_desc_text(&[], &s);
            }
        }
        inv::ENTER_ITEM => obj::set_desc_text_from_item(item),
        inv::ENTER_CLEAR => obj::set_desc_text(&[], &[]),
        _ => {}
    }
    true
}

fn slot_leave(w: usize, idx: usize) -> bool {
    if g().is_some_and(|i| i.current_widget == w) {
        obj::set_current_widget(0);
    }
    let item = item_at(idx);
    let none = g().is_some_and(|i| i.current_widget == 0);
    if inv::slot_mouse_leave_clears(item.is_some(), none) {
        obj::set_desc_text(&[], &[]);
    }
    true
}

fn slot_ui_press(w: usize, idx: usize, val: i32) -> bool {
    if (val & inv::BUTTON_PRIMARY) == 0 {
        return false;
    }
    if let Some(it) = item_at(idx) {
        slot_pick(w, it, false);
    }
    true
}

fn slot_ui_release(idx: usize, val: i32) -> bool {
    let pk = picked();
    let item = item_at(idx);
    let same = pk.is_some() && item.is_some() && item == pk;
    let t = obj::now();
    let r = g().map_or(inv::RELEASE_IGNORE, |i| {
        inv::slot_ui_release(&i.st, val, same, t)
    });
    match r {
        inv::RELEASE_IGNORE => true,
        inv::RELEASE_DOUBLE_CLICK => slot_double_click(idx),
        _ => slot_mouse_up(idx),
    }
}

fn slot_update(idx: usize, dt: f32) -> bool {
    let Some(i) = g() else { return true };
    let hovered = i
        .gui
        .slots
        .get(idx)
        .is_some_and(|s| s.widget == i.current_widget)
        && i.st.message_active == 0;
    if let Some(s) = i.gui.slots.get_mut(idx) {
        s.glow_alpha = inv::slot_glow_update(s.glow_alpha, hovered, dt);
    }
    true
}

fn slot_draw(w: usize, idx: usize) -> bool {
    let Some(i) = g() else { return true };
    let size = host::widget_get(w, wd::GET_SIZE).f;
    let Some(item) = i.order.get(idx).copied() else {
        return true;
    };
    if i.picked == Some(item) {
        return true;
    }
    let Some((img, ty, count)) = i.items.get(item).map(|it| (it.image, it.ty, it.count)) else {
        return true;
    };
    let gp = host::widget_get(w, wd::GET_GLOBAL).f;
    let image = host::gfx_sizes(img).1;
    let Some(i) = g() else { return true };
    let pulse = [i.layout.pulse_min, i.layout.pulse_amp, i.layout.pulse_freq];
    let glow = i.gui.slots.get(idx).map_or(0.0, |s| s.glow_alpha);
    let d = inv::slot_draw(
        &i.st,
        [gp[0], gp[1], gp[2]],
        [size[0], size[1]],
        image,
        glow,
        pulse,
        i.layout.count_offset,
    );
    let (a, fd, fc) = (i.st.alpha, i.gui.font_default, i.layout.font_count);
    host::draw_gfx(img, d.pos, [-1.0; 2], white_color(a), MAT_DEFAULT);
    if d.glow {
        for _ in 0..2 {
            host::draw_gfx(
                img,
                d.glow_pos,
                [-1.0; 2],
                [d.glow_alpha, d.glow_alpha, d.glow_alpha, 1.0],
                MAT_ADD,
            );
        }
    }
    if obj::has_count(ty) {
        let text: Vec<u32> = format!("x{count}").chars().map(|c| c as u32).collect();
        host::draw_font(fd, &text, d.count_pos, fc, white_color(a), ALIGN_CENTER);
    }
    true
}

fn status_update(w: usize, status: i32, gfx: [usize; 4], center: [f32; 3]) {
    let old = host::widget_get(w, wd::GET_IMAGE).r;
    let new = gfx.get(status as usize).copied().unwrap_or(0);
    if new != old {
        host::widget_set(w, wd::SET_IMAGE, &[], 0, new);
        let image = host::gfx_sizes(new).1;
        let p = inv::status_widget_pos(center, image);
        host::widget_set(w, wd::SET_POSITION, &p, 0, 0);
        host::widget_set(w, wd::SET_SIZE, &image, 0, 0);
    }
}

fn status_draw(pos: [f32; 3], status: i32, glow: [usize; 4]) {
    let Some(i) = g() else { return };
    let ga = inv::status_glow_alpha(&i.st, status as usize);
    let gfx = glow.get(status as usize).copied().unwrap_or(0);
    let size = host::gfx_sizes(gfx).0;
    draw_element_at_center([pos[0], pos[1], pos[2] + 0.1], gfx, ga, size, false);
}

fn mouse_over(w: usize, header: &[u32], msg: &[u32]) {
    let fader = host::widget_get(w, wd::GET_USER_VALUE).r as i32;
    let Some(i) = g() else { return };
    let active = usize::try_from(fader)
        .ok()
        .and_then(|f| i.faders.get(f))
        .is_some_and(|f| f.active);
    let r = inv::mouse_over(
        fader,
        i.st.message_active != 0,
        w == i.current_widget,
        active,
        i.header == header && i.normal == msg,
    );
    let idx = usize::try_from(fader).unwrap_or(0);
    match r {
        inv::MO_INVALID => host::log(1, format!("Invalid InventoryFader {fader}\n").as_bytes()),
        inv::MO_SHOW => {
            if let Some(f) = i.faders.get_mut(idx) {
                f.active = true;
            }
            obj::set_desc_text(header, msg);
        }
        inv::MO_HIDE => {
            if let Some(f) = i.faders.get_mut(idx) {
                f.active = false;
            }
        }
        inv::MO_HIDE_AND_CLEAR => {
            if let Some(f) = i.faders.get_mut(idx) {
                f.active = false;
            }
            obj::set_desc_text(&[], &[]);
        }
        _ => {}
    }
}

fn tr(key: &[u8]) -> Vec<u32> {
    host::translate(b"Inventory", key)
}

fn status_index_of(kind: i32) -> i32 {
    let v = if kind == 0 {
        host::game(game::SANITY).f[0]
    } else {
        host::game(game::HEALTH).f[0]
    };
    inv::status_index(v)
}

fn sanity_draw(w: usize) -> bool {
    let st = status_index_of(0);
    let Some(i) = g() else { return true };
    let (pos, glow, frame, b) = (
        i.layout.sanity.center,
        i.gui.sanity_glow,
        i.gui.sanity_frame,
        i.layout.sanity,
    );
    status_draw(pos, st, glow);
    draw_frame_around_widget(frame, w, &b);
    true
}

fn sanity_update(w: usize) -> bool {
    let st = status_index_of(0);
    let Some(i) = g() else { return true };
    let (gfx, center) = (i.gui.sanity_status, i.layout.sanity.center);
    status_update(w, st, gfx, center);
    let desc = format!("SanityDesc{st}");
    mouse_over(w, &tr(b"Sanity"), &tr(desc.as_bytes()));
    true
}

fn health_draw(w: usize) -> bool {
    let st = status_index_of(1);
    let Some(i) = g() else { return true };
    let (pos, glow, frame, b) = (
        i.layout.health.center,
        i.gui.health_glow,
        i.gui.health_frame,
        i.layout.health,
    );
    status_draw(pos, st, glow);
    draw_frame_around_widget(frame, w, &b);
    true
}

fn health_update(w: usize) -> bool {
    let st = status_index_of(1);
    let Some(i) = g() else { return true };
    let (gfx, center) = (i.gui.health_status, i.layout.health.center);
    status_update(w, st, gfx, center);
    let desc = format!("HealthDesc{st}");
    mouse_over(w, &tr(b"Health"), &tr(desc.as_bytes()));
    true
}

fn oil_draw(w: usize) -> bool {
    let Some(i) = g() else { return true };
    let (center, bubbles, fg, generic, b) = (
        i.layout.oil.center,
        i.gui.oil_bubble,
        i.gui.oil_fg,
        i.gui.generic_frame,
        i.layout.oil,
    );
    let size = host::widget_get(w, wd::GET_SIZE).f;
    let gp = host::widget_get(w, wd::GET_GLOBAL).f;
    let mut sizes = [[0.0f32; 2]; 3];
    for (s, gfx) in sizes.iter_mut().zip(bubbles) {
        *s = host::gfx_sizes(gfx).0;
    }
    let lamp = host::game(game::LAMP_OIL).f[0];
    let mut o = Out::default();
    {
        let Some(i) = g() else { return true };
        inv::draw_oil(
            &i.st,
            &inv::OilDraw {
                center,
                widget_pos: [gp[0], gp[1], gp[2]],
                widget_size: [size[0], size[1]],
                lamp_oil: lamp,
                bubble_sizes: &sizes,
            },
            &mut o,
        );
    }
    let Some(i) = g() else { return true };
    let liquid = i.gui.oil_liquid;
    let mut old = 0;
    for e in &o.evs {
        match e.kind {
            inv::DRAW_CLIP_PUSH => {
                old = host::set_query(set::CLIP_CURRENT).r;
                let child = clip_child(old, ev_pos(e), ev_size(e));
                clip_set(child);
            }
            inv::DRAW_OIL_LIQUID => {
                host::draw_gfx(
                    liquid,
                    ev_pos(e),
                    ev_size(e),
                    [e.v[5], e.v[5], e.v[5], e.v[8]],
                    MAT_DEFAULT,
                );
            }
            inv::DRAW_OIL_BUBBLE => {
                let gfx = bubbles.get(e.a as usize).copied().unwrap_or(0);
                host::draw_gfx(
                    gfx,
                    ev_pos(e),
                    ev_size(e),
                    [e.v[5], e.v[5], e.v[5], e.v[8]],
                    MAT_DEFAULT,
                );
            }
            inv::DRAW_CLIP_POP => clip_set(old),
            _ => {}
        }
    }
    let a = alpha();
    draw_element_at_center(
        [center[0], center[1], center[2] + 1.0],
        fg,
        a,
        [-1.0; 2],
        false,
    );
    draw_frame_around_widget(generic, w, &b);
    true
}

fn oil_update(w: usize, dt: f32) -> bool {
    mouse_over(w, &tr(b"LampOil"), &tr(b"LampOilDesc"));
    let size = host::widget_get(w, wd::GET_SIZE).f;
    let gp = host::widget_get(w, wd::GET_GLOBAL).f;
    let lamp = host::game(game::LAMP_OIL).f[0];
    if let Some(i) = g() {
        inv::oil_update(
            &mut i.st,
            [gp[0], gp[1], gp[2]],
            [size[0], size[1]],
            lamp,
            dt,
        );
    }
    true
}

fn oil_mouse_up() -> bool {
    let item = picked();
    if let Some(p) = item
        && g().and_then(|i| i.items.get(p).map(|it| it.ty)) == Some(itemtype::LAMP_OIL)
    {
        use_item(p);
    }
    true
}

fn tinder_draw(w: usize) -> bool {
    let Some(i) = g() else { return true };
    let (center, fl, fd, a, gfr, b) = (
        i.layout.tinder.center,
        i.layout.font_label,
        i.gui.font_default,
        i.st.alpha,
        i.gui.generic_frame,
        i.layout.tinder,
    );
    let count = host::game(game::TINDERBOXES).r as i32;
    let text: Vec<u32> = format!("x {count}").chars().map(|c| c as u32).collect();
    host::draw_font(
        fd,
        &text,
        [center[0], center[1] + -fl[1] * 0.5, center[2] + 1.0],
        fl,
        white_color(a),
        ALIGN_CENTER,
    );
    draw_frame_around_widget(gfr, w, &b);
    true
}

fn journal_draw(w: usize) -> bool {
    let Some(i) = g() else { return true };
    let (fa, over, gfr, b) = (
        i.faders
            .get(inv::FADER_JOURNAL as usize)
            .map_or(0.0, |f| f.alpha)
            * i.st.alpha,
        i.gui.journal_over,
        i.gui.generic_frame,
        i.layout.journal,
    );
    let active = host::gfx_sizes(over).0;
    let size = host::widget_get(w, wd::GET_SIZE).f;
    let gp = host::widget_get(w, wd::GET_GLOBAL).f;
    let add = [(active[0] - size[0]) * 0.5, (active[1] - size[1]) * 0.5];
    host::draw_gfx(
        over,
        [gp[0] + add[0], gp[1] + add[1], gp[2] + 0.5],
        [-1.0; 2],
        white_color(fa),
        MAT_DEFAULT,
    );
    draw_frame_around_widget(gfr, w, &b);
    true
}

fn slots_draw(w: usize) -> bool {
    let Some(i) = g() else { return true };
    let (draw_frame_on, draw_grid_on, gfr, fb, white) = (
        i.layout.draw_frame,
        i.layout.draw_grid,
        i.gui.generic_frame,
        i.layout.slots_frame,
        i.gui.white,
    );
    if draw_frame_on {
        draw_frame_around_widget(gfr, w, &fb);
    }
    if draw_grid_on {
        let size = host::widget_get(w, wd::GET_SIZE).f;
        let Some(i) = g() else { return true };
        let l = &i.layout;
        let grid = inv::GridIn {
            widget_size: [size[0], size[1]],
            h_ext: l.grid_hext,
            v_ext: l.grid_vext,
            separation: l.slots_sep,
            slot_size: l.slot_size,
            start: l.slots_start,
            frame_hpad: l.slots_frame.hpad,
            frame_vpad: l.slots_frame.vpad,
            frame_offset: l.slots_frame.offset,
            grid_offset: l.grid_offset,
            color: l.grid_color,
            rows: l.rows,
            cols: l.cols,
            alpha: i.st.alpha,
        };
        let mut o = Out::default();
        inv::draw_grid(&grid, &mut o);
        for e in &o.evs {
            host::draw_gfx(white, ev_pos(e), ev_size(e), ev_rgba(e), MAT_DEFAULT);
        }
    }
    true
}

fn ui_button_press(val: i32) -> bool {
    let focused = host::set_query(set::GET_FOCUSED).r;
    let uv = if focused == 0 {
        0
    } else {
        host::widget_get(focused, wd::GET_USER_VALUE).r as i32
    };
    let Some(i) = g() else { return false };
    let gi = &i.gui;
    let (t, o, j, s, h) = (
        gi.tinder_widget,
        gi.oil_widget,
        gi.journal_widget,
        gi.sanity_widget,
        gi.health_widget,
    );
    let target = match inv::ui_button_nav(val, uv) {
        inv::NAV_TINDERBOX => t,
        inv::NAV_OIL => o,
        inv::NAV_JOURNAL => j,
        inv::NAV_SANITY => s,
        inv::NAV_HEALTH => h,
        _ => return false,
    };
    obj::set_current_widget(target);
    true
}

fn focus_draw() -> bool {
    let Some(i) = g() else { return true };
    let (cur, a) = (i.current_widget, i.st.alpha);
    if cur != 0 {
        let gp = host::widget_get(cur, wd::GET_GLOBAL).f;
        let size = host::widget_get(cur, wd::GET_SIZE).f;
        let z = host::set_query(set::MOUSE_Z).f[0] - 1.0;
        let pointer = host::set_query(set::POINTER_GFX).r;
        host::draw_gfx(
            pointer,
            [gp[0] + size[0] * 0.5, gp[1] + size[1] * 0.5, z],
            [-1.0; 2],
            white_color(a),
            MAT_DEFAULT,
        );
    }
    true
}

/// Entry of every widget callback: `token` as registered by [`add_cb`].
pub fn dispatch(token: usize, w: usize, lval: i32, fval: f32) -> bool {
    let (kind, slot) = (token >> 16, token & 0xFFFF);
    match kind {
        cb::SLOT_DOWN => slot_mouse_down(w, slot),
        cb::SLOT_UP => slot_mouse_up(slot),
        cb::SLOT_DBL => slot_double_click(slot),
        cb::SLOT_ENTER => slot_enter(w, slot),
        cb::SLOT_LEAVE => slot_leave(w, slot),
        cb::SLOT_GET_NAV => slot_enter(w, slot),
        cb::SLOT_LOSE_NAV => slot_leave(w, slot),
        cb::SLOT_UI_PRESS => slot_ui_press(w, slot, lval),
        cb::SLOT_UI_DBL => (lval & inv::BUTTON_PRIMARY) != 0 && slot_double_click(slot),
        cb::SLOT_UI_RELEASE => slot_ui_release(slot, lval),
        cb::SLOT_UPDATE => slot_update(slot, fval),
        cb::SLOT_DRAW => slot_draw(w, slot),
        cb::SANITY_DRAW => sanity_draw(w),
        cb::SANITY_UPDATE => sanity_update(w),
        cb::HEALTH_DRAW => health_draw(w),
        cb::HEALTH_UPDATE => health_update(w),
        cb::OIL_DRAW => oil_draw(w),
        cb::OIL_UPDATE => oil_update(w, fval),
        cb::OIL_UP => oil_mouse_up(),
        cb::TINDER_DRAW => tinder_draw(w),
        cb::TINDER_UPDATE => {
            mouse_over(w, &tr(b"Tinderboxes"), &tr(b"TinderboxesDesc"));
            true
        }
        cb::JOURNAL_DRAW => journal_draw(w),
        cb::JOURNAL_UPDATE => {
            mouse_over(w, &tr(b"Journal"), &tr(b"JournalDesc"));
            true
        }
        cb::JOURNAL_DOWN => {
            obj::open_journal();
            true
        }
        cb::JOURNAL_UI_PRESS => {
            if lval == inv::BUTTON_PRIMARY {
                obj::open_journal();
                true
            } else {
                false
            }
        }
        cb::SLOTS_DRAW => slots_draw(w),
        cb::UI_BUTTON_PRESS => ui_button_press(lval),
        cb::WIDGET_ENTER | cb::WIDGET_GET_NAV => {
            obj::set_current_widget(w);
            true
        }
        cb::WIDGET_LOSE_NAV => {
            if g().is_some_and(|i| i.current_widget == w) {
                obj::set_current_widget(0);
            }
            true
        }
        cb::FOCUS_DRAW => focus_draw(),
        _ => false,
    }
}
