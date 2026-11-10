//! Particle emitter kernel (`iParticleEmitter` / `cParticleEmitter_UserData`): spawn, age, forces,
//! collision callback, colour/size curves, vertex generation and bounds in one call per emitter.
//!
//! Float expressions mirror the gcc build: `fma` marks a site where gcc contracted `a*b + c`.
//! The particle pool is a value array swapped like the C++ pointer array, so a recycled slot keeps
//! the fields `SetParticleDefaults` does not reset.

use crate::math::{
    IDENTITY, dot3, fast_random_float, fma, len3, matrix_mul_vec, matrix_rotate, modulus,
    normalize3, rand_rectf, rand_rectl, rotate_z, v3,
};
use crate::types::{Color, Matrixf, Vector2f, Vector2l, Vector3f};

unsafe extern "C" {
    fn powf(x: f32, y: f32) -> f32;
}

fn pow_f32(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { powf(x, y) }
}

const K2PI: f32 = std::f32::consts::TAU;

pub const START_BOX: i32 = 0;
pub const START_SPHERE: i32 = 1;
pub const SUBDIV_ANIMATION: i32 = 1;
pub const GRAVITY_VECTOR: i32 = 1;
pub const GRAVITY_CENTER: i32 = 2;
pub const SPIN_MOVEMENT: i32 = 1;
pub const DRAW_FIXED_POINT: i32 = 0;
pub const DRAW_DYNAMIC_POINT: i32 = 1;
pub const DRAW_LINE: i32 = 2;
pub const DRAW_AXIS: i32 = 3;
pub const COORD_LOCAL: i32 = 1;
pub const PE_BEAM: i32 = 1;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Particle {
    pub pos: Vector3f,
    pub last_pos: Vector3f,
    pub last_collide_pos: Vector3f,
    pub acc: Vector3f,
    pub vel: Vector3f,
    pub speed_mul: f32,
    pub max_speed: f32,
    pub start_color: Color,
    pub color: Color,
    pub start_size: Vector2f,
    pub size: Vector2f,
    pub start_life: f32,
    pub life: f32,
    pub life_size_middle_start: f32,
    pub life_size_middle_end: f32,
    pub life_color_middle_start: f32,
    pub life_color_middle_end: f32,
    pub sub_div_num: i32,
    pub bounce_amount: f32,
    pub bounce_count: i32,
    pub spin: f32,
    pub spin_vel: f32,
    pub spin_factor: f32,
    pub revolution_vel: Vector3f,
}

impl Particle {
    /// A pooled particle as `cParticle()` leaves it: zeroed, colours `cColor()` = (0, 0, 0, 1).
    fn fresh() -> Self {
        let black = Color {
            r: 0.0,
            g: 0.0,
            b: 0.0,
            a: 1.0,
        };
        Self {
            start_color: black,
            color: black,
            ..Self::default()
        }
    }
}

/// Mirror of `cParticleEmitterData_UserData`'s runtime fields (`RustParticles.h`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Params {
    pub pe_type: i32,
    pub draw_type: i32,
    pub coord_system: i32,
    pub sub_div_type: i32,
    pub start_pos_type: i32,
    pub start_vel_type: i32,
    pub gravity_type: i32,
    pub part_spin_type: i32,
    pub min_collision_max: i32,
    pub max_collision_max: i32,
    pub collision_update_rate: i32,
    pub uses_direction: bool,
    pub use_part_spin: bool,
    pub use_revolution: bool,
    pub multiply_rgb_with_alpha: bool,
    pub collides: bool,
    pub particles_per_second: f32,
    pub start_time_offset: f32,
    pub min_pause_length: f32,
    pub max_pause_length: f32,
    pub min_pause_interval: f32,
    pub max_pause_interval: f32,
    pub min_start_pos: Vector3f,
    pub max_start_pos: Vector3f,
    pub min_start_angles: Vector2f,
    pub max_start_angles: Vector2f,
    pub min_start_radius: f32,
    pub max_start_radius: f32,
    pub min_start_vel: Vector3f,
    pub max_start_vel: Vector3f,
    pub min_start_vel_angles: Vector2f,
    pub max_start_vel_angles: Vector2f,
    pub min_start_vel_speed: f32,
    pub max_start_vel_speed: f32,
    pub min_speed_multiply: f32,
    pub max_speed_multiply: f32,
    pub min_start_acc: Vector3f,
    pub max_start_acc: Vector3f,
    pub min_vel_maximum: f32,
    pub max_vel_maximum: f32,
    pub gravity_acc: Vector3f,
    pub min_spin_range: f32,
    pub max_spin_range: f32,
    pub min_rev_vel: Vector3f,
    pub max_rev_vel: Vector3f,
    pub min_life_span: f32,
    pub max_life_span: f32,
    pub min_start_size: Vector2f,
    pub max_start_size: Vector2f,
    pub start_rel_size: f32,
    pub middle_rel_size: f32,
    pub middle_rel_size_time: f32,
    pub middle_rel_size_length: f32,
    pub end_rel_size: f32,
    pub min_start_color: Color,
    pub max_start_color: Color,
    pub start_rel_color: Color,
    pub middle_rel_color: Color,
    pub middle_rel_color_time: f32,
    pub middle_rel_color_length: f32,
    pub end_rel_color: Color,
    pub min_bounce_amount: f32,
    pub max_bounce_amount: f32,
    pub frame_step: f32,
    pub max_frame_time: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GfxIn {
    pub hpl3_color: bool,
    pub add_blend: bool,
    pub fade_at_distance: bool,
    pub inverts_cull: bool,
    pub ps_color: Color,
    pub brightness: f32,
    pub light_color: Color,
    pub light_brightness: f32,
    pub fade_center: Vector3f,
    pub origin: Vector3f,
    pub min_fade_start: f32,
    pub min_fade_end: f32,
    pub max_fade_start: f32,
    pub max_fade_end: f32,
    pub draw_size: Vector2f,
    pub world: Matrixf,
    pub view: Matrixf,
    pub ps_world: Matrixf,
    pub axis_right: Vector3f,
    pub axis_forward: Vector3f,
    pub pos_stride: i32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Gfx {
    Hidden,
    Beam,
    Drawn,
}

/// World-space ray cast between two points; `Some((normal, point))` on the nearest hit.
pub type Collider<'a> = dyn FnMut(&Vector3f, &Vector3f) -> Option<(Vector3f, Vector3f)> + 'a;

pub struct Emitter {
    p: Params,
    particles: Vec<Particle>,
    num: u32,
    max: u32,
    sub_div_uv: Vec<[Vector3f; 4]>,
    materials: usize,
    time: f32,
    create_count: f32,
    collide_count: f32,
    pause_count: f32,
    pause_wait_count: f32,
    paused: bool,
    respawn: bool,
    dying: bool,
    frame: f32,
}

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

fn rand_v3(min: Vector3f, max: Vector3f) -> Vector3f {
    let x = rand_rectf(min.x, max.x);
    let y = rand_rectf(min.y, max.y);
    let z = rand_rectf(min.z, max.z);
    v3(x, y, z)
}

fn rand_v2(min: Vector2f, max: Vector2f) -> Vector2f {
    let x = rand_rectf(min.x, max.x);
    let y = rand_rectf(min.y, max.y);
    Vector2f { x, y }
}

fn rand_color(min: Color, max: Color) -> Color {
    let r = rand_rectf(min.r, max.r);
    let g = rand_rectf(min.g, max.g);
    let b = rand_rectf(min.b, max.b);
    let a = rand_rectf(min.a, max.a);
    Color { r, g, b, a }
}

fn col_mul(a: Color, b: Color) -> Color {
    Color {
        r: a.r * b.r,
        g: a.g * b.g,
        b: a.b * b.b,
        a: a.a * b.a,
    }
}

fn col_scale(a: Color, s: f32) -> Color {
    Color {
        r: a.r * s,
        g: a.g * s,
        b: a.b * s,
        a: a.a * s,
    }
}

fn col_add(a: Color, b: Color) -> Color {
    Color {
        r: a.r + b.r,
        g: a.g + b.g,
        b: a.b + b.b,
        a: a.a + b.a,
    }
}

fn splat(v: f32, a: f32) -> Color {
    Color {
        r: v,
        g: v,
        b: v,
        a,
    }
}

fn linear(c: Color) -> Color {
    col_mul(
        c,
        Color {
            r: c.r,
            g: c.g,
            b: c.b,
            a: 1.0,
        },
    )
}

fn max_f(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

pub(super) fn rotation_part(m: &Matrixf) -> Matrixf {
    let [r0, r1, r2, _] = m.m;
    Matrixf {
        m: [
            [r0[0], r0[1], r0[2], 0.0],
            [r1[0], r1[1], r1[2], 0.0],
            [r2[0], r2[1], r2[2], 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

pub(super) fn translation(m: &Matrixf) -> Vector3f {
    let [r0, r1, r2, _] = m.m;
    v3(r0[3], r1[3], r2[3])
}

/// `a*b + c*d` as gcc contracts it: the first product is fused.
fn mul_add2(a: f32, b: f32, c: f32, d: f32) -> f32 {
    fma(a, b, c * d)
}

/// `(s * k1) * t + (s * k2) * (1 - t)` per component, as gcc contracts the inline `cVector2`.
fn lerp_size(s: Vector2f, k1: f32, k2: f32, t: f32) -> Vector2f {
    let u = 1.0 - t;
    Vector2f {
        x: mul_add2(s.x * k1, t, s.x * k2, u),
        y: mul_add2(s.y * k1, t, s.y * k2, u),
    }
}

impl Emitter {
    pub fn new(
        p: Params,
        max_particles: u32,
        respawn: bool,
        sub_div: Vector2l,
        materials: usize,
    ) -> Self {
        let mut e = Self {
            p,
            particles: vec![Particle::fresh(); max_particles as usize],
            num: 0,
            max: max_particles,
            sub_div_uv: Vec::new(),
            materials,
            time: 0.0,
            create_count: 0.0,
            collide_count: 0.0,
            pause_count: 0.0,
            pause_wait_count: 0.0,
            paused: false,
            respawn,
            dying: false,
            frame: 0.0,
        };
        e.collide_count = 1.0 / p.collision_update_rate as f32;
        e.set_sub_div_uv(sub_div);
        e
    }

    // C++ tests `x > 1 || x > 1`: a y-only subdivision is ignored.
    fn set_sub_div_uv(&mut self, sd: Vector2l) {
        if !(sd.x > 1 && sd.y > 0) {
            return;
        }
        let inv_w = 1.0f32 / sd.x as f32;
        let inv_h = 1.0f32 / sd.y as f32;
        let n = (sd.x as usize).saturating_mul(sd.y as usize);
        self.sub_div_uv = vec![[v3(0.0, 0.0, 0.0); 4]; n];
        for x in 0..sd.x {
            for y in 0..sd.y {
                let idx = (y as usize) * (sd.x as usize) + x as usize;
                let (fx, fy) = (x as f32, y as f32);
                if let Some(s) = self.sub_div_uv.get_mut(idx) {
                    *s = [
                        v3((fx + 1.0) * inv_w, (fy + 1.0) * inv_h, 0.0),
                        v3(fx * inv_w, (fy + 1.0) * inv_h, 0.0),
                        v3(fx * inv_w, fy * inv_h, 0.0),
                        v3((fx + 1.0) * inv_w, fy * inv_h, 0.0),
                    ];
                }
            }
        }
    }

    pub fn num_particles(&self) -> u32 {
        self.num
    }

    pub fn is_dying(&self) -> bool {
        !self.respawn || self.dying
    }

    pub fn is_dead(&self) -> bool {
        self.num == 0 && self.dying
    }

    pub fn kill(&mut self) {
        self.respawn = false;
    }

    pub fn kill_instantly(&mut self) {
        self.max = 0;
        self.num = 0;
        self.dying = true;
    }

    pub fn frame(&self) -> f32 {
        self.frame
    }

    pub fn particle(&self, i: usize) -> Option<&Particle> {
        self.particles.get(i)
    }

    pub fn debug_state(&self) -> [f32; 8] {
        [
            self.time,
            self.create_count,
            self.collide_count,
            self.pause_count,
            self.pause_wait_count,
            f32::from(u8::from(self.paused)),
            self.max as f32,
            f32::from(u8::from(self.dying)),
        ]
    }

    pub fn sub_div_count(&self) -> usize {
        self.sub_div_uv.len()
    }

    fn set_defaults(&mut self, pi: usize, start: &Matrixf) {
        let p = self.p;
        let mut a = self.particles.get(pi).copied().unwrap_or_default();

        a.start_color = rand_color(p.min_start_color, p.max_start_color);
        a.color = col_mul(a.start_color, p.start_rel_color);

        if p.min_start_size.y == 0.0 && p.max_start_size.y == 0.0 {
            let s = rand_rectf(p.min_start_size.x, p.max_start_size.x);
            a.start_size = Vector2f { x: s, y: s };
        } else {
            a.start_size = rand_v2(p.min_start_size, p.max_start_size);
        }
        a.size = Vector2f {
            x: a.start_size.x * p.start_rel_size,
            y: a.start_size.y * p.start_rel_size,
        };

        if self.sub_div_uv.len() > 1 {
            a.sub_div_num = if p.sub_div_type == SUBDIV_ANIMATION {
                0
            } else {
                rand_rectl(0, self.sub_div_uv.len() as i32 - 1)
            };
        }

        a.bounce_amount = rand_rectf(p.min_bounce_amount, p.max_bounce_amount);
        a.bounce_count = rand_rectl(p.min_collision_max, p.max_collision_max);

        let trans = translation(start);
        if p.start_pos_type == START_BOX {
            a.pos = add3(trans, rand_v3(p.min_start_pos, p.max_start_pos));
        } else if p.start_pos_type == START_SPHERE {
            let rot = rand_v2(p.min_start_angles, p.max_start_angles);
            let mtx = matrix_rotate(v3(rot.x, rot.y, 0.0), 0);
            let r = rand_rectf(p.min_start_radius, p.max_start_radius);
            a.pos = add3(trans, matrix_mul_vec(&mtx, v3(0.0, r, 0.0)));
        }

        a.last_pos = a.pos;
        a.last_collide_pos = a.pos;

        if p.start_vel_type == START_BOX {
            a.vel = rand_v3(p.min_start_vel, p.max_start_vel);
        } else if p.start_vel_type == START_SPHERE {
            let rot = rand_v2(p.min_start_vel_angles, p.max_start_vel_angles);
            let mtx = matrix_rotate(v3(rot.x, rot.y, 0.0), 0);
            let s = rand_rectf(p.min_start_vel_speed, p.max_start_vel_speed);
            a.vel = matrix_mul_vec(&mtx, v3(0.0, s, 0.0));
        }

        if p.uses_direction && p.coord_system == 0 {
            a.vel = matrix_mul_vec(&rotation_part(start), a.vel);
        }

        a.max_speed = rand_rectf(p.min_vel_maximum, p.max_vel_maximum);
        a.speed_mul = rand_rectf(p.min_speed_multiply, p.max_speed_multiply);
        a.acc = rand_v3(p.min_start_acc, p.max_start_acc);

        if p.part_spin_type == 0 {
            a.spin_vel = rand_rectf(p.min_spin_range, p.max_spin_range);
        } else if p.part_spin_type == SPIN_MOVEMENT {
            a.spin_factor = rand_rectf(p.min_spin_range, p.max_spin_range);
            a.spin_vel = 0.0;
        }
        a.spin = rand_rectf(0.0, K2PI);

        a.revolution_vel = rand_v3(p.min_rev_vel, p.max_rev_vel);

        a.start_life = rand_rectf(p.min_life_span, p.max_life_span);
        a.life = a.start_life;
        a.life_size_middle_start = a.life * (1.0 - p.middle_rel_size_time);
        a.life_size_middle_end =
            a.life * (1.0 - (p.middle_rel_size_time + p.middle_rel_size_length));
        a.life_color_middle_start = a.life * (1.0 - p.middle_rel_color_time);
        a.life_color_middle_end =
            a.life * (1.0 - (p.middle_rel_color_time + p.middle_rel_color_length));

        if let Some(slot) = self.particles.get_mut(pi) {
            *slot = a;
        }
    }

    fn swap_remove(&mut self, i: u32) {
        let last = self.num.wrapping_sub(1);
        if i < last {
            self.particles.swap(i as usize, last as usize);
        }
        self.num = self.num.wrapping_sub(1);
    }

    /// `UpdateMotion`. `start` is the world matrix (world coords) or local matrix (local coords).
    pub fn update_motion(&mut self, dt: f32, start: &Matrixf, collider: &mut Collider<'_>) {
        let p = self.p;
        if self.time < p.start_time_offset {
            self.time += dt;
            return;
        }

        if self.paused {
            self.pause_count -= dt;
            if self.pause_count <= 0.0 {
                self.paused = false;
                self.pause_wait_count = rand_rectf(p.min_pause_interval, p.max_pause_interval);
            }
        } else if p.min_pause_length > 0.0 && p.min_pause_interval > 0.0 {
            self.pause_wait_count -= dt;
            if self.pause_wait_count <= 0.0 {
                self.pause_count = rand_rectf(p.min_pause_length, p.max_pause_length);
                self.paused = true;
            }
        }

        if !self.paused && self.num < self.max {
            self.create_count = fma(p.particles_per_second, dt, self.create_count);
            while self.create_count >= 0.99999 && self.num < self.max {
                self.num += 1;
                let pi = (self.num - 1) as usize;
                self.set_defaults(pi, start);
                self.create_count -= 1.0;
            }
        }

        let mut colliding = false;
        if p.collides {
            self.collide_count -= dt;
            if self.collide_count <= 0.0 {
                self.collide_count = 1.0 / p.collision_update_rate as f32;
                colliding = true;
            }
        }

        let origin = translation(start);
        let mut i = 0u32;
        while i < self.num {
            let mut pi = i as usize;
            let Some(mut a) = self.particles.get(pi).copied() else {
                break;
            };

            a.last_pos = a.pos;
            a.pos = v3(
                fma(a.vel.x, dt, a.pos.x),
                fma(a.vel.y, dt, a.pos.y),
                fma(a.vel.z, dt, a.pos.z),
            );
            a.vel = v3(
                fma(a.acc.x, dt, a.vel.x),
                fma(a.acc.y, dt, a.vel.y),
                fma(a.acc.z, dt, a.vel.z),
            );

            match p.gravity_type {
                GRAVITY_VECTOR => {
                    a.vel = v3(
                        fma(p.gravity_acc.x, dt, a.vel.x),
                        fma(p.gravity_acc.y, dt, a.vel.y),
                        fma(p.gravity_acc.z, dt, a.vel.z),
                    );
                }
                GRAVITY_CENTER => {
                    let mut dir = v3(a.pos.x - origin.x, a.pos.y - origin.y, a.pos.z - origin.z);
                    normalize3(&mut dir);
                    let g = p.gravity_acc.y;
                    a.vel = v3(
                        fma(dir.x * g, dt, a.vel.x),
                        fma(dir.y * g, dt, a.vel.y),
                        fma(dir.z * g, dt, a.vel.z),
                    );
                }
                _ => {}
            }

            if a.max_speed > 0.0 {
                let speed = len3(a.vel);
                if speed > a.max_speed {
                    a.vel = v3(
                        a.vel.x / speed * a.max_speed,
                        a.vel.y / speed * a.max_speed,
                        a.vel.z / speed * a.max_speed,
                    );
                }
            }

            if a.speed_mul != 0.0 && a.speed_mul != 1.0 {
                let m = pow_f32(a.speed_mul, dt);
                a.vel = v3(a.vel.x * m, a.vel.y * m, a.vel.z * m);
            }

            if p.use_part_spin {
                a.spin = fma(a.spin_vel, dt, a.spin);
                if p.part_spin_type == SPIN_MOVEMENT {
                    a.spin_vel = len3(a.vel) * a.spin_factor;
                }
                if a.spin >= K2PI {
                    a.spin -= K2PI;
                } else if a.spin <= -K2PI {
                    a.spin += K2PI;
                }
            }

            if p.use_revolution {
                let r = a.revolution_vel;
                let mtx = matrix_rotate(v3(r.x * dt, r.y * dt, r.z * dt), 0);
                a.pos = matrix_mul_vec(&mtx, a.pos);
                a.vel = matrix_mul_vec(&mtx, a.vel);
            }

            if colliding {
                if let Some((normal, point)) = collider(&a.last_collide_pos, &a.pos) {
                    a.pos = point;
                    let speed = len3(a.vel);
                    let d2 = dot3(a.vel, normal);
                    let mut refl = v3(
                        fma(-(normal.x * 2.0), d2, a.vel.x),
                        fma(-(normal.y * 2.0), d2, a.vel.y),
                        fma(-(normal.z * 2.0), d2, a.vel.z),
                    );
                    normalize3(&mut refl);
                    let s = speed * a.bounce_amount;
                    a.vel = v3(refl.x * s, refl.y * s, refl.z * s);
                    a.bounce_count = a.bounce_count.wrapping_sub(1);
                    if a.bounce_count <= 0 {
                        a.life = 0.0;
                    }
                }
                a.last_collide_pos = a.pos;
            }

            a.life -= dt;

            if a.life <= 0.0 {
                if let Some(slot) = self.particles.get_mut(pi) {
                    *slot = a;
                }
                if self.respawn {
                    if self.paused {
                        pi = self.remove_at(i, pi);
                    } else {
                        self.set_defaults(pi, start);
                    }
                } else {
                    pi = self.remove_at(i, pi);
                    self.max = self.max.wrapping_sub(1);
                    if self.max == 0 {
                        self.dying = true;
                    }
                }
                a = self.particles.get(pi).copied().unwrap_or(a);
            }

            if p.sub_div_type == SUBDIV_ANIMATION {
                let percent = 1.0 - a.life / a.start_life;
                let n = self.sub_div_uv.len() as f32;
                a.sub_div_num = fma(percent, n, -0.0001) as i32;
            }

            if a.life > a.life_color_middle_start {
                let t = (a.life - a.life_color_middle_start)
                    / (a.start_life - a.life_color_middle_start);
                a.color = col_add(
                    col_scale(col_mul(a.start_color, p.start_rel_color), t),
                    col_scale(col_mul(a.start_color, p.middle_rel_color), 1.0 - t),
                );
            } else if a.life > a.life_color_middle_end {
                a.color = col_mul(a.start_color, p.middle_rel_color);
            } else {
                let t = a.life / a.life_color_middle_end;
                a.color = col_add(
                    col_scale(col_mul(a.start_color, p.middle_rel_color), t),
                    col_scale(col_mul(a.start_color, p.end_rel_color), 1.0 - t),
                );
            }
            if p.multiply_rgb_with_alpha {
                a.color.r *= a.color.a;
                a.color.g *= a.color.a;
                a.color.b *= a.color.a;
            }

            if a.life > a.life_size_middle_start {
                let t =
                    (a.life - a.life_size_middle_start) / (a.start_life - a.life_size_middle_start);
                a.size = lerp_size(a.start_size, p.start_rel_size, p.middle_rel_size, t);
            } else if a.life > a.life_size_middle_end {
                a.size = Vector2f {
                    x: a.start_size.x * p.middle_rel_size,
                    y: a.start_size.y * p.middle_rel_size,
                };
            } else {
                let t = a.life / a.life_size_middle_end;
                a.size = lerp_size(a.start_size, p.middle_rel_size, p.end_rel_size, t);
            }

            if let Some(slot) = self.particles.get_mut(pi) {
                *slot = a;
            }
            i += 1;
        }

        if self.materials > 1 {
            self.frame = fma(p.frame_step, dt, self.frame);
            if self.frame >= p.max_frame_time {
                self.frame = 0.0;
            }
        }
    }

    /// `SwapRemove(i)`; returns the slot the removed particle now occupies.
    fn remove_at(&mut self, i: u32, pi: usize) -> usize {
        let last = self.num.wrapping_sub(1);
        self.swap_remove(i);
        if i < last { last as usize } else { pi }
    }

    pub fn bounds(
        &self,
        world_pos: Vector3f,
        right: Vector3f,
        forward: Vector3f,
        max_draw: Vector2f,
    ) -> (Vector3f, Vector3f) {
        let mut mn = world_pos;
        let mut mx = world_pos;
        for a in self.particles.iter().take(self.num as usize) {
            let q = a.pos;
            if q.x < mn.x {
                mn.x = q.x;
            } else if q.x > mx.x {
                mx.x = q.x;
            }
            if q.y < mn.y {
                mn.y = q.y;
            } else if q.y > mx.y {
                mx.y = q.y;
            }
            if q.z < mn.z {
                mn.z = q.z;
            } else if q.z > mx.z {
                mx.z = q.z;
            }
        }
        if self.p.draw_type == DRAW_AXIS {
            let add = v3(
                fma(right.x, max_draw.x, forward.x * max_draw.y),
                fma(right.y, max_draw.x, forward.y * max_draw.y),
                fma(right.z, max_draw.x, forward.z * max_draw.y),
            );
            mx = add3(mx, add);
            mn = v3(mn.x - add.x, mn.y - add.y, mn.z - add.z);
        } else {
            mx = v3(mx.x + max_draw.x, mx.y + max_draw.y, mx.z + max_draw.x);
            mn = v3(mn.x - max_draw.x, mn.y - max_draw.y, mn.z - max_draw.x);
        }
        (mn, mx)
    }

    /// `UpdateGraphicsForViewport` up to the vertex buffer calls: fills position and colour (and
    /// UV for subdivided sheets) of the first `num` quads.
    pub fn build_vertices(
        &self,
        g: &GfxIn,
        pos: &mut [f32],
        col: &mut [f32],
        tex: &mut [f32],
    ) -> Gfx {
        let p = &self.p;
        let mut cm = g.ps_color;
        if g.hpl3_color {
            cm = col_mul(linear(g.ps_color), splat(g.brightness, 1.0));
            let light = col_mul(linear(g.light_color), splat(g.light_brightness, 1.0));
            let f_max = max_f(light.r, max_f(light.g, light.b));
            if g.add_blend || f_max > 1.0 {
                cm = col_mul(cm, light);
            } else if f_max <= 0.0 {
                cm = Color {
                    r: 0.0,
                    g: 0.0,
                    b: 0.0,
                    a: 1.0,
                };
            } else {
                cm = col_mul(
                    cm,
                    Color {
                        r: light.r / f_max,
                        g: light.g / f_max,
                        b: light.b / f_max,
                        a: f_max,
                    },
                );
            }
        }

        if g.fade_at_distance {
            let d = v3(
                g.origin.x - g.fade_center.x,
                g.origin.y - g.fade_center.y,
                g.origin.z - g.fade_center.z,
            );
            let dist_sqr = dot3(d, d);
            let (min_start, max_start) = (g.min_fade_start, g.max_fade_start);
            let (min_end, max_end) = (g.min_fade_end, g.max_fade_end);
            if min_start > 0.0 && dist_sqr < min_start * min_start {
                if dist_sqr <= min_end * min_end {
                    cm.a = 0.0;
                } else {
                    let dist = dist_sqr.sqrt();
                    cm.a *= (dist - min_end) / (min_start - min_end);
                }
            }
            if max_start > 0.0 && dist_sqr > max_start * max_start {
                if dist_sqr >= max_end * max_end {
                    cm.a = 0.0;
                } else {
                    let dist = dist_sqr.sqrt();
                    cm.a *= 1.0 - (dist - max_start) / (max_end - max_start);
                }
            }
        }
        if p.multiply_rgb_with_alpha {
            cm.r *= cm.a;
            cm.g *= cm.a;
            cm.b *= cm.a;
        }
        if cm.a <= 0.0 {
            return Gfx::Hidden;
        }

        let mut draw = g.draw_size;
        if g.hpl3_color {
            let w = &g.world.m;
            let at =
                |r: usize, c: usize| w.get(r).and_then(|row| row.get(c)).copied().unwrap_or(0.0);
            draw.x *= len3(v3(at(0, 0), at(1, 0), at(2, 0)));
            draw.y *= len3(v3(at(0, 1), at(1, 1), at(2, 1)));
        }

        if p.pe_type == PE_BEAM {
            return Gfx::Beam;
        }

        let n = self.num as usize;
        let parts = self.particles.iter().take(n);

        if self.sub_div_uv.len() > 1 {
            for (i, a) in parts.clone().enumerate() {
                let Some(sd) = usize::try_from(a.sub_div_num)
                    .ok()
                    .and_then(|k| self.sub_div_uv.get(k))
                else {
                    continue;
                };
                for (k, uv) in sd.iter().enumerate() {
                    put3(tex, i * 12 + k * 3, *uv);
                }
            }
        }

        let stride = usize::try_from(g.pos_stride).unwrap_or(0);
        let quad = stride * 4;
        let local = p.coord_system == COORD_LOCAL;
        let to_view = |q: Vector3f| -> Vector3f {
            let w = if local {
                matrix_mul_vec(&g.ps_world, q)
            } else {
                q
            };
            matrix_mul_vec(&g.view, w)
        };
        let color_of = |a: &Particle| -> Color {
            col_mul(
                if g.hpl3_color {
                    linear(a.color)
                } else {
                    a.color
                },
                cm,
            )
        };
        let mut emit = |i: usize, k: usize, v: Vector3f, c: Color| {
            put3(pos, i * quad + k * stride, v);
            put4(col, i * 16 + k * 4, c);
        };

        let flip = g.inverts_cull;
        match p.draw_type {
            DRAW_FIXED_POINT | DRAW_DYNAMIC_POINT => {
                let mut add = [
                    v3(draw.x, -draw.y, 0.0),
                    v3(-draw.x, -draw.y, 0.0),
                    v3(-draw.x, draw.y, 0.0),
                    v3(draw.x, draw.y, 0.0),
                ];
                if flip {
                    for v in &mut add {
                        v.y = -v.y;
                    }
                }
                let dynamic = p.draw_type == DRAW_DYNAMIC_POINT;
                let rot = p.use_part_spin;
                for (i, a) in parts.enumerate() {
                    let v = to_view(a.pos);
                    let c = color_of(a);
                    let sz = v3(a.size.x, a.size.y, 0.0);
                    let rm = if dynamic && rot {
                        rotate_z(a.spin)
                    } else {
                        IDENTITY
                    };
                    for (k, ad) in add.iter().enumerate() {
                        let q = if !dynamic {
                            add3(v, *ad)
                        } else if rot {
                            let s = v3(ad.x * sz.x, ad.y * sz.y, ad.z * sz.z);
                            add3(v, matrix_mul_vec(&rm, s))
                        } else {
                            v3(
                                fma(ad.x, sz.x, v.x),
                                fma(ad.y, sz.y, v.y),
                                fma(ad.z, sz.z, v.z),
                            )
                        };
                        emit(i, k, q, c);
                    }
                }
            }
            DRAW_LINE => {
                for (i, a) in parts.enumerate() {
                    let v1 = to_view(a.pos);
                    let v2 = to_view(a.last_pos);
                    let (mut dir_y, mut dir_x);
                    if v1 == v2 {
                        dir_y = v3(0.0, 1.0, 0.0);
                        dir_x = Vector2f { x: 1.0, y: 0.0 };
                    } else {
                        dir_y = v3(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
                        normalize3(&mut dir_y);
                        dir_x = Vector2f {
                            x: dir_y.y,
                            y: -dir_y.x,
                        };
                        let len = fma(dir_x.x, dir_x.x, dir_x.y * dir_x.y).sqrt();
                        if f64::from(len) > 1e-08 {
                            let inv = 1.0f32 / len;
                            dir_x.x *= inv;
                            dir_x.y *= inv;
                        }
                    }
                    dir_x = Vector2f {
                        x: dir_x.x * draw.x * a.size.x,
                        y: dir_x.y * draw.x * a.size.x,
                    };
                    dir_y = v3(
                        dir_y.x * draw.y * a.size.y,
                        dir_y.y * draw.y * a.size.y,
                        dir_y.z * draw.y * a.size.y,
                    );
                    if flip {
                        dir_y = v3(-dir_y.x, -dir_y.y, -dir_y.z);
                    }
                    let c = color_of(a);
                    let (nx, ny) = (v3(-dir_y.x, -dir_y.y, -dir_y.z), dir_y);
                    let line = |base: Vector3f, y: Vector3f, sx: f32| {
                        v3(
                            base.x + y.x + dir_x.x * sx,
                            base.y + y.y + dir_x.y * sx,
                            base.z + y.z,
                        )
                    };
                    emit(i, 0, line(v2, nx, 1.0), c);
                    emit(i, 1, line(v2, nx, -1.0), c);
                    emit(i, 2, line(v1, ny, -1.0), c);
                    emit(i, 3, line(v1, ny, 1.0), c);
                }
            }
            DRAW_AXIS => {
                let (r, f) = (g.axis_right, g.axis_forward);
                for (i, a) in parts.enumerate() {
                    let v = if local {
                        matrix_mul_vec(&g.ps_world, a.pos)
                    } else {
                        a.pos
                    };
                    let (sx, sy) = (a.size.x, a.size.y);
                    let comb = |cx: f32, cy: f32| {
                        v3(
                            fma(r.x, cx, f.x * cy),
                            fma(r.y, cx, f.y * cy),
                            fma(r.z, cx, f.z * cy),
                        )
                    };
                    let adds = [comb(sx, sy), comb(-sx, sy), comb(-sx, -sy), comb(sx, -sy)];
                    let c = color_of(a);
                    for (k, ad) in adds.iter().enumerate() {
                        emit(i, k, add3(v, *ad), c);
                    }
                }
            }
            _ => {}
        }
        Gfx::Drawn
    }
}

fn put3(dst: &mut [f32], at: usize, v: Vector3f) {
    if let Some(s) = dst.get_mut(at..at + 3) {
        s.copy_from_slice(&[v.x, v.y, v.z]);
    }
}

fn put4(dst: &mut [f32], at: usize, c: Color) {
    if let Some(s) = dst.get_mut(at..at + 4) {
        s.copy_from_slice(&[c.r, c.g, c.b, c.a]);
    }
}

pub const ENV_MAX_ITER: usize = 30;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EnvVelIn {
    pub gravity_vel: Vector3f,
    pub wind_vel: Vector3f,
    pub rotate_vel: Vector3f,
    pub gravity_speed_rand: f32,
    pub wind_speed_rand: f32,
    pub wind_dir_rand: f32,
    pub rotate_speed_rand: f32,
    pub rotate_both_dirs: bool,
}

pub type EnvVecs = [Vector3f; ENV_MAX_ITER];

fn jitter(v: Vector3f, amount: f32, r: f32) -> Vector3f {
    v3(
        fma(v.x * amount, r, v.x),
        fma(v.y * amount, r, v.y),
        fma(v.z * amount, r, v.z),
    )
}

/// `cEnvironmentParticles::UpdateVelocityVectors` body (the dirty flag stays in C++).
pub fn env_velocity_vectors(
    i: &EnvVelIn,
    grav: &mut EnvVecs,
    wind: &mut EnvVecs,
    rot: &mut EnvVecs,
) {
    let seed = |a: &mut EnvVecs, n: usize, v: Vector3f| {
        if let Some(e) = a.get_mut(n) {
            *e = v;
        }
    };
    seed(grav, 0, i.gravity_vel);
    seed(wind, 0, i.wind_vel);
    seed(rot, 0, i.rotate_vel);
    for n in 1..ENV_MAX_ITER {
        let s = 22 + 13 * (n as i32 - 1);
        let fr = fast_random_float;
        let g = jitter(i.gravity_vel, i.gravity_speed_rand, fr(s - 9));
        let mut w = jitter(i.wind_vel, i.wind_speed_rand, fr(s - 8));
        let mut r = jitter(i.rotate_vel, i.rotate_speed_rand, fr(s - 7));
        if i.rotate_both_dirs {
            if fr(s - 6) > 0.0 {
                r.x = -r.x;
            }
            if fr(s - 5) > 0.0 {
                r.y = -r.y;
            }
            if fr(s - 4) > 0.0 {
                r.z = -r.z;
            }
        }
        if i.wind_dir_rand > 0.0 {
            let mut axis = v3(fr(s - 3), fr(s - 2), fr(s - 1));
            normalize3(&mut axis);
            let k = i.wind_dir_rand * std::f32::consts::PI * fr(s);
            let m = matrix_rotate(v3(axis.x * k, axis.y * k, axis.z * k), 0);
            w = matrix_mul_vec(&m, w);
        }
        seed(grav, n, g);
        seed(wind, n, w);
        seed(rot, n, r);
    }
}

fn wrap_big(x: f32) -> f32 {
    if x.abs() > 10000.0 {
        modulus(x, 10000.0)
    } else {
        x
    }
}

/// `cEnvironmentParticles::Update` iteration loop.
pub fn env_update(
    n: usize,
    dt: f32,
    vel: [&EnvVecs; 3],
    offset: &mut EnvVecs,
    rotation: &mut EnvVecs,
) {
    let [grav, wind, rotv] = vel;
    let it = offset.iter_mut().zip(rotation.iter_mut());
    let vs = grav.iter().zip(wind.iter()).zip(rotv.iter());
    for ((off, rot), ((g, w), rv)) in it.zip(vs).take(n) {
        let (sx, sy, sz) = (g.x + w.x, g.y + w.y, g.z + w.z);
        *off = v3(fma(sx, dt, off.x), fma(sy, dt, off.y), fma(sz, dt, off.z));
        *rot = v3(
            fma(rv.x, dt, rot.x),
            fma(rv.y, dt, rot.y),
            fma(rv.z, dt, rot.z),
        );
        *off = v3(wrap_big(off.x), wrap_big(off.y), wrap_big(off.z));
        *rot = v3(wrap_big(rot.x), wrap_big(rot.y), wrap_big(rot.z));
    }
}

/// `cEnvironmentParticles::Setup` random placement: per particle four vertices of position
/// (xyz + 1), UV0 and UV1 (offset scaled by the particle size).
pub fn env_setup(
    box_size: f32,
    particle_size: Vector2f,
    sub_div: Vector2l,
    pos: &mut [f32],
    uv0: &mut [f32],
    uv1: &mut [f32],
) {
    let sx = sub_div.x.max(1);
    let sy = sub_div.y.max(1);
    let cell = Vector2f {
        x: 1.0 / sx as f32,
        y: 1.0 / sy as f32,
    };
    let corners = [(1.0f32, 1.0f32), (0.0, 1.0), (0.0, 0.0), (1.0, 0.0)];
    let offsets = [(0.5f32, -0.5f32), (-0.5, -0.5), (-0.5, 0.5), (0.5, 0.5)];
    let quads = pos
        .chunks_exact_mut(16)
        .zip(uv0.chunks_exact_mut(12))
        .zip(uv1.chunks_exact_mut(12));
    for ((p, u0), u1) in quads {
        let vp = rand_v3(v3(0.0, 0.0, 0.0), v3(box_size, box_size, box_size));
        let cx = rand_rectl(0, sx - 1) as f32 * cell.x;
        let cy = rand_rectl(0, sy - 1) as f32 * cell.y;
        let verts = p
            .chunks_exact_mut(4)
            .zip(u0.chunks_exact_mut(3))
            .zip(u1.chunks_exact_mut(3));
        for (((pv, uv), ov), (c, o)) in verts.zip(corners.iter().zip(offsets.iter())) {
            pv.copy_from_slice(&[vp.x, vp.y, vp.z, 1.0]);
            uv.copy_from_slice(&[fma(c.0, cell.x, cx), fma(c.1, cell.y, cy), 0.0]);
            ov.copy_from_slice(&[o.0 * particle_size.x, o.1 * particle_size.y, 0.0]);
        }
    }
}
