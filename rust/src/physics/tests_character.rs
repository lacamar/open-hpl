use super::character::{self, CB_GRAVITY, CharHost, CharState, S};
use super::rope::{self, Attachment, Body, P, Particle, R, RopeHost, RopeState};
use crate::math::v3;
use crate::types::{Matrixf, Vector3f};
use std::cell::{Cell, RefCell};
use std::mem::{offset_of, size_of};

#[test]
fn state_layouts_match_cpp() {
    assert_eq!(size_of::<CharState>(), 364);
    assert_eq!(offset_of!(CharState, mtx_move), 144);
    assert_eq!(offset_of!(CharState, pitch), 208);
    assert_eq!(offset_of!(CharState, on_ground_count), 336);
    assert_eq!(offset_of!(CharState, moving), 352);
    assert_eq!(offset_of!(CharState, accurate_climbing), 362);
    assert_eq!(size_of::<RopeState>(), 184);
    assert_eq!(size_of::<Attachment>(), 32);
    assert_eq!(offset_of!(RopeState, gravity_force), 64);
    assert_eq!(offset_of!(RopeState, damping_mul), 76);
    assert_eq!(offset_of!(RopeState, sleep_count), 156);
    assert_eq!(offset_of!(RopeState, sleeping), 172);
    assert_eq!(offset_of!(RopeState, has_updated), 178);
    assert_eq!(size_of::<Particle>(), 64);
    assert_eq!(offset_of!(Particle, position), 8);
    assert_eq!(offset_of!(Particle, inv_mass), 56);
}

struct FloorHost {
    floor_y: f32,
    half_height: f32,
    gravity_calls: Cell<u32>,
    hits: Cell<u32>,
    log: RefCell<Vec<&'static str>>,
}

impl FloorHost {
    fn new() -> Self {
        FloorHost {
            floor_y: 0.0,
            half_height: 0.9,
            gravity_calls: Cell::new(0),
            hits: Cell::new(0),
            log: RefCell::new(Vec::new()),
        }
    }
    fn note(&self, what: &'static str) {
        self.log.borrow_mut().push(what);
    }
}

impl CharHost for FloorHost {
    fn world_gravity(&self) -> Vector3f {
        self.gravity_calls.set(self.gravity_calls.get() + 1);
        v3(0.0, -9.8, 0.0)
    }
    fn shape_radius(&self) -> f32 {
        0.3
    }
    fn shape_size_y(&self, _: i32) -> f32 {
        self.half_height * 2.0
    }
    fn check_collision(&self, push: &mut Vector3f, pos: Vector3f, cb: i32, _: i32) -> bool {
        *push = v3(0.0, 0.0, 0.0);
        let depth = self.floor_y - (pos.y - self.half_height);
        if depth > 0.0 {
            *push = v3(0.0, depth, 0.0);
            if cb == CB_GRAVITY {
                self.note("gravity-collide");
            }
            return true;
        }
        false
    }
    fn cast_ray(&self, start: Vector3f, end: Vector3f, d: &mut f32, n: &mut Vector3f) -> bool {
        if start.y >= self.floor_y && end.y <= self.floor_y {
            *d = start.y - self.floor_y;
            *n = v3(0.0, 1.0, 0.0);
            return true;
        }
        false
    }
    fn on_hit_ground(&self) {
        self.hits.set(self.hits.get() + 1);
    }
    fn pre_connection(&self, _: f32) {
        self.note("pre");
    }
    fn clear_gravity_attachment(&self) {
        self.note("clear");
    }
    fn update_gravity_attachment(&self) {
        self.note("attach");
    }
    fn update_body(&self) {
        self.note("body");
    }
    fn update_character_connection(&self, _: f32) {
        self.note("char-conn");
    }
    fn update_body_connection(&self, _: f32) {
        self.note("body-conn");
    }
    fn update_camera(&self) {
        self.note("camera");
    }
    fn update_entity(&self) {
        self.note("entity");
    }
    fn post_connection(&self, _: f32) {
        self.note("post");
    }
    fn update_safe_positions(&self, _: f32) {
        self.note("safe");
    }
}

fn new_state() -> Box<CharState> {
    // SAFETY: `CharState` is plain floats, integers and bools, for which all-zero is valid.
    let mut st: Box<CharState> = Box::new(unsafe { std::mem::zeroed() });
    st.mass = 1.0;
    st.size = v3(0.6, 1.8, 0.6);
    st.max_gravity_speed = 30.0;
    st.ground_friction = 6.0;
    st.air_friction = 0.3;
    st.max_no_slide_slope_cos = 0.7;
    st.ground_angle_min_cos = 0.001;
    st.max_on_ground_count = 12;
    st.check_step_climb_interval = 0.05;
    st.max_step_height = 0.3;
    st.max_step_height_in_air = 0.3;
    st.step_climb_speed = 1.0;
    st.climb_forward_mul = 1.0;
    st.climb_height_add = 0.01;
    st.max_pos_move_speed = [10.0; 2];
    st.max_neg_move_speed = [-10.0; 2];
    st.move_acc = [20.0; 2];
    st.move_deacc = [20.0; 2];
    st.move_speed_mul = [1.0; 2];
    st.move_opposite_dir_acc_mul = [1.0; 2];
    st.gravity_active = true;
    st.test_collision = true;
    st.active = true;
    st.position = v3(0.0, 0.9, 0.0);
    st
}

fn step(h: &FloorHost, st: &mut CharState, dt: f32) {
    // SAFETY: `st` outlives the call and nothing else touches it.
    let s = unsafe { S::new(st) };
    character::update(h, &s, dt, false);
}

#[test]
fn update_order_is_fixed() {
    let h = FloorHost::new();
    let mut st = new_state();
    st.position.y = 5.0;
    step(&h, &mut st, 1.0 / 60.0);
    let log = h.log.borrow().clone();
    assert_eq!(
        log,
        [
            "pre",
            "clear",
            "attach",
            "body",
            "char-conn",
            "body-conn",
            "camera",
            "entity",
            "post",
            "safe"
        ]
    );
}

#[test]
fn inactive_character_does_nothing() {
    let h = FloorHost::new();
    let mut st = new_state();
    st.active = false;
    let before = st.position;
    step(&h, &mut st, 1.0 / 60.0);
    assert!(h.log.borrow().is_empty());
    assert_eq!(st.position, before);
}

#[test]
fn falls_lands_and_settles_on_ground() {
    let h = FloorHost::new();
    let mut st = new_state();
    st.position.y = 3.0;
    let mut hit_air = false;
    for _ in 0..240 {
        step(&h, &mut st, 1.0 / 60.0);
        hit_air |= st.on_ground_count == 0;
    }
    assert!(hit_air, "started airborne");
    assert!(st.on_ground_count > 0);
    assert!(h.hits.get() > 0);
    assert!((st.position.y - 0.9).abs() < 0.2, "y = {}", st.position.y);
    assert_eq!(st.velocity.x, 0.0);
}

#[test]
fn gravity_speed_is_capped() {
    let h = FloorHost::new();
    let mut st = new_state();
    st.position.y = 10_000.0;
    st.max_gravity_speed = 12.0;
    for _ in 0..600 {
        step(&h, &mut st, 1.0 / 60.0);
    }
    assert!(
        (st.velocity.y + 12.0).abs() < 1e-3,
        "vy = {}",
        st.velocity.y
    );
}

#[test]
fn diagonal_move_is_slowed_when_boost_disabled() {
    let h = FloorHost::new();
    let mut straight = new_state();
    let mut diagonal = new_state();
    for st in [&mut straight, &mut diagonal] {
        st.on_ground_count = 12;
        st.disable_diag_speed_boost = true;
    }
    for _ in 0..60 {
        straight.moving = [true, false];
        straight.current_move_acc = [1.0, 0.0];
        diagonal.moving = [true, true];
        diagonal.current_move_acc = [1.0, 1.0];
        step(&h, &mut straight, 1.0 / 60.0);
        step(&h, &mut diagonal, 1.0 / 60.0);
    }
    assert!((straight.move_speed[0] - 10.0).abs() < 1e-4);
    assert!((diagonal.move_speed[0] - 7.071).abs() < 1e-3);
}

#[test]
fn nan_velocity_does_not_panic() {
    let h = FloorHost::new();
    let mut st = new_state();
    st.velocity = v3(f32::NAN, f32::INFINITY, -f32::INFINITY);
    for dt in [0.0, -1.0, 1e9, f32::NAN] {
        step(&h, &mut st, dt);
    }
}

#[test]
fn fits_uses_lexicographic_epsilon() {
    let h = FloorHost::new();
    let (fits, push) = character::check_character_fits(&h, v3(0.0, 0.9, 0.0), false, 0, 0.0);
    assert!(fits && push == v3(0.0, 0.0, 0.0));
    // cVector3::operator< compares x first: a push of (0, 0.4, 0) is "less than" epsilon 0.1 (bug kept)
    let (fits, push) = character::check_character_fits(&h, v3(0.0, 0.5, 0.0), false, 0, 0.1);
    assert!(fits && push.y > 0.1);
    let (fits, push) = character::check_character_fits(&h, v3(0.0, 0.0, 0.0), true, 0, 0.0);
    assert!(fits && push == v3(0.0, 0.0, 0.0), "feet at the floor fit");
}

struct RopeMock {
    parts: Vec<*mut Particle>,
    sleeping_ok: Cell<bool>,
    set_sleeping_calls: RefCell<Vec<bool>>,
    total_len_calls: RefCell<Vec<f32>>,
    state: Cell<*mut RopeState>,
}

impl RopeHost for RopeMock {
    fn particles(&self) -> (*const *mut Particle, usize) {
        (self.parts.as_ptr(), self.parts.len())
    }
    fn check_specific_sleeping(&self) -> bool {
        self.sleeping_ok.get()
    }
    fn check_specific_awake(&self) -> bool {
        false
    }
    fn set_sleeping(&self, s: bool) {
        self.set_sleeping_calls.borrow_mut().push(s);
        // SAFETY: the tests keep the state alive and only touch it through `R` between calls.
        if let Some(st) = unsafe { self.state.get().as_mut() } {
            st.sleeping = s;
            st.sleep_count = 0;
            st.sleep_check_count = 0.0;
        }
    }
    fn set_total_length(&self, len: f32) {
        self.total_len_calls.borrow_mut().push(len);
    }
    fn update_motor_sound(&self, _: f32) {}
    fn cast_particle_ray(
        &self,
        _: Vector3f,
        _: Vector3f,
        _: &mut Vector3f,
        _: &mut Vector3f,
    ) -> bool {
        false
    }
    fn body_local_matrix(&self, _: Body) -> Matrixf {
        crate::math::IDENTITY
    }
    fn body_local_position(&self, _: Body) -> Vector3f {
        v3(0.0, 0.0, 0.0)
    }
    fn body_mass_centre(&self, _: Body) -> Vector3f {
        v3(0.0, 0.0, 0.0)
    }
    fn body_inertia_matrix(&self, _: Body) -> Matrixf {
        crate::math::IDENTITY
    }
    fn body_mass(&self, _: Body) -> f32 {
        1.0
    }
    fn body_add_force(&self, _: Body, _: Vector3f) {}
    fn body_add_torque(&self, _: Body, _: Vector3f) {}
    fn pid_output(&self, _: i32, _: f32, _: f32, _: Vector3f, _: f32) -> Vector3f {
        v3(0.0, 0.0, 0.0)
    }
}

fn particle(pos: Vector3f, inv_mass: f32) -> Box<Particle> {
    Box::new(Particle {
        container: std::ptr::null_mut(),
        position: pos,
        prev_position: pos,
        smooth_position: pos,
        force: v3(0.0, 0.0, 0.0),
        inv_mass,
    })
}

fn rope_state() -> Box<RopeState> {
    // SAFETY: `RopeState` is floats, ints, bools and raw pointers; all-zero is valid.
    let mut st: Box<RopeState> = Box::new(unsafe { std::mem::zeroed() });
    st.gravity_force = v3(0.0, -9.8, 0.0);
    st.damping_mul = 0.999;
    st.physics_active = true;
    st.max_iterations = 3;
    st.sleep_check_time = 1.0 / 3.0;
    st.sleep_check_sqr_limit = 0.0015 * 0.0015;
    st.sleep_max_count = 3;
    st.first_segment_length = 1.0;
    st.segment_length = 1.0;
    st.total_length = 2.0;
    st
}

#[test]
fn rope_hangs_from_fixed_start_and_keeps_segment_length() {
    let mut a = particle(v3(0.0, 5.0, 0.0), 0.0);
    let mut b = particle(v3(1.0, 5.0, 0.0), 1.0);
    let mut c = particle(v3(2.0, 5.0, 0.0), 1.0);
    let h = RopeMock {
        parts: vec![&mut *a, &mut *b, &mut *c],
        sleeping_ok: Cell::new(false),
        set_sleeping_calls: RefCell::new(Vec::new()),
        total_len_calls: RefCell::new(Vec::new()),
        state: Cell::new(std::ptr::null_mut()),
    };
    let mut st = rope_state();
    // SAFETY: `st` outlives `r` and nothing else touches it.
    let r = unsafe { R::new(&mut *st) };
    for _ in 0..120 {
        rope::update_before_simulate(&h, &r, 1.0 / 60.0);
    }
    assert_eq!(a.position, v3(0.0, 5.0, 0.0), "pinned particle never moves");
    assert!(c.position.y < 5.0, "free end fell");
    let d = |p: &Particle, q: &Particle| {
        ((p.position.x - q.position.x).powi(2)
            + (p.position.y - q.position.y).powi(2)
            + (p.position.z - q.position.z).powi(2))
        .sqrt()
    };
    assert!(
        (d(&a, &b) - 1.0).abs() < 0.05,
        "first segment {}",
        d(&a, &b)
    );
    assert!(
        (d(&b, &c) - 1.0).abs() < 0.05,
        "second segment {}",
        d(&b, &c)
    );
    assert!(st.has_updated);
}

#[test]
fn rope_falls_asleep_after_three_quiet_checks_and_stops_updating() {
    let mut a = particle(v3(0.0, 0.0, 0.0), 0.0);
    let mut b = particle(v3(1.0, 0.0, 0.0), 0.0);
    let h = RopeMock {
        parts: vec![&mut *a, &mut *b],
        sleeping_ok: Cell::new(true),
        set_sleeping_calls: RefCell::new(Vec::new()),
        total_len_calls: RefCell::new(Vec::new()),
        state: Cell::new(std::ptr::null_mut()),
    };
    let mut st = rope_state();
    // SAFETY: as above.
    h.state.set(&mut *st);
    let r = unsafe { R::new(&mut *st) };
    for _ in 0..60 {
        rope::update_before_simulate(&h, &r, 1.0 / 20.0);
    }
    assert_eq!(*h.set_sleeping_calls.borrow(), [true], "slept exactly once");
    assert_eq!(
        st.update_count, 9,
        "seven updates to the first check, then one per quiet check"
    );
}

#[test]
fn auto_move_clamps_total_length_and_stops() {
    let mut a = particle(v3(0.0, 0.0, 0.0), 0.0);
    let mut b = particle(v3(1.0, 0.0, 0.0), 1.0);
    let h = RopeMock {
        parts: vec![&mut *a, &mut *b],
        sleeping_ok: Cell::new(false),
        set_sleeping_calls: RefCell::new(Vec::new()),
        total_len_calls: RefCell::new(Vec::new()),
        state: Cell::new(std::ptr::null_mut()),
    };
    let mut st = rope_state();
    st.physics_active = false;
    st.auto_move_active = true;
    st.auto_move_acc = 100.0;
    st.auto_move_max_speed = 10.0;
    st.max_total_length = 2.5;
    st.min_total_length = 0.5;
    st.total_length = 2.0;
    // SAFETY: as above.
    let r = unsafe { R::new(&mut *st) };
    rope::update_before_simulate(&h, &r, 1.0);
    assert_eq!(*h.total_len_calls.borrow(), [2.5]);
    assert_eq!(st.auto_move_speed, 0.0);
}

#[test]
fn smooth_positions_interpolate_attachment_offsets() {
    let mut a = particle(v3(0.0, 0.0, 0.0), 0.0);
    let mut b = particle(v3(1.0, 0.0, 0.0), 1.0);
    let mut c = particle(v3(2.0, 0.0, 0.0), 1.0);
    let h = RopeMock {
        parts: vec![&mut *a, &mut *b, &mut *c],
        sleeping_ok: Cell::new(false),
        set_sleeping_calls: RefCell::new(Vec::new()),
        total_len_calls: RefCell::new(Vec::new()),
        state: Cell::new(std::ptr::null_mut()),
    };
    let mut st = rope_state();
    st.attached[1] = Attachment {
        body: 1,
        particle: &mut *c,
        body_local_pos: v3(2.0, 2.0, 0.0),
    };
    // SAFETY: as above.
    let r = unsafe { R::new(&mut *st) };
    rope::update_after_simulate(&h, &r);
    assert_eq!(a.smooth_position, v3(0.0, 0.0, 0.0));
    assert_eq!(b.smooth_position, v3(1.0, 1.0, 0.0));
    assert_eq!(c.smooth_position, v3(2.0, 2.0, 0.0));
    let _ = P(&mut *a);
}
