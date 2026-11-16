//! Script struct blocks (`cImGuiGfx`, `cImGuiButtonData`, ...) read at the property offsets the
//! official binary uses (`script_api.txt`, `soma-re-struct-defaults.py`).

use super::s8d::{Host, Ptr};

#[allow(dead_code)]
pub mod off {
    pub const GFX_MATERIAL: usize = 16;
    pub const GFX_TYPE: usize = 20;
    pub const GFX_COLOR: usize = 40;
    pub const GFX_UV_MIN: usize = 64;
    pub const GFX_UV_MAX: usize = 72;
    pub const GFX_FILE: usize = 152;
    pub const GFX_ALPHA_LIMIT: usize = 160;
    pub const GFX_SIZE: usize = 168;
    pub const FONT_COLOR: usize = 20;
    pub const FONT_SIZE: usize = 36;
    pub const FONT_FILE: usize = 48;
    pub const W_COLOR_BASE: usize = 16;
    pub const W_COLOR_TEXT: usize = 32;
    pub const W_COLOR_DISABLED: usize = 48;
    pub const W_USE_DISABLED_COLOR: usize = 312;
    pub const W_DEFAULT_SIZE: usize = 64;
    pub const W_USE_BG: usize = 72;
    pub const W_GFX_BG: usize = 80;
    pub const W_FONT: usize = 248;
    pub const W_FONT_ALIGN: usize = 316;
    pub const B_USE_FRAME: usize = 320;
    pub const B_FRAME: usize = 328;
    pub const B_USE_IN_FOCUS_GFX: usize = 1920;
    pub const B_GFX_IN_FOCUS: usize = 1928;
    pub const B_USE_IN_FOCUS_COLOR: usize = 2096;
    pub const B_COLOR_IN_FOCUS: usize = 2100;
    pub const B_USE_TRIG_GFX: usize = 2116;
    pub const B_GFX_TRIG: usize = 2120;
    pub const B_USE_TRIG_COLOR: usize = 2288;
    pub const B_COLOR_TRIG: usize = 2292;
    pub const B_USE_TRIG_FOCUS_COLOR: usize = 2308;
    pub const B_COLOR_TRIG_FOCUS: usize = 2312;
    pub const SLIDER_USE_BUTTON: usize = 320;
    pub const SLIDER_GFX_BUTTON: usize = 328;
    pub const SLIDER_BUTTON_SIZE: usize = 496;
    pub const CHECK_BOX_SIZE: usize = 496;
    pub const CHECK_GFX_BOX: usize = 504;
    pub const CHECK_OVERLAY_SIZE: usize = 672;
    pub const CHECK_GFX_OVERLAY: usize = 680;
    pub const MULTI_ARROW_SIZE: usize = 496;
    pub const MULTI_GFX_ARROW_RIGHT: usize = 512;
    pub const MULTI_GFX_ARROW_LEFT: usize = 848;
    pub const WINDOW_FRAME: usize = 320;
    pub const W_USE_HEADER: usize = 1912;
    pub const W_HEADER_TYPE: usize = 1916;
    pub const W_LABEL_OFFSET: usize = 1920;
    pub const W_CAPTION_SIZE_MUL: usize = 1928;
    pub const W_LABEL_FRAME: usize = 1936;
    pub const WINDOW_LABEL_PAD_TOP: usize = 3528;
    pub const WINDOW_LABEL_MIN_WIDTH: usize = 3544;
    pub const WINDOW_PAD_TOP: usize = 3548;
    pub const WINDOW_PAD_RIGHT: usize = 3552;
    pub const WINDOW_PAD_BOTTOM: usize = 3556;
    pub const WINDOW_PAD_LEFT: usize = 3560;
    pub const GAUGE_FRAME: usize = 320;
    pub const GAUGE_USE_FRAME: usize = 1912;
    pub const GAUGE_FILL: usize = 1920;
    pub const GAUGE_ORIENT: usize = 2092;
    pub const GAUGE_PADDING: usize = 2096;
    pub const PARAM_LINE_WIDTH: usize = 16;
    pub const PARAM_LABEL: usize = 24;
    pub const PARAM_ICON_KEY_H: usize = 364;
    pub const PARAM_ICON_MOUSE_H: usize = 368;
}

/// A pointer into a script struct block (4096 bytes, see `new_script_struct`).
#[derive(Clone, Copy)]
pub struct Sd(pub *const u8);

impl Sd {
    fn read<T: Copy>(self, o: usize) -> T {
        // SAFETY: script struct blocks are 4096 bytes and every offset used here lies inside.
        unsafe { self.0.add(o).cast::<T>().read_unaligned() }
    }

    pub fn at(self, o: usize) -> Sd {
        // SAFETY: as `read`.
        Sd(unsafe { self.0.add(o) })
    }

    pub fn i32(self, o: usize) -> i32 {
        self.read(o)
    }

    pub fn f32(self, o: usize) -> f32 {
        self.read(o)
    }

    pub fn bool(self, o: usize) -> bool {
        self.read::<u8>(o) != 0
    }

    pub fn v2(self, o: usize) -> [f32; 2] {
        self.read(o)
    }

    pub fn v3(self, o: usize) -> [f32; 3] {
        self.read(o)
    }

    pub fn col(self, o: usize) -> [f32; 4] {
        self.read(o)
    }

    pub fn ptr(self, o: usize) -> Ptr {
        self.read(o)
    }

    pub fn set_i32(self, o: usize, v: i32) {
        // SAFETY: as `read`; the block is writable script memory.
        unsafe { self.0.add(o).cast_mut().cast::<i32>().write_unaligned(v) }
    }

    pub fn set_f32(self, o: usize, v: f32) {
        // SAFETY: as `set_i32`.
        unsafe { self.0.add(o).cast_mut().cast::<f32>().write_unaligned(v) }
    }

    pub fn set_ptr(self, o: usize, v: Ptr) {
        // SAFETY: as `set_i32`.
        unsafe { self.0.add(o).cast_mut().cast::<usize>().write_unaligned(v) }
    }

    pub fn set_col(self, o: usize, v: [f32; 4]) {
        // SAFETY: as `set_i32`.
        unsafe {
            self.0
                .add(o)
                .cast_mut()
                .cast::<[f32; 4]>()
                .write_unaligned(v)
        }
    }

    pub fn set_v2(self, o: usize, v: [f32; 2]) {
        // SAFETY: as `set_i32`.
        unsafe {
            self.0
                .add(o)
                .cast_mut()
                .cast::<[f32; 2]>()
                .write_unaligned(v)
        }
    }

    /// The interned `tString` an `imgui` struct property points at; empty when null.
    pub fn string(self, h: &dyn Host, o: usize) -> Vec<u8> {
        match self.ptr(o) {
            0 => Vec::new(),
            p => h.string(p),
        }
    }

    pub fn copy_to(self, dst: Sd, o: usize, n: usize) {
        // SAFETY: both blocks hold `o + n` bytes and are distinct script objects.
        unsafe { std::ptr::copy(self.0.add(o), dst.0.add(o).cast_mut(), n) }
    }
}
