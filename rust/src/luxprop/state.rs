//! `iLuxProp` base state: one boxed arena entry per prop behind a handle, mirrored by
//! `OhplA5eState` in `amnesia/src/game/RustLuxProp.h`. Engine object pointers stay in the C++ shell.

use super::base::BaseState;
use crate::obj::{Arena, Handle};
use std::ffi::c_char;
use std::sync::{Mutex, MutexGuard, PoisonError};

pub const STR_SLOTS: usize = 10;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PropState {
    pub base: BaseState,
    pub health: f32,
    pub toughness: i32,
    pub anim_index: i32,
    pub move_loop_sound_id: i32,
    pub dissolve_time: f32,
    pub effects_on_time: f32,
    pub effects_off_time: f32,
    pub effects_off_light_radius: f32,
    pub body_mass_backup: f32,
    pub effects_off_light_color: [f32; 4],
    pub on_load_scale: [f32; 3],
    pub on_load_transform: [f32; 16],
    pub parent_bone_offset: [f32; 16],
    pub body_matrix_backup: [f32; 16],
    pub interacted_with: bool,
    pub static_physics: bool,
    pub has_flickering: bool,
    pub dissolve_on_destruction: bool,
    pub show_mesh: bool,
    pub move_check_collision: bool,
    pub show_hints: bool,
    pub body_collide_backup: bool,
    pub body_collide_character_backup: bool,
    pub body_active_backup: bool,
}

const _: () = assert!(std::mem::size_of::<PropState>() == 512);

impl PropState {
    pub fn new() -> Self {
        Self {
            base: BaseState::new(),
            health: 0.0,
            toughness: 0,
            anim_index: -1,
            move_loop_sound_id: 0,
            dissolve_time: 0.0,
            effects_on_time: 0.0,
            effects_off_time: 0.0,
            effects_off_light_radius: 0.0,
            body_mass_backup: 1.0,
            effects_off_light_color: [0.0; 4],
            on_load_scale: [0.0; 3],
            on_load_transform: [0.0; 16],
            parent_bone_offset: [0.0; 16],
            body_matrix_backup: [0.0; 16],
            interacted_with: false,
            static_physics: false,
            has_flickering: false,
            dissolve_on_destruction: false,
            show_mesh: false,
            move_check_collision: false,
            show_hints: false,
            body_collide_backup: false,
            body_collide_character_backup: false,
            body_active_backup: false,
        }
    }
}

impl Default for PropState {
    fn default() -> Self {
        Self::new()
    }
}

/// Strings keep a trailing NUL so C++ can hand out `c_str()` pointers.
struct Entry {
    st: PropState,
    strs: [Vec<u8>; STR_SLOTS],
}

static PROPS: Mutex<Option<Arena<Box<Entry>>>> = Mutex::new(None);

fn props() -> MutexGuard<'static, Option<Arena<Box<Entry>>>> {
    PROPS.lock().unwrap_or_else(PoisonError::into_inner)
}

/// Boxed entries never move; the pointer stays valid until `ohpl_a5e_free`.
fn entry_ptr(h: u64) -> Option<*mut Entry> {
    let mut g = props();
    let e = g.as_mut()?.get_mut(Handle::from_bits(h)?)?;
    Some(&raw mut **e)
}

/// Creates the entry; `out` receives the state pointer. Returns the handle (0 on failure).
///
/// # Safety
/// `out` must be valid for one pointer write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_new(out: *mut *mut PropState) -> u64 {
    let entry = Box::new(Entry {
        st: PropState::new(),
        strs: std::array::from_fn(|_| vec![0]),
    });
    let mut g = props();
    let Some(h) = g.get_or_insert_with(Arena::default).insert(entry) else {
        return 0;
    };
    if let Some(e) = g.as_mut().and_then(|a| a.get_mut(h)) {
        // SAFETY: caller contract.
        unsafe { *out = &raw mut e.st };
    }
    h.to_bits()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a5e_free(h: u64) {
    let removed = {
        let mut g = props();
        match (g.as_mut(), Handle::from_bits(h)) {
            (Some(a), Some(h)) => a.remove(h),
            _ => None,
        }
    };
    drop(removed);
}

/// NUL-terminated string of `slot`; `len` receives its length.
///
/// # Safety
/// `len` must be valid for one write; the result is valid until the slot is set or freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_str_get(h: u64, slot: usize, len: *mut usize) -> *const c_char {
    let Some(e) = entry_ptr(h) else {
        return c"".as_ptr();
    };
    // SAFETY: live entry (see `entry_ptr`).
    let Some(s) = (unsafe { (*e).strs.get(slot) }) else {
        return c"".as_ptr();
    };
    // SAFETY: caller contract.
    unsafe { *len = s.len().saturating_sub(1) };
    s.as_ptr().cast()
}

/// # Safety
/// `p` must be valid for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_str_set(h: u64, slot: usize, p: *const u8, len: usize) {
    let Some(e) = entry_ptr(h) else { return };
    // SAFETY: caller contract.
    let src = unsafe { std::slice::from_raw_parts(p, len) };
    let mut v = Vec::with_capacity(len + 1);
    v.extend_from_slice(src);
    v.push(0);
    // SAFETY: live entry.
    if let Some(s) = unsafe { (*e).strs.get_mut(slot) } {
        *s = v;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strings_and_state() {
        let mut p: *mut PropState = std::ptr::null_mut();
        // SAFETY: `p` is a valid out slot.
        let h = unsafe { ohpl_a5e_new(&mut p) };
        assert!(h != 0 && !p.is_null());
        // SAFETY: fresh live entry.
        unsafe {
            assert_eq!((*p).anim_index, -1);
            assert_eq!((*p).body_mass_backup, 1.0);
            let mut n = 7usize;
            ohpl_a5e_str_get(h, 3, &mut n);
            assert_eq!(n, 0);
            ohpl_a5e_str_set(h, 3, b"abc".as_ptr(), 3);
            let c = ohpl_a5e_str_get(h, 3, &mut n);
            assert_eq!((n, *c.add(3)), (3, 0));
        }
        ohpl_a5e_free(h);
        assert!(entry_ptr(h).is_none());
    }
}
