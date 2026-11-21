#ifndef HPL_RUST_MESH_ENT_H
#define HPL_RUST_MESH_ENT_H

// C ABI of rust/src/scene/meshent.rs (cMeshEntity state and control flow).

#include "math/RustTypes.h"
#include "scene/RustMeshEntity.h"
#include "scene/RustSceneH11.h"
#include "scene/RustWm2.h"
#include "graphics/RustMeshMm.h"
#include "graphics/RustGraphicsH6.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplMeshEnt;

	// Thunks for the objects whose classes are still C++ (nodes, bone states, bodies, world,
	// sub entities, animation states); layout mirrors `Fx` in meshent.rs.
	struct OhplMeFx
	{
		const char* (*name)(int32_t, void*, size_t*);
		bool (*n_active)(void*);
		void (*n_update_matrix)(void*);
		void (*n_anim_xform)(void*);
		const cMatrixf* (*n_world)(void*);
		void (*n_update_world)(void*);
		void (*n_set_matrix)(void*, const cMatrixf*, bool);
		const cMatrixf* (*n_local)(void*);
		void* (*n_parent)(void*);
		size_t (*n_child_count)(void*);
		void* (*n_child_at)(void*, size_t);
		void (*n_set_parent)(void*, void*);
		void (*n_add_child)(void*, void*);
		void (*n_ent_children)(void*);
		void* (*new_state)(void*, const char*, size_t, const cMatrixf*);
		void* (*new_root)(void*);
		void (*delete_node)(void*);
		void* (*bs_body)(void*);
		void* (*bs_collider)(void*);
		const cMatrixf* (*bs_inv_body)(void*);
		const cMatrixf* (*bs_body_matrix)(void*);
		const cMatrixf* (*body_world)(void*);
		void (*body_set_active)(void*, bool);
		void (*body_enable)(void*);
		void (*body_zero_vel)(void*);
		void (*body_set_matrix)(void*, const cMatrixf*);
		bool (*collide)(void*, size_t);
		void (*push_pos)(size_t);
		void (*push_num)(size_t, int32_t);
		const cMatrixf* (*ent_world)(void*);
		const cMatrixf* (*ent_parent_world)(void*);
		void (*ent_set_matrix)(void*, const cMatrixf*);
		void (*ent_transform_updated)(void*);
		int32_t (*ent_count)(void*);
		void (*ent_add_node_child)(void*, void*);
		size_t (*ent_node_child_count)(void*);
		void* (*ent_node_child_at)(void*, size_t);
		void (*ent_add_child)(void*, void*);
		void (*ent_remove_child)(void*, void*);
		bool (*bv_flag)(void*);
		void (*bv_clear)(void*);
		void (*bv_from_vertices)(void*);
		void (*bv_set_local)(void*, const cVector3f*, const cVector3f*);
		void (*bv_set_identity)(void*);
		void* (*new_sub)(void*, int32_t, OhplWm2Sub**);
		void (*delete_sub)(void*, void*);
		void (*sub_set_matrix)(void*, const cMatrixf*);
		void (*sub_visible)(void*, bool);
		void (*sub_static)(void*, bool);
		void (*sub_illum)(void*, float);
		void (*sub_coverage)(void*, float);
		void (*sub_liquid)(void*, float);
		void (*sub_auto_liquid)(void*);
		void (*sub_render_flag)(void*, int32_t, bool);
		void (*sub_color_mul)(void*, void*);
		void (*sub_illum_color)(void*, void*);
		void (*sub_bv)(void*, cVector3f*, cVector3f*);
		void (*sub_update_logic)(void*, float);
		void (*sub_transform_updated)(void*);
		void (*sub_set_local_node)(void*, void*);
		void* (*new_anim_state)(void*, void*);
		void* (*add_anim_state)(void*, void*, const char*, size_t);
		OhplAnimCore* (*anim_core)(void*);
		void (*delete_state)(void*);
		void* (*state_anim)(void*);
		int32_t (*anim_track_count)(void*);
		void* (*anim_track)(void*, int32_t);
		const char* (*track_name)(void*, size_t*);
		float (*track_unit)(void*);
		void (*track_apply)(void*, void*, float, float, bool, float);
		int32_t (*ev_count)(void*);
		void* (*ev_at)(void*, int32_t);
		float (*ev_time)(void*);
		bool (*ev_is_sound)(void*);
		bool (*ev_value_empty)(void*);
		bool (*ev_callback)(void*, void*);
		void (*ev_play_sound)(void*, void*);
		void (*after_anim)(void*, float);
		void (*mesh_destroy)(void*);
		void (*report)(void*, int32_t, const char*, size_t);
	};

	const OhplMeFx* OhplMeFxGet();

extern "C" {

OhplMeshEnt* ohpl_me_new(void*, const OhplMeFx*, OhplH11MeshCtl*, const OhplMmMesh*, OhplH6Skel*, const bool*);
void ohpl_me_free(OhplMeshEnt*);
void ohpl_me_update_logic(OhplMeshEnt*, bool, float);
void ohpl_me_update_graphics(OhplMeshEnt*);
const cMatrixf* ohpl_me_bone_matrices(OhplMeshEnt*, int32_t*);
void* ohpl_me_sub(OhplMeshEnt*, uint32_t);
int32_t ohpl_me_sub_num(OhplMeshEnt*);
void* ohpl_me_anim_state(OhplMeshEnt*, int32_t);
int32_t ohpl_me_anim_num(OhplMeshEnt*);
void* ohpl_me_bone(OhplMeshEnt*, int32_t);
int32_t ohpl_me_bone_num(OhplMeshEnt*);
void* ohpl_me_node(OhplMeshEnt*, int32_t);
int32_t ohpl_me_node_num(OhplMeshEnt*);
void* ohpl_me_root(OhplMeshEnt*);
int32_t ohpl_me_bone_index_of(OhplMeshEnt*, void*);
int32_t ohpl_me_anim_index(OhplMeshEnt*, const char*, size_t);
int32_t ohpl_me_bone_index(OhplMeshEnt*, const char*, size_t);
int32_t ohpl_me_node_index(OhplMeshEnt*, const char*, size_t);
int32_t ohpl_me_sub_index(OhplMeshEnt*, const char*, size_t);
void* ohpl_me_add_animation(OhplMeshEnt*, void*, const char*, size_t, float);
void ohpl_me_clear_animations(OhplMeshEnt*);
void ohpl_me_play(OhplMeshEnt*, int32_t, bool, bool);
void ohpl_me_play_fade_to(OhplMeshEnt*, int32_t, bool, float);
void ohpl_me_stop(OhplMeshEnt*);
void ohpl_me_play_name(OhplMeshEnt*, const char*, size_t, bool, float, bool, bool);
bool ohpl_me_anim_is_over(OhplMeshEnt*, const char*, size_t);
void ohpl_me_set_rest_pose(OhplMeshEnt*, const cMatrixf*, size_t);
void ohpl_me_physics_active(OhplMeshEnt*, bool);
void ohpl_me_physics_fade(OhplMeshEnt*, float);
void ohpl_me_colliders_active(OhplMeshEnt*, bool);
void ohpl_me_align_bodies(OhplMeshEnt*);
void ohpl_me_reset_graphics(OhplMeshEnt*);
void ohpl_me_bounding_volume(OhplMeshEnt*);
bool ohpl_me_check_colliders(OhplMeshEnt*, size_t, bool, bool);
void ohpl_me_transform_from_skeleton(OhplMeshEnt*, cMatrixf*, cVector3f*, float*);
void ohpl_me_fan_visible(OhplMeshEnt*, bool);
void ohpl_me_fan_static(OhplMeshEnt*, bool);
void ohpl_me_fan_illum(OhplMeshEnt*, float);
void ohpl_me_fan_coverage(OhplMeshEnt*, float);
void ohpl_me_fan_liquid(OhplMeshEnt*, float);
void ohpl_me_fan_auto_liquid(OhplMeshEnt*);
void ohpl_me_fan_render_flag(OhplMeshEnt*, int32_t, bool);
void ohpl_me_fan_color_mul(OhplMeshEnt*, const void*);
void ohpl_me_fan_illum_color(OhplMeshEnt*, const void*);

}
}

#endif
