//! `iPhysicsBody` bookkeeping that does not depend on the physics backend.

use super::host::{H, Host};
use crate::math::{cross3, fma, matrix_mul, matrix_mul_vec, matrix_rotate, mul3, sub3, v3};
use crate::types::{Matrixf, Vector3f};

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);

pub(super) fn rotation(m: &Matrixf) -> Matrixf {
    let r = |i: usize| -> [f32; 4] {
        let row = m.m.get(i).copied().unwrap_or_default();
        [row[0], row[1], row[2], 0.0]
    };
    Matrixf {
        m: [r(0), r(1), r(2), [0.0, 0.0, 0.0, 1.0]],
    }
}

fn local_pos_from_centre<Ho: Host>(h: &Ho, b: H, pos: Vector3f) -> Vector3f {
    let mut local = sub3(pos, h.body_local_position(b));
    let mut centre = h.body_mass_centre(b);
    if centre != ZERO {
        centre = matrix_mul_vec(&rotation(&h.body_local_matrix(b)), centre);
        local = sub3(local, centre);
    }
    local
}

pub fn velocity_at_position<Ho: Host>(h: &Ho, b: H, pos: Vector3f) -> Vector3f {
    let local = local_pos_from_centre(h, b, pos);
    let lin = h.body_linear_velocity(b);
    let cross = cross3(h.body_angular_velocity(b), local);
    v3(lin.x + cross.x, lin.y + cross.y, lin.z + cross.z)
}

pub fn torque_from_force_at_position<Ho: Host>(
    h: &Ho,
    b: H,
    force: Vector3f,
    pos: Vector3f,
) -> Vector3f {
    cross3(local_pos_from_centre(h, b, pos), force)
}

/// False when the body can leave the world's update list.
pub fn update_before_simulate<Ho: Host>(h: &Ho, b: H) -> bool {
    let mut needs_update = false;
    if h.body_has_slide(b) || h.body_scrape_entity(b) != 0 {
        needs_update = true;
    }
    if h.body_mass(b) > 0.0 {
        if h.body_enabled(b) {
            needs_update = true;
        }
    } else if h.body_static_movement(b) {
        needs_update = true;
    }

    h.body_set_has_slide(b, false);
    h.body_set_has_collision(b, false);

    if !needs_update {
        return false;
    }

    if h.body_static_movement(b) {
        let lin = h.body_total_static_linear(b);
        let ang = h.body_total_static_angular(b);
        if lin == ZERO && ang == ZERO {
            h.body_set_static_movement(b, false);
        }
        h.body_set_linear_velocity(b, h.body_total_static_linear(b));
        h.body_set_angular_velocity(b, h.body_total_static_angular(b));
        h.body_enable_bodies_around(b);
    }
    true
}

pub fn update_after_simulate<Ho: Host>(h: &Ho, b: H, time_step: f32) {
    if h.body_static_movement(b) {
        let mut pos = h.body_local_position(b);
        let lin = h.body_total_static_linear(b);
        if lin != ZERO {
            pos = v3(
                fma(lin.x, time_step, pos.x),
                fma(lin.y, time_step, pos.y),
                fma(lin.z, time_step, pos.z),
            );
        }
        let ang = h.body_total_static_angular(b);
        if ang != ZERO {
            let rot = rotation(&h.body_local_matrix(b));
            let rot = matrix_mul(&matrix_rotate(mul3(ang, time_step), 0), &rot);
            h.body_set_matrix(b, &rot);
        }
        h.body_set_position(b, pos);
        h.body_set_total_static_linear(b, ZERO);
        h.body_set_total_static_angular(b, ZERO);
    }

    if h.body_disable_after_simulation(b) {
        h.body_set_disable_after_simulation(b, false);
        h.body_set_angular_velocity(b, ZERO);
        h.body_set_linear_velocity(b, ZERO);
    }

    if !h.body_has_slide(b) {
        if h.body_scrape_entity(b) != 0 {
            let count = h.body_slide_count(b);
            if count <= 0 {
                let world = h.body_world_world(b);
                if world != 0 {
                    let e = h.body_scrape_entity(b);
                    if h.world_sound_exists(world, e, h.body_scrape_entity_id(b)) {
                        h.ent_fade_out(h.body_scrape_entity(b), 5.2);
                    }
                }
                h.body_set_scrape_entity(b, 0);
                h.body_set_scrape_body(b, 0);
            } else {
                h.body_set_slide_count(b, count - 1);
            }
        }
    } else {
        h.body_set_slide_count(b, 8);
    }

    if h.body_enabled(b) {
        h.body_update_roll_effect(b);
    }
}

pub fn static_linear_move<Ho: Host>(h: &Ho, b: H, v: Vector3f) {
    if h.body_mass(b) != 0.0 {
        return;
    }
    let t = h.body_total_static_linear(b);
    h.body_set_total_static_linear(b, v3(t.x + v.x, t.y + v.y, t.z + v.z));
    h.body_set_static_movement(b, true);
    h.body_add_to_update_list(b);
}

pub fn static_angular_move<Ho: Host>(h: &Ho, b: H, v: Vector3f) {
    if h.body_mass(b) != 0.0 {
        return;
    }
    let t = h.body_total_static_angular(b);
    h.body_set_total_static_angular(b, v3(t.x + v.x, t.y + v.y, t.z + v.z));
    h.body_set_static_movement(b, true);
    h.body_add_to_update_list(b);
}
