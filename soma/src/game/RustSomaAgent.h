#ifndef SOMA_RUST_AGENT_H
#define SOMA_RUST_AGENT_H

// C ABI of rust/src/somaagent/ffi.rs. The POD structs keep the field names of the C++ agent
// classes that derive from them, so script natives read and write them in place.

#include <cstddef>
#include <cstdint>

struct OhplGs2Events;

enum
{
	eOhplGs2Ev_Message = 1,
	eOhplGs2Ev_SetInactive = 2,
	eOhplGs2Ev_AutoDisableCallback = 3,
	eOhplGs2Ev_DoorMessage = 4,
	eOhplGs2Ev_TurnedCallback = 5,
	eOhplGs2Ev_EndOfPathCallback = 6,
	eOhplGs2Ev_ResultCallback = 7,
	eOhplGs2Ev_TrackCallback = 8,
	eOhplGs2Ev_TrackPlayAnim = 9,
};

enum
{
	eOhplGs2Act_Leave = 1,
	eOhplGs2Act_Enter = 2,
	eOhplGs2Act_Update = 3,
	eOhplGs2Act_TimerUp = 4,
	eOhplGs2Act_SubStateOver = 5,
	eOhplGs2Act_Message = 6,
};

struct OhplGs2V3
{
	float x = 0, y = 0, z = 0;
};

struct OhplGs2Event
{
	uint32_t kind;
	int a, b;
	float v[3];
};

struct OhplGs2Action
{
	uint32_t kind;
	bool sub;
	int state, arg_i;
	float arg_f;
	uint64_t arg_u;
};

struct OhplGs2AgentState
{
	bool mbSensesActive = true, mbUpdateDetection = true, mbSeen = false, mbDetected = false, mbSightRangeAffectedByModifiers = true;
	bool mbStaticCollider = false, mbCheckForDoors = true, mbAlignGround = false, mbPosBoneIsFeet = true, mbGlobalSpace = false;
	bool mbGravityBeforeGlobal = true, mbCollisionBeforeGlobal = true, mbAutoDisable = false;
	float mfFOV = 0, mfFOVMul = 1, mfSightRange = 30, mfSightRangeMul = 1, mfEyeHeight = 0.9f;
	float mfDetectMinTime = 0, mfDetectCount = 0, mfDetectTimer = 0, mfCurrentSightDist = 0;
	int mlSeenCount = 0;
	OhplGs2V3 mvLastKnownPlayerPos;
	float mfAlignRayStart = 0.5f, mfAlignRayMax = 0.5f, mfAlignTimer = 0, mfAlignY = 0, mvAlignDist[3] = {};
	int mlAlignCount = 0, mlAlignIdx = 0;
	OhplGs2V3 mvGroundAlignPos;
	float mfPosBoneYOffset = 0;
	float mfMaxDoorDist = 1, mfCheckDoorsCount = 0, mfDoorCheckTimer = 0;
	float mfAutoDisableMinDist = 0, mfAutoDisableTimer = 0, mfAutoDisableCount = 0;
};
static_assert(sizeof(OhplGs2AgentState) == 144 && offsetof(OhplGs2AgentState, mfFOV) == 16 && offsetof(OhplGs2AgentState, mvGroundAlignPos) == 104, "OhplGs2AgentState layout");

struct OhplGs2SoundListener
{
	float mfHearRadius = 30, mfRadiusMul = 1, mfMinRadius = 0, mfMaxRadius = 1000, mfIgnoreRadius = 0, mfMaxPlayerInteractTime = 0;
	int mlMinPrio = 0;
};
static_assert(sizeof(OhplGs2SoundListener) == 28, "OhplGs2SoundListener layout");

struct OhplGs2ProxState
{
	bool mbExists = false, mbActive = false, mbInside = false, mbLOS = false, mbDetectedCheck = false;
	float mfEnter = 0, mfLeave = 0;
};
static_assert(sizeof(OhplGs2ProxState) == 16 && offsetof(OhplGs2ProxState, mfEnter) == 8, "OhplGs2ProxState layout");

struct OhplGs2SpeedState
{
	float mfForward = 1, mfBackward = 1, mfSideways = 1;
	float mfTurnBreakMul = -1, mfTurnSpeedMul = -1, mfTurnMaxSpeed = -1;
	float mfForwardAcc = -1, mfForwardDeacc = -1, mfSidewayAcc = -1, mfSidewayDeacc = -1;
};
static_assert(sizeof(OhplGs2SpeedState) == 40, "OhplGs2SpeedState layout");

struct OhplGs2WallSample
{
	float mvDir[3] = {}, mvPush[3] = {};
	float mfWeight = 0;
	bool mbHit = false;
};
static_assert(sizeof(OhplGs2WallSample) == 32, "OhplGs2WallSample layout");

struct OhplGs2AvoidBody
{
	float center[3];
	float radius, mass;
	bool active, collide, character, player, own;
};

struct OhplGs2TrackNodeInfo
{
	float min_wait, max_wait;
	bool anim_nonempty, loop_anim;
};

struct OhplGs2CharMover
{
	float mfMaxForward = 1, mfMaxBackward = 1;
	float mfTurnMinBreakAngle = 0, mfTurnBreakMul = 0, mfTurnSpeedMul = 3, mfTurnMaxSpeed = 4;
	float mfStoppedToWalk = 0.05f, mfWalkToRun = 3, mfWalkToStopped = 0.025f, mfRunToWalk = 0.8f;
	float mfMoveSpeedAnimMul = 1;
	bool mbUseMoveStateAnims = true, mbMoving = false, mbMoveRequested = false, mbSlowDownAtGoal = false, mb3D = false;
	bool mbTurning = false, mbBanking = false, mbAnimPlaying = false, mbIdleExtraActive = true, mbWallAvoid = false, mbObjectAvoid = false;
	int mlSpeedState = -1;
	float mvGoal[3] = {};
	float mfTurnGoal = 0;
	float mfBankAngleMul = 1, mfBankMaxAngle = 0, mfBankMaxSpeed = 1, mfTurnSpeed = 0, mfTurnSign = 1, mfRoll = 0, mfRollGoal = 0;
	int mlAnimState = -1;
	float mfStuck = 0;
	static constexpr int kStuckFrames = 30;
	float mvStuckDisp[kStuckFrames][3] = {};
	int mlStuckIdx = 0, mlStuckCount = 0;
	float mfIdleExtraMin = 0, mfIdleExtraMax = 0, mfIdleExtraWait = 0, mfIdleExtraLeft = 0;
	int mlIdleExtraCount = 0;
	float mfWallRadius = 0, mfWallSteer = 0, mfWallTimer = 0;
	float mfObjectAvoidDist = 2, mfObjectAvoidMinMass = 4, mfObjectAvoidSteer = 1;
};

struct OhplGs2Pathfinder
{
	static constexpr int kHistory = 152;
	bool mbMoving = false, mbExact = false, mbGoalPending = false, mbCallbackInMap = false;
	bool mbTrackActive = false, mbTrackPaused = false, mbTrackLoop = false, mbAtTrackNode = false;
	float mvGoal[3] = {};
	uint64_t mlPathIdx = 0;
	int mlTrackIdx = -1;
	float mfTrackWait = 0, mfTrackFreq = 1;
	int mlHistoryLen = 0;
	float mvHistory[kHistory] = {};
};

struct OhplGs2SmPod
{
	int mlCur = -1, mlPrev = -1, mlNext = -1;
	int mlSubCur = -1, mlSubPrev = -1, mlSubNext = -1;
	bool mbInSub = false;
	void *rs = nullptr;
};

struct OhplGs2Host
{
	void *ctx;
	bool (*has_body)(void *);
	bool (*has_map)(void *);
	bool (*has_mesh)(void *);
	void (*ent_pos)(void *, float *);
	float (*ent_scale_y)(void *);
	bool (*ent_global_space_anim)(void *);
	void (*body_pos)(void *, float *);
	void (*body_feet)(void *, float *);
	void (*body_size)(void *, float *);
	void (*body_forward)(void *, float *);
	float (*body_yaw)(void *);
	float (*body_pitch)(void *);
	float (*body_entity_pitch_amount)(void *);
	float (*body_move_speed)(void *);
	bool (*body_on_ground)(void *);
	bool (*body_test_collision)(void *);
	bool (*body_gravity_active)(void *);
	float (*body_max_step_size)(void *);
	void (*set_body_yaw)(void *, float);
	void (*set_body_pos)(void *, const float *);
	void (*set_body_feet)(void *, const float *);
	void (*set_body_gravity)(void *, bool);
	void (*set_body_collision)(void *, bool);
	bool (*has_pos_bone)(void *);
	void (*pos_bone_world_pos)(void *, float *);
	void (*set_mesh_matrix)(void *, const float *);
	bool (*player_body_present)(void *);
	bool (*player_dead)(void *);
	float (*player_visibility_range_mul)(void *);
	float (*player_visibility_max_range)(void *);
	void (*player_pos)(void *, float *);
	void (*player_feet)(void *, float *);
	float (*player_size_y)(void *);
	bool (*line_of_sight)(void *, const float *, const float *);
	bool (*entity_in_player_los)(void *);
	float (*floor_dist)(void *, const float *, const float *);
	void (*body_last_pos)(void *, float *);
	void (*body_right)(void *, float *);
	void (*body_velocity)(void *, float, float *);
	void (*body_add_pitch)(void *, float);
	void (*body_set_max_forward)(void *, float);
	void (*body_set_move_acc)(void *, float);
	void (*body_set_move_deacc)(void *, float);
	void (*body_move_forward)(void *);
	bool (*raycast)(void *, const float *, const float *, float *, float *);
	size_t (*avoid_query)(void *, const float *, float);
	void (*avoid_get)(void *, size_t, OhplGs2AvoidBody *);
	int (*anim_current)(void *);
	bool (*anim_name_nonempty)(void *, int);
	void (*play_move_anim)(void *, int);
	void (*set_move_anim_speed)(void *, float);
	void (*idle_restart)(void *);
	bool (*idle_extra_length)(void *, int, float *);
	void (*play_idle_extra)(void *, int);
	void (*pf_feet)(void *, float *);
	size_t (*pf_path_len)(void *);
	void (*pf_path_get)(void *, size_t, float *);
	void (*pf_build_path)(void *, const float *);
	size_t (*pf_track_len)(void *);
	void (*pf_track_info)(void *, size_t, OhplGs2TrackNodeInfo *);
	bool (*pf_track_node_pos)(void *, size_t, float *);
	size_t (*door_count)(void *);
	bool (*door_hit)(void *, size_t, const float *, const float *, float, float *);
};

extern "C" {
OhplGs2Events *ohpl_gs2_events_new();
void ohpl_gs2_events_free(OhplGs2Events *);
size_t ohpl_gs2_events_len(const OhplGs2Events *);
const OhplGs2Event *ohpl_gs2_events_data(const OhplGs2Events *);
void ohpl_gs2_events_clear(OhplGs2Events *);

void ohpl_gs2_agent_pre_update(OhplGs2AgentState *, const OhplGs2Host *, OhplGs2Events *, float dt);
void ohpl_gs2_agent_step_up(OhplGs2AgentState *, const OhplGs2Host *, OhplGs2Events *);
void ohpl_gs2_agent_align(OhplGs2AgentState *, const OhplGs2Host *, OhplGs2Events *, float dt);
void ohpl_gs2_agent_set_senses_active(OhplGs2AgentState *, const OhplGs2Host *, OhplGs2Events *, bool on);
void ohpl_gs2_agent_sync_mesh(OhplGs2AgentState *, const OhplGs2Host *, OhplGs2Events *, const float *mesh_offset16, float roll);
void ohpl_gs2_agent_reset_detection(OhplGs2AgentState *);
bool ohpl_gs2_sound_heard(const OhplGs2SoundListener *, bool active, bool ent_active, const float *agent_pos, const float *pos, float radius, int prio);
void ohpl_gs2_proximity_update(float *timer, OhplGs2ProxState *states, size_t n, const OhplGs2AgentState *agent, bool ai, float dt, const OhplGs2Host *, OhplGs2Events *);
bool ohpl_gs2_bark_due(float *count, float dt, bool active, bool has_sound, float min, float max);

void ohpl_gs2_cm_update(OhplGs2CharMover *, const OhplGs2SpeedState *, OhplGs2WallSample *, size_t n, float dt, const OhplGs2Host *, OhplGs2Events *);
void ohpl_gs2_cm_move_to_pos(OhplGs2CharMover *, const float *pos, bool slow_down);
void ohpl_gs2_cm_stop(OhplGs2CharMover *);
void ohpl_gs2_cm_turn_to(OhplGs2CharMover *, float yaw);
void ohpl_gs2_cm_set_use_move_state_anims(OhplGs2CharMover *, bool);

void ohpl_gs2_pf_update(OhplGs2Pathfinder *, OhplGs2CharMover *, float dt, const OhplGs2Host *, OhplGs2Events *);
void ohpl_gs2_pf_send_goal(OhplGs2Pathfinder *, OhplGs2CharMover *, const OhplGs2Host *);
void ohpl_gs2_pf_move_to(OhplGs2Pathfinder *, OhplGs2CharMover *, const OhplGs2Host *, const float *goal, bool exact, bool in_map);
void ohpl_gs2_pf_stop(OhplGs2Pathfinder *, OhplGs2CharMover *);
void ohpl_gs2_pf_start_track_node(OhplGs2Pathfinder *, OhplGs2CharMover *, const OhplGs2Host *);
void ohpl_gs2_pf_go_to_next_track_node(OhplGs2Pathfinder *, OhplGs2CharMover *, const OhplGs2Host *, OhplGs2Events *);
void ohpl_gs2_pf_on_message(OhplGs2Pathfinder *, OhplGs2CharMover *, int msg, const OhplGs2Host *, OhplGs2Events *);

void ohpl_gs2_sm_init(OhplGs2SmPod *);
void ohpl_gs2_sm_free(OhplGs2SmPod *);
void ohpl_gs2_sm_reset(OhplGs2SmPod *, bool has_states);
void ohpl_gs2_sm_begin_update(OhplGs2SmPod *, float dt);
void ohpl_gs2_sm_begin_message(OhplGs2SmPod *, int msg);
bool ohpl_gs2_sm_next(OhplGs2SmPod *, void *ctx, bool (*exists)(void *, bool sub, int id), OhplGs2Action *out);
void ohpl_gs2_sm_add_timer(OhplGs2SmPod *, bool sub, uint64_t id, float time);
void ohpl_gs2_sm_stop_timer(OhplGs2SmPod *, bool sub, uint64_t id);
bool ohpl_gs2_sm_timer_exists(OhplGs2SmPod *, bool sub, uint64_t id);
size_t ohpl_gs2_sm_timer_len(OhplGs2SmPod *, bool sub);
}

#endif
