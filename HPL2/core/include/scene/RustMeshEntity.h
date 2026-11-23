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

}
}

#endif
