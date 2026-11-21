//! Callbacks into the C++ engine objects (bodies, joints, worlds, sound entities).
//! Handles are the C++ object addresses; 0 is null. Strings cross as (pointer, length).

use crate::backend::newton::body as nb;
use crate::types::{Matrixf, Vector3f};

pub type H = usize;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Str {
    pub p: *const u8,
    pub n: usize,
}

impl Default for Str {
    fn default() -> Self {
        Str::new(&[])
    }
}

impl Str {
    pub fn new(s: &[u8]) -> Self {
        Str {
            p: s.as_ptr(),
            n: s.len(),
        }
    }

    /// # Safety
    /// `p` points to `n` readable bytes, or `n` is 0.
    pub unsafe fn bytes<'a>(self) -> &'a [u8] {
        if self.n == 0 || self.p.is_null() {
            return &[];
        }
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(self.p, self.n) }
    }
}

const FLAG_HAS_SLIDE: i32 = 0;
const FLAG_HAS_COLLISION: i32 = 1;
const FLAG_STATIC_MOVEMENT: i32 = 2;
const FLAG_DISABLE_AFTER_SIM: i32 = 3;

/// The `BodyState` embedded in the C++ `iPhysicsBody` at `b`; handles are live body addresses.
fn body<'a>(b: H) -> &'a mut super::shell::BodyState {
    // SAFETY: Rust and the C++ shell only pass addresses of live bodies.
    unsafe { super::cxx_body::state(b as *const u8) }
}

fn flag_get(b: H, id: i32) -> bool {
    super::ffi::ohpl_h15_body_flag_get(body(b).book, id)
}

fn flag_set(b: H, id: i32, v: bool) {
    super::ffi::ohpl_h15_body_flag_set(body(b).book, id, v)
}

macro_rules! host {
    ($( $name:ident ( $($a:ident : $t:ty),* ) $(-> $r:ty)? ;)* ; direct { $( $dname:ident ( $($da:ident : $dt:ty),* ) $(-> $dr:ty)? $dbody:block )* }) => {
        pub trait Host {
            $( fn $name(&self, $($a: $t),*) $(-> $r)?; )*
            $( fn $dname(&self, $($da: $dt),*) $(-> $dr)?; )*
            fn body_local_matrix(&self, b: H) -> Matrixf;
            fn body_prev_scrape_matrix(&self, b: H) -> Matrixf;
            fn body_set_prev_scrape_matrix(&self, b: H, m: &Matrixf);
            fn body_set_matrix(&self, b: H, m: &Matrixf);
        }

        #[repr(C)]
        pub struct Vt {
            $( pub $name: unsafe extern "C" fn($($t),*) $(-> $r)?, )*
            pub body_local_matrix: unsafe extern "C" fn(H, *mut Matrixf),
            pub body_prev_scrape_matrix: unsafe extern "C" fn(H, *mut Matrixf),
            pub body_set_prev_scrape_matrix: unsafe extern "C" fn(H, *const Matrixf),
            pub body_set_matrix: unsafe extern "C" fn(H, *const Matrixf),
        }

        impl Host for &Vt {
            // SAFETY (every forwarder): the C++ side fills `Vt` with live function pointers.
            $( fn $name(&self, $($a: $t),*) $(-> $r)? { unsafe { (self.$name)($($a),*) } } )*
            $( fn $dname(&self, $($da: $dt),*) $(-> $dr)? $dbody )*
            fn body_local_matrix(&self, b: H) -> Matrixf {
                let mut m = Matrixf::default();
                // SAFETY: `m` is a valid out-pointer.
                unsafe { (self.body_local_matrix)(b, &mut m) };
                m
            }
            fn body_prev_scrape_matrix(&self, b: H) -> Matrixf {
                let mut m = Matrixf::default();
                // SAFETY: `m` is a valid out-pointer.
                unsafe { (self.body_prev_scrape_matrix)(b, &mut m) };
                m
            }
            fn body_set_prev_scrape_matrix(&self, b: H, m: &Matrixf) {
                // SAFETY: `m` is a valid in-pointer.
                unsafe { (self.body_set_prev_scrape_matrix)(b, m) }
            }
            fn body_set_matrix(&self, b: H, m: &Matrixf) {
                // SAFETY: `m` is a valid in-pointer.
                unsafe { (self.body_set_matrix)(b, m) }
            }
        }

        #[cfg(test)]
        impl Host for mock_support::Mock {
            $( fn $name(&self, $($a: $t),*) $(-> $r)? {
                self.log.borrow_mut().push(format!("{} {:?}", stringify!($name), ($(&$a,)*)));
                $( let r: $r = self.ret(stringify!($name)); r )?
            } )*
            $( fn $dname(&self, $($da: $dt),*) $(-> $dr)? {
                self.log.borrow_mut().push(format!("{} {:?}", stringify!($dname), ($(&$da,)*)));
                $( let r: $dr = self.ret(stringify!($dname)); r )?
            } )*
            fn body_local_matrix(&self, _b: H) -> Matrixf {
                self.local.get()
            }
            fn body_prev_scrape_matrix(&self, _b: H) -> Matrixf {
                self.prev.get()
            }
            fn body_set_prev_scrape_matrix(&self, _b: H, m: &Matrixf) {
                self.prev.set(*m);
                self.log.borrow_mut().push("body_set_prev_scrape_matrix".into());
            }
            fn body_set_matrix(&self, _b: H, m: &Matrixf) {
                self.log.borrow_mut().push(format!("body_set_matrix {:?}", m.m));
            }
        }
    };
}

host! {
    can_play_impact(physics: H) -> bool;
    add_impact(physics: H);

    pworld_world(pw: H) -> H;
    joint_world_world(j: H) -> H;
    body_world_world(b: H) -> H;
    world_silent(w: H) -> bool;
    world_sound_exists(w: H, e: H, id: i32) -> bool;
    world_create_sound(w: H, name: Str, file: Str) -> H;
    world_create_impact_ps(w: H, ps: Str, pos: Vector3f);

    ent_set_saved_false(e: H);
    ent_set_position(e: H, p: Vector3f);
    ent_set_param_named(e: H, name: Str, v: f32);
    ent_set_param_index(e: H, idx: i32, v: f32);
    ent_fade_in(e: H, t: f32);
    ent_fade_out(e: H, t: f32);
    ent_main_entry(e: H) -> H;
    ent_volume(e: H) -> f32;
    ent_set_volume(e: H, v: f32);
    ent_creation_id(e: H) -> i32;
    entry_set_speed_mul(y: H, v: f32);
    entry_set_volume_mul(y: H, v: f32);
    entry_fade_volume_mul_to(y: H, v: f32, t: f32);

    surf_impact_count(s: H) -> i32;
    surf_impact_min_speed(s: H, i: i32) -> f32;
    surf_impact_sound(s: H, i: i32) -> Str;
    surf_impact_ps(s: H, i: i32) -> Str;
    surf_impact_ps_prio(s: H, i: i32) -> i32;

    body_world_position(b: H) -> Vector3f;
    body_local_position(b: H) -> Vector3f;
    body_set_position(b: H, p: Vector3f);
    body_set_scrape_entity(b: H, e: H);
    body_set_roll_entity(b: H, e: H);
    body_add_to_update_list(b: H);
    body_enable_bodies_around(b: H);
    body_update_roll_effect(b: H);

    joint_force_size(j: H) -> f32;
    joint_angle(j: H) -> f32;
    joint_distance(j: H) -> f32;
    joint_parent(j: H) -> H;
    joint_child(j: H) -> H;
    joint_has_collided(j: H) -> bool;
    joint_set_has_collided(j: H, v: bool);
    joint_callback_max(j: H);
    joint_callback_min(j: H);
    joint_get_controller(j: H, name: Str) -> H;
    joint_controller_count(j: H) -> i32;
    joint_controller_at(j: H, i: i32) -> H;
    joint_warn_missing_controller(j: H, c: H);
    joint_pivot(j: H) -> Vector3f;

    ctrl_is_active(c: H) -> bool;
    ctrl_set_active(c: H, v: bool);
    ctrl_deactivate(c: H);
    ctrl_end_type(c: H) -> i32;
    ctrl_next_name(c: H) -> Str;
    ctrl_log(c: H, value: f32, dest: f32, error: f32, output: f32);

    ; direct {
        body_mass(b: H) -> f32 { nb::ohpl_ntb_mass(body(b).rust) }
        body_enabled(b: H) -> bool { nb::ohpl_ntb_enabled(body(b).rust) }
        body_linear_velocity(b: H) -> Vector3f {
            let mut v = Vector3f::default();
            // SAFETY: `v` is a valid out-pointer.
            unsafe { nb::ohpl_ntb_velocity(body(b).rust, &mut v) };
            v
        }
        body_angular_velocity(b: H) -> Vector3f {
            let mut v = Vector3f::default();
            // SAFETY: `v` is a valid out-pointer.
            unsafe { nb::ohpl_ntb_omega(body(b).rust, &mut v) };
            v
        }
        body_set_linear_velocity(b: H, v: Vector3f) {
            // SAFETY: `v` is a valid in-pointer.
            unsafe { nb::ohpl_ntb_set_velocity(body(b).rust, &v) }
        }
        body_set_angular_velocity(b: H, v: Vector3f) {
            // SAFETY: `v` is a valid in-pointer.
            unsafe { nb::ohpl_ntb_set_omega(body(b).rust, &v) }
        }
        body_mass_centre(b: H) -> Vector3f {
            let mut v = Vector3f::default();
            // SAFETY: `v` is a valid out-pointer.
            unsafe { nb::ohpl_ntb_mass_centre(body(b).rust, &mut v) };
            v
        }
        body_add_force(b: H, v: Vector3f) {
            let s = body(b);
            // SAFETY: `v` is a valid in-pointer.
            unsafe { nb::ohpl_ntb_add_force(s.rust, &v) };
            super::cxx_body::enable(s);
        }
        body_add_torque(b: H, v: Vector3f) {
            let s = body(b);
            // SAFETY: `v` is a valid in-pointer.
            unsafe { nb::ohpl_ntb_add_torque(s.rust, &v) };
            super::cxx_body::enable(s);
        }
        body_use_surface_effects(b: H) -> bool { body(b).use_surface_effects }
        body_scrape_body(b: H) -> H { body(b).scrape_body }
        body_set_scrape_body(b: H, other: H) { body(b).scrape_body = other }
        body_scrape_entity(b: H) -> H { body(b).scrape_sound }
        body_scrape_entity_id(b: H) -> i32 { body(b).scrape_sound_id }
        body_roll_entity(b: H) -> H { body(b).roll_sound }
        body_roll_entity_id(b: H) -> i32 { body(b).roll_sound_id }
        body_has_slide(b: H) -> bool { flag_get(b, FLAG_HAS_SLIDE) }
        body_set_has_slide(b: H, v: bool) { flag_set(b, FLAG_HAS_SLIDE, v) }
        body_has_collision(b: H) -> bool { flag_get(b, FLAG_HAS_COLLISION) }
        body_set_has_collision(b: H, v: bool) { flag_set(b, FLAG_HAS_COLLISION, v) }
        body_in_update_list(b: H) -> bool { body(b).in_update_list }
        body_static_movement(b: H) -> bool { flag_get(b, FLAG_STATIC_MOVEMENT) }
        body_set_static_movement(b: H, v: bool) { flag_set(b, FLAG_STATIC_MOVEMENT, v) }
        body_total_static_linear(b: H) -> Vector3f { body(b).total_static_linear }
        body_total_static_angular(b: H) -> Vector3f { body(b).total_static_angular }
        body_set_total_static_linear(b: H, v: Vector3f) { body(b).total_static_linear = v }
        body_set_total_static_angular(b: H, v: Vector3f) { body(b).total_static_angular = v }
        body_slide_count(b: H) -> i32 { body(b).slide_count }
        body_set_slide_count(b: H, v: i32) { body(b).slide_count = v }
        body_disable_after_simulation(b: H) -> bool { flag_get(b, FLAG_DISABLE_AFTER_SIM) }
        body_set_disable_after_simulation(b: H, v: bool) { flag_set(b, FLAG_DISABLE_AFTER_SIM, v) }
    }
}

#[cfg(test)]
pub mod mock_support {
    use super::*;
    use std::cell::{Cell, RefCell};
    use std::collections::HashMap;

    #[derive(Clone, Debug)]
    pub enum Val {
        B(bool),
        F(f32),
        I(i32),
        H(usize),
        V(Vector3f),
        S(Vec<u8>),
    }

    pub trait FromVal: Sized + Default {
        fn from_val(v: &Val) -> Option<Self>;
    }

    macro_rules! from_val {
        ($t:ty, $variant:ident) => {
            impl FromVal for $t {
                fn from_val(v: &Val) -> Option<Self> {
                    match v {
                        Val::$variant(x) => Some(*x),
                        _ => None,
                    }
                }
            }
        };
    }
    from_val!(bool, B);
    from_val!(f32, F);
    from_val!(i32, I);
    from_val!(usize, H);
    from_val!(Vector3f, V);

    impl FromVal for Str {
        fn from_val(v: &Val) -> Option<Self> {
            match v {
                Val::S(s) => Some(Str::new(Box::leak(s.clone().into_boxed_slice()))),
                _ => None,
            }
        }
    }

    #[derive(Default)]
    pub struct Mock {
        pub log: RefCell<Vec<String>>,
        pub rets: RefCell<HashMap<&'static str, Vec<Val>>>,
        pub counts: RefCell<HashMap<&'static str, usize>>,
        pub local: Cell<Matrixf>,
        pub prev: Cell<Matrixf>,
    }

    impl Mock {
        pub fn new() -> Self {
            let m = Mock::default();
            m.local.set(crate::math::IDENTITY);
            m.prev.set(crate::math::IDENTITY);
            m
        }

        /// Successive calls return `vals` in order, then the last one repeats.
        pub fn set(&self, name: &'static str, vals: Vec<Val>) {
            self.rets.borrow_mut().insert(name, vals);
        }

        pub fn ret<T: FromVal>(&self, name: &'static str) -> T {
            let mut counts = self.counts.borrow_mut();
            let n = counts.entry(name).or_insert(0);
            let rets = self.rets.borrow();
            let v = rets
                .get(name)
                .and_then(|vals| vals.get(*n).or(vals.last()))
                .and_then(T::from_val)
                .unwrap_or_default();
            *n += 1;
            v
        }

        pub fn calls(&self, prefix: &str) -> Vec<String> {
            self.log
                .borrow()
                .iter()
                .filter(|l| l.starts_with(prefix))
                .cloned()
                .collect()
        }
    }
}
