#ifndef LUX_RUST_LUX_FX_H
#define LUX_RUST_LUX_FX_H

// C ABI of rust/src/luxfx/: insanity events, music priority stack, progress log lines, plus the
// POD types the effect handler shares with C++ (see RustLuxFxA12a.h). Steps edit POD copies of the
// C++ members and return ordered ops that C++ replays.

#include <cstddef>
#include <cstdint>

struct OhplGa9bOp
{
	uint32_t kind;
	int32_t i;
	float a, b, c, d;
};

struct OhplGa9bOps
{
	uint32_t n;
	OhplGa9bOp ops[24];
};
static_assert(sizeof(OhplGa9bOps) == 580, "OhplGa9bOps layout");

enum OhplGa9bMusicOpKind : uint32_t
{
	kOhplGa9bMusicPlay = 1,
	kOhplGa9bMusicClearSlot = 3,
	kOhplGa9bMusicPlayDanger = 4,
	kOhplGa9bMusicStopDanger = 5,
	kOhplGa9bMusicStopClose = 6,
	kOhplGa9bMusicStopLower = 7,
	kOhplGa9bMusicPlayEnemy = 8,
	kOhplGa9bMusicStopEnemy = 9,
};

enum
{
	kOhplGa9bDangerReturn = 0,
	kOhplGa9bDangerScanIdle = 1,
	kOhplGa9bDangerScanPlaying = 2,
	kOhplGa9bEnemyReturn = 0,
	kOhplGa9bEnemyScan = 1,
	kOhplGa9bEnemyDone = 2,
	kOhplGa9bUnlockOk = 0,
	kOhplGa9bUnlockNotRegistered = 1,
	kOhplGa9bUnlockUnknown = 2,
};

struct OhplGa9bAmount
{
	float amount, goal, speed;
	uint32_t active;
};

struct OhplGa9bFade
{
	float goal, alpha, speed;
	uint32_t active;
};

struct OhplGa9bFlash
{
	float alpha;
	int32_t step;
	float count, inSpeed, whiteSpeed, outSpeed;
	uint32_t active;
};

struct OhplGa9bEmotion
{
	float alpha;
	int32_t step;
	float count, textAlpha, textTime;
	uint32_t active;
};

struct OhplGa9bRow
{
	uint32_t row, align;
	float x, y, z;
};

struct OhplGa9bOutline
{
	uint32_t collides, hasAlpha;
	float min[3], max[3], localMin[3], localMax[3], localCenter[3];
	float world[16];
};

struct OhplGa9bROp
{
	uint32_t kind;
	int32_t i, j;
	float f[16];
};

struct OhplGa9bPOp
{
	uint32_t kind;
	int32_t a, b;
	float f[2];
};

enum OhplGa9bROpKind : uint32_t
{
	kOhplGa9bRDepthFunc = 0,
	kOhplGa9bRDepthTest = 1,
	kOhplGa9bRDepthWrite = 2,
	kOhplGa9bRBlend = 3,
	kOhplGa9bRAlphaMode = 4,
	kOhplGa9bRChannel = 5,
	kOhplGa9bRProgram = 6,
	kOhplGa9bRProgFloat = 7,
	kOhplGa9bRProgColor3 = 8,
	kOhplGa9bRTexDiffuse = 9,
	kOhplGa9bRTexAlpha = 10,
	kOhplGa9bRTexNull = 11,
	kOhplGa9bRTexSlot = 12,
	kOhplGa9bRVertexObj = 13,
	kOhplGa9bRMatrixObj = 14,
	kOhplGa9bRMatrixNull = 15,
	kOhplGa9bRMatrixTemp = 16,
	kOhplGa9bRDraw = 17,
	kOhplGa9bRFrameBuffer = 18,
	kOhplGa9bRClearStencilColor = 19,
	kOhplGa9bRStencilActive = 20,
	kOhplGa9bRStencilAlwaysReplace = 21,
	kOhplGa9bRStencilNotEqualKeep = 22,
	kOhplGa9bRFlatProjection = 23,
	kOhplGa9bRNormalProjection = 24,
	kOhplGa9bRTextureRangeNull = 25,
	kOhplGa9bRScissorActive = 26,
	kOhplGa9bRScissorRect = 27,
	kOhplGa9bRQuadMaxUv = 28,
	kOhplGa9bRQuadMinMax = 29,
};

enum
{
	kOhplGa9bDepthEqual = 0,
	kOhplGa9bDepthLessOrEqual = 1,
	kOhplGa9bBlendNone = 0,
	kOhplGa9bBlendAdd = 1,
	kOhplGa9bAlphaSolid = 0,
	kOhplGa9bAlphaTrans = 1,
	kOhplGa9bChannelRgba = 0,
	kOhplGa9bChannelNone = 1,
	kOhplGa9bProgFlash = 0,
	kOhplGa9bProgGlow = 1,
	kOhplGa9bProgOutlineColor0 = 2,
	kOhplGa9bProgOutlineColor1 = 3,
	kOhplGa9bProgOutlineStencil = 4,
	kOhplGa9bProgOutlineStencilAlpha = 5,
	kOhplGa9bProgBlur0 = 6,
	kOhplGa9bProgBlur1 = 7,
	kOhplGa9bTexOutlineColor = 0,
	kOhplGa9bTexBlur0 = 1,
	kOhplGa9bTexBlur1 = 2,
	kOhplGa9bFbColor = 0,
	kOhplGa9bFbBlur0 = 1,
	kOhplGa9bFbBlur1 = 2,
	kOhplGa9bFbAccum = 3,
};

enum OhplGa9bPOpKind : uint32_t
{
	kOhplGa9bPFlatProjection = 0,
	kOhplGa9bPBlendNone = 1,
	kOhplGa9bPChannelRgba = 2,
	kOhplGa9bPFinalFb = 3,
	kOhplGa9bPTex = 4,
	kOhplGa9bPProgram = 5,
	kOhplGa9bPFloat = 6,
	kOhplGa9bPVec2 = 7,
	kOhplGa9bPQuad1 = 8,
	kOhplGa9bPTextureRangeNull = 9,
};

enum
{
	kOhplGa9bPTexInput = 0,
	kOhplGa9bPTexAmp0 = 1,
	kOhplGa9bPTexZoom = 4,
};

struct OhplGa9bBug
{
	bool active;
	unsigned char pad[3];
	float pos[2], vel[2];
	float sizeMul, angle, alpha;
};

struct OhplGa9bBugsParams
{
	float minSizeMul, maxSizeMul, wanderDist, wanderRadius, swarmMul, maxSpeed;
};

struct OhplGa9bStream
{
	float soundCount, fadeAlpha, fadeAlphaSpeed, fadeOutSpeed;
	uint32_t fadeScreen;
};

struct OhplGa9bSteps
{
	float soundCount, soundCountMax;
	int32_t count;
	float pos[3];
	float timeMul, distMul;
};

struct OhplGa9bPick
{
	int32_t index;
	uint32_t resetUsed;
};

struct OhplGa9bTiming
{
	float maxSanityLong, maxSanityMed, maxSanityShort, timeLong, timeMed, timeShort;
};

struct OhplGa9bSlot
{
	uint32_t hasFile, loop, resume;
	float volume;
};

struct OhplGa9bPlayOut
{
	uint32_t play;
	float volume, fadeSpeed;
	int32_t curPrio;
};

struct OhplGa9bStopOut
{
	uint32_t clear, stop;
	float fadeSpeed;
};

struct OhplGa9bEnemyMusic
{
	uint32_t playing[2];
	float playCount[2], stopCount[2];
	uint32_t closePlaying;
	float closeCount, goneCount;
};

struct OhplGa9bCompletion
{
	uint32_t active;
	float alpha, delay, newAmount, displayed, disableDelay;
};

static_assert(sizeof(OhplGa9bAmount) == 16 && sizeof(OhplGa9bFade) == 16, "layout");
static_assert(sizeof(OhplGa9bFlash) == 28 && sizeof(OhplGa9bEmotion) == 24, "layout");
static_assert(sizeof(OhplGa9bRow) == 20, "layout");
static_assert(sizeof(OhplGa9bOutline) == 132, "layout");
static_assert(sizeof(OhplGa9bROp) == 76 && sizeof(OhplGa9bPOp) == 20, "layout");
static_assert(sizeof(OhplGa9bBug) == 32 && sizeof(OhplGa9bBugsParams) == 24 && sizeof(OhplGa9bStream) == 20, "layout");
static_assert(sizeof(OhplGa9bSteps) == 32 && sizeof(OhplGa9bPick) == 8 && sizeof(OhplGa9bTiming) == 24, "layout");
static_assert(sizeof(OhplGa9bSlot) == 16 && sizeof(OhplGa9bPlayOut) == 16 && sizeof(OhplGa9bStopOut) == 12, "layout");
static_assert(sizeof(OhplGa9bEnemyMusic) == 36 && sizeof(OhplGa9bCompletion) == 24, "layout");

extern "C"
{
	size_t ohpl_ga9b_stream_rows(size_t rows, OhplGa9bRow *out);

	void ohpl_ga9b_bugs_start(OhplGa9bBug *bugs, size_t n, const OhplGa9bBugsParams *p, float minX, float minY, float maxX, float maxY);
	bool ohpl_ga9b_bugs_update(OhplGa9bBug *bugs, size_t total, int count, const OhplGa9bBugsParams *p, float dt, float *time);
	bool ohpl_ga9b_timer_expired(float *time, float dt);
	void ohpl_ga9b_stream_load(float delay, float fadeIn, float fadeOut, float *out3);
	bool ohpl_ga9b_stream_a(OhplGa9bStream *s, bool entryNull, float dt);
	bool ohpl_ga9b_stream_b(OhplGa9bStream *s, bool entryGone, float dt);
	void ohpl_ga9b_steps_start(OhplGa9bSteps *s, float stepTime, int stepNum, float distance);
	bool ohpl_ga9b_steps_update(OhplGa9bSteps *s, float dt, bool *create);
	OhplGa9bPick ohpl_ga9b_insanity_pick(float sanity, size_t n, const uint32_t *setDisabled, uint32_t *used, const float *maxSanity);
	float ohpl_ga9b_insanity_wait_after(const OhplGa9bTiming *t, float sanity);
	float ohpl_ga9b_insanity_count_down(float count, int current, float dt);
	bool ohpl_ga9b_insanity_possible(const OhplGa9bTiming *t, float count, float sanity, int current);

	int ohpl_ga9b_music_prio(int p, int max);
	bool ohpl_ga9b_music_play(int cur, int p, bool sameFile, float volume, float volumeMul, float fadeTime, OhplGa9bPlayOut *out);
	OhplGa9bStopOut ohpl_ga9b_music_stop(int cur, int p, bool hasFile, float slotVolume, float fadeTime);
	int ohpl_ga9b_music_play_highest(const OhplGa9bSlot *slots, size_t n, int maxPrio, OhplGa9bOps *o);
	bool ohpl_ga9b_music_song_over(int cur, bool noSong);
	int ohpl_ga9b_music_pick_danger(const uint32_t *nonEmpty, const int *prios, size_t n);
	int ohpl_ga9b_music_pick_enemy(const uint32_t *nonEmpty, const int *prios, size_t n);
	int ohpl_ga9b_music_danger_begin(bool hard, const OhplGa9bEnemyMusic *e, bool hasMap);
	void ohpl_ga9b_music_danger_idle(OhplGa9bEnemyMusic *e, bool found, float dt, OhplGa9bOps *o);
	void ohpl_ga9b_music_danger_playing(OhplGa9bEnemyMusic *e, bool found, float dt, OhplGa9bOps *o);
	int ohpl_ga9b_music_enemy_begin(OhplGa9bEnemyMusic *e, int t, bool hasMap, bool setEmpty, float dt, OhplGa9bOps *o);
	void ohpl_ga9b_music_enemy_found(OhplGa9bEnemyMusic *e, int t, OhplGa9bOps *o);

	void ohpl_ga9b_completion_reset(OhplGa9bCompletion *c);
	void ohpl_ga9b_completion_update(OhplGa9bCompletion *c, float dt);
	void ohpl_ga9b_completion_show(OhplGa9bCompletion *c, float previous, float newAmount, float delay);
	int ohpl_ga9b_achievement_unlock_gate(bool registered, bool known);
}

#endif // LUX_RUST_LUX_FX_H
