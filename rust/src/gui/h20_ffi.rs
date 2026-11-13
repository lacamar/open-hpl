//! C ABI of the H20 widgets, declared in `gui/RustGuiH20.h`.
//!
//! `vt` is a valid host table and widget state pointers are the structs embedded in the C++
//! widgets; a null state makes a call a no-op returning the zero value.

use super::h19_widget::Base;
use super::h20_host::{HostVt, Hx};
use super::h20_list::{self as l, ListBase};
use super::set::{Msg, W};
use crate::types::Vector3f;

/// # Safety
/// `p` is null or valid for reads.
unsafe fn rd_msg(p: *const Msg) -> Msg {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.copied().unwrap_or(Msg::zero())
}

macro_rules! lbf {
    ($name:ident, |$hx:ident, $lb:ident $(, $arg:ident: $t:ty)*| -> $r:ty $body:block) => {
        /// # Safety
        /// `vt` is null or valid and `lb` is null or an initialised `ListBase`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(vt: *const HostVt, $lb: *mut ListBase $(, $arg: $t)*) -> $r {
            if $lb.is_null() {
                return Default::default();
            }
            // SAFETY: caller contract.
            let $hx = unsafe { Hx::new(vt) };
            // SAFETY: caller contract.
            unsafe { $body }
        }
    };
    ($name:ident, |$hx:ident, $lb:ident $(, $arg:ident: $t:ty)*| $body:expr) => {
        /// # Safety
        /// `vt` is null or valid and `lb` is null or an initialised `ListBase`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(vt: *const HostVt, $lb: *mut ListBase $(, $arg: $t)*) {
            if $lb.is_null() {
                return;
            }
            // SAFETY: caller contract.
            let $hx = unsafe { Hx::new(vt) };
            // SAFETY: caller contract.
            unsafe { $body }
        }
    };
}

/// # Safety
/// `lb` is uninitialised storage of a `ListBase`, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_lb_init(
    vt: *const HostVt,
    lb: *mut ListBase,
    this: W,
    container: usize,
    base: *mut Base,
    kind: i32,
) {
    if !lb.is_null() {
        // SAFETY: caller contract.
        unsafe { l::init(Hx::new(vt), lb, this, container, base, kind) };
    }
}

/// # Safety
/// `lb` is null or initialised and not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_lb_free(lb: *mut ListBase) {
    if !lb.is_null() {
        // SAFETY: caller contract.
        unsafe { l::free(lb) };
    }
}

lbf!(ohpl_h20_lb_destroy_slider, |hx, lb| l::destroy_slider(
    hx, lb
));
lbf!(ohpl_h20_lb_destroy_columns, |hx, lb| l::destroy_columns(
    hx, lb
));
lbf!(
    ohpl_h20_lb_set_selected_item,
    |hx, lb, x: i32, move_list: bool, cb: bool, ty: i32, clear: bool| l::set_selected_item(
        hx, lb, x, move_list, cb, ty, clear
    )
);
lbf!(ohpl_h20_lb_selected_item, |hx, lb| -> i32 {
    let _ = hx;
    l::selected_item(lb)
});
lbf!(ohpl_h20_lb_multi_selection, |hx, lb, idx: i32| -> i32 {
    let _ = hx;
    l::multi_selection(lb, idx)
});
lbf!(ohpl_h20_lb_multi_selection_num, |hx, lb| -> i32 {
    let _ = hx;
    l::multi_selection_num(lb)
});
lbf!(ohpl_h20_lb_clear_selection, |hx, lb| l::clear_selection(
    hx, lb
));
lbf!(
    ohpl_h20_lb_update_properties,
    |hx, lb| l::update_properties(hx, lb)
);
lbf!(ohpl_h20_lb_on_change_size, |hx, lb| l::on_change_size(
    hx, lb
));
lbf!(ohpl_h20_lb_on_init, |hx, lb| l::on_init(hx, lb));
lbf!(ohpl_h20_lb_add_item_to_selection, |hx, lb, it: usize| {
    l::add_item_to_selection(hx, lb, it)
});
lbf!(
    ohpl_h20_lb_remove_item_from_selection,
    |hx, lb, it: usize| l::remove_item_from_selection(hx, lb, it)
);
lbf!(ohpl_h20_lb_on_load_graphics, |hx, lb| l::on_load_graphics(
    hx, lb
));
lbf!(ohpl_h20_lb_on_draw, |hx, lb| l::on_draw(hx, lb));
lbf!(ohpl_h20_lb_on_draw_after_clip, |hx, lb, clip: usize| {
    l::on_draw_after_clip(hx, lb, clip)
});
lbf!(
    ohpl_h20_lb_set_cursor_pos,
    |hx, lb, pos: i32, move_list: bool, clamp: bool| l::set_cursor_pos(
        hx, lb, pos, move_list, clamp
    )
);
lbf!(ohpl_h20_lb_on_mouse_down, |hx, lb, d: *const Msg| -> bool {
    l::on_mouse_down(hx, lb, &rd_msg(d))
});
lbf!(ohpl_h20_lb_on_mouse_double_click, |hx,
                                         lb,
                                         d: *const Msg|
 -> bool {
    l::on_mouse_double_click(hx, lb, &rd_msg(d))
});
lbf!(ohpl_h20_lb_on_key_press, |hx, lb, d: *const Msg| -> bool {
    l::on_key_press(hx, lb, &rd_msg(d))
});
lbf!(ohpl_h20_lb_on_ui_arrow_press, |hx,
                                     lb,
                                     d: *const Msg|
 -> bool {
    l::on_ui_arrow_press(hx, lb, &rd_msg(d))
});
lbf!(ohpl_h20_lb_on_ui_button_press, |hx,
                                      lb,
                                      d: *const Msg|
 -> bool {
    l::on_ui_button_press(hx, lb, &rd_msg(d))
});
lbf!(ohpl_h20_lb_on_ui_button_release, |hx, lb| -> bool {
    l::on_ui_button_release(hx, lb)
});
lbf!(ohpl_h20_lb_on_ui_button_double_press, |hx,
                                             lb,
                                             d: *const Msg|
 -> bool {
    l::on_ui_button_double_press(hx, lb, &rd_msg(d))
});
lbf!(
    ohpl_h20_lb_add_column,
    |hx, lb, text: *const u32, len: usize, index: i32, align: i32| l::add_column(
        hx,
        lb,
        (text, len),
        index,
        align
    )
);
lbf!(
    ohpl_h20_lb_set_column_width,
    |hx, lb, idx: i32, width: f32| l::set_column_width(hx, lb, idx, width)
);

/// # Safety
/// `out` is writable for 3 floats; others as the `lb` functions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_lb_item_start_pos(
    vt: *const HostVt,
    lb: *mut ListBase,
    out: *mut Vector3f,
) {
    if lb.is_null() || out.is_null() {
        return;
    }
    // SAFETY: caller contract.
    unsafe { out.write(l::item_start_pos(Hx::new(vt), lb)) };
}
