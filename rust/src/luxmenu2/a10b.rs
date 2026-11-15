//! A10b object store and shared pieces: the objects live in arenas, C++ shells hold handle bits.
//! No lock is held across a host call, because host calls re-enter through widget callbacks.

use crate::obj::{Arena, Handle};
use std::sync::Mutex;

pub struct Store<T>(Mutex<Arena<T>>);

impl<T> Store<T> {
    pub fn new() -> Store<T> {
        Store(Mutex::new(Arena::default()))
    }

    pub fn add(&self, v: T) -> u64 {
        let Ok(mut a) = self.0.lock() else {
            return 0;
        };
        a.insert(v).map_or(0, Handle::to_bits)
    }

    /// `f` must not call the host.
    pub fn with<R>(&self, h: u64, f: impl FnOnce(&mut T) -> R) -> Option<R> {
        let hd = Handle::from_bits(h)?;
        let mut a = self.0.lock().ok()?;
        a.get_mut(hd).map(f)
    }

    pub fn remove(&self, h: u64) -> Option<T> {
        let hd = Handle::from_bits(h)?;
        self.0.lock().ok()?.remove(hd)
    }
}

impl<T> Default for Store<T> {
    fn default() -> Store<T> {
        Store::new()
    }
}

/// Message ids of `widget_callback`; the C++ side maps them to `eGuiMessage`.
pub const MSG_BUTTON_PRESSED: i64 = 1;
pub const MSG_UI_ARROW: i64 = 2;
pub const MSG_UI_BUTTON: i64 = 3;
pub const MSG_SLIDER_MOVE: i64 = 4;
pub const MSG_ON_UPDATE: i64 = 5;
pub const MSG_ON_DRAW: i64 = 6;
pub const MSG_MOUSE_UP: i64 = 7;
pub const MSG_MOUSE_ENTER: i64 = 8;
pub const MSG_MOUSE_LEAVE: i64 = 9;

pub const UI_PRIMARY: i32 = 1;

pub fn wide(s: &[u8]) -> Vec<u32> {
    s.iter().map(|b| u32::from(*b)).collect()
}

pub fn v3(a: [f32; 2], z: f32) -> [f32; 3] {
    [a[0], a[1], z]
}

pub const WHITE: [f32; 4] = [1.0, 1.0, 1.0, 1.0];
