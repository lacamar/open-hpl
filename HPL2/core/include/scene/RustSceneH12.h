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

extern "C" {

typedef const char* (*OhplH12AttrFn)(void *apCtx, const char *asName);

OhplH12PsBox* ohpl_h12_ps_parse(OhplH12AttrFn, void *apCtx);
const OhplH12PsDesc* ohpl_h12_ps_desc(const OhplH12PsBox*);
void ohpl_h12_ps_free(OhplH12PsBox*);
float ohpl_h12_ps_frame_step(float afAnimationLength, uint32_t alMaterials);
float ohpl_h12_ps_max_frame_time(uint32_t alMaterials);

}
}

#endif
