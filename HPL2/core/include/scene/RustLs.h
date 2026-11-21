#ifndef HPL_RUST_LS_H
#define HPL_RUST_LS_H
// C ABI of rust/src/scene/ls_*.rs: Rust-owned state of the rope entity and the environment particles.
// Layouts are asserted here and in Rust.
#include "math/RustTypes.h"
#include "math/RustBv.h"
#include "scene/RustSL.h"
#include <cstdint>

namespace hpl {
	class cFrustum;
	struct OhplEntPod;
	struct OhplRnd;
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
		void (*ent_updated)(void *apCtx, bool abCallbacks);
		void (*tex_destroy)(void *apRes, void *apTex);
		void* (*prog_create)(void *apGfx, const char *asName, size_t alLen, const OhplLsVar *apVars, size_t alNum);
		void (*prog_destroy)(void *apGfx, void *apProg);
		void (*prog_var)(void *apProg, const char *asName, size_t alLen, int32_t alId);
		void (*prog_set)(void *apProg, int32_t alId, int32_t alComps, const float *apV);
		void (*ent_clip)(void *apEnt, OhplLsClip *apOut);
		void (*spot_info)(void *apSpot, OhplLsSpot *apOut);
		bool (*light_level)(const cVector3f *apPos, float *apOut);
		bool (*hdr)();
		void (*ent_pos)(void *apCtx, cVector3f *apOut);
		void (*occ_assign)(void *apRenderer, void *apCtx, int32_t alIdx, void *apVb, const cMatrixf *a_mtx, bool abFlag);
		int32_t (*occ_samples)(void *apRenderer, void *apCtx, int32_t alIdx);
		const OhplFrustum* (*renderer_frustum)(void *apRenderer);
		void (*log)(bool abError, const char *asMsg, size_t alLen);
		void* (*xml_open)(void *apRes, const char *asFile, size_t alLen, int32_t *apStatus);
		const char* (*xml_attr)(void *apDoc, const char *asName, size_t alLen);
		void (*xml_close)(void *apDoc);
		void* (*mat_create)(void *apRes, const char *asName, size_t alLen);
		OhplBv* (*ent_bv)(void *apCtx);
		void* (*flare_type_new)(void *apCtx, OhplSLFlare *apFlare, int32_t alKind);
		void (*flare_type_delete)(void *apType);
		bool (*flare_parent_mesh)(void *apCtx, void **apVb, const cMatrixf **apMtx);
		int32_t (*vb_vertex_num)(void *apVb);
	};

extern "C" {
void ohpl_ls_set_host(const OhplLsHost*);
OhplLsRope* ohpl_ls_rope_new(uintptr_t, uintptr_t, uintptr_t, int32_t);
void ohpl_ls_rope_free(OhplLsRope*);
void ohpl_ls_rope_set_color(OhplLsRope*, const cColor*);
void ohpl_ls_rope_bounds(OhplLsRope*, OhplBv*);
bool ohpl_ls_rope_viewport(OhplLsRope*, const OhplFrustum*);

OhplSLBb* ohpl_ls_bb_new(void*, OhplEntPod*, OhplBv*, uintptr_t, uintptr_t, int32_t, const cVector2f*);
void ohpl_ls_bb_free(OhplSLBb*);
void ohpl_ls_bb_set_size(OhplSLBb*, const cVector2f*);
void ohpl_ls_bb_set_axis(OhplSLBb*, const cVector3f*);
void ohpl_ls_bb_set_color(OhplSLBb*, const cColor*);
void ohpl_ls_bb_set_halo_alpha(OhplSLBb*, float);
void ohpl_ls_bb_set_brightness(OhplSLBb*, float);
cMatrixf* ohpl_ls_bb_model(OhplSLBb*, const OhplFrustum*);
void ohpl_ls_bb_set_is_halo(OhplSLBb*, bool);
void ohpl_ls_bb_assign_occlusion(OhplSLBb*, void*);
bool ohpl_ls_bb_retrieve_occlusion(OhplSLBb*, void*);

OhplSLBeam* ohpl_ls_beam_new(void*, OhplEntPod*, OhplBv*, uintptr_t, uintptr_t);
void ohpl_ls_beam_set_end(OhplSLBeam*, uintptr_t, OhplSLBeamEnd*);
void ohpl_ls_beam_free(OhplSLBeam*);
void ohpl_ls_beam_set_size(OhplSLBeam*, const cVector2f*);
void ohpl_ls_beam_set_tile_height(OhplSLBeam*, bool);
void ohpl_ls_beam_set_color(OhplSLBeam*, const cColor*);
void ohpl_ls_beam_set_end_color(OhplSLBeam*, const cColor*);
void ohpl_ls_beam_bounds(OhplSLBeam*);
void ohpl_ls_beam_update_frame(OhplSLBeam*);
cMatrixf* ohpl_ls_beam_model(OhplSLBeam*, const OhplFrustum*);
bool ohpl_ls_beam_load(OhplSLBeam*, const char*, size_t);

OhplSLFlare* ohpl_ls_flare_new(void*, OhplEntPod*, OhplBv*, uintptr_t, uintptr_t);
void ohpl_ls_flare_free(OhplSLFlare*);
void ohpl_ls_flare_set_source_size(OhplSLFlare*, const cVector3f*);
void ohpl_ls_flare_set_size(OhplSLFlare*, int32_t, const cVector2f*);
void ohpl_ls_flare_set_active(OhplSLFlare*, int32_t, bool);
void ohpl_ls_flare_set_material(OhplSLFlare*, int32_t, uintptr_t);
void ohpl_ls_flare_color(OhplSLFlare*, const OhplFrustum*, int32_t, float, cColor*);
void ohpl_ls_flare_glare(OhplSLFlare*, const OhplFrustum*, cVector3f*, float, cColor*);
cMatrixf* ohpl_ls_flare_model(OhplSLFlare*, const OhplFrustum*);
bool ohpl_ls_flare_collides(OhplSLFlare*, const OhplFrustum*);
void ohpl_ls_flare_assign_occlusion(OhplSLFlare*, void*);
bool ohpl_ls_flare_retrieve_occlusion(OhplSLFlare*, void*);
void* ohpl_ls_flare_multi_iris(OhplSLFlare*, const OhplFrustum*, float);
OhplSLFlareType* ohpl_ls_ft_new(uintptr_t, OhplRnd*, OhplSLFlare*, int32_t);
void ohpl_ls_ft_free(OhplSLFlareType*);
cMatrixf* ohpl_ls_ft_model(OhplSLFlareType*, const OhplFrustum*);
bool ohpl_ls_ft_update_viewport(OhplSLFlareType*, const OhplFrustum*);
uintptr_t ohpl_ls_ft_material(const OhplSLFlareType*);
uintptr_t ohpl_ls_ft_vertex_buffer(const OhplSLFlareType*);
int32_t ohpl_ls_ft_update_count(const OhplSLFlareType*);

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
