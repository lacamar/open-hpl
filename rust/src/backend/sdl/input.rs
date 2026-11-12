//! SDL side of the input classes: event polling, `SDL_Event` flattening, mouse and gamepad handles.

use crate::input::{
    Event, KIND_KEY, KIND_MOUSE_BUTTON, KIND_MOUSE_MOTION, KIND_PAD_AXIS, KIND_PAD_BUTTON,
    KIND_PAD_REMOVED, KIND_TEXT, KIND_WHEEL,
};
use crate::string_ffi::{Sink, emit};
use crate::sys::sdl2::{
    SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, SDL_CONTROLLERAXISMOTION,
    SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP, SDL_CONTROLLERDEVICEADDED,
    SDL_CONTROLLERDEVICEREMOVED, SDL_Event, SDL_GameController, SDL_GameControllerClose,
    SDL_GameControllerGetJoystick, SDL_GameControllerName, SDL_GameControllerOpen, SDL_GetModState,
    SDL_GetRelativeMouseState, SDL_Haptic, SDL_HapticClose, SDL_HapticOpenFromJoystick,
    SDL_HapticRumbleInit, SDL_HapticRumblePlay, SDL_HapticRumbleStop, SDL_INIT_GAMECONTROLLER,
    SDL_InitSubSystem, SDL_JoystickInstanceID, SDL_KEYDOWN, SDL_KEYUP, SDL_MOUSEBUTTONDOWN,
    SDL_MOUSEBUTTONUP, SDL_MOUSEMOTION, SDL_MOUSEWHEEL, SDL_NumJoysticks, SDL_PollEvent,
    SDL_PumpEvents, SDL_QUIT, SDL_TEXTINPUT,
};
use core::ffi::{CStr, c_int, c_void};

pub const POLL_QUIT: u32 = 1;
pub const POLL_PLUGGED: u32 = 2;
pub const POLL_REMOVED: u32 = 4;

/// Flattens one event as `OhplConvertEvents`: unknown types give a zeroed event.
pub fn flatten(e: &SDL_Event) -> Event {
    let mut o = Event {
        kind: 0,
        sym: 0,
        key_mod: 0,
        state: 0,
        button: 0,
        axis_value: 0,
        x: 0,
        y: 0,
        xrel: 0,
        yrel: 0,
        wheel_y: 0,
        which: 0,
        text: [0; 32],
    };
    // SAFETY: every arm reads the union member selected by the event's type tag.
    unsafe {
        match e.type_ {
            SDL_KEYDOWN | SDL_KEYUP => {
                o.kind = KIND_KEY;
                o.sym = e.key.keysym.sym;
                o.key_mod = i32::from(e.key.keysym.mod_);
                o.state = e.key.state;
            }
            SDL_TEXTINPUT => {
                o.kind = KIND_TEXT;
                for (d, s) in o.text.iter_mut().zip(e.text.text.iter()) {
                    *d = *s;
                }
            }
            SDL_MOUSEMOTION => {
                o.kind = KIND_MOUSE_MOTION;
                o.x = e.motion.x;
                o.y = e.motion.y;
                o.xrel = e.motion.xrel;
                o.yrel = e.motion.yrel;
            }
            SDL_MOUSEBUTTONDOWN | SDL_MOUSEBUTTONUP => {
                o.kind = KIND_MOUSE_BUTTON;
                o.button = e.button.button;
                o.state = u8::from(e.type_ == SDL_MOUSEBUTTONDOWN);
            }
            SDL_MOUSEWHEEL => {
                o.kind = KIND_WHEEL;
                o.wheel_y = e.wheel.y;
            }
            SDL_CONTROLLERAXISMOTION => {
                o.kind = KIND_PAD_AXIS;
                o.which = e.caxis.which;
                o.button = e.caxis.axis;
                o.axis_value = e.caxis.value;
            }
            SDL_CONTROLLERBUTTONDOWN | SDL_CONTROLLERBUTTONUP => {
                o.kind = KIND_PAD_BUTTON;
                o.which = e.cbutton.which;
                o.button = e.cbutton.button;
                o.state = e.cbutton.state;
            }
            SDL_CONTROLLERDEVICEREMOVED => {
                o.kind = KIND_PAD_REMOVED;
                o.which = e.cdevice.which;
            }
            _ => {}
        }
    }
    o
}

/// Routes one polled event: hotplug is flagged, `SDL_QUIT` is flagged and dropped, everything else
/// is kept unless `rejected` says so. Returns whether to keep it and the flags raised.
pub fn route(e: &SDL_Event, rejected: impl FnOnce(&SDL_Event) -> bool) -> (bool, u32) {
    let mut flags = 0;
    // SAFETY: `type_` is the shared tag of every member.
    let ty = unsafe { e.type_ };
    if ty == SDL_CONTROLLERDEVICEADDED {
        flags |= POLL_PLUGGED;
    } else if ty == SDL_CONTROLLERDEVICEREMOVED {
        flags |= POLL_REMOVED;
    }
    if ty == SDL_QUIT {
        return (false, flags | POLL_QUIT);
    }
    (!rejected(e), flags)
}

type Filter = Option<unsafe extern "C" fn(*mut c_void, *const SDL_Event) -> bool>;
type Push = unsafe extern "C" fn(*mut c_void, *const SDL_Event);

/// Drains `SDL_PollEvent`, calling `push` for each kept event in order; returns `POLL_*` flags.
/// `filter` (may be null) returns true to drop an event.
///
/// # Safety
/// `filter` and `push` accept `ctx`; neither re-enters this module.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_poll(filter: Filter, push: Push, ctx: *mut c_void) -> u32 {
    // SAFETY: an all-zero SDL_Event is valid.
    let mut ev: SDL_Event = unsafe { core::mem::zeroed() };
    let mut flags = 0;
    // SAFETY: valid out pointer.
    while unsafe { SDL_PollEvent(&mut ev) } != 0 {
        let (keep, f) = route(&ev, |e| {
            // SAFETY: caller contract.
            filter.is_some_and(|f| unsafe { f(ctx, e) })
        });
        flags |= f;
        if keep {
            // SAFETY: caller contract.
            unsafe { push(ctx, &ev) };
        }
    }
    flags
}

/// Routes an already-queued (injected) event through the same filter; true when it is kept.
///
/// # Safety
/// `filter` accepts `ctx`; `ev` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_filter_keeps(
    filter: Filter,
    ctx: *mut c_void,
    ev: *const SDL_Event,
) -> bool {
    // SAFETY: caller contract.
    !filter.is_some_and(|f| unsafe { f(ctx, ev) })
}

/// # Safety
/// `ev` has `n` events, `out` has room for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_convert_events(ev: *const SDL_Event, n: usize, out: *mut Event) {
    for i in 0..n {
        // SAFETY: caller contract.
        unsafe { out.add(i).write(flatten(&*ev.add(i))) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_input_init() {
    // SAFETY: plain SDL call.
    unsafe { SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_joystick_count() -> c_int {
    SDL_NumJoysticks()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_mod_state() -> c_int {
    SDL_GetModState()
}

/// Ten pump + relative-state reads that discard events queued before the first mouse update.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_mouse_flush() {
    for _ in 0..10 {
        let (mut x, mut y) = (0, 0);
        // SAFETY: plain SDL call.
        unsafe { SDL_PumpEvents() };
        // SAFETY: valid out pointers.
        unsafe { SDL_GetRelativeMouseState(&mut x, &mut y) };
    }
}

/// # Safety
/// `x` and `y` are valid out pointers.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_mouse_relative(x: *mut c_int, y: *mut c_int) {
    // SAFETY: caller contract.
    unsafe { SDL_GetRelativeMouseState(x, y) };
}

pub struct Pad {
    handle: *mut SDL_GameController,
    haptic: *mut SDL_Haptic,
    instance: i32,
}

#[repr(C)]
pub struct PadInfo {
    pub instance: i32,
    pub buttons: usize,
    pub axes: usize,
}

/// Opens game controller `index`; `info` is filled even when the open fails (zeros).
///
/// # Safety
/// `info` valid out pointer; `sink` accepts `ctx`. Release with `ohpl_l1_pad_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_pad_open(
    index: c_int,
    info: *mut PadInfo,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> *mut Pad {
    // SAFETY: SDL handles are used only while open; names are NUL-terminated.
    unsafe {
        let handle = SDL_GameControllerOpen(index);
        let mut pad = Pad {
            handle,
            haptic: core::ptr::null_mut(),
            instance: 0,
        };
        let mut i = PadInfo {
            instance: 0,
            buttons: 0,
            axes: 0,
        };
        if !handle.is_null() {
            let joy = SDL_GameControllerGetJoystick(handle);
            pad.instance = SDL_JoystickInstanceID(joy);
            i.instance = pad.instance;
            let name = SDL_GameControllerName(handle);
            if !name.is_null() {
                emit(sink, ctx, CStr::from_ptr(name).to_bytes());
            }
            i.buttons = SDL_CONTROLLER_BUTTON_MAX as usize;
            i.axes = SDL_CONTROLLER_AXIS_MAX as usize;
            pad.haptic = SDL_HapticOpenFromJoystick(joy);
            if SDL_HapticRumbleInit(pad.haptic) != 0 {
                if !pad.haptic.is_null() {
                    SDL_HapticClose(pad.haptic);
                }
                pad.haptic = core::ptr::null_mut();
            }
        }
        *info = i;
        Box::into_raw(Box::new(pad))
    }
}

/// # Safety
/// `p` from `ohpl_l1_pad_open`, freed once (or null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_pad_free(p: *mut Pad) {
    if p.is_null() {
        return;
    }
    // SAFETY: caller contract.
    let pad = unsafe { Box::from_raw(p) };
    // SAFETY: handles were opened by `ohpl_l1_pad_open` and are closed once.
    unsafe {
        if !pad.haptic.is_null() {
            SDL_HapticRumbleStop(pad.haptic);
            SDL_HapticClose(pad.haptic);
        }
        if !pad.handle.is_null() {
            SDL_GameControllerClose(pad.handle);
        }
    }
}

/// # Safety
/// `p` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_pad_rumble(p: *mut Pad, value: f32, millisec: i32) {
    // SAFETY: caller contract.
    let Some(pad) = (unsafe { p.as_ref() }).filter(|p| !p.haptic.is_null()) else {
        return;
    };
    // SAFETY: the haptic handle is open.
    unsafe {
        if value > 0.0 {
            SDL_HapticRumblePlay(pad.haptic, value, millisec as u32);
        } else {
            SDL_HapticRumbleStop(pad.haptic);
        }
    }
}
