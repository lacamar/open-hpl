//! `cCamera` members defined in Rust under their Itanium symbols (aarch64: `this` in x0, results
//! of non-trivially-copyable types through x8). Layout: `OhplH10Cam` at 0, `cNode3D mNode` at 472
//! (its node pod pointer at +40), `cFrustum mFrustum` at 520; asserted in Camera.h.

use super::h10::{CamState, Host, NodeBox, node_set_position};
use super::scx::scx;
use crate::math::{IDENTITY, bv::Frustum};
use crate::types::{Matrixf, Vector2f, Vector3f};
use std::arch::global_asm;

const NODE_OFFSET: usize = 472;
const NODE_POD: usize = NODE_OFFSET + 40;
const FRUSTUM_OFFSET: usize = 520;

unsafe extern "C" {
    #[link_name = "_ZN3hpl14OhplH10GetHostEv"]
    fn host() -> *const Host;
    #[link_name = "_ZN3hpl7cNode3D9AddEntityEPNS_9iEntity3DE"]
    fn node_add_entity(node: *mut u8, e: *mut u8) -> bool;
    #[link_name = "_ZN3hpl7cNode3D12RemoveEntityEPNS_9iEntity3DE"]
    fn node_remove_entity(node: *mut u8, e: *mut u8) -> bool;
}

struct Cam<'a> {
    s: &'a mut CamState,
    this: *const u8,
}

impl Cam<'_> {
    unsafe fn sync_node(&self) {
        // SAFETY: the node pod pointer is set by the cNode3D constructor.
        unsafe {
            let n = *self.this.add(NODE_POD).cast::<*mut NodeBox>();
            node_set_position(&*host(), n, self.s.position);
        }
    }

    fn mv(&mut self, axis: i32, d: f32) {
        self.s.move_along(axis, d);
        // SAFETY: `this` is a live cCamera.
        unsafe { self.sync_node() }
    }
}

/// # Safety
/// `this` is a live `cCamera`.
unsafe fn cam<'a>(this: *const u8) -> Cam<'a> {
    // SAFETY: caller contract; mS is the first member.
    Cam {
        s: unsafe { &mut *(this as *mut CamState) },
        this,
    }
}

/// # Safety
/// `s` points to storage for a `CamState`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_cam_init(s: *mut CamState) {
    let aspect = 4.0f32 / 3.0f32;
    let ident = IDENTITY;
    // SAFETY: caller contract.
    unsafe {
        s.write(CamState {
            position: Vector3f::default(),
            fov: crate::math::to_rad(70.0),
            aspect,
            far: 1000.0,
            near: 0.05,
            view_size: Vector2f {
                x: 2.0 * aspect,
                y: 2.0,
            },
            projection_type: 0,
            pitch: 0.0,
            yaw: 0.0,
            roll: 0.0,
            pitch_min: -std::f32::consts::FRAC_PI_2,
            pitch_max: std::f32::consts::FRAC_PI_2,
            yaw_min: 0.0,
            yaw_max: 0.0,
            rotate_mode: 0,
            move_mode: 1,
            view: ident,
            projection: ident,
            mv: ident,
            prev_view: ident,
            prev_projection: ident,
            rotation: ident,
            inf_far: false,
            view_updated: true,
            projection_updated: true,
            move_updated: true,
            frustum_updated: true,
        });
    }
}

scx! { cam;
    "_ZN3hpl7cCamera11SetPositionERKNS_8cVector3IfEE" set_position[this, c](p: *const Vector3f) => { c.s.set_position(*p); c.sync_node() };
    "_ZN3hpl7cCamera8SetPitchEf" set_pitch[this, c](a: f32) => c.s.set_pitch(a);
    "_ZN3hpl7cCamera6SetYawEf" set_yaw[this, c](a: f32) => c.s.set_yaw(a);
    "_ZN3hpl7cCamera7SetRollEf" set_roll[this, c](a: f32) => c.s.set_roll(a);
    "_ZN3hpl7cCamera8AddPitchEf" add_pitch[this, c](a: f32) => c.s.add_pitch(a);
    "_ZN3hpl7cCamera6AddYawEf" add_yaw[this, c](a: f32) => c.s.add_yaw(a);
    "_ZN3hpl7cCamera7AddRollEf" add_roll[this, c](a: f32) => c.s.add_roll(a);
    "_ZN3hpl7cCamera11MoveForwardEf" move_forward[this, c](d: f32) => c.mv(0, d);
    "_ZN3hpl7cCamera9MoveRightEf" move_right[this, c](d: f32) => c.mv(1, d);
    "_ZN3hpl7cCamera6MoveUpEf" move_up[this, c](d: f32) => c.mv(2, d);
    "_ZN3hpl7cCamera6SetFOVEf" set_fov[this, c](a: f32) => c.s.set_fov(a);
    "_ZN3hpl7cCamera9SetAspectEf" set_aspect[this, c](a: f32) => c.s.set_aspect(a);
    "_ZN3hpl7cCamera15SetFarClipPlaneEf" set_far[this, c](a: f32) => c.s.set_far(a);
    "_ZN3hpl7cCamera16SetNearClipPlaneEf" set_near[this, c](a: f32) => c.s.set_near(a);
    "_ZN3hpl7cCamera17SetProjectionTypeENS_15eProjectionTypeE" set_projection_type[this, c](t: i32) => c.s.set_projection_type(t);
    "_ZN3hpl7cCamera16SetOrthoViewSizeERKNS_8cVector2IfEE" set_ortho[this, c](v: *const Vector2f) => c.s.set_ortho_view_size(*v);
    "_ZN3hpl7cCamera13SetRotateModeENS_17eCameraRotateModeE" set_rotate_mode[this, c](m: i32) => c.s.set_rotate_mode(m);
    "_ZN3hpl7cCamera11SetMoveModeENS_15eCameraMoveModeE" set_move_mode[this, c](m: i32) => c.s.set_move_mode(m);
    "_ZN3hpl7cCamera13ResetRotationEv" reset_rotation[this, c]() => c.s.reset_rotation();
    "_ZN3hpl7cCamera10SetForwardERKNS_8cVector3IfEE" set_forward[this, c](v: *const Vector3f) => c.s.set_rotation_row(2, *v);
    "_ZN3hpl7cCamera8SetRightERKNS_8cVector3IfEE" set_right[this, c](v: *const Vector3f) => c.s.set_rotation_row(0, *v);
    "_ZN3hpl7cCamera5SetUpERKNS_8cVector3IfEE" set_up[this, c](v: *const Vector3f) => c.s.set_rotation_row(1, *v);
    "_ZN3hpl7cCamera17SetRotationMatrixERKNS_7cMatrixIfEE" set_rotation_matrix[this, c](m: *const Matrixf) => c.s.set_rotation_matrix(&*m);
    "_ZN3hpl7cCamera13GetViewMatrixEv" get_view[this, c]() -> *const Matrixf => c.s.view_matrix();
    "_ZN3hpl7cCamera19GetProjectionMatrixEv" get_projection[this, c]() -> *const Matrixf => c.s.projection_matrix();
    "_ZN3hpl7cCamera13GetMoveMatrixEv" get_move[this, c]() -> *const Matrixf => { c.s.update_move(); &c.s.mv };
    "_ZN3hpl7cCamera12AttachEntityEPNS_9iEntity3DE" attach[this, c](e: *mut u8) => { node_add_entity(c.this.add(NODE_OFFSET).cast_mut(), e); };
    "_ZN3hpl7cCamera12RemoveEntityEPNS_9iEntity3DE" detach[this, c](e: *mut u8) => { node_remove_entity(c.this.add(NODE_OFFSET).cast_mut(), e); };
    "_ZN3hpl7cCamera21ClearAttachedEntitiesEv" clear_entities[this, c]() => super::ffi_h10::ohpl_node_clear_entities(*c.this.add(NODE_POD).cast::<*mut NodeBox>());
    "_ZN3hpl7cCamera10GetFrustumEv" get_frustum[this, c]() -> *mut u8 => frustum(&mut c);
    "_ZN3hpl7cCamera9UnProjectEPNS_8cVector3IfEES3_RKNS_8cVector2IfEES7_" unproject[this, c](
        pos: *mut Vector3f, dir: *mut Vector3f, screen: *const Vector2f, virt: *const Vector2f) => {
        let m = super::particles::rotation_part(c.s.view_matrix());
        let (p, f, u, r) = (c.s.position, c.s.view_row(2, true), c.s.view_row(1, false), c.s.view_row(0, false));
        helper(c.s, pos, dir, screen, virt, &m, &p, &f, &u, &r)
    };
    "_ZN3hpl7cCamera15UnProjectHelperEPNS_8cVector3IfEES3_RKNS_8cVector2IfEES7_RKNS_7cMatrixIfEERKS2_SD_SD_SD_" unproject_helper[this, c](
        pos: *mut Vector3f, dir: *mut Vector3f, screen: *const Vector2f, virt: *const Vector2f,
        rot: *const Matrixf, p: *const Vector3f, f: *const Vector3f, u: *const Vector3f, r: *const Vector3f) => {
        helper(c.s, pos, dir, screen, virt, &*rot, &*p, &*f, &*u, &*r)
    };
}

/// `cCamera::GetFrustum` for the scene render loop.
///
/// # Safety
/// `this` is a live `cCamera`.
pub(super) unsafe fn camera_frustum(this: *const u8) -> *mut u8 {
    // SAFETY: caller contract.
    unsafe { frustum(&mut cam(this)) }
}

/// Listener attributes of the scene: eye position, `GetForward() * -1.0f`, `GetUp()`.
///
/// # Safety
/// `this` is a live `cCamera`.
pub(super) unsafe fn listener_basis(this: *const u8) -> (Vector3f, Vector3f, Vector3f) {
    // SAFETY: caller contract.
    let c = unsafe { cam(this) };
    let f = c.s.view_row(2, true);
    let fwd = Vector3f {
        x: -f.x,
        y: -f.y,
        z: -f.z,
    };
    (c.s.position, fwd, c.s.view_row(1, false))
}

unsafe fn frustum(c: &mut Cam) -> *mut u8 {
    // SAFETY: mFrustum is a cFrustum, whose layout is the Rust `Frustum`.
    unsafe {
        let fr = c.this.add(FRUSTUM_OFFSET).cast_mut().cast::<Frustum>();
        if c.s.frustum_updated {
            let was_inf = c.s.frustum_begin() & 2 != 0;
            let proj = *c.s.projection_matrix();
            let view = *c.s.view_matrix();
            if c.s.projection_type == 0 {
                (*fr).setup_perspective(
                    &proj,
                    &view,
                    c.s.far,
                    c.s.near,
                    c.s.fov,
                    c.s.aspect,
                    c.s.position,
                    c.s.inf_far,
                    None,
                    false,
                );
            } else {
                (*fr).setup_ortho(
                    &proj,
                    &view,
                    c.s.far,
                    c.s.near,
                    c.s.view_size,
                    c.s.position,
                    c.s.inf_far,
                );
            }
            c.s.frustum_end(was_inf);
        }
        fr.cast()
    }
}

#[allow(clippy::too_many_arguments)]
unsafe fn helper(
    s: &CamState,
    pos: *mut Vector3f,
    dir: *mut Vector3f,
    screen: *const Vector2f,
    virt: *const Vector2f,
    rot: &Matrixf,
    p: &Vector3f,
    f: &Vector3f,
    u: &Vector3f,
    r: &Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let params = super::camera::ProjectionParams {
            kind: s.projection_type,
            near: s.near,
            far: 0.0,
            fov: s.fov,
            aspect: s.aspect,
            inf_far: false,
            view_size: s.view_size,
        };
        let basis = super::camera::CameraBasis {
            rotation: *rot,
            position: *p,
            forward: *f,
            up: *u,
            right: *r,
        };
        if let Some((p, d)) = super::camera::unproject(&params, *screen, *virt, &basis) {
            if !dir.is_null() {
                *dir = d;
            }
            if !pos.is_null() {
                *pos = p;
            }
        }
    }
}

macro_rules! row_getter {
    ($($sym:literal $imp:ident($row:literal, $neg:literal);)*) => {$(
        #[unsafe(no_mangle)]
        unsafe extern "C" fn $imp(this: *const u8, out: *mut Vector3f) {
            // SAFETY: C++ passes a live cCamera and the result slot.
            unsafe { *out = cam(this).s.view_row($row, $neg) }
        }
        global_asm!(
            concat!(".globl ", $sym),
            concat!(".type ", $sym, ", %function"),
            concat!($sym, ":"),
            "mov x1, x8",
            concat!("b ", stringify!($imp)),
        );
    )*};
}

row_getter! {
    "_ZN3hpl7cCamera10GetForwardEv" ohpl_cam_forward(2, true);
    "_ZN3hpl7cCamera8GetRightEv" ohpl_cam_right(0, false);
    "_ZN3hpl7cCamera5GetUpEv" ohpl_cam_up(1, false);
}
