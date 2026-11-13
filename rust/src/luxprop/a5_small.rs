//! A5: whole-prop state and methods of `cLuxProp_Button`, `_Photocell`, `_LevelDoor` and
//! `_OilBarrel`. C++ keeps the state structs as members and applies the ops.

use super::ops::{Op, Ops, Pod};
use super::smallprops as sp;
use super::vars::{Vars, put_var};
use crate::types::Vector3f;

pub mod a5op {
    pub const CONN_STATE: i32 = 1000 + 1;
    pub const SET_EFFECTS: i32 = 1000 + 2;
    pub const PHOTO_ILLUM: i32 = 1000 + 3;
    pub const DOOR_LOCKED_FEEDBACK: i32 = 1000 + 4;
    pub const DOOR_CHANGE_MAP: i32 = 1000 + 5;
    pub const OIL_SOUND: i32 = 1000 + 6;
    pub const OIL_MESSAGE: i32 = 1000 + 7;
    pub const OIL_SET_LAMP: i32 = 1000 + 8;

    pub const MSG_EMPTY: i32 = 0;
    pub const MSG_LANTERN_FULL: i32 = 1;
    pub const MSG_REFILL_FULL: i32 = 2;
    pub const MSG_REFILL_025: i32 = 3;
    pub const MSG_REFILL_050: i32 = 4;
    pub const MSG_REFILL_075: i32 = 5;
    pub const MSG_REFILL_100: i32 = 6;

    pub const SND_EMPTY: i32 = 0;
    pub const SND_FILL: i32 = 1;

    pub const SLOT_DOOR_ENTER: i32 = 200;
    pub const SLOT_DOOR_EXIT: i32 = 201;
    pub const SLOT_DOOR_TEXT_ENTRY: i32 = 202;
    pub const SLOT_DOOR_MAP_FILE: i32 = 203;
    pub const SLOT_DOOR_START_POS: i32 = 204;
    pub const SLOT_DOOR_LOCKED_SOUND: i32 = 205;
    pub const SLOT_DOOR_LOCKED_CAT: i32 = 206;
    pub const SLOT_DOOR_LOCKED_ENTRY: i32 = 207;
    pub const SLOT_OIL_FILL: i32 = 208;
    pub const SLOT_OIL_EMPTY: i32 = 209;
}
use a5op as o;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct A5Button {
    pub on: bool,
    pub can_on: bool,
    pub can_off: bool,
}

impl A5Button {
    pub fn new() -> Self {
        Self {
            on: true,
            ..Self::default()
        }
    }
}

fn button_set(p: Pod<A5Button>, new_on: bool, effects: bool, ops: &mut Ops) {
    if let Some(c) = sp::button_set(&mut p.m().on, new_on) {
        ops.event(Op::new(o::CONN_STATE).i(c));
        ops.push(
            Op::new(o::SET_EFFECTS)
                .i(i32::from(p.r().on))
                .j(i32::from(effects)),
        );
    }
}

pub fn button_load_instance(v: &dyn Vars, p: Pod<A5Button>, ops: &mut Ops) {
    button_set(p, v.boolean("SwitchedOn", false), false, ops);
    p.m().can_on = v.boolean("CanBeSwitchedOn", true);
    p.m().can_off = v.boolean("CanBeSwitchedOff", true);
}

pub fn button_can_interact(s: &A5Button) -> bool {
    sp::button_can_interact(s.on, s.can_on, s.can_off)
}

pub fn button_interact(p: Pod<A5Button>, ops: &mut Ops) {
    button_set(p, !p.r().on, true, ops);
}

pub fn button_set_on(p: Pod<A5Button>, on: bool, effects: bool, ops: &mut Ops) {
    button_set(p, on, effects, ops);
}

pub fn button_conn(p: Pod<A5Button>, state: i32, ops: &mut Ops) {
    if let Some(on) = sp::button_conn_state(state) {
        button_set(p, on, true, ops);
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct A5Photocell {
    pub lit: bool,
    pub light_level: f32,
    pub check_count: f32,
    pub on_limit: f32,
    pub off_limit: f32,
}

pub fn photocell_load_instance(v: &dyn Vars, s: &mut A5Photocell) {
    s.on_limit = v.float("LightLevelOnLimit", 0.0);
    s.off_limit = v.float("LightLevelOffLimit", 0.0);
}

/// Engine queries of the photocell's mesh and the map's light grid.
pub trait PhotoHost {
    /// Bounding volume size and world centre; only asked when a sample is due.
    fn bounds(&mut self) -> (Vector3f, Vector3f);
    fn light_at(&mut self, pos: Vector3f) -> f32;
    /// Current mesh illumination, `None` without a mesh.
    fn illumination(&mut self) -> Option<f32>;
}

pub fn photocell_update(p: Pod<A5Photocell>, dt: f32, host: &mut dyn PhotoHost, ops: &mut Ops) {
    if sp::photo_tick(&mut p.m().check_count, dt) {
        let (size, center) = host.bounds();
        let mut light = [0.0f32; 6];
        for (l, pos) in light.iter_mut().zip(sp::photo_samples(size, center)) {
            *l = host.light_at(pos);
        }
        p.m().light_level = sp::photo_level(&light);
        let s = *p.r();
        let c = sp::photo_switch(s.light_level, &mut p.m().lit, s.on_limit, s.off_limit);
        if c != 0 {
            ops.event(Op::new(o::CONN_STATE).i(c));
        }
    }
    let s = *p.r();
    let goal = sp::photo_goal(s.light_level, s.on_limit, s.off_limit);
    if let Some(cur) = host.illumination()
        && cur != goal
    {
        ops.push(Op::new(o::PHOTO_ILLUM).f(sp::photo_illum_step(cur, goal, dt)));
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct A5Door {
    pub locked: bool,
    pub show_stats: bool,
    pub locked_count: f32,
}

pub fn door_load(v: &dyn Vars) {
    put_var(v, o::SLOT_DOOR_ENTER, "EnterSound");
    put_var(v, o::SLOT_DOOR_EXIT, "ExitSound");
}

pub fn door_load_instance(v: &dyn Vars, s: &mut A5Door) {
    put_var(v, o::SLOT_DOOR_TEXT_ENTRY, "TextEntry");
    put_var(v, o::SLOT_DOOR_MAP_FILE, "MapFile");
    put_var(v, o::SLOT_DOOR_START_POS, "StartPos");
    s.locked = v.boolean("Locked", false);
    put_var(v, o::SLOT_DOOR_LOCKED_SOUND, "LockedSound");
    put_var(v, o::SLOT_DOOR_LOCKED_CAT, "LockedTextCat");
    put_var(v, o::SLOT_DOOR_LOCKED_ENTRY, "LockedTextEntry");
    s.show_stats = v.boolean("ShowStats", true);
}

pub fn door_update(s: &mut A5Door, dt: f32) {
    s.locked_count = sp::door_tick(s.locked_count, dt);
}

pub fn door_interact(p: Pod<A5Door>, ops: &mut Ops) {
    let act = sp::door_interact(p.r().locked, p.r().locked_count);
    if act.locked_feedback {
        ops.push(Op::new(o::DOOR_LOCKED_FEEDBACK));
        p.m().locked_count = act.count;
    }
    if act.change_map {
        ops.event(Op::new(o::DOOR_CHANGE_MAP));
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct A5Oil {
    pub calculated: bool,
    pub amount: f32,
}

pub fn oil_load(v: &dyn Vars) {
    put_var(v, o::SLOT_OIL_FILL, "FillSound");
    put_var(v, o::SLOT_OIL_EMPTY, "EmptySound");
}

/// Player state the barrel reads; `total_oil` is the lamp plus every lamp-oil item.
pub trait OilHost {
    fn hard_mode(&mut self) -> bool;
    fn lamp_oil(&mut self) -> f32;
    fn total_oil(&mut self) -> f32;
}

fn oil_calculate(s: &mut A5Oil, host: &mut dyn OilHost) {
    if s.calculated {
        return;
    }
    s.amount = if host.hard_mode() {
        10.0
    } else {
        sp::oil_amount(host.total_oil())
    };
    s.calculated = true;
}

pub fn oil_focus_tier(s: &mut A5Oil, host: &mut dyn OilHost) -> i32 {
    oil_calculate(s, host);
    sp::oil_focus_tier(s.amount)
}

/// Returns the `OnInteract` result.
pub fn oil_interact(s: &mut A5Oil, host: &mut dyn OilHost, ops: &mut Ops) -> bool {
    oil_calculate(s, host);
    let fill = sp::oil_fill(s.amount, host.lamp_oil());
    if fill.result == sp::OIL_EMPTY {
        ops.push(Op::new(o::OIL_SOUND).i(o::SND_EMPTY));
        ops.push(Op::new(o::OIL_MESSAGE).i(o::MSG_EMPTY));
        return false;
    }
    if fill.result == sp::OIL_FULL {
        ops.push(Op::new(o::OIL_MESSAGE).i(o::MSG_LANTERN_FULL));
        return false;
    }
    s.amount = fill.barrel;
    ops.push(Op::new(o::OIL_SET_LAMP).f(fill.lamp));
    ops.push(Op::new(o::OIL_SOUND).i(o::SND_FILL));
    let msg = match fill.tier {
        0 => o::MSG_REFILL_FULL,
        1 => o::MSG_REFILL_025,
        2 => o::MSG_REFILL_050,
        3 => o::MSG_REFILL_075,
        _ => o::MSG_REFILL_100,
    };
    ops.push(Op::new(o::OIL_MESSAGE).i(msg));
    true
}
