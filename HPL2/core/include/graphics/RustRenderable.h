#ifndef HPL_RUST_RENDERABLE_H
#define HPL_RUST_RENDERABLE_H

// iRenderable state, owned and laid out by rust/src/scene/rnd.rs (Rnd); read in place.

#include "graphics/Color.h"
#include "math/MathTypes.h"
#include <cstdint>

namespace hpl {

	class iRenderableCallback;
	class iRenderableContainerNode;

	struct OhplRnd
	{
		cMatrixf m_mtxInvModel;
		cMatrixf m_mtxPrevious;
		cMatrixf *mpModelMatrix;
		iRenderableCallback *mpRenderCallback;
		bool mbIsOneSided;
		cVector3f mvOneSidedNormal;
		int mlLastMatrixCount;
		uint32_t mlRenderFlags;
		bool mbIsVisible, mbStatic, mbForceShadow;
		int mlLargePlaneSurfacePlacement;
		int mlRenderFrameCount;
		int mlCalcScaleMatrixCount;
		cVector3f mvCalcScale;
		float mfViewSpaceZ;
		float mfIlluminationAmount;
		cColor mIlluminationColor;
		cColor mColorMul;
		float mfCoverageAmount;
		float mfLiquidAmount;
		bool mbCulledByDistance, mbCulledByFog;
		float mfDistanceCullFade;
		iRenderableContainerNode *mpRenderContainerNode;
		void *mpRenderableUserData;
	};

	static_assert(sizeof(OhplRnd) == 272, "Rnd layout");

extern "C" {

OhplRnd* ohpl_rnd_new();
void ohpl_rnd_free(OhplRnd*);

}
}

#endif
