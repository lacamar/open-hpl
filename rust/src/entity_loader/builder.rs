//! The engine side of the `.ent` parse: every object the parser creates or inspects goes through
//! [`Builder`]. Mirrors `iEntityObjectBuilder` (`EntityObjectBuilder.h`).

use crate::types::{Matrixf, Vector3f};
use std::num::NonZeroUsize;

/// Opaque engine object (entity, shape, body, joint, bone, animation); never null.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct Handle(NonZeroUsize);

impl Handle {
    pub fn new(raw: usize) -> Option<Self> {
        NonZeroUsize::new(raw).map(Self)
    }

    pub fn get(self) -> usize {
        self.0.get()
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum WorldEntity {
    ParticleSystem,
    Billboard,
    LensFlare,
    Sound,
    Light,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum AnimEventType {
    PlaySound,
    CreateParticle,
    Step,
    Message,
    PlayLoopSound,
    StopLoopSound,
    LastEnum,
}

pub struct Params<'a> {
    pub name: &'a [u8],
    pub file_name: &'a [u8],
    pub full_path: &'a [u8],
    pub full_path_dir: &'a [u8],
    pub transform: Matrixf,
    pub scale: Vector3f,
    pub physics_world: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Flags {
    pub animations: bool,
    pub particle_systems: bool,
    pub billboards: bool,
    pub sounds: bool,
    pub lights: bool,
    pub sub_mesh_scale_includes_model_scale: bool,
    pub sub_mesh_instance_scale_after_rotation: bool,
    pub sub_mesh_materials: bool,
    pub share_body_shapes: bool,
}

#[derive(Clone, Copy, Debug)]
pub struct MeshInfo {
    pub skeleton: bool,
    pub anim_num: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BodyProps {
    pub matrix: Matrixf,
    pub mass: f32,
    pub angular_damping: f32,
    pub linear_damping: f32,
    pub buoyancy_density_mul: f32,
    pub max_angular_speed: f32,
    pub max_linear_speed: f32,
    pub blocks_sound: bool,
    pub collide_character: bool,
    pub collide: bool,
    pub gravity: bool,
    pub continuous_collision: bool,
    pub pushed_by_character_gravity: bool,
    pub volatile: bool,
    pub use_surface_effects: bool,
    pub can_attach_character: bool,
    pub id: i32,
}

#[derive(Clone, Debug, PartialEq)]
pub struct JointProps {
    pub move_sound: Vec<u8>,
    pub break_sound: Vec<u8>,
    pub max_limit_sound: Vec<u8>,
    pub min_limit_sound: Vec<u8>,
    pub min_move_speed: f32,
    pub min_move_freq: f32,
    pub min_move_volume: f32,
    pub min_move_freq_speed: f32,
    pub max_move_freq: f32,
    pub max_move_volume: f32,
    pub max_move_freq_speed: f32,
    pub middle_move_speed: f32,
    pub middle_move_volume: f32,
    pub break_force: f32,
    pub limit_auto_sleep_dist: f32,
    pub max_limit_max_speed: f32,
    pub max_limit_min_speed: f32,
    pub min_limit_max_speed: f32,
    pub min_limit_min_speed: f32,
    pub angular_move: bool,
    pub sticky_min_limit: bool,
    pub sticky_max_limit: bool,
    pub breakable: bool,
    pub limit_auto_sleep: bool,
    pub collide_bodies: bool,
    pub limit_auto_sleep_num_steps: i32,
    pub id: i32,
}

/// Shape of a joint to create; the limits follow through the matching `set_*_limits`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum JointKind {
    Hinge,
    Ball,
    Slider,
    Screw,
}

pub trait Builder {
    fn error(&mut self, msg: &[u8]);
    fn warning(&mut self, msg: &[u8]);

    fn before_load(&mut self);
    fn after_load(&mut self);
    fn before_joints(&mut self) -> bool;
    fn flags(&mut self) -> Flags;

    fn create_mesh(&mut self, file: &[u8]) -> Option<MeshInfo>;
    fn create_mesh_entity(&mut self, name: &[u8]) -> Option<Handle>;
    fn set_matrix(&mut self, ent: Handle, m: &Matrixf);
    fn set_shadow_caster(&mut self, ent: Handle);
    fn update_logic(&mut self, ent: Handle);
    fn get_sub_mesh_entity(&mut self, name: &[u8]) -> Option<Handle>;
    fn warn_missing_sub_mesh(&mut self, name: &[u8]);
    fn get_sub_mesh_model_scale(&mut self, sub: Handle) -> Vector3f;
    fn set_world_matrix(&mut self, ent: Handle, m: &Matrixf);
    fn set_unique_id(&mut self, ent: Handle, id: i32);
    fn set_sub_mesh_material(&mut self, sub: Handle, file: &[u8]);

    fn add_animation(&mut self, file: &[u8], name: &[u8], speed: f32) -> Option<Handle>;
    fn set_special_event_time(&mut self, state: Handle, time: f32);
    fn add_animation_event(
        &mut self,
        state: Handle,
        time: f32,
        ty: AnimEventType,
        value: &[u8],
        name: &[u8],
        dest_socket: &[u8],
    );

    /// `elem` is the XML node of the entity element.
    fn load_world_entity(&mut self, kind: WorldEntity, elem: u32, prefix: &[u8]) -> Option<Handle>;
    fn get_local_position(&mut self, ent: Handle) -> Vector3f;
    fn set_position(&mut self, ent: Handle, pos: Vector3f);
    fn link_light_billboards(&mut self);

    fn get_bone_state(&mut self, name: &[u8]) -> Option<Handle>;
    fn get_bone_state_num(&mut self) -> i32;
    fn warn_skeleton_mismatch(&mut self);
    fn get_bone_world_matrix(&mut self, bone: Handle) -> Matrixf;
    fn get_bone_name(&mut self, bone: Handle) -> Vec<u8>;

    fn get_unique_id(&mut self, ent: Handle) -> i32;
    fn is_sub_mesh(&mut self, ent: Handle) -> bool;
    fn get_local_matrix(&mut self, ent: Handle) -> Matrixf;
    fn get_world_matrix(&mut self, ent: Handle) -> Matrixf;
    fn get_entity_name(&mut self, ent: Handle) -> Vec<u8>;
    fn set_node_animation(&mut self);
    fn attach_entity_child(&mut self, parent: Handle, inv_parent: &Matrixf, child: Handle);
    fn attach_bone_child(&mut self, bone: Handle, inv_parent: &Matrixf, child: Handle);
    fn attach_bone_to_body(&mut self, body: Handle, inv_parent: &Matrixf, bone: Handle);

    fn create_box_shape(&mut self, size: Vector3f, offset: &Matrixf) -> Option<Handle>;
    fn create_sphere_shape(&mut self, size: Vector3f, offset: &Matrixf) -> Option<Handle>;
    fn create_cylinder_shape(
        &mut self,
        radius: f32,
        height: f32,
        offset: &Matrixf,
    ) -> Option<Handle>;
    fn create_capsule_shape(
        &mut self,
        radius: f32,
        height: f32,
        offset: &Matrixf,
    ) -> Option<Handle>;
    fn create_compound_shape(&mut self, shapes: &[Handle]) -> Option<Handle>;
    fn inc_shape_user_count(&mut self, shape: Handle);
    fn destroy_shape(&mut self, shape: Handle);

    fn create_body(&mut self, name: &[u8], shape: Handle) -> Option<Handle>;
    fn set_body_properties(&mut self, body: Handle, props: &BodyProps);
    fn set_body_material(&mut self, body: Handle, name: &[u8]);
    fn register_body(&mut self, body: Handle);

    fn create_joint(
        &mut self,
        kind: JointKind,
        name: &[u8],
        pivot: Vector3f,
        pin: Vector3f,
        parent: Option<Handle>,
        child: Handle,
    ) -> Option<Handle>;
    fn set_hinge_limits(&mut self, joint: Handle, min: f32, max: f32);
    fn set_ball_limits(&mut self, joint: Handle, cone: f32, twist: f32);
    fn set_slider_limits(&mut self, joint: Handle, min: f32, max: f32);
    fn set_screw_limits(&mut self, joint: Handle, min: f32, max: f32);
    fn set_joint_properties(&mut self, joint: Handle, props: &JointProps);
    fn register_joint(&mut self, joint: Handle);

    fn get_animation_state_num(&mut self) -> i32;
    fn play_first_animation(&mut self);
    fn set_skeleton_physics_active(&mut self);
    fn load_user_variables(&mut self);
}
