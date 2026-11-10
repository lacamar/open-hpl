use super::camera::*;
use super::node::*;
use crate::math::IDENTITY;
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

fn translation(x: f32, y: f32, z: f32) -> Matrixf {
    let mut m = IDENTITY;
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    m
}

const IDENTITY_Q: Quaternion = Quaternion {
    v: Vector3f {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    },
    w: 1.0,
};

#[test]
fn world_to_local_undoes_parent_translation() {
    let local = world_to_local(&translation(1.0, 2.0, 3.0), &translation(5.0, 5.0, 5.0));
    assert_eq!(local, translation(4.0, 3.0, 2.0));
}

#[test]
fn update_matrix_identity_rotation_adds_translation() {
    let m = update_matrix(
        &translation(1.0, 2.0, 3.0),
        &IDENTITY_Q,
        v3(10.0, 20.0, 30.0),
    );
    assert_eq!(m, translation(11.0, 22.0, 33.0));
}

#[test]
fn add_rotation_zero_euler_keeps_current() {
    let q = add_rotation(v3(0.0, 0.0, 0.0), 0, &IDENTITY_Q);
    assert_eq!(q, IDENTITY_Q);
}

#[test]
fn scaled_world_and_calc_scale_roundtrip() {
    let m = scaled_world(&translation(1.0, 0.0, 0.0), v3(2.0, 3.0, 4.0));
    assert_eq!(calc_scale(&m), v3(2.0, 3.0, 4.0));
}

#[test]
fn inverse_of_translation() {
    assert_eq!(
        inverse(&translation(1.0, -2.0, 3.0)),
        translation(-1.0, 2.0, -3.0)
    );
}

fn view_params(mode: i32) -> ViewParams {
    ViewParams {
        rotate_mode: mode,
        position: v3(1.0, 2.0, 3.0),
        yaw: 0.0,
        pitch: 0.0,
        roll: 0.0,
        rotation: IDENTITY,
    }
}

#[test]
fn view_matrix_modes() {
    let t = translation(-1.0, -2.0, -3.0);
    assert_eq!(view_matrix(&view_params(ROTATE_EULER)), t);
    assert_eq!(view_matrix(&view_params(ROTATE_MATRIX)), t);
    assert_eq!(view_matrix(&view_params(2)), IDENTITY);
}

#[test]
fn move_matrix_only_in_euler_mode() {
    assert_eq!(move_matrix(ROTATE_MATRIX, MOVE_FLY, 1.0, 1.0), None);
    assert_eq!(
        move_matrix(ROTATE_EULER, MOVE_FLY, 0.0, 0.0),
        Some(IDENTITY)
    );
}

fn params(kind: i32) -> ProjectionParams {
    ProjectionParams {
        kind,
        near: 0.5,
        far: 100.0,
        fov: 1.2,
        aspect: 1.5,
        inf_far: false,
        view_size: Vector2f { x: 4.0, y: 2.0 },
    }
}

#[test]
fn orthographic_projection_scales_by_view_size() {
    let m = projection_matrix(&params(PROJECTION_ORTHOGRAPHIC));
    assert_eq!((m.m[0][0], m.m[1][1], m.m[3][3]), (0.5, 1.0, 1.0));
}

fn basis() -> CameraBasis {
    CameraBasis {
        rotation: IDENTITY,
        position: v3(1.0, 2.0, 3.0),
        forward: v3(0.0, 0.0, -1.0),
        up: v3(0.0, 1.0, 0.0),
        right: v3(1.0, 0.0, 0.0),
    }
}

#[test]
fn unproject_screen_centre() {
    let half = Vector2f { x: 50.0, y: 50.0 };
    let size = Vector2f { x: 100.0, y: 100.0 };
    let (p, d) = unproject(&params(PROJECTION_PERSPECTIVE), half, size, &basis()).unwrap();
    assert_eq!((p, d), (v3(1.0, 2.0, 3.0), v3(0.0, 0.0, -1.0)));
    let (p, d) = unproject(&params(PROJECTION_ORTHOGRAPHIC), half, size, &basis()).unwrap();
    assert_eq!((p, d), (v3(1.0, 2.0, 2.5), v3(0.0, 0.0, -1.0)));
    assert!(unproject(&params(7), half, size, &basis()).is_none());
}

#[test]
fn unproject_ortho_corner_offsets_by_view_size() {
    let size = Vector2f { x: 100.0, y: 100.0 };
    let (p, _) = unproject(
        &params(PROJECTION_ORTHOGRAPHIC),
        Vector2f { x: 100.0, y: 0.0 },
        size,
        &basis(),
    )
    .unwrap();
    assert_eq!((p.x, p.y), (1.0 + 0.5 * 4.0, 2.0 + 0.5 * 2.0));
}
