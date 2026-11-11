#ifndef HPL_RUST_CHARACTER_H
#define HPL_RUST_CHARACTER_H

// C ABI of rust/src/physics/ffi_character.rs; layouts mirror character.rs and rope.rs.

#include "math/RustTypes.h"
#include "physics/RustPhysics.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

struct OhplCharState
{
	cVector3f position, lastPosition, lastMovePosAdd, force, velocity, lastGroundNormal, gravityAttachmentVelocity,
		customGravity, size, forward, right, up;
	cMatrixf mtxMove;
	float pitch, yaw, mass, maxGravitySpeed, groundFriction, airFriction, maxNoSlideSlopeCos;
	float groundAngleMinCos = 0.001f;
	float maxStepHeight, maxStepHeightInAir, stepClimbSpeed, climbForwardMul, climbHeightAdd, checkStepClimbCount,
		checkStepClimbInterval, moveDelayCount;
	float maxPosMoveSpeed[2], maxNegMoveSpeed[2], currentMoveAcc[2], moveAcc[2], moveDeacc[2], moveSpeed[2],
		moveSpeedMul[2], moveOppositeDirAccMul[2];
	int32_t onGroundCount, maxOnGroundCount, currentShapeIdx;
	uint32_t collideFlags;
	bool moving[2];
	bool active, testCollision, gravityActive, customGravityActive, deaccelerateMoveSpeedInAir, movedLastUpdate,
		disableDiagSpeedBoost, climbing, accurateClimbing;
};

static_assert(sizeof(OhplCharState) == 364 && offsetof(OhplCharState, mtxMove) == 144 &&
	offsetof(OhplCharState, pitch) == 208 && offsetof(OhplCharState, onGroundCount) == 336 &&
	offsetof(OhplCharState, moving) == 352 && offsetof(OhplCharState, accurateClimbing) == 362, "char state layout");

struct OhplCharVt
{
	void* ctx;
	OhplV3 (*world_gravity)(void*);
	float (*shape_radius)(void*);
	float (*shape_size_y)(void*, int32_t);
	bool (*check_collision)(void*, OhplV3*, OhplV3, int32_t, int32_t);
	bool (*cast_ray)(void*, OhplV3, OhplV3, float*, OhplV3*);
	void (*on_hit_ground)(void*);
	void (*pre_connection)(void*, float);
	void (*clear_gravity_attachment)(void*);
	void (*update_gravity_attachment)(void*);
	void (*update_body)(void*);
	void (*update_character_connection)(void*, float);
	void (*update_body_connection)(void*, float);
	void (*update_camera)(void*);
	void (*update_entity)(void*);
	void (*post_connection)(void*, float);
	void (*update_safe_positions)(void*, float);
};

class iPhysicsBody;
class cVerletParticle;

struct OhplRopeAttachment
{
	iPhysicsBody *mpBody;
	cVerletParticle *mpParticle;
	cVector3f mvBodyLocalPos;
};

struct OhplRopeState
{
	OhplRopeAttachment attached[2];
	cVector3f gravityForce;
	float dampingMul, particleRadius, slideAmount, sleepCheckCount, sleepCheckTime, sleepCheckSqrLimit, motorWantedLength,
		motorSpeedMul, motorMaxSpeed, motorMinSpeed, autoMoveAcc, autoMoveMaxSpeed, autoMoveSpeed, totalLength, minTotalLength,
		maxTotalLength, segmentLength, firstSegmentLength, strength, stiffness;
	int32_t sleepCount, sleepMaxCount, updateCount, maxIterations;
	bool sleeping, collide;
	bool physicsActive = true;
	bool motorActive, autoMoveActive, collideAttachments, hasUpdated;
};

static_assert(sizeof(OhplRopeState) == 184 && sizeof(OhplRopeAttachment) == 32 && offsetof(OhplRopeState, gravityForce) == 64 &&
	offsetof(OhplRopeState, dampingMul) == 76 && offsetof(OhplRopeState, sleepCount) == 156 &&
	offsetof(OhplRopeState, sleeping) == 172 && offsetof(OhplRopeState, hasUpdated) == 178, "rope state layout");

struct OhplRopeVt
{
	void* ctx;
	cVerletParticle* const* (*particles)(void*, size_t*);
	bool (*check_specific_sleeping)(void*);
	bool (*check_specific_awake)(void*);
	void (*set_sleeping)(void*, bool);
	void (*set_total_length)(void*, float);
	void (*update_motor_sound)(void*, float);
	bool (*cast_particle_ray)(void*, OhplV3, OhplV3, OhplV3*, OhplV3*);
	void (*body_local_matrix)(size_t, cMatrixf*);
	OhplV3 (*body_local_position)(size_t);
	OhplV3 (*body_mass_centre)(size_t);
	void (*body_inertia_matrix)(size_t, cMatrixf*);
	float (*body_mass)(size_t);
	void (*body_add_force)(size_t, OhplV3);
	void (*body_add_torque)(size_t, OhplV3);
	OhplV3 (*pid_output)(void*, int32_t, float, float, OhplV3, float);
};

extern "C" {
void ohpl_rope_update_before(const OhplRopeVt*, OhplRopeState*, float);
void ohpl_rope_update_after(const OhplRopeVt*, OhplRopeState*);
void ohpl_rope_update_constraints(const OhplRopeVt*, OhplRopeState*);
void ohpl_verlet_update_movement(cVerletParticle*, float, OhplV3, float);
void ohpl_verlet_length_constraint(cVerletParticle*, cVerletParticle*, float, float);
void ohpl_verlet_length_constraint_range(cVerletParticle*, cVerletParticle*, float, float, float);
void ohpl_verlet_length_constraint_stretch(cVerletParticle*, cVerletParticle*, float, float);
void ohpl_verlet_collision_constraint(bool (*)(void*, OhplV3, OhplV3, OhplV3*, OhplV3*), void*, OhplRopeState*, cVerletParticle*, OhplV3, float);
void ohpl_char_update(const OhplCharVt*, OhplCharState*, float, bool);
void ohpl_char_update_move_matrix(OhplCharState*);
bool ohpl_char_check_fits(const OhplCharVt*, OhplV3, bool, int32_t, float, OhplV3*);
}

}

#endif
