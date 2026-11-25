use super::cxx::{CxxStr, ptr_member, val_member};
use super::preg::{self, PhysState};

const RS_OFFSET: usize = 96;
const MS_OFFSET: usize = 104;

struct Phys<'a> {
    rs: u64,
    s: &'a mut PhysState,
}

unsafe fn state<'a>(this: *const u8) -> Phys<'a> {
    unsafe {
        Phys {
            rs: val_member(this, RS_OFFSET),
            s: ptr_member(this, MS_OFFSET),
        }
    }
}

super::cxx::cxx! {
    state;
    "_ZN3hpl8cPhysics6UpdateEf" update[this, c](dt: f32) => preg::update_impacts(c.rs, dt, c.s.impact_duration);
    "_ZN3hpl8cPhysics11CreateWorldEb" create_world[this, c](surface: bool) -> usize => preg::create_world(c.rs, surface);
    "_ZN3hpl8cPhysics12DestroyWorldEPNS_13iPhysicsWorldE" destroy_world[this, c](w: usize) => preg::destroy_world(c.rs, w);
    "_ZN3hpl8cPhysics13CanPlayImpactEv" can_play[this, c]() -> bool => preg::can_play(c.rs, c.s.max_impacts);
    "_ZN3hpl8cPhysics9AddImpactEv" add_impact[this, c]() => preg::add_impact(c.rs);
    "_ZN3hpl8cPhysics15GetNumOfImpactsEv" impacts[this, c]() -> i32 => preg::impact_count(c.rs) as i32;
    "_ZN3hpl8cPhysics17CreateSurfaceDataERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" create_surface[this, c](n: *const CxxStr) -> usize => preg::create_surface(c.rs, CxxStr::bytes(n));
    "_ZN3hpl8cPhysics14GetSurfaceDataERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" get_surface[this, c](n: *const CxxStr) -> usize => preg::get_surface(c.rs, CxxStr::bytes(n));
    "_ZN3hpl8cPhysics15LoadSurfaceDataERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" load_surface[this, c](n: *const CxxStr) -> bool => preg::load_surface(c.rs, CxxStr::bytes(n));
}
