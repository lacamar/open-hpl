#ifndef HPL_RUST_SCENE_H11_H
#define HPL_RUST_SCENE_H11_H

// C ABI of rust/src/scene/ffi_h11.rs. Layouts mirror rust/src/scene/h11.rs.

#include "math/RustTypes.h"
#include "scene/RustMeshEntity.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplH11MeshCtl
	{
		float physics_fade_speed = 1.0f;
		float physics_weight = 1.0f;
		int bone_matrices_transform_count = -1;
		int bone_matrices_version = 0;
		int inv_world_transform_count = -1;
		bool physics = false;
		bool fading = false;
		bool sleeping = false;
		bool can_sleep = true;
		bool colliders = false;
		bool normalize = true;
		bool updated_bones = false;
		bool has_updated_animation = true;
		bool matrices_need_update = true;
	};

	struct OhplH11SpotCtl
	{
		int view_proj_count = -1;
		int view_count = -1;
		int frustum_count = -1;
		bool projection_updated = true;
		bool view_proj_updated = true;
		bool frustum_updated = true;
	};

	struct OhplH11Caster
	{
		uintptr_t object;
		int count;
		bool is_static;
	};

	struct OhplH11ShadowCache;
	struct OhplH11Names;

	struct OhplH11Call
	{
		void *ctx;
		size_t (*call)(void*, int32_t, size_t, size_t, int32_t, float);
	};

	enum OhplH11Op
	{
		OhplH11Op_NodeActive = 0, OhplH11Op_NodeUpdateMatrix, OhplH11Op_NodeAnimXform, OhplH11Op_NodeChildCount,
		OhplH11Op_NodeChild, OhplH11Op_NodeWorld, OhplH11Op_NodeUpdateWorld, OhplH11Op_NodeSetMatrix,
		OhplH11Op_BoneBody, OhplH11Op_BoneCollider, OhplH11Op_BoneInvBody, OhplH11Op_BoneBodyMatrix,
		OhplH11Op_BodyWorld, OhplH11Op_BodySetActive, OhplH11Op_BodyEnable, OhplH11Op_BodyZeroVel,
		OhplH11Op_BodySetMatrix, OhplH11Op_BoneState, OhplH11Op_BoneCount, OhplH11Op_Root, OhplH11Op_ResetGfx,
		OhplH11Op_EntWorld, OhplH11Op_EntParentWorld, OhplH11Op_EntSetMatrix, OhplH11Op_Collide,
		OhplH11Op_PushPos, OhplH11Op_PushNum, OhplH11Op_SubCount, OhplH11Op_SubVisible, OhplH11Op_SubStatic,
		OhplH11Op_SubIllum, OhplH11Op_SubCoverage, OhplH11Op_SubLiquid, OhplH11Op_SubAutoLiquid,
		OhplH11Op_SubRenderFlag, OhplH11Op_SubColorMul, OhplH11Op_SubIllumColor, OhplH11Op_SubBvMin,
		OhplH11Op_SubBvMax, OhplH11Op_BvFlag, OhplH11Op_BvClearFlag, OhplH11Op_BvFromVertices,
		OhplH11Op_BvSetLocal, OhplH11Op_BvSetIdentity, OhplH11Op_BoneRadius, OhplH11Op_HasSkeleton,
		OhplH11Op_Play, OhplH11Op_PlayFade, OhplH11Op_Warn, OhplH11Op_AnimIsOver, OhplH11Op_EvCallback,
		OhplH11Op_EvValueEmpty, OhplH11Op_EvType, OhplH11Op_EvPlaySound, OhplH11Op_ListCount, OhplH11Op_ListAt
	};

	struct OhplH11MeshHost
	{
		void *ctx;
		OhplAnimCore *const *(*cores)(void*, int32_t*);
		int32_t (*state_count)(void*);
		int32_t (*bone_count)(void*);
		bool (*has_skeleton)(void*);
		bool (*has_rest_pose)(void*);
		void (*bone_reset)(void*, int32_t, bool, bool);
		void (*physics_from_bodies)(void*);
		void (*physics_mix)(void*, int32_t, float);
		void (*physics_deactivate)(void*);
		void (*skeleton_apply)(void*, int32_t, float);
		void (*node_apply)(void*, int32_t, float);
		void (*bone_nodes_update)(void*);
		void (*bone_entity_children)(void*);
		void (*colliders_update)(void*);
		void (*sub_transform_updated)(void*);
		void (*node_reset)(void*);
		void (*node_update)(void*);
		void (*node_restore)(void*);
		void (*after_anim)(void*, float);
		void (*sub_update_logic)(void*, float);
		int32_t (*event_count)(void*, int32_t);
		float (*event_time)(void*, int32_t, int32_t);
		void (*handle_event)(void*, int32_t, int32_t);
	};

	static_assert(sizeof(OhplH11MeshCtl) == 32 && offsetof(OhplH11MeshCtl, physics) == 20, "H11 mesh ctl layout");
	static_assert(sizeof(OhplH11SpotCtl) == 16 && offsetof(OhplH11SpotCtl, projection_updated) == 12, "H11 spot ctl layout");
	static_assert(sizeof(OhplH11Caster) == 16 && offsetof(OhplH11Caster, is_static) == 12, "H11 caster layout");

extern "C" {

void ohpl_h11_mesh_update_logic(const OhplH11MeshHost*, OhplH11MeshCtl*, bool, float);
void ohpl_h11_mesh_ctl_init(OhplH11MeshCtl*);
void ohpl_h11_mesh_stop(OhplAnimCore *const*, int32_t);
void ohpl_h11_mesh_play(OhplAnimCore *const*, int32_t, int32_t, bool, bool);
void ohpl_h11_mesh_play_fade_to(OhplAnimCore *const*, int32_t, int32_t, bool, float);
void ohpl_h11_mesh_physics_set(OhplH11MeshCtl*, bool);
bool ohpl_h11_mesh_physics_fade(OhplH11MeshCtl*, float);
uint32_t ohpl_h11_mesh_graphics_gate(OhplH11MeshCtl*, bool, int32_t);
bool ohpl_h11_sub_skin_gate(bool*, int32_t*, int32_t, bool);
bool ohpl_h11_sub_static_null(int32_t*, const cMatrixf*);
bool ohpl_h11_rest_pose_from_map(const float*, int32_t, const cMatrixf*, const float*, cMatrixf*, int32_t);
void ohpl_h11_transform_from_skeleton(const cMatrixf*, const cMatrixf*, cMatrixf*, cVector3f*, float*);
bool ohpl_h11_bounds_union(const cVector3f*, const cVector3f*, int32_t, cVector3f*, cVector3f*);
bool ohpl_h11_light_is_lit(const cColor*, float, bool);
bool ohpl_h11_light_is_visible(bool, float, bool);
void ohpl_h11_light_mask_bounds(const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*, cVector3f*, cVector3f*);
void ohpl_h11_spot_init(OhplH11SpotCtl*);
bool ohpl_h11_spot_view_stale(OhplH11SpotCtl*, int32_t);
bool ohpl_h11_spot_projection_stale(OhplH11SpotCtl*);
bool ohpl_h11_spot_view_proj_stale(const OhplH11SpotCtl*, int32_t);
void ohpl_h11_spot_view_proj_done(OhplH11SpotCtl*, int32_t);
bool ohpl_h11_spot_frustum_stale(const OhplH11SpotCtl*, int32_t);
void ohpl_h11_spot_frustum_done(OhplH11SpotCtl*, int32_t);
OhplH11ShadowCache* ohpl_h11_shadow_cache_new();
void ohpl_h11_shadow_cache_free(OhplH11ShadowCache*);
void ohpl_h11_shadow_cache_add(OhplH11ShadowCache*, uintptr_t, int32_t);
bool ohpl_h11_shadow_cache_valid(const OhplH11ShadowCache*, uintptr_t, int32_t);
bool ohpl_h11_shadow_cache_unchanged(const OhplH11ShadowCache*, const OhplH11Caster*, int32_t);
void ohpl_h11_shadow_cache_set(OhplH11ShadowCache*, const OhplH11Caster*, int32_t);
void ohpl_h11_shadow_cache_clear(OhplH11ShadowCache*);
int32_t ohpl_h11_shadow_cache_len(const OhplH11ShadowCache*);
void ohpl_h11_fog_bounds_size(const cVector3f*, cVector3f*);

void ohpl_h11b_physics_from_bodies(const OhplH11Call*);
void ohpl_h11b_bone_nodes_update(const OhplH11Call*);
void ohpl_h11b_node_update(const OhplH11Call*);
void ohpl_h11b_physics_active(const OhplH11Call*, OhplH11MeshCtl*, bool);
void ohpl_h11b_physics_fade(const OhplH11Call*, OhplH11MeshCtl*, float);
void ohpl_h11b_colliders_active(const OhplH11Call*, OhplH11MeshCtl*, bool);
void ohpl_h11b_align_bodies(const OhplH11Call*);
bool ohpl_h11b_check_colliders(const OhplH11Call*, size_t, bool, bool);
void ohpl_h11b_bounding_volume(const OhplH11Call*);
void ohpl_h11b_animation_event(const OhplH11Call*, size_t);
void ohpl_h11b_fan_out(const OhplH11Call*, int32_t, size_t, size_t, float);
OhplH11Names* ohpl_h11b_names_new();
void ohpl_h11b_names_free(OhplH11Names*);
void ohpl_h11b_names_insert(OhplH11Names*, int32_t, const char*, size_t, int32_t);
int32_t ohpl_h11b_names_find(const OhplH11Names*, int32_t, const char*, size_t);
void ohpl_h11b_play_name(const OhplH11Call*, const OhplH11Names*, const char*, size_t, bool, float, int32_t, bool);
bool ohpl_h11b_anim_is_over(const OhplH11Call*, const OhplH11Names*, const char*, size_t);

}
}

#endif
