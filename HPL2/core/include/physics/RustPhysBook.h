#ifndef HPL_RUST_PHYS_BOOK_H
#define HPL_RUST_PHYS_BOOK_H

// H15: Rust-owned iPhysicsBody / iPhysicsJoint bookkeeping (OHPL_IMPL=rust). Tokens are arena handles; 0 means C++ fields.

#include "impl/RustBackends.h"
#include <cstddef>
#include <cstdint>

extern "C" {
uint64_t ohpl_h15_body_new();
void ohpl_h15_body_free(uint64_t);
bool ohpl_h15_body_flag_get(uint64_t, int32_t);
void ohpl_h15_body_flag_set(uint64_t, int32_t, bool);
uint64_t ohpl_h15_joint_new();
void ohpl_h15_joint_free(uint64_t);
bool ohpl_h15_joint_limit_auto_sleep(uint64_t, bool, float, int32_t, float, float, float);
uint64_t ohpl_h15_world_new();
void ohpl_h15_world_free(uint64_t);
bool ohpl_h15_world_update_insert(uint64_t, size_t);
bool ohpl_h15_world_update_remove(uint64_t, size_t);
void ohpl_h15_world_update_clear(uint64_t);
void ohpl_h15_world_update_before(uint64_t, void*, bool (*)(void*, size_t), void (*)(void*, size_t));
void ohpl_h15_world_update_after(uint64_t, void*, void (*)(void*, size_t));
}

namespace hpl {

	// World list kinds (rust/src/physics/wreg.rs); 6 is a material.
	enum OhplH15Kind : size_t { OhplKind_Shape = 0, OhplKind_Body, OhplKind_Char, OhplKind_Joint, OhplKind_Ctrl, OhplKind_Rope, OhplKind_Material };

	// Argument/result block of OhplH15WorldOp: p = pointer args, f/i = scalars, s/n = returned string, rf/rv = returned scalars.
	struct OhplH15Wa
	{
		size_t p[6];
		float f[2];
		int32_t i[2];
		const char *s;
		size_t n;
		float rf;
		float rv[3];
	};

	struct OhplH15Query
	{
		size_t skipBody;
		uint32_t flags;
		bool collideCharacter, isCharacter, skipStatic;
		int32_t minPush;
	};

	typedef size_t (*OhplH15WorldOp)(int32_t, OhplH15Wa*);
	static_assert(sizeof(OhplH15Wa) == 96 && sizeof(OhplH15Query) == 24, "h15 wa layout");

}

extern "C" {
uint64_t ohpl_h15w_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t);
void ohpl_h15w_add(uint64_t, size_t, size_t);
void ohpl_h15w_add_material(uint64_t, const char*, size_t, size_t);
size_t ohpl_h15w_len(uint64_t, size_t);
size_t ohpl_h15w_at(uint64_t, size_t, size_t);
size_t ohpl_h15w_material_len(uint64_t);
bool ohpl_h15w_material_at(uint64_t, size_t, void*, size_t*);
size_t ohpl_h15w_find_by_name(uint64_t, size_t, const char*, size_t);
void ohpl_h15w_find_and_delete(uint64_t, size_t, size_t);
bool ohpl_h15w_contains(uint64_t, size_t, size_t);
void ohpl_h15w_destroy_shape(uint64_t, size_t);
void ohpl_h15w_destroy_body(uint64_t, size_t);
void ohpl_h15w_destroy_all(uint64_t);
void ohpl_h15w_update_add(uint64_t, size_t);
void ohpl_h15w_update_remove(uint64_t, size_t);
void ohpl_h15w_update(uint64_t, float);
size_t ohpl_h15w_get_material(uint64_t, const char*, size_t);
size_t ohpl_h15w_rope_from_unique_id(uint64_t, int32_t);
void ohpl_h15w_enable_bodies_in_bv(uint64_t, size_t);
void ohpl_h15w_fadeout_all_loop_sounds(uint64_t, float);
void ohpl_h15w_render_contacts(uint64_t, size_t, size_t);
bool ohpl_h15w_shape_world_collision(uint64_t, void*, size_t, const void*, const hpl::OhplH15Query*, size_t);
}

namespace hpl {

	enum OhplH15BodyFlag : int32_t { OhplH15_HasSlide = 0, OhplH15_HasCollision = 1, OhplH15_StaticMovement = 2, OhplH15_DisableAfterSim = 3 };

}

#endif
