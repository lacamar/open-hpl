//! State of `iLuxEntity`: scalars (C++ reads them through a prefix mirror), the five strings and
//! the connection list. Allocated and freed here.
#![allow(clippy::missing_safety_doc, clippy::too_many_arguments)]

use crate::luxprop::entc::{self, ConnHost, EntityCb};
use crate::luxprop::ffi::Sink;
use crate::luxprop::ffi_b::LosRef;
use crate::luxprop::ffi_c::LookHostRef;
use crate::luxprop::opc as op_c;
use crate::luxprop::ops::{Op, Ops, Pod};
use std::ffi::c_void;

pub const STR_NAME: i32 = 0;
pub const STR_CALLBACK: i32 = 1;
pub const STR_CONN_CHANGE: i32 = 2;
pub const STR_INTERACT: i32 = 3;
pub const STR_LOOKAT: i32 = 4;

#[derive(Debug)]
pub struct Conn {
    name: Vec<u8>,
    entity: *mut c_void,
    entity_state: *mut EntState,
    invert: bool,
    states: i32,
    callback: Vec<u8>,
}

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
    strs: [Vec<u8>; 5],
    conns: Vec<Conn>,
    scratch: Vec<u8>,
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
            strs: Default::default(),
            conns: Vec::new(),
            scratch: Vec::new(),
        }
    }

    fn str_at(&self, kind: i32) -> &[u8] {
        usize::try_from(kind)
            .ok()
            .and_then(|k| self.strs.get(k))
            .map_or(&[], Vec::as_slice)
    }

    fn clear(&mut self, kind: i32) {
        if let Some(v) = usize::try_from(kind)
            .ok()
            .and_then(|k| self.strs.get_mut(k))
        {
            v.clear();
        }
    }

    fn has(&self, kind: i32) -> bool {
        !self.str_at(kind).is_empty()
    }

    fn script(&mut self, code: i32, i: i32, j: i32, ty: &[u8]) -> bool {
        let name = self.str_at(STR_NAME).to_vec();
        let q = |out: &mut Vec<u8>, b: &[u8]| {
            out.push(b'"');
            out.extend_from_slice(b);
            out.push(b'"');
        };
        let mut out = Vec::new();
        match code {
            op_c::ENT_CALLBACK => {
                out.extend_from_slice(self.str_at(STR_CALLBACK));
                out.push(b'(');
                q(&mut out, &name);
                out.extend_from_slice(b", ");
                q(&mut out, ty);
                out.push(b')');
            }
            op_c::ENT_INTERACT => {
                out.extend_from_slice(self.str_at(STR_INTERACT));
                out.push(b'(');
                q(&mut out, &name);
                out.push(b')');
            }
            op_c::ENT_CLEAR_INTERACT => {
                self.clear(STR_INTERACT);
                return false;
            }
            op_c::ENT_LOOKAT_ENTER => {
                out.extend_from_slice(self.str_at(STR_LOOKAT));
                if i != 0 {
                    self.clear(STR_LOOKAT);
                }
                out.push(b'(');
                q(&mut out, &name);
                out.extend_from_slice(b", 1)");
            }
            op_c::ENT_LOOKAT_LEAVE => {
                out.extend_from_slice(self.str_at(STR_LOOKAT));
                out.push(b'(');
                q(&mut out, &name);
                out.extend_from_slice(b", -1)");
            }
            op_c::ENT_CONN_CHANGE_CB => {
                out.extend_from_slice(self.str_at(STR_CONN_CHANGE));
                out.push(b'(');
                q(&mut out, &name);
                out.extend_from_slice(format!(", {i})").as_bytes());
            }
            op_c::ENT_CONN_SCRIPT => {
                let Some(c) = usize::try_from(i).ok().and_then(|i| self.conns.get(i)) else {
                    return false;
                };
                let child = if c.entity_state.is_null() {
                    &[][..]
                } else {
                    // SAFETY: the connected entity outlives its parent's connection, as in C++.
                    unsafe { (*c.entity_state).str_at(STR_NAME) }
                };
                out.extend_from_slice(&c.callback);
                out.push(b'(');
                q(&mut out, &c.name);
                out.push(b',');
                q(&mut out, &name);
                out.push(b',');
                q(&mut out, child);
                out.extend_from_slice(format!(",{j})").as_bytes());
            }
            _ => return false,
        }
        self.scratch = out;
        true
    }
}

struct StateConns(*mut EntState);

impl ConnHost for StateConns {
    fn conn_num(&mut self) -> i32 {
        // SAFETY: the entity state outlives the call; borrows are not held across events.
        i32::try_from(unsafe { (*self.0).conns.len() }).unwrap_or(i32::MAX)
    }
    fn conn(&mut self, i: i32) -> (i32, bool) {
        // SAFETY: as above.
        let s = unsafe { &*self.0 };
        usize::try_from(i)
            .ok()
            .and_then(|i| s.conns.get(i))
            .map_or((0, false), |c| (c.states, c.invert))
    }
    fn conn_has_callback(&mut self, i: i32) -> bool {
        // SAFETY: as above.
        let s = unsafe { &*self.0 };
        usize::try_from(i)
            .ok()
            .and_then(|i| s.conns.get(i))
            .is_some_and(|c| !c.callback.is_empty())
    }
}

fn st<'a>(s: *const EntState) -> &'a EntState {
    // SAFETY: C++ passes the live state of its entity.
    unsafe { &*s }
}

fn stm<'a>(s: *mut EntState) -> &'a mut EntState {
    // SAFETY: as above; no other borrow is live (callers borrow only between events).
    unsafe { &mut *s }
}

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract: `p` holds `n` bytes.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

fn with_ops<R>(sink: *const Sink, f: impl FnOnce(&mut Ops) -> R) -> R {
    // SAFETY: caller contract: the sink stays valid for the call.
    let (ctx, push) = unsafe { ((*sink).ctx, (*sink).push) };
    let mut fwd = move |op: &Op| {
        // SAFETY: as above.
        unsafe { push(ctx, op) }
    };
    f(&mut Ops(&mut fwd))
}

fn cb_pod(s: *mut EntState) -> Pod<EntityCb> {
    // SAFETY: `s` is live state; `cb` lives as long as it.
    unsafe { Pod::from_ptr(&raw mut (*s).cb) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_str(s: *const EntState, kind: i32, len: *mut usize) -> *const u8 {
    let v = st(s).str_at(kind);
    // SAFETY: caller passes a valid out pointer.
    unsafe { *len = v.len() };
    v.as_ptr()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_str_set(s: *mut EntState, kind: i32, p: *const u8, n: usize) {
    if let Some(v) = usize::try_from(kind)
        .ok()
        .and_then(|k| stm(s).strs.get_mut(k))
    {
        *v = bytes(p, n).to_vec();
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_script(
    s: *mut EntState,
    code: i32,
    i: i32,
    j: i32,
    ty: *const u8,
    ty_len: usize,
    len: *mut usize,
) -> *const u8 {
    let me = stm(s);
    if !me.script(code, i, j, bytes(ty, ty_len)) {
        return std::ptr::null();
    }
    // SAFETY: caller passes a valid out pointer.
    unsafe { *len = me.scratch.len() };
    me.scratch.as_ptr()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_run_callback(s: *mut EntState, sink: *const Sink) {
    let has = st(s).has(STR_CALLBACK);
    with_ops(sink, |o| entc::run_callback(has, o));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_run_interact(s: *mut EntState, sink: *const Sink) {
    let has = st(s).has(STR_INTERACT);
    with_ops(sink, |o| entc::run_interact(cb_pod(s), has, o));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_look_at(
    s: *mut EntState,
    dt: f32,
    host: *const LookHostRef,
    los: *const LosRef,
    sink: *const Sink,
) {
    let has = st(s).has(STR_LOOKAT);
    // SAFETY: caller contract.
    let (mut host, mut los) = unsafe { (&*host, &*los) };
    with_ops(sink, |o| {
        entc::update_look_at(cb_pod(s), has, dt, &mut host, &mut los, o);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_conn_change(s: *mut EntState, state: i32, sink: *const Sink) {
    let has = st(s).has(STR_CONN_CHANGE);
    with_ops(sink, |o| {
        entc::connection_state_change(state, has, &mut StateConns(s), o);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_conn_add(
    s: *mut EntState,
    name: *const u8,
    name_len: usize,
    entity: *mut c_void,
    entity_state: *mut EntState,
    invert: bool,
    states: i32,
    cb: *const u8,
    cb_len: usize,
) {
    stm(s).conns.push(Conn {
        name: bytes(name, name_len).to_vec(),
        entity,
        entity_state,
        invert,
        states,
        callback: bytes(cb, cb_len).to_vec(),
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_conn_clear(s: *mut EntState) {
    stm(s).conns.clear();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_conn_num(s: *const EntState) -> i32 {
    i32::try_from(st(s).conns.len()).unwrap_or(i32::MAX)
}

/// Name (`kind` 0) or callback (1) of connection `i`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_conn_str(
    s: *const EntState,
    i: i32,
    kind: i32,
    len: *mut usize,
) -> *const u8 {
    let v = usize::try_from(i)
        .ok()
        .and_then(|i| st(s).conns.get(i))
        .map_or(&[][..], |c| if kind == 0 { &c.name } else { &c.callback });
    // SAFETY: caller passes a valid out pointer.
    unsafe { *len = v.len() };
    v.as_ptr()
}

/// Connected entity id (-1 when unresolved), invert flag and states used of connection `i`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_conn_info(
    s: *const EntState,
    i: i32,
    entity_id: *mut i32,
    invert: *mut bool,
    states: *mut i32,
) {
    let Some(c) = usize::try_from(i).ok().and_then(|i| st(s).conns.get(i)) else {
        return;
    };
    // SAFETY: caller passes valid out pointers; the connected entity is live.
    unsafe {
        *entity_id = if c.entity_state.is_null() {
            -1
        } else {
            (*c.entity_state).id
        };
        *invert = c.invert;
        *states = c.states;
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_conn_entity(s: *const EntState, i: i32) -> *mut c_void {
    usize::try_from(i)
        .ok()
        .and_then(|i| st(s).conns.get(i))
        .map_or(std::ptr::null_mut(), |c| c.entity)
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
    fn scripts_and_connections() {
        let mut s = EntState::new(1, 0, std::ptr::null_mut());
        s.strs[STR_NAME as usize] = b"e".to_vec();
        s.strs[STR_CALLBACK as usize] = b"cb".to_vec();
        assert!(s.script(op_c::ENT_CALLBACK, 0, 0, b"t"));
        assert_eq!(s.scratch, b"cb(\"e\", \"t\")");
        s.strs[STR_LOOKAT as usize] = b"look".to_vec();
        assert!(s.script(op_c::ENT_LOOKAT_ENTER, 1, 0, b""));
        assert_eq!(s.scratch, b"look(\"e\", 1)");
        assert!(s.strs[STR_LOOKAT as usize].is_empty());
        let mut child = EntState::new(9, 0, std::ptr::null_mut());
        child.strs[STR_NAME as usize] = b"kid".to_vec();
        s.conns.push(Conn {
            name: b"c".to_vec(),
            entity: std::ptr::null_mut(),
            entity_state: &raw mut child,
            invert: false,
            states: 1,
            callback: b"f".to_vec(),
        });
        assert!(s.script(op_c::ENT_CONN_SCRIPT, 0, 2, b""));
        assert_eq!(s.scratch, b"f(\"c\",\"e\",\"kid\",2)");
    }

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
        assert_eq!(std::mem::offset_of!(EntState, strs), 48);
    }
}
