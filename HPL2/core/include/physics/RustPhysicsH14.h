#ifndef HPL_RUST_PHYSICS_H14_H
#define HPL_RUST_PHYSICS_H14_H

// C ABI of rust/src/physics/ffi_h14.rs; layouts mirror char_aux.rs (OHPL_IMPL=rust).

#include "math/RustTypes.h"
#include "physics/RustCharacter.h"
#include "physics/RustPhysBook.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

struct OhplH14Aux;

struct OhplH14BodyVt
{
	float (*mass)(size_t);
	void (*worldMatrix)(size_t, cMatrixf*);
	void (*localMatrix)(size_t, cMatrixf*);
	OhplV3 (*worldPosition)(size_t);
	void (*inertia)(size_t, cMatrixf*);
	OhplV3 (*torqueFromForce)(size_t, OhplV3, OhplV3);
	OhplV3 (*velocityAtPosition)(size_t, OhplV3);
	void (*addForce)(size_t, OhplV3);
	void (*addTorque)(size_t, OhplV3);
	bool (*gravityAttachmentRotation)(size_t);
	bool (*gravityAttachmentVelocity)(size_t);
	int32_t (*gravityAttachmentAxes)(size_t);
};

struct OhplH14RopePlan
{
	int32_t wanted;
	float firstSegment;
};

struct OhplH14Conn
{
	size_t body;
	float maxForce, maxTorque, distanceToBody;
	cVector3f posLocalToBody, posLocalToChar, normalLocalToBody, localUpBody, bodyStartRotation, charLocalForward, worldGravity;
	bool dependOnBodyRotation, alignCenter, alignRotation, affectChar, dependOnCharRotation, alignCharRotation;
};

struct OhplH14Grav
{
	size_t attached, lastAttached;
	cMatrixf prevMatrix;
	cVector3f bodyVelocity;
	bool rotateYaw;
};

struct OhplH14CamIn
{
	cVector3f position, right, forward, posAdd;
	float mainHeight, sizeY, yaw;
	int32_t smoothNum;
	bool updateActive, useSmoothing;
};

struct OhplH14CamOut
{
	cVector3f position;
	float yaw;
	bool set;
};

struct OhplH14EntIn
{
	cVector3f position;
	cMatrixf mtxMove, offset, postOffset;
	float yaw;
	int32_t smoothPosNum, smoothYNum;
	bool smoothY;
};

static_assert(sizeof(OhplH14Conn) == 112 && offsetof(OhplH14Conn, posLocalToBody) == 20 && offsetof(OhplH14Conn, dependOnBodyRotation) == 104,
	"h14 conn layout");
static_assert(sizeof(OhplH14Grav) == 96 && offsetof(OhplH14Grav, prevMatrix) == 16 && offsetof(OhplH14Grav, rotateYaw) == 92, "h14 grav layout");
static_assert(sizeof(OhplH14CamIn) == 68 && offsetof(OhplH14CamIn, mainHeight) == 48 && offsetof(OhplH14CamIn, updateActive) == 64,
	"h14 cam layout");
static_assert(sizeof(OhplH14CamOut) == 20 && offsetof(OhplH14CamOut, set) == 16, "h14 cam out layout");
static_assert(sizeof(OhplH14EntIn) == 220 && offsetof(OhplH14EntIn, yaw) == 204 && offsetof(OhplH14EntIn, smoothY) == 216, "h14 ent layout");

extern "C" {
void ohpl_h14_collide_gravity(size_t (*)(int32_t, OhplH15Wa*), size_t, void*, const OhplCharState*, size_t, size_t);
void ohpl_h14_collide_push(size_t (*)(int32_t, OhplH15Wa*), size_t, void*, const OhplCharState*, size_t, size_t);
void ohpl_h14_rope_init(OhplRopeState*, OhplV3, OhplV3, OhplV3);
bool ohpl_h14_rope_check_collision(const OhplRopeState*, size_t);
bool ohpl_h14_rope_check_sleeping(size_t (*)(int32_t, OhplH15Wa*), OhplRopeState*);
bool ohpl_h14_rope_check_awake(size_t (*)(int32_t, OhplH15Wa*), OhplRopeState*);
void ohpl_h14_rope_set_specific_sleeping(size_t (*)(int32_t, OhplH15Wa*), OhplRopeState*, bool);
void ohpl_h14_rope_set_motor(size_t (*)(int32_t, OhplH15Wa*), size_t, OhplRopeState*, bool);
void ohpl_h14_rope_set_auto_move(size_t (*)(int32_t, OhplH15Wa*), size_t, OhplRopeState*, bool);
void ohpl_h14_rope_remove_attached(size_t (*)(int32_t, OhplH15Wa*), size_t, OhplRopeState*, size_t, bool);
void ohpl_h14_rope_set_attached(size_t (*)(int32_t, OhplH15Wa*), size_t, OhplRopeState*, size_t, size_t, size_t);
void ohpl_h14_char_init(void*, OhplCharState*, size_t, float);
void ohpl_h14_connect_to_body(size_t (*)(int32_t, OhplH15Wa*), size_t, void*, OhplCharState*, size_t, OhplV3, uint32_t);
void ohpl_h14_disconnect_body(size_t (*)(int32_t, OhplH15Wa*), size_t, void*);
OhplH14Aux* ohpl_h14_aux_new();
void ohpl_h14_counts(uint64_t*);
void ohpl_h14_rope_plan(float, float, OhplH14RopePlan*);
OhplV3 ohpl_h14_rope_particle_pos(OhplV3, OhplV3, float);
void ohpl_h14_aux_free(OhplH14Aux*);
void ohpl_h14_reset_pids(OhplH14Aux*);
void ohpl_h14_clear_smoothing(OhplH14Aux*);
void ohpl_h14_clear_camera(OhplH14Aux*);
void ohpl_h14_setup_safe_positions(OhplH14Aux*, float, float);
void ohpl_h14_update_safe_positions(OhplH14Aux*, float, OhplV3);
bool ohpl_h14_safe_position(const OhplH14Aux*, float, OhplV3*);
void ohpl_h14_body_connection_pos(const OhplH14BodyVt*, const OhplH14Conn*, OhplV3*);
void ohpl_h14_char_connection_pos(const OhplCharState*, const OhplH14Conn*, OhplV3*);
void ohpl_h14_update_body_connection(OhplH14Aux*, const OhplH14BodyVt*, const OhplCharState*, const OhplH14Conn*, bool, float);
bool ohpl_h14_update_character_connection(OhplH14Aux*, const OhplH14BodyVt*, OhplCharState*, const OhplH14Conn*, float, OhplV3*);
void ohpl_h14_update_gravity_attachment(const OhplH14BodyVt*, OhplCharState*, OhplH14Grav*);
void ohpl_h14_update_camera(OhplH14Aux*, const OhplH14CamIn*, OhplH14CamOut*);
void ohpl_h14_update_entity(OhplH14Aux*, const OhplH14EntIn*, cMatrixf*);
}

}

#endif
