//! `cSurfaceData` accessors defined in Rust under their Itanium symbols (`mS` at offset 80, asserted in SurfaceData.cpp).

use super::surf::{self, SurfaceState};

/// # Safety
/// `this` is a live object whose constructor has run.
unsafe fn state<'a>(this: *const u8) -> &'a mut SurfaceState {
    // SAFETY: caller contract.
    unsafe { super::cxx::ptr_member(this, MS_OFFSET) }
}

const MS_OFFSET: usize = 80;

super::cxx::cxx! {
    state;
    "_ZN3hpl12cSurfaceData13SetElasticityEf" set_elasticity[this, s](v: f32) => s.set_f32(0, v);
    "_ZNK3hpl12cSurfaceData13GetElasticityEv" elasticity[this, s]() -> f32 => s.get_f32(0);
    "_ZN3hpl12cSurfaceData17SetStaticFrictionEf" set_static_friction[this, s](v: f32) => s.set_f32(1, v);
    "_ZNK3hpl12cSurfaceData17GetStaticFrictionEv" static_friction[this, s]() -> f32 => s.get_f32(1);
    "_ZN3hpl12cSurfaceData18SetKineticFrictionEf" set_kinetic_friction[this, s](v: f32) => s.set_f32(2, v);
    "_ZNK3hpl12cSurfaceData18GetKineticFrictionEv" kinetic_friction[this, s]() -> f32 => s.get_f32(2);
    "_ZN3hpl12cSurfaceData11SetPriorityEi" set_priority[this, s](v: i32) => s.set_i32(0, v);
    "_ZNK3hpl12cSurfaceData11GetPriorityEv" priority[this, s]() -> i32 => s.get_i32(0);
    "_ZN3hpl12cSurfaceData19SetFrictionCombModeENS_24ePhysicsMaterialCombModeE" set_friction_mode[this, s](v: i32) => s.set_i32(1, v);
    "_ZNK3hpl12cSurfaceData19GetFrictionCombModeEv" friction_mode[this, s]() -> i32 => s.get_i32(1);
    "_ZN3hpl12cSurfaceData21SetElasticityCombModeENS_24ePhysicsMaterialCombModeE" set_elasticity_mode[this, s](v: i32) => s.set_i32(2, v);
    "_ZNK3hpl12cSurfaceData21GetElasticityCombModeEv" elasticity_mode[this, s]() -> i32 => s.get_i32(2);
    "_ZN3hpl12cSurfaceData11PreloadDataEv" preload[this, s]() => surf::preload_data(s.rs);
    "_ZN3hpl12cSurfaceData16CreateImpactDataEf" create_impact[this, s](v: f32) -> usize => surf::create_impact(s.rs, v, false);
    "_ZN3hpl12cSurfaceData13GetImpactDataEi" impact_at[this, s](i: i32) -> usize => surf::impact_at(s.rs, false, i);
    "_ZN3hpl12cSurfaceData16GetImpactDataNumEv" impact_num[this, s]() -> i32 => surf::impact_num(s.rs, false);
    "_ZN3hpl12cSurfaceData22GetImpactDataFromSpeedEf" impact_from_speed[this, s](v: f32) -> usize => surf::impact_from_speed(s.rs, false, v);
    "_ZN3hpl12cSurfaceData13CreateHitDataEf" create_hit[this, s](v: f32) -> usize => surf::create_impact(s.rs, v, true);
    "_ZN3hpl12cSurfaceData10GetHitDataEi" hit_at[this, s](i: i32) -> usize => surf::impact_at(s.rs, true, i);
    "_ZN3hpl12cSurfaceData13GetHitDataNumEv" hit_num[this, s]() -> i32 => surf::impact_num(s.rs, true);
    "_ZN3hpl12cSurfaceData19GetHitDataFromSpeedEf" hit_from_speed[this, s](v: f32) -> usize => surf::impact_from_speed(s.rs, true, v);
}
