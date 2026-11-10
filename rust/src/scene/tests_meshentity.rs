use super::animstate::{AnimCore, weight_mul};
use super::ffi_meshentity::*;
use super::meshentity::update_active;
use super::skinning::{SkinJob, aabb_from_bones, bone_matrices, skin};
use crate::math::IDENTITY;
use crate::types::{Matrixf, Vector3f};

fn translation(x: f32, y: f32, z: f32) -> Matrixf {
    let mut m = IDENTITY;
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    m
}

fn active(length: f32) -> AnimCore {
    let mut c = AnimCore::new(length);
    c.set_active(true);
    c
}

#[test]
fn update_advances_time_by_speed_and_base_speed() {
    let mut c = active(10.0);
    c.speed = 2.0;
    c.base_speed = 0.5;
    c.update(1.0);
    assert_eq!((c.time_pos, c.prev_time_pos), (1.0, 0.0));
}

#[test]
fn loop_wraps_and_clamp_stops_at_length() {
    let mut c = active(2.0);
    c.update(2.5);
    assert_eq!(c.time_pos, 2.0);
    assert!(c.is_over());
    c.looping = true;
    c.set_time_position(2.5);
    assert_eq!(c.time_pos, 0.5);
    assert!(!c.is_over());
}

#[test]
fn paused_keeps_prev_time_and_position() {
    let mut c = active(5.0);
    c.time_pos = 1.0;
    c.prev_time_pos = 0.25;
    c.paused = true;
    c.update(1.0);
    assert_eq!((c.time_pos, c.prev_time_pos), (1.0, 0.25));
}

#[test]
fn fade_out_deactivates_at_zero_weight() {
    let mut c = active(5.0);
    c.fade_out(2.0);
    assert_eq!(c.fade_step, -0.5);
    c.update(1.0);
    assert_eq!((c.weight, c.active), (0.5, true));
    c.update(1.0);
    assert_eq!((c.weight, c.active, c.fade_step), (0.0, false, 0.0));
}

#[test]
fn fade_in_stops_at_full_weight() {
    let mut c = active(5.0);
    c.weight = 0.0;
    c.fade_in(0.5);
    c.update(1.0);
    assert_eq!((c.weight, c.fade_step), (1.0, 0.0));
}

#[test]
fn fade_speed_reaches_destination() {
    let mut c = active(100.0);
    c.fade_speed(3.0, 2.0);
    assert_eq!(c.speed_fade_rate, 1.0);
    c.update(1.0);
    assert_eq!(c.speed, 2.0);
    c.update(5.0);
    assert_eq!(c.speed, 3.0);
}

#[test]
fn fade_speed_floors_duration() {
    let mut c = AnimCore::new(1.0);
    c.fade_speed(2.0, 0.0);
    assert_eq!(c.speed_fade_rate, 1.0 / 0.0001);
}

#[test]
fn set_active_resets_pause_and_fade_only_on_change() {
    let mut c = AnimCore::new(1.0);
    c.paused = true;
    c.fade_step = 1.0;
    c.set_active(false);
    assert!(c.paused && c.fade_step == 1.0);
    c.set_active(true);
    assert!(!c.paused && c.fade_step == 0.0);
}

#[test]
fn weight_mul_normalizes_over_active_non_layer_states() {
    let mut a = active(1.0);
    a.weight = 0.25;
    let mut b = active(1.0);
    b.weight = 0.25;
    let mut layer = active(1.0);
    layer.layer = true;
    let idle = AnimCore::new(1.0);
    let all = [a, b, layer, idle];
    assert_eq!(weight_mul(all.iter(), true), 2.0);
    assert_eq!(weight_mul(all.iter(), false), 1.0);
    assert_eq!(weight_mul([idle].iter(), true), 1.0);
    a.weight = 0.0;
    assert_eq!(weight_mul([a].iter(), true), 1.0);
}

#[test]
fn update_active_skips_inactive_states() {
    let mut states = [active(5.0), AnimCore::new(5.0)];
    update_active(states.iter_mut(), 1.0);
    assert_eq!((states[0].time_pos, states[1].time_pos), (1.0, 0.0));
}

fn skin_one(
    weights: [f32; 4],
    bones: [u8; 4],
    matrices: &[Matrixf],
) -> ([f32; 3], [f32; 3], [f32; 4]) {
    let (mut p, mut n, mut t) = ([9.0; 3], [9.0; 3], [9.0; 4]);
    skin(SkinJob {
        bind_pos: &[1.0, 2.0, 3.0],
        bind_normal: &[0.0, 1.0, 0.0],
        bind_tangent: &[1.0, 0.0, 0.0, 1.0],
        skin_pos: &mut p,
        skin_normal: &mut n,
        skin_tangent: &mut t,
        weights: &weights,
        bones: &bones,
        matrices,
        pos_stride: 3,
    });
    (p, n, t)
}

#[test]
fn skin_blends_influences_and_leaves_tangent_w() {
    let ms = [translation(10.0, 0.0, 0.0), translation(0.0, 20.0, 0.0)];
    let (p, n, t) = skin_one([0.5, 0.5, 0.0, 0.0], [0, 1, 0, 0], &ms);
    assert_eq!(p, [6.0, 12.0, 3.0]);
    assert_eq!(n, [0.0, 1.0, 0.0]);
    assert_eq!(t, [1.0, 0.0, 0.0, 9.0]);
}

#[test]
fn skin_zero_first_weight_keeps_previous_output() {
    let (p, _, _) = skin_one([0.0, 1.0, 0.0, 0.0], [0, 0, 0, 0], &[IDENTITY]);
    assert_eq!(p, [9.0; 3]);
}

#[test]
fn skin_stops_at_zero_weight_hole() {
    let ms = [translation(10.0, 0.0, 0.0)];
    let (p, _, _) = skin_one([1.0, 0.0, 1.0, 0.0], [0, 0, 0, 0], &ms);
    assert_eq!(p, [11.0, 2.0, 3.0]);
}

#[test]
fn skin_out_of_range_bone_is_skipped() {
    let (p, _, _) = skin_one([0.5, 0.5, 0.0, 0.0], [7, 0, 0, 0], &[IDENTITY]);
    assert_eq!(p, [0.5, 1.0, 1.5]);
}

#[test]
fn skin_stride_four_leaves_padding_untouched() {
    let (mut p, mut n, mut t) = ([9.0; 4], [0.0; 3], [0.0; 4]);
    skin(SkinJob {
        bind_pos: &[1.0, 2.0, 3.0, 1.0],
        bind_normal: &[0.0; 3],
        bind_tangent: &[0.0; 4],
        skin_pos: &mut p,
        skin_normal: &mut n,
        skin_tangent: &mut t,
        weights: &[1.0, 0.0, 0.0, 0.0],
        bones: &[0; 4],
        matrices: &[IDENTITY],
        pos_stride: 4,
    });
    assert_eq!(p, [1.0, 2.0, 3.0, 9.0]);
}

#[test]
fn bone_matrices_count_only_bitwise_changes() {
    let (world, bind) = (translation(1.0, 2.0, 3.0), translation(-1.0, 0.0, 0.0));
    let mut out = [Matrixf::default(); 2];
    let args = (&IDENTITY, &[&world, &world][..], &[&bind, &IDENTITY][..]);
    assert_eq!(bone_matrices(args.0, args.1, args.2, &mut out), 2);
    assert_eq!(out[0], translation(0.0, 2.0, 3.0));
    assert_eq!(out[1], world);
    assert_eq!(bone_matrices(args.0, args.1, args.2, &mut out), 0);
}

#[test]
fn bone_matrices_distinguish_negative_zero() {
    let mut neg = IDENTITY;
    neg.m[0][1] = -0.0;
    let mut out = [neg];
    assert_eq!(neg, IDENTITY);
    assert_eq!(
        bone_matrices(&IDENTITY, &[&IDENTITY], &[&IDENTITY], &mut out),
        1
    );
}

#[test]
fn aabb_ignores_unweighted_bones_and_falls_back_to_first() {
    let (a, b) = (translation(0.0, 0.0, 0.0), translation(5.0, 0.0, 0.0));
    let (min, max) = aabb_from_bones(&[&a, &b], &[1.0, 0.0]);
    assert_eq!(
        (min, max),
        (
            Vector3f {
                x: -1.0,
                y: -1.0,
                z: -1.0
            },
            Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0
            }
        )
    );
    let (min, max) = aabb_from_bones(&[&b, &a], &[0.0, -1.0]);
    assert_eq!(
        (min, max),
        (
            Vector3f {
                x: 5.0,
                y: 0.0,
                z: 0.0
            },
            Vector3f {
                x: 5.0,
                y: 0.0,
                z: 0.0
            }
        )
    );
}

#[test]
fn ffi_tolerates_null_and_empty_inputs() {
    // SAFETY: null core pointers are accepted and ignored; empty arrays are never read.
    unsafe {
        ohpl_anim_update(std::ptr::null_mut(), 1.0);
        assert!(!ohpl_anim_is_over(std::ptr::null()));
        assert_eq!(ohpl_meshentity_weight_mul(std::ptr::null(), 0, true), 1.0);
        ohpl_meshentity_update_anims(std::ptr::null(), 0, 1.0);
        let none = std::ptr::null();
        assert_eq!(
            ohpl_meshentity_bone_matrices(&IDENTITY, none, none, std::ptr::null_mut(), 0),
            0
        );
    }
}

#[test]
fn ffi_anim_core_roundtrip() {
    let mut c = AnimCore::new(1.0);
    ohpl_anim_core_init(&mut c, 4.0);
    assert_eq!(c, AnimCore::new(4.0));
    // SAFETY: `c` is a valid exclusive core.
    unsafe {
        ohpl_anim_set_active(&mut c, true);
        ohpl_anim_set_relative_time(&mut c, 0.5);
        assert_eq!(ohpl_anim_relative_time(&c), 0.5);
        ohpl_anim_add_time(&mut c, 2.0);
        assert!(ohpl_anim_is_over(&c));
    }
}
