#ifndef HPL_RUST_SCENE_H12_H
#define HPL_RUST_SCENE_H12_H

// C ABI of rust/src/scene/h12_psload.rs. Layouts mirror its PsDesc.

#include "math/RustTypes.h"
#include "scene/RustParticles.h"
#include <cstdint>

namespace hpl {
	struct OhplH12PsDesc
	{
		OhplPeParams mParams;
		int32_t mlMaxParticleNum;
		bool mbRespawn;
		float mfWarmUpTime, mfWarmUpStepsPerSec;
		cVector3f mvPosOffset, mvAngleOffset;
		int32_t mlMaterialNum;
		float mfAnimationLength;
		cVector2l mvSubDiv;
		bool mbHasSubDiv, mbUseBeamNoise;
		int32_t mlLowFreqPoints, mlHighFreqPoints;
		cVector3f mvMinLowFreqNoise, mvMaxLowFreqNoise, mvMinHighFreqNoise, mvMaxHighFreqNoise;
		const char *msName, *msMaterial, *msDeathPS;
	};

	static_assert(sizeof(OhplH12PsDesc) == 544, "H12 ps desc layout");

	struct OhplH12PsBox;

	struct OhplH12PeCtl
	{
		int sleep_count = 60*5;
		int direction_update_count = -1;
		int axis_update_count = -1;
		bool update_gfx = true;
		bool update_bv = true;
	};

	static_assert(sizeof(OhplH12PeCtl) == 16, "H12 emitter ctl layout");

	struct OhplH12PsHost
	{
		void *ctx;
		int32_t (*emitter_count)(void*);
		float (*warm_up_time)(void*, int32_t);
		float (*warm_up_steps)(void*, int32_t);
		void (*update)(void*, int32_t, float);
		void (*apply)(void*, int32_t, int32_t, bool);
		bool (*query)(void*, int32_t, int32_t);
	};

	enum { kOhplH12OpVisible, kOhplH12OpCullDistance, kOhplH12OpCullFog, kOhplH12OpKill, kOhplH12OpKillInstantly };
	enum { kOhplH12QueryDead, kOhplH12QueryDying };
	enum { kOhplH12GateRun = 1, kOhplH12GateDirection = 2 };
	enum { kOhplH12HaloSize = 1, kOhplH12HaloTransform = 2 };
	enum { kOhplH12IrisNone, kOhplH12IrisReuse, kOhplH12IrisRecreate, kOhplH12IrisCreate };

extern "C" {

typedef const char* (*OhplH12AttrFn)(void *apCtx, const char *asName);

OhplH12PsBox* ohpl_h12_ps_parse(OhplH12AttrFn, void *apCtx);
const OhplH12PsDesc* ohpl_h12_ps_desc(const OhplH12PsBox*);
void ohpl_h12_ps_free(OhplH12PsBox*);
float ohpl_h12_ps_frame_step(float afAnimationLength, uint32_t alMaterials);
float ohpl_h12_ps_max_frame_time(uint32_t alMaterials);

void ohpl_h12b_ps_update_logic(const OhplH12PsHost*, bool *apFirst, bool abActive, float afStep);
void ohpl_h12b_ps_set_visible(const OhplH12PsHost*, bool *apVisible, bool abX);
void ohpl_h12b_ps_apply_all(const OhplH12PsHost*, int32_t alOp, bool abArg);
bool ohpl_h12b_ps_all(const OhplH12PsHost*, int32_t alQuery);
bool ohpl_h12b_psdata_plan(bool abRootOk, const bool *apChildIsEmitter, uint32_t alN, uint32_t alExisting, bool *apLoad);
bool ohpl_h12b_psdata_can_create(uint32_t alEmitters);
void ohpl_h12b_pe_ctl_init(OhplH12PeCtl*);
uint32_t ohpl_h12b_pe_update_gate(OhplH12PeCtl*, bool abActive, bool abDying, bool abSameRenderFrame, bool abUsesDirection, int32_t alMatrixCount);
bool ohpl_h12b_pe_axis_gate(OhplH12PeCtl*, bool abIsAxis, int32_t alMatrixCount);
uint32_t ohpl_h12b_pe_bv_gate(OhplH12PeCtl*, bool abIsAxis, int32_t alMatrixCount);
uint32_t ohpl_h12b_pe_model_kind(bool abHasFrustum, bool abIsAxis);

bool ohpl_h12b_is_visible(const cColor*, bool abVisible);
bool ohpl_h12b_bb_axis_size(bool abIsAxisType, const cVector3f *apAxis, float afSizeX, float afSizeY, float *apOut);
uint32_t ohpl_h12b_bb_halo_gates(bool *apSizeUpdated, int32_t *apBvCount, int32_t alTransformCount);
float ohpl_h12b_bb_halo_alpha(int32_t alSamples, int32_t alMaxSamples, bool abClipOk, const cVector3f *apMin, const cVector3f *apMax);
bool ohpl_h12b_beam_needs_geometry(int32_t alStart, int32_t alEnd, int32_t alTransform);
int32_t ohpl_h12b_rope_points(int32_t alAvailable, int32_t alMaxSegments);
int32_t ohpl_h12b_rope_elements(int32_t alCount);
uint32_t ohpl_h12b_iris_plan(bool abActive, int32_t alCount, int32_t alExistingVertices);
int32_t ohpl_h12b_flare_first_active(const bool *apActive);
bool ohpl_h12b_flare_any_active(float afFade, const bool *apActive);
uint32_t ohpl_h12b_snd_play_route(bool abForceGui, bool abUse3D);

}
}

#endif
