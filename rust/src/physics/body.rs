//! `iPhysicsBody` bookkeeping that does not depend on the physics backend.

use super::book;
use super::host::{H, Host};
use crate::math::{cross3, fma, matrix_mul, matrix_mul_vec, matrix_rotate, mul3, sub3, v3};
use crate::types::{Matrixf, Vector3f};

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);

/// Body bookkeeping slots: the Rust-owned book when `book` is non-zero, else the C++ fields via the host.
struct Slots<'a, Ho: Host> {
    h: &'a Ho,
    b: H,
    book: u64,
}

impl<Ho: Host> Slots<'_, Ho> {
    fn set_has_collision(&self, v: bool) {
        if book::body_with(self.book, |k| k.has_collision = v).is_none() {
            self.h.body_set_has_collision(self.b, v);
        }
    }
}

macro_rules! slots {
    ($( $get:ident $set:ident $field:ident $hget:ident $hset:ident : $t:ty ;)*) => {
        impl<Ho: Host> Slots<'_, Ho> {
            $(
                fn $get(&self) -> $t {
                    match book::body_get(self.book) {
                        Some(k) => k.$field,
                        None => self.h.$hget(self.b),
                    }
                }
                fn $set(&self, v: $t) {
                    if book::body_with(self.book, |k| k.$field = v).is_none() {
                        self.h.$hset(self.b, v);
                    }
                }
            )*
        }
    };
}

slots! {
    has_slide set_has_slide has_slide body_has_slide body_set_has_slide: bool;
    static_movement set_static_movement static_movement body_static_movement body_set_static_movement: bool;
    disable_after_simulation set_disable_after_simulation disable_after_sim body_disable_after_simulation body_set_disable_after_simulation: bool;
    slide_count set_slide_count slide_count body_slide_count body_set_slide_count: i32;
    total_static_linear set_total_static_linear total_linear body_total_static_linear body_set_total_static_linear: Vector3f;
    total_static_angular set_total_static_angular total_angular body_total_static_angular body_set_total_static_angular: Vector3f;
}

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
    update_before_simulate_bk(h, b, 0)
}

pub fn update_before_simulate_bk<Ho: Host>(h: &Ho, b: H, book: u64) -> bool {
    let k = Slots { h, b, book };
    let mut needs_update = false;
    if k.has_slide() || h.body_scrape_entity(b) != 0 {
        needs_update = true;
    }
    if h.body_mass(b) > 0.0 {
        if h.body_enabled(b) {
            needs_update = true;
        }
    } else if k.static_movement() {
        needs_update = true;
    }

    k.set_has_slide(false);
    k.set_has_collision(false);

    if !needs_update {
        return false;
    }

    if k.static_movement() {
        let lin = k.total_static_linear();
        let ang = k.total_static_angular();
        if lin == ZERO && ang == ZERO {
            k.set_static_movement(false);
        }
        h.body_set_linear_velocity(b, k.total_static_linear());
        h.body_set_angular_velocity(b, k.total_static_angular());
        h.body_enable_bodies_around(b);
    }
    true
}

pub fn update_after_simulate<Ho: Host>(h: &Ho, b: H, time_step: f32) {
    update_after_simulate_bk(h, b, time_step, 0);
}

pub fn update_after_simulate_bk<Ho: Host>(h: &Ho, b: H, time_step: f32, book: u64) {
    let k = Slots { h, b, book };
    if k.static_movement() {
        let mut pos = h.body_local_position(b);
        let lin = k.total_static_linear();
        if lin != ZERO {
            pos = v3(
                fma(lin.x, time_step, pos.x),
                fma(lin.y, time_step, pos.y),
                fma(lin.z, time_step, pos.z),
            );
        }
        let ang = k.total_static_angular();
        if ang != ZERO {
            let rot = rotation(&h.body_local_matrix(b));
            let rot = matrix_mul(&matrix_rotate(mul3(ang, time_step), 0), &rot);
            h.body_set_matrix(b, &rot);
        }
        h.body_set_position(b, pos);
        k.set_total_static_linear(ZERO);
        k.set_total_static_angular(ZERO);
    }

    if k.disable_after_simulation() {
        k.set_disable_after_simulation(false);
        h.body_set_angular_velocity(b, ZERO);
        h.body_set_linear_velocity(b, ZERO);
    }

    if !k.has_slide() {
        if h.body_scrape_entity(b) != 0 {
            let count = k.slide_count();
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
                k.set_slide_count(count - 1);
            }
        }
    } else {
        k.set_slide_count(8);
    }

    if h.body_enabled(b) {
        h.body_update_roll_effect(b);
    }
}

pub fn static_linear_move<Ho: Host>(h: &Ho, b: H, v: Vector3f) {
    static_linear_move_bk(h, b, v, 0);
}

pub fn static_linear_move_bk<Ho: Host>(h: &Ho, b: H, v: Vector3f, book: u64) {
    let k = Slots { h, b, book };
    if h.body_mass(b) != 0.0 {
        return;
    }
    let t = k.total_static_linear();
    k.set_total_static_linear(v3(t.x + v.x, t.y + v.y, t.z + v.z));
    k.set_static_movement(true);
    h.body_add_to_update_list(b);
}

pub fn static_angular_move<Ho: Host>(h: &Ho, b: H, v: Vector3f) {
    static_angular_move_bk(h, b, v, 0);
}

pub fn static_angular_move_bk<Ho: Host>(h: &Ho, b: H, v: Vector3f, book: u64) {
    let k = Slots { h, b, book };
    if h.body_mass(b) != 0.0 {
        return;
    }
    let t = k.total_static_angular();
    k.set_total_static_angular(v3(t.x + v.x, t.y + v.y, t.z + v.z));
    k.set_static_movement(true);
    h.body_add_to_update_list(b);
}
