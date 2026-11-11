//! `cLuxInventory` and its slots: fade and timer state, add/remove/combine/use decisions, hover
//! and description-text rules, focus navigation, and the layout math of the frames, grid, slots
//! and oil gauge. Item and callback containers, widgets, gfx, fonts and script calls stay C++.

use super::event::{Ev, Out};
use super::itemtype;
use crate::math::{EPSILON, cos, rand_rectf, sin};
use crate::string;

pub const FADER_JOURNAL: i32 = 0;
pub const FADER_SANITY: i32 = 1;
pub const FADER_HEALTH: i32 = 2;
pub const FADER_TINDERBOX: i32 = 3;
pub const FADER_OIL: i32 = 4;
pub const FADER_COUNT: i32 = 5;

pub const EXIT_NONE: i32 = 0;
pub const EXIT_TO_JOURNAL: i32 = 1;
pub const EXIT_TO_DEFAULT: i32 = 2;

pub const PRESS_NONE: i32 = 0;
pub const PRESS_CLEAR_MESSAGE: i32 = 1;
pub const PRESS_EXIT: i32 = 2;

pub const ADD_REJECT: i32 = 0;
pub const ADD_COUNT: i32 = 1;
pub const ADD_CREATE: i32 = 2;

pub const BUTTON_PRIMARY: i32 = 1;
pub const BUTTON_SECONDARY: i32 = 2;
pub const BUTTON_PREV_PAGE: i32 = 4;
pub const BUTTON_NEXT_PAGE: i32 = 8;

pub const NAV_NONE: i32 = 0;
pub const NAV_TINDERBOX: i32 = 1;
pub const NAV_OIL: i32 = 2;
pub const NAV_JOURNAL: i32 = 3;
pub const NAV_SANITY: i32 = 4;
pub const NAV_HEALTH: i32 = 5;

pub const DRAW_CORNER: u32 = 1;
pub const DRAW_BORDER: u32 = 2;
pub const DRAW_GRID_LINE: u32 = 3;
pub const DRAW_DESC_ROW: u32 = 4;
pub const DRAW_CLIP_PUSH: u32 = 5;
pub const DRAW_CLIP_POP: u32 = 6;
pub const DRAW_OIL_LIQUID: u32 = 7;
pub const DRAW_OIL_BUBBLE: u32 = 8;

const NO_SIZE: f32 = -1.0;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct InvState {
    pub active: u8,
    pub exit_to_journal: u8,
    pub enter_from_journal: u8,
    pub message_active: u8,
    pub picked_moved: u8,
    pub fade_in_time: f32,
    pub fade_out_time: f32,
    pub alpha: f32,
    pub old_desc_alpha: f32,
    pub message_life: f32,
    pub message_alpha: f32,
    pub timer: f32,
    pub oil_t: f32,
    pub picked_at: f64,
    pub bubble_pos: [[f32; 3]; 3],
    pub bubble_speed: [f32; 3],
    pub pulse_middle: [f32; 4],
    pub pulse_freq: [f32; 4],
}

pub fn init(s: &mut InvState, fade_in: f32, fade_out: f32, bubble_pos: [[f32; 3]; 3]) {
    s.fade_in_time = fade_in;
    s.fade_out_time = fade_out;
    s.picked_at = -1.0;
    s.picked_moved = 0;
    s.timer = 0.0;
    s.bubble_speed = [40.0; 3];
    s.bubble_pos = bubble_pos;
    s.pulse_middle = [0.5, 0.55, 0.65, 0.7];
    s.pulse_freq = [1.5, 1.4, 1.2, 1.0];
}

pub fn reset_session(s: &mut InvState) {
    s.old_desc_alpha = 0.0;
    s.message_active = 0;
    s.oil_t = 0.0;
    s.message_life = 0.0;
    s.message_alpha = 0.0;
}

pub fn reset(s: &mut InvState) {
    s.alpha = 0.0;
    s.active = 0;
    s.exit_to_journal = 0;
    s.enter_from_journal = 0;
    s.message_active = 0;
    reset_session(s);
}

pub fn should_drop_picked(left_click: bool, ui_primary: bool) -> bool {
    !left_click && !ui_primary
}

pub fn update_alpha(s: &mut InvState, dt: f32) -> i32 {
    let mut exit = EXIT_NONE;
    if s.active != 0 {
        if s.alpha < 1.0 {
            s.alpha = (1.0 / s.fade_in_time).mul_add(dt, s.alpha);
            if s.alpha > 1.0 {
                s.alpha = 1.0;
                s.enter_from_journal = 0;
            }
        }
    } else {
        s.alpha = (-(1.0 / s.fade_out_time)).mul_add(dt, s.alpha);
        if s.alpha < 0.0 {
            s.alpha = 0.0;
            exit = if s.exit_to_journal != 0 {
                EXIT_TO_JOURNAL
            } else {
                EXIT_TO_DEFAULT
            };
        }
    }
    exit
}

pub fn update_texts(s: &mut InvState, dt: f32) {
    s.timer += dt;
    if s.old_desc_alpha > 0.0 {
        s.old_desc_alpha = (-dt).mul_add(2.2, s.old_desc_alpha);
        if s.old_desc_alpha < 0.0 {
            s.old_desc_alpha = 0.0;
        }
    }
    if s.message_active != 0 {
        s.message_alpha = dt.mul_add(3.5, s.message_alpha);
        if s.message_alpha > 1.0 {
            s.message_alpha = 1.0;
        }
    }
    if s.message_active == 0 && s.message_alpha > 0.0 {
        s.message_alpha = (-dt).mul_add(1.5, s.message_alpha);
        if s.message_alpha < 0.0 {
            s.message_alpha = 0.0;
        }
    }
}

pub fn exit_pressed(s: &mut InvState) -> i32 {
    if s.active == 0 {
        return PRESS_NONE;
    }
    if s.message_active != 0 {
        s.message_active = 0;
        PRESS_CLEAR_MESSAGE
    } else {
        s.active = 0;
        s.enter_from_journal = 0;
        PRESS_EXIT
    }
}

pub fn open_journal(s: &mut InvState) -> i32 {
    if s.active == 0 {
        return PRESS_NONE;
    }
    let r = exit_pressed(s);
    s.exit_to_journal = 1;
    r
}

/// Whether voices are paused on entering (false when coming back from the journal).
pub fn pauses_voices_on_enter(s: &InvState) -> bool {
    s.enter_from_journal == 0
}

/// Leaving: returns whether voices and world volume are restored, then clears the journal flags.
pub fn leave(s: &mut InvState) -> bool {
    let restore = s.exit_to_journal == 0;
    s.exit_to_journal = 0;
    s.enter_from_journal = 0;
    restore
}

pub fn set_desc_text(s: &mut InvState, header_empty: bool, normal_empty: bool) -> bool {
    let save_old = header_empty && normal_empty;
    s.old_desc_alpha = if save_old { 1.0 } else { 0.0 };
    save_old
}

pub fn set_message(s: &mut InvState, life: f32, parsed_duration: f32) {
    s.message_life = if life <= 0.0 { parsed_duration } else { life };
    s.message_alpha = 0.0;
    s.message_active = 1;
}

pub fn set_picked(s: &mut InvState, game_time: f64) {
    s.picked_at = game_time;
    s.picked_moved = 0;
}

pub fn note_current_widget(s: &mut InvState, game_time: f64) {
    if s.picked_at + f64::from(1.0f32 / 60.0) < game_time {
        s.picked_moved = 1;
    }
}

pub fn status_index(x: f32) -> i32 {
    if x < 25.0 {
        0
    } else if x < 50.0 {
        1
    } else if x < 75.0 {
        2
    } else {
        3
    }
}

/// Alpha of the status glow: the pulse of `status` scaled by the inventory alpha.
pub fn status_glow_alpha(s: &InvState, status: usize) -> f32 {
    let mid = s.pulse_middle.get(status).copied().unwrap_or(0.0);
    let freq = s.pulse_freq.get(status).copied().unwrap_or(0.0);
    s.alpha * (1.0 - mid).mul_add(cos(s.timer * freq), mid)
}

pub fn item_type_from_string(name: &[u8]) -> i32 {
    itemtype::from_string(name).unwrap_or(itemtype::PUZZLE)
}

// ---- items, combines ------------------------------------------------------------------------

pub struct AddIn<'a> {
    pub ty: i32,
    pub subtype: &'a [u8],
    pub existing: bool,
    pub existing_count: i32,
}

pub fn add_plan(a: &AddIn) -> i32 {
    if itemtype::has_max_amount(a.ty) {
        return ADD_REJECT;
    }
    let Some(info) = itemtype::type_info(a.ty) else {
        return ADD_CREATE;
    };
    if info.has_count && a.existing {
        return if a.existing_count >= info.max_count {
            ADD_REJECT
        } else {
            ADD_COUNT
        };
    }
    ADD_CREATE
}

/// Tinderbox counter: new value text and whether the achievement unlocks.
pub fn tinderbox_count(current: &[u8]) -> (Vec<u8>, bool) {
    let mut n = 0;
    if !current.is_empty() {
        n = string::token_to_int(current).wrapping_add(1);
    }
    (string::int_to_string(n, -1), n >= 150)
}

pub fn is_still_alive_item(val: &[u8]) -> bool {
    val == b"L02_Shipment"
}

pub fn game_name_entry(subtype: &[u8]) -> Vec<u8> {
    [b"ItemName_".as_slice(), subtype].concat()
}

pub fn game_desc_entry(subtype: &[u8]) -> Vec<u8> {
    [b"ItemDesc_".as_slice(), subtype].concat()
}

pub fn full_image_name(image: &[u8]) -> Vec<u8> {
    [b"graphics/item/".as_slice(), image].concat()
}

/// `RemoveItem(item)`: whether the item leaves the list after one is used.
pub fn remove_one(has_count: bool, count_after_decrement: i32) -> bool {
    !(has_count && count_after_decrement > 0)
}

pub fn combine_matches(cb_a: &[u8], cb_b: &[u8], a: &[u8], b: &[u8]) -> bool {
    (cb_a == a && cb_b == b) || (cb_a == b && cb_b == a)
}

/// Lantern with lamp oil: 1 when `a` is the oil, 2 when `b` is, 0 for any other pair.
pub fn special_combine(ta: i32, tb: i32) -> i32 {
    let pair = (ta == itemtype::LANTERN && tb == itemtype::LAMP_OIL)
        || (ta == itemtype::LAMP_OIL && tb == itemtype::LANTERN);
    if !pair {
        0
    } else if ta == itemtype::LAMP_OIL {
        1
    } else {
        2
    }
}

pub fn run_script_command(func: &[u8], a: &[u8], b: &[u8]) -> Vec<u8> {
    [func, b"(\"", a, b"\", \"", b, b"\")"].concat()
}

// ---- slots ----------------------------------------------------------------------------------

pub const UP_NONE: i32 = 0;
pub const UP_COMBINE: i32 = 1;

pub fn slot_mouse_up(has_item: bool, has_picked: bool, same: bool) -> i32 {
    if has_item && has_picked && !same {
        UP_COMBINE
    } else {
        UP_NONE
    }
}

pub const ENTER_COMBINE: i32 = 1;
pub const ENTER_ITEM: i32 = 2;
pub const ENTER_CLEAR: i32 = 3;

pub fn slot_mouse_enter(has_item: bool, message_active: bool, has_picked: bool, same: bool) -> i32 {
    if has_item && !message_active {
        if has_picked && !same {
            ENTER_COMBINE
        } else {
            ENTER_ITEM
        }
    } else if !has_picked {
        ENTER_CLEAR
    } else {
        0
    }
}

pub fn slot_mouse_leave_clears(has_item: bool, current_none: bool) -> bool {
    has_item && current_none
}

pub const RELEASE_IGNORE: i32 = 0;
pub const RELEASE_DOUBLE_CLICK: i32 = 1;
pub const RELEASE_MOUSE_UP: i32 = 2;

pub fn slot_ui_release(s: &InvState, button: i32, picked_same: bool, game_time: f64) -> i32 {
    if button & BUTTON_PRIMARY == 0 {
        return RELEASE_IGNORE;
    }
    if picked_same && s.picked_at + f64::from(0.5f32) > game_time && s.picked_moved == 0 {
        return RELEASE_DOUBLE_CLICK;
    }
    RELEASE_MOUSE_UP
}

pub fn slot_glow_update(glow: f32, hovered: bool, dt: f32) -> f32 {
    if hovered {
        let g = dt.mul_add(2.0, glow);
        if g > 1.0 { 1.0 } else { g }
    } else {
        let g = (-dt).mul_add(1.5, glow);
        if g < 0.0 { 0.0 } else { g }
    }
}

fn item_pos(gpos: [f32; 3], size: [f32; 2], image: [f32; 2]) -> [f32; 3] {
    [
        gpos[0] + (size[0] - image[0]) * 0.5,
        gpos[1] + (size[1] - image[1]) * 0.5,
        gpos[2],
    ]
}

/// Offset of the picked item from the mouse when a slot is pressed with the mouse.
pub fn pick_offset_mouse(
    gpos: [f32; 3],
    size: [f32; 2],
    image: [f32; 2],
    mouse: [f32; 2],
) -> [f32; 2] {
    let p = item_pos(gpos, size, image);
    [p[0] - mouse[0], p[1] - mouse[1]]
}

/// Same for the UI button press, measured from a fixed point inside the slot.
pub fn pick_offset_button(gpos: [f32; 3], size: [f32; 2], image: [f32; 2]) -> [f32; 2] {
    let p = item_pos(gpos, size, image);
    [p[0] - (gpos[0] + 37.0), p[1] - (gpos[1] + 50.0)]
}

pub struct SlotDraw {
    pub pos: [f32; 3],
    pub glow: bool,
    pub glow_alpha: f32,
    pub glow_pos: [f32; 3],
    pub count_pos: [f32; 3],
}

pub fn slot_draw(
    s: &InvState,
    gpos: [f32; 3],
    size: [f32; 2],
    image: [f32; 2],
    glow: f32,
    pulse: [f32; 3],
    count_offset: [f32; 3],
) -> SlotDraw {
    let mut pos = item_pos(gpos, size, image);
    pos[2] += 0.1;
    let mut r = SlotDraw {
        pos,
        glow: glow > 0.0,
        glow_alpha: 0.0,
        glow_pos: [pos[0], pos[1], pos[2] + 0.1],
        count_pos: [
            gpos[0] + size[0] + count_offset[0],
            gpos[1] + count_offset[1],
            gpos[2] + count_offset[2],
        ],
    };
    if r.glow {
        let (min, _amp, freq) = (pulse[0], pulse[1], pulse[2]);
        let c = f64::from(cos(s.timer * freq));
        let inner = c.mul_add(0.5, f64::from(0.5f32));
        let p = f64::from(1.0 - min).mul_add(inner, f64::from(min)) as f32;
        r.glow_alpha = p * glow * s.alpha;
    }
    r
}

pub fn ui_button_nav(button: i32, user_value: i32) -> i32 {
    if user_value == 0 {
        return NAV_NONE;
    }
    match button {
        BUTTON_NEXT_PAGE => match user_value {
            1 | 2 => NAV_TINDERBOX,
            3 => NAV_OIL,
            4 => NAV_JOURNAL,
            _ => NAV_TINDERBOX,
        },
        BUTTON_PREV_PAGE => match user_value {
            2 => NAV_SANITY,
            _ => NAV_HEALTH,
        },
        _ => NAV_NONE,
    }
}

pub const MO_NONE: i32 = 0;
pub const MO_SHOW: i32 = 1;
pub const MO_HIDE: i32 = 2;
pub const MO_HIDE_AND_CLEAR: i32 = 3;
pub const MO_INVALID: i32 = -1;

/// Mouse-over description rule: the fader state change and whether the description is shown or
/// cleared. `same_text` is whether the current texts equal the widget's.
pub fn mouse_over(
    fader_id: i32,
    message_active: bool,
    is_current: bool,
    fader_active: bool,
    same_text: bool,
) -> i32 {
    if !(0..FADER_COUNT).contains(&fader_id) {
        return MO_INVALID;
    }
    if !message_active && is_current && !fader_active {
        return MO_SHOW;
    }
    if (!is_current || message_active) && fader_active {
        return if same_text {
            MO_HIDE_AND_CLEAR
        } else {
            MO_HIDE
        };
    }
    MO_NONE
}

// ---- layout ---------------------------------------------------------------------------------

fn col(a: f32) -> [f32; 4] {
    [1.0, 1.0, 1.0, a]
}

fn draw(kind: u32, a: i32, pos: [f32; 3], size: [f32; 2], c: [f32; 4]) -> Ev {
    Ev {
        a,
        ..Ev::floats(
            kind,
            &[
                pos[0], pos[1], pos[2], size[0], size[1], c[0], c[1], c[2], c[3],
            ],
        )
    }
}

/// Frame of 4 corners and 4 borders. `c0`, `c1`, `c3` are the active sizes of corners 0, 1, 3.
pub fn draw_frame(
    pos: [f32; 3],
    size: [f32; 2],
    c0: [f32; 2],
    c1: [f32; 2],
    c3: [f32; 2],
    alpha: f32,
    o: &mut Out,
) {
    let c = col(alpha);
    let none = [NO_SIZE, NO_SIZE];
    let mut p = pos;
    o.push(draw(DRAW_CORNER, 0, p, none, c));
    o.push(draw(
        DRAW_BORDER,
        3,
        [p[0], p[1] + c0[1], p[2]],
        [c0[0], size[1]],
        c,
    ));
    o.push(draw(
        DRAW_CORNER,
        3,
        [p[0], p[1] + (c0[1] + size[1]), p[2]],
        none,
        c,
    ));
    p[0] += c0[0];
    o.push(draw(DRAW_BORDER, 0, p, [size[0], c0[1]], c));
    o.push(draw(
        DRAW_BORDER,
        2,
        [p[0], p[1] + (c0[1] + size[1]), p[2]],
        [size[0], c3[1]],
        c,
    ));
    p[0] += size[0];
    o.push(draw(DRAW_CORNER, 1, p, none, c));
    o.push(draw(
        DRAW_BORDER,
        1,
        [p[0], p[1] + c1[1], p[2]],
        [c1[0], size[1]],
        c,
    ));
    o.push(draw(
        DRAW_CORNER,
        2,
        [p[0], p[1] + (c1[1] + size[1]), p[2]],
        none,
        c,
    ));
}

pub struct FrameIn {
    pub widget_pos: [f32; 3],
    pub widget_size: [f32; 2],
    pub hpad: [f32; 2],
    pub vpad: [f32; 2],
    pub offset: [f32; 3],
    pub size: [f32; 2],
    pub c0: [f32; 2],
    pub c1: [f32; 2],
    pub c3: [f32; 2],
    pub alpha: f32,
}

pub fn draw_frame_around(f: &FrameIn, o: &mut Out) {
    let size = [
        if f.size[0] == -1.0 {
            f.widget_size[0]
        } else {
            f.size[0]
        },
        if f.size[1] == -1.0 {
            f.widget_size[1]
        } else {
            f.size[1]
        },
    ];
    let pos = [
        f.widget_pos[0] - (f.c0[0] + f.hpad[0]) + f.offset[0],
        f.widget_pos[1] - (f.c0[1] + f.vpad[0]) + f.offset[1],
        f.widget_pos[2] - 0.0 + f.offset[2],
    ];
    let total = [
        size[0] + (f.hpad[0] + f.hpad[1]),
        size[1] + (f.vpad[0] + f.vpad[1]),
    ];
    draw_frame(pos, total, f.c0, f.c1, f.c3, f.alpha, o);
}

pub struct GridIn {
    pub widget_size: [f32; 2],
    pub h_ext: [f32; 2],
    pub v_ext: [f32; 2],
    pub separation: [f32; 3],
    pub slot_size: [f32; 2],
    pub start: [f32; 3],
    pub frame_hpad: [f32; 2],
    pub frame_vpad: [f32; 2],
    pub frame_offset: [f32; 3],
    pub grid_offset: [f32; 3],
    pub color: [f32; 4],
    pub rows: i32,
    pub cols: i32,
    pub alpha: f32,
}

pub fn draw_grid(g: &GridIn, o: &mut Out) {
    let hori = [g.widget_size[0] + g.h_ext[0] + g.h_ext[1], g.separation[1]];
    let vert = [g.separation[0], g.widget_size[1] + g.v_ext[0] + g.v_ext[1]];
    let sep = [
        g.slot_size[0] + g.separation[0],
        g.slot_size[1] + g.separation[1],
    ];
    let pos = [
        g.start[0] - g.frame_hpad[0] + g.frame_offset[0] + g.grid_offset[0],
        g.start[1] - g.frame_vpad[0] + g.frame_offset[1] + g.grid_offset[1],
        g.start[2] - 0.0 + g.frame_offset[2] + g.grid_offset[2],
    ];
    let mut c = g.color;
    c[3] *= g.alpha;
    for i in 1..g.rows {
        let p = [
            pos[0] + -g.h_ext[0],
            sep[1].mul_add(i as f32, pos[1]),
            pos[2],
        ];
        o.push(draw(DRAW_GRID_LINE, 0, p, hori, c));
    }
    for i in 1..g.cols {
        let p = [
            sep[0].mul_add(i as f32, pos[0]),
            pos[1] + -g.v_ext[0],
            pos[2],
        ];
        o.push(draw(DRAW_GRID_LINE, 1, p, vert, c));
    }
}

/// Wrapped description rows, left aligned from the centre minus half the text width.
pub fn draw_desc_rows(
    center: [f32; 3],
    text_width: f32,
    font: [f32; 2],
    rows: usize,
    color: [f32; 4],
    alpha: f32,
    o: &mut Out,
) {
    let start_y = center[1] + 30.0;
    let half = text_width * 0.5;
    let mut c = color;
    c[3] *= alpha;
    for i in 0..rows {
        let y = (i as f32).mul_add(font[1] + 1.0, start_y);
        o.push(draw(
            DRAW_DESC_ROW,
            i as i32,
            [center[0] - half, y, 1.0],
            [0.0, font[1]],
            c,
        ));
    }
}

pub fn wrap_line_height(font: [f32; 2]) -> f32 {
    font[1] + 1.0
}

pub fn picked_pos(
    offset: [f32; 2],
    mouse_down: bool,
    mouse: [f32; 2],
    widget_pos: [f32; 3],
    widget_size: [f32; 2],
) -> [f32; 3] {
    let mut p = [offset[0], offset[1], 0.0];
    if mouse_down {
        p[0] += mouse[0];
        p[1] += mouse[1];
    } else {
        p[0] += widget_pos[0] + widget_size[0] * 0.5;
        p[1] += widget_pos[1] + widget_size[1] * 0.5;
        p[2] += widget_pos[2];
    }
    p[2] = 40.0;
    p
}

/// Message bar: the modulative quad's position and size; the text itself is drawn by C++.
pub fn message_bar(
    gui_start_x: f32,
    gui_width: f32,
    center: [f32; 3],
    font_h: f32,
) -> ([f32; 3], [f32; 2]) {
    (
        [gui_start_x, center[1] - 20.0, center[2] - 0.1],
        [gui_width, font_h + 20.0 * 2.0],
    )
}

pub fn message_alpha(s: &InvState) -> f32 {
    s.message_alpha * s.alpha
}

pub fn text_frame_pos(
    header_center: [f32; 3],
    frame_size_x: f32,
    c0: [f32; 2],
    hpad_x: f32,
    vpad_y: f32,
    offset: [f32; 3],
) -> [f32; 3] {
    [
        header_center[0] - frame_size_x * 0.5 - c0[0] - hpad_x + offset[0],
        header_center[1] - 0.0 - c0[1] - vpad_y + offset[1],
        header_center[2] - 0.0 - 0.0 - 0.0 + offset[2],
    ]
}

pub fn item_text_frame_default(width: f32) -> [f32; 2] {
    [width, width * 0.25]
}

pub fn element_at_center(
    center: [f32; 3],
    image: [f32; 2],
    size: [f32; 2],
    alpha: f32,
    mul_alpha: bool,
) -> ([f32; 3], [f32; 2], [f32; 4]) {
    let sz = if size[0] == -1.0 && size[1] == -1.0 {
        image
    } else {
        size
    };
    let c = if mul_alpha {
        [alpha, alpha, alpha, alpha]
    } else {
        col(alpha)
    };
    (
        [
            center[0] - sz[0] * 0.5,
            center[1] - sz[1] * 0.5,
            center[2] - 0.0,
        ],
        sz,
        c,
    )
}

// ---- oil ------------------------------------------------------------------------------------

pub fn oil_remaining_height(lamp_oil: f32, widget_h: f32) -> f32 {
    lamp_oil * 0.01 * widget_h
}

pub struct OilDraw<'a> {
    pub center: [f32; 3],
    pub widget_pos: [f32; 3],
    pub widget_size: [f32; 2],
    pub lamp_oil: f32,
    pub bubble_sizes: &'a [[f32; 2]; 3],
}

pub fn draw_oil(s: &InvState, d: &OilDraw, o: &mut Out) {
    let size = d.widget_size;
    let remaining = oil_remaining_height(d.lamp_oil, size[1]);
    let start = [
        d.center[0] - size[0] * 0.5,
        d.center[1] - size[1] * 0.5,
        d.center[2] - 0.0,
    ];
    o.push(draw(
        DRAW_CLIP_PUSH,
        0,
        [start[0], start[1] + (size[1] - remaining), start[2] + 0.6],
        [size[0], remaining],
        col(0.0),
    ));
    let lsize = [size[0] * 2.0, size[1] * 2.0];
    let lpos = [
        d.widget_pos[0] - lsize[0] * 0.25,
        d.widget_pos[1] - lsize[1] * 0.25,
        d.widget_pos[2] - 0.0,
    ];
    let t = s.oil_t;
    let add1 = [
        (f64::from(sin(t * 0.3) * size[0]) * 0.5) as f32,
        (f64::from(cos(t * 0.2) * size[1]) * 0.2) as f32,
        0.6,
    ];
    let add2 = [
        (f64::from(cos(t * 0.2) * size[0]) * 0.3) as f32,
        (f64::from(sin(t * 0.15) * size[1]) * 0.15) as f32,
        0.6,
    ];
    o.push(draw(
        DRAW_OIL_LIQUID,
        0,
        [lpos[0] + add1[0], lpos[1] + add1[1], lpos[2] + add1[2]],
        lsize,
        col(0.75 * s.alpha),
    ));
    o.push(draw(
        DRAW_OIL_LIQUID,
        1,
        [lpos[0] + add2[0], lpos[1] + add2[1], lpos[2] + add2[2]],
        lsize,
        col(0.25 * s.alpha),
    ));
    for (i, (p, sz)) in s.bubble_pos.iter().zip(d.bubble_sizes.iter()).enumerate() {
        o.push(draw(DRAW_OIL_BUBBLE, i as i32, *p, *sz, col(s.alpha)));
    }
    o.push(draw(DRAW_CLIP_POP, 0, [0.0; 3], [0.0; 2], col(0.0)));
}

/// Bubble movement; true when the gauge is empty and nothing moved.
pub fn oil_update(
    s: &mut InvState,
    widget_pos: [f32; 3],
    widget_size: [f32; 2],
    lamp_oil: f32,
    dt: f32,
) -> bool {
    if oil_remaining_height(lamp_oil, widget_size[1]) < EPSILON {
        return true;
    }
    s.oil_t += dt;
    let mut outside = 0;
    for (pos, speed) in s.bubble_pos.iter_mut().zip(&s.bubble_speed) {
        if pos[1] < widget_pos[1] - 30.0 {
            outside += 1;
            continue;
        }
        pos[1] = (-dt).mul_add(*speed, pos[1]);
    }
    if outside == 3 {
        for (pos, speed) in s.bubble_pos.iter_mut().zip(s.bubble_speed.iter_mut()) {
            *speed = widget_size[1] * 0.5 * rand_rectf(0.5, 1.25);
            pos[1] = widget_pos[1] + widget_size[1];
            pos[0] = rand_rectf(widget_pos[0], widget_pos[0] + widget_size[0]);
        }
    }
    false
}

pub fn status_widget_pos(center: [f32; 3], image: [f32; 2]) -> [f32; 3] {
    [
        center[0] - image[0] * 0.5,
        center[1] - image[1] * 0.5,
        center[2] - 0.0,
    ]
}
