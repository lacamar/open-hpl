//! `.ent` object parse, driving a [`Builder`]. Port of `cEntityLoader_Object::Load`; behaviour
//! and quirks follow the C++ (including the min-limit speed fix-up that writes the max limit).

use super::builder::{
    AnimEventType, BodyProps, Builder, Flags, Handle, JointKind, JointProps, Params, WorldEntity,
};
use crate::math::{
    IDENTITY, matrix_inverse, matrix_mul, matrix_mul_3x3, matrix_mul_vec, matrix_rotate,
    matrix_scale, rotate_z, to_rad,
};
use crate::string;
use crate::types::{Matrixf, Vector3f};
use crate::xml::{Doc, NodeId};
use std::collections::BTreeMap;

const PI2: f32 = 1.570_796_4;
const MSG_MAX: usize = 2047;

#[derive(Clone, Copy)]
struct El<'a> {
    doc: &'a Doc,
    id: NodeId,
}

impl<'a> El<'a> {
    fn child(self, name: &[u8]) -> Option<El<'a>> {
        self.doc
            .first_child_element(self.id, Some(name))
            .map(|id| El { doc: self.doc, id })
    }

    fn children(self) -> impl Iterator<Item = El<'a>> {
        let doc = self.doc;
        std::iter::successors(doc.first_child_element(self.id, None), move |&c| {
            doc.next_sibling_element(c, None)
        })
        .map(move |id| El { doc, id })
    }

    fn value(self) -> &'a [u8] {
        crate::xml::c_str(&self.doc.node(self.id).value)
    }

    fn attr(self, name: &[u8]) -> Option<&'a [u8]> {
        self.doc.attribute(self.id, name)
    }

    fn string(self, name: &[u8], default: &[u8]) -> Vec<u8> {
        self.attr(name).unwrap_or(default).to_vec()
    }

    fn float(self, name: &[u8], default: f32) -> f32 {
        self.attr(name).map_or(default, string::token_to_float)
    }

    fn int(self, name: &[u8], default: i32) -> i32 {
        self.attr(name).map_or(default, string::token_to_int)
    }

    fn bool(self, name: &[u8], default: bool) -> bool {
        self.attr(name).map_or(default, string::to_bool)
    }

    fn vec3(self, name: &[u8], default: Vector3f) -> Vector3f {
        let mut v = [0.0f32; 3];
        match self.attr(name) {
            Some(s) if string::floats_exact(s, &mut v) => Vector3f {
                x: v[0],
                y: v[1],
                z: v[2],
            },
            _ => default,
        }
    }
}

const ZERO: Vector3f = Vector3f {
    x: 0.0,
    y: 0.0,
    z: 0.0,
};

fn mul3(a: Vector3f, b: Vector3f) -> Vector3f {
    Vector3f {
        x: a.x * b.x,
        y: a.y * b.y,
        z: a.z * b.z,
    }
}

fn msg(parts: &[&[u8]]) -> Vec<u8> {
    let mut m = parts.concat();
    m.truncate(MSG_MAX);
    m
}

fn num(x: i32) -> Vec<u8> {
    x.to_string().into_bytes()
}

fn set_translation(m: &mut Matrixf, t: Vector3f) {
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
}

fn matrix_from_vectors(pos: Vector3f, rot: Vector3f, scale: Vector3f) -> Matrixf {
    let mut out = matrix_mul(&matrix_rotate(rot, 0), &matrix_scale(scale));
    set_translation(&mut out, pos);
    out
}

fn anim_event_type<B: Builder>(b: &mut B, ty: &[u8]) -> AnimEventType {
    match string::to_lower(ty).as_slice() {
        b"playsound" => AnimEventType::PlaySound,
        b"createparticle" => AnimEventType::CreateParticle,
        b"step" => AnimEventType::Step,
        b"message" => AnimEventType::Message,
        b"playloopsound" => AnimEventType::PlayLoopSound,
        b"stoploopsound" => AnimEventType::StopLoopSound,
        b"" | b"ik" => AnimEventType::LastEnum,
        _ => {
            b.error(&msg(&[b"No animation event named '", ty, b"'\n"]));
            AnimEventType::LastEnum
        }
    }
}

/// `std::multimap<int, Handle>`: equal keys keep insertion order.
#[derive(Default)]
struct MultiMap(Vec<(i32, Handle)>);

impl MultiMap {
    fn insert(&mut self, key: i32, h: Handle) {
        let at = self.0.partition_point(|&(k, _)| k <= key);
        self.0.insert(at, (key, h));
    }

    fn find(&self, key: i32) -> Option<usize> {
        let at = self.0.partition_point(|&(k, _)| k < key);
        (self.0.get(at)?.0 == key).then_some(at)
    }
}

fn create_shape<B: Builder>(b: &mut B, e: El, scale: Vector3f) -> Option<Handle> {
    let ty = e.string(b"ShapeType", b"");
    let kind = match string::to_lower(&ty).as_slice() {
        b"box" => Some(0),
        b"cylinder" => Some(1),
        b"sphere" => Some(2),
        b"capsule" => Some(3),
        _ => {
            b.error(&msg(&[b"CollideShape '", &ty, b"' does not exist!\n"]));
            None
        }
    };
    let size = mul3(e.vec3(b"Scale", ZERO), scale);
    let pos = mul3(e.vec3(b"RelativeTranslation", ZERO), scale);
    let rot = e.vec3(b"RelativeRotation", ZERO);

    let mut offset = matrix_rotate(rot, 0);
    set_translation(&mut offset, pos);
    offset = matrix_mul(&IDENTITY, &offset);

    match kind? {
        0 => b.create_box_shape(size, &offset),
        2 => b.create_sphere_shape(size, &offset),
        1 => {
            offset = matrix_mul(&offset, &rotate_z(PI2));
            b.create_cylinder_shape(size.x, size.y, &offset)
        }
        _ => {
            offset = matrix_mul(&offset, &rotate_z(PI2));
            b.create_capsule_shape(size.x, size.y, &offset)
        }
    }
}

fn body_shape<B: Builder>(
    b: &mut B,
    body: El,
    shapes: &mut MultiMap,
    used: &mut Vec<Handle>,
    share: bool,
) -> Option<Handle> {
    let mut found = Vec::new();
    for s in body.children() {
        let id = s.int(b"ID", 0);
        if let Some(at) = shapes.find(id) {
            let h = shapes.0[at].1;
            found.push(h);
            if share {
                if !used.contains(&h) {
                    used.push(h);
                    b.inc_shape_user_count(h);
                }
            } else {
                shapes.0.remove(at);
            }
        }
    }
    match found.as_slice() {
        [] => None,
        [one] => Some(*one),
        many => b.create_compound_shape(many),
    }
}

fn body_properties(e: El, scale: Vector3f) -> BodyProps {
    BodyProps {
        matrix: matrix_from_vectors(
            mul3(e.vec3(b"WorldPos", ZERO), scale),
            e.vec3(b"Rotation", ZERO),
            Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
        ),
        mass: e.float(b"Mass", 1.0),
        angular_damping: e.float(b"AngularDamping", 0.0),
        linear_damping: e.float(b"LinearDamping", 0.0),
        blocks_sound: e.bool(b"BlocksSound", false),
        collide_character: e.bool(b"CollideCharacter", true),
        collide: e.bool(b"CollideNonCharacter", true),
        gravity: e.bool(b"HasGravity", true),
        buoyancy_density_mul: e.float(b"BuoyancyDensityMul", 1.0),
        max_angular_speed: e.float(b"MaxAngularSpeed", 0.0),
        max_linear_speed: e.float(b"MaxLinearSpeed", 0.0),
        continuous_collision: e.bool(b"ContinuousCollision", true),
        pushed_by_character_gravity: e.bool(b"PushedByCharacterGravity", false),
        volatile: e.bool(b"Volatile", false),
        use_surface_effects: e.bool(b"UseSurfaceEffects", true),
        can_attach_character: e.bool(b"CanAttachCharacter", false),
        id: e.int(b"ID", -1),
    }
}

fn joint_properties(e: El) -> JointProps {
    let mut j = JointProps {
        move_sound: e.string(b"MoveSound", b""),
        min_move_speed: e.float(b"MinMoveSpeed", 0.5),
        min_move_freq: e.float(b"MinMoveFreq", 0.9),
        min_move_volume: e.float(b"MinMoveVolume", 0.3),
        min_move_freq_speed: e.float(b"MinMoveFreqSpeed", 0.9),
        max_move_freq: e.float(b"MaxMoveFreq", 1.1),
        max_move_volume: e.float(b"MaxMoveVolume", 1.0),
        max_move_freq_speed: e.float(b"MaxMoveFreqSpeed", 1.1),
        middle_move_speed: e.float(b"MiddleMoveSpeed", 1.0),
        middle_move_volume: e.float(b"MiddleMoveVolume", 1.0),
        angular_move: string::to_lower(&e.string(b"MoveType", b"Linear")) == b"angular",
        sticky_min_limit: e.bool(b"StickyMinLimit", false),
        sticky_max_limit: e.bool(b"StickyMaxLimit", false),
        breakable: e.bool(b"Breakable", false),
        break_force: e.float(b"BreakForce", 1000.0),
        break_sound: e.string(b"BreakSound", b""),
        limit_auto_sleep: e.bool(b"LimitAutoSleep", false),
        limit_auto_sleep_dist: e.float(b"LimitAutoSleepDist", 0.02),
        limit_auto_sleep_num_steps: e.int(b"LimitAutoSleepNumSteps", 10),
        collide_bodies: e.bool(b"CollideBodies", true),
        max_limit_sound: e.string(b"MaxLimitSound", b""),
        max_limit_max_speed: e.float(b"MaxLimitMaxSpeed", 10.0),
        max_limit_min_speed: e.float(b"MaxLimit_MinSpeed", 20.0),
        min_limit_sound: Vec::new(),
        min_limit_max_speed: 0.0,
        min_limit_min_speed: 0.0,
        id: 0,
    };
    if j.max_limit_max_speed <= 0.0 {
        j.max_limit_max_speed = 0.01;
    }
    j.min_limit_sound = e.string(b"MinLimitSound", b"");
    j.min_limit_max_speed = e.float(b"MinLimitMaxSpeed", 10.0);
    j.min_limit_min_speed = e.float(b"MinLimitMinSpeed", 20.0);
    if j.min_limit_max_speed <= 0.0 {
        j.max_limit_max_speed = 0.01;
    }
    j.id = e.int(b"ID", -1);
    j
}

struct Parse<'a, B: Builder> {
    b: &'a mut B,
    p: &'a Params<'a>,
    skeleton: bool,
    anim_num: i32,
    ent: Handle,
    node_animation: bool,
}

impl<B: Builder> Parse<'_, B> {
    fn attach_children(
        &mut self,
        main: El,
        parent: Option<Handle>,
        bone: Option<Handle>,
        list: &mut Vec<Handle>,
        bones: &mut BTreeMap<i32, Handle>,
        is_body: bool,
    ) {
        let Some(children) = main.child(b"Children") else {
            return;
        };
        let inv_parent = match (parent, bone) {
            (Some(p), _) => matrix_inverse(&self.b.get_local_matrix(p)),
            (None, Some(bn)) => matrix_inverse(&self.b.get_bone_world_matrix(bn)),
            (None, None) => IDENTITY,
        };

        for child in children.children() {
            let id = child.int(b"ID", 0);

            let mut found = false;
            let mut i = 0;
            while i < list.len() {
                let ent = list[i];
                i += 1;
                if self.b.get_unique_id(ent) != id {
                    continue;
                }
                if let Some(parent) = parent {
                    if self.b.is_sub_mesh(ent) && !self.skeleton && self.anim_num > 0 {
                        self.node_animation = true;
                        self.b.set_node_animation();
                        continue;
                    }
                    self.b.attach_entity_child(parent, &inv_parent, ent);
                } else if let Some(bone) = bone {
                    self.b.attach_bone_child(bone, &inv_parent, ent);
                }
                list.remove(i - 1);
                found = true;
                break;
            }

            if is_body
                && !found
                && let Some(&bn) = bones.get(&id)
            {
                if let Some(parent) = parent {
                    self.b.attach_bone_to_body(parent, &inv_parent, bn);
                }
                found = true;
                bones.remove(&id);
            }

            if !found {
                let file = self.p.file_name;
                if let Some(parent) = parent {
                    let name = self.b.get_entity_name(parent);
                    self.b.warning(&msg(&[
                        b"Could not find child with unique ID ",
                        &num(id),
                        b" to attach to entity '",
                        &name,
                        b"' in '",
                        file,
                        b"'\n",
                    ]));
                } else if let Some(bone) = bone {
                    let name = self.b.get_bone_name(bone);
                    self.b.warning(&msg(&[
                        b"Could not find child with unique ID ",
                        &num(id),
                        b" to attach to bone '",
                        &name,
                        b"' in '",
                        file,
                        b"'\n",
                    ]));
                }
            }
        }
    }

    fn create_joint(
        &mut self,
        e: El,
        bodies: &MultiMap,
        rest: &BTreeMap<Handle, Matrixf>,
    ) -> Option<Handle> {
        let scale = self.p.scale;
        let mut pivot = mul3(e.vec3(b"WorldPos", ZERO), scale);

        let pin_dir = e.string(b"PinDir", b"");
        let mut pin = if !pin_dir.is_empty() {
            e.vec3(b"PinDir", ZERO)
        } else {
            matrix_mul_3x3(
                &matrix_rotate(e.vec3(b"Rotation", ZERO), 0),
                Vector3f {
                    x: 0.0,
                    y: 1.0,
                    z: 0.0,
                },
            )
        };

        pivot = matrix_mul_vec(&self.p.transform, pivot);
        pin = matrix_mul_3x3(&self.p.transform, pin);

        let mut joint_name = self.p.name.to_vec();
        joint_name.push(b'_');
        joint_name.extend_from_slice(&e.string(b"Name", b""));

        let ty = e.value();
        let kind = match string::to_lower(ty).as_slice() {
            b"jointhinge" => JointKind::Hinge,
            b"jointball" => JointKind::Ball,
            b"jointslider" => JointKind::Slider,
            b"joinscrew" => JointKind::Screw,
            _ => {
                self.b
                    .error(&msg(&[b"Joint type '", ty, b"' does not exist!\n"]));
                JointKind::Ball
            }
        };

        let parent_id = e.int(b"ConnectedParentBodyID", 0);
        let child_id = e.int(b"ConnectedChildBodyID", 0);

        let parent = if parent_id > 0 {
            bodies.find(parent_id).map(|at| bodies.0[at].1)
        } else {
            None
        };
        let Some(child) = bodies.find(child_id).map(|at| bodies.0[at].1) else {
            self.b.error(&msg(&[
                b"Could not find child body with ID ",
                &num(child_id),
                b" for joint '",
                &joint_name,
                b"'\n",
            ]));
            return None;
        };

        if let Some(rest) = rest.get(&child) {
            let delta = matrix_mul(&self.b.get_world_matrix(child), &matrix_inverse(rest));
            pivot = matrix_mul_vec(&delta, pivot);
            pin = matrix_mul_3x3(&delta, pin);
        }

        let joint = self
            .b
            .create_joint(kind, &joint_name, pivot, pin, parent, child)?;
        match kind {
            JointKind::Hinge => {
                let min = to_rad(e.float(b"MinAngle", 0.0));
                let max = to_rad(e.float(b"MaxAngle", 0.0));
                self.b.set_hinge_limits(joint, min, max);
            }
            JointKind::Ball => {
                let cone = to_rad(e.float(b"MaxConeAngle", 0.0));
                let twist = to_rad(e.float(b"MaxTwistAngle", 0.0));
                self.b.set_ball_limits(joint, cone, twist);
            }
            JointKind::Slider => {
                let min = e.float(b"MinDistance", 0.0);
                let max = e.float(b"MaxDistance", 0.0);
                self.b.set_slider_limits(joint, min, max);
            }
            JointKind::Screw => {
                let min = e.float(b"MinDistance", 0.0);
                let max = e.float(b"MaxDistance", 0.0);
                self.b.set_screw_limits(joint, min, max);
            }
        }
        Some(joint)
    }

    /// `false` on the early exits where the C++ returns NULL.
    fn run(&mut self, root: El) -> bool {
        let scale = self.p.scale;
        let mut entities: Vec<Handle> = Vec::new();

        let Some(model) = root.child(b"ModelData") else {
            self.b.error(b"Couldn't load element ModelData");
            return false;
        };

        self.b.before_load();
        let f: Flags = self.b.flags();

        let Some(mesh_elem) = model.child(b"Mesh") else {
            self.b.error(b"Couldn't load element Mesh");
            return false;
        };

        let mut mesh_file = mesh_elem.string(b"Filename", b"");
        if string::file_path(&mesh_file).is_empty() {
            mesh_file = string::set_file_path(&mesh_file, self.p.full_path_dir);
        }
        let Some(mesh) = self.b.create_mesh(&mesh_file) else {
            return false;
        };
        self.skeleton = mesh.skeleton;
        self.anim_num = mesh.anim_num;

        // The C++ dereferences a null mesh entity here.
        let Some(ent) = self.b.create_mesh_entity(self.p.name) else {
            return false;
        };
        self.ent = ent;
        if self.skeleton {
            self.b.set_matrix(ent, &matrix_scale(scale));
        }
        self.b.set_shadow_caster(ent);

        for sub_elem in mesh_elem.children() {
            let name = sub_elem.string(b"Name", b"");

            let Some(sub) = self.b.get_sub_mesh_entity(&name) else {
                self.b.warn_missing_sub_mesh(&name);
                continue;
            };
            if !self.skeleton {
                entities.push(sub);

                let mut sub_scale = sub_elem.vec3(b"Scale", ZERO);
                if f.sub_mesh_scale_includes_model_scale {
                    let m = self.b.get_sub_mesh_model_scale(sub);
                    if m.x != 0.0 {
                        sub_scale.x /= m.x;
                    }
                    if m.y != 0.0 {
                        sub_scale.y /= m.y;
                    }
                    if m.z != 0.0 {
                        sub_scale.z /= m.z;
                    }
                }
                let world_pos = sub_elem.vec3(b"WorldPos", ZERO);
                let rot = sub_elem.vec3(b"Rotation", ZERO);
                let local = if f.sub_mesh_instance_scale_after_rotation {
                    matrix_mul(
                        &matrix_scale(scale),
                        &matrix_from_vectors(world_pos, rot, sub_scale),
                    )
                } else {
                    matrix_from_vectors(mul3(world_pos, scale), rot, mul3(sub_scale, scale))
                };
                self.b.set_world_matrix(sub, &local);
            }

            let mut id = sub_elem.int(b"ID", -1);
            if id < 0 {
                id = sub_elem.int(b"SubMeshID", 0);
            }
            self.b.set_unique_id(sub, id);

            let material = if f.sub_mesh_materials {
                sub_elem.string(b"Material", b"")
            } else {
                Vec::new()
            };
            if !string::file_ext(&material).is_empty() {
                self.b.set_sub_mesh_material(sub, &material);
            }
        }

        if let Some(anims) = model.child(b"Animations").filter(|_| f.animations) {
            for anim in anims.children() {
                let mut file = anim.string(b"File", b"");
                let name = anim.string(b"Name", b"");
                let speed = anim.float(b"Speed", 1.0);
                let special = anim.float(b"SpecialEventTime", 0.0);

                if string::file_path(&file).len() <= 1 {
                    file = string::set_file_path(&file, self.p.full_path_dir);
                }

                let Some(state) = self.b.add_animation(&file, &name, speed) else {
                    continue;
                };
                self.b.set_special_event_time(state, special);

                for ev in anim.children() {
                    let time = ev.float(b"Time", 0.0);
                    let ty = anim_event_type(self.b, &ev.string(b"Type", b""));
                    self.b.add_animation_event(
                        state,
                        time,
                        ty,
                        &ev.string(b"Value", b""),
                        &ev.string(b"Name", b""),
                        &ev.string(b"DestSocket", b""),
                    );
                }
            }
        }

        if let Some(ents) = model.child(b"Entities") {
            let mut prefix = self.p.name.to_vec();
            prefix.push(b'_');
            for e in ents.children() {
                let ty = e.value();
                let kind = if ty == b"ParticleSystem" {
                    f.particle_systems.then_some(WorldEntity::ParticleSystem)
                } else if ty == b"Billboard" {
                    f.billboards.then_some(WorldEntity::Billboard)
                } else if ty == b"LensFlare" {
                    Some(WorldEntity::LensFlare)
                } else if ty == b"Sound" {
                    f.sounds.then_some(WorldEntity::Sound)
                } else if string::last_pos(ty, b"Light") > 0 {
                    f.lights.then_some(WorldEntity::Light)
                } else {
                    self.b.error(&msg(&[
                        b"Entity world entity type '",
                        ty,
                        b"' is unknown!\n",
                    ]));
                    None
                };
                let loaded = kind.and_then(|k| self.b.load_world_entity(k, e.id, &prefix));
                if let Some(h) = loaded {
                    let pos = self.b.get_local_position(h);
                    self.b.set_position(h, mul3(pos, scale));
                    entities.push(h);
                }
            }
        }
        self.b.link_light_billboards();

        let mut temp = entities.clone();

        let mut bones: BTreeMap<i32, Handle> = BTreeMap::new();
        if self.skeleton
            && let Some(bones_elem) = model.child(b"Bones")
        {
            for bone_elem in bones_elem.children() {
                let id = bone_elem.int(b"ID", 0);
                let name = bone_elem.string(b"Name", b"");

                let Some(bone) = self.b.get_bone_state(&name) else {
                    self.b.error(&msg(&[
                        b"Could not find bone '",
                        &name,
                        b"' in model '",
                        self.p.full_path,
                        b"'\n",
                    ]));
                    continue;
                };
                bones.entry(id).or_insert(bone);
                self.attach_children(bone_elem, None, Some(bone), &mut temp, &mut bones, false);
            }
        }

        let mut shapes = MultiMap::default();
        let mut used_shapes: Vec<Handle> = Vec::new();
        if self.p.physics_world
            && let Some(shapes_elem) = model.child(b"Shapes")
        {
            for se in shapes_elem.children() {
                let Some(shape) = create_shape(self.b, se, scale) else {
                    continue;
                };
                shapes.insert(se.int(b"ID", 0), shape);
            }
        }

        let mut bodies = MultiMap::default();
        let mut body_list: Vec<Handle> = Vec::new();
        if self.p.physics_world {
            if let Some(bodies_elem) = model.child(b"Bodies") {
                for be in bodies_elem.children() {
                    let id = be.int(b"ID", 0);
                    let body_name = be.string(b"Name", b"");

                    let Some(shape) = body_shape(
                        self.b,
                        be,
                        &mut shapes,
                        &mut used_shapes,
                        f.share_body_shapes,
                    ) else {
                        self.b
                            .error(&msg(&[b"No shapes found for body '", &body_name, b"'\n"]));
                        continue;
                    };

                    let full = [self.p.name, b"_", &body_name].concat();
                    // The C++ dereferences a null body here.
                    let Some(body) = self.b.create_body(&full, shape) else {
                        return false;
                    };
                    self.b
                        .set_body_properties(body, &body_properties(be, scale));
                    self.b.set_body_material(body, &be.string(b"Material", b""));

                    bodies.insert(id, body);
                    body_list.push(body);
                    self.b.register_body(body);

                    self.attach_children(be, Some(body), None, &mut temp, &mut bones, true);
                }
            }

            if self.skeleton {
                if bones.len() != usize::try_from(self.b.get_bone_state_num()).unwrap_or(usize::MAX)
                {
                    self.b.warn_skeleton_mismatch();
                }
                temp.push(ent);
            }

            for &(_, shape) in &shapes.0 {
                if !used_shapes.contains(&shape) {
                    self.b.destroy_shape(shape);
                }
            }
        }

        if let Some(&main) = body_list.first() {
            let inv_parent = matrix_inverse(&self.b.get_local_matrix(main));

            for &e in &temp {
                if self.b.is_sub_mesh(e) && !self.skeleton && self.anim_num > 0 {
                    self.node_animation = true;
                    self.b.set_node_animation();
                    continue;
                }
                self.b.attach_entity_child(main, &inv_parent, e);
            }

            if self.node_animation {
                self.b.attach_entity_child(main, &inv_parent, ent);
            }
        }

        if body_list.is_empty() {
            let m = if self.skeleton {
                matrix_mul(&self.p.transform, &matrix_scale(scale))
            } else {
                self.p.transform
            };
            self.b.set_matrix(ent, &m);
            self.b.update_logic(ent);
        } else {
            for &body in &body_list {
                let local = self.b.get_local_matrix(body);
                self.b
                    .set_matrix(body, &matrix_mul(&self.p.transform, &local));
            }
        }

        let mut rest: BTreeMap<Handle, Matrixf> = BTreeMap::new();
        for &body in &body_list {
            rest.insert(body, self.b.get_world_matrix(body));
        }
        let posed = self.b.before_joints();
        if !posed {
            rest.clear();
        }

        if self.p.physics_world
            && let Some(joints) = model.child(b"Joints")
        {
            for je in joints.children() {
                if let Some(joint) = self.create_joint(je, &bodies, &rest) {
                    self.b.set_joint_properties(joint, &joint_properties(je));
                    self.b.register_joint(joint);
                }
            }
        }

        for &e in &entities {
            self.b.set_unique_id(e, -1);
        }

        if self.b.get_animation_state_num() > 0 && !posed {
            self.b.play_first_animation();
        } else if self.skeleton && !body_list.is_empty() {
            self.b.set_skeleton_physics_active();
        }

        self.b.load_user_variables();
        self.b.after_load();
        true
    }
}

/// Parses the `.ent` rooted at element `root` of `doc`; `false` when the C++ loader returns NULL.
pub fn parse<B: Builder>(b: &mut B, doc: &Doc, root: NodeId, p: &Params) -> bool {
    let mut parse = Parse {
        b,
        p,
        skeleton: false,
        anim_num: 0,
        ent: Handle::new(1).expect("nonzero"),
        node_animation: false,
    };
    parse.run(El { doc, id: root })
}
