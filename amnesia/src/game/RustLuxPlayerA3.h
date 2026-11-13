#ifndef LUX_RUST_LUX_PLAYER_A3_H
#define LUX_RUST_LUX_PLAYER_A3_H

// C ABI of rust/src/luxplayer/a3_*.rs: the player state family lives in Rust PODs embedded in the C++ state
// classes; engine calls go back through the host table.

#include "RustLuxPlayer.h"

struct OhplA3Io
{
	void *p[3];
	int32_t i[3];
	float f[24];
};

struct OhplA3Host
{
	void *ctx;
	int32_t (*call)(void *ctx, uint32_t op, OhplA3Io *io);
};

enum OhplA3Op : uint32_t
{
	kOhplA3Op_PREV_STATE = 1,
	kOhplA3Op_CHANGE_STATE = 2,
	kOhplA3Op_CHANGE_MOVE_STATE = 3,
	kOhplA3Op_SET_INTERACT_MUL = 4,
	kOhplA3Op_DEFAULT_MASS = 5,
	kOhplA3Op_HAND_DRAWN = 6,
	kOhplA3Op_SET_FOCUS = 7,
	kOhplA3Op_SET_FOCUS_TEXT = 8,
	kOhplA3Op_ICON_STYLE_DEFAULT = 9,
	kOhplA3Op_MOVE_ALLOWS_INTERACTION = 10,
	kOhplA3Op_HANDS_DO_ACTION = 11,
	kOhplA3Op_CUR_MOVE_STATE = 12,
	kOhplA3Op_MOVE_NORMAL_GET = 13,
	kOhplA3Op_MOVE_NORMAL_CMD = 14,
	kOhplA3Op_LANTERN_ACTIVE = 15,
	kOhplA3Op_LANTERN_SET = 16,
	kOhplA3Op_CAM_V3 = 20,
	kOhplA3Op_CAM_ANGLES = 21,
	kOhplA3Op_CAM_PITCH_LIMITS = 22,
	kOhplA3Op_CAM_ADD_PITCH = 23,
	kOhplA3Op_CAM_ADD_YAW = 24,
	kOhplA3Op_CAM_SET_PITCH_LIMITS = 25,
	kOhplA3Op_CAM_SET_YAW_LIMITS = 26,
	kOhplA3Op_CB_V3 = 30,
	kOhplA3Op_CB_SET_POS = 31,
	kOhplA3Op_CB_SET_YAW = 32,
	kOhplA3Op_CB_SET_MASS = 33,
	kOhplA3Op_CB_FLAG = 34,
	kOhplA3Op_CB_CMD = 35,
	kOhplA3Op_CB_FITS = 36,
	kOhplA3Op_CB_RAY = 37,
	kOhplA3Op_CB_MOVE = 38,
	kOhplA3Op_CB_MAX_SPEED = 39,
	kOhplA3Op_CB_SHAPE_COLLIDE = 40,
	kOhplA3Op_CB_EXISTS = 41,
	kOhplA3Op_BODY_MASS = 50,
	kOhplA3Op_BODY_V3 = 51,
	kOhplA3Op_BODY_MATRIX = 52,
	kOhplA3Op_BODY_VEL_AT = 53,
	kOhplA3Op_BODY_FLAG = 54,
	kOhplA3Op_BODY_SET_V3 = 55,
	kOhplA3Op_BODY_SET_MASS = 56,
	kOhplA3Op_BODY_SET_FLAG = 57,
	kOhplA3Op_BODY_ADD = 58,
	kOhplA3Op_BODY_JOINT_NUM = 59,
	kOhplA3Op_BODY_JOINT = 60,
	kOhplA3Op_BODY_USER_DATA = 61,
	kOhplA3Op_JOINT_V3 = 62,
	kOhplA3Op_JOINT_BODY = 63,
	kOhplA3Op_ENT_IS_PROP = 70,
	kOhplA3Op_ENT_MAX_FOCUS = 71,
	kOhplA3Op_ENT_CAN_INTERACT = 72,
	kOhplA3Op_ENT_DISABLED = 73,
	kOhplA3Op_ENT_CROSSHAIR = 74,
	kOhplA3Op_ENT_ON_INTERACT = 75,
	kOhplA3Op_ENT_INTERACT_CB = 76,
	kOhplA3Op_ENT_FOCUS_DRAW = 77,
	kOhplA3Op_PROP_IS_ITEM = 80,
	kOhplA3Op_PROP_SET_INTERACTED = 81,
	kOhplA3Op_PROP_SET_DISABLE_COLL = 82,
	kOhplA3Op_PROP_BODY_NUM = 83,
	kOhplA3Op_PROP_BODY = 84,
	kOhplA3Op_PROP_CONNECTED = 85,
	kOhplA3Op_PROP_EFFECT_MESH = 86,
	kOhplA3Op_PROP_DATA = 87,
	kOhplA3Op_DATA_F = 88,
	kOhplA3Op_MESH_SUB_NUM = 90,
	kOhplA3Op_MESH_SUB_PARENT = 91,
	kOhplA3Op_MESH_SUB_OUTLINE = 92,
	kOhplA3Op_FX_CLEAR_OUTLINE = 93,
	kOhplA3Op_CLOSEST_ENTITY = 100,
	kOhplA3Op_MAP_EXISTS = 101,
	kOhplA3Op_USE_CB_GET = 102,
	kOhplA3Op_USE_CB_RUN = 103,
	kOhplA3Op_USE_CB_REMOVE = 104,
	kOhplA3Op_MESSAGE = 105,
	kOhplA3Op_GUI_DRAW_GFX = 106,
	kOhplA3Op_IN_SMOOTH_MOUSE = 110,
	kOhplA3Op_IN_RESET_SMOOTH = 111,
	kOhplA3Op_IN_SMOOTH_POS = 112,
	kOhplA3Op_IN_INVERT_PITCH = 113,
	kOhplA3Op_IN_ROTATE_TRIGGERED = 114,
	kOhplA3Op_LADDER_V3 = 120,
	kOhplA3Op_LADDER_SOUND = 121,
	kOhplA3Op_CFG_FLOAT = 130,
	kOhplA3Op_DEBUG_ROW = 150,
	kOhplA3Op_VARS_ITEM_TAKE = 140,
	kOhplA3Op_VARS_INTERACT = 141,
	kOhplA3Op_VARS_LADDER = 142,
	kOhplA3Op_PROP_AS_ENTITY = 143,
};

enum OhplA3Crosshair : int32_t
{
	kOhplA3Crosshair_None = -3,
	kOhplA3Crosshair_Default = -1,
	kOhplA3Crosshair_Simple = -2,
	kOhplA3Crosshair_Item = -4,
};

namespace hpl {
class iPhysicsBody;
class iPhysicsJoint;
}
class iLuxEntity;
class iLuxProp;
class cLuxArea_Ladder;
class cLuxInventory_Item;
class cLuxInteractData_Grab;
class cLuxInteractData_Push;
class cLuxInteractData_Slide;
class iLuxInteractData_RotateBase;

struct OhplA3Base
{
	hpl::iPhysicsBody *bodyInFocus;
	iLuxEntity *entityInFocus;
	hpl::iPhysicsBody *curBody;
	iLuxProp *curProp;
	hpl::cVector3f focusPos, curFocusPos;
	float focusDistance;
	int32_t kind;
	bool hasOutline;
	uint8_t pad[7];
	void *ext;
};

struct OhplA3Plain
{
	OhplA3Base base;
};

struct OhplA3UseItem
{
	OhplA3Base base;
	cLuxInventory_Item *item;
	float flash[6];
	float minUseDistance;
	uint32_t pad;
};

struct OhplA3Grab
{
	OhplA3Base base;
	cLuxInteractData_Grab *data;
	float maxForce, maxTorque, maxAngularSpeed, minSlowMass, maxSlowMass, minSlowMul, maxLeaveAngular, maxLeaveLinear;
	hpl::cVector3f localBodyOffset;
	hpl::cMatrixf bodyRotation;
	OhplGa2bPid3 forcePid, torquePid;
	float depth, maxDistance, massSum;
	bool customMaterials;
	uint8_t pad[7];
};

struct OhplA3Push
{
	OhplA3Base base;
	cLuxInteractData_Push *data;
	float totalMoveMul[2];
	float maxSpeedInDir[2];
	OhplGa2bPid3 velocityPid, stopPid;
	hpl::cVector3f localAttach, pushDir;
	float maxAttachDist, walkMaxSpeed, walkForce, runMaxSpeed, runForce, crouchMaxSpeed, crouchForce;
	float pitchMinRange, pitchMaxRange, yawRange, prevMinPitch, prevMaxPitch, maxForce;
	bool moving[2];
	uint8_t pad[2];
};

struct OhplA3Slide
{
	OhplA3Base base;
	cLuxInteractData_Slide *data;
	hpl::iPhysicsJoint *joint;
	OhplGa2bPid3 forcePid;
	float maxDistance;
	hpl::cVector3f localInteractPos;
	hpl::cVector2f mouseAdd, lastMouseAdd;
	hpl::cVector3f lastForce;
	float slideSpeed, moveToMouseFactor, maxForce;
	bool hasGravity;
	uint8_t pad[7];
};

struct OhplA3Rotate
{
	OhplA3Base base;
	iLuxInteractData_RotateBase *data;
	hpl::iPhysicsJoint *joint;
	float moveToMouseFactor, maxTorque, maxDistance;
	hpl::cVector3f jointRight, jointForward, forceAxis[2];
	hpl::cVector2f mouseAdd, lastMouseAdd;
	float rotSpeed;
	OhplGa2bPid3 rotatePid;
	hpl::cVector3f camDir[3];
	float axisCos[2];
	bool hasGravity;
	uint8_t pad[3];
};

struct OhplA3Ladder
{
	OhplA3Base base;
	cLuxArea_Ladder *ladder;
	int32_t state;
	float timeCount;
	hpl::cVector3f startPosition, charPosition, goalPos, goalRot, posAdd, rotAdd;
	float moveMul, upSpeed, downSpeed, stepLength, stepCount;
	float pitchMaxLimit, pitchMinLimit, leaveAtTop;
	bool playedSound, lanternDrawn;
	uint8_t pad[6];
};

static_assert(sizeof(OhplA3Base) == 80, "A3 layout");
static_assert(sizeof(OhplA3UseItem) == 120, "A3 layout");
static_assert(sizeof(OhplA3Grab) == 952, "A3 layout");
static_assert(sizeof(OhplA3Push) == 920, "A3 layout");
static_assert(sizeof(OhplA3Slide) == 528, "A3 layout");
static_assert(sizeof(OhplA3Rotate) == 592, "A3 layout");
static_assert(sizeof(OhplA3Ladder) == 208, "A3 layout");

template<class T> struct OhplA3Holder
{
	T mR = {};
};

extern "C" {
void ohpl_a3_init(OhplA3Base *b, const OhplA3Host *h, int32_t kind);
void ohpl_a3_free(OhplA3Base *b);
void ohpl_a3_enter(OhplA3Base *b, const OhplA3Host *h, int32_t prev);
void ohpl_a3_leave(OhplA3Base *b, const OhplA3Host *h, int32_t newState);
void ohpl_a3_map_leave(OhplA3Base *b);
void ohpl_a3_update(OhplA3Base *b, const OhplA3Host *h, float dt);
void ohpl_a3_post_update(OhplA3Base *b, const OhplA3Host *h, float dt);
bool ohpl_a3_do_action(OhplA3Base *b, const OhplA3Host *h, int32_t action, bool pressed);
void ohpl_a3_scroll(OhplA3Base *b, const OhplA3Host *h, float amount);
bool ohpl_a3_on_move(OhplA3Base *b, const OhplA3Host *h, int32_t dir, float mul);
bool ohpl_a3_add_yaw(OhplA3Base *b, const OhplA3Host *h, float amount);
bool ohpl_a3_add_pitch(OhplA3Base *b, const OhplA3Host *h, float amount);
bool ohpl_a3_on_run(OhplA3Base *b, const OhplA3Host *h, bool pressed);
bool ohpl_a3_on_crouch(OhplA3Base *b, const OhplA3Host *h, bool pressed);
bool ohpl_a3_on_jump(OhplA3Base *b, const OhplA3Host *h, bool pressed);
int32_t ohpl_a3_crosshair(OhplA3Base *b, const OhplA3Host *h);
void ohpl_a3_draw(OhplA3Base *b, const OhplA3Host *h, void *guiSet, float frameTime);
bool ohpl_a3_draw_crosshair(OhplA3Base *b, const OhplA3Host *h, void *gfx, const hpl::cVector3f *pos, const hpl::cVector2f *size);
void ohpl_a3_save_body(OhplA3Base *b, void *body, float *mass, bool *collide);
bool ohpl_a3_allow_buoyancy(OhplA3Base *b, const OhplA3Host *h, void *body);
void ohpl_a3_destroy_entity(OhplA3Base *b, const OhplA3Host *h, void *entity);
void ohpl_a3_attach_sticky(OhplA3Base *b, const OhplA3Host *h, void *body);
float ohpl_a3_draw_debug(OhplA3Base *b, const OhplA3Host *h, void *guiSet, void *font, float y);
}

#endif
