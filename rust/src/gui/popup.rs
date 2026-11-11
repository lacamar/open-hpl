//! `iGuiPopUp` and the message box, colour picker, file picker and on-screen keyboard popups:
//! layout, button logic, colour component conversion, file-name and directory-list decisions and
//! the keyboard key grid with its focus navigation. Widgets, callbacks, `cGuiSet` calls and the
//! filesystem stay in C++. Widget pointers cross the boundary as `usize` ids.

use super::widget::{add3, v2};
use crate::math::v3;
use crate::string;
use crate::types::{Color, Vector2f, Vector3f};

pub const UI_BUTTON_PRIMARY: i32 = 1;
pub const UI_BUTTON_SECONDARY: i32 = 2;
pub const UI_BUTTON_PREV_PAGE: i32 = 4;
pub const UI_BUTTON_DELETE: i32 = 32;
pub const UI_BUTTON_CLEAR: i32 = 64;

// ---------------------------------------------------------------- iGuiPopUp

/// Focus bookkeeping of `iGuiPopUp`: what to restore on destruction.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PopUpState {
    pub att_changed: bool,
    pub focus_changed: bool,
    pub default_focus_changed: bool,
    pub new_attention: usize,
    pub new_focus: usize,
    pub new_default_focus: usize,
}

impl PopUpState {
    pub fn new(attention: usize, focus: usize, default_focus: usize) -> Self {
        PopUpState {
            new_attention: attention,
            new_focus: focus,
            new_default_focus: default_focus,
            ..PopUpState::default()
        }
    }

    pub fn set_next_focus(&mut self, widget: usize) {
        self.new_focus = widget;
        self.focus_changed = widget != 0;
    }

    /// `RunCallback` after the callback ran with focus checks on; `pre` and `post` are
    /// (attention, focus, default focus).
    pub fn after_callback(&mut self, pre: [usize; 3], post: [usize; 3]) {
        self.att_changed = pre[0] != post[0];
        if self.att_changed {
            self.new_attention = post[0];
        }
        self.focus_changed = pre[1] != post[1];
        if self.focus_changed {
            self.new_focus = post[1];
        }
        self.default_focus_changed = pre[2] != post[2];
        if self.default_focus_changed {
            self.new_default_focus = post[2];
        }
    }
}

/// Centred window position of a new popup.
pub fn popup_pos(virtual_size: Vector2f, size: Vector2f, popup_z: f32) -> Vector3f {
    let d = v3(virtual_size.x - size.x, virtual_size.y - size.y, 0.0);
    add3(v3(d.x * 0.5, d.y * 0.5, d.z * 0.5), v3(0.0, 0.0, popup_z))
}

// ---------------------------------------------------------------- message box

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MsgBoxLayout {
    pub window_pos: Vector3f,
    pub window_size: Vector2f,
    pub button0_pos: Vector3f,
    pub button1_pos: Vector3f,
    pub label_pos: Vector3f,
    pub label_size: Vector2f,
}

fn fmax(x: f32, y: f32) -> f32 {
    if x > y { x } else { y }
}

pub fn msgbox_layout(
    label_len: f32,
    text_len: f32,
    font_y: f32,
    virtual_size: Vector2f,
    two_buttons: bool,
) -> MsgBoxLayout {
    let mut min_len = label_len;
    if text_len > min_len {
        min_len = text_len;
    }
    let width = if min_len + 40.0 > 200.0 {
        min_len + 40.0
    } else {
        200.0
    };
    let height = 90.0 + font_y;
    let window_pos = v3(
        fmax(virtual_size.x / 2.0 - width / 2.0, 0.0),
        fmax(virtual_size.y / 2.0 - height / 2.0, 0.0),
        100.0,
    );
    let b0 = if two_buttons {
        v3(width / 2.0 - (80.0 * 2.0 + 20.0) / 2.0, 50.0 + font_y, 1.0)
    } else {
        v3(width / 2.0 - 40.0, 50.0 + font_y, 1.0)
    };
    let b1 = if two_buttons {
        v3(b0.x + (80.0 + 20.0), b0.y, b0.z)
    } else {
        Vector3f::default()
    };
    MsgBoxLayout {
        window_pos,
        window_size: v2(width, height),
        button0_pos: b0,
        button1_pos: b1,
        label_pos: v3(20.0, 30.0, 1.0),
        label_size: v2(width - 10.0, font_y),
    }
}

/// Gamepad press: `None` when ignored, else whether the second button is the one pressed.
pub fn msgbox_gamepad(val: i32, has_button1: bool) -> Option<bool> {
    if !(val == UI_BUTTON_PRIMARY || val == UI_BUTTON_SECONDARY) {
        return None;
    }
    Some(val == UI_BUTTON_SECONDARY && has_button1)
}

// ---------------------------------------------------------------- colour picker

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ColorLayout {
    pub button_pos: [Vector3f; 3],
    pub button_size: [Vector2f; 3],
    pub label_pos: [Vector3f; 4],
    pub slider_pos: [Vector3f; 4],
    pub input_pos: [Vector3f; 4],
    pub current_frame_pos: Vector3f,
    pub previous_frame_pos: Vector3f,
}

pub fn color_layout() -> ColorLayout {
    let base = v3(95.0, 50.0, 0.2);
    let row = |dx: f32, i: i32| add3(base, v3(dx, 30.0 * i as f32, 0.0));
    let mut l = ColorLayout {
        button_pos: [
            v3(60.0, 200.0, 0.1),
            v3(60.0, 220.0, 0.5),
            v3(150.0, 220.0, 0.5),
        ],
        button_size: [v2(23.0, 23.0), v2(70.0, 20.0), v2(70.0, 20.0)],
        label_pos: [Vector3f::default(); 4],
        slider_pos: [Vector3f::default(); 4],
        input_pos: [Vector3f::default(); 4],
        current_frame_pos: v3(10.0, 40.0, 0.5),
        previous_frame_pos: v3(35.0, 65.0, 0.1),
    };
    for (i, ((lp, sp), ip)) in l
        .label_pos
        .iter_mut()
        .zip(l.slider_pos.iter_mut())
        .zip(l.input_pos.iter_mut())
        .enumerate()
    {
        let i = i as i32;
        *lp = add3(add3(base, v3(5.0, 5.0, 0.0)), v3(0.0, 30.0 * i as f32, 0.0));
        *sp = row(30.0, i);
        *ip = row(150.0, i);
    }
    l
}

/// Slider value of a colour component.
pub fn color_slider_value(c: f32) -> i32 {
    (c * 1000.0) as i32
}

/// Colour component of a slider value.
pub fn color_from_slider(v: i32) -> f32 {
    v as f32 * 0.001
}

/// Index of the component whose widget fired; `None` where the C++ loop ran off the end and
/// wrote component 4.
pub fn color_component(hits: &[bool; 4]) -> Option<usize> {
    hits.iter().position(|&h| h)
}

/// `ClosePopUp`: whether the colour differs from the original.
pub fn color_changed(a: Color, b: Color) -> bool {
    !(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a)
}

// ---------------------------------------------------------------- file picker

pub const PICKER_SAVE: i32 = 0;
pub const PICKER_LOAD: i32 = 1;

/// Start filename of the picker: file name of the given name, else of the start path, cleared
/// when that equals the whole start path.
pub fn fp_start_filename(start_filename: &[u32], start_path: &[u32]) -> Vec<u32> {
    let mut f = string::file_name(start_filename).to_vec();
    if f.is_empty() {
        f = string::file_name(start_path).to_vec();
    }
    if f == start_path {
        f.clear();
    }
    f
}

/// Window caption: `Load file`, `Save file` or ` file`.
pub fn fp_caption(kind: i32) -> Vec<u32> {
    let head: &str = match kind {
        PICKER_LOAD => "Load",
        PICKER_SAVE => "Save",
        _ => "",
    };
    head.chars().chain(" file".chars()).map(u32::from).collect()
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FpLayout {
    pub dir_pos: Vector3f,
    pub dir_size: Vector2f,
    pub button_pos: [Vector3f; 3],
    pub button_size: [Vector2f; 3],
    pub name_pos: Vector3f,
    pub name_size: Vector2f,
    pub filter_pos: Vector3f,
    pub filter_size: Vector2f,
    pub list_pos: Vector3f,
    pub list_size: Vector2f,
    pub column_width: [i32; 4],
}

pub fn fp_layout(font_y: f32) -> FpLayout {
    FpLayout {
        dir_pos: v3(30.0, 60.0, 0.1),
        dir_size: v2(400.0, font_y + 2.0),
        button_pos: [
            v3(435.0, 60.0, 0.1),
            v3(500.0, 425.0, 0.5),
            v3(500.0, 450.0, 0.5),
        ],
        button_size: [v2(23.0, 23.0), v2(70.0, 20.0), v2(70.0, 20.0)],
        name_pos: v3(30.0, 425.0, 0.5),
        name_size: v2(400.0, font_y + 2.0),
        filter_pos: v3(30.0, 450.0, 0.5),
        filter_size: v2(300.0, font_y + 2.0),
        list_pos: v3(30.0, 90.0, 1.0),
        list_size: v2(540.0, 300.0),
        column_width: [24, 300, 75, 125],
    }
}

/// Filter combo text of one category: `name (f1, f2)`.
pub fn fp_filter_text(name: &[u32], filters: &[&[u32]]) -> Vec<u32> {
    let mut t = name.to_vec();
    t.extend(" (".chars().map(u32::from));
    for (j, f) in filters.iter().enumerate() {
        if j != 0 {
            t.extend(", ".chars().map(u32::from));
        }
        t.extend_from_slice(f);
    }
    t.push(u32::from(b')'));
    t
}

pub const DIR_ROOT: i32 = -1;
pub const DIR_PERSONAL: i32 = -2;
pub const DIR_GAME: i32 = -3;

/// Entries of the directory combo: the special folders, then path component `i` as `i`.
pub fn fp_dir_entries(has_personal: bool, has_game: bool, path_len: usize) -> Vec<i32> {
    let mut v = vec![DIR_ROOT];
    if has_personal {
        v.push(DIR_PERSONAL);
    }
    if has_game {
        v.push(DIR_GAME);
    }
    v.extend((0..path_len).map(|i| i as i32));
    v
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DirSelect {
    /// 0 nothing, 1 clear the path then navigate, 2 pop `pops` entries then navigate.
    pub action: i32,
    pub pops: usize,
}

/// `Directory_OnSelectionChange`. The first test compares as `size_t`; the pop count is clamped
/// to the path length where the C++ popped an empty vector.
pub fn fp_dir_select(selection: i32, num_items: i32, path_len: usize) -> DirSelect {
    let sel_u = i64::from(selection) as u64;
    let limit = (i64::from(num_items) as u64).wrapping_sub(path_len as u64);
    if sel_u < limit {
        return DirSelect { action: 1, pops: 0 };
    }
    if selection != num_items.wrapping_sub(1) {
        let wanted = i64::from(num_items) - i64::from(selection);
        let pops = usize::try_from(wanted).unwrap_or(0).min(path_len);
        return DirSelect { action: 2, pops };
    }
    DirSelect { action: 0, pops: 0 }
}

pub const FP_NONE: i32 = 0;
pub const FP_ERROR_EMPTY: i32 = 1;
pub const FP_SAVE_CONFIRM: i32 = 2;
pub const FP_SAVE_DONE: i32 = 3;
pub const FP_LOAD_MISSING: i32 = 4;
pub const FP_LOAD_DONE: i32 = 5;

/// Outcome of the accept button once the file's existence is known (`exists` includes the
/// extension-adjusted path for saves).
pub fn fp_accept(kind: i32, name_empty: bool, exists: bool) -> i32 {
    if name_empty {
        return FP_ERROR_EMPTY;
    }
    match kind {
        PICKER_SAVE if exists => FP_SAVE_CONFIRM,
        PICKER_SAVE => FP_SAVE_DONE,
        PICKER_LOAD if !exists => FP_LOAD_MISSING,
        PICKER_LOAD => FP_LOAD_DONE,
        _ => FP_NONE,
    }
}

/// Extension to append to a saved file name: `filter0` is the first filter (`*.ext`). `None`
/// when the path already ends in it or when the picker has no filter (the C++ indexed an empty
/// vector).
pub fn fp_save_extension(full_path: &[u32], filter0: Option<&[u32]>) -> Option<Vec<u32>> {
    let f = filter0?;
    let want = string::sub(f, 2, -1, true)?;
    if string::file_ext(full_path) == want {
        return None;
    }
    Some(string::sub(f, 1, -1, true)?.to_vec())
}

/// Message box answer of the overwrite prompt: whether to save.
pub fn fp_overwrite_confirmed(kind: i32, val: i32) -> bool {
    val == 0 && kind == PICKER_SAVE
}

// ---------------------------------------------------------------- UI keyboard

pub const KEY_DIGIT: i32 = 0;
pub const KEY_LETTER: i32 = 1;
pub const KEY_ENTER: i32 = 2;
pub const KEY_CANCEL: i32 = 3;
pub const KEY_SPACE: i32 = 4;

pub const KB_KEY_SIZE: f32 = 32.0;
pub const KB_KEYS: usize = 39;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct KbKey {
    pub kind: i32,
    /// Offset from the first digit or letter key.
    pub offset: i32,
    pub size: Vector2f,
    pub lower: i32,
    pub upper: i32,
    /// `cUIKey` is attached: the key types a character.
    pub types_char: bool,
}

/// Keys in creation order: digits, letters, Enter, Cancel, Space.
pub fn kb_keys() -> Vec<KbKey> {
    let sq = v2(KB_KEY_SIZE, KB_KEY_SIZE);
    let wide = v2(KB_KEY_SIZE * 2.0 + 1.0, KB_KEY_SIZE);
    let mut v = Vec::with_capacity(KB_KEYS);
    for i in 0..10 {
        let u = i32::from(b'0') + i;
        v.push(KbKey {
            kind: KEY_DIGIT,
            offset: i,
            size: sq,
            lower: u,
            upper: u,
            types_char: true,
        });
    }
    for i in 0..26 {
        let u = i32::from(b'a') + i;
        v.push(KbKey {
            kind: KEY_LETTER,
            offset: i,
            size: sq,
            lower: u,
            upper: u + (i32::from(b'A') - i32::from(b'a')),
            types_char: true,
        });
    }
    v.push(KbKey {
        kind: KEY_ENTER,
        size: wide,
        ..KbKey::default()
    });
    v.push(KbKey {
        kind: KEY_CANCEL,
        size: wide,
        ..KbKey::default()
    });
    let u = i32::from(b' ');
    v.push(KbKey {
        kind: KEY_SPACE,
        size: v2((KB_KEY_SIZE + 1.0) * 10.0 - 1.0, KB_KEY_SIZE),
        lower: u,
        upper: u,
        types_char: true,
        ..KbKey::default()
    });
    v
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct KbLayout {
    pub window_size: Vector2f,
    pub rows: usize,
}

/// Greedy row wrap of the key sizes; writes each key's position and row index.
pub fn kb_layout(sizes: &[Vector2f], pos: &mut [Vector3f], row: &mut [i32]) -> KbLayout {
    let mut p = v3(0.0, 0.0, 2.0);
    let mut r = 0usize;
    for ((s, pos), row) in sizes.iter().zip(pos.iter_mut()).zip(row.iter_mut()) {
        if p.x + s.x > (KB_KEY_SIZE + 1.0) * 10.0 - 1.0 {
            r += 1;
            p.x = 0.0;
            p.y += s.y + 1.0;
        }
        *pos = p;
        *row = r as i32;
        p.x += s.x + 1.0;
    }
    KbLayout {
        window_size: v2((KB_KEY_SIZE + 1.0) * 10.0, p.y + KB_KEY_SIZE + 1.0),
        rows: r + 1,
    }
}

/// Window position below (and centred on) the target text box.
pub fn kb_window_pos(target_global: Vector3f, target_size: Vector2f, window_w: f32) -> Vector3f {
    let mut p = target_global;
    p.y += target_size.y + 8.0;
    p.x += target_size.x * 0.5;
    p.x -= window_w * 0.5;
    p
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct KbNav {
    pub up: i32,
    pub right: i32,
    pub down: i32,
    pub left: i32,
}

/// Wrap-around navigation over the rows; indices are into the flat key list, -1 where the C++
/// took the `back()` of an empty row.
pub fn kb_nav(row_len: &[usize], out: &mut [KbNav]) {
    let mut starts = Vec::with_capacity(row_len.len());
    let mut acc = 0usize;
    for &n in row_len {
        starts.push(acc);
        acc += n;
    }
    let at = |r: usize, j: usize| -> i32 {
        let (Some(&n), Some(&s)) = (row_len.get(r), starts.get(r)) else {
            return -1;
        };
        if n == 0 {
            return -1;
        }
        i32::try_from(s + j.min(n - 1)).unwrap_or(-1)
    };
    let rows = row_len.len();
    let mut k = 0usize;
    for (i, &n) in row_len.iter().enumerate() {
        let up_row = if i > 0 { i - 1 } else { rows - 1 };
        let down_row = if i + 1 < rows { i + 1 } else { 0 };
        for j in 0..n {
            let left = if j > 0 { at(i, j - 1) } else { at(i, n - 1) };
            let right = if j + 1 < n { at(i, j + 1) } else { at(i, 0) };
            if let Some(o) = out.get_mut(k) {
                *o = KbNav {
                    up: at(up_row, j),
                    right,
                    down: at(down_row, j),
                    left,
                };
            }
            k += 1;
        }
    }
}

pub const KB_NONE: i32 = 0;
pub const KB_CANCEL: i32 = 1;
pub const KB_DELETE: i32 = 2;
pub const KB_CLEAR: i32 = 3;
pub const KB_SHIFT_ON: i32 = 4;

/// `Window_OnUIButtonPress`.
pub fn kb_ui_press(val: i32) -> i32 {
    match val {
        UI_BUTTON_SECONDARY => KB_CANCEL,
        UI_BUTTON_DELETE => KB_DELETE,
        UI_BUTTON_CLEAR => KB_CLEAR,
        UI_BUTTON_PREV_PAGE => KB_SHIFT_ON,
        _ => KB_NONE,
    }
}

/// `Window_OnUIButtonRelease`: whether shift is released.
pub fn kb_ui_release(val: i32) -> bool {
    val & UI_BUTTON_PREV_PAGE != 0
}
