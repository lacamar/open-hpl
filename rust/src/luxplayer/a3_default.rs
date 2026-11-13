//! `iLuxPlayerState_DefaultBase` (focus, outline, interact action) with the `Normal`, `HandObject` and `UseItem`
//! variants, and the `iLuxPlayerState_Interact` base shared by the grab, push, slide and rotate states.

use super::a3_host::{Host, Ptr};
use super::a3_state::{Base, Kind, UseItem, act, ps};
use super::states_base as sb;
use super::states_vec::madd;
use crate::types::{Vector2f, Vector3f};

macro_rules! s {
    ($p:expr) => {
        // SAFETY: every driver gets valid state pointers from the C++ shell and keeps each borrow
        // inside one statement, never across a host call that can re-enter the state.
        unsafe { &mut *$p }
    };
}
pub(super) use s;

pub const CROSSHAIR_NONE: i32 = -3;
pub const CROSSHAIR_DEFAULT: i32 = -1;
pub const CROSSHAIR_SIMPLE: i32 = -2;
pub const CROSSHAIR_ITEM: i32 = -4;

const CROSSHAIR_LAST: i32 = 7;
const FOCUS_RANGE: f32 = 20.0;

pub fn act_of(raw: i32) -> i32 {
    match raw {
        act::HOLSTER => sb::ACT_HOLSTER,
        act::ATTACK => sb::ACT_ATTACK,
        act::INTERACT => sb::ACT_INTERACT,
        act::LANTERN => sb::ACT_LANTERN,
        _ => 0,
    }
}

pub fn kind(p: *mut Base) -> Option<Kind> {
    Kind::from_i32(s!(p).kind)
}

pub unsafe fn enter(p: *mut Base, h: &Host, kind: Kind, prev: i32) {
    if kind.is_default_base() {
        s!(p).body_in_focus = std::ptr::null_mut();
        s!(p).entity_in_focus = std::ptr::null_mut();
        match kind {
            Kind::HandObject => h.hand_drawn(true),
            Kind::UseItem => {
                let item = h.take_inventory_item();
                s!(p.cast::<UseItem>()).item = item;
            }
            _ => {}
        }
    }
    let _ = prev;
}

pub unsafe fn leave(p: *mut Base, h: &Host, kind: Kind, new_state: i32) {
    if !kind.is_default_base() {
        return;
    }
    h.fx_clear_outline();
    if kind == Kind::HandObject {
        let new = match new_state {
            ps::NORMAL => sb::PS_NORMAL,
            ps::PUSH => sb::PS_PUSH,
            ps::GRAB => sb::PS_GRAB,
            _ => 9,
        };
        if sb::hand_object_hides(new) {
            h.hand_drawn(false);
        }
    }
    let _ = p;
}

pub unsafe fn map_leave(p: *mut Base, kind: Kind) {
    if kind.is_default_base() {
        s!(p).body_in_focus = std::ptr::null_mut();
        s!(p).entity_in_focus = std::ptr::null_mut();
    }
}

fn entity_max_focus(h: &Host, e: Ptr) -> f32 {
    if e.is_null() { 0.0 } else { h.ent_max_focus(e) }
}

unsafe fn in_range(p: *mut Base, h: &Host) -> bool {
    let (e, d) = (s!(p).entity_in_focus, s!(p).focus_distance);
    sb::focus_in_range(!e.is_null(), d, entity_max_focus(h, e))
}

unsafe fn can_interact_with_entity(p: *mut Base, h: &Host) -> bool {
    // SAFETY: caller contract.
    let in_range = unsafe { in_range(p, h) };
    let (e, b) = (s!(p).entity_in_focus, s!(p).body_in_focus);
    let can = in_range && h.ent_can_interact(e, b);
    let disabled = can && h.ent_disabled(e);
    sb::focus_can_interact(in_range, can, disabled)
}

fn attached_bodies(h: &Host, body: Ptr, out: &mut Vec<Ptr>) {
    out.push(body);
    for i in 0..h.body_joint_num(body) {
        let joint = h.body_joint(body, i);
        let child = h.joint_child(joint);
        let parent = h.joint_parent(joint);
        if !child.is_null() && !out.contains(&child) {
            attached_bodies(h, child, out);
        }
        if !parent.is_null() && !out.contains(&parent) {
            attached_bodies(h, parent, out);
        }
    }
}

unsafe fn show_outline(p: *mut Base, h: &Host, kind: Kind) -> bool {
    if kind == Kind::UseItem {
        return false;
    }
    let (e, b) = (s!(p).entity_in_focus, s!(p).body_in_focus);
    if !h.ent_is_prop(e) {
        return false;
    }
    let item = h.prop_is_item(e);
    let can = item && h.ent_can_interact(e, b);
    let disabled = can && h.ent_disabled(e);
    sb::show_outline(item, can, disabled)
}

unsafe fn add_outline_objects(p: *mut Base, h: &Host, kind: Kind) {
    h.fx_clear_outline();
    let (e, b, d) = (
        s!(p).entity_in_focus,
        s!(p).body_in_focus,
        s!(p).focus_distance,
    );
    let gate = sb::outline_gate(
        !e.is_null(),
        !e.is_null() && h.ent_is_prop(e),
        d,
        entity_max_focus(h, e),
    );
    // SAFETY: caller contract.
    if !gate || !unsafe { show_outline(p, h, kind) } {
        s!(p).has_outline = false;
        return;
    }
    s!(p).has_outline = true;
    let mesh_of = || h.prop_effect_mesh(e);
    if h.prop_connected(e) {
        let mut attached = Vec::new();
        attached_bodies(h, b, &mut attached);
        let mesh = mesh_of();
        if mesh.is_null() {
            return;
        }
        for i in 0..h.mesh_sub_num(mesh) {
            if attached.contains(&h.mesh_sub_parent(mesh, i)) {
                h.mesh_sub_outline(mesh, i);
            }
        }
    } else {
        let mesh = mesh_of();
        if mesh.is_null() {
            return;
        }
        for i in 0..h.mesh_sub_num(mesh) {
            if h.mesh_sub_parent(mesh, i) == b {
                h.mesh_sub_outline(mesh, i);
            }
        }
    }
}

pub unsafe fn update(p: *mut Base, h: &Host, kind: Kind, dt: f32) {
    if !kind.is_default_base() {
        return;
    }
    let (pos, fwd) = (h.cam_pos(), h.cam_forward());
    let (dist, body, entity) = h.closest_entity(pos, fwd, FOCUS_RANGE);
    s!(p).focus_distance = dist;
    s!(p).body_in_focus = body;
    s!(p).entity_in_focus = entity;
    s!(p).focus_pos = madd(fwd, dist, pos);
    // SAFETY: caller contract.
    unsafe { add_outline_objects(p, h, kind) };
    h.set_focus(entity, body, dist);
    // SAFETY: caller contract.
    if !entity.is_null() && unsafe { can_interact_with_entity(p, h) } {
        h.set_focus_text(entity);
    } else {
        h.set_focus_text(std::ptr::null_mut());
    }
    if kind == Kind::UseItem {
        s!(p.cast::<UseItem>()).flash.update(dt);
    }
}

unsafe fn use_item_max_focus(p: *mut Base, h: &Host, e: Ptr) -> f32 {
    sb::use_item_max_focus(h.ent_max_focus(e), s!(p.cast::<UseItem>()).min_use_distance)
}

unsafe fn use_item(p: *mut Base, h: &Host) {
    let e = s!(p).entity_in_focus;
    let map = h.map_exists();
    // SAFETY: caller contract.
    let max = if e.is_null() {
        0.0
    } else {
        unsafe { use_item_max_focus(p, h, e) }
    };
    let item = s!(p.cast::<UseItem>()).item;
    let dist = s!(p).focus_distance;
    match sb::use_item_focus(!item.is_null(), !e.is_null(), map, dist, max) {
        sb::PLAN_NONE => {}
        sb::PLAN_CALLBACK => {
            let cb = h.use_cb_get(item, e);
            if sb::use_item_plan(!cb.is_null()) == sb::PLAN_CALLBACK {
                if h.use_cb_run(cb) {
                    h.use_cb_remove(cb);
                }
            } else {
                h.message(0);
            }
        }
        _ => h.message(1),
    }
}

/// `DefaultBase::OnDoAction`; the result is the C++ return value.
pub unsafe fn do_action(p: *mut Base, h: &Host, kind: Kind, raw: i32, pressed: bool) -> bool {
    if !kind.is_default_base() || !h.move_allows_interaction() {
        return true;
    }
    let a = act_of(raw);
    let implemented = match kind {
        Kind::Normal => {
            let r = sb::normal_action(a, pressed);
            if r.change == sb::PS_HAND {
                h.change_state(ps::HAND_OBJECT);
            }
            r.pass != 0
        }
        Kind::HandObject => {
            let r = sb::hand_object_action(a, pressed);
            if r.change == sb::PS_NORMAL {
                h.change_state(ps::NORMAL);
            }
            if r.forward_to_hands != 0 {
                h.hands_do_action(raw, pressed);
            }
            r.pass != 0
        }
        _ => {
            let r = sb::use_item_action(a, pressed);
            if r.use_item != 0 {
                // SAFETY: caller contract.
                unsafe { use_item(p, h) };
            }
            if r.change == sb::PS_HAND {
                h.change_state(ps::HAND_OBJECT);
            } else if r.change == sb::PS_NORMAL {
                h.change_state(ps::NORMAL);
            }
            r.pass != 0
        }
    };
    if !implemented {
        return false;
    }
    // SAFETY: caller contract.
    let can = raw == act::INTERACT && pressed && unsafe { can_interact_with_entity(p, h) };
    let (pass, interact) = sb::default_base_interact(a, pressed, can);
    if interact {
        let (e, b, pos) = (s!(p).entity_in_focus, s!(p).body_in_focus, s!(p).focus_pos);
        h.ent_on_interact(e, b, pos);
        h.ent_interact_cb(e);
    }
    pass
}

/// `GetCrosshair`: a `CROSSHAIR_*` code or a focus gfx index.
pub unsafe fn crosshair(p: *mut Base, h: &Host, kind: Kind) -> i32 {
    match kind {
        Kind::UseItem => {
            if s!(p.cast::<UseItem>()).item.is_null() {
                CROSSHAIR_NONE
            } else {
                CROSSHAIR_ITEM
            }
        }
        Kind::Normal | Kind::HandObject => {
            // SAFETY: caller contract.
            if !unsafe { in_range(p, h) } {
                return CROSSHAIR_DEFAULT;
            }
            let (e, b, pos) = (s!(p).entity_in_focus, s!(p).body_in_focus, s!(p).focus_pos);
            let c = h.ent_crosshair(e, b, pos);
            let pick = sb::crosshair_pick(
                c,
                0,
                CROSSHAIR_LAST,
                h.ent_disabled(e),
                h.icon_style_default(),
            );
            match pick {
                sb::CROSSHAIR_DEFAULT_GFX => CROSSHAIR_DEFAULT,
                sb::CROSSHAIR_SIMPLE_GFX => CROSSHAIR_SIMPLE,
                n => n,
            }
        }
        _ => CROSSHAIR_NONE,
    }
}

pub unsafe fn draw(p: *mut Base, h: &Host, kind: Kind, gui_set: Ptr, frame_time: f32) {
    if !matches!(kind, Kind::Normal | Kind::HandObject) {
        return;
    }
    // SAFETY: caller contract.
    if !s!(p).entity_in_focus.is_null() && unsafe { can_interact_with_entity(p, h) } {
        h.ent_focus_draw(s!(p).entity_in_focus, gui_set, frame_time);
    }
}

pub unsafe fn draw_crosshair(
    p: *mut Base,
    h: &Host,
    kind: Kind,
    gfx: Ptr,
    pos: Vector3f,
    size: Vector2f,
) -> bool {
    if kind != Kind::UseItem {
        return true;
    }
    let e = s!(p).entity_in_focus;
    if e.is_null() {
        return true;
    }
    // SAFETY: caller contract.
    let max = unsafe { use_item_max_focus(p, h, e) };
    let is_prop = h.ent_is_prop(e);
    let is_item = is_prop && h.prop_is_item(e);
    let dist = s!(p).focus_distance;
    match sb::use_item_draw(true, dist, max, is_prop, is_item) {
        sb::DRAW_NO => return true,
        sb::DRAW_CHECK_CALLBACK if h.use_cb_get(s!(p.cast::<UseItem>()).item, e).is_null() => {
            return true;
        }
        _ => {}
    }
    let pos = Vector3f {
        x: pos.x,
        y: pos.y,
        z: pos.z + 1.0,
    };
    for _ in 0..3 {
        h.gui_draw_gfx(gfx, pos, size, s!(p.cast::<UseItem>()).flash.val);
    }
    true
}

pub unsafe fn setup_interact_vars(p: *mut Base, h: &Host) {
    let (prop, body, pos) = h.vars_interact();
    s!(p).cur_prop = prop;
    s!(p).cur_body = body;
    s!(p).cur_focus_pos = pos;
    h.prop_set_interacted(prop, true);
}

pub unsafe fn reset_interact_vars(p: *mut Base, h: &Host) {
    h.prop_set_interacted(s!(p).cur_prop, false);
}

pub unsafe fn on_destroy_entity(p: *mut Base, h: &Host, e: Ptr) {
    if h.prop_as_entity(s!(p).cur_prop) == e {
        let prev = h.prev_state();
        h.change_state(prev);
    }
}

pub unsafe fn on_attach_sticky(p: *mut Base, h: &Host, body: Ptr) {
    let prop = s!(p).cur_prop;
    let found = (0..h.prop_body_num(prop)).any(|i| h.prop_body(prop, i) == body);
    if found {
        let prev = h.prev_state();
        h.change_state(prev);
    }
}
