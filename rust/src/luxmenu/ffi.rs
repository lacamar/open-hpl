//! C ABI of the profile window rules, mirrored in `amnesia/src/game/RustLuxMenu.h`.

use super::profile::{self as pf, Date};

fn units<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-unit buffer.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn put<T>(p: *mut T, v: T) {
    if !p.is_null() {
        // SAFETY: non-null out pointers are writable.
        unsafe { p.write(v) }
    }
}

// ---- profile --------------------------------------------------------------------------------

/// # Safety
/// `dates` holds `n` dates and `out` has room for `n` indices.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga7a_pf_sort(dates: *const Date, n: usize, out: *mut u32) {
    let d: &[Date] = if n == 0 || dates.is_null() {
        &[]
    } else {
        // SAFETY: per the contract.
        unsafe { std::slice::from_raw_parts(dates, n) }
    };
    for (i, v) in pf::sort_order(d).into_iter().enumerate() {
        // SAFETY: `out` has room for `n` indices.
        unsafe { out.add(i).write(v) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_trim(p: *const u32, n: usize, start: *mut usize, end: *mut usize) {
    let (s, e) = pf::trim(units(p, n));
    put(start, s);
    put(end, e);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_illegal(len: *mut usize) -> *const u32 {
    put(len, pf::ILLEGAL_CHARS.len());
    pf::ILLEGAL_CHARS.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_name_max() -> i32 {
    pf::NAME_MAX
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_select(num: i32, count: usize) -> i32 {
    pf::select_target(num, count).map_or(-1, |n| n as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_delete_plan(
    selected: i32,
    in_use: i32,
    config_differs: i32,
    new_selection: *mut i32,
) -> i32 {
    let p = pf::delete_plan(selected, in_use != 0, config_differs != 0);
    put(new_selection, p.new_selection);
    i32::from(p.reset_profile) | (i32::from(p.remove_config) << 1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_activate(item_num: i32) -> i32 {
    i32::from(pf::activate(item_num) == pf::Activate::CreateWindow)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7a_pf_ui_press(
    secondary: i32,
    list_locked: i32,
    name_visible: i32,
) -> i32 {
    i32::from(pf::window_ui_press(
        secondary != 0,
        list_locked != 0,
        name_visible != 0,
    ))
}
