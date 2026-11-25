//! `cLuxPlayerHands` driver (A2c): hands state, charge/alpha vars, camera-rotation smoothing and the object switch plan.
//! The hands entity, hand objects and animations stay C++ behind the host call (`a2d_disp::DHost`).

use super::a2d_disp::{DHost, ring};
use super::hands::{self, HandsPos};
use super::ops::Ops;
use crate::types::Vector3f;
use std::collections::LinkedList;

pub mod hk {
    pub const HAS_OBJECT: u32 = 1;
    pub const ANIM_OVER: u32 = 2;
    pub const OBJ_ANIM_OVER: u32 = 3;
    pub const ATTACH_DRAW: u32 = 4;
    pub const DISABLE: u32 = 5;
    pub const IDLE: u32 = 6;
    pub const UPDATE_OBJECTS: u32 = 7;
    pub const OBJ_DO_ACTION: u32 = 8;
    pub const SAME_OBJECT: u32 = 9;
    pub const HOLSTER: u32 = 10;
    pub const DRAW_NEW: u32 = 11;
    pub const SET_CURRENT: u32 = 12;
    pub const RESET_PTRS: u32 = 13;
    pub const HAS_ENTITY: u32 = 14;
    pub const FILL_ROT: u32 = 15;
    pub const FILL_POS: u32 = 16;
    pub const SET_MATRIX: u32 = 17;
}

#[repr(C)]
pub struct HandsD {
    ring: *mut LinkedList<Vector3f>,
    pub state: i32,
    pub max_rotations: i32,
    pub pos_add_mul: f32,
    pub charge_count: f32,
    pub object_state: i32,
    pub alpha: f32,
    pub attack_down: bool,
    pub interact_down: bool,
}

const _: () = assert!(std::mem::size_of::<HandsD>() == 40);

/// # Safety
/// `d` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_hands_init(d: *mut HandsD, max_rotations: i32, pos_add_mul: f32) {
    // SAFETY: caller contract.
    unsafe {
        (*d).max_rotations = max_rotations;
        (*d).pos_add_mul = pos_add_mul;
    }
}

/// # Safety
/// `d` initialised and not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_hands_free(d: *mut HandsD) {
    // SAFETY: caller contract.
    unsafe {
        if !(*d).ring.is_null() {
            drop(Box::from_raw((*d).ring));
            (*d).ring = std::ptr::null_mut();
        }
    }
}

/// # Safety
/// `d` and `dh` valid; the host may call back.
unsafe fn reset_vars(d: *mut HandsD) {
    // SAFETY: caller contract.
    unsafe {
        (*d).charge_count = 0.0;
        (*d).object_state = 0;
        (*d).attack_down = false;
        (*d).interact_down = false;
    }
}

/// # Safety
/// As `ohpl_a2d_hands_init`; `dh` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_hands_reset(d: *mut HandsD, dh: *const DHost) {
    // SAFETY: caller contract.
    unsafe {
        (*dh).c(hk::RESET_PTRS, 0, 0.0, 0.0);
        (*d).state = hands::DISABLED;
        (*d).alpha = 0.0;
        if let Some(r) = (*d).ring.as_mut() {
            r.clear();
        }
        reset_vars(d);
    }
}

/// # Safety
/// As `ohpl_a2d_hands_reset`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_hands_clear_rotations(d: *mut HandsD) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(r) = (*d).ring.as_mut() {
            r.clear();
        }
    }
}

/// # Safety
/// As `ohpl_a2d_hands_reset`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_hands_update(d: *mut HandsD, dh: *const DHost, dt: f32) {
    // SAFETY: caller contract; the state is re-read after every host call that may call back.
    unsafe {
        let dh = &*dh;
        let state = (*d).state;
        let has_object = dh.cb(hk::HAS_OBJECT, 0, 0.0, 0.0);
        let mut anim_over = false;
        if state == hands::HOLSTER
            || state == hands::DRAW
            || (state == hands::HAND_OBJECT && has_object)
        {
            anim_over = dh.cb(hk::ANIM_OVER, 0, 0.0, 0.0);
        }
        let mut object_anim_over = false;
        if state == hands::HAND_OBJECT && has_object && anim_over {
            object_anim_over = dh.cb(hk::OBJ_ANIM_OVER, 0, 0.0, 0.0);
        }
        let mut o = Ops::default();
        let (new_state, run) =
            hands::hands_update(state, has_object, anim_over, object_anim_over, &mut o);
        for op in o.ops.iter().take(o.n as usize) {
            match op.kind {
                hands::k::ATTACH_DRAW => dh.c(hk::ATTACH_DRAW, 0, 0.0, 0.0),
                hands::k::DISABLE => dh.c(hk::DISABLE, 0, 0.0, 0.0),
                hands::k::IDLE => dh.c(hk::IDLE, 0, 0.0, 0.0),
                _ => 0.0,
            };
        }
        (*d).state = new_state;
        if run {
            dh.c(hk::UPDATE_OBJECTS, 0, dt, 0.0);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_hands_reset`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_hands_do_action(
    d: *mut HandsD,
    dh: *const DHost,
    action: i32,
    pressed: bool,
) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if !hands::hands_action_gate((*d).state, dh.cb(hk::HAS_OBJECT, 0, 0.0, 0.0)) {
            return;
        }
        if dh.cb(
            hk::OBJ_DO_ACTION,
            0,
            action as f32,
            f32::from(u8::from(pressed)),
        ) {
            (*d).state = hands::HAND_OBJECT;
        }
    }
}

/// # Safety
/// As `ohpl_a2d_hands_reset`; `obj` is only passed through to the host (null = holster).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_hands_set_object(
    d: *mut HandsD,
    dh: *const DHost,
    obj: *mut std::ffi::c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if (dh.call)(dh.ctx, hk::SAME_OBJECT, 0, 0.0, 0.0, obj) != 0.0 {
            return;
        }
        reset_vars(d);
        match hands::hands_switch_plan(dh.cb(hk::HAS_OBJECT, 0, 0.0, 0.0), !obj.is_null()) {
            hands::PLAN_HOLSTER => {
                dh.c(hk::HOLSTER, 0, 0.0, 0.0);
                (*d).state = hands::HOLSTER;
            }
            hands::PLAN_DRAW => {
                (dh.call)(dh.ctx, hk::DRAW_NEW, 0, 0.0, 0.0, obj);
                (*d).state = hands::DRAW;
            }
            _ => {}
        }
        (dh.call)(dh.ctx, hk::SET_CURRENT, 0, 0.0, 0.0, obj);
    }
}

/// # Safety
/// As `ohpl_a2d_hands_reset`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_hands_post_update(d: *mut HandsD, dh: *const DHost) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        if !dh.cb(hk::HAS_ENTITY, 0, 0.0, 0.0) {
            return;
        }
        let mut rot = [0.0f32; 3];
        (dh.call)(dh.ctx, hk::FILL_ROT, 0, 0.0, 0.0, rot.as_mut_ptr().cast());
        let Some(r) = ring(&mut (*d).ring) else {
            return;
        };
        r.push_back(Vector3f {
            x: rot[0],
            y: rot[1],
            z: rot[2],
        });
        if hands::rotations_pop(r.len(), (*d).max_rotations) {
            r.pop_front();
        }
        let v: Vec<Vector3f> = r.iter().copied().collect();
        let fin = hands::hands_final_rotation(&v);
        let mut buf = [0.0f32; 21];
        (dh.call)(dh.ctx, hk::FILL_POS, 0, 0.0, 0.0, buf.as_mut_ptr().cast());
        let mut it = buf
            .as_chunks::<3>()
            .0
            .iter()
            .map(|&[x, y, z]| Vector3f { x, y, z });
        let mut next = || it.next().unwrap_or_default();
        let p = HandsPos {
            cam_pos: next(),
            right: next(),
            up: next(),
            forward: next(),
            bob: next(),
            shake: next(),
            hurt: next(),
            pos_add_mul: (*d).pos_add_mul,
        };
        let pos = hands::hands_translation(&p);
        let mut out = [fin.x, fin.y, fin.z, pos.x, pos.y, pos.z];
        (dh.call)(dh.ctx, hk::SET_MATRIX, 0, 0.0, 0.0, out.as_mut_ptr().cast());
    }
}
