//! Raw SDL2 bindings (through sdl2-compat): the functions, constants and event structs the engine uses.
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

use core::ffi::{CStr, c_char, c_int, c_ulong, c_void};

use core::mem::offset_of;

#[repr(C)]
pub struct SDL_GameController {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SDL_Haptic {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SDL_Joystick {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SDL_MessageBoxColorScheme {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SDL_Thread {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SDL_Window {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SDL_mutex {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_Rect {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_Keysym {
    pub scancode: c_int,
    pub sym: i32,
    pub mod_: u16,
    pub unused: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_CommonEvent {
    pub type_: u32,
    pub timestamp: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_KeyboardEvent {
    pub type_: u32,
    pub timestamp: u32,
    pub windowID: u32,
    pub state: u8,
    pub repeat: u8,
    pub padding2: u8,
    pub padding3: u8,
    pub keysym: SDL_Keysym,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_TextInputEvent {
    pub type_: u32,
    pub timestamp: u32,
    pub windowID: u32,
    pub text: [c_char; 32],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_MouseMotionEvent {
    pub type_: u32,
    pub timestamp: u32,
    pub windowID: u32,
    pub which: u32,
    pub state: u32,
    pub x: i32,
    pub y: i32,
    pub xrel: i32,
    pub yrel: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_MouseButtonEvent {
    pub type_: u32,
    pub timestamp: u32,
    pub windowID: u32,
    pub which: u32,
    pub button: u8,
    pub state: u8,
    pub clicks: u8,
    pub padding1: u8,
    pub x: i32,
    pub y: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_MouseWheelEvent {
    pub type_: u32,
    pub timestamp: u32,
    pub windowID: u32,
    pub which: u32,
    pub x: i32,
    pub y: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_ControllerAxisEvent {
    pub type_: u32,
    pub timestamp: u32,
    pub which: i32,
    pub axis: u8,
    pub padding1: u8,
    pub padding2: u8,
    pub padding3: u8,
    pub value: i16,
    pub padding4: u16,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_ControllerButtonEvent {
    pub type_: u32,
    pub timestamp: u32,
    pub which: i32,
    pub button: u8,
    pub state: u8,
    pub padding1: u8,
    pub padding2: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_ControllerDeviceEvent {
    pub type_: u32,
    pub timestamp: u32,
    pub which: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_DisplayMode {
    pub format: u32,
    pub w: c_int,
    pub h: c_int,
    pub refresh_rate: c_int,
    pub driverdata: *mut c_void,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_MessageBoxButtonData {
    pub flags: u32,
    pub buttonid: c_int,
    pub text: *const c_char,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SDL_MessageBoxData {
    pub flags: u32,
    pub window: *mut SDL_Window,
    pub title: *const c_char,
    pub message: *const c_char,
    pub numbuttons: c_int,
    pub buttons: *const SDL_MessageBoxButtonData,
    pub colorScheme: *const SDL_MessageBoxColorScheme,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub union SDL_Event {
    pub type_: u32,
    pub common: SDL_CommonEvent,
    pub key: SDL_KeyboardEvent,
    pub text: SDL_TextInputEvent,
    pub motion: SDL_MouseMotionEvent,
    pub button: SDL_MouseButtonEvent,
    pub wheel: SDL_MouseWheelEvent,
    pub caxis: SDL_ControllerAxisEvent,
    pub cbutton: SDL_ControllerButtonEvent,
    pub cdevice: SDL_ControllerDeviceEvent,
    pub padding: [u8; 56],
    pub _align: [u64; 0],
}

pub type SDL_ThreadFunction = unsafe extern "C" fn(data: *mut c_void) -> c_int;

pub const fn SDL_WINDOWPOS_CENTERED_DISPLAY(display: u32) -> u32 {
    SDL_WINDOWPOS_CENTERED_MASK | display
}

pub const SDL_QUIT: u32 = 0x100;
pub const SDL_KEYDOWN: u32 = 0x300;
pub const SDL_KEYUP: u32 = 0x301;
pub const SDL_TEXTINPUT: u32 = 0x303;
pub const SDL_MOUSEMOTION: u32 = 0x400;
pub const SDL_MOUSEBUTTONDOWN: u32 = 0x401;
pub const SDL_MOUSEBUTTONUP: u32 = 0x402;
pub const SDL_MOUSEWHEEL: u32 = 0x403;
pub const SDL_CONTROLLERAXISMOTION: u32 = 0x650;
pub const SDL_CONTROLLERBUTTONDOWN: u32 = 0x651;
pub const SDL_CONTROLLERBUTTONUP: u32 = 0x652;
pub const SDL_CONTROLLERDEVICEADDED: u32 = 0x653;
pub const SDL_CONTROLLERDEVICEREMOVED: u32 = 0x654;
pub const SDL_PRESSED: u8 = 1;
pub const SDL_RELEASED: u8 = 0;
pub const SDL_BUTTON_LEFT: u8 = 1;
pub const SDL_BUTTON_MIDDLE: u8 = 2;
pub const SDL_BUTTON_RIGHT: u8 = 3;
pub const SDL_FALSE: c_int = 0;
pub const SDL_TRUE: c_int = 1;
pub const SDL_ENABLE: c_int = 1;
pub const SDL_DISABLE: c_int = 0;
pub const SDL_QUERY: c_int = -1;
pub const SDL_INIT_TIMER: u32 = 1;
pub const SDL_INIT_VIDEO: u32 = 32;
pub const SDL_INIT_JOYSTICK: u32 = 0x200;
pub const SDL_INIT_GAMECONTROLLER: u32 = 0x2000;
pub const SDL_WINDOW_FULLSCREEN: u32 = 1;
pub const SDL_WINDOW_OPENGL: u32 = 2;
pub const SDL_WINDOW_SHOWN: u32 = 4;
pub const SDL_WINDOW_HIDDEN: u32 = 8;
pub const SDL_WINDOW_FULLSCREEN_DESKTOP: u32 = 0x1001;
pub const SDL_WINDOW_MOUSE_FOCUS: u32 = 0x400;
pub const SDL_WINDOW_INPUT_FOCUS: u32 = 0x200;
pub const SDL_WINDOW_VULKAN: u32 = 0x10000000;
pub const SDL_WINDOWPOS_CENTERED_MASK: u32 = 0x2FFF0000;
pub const SDL_GL_RED_SIZE: c_int = 0;
pub const SDL_GL_GREEN_SIZE: c_int = 1;
pub const SDL_GL_BLUE_SIZE: c_int = 2;
pub const SDL_GL_ALPHA_SIZE: c_int = 3;
pub const SDL_GL_DEPTH_SIZE: c_int = 6;
pub const SDL_GL_STENCIL_SIZE: c_int = 7;
pub const SDL_GL_DOUBLEBUFFER: c_int = 5;
pub const SDL_GL_MULTISAMPLEBUFFERS: c_int = 13;
pub const SDL_GL_MULTISAMPLESAMPLES: c_int = 14;
pub const SDL_GL_ACCELERATED_VISUAL: c_int = 15;
pub const SDL_MESSAGEBOX_ERROR: u32 = 16;
pub const SDL_MESSAGEBOX_WARNING: u32 = 32;
pub const SDL_MESSAGEBOX_INFORMATION: u32 = 64;
pub const SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT: u32 = 1;
pub const SDL_THREAD_PRIORITY_LOW: c_int = 0;
pub const SDL_THREAD_PRIORITY_NORMAL: c_int = 1;
pub const SDL_THREAD_PRIORITY_HIGH: c_int = 2;
pub const SDL_CONTROLLER_AXIS_MAX: c_int = 6;
pub const SDL_CONTROLLER_BUTTON_MAX: c_int = 21;
pub const KMOD_CTRL: u16 = 192;
pub const KMOD_SHIFT: u16 = 3;
pub const KMOD_GUI: u16 = 0xC00;
pub const SDLK_UNKNOWN: i32 = 0;
pub const SDLK_q: i32 = 113;
pub const SDL_HINT_NO_SIGNAL_HANDLERS: &CStr = c"SDL_NO_SIGNAL_HANDLERS";
pub const SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES: &CStr = c"SDL_VIDEO_MAC_FULLSCREEN_SPACES";

unsafe extern "C" {
    pub fn SDL_CreateMutex() -> *mut SDL_mutex;
    pub fn SDL_CreateThread(
        fn_: Option<SDL_ThreadFunction>,
        name: *const c_char,
        data: *mut c_void,
    ) -> *mut SDL_Thread;
    pub fn SDL_CreateWindow(
        title: *const c_char,
        x: c_int,
        y: c_int,
        w: c_int,
        h: c_int,
        flags: u32,
    ) -> *mut SDL_Window;
    pub safe fn SDL_Delay(ms: u32);
    pub fn SDL_DestroyMutex(mutex: *mut SDL_mutex);
    pub fn SDL_DestroyWindow(window: *mut SDL_Window);
    pub fn SDL_DisableScreenSaver();
    pub fn SDL_EnableScreenSaver();
    pub fn SDL_EventState(type_: u32, state: c_int) -> u8;
    pub fn SDL_GL_CreateContext(window: *mut SDL_Window) -> *mut c_void;
    pub fn SDL_GL_DeleteContext(context: *mut c_void);
    pub fn SDL_GL_GetCurrentContext() -> *mut c_void;
    pub fn SDL_GL_GetProcAddress(proc: *const c_char) -> *mut c_void;
    pub fn SDL_GL_SetAttribute(attr: c_int, value: c_int) -> c_int;
    pub fn SDL_GL_SetSwapInterval(interval: c_int) -> c_int;
    pub fn SDL_GL_SwapWindow(window: *mut SDL_Window);
    pub fn SDL_GameControllerClose(gamecontroller: *mut SDL_GameController);
    pub fn SDL_GameControllerGetJoystick(
        gamecontroller: *mut SDL_GameController,
    ) -> *mut SDL_Joystick;
    pub fn SDL_GameControllerName(gamecontroller: *mut SDL_GameController) -> *const c_char;
    pub fn SDL_GameControllerOpen(joystick_index: c_int) -> *mut SDL_GameController;
    pub fn SDL_GetClipboardText() -> *mut c_char;
    pub fn SDL_GetClosestDisplayMode(
        displayIndex: c_int,
        mode: *const SDL_DisplayMode,
        closest: *mut SDL_DisplayMode,
    ) -> *mut SDL_DisplayMode;
    pub fn SDL_GetCurrentVideoDriver() -> *const c_char;
    pub fn SDL_GetDesktopDisplayMode(displayIndex: c_int, mode: *mut SDL_DisplayMode) -> c_int;
    pub fn SDL_GetDisplayMode(
        displayIndex: c_int,
        modeIndex: c_int,
        mode: *mut SDL_DisplayMode,
    ) -> c_int;
    pub fn SDL_GetDisplayName(displayIndex: c_int) -> *const c_char;
    pub fn SDL_GetError() -> *const c_char;
    pub fn SDL_GetKeyFromName(name: *const c_char) -> i32;
    pub safe fn SDL_GetModState() -> c_int;
    pub safe fn SDL_GetNumDisplayModes(displayIndex: c_int) -> c_int;
    pub safe fn SDL_GetNumVideoDisplays() -> c_int;
    pub fn SDL_GetRelativeMouseState(x: *mut c_int, y: *mut c_int) -> u32;
    pub fn SDL_GetScancodeFromKey(key: i32) -> c_int;
    pub fn SDL_GetThreadID(thread: *mut SDL_Thread) -> c_ulong;
    pub safe fn SDL_GetTicks() -> u32;
    pub fn SDL_GetWindowDisplayIndex(window: *mut SDL_Window) -> c_int;
    pub fn SDL_GetWindowFlags(window: *mut SDL_Window) -> u32;
    pub fn SDL_GetWindowSize(window: *mut SDL_Window, w: *mut c_int, h: *mut c_int);
    pub fn SDL_HapticClose(haptic: *mut SDL_Haptic);
    pub fn SDL_HapticOpenFromJoystick(joystick: *mut SDL_Joystick) -> *mut SDL_Haptic;
    pub fn SDL_HapticRumbleInit(haptic: *mut SDL_Haptic) -> c_int;
    pub fn SDL_HapticRumblePlay(haptic: *mut SDL_Haptic, strength: f32, length: u32) -> c_int;
    pub fn SDL_HapticRumbleStop(haptic: *mut SDL_Haptic) -> c_int;
    pub safe fn SDL_HasClipboardText() -> c_int;
    pub fn SDL_Init(flags: u32) -> c_int;
    pub fn SDL_InitSubSystem(flags: u32) -> c_int;
    pub safe fn SDL_IsTextInputActive() -> c_int;
    pub fn SDL_JoystickInstanceID(joystick: *mut SDL_Joystick) -> i32;
    pub fn SDL_JoystickName(joystick: *mut SDL_Joystick) -> *const c_char;
    pub fn SDL_JoystickNumAxes(joystick: *mut SDL_Joystick) -> c_int;
    pub fn SDL_JoystickNumBalls(joystick: *mut SDL_Joystick) -> c_int;
    pub fn SDL_JoystickNumButtons(joystick: *mut SDL_Joystick) -> c_int;
    pub fn SDL_JoystickNumHats(joystick: *mut SDL_Joystick) -> c_int;
    pub fn SDL_JoystickOpen(device_index: c_int) -> *mut SDL_Joystick;
    pub fn SDL_LockMutex(mutex: *mut SDL_mutex) -> c_int;
    pub safe fn SDL_NumJoysticks() -> c_int;
    pub fn SDL_PollEvent(event: *mut SDL_Event) -> c_int;
    pub fn SDL_PumpEvents();
    pub fn SDL_PushEvent(event: *mut SDL_Event) -> c_int;
    pub fn SDL_Quit();
    pub fn SDL_QuitSubSystem(flags: u32);
    pub fn SDL_SetClipboardText(text: *const c_char) -> c_int;
    pub fn SDL_SetHint(name: *const c_char, value: *const c_char) -> c_int;
    pub fn SDL_SetRelativeMouseMode(enabled: c_int) -> c_int;
    pub fn SDL_SetThreadPriority(priority: c_int) -> c_int;
    pub fn SDL_SetWindowDisplayMode(window: *mut SDL_Window, mode: *const SDL_DisplayMode)
    -> c_int;
    pub fn SDL_SetWindowFullscreen(window: *mut SDL_Window, flags: u32) -> c_int;
    pub fn SDL_SetWindowGrab(window: *mut SDL_Window, grabbed: c_int);
    pub fn SDL_SetWindowSize(window: *mut SDL_Window, w: c_int, h: c_int);
    pub fn SDL_SetWindowTitle(window: *mut SDL_Window, title: *const c_char);
    pub fn SDL_ShowCursor(toggle: c_int) -> c_int;
    pub fn SDL_ShowMessageBox(
        messageboxdata: *const SDL_MessageBoxData,
        buttonid: *mut c_int,
    ) -> c_int;
    pub fn SDL_StartTextInput();
    pub fn SDL_StopTextInput();
    pub fn SDL_UnlockMutex(mutex: *mut SDL_mutex) -> c_int;
    pub fn SDL_WaitThread(thread: *mut SDL_Thread, status: *mut c_int);
    pub fn SDL_free(mem: *mut c_void);
}

const _: () = {
    assert!(size_of::<SDL_Rect>() == 16);
    assert!(align_of::<SDL_Rect>() == 4);
    assert!(offset_of!(SDL_Rect, x) == 0);
    assert!(offset_of!(SDL_Rect, y) == 4);
    assert!(offset_of!(SDL_Rect, w) == 8);
    assert!(offset_of!(SDL_Rect, h) == 12);
    assert!(size_of::<SDL_Keysym>() == 16);
    assert!(align_of::<SDL_Keysym>() == 4);
    assert!(offset_of!(SDL_Keysym, scancode) == 0);
    assert!(offset_of!(SDL_Keysym, sym) == 4);
    assert!(offset_of!(SDL_Keysym, mod_) == 8);
    assert!(offset_of!(SDL_Keysym, unused) == 12);
    assert!(size_of::<SDL_CommonEvent>() == 8);
    assert!(align_of::<SDL_CommonEvent>() == 4);
    assert!(offset_of!(SDL_CommonEvent, type_) == 0);
    assert!(offset_of!(SDL_CommonEvent, timestamp) == 4);
    assert!(size_of::<SDL_KeyboardEvent>() == 32);
    assert!(align_of::<SDL_KeyboardEvent>() == 4);
    assert!(offset_of!(SDL_KeyboardEvent, type_) == 0);
    assert!(offset_of!(SDL_KeyboardEvent, timestamp) == 4);
    assert!(offset_of!(SDL_KeyboardEvent, windowID) == 8);
    assert!(offset_of!(SDL_KeyboardEvent, state) == 12);
    assert!(offset_of!(SDL_KeyboardEvent, repeat) == 13);
    assert!(offset_of!(SDL_KeyboardEvent, padding2) == 14);
    assert!(offset_of!(SDL_KeyboardEvent, padding3) == 15);
    assert!(offset_of!(SDL_KeyboardEvent, keysym) == 16);
    assert!(size_of::<SDL_TextInputEvent>() == 44);
    assert!(align_of::<SDL_TextInputEvent>() == 4);
    assert!(offset_of!(SDL_TextInputEvent, type_) == 0);
    assert!(offset_of!(SDL_TextInputEvent, timestamp) == 4);
    assert!(offset_of!(SDL_TextInputEvent, windowID) == 8);
    assert!(offset_of!(SDL_TextInputEvent, text) == 12);
    assert!(size_of::<SDL_MouseMotionEvent>() == 36);
    assert!(align_of::<SDL_MouseMotionEvent>() == 4);
    assert!(offset_of!(SDL_MouseMotionEvent, type_) == 0);
    assert!(offset_of!(SDL_MouseMotionEvent, timestamp) == 4);
    assert!(offset_of!(SDL_MouseMotionEvent, windowID) == 8);
    assert!(offset_of!(SDL_MouseMotionEvent, which) == 12);
    assert!(offset_of!(SDL_MouseMotionEvent, state) == 16);
    assert!(offset_of!(SDL_MouseMotionEvent, x) == 20);
    assert!(offset_of!(SDL_MouseMotionEvent, y) == 24);
    assert!(offset_of!(SDL_MouseMotionEvent, xrel) == 28);
    assert!(offset_of!(SDL_MouseMotionEvent, yrel) == 32);
    assert!(size_of::<SDL_MouseButtonEvent>() == 28);
    assert!(align_of::<SDL_MouseButtonEvent>() == 4);
    assert!(offset_of!(SDL_MouseButtonEvent, type_) == 0);
    assert!(offset_of!(SDL_MouseButtonEvent, timestamp) == 4);
    assert!(offset_of!(SDL_MouseButtonEvent, windowID) == 8);
    assert!(offset_of!(SDL_MouseButtonEvent, which) == 12);
    assert!(offset_of!(SDL_MouseButtonEvent, button) == 16);
    assert!(offset_of!(SDL_MouseButtonEvent, state) == 17);
    assert!(offset_of!(SDL_MouseButtonEvent, clicks) == 18);
    assert!(offset_of!(SDL_MouseButtonEvent, padding1) == 19);
    assert!(offset_of!(SDL_MouseButtonEvent, x) == 20);
    assert!(offset_of!(SDL_MouseButtonEvent, y) == 24);
    assert!(align_of::<SDL_MouseWheelEvent>() == 4);
    assert!(offset_of!(SDL_MouseWheelEvent, type_) == 0);
    assert!(offset_of!(SDL_MouseWheelEvent, timestamp) == 4);
    assert!(offset_of!(SDL_MouseWheelEvent, windowID) == 8);
    assert!(offset_of!(SDL_MouseWheelEvent, which) == 12);
    assert!(offset_of!(SDL_MouseWheelEvent, x) == 16);
    assert!(offset_of!(SDL_MouseWheelEvent, y) == 20);
    assert!(size_of::<SDL_ControllerAxisEvent>() == 20);
    assert!(align_of::<SDL_ControllerAxisEvent>() == 4);
    assert!(offset_of!(SDL_ControllerAxisEvent, type_) == 0);
    assert!(offset_of!(SDL_ControllerAxisEvent, timestamp) == 4);
    assert!(offset_of!(SDL_ControllerAxisEvent, which) == 8);
    assert!(offset_of!(SDL_ControllerAxisEvent, axis) == 12);
    assert!(offset_of!(SDL_ControllerAxisEvent, padding1) == 13);
    assert!(offset_of!(SDL_ControllerAxisEvent, padding2) == 14);
    assert!(offset_of!(SDL_ControllerAxisEvent, padding3) == 15);
    assert!(offset_of!(SDL_ControllerAxisEvent, value) == 16);
    assert!(offset_of!(SDL_ControllerAxisEvent, padding4) == 18);
    assert!(size_of::<SDL_ControllerButtonEvent>() == 16);
    assert!(align_of::<SDL_ControllerButtonEvent>() == 4);
    assert!(offset_of!(SDL_ControllerButtonEvent, type_) == 0);
    assert!(offset_of!(SDL_ControllerButtonEvent, timestamp) == 4);
    assert!(offset_of!(SDL_ControllerButtonEvent, which) == 8);
    assert!(offset_of!(SDL_ControllerButtonEvent, button) == 12);
    assert!(offset_of!(SDL_ControllerButtonEvent, state) == 13);
    assert!(offset_of!(SDL_ControllerButtonEvent, padding1) == 14);
    assert!(offset_of!(SDL_ControllerButtonEvent, padding2) == 15);
    assert!(size_of::<SDL_ControllerDeviceEvent>() == 12);
    assert!(align_of::<SDL_ControllerDeviceEvent>() == 4);
    assert!(offset_of!(SDL_ControllerDeviceEvent, type_) == 0);
    assert!(offset_of!(SDL_ControllerDeviceEvent, timestamp) == 4);
    assert!(offset_of!(SDL_ControllerDeviceEvent, which) == 8);
    assert!(size_of::<SDL_DisplayMode>() == 24);
    assert!(align_of::<SDL_DisplayMode>() == 8);
    assert!(offset_of!(SDL_DisplayMode, format) == 0);
    assert!(offset_of!(SDL_DisplayMode, w) == 4);
    assert!(offset_of!(SDL_DisplayMode, h) == 8);
    assert!(offset_of!(SDL_DisplayMode, refresh_rate) == 12);
    assert!(offset_of!(SDL_DisplayMode, driverdata) == 16);
    assert!(size_of::<SDL_MessageBoxButtonData>() == 16);
    assert!(align_of::<SDL_MessageBoxButtonData>() == 8);
    assert!(offset_of!(SDL_MessageBoxButtonData, flags) == 0);
    assert!(offset_of!(SDL_MessageBoxButtonData, buttonid) == 4);
    assert!(offset_of!(SDL_MessageBoxButtonData, text) == 8);
    assert!(size_of::<SDL_MessageBoxData>() == 56);
    assert!(align_of::<SDL_MessageBoxData>() == 8);
    assert!(offset_of!(SDL_MessageBoxData, flags) == 0);
    assert!(offset_of!(SDL_MessageBoxData, window) == 8);
    assert!(offset_of!(SDL_MessageBoxData, title) == 16);
    assert!(offset_of!(SDL_MessageBoxData, message) == 24);
    assert!(offset_of!(SDL_MessageBoxData, numbuttons) == 32);
    assert!(offset_of!(SDL_MessageBoxData, buttons) == 40);
    assert!(offset_of!(SDL_MessageBoxData, colorScheme) == 48);
    assert!(size_of::<SDL_Event>() == 56);
    assert!(align_of::<SDL_Event>() == 8);
    assert!(offset_of!(SDL_Event, type_) == 0);
    assert!(offset_of!(SDL_Event, common) == 0);
    assert!(offset_of!(SDL_Event, key) == 0);
    assert!(offset_of!(SDL_Event, text) == 0);
    assert!(offset_of!(SDL_Event, motion) == 0);
    assert!(offset_of!(SDL_Event, button) == 0);
    assert!(offset_of!(SDL_Event, wheel) == 0);
    assert!(offset_of!(SDL_Event, caxis) == 0);
    assert!(offset_of!(SDL_Event, cbutton) == 0);
    assert!(offset_of!(SDL_Event, cdevice) == 0);
    assert!(offset_of!(SDL_Event, padding) == 0);
};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn layout() {
        assert_eq!(size_of::<SDL_Rect>(), 16);
        assert_eq!(align_of::<SDL_Rect>(), 4);
        assert_eq!(offset_of!(SDL_Rect, x), 0);
        assert_eq!(offset_of!(SDL_Rect, y), 4);
        assert_eq!(offset_of!(SDL_Rect, w), 8);
        assert_eq!(offset_of!(SDL_Rect, h), 12);
        assert_eq!(size_of::<SDL_Keysym>(), 16);
        assert_eq!(align_of::<SDL_Keysym>(), 4);
        assert_eq!(offset_of!(SDL_Keysym, scancode), 0);
        assert_eq!(offset_of!(SDL_Keysym, sym), 4);
        assert_eq!(offset_of!(SDL_Keysym, mod_), 8);
        assert_eq!(offset_of!(SDL_Keysym, unused), 12);
        assert_eq!(size_of::<SDL_CommonEvent>(), 8);
        assert_eq!(align_of::<SDL_CommonEvent>(), 4);
        assert_eq!(offset_of!(SDL_CommonEvent, type_), 0);
        assert_eq!(offset_of!(SDL_CommonEvent, timestamp), 4);
        assert_eq!(size_of::<SDL_KeyboardEvent>(), 32);
        assert_eq!(align_of::<SDL_KeyboardEvent>(), 4);
        assert_eq!(offset_of!(SDL_KeyboardEvent, type_), 0);
        assert_eq!(offset_of!(SDL_KeyboardEvent, timestamp), 4);
        assert_eq!(offset_of!(SDL_KeyboardEvent, windowID), 8);
        assert_eq!(offset_of!(SDL_KeyboardEvent, state), 12);
        assert_eq!(offset_of!(SDL_KeyboardEvent, repeat), 13);
        assert_eq!(offset_of!(SDL_KeyboardEvent, padding2), 14);
        assert_eq!(offset_of!(SDL_KeyboardEvent, padding3), 15);
        assert_eq!(offset_of!(SDL_KeyboardEvent, keysym), 16);
        assert_eq!(size_of::<SDL_TextInputEvent>(), 44);
        assert_eq!(align_of::<SDL_TextInputEvent>(), 4);
        assert_eq!(offset_of!(SDL_TextInputEvent, type_), 0);
        assert_eq!(offset_of!(SDL_TextInputEvent, timestamp), 4);
        assert_eq!(offset_of!(SDL_TextInputEvent, windowID), 8);
        assert_eq!(offset_of!(SDL_TextInputEvent, text), 12);
        assert_eq!(size_of::<SDL_MouseMotionEvent>(), 36);
        assert_eq!(align_of::<SDL_MouseMotionEvent>(), 4);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, type_), 0);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, timestamp), 4);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, windowID), 8);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, which), 12);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, state), 16);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, x), 20);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, y), 24);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, xrel), 28);
        assert_eq!(offset_of!(SDL_MouseMotionEvent, yrel), 32);
        assert_eq!(size_of::<SDL_MouseButtonEvent>(), 28);
        assert_eq!(align_of::<SDL_MouseButtonEvent>(), 4);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, type_), 0);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, timestamp), 4);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, windowID), 8);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, which), 12);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, button), 16);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, state), 17);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, clicks), 18);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, padding1), 19);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, x), 20);
        assert_eq!(offset_of!(SDL_MouseButtonEvent, y), 24);
        assert_eq!(align_of::<SDL_MouseWheelEvent>(), 4);
        assert_eq!(offset_of!(SDL_MouseWheelEvent, type_), 0);
        assert_eq!(offset_of!(SDL_MouseWheelEvent, timestamp), 4);
        assert_eq!(offset_of!(SDL_MouseWheelEvent, windowID), 8);
        assert_eq!(offset_of!(SDL_MouseWheelEvent, which), 12);
        assert_eq!(offset_of!(SDL_MouseWheelEvent, x), 16);
        assert_eq!(offset_of!(SDL_MouseWheelEvent, y), 20);
        assert_eq!(size_of::<SDL_ControllerAxisEvent>(), 20);
        assert_eq!(align_of::<SDL_ControllerAxisEvent>(), 4);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, type_), 0);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, timestamp), 4);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, which), 8);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, axis), 12);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, padding1), 13);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, padding2), 14);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, padding3), 15);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, value), 16);
        assert_eq!(offset_of!(SDL_ControllerAxisEvent, padding4), 18);
        assert_eq!(size_of::<SDL_ControllerButtonEvent>(), 16);
        assert_eq!(align_of::<SDL_ControllerButtonEvent>(), 4);
        assert_eq!(offset_of!(SDL_ControllerButtonEvent, type_), 0);
        assert_eq!(offset_of!(SDL_ControllerButtonEvent, timestamp), 4);
        assert_eq!(offset_of!(SDL_ControllerButtonEvent, which), 8);
        assert_eq!(offset_of!(SDL_ControllerButtonEvent, button), 12);
        assert_eq!(offset_of!(SDL_ControllerButtonEvent, state), 13);
        assert_eq!(offset_of!(SDL_ControllerButtonEvent, padding1), 14);
        assert_eq!(offset_of!(SDL_ControllerButtonEvent, padding2), 15);
        assert_eq!(size_of::<SDL_ControllerDeviceEvent>(), 12);
        assert_eq!(align_of::<SDL_ControllerDeviceEvent>(), 4);
        assert_eq!(offset_of!(SDL_ControllerDeviceEvent, type_), 0);
        assert_eq!(offset_of!(SDL_ControllerDeviceEvent, timestamp), 4);
        assert_eq!(offset_of!(SDL_ControllerDeviceEvent, which), 8);
        assert_eq!(size_of::<SDL_DisplayMode>(), 24);
        assert_eq!(align_of::<SDL_DisplayMode>(), 8);
        assert_eq!(offset_of!(SDL_DisplayMode, format), 0);
        assert_eq!(offset_of!(SDL_DisplayMode, w), 4);
        assert_eq!(offset_of!(SDL_DisplayMode, h), 8);
        assert_eq!(offset_of!(SDL_DisplayMode, refresh_rate), 12);
        assert_eq!(offset_of!(SDL_DisplayMode, driverdata), 16);
        assert_eq!(size_of::<SDL_MessageBoxButtonData>(), 16);
        assert_eq!(align_of::<SDL_MessageBoxButtonData>(), 8);
        assert_eq!(offset_of!(SDL_MessageBoxButtonData, flags), 0);
        assert_eq!(offset_of!(SDL_MessageBoxButtonData, buttonid), 4);
        assert_eq!(offset_of!(SDL_MessageBoxButtonData, text), 8);
        assert_eq!(size_of::<SDL_MessageBoxData>(), 56);
        assert_eq!(align_of::<SDL_MessageBoxData>(), 8);
        assert_eq!(offset_of!(SDL_MessageBoxData, flags), 0);
        assert_eq!(offset_of!(SDL_MessageBoxData, window), 8);
        assert_eq!(offset_of!(SDL_MessageBoxData, title), 16);
        assert_eq!(offset_of!(SDL_MessageBoxData, message), 24);
        assert_eq!(offset_of!(SDL_MessageBoxData, numbuttons), 32);
        assert_eq!(offset_of!(SDL_MessageBoxData, buttons), 40);
        assert_eq!(offset_of!(SDL_MessageBoxData, colorScheme), 48);
        assert_eq!(size_of::<SDL_Event>(), 56);
        assert_eq!(align_of::<SDL_Event>(), 8);
        assert_eq!(offset_of!(SDL_Event, type_), 0);
        assert_eq!(offset_of!(SDL_Event, common), 0);
        assert_eq!(offset_of!(SDL_Event, key), 0);
        assert_eq!(offset_of!(SDL_Event, text), 0);
        assert_eq!(offset_of!(SDL_Event, motion), 0);
        assert_eq!(offset_of!(SDL_Event, button), 0);
        assert_eq!(offset_of!(SDL_Event, wheel), 0);
        assert_eq!(offset_of!(SDL_Event, caxis), 0);
        assert_eq!(offset_of!(SDL_Event, cbutton), 0);
        assert_eq!(offset_of!(SDL_Event, cdevice), 0);
        assert_eq!(offset_of!(SDL_Event, padding), 0);
    }

    #[test]
    fn constants() {
        assert_eq!(i64::from(SDL_QUIT), 256);
        assert_eq!(i64::from(SDL_KEYDOWN), 768);
        assert_eq!(i64::from(SDL_KEYUP), 769);
        assert_eq!(i64::from(SDL_TEXTINPUT), 771);
        assert_eq!(i64::from(SDL_MOUSEMOTION), 1024);
        assert_eq!(i64::from(SDL_MOUSEBUTTONDOWN), 1025);
        assert_eq!(i64::from(SDL_MOUSEBUTTONUP), 1026);
        assert_eq!(i64::from(SDL_MOUSEWHEEL), 1027);
        assert_eq!(i64::from(SDL_CONTROLLERAXISMOTION), 1616);
        assert_eq!(i64::from(SDL_CONTROLLERBUTTONDOWN), 1617);
        assert_eq!(i64::from(SDL_CONTROLLERBUTTONUP), 1618);
        assert_eq!(i64::from(SDL_CONTROLLERDEVICEADDED), 1619);
        assert_eq!(i64::from(SDL_CONTROLLERDEVICEREMOVED), 1620);
        assert_eq!(i64::from(SDL_PRESSED), 1);
        assert_eq!(i64::from(SDL_RELEASED), 0);
        assert_eq!(i64::from(SDL_BUTTON_LEFT), 1);
        assert_eq!(i64::from(SDL_BUTTON_MIDDLE), 2);
        assert_eq!(i64::from(SDL_BUTTON_RIGHT), 3);
        assert_eq!(i64::from(SDL_FALSE), 0);
        assert_eq!(i64::from(SDL_TRUE), 1);
        assert_eq!(i64::from(SDL_ENABLE), 1);
        assert_eq!(i64::from(SDL_DISABLE), 0);
        assert_eq!(i64::from(SDL_QUERY), -1);
        assert_eq!(i64::from(SDL_INIT_TIMER), 1);
        assert_eq!(i64::from(SDL_INIT_VIDEO), 32);
        assert_eq!(i64::from(SDL_INIT_JOYSTICK), 512);
        assert_eq!(i64::from(SDL_INIT_GAMECONTROLLER), 8192);
        assert_eq!(i64::from(SDL_WINDOW_FULLSCREEN), 1);
        assert_eq!(i64::from(SDL_WINDOW_OPENGL), 2);
        assert_eq!(i64::from(SDL_WINDOW_SHOWN), 4);
        assert_eq!(i64::from(SDL_WINDOW_HIDDEN), 8);
        assert_eq!(i64::from(SDL_WINDOW_FULLSCREEN_DESKTOP), 4097);
        assert_eq!(i64::from(SDL_WINDOW_MOUSE_FOCUS), 1024);
        assert_eq!(i64::from(SDL_WINDOW_INPUT_FOCUS), 512);
        assert_eq!(i64::from(SDL_WINDOW_VULKAN), 268435456);
        assert_eq!(i64::from(SDL_WINDOWPOS_CENTERED_MASK), 805240832);
        assert_eq!(i64::from(SDL_GL_RED_SIZE), 0);
        assert_eq!(i64::from(SDL_GL_GREEN_SIZE), 1);
        assert_eq!(i64::from(SDL_GL_BLUE_SIZE), 2);
        assert_eq!(i64::from(SDL_GL_ALPHA_SIZE), 3);
        assert_eq!(i64::from(SDL_GL_DEPTH_SIZE), 6);
        assert_eq!(i64::from(SDL_GL_STENCIL_SIZE), 7);
        assert_eq!(i64::from(SDL_GL_DOUBLEBUFFER), 5);
        assert_eq!(i64::from(SDL_GL_MULTISAMPLEBUFFERS), 13);
        assert_eq!(i64::from(SDL_GL_MULTISAMPLESAMPLES), 14);
        assert_eq!(i64::from(SDL_GL_ACCELERATED_VISUAL), 15);
        assert_eq!(i64::from(SDL_MESSAGEBOX_ERROR), 16);
        assert_eq!(i64::from(SDL_MESSAGEBOX_WARNING), 32);
        assert_eq!(i64::from(SDL_MESSAGEBOX_INFORMATION), 64);
        assert_eq!(i64::from(SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT), 1);
        assert_eq!(i64::from(SDL_THREAD_PRIORITY_LOW), 0);
        assert_eq!(i64::from(SDL_THREAD_PRIORITY_NORMAL), 1);
        assert_eq!(i64::from(SDL_THREAD_PRIORITY_HIGH), 2);
        assert_eq!(i64::from(SDL_CONTROLLER_AXIS_MAX), 6);
        assert_eq!(i64::from(SDL_CONTROLLER_BUTTON_MAX), 21);
        assert_eq!(i64::from(KMOD_CTRL), 192);
        assert_eq!(i64::from(KMOD_SHIFT), 3);
        assert_eq!(i64::from(KMOD_GUI), 3072);
        assert_eq!(i64::from(SDLK_UNKNOWN), 0);
        assert_eq!(i64::from(SDLK_q), 113);
    }
}
