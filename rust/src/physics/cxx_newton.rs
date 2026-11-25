use super::cxx::{ptr_member, val_member};
use super::newton_material::{MatProps, surface_calls};
use super::shell::BodyState;
use super::wreg;
use crate::backend::newton::reg::Buoy;
use crate::backend::newton::{body as nb, material as nm, world as nw};
use crate::types::{Matrixf, Vector3f};
use std::arch::global_asm;

const MAT_WORLD: usize = 0;
const MAT_ID: usize = 64;
const MAT_PROPS: usize = 68;
const WORLD_BOOK: usize = 32;
const WORLD_NEWTON: usize = 176;
const ENT_POD: usize = 8;
const SHAPE_COLLISION: usize = 304;
const BODY_STATE: usize = 304;

unsafe fn props(m: *const u8) -> MatProps {
    unsafe { *m.add(MAT_PROPS).cast::<MatProps>() }
}

unsafe fn update_materials(this: *const u8) {
    unsafe {
        let world: *const u8 = val_member(this, MAT_WORLD);
        let book: u64 = val_member(world, WORLD_BOOK);
        let newton: u64 = val_member(world, WORLD_NEWTON);
        let a = props(this);
        let id_a: i32 = val_member(this, MAT_ID);
        let mut all: Vec<(&[u8], usize)> = (0..wreg::material_len(book))
            .filter_map(|i| wreg::material_at(book, i))
            .map(|(p, n, h)| (std::slice::from_raw_parts(p, n), h))
            .collect();
        all.sort_by(|x, y| x.0.cmp(y.0));
        for (_, h) in all {
            let m = h as *const u8;
            let b = props(m);
            nm::ohpl_ntm_update_pair(newton, id_a, val_member(m, MAT_ID), &a, &b, m == this);
        }
    }
}

macro_rules! mat_prop {
    ($($set:literal $setn:ident $get:literal $getn:ident $t:ty, $off:expr;)*) => {$(
        #[unsafe(export_name = $set)]
        unsafe extern "C" fn $setn(this: *const u8, v: $t) {
            unsafe {
                *this.add(MAT_PROPS + $off).cast_mut().cast::<$t>() = v;
                update_materials(this);
            }
        }
        #[unsafe(export_name = $get)]
        unsafe extern "C" fn $getn(this: *const u8) -> $t {
            unsafe { val_member(this, MAT_PROPS + $off) }
        }
    )*};
}

mat_prop! {
    "_ZN3hpl16iPhysicsMaterial19SetFrictionCombModeENS_24ePhysicsMaterialCombModeE" set_friction_mode "_ZNK3hpl16iPhysicsMaterial19GetFrictionCombModeEv" friction_mode i32, 0;
    "_ZN3hpl16iPhysicsMaterial21SetElasticityCombModeENS_24ePhysicsMaterialCombModeE" set_elasticity_mode "_ZNK3hpl16iPhysicsMaterial21GetElasticityCombModeEv" elasticity_mode i32, 4;
    "_ZN3hpl16iPhysicsMaterial13SetElasticityEf" set_elasticity "_ZNK3hpl16iPhysicsMaterial13GetElasticityEv" elasticity f32, 8;
    "_ZN3hpl16iPhysicsMaterial17SetStaticFrictionEf" set_static_friction "_ZNK3hpl16iPhysicsMaterial17GetStaticFrictionEv" static_friction f32, 12;
    "_ZN3hpl16iPhysicsMaterial18SetKineticFrictionEf" set_kinetic_friction "_ZNK3hpl16iPhysicsMaterial18GetKineticFrictionEv" kinetic_friction f32, 16;
}

#[unsafe(export_name = "_ZN3hpl16iPhysicsMaterial15UpdateMaterialsEv")]
unsafe extern "C" fn update_materials_sym(this: *const u8) {
    unsafe { update_materials(this) }
}

const MAT_SURFACE: usize = 48;
const CD_TANGENT: usize = 4;
const CD_POS: usize = 20;

macro_rules! weak_calls {
    ($($sym:literal $tramp:literal $name:ident($($a:ident: $t:ty),*) $(-> $r:ty)?;)*) => {
        $(global_asm!(concat!(
            ".weak ", $sym, "\n.global ", $tramp, "\n", $tramp, ":\nadrp x16, :got:", $sym,
            "\nldr x16, [x16, :got_lo12:", $sym, "]\nbr x16"
        ));)*
        unsafe extern "C" { $(#[link_name = $tramp] fn $name($($a: $t),*) $(-> $r)?;)* }
    };
}

weak_calls! {
    "_ZN3hpl12cSurfaceData18CreateImpactEffectEfRKNS_8cVector3IfEEiPS0_PNS_13iPhysicsWorldE" "ohpl_nwt_impact_effect" surface_impact_effect(s: *const u8, speed: f32, pos: *const Vector3f, contacts: i32, other: *const u8, world: *const u8);
    "_ZN3hpl12cSurfaceData8OnImpactEfRKNS_8cVector3IfEEiPNS_12iPhysicsBodyE" "ohpl_nwt_on_impact" surface_on_impact(s: *const u8, speed: f32, pos: *const Vector3f, contacts: i32, body: *const u8);
    "_ZN3hpl12cSurfaceData7OnSlideEfRKNS_8cVector3IfEEiPNS_12iPhysicsBodyES6_" "ohpl_nwt_on_slide" surface_on_slide(s: *const u8, speed: f32, pos: *const Vector3f, contacts: i32, body: *const u8, against: *const u8);
}

unsafe extern "C" {
    #[link_name = "_ZN3hpl12iPhysicsBody9OnCollideEPS0_PNS_19cPhysicsContactDataE"]
    fn body_on_collide(b: *const u8, other: *const u8, data: *const u8);
}

#[unsafe(export_name = "_ZN3hpl16iPhysicsMaterial16DispatchContactsEPNS_12iPhysicsBodyES2_RNS_19cPhysicsContactDataEi")]
unsafe extern "C" fn dispatch_contacts(
    b1: *const u8,
    b2: *const u8,
    data: *const u8,
    contacts: i32,
) {
    unsafe {
        let (s1, s2) = (body(b1), body(b2));
        let (m1, m2) = (s1.material as *const u8, s2.material as *const u8);
        let (d1, d2): (*const u8, *const u8) =
            (val_member(m1, MAT_SURFACE), val_member(m2, MAT_SURFACE));
        let buoy_active = |s: &BodyState| {
            let mut b = std::mem::zeroed::<Buoy>();
            nb::ohpl_ntb_buoy(s.rust, &mut b);
            b.active
        };
        if !d1.is_null()
            && !d2.is_null()
            && s1.use_surface_effects
            && s2.use_surface_effects
            && !buoy_active(s1)
            && !buoy_active(s2)
        {
            let normal: f32 = val_member(data, 0);
            let tangent: f32 = val_member(data, CD_TANGENT);
            let pos = data.add(CD_POS).cast::<Vector3f>();
            surface_impact_effect(d1, normal, pos, contacts, d2, s1.world as *const u8);
            let c = surface_calls(
                super::cxx_surface::priority_of(d1),
                super::cxx_surface::priority_of(d2),
                m1 == m2,
                normal,
                tangent,
            );
            if c.impact1 {
                surface_on_impact(d1, normal, pos, contacts, b1);
            }
            if c.slide1 {
                surface_on_slide(d1, tangent, pos, contacts, b1, b2);
            }
            if c.impact2 {
                surface_on_impact(d2, normal, pos, contacts, b2);
            }
            if c.slide2 {
                surface_on_slide(d2, tangent, pos, contacts, b2, b1);
            }
        }
        body_on_collide(b1, b2, data);
        body_on_collide(b2, b1, data);
    }
}

unsafe fn body<'a>(b: *const u8) -> &'a mut BodyState {
    unsafe { ptr_member(b, BODY_STATE) }
}

super::cxx::cxx! {
    body;
    "_ZN3hpl12iPhysicsBody11SetMaterialEPNS_16iPhysicsMaterialE" set_material[this, s](m: *const u8) => {
        s.material = m as usize;
        if !m.is_null() {
            nb::ohpl_ntb_set_material(s.rust, val_member(m, MAT_ID));
        }
    };
    "_ZN3hpl12iPhysicsBody7SetMassEf" set_mass[this, s](mass: f32) => nb::ohpl_ntb_set_mass(s.rust, val_member(s.shape as *const u8, SHAPE_COLLISION), mass);
    "_ZN3hpl12iPhysicsBody18AddForceAtPositionERKNS_8cVector3IfEES4_" add_force_at[this, s](f: *const Vector3f, p: *const Vector3f) => {
        let local: *const Matrixf = val_member(this, ENT_POD);
        nb::ohpl_ntb_add_force_at_position(s.rust, f, p, local);
        super::cxx_body::enable(s)
    };
}

macro_rules! sret {
    ($($sym:literal $imp:ident;)*) => {$(
        global_asm!(concat!(".global ", $sym, "\n.type ", $sym, ",%function\n", $sym, ":\nmov x1, x0\nmov x0, x8\nb ", stringify!($imp)));
    )*};
}

sret! {
    "_ZNK3hpl12iPhysicsBody17GetLinearVelocityEv" ohpl_phy_linear_velocity;
    "_ZNK3hpl12iPhysicsBody18GetAngularVelocityEv" ohpl_phy_angular_velocity;
    "_ZN3hpl12iPhysicsBody16GetInertiaVectorEv" ohpl_phy_inertia_vector;
    "_ZNK3hpl12iPhysicsBody13GetMassCentreEv" ohpl_phy_mass_centre;
    "_ZN3hpl13iPhysicsWorld15GetWorldSizeMinEv" ohpl_phy_size_min;
    "_ZN3hpl13iPhysicsWorld15GetWorldSizeMaxEv" ohpl_phy_size_max;
    "_ZN3hpl13iPhysicsWorld10GetGravityEv" ohpl_phy_gravity;
}

macro_rules! sret_impl {
    ($($n:ident[$this:ident] => $e:expr;)*) => {$(
        #[unsafe(no_mangle)]
        unsafe extern "C" fn $n(out: *mut Vector3f, $this: *const u8) {
            unsafe { ($e)(out) }
        }
    )*};
}

sret_impl! {
    ohpl_phy_linear_velocity[this] => |o| nb::ohpl_ntb_velocity(body(this).rust, o);
    ohpl_phy_angular_velocity[this] => |o| nb::ohpl_ntb_omega(body(this).rust, o);
    ohpl_phy_inertia_vector[this] => |o| nb::ohpl_ntb_inertia(body(this).rust, o);
    ohpl_phy_mass_centre[this] => |o| nb::ohpl_ntb_mass_centre(body(this).rust, o);
    ohpl_phy_size_min[this] => |o| nw::ohpl_ntw_size(val_member(this, WORLD_NEWTON), false, o);
    ohpl_phy_size_max[this] => |o| nw::ohpl_ntw_size(val_member(this, WORLD_NEWTON), true, o);
    ohpl_phy_gravity[this] => |o| nw::ohpl_ntw_gravity(val_member(this, WORLD_NEWTON), o);
}

unsafe fn world_handles(this: *const u8) -> (u64, u64) {
    unsafe { (val_member(this, WORLD_BOOK), val_member(this, WORLD_NEWTON)) }
}

super::cxx::cxx! {
    world_handles;
    "_ZN3hpl13iPhysicsWorld7CastRayEPNS_19iPhysicsRayCallbackERKNS_8cVector3IfEES6_bbbb" cast_ray[this, s](cb: usize, o: *const Vector3f, e: *const Vector3f, dist: bool, normal: bool, point: bool, prefilter: bool)
        => nw::ohpl_ntw_cast_ray(s.1, cb, o, e, dist, normal, point, prefilter);
    "_ZN3hpl13iPhysicsWorld14CastRayOnShapeEPNS_13iCollideShapeERKNS_7cMatrixIfEERKNS_8cVector3IfEESA_RS8_b" cast_ray_on_shape[this, s](shape: *const u8, m: *const Matrixf, o: *const Vector3f, e: *const Vector3f, n: *mut Vector3f, local: bool) -> f32
        => nw::ohpl_ntw_cast_ray_on_shape(val_member(shape, SHAPE_COLLISION), m, o, e, n, local);
    "_ZN3hpl13iPhysicsWorld24RenderShapeDebugGeometryEPNS_13iCollideShapeERKNS_7cMatrixIfEEPNS_17iLowLevelGraphicsERKNS_6cColorE" debug_shape[this, s](shape: *const u8, m: *const Matrixf, low: usize, color: usize)
        => nw::ohpl_ntw_debug_shape(val_member(shape, SHAPE_COLLISION), m, low, color);
    "_ZN3hpl13iPhysicsWorld19RenderContactPointsEPNS_17iLowLevelGraphicsERKNS_6cColorES5_" render_contacts[this, s](low: usize, point: *const u8, line: *const u8) => {
        let colors = [*point.cast::<[f32; 4]>(), *line.cast::<[f32; 4]>()];
        wreg::render_contacts(s.0, low, colors.as_ptr() as usize)
    };
    "_ZN3hpl13iPhysicsWorld24CheckShapeWorldCollisionEPNS_8cVector3IfEEPNS_13iCollideShapeERKNS_7cMatrixIfEEPNS_12iPhysicsBodyEbbPNS_30iPhysicsWorldCollisionCallbackEbijb" check_world[this, s](
        push: *mut Vector3f, shape: usize, m: *const Matrixf, skip: usize, skip_static: bool, is_char: bool, cb: usize, collide_char: bool, min_push: i32, flags: u32, _debug: bool) -> bool => {
        let q = wreg::Query { skip_body: skip, flags, collide_character: collide_char, is_character: is_char, skip_static, min_push };
        wreg::shape_world_collision(s.0, push, shape, m, &q, cb)
    };
}
