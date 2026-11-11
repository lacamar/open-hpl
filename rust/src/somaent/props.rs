//! `ApplyInstanceVars`: callbacks and interaction settings read from the `.ent` user variables
//! and the map's instance variables. C++ replays the ops in order.

use crate::string;

/// `cResourceVarsObject::GetVar*`; the value strings are parsed on the C++ side.
pub trait Vars {
    fn string(&self, name: &str) -> Vec<u8>;
    fn boolean(&self, name: &str, def: bool) -> bool;
    fn float(&self, name: &str, def: f32) -> f32;
    fn int(&self, name: &str, def: i32) -> i32;
}

#[derive(Debug, PartialEq)]
pub enum Op<'a> {
    InteractCallback {
        func: &'a [u8],
        auto_remove: bool,
    },
    LookAt {
        func: &'a [u8],
        auto_remove: bool,
        check_center: bool,
        check_ray: bool,
        max_distance: f32,
        delay: f32,
    },
    /// A collide callback: on the player's container when `to_player`, with this entity as child.
    CollideCallback {
        child: &'a [u8],
        func: &'a [u8],
        to_player: bool,
    },
    UserVar(&'a [u8]),
    ConnectionCallback(&'a [u8]),
    Connection {
        entity: &'a [u8],
        invert: bool,
        states_used: i32,
    },
    Health(f32),
    Breakable,
    MaxInteractDistance(f32),
    InteractionDisabled,
}

const SEPARATORS: &[u8] = b";, ";

pub fn apply_instance_vars(inst: &dyn Vars, vars: &dyn Vars, sink: &mut dyn FnMut(Op)) {
    let func = inst.string("PlayerInteractCallback");
    if !func.is_empty() {
        sink(Op::InteractCallback {
            func: &func,
            auto_remove: inst.boolean("PlayerInteractCallbackAutoRemove", false),
        });
    }
    let func = inst.string("PlayerLookAtCallback");
    if !func.is_empty() {
        sink(Op::LookAt {
            func: &func,
            auto_remove: inst.boolean("PlayerLookAtCallbackAutoRemove", false),
            check_center: inst.boolean("PlayerLookAtCheckCenterOfScreen", true),
            check_ray: inst.boolean("PlayerLookAtCheckRayIntersection", true),
            max_distance: inst.float("PlayerLookAtMaxDistance", -1.0),
            delay: inst.float("PlayerLookAtCallbackDelay", 0.0),
        });
    }
    let ents = inst.string("CC_Entities");
    let funcs = inst.string("CC_Funcs");
    let ents: Vec<&[u8]> = string::split(&ents, Some(SEPARATORS), true).collect();
    let funcs: Vec<&[u8]> = string::split(&funcs, Some(SEPARATORS), true).collect();
    if let Some(last) = funcs.len().checked_sub(1) {
        for (i, ent) in ents.iter().enumerate() {
            let func = funcs.get(i.min(last)).copied().unwrap_or_default();
            let to_player = string::to_lower(ent) == b"player";
            sink(Op::CollideCallback {
                child: if to_player { b"" } else { ent },
                func,
                to_player,
            });
        }
    }
    let user_var = inst.string("UserVar");
    if !user_var.is_empty() {
        sink(Op::UserVar(&user_var));
    }
    sink(Op::ConnectionCallback(
        &inst.string("ConnectionStateChangeCallback"),
    ));
    let connected = inst.string("ConnectedEntity");
    for entity in string::split(&connected, Some(SEPARATORS), true) {
        sink(Op::Connection {
            entity,
            invert: inst.boolean("ConnectedEntityInvertState", false),
            states_used: inst.int("ConnectedEntityStatesUsed", 0),
        });
    }
    sink(Op::Health(vars.float("Health", 100.0)));
    if vars.boolean("BreakActive", false) && !inst.boolean("DisableBreakable", false) {
        sink(Op::Breakable);
    }
    if inst.float("MaxInteractDistance", 0.0) > 0.0 {
        sink(Op::MaxInteractDistance(
            inst.float("MaxInteractDistance", 0.0),
        ));
    }
    if inst.boolean("InteractionDisabled", false) {
        sink(Op::InteractionDisabled);
    }
}
