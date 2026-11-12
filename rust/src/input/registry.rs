//! `cInput`'s action registry: named/id-keyed `cAction`s, their sub actions and state. The C++
//! `cAction` objects are shells carrying a handle; devices stay owned by the C++ device objects.
//!
//! Kept C++ quirks: a second action with an existing name is never updated or found by name,
//! `find_id` returns the first action created with that id, hats never trigger on SDL2 pads.

use super::{ActionState, Gamepad, KEY_LAST, Keyboard, Mouse};
use crate::obj::{Arena, Handle};
use std::collections::BTreeMap;
use std::ffi::c_void;

pub const SUB_KEY: u32 = 1;
pub const SUB_MOUSE: u32 = 2;
pub const SUB_PAD_BUTTON: u32 = 3;
pub const SUB_PAD_AXIS: u32 = 4;
pub const SUB_PAD_HAT: u32 = 5;

const MOUSE_BUTTON_COUNT: i32 = 9;
const RANGE_POSITIVE: i32 = 1;
const RANGE_NEGATIVE: i32 = 2;

/// `kind`-tagged sub action. `id`: key, mouse button, pad button, axis or hat; `range`: axis range
/// or hat state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SubSpec {
    pub kind: u32,
    pub pad: i32,
    pub id: i32,
    pub range: i32,
    pub min: f32,
    pub max: f32,
}

pub struct Action {
    id: i32,
    user: *mut c_void,
    state: ActionState,
    subs: Vec<SubSpec>,
}

#[derive(Default)]
pub struct Registry {
    actions: Arena<Action>,
    by_name: BTreeMap<Vec<u8>, Handle<Action>>,
    by_id: Vec<(i32, Handle<Action>)>,
    keyboard: Option<*const Keyboard>,
    mouse: Option<*const Mouse>,
    pads: Vec<*const Gamepad>,
}

impl Registry {
    pub fn new() -> Self {
        Self::default()
    }

    /// # Safety
    /// The devices outlive every later call, or are replaced/cleared first.
    pub unsafe fn set_devices(&mut self, keyboard: *const Keyboard, mouse: *const Mouse) {
        self.keyboard = (!keyboard.is_null()).then_some(keyboard);
        self.mouse = (!mouse.is_null()).then_some(mouse);
    }

    /// # Safety
    /// As [`Self::set_devices`]; `pads` holds the gamepads in `cInput` list order.
    pub unsafe fn set_pads(&mut self, pads: &[*const Gamepad]) {
        self.pads = pads.to_vec();
    }

    pub fn create(&mut self, name: &[u8], id: i32, user: *mut c_void) -> Option<Handle<Action>> {
        let h = self.actions.insert(Action {
            id,
            user,
            state: ActionState::default(),
            subs: Vec::new(),
        })?;
        self.by_name.entry(name.to_vec()).or_insert(h);
        // multimap insert: after the last entry with an equal id
        let at = self.by_id.partition_point(|e| e.0 <= id);
        self.by_id.insert(at, (id, h));
        Some(h)
    }

    pub fn destroy(&mut self, h: Handle<Action>) {
        if let Some(name) = self
            .by_name
            .iter()
            .find_map(|(k, v)| (*v == h).then(|| k.clone()))
        {
            self.by_name.remove(&name);
        }
        if let Some(i) = self.by_id.iter().position(|e| e.1 == h) {
            self.by_id.remove(i);
        }
        self.actions.remove(h);
    }

    pub fn find_name(&self, name: &[u8]) -> Option<Handle<Action>> {
        self.by_name.get(name).copied()
    }

    pub fn find_id(&self, id: i32) -> Option<Handle<Action>> {
        self.by_id.iter().find(|e| e.0 == id).map(|e| e.1)
    }

    pub fn user(&self, h: Handle<Action>) -> *mut c_void {
        self.actions
            .get(h)
            .map_or(core::ptr::null_mut(), |a| a.user)
    }

    pub fn action_id(&self, h: Handle<Action>) -> Option<i32> {
        self.actions.get(h).map(|a| a.id)
    }

    pub fn add_sub(&mut self, h: Handle<Action>, spec: SubSpec) {
        if let Some(a) = self.actions.get_mut(h) {
            a.subs.push(spec);
        }
    }

    pub fn clear_subs(&mut self, h: Handle<Action>) {
        if let Some(a) = self.actions.get_mut(h) {
            a.subs.clear();
        }
    }

    fn key_down(&self, key: i32) -> bool {
        // SAFETY: `set_devices` contract.
        self.keyboard
            .and_then(|k| unsafe { k.as_ref() })
            .is_some_and(|k| k.key_is_down(key))
    }

    fn mouse_down(&self, button: i32) -> bool {
        // SAFETY: `set_devices` contract.
        self.mouse
            .and_then(|m| unsafe { m.as_ref() })
            .is_some_and(|m| m.button_is_down(button))
    }

    fn pad(&self, index: i32) -> Option<&Gamepad> {
        let i = usize::try_from(index).ok()?;
        // SAFETY: `set_pads` contract.
        self.pads.get(i).and_then(|p| unsafe { p.as_ref() })
    }

    fn sub_triggered(&self, s: &SubSpec) -> bool {
        match s.kind {
            SUB_KEY => self.key_down(s.id),
            SUB_MOUSE => self.mouse_down(s.id),
            SUB_PAD_BUTTON => self.pad(s.pad).is_some_and(|p| p.button_is_down(s.id)),
            SUB_PAD_AXIS => self.pad(s.pad).is_some_and(|p| {
                let v = p.axis_value(s.id);
                let valid = (s.range == RANGE_POSITIVE && v > 0.0)
                    || (s.range == RANGE_NEGATIVE && v < 0.0);
                let v = v.abs();
                valid && s.min <= v && v <= s.max
            }),
            _ => false,
        }
    }

    fn any_down(&self, h: Handle<Action>) -> bool {
        self.actions
            .get(h)
            .is_some_and(|a| a.subs.iter().any(|s| self.sub_triggered(s)))
    }

    fn with_state<R>(
        &mut self,
        h: Handle<Action>,
        f: impl FnOnce(&mut ActionState) -> R,
    ) -> Option<R> {
        self.actions.get_mut(h).map(|a| f(&mut a.state))
    }

    pub fn is_down(&self, h: Handle<Action>) -> bool {
        self.actions.get(h).is_some_and(|a| a.state.is_down())
    }

    pub fn was_triggered(&mut self, h: Handle<Action>) -> bool {
        self.with_state(h, ActionState::was_triggered)
            .unwrap_or(false)
    }

    pub fn became_triggered(&mut self, h: Handle<Action>) -> bool {
        self.with_state(h, ActionState::became_triggered)
            .unwrap_or(false)
    }

    pub fn double_triggered(&mut self, h: Handle<Action>, limit: f32) -> bool {
        self.with_state(h, |s| s.double_triggered(limit))
            .unwrap_or(false)
    }

    pub fn update(&mut self, h: Handle<Action>, time_step: f32) {
        let any = self.any_down(h);
        self.with_state(h, |s| s.update(any, time_step));
    }

    pub fn reset_to_current_state(&mut self, h: Handle<Action>) {
        let any = self.any_down(h);
        self.with_state(h, |s| s.reset_to_current_state(any));
    }

    /// `cInput::Update` tail: every name-registered action, in name order.
    pub fn update_all(&mut self, time_step: f32) {
        let order: Vec<_> = self.by_name.values().copied().collect();
        for h in order {
            self.update(h, time_step);
        }
    }

    pub fn reset_all(&mut self) {
        let order: Vec<_> = self.by_name.values().copied().collect();
        for h in order {
            self.reset_to_current_state(h);
        }
    }

    /// Removes the first name-registered action; its `user`, null when none is left.
    pub fn pop_registered(&mut self) -> *mut c_void {
        let Some((_, h)) = self.by_name.pop_first() else {
            return core::ptr::null_mut();
        };
        let user = self.user(h);
        self.destroy(h);
        user
    }

    /// First key, else first mouse button, currently down.
    pub fn first_down(&self) -> Option<(u32, i32)> {
        (0..i32::from(KEY_LAST))
            .find(|&k| self.key_down(k))
            .map(|k| (SUB_KEY, k))
            .or_else(|| {
                (0..MOUSE_BUTTON_COUNT)
                    .find(|&b| self.mouse_down(b))
                    .map(|b| (SUB_MOUSE, b))
            })
    }
}

mod ffi {
    use super::*;
    use crate::string_ffi::units;

    /// # Safety
    /// `p` is null or a live registry from `ohpl_input_reg_new`.
    unsafe fn reg<'a>(p: *mut Registry) -> Option<&'a mut Registry> {
        // SAFETY: caller contract.
        unsafe { p.as_mut() }
    }

    fn handle(bits: u64) -> Option<Handle<Action>> {
        Handle::from_bits(bits)
    }

    #[unsafe(no_mangle)]
    pub extern "C" fn ohpl_input_reg_new() -> *mut Registry {
        Box::into_raw(Box::new(Registry::new()))
    }

    /// # Safety
    /// `r` comes from `ohpl_input_reg_new` and is not used afterwards.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_free(r: *mut Registry) {
        if !r.is_null() {
            // SAFETY: caller contract.
            drop(unsafe { Box::from_raw(r) });
        }
    }

    /// # Safety
    /// `r` live; the devices stay alive until replaced or cleared with null.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_set_devices(
        r: *mut Registry,
        kb: *const Keyboard,
        mouse: *const Mouse,
    ) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { reg(r) } {
            unsafe { r.set_devices(kb, mouse) };
        }
    }

    /// # Safety
    /// `r` live; `pads` points to `n` gamepads that stay alive until replaced.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_set_pads(
        r: *mut Registry,
        pads: *const *const Gamepad,
        n: usize,
    ) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { reg(r) } {
            unsafe { r.set_pads(units(pads, n)) };
        }
    }

    /// # Safety
    /// `r` live; `name` points to `n` bytes.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_create(
        r: *mut Registry,
        name: *const u8,
        n: usize,
        id: i32,
        user: *mut c_void,
    ) -> u64 {
        // SAFETY: caller contract.
        unsafe { reg(r) }
            .and_then(|r| r.create(unsafe { units(name, n) }, id, user))
            .map_or(0, Handle::to_bits)
    }

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_destroy(r: *mut Registry, h: u64) {
        // SAFETY: caller contract.
        if let (Some(r), Some(h)) = (unsafe { reg(r) }, handle(h)) {
            r.destroy(h);
        }
    }

    /// # Safety
    /// `r` live; `name` points to `n` bytes.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_find_name(
        r: *mut Registry,
        name: *const u8,
        n: usize,
    ) -> u64 {
        // SAFETY: caller contract.
        unsafe { reg(r) }
            .and_then(|r| r.find_name(unsafe { units(name, n) }))
            .map_or(0, Handle::to_bits)
    }

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_find_id(r: *mut Registry, id: i32) -> u64 {
        // SAFETY: caller contract.
        unsafe { reg(r) }
            .and_then(|r| r.find_id(id))
            .map_or(0, Handle::to_bits)
    }

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_user(r: *mut Registry, h: u64) -> *mut c_void {
        // SAFETY: caller contract.
        match (unsafe { reg(r) }, handle(h)) {
            (Some(r), Some(h)) => r.user(h),
            _ => core::ptr::null_mut(),
        }
    }

    /// # Safety
    /// `r` live; `spec` readable.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_add_sub(
        r: *mut Registry,
        h: u64,
        spec: *const SubSpec,
    ) {
        // SAFETY: caller contract.
        if let (Some(r), Some(h), Some(s)) =
            (unsafe { reg(r) }, handle(h), unsafe { spec.as_ref() })
        {
            r.add_sub(h, *s);
        }
    }

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_clear_subs(r: *mut Registry, h: u64) {
        // SAFETY: caller contract.
        if let (Some(r), Some(h)) = (unsafe { reg(r) }, handle(h)) {
            r.clear_subs(h);
        }
    }

    macro_rules! query {
        ($name:ident, $m:ident $(, $arg:ident: $t:ty)*) => {
            /// # Safety
            /// `r` live.
            #[unsafe(no_mangle)]
            pub unsafe extern "C" fn $name(r: *mut Registry, h: u64 $(, $arg: $t)*) -> bool {
                // SAFETY: caller contract.
                match (unsafe { reg(r) }, handle(h)) {
                    (Some(r), Some(h)) => r.$m(h $(, $arg)*),
                    _ => false,
                }
            }
        };
    }
    query!(ohpl_input_reg_is_down, is_down);
    query!(ohpl_input_reg_was_triggered, was_triggered);
    query!(ohpl_input_reg_became_triggered, became_triggered);
    query!(ohpl_input_reg_double_triggered, double_triggered, limit: f32);

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_update(r: *mut Registry, time_step: f32) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { reg(r) } {
            r.update_all(time_step);
        }
    }

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_update_action(
        r: *mut Registry,
        h: u64,
        time_step: f32,
    ) {
        // SAFETY: caller contract.
        if let (Some(r), Some(h)) = (unsafe { reg(r) }, handle(h)) {
            r.update(h, time_step);
        }
    }

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_reset_all(r: *mut Registry) {
        // SAFETY: caller contract.
        if let Some(r) = unsafe { reg(r) } {
            r.reset_all();
        }
    }

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_reset(r: *mut Registry, h: u64) {
        // SAFETY: caller contract.
        if let (Some(r), Some(h)) = (unsafe { reg(r) }, handle(h)) {
            r.reset_to_current_state(h);
        }
    }

    /// # Safety
    /// `r` live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_pop_registered(r: *mut Registry) -> *mut c_void {
        // SAFETY: caller contract.
        unsafe { reg(r) }.map_or(core::ptr::null_mut(), Registry::pop_registered)
    }

    /// # Safety
    /// `r` live; `kind` and `id` writable or null.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_input_reg_first_down(
        r: *mut Registry,
        kind: *mut u32,
        id: *mut i32,
    ) -> bool {
        // SAFETY: caller contract.
        let Some((k, i)) = unsafe { reg(r) }.and_then(|r| r.first_down()) else {
            return false;
        };
        // SAFETY: caller contract.
        if let Some(o) = unsafe { kind.as_mut() } {
            *o = k;
        }
        // SAFETY: caller contract.
        if let Some(o) = unsafe { id.as_mut() } {
            *o = i;
        }
        true
    }
}

#[cfg(test)]
#[path = "registry_tests.rs"]
mod tests;
