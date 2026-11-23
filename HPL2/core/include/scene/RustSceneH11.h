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

	static_assert(sizeof(OhplH11MeshCtl) == 32 && offsetof(OhplH11MeshCtl, physics) == 20, "H11 mesh ctl layout");
	static_assert(sizeof(OhplH11SpotCtl) == 16 && offsetof(OhplH11SpotCtl, projection_updated) == 12, "H11 spot ctl layout");
	static_assert(sizeof(OhplH11Caster) == 16 && offsetof(OhplH11Caster, is_static) == 12, "H11 caster layout");

extern "C" {

bool ohpl_h11_rest_pose_from_map(const float*, int32_t, const cMatrixf*, const float*, cMatrixf*, int32_t);

}
}

#endif
