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

	struct OhplV3Pod { float x, y, z; };

	// Scalar state of cSurfaceData / cPhysics, owned by rust/src/physics/surf.rs and preg.rs; C++ members are references into it.
	struct OhplSurfaceState
	{
		uint64_t rs;
		int32_t frictionMode, elasticityMode;
		float elasticity, staticFriction, kineticFriction;
		int32_t priority;
		float minScrapeSpeed, minScrapeFreq, minScrapeFreqSpeed, maxScrapeFreq, maxScrapeFreqSpeed, middleScrapeSpeed;
		int32_t minScrapeContacts;
		float minRollSpeed, minRollFreq, minRollVolume, minRollFreqSpeed, maxRollFreq, maxRollVolume, maxRollFreqSpeed, middleRollSpeed;
		int32_t minRollContacts;
		uint32_t rollAxisFlags;
	};

	struct OhplPhysState
	{
		float impactDuration;
		int32_t maxImpacts;
		bool log;
	};

	static_assert(sizeof(OhplH15Wa) == 96 && sizeof(OhplH15Query) == 24 && sizeof(OhplSurfaceState) == 104 && sizeof(OhplPhysState) == 12, "h15 wa layout");

}

struct OhplPid;
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

OhplPid* ohpl_h15c_init(void*, size_t);
uint64_t ohpl_h15b_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t, void*, size_t, size_t);
void ohpl_h15b_free(uint64_t);
void ohpl_h15b_add_joint(uint64_t, size_t);
size_t ohpl_h15b_joint_at(uint64_t, int32_t);
int32_t ohpl_h15b_joint_num(uint64_t);
void ohpl_h15b_remove_joint(uint64_t, size_t);
void ohpl_h15b_add_callback(uint64_t, size_t);
void ohpl_h15b_remove_callback(uint64_t, size_t);
bool ohpl_h15b_on_aabb(uint64_t, size_t, size_t);
void ohpl_h15b_on_collide(uint64_t, size_t, size_t, size_t);
void ohpl_h15b_add_char(uint64_t, size_t);
void ohpl_h15b_remove_char(uint64_t, size_t);
void ohpl_h15b_add_verlet(uint64_t, size_t);
void ohpl_h15b_remove_verlet(uint64_t, size_t);
void ohpl_h15b_destroy(uint64_t, size_t);
uint64_t ohpl_h15j_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t, void*, size_t, size_t, size_t, size_t, hpl::OhplV3Pod, hpl::OhplV3Pod);
void ohpl_h15j_drop(uint64_t, size_t);
void ohpl_h15j_add_controller(uint64_t, size_t, size_t);
size_t ohpl_h15j_get_controller(uint64_t, const char*, size_t);
bool ohpl_h15j_change_controller(uint64_t, const char*, size_t);
void ohpl_h15j_set_all_paused(uint64_t, bool);
size_t ohpl_h15j_controller_len(uint64_t);
bool ohpl_h15j_controller_at(uint64_t, size_t, void*, size_t*);
bool ohpl_h15j_update(uint64_t, size_t);
bool ohpl_h15j_check_breakage(uint64_t, size_t);

uint64_t ohpl_h15p_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t, hpl::OhplPhysState*);
void ohpl_h15p_free(uint64_t);
size_t ohpl_h15p_create_world(uint64_t, bool);
void ohpl_h15p_destroy_world(uint64_t, size_t);
size_t ohpl_h15p_create_surface(uint64_t, const char*, size_t);
size_t ohpl_h15p_get_surface(uint64_t, const char*, size_t);
size_t ohpl_h15p_surface_len(uint64_t);
bool ohpl_h15p_surface_at(uint64_t, size_t, void*, size_t*);
bool ohpl_h15p_can_play(uint64_t, int32_t);
void ohpl_h15p_add_impact(uint64_t);
size_t ohpl_h15p_impact_count(uint64_t);
void ohpl_h15p_update_impacts(uint64_t, float, float);
bool ohpl_h15p_load_surface(uint64_t, const char*, size_t);

void ohpl_h15s_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t, hpl::OhplSurfaceState*);
void ohpl_h15s_free(uint64_t);
void ohpl_h15s_set_f32(hpl::OhplSurfaceState*, int32_t, float);
float ohpl_h15s_get_f32(hpl::OhplSurfaceState*, int32_t);
void ohpl_h15s_set_i32(hpl::OhplSurfaceState*, int32_t, int32_t);
int32_t ohpl_h15s_get_i32(const hpl::OhplSurfaceState*, int32_t);
size_t ohpl_h15s_create_impact(uint64_t, float, bool);
size_t ohpl_h15s_impact_at(uint64_t, bool, int32_t);
int32_t ohpl_h15s_impact_num(uint64_t, bool);
size_t ohpl_h15s_impact_from_speed(uint64_t, bool, float);
void ohpl_h15s_preload(uint64_t);
size_t ohpl_h15s_to_material(uint64_t, const hpl::OhplSurfaceState*, const char*, size_t, size_t);
}

namespace hpl {

	enum OhplH15BodyFlag : int32_t { OhplH15_HasSlide = 0, OhplH15_HasCollision = 1, OhplH15_StaticMovement = 2, OhplH15_DisableAfterSim = 3 };

}

#endif
