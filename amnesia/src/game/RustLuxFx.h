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

enum
{
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
static_assert(sizeof(OhplGa9bCompletion) == 24, "layout");

extern "C"
{

	void ohpl_ga9b_completion_reset(OhplGa9bCompletion *c);
	void ohpl_ga9b_completion_update(OhplGa9bCompletion *c, float dt);
	void ohpl_ga9b_completion_show(OhplGa9bCompletion *c, float previous, float newAmount, float delay);
	int ohpl_ga9b_achievement_unlock_gate(bool registered, bool known);
}

#endif // LUX_RUST_LUX_FX_H
