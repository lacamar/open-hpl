#ifndef HPL_RUST_PHYSICS_H
#define HPL_RUST_PHYSICS_H

// C ABI of rust/src/physics/ffi.rs; struct layouts and the OhplPhysVt order mirror host.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>
#include "system/SystemTypes.h"
#include "physics/RustPhysBook.h"

struct OhplImpactCounts;

namespace hpl {

struct OhplStr { const char* p; size_t n; };
struct OhplV3 { float x, y, z; };

struct OhplPhysVt
{
	bool (*can_play_impact)(size_t);
	void (*add_impact)(size_t);
	size_t (*pworld_world)(size_t);
	size_t (*joint_world_world)(size_t);
	size_t (*body_world_world)(size_t);
	bool (*world_silent)(size_t);
	bool (*world_sound_exists)(size_t, size_t, int32_t);
	size_t (*world_create_sound)(size_t, OhplStr, OhplStr);
	void (*world_create_impact_ps)(size_t, OhplStr, OhplV3);
	void (*ent_set_saved_false)(size_t);
	void (*ent_set_position)(size_t, OhplV3);
	void (*ent_set_param_named)(size_t, OhplStr, float);
	void (*ent_set_param_index)(size_t, int32_t, float);
	void (*ent_fade_in)(size_t, float);
	void (*ent_fade_out)(size_t, float);
	size_t (*ent_main_entry)(size_t);
	float (*ent_volume)(size_t);
	void (*ent_set_volume)(size_t, float);
	int32_t (*ent_creation_id)(size_t);
	void (*entry_set_speed_mul)(size_t, float);
	void (*entry_set_volume_mul)(size_t, float);
	void (*entry_fade_volume_mul_to)(size_t, float, float);
	int32_t (*surf_impact_count)(size_t);
	float (*surf_impact_min_speed)(size_t, int32_t);
	OhplStr (*surf_impact_sound)(size_t, int32_t);
	OhplStr (*surf_impact_ps)(size_t, int32_t);
	int32_t (*surf_impact_ps_prio)(size_t, int32_t);
	OhplV3 (*body_world_position)(size_t);
	OhplV3 (*body_local_position)(size_t);
	void (*body_set_position)(size_t, OhplV3);
	void (*body_set_scrape_entity)(size_t, size_t);
	void (*body_set_roll_entity)(size_t, size_t);
	void (*body_add_to_update_list)(size_t);
	void (*body_enable_bodies_around)(size_t);
	void (*body_update_roll_effect)(size_t);
	float (*joint_force_size)(size_t);
	float (*joint_angle)(size_t);
	float (*joint_distance)(size_t);
	size_t (*joint_parent)(size_t);
	size_t (*joint_child)(size_t);
	bool (*joint_has_collided)(size_t);
	void (*joint_set_has_collided)(size_t, bool);
	void (*joint_callback_max)(size_t);
	void (*joint_callback_min)(size_t);
	size_t (*joint_get_controller)(size_t, OhplStr);
	int32_t (*joint_controller_count)(size_t);
	size_t (*joint_controller_at)(size_t, int32_t);
	void (*joint_warn_missing_controller)(size_t, size_t);
	OhplV3 (*joint_pivot)(size_t);
	bool (*ctrl_is_active)(size_t);
	void (*ctrl_set_active)(size_t, bool);
	void (*ctrl_deactivate)(size_t);
	int32_t (*ctrl_end_type)(size_t);
	OhplStr (*ctrl_next_name)(size_t);
	void (*ctrl_log)(size_t, float, float, float, float);
	void (*body_local_matrix)(size_t, cMatrixf*);
	void (*body_prev_scrape_matrix)(size_t, cMatrixf*);
	void (*body_set_prev_scrape_matrix)(size_t, const cMatrixf*);
	void (*body_set_matrix)(size_t, const cMatrixf*);
};

struct OhplSurfaceParams
{
	size_t self, physics;
	float minScrapeSpeed, minScrapeFreq, minScrapeFreqSpeed, maxScrapeFreq, maxScrapeFreqSpeed, middleScrapeSpeed;
	int32_t minScrapeContacts;
	float minRollSpeed, minRollFreq, minRollVolume, minRollFreqSpeed, maxRollFreq, maxRollVolume, maxRollFreqSpeed, middleRollSpeed;
	uint32_t rollAxisFlags;
	OhplStr scrapeSound, rollSound;
};

struct OhplImpactDef
{
	float minSpeed;
	OhplStr sound, ps;
	int32_t psPrio;
};

struct OhplSurfaceDef
{
	OhplStr name, userVar, stepType, scrapeSound, rollSound;
	float elasticity, kineticFriction, staticFriction;
	int32_t priority, elasticityMode, frictionMode;
	float minScrapeSpeed, minScrapeFreq, minScrapeFreqSpeed, middleScrapeSpeed, maxScrapeFreqSpeed, maxScrapeFreq;
	int32_t minScrapeContacts;
	float minRollSpeed, minRollFreq, minRollVolume, minRollFreqSpeed, middleRollSpeed, maxRollFreqSpeed, maxRollFreq, maxRollVolume;
	uint32_t rollAxisFlags;
	const OhplImpactDef* impacts; size_t nImpacts;
	const OhplImpactDef* hits; size_t nHits;
};

struct OhplMoveParams
{
	float minSpeed, minFreq, minFreqSpeed, minVolume, maxFreq, maxFreqSpeed, maxVolume, middleSpeed, middleVolume;
	int32_t speedType;
	OhplStr moveSound;
};

struct OhplJointState
{
	size_t parent, child, world;
	cVector3f pivot, pinDir, localPivot, localPinDir;
	cMatrixf prevChild, prevParent;
	size_t sound;
	int32_t soundId, speedCount;
};

struct OhplLimitEffect
{
	OhplStr sound;
	float minSpeed, maxSpeed;
};

struct OhplBodyFilter
{
	bool active, isCharacter, isSkipBody;
	float mass;
	bool collideCharacter, collide;
	int32_t pushStrength;
	bool flagsMatch, queryCollideCharacter, queryIsCharacter, skipStatic;
	int32_t minPushStrength;
};

static_assert(sizeof(OhplSurfaceParams) == 112, "param layout");
static_assert(sizeof(OhplJointState) == 216 && sizeof(OhplMoveParams) == 56 && sizeof(OhplLimitEffect) == 24, "state layout");
static_assert(sizeof(OhplBodyFilter) == 24, "filter layout");

extern "C" {
void ohpl_h15s_params(const OhplSurfaceState*, size_t, size_t, OhplStr, OhplStr, OhplSurfaceParams*);

void ohpl_phys_surface_on_impact(const OhplPhysVt*, const OhplSurfaceParams*, float, const cVector3f*, size_t);
void ohpl_phys_surface_on_slide(const OhplPhysVt*, const OhplSurfaceParams*, float, const cVector3f*, int32_t, size_t, size_t);
void ohpl_phys_surface_create_impact_effect(const OhplPhysVt*, const OhplSurfaceParams*, float, const cVector3f*, size_t, size_t);
void ohpl_phys_surface_update_roll_effect(const OhplPhysVt*, const OhplSurfaceParams*, size_t);

void ohpl_phys_body_velocity_at_position(const OhplPhysVt*, size_t, const cVector3f*, cVector3f*);
void ohpl_phys_body_torque_from_force(const OhplPhysVt*, size_t, const cVector3f*, const cVector3f*, cVector3f*);
bool ohpl_phys_body_update_before(const OhplPhysVt*, size_t, uint64_t);
void ohpl_phys_body_update_after(const OhplPhysVt*, size_t, float, uint64_t);
void ohpl_phys_body_static_linear_move(const OhplPhysVt*, size_t, const cVector3f*, uint64_t);
void ohpl_phys_body_static_angular_move(const OhplPhysVt*, size_t, const cVector3f*, uint64_t);

void ohpl_phys_joint_on_limit(const OhplPhysVt*, size_t, bool, const OhplLimitEffect*);
bool ohpl_phys_joint_limit_auto_sleep(bool, float, int32_t, int32_t*, float, float, float);

}

const OhplPhysVt* OhplPhysHost();

inline OhplStr OhplS(const tString& s) { OhplStr r = { s.data(), s.size() }; return r; }

}

#endif
