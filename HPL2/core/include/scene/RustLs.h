#ifndef HPL_RUST_LS_H
#define HPL_RUST_LS_H
// C ABI of rust/src/scene/ls_*.rs: Rust-owned state of the rope entity and the environment particles.
// Layouts are asserted here and in Rust.
#include "math/RustTypes.h"
#include "math/RustBv.h"
#include <cstdint>

namespace hpl {
	class cFrustum;
	inline const OhplFrustum* OhplLsFrustum(cFrustum *apFrustum){ return reinterpret_cast<const OhplFrustum*>(apFrustum);}

	struct OhplLsRope
	{
		uintptr_t res, rope, material, vtx;
		cColor color;
		float radius, tileAmount, tileSize;
		int32_t maxSegments, lastUpdateCount;
		bool multiplyAlpha;
	};

	struct OhplLsEnv
	{
		uintptr_t res, gfx, vtx, texture, program[2], spot;
		int32_t programKey[2];
		float boxSize;
		cVector2f particleSize;
		float boxDistance, fadeInStart, fadeInEnd, fadeOutStart, fadeOutEnd, iterationNum;
		int32_t iterationInt;
		cColor color;
		float brightness, spotMul;
		cVector3f gravityVel, windVel, rotateVel;
		float gravitySpeedRand, windSpeedRand, windDirRand, rotateSpeedRand;
		cVector3f iterGravity[30], iterWind[30], iterRotate[30], iterOffset[30], iterRotation[30];
		bool visible, affectedByLight, rotateBothDirs, velDirty, clipActive;
	};

	static_assert(sizeof(OhplLsRope) == 72 && sizeof(OhplLsEnv) == 1992, "LS layout");

	struct OhplLsVar
	{
		const char *name;
		size_t len;
		int32_t value;
		bool hasValue;
	};

	struct OhplLsClip
	{
		bool active;
		OhplBv *bv;
	};

	struct OhplLsSpot
	{
		cColor color;
		float brightness, radius, falloffPow, spotFalloffPow;
		cVector3f pos;
		bool visible;
		cMatrixf viewProj;
	};

	struct OhplLsHost
	{
		void* (*vb_create)(void *apGfx, int32_t alUsage, int32_t alVerts, int32_t alIdx);
		void (*vb_elem)(void *apVb, int32_t alElem, int32_t alComps);
		void (*vb_add)(void *apVb, int32_t alElem, int32_t alComps, const float *apV);
		void (*vb_index)(void *apVb, int32_t alIdx);
		void (*vb_compile)(void *apVb, int32_t alFlags);
		float* (*vb_array)(void *apVb, int32_t alElem);
		void (*vb_update)(void *apVb, int32_t alFlags);
		void (*vb_count)(void *apVb, int32_t alNum);
		void (*vb_free)(void *apVb);
		void (*mat_destroy)(void *apRes, void *apMat);
		void (*rope_info)(void *apRope, int32_t *apOut);
		int32_t (*rope_points)(void *apRope, cVector3f *apOut, int32_t alTake, bool abSmooth);
		const cMatrixf* (*ent_world)(void *apCtx);
		void (*ent_updated)(void *apCtx);
		void (*tex_destroy)(void *apRes, void *apTex);
		void* (*prog_create)(void *apGfx, const char *asName, size_t alLen, const OhplLsVar *apVars, size_t alNum);
		void (*prog_destroy)(void *apGfx, void *apProg);
		void (*prog_var)(void *apProg, const char *asName, size_t alLen, int32_t alId);
		void (*prog_set)(void *apProg, int32_t alId, int32_t alComps, const float *apV);
		void (*ent_clip)(void *apEnt, OhplLsClip *apOut);
		void (*spot_info)(void *apSpot, OhplLsSpot *apOut);
		bool (*light_level)(const cVector3f *apPos, float *apOut);
	};

extern "C" {
void ohpl_ls_set_host(const OhplLsHost*);
OhplLsRope* ohpl_ls_rope_new(uintptr_t, uintptr_t, uintptr_t, int32_t);
void ohpl_ls_rope_free(OhplLsRope*);
void ohpl_ls_rope_set_color(OhplLsRope*, const cColor*);
void ohpl_ls_rope_bounds(OhplLsRope*, OhplBv*);
bool ohpl_ls_rope_viewport(OhplLsRope*, const OhplFrustum*);

OhplLsEnv* ohpl_ls_env_new(uintptr_t, uintptr_t);
void ohpl_ls_env_free(OhplLsEnv*);
void ohpl_ls_env_setup(OhplLsEnv*, float, int32_t, const cVector2f*, const cVector2l*, bool, uintptr_t);
void ohpl_ls_env_set_iteration_num(OhplLsEnv*, float);
void ohpl_ls_env_update(OhplLsEnv*, float);
bool ohpl_ls_env_is_visible(OhplLsEnv*, const OhplFrustum*);
void ohpl_ls_env_add_clip_area(OhplLsEnv*, uintptr_t, bool);
void ohpl_ls_env_remove_clip_area(OhplLsEnv*, uintptr_t);
uintptr_t ohpl_ls_env_program(OhplLsEnv*, bool);
void ohpl_ls_env_setup_base(OhplLsEnv*, uintptr_t, const OhplFrustum*, const float*);
void ohpl_ls_env_setup_iteration(OhplLsEnv*, uintptr_t, int32_t);
}
}
#endif
