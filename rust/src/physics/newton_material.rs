//! Newton material pair parameters, AABB-overlap gate and contact aggregation
//! (`cPhysicsMaterialNewton`).

use crate::math::{div3, v3};
use crate::types::Vector3f;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct MatProps {
    pub friction_mode: i32,
    pub elasticity_mode: i32,
    pub elasticity: f32,
    pub static_friction: f32,
    pub kinetic_friction: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PairParams {
    pub elasticity: f32,
    pub static_friction: f32,
    pub kinetic_friction: f32,
}

const MODE_MIN: i32 = 1;
const MODE_MULTIPLY: i32 = 2;
const MODE_MAX: i32 = 3;

pub fn combine(mode: i32, x: f32, y: f32) -> f32 {
    match mode {
        MODE_MIN => {
            if x < y {
                x
            } else {
                y
            }
        }
        MODE_MULTIPLY => x * y,
        MODE_MAX => {
            if x > y {
                x
            } else {
                y
            }
        }
        _ => (x + y) / 2.0,
    }
}

/// `same` = a material blended with itself, which always averages.
pub fn pair_params(a: &MatProps, b: &MatProps, same: bool) -> PairParams {
    let (mut fm, mut em) = (
        a.friction_mode.max(b.friction_mode),
        a.elasticity_mode.max(b.elasticity_mode),
    );
    if same {
        fm = 0;
        em = 0;
    }
    PairParams {
        elasticity: combine(em, a.elasticity, b.elasticity),
        static_friction: combine(fm, a.static_friction, b.static_friction),
        kinetic_friction: combine(fm, a.kinetic_friction, b.kinetic_friction),
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct GateBody {
    pub collide: bool,
    pub active: bool,
    pub is_rag_doll: bool,
    pub collide_rag_doll: bool,
    pub is_character: bool,
    pub collide_character: bool,
}

/// Static part of `OnAABBOverlapCallback`; the per-body virtual callbacks run after it passes.
pub fn aabb_overlap_gate(flags_match: bool, a: &GateBody, b: &GateBody) -> bool {
    flags_match
        && a.collide
        && b.collide
        && a.active
        && b.active
        && !(a.is_rag_doll && !b.collide_rag_doll)
        && !(b.is_rag_doll && !a.collide_rag_doll)
        && !(a.is_character && !b.collide_character)
        && !(b.is_character && !a.collide_character)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Contact {
    pub normal_speed: f32,
    pub tangent_speed0: f32,
    pub tangent_speed1: f32,
    pub force: Vector3f,
    pub position: Vector3f,
    pub normal: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ContactAgg {
    pub max_normal_speed: f32,
    pub max_tangent_speed: f32,
    pub force: Vector3f,
    pub normal: Vector3f,
    pub position: Vector3f,
}

/// Zero contacts divide by zero, as the C++ does.
pub fn aggregate_contacts(contacts: &[Contact]) -> ContactAgg {
    let mut agg = ContactAgg::default();
    for c in contacts {
        if agg.max_normal_speed < c.normal_speed {
            agg.max_normal_speed = c.normal_speed;
        }
        if agg.max_tangent_speed.abs() < c.tangent_speed0.abs() {
            agg.max_tangent_speed = c.tangent_speed0;
        }
        if agg.max_tangent_speed.abs() < c.tangent_speed1.abs() {
            agg.max_tangent_speed = c.tangent_speed1;
        }
        agg.force = add3(agg.force, c.force);
        agg.normal = add3(agg.normal, c.normal);
        agg.position = add3(agg.position, c.position);
    }
    let n = contacts.len() as f32;
    agg.normal = div3(agg.normal, n);
    agg.position = div3(agg.position, n);
    agg
}

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SurfaceCalls {
    pub impact1: bool,
    pub slide1: bool,
    pub impact2: bool,
    pub slide2: bool,
}

pub fn surface_calls(
    prio1: i32,
    prio2: i32,
    same_material: bool,
    normal_speed: f32,
    tangent_speed: f32,
) -> SurfaceCalls {
    let imp = normal_speed.abs() > 0.0;
    let slide = tangent_speed.abs() > 0.0;
    let first = prio1 >= prio2;
    let second = prio2 >= prio1 && !same_material;
    SurfaceCalls {
        impact1: first && imp,
        slide1: first && slide,
        impact2: second && imp,
        slide2: second && slide,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn combine_modes() {
        assert_eq!(combine(0, 1.0, 2.0), 1.5);
        assert_eq!(combine(1, 1.0, 2.0), 1.0);
        assert_eq!(combine(2, 3.0, 2.0), 6.0);
        assert_eq!(combine(3, 1.0, 2.0), 2.0);
        assert_eq!(combine(9, 1.0, 2.0), 1.5);
    }

    #[test]
    fn same_material_averages() {
        let m = MatProps {
            friction_mode: 3,
            elasticity_mode: 2,
            elasticity: 0.5,
            static_friction: 0.4,
            kinetic_friction: 0.2,
        };
        let p = pair_params(&m, &m, true);
        assert_eq!(p.elasticity, 0.5);
        assert_eq!(p.static_friction, 0.4);
    }

    #[test]
    fn tangent_keeps_signed_max() {
        let c = Contact {
            tangent_speed0: -3.0,
            tangent_speed1: 2.0,
            ..Contact::default()
        };
        assert_eq!(aggregate_contacts(&[c]).max_tangent_speed, -3.0);
    }

    #[test]
    fn empty_contacts_divide_by_zero() {
        assert!(aggregate_contacts(&[]).normal.x.is_nan());
    }

    #[test]
    fn surface_priority() {
        let s = surface_calls(2, 2, true, 1.0, 0.0);
        assert!(s.impact1 && !s.impact2 && !s.slide1);
        let s = surface_calls(1, 2, false, 1.0, 1.0);
        assert!(!s.impact1 && s.impact2 && s.slide2);
    }
}
