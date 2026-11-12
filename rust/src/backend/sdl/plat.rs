//! `cPlatform` SDL half (PlatformSDL.cpp): ticks, clipboard, displays, message box.

use crate::string_ffi::{Sink, emit};
use crate::sys::sdl2::{
    SDL_Delay, SDL_DisplayMode, SDL_GetClipboardText, SDL_GetDesktopDisplayMode,
    SDL_GetDisplayMode, SDL_GetDisplayName, SDL_GetNumDisplayModes, SDL_GetNumVideoDisplays,
    SDL_GetTicks, SDL_HasClipboardText, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
    SDL_MESSAGEBOX_ERROR, SDL_MESSAGEBOX_INFORMATION, SDL_MESSAGEBOX_WARNING,
    SDL_MessageBoxButtonData, SDL_MessageBoxData, SDL_SetClipboardText, SDL_ShowMessageBox,
    SDL_free,
};
use core::ffi::{CStr, c_char, c_int, c_void};
use std::collections::BTreeMap;

fn zero_mode() -> SDL_DisplayMode {
    SDL_DisplayMode {
        format: 0,
        w: 0,
        h: 0,
        refresh_rate: 0,
        driverdata: core::ptr::null_mut(),
    }
}

/// `SDL_BITSPERPIXEL`.
pub fn bits_per_pixel(format: u32) -> i32 {
    ((format >> 8) & 0xFF) as i32
}

type DisplayModes = (SDL_DisplayMode, Vec<SDL_DisplayMode>);

/// Unique `(display, w, h, bpp)` in `VideoComp` order (display, width, height); the first insertion
/// of a key wins, like `std::set`. Per display: its modes with the desktop bpp, then fullscreen desktop `0x0`.
pub fn unique_modes(displays: &[DisplayModes]) -> Vec<(i32, i32, i32, i32)> {
    let mut set: BTreeMap<(i32, i32, i32), i32> = BTreeMap::new();
    for (d, (desktop, modes)) in displays.iter().enumerate() {
        let d = d as i32;
        let want = bits_per_pixel(desktop.format);
        for m in modes {
            if bits_per_pixel(m.format) == want {
                set.entry((d, m.w, m.h)).or_insert(want);
            }
        }
        set.entry((d, 0, 0)).or_insert(want);
    }
    set.into_iter()
        .map(|((d, w, h), bpp)| (d, w, h, bpp))
        .collect()
}

fn read_displays() -> Vec<DisplayModes> {
    let mut out = Vec::new();
    for d in 0..SDL_GetNumVideoDisplays() {
        let mut desktop = zero_mode();
        // SAFETY: valid out pointer.
        unsafe { SDL_GetDesktopDisplayMode(d, &mut desktop) };
        let mut modes = Vec::new();
        for m in 0..SDL_GetNumDisplayModes(d) {
            let mut mode = zero_mode();
            // SAFETY: valid out pointer.
            unsafe { SDL_GetDisplayMode(d, m, &mut mode) };
            modes.push(mode);
        }
        out.push((desktop, modes));
    }
    out
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_ticks() -> u32 {
    SDL_GetTicks()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_delay(ms: u32) {
    SDL_Delay(ms);
}

/// # Safety
/// `text` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_clipboard_set(text: *const c_char) {
    // SAFETY: caller contract.
    unsafe { SDL_SetClipboardText(text) };
}

/// Emits the clipboard text; false when there is none.
///
/// # Safety
/// `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_clipboard_get(sink: Sink<u8>, ctx: *mut c_void) -> bool {
    if SDL_HasClipboardText() == 0 {
        return false;
    }
    // SAFETY: SDL returns a NUL-terminated string or null, released with SDL_free.
    unsafe {
        let clip = SDL_GetClipboardText();
        if clip.is_null() {
            return false;
        }
        emit(sink, ctx, CStr::from_ptr(clip).to_bytes());
        SDL_free(clip.cast());
    }
    true
}

/// # Safety
/// `w` and `h` are valid out pointers.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_display_resolution(display: c_int, w: *mut c_int, h: *mut c_int) {
    let mut desktop = zero_mode();
    // SAFETY: valid out pointer.
    unsafe { SDL_GetDesktopDisplayMode(display, &mut desktop) };
    // SAFETY: caller contract.
    unsafe {
        *w = desktop.w;
        *h = desktop.h;
    }
}

type ModeCb = unsafe extern "C" fn(*mut c_void, c_int, c_int, c_int, c_int);

/// Calls `cb(ctx, display, w, h, bpp)` for each unique mode in order.
///
/// # Safety
/// `cb` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_video_modes(cb: ModeCb, ctx: *mut c_void) {
    for (d, w, h, bpp) in unique_modes(&read_displays()) {
        // SAFETY: caller contract.
        unsafe { cb(ctx, d, w, h, bpp) };
    }
}

/// Emits the display name; false when SDL has none.
///
/// # Safety
/// `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_display_name(
    display: c_int,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: SDL returns a NUL-terminated string or null.
    unsafe {
        let name = SDL_GetDisplayName(display);
        if name.is_null() {
            return false;
        }
        emit(sink, ctx, CStr::from_ptr(name).to_bytes());
    }
    true
}

/// `kind`: `eMsgBoxType` 0 info, 1 error, 2 warning.
///
/// # Safety
/// `caption` and `message` are NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_message_box(
    kind: u32,
    caption: *const c_char,
    message: *const c_char,
) {
    let flags = match kind {
        0 => SDL_MESSAGEBOX_INFORMATION,
        1 => SDL_MESSAGEBOX_ERROR,
        _ => SDL_MESSAGEBOX_WARNING,
    };
    let buttons = [SDL_MessageBoxButtonData {
        flags: SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
        buttonid: 1,
        text: c"Dismiss".as_ptr(),
    }];
    let data = SDL_MessageBoxData {
        flags,
        window: core::ptr::null_mut(),
        title: caption,
        message,
        numbuttons: 1,
        buttons: buttons.as_ptr(),
        colorScheme: core::ptr::null(),
    };
    let mut pressed: c_int = -1;
    // SAFETY: `data` and `buttons` outlive the call.
    unsafe { SDL_ShowMessageBox(&data, &mut pressed) };
}
