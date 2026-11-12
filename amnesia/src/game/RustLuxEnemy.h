#ifndef LUX_RUST_ENEMY_H
#define LUX_RUST_ENEMY_H

// C ABI of rust/src/luxenemy/ffi.rs. OhplGa3Ctx points into the C++ enemy (built once per enemy);
// the enemy state stays in C++ members and Rust edits it in place.

#include <cstddef>
#include <cstdint>

struct OhplGa3Queue;

struct OhplGa3Msg
{
	int ty;
	float time;
	float count;
	float customV[3];
	float customF;
	int customI;
	int state;
};

struct OhplGa3Io
{
	int i[8];
	float f[4];
	float v[3][3];
};

struct OhplGa3Host
{
	void *ctx;
	int (*call)(void *ctx, unsigned op, OhplGa3Io *io);
};

struct OhplGa3Mover
{
	bool turning;
	bool overideMoveState;
	float turnGoalAngle;
	float turnSpeed;
	float turnBreakAcc;
	float stuckCounter;
	float stuckLimit;
	float maxStuckCounter;
	int moveState;
};

struct OhplGa3MoverParams
{
	float turnMinBreakAngle;
	float turnBreakMul;
	float turnSpeedMul;
	float turnMaxSpeed;
	float moveSpeedAnimMul;
	int pose;
	float stoppedToWalk[2];
	float walkToStopped[2];
	float walkToRun[2];
	float runToWalk[2];
};

struct OhplGa3PfWin
{
	int len;
	float v[152];
};

struct OhplGa3Ctx
{
	OhplGa3Queue *queue;
	int *cur;
	int *next;
	int *prev;
	bool *disabled;
	OhplGa3Mover *mover;
	bool *pfMoving;
	float *pfGoal;
	OhplGa3PfWin *pfWin;
	float *lookCount;
	int *losCount;
	bool *canSee;
	bool *detected;
	bool *inRange;
	float *checkDoorCount;
	bool *stuckAtDoor;
	int *stuckDoorId;
	float *lastPosCount;
	float *lastKnown;
	float *fovMul;
	float *sightRange;
	float *health;
	bool *blind;
	float *darknessSightRange;
	float *inDarknessLightLevel;
	float *crouchVisibleRangeMul;
	bool *skipVisibilityHandicaps;
	float *fov;
	float *fovXMul;
	float *activationDistance;
	float *maxRegenHealth;
	float *regenHealthSpeed;
	float *damageCount;
	float *tempPos;
	float *tempVal;
	float *tempValL;
	int *attackHitCounter;
	float *forwardSpeed;
	float *backwardSpeed;
	float *forwardAcc;
	float *forwardDeacc;
	float (*defForwardSpeed)[2][2];
	float (*defBackwardSpeed)[2][2];
	float (*defForwardAcc)[2][2];
	float (*defForwardDeacc)[2][2];
	int *pose;
	float *startPosition;
	float *hearVolume;
	float *playerSearchTime;
	float *normalAttackDistance;
	int *curPatrol;
	bool *isSeenByPlayer;
	bool *autoRemoveAtPathEnd;
	float *waitTime;
	float *alertRunTowardsCount;
	float *groggyDamageCount;
	float *alertToHuntDistance;
	float *alertToInstantHuntDistance;
	float *huntPauseMinTime;
	float *huntPauseMaxTime;
	float *increaseAlertSpeedDistance;
	float *increasedAlertSpeedMul;
	float *alertRunTowardsToHuntLimit;
	float *alertRunTowardsCheckDistance;
	float *idleExtraTimeMin;
	float *idleExtraTimeMax;
	int *idleExtraNum;
};

enum
{
	eOhplGa3Op_StepSize = 1,
	eOhplGa3Op_BodyPos = 2,
	eOhplGa3Op_BodyLastPos = 3,
	eOhplGa3Op_BodyFeet = 4,
	eOhplGa3Op_BodySize = 5,
	eOhplGa3Op_BodyForward = 6,
	eOhplGa3Op_BodyYaw = 7,
	eOhplGa3Op_BodyMoveSpeedFwd = 8,
	eOhplGa3Op_BodyMoveDelay = 9,
	eOhplGa3Op_BodyVelocity = 10,
	eOhplGa3Op_BodyMoveFwd = 11,
	eOhplGa3Op_BodyAddYaw = 12,
	eOhplGa3Op_BodySetSpeeds = 13,
	eOhplGa3Op_BodyBvRadius = 14,
	eOhplGa3Op_PlayAnim = 15,
	eOhplGa3Op_AnimExists = 16,
	eOhplGa3Op_AnimSetSpeed = 17,
	eOhplGa3Op_FadeOutAnim = 18,
	eOhplGa3Op_PlayerDead = 19,
	eOhplGa3Op_PlayerFeet = 20,
	eOhplGa3Op_PlayerPos = 21,
	eOhplGa3Op_PlayerSize = 22,
	eOhplGa3Op_PlayerBvRadius = 23,
	eOhplGa3Op_PlayerLantern = 24,
	eOhplGa3Op_PlayerCrouch = 25,
	eOhplGa3Op_PlayerLightLevel = 26,
	eOhplGa3Op_PlayerAvgSpeed = 27,
	eOhplGa3Op_PlayerVelocity = 28,
	eOhplGa3Op_PlayerTerror = 29,
	eOhplGa3Op_PlayerSetTerror = 30,
	eOhplGa3Op_PlayerAddTerrorEnemy = 31,
	eOhplGa3Op_PlayerRemoveTerrorEnemy = 32,
	eOhplGa3Op_TriggersDisabled = 33,
	eOhplGa3Op_Music = 34,
	eOhplGa3Op_LosRay = 35,
	eOhplGa3Op_EntityAttachPos = 36,
	eOhplGa3Op_DoorBroken = 37,
	eOhplGa3Op_DebugMsg = 38,
	eOhplGa3Op_PlaySound = 39,
	eOhplGa3Op_ChangeSoundState = 40,
	eOhplGa3Op_ActivateSoundAllowed = 41,
	eOhplGa3Op_SetActivateSoundMade = 42,
	eOhplGa3Op_PfMoveTo = 43,
	eOhplGa3Op_PfStop = 44,
	eOhplGa3Op_PfPathLen = 45,
	eOhplGa3Op_PfPathBack = 46,
	eOhplGa3Op_PfPathPop = 47,
	eOhplGa3Op_PfPathClear = 48,
	eOhplGa3Op_NcPresent = 49,
	eOhplGa3Op_NcCount = 50,
	eOhplGa3Op_NcIterBegin = 51,
	eOhplGa3Op_NcIterNode = 52,
	eOhplGa3Op_NcFreePath = 53,
	eOhplGa3Op_NcCommitLast = 54,
	eOhplGa3Op_BvContains = 55,
	eOhplGa3Op_DoorCount = 56,
	eOhplGa3Op_DoorAt = 57,
	eOhplGa3Op_EnemyStateFatal = 60,
	eOhplGa3Op_BodySetActive = 62,
	eOhplGa3Op_PlayerCamPos = 63,
	eOhplGa3Op_PatrolNodePos = 64,
	eOhplGa3Op_PatrolWait = 65,
	eOhplGa3Op_PatrolCount = 67,
	eOhplGa3Op_SetActiveFalse = 100,
	eOhplGa3Op_RunCallback = 101,
	eOhplGa3Op_BroadcastHelp = 102,
	eOhplGa3Op_Attack = 103,
};

// Animation kinds in OhplGa3Io for eOhplGa3Op_PlayAnim.
enum
{
	eOhplGa3Anim_Backward = 0,
	eOhplGa3Anim_Idle = 1,
	eOhplGa3Anim_Walk = 2,
	eOhplGa3Anim_Run = 3,
	eOhplGa3Anim_IdleExtra = 10,
	eOhplGa3Anim_Notice = 11,
	eOhplGa3Anim_BreakDoor = 12,
	eOhplGa3Anim_SwingClaws = 13,
	eOhplGa3Anim_SwingLaunch = 14,
	eOhplGa3Anim_Flinch = 15,
	eOhplGa3Anim_Dead = 16,
};

extern "C" {
typedef bool (*OhplGa3StateCb)(void *ctx, int state, int ev, const OhplGa3Msg *msg);
typedef bool (*OhplGa3DetectedCb)(void *ctx);

OhplGa3Queue *ohpl_ga3_queue_new();
void ohpl_ga3_queue_free(OhplGa3Queue *q);
size_t ohpl_ga3_queue_len(const OhplGa3Queue *q);
bool ohpl_ga3_queue_get(const OhplGa3Queue *q, size_t i, OhplGa3Msg *out);
void ohpl_ga3_queue_clear(OhplGa3Queue *q);
void ohpl_ga3_queue_push_raw(OhplGa3Queue *q, const OhplGa3Msg *m);

void ohpl_ga3_send_message(const OhplGa3Ctx *c, const OhplGa3Host *h, int ty, float time, bool local, const float *v, float f, int i);
void ohpl_ga3_update_state_machine(const OhplGa3Ctx *c, const OhplGa3Host *h, float dt, OhplGa3StateCb cb, void *cbCtx);
bool ohpl_ga3_grunt_event(const OhplGa3Ctx *c, const OhplGa3Host *h, int state, int ev, const OhplGa3Msg *m);
bool ohpl_ga3_grunt_player_is_detected(const OhplGa3Ctx *c, const OhplGa3Host *h);
float ohpl_ga3_grunt_damage_mul(int cur);

void ohpl_ga3_mover_update(OhplGa3Mover *m, const OhplGa3MoverParams *p, float dt, const OhplGa3Host *h);
void ohpl_ga3_mover_update_move_animation(OhplGa3Mover *m, const OhplGa3MoverParams *p, const OhplGa3Host *h);
void ohpl_ga3_mover_turn_to_pos(OhplGa3Mover *m, const float *feet, const OhplGa3Host *h);
void ohpl_ga3_mover_move_to_pos(OhplGa3Mover *m, const float *feet, const OhplGa3Host *h);
void ohpl_ga3_mover_turn_to_angle(OhplGa3Mover *m, float a);
void ohpl_ga3_mover_use_move_state_animations(OhplGa3Mover *m);
float ohpl_ga3_mover_speed_mul(const OhplGa3Mover *m, const OhplGa3MoverParams *p, const OhplGa3Host *h);
float ohpl_ga3_mover_move_speed(const OhplGa3Host *h);
float ohpl_ga3_mover_wanted_speed_amount(const OhplGa3Host *h);

void ohpl_ga3_update_can_see_player(const OhplGa3Ctx *c, const OhplGa3Host *h, float dt);
void ohpl_ga3_update_player_in_range(const OhplGa3Ctx *c, const OhplGa3Host *h);
void ohpl_ga3_update_check_stuck_at_door(const OhplGa3Ctx *c, const OhplGa3Host *h, float dt);
void ohpl_ga3_update_check_last_player_pos(const OhplGa3Ctx *c, const OhplGa3Host *h, float dt);
void ohpl_ga3_update_regen_health(const OhplGa3Ctx *c, const OhplGa3Host *h, float dt);
void ohpl_ga3_update_player_detected(const OhplGa3Ctx *c, const OhplGa3Host *h, OhplGa3DetectedCb cb, void *cbCtx);
void ohpl_ga3_update_char_body(const OhplGa3Ctx *c, const OhplGa3Host *h, const OhplGa3MoverParams *p);
void ohpl_ga3_update_moving(const OhplGa3Ctx *c, const OhplGa3Host *h);

int ohpl_ga3_node_at_pos(const OhplGa3Host *h, const float *pos, float minDist, float maxDist, bool getClosest, bool posFree,
						 bool enemyFree, int skip, int rayNum, bool skipUsed, float *outPos);
void ohpl_ga3_next_goal_pos(const OhplGa3Host *h, const float *goal, float *out);
int ohpl_ga3_search_for_player_node(const OhplGa3Ctx *c, const OhplGa3Host *h, int iters, float a, float b, float d, const float (*p)[4]);
int ohpl_ga3_patrol_around_player_node(const OhplGa3Ctx *c, const OhplGa3Host *h, int iters, float a, float b, float d, const float (*p)[3],
									   const float *start);
bool ohpl_ga3_line_of_sight(const OhplGa3Ctx *c, const OhplGa3Host *h, const float *pos, const float *size, bool checkFov, const float *src);
bool ohpl_ga3_in_fov(const OhplGa3Ctx *c, const OhplGa3Host *h, const float *pos);
bool ohpl_ga3_player_in_fov(const OhplGa3Ctx *c, const OhplGa3Host *h);
float ohpl_ga3_dist_to_player(const OhplGa3Ctx *c, const OhplGa3Host *h);
float ohpl_ga3_dist_to_player_2d(const OhplGa3Ctx *c, const OhplGa3Host *h);
float ohpl_ga3_player_movement_toward_enemy(const OhplGa3Ctx *c, const OhplGa3Host *h);
bool ohpl_ga3_patrol_is_at_last(int cur, int size);
void ohpl_ga3_patrol_inc(int *cur, int size, bool loopAtEnd);
}

// GA3b: ManPig and WaterLurker handlers (rust/src/luxenemy/manpig.rs, waterlurker.rs).
struct OhplGa3bPigCtx
{
	bool *lastShortMiss;
	bool *forceCharge;
	float *fleeInvisibleCount;
	bool *threatenOnAlert;
	bool *fleeFromPlayer;
	bool *autoDisableAfterFlee;
	int *idleBehavior;
	int *patrolMoveSpeed;
	bool *allowZeroWait;
	float *damageMul;
	float *runSpeedMul;
	bool *playActivateSound;
	float *huntPauseTimeMul;
	bool *isTesla;
	float *teslaNotice;
	float *inLanternCount;
	float *checkFlashLight;
	float *lanternSensitivity;
	bool *disableTriggers;
};

struct OhplGa3bWlCtx
{
	float *playerDetectionHeight;
	float *afterAttackPause;
	bool *causesSanityDecrease;
};

enum
{
	eOhplGa3bOp_SeenByPlayer = 200,
	eOhplGa3bOp_InFovAtFeet = 201,
	eOhplGa3bOp_VisibleAtFeet = 202,
	eOhplGa3bOp_TeleportFeet = 203,
	eOhplGa3bOp_BodySetMoveDelay = 204,
	eOhplGa3bOp_PlayHuntSound = 205,
	eOhplGa3bOp_InsidePlayerView = 206,
	eOhplGa3bOp_PatrolNodeInfo = 207,
	eOhplGa3bOp_AttackEx = 208,
	eOhplGa3bOp_ForceTesla = 209,
	eOhplGa3bOp_PlayerYaw = 210,
	eOhplGa3bOp_WlSplash = 220,
	eOhplGa3bOp_WlFoodBite = 221,
	eOhplGa3bOp_WlDebug = 222,
};

// Animation kinds for eOhplGa3Op_PlayAnim; the name is built in C++ with the pose suffix.
enum
{
	eOhplGa3bAnim_IdleExtraPose = 100,
	eOhplGa3bAnim_NoticePose = 101,
	eOhplGa3bAnim_ThreatLoop = 102,
	eOhplGa3bAnim_AttackPose = 103,
	eOhplGa3bAnim_Bite = 104,
	eOhplGa3bAnim_Charge = 105,
	eOhplGa3bAnim_FlinchPose = 106,
	eOhplGa3bAnim_DeadPlain = 107,
	eOhplGa3bAnim_PatrolNode = 108,
};

extern "C" {
bool ohpl_ga3b_pig_event(const OhplGa3Ctx *c, const OhplGa3bPigCtx *p, const OhplGa3Host *h, int state, int ev, const OhplGa3Msg *m);
void ohpl_ga3b_pig_update_lantern(const OhplGa3Ctx *c, const OhplGa3bPigCtx *p, const OhplGa3Host *h, float dt);
bool ohpl_ga3b_wl_event(const OhplGa3Ctx *c, const OhplGa3bWlCtx *w, const OhplGa3Host *h, int state, int ev, const OhplGa3Msg *m);
bool ohpl_ga3b_wl_player_is_detected(const OhplGa3Ctx *c, const OhplGa3bWlCtx *w, const OhplGa3Host *h);
}

#endif
