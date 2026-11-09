#ifndef HPL_RUST_ENTITY_LOADER_H
#define HPL_RUST_ENTITY_LOADER_H
// C ABI of rust/src/entity_loader/ffi.rs; layouts are asserted on the Rust side.
#include <cstddef>
#include <cstdint>

struct OhplXmlDoc;

struct OhplEntStr { const char* p; size_t n; };
struct OhplEntVec3 { float x, y, z; };
struct OhplEntMat { float m[4][4]; };

struct OhplEntParams
{
	OhplEntStr name, file_name, full_path, full_path_dir;
	OhplEntMat transform;
	OhplEntVec3 scale;
	bool physics_world;
};

struct OhplEntFlags
{
	bool animations, particle_systems, billboards, sounds, lights;
	bool sub_mesh_scale_includes_model_scale, sub_mesh_instance_scale_after_rotation, sub_mesh_materials, share_body_shapes;
};

struct OhplEntBodyProps
{
	OhplEntMat matrix;
	float mass, angular_damping, linear_damping, buoyancy_density_mul, max_angular_speed, max_linear_speed;
	bool blocks_sound, collide_character, collide, gravity, continuous_collision, pushed_by_character_gravity;
	bool volatile_, use_surface_effects, can_attach_character;
	int id;
};

struct OhplEntJointProps
{
	OhplEntStr move_sound, break_sound, max_limit_sound, min_limit_sound;
	float floats[15];
	bool bools[6];
	int ints[2];
};

typedef uintptr_t OhplEntRaw;

static_assert(sizeof(OhplEntParams) == 144 && sizeof(OhplEntFlags) == 9 && sizeof(OhplEntBodyProps) == 104 && sizeof(OhplEntJointProps) == 144,
	"layout of rust/src/entity_loader/ffi.rs");

struct OhplEntVt
{
	void* ctx;
	void (*error)(void*, OhplEntStr) noexcept;
	void (*warning)(void*, OhplEntStr) noexcept;
	void (*before_load)(void*) noexcept;
	void (*after_load)(void*) noexcept;
	bool (*before_joints)(void*) noexcept;
	void (*flags)(void*, OhplEntFlags*) noexcept;
	bool (*create_mesh)(void*, OhplEntStr, bool*, int*) noexcept;
	OhplEntRaw (*create_mesh_entity)(void*, OhplEntStr) noexcept;
	void (*set_matrix)(void*, OhplEntRaw, const OhplEntMat*) noexcept;
	void (*set_shadow_caster)(void*, OhplEntRaw) noexcept;
	void (*update_logic)(void*, OhplEntRaw) noexcept;
	OhplEntRaw (*get_sub_mesh_entity)(void*, OhplEntStr) noexcept;
	void (*warn_missing_sub_mesh)(void*, OhplEntStr) noexcept;
	OhplEntVec3 (*get_sub_mesh_model_scale)(void*, OhplEntRaw) noexcept;
	void (*set_world_matrix)(void*, OhplEntRaw, const OhplEntMat*) noexcept;
	void (*set_unique_id)(void*, OhplEntRaw, int) noexcept;
	void (*set_sub_mesh_material)(void*, OhplEntRaw, OhplEntStr) noexcept;
	OhplEntRaw (*add_animation)(void*, OhplEntStr, OhplEntStr, float) noexcept;
	void (*set_special_event_time)(void*, OhplEntRaw, float) noexcept;
	void (*add_animation_event)(void*, OhplEntRaw, float, int, OhplEntStr, OhplEntStr, OhplEntStr) noexcept;
	OhplEntRaw (*load_world_entity)(void*, int, unsigned, OhplEntStr) noexcept;
	OhplEntVec3 (*get_local_position)(void*, OhplEntRaw) noexcept;
	void (*set_position)(void*, OhplEntRaw, OhplEntVec3) noexcept;
	void (*link_light_billboards)(void*) noexcept;
	OhplEntRaw (*get_bone_state)(void*, OhplEntStr) noexcept;
	int (*get_bone_state_num)(void*) noexcept;
	void (*warn_skeleton_mismatch)(void*) noexcept;
	void (*get_bone_world_matrix)(void*, OhplEntRaw, OhplEntMat*) noexcept;
	OhplEntStr (*get_bone_name)(void*, OhplEntRaw) noexcept;
	int (*get_unique_id)(void*, OhplEntRaw) noexcept;
	bool (*is_sub_mesh)(void*, OhplEntRaw) noexcept;
	void (*get_local_matrix)(void*, OhplEntRaw, OhplEntMat*) noexcept;
	void (*get_world_matrix)(void*, OhplEntRaw, OhplEntMat*) noexcept;
	OhplEntStr (*get_entity_name)(void*, OhplEntRaw) noexcept;
	void (*set_node_animation)(void*) noexcept;
	void (*attach_entity_child)(void*, OhplEntRaw, const OhplEntMat*, OhplEntRaw) noexcept;
	void (*attach_bone_child)(void*, OhplEntRaw, const OhplEntMat*, OhplEntRaw) noexcept;
	void (*attach_bone_to_body)(void*, OhplEntRaw, const OhplEntMat*, OhplEntRaw) noexcept;
	OhplEntRaw (*create_box_shape)(void*, OhplEntVec3, const OhplEntMat*) noexcept;
	OhplEntRaw (*create_sphere_shape)(void*, OhplEntVec3, const OhplEntMat*) noexcept;
	OhplEntRaw (*create_cylinder_shape)(void*, float, float, const OhplEntMat*) noexcept;
	OhplEntRaw (*create_capsule_shape)(void*, float, float, const OhplEntMat*) noexcept;
	OhplEntRaw (*create_compound_shape)(void*, const OhplEntRaw*, size_t) noexcept;
	void (*inc_shape_user_count)(void*, OhplEntRaw) noexcept;
	void (*destroy_shape)(void*, OhplEntRaw) noexcept;
	OhplEntRaw (*create_body)(void*, OhplEntStr, OhplEntRaw) noexcept;
	void (*set_body_properties)(void*, OhplEntRaw, const OhplEntBodyProps*) noexcept;
	void (*set_body_material)(void*, OhplEntRaw, OhplEntStr) noexcept;
	void (*register_body)(void*, OhplEntRaw) noexcept;
	OhplEntRaw (*create_joint_hinge)(void*, OhplEntStr, OhplEntVec3, OhplEntVec3, OhplEntRaw, OhplEntRaw) noexcept;
	OhplEntRaw (*create_joint_ball)(void*, OhplEntStr, OhplEntVec3, OhplEntVec3, OhplEntRaw, OhplEntRaw) noexcept;
	OhplEntRaw (*create_joint_slider)(void*, OhplEntStr, OhplEntVec3, OhplEntVec3, OhplEntRaw, OhplEntRaw) noexcept;
	OhplEntRaw (*create_joint_screw)(void*, OhplEntStr, OhplEntVec3, OhplEntVec3, OhplEntRaw, OhplEntRaw) noexcept;
	void (*set_hinge_limits)(void*, OhplEntRaw, float, float) noexcept;
	void (*set_ball_limits)(void*, OhplEntRaw, float, float) noexcept;
	void (*set_slider_limits)(void*, OhplEntRaw, float, float) noexcept;
	void (*set_screw_limits)(void*, OhplEntRaw, float, float) noexcept;
	void (*set_joint_properties)(void*, OhplEntRaw, const OhplEntJointProps*) noexcept;
	void (*register_joint)(void*, OhplEntRaw) noexcept;
	int (*get_animation_state_num)(void*) noexcept;
	void (*play_first_animation)(void*) noexcept;
	void (*set_skeleton_physics_active)(void*) noexcept;
	void (*load_user_variables)(void*) noexcept;
};

extern "C" bool ohpl_ent_parse(const OhplXmlDoc* doc, unsigned root, const OhplEntParams* params, const OhplEntVt* vt);

#endif
