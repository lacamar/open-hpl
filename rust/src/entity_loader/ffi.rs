//! C ABI of the `.ent` parse (`HPL2/core/include/resources/RustEntityLoader.h`). The C++ side
//! passes a table of callbacks ([`Vt`]) that implements [`Builder`]; every callback is `noexcept`.

use super::builder::{
    AnimEventType, BodyProps, Builder, Flags, Handle, JointKind, JointProps, MeshInfo, Params,
    WorldEntity,
};
use super::parse::parse;
use crate::types::{Matrixf, Vector3f};
use crate::xml::{Doc, NodeId};
use std::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    p: *const u8,
    n: usize,
}

impl Str {
    fn new(s: &[u8]) -> Self {
        Str {
            p: s.as_ptr(),
            n: s.len(),
        }
    }

    /// # Safety
    /// `p` points to `n` readable bytes, or `n` is 0.
    unsafe fn bytes<'a>(self) -> &'a [u8] {
        if self.n == 0 {
            return &[];
        }
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(self.p, self.n) }
    }
}

type Raw = usize;

#[repr(C)]
pub struct ParamsC {
    name: Str,
    file_name: Str,
    full_path: Str,
    full_path_dir: Str,
    transform: Matrixf,
    scale: Vector3f,
    physics_world: bool,
}

#[repr(C)]
struct JointPropsC {
    move_sound: Str,
    break_sound: Str,
    max_limit_sound: Str,
    min_limit_sound: Str,
    floats: [f32; 15],
    bools: [bool; 6],
    ints: [i32; 2],
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<ParamsC>() == 144 && size_of::<JointPropsC>() == 144);
    assert!(size_of::<BodyProps>() == 104 && size_of::<Flags>() == 9);
};

macro_rules! vt {
    ($($name:ident: fn($($arg:ty),*) $(-> $ret:ty)?;)*) => {
        #[repr(C)]
        pub struct Vt {
            ctx: *mut c_void,
            $($name: unsafe extern "C" fn(*mut c_void $(, $arg)*) $(-> $ret)?,)*
        }
    };
}

vt! {
    error: fn(Str);
    warning: fn(Str);
    before_load: fn();
    after_load: fn();
    before_joints: fn() -> bool;
    flags: fn(*mut Flags);
    create_mesh: fn(Str, *mut bool, *mut i32) -> bool;
    create_mesh_entity: fn(Str) -> Raw;
    set_matrix: fn(Raw, *const Matrixf);
    set_shadow_caster: fn(Raw);
    update_logic: fn(Raw);
    get_sub_mesh_entity: fn(Str) -> Raw;
    warn_missing_sub_mesh: fn(Str);
    get_sub_mesh_model_scale: fn(Raw) -> Vector3f;
    set_world_matrix: fn(Raw, *const Matrixf);
    set_unique_id: fn(Raw, i32);
    set_sub_mesh_material: fn(Raw, Str);
    add_animation: fn(Str, Str, f32) -> Raw;
    set_special_event_time: fn(Raw, f32);
    add_animation_event: fn(Raw, f32, i32, Str, Str, Str);
    load_world_entity: fn(i32, u32, Str) -> Raw;
    get_local_position: fn(Raw) -> Vector3f;
    set_position: fn(Raw, Vector3f);
    link_light_billboards: fn();
    get_bone_state: fn(Str) -> Raw;
    get_bone_state_num: fn() -> i32;
    warn_skeleton_mismatch: fn();
    get_bone_world_matrix: fn(Raw, *mut Matrixf);
    get_bone_name: fn(Raw) -> Str;
    get_unique_id: fn(Raw) -> i32;
    is_sub_mesh: fn(Raw) -> bool;
    get_local_matrix: fn(Raw, *mut Matrixf);
    get_world_matrix: fn(Raw, *mut Matrixf);
    get_entity_name: fn(Raw) -> Str;
    set_node_animation: fn();
    attach_entity_child: fn(Raw, *const Matrixf, Raw);
    attach_bone_child: fn(Raw, *const Matrixf, Raw);
    attach_bone_to_body: fn(Raw, *const Matrixf, Raw);
    create_box_shape: fn(Vector3f, *const Matrixf) -> Raw;
    create_sphere_shape: fn(Vector3f, *const Matrixf) -> Raw;
    create_cylinder_shape: fn(f32, f32, *const Matrixf) -> Raw;
    create_capsule_shape: fn(f32, f32, *const Matrixf) -> Raw;
    create_compound_shape: fn(*const Raw, usize) -> Raw;
    inc_shape_user_count: fn(Raw);
    destroy_shape: fn(Raw);
    create_body: fn(Str, Raw) -> Raw;
    set_body_properties: fn(Raw, *const BodyProps);
    set_body_material: fn(Raw, Str);
    register_body: fn(Raw);
    create_joint_hinge: fn(Str, Vector3f, Vector3f, Raw, Raw) -> Raw;
    create_joint_ball: fn(Str, Vector3f, Vector3f, Raw, Raw) -> Raw;
    create_joint_slider: fn(Str, Vector3f, Vector3f, Raw, Raw) -> Raw;
    create_joint_screw: fn(Str, Vector3f, Vector3f, Raw, Raw) -> Raw;
    set_hinge_limits: fn(Raw, f32, f32);
    set_ball_limits: fn(Raw, f32, f32);
    set_slider_limits: fn(Raw, f32, f32);
    set_screw_limits: fn(Raw, f32, f32);
    set_joint_properties: fn(Raw, *const JointPropsC);
    register_joint: fn(Raw);
    get_animation_state_num: fn() -> i32;
    play_first_animation: fn();
    set_skeleton_physics_active: fn();
    load_user_variables: fn();
}

struct Ffi<'a>(&'a Vt);

fn raw(h: Handle) -> Raw {
    h.get()
}

fn opt(r: Raw) -> Option<Handle> {
    Handle::new(r)
}

// SAFETY (all calls below): `Vt` callbacks are valid for the duration of `ohpl_ent_parse`; every
// pointer passed is to a live local or to bytes borrowed for the call.
impl Builder for Ffi<'_> {
    fn error(&mut self, m: &[u8]) {
        unsafe { (self.0.error)(self.0.ctx, Str::new(m)) }
    }

    fn warning(&mut self, m: &[u8]) {
        unsafe { (self.0.warning)(self.0.ctx, Str::new(m)) }
    }

    fn before_load(&mut self) {
        unsafe { (self.0.before_load)(self.0.ctx) }
    }

    fn after_load(&mut self) {
        unsafe { (self.0.after_load)(self.0.ctx) }
    }

    fn before_joints(&mut self) -> bool {
        unsafe { (self.0.before_joints)(self.0.ctx) }
    }

    fn flags(&mut self) -> Flags {
        let mut f = Flags::default();
        unsafe { (self.0.flags)(self.0.ctx, &mut f) };
        f
    }

    fn create_mesh(&mut self, file: &[u8]) -> Option<MeshInfo> {
        let (mut skeleton, mut anim_num) = (false, 0);
        unsafe { (self.0.create_mesh)(self.0.ctx, Str::new(file), &mut skeleton, &mut anim_num) }
            .then_some(MeshInfo { skeleton, anim_num })
    }

    fn create_mesh_entity(&mut self, name: &[u8]) -> Option<Handle> {
        opt(unsafe { (self.0.create_mesh_entity)(self.0.ctx, Str::new(name)) })
    }

    fn set_matrix(&mut self, ent: Handle, m: &Matrixf) {
        unsafe { (self.0.set_matrix)(self.0.ctx, raw(ent), m) }
    }

    fn set_shadow_caster(&mut self, ent: Handle) {
        unsafe { (self.0.set_shadow_caster)(self.0.ctx, raw(ent)) }
    }

    fn update_logic(&mut self, ent: Handle) {
        unsafe { (self.0.update_logic)(self.0.ctx, raw(ent)) }
    }

    fn get_sub_mesh_entity(&mut self, name: &[u8]) -> Option<Handle> {
        opt(unsafe { (self.0.get_sub_mesh_entity)(self.0.ctx, Str::new(name)) })
    }

    fn warn_missing_sub_mesh(&mut self, name: &[u8]) {
        unsafe { (self.0.warn_missing_sub_mesh)(self.0.ctx, Str::new(name)) }
    }

    fn get_sub_mesh_model_scale(&mut self, sub: Handle) -> Vector3f {
        unsafe { (self.0.get_sub_mesh_model_scale)(self.0.ctx, raw(sub)) }
    }

    fn set_world_matrix(&mut self, ent: Handle, m: &Matrixf) {
        unsafe { (self.0.set_world_matrix)(self.0.ctx, raw(ent), m) }
    }

    fn set_unique_id(&mut self, ent: Handle, id: i32) {
        unsafe { (self.0.set_unique_id)(self.0.ctx, raw(ent), id) }
    }

    fn set_sub_mesh_material(&mut self, sub: Handle, file: &[u8]) {
        unsafe { (self.0.set_sub_mesh_material)(self.0.ctx, raw(sub), Str::new(file)) }
    }

    fn add_animation(&mut self, file: &[u8], name: &[u8], speed: f32) -> Option<Handle> {
        opt(unsafe { (self.0.add_animation)(self.0.ctx, Str::new(file), Str::new(name), speed) })
    }

    fn set_special_event_time(&mut self, state: Handle, time: f32) {
        unsafe { (self.0.set_special_event_time)(self.0.ctx, raw(state), time) }
    }

    fn add_animation_event(
        &mut self,
        state: Handle,
        time: f32,
        ty: AnimEventType,
        value: &[u8],
        name: &[u8],
        dest_socket: &[u8],
    ) {
        unsafe {
            (self.0.add_animation_event)(
                self.0.ctx,
                raw(state),
                time,
                ty as i32,
                Str::new(value),
                Str::new(name),
                Str::new(dest_socket),
            )
        }
    }

    fn load_world_entity(&mut self, kind: WorldEntity, elem: u32, prefix: &[u8]) -> Option<Handle> {
        opt(unsafe { (self.0.load_world_entity)(self.0.ctx, kind as i32, elem, Str::new(prefix)) })
    }

    fn get_local_position(&mut self, ent: Handle) -> Vector3f {
        unsafe { (self.0.get_local_position)(self.0.ctx, raw(ent)) }
    }

    fn set_position(&mut self, ent: Handle, pos: Vector3f) {
        unsafe { (self.0.set_position)(self.0.ctx, raw(ent), pos) }
    }

    fn link_light_billboards(&mut self) {
        unsafe { (self.0.link_light_billboards)(self.0.ctx) }
    }

    fn get_bone_state(&mut self, name: &[u8]) -> Option<Handle> {
        opt(unsafe { (self.0.get_bone_state)(self.0.ctx, Str::new(name)) })
    }

    fn get_bone_state_num(&mut self) -> i32 {
        unsafe { (self.0.get_bone_state_num)(self.0.ctx) }
    }

    fn warn_skeleton_mismatch(&mut self) {
        unsafe { (self.0.warn_skeleton_mismatch)(self.0.ctx) }
    }

    fn get_bone_world_matrix(&mut self, bone: Handle) -> Matrixf {
        let mut m = Matrixf::default();
        unsafe { (self.0.get_bone_world_matrix)(self.0.ctx, raw(bone), &mut m) };
        m
    }

    fn get_bone_name(&mut self, bone: Handle) -> Vec<u8> {
        // SAFETY: the callback returns a string valid until the next call; copied at once.
        unsafe { (self.0.get_bone_name)(self.0.ctx, raw(bone)).bytes() }.to_vec()
    }

    fn get_unique_id(&mut self, ent: Handle) -> i32 {
        unsafe { (self.0.get_unique_id)(self.0.ctx, raw(ent)) }
    }

    fn is_sub_mesh(&mut self, ent: Handle) -> bool {
        unsafe { (self.0.is_sub_mesh)(self.0.ctx, raw(ent)) }
    }

    fn get_local_matrix(&mut self, ent: Handle) -> Matrixf {
        let mut m = Matrixf::default();
        unsafe { (self.0.get_local_matrix)(self.0.ctx, raw(ent), &mut m) };
        m
    }

    fn get_world_matrix(&mut self, ent: Handle) -> Matrixf {
        let mut m = Matrixf::default();
        unsafe { (self.0.get_world_matrix)(self.0.ctx, raw(ent), &mut m) };
        m
    }

    fn get_entity_name(&mut self, ent: Handle) -> Vec<u8> {
        // SAFETY: the callback returns a string valid until the next call; copied at once.
        unsafe { (self.0.get_entity_name)(self.0.ctx, raw(ent)).bytes() }.to_vec()
    }

    fn set_node_animation(&mut self) {
        unsafe { (self.0.set_node_animation)(self.0.ctx) }
    }

    fn attach_entity_child(&mut self, parent: Handle, inv_parent: &Matrixf, child: Handle) {
        unsafe { (self.0.attach_entity_child)(self.0.ctx, raw(parent), inv_parent, raw(child)) }
    }

    fn attach_bone_child(&mut self, bone: Handle, inv_parent: &Matrixf, child: Handle) {
        unsafe { (self.0.attach_bone_child)(self.0.ctx, raw(bone), inv_parent, raw(child)) }
    }

    fn attach_bone_to_body(&mut self, body: Handle, inv_parent: &Matrixf, bone: Handle) {
        unsafe { (self.0.attach_bone_to_body)(self.0.ctx, raw(body), inv_parent, raw(bone)) }
    }

    fn create_box_shape(&mut self, size: Vector3f, offset: &Matrixf) -> Option<Handle> {
        opt(unsafe { (self.0.create_box_shape)(self.0.ctx, size, offset) })
    }

    fn create_sphere_shape(&mut self, size: Vector3f, offset: &Matrixf) -> Option<Handle> {
        opt(unsafe { (self.0.create_sphere_shape)(self.0.ctx, size, offset) })
    }

    fn create_cylinder_shape(
        &mut self,
        radius: f32,
        height: f32,
        offset: &Matrixf,
    ) -> Option<Handle> {
        opt(unsafe { (self.0.create_cylinder_shape)(self.0.ctx, radius, height, offset) })
    }

    fn create_capsule_shape(
        &mut self,
        radius: f32,
        height: f32,
        offset: &Matrixf,
    ) -> Option<Handle> {
        opt(unsafe { (self.0.create_capsule_shape)(self.0.ctx, radius, height, offset) })
    }

    fn create_compound_shape(&mut self, shapes: &[Handle]) -> Option<Handle> {
        let raws: Vec<Raw> = shapes.iter().map(|&h| raw(h)).collect();
        opt(unsafe { (self.0.create_compound_shape)(self.0.ctx, raws.as_ptr(), raws.len()) })
    }

    fn inc_shape_user_count(&mut self, shape: Handle) {
        unsafe { (self.0.inc_shape_user_count)(self.0.ctx, raw(shape)) }
    }

    fn destroy_shape(&mut self, shape: Handle) {
        unsafe { (self.0.destroy_shape)(self.0.ctx, raw(shape)) }
    }

    fn create_body(&mut self, name: &[u8], shape: Handle) -> Option<Handle> {
        opt(unsafe { (self.0.create_body)(self.0.ctx, Str::new(name), raw(shape)) })
    }

    fn set_body_properties(&mut self, body: Handle, props: &BodyProps) {
        unsafe { (self.0.set_body_properties)(self.0.ctx, raw(body), props) }
    }

    fn set_body_material(&mut self, body: Handle, name: &[u8]) {
        unsafe { (self.0.set_body_material)(self.0.ctx, raw(body), Str::new(name)) }
    }

    fn register_body(&mut self, body: Handle) {
        unsafe { (self.0.register_body)(self.0.ctx, raw(body)) }
    }

    fn create_joint(
        &mut self,
        kind: JointKind,
        name: &[u8],
        pivot: Vector3f,
        pin: Vector3f,
        parent: Option<Handle>,
        child: Handle,
    ) -> Option<Handle> {
        let create = match kind {
            JointKind::Hinge => self.0.create_joint_hinge,
            JointKind::Ball => self.0.create_joint_ball,
            JointKind::Slider => self.0.create_joint_slider,
            JointKind::Screw => self.0.create_joint_screw,
        };
        opt(unsafe {
            create(
                self.0.ctx,
                Str::new(name),
                pivot,
                pin,
                parent.map_or(0, raw),
                raw(child),
            )
        })
    }

    fn set_hinge_limits(&mut self, joint: Handle, min: f32, max: f32) {
        unsafe { (self.0.set_hinge_limits)(self.0.ctx, raw(joint), min, max) }
    }

    fn set_ball_limits(&mut self, joint: Handle, cone: f32, twist: f32) {
        unsafe { (self.0.set_ball_limits)(self.0.ctx, raw(joint), cone, twist) }
    }

    fn set_slider_limits(&mut self, joint: Handle, min: f32, max: f32) {
        unsafe { (self.0.set_slider_limits)(self.0.ctx, raw(joint), min, max) }
    }

    fn set_screw_limits(&mut self, joint: Handle, min: f32, max: f32) {
        unsafe { (self.0.set_screw_limits)(self.0.ctx, raw(joint), min, max) }
    }

    fn set_joint_properties(&mut self, joint: Handle, p: &JointProps) {
        let c = JointPropsC {
            move_sound: Str::new(&p.move_sound),
            break_sound: Str::new(&p.break_sound),
            max_limit_sound: Str::new(&p.max_limit_sound),
            min_limit_sound: Str::new(&p.min_limit_sound),
            floats: [
                p.min_move_speed,
                p.min_move_freq,
                p.min_move_volume,
                p.min_move_freq_speed,
                p.max_move_freq,
                p.max_move_volume,
                p.max_move_freq_speed,
                p.middle_move_speed,
                p.middle_move_volume,
                p.break_force,
                p.limit_auto_sleep_dist,
                p.max_limit_max_speed,
                p.max_limit_min_speed,
                p.min_limit_max_speed,
                p.min_limit_min_speed,
            ],
            bools: [
                p.angular_move,
                p.sticky_min_limit,
                p.sticky_max_limit,
                p.breakable,
                p.limit_auto_sleep,
                p.collide_bodies,
            ],
            ints: [p.limit_auto_sleep_num_steps, p.id],
        };
        unsafe { (self.0.set_joint_properties)(self.0.ctx, raw(joint), &c) }
    }

    fn register_joint(&mut self, joint: Handle) {
        unsafe { (self.0.register_joint)(self.0.ctx, raw(joint)) }
    }

    fn get_animation_state_num(&mut self) -> i32 {
        unsafe { (self.0.get_animation_state_num)(self.0.ctx) }
    }

    fn play_first_animation(&mut self) {
        unsafe { (self.0.play_first_animation)(self.0.ctx) }
    }

    fn set_skeleton_physics_active(&mut self) {
        unsafe { (self.0.set_skeleton_physics_active)(self.0.ctx) }
    }

    fn load_user_variables(&mut self) {
        unsafe { (self.0.load_user_variables)(self.0.ctx) }
    }
}

/// Runs the `.ent` parse for the element `root` of `doc`; false when the C++ loader returns NULL.
///
/// # Safety
/// `doc` is live; `params` and `vt` are valid for the call; the callbacks in `vt` are `noexcept`
/// and stay valid until return.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_parse(
    doc: *const Doc,
    root: NodeId,
    params: *const ParamsC,
    vt: *const Vt,
) -> bool {
    // SAFETY: caller contract.
    let (doc, c, vt) = unsafe { (&*doc, &*params, &*vt) };
    // SAFETY: the strings borrow C++ buffers that outlive the call.
    let p = unsafe {
        Params {
            name: c.name.bytes(),
            file_name: c.file_name.bytes(),
            full_path: c.full_path.bytes(),
            full_path_dir: c.full_path_dir.bytes(),
            transform: c.transform,
            scale: c.scale,
            physics_world: c.physics_world,
        }
    };
    parse(&mut Ffi(vt), doc, root, &p)
}
