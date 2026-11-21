//! `iPhysicsBody` methods defined in Rust under their Itanium symbols; the C++ header only declares them.
//! `this` is the C++ object; its `mS` member (offset asserted in PhysicsBody.cpp) is the Rust `BodyState`.

use super::ffi_h15s as sh;
use super::shell::BodyState;
use crate::backend::newton::body as nb;
use crate::types::Vector3f;

/// # Safety
/// `this` is a live object whose constructor has run.
pub(super) unsafe fn state<'a>(this: *const u8) -> &'a mut BodyState {
    // SAFETY: caller contract.
    unsafe { super::cxx::ptr_member(this, MS_OFFSET) }
}

const MS_OFFSET: usize = 304;
const FLAG_HAS_COLLISION: i32 = 1;

pub(super) fn enable(s: &mut BodyState) {
    s.freeze_count = 0;
    nb::ohpl_ntb_set_freeze(s.rust, 0);
}

super::cxx::cxx! {
    state;
    "_ZN3hpl12iPhysicsBody17SetLinearVelocityERKNS_8cVector3IfEE" set_linear_velocity[this, s](v: *const Vector3f) => nb::ohpl_ntb_set_velocity(s.rust, v);
    "_ZN3hpl12iPhysicsBody18SetAngularVelocityERKNS_8cVector3IfEE" set_angular_velocity[this, s](v: *const Vector3f) => nb::ohpl_ntb_set_omega(s.rust, v);
    "_ZN3hpl12iPhysicsBody16SetLinearDampingEf" set_linear_damping[this, s](v: f32) => nb::ohpl_ntb_set_linear_damping(s.rust, v);
    "_ZNK3hpl12iPhysicsBody16GetLinearDampingEv" linear_damping[this, s]() -> f32 => nb::ohpl_ntb_linear_damping(s.rust);
    "_ZN3hpl12iPhysicsBody17SetAngularDampingEf" set_angular_damping[this, s](v: f32) => nb::ohpl_ntb_set_angular_damping(s.rust, v);
    "_ZNK3hpl12iPhysicsBody17GetAngularDampingEv" angular_damping[this, s]() -> f32 => nb::ohpl_ntb_angular_damping(s.rust);
    "_ZN3hpl12iPhysicsBody17SetMaxLinearSpeedEf" set_max_linear[this, s](v: f32) => nb::ohpl_ntb_set_max_linear(s.rust, v);
    "_ZNK3hpl12iPhysicsBody17GetMaxLinearSpeedEv" max_linear[this, s]() -> f32 => nb::ohpl_ntb_max_linear(s.rust);
    "_ZN3hpl12iPhysicsBody18SetMaxAngularSpeedEf" set_max_angular[this, s](v: f32) => nb::ohpl_ntb_set_max_angular(s.rust, v);
    "_ZNK3hpl12iPhysicsBody18GetMaxAngularSpeedEv" max_angular[this, s]() -> f32 => nb::ohpl_ntb_max_angular(s.rust);
    "_ZNK3hpl12iPhysicsBody7GetMassEv" mass[this, s]() -> f32 => nb::ohpl_ntb_mass(s.rust);
    "_ZN3hpl12iPhysicsBody13SetMassCentreERKNS_8cVector3IfEE" set_mass_centre[this, s](v: *const Vector3f) => nb::ohpl_ntb_set_mass_centre(s.rust, v);
    "_ZN3hpl12iPhysicsBody8AddForceERKNS_8cVector3IfEE" add_force[this, s](v: *const Vector3f) => { nb::ohpl_ntb_add_force(s.rust, v); enable(s) };
    "_ZN3hpl12iPhysicsBody9AddTorqueERKNS_8cVector3IfEE" add_torque[this, s](v: *const Vector3f) => { nb::ohpl_ntb_add_torque(s.rust, v); enable(s) };
    "_ZN3hpl12iPhysicsBody20AddImpulseAtPositionERKNS_8cVector3IfEES4_" add_impulse_at[this, s](i: *const Vector3f, p: *const Vector3f) => nb::ohpl_ntb_add_impulse_at(s.rust, i, p);
    "_ZN3hpl12iPhysicsBody5SleepEv" sleep[this, s]() => nb::ohpl_ntb_set_freeze(s.rust, 1);
    "_ZN3hpl12iPhysicsBody6EnableEv" enable_body[this, s]() => enable(s);
    "_ZNK3hpl12iPhysicsBody10GetEnabledEv" enabled[this, s]() -> bool => nb::ohpl_ntb_enabled(s.rust);
    "_ZN3hpl12iPhysicsBody14SetAutoDisableEb" set_auto_disable[this, s](v: bool) => nb::ohpl_ntb_set_auto_sleep(s.rust, v);
    "_ZNK3hpl12iPhysicsBody14GetAutoDisableEv" auto_disable[this, s]() -> bool => nb::ohpl_ntb_auto_sleep(s.rust);
    "_ZN3hpl12iPhysicsBody29SetAutoDisableLinearThresholdEf" set_auto_linear[this, s](v: f32) => nb::ohpl_ntb_set_auto_linear(s.rust, v);
    "_ZNK3hpl12iPhysicsBody29GetAutoDisableLinearThresholdEv" auto_linear[this, s]() -> f32 => nb::ohpl_ntb_auto_linear(s.rust);
    "_ZN3hpl12iPhysicsBody30SetAutoDisableAngularThresholdEf" set_auto_angular[this, s](v: f32) => nb::ohpl_ntb_set_auto_angular(s.rust, v);
    "_ZNK3hpl12iPhysicsBody30GetAutoDisableAngularThresholdEv" auto_angular[this, s]() -> f32 => nb::ohpl_ntb_auto_angular(s.rust);
    "_ZN3hpl12iPhysicsBody22SetAutoDisableNumStepsEi" set_auto_steps[this, s](v: i32) => nb::ohpl_ntb_set_auto_steps(s.rust, v);
    "_ZNK3hpl12iPhysicsBody22GetAutoDisableNumStepsEv" auto_steps[this, s]() -> i32 => nb::ohpl_ntb_auto_steps(s.rust);
    "_ZN3hpl12iPhysicsBody22SetContinuousCollisionEb" set_continuous[this, s](v: bool) => nb::ohpl_ntb_set_continuous(s.rust, v);
    "_ZN3hpl12iPhysicsBody22GetContinuousCollisionEv" continuous[this, s]() -> bool => nb::ohpl_ntb_continuous(s.rust);
    "_ZN3hpl12iPhysicsBody10SetGravityEb" set_gravity[this, s](v: bool) => nb::ohpl_ntb_set_gravity(s.rust, v);
    "_ZNK3hpl12iPhysicsBody10GetGravityEv" gravity[this, s]() -> bool => nb::ohpl_ntb_gravity(s.rust);
    "_ZN3hpl12iPhysicsBody8AddJointEPNS_13iPhysicsJointE" add_joint[this, s](j: usize) => sh::ohpl_h15b_add_joint(s.shell, j);
    "_ZN3hpl12iPhysicsBody8GetJointEi" joint_at[this, s](i: i32) -> usize => sh::ohpl_h15b_joint_at(s.shell, i);
    "_ZN3hpl12iPhysicsBody11GetJointNumEv" joint_num[this, s]() -> i32 => sh::ohpl_h15b_joint_num(s.shell);
    "_ZN3hpl12iPhysicsBody11RemoveJointEPNS_13iPhysicsJointE" remove_joint[this, s](j: usize) => sh::ohpl_h15b_remove_joint(s.shell, j);
    "_ZN3hpl12iPhysicsBody15AddBodyCallbackEPNS_20iPhysicsBodyCallbackE" add_callback[this, s](c: usize) => sh::ohpl_h15b_add_callback(s.shell, c);
    "_ZN3hpl12iPhysicsBody18RemoveBodyCallbackEPNS_20iPhysicsBodyCallbackE" remove_callback[this, s](c: usize) => sh::ohpl_h15b_remove_callback(s.shell, c);
    "_ZN3hpl12iPhysicsBody15OnAABBCollisionEPS0_" on_aabb[this, s](o: usize) -> bool => sh::ohpl_h15b_on_aabb(s.shell, this as usize, o);
    "_ZN3hpl12iPhysicsBody9OnCollideEPS0_PNS_19cPhysicsContactDataE" on_collide[this, s](o: usize, c: usize) => {
        super::ffi::ohpl_h15_body_flag_set(s.book, FLAG_HAS_COLLISION, true);
        sh::ohpl_h15b_on_collide(s.shell, this as usize, o, c)
    };
    "_ZN3hpl12iPhysicsBody21AddConnectedCharacterEPNS_14iCharacterBodyE" add_char[this, s](c: usize) => sh::ohpl_h15b_add_char(s.shell, c);
    "_ZN3hpl12iPhysicsBody24RemoveConnectedCharacterEPNS_14iCharacterBodyE" remove_char[this, s](c: usize) => sh::ohpl_h15b_remove_char(s.shell, c);
    "_ZN3hpl12iPhysicsBody26AddAttachedVerletContainerEPNS_24iVerletParticleContainerE" add_verlet[this, s](c: usize) => sh::ohpl_h15b_add_verlet(s.shell, c);
    "_ZN3hpl12iPhysicsBody29RemoveAttachedVerletContainerEPNS_24iVerletParticleContainerE" remove_verlet[this, s](c: usize) => sh::ohpl_h15b_remove_verlet(s.shell, c);
    "_ZN3hpl12iPhysicsBody7DestroyEv" destroy[this, s]() => sh::ohpl_h15b_destroy(s.shell, this as usize);
}
