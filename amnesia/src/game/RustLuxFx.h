#ifndef LUX_RUST_LUX_FX_H
#define LUX_RUST_LUX_FX_H

// C ABI of rust/src/luxfx/: effect state machines, effect renderer and insanity post effect plans,
// insanity events, music priority stack, progress log lines. Steps edit POD copies of the C++
// members and return ordered ops (or a plan) that C++ replays.

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

enum OhplGa9bOpKind : uint32_t
{
	kOhplGa9bAmountParams = 1,
	kOhplGa9bAmountOff = 2,
	kOhplGa9bEmoFovOut = 3,
	kOhplGa9bEmoUnpause = 4,
	kOhplGa9bEmoRadialOut = 5,
	kOhplGa9bShakeHead = 6,
	kOhplGa9bShakeEnd = 7,

	kOhplGa9bVoiceFadeVoice = 10,
	kOhplGa9bVoiceVoiceNull = 11,
	kOhplGa9bVoiceFadeEffect = 12,
	kOhplGa9bVoiceEffectNull = 13,
	kOhplGa9bVoicePopFront = 14,
	kOhplGa9bVoicePushFront = 15,
	kOhplGa9bVoicePushBack = 16,
	kOhplGa9bVoiceVolVoice = 17,
	kOhplGa9bVoiceVolEffect = 18,
	kOhplGa9bVoiceClearText = 19,
	kOhplGa9bVoiceWrapNow = 20,
	kOhplGa9bVoiceQueue = 21,
};

enum
{
	kOhplGa9bVoiceReturn = 0,
	kOhplGa9bVoiceFinalize = 1,
	kOhplGa9bVoicePlay = 2,
	kOhplGa9bUnlockOk = 0,
	kOhplGa9bUnlockNotRegistered = 1,
	kOhplGa9bUnlockUnknown = 2,
	kOhplGa9bAlignLeft = 0,
	kOhplGa9bAlignCenter = 1,
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

struct OhplGa9bShake
{
	float maxSize, size, time, fadeInTime, maxFadeInTime, fadeOutTime, maxFadeOutTime;
};

struct OhplGa9bRow
{
	uint32_t row, align;
	float x, y, z;
};

struct OhplGa9bVoiceAdd
{
	uint32_t active, listEmpty;
	int32_t frontPriority, priority;
	uint32_t removeInterrupted, voiceValid, effectValid;
};

struct OhplGa9bVoiceUpdate
{
	float volumeMul;
	uint32_t voiceNonNull, voiceValid, effectNonNull, effectValid, listEmpty;
};

struct OhplGa9bVoiceTexts
{
	uint32_t nonEmptyMask;
	float delays[7];
	uint32_t listSize, voiceNull, effectNull;
};

struct OhplGa9bGlow
{
	uint32_t collides;
	float alpha;
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

struct OhplGa9bRPlan;
struct OhplGa9bPPlan;

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

struct OhplGa9bClock
{
	float t, animCount, waveAlpha, zoomAlpha, waveSpeed;
};

struct OhplGa9bCompletion
{
	uint32_t active;
	float alpha, delay, newAmount, displayed, disableDelay;
};

static_assert(sizeof(OhplGa9bAmount) == 16 && sizeof(OhplGa9bFade) == 16, "layout");
static_assert(sizeof(OhplGa9bFlash) == 28 && sizeof(OhplGa9bEmotion) == 24 && sizeof(OhplGa9bShake) == 28, "layout");
static_assert(sizeof(OhplGa9bRow) == 20 && sizeof(OhplGa9bVoiceAdd) == 28 && sizeof(OhplGa9bVoiceUpdate) == 24, "layout");
static_assert(sizeof(OhplGa9bVoiceTexts) == 44 && sizeof(OhplGa9bGlow) == 8 && sizeof(OhplGa9bOutline) == 132, "layout");
static_assert(sizeof(OhplGa9bROp) == 76 && sizeof(OhplGa9bPOp) == 20 && sizeof(OhplGa9bClock) == 20, "layout");
static_assert(sizeof(OhplGa9bCompletion) == 24, "layout");

extern "C"
{
	void ohpl_ga9b_amount_update(OhplGa9bAmount *s, float dt, OhplGa9bOps *o);
	void ohpl_ga9b_fade_in(OhplGa9bFade *s, float t);
	void ohpl_ga9b_fade_out(OhplGa9bFade *s, float t);
	void ohpl_ga9b_fade_direct(OhplGa9bFade *s, float x);
	bool ohpl_ga9b_fade_is_fading(const OhplGa9bFade *s);
	void ohpl_ga9b_fade_update(OhplGa9bFade *s, float dt);
	void ohpl_ga9b_fade_reset(OhplGa9bFade *s);
	void ohpl_ga9b_flash_start(OhplGa9bFlash *s, float fadeIn, float white, float fadeOut);
	void ohpl_ga9b_sanity_flash_start(OhplGa9bFlash *s, float fadeInTime, float fadeOutTime);
	void ohpl_ga9b_flash_reset(OhplGa9bFlash *s);
	void ohpl_ga9b_flash_update(OhplGa9bFlash *s, float dt);
	void ohpl_ga9b_emotion_start(OhplGa9bEmotion *s, size_t len);
	void ohpl_ga9b_emotion_update(OhplGa9bEmotion *s, float dt, OhplGa9bOps *o);
	void ohpl_ga9b_emotion_action(OhplGa9bEmotion *s, bool pressed);
	bool ohpl_ga9b_shake_update(OhplGa9bShake *shakes, uint32_t *n, float dt, OhplGa9bOps *o);
	size_t ohpl_ga9b_voice_rows(size_t rows, float rowWidth, float fontY, OhplGa9bRow *out);
	size_t ohpl_ga9b_stream_rows(size_t rows, OhplGa9bRow *out);
	size_t ohpl_ga9b_emotion_rows(size_t rows, float fontY, OhplGa9bRow *out);
	void ohpl_ga9b_voice_add(const OhplGa9bVoiceAdd *i, OhplGa9bOps *o);
	size_t ohpl_ga9b_voice_text_tick(float *delays, size_t n, float dt);
	int ohpl_ga9b_voice_update_a(const OhplGa9bVoiceUpdate *i, OhplGa9bOps *o);
	double ohpl_ga9b_voice_start_time(double current, double interruptedAt, double gameTime);
	bool ohpl_ga9b_voice_update_b(const OhplGa9bVoiceTexts *i, OhplGa9bOps *o);

	OhplGa9bRPlan *ohpl_ga9b_plan_flash(const OhplGa9bGlow *objs, size_t n, float osc, bool ok);
	OhplGa9bRPlan *ohpl_ga9b_plan_glow(const OhplGa9bGlow *objs, size_t n, bool ok);
	OhplGa9bRPlan *ohpl_ga9b_plan_outline(const OhplGa9bOutline *objs, size_t n, uint32_t programsOk);
	bool ohpl_ga9b_rplan_next(OhplGa9bRPlan *p, OhplGa9bROp *out);
	void ohpl_ga9b_rplan_clip(const OhplGa9bRPlan *p, float *min3, float *max3);
	void ohpl_ga9b_rplan_free(OhplGa9bRPlan *p);

	void ohpl_ga9b_post_update(OhplGa9bClock *c, float dt);
	OhplGa9bPPlan *ohpl_ga9b_post_plan(const OhplGa9bClock *c, float sx, float sy, bool ok);
	bool ohpl_ga9b_pplan_next(OhplGa9bPPlan *p, OhplGa9bPOp *out);
	void ohpl_ga9b_pplan_free(OhplGa9bPPlan *p);

	void ohpl_ga9b_completion_reset(OhplGa9bCompletion *c);
	void ohpl_ga9b_completion_update(OhplGa9bCompletion *c, float dt);
	void ohpl_ga9b_completion_show(OhplGa9bCompletion *c, float previous, float newAmount, float delay);
	int ohpl_ga9b_achievement_unlock_gate(bool registered, bool known);
}

#endif // LUX_RUST_LUX_FX_H
