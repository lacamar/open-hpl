//! Host table of the H20 widgets (text box, list boxes, combo box, tab frame, menus): one `op`
//! entry the C++ shell implements (`gui/RustGuiH20.h`), reaching the widgets, items, sliders,
//! skin and draw calls the Rust orchestration does not own. Calls may re-enter Rust, so no
//! reference to widget state is held across [`Hx::op`].

use super::set::{Msg, W};
use crate::math::v3;
use crate::types::{Vector2f, Vector3f};
use std::ffi::c_void;

macro_rules! ops {
    ($($name:ident = $v:expr),* $(,)?) => {
        $(pub const $name: i32 = $v;)*
        #[cfg(test)]
        pub const OP_TABLE: &[(&str, i32)] = &[$((stringify!($name), $v)),*];
    };
}

ops! {
    SLIDER_SET_VALUE = 1,
    SLIDER_GET_VALUE = 2,
    SLIDER_SET_BAR_VALUE_SIZE = 3,
    SLIDER_SET_MAX_VALUE = 4,
    SLIDER_SET_BUTTON_VALUE_ADD = 5,
    SLIDER_SET_BAR_CLICK_VALUE_ADD = 6,
    SLIDER_GET_BUTTON_VALUE_ADD = 7,
    WIDGET_SET_SIZE = 8,
    WIDGET_SET_POSITION = 9,
    WIDGET_GET_SIZE = 10,
    ITEM_NUM = 11,
    ITEM_GET = 12,
    ITEM_IS_SELECTED = 13,
    ITEM_SET_SELECTED = 14,
    ITEM_IS_SELECTABLE = 15,
    ITEM_INDEX = 16,
    ITEM_TEXT = 17,
    PROCESS_MESSAGE = 18,
    WORLD_TO_LOCAL = 19,
    GET_GLOBAL = 20,
    GET_FONT_SIZE = 21,
    HAS_FOCUS = 22,
    CREATE_SLIDER = 23,
    ADD_CALLBACK = 24,
    DRAW_GFX = 25,
    DRAW_BORDERS = 26,
    SKIN_GFX = 27,
    SKIN_ATTRIBUTE = 28,
    GFX_ACTIVE_SIZE = 29,
    GFX_IMAGE_SIZE = 30,
    CLIP_CREATE_CHILD = 31,
    SET_CLIP = 32,
    DRAW_TEXT = 33,
    GFX_RECT = 34,
    CREATE_LABEL = 35,
    LABEL_SET_COLOR = 36,
    LABEL_SET_FONT_SIZE = 37,
    TEXT_LENGTH = 38,
    ITEM_PROP_GET = 39,
    PROP_TYPE = 40,
    PROP_TEXT = 41,
    PROP_GFX = 42,
    WIDGET_GET_TEXT = 43,
    DESTROY_WIDGET = 44,
    SET_DESTROYING = 45,
    WIDGET_SET_VISIBLE = 46,
    WIDGET_SET_ENABLED = 47,
    WIDGET_IS_ENABLED = 48,
    CLIP_GET_CURRENT = 49,
    SET_CALL = 50,
    WIDGET_SET_TEXT = 51,
    WIDGET_SET_FONT_SIZE = 52,
    TEXTBOX_SET_CAN_EDIT = 53,
    TEXTBOX_GET_CAN_EDIT = 54,
    TEXTBOX_BG_SIZE = 55,
    CREATE_TEXTBOX = 56,
    CREATE_BUTTON = 57,
    BUTTON_SET_IMAGE = 58,
    CHECK_POINT_CLIP = 59,
    WIDGET_GET_POSITION = 60,
    WIDGET_IS_VISIBLE = 61,
    STATE_PTR = 62,
    CREATE_TAB = 63,
    CREATE_TAB_LABEL = 64,
    CREATE_FRAME = 65,
    WIDGET_SET_CLIP_ACTIVE = 66,
    ATTACH_CHILD = 67,
    REMOVE_CHILD = 68,
    SKIN_FONT_SIZE = 69,
    TEXT_LENGTH_SKIN = 70,
    CREATE_BUTTON_AT = 71,
    FONT_RANGE = 72,
    GLYPH = 73,
    FONT_LENGTH = 74,
    CLIPBOARD = 75,
    COPY = 76,
    TO_FLOAT = 77,
    FLOAT_TEXT = 78,
    SET_POINTER_GFX = 79,
    GFX_ANIM_RESET = 80,
    CREATE_POPUP_KEYBOARD = 81,
    BUTTON_SETUP = 82,
    MENU_CALL = 84,
    WIDGET_TYPE = 85,
    WIDGET_SET_CHILDREN_OFFSET = 86,
    IS_CONNECTED = 87,
    ITEM_TEXT_LENGTH = 88,
    ITEM_IS_SEPARATOR = 89,
    WIDGET_SHORTCUT_TEXT = 90,
}

pub const TYPE_ROOT: i32 = 0;
pub const TYPE_WINDOW: i32 = 1;

pub const MC_HIGHLIGHTED: i32 = 0;
pub const MC_SET_HIGHLIGHTED: i32 = 1;
pub const MC_HIDE: i32 = 2;
pub const MC_ARROW_OFFSET: i32 = 3;
pub const MC_CHECK_OFFSET: i32 = 4;
pub const MC_TEXT_OFFSET: i32 = 5;
pub const MC_ITEM_WIDTH: i32 = 6;
pub const MC_IS_SUBMENU_OPEN: i32 = 8;
pub const MC_TOP_MOST: i32 = 9;
pub const MC_PARENT_MENU: i32 = 10;
pub const MC_ON_CHILD_HIDE: i32 = 11;
pub const MC_SET_MUST_HIDE: i32 = 12;
pub const MC_GET_MUST_HIDE: i32 = 13;
pub const MC_PARENT_ITEM: i32 = 14;
pub const MC_PREV_ATTENTION: i32 = 15;
pub const MC_ITEM_COUNT: i32 = 16;
pub const MC_ITEM_AT: i32 = 17;
pub const MC_SET_OFFSETS: i32 = 18;
pub const MC_ITEM_PARENT_MENU: i32 = 19;

pub const SC_PUSH_ATTENTION: i32 = 0;
pub const SC_PUSH_FOCUSED: i32 = 1;
pub const SC_PUSH_DEFAULT_NAV: i32 = 2;
pub const SC_SET_ATTENTION: i32 = 3;
pub const SC_SET_FOCUSED: i32 = 4;
pub const SC_POP_ATTENTION: i32 = 5;
pub const SC_POP_FOCUSED: i32 = 6;
pub const SC_POP_DEFAULT_NAV: i32 = 7;
pub const SC_POPUP_Z: i32 = 8;
pub const SC_VIRTUAL_SIZE: i32 = 9;
pub const SC_DEC_CONTEXT_Z: i32 = 10;
pub const SC_GET_ATTENTION: i32 = 11;
pub const SC_GET_FOCUSED: i32 = 12;
pub const SC_BASE_CLIP: i32 = 13;
pub const SC_CURRENT_CLIP: i32 = 14;
pub const SC_SET_ATTENTION_NO_CLEAR: i32 = 15;
pub const SC_SHOW_CONTEXT_MENU: i32 = 16;
pub const SC_GET_DRAW_MOUSE: i32 = 17;

pub const ARGS_F: usize = 10;

/// Argument block of one host call; the fields each op reads and writes are in `RustGuiH20.h`.
#[repr(C)]
pub struct Args {
    pub w: W,
    pub p: [usize; 3],
    pub i: [i32; 4],
    pub f: [f32; ARGS_F],
    pub text: *const u32,
    pub text_len: usize,
    pub rp: usize,
    pub ri: [i32; 2],
    pub rf: [f32; 3],
}

impl Args {
    pub fn new(w: W) -> Args {
        Args {
            w,
            p: [0; 3],
            i: [0; 4],
            f: [0.0; ARGS_F],
            text: std::ptr::null(),
            text_len: 0,
            rp: 0,
            ri: [0; 2],
            rf: [0.0; 3],
        }
    }
}

#[repr(C)]
pub struct HostVt {
    pub ctx: *mut c_void,
    pub op: extern "C" fn(*mut c_void, i32, *mut Args) -> usize,
}

/// Safe handle on a host table, copied freely.
#[derive(Clone, Copy)]
pub struct Hx(*const HostVt);

impl Hx {
    /// # Safety
    /// `vt` is null or valid for the whole call.
    pub unsafe fn new(vt: *const HostVt) -> Hx {
        Hx(vt)
    }

    pub fn raw(self, code: i32, a: &mut Args) -> usize {
        // SAFETY: `Hx::new` contract; null tables make every call a no-op.
        match unsafe { self.0.as_ref() } {
            Some(h) => (h.op)(h.ctx, code, a),
            None => 0,
        }
    }

    fn with<R>(
        self,
        code: i32,
        w: W,
        f: impl FnOnce(&mut Args),
        r: impl FnOnce(&Args, usize) -> R,
    ) -> R {
        let mut a = Args::new(w);
        f(&mut a);
        let v = self.raw(code, &mut a);
        r(&a, v)
    }

    pub fn slider_set_value(self, w: W, v: i32, flag: Option<bool>) {
        self.with(
            SLIDER_SET_VALUE,
            w,
            |a| {
                a.i[0] = v;
                a.i[1] = flag.map_or(0, |_| 1);
                a.i[2] = i32::from(flag == Some(true));
            },
            |_, _| (),
        );
    }

    pub fn slider_value(self, w: W) -> i32 {
        self.with(SLIDER_GET_VALUE, w, |_| (), |a, _| a.ri[0])
    }

    pub fn slider_call(self, code: i32, w: W, v: i32) -> i32 {
        self.with(code, w, |a| a.i[0] = v, |a, _| a.ri[0])
    }

    pub fn set_size(self, w: W, s: Vector2f) {
        self.with(
            WIDGET_SET_SIZE,
            w,
            |a| {
                a.f[0] = s.x;
                a.f[1] = s.y;
            },
            |_, _| (),
        );
    }

    pub fn size(self, w: W) -> Vector2f {
        self.with(
            WIDGET_GET_SIZE,
            w,
            |_| (),
            |a, _| Vector2f {
                x: a.rf[0],
                y: a.rf[1],
            },
        )
    }

    pub fn set_position(self, w: W, p: Vector3f) {
        self.with(
            WIDGET_SET_POSITION,
            w,
            |a| {
                a.f[0] = p.x;
                a.f[1] = p.y;
                a.f[2] = p.z;
            },
            |_, _| (),
        );
    }

    pub fn item_num(self, container: usize) -> i32 {
        self.with(ITEM_NUM, 0, |a| a.p[0] = container, |a, _| a.ri[0])
    }

    pub fn item(self, container: usize, idx: i32) -> usize {
        self.with(
            ITEM_GET,
            0,
            |a| {
                a.p[0] = container;
                a.i[0] = idx;
            },
            |_, v| v,
        )
    }

    pub fn item_flag(self, code: i32, item: usize) -> bool {
        self.with(code, 0, |a| a.p[0] = item, |_, v| v != 0)
    }

    pub fn item_set_selected(self, item: usize, v: bool) {
        self.with(
            ITEM_SET_SELECTED,
            0,
            |a| {
                a.p[0] = item;
                a.i[0] = i32::from(v);
            },
            |_, _| (),
        );
    }

    pub fn item_index(self, item: usize) -> i32 {
        self.with(ITEM_INDEX, 0, |a| a.p[0] = item, |a, _| a.ri[0])
    }

    pub fn item_text(self, item: usize) -> (*const u32, usize) {
        self.with(
            ITEM_TEXT,
            0,
            |a| a.p[0] = item,
            |a, v| (v as *const u32, usize::try_from(a.ri[0]).unwrap_or(0)),
        )
    }

    /// `ProcessMessage(msg, data)`, `data` the original message or a value-only one.
    pub fn process(self, w: W, msg: i32, val: i32, data: Option<&Msg>) -> bool {
        self.with(
            PROCESS_MESSAGE,
            w,
            |a| {
                a.i[0] = msg;
                a.i[1] = val;
                a.p[0] = data.map_or(0, |d| std::ptr::from_ref(d) as usize);
            },
            |_, v| v != 0,
        )
    }

    pub fn world_to_local(self, w: W, p: [f32; 2]) -> Vector3f {
        self.with(
            WORLD_TO_LOCAL,
            w,
            |a| {
                a.f[0] = p[0];
                a.f[1] = p[1];
            },
            |a, _| v3(a.rf[0], a.rf[1], a.rf[2]),
        )
    }

    pub fn global(self, w: W) -> Vector3f {
        self.with(GET_GLOBAL, w, |_| (), |a, _| v3(a.rf[0], a.rf[1], a.rf[2]))
    }

    pub fn font_size(self, w: W) -> Vector2f {
        self.with(
            GET_FONT_SIZE,
            w,
            |_| (),
            |a, _| Vector2f {
                x: a.rf[0],
                y: a.rf[1],
            },
        )
    }

    pub fn has_focus(self, w: W) -> bool {
        self.with(HAS_FOCUS, w, |_| (), |_, v| v != 0)
    }

    pub fn add_callback(self, w: W, msg: i32, obj: usize, f: usize) {
        self.with(
            ADD_CALLBACK,
            w,
            |a| {
                a.i[0] = msg;
                a.p[0] = obj;
                a.p[1] = f;
            },
            |_, _| (),
        );
    }

    pub fn draw_gfx(
        self,
        w: W,
        gfx: usize,
        pos: Vector3f,
        size: Vector2f,
        color: Option<[f32; 4]>,
    ) {
        self.with(
            DRAW_GFX,
            w,
            |a| {
                a.p[0] = gfx;
                a.f[..5].copy_from_slice(&[pos.x, pos.y, pos.z, size.x, size.y]);
                if let Some(c) = color {
                    a.i[0] = 1;
                    a.f[5..9].copy_from_slice(&c);
                }
            },
            |_, _| (),
        );
    }

    /// `DrawBordersAndCorners(bg, borders, corners, pos, size)`; `borders` and `corners` hold 4 gfx.
    pub fn draw_borders(
        self,
        w: W,
        bg: usize,
        (borders, corners): (&[usize; 4], &[usize; 4]),
        pos: Vector3f,
        size: Vector2f,
        flag: Option<bool>,
    ) {
        self.with(
            DRAW_BORDERS,
            w,
            |a| {
                a.p[0] = bg;
                a.p[1] = borders.as_ptr() as usize;
                a.p[2] = corners.as_ptr() as usize;
                a.f[..5].copy_from_slice(&[pos.x, pos.y, pos.z, size.x, size.y]);
                a.i[0] = flag.map_or(0, |_| 1);
                a.i[1] = i32::from(flag == Some(true));
            },
            |_, _| (),
        );
    }

    pub fn skin_gfx(self, w: W, idx: i32) -> usize {
        self.with(SKIN_GFX, w, |a| a.i[0] = idx, |_, v| v)
    }

    pub fn skin_attr(self, w: W, idx: i32) -> Vector3f {
        self.with(
            SKIN_ATTRIBUTE,
            w,
            |a| a.i[0] = idx,
            |a, _| v3(a.rf[0], a.rf[1], a.rf[2]),
        )
    }

    pub fn gfx_size(self, code: i32, gfx: usize) -> Vector2f {
        self.with(
            code,
            0,
            |a| a.p[0] = gfx,
            |a, _| Vector2f {
                x: a.rf[0],
                y: a.rf[1],
            },
        )
    }

    pub fn clip_child(self, clip: usize, pos: Vector3f, size: Vector2f) -> usize {
        self.with(
            CLIP_CREATE_CHILD,
            0,
            |a| {
                a.p[0] = clip;
                a.f[..5].copy_from_slice(&[pos.x, pos.y, pos.z, size.x, size.y]);
            },
            |_, v| v,
        )
    }

    pub fn set_clip(self, w: W, clip: usize) {
        self.with(SET_CLIP, w, |a| a.p[0] = clip, |_, _| ());
    }

    /// `DrawDefaultText` (`highlight` false) or `DrawDefaultTextHighlight`.
    pub fn draw_text(
        self,
        w: W,
        text: (*const u32, usize),
        pos: Vector3f,
        align: i32,
        highlight: bool,
    ) {
        self.with(
            DRAW_TEXT,
            w,
            |a| {
                a.text = text.0;
                a.text_len = text.1;
                a.f[..3].copy_from_slice(&[pos.x, pos.y, pos.z]);
                a.i[0] = align;
                a.i[1] = i32::from(highlight);
            },
            |_, _| (),
        );
    }

    /// `cGuiSet` call `which` (`SC_*`) of the set owning widget `w`; `p` is the widget argument.
    pub fn set_call(self, w: W, which: i32, p: usize) -> (usize, [f32; 2]) {
        self.with(
            SET_CALL,
            w,
            |a| {
                a.i[0] = which;
                a.p[0] = p;
            },
            |a, v| (v, [a.rf[0], a.rf[1]]),
        )
    }

    pub fn set_text(self, w: W, text: (*const u32, usize)) {
        self.with(
            WIDGET_SET_TEXT,
            w,
            |a| {
                a.text = text.0;
                a.text_len = text.1;
            },
            |_, _| (),
        );
    }

    pub fn text(self, w: W) -> (*const u32, usize) {
        self.with(
            WIDGET_GET_TEXT,
            w,
            |_| (),
            |a, v| (v as *const u32, usize::try_from(a.ri[0]).unwrap_or(0)),
        )
    }

    pub fn set_flag(self, code: i32, w: W, v: bool) {
        self.with(code, w, |a| a.i[0] = i32::from(v), |_, _| ());
    }

    pub fn flag(self, code: i32, w: W) -> bool {
        self.with(code, w, |_| (), |_, v| v != 0)
    }

    /// `DrawDefaultText` in the widget's default font colour (`mode` 1: disabled aware).
    pub fn draw_text_col(
        self,
        w: W,
        text: (*const u32, usize),
        pos: Vector3f,
        align: i32,
        mode: i32,
    ) {
        self.with(
            DRAW_TEXT,
            w,
            |a| {
                a.text = text.0;
                a.text_len = text.1;
                a.f[..3].copy_from_slice(&[pos.x, pos.y, pos.z]);
                a.i[0] = align;
                a.i[2] = mode;
            },
            |_, _| (),
        );
    }

    pub fn gfx_rect(self) -> usize {
        self.with(GFX_RECT, 0, |_| (), |_, v| v)
    }
}
