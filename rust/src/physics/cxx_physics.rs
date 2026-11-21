//! `cPhysics` members defined in Rust under their Itanium symbols. `mRs` (offset 96) is the registry
//! handle and `mS` (offset 104) the `PhysState` pointer; offsets asserted in Physics.cpp.

use super::cxx::{CxxStr, ptr_member, val_member};
use super::ffi_h15p as p;
use super::preg::PhysState;

const RS_OFFSET: usize = 96;
const MS_OFFSET: usize = 104;

struct Phys<'a> {
    rs: u64,
    s: &'a mut PhysState,
}

/// # Safety
/// `this` is a live `cPhysics` whose constructor has run.
unsafe fn state<'a>(this: *const u8) -> Phys<'a> {
    // SAFETY: caller contract.
    unsafe {
        Phys {
            rs: val_member(this, RS_OFFSET),
            s: ptr_member(this, MS_OFFSET),
        }
    }
}

super::cxx::cxx! {
    state;
    "_ZN3hpl8cPhysics6UpdateEf" update[this, c](dt: f32) => p::ohpl_h15p_update_impacts(c.rs, dt, c.s.impact_duration);
    "_ZN3hpl8cPhysics11CreateWorldEb" create_world[this, c](surface: bool) -> usize => p::ohpl_h15p_create_world(c.rs, surface);
    "_ZN3hpl8cPhysics12DestroyWorldEPNS_13iPhysicsWorldE" destroy_world[this, c](w: usize) => p::ohpl_h15p_destroy_world(c.rs, w);
    "_ZN3hpl8cPhysics13CanPlayImpactEv" can_play[this, c]() -> bool => p::ohpl_h15p_can_play(c.rs, c.s.max_impacts);
    "_ZN3hpl8cPhysics9AddImpactEv" add_impact[this, c]() => p::ohpl_h15p_add_impact(c.rs);
    "_ZN3hpl8cPhysics15GetNumOfImpactsEv" impacts[this, c]() -> i32 => p::ohpl_h15p_impact_count(c.rs) as i32;
    "_ZN3hpl8cPhysics17CreateSurfaceDataERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" create_surface[this, c](n: *const CxxStr) -> usize => {
        let b = CxxStr::bytes(n);
        p::ohpl_h15p_create_surface(c.rs, b.as_ptr(), b.len())
    };
    "_ZN3hpl8cPhysics14GetSurfaceDataERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" get_surface[this, c](n: *const CxxStr) -> usize => {
        let b = CxxStr::bytes(n);
        p::ohpl_h15p_get_surface(c.rs, b.as_ptr(), b.len())
    };
    "_ZN3hpl8cPhysics15LoadSurfaceDataERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" load_surface[this, c](n: *const CxxStr) -> bool => {
        let b = CxxStr::bytes(n);
        p::ohpl_h15p_load_surface(c.rs, b.as_ptr(), b.len())
    };
}
