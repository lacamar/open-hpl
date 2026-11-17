//! `iLuxProp` base state: one boxed arena entry per prop behind a handle, mirrored by
//! `OhplA5eState` in `amnesia/src/game/RustLuxProp.h`. Engine object pointers stay in the C++ shell.

use super::base::{
    self, BaseParams, BaseState, EffectsIn, LightData, MoveIn, SLOT_EFFECTS_OFF_SOUND,
    SLOT_EFFECTS_ON_SOUND, SLOT_MAIN_BODY, SLOT_MOVE_LOOP_SOUND, SLOT_MOVE_START_SOUND,
    SLOT_MOVE_STOP_SOUND,
};
use super::ffi::{
    Host, Sink, VarsRef, ohpl_ga4_anim_update, ohpl_ga4_effects_set_active,
    ohpl_ga4_effects_update, ohpl_ga4_give_damage, ohpl_ga4_update_moving,
};
use super::vars::Vars;
use crate::obj::{Arena, Handle};
use crate::types::{Color, Matrixf, Vector3f};
use std::ffi::c_char;
use std::sync::{Mutex, MutexGuard, PoisonError};

pub const STR_SLOTS: usize = 10;
const STR_ANIM_CALLBACK: usize = 0;
const STR_MAIN_BODY: usize = 1;
const STR_MOVE_START: usize = 2;
const STR_MOVE_STOP: usize = 3;
const STR_MOVE_LOOP: usize = 4;
const STR_EFFECTS_ON: usize = 5;
const STR_EFFECTS_OFF: usize = 6;

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

impl Entry {
    fn set_str(&mut self, slot: usize, src: &[u8]) {
        if let Some(s) = self.strs.get_mut(slot) {
            s.clear();
            s.extend_from_slice(src);
            s.push(0);
        }
    }

    fn has_str(&self, slot: usize) -> bool {
        self.strs.get(slot).is_some_and(|s| s.len() > 1)
    }

    fn effects_in(&self, active: bool, fade: bool, n_ps: i32, n_sounds: i32) -> EffectsIn {
        EffectsIn {
            active,
            fade,
            n_ps,
            n_sounds,
            on_time: self.st.effects_on_time,
            off_time: self.st.effects_off_time,
            off_radius: self.st.effects_off_light_radius,
            on_sound: self.has_str(STR_EFFECTS_ON),
            off_sound: self.has_str(STR_EFFECTS_OFF),
        }
    }
}

/// Routes the base sound slots into the entry and forwards everything else.
struct Capture<'a> {
    inner: &'a VarsRef,
    got: std::cell::RefCell<Vec<(i32, Vec<u8>)>>,
}

impl Vars for Capture<'_> {
    fn string(&self, name: &str, def: &[u8]) -> Vec<u8> {
        self.inner.string(name, def)
    }
    fn boolean(&self, name: &str, def: bool) -> bool {
        self.inner.boolean(name, def)
    }
    fn float(&self, name: &str, def: f32) -> f32 {
        self.inner.float(name, def)
    }
    fn int(&self, name: &str, def: i32) -> i32 {
        self.inner.int(name, def)
    }
    fn vec3(&self, name: &str, def: Vector3f) -> Vector3f {
        self.inner.vec3(name, def)
    }
    fn color(&self, name: &str, def: Color) -> Color {
        self.inner.color(name, def)
    }
    fn put(&self, slot: i32, s: &[u8]) {
        self.got.borrow_mut().push((slot, s.to_vec()));
    }
}

/// `iLuxPropLoader::AfterLoad` base properties into the entry; `out` keeps the loader-only values.
///
/// # Safety
/// `v` and `out` must be valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_load_base(h: u64, v: *const VarsRef, out: *mut BaseParams) {
    let Some(e) = entry_ptr(h) else { return };
    // SAFETY: caller contract.
    let (v, out) = unsafe { (&*v, &mut *out) };
    let cap = Capture {
        inner: v,
        got: std::cell::RefCell::default(),
    };
    base::load_base(&cap, out);
    // SAFETY: live entry; the loader holds no other reference.
    let e = unsafe { &mut *e };
    for (slot, s) in cap.got.take() {
        let dst = match slot {
            SLOT_MAIN_BODY => STR_MAIN_BODY,
            SLOT_EFFECTS_ON_SOUND => STR_EFFECTS_ON,
            SLOT_EFFECTS_OFF_SOUND => STR_EFFECTS_OFF,
            SLOT_MOVE_START_SOUND => STR_MOVE_START,
            SLOT_MOVE_STOP_SOUND => STR_MOVE_STOP,
            SLOT_MOVE_LOOP_SOUND => STR_MOVE_LOOP,
            _ => continue,
        };
        e.set_str(dst, &s);
    }
    let st = &mut e.st;
    st.health = out.health;
    st.toughness = out.toughness;
    st.dissolve_on_destruction = out.dissolve_on_destruction;
    st.dissolve_time = out.dissolve_time;
    st.show_mesh = out.show_mesh;
    st.show_hints = out.show_hints;
    st.effects_on_time = out.effects_on_time;
    st.effects_off_time = out.effects_off_time;
    st.effects_off_light_color = [
        out.effects_off_light_color.r,
        out.effects_off_light_color.g,
        out.effects_off_light_color.b,
        out.effects_off_light_color.a,
    ];
    st.effects_off_light_radius = out.effects_off_light_radius;
    st.move_check_collision = out.move_check_collision;
}

/// # Safety
/// `out` must be valid for one write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_effects_env(
    h: u64,
    active: bool,
    fade: bool,
    n_ps: i32,
    n_sounds: i32,
    out: *mut EffectsIn,
) {
    let Some(e) = entry_ptr(h) else { return };
    // SAFETY: live entry; caller contract for `out`.
    unsafe { *out = (*e).effects_in(active, fade, n_ps, n_sounds) };
}

/// # Safety
/// Pointers as for `ohpl_ga4_effects_set_active`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_effects_set_active(
    h: u64,
    active: bool,
    fade: bool,
    n_ps: i32,
    n_sounds: i32,
    lights: *const LightData,
    n_lights: i32,
    sink: *const Sink,
) {
    let Some(e) = entry_ptr(h) else { return };
    // SAFETY: live entry.
    let env = unsafe { (*e).effects_in(active, fade, n_ps, n_sounds) };
    // SAFETY: caller contract; the state pointer outlives the call.
    unsafe { ohpl_ga4_effects_set_active(&raw mut (*e).st.base, &env, lights, n_lights, sink) };
}

/// # Safety
/// Pointers as for `ohpl_ga4_effects_update`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_effects_update(
    h: u64,
    dt: f32,
    lights: *const LightData,
    n_lights: i32,
    has_mesh: bool,
    sink: *const Sink,
) {
    let Some(e) = entry_ptr(h) else { return };
    // SAFETY: live entry; caller contract.
    unsafe {
        let (on, off) = ((*e).st.effects_on_time, (*e).st.effects_off_time);
        ohpl_ga4_effects_update(
            &raw mut (*e).st.base,
            dt,
            on,
            off,
            lights,
            n_lights,
            has_mesh,
            sink,
        );
    }
}

/// `UpdateMoving`: `body` is read only while moving.
///
/// # Safety
/// `body` holds 16 floats or is null; `host` and `sink` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_update_moving(
    h: u64,
    dt: f32,
    body: *const Matrixf,
    loop_present: bool,
    loop_alive: bool,
    host: *const Host,
    sink: *const Sink,
) {
    let Some(e) = entry_ptr(h) else { return };
    // SAFETY: live entry; caller contract.
    unsafe {
        let a = MoveIn {
            dt,
            body: if (*e).st.base.moving && !body.is_null() {
                *body
            } else {
                Matrixf::default()
            },
            check_collision: (*e).st.move_check_collision,
            loop_present,
            loop_alive,
            loop_sound: (*e).has_str(STR_MOVE_LOOP),
        };
        ohpl_ga4_update_moving(&raw mut (*e).st.base, &a, host, sink);
    }
}

/// # Safety
/// `sink` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_anim_update(h: u64, over: bool, sink: *const Sink) {
    let Some(e) = entry_ptr(h) else { return };
    // SAFETY: live entry; caller contract.
    unsafe {
        let has_cb = (*e).has_str(STR_ANIM_CALLBACK);
        ohpl_ga4_anim_update(&raw mut (*e).st.anim_index, over, has_cb, sink);
    }
}

/// `GiveDamage`: the new health; `out_amount` receives the damage that applied.
///
/// # Safety
/// `out_amount` valid for one write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a5e_give_damage(
    h: u64,
    amount: f32,
    strength: i32,
    out_amount: *mut f32,
) -> f32 {
    let Some(e) = entry_ptr(h) else { return 0.0 };
    // SAFETY: live entry; caller contract.
    unsafe {
        ohpl_ga4_give_damage(
            (*e).st.health,
            (*e).st.toughness,
            amount,
            strength,
            out_amount,
        )
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
        let mut env = EffectsIn::default();
        // SAFETY: live entry and out slot.
        unsafe {
            ohpl_a5e_str_set(h, STR_EFFECTS_ON, b"snd".as_ptr(), 3);
            (*p).effects_on_time = 2.0;
            ohpl_a5e_effects_env(h, true, false, 1, 2, &mut env);
        }
        assert!(env.on_sound && !env.off_sound && env.on_time == 2.0 && env.n_sounds == 2);
        ohpl_a5e_free(h);
        assert!(entry_ptr(h).is_none());
    }
}
