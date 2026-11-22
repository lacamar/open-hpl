//! Scalar state of `iLuxEntity`, allocated and initialised here; C++ reads it through a mirror.

use crate::luxprop::entc::EntityCb;
use std::ffi::c_void;

#[repr(C)]
#[derive(Debug)]
pub struct EntState {
    pub map: *mut c_void,
    pub max_focus_distance: f32,
    pub id: i32,
    pub entity_type: i32,
    pub crosshair: i32,
    pub cb: EntityCb,
    pub active: bool,
    pub full_game_save: bool,
    pub is_saved: bool,
    pub interaction_disabled: bool,
    pub destroy_me: bool,
}

impl EntState {
    pub fn new(id: i32, entity_type: i32, map: *mut c_void) -> Self {
        Self {
            map,
            max_focus_distance: 2.0,
            id,
            entity_type,
            crosshair: 0,
            cb: EntityCb {
                interact_remove: true,
                lookat_remove: true,
                lookat_count: 0.0,
                is_looked_at: false,
            },
            active: true,
            full_game_save: false,
            is_saved: true,
            interaction_disabled: false,
            destroy_me: false,
        }
    }
}

/// # Safety
/// `map` is only stored.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_state_new(
    id: i32,
    entity_type: i32,
    map: *mut c_void,
) -> *mut EntState {
    Box::into_raw(Box::new(EntState::new(id, entity_type, map)))
}

/// # Safety
/// `s` came from `ohpl_ent_state_new` and is freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_state_free(s: *mut EntState) {
    if !s.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(s) });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_match_cpp_constructor() {
        let s = EntState::new(7, 3, std::ptr::null_mut());
        assert!(s.active && s.is_saved && s.cb.interact_remove && s.cb.lookat_remove);
        assert!(
            !s.full_game_save && !s.interaction_disabled && !s.destroy_me && !s.cb.is_looked_at
        );
        assert_eq!(
            (s.id, s.entity_type, s.crosshair, s.max_focus_distance),
            (7, 3, 0, 2.0)
        );
        assert_eq!(std::mem::size_of::<EntState>(), 48);
    }
}
