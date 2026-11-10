//! Pure logic of the HPL2 button, check box, label, image, dummy and group widgets and of the
//! `iFileBrowser` helpers in `WidgetBaseClasses.cpp`. Same contract as [`super::widget`]: C++ owns
//! the objects and the `cGuiSet` calls, Rust decides.

use super::widget::{
    GFX_IMAGE, GFX_SLOTS, GfxMetrics, MOUSE_LEFT, MOUSE_WHEEL_DOWN, MOUSE_WHEEL_UP, Ops,
    UI_BUTTON_PRIMARY, WHITE, add2, add3, borders_and_corners, color_mul, v2,
};
use crate::math::v3;
use crate::types::{Color, Vector2f, Vector3f};

fn grey(v: f32, a: f32) -> Color {
    Color {
        r: v,
        g: v,
        b: v,
        a,
    }
}

// ---------------------------------------------------------------- button

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct ButtonState {
    pub toggleable: bool,
    pub pressed: bool,
    pub repeat_active: bool,
    pub repeat_times: i32,
    pub timer: f32,
    pub repeat_period: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct ButtonAction {
    pub state: ButtonState,
    pub ret: bool,
    /// Send `ButtonPressed` before applying `state` (mouse up) or after (toggling mouse down).
    pub fire: bool,
    pub store_data: bool,
}

/// `cWidgetButton::SetRepeatFreq`.
pub fn button_repeat_period(freq: f32) -> f32 {
    1.0 / freq
}

/// `cWidgetButton::OnMouseDown`; `fire` is sent with the new state, then `store_data` applies.
pub fn button_mouse_down(s: ButtonState, enabled: bool, val: i32) -> ButtonAction {
    let mut a = ButtonAction {
        state: s,
        ..ButtonAction::default()
    };
    if enabled && val == MOUSE_LEFT {
        if s.toggleable {
            a.state.pressed = !s.pressed;
            a.fire = true;
        } else {
            a.state.pressed = true;
        }
        a.store_data = true;
        a.ret = true;
    }
    a
}

/// `cWidgetButton::OnMouseUp`; `fire` is sent with the old state, then `state` applies.
pub fn button_mouse_up(s: ButtonState, enabled: bool, val: i32, mouse_over: bool) -> ButtonAction {
    let mut a = ButtonAction {
        state: s,
        ..ButtonAction::default()
    };
    if enabled && val == MOUSE_LEFT {
        if !mouse_over {
            return a;
        }
        if !s.toggleable {
            a.fire = s.pressed && s.repeat_times == 0;
            a.state.pressed = false;
            a.state.repeat_times = 0;
        }
        a.ret = true;
    }
    a
}

/// `cWidgetButton::OnMouseLeave` / `OnLostFocus`.
pub fn button_release(toggleable: bool, pressed: bool) -> bool {
    if toggleable { pressed } else { false }
}

/// `cWidgetButton::OnUpdate`; `fire` sends `ButtonPressed` with the stored press data.
pub fn button_update(s: ButtonState, dt: f32) -> ButtonAction {
    let mut a = ButtonAction {
        state: s,
        ..ButtonAction::default()
    };
    if s.pressed && s.repeat_active {
        a.state.timer = s.timer + dt;
        if a.state.timer > s.repeat_period {
            a.state.timer = 0.0;
            a.state.repeat_times = s.repeat_times.wrapping_add(1);
            a.fire = true;
        }
    }
    a
}

/// Whether a UI button message is the primary button (the only one buttons and check boxes react to).
pub fn ui_primary(val: i32) -> bool {
    val == UI_BUTTON_PRIMARY
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct ButtonDraw {
    pub global: Vector3f,
    pub size: Vector2f,
    pub pressed: bool,
    pub enabled: bool,
    pub has_image: bool,
    pub has_default_font: bool,
    pub pressed_offset: Vector3f,
    pub font_size: Vector2f,
    pub font_color: Color,
    pub color_mul: Color,
    pub has_background: bool,
}

/// `cWidgetButton::OnDraw`; `m` holds the up or down skin gfx metrics, `m[9]` the image.
pub fn button_draw(o: &mut Ops, d: &ButtonDraw, m: &[GfxMetrics; GFX_SLOTS]) {
    let offset = if d.pressed {
        d.pressed_offset
    } else {
        Vector3f::default()
    };
    if d.has_image {
        let image = m
            .get(GFX_IMAGE as usize)
            .map(|g| g.image)
            .unwrap_or_default();
        let color = if d.enabled { WHITE } else { grey(1.0, 0.2) };
        o.gfx(
            GFX_IMAGE,
            add3(
                add3(
                    d.global,
                    v3(
                        d.size.x / 2.0 - image.x / 2.0,
                        d.size.y / 2.0 - image.y / 2.0,
                        0.2,
                    ),
                ),
                offset,
            ),
            Some(v2(-1.0, -1.0)),
            Some(color),
        );
    }
    if d.has_default_font {
        let col = if d.enabled {
            d.font_color
        } else {
            grey(0.5, 1.0)
        };
        o.font(
            0,
            -1,
            0,
            add3(
                add3(
                    d.global,
                    v3(d.size.x / 2.0, d.size.y / 2.0 - d.font_size.y / 2.0, 0.5),
                ),
                offset,
            ),
            d.font_size,
            color_mul(col, d.color_mul),
            2,
        );
    }
    borders_and_corners(o, d.has_background, false, d.global, d.size, m);
}

// ---------------------------------------------------------------- check box

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct CheckLayout {
    pub box_size: Vector2f,
    pub size: Vector2f,
}

/// `cWidgetCheckBox::OnChangeSize`.
pub fn check_layout(
    box_width: f32,
    font_height: f32,
    label_pos_x: f32,
    label_size: Vector2f,
) -> CheckLayout {
    CheckLayout {
        box_size: v2(box_width, font_height),
        size: v2(box_width + 3.0 + label_pos_x + label_size.x, label_size.y),
    }
}

/// `cWidgetCheckBox::OnInit`: label position.
pub fn check_label_pos(box_width: f32, box_height: f32, font_height: f32) -> Vector3f {
    v3(box_width + 3.0, box_height / 2.0 - font_height / 2.0, 0.0)
}

/// `cWidgetCheckBox::OnDraw`: box position; the gfx is chosen by C++ (`[enabled][checked]`).
pub fn check_box_pos(global: Vector3f, box_height: f32, active_height: f32) -> Vector3f {
    add3(global, v3(0.0, box_height / 2.0 - active_height / 2.0, 0.0))
}

pub fn check_mouse_down(val: i32) -> bool {
    val & MOUSE_LEFT != 0
}

/// `Label_MouseDown` / `Label_MouseUp` callbacks: `(return value, forward the message)`.
pub fn check_label_forward(val: i32, enabled: bool) -> (bool, bool) {
    if val & MOUSE_LEFT == 0 && val & super::widget::MOUSE_RIGHT == 0 {
        (false, false)
    } else {
        (true, enabled)
    }
}

// ---------------------------------------------------------------- label

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct LabelScroll {
    pub timer: f32,
    pub offset: f32,
    pub down: bool,
}

/// `cWidgetLabel::OnUpdate` for a word-wrapped label.
pub fn label_update(
    s: LabelScroll,
    dt: f32,
    speed_mul: f32,
    height: f32,
    rows_height: f32,
) -> LabelScroll {
    let mut s = s;
    s.timer -= dt;
    if s.timer >= 0.0 {
        return s;
    }
    let adv = dt * speed_mul;
    if s.down {
        if s.offset + height + adv > rows_height {
            s.down = false;
        } else {
            s.offset += adv;
        }
    } else if s.offset - adv < 0.0 {
        s.down = true;
    } else {
        s.offset -= adv;
    }
    s
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct LabelDraw {
    pub global: Vector3f,
    pub size: Vector2f,
    pub align: i32,
    pub word_wrap: bool,
    pub max_chars: i32,
    pub enabled: bool,
    pub has_default_font: bool,
    pub font_size: Vector2f,
    pub font_color: Color,
    pub color_mul: Color,
    pub wrap_offset: f32,
    pub text_len: i32,
}

/// `cWidgetLabel::DrawText`; returns `mfWordWrapRowsHeight` for a wrapped label. `rows` are the row
/// lengths from `GetWordWrapRows`; row `i` is text slot `1 + i`.
pub fn label_draw_text(o: &mut Ops, d: &LabelDraw, rows: &[i32]) -> Option<f32> {
    let mut offset = Vector3f::default();
    if d.align == 2 {
        offset.x += d.size.x / 2.0;
    } else if d.align == 1 {
        offset.x += d.size.x;
    }
    let col = if d.enabled {
        d.font_color
    } else {
        grey(0.5, d.font_color.a)
    };
    let color = color_mul(col, d.color_mul);
    if d.word_wrap {
        let mut chars = 0i32;
        let height = d.font_size.y + 2.0;
        let rows_height = (height - 1.0) * rows.len() as f32;
        for (i, &len) in rows.iter().enumerate() {
            let mut brk = false;
            let mut len = len;
            let mut text_len = -1;
            if d.max_chars >= 0 {
                if chars.wrapping_add(len) > d.max_chars {
                    len = d.max_chars.wrapping_sub(chars);
                    text_len = len;
                    brk = true;
                }
                chars = chars.wrapping_add(len);
            }
            if d.has_default_font {
                let pos = sub3_y(add3(d.global, offset), d.wrap_offset);
                o.font(1 + i as u32, text_len, 0, pos, d.font_size, color, d.align);
            }
            offset.y += height;
            if brk {
                break;
            }
        }
        Some(rows_height)
    } else {
        if d.has_default_font {
            let text_len = if d.max_chars >= 0 && d.text_len > d.max_chars {
                d.max_chars
            } else {
                -1
            };
            o.font(
                0,
                text_len,
                0,
                add3(d.global, offset),
                d.font_size,
                color,
                d.align,
            );
        }
        None
    }
}

/// `v - cVector3f(0, y, 0)`.
fn sub3_y(v: Vector3f, y: f32) -> Vector3f {
    v3(v.x - 0.0, v.y - y, v.z - 0.0)
}

/// `cWidgetLabel::OnDraw` background: `(pos, size)`.
pub fn label_background(global: Vector3f, size: Vector2f) -> (Vector3f, Vector2f) {
    (
        v3(global.x - 2.0, global.y - 2.0, global.z - 0.001),
        v2(size.x + 4.0, size.y + 4.0),
    )
}

// ---------------------------------------------------------------- image

/// `cWidgetImage::OnMouseDown` / `OnMouseUp`.
pub fn image_mouse(val: i32) -> bool {
    !(val == MOUSE_WHEEL_UP || val == MOUSE_WHEEL_DOWN)
}

// ---------------------------------------------------------------- dummy

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct DummyChild {
    pub pos: Vector3f,
    pub size: Vector2f,
}

/// `cWidgetDummy::OnChildUpdate`: the bounding size of the children.
pub fn dummy_enclose(children: &[DummyChild]) -> Vector2f {
    let mut min = v2(9999.9, 9999.9);
    let mut max = v2(-9999.9, -9999.9);
    for c in children {
        let lr = add2(v2(c.pos.x, c.pos.y), c.size);
        if c.pos.x < min.x {
            min.x = c.pos.x;
        }
        if c.pos.y < min.y {
            min.y = c.pos.y;
        }
        if lr.x > max.x {
            max.x = lr.x;
        }
        if lr.y > max.y {
            max.y = lr.y;
        }
    }
    v2(max.x - min.x, max.y - min.y)
}

// ---------------------------------------------------------------- group

/// `cWidgetGroup::OnInit`: header label position.
pub fn group_header_pos(header_offset: f32, font_height: f32) -> Vector3f {
    v3(header_offset, -font_height * 0.5, 0.1)
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct GroupDraw {
    pub global: Vector3f,
    pub size: Vector2f,
    pub header_pos: Vector3f,
    pub header_size: Vector2f,
    pub has_background: bool,
    pub header_breaks_border: bool,
}

/// `cWidgetGroup::OnDraw`.
pub fn group_draw(o: &mut Ops, d: &GroupDraw, m: &[GfxMetrics; GFX_SLOTS]) {
    if !d.header_breaks_border {
        borders_and_corners(o, d.has_background, false, d.global, d.size, m);
        return;
    }
    let act = |i: usize| m.get(i).map(|g| g.active).unwrap_or_default();
    let img = |i: usize| m.get(i).map(|g| g.image).unwrap_or_default();
    let (c0, c1, c2, c3) = (act(5), act(6), act(7), act(8));
    let (b0, b2) = (act(1), act(3));
    let (i0, i1, i3) = (img(1), img(2), img(4));
    let size = d.size;
    let post = v3(d.header_pos.x + d.header_size.x, 0.0, 0.0);
    let pre_width = d.header_pos.x;
    let post_width = size.x - pre_width - d.header_size.x;
    o.offset(d.global);
    if d.has_background {
        o.gfx(
            0,
            v3(c0.x, c0.y, 0.0),
            Some(super::widget::sub2(super::widget::sub2(size, c2), c0)),
            Some(WHITE),
        );
    }
    o.gfx(
        1,
        v3(size.x - b0.x, c1.y, 0.0),
        Some(v2(i0.x, size.y - (c2.y + c1.y))),
        None,
    );
    o.gfx(
        2,
        v3(0.0, c0.y, 0.0),
        Some(v2(i1.x, size.y - (c3.y + c0.y))),
        None,
    );
    o.gfx(3, v3(c0.x, 0.0, 0.0), Some(v2(pre_width - 4.0, b2.y)), None);
    o.gfx(
        3,
        add3(post, v3(4.0, 0.0, 0.0)),
        Some(v2(post_width - c1.x - 4.0, b2.y)),
        None,
    );
    o.gfx(
        4,
        v3(c3.x, size.y - c3.y, 0.0),
        Some(v2(size.x - (c2.x + c3.x), i3.y)),
        None,
    );
    o.gfx(5, v3(0.0, 0.0, 0.0), None, None);
    o.gfx(6, v3(size.x - c1.x, 0.0, 0.0), None, None);
    o.gfx(7, v3(size.x - c2.x, size.y - c2.y, 0.0), None, None);
    o.gfx(8, v3(0.0, size.y - c3.y, 0.0), None, None);
    o.offset(v3(0.0, 0.0, 0.0));
}

// ---------------------------------------------------------------- file browser

/// `eFileBrowserFileType` of a lowercase extension, `None` when no list matches (`Other`).
pub fn file_type_of_extension(ext: &[u32]) -> Option<i32> {
    const TABLE: [(i32, &[&str]); 4] = [
        (0, &["bmp", "tga", "jpg"]),
        (1, &["dae", "ent"]),
        (2, &["ogg", "wav"]),
        (3, &["txt", "hps", "log"]),
    ];
    TABLE
        .iter()
        .find(|(_, exts)| {
            exts.iter()
                .any(|e| e.chars().map(u32::from).eq(ext.iter().copied()))
        })
        .map(|(t, _)| *t)
}

/// `iFileBrowser::GetHumanReadableSize`: `(number, unit index)` into bytes/KB/MB/GB. Sizes above
/// 1 TiB index one past the C++ unit array (undefined behaviour there); the unit clamps to GB.
pub fn human_readable_size(size: u64) -> (u64, usize) {
    let mut size = size;
    let mut unit = 0usize;
    while unit < 4 {
        if size > 1024 {
            size >>= 10;
        } else {
            break;
        }
        unit += 1;
    }
    (size, unit.min(3))
}

/// Drive letters of an `iFileBrowser` constructor: one per set bit, `'A'` plus the bit index.
pub fn drive_letters(mask: u64, out: &mut [u32]) -> usize {
    let mut mask = mask;
    let mut letter = u32::from(b'A');
    let mut n = 0;
    while mask != 0 {
        if mask & 1 != 0 {
            if let Some(slot) = out.get_mut(n) {
                *slot = letter;
            }
            n += 1;
        }
        letter += 1;
        mask >>= 1;
    }
    n.min(out.len())
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NavTarget {
    SystemRoot = 0,
    GameFolder = 1,
    PersonalFolder = 2,
    DiskDrive = 3,
    Up = 4,
    Here = 5,
    Path = 6,
}

fn eq_str(a: &[u32], s: &str) -> bool {
    s.chars().map(u32::from).eq(a.iter().copied())
}

/// `iFileBrowser::NavigateTo` branch selection.
pub fn navigate_target(path: &[u32]) -> NavTarget {
    if eq_str(path, "<System Root>") {
        NavTarget::SystemRoot
    } else if eq_str(path, "<Game Folder>") {
        NavTarget::GameFolder
    } else if eq_str(path, "<Personal Folder>") {
        NavTarget::PersonalFolder
    } else if path.get(..12).is_some_and(|p| eq_str(p, "<Disk Drive>")) {
        NavTarget::DiskDrive
    } else if eq_str(path, "..") {
        NavTarget::Up
    } else if eq_str(path, ".") {
        NavTarget::Here
    } else {
        NavTarget::Path
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PathPiece {
    Skip = 0,
    Pop = 1,
    Push = 2,
}

/// `iFileBrowser::SetPathFromString` handling of one path component; `depth` is the current
/// component count, and a `..` at depth 0 (a `pop_back` on an empty vector in C++) is skipped.
pub fn path_piece(piece: &[u32], depth: usize) -> PathPiece {
    if piece.is_empty() || eq_str(piece, ".") {
        PathPiece::Skip
    } else if eq_str(piece, "..") {
        if depth == 0 {
            PathPiece::Skip
        } else {
            PathPiece::Pop
        }
    } else {
        PathPiece::Push
    }
}
