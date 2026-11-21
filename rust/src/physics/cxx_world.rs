//! `iPhysicsWorld` members defined in Rust under their Itanium symbols. `mRsBook` (offset 32) is the
//! object registry handle and `mhRust` (offset 176) the Newton world handle; both asserted in PhysicsWorld.cpp.

use super::cxx::{CxxStr, val_member};
use super::ffi_h15w as w;
use crate::backend::newton::world as nw;
use crate::types::Vector3f;

const BOOK_OFFSET: usize = 32;
const NEWTON_OFFSET: usize = 176;
const SHAPE: usize = 0;
const BODY: usize = 1;
const CHAR: usize = 2;
const JOINT: usize = 3;
const CTRL: usize = 4;
const ROPE: usize = 5;

struct Handles {
    book: u64,
    newton: u64,
}

/// # Safety
/// `this` is a live `iPhysicsWorld`.
unsafe fn handles(this: *const u8) -> Handles {
    // SAFETY: caller contract.
    unsafe {
        Handles {
            book: val_member(this, BOOK_OFFSET),
            newton: val_member(this, NEWTON_OFFSET),
        }
    }
}

/// # Safety
/// `n` points to a live `std::string`.
unsafe fn find(h: &Handles, kind: usize, n: *const CxxStr) -> usize {
    // SAFETY: caller contract.
    let b = unsafe { CxxStr::bytes(n) };
    unsafe { w::ohpl_h15w_find_by_name(h.book, kind, b.as_ptr(), b.len()) }
}

super::cxx::cxx! {
    handles;
    "_ZN3hpl13iPhysicsWorld13RegisterShapeEPNS_13iCollideShapeE" register_shape[this, s](o: usize) => w::ohpl_h15w_add(s.book, SHAPE, o);
    "_ZN3hpl13iPhysicsWorld12RegisterBodyEPNS_12iPhysicsBodyE" register_body[this, s](o: usize) => w::ohpl_h15w_add(s.book, BODY, o);
    "_ZN3hpl13iPhysicsWorld16RegisterCharBodyEPNS_14iCharacterBodyE" register_char[this, s](o: usize) => w::ohpl_h15w_add(s.book, CHAR, o);
    "_ZN3hpl13iPhysicsWorld13RegisterJointEPNS_13iPhysicsJointE" register_joint[this, s](o: usize) => w::ohpl_h15w_add(s.book, JOINT, o);
    "_ZN3hpl13iPhysicsWorld18RegisterControllerEPNS_18iPhysicsControllerE" register_ctrl[this, s](o: usize) => w::ohpl_h15w_add(s.book, CTRL, o);
    "_ZN3hpl13iPhysicsWorld12RegisterRopeEPNS_12iPhysicsRopeE" register_rope[this, s](o: usize) => w::ohpl_h15w_add(s.book, ROPE, o);
    "_ZN3hpl13iPhysicsWorld16RegisterMaterialERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS_16iPhysicsMaterialE" register_material[this, s](n: *const CxxStr, m: usize) => {
        let b = CxxStr::bytes(n);
        w::ohpl_h15w_add_material(s.book, b.as_ptr(), b.len(), m)
    };
    "_ZN3hpl13iPhysicsWorld6UpdateEf" update[this, s](dt: f32) => w::ohpl_h15w_update(s.book, dt);
    "_ZN3hpl13iPhysicsWorld12DestroyShapeEPNS_13iCollideShapeE" destroy_shape[this, s](o: usize) => w::ohpl_h15w_destroy_shape(s.book, o);
    "_ZN3hpl13iPhysicsWorld11DestroyBodyEPNS_12iPhysicsBodyE" destroy_body[this, s](o: usize) => w::ohpl_h15w_destroy_body(s.book, o);
    "_ZN3hpl13iPhysicsWorld7GetBodyERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" get_body[this, s](n: *const CxxStr) -> usize => find(&s, BODY, n);
    "_ZN3hpl13iPhysicsWorld16EnableBodiesInBVEPNS_15cBoundingVolumeEb" enable_in_bv[this, s](bv: usize, _on: bool) => w::ohpl_h15w_enable_bodies_in_bv(s.book, bv);
    "_ZN3hpl13iPhysicsWorld19AddBodyToUpdateListEPNS_12iPhysicsBodyE" update_add[this, s](o: usize) => w::ohpl_h15w_update_add(s.book, o);
    "_ZN3hpl13iPhysicsWorld24RemoveBodyFromUpdateListEPNS_12iPhysicsBodyEb" update_remove[this, s](o: usize, _destroying: bool) => w::ohpl_h15w_update_remove(s.book, o);
    "_ZN3hpl13iPhysicsWorld12DestroyJointEPNS_13iPhysicsJointE" destroy_joint[this, s](o: usize) => w::ohpl_h15w_find_and_delete(s.book, JOINT, o);
    "_ZN3hpl13iPhysicsWorld8GetJointERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" get_joint[this, s](n: *const CxxStr) -> usize => find(&s, JOINT, n);
    "_ZN3hpl13iPhysicsWorld11JointExistsEPNS_13iPhysicsJointE" joint_exists[this, s](o: usize) -> bool => w::ohpl_h15w_contains(s.book, JOINT, o);
    "_ZN3hpl13iPhysicsWorld20DestroyCharacterBodyEPNS_14iCharacterBodyE" destroy_char[this, s](o: usize) => w::ohpl_h15w_find_and_delete(s.book, CHAR, o);
    "_ZN3hpl13iPhysicsWorld16GetCharacterBodyERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" get_char[this, s](n: *const CxxStr) -> usize => find(&s, CHAR, n);
    "_ZN3hpl13iPhysicsWorld19GetMaterialFromNameERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" get_material[this, s](n: *const CxxStr) -> usize => {
        let b = CxxStr::bytes(n);
        w::ohpl_h15w_get_material(s.book, b.as_ptr(), b.len())
    };
    "_ZN3hpl13iPhysicsWorld7GetRopeERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE" get_rope[this, s](n: *const CxxStr) -> usize => find(&s, ROPE, n);
    "_ZN3hpl13iPhysicsWorld19GetRopeFromUniqueIDEi" rope_from_id[this, s](id: i32) -> usize => w::ohpl_h15w_rope_from_unique_id(s.book, id);
    "_ZN3hpl13iPhysicsWorld11DestroyRopeEPNS_12iPhysicsRopeE" destroy_rope[this, s](o: usize) => w::ohpl_h15w_find_and_delete(s.book, ROPE, o);
    "_ZN3hpl13iPhysicsWorld10DestroyAllEv" destroy_all[this, s]() => w::ohpl_h15w_destroy_all(s.book);
    "_ZN3hpl13iPhysicsWorld17DestroyControllerEPNS_18iPhysicsControllerE" destroy_ctrl[this, s](o: usize) => w::ohpl_h15w_find_and_delete(s.book, CTRL, o);
    "_ZN3hpl13iPhysicsWorld20FadeoutAllLoopSoundsEf" fadeout_all[this, s](v: f32) => w::ohpl_h15w_fadeout_all_loop_sounds(s.book, v);
    "_ZN3hpl13iPhysicsWorld8SimulateEf" simulate[this, s](dt: f32) => nw::ohpl_ntw_simulate(s.newton, dt);
    "_ZN3hpl13iPhysicsWorld14SetMaxTimeStepEf" set_max_step[this, s](v: f32) => nw::ohpl_ntw_set_max_step(s.newton, v);
    "_ZN3hpl13iPhysicsWorld14GetMaxTimeStepEv" max_step[this, s]() -> f32 => nw::ohpl_ntw_max_step(s.newton);
    "_ZN3hpl13iPhysicsWorld12SetWorldSizeERKNS_8cVector3IfEES4_" set_size[this, s](min: *const f32, max: *const f32) => nw::ohpl_ntw_set_size(s.newton, min, max);
    "_ZN3hpl13iPhysicsWorld10SetGravityERKNS_8cVector3IfEE" set_gravity[this, s](g: *const Vector3f) => nw::ohpl_ntw_set_gravity(s.newton, g);
    "_ZN3hpl13iPhysicsWorld16SetAccuracyLevelENS_16ePhysicsAccuracyE" set_accuracy[this, s](v: i32) => nw::ohpl_ntw_set_accuracy(s.newton, v);
    "_ZN3hpl13iPhysicsWorld16GetAccuracyLevelEv" accuracy[this, s]() -> i32 => nw::ohpl_ntw_accuracy(s.newton);
    "_ZN3hpl13iPhysicsWorld18SetNumberOfThreadsEi" set_threads[this, s](v: i32) => nw::ohpl_ntw_set_threads(s.newton, v);
    "_ZN3hpl13iPhysicsWorld18GetNumberOfThreadsEv" threads[this, s]() -> i32 => nw::ohpl_ntw_threads(s.newton);
}
