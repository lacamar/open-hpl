//! Replay of `rust/tests/golden/H12/ps_emitters.txt`, recorded from the C++ loader and emitter
//! kernel by `HPL2/tests/SceneH12Tests.cpp`: attribute strings in, parsed fields and emitter frames out.

use super::h12_psload::{PsDesc, frame_step, max_frame_time, parse};
use super::particles::Emitter;
use crate::math::{IDENTITY, c_srand};
use crate::types::{Color, Vector2f, Vector3f};
use std::collections::HashMap;
use std::ffi::{CString, c_char, c_void};
use std::fmt::Write;

const GOLDEN: &str = include_str!("../../tests/golden/H12/ps_emitters.txt");

type Attrs = HashMap<Vec<u8>, CString>;

unsafe extern "C" fn getter(ctx: *mut c_void, name: *const c_char) -> *const c_char {
    // SAFETY: `ctx` is the `Attrs` under test, `name` a NUL-terminated literal.
    let (m, n) = unsafe { (&*ctx.cast::<Attrs>(), std::ffi::CStr::from_ptr(name)) };
    m.get(n.to_bytes()).map_or(std::ptr::null(), |v| v.as_ptr())
}

struct Dump(String);

impl Dump {
    fn f(&mut self, v: f32) {
        let _ = write!(self.0, " {:08x}", v.to_bits());
    }
    fn i(&mut self, v: i32) {
        let _ = write!(self.0, " {v}");
    }
    fn b(&mut self, v: bool) {
        self.i(i32::from(v));
    }
    fn v2(&mut self, v: Vector2f) {
        self.f(v.x);
        self.f(v.y);
    }
    fn v3(&mut self, v: Vector3f) {
        self.f(v.x);
        self.f(v.y);
        self.f(v.z);
    }
    fn c(&mut self, c: Color) {
        self.f(c.r);
        self.f(c.g);
        self.f(c.b);
        self.f(c.a);
    }
}

fn cstr(p: *const c_char) -> String {
    // SAFETY: `PsDesc` strings are NUL-terminated and live as long as the box.
    unsafe { std::ffi::CStr::from_ptr(p) }
        .to_string_lossy()
        .into_owned()
}

fn dump_line(d: &PsDesc) -> String {
    let p = &d.params;
    let mut o = Dump(format!("d {}", cstr(d.name)));
    o.i(p.pe_type);
    o.i(d.max_particle_num);
    o.b(d.respawn);
    o.f(p.particles_per_second);
    o.f(p.start_time_offset);
    o.f(d.warm_up_time);
    o.f(d.warm_up_steps_per_sec);
    o.f(p.min_pause_length);
    o.f(p.max_pause_length);
    o.f(p.min_pause_interval);
    o.f(p.max_pause_interval);
    o.v3(d.pos_offset);
    o.v3(d.angle_offset);
    o.i(d.sub_div.x);
    o.i(d.sub_div.y);
    o.i(p.sub_div_type);
    o.f(frame_step(d.animation_length, 0));
    o.f(max_frame_time(0));
    o.i(p.start_pos_type);
    o.v3(p.min_start_pos);
    o.v3(p.max_start_pos);
    o.v2(p.min_start_angles);
    o.v2(p.max_start_angles);
    o.f(p.min_start_radius);
    o.f(p.max_start_radius);
    o.i(p.start_vel_type);
    o.v3(p.min_start_vel);
    o.v3(p.max_start_vel);
    o.v2(p.min_start_vel_angles);
    o.v2(p.max_start_vel_angles);
    o.f(p.min_start_vel_speed);
    o.f(p.max_start_vel_speed);
    o.f(p.min_speed_multiply);
    o.f(p.max_speed_multiply);
    o.v3(p.min_start_acc);
    o.v3(p.max_start_acc);
    o.f(p.min_vel_maximum);
    o.f(p.max_vel_maximum);
    o.b(p.uses_direction);
    o.i(p.gravity_type);
    o.v3(p.gravity_acc);
    o.i(p.coord_system);
    o.b(p.use_part_spin);
    o.i(p.part_spin_type);
    o.f(p.min_spin_range);
    o.f(p.max_spin_range);
    o.b(p.use_revolution);
    o.v3(p.min_rev_vel);
    o.v3(p.max_rev_vel);
    o.f(p.min_life_span);
    o.f(p.max_life_span);
    o.i(0);
    let _ = write!(o.0, " {}", cstr(d.death_ps));
    o.i(p.draw_type);
    o.v2(p.min_start_size);
    o.v2(p.max_start_size);
    o.f(p.start_rel_size);
    o.f(p.middle_rel_size);
    o.f(p.middle_rel_size_time);
    o.f(p.middle_rel_size_length);
    o.f(p.end_rel_size);
    o.b(p.multiply_rgb_with_alpha);
    o.c(p.min_start_color);
    o.c(p.max_start_color);
    o.c(p.start_rel_color);
    o.c(p.middle_rel_color);
    o.f(p.middle_rel_color_time);
    o.f(p.middle_rel_color_length);
    o.c(p.end_rel_color);
    o.b(p.collides);
    o.f(p.min_bounce_amount);
    o.f(p.max_bounce_amount);
    o.i(p.min_collision_max);
    o.i(p.max_collision_max);
    o.i(p.collision_update_rate);
    o.b(d.use_beam_noise);
    o.i(d.low_freq_points);
    o.i(d.high_freq_points);
    o.v3(d.min_low_freq_noise);
    o.v3(d.max_low_freq_noise);
    o.v3(d.min_high_freq_noise);
    o.v3(d.max_high_freq_noise);
    o.0
}

fn sim_lines(d: &PsDesc, seed: u32, frames: usize) -> Vec<String> {
    let mut p = d.params;
    p.frame_step = frame_step(d.animation_length, 0);
    p.max_frame_time = max_frame_time(0);
    c_srand(seed);
    let mut e = Emitter::new(p, d.max_particle_num as u32, d.respawn, d.sub_div, 1);
    let mut out = vec![format!("sim {seed}")];
    for _ in 0..frames {
        e.update_motion(1.0 / 30.0, &IDENTITY, &mut |_, _| None);
        let n = e.num_particles();
        out.push(format!("f {n}"));
        for i in 0..n.min(24) {
            let Some(q) = e.particle(i as usize) else {
                continue;
            };
            let mut o = Dump(format!("p {i}"));
            o.v3(q.pos);
            o.c(q.color);
            o.v2(q.size);
            out.push(o.0);
        }
    }
    out
}

struct Block<'a> {
    attrs: Attrs,
    dump: &'a str,
    sim: Vec<&'a str>,
}

fn blocks() -> Vec<Block<'static>> {
    let mut v: Vec<Block> = Vec::new();
    for line in GOLDEN.lines() {
        if line.starts_with("emitter ") {
            v.push(Block {
                attrs: Attrs::new(),
                dump: "",
                sim: Vec::new(),
            });
        }
        let Some(b) = v.last_mut() else { continue };
        if let Some(a) = line.strip_prefix("a ") {
            let (k, val) = a.split_once('\t').unwrap_or((a, ""));
            b.attrs.insert(
                k.as_bytes().to_vec(),
                CString::new(val).expect("no NUL in attribute"),
            );
        } else if line.starts_with("d ") {
            b.dump = line;
        } else if line.starts_with("sim ") || !b.sim.is_empty() {
            b.sim.push(line);
        }
    }
    v
}

#[test]
fn golden_emitters_parse_and_simulate() {
    let all = blocks();
    assert!(all.len() > 100);
    let mut sims = 0;
    for (n, b) in all.iter().enumerate() {
        let mut attrs = b.attrs.clone();
        let boxed = parse(getter, std::ptr::from_mut(&mut attrs).cast());
        assert_eq!(dump_line(&boxed.desc), b.dump, "emitter block {n}");
        if let Some(first) = b.sim.first() {
            let seed: u32 = first["sim ".len()..].parse().expect("seed");
            let got = sim_lines(&boxed.desc, seed, 24);
            assert_eq!(got.len(), b.sim.len(), "sim length, block {n}");
            for (i, (g, w)) in got.iter().zip(&b.sim).enumerate() {
                assert_eq!(g, w, "sim block {n} line {i}");
            }
            sims += 1;
        }
    }
    assert!(sims >= 20);
}

#[test]
fn missing_and_malformed_attributes_take_defaults() {
    let mut a = Attrs::new();
    a.insert(b"MinStartPos".to_vec(), CString::new("1 2").expect("s"));
    a.insert(b"SubDiv".to_vec(), CString::new("2").expect("s"));
    let b = parse(getter, std::ptr::from_mut(&mut a).cast());
    let d = &b.desc;
    assert_eq!(d.params.min_start_pos, Vector3f::default());
    assert_eq!((d.sub_div.x, d.sub_div.y), (1, 1));
    assert!(d.has_sub_div);
    assert_eq!(
        (d.max_particle_num, d.low_freq_points, d.high_freq_points),
        (1, 4, 5)
    );
    assert_eq!(d.warm_up_steps_per_sec, 60.0);
}

#[test]
fn frame_step_without_materials_is_zero() {
    assert_eq!(frame_step(1.0, 0), 0.0);
    assert_eq!(frame_step(2.0, 4), 2.0);
    assert_eq!(max_frame_time(3), 3.0 - 0.0001);
}
