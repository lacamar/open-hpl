#ifndef HPL_RUST_PHYS_BOOK_H
#define HPL_RUST_PHYS_BOOK_H

// H15: Rust-owned iPhysicsBody / iPhysicsJoint bookkeeping (OHPL_IMPL=rust). Tokens are arena handles; 0 means C++ fields.

#include "impl/RustBackends.h"
#include <cstddef>
#include <cstdint>

namespace hpl { struct OhplJointShellState; struct OhplBodyState; }

extern "C" {
uint64_t ohpl_h15_body_new();
void ohpl_h15_body_free(uint64_t);
bool ohpl_h15_body_flag_get(uint64_t, int32_t);
void ohpl_h15_body_flag_set(uint64_t, int32_t, bool);
void ohpl_h15_world_free(uint64_t);
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

extern "C" {
uint64_t ohpl_h15w_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t);
size_t ohpl_h15w_len(uint64_t, size_t);
size_t ohpl_h15w_at(uint64_t, size_t, size_t);
size_t ohpl_h15w_material_len(uint64_t);
bool ohpl_h15w_material_at(uint64_t, size_t, void*, size_t*);

uint64_t ohpl_h15b_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t, hpl::OhplBodyState**, size_t, size_t);
void ohpl_h15b_free(uint64_t);
hpl::OhplJointShellState* ohpl_h15j_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t, size_t, size_t, size_t, size_t, hpl::OhplV3Pod, hpl::OhplV3Pod);
void ohpl_h15j_drop(uint64_t, size_t);
void ohpl_h15j_add_controller(uint64_t, size_t, size_t);
size_t ohpl_h15j_get_controller(uint64_t, const char*, size_t);
bool ohpl_h15j_change_controller(uint64_t, const char*, size_t);
void ohpl_h15j_set_all_paused(uint64_t, bool);
size_t ohpl_h15j_controller_len(uint64_t);
bool ohpl_h15j_controller_at(uint64_t, size_t, void*, size_t*);
bool ohpl_h15j_update(uint64_t, size_t);
bool ohpl_h15j_check_breakage(uint64_t, size_t);

uint64_t ohpl_h15p_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t, hpl::OhplPhysState**);
void ohpl_h15p_free(uint64_t);
size_t ohpl_h15p_surface_len(uint64_t);
bool ohpl_h15p_surface_at(uint64_t, size_t, void*, size_t*);

hpl::OhplSurfaceState* ohpl_h15s_new(size_t (*)(int32_t, hpl::OhplH15Wa*), size_t);
void ohpl_h15s_free(const hpl::OhplSurfaceState*);
size_t ohpl_h15s_to_material(uint64_t, const hpl::OhplSurfaceState*, const char*, size_t, size_t);
}

namespace hpl {

	enum OhplH15BodyFlag : int32_t { OhplH15_HasSlide = 0, OhplH15_HasCollision = 1, OhplH15_StaticMovement = 2, OhplH15_DisableAfterSim = 3 };

}

#endif
