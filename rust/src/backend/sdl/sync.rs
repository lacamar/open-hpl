//! `cMutexSDL` and `cThreadSDL`: SDL mutex and thread handles owned here.

use crate::sys::sdl2::{
    SDL_CreateMutex, SDL_CreateThread, SDL_DestroyMutex, SDL_LockMutex, SDL_SetThreadPriority,
    SDL_THREAD_PRIORITY_HIGH, SDL_THREAD_PRIORITY_LOW, SDL_THREAD_PRIORITY_NORMAL, SDL_Thread,
    SDL_ThreadFunction, SDL_UnlockMutex, SDL_WaitThread, SDL_mutex,
};
use core::ffi::{c_char, c_int, c_void};

pub struct Mutex(*mut SDL_mutex);

impl Mutex {
    pub fn new() -> Self {
        // SAFETY: plain SDL call.
        Self(unsafe { SDL_CreateMutex() })
    }

    pub fn lock(&self) -> bool {
        // SAFETY: handle from SDL_CreateMutex (null is rejected by SDL with -1).
        unsafe { SDL_LockMutex(self.0) != -1 }
    }

    pub fn unlock(&self) -> bool {
        // SAFETY: as `lock`.
        unsafe { SDL_UnlockMutex(self.0) != -1 }
    }
}

impl Drop for Mutex {
    fn drop(&mut self) {
        if !self.0.is_null() {
            // SAFETY: created by SDL_CreateMutex and destroyed once.
            unsafe { SDL_DestroyMutex(self.0) };
        }
    }
}

pub struct Thread(*mut SDL_Thread);

/// `eThreadPrio` Low=0 Normal=1 High=2; anything else is normal.
pub fn translate_prio(prio: i32) -> c_int {
    match prio {
        0 => SDL_THREAD_PRIORITY_LOW,
        2 => SDL_THREAD_PRIORITY_HIGH,
        _ => SDL_THREAD_PRIORITY_NORMAL,
    }
}

impl Thread {
    pub fn new() -> Self {
        Self(core::ptr::null_mut())
    }

    pub fn start(&mut self, f: SDL_ThreadFunction, name: *const c_char, data: *mut c_void) {
        // SAFETY: `name` is NUL-terminated and `data` outlives the thread (the C++ owner joins in Stop).
        self.0 = unsafe { SDL_CreateThread(Some(f), name, data) };
    }

    pub fn stop(&mut self) {
        // SAFETY: the handle is null or a thread not yet waited on.
        unsafe { SDL_WaitThread(self.0, core::ptr::null_mut()) };
        self.0 = core::ptr::null_mut();
    }
}

pub fn set_priority(prio: i32) {
    // SAFETY: plain SDL call.
    unsafe { SDL_SetThreadPriority(translate_prio(prio)) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_mutex_new() -> *mut Mutex {
    Box::into_raw(Box::new(Mutex::new()))
}

/// # Safety
/// `m` from `ohpl_l1_mutex_new`, freed once (or null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_mutex_free(m: *mut Mutex) {
    if !m.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(m) });
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_mutex_lock(m: *const Mutex) -> bool {
    // SAFETY: caller contract; SDL mutex state is internal so a shared ref is enough.
    unsafe { m.as_ref() }.is_some_and(Mutex::lock)
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_mutex_unlock(m: *const Mutex) -> bool {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.is_some_and(Mutex::unlock)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_thread_new() -> *mut Thread {
    Box::into_raw(Box::new(Thread::new()))
}

/// # Safety
/// `t` from `ohpl_l1_thread_new`, freed once (or null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_thread_free(t: *mut Thread) {
    if !t.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(t) });
    }
}

/// # Safety
/// `t` is live; `name` NUL-terminated; `data` stays valid until `ohpl_l1_thread_stop`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_thread_start(
    t: *mut Thread,
    f: SDL_ThreadFunction,
    name: *const c_char,
    data: *mut c_void,
) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.start(f, name, data);
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_thread_stop(t: *mut Thread) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.stop();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l1_thread_set_priority(prio: i32) {
    set_priority(prio);
}
