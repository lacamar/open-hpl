//! `SomaLoaders.cpp`: the entity loader's decisions. XML access, object creation and the engine
//! calls stay C++.

/// `eSomaLuxEntityType` as C++ numbers it is looked up by the caller; these are the loader's picks.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EntityKind {
    Prop = 0,
    Agent = 1,
    Critter = 2,
}

pub fn kind_from_entity_type(name: &[u8]) -> EntityKind {
    if name.starts_with(b"Agent_") {
        EntityKind::Agent
    } else if name.starts_with(b"Critter_") || name.starts_with(b"critter_") {
        EntityKind::Critter
    } else {
        EntityKind::Prop
    }
}

/// Static meshes are never animated or moved by HPL2's world update; `masses` are the `Mass`
/// attributes of the `Bodies` elements in order.
pub fn is_dynamic(spawned: bool, has_anim: bool, has_proc_anim: bool, masses: &[f32]) -> bool {
    spawned || has_anim || has_proc_anim || masses.iter().any(|m| *m > 0.0)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ElementId {
    pub a: u8,
    pub b: i32,
    pub c: i32,
}

/// `UID="a b c"` of the element, else its `ID` attribute.
pub fn element_id(uid: &[i32], id_attr: i32) -> ElementId {
    match uid {
        [a, b, c] => ElementId {
            a: *a as u8,
            b: *b,
            c: *c,
        },
        _ => ElementId {
            a: 0,
            b: 0,
            c: id_attr,
        },
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum JointsStep {
    /// Nothing to pose: `BeforeJoints` returns false.
    NoPose,
    /// Set the rest pose from the map values; then call `after_rest_pose`.
    SetRestPose,
}

pub fn before_joints(has_pose: bool, has_skeleton: bool) -> JointsStep {
    if has_pose && has_skeleton {
        JointsStep::SetRestPose
    } else {
        JointsStep::NoPose
    }
}

/// After `SetBoneRestPoseFromMapValues`: whether the bodies are aligned to the skeleton.
pub fn align_bodies(rest_pose_set: bool) -> bool {
    rest_pose_set
}

pub fn mean_scale(s: [f32; 3]) -> f32 {
    (s[0] + s[1] + s[2]) / 3.0
}

/// Child children are re-parented to a mesh entity that has no bodies, at a scaled offset.
pub fn reparent_children(has_mesh: bool, bodies: usize) -> bool {
    has_mesh && bodies == 0
}

pub fn scaled_offset(local: [f32; 3], scale: [f32; 3]) -> [f32; 3] {
    [
        local[0] * scale[0],
        local[1] * scale[1],
        local[2] * scale[2],
    ]
}

/// A prop whose first animation is active starts at a random time when `RandomizeAnimationStart`.
pub fn random_anim_start(is_prop: bool, randomize_var: bool) -> bool {
    is_prop && randomize_var
}

/// cLuxPropLoader::AfterLoad: gravity-free first steps let Newton put resting props to sleep.
pub fn freeze_props(
    is_prop: bool,
    active: bool,
    disable_var: bool,
    skeleton_physics: bool,
) -> bool {
    is_prop && active && !disable_var && !skeleton_physics
}

pub fn no_gravity_underwater(is_prop: bool, var: bool) -> bool {
    is_prop && var
}

/// `Socket_` bones (case-insensitive over the first 7 bytes) become sockets.
pub fn socket_bone_name(bone: &[u8]) -> Option<&[u8]> {
    let (head, tail) = bone.split_at_checked(7)?;
    head.eq_ignore_ascii_case(b"socket_").then_some(tail)
}

/// Light and billboard colour multipliers of the instance (`EffectColorMul`).
pub fn color_mul(col: [f32; 4], mul: [f32; 4]) -> [f32; 4] {
    [col[0] * mul[0], col[1] * mul[1], col[2] * mul[2], col[3]]
}

/// `cColor * cColor` of the billboard colour.
pub fn color_mul_all(col: [f32; 4], mul: [f32; 4]) -> [f32; 4] {
    [
        col[0] * mul[0],
        col[1] * mul[1],
        col[2] * mul[2],
        col[3] * mul[3],
    ]
}

/// `ShowMesh` default: collision-only `StaticCollider`s are never visible.
pub fn default_show_mesh(entity_type: &[u8]) -> bool {
    entity_type != b"StaticCollider"
}
