#ifndef HPL_RUST_MESH_ENTITY_H
#define HPL_RUST_MESH_ENTITY_H

// C ABI of rust/src/scene/ffi_meshentity.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {
extern "C" {

struct OhplAnimCore
{
	float length, weight, speed, speed_dest, speed_fade_rate, time_pos, prev_time_pos, base_speed,
		special_event_time, fade_step;
	bool active, looping, paused, layer;
};
static_assert(sizeof(OhplAnimCore) == 44 && offsetof(OhplAnimCore, active) == 40, "OhplAnimCore layout");

void ohpl_anim_core_init(OhplAnimCore*, float);
void ohpl_anim_update(OhplAnimCore*, float);
void ohpl_anim_add_time(OhplAnimCore*, float);
void ohpl_anim_set_time(OhplAnimCore*, float);
void ohpl_anim_set_relative_time(OhplAnimCore*, float);
float ohpl_anim_relative_time(const OhplAnimCore*);
bool ohpl_anim_is_over(const OhplAnimCore*);
void ohpl_anim_fade_in(OhplAnimCore*, float);
void ohpl_anim_fade_out(OhplAnimCore*, float);
void ohpl_anim_fade_speed(OhplAnimCore*, float, float);
void ohpl_anim_set_active(OhplAnimCore*, bool);

float ohpl_meshentity_weight_mul(const OhplAnimCore* const*, int32_t, bool);
void ohpl_meshentity_update_anims(OhplAnimCore* const*, int32_t, float);
uint32_t ohpl_meshentity_bone_matrices(const cMatrixf*, const cMatrixf* const*, const cMatrixf* const*, cMatrixf*, int32_t);
void ohpl_meshentity_aabb_from_bones(const cMatrixf* const*, const float*, int32_t, cVector3f*, cVector3f*);
void ohpl_submesh_skin(const float*, const float*, const float*, float*, float*, float*, const float*,
					   const unsigned char*, const cMatrixf*, int32_t, int32_t, int32_t);

}
}

#endif
