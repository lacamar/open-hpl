//! `iCharacterBody` construction defaults and `ConnectToBody` / `DisconnectBody` (`OHPL_IMPL=rust`).
//! The extension state is a repr(C) struct embedded in the C++ object; its members are references into it.

use super::body::rotation;
use super::character::{CharState, S, update_move_matrix};
use super::host::H;
use super::wreg::{Wa, WorldOp};
use crate::math::{
    IDENTITY, len3, matrix_inverse, matrix_mul_3x3, matrix_mul_vec, matrix_to_euler_angles,
    normalize3, v3,
};
use crate::types::{Matrixf, Vector3f};

pub const CO_BODY_ADD_CONNECTED: i32 = 501;
pub const CO_BODY_LOCAL_MATRIX: i32 = 502;
pub const CO_BODY_WORLD_POS: i32 = 503;
pub const CO_BODY_GRAVITY: i32 = 504;
pub const CO_BODY_SET_GRAVITY: i32 = 505;
pub const CO_BODY_IS_ACTIVE: i32 = 506;
pub const CO_BODY_REMOVE_CONNECTED: i32 = 507;

const FLAG_DEPEND_ON_BODY_ROTATION: u32 = 0x1;
const FLAG_ALIGN_BODY_CENTER: u32 = 0x2;
const FLAG_ALIGN_BODY_ROTATION: u32 = 0x4;
const FLAG_DISABLE_BODY_GRAVITY: u32 = 0x8;
const FLAG_AFFECT_CHAR: u32 = 0x10;
const FLAG_ALIGN_CHAR_ROTATION: u32 = 0x20;
const FLAG_DEPEND_ON_CHAR_ROTATION: u32 = 0x40;
const FLAG_ALLOW_COLLISION: u32 = 0x80;

/// Scalars, pointers and connection data of an `iCharacterBody` beside `OhplCharState`;
/// layout mirrored by `OhplCharExt` in `CharacterBody.h`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CharExt {
    pub world: H,
    pub callback: H,
    pub user_data: H,
    pub camera: H,
    pub entity: H,
    pub connected_body: H,
    pub gravity_attached_body: H,
    pub gravity_last_attached_body: H,
    pub gravity_collide_material: H,
    pub camera_pos_add: Vector3f,
    pub conn_pos_local_to_body: Vector3f,
    pub conn_pos_local_to_char: Vector3f,
    pub conn_normal_local_to_body: Vector3f,
    pub conn_local_up_body: Vector3f,
    pub conn_body_start_rotation: Vector3f,
    pub conn_char_local_forward: Vector3f,
    pub gravity_attach_body_velocity: Vector3f,
    pub entity_offset: Matrixf,
    pub entity_post_offset: Matrixf,
    pub gravity_attached_prev_matrix: Matrixf,
    pub max_push_mass: f32,
    pub push_force: f32,
    pub push_impulse: f32,
    pub char_max_push_mass: f32,
    pub char_push_force: f32,
    pub constant_contact_force_mul: f32,
    pub velocity_contact_force_mul: f32,
    pub max_contact_force_per_mass_unit: f32,
    pub entity_pitch_amount: f32,
    pub max_connection_torque: f32,
    pub max_connection_force: f32,
    pub conn_distance_to_body: f32,
    pub min_body_push_strength: i32,
    pub camera_smooth_pos_num: i32,
    pub entity_smooth_pos_num: i32,
    pub entity_smooth_y_pos_num: i32,
    pub gravity_attachment_contacts: i32,
    pub collide_character: bool,
    pub physics_body_active: bool,
    pub stick_to_slope: bool,
    pub push_in_2d: bool,
    pub char_push_in_2d: bool,
    pub camera_use_smoothing: bool,
    pub camera_update_active: bool,
    pub entity_smooth_y_pos: bool,
    pub connected_body_had_gravity: bool,
    pub conn_depend_on_body_rotation: bool,
    pub conn_align_center: bool,
    pub conn_align_rotation: bool,
    pub conn_collision: bool,
    pub connected_body_is_active: bool,
    pub conn_affect_char: bool,
    pub conn_depend_on_char_rotation: bool,
    pub conn_align_char_rotation: bool,
    pub rotate_yaw_when_gravity_attached: bool,
}

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);

impl CharExt {
    fn initial(world: H) -> Self {
        Self {
            world,
            callback: 0,
            user_data: 0,
            camera: 0,
            entity: 0,
            connected_body: 0,
            gravity_attached_body: 0,
            gravity_last_attached_body: 0,
            gravity_collide_material: 0,
            camera_pos_add: ZERO,
            conn_pos_local_to_body: ZERO,
            conn_pos_local_to_char: ZERO,
            conn_normal_local_to_body: ZERO,
            conn_local_up_body: ZERO,
            conn_body_start_rotation: ZERO,
            conn_char_local_forward: ZERO,
            gravity_attach_body_velocity: ZERO,
            entity_offset: IDENTITY,
            entity_post_offset: IDENTITY,
            gravity_attached_prev_matrix: IDENTITY,
            max_push_mass: 0.0,
            push_force: 0.0,
            push_impulse: 0.0,
            char_max_push_mass: 0.0,
            char_push_force: 0.0,
            constant_contact_force_mul: 16.0,
            velocity_contact_force_mul: 4.0,
            max_contact_force_per_mass_unit: 250.0,
            entity_pitch_amount: 1.0,
            max_connection_torque: 0.0,
            max_connection_force: 0.0,
            conn_distance_to_body: 0.0,
            min_body_push_strength: 0,
            camera_smooth_pos_num: 0,
            entity_smooth_pos_num: 0,
            entity_smooth_y_pos_num: 20,
            gravity_attachment_contacts: 0,
            collide_character: true,
            physics_body_active: true,
            stick_to_slope: true,
            push_in_2d: true,
            char_push_in_2d: true,
            camera_use_smoothing: true,
            camera_update_active: true,
            entity_smooth_y_pos: false,
            connected_body_had_gravity: false,
            conn_depend_on_body_rotation: false,
            conn_align_center: false,
            conn_align_rotation: false,
            conn_collision: false,
            connected_body_is_active: false,
            conn_affect_char: false,
            conn_depend_on_char_rotation: false,
            conn_align_char_rotation: false,
            rotate_yaw_when_gravity_attached: true,
        }
    }
}

impl CharState {
    /// The values `iCharacterBody::iCharacterBody` gives a fresh character of `size`.
    fn initial(size: Vector3f, max_no_slide_slope_cos: f32) -> Self {
        let step = size.y * 0.2f32;
        Self {
            position: ZERO,
            last_position: ZERO,
            last_move_pos_add: ZERO,
            force: ZERO,
            velocity: ZERO,
            last_ground_normal: ZERO,
            gravity_attachment_velocity: ZERO,
            custom_gravity: v3(0.0, 9.8, 0.0),
            size,
            forward: ZERO,
            right: ZERO,
            up: ZERO,
            mtx_move: IDENTITY,
            pitch: 0.0,
            yaw: 0.0,
            mass: 1.0,
            max_gravity_speed: 30.0,
            ground_friction: 6.0,
            air_friction: 0.3,
            max_no_slide_slope_cos,
            ground_angle_min_cos: 0.001,
            max_step_height: step,
            max_step_height_in_air: step,
            step_climb_speed: 1.0,
            climb_forward_mul: 1.0,
            climb_height_add: 0.01,
            check_step_climb_count: 0.0,
            check_step_climb_interval: 1.0 / 20.0f32,
            move_delay_count: 0.0,
            max_pos_move_speed: [10.0; 2],
            max_neg_move_speed: [-10.0; 2],
            current_move_acc: [0.0; 2],
            move_acc: [20.0; 2],
            move_deacc: [20.0; 2],
            move_speed: [0.0; 2],
            move_speed_mul: [1.0; 2],
            move_opposite_dir_acc_mul: [1.0; 2],
            on_ground_count: 0,
            max_on_ground_count: 12,
            current_shape_idx: 0,
            collide_flags: u32::MAX,
            moving: [false; 2],
            active: true,
            test_collision: true,
            gravity_active: true,
            custom_gravity_active: false,
            deaccelerate_move_speed_in_air: false,
            moved_last_update: false,
            disable_diag_speed_boost: true,
            climbing: false,
            accurate_climbing: false,
        }
    }
}

/// Sets every default; `st.size` and the shapes are already in place on the C++ side.
///
/// # Safety
/// `ext` and `st` are writable and embedded in the character.
pub unsafe fn char_init(
    ext: *mut CharExt,
    st: *mut CharState,
    world: H,
    max_no_slide_slope_cos: f32,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(e), Some(s)) = (ext.as_mut(), st.as_mut()) {
            *e = CharExt::initial(world);
            *s = CharState::initial(s.size, max_no_slide_slope_cos);
        }
    }
}

fn call(op: WorldOp, code: i32, p: [usize; 6], i0: i32) -> Wa {
    let mut a = Wa {
        p,
        f: [0.0; 2],
        i: [i0, 0],
        s: std::ptr::null(),
        n: 0,
        rf: 0.0,
        rv: [0.0; 3],
    };
    // SAFETY: the op belongs to the live character and `a` is a live block.
    a.p[5] = unsafe { op(code, &mut a) };
    a
}

fn local_matrix(op: WorldOp, body: H) -> Matrixf {
    let mut m = IDENTITY;
    call(
        op,
        CO_BODY_LOCAL_MATRIX,
        [body, &mut m as *mut Matrixf as usize, 0, 0, 0, 0],
        0,
    );
    m
}

fn world_pos(op: WorldOp, body: H) -> Vector3f {
    let a = call(op, CO_BODY_WORLD_POS, [body, 0, 0, 0, 0, 0], 0);
    v3(a.rv[0], a.rv[1], a.rv[2])
}

fn normalized(mut v: Vector3f) -> Vector3f {
    normalize3(&mut v);
    v
}

fn sub(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

/// `iCharacterBody::ConnectToBody`. The states are re-read through raw pointers around every host
/// call, which can re-enter C++.
///
/// # Safety
/// `ext` and `st` are the live embedded states of character `me`.
pub unsafe fn connect_to_body(
    op: WorldOp,
    me: H,
    ext: *mut CharExt,
    st: *mut CharState,
    body: H,
    pos: Vector3f,
    flags: u32,
) {
    if body == 0 {
        return;
    }
    // SAFETY: caller contract for every deref below.
    let e = || unsafe { ext.as_mut() };
    // SAFETY: as above.
    let s = || unsafe { st.as_mut() };
    let Some(x) = e() else { return };
    x.connected_body = body;
    call(op, CO_BODY_ADD_CONNECTED, [body, me, 0, 0, 0, 0], 0);

    let depend_on_body_rotation = flags & FLAG_DEPEND_ON_BODY_ROTATION != 0;
    let body_matrix = local_matrix(op, body);
    let inv_body = matrix_inverse(&body_matrix);

    let body_world = world_pos(op, body);
    let pos_local_to_body = if depend_on_body_rotation {
        matrix_mul_vec(&inv_body, pos)
    } else {
        sub(pos, body_world)
    };
    let start_rotation = matrix_to_euler_angles(&rotation(&body_matrix));

    let align_rotation = flags & FLAG_ALIGN_BODY_ROTATION != 0;
    let align_center = flags & FLAG_ALIGN_BODY_CENTER != 0;
    let mut normal = normalized(sub(body_world, pos));
    let distance = len3(pos_local_to_body);

    let Some(sv) = s() else { return };
    let mut up_local = matrix_mul_3x3(&inv_body, sv.up);
    normalize3(&mut up_local);

    if flags & FLAG_DISABLE_BODY_GRAVITY != 0 {
        let had = call(op, CO_BODY_GRAVITY, [body, 0, 0, 0, 0, 0], 0).p[5] != 0;
        if let Some(x) = e() {
            x.connected_body_had_gravity = had;
        }
        if had {
            call(op, CO_BODY_SET_GRAVITY, [body, 0, 0, 0, 0, 0], 0);
        }
    }

    let affect_char = flags & FLAG_AFFECT_CHAR != 0;
    let Some(sv) = s() else { return };
    let mut pos_local_to_char = sub(pos, sv.position);

    let align_char = flags & FLAG_ALIGN_CHAR_ROTATION != 0;
    let mut char_local_forward = ZERO;
    if align_char {
        char_local_forward = matrix_mul_3x3(&inv_body, sv.forward);
        let inv_char_rot = matrix_inverse(&rotation(&sv.mtx_move));
        pos_local_to_char = matrix_mul_vec(&inv_char_rot, pos_local_to_char);
    }

    let depend_on_char = !align_char && flags & FLAG_DEPEND_ON_CHAR_ROTATION != 0;
    if depend_on_char {
        // SAFETY: caller contract.
        update_move_matrix(&unsafe { S::new(st) });
        let Some(sv) = s() else { return };
        let inv_char_rot = matrix_inverse(&rotation(&sv.mtx_move));
        pos_local_to_char = matrix_mul_vec(&inv_char_rot, pos_local_to_char);
        normal = matrix_mul_vec(&inv_char_rot, normal);
    }

    let is_active = call(op, CO_BODY_IS_ACTIVE, [body, 0, 0, 0, 0, 0], 0).p[5] != 0;
    if let Some(x) = e() {
        x.conn_depend_on_body_rotation = depend_on_body_rotation;
        x.conn_pos_local_to_body = pos_local_to_body;
        x.conn_body_start_rotation = start_rotation;
        x.conn_align_rotation = align_rotation;
        x.conn_align_center = align_center;
        x.conn_normal_local_to_body = normal;
        x.conn_distance_to_body = distance;
        x.conn_local_up_body = up_local;
        x.conn_affect_char = affect_char;
        x.conn_pos_local_to_char = pos_local_to_char;
        x.conn_align_char_rotation = align_char;
        x.conn_char_local_forward = char_local_forward;
        x.conn_depend_on_char_rotation = depend_on_char;
        x.conn_collision = flags & FLAG_ALLOW_COLLISION != 0;
        x.connected_body_is_active = is_active;
    }
}

/// `iCharacterBody::DisconnectBody`.
///
/// # Safety
/// `ext` is the live embedded state of character `me`.
pub unsafe fn disconnect_body(op: WorldOp, me: H, ext: *mut CharExt) {
    // SAFETY: caller contract.
    let Some(x) = (unsafe { ext.as_mut() }) else {
        return;
    };
    let body = x.connected_body;
    if body == 0 {
        return;
    }
    let had = x.connected_body_had_gravity;
    call(op, CO_BODY_REMOVE_CONNECTED, [body, me, 0, 0, 0, 0], 0);
    if had {
        call(op, CO_BODY_SET_GRAVITY, [body, 0, 0, 0, 0, 0], 1);
    }
    // SAFETY: the host calls above never move the embedded state.
    if let Some(x) = unsafe { ext.as_mut() } {
        x.connected_body = 0;
    }
}
