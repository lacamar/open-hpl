#ifndef HPL_RUST_PARTICLES_H
#define HPL_RUST_PARTICLES_H

// C ABI of rust/src/scene/ffi_particles.rs. Layouts mirror particles.rs / billboard.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

struct OhplEmitter;

namespace hpl {
	struct OhplPeParams
	{
		int32_t mlPEType, mlDrawType, mlCoordSystem, mlSubDivType, mlStartPosType, mlStartVelType,
				mlGravityType, mlPartSpinType, mlMinCollisionMax, mlMaxCollisionMax, mlCollisionUpdateRate;
		bool mbUsesDirection, mbUsePartSpin, mbUseRevolution, mbMultiplyRGBWithAlpha, mbCollides;
		float mfParticlesPerSecond, mfStartTimeOffset, mfMinPauseLength, mfMaxPauseLength,
				mfMinPauseInterval, mfMaxPauseInterval;
		cVector3f mvMinStartPos, mvMaxStartPos;
		cVector2f mvMinStartAngles, mvMaxStartAngles;
		float mfMinStartRadius, mfMaxStartRadius;
		cVector3f mvMinStartVel, mvMaxStartVel;
		cVector2f mvMinStartVelAngles, mvMaxStartVelAngles;
		float mfMinStartVelSpeed, mfMaxStartVelSpeed, mfMinSpeedMultiply, mfMaxSpeedMultiply;
		cVector3f mvMinStartAcc, mvMaxStartAcc;
		float mfMinVelMaximum, mfMaxVelMaximum;
		cVector3f mvGravityAcc;
		float mfMinSpinRange, mfMaxSpinRange;
		cVector3f mvMinRevVel, mvMaxRevVel;
		float mfMinLifeSpan, mfMaxLifeSpan;
		cVector2f mvMinStartSize, mvMaxStartSize;
		float mfStartRelSize, mfMiddleRelSize, mfMiddleRelSizeTime, mfMiddleRelSizeLength, mfEndRelSize;
		cColor mMinStartColor, mMaxStartColor, mStartRelColor, mMiddleRelColor;
		float mfMiddleRelColorTime, mfMiddleRelColorLength;
		cColor mEndRelColor;
		float mfMinBounceAmount, mfMaxBounceAmount, mfFrameStep, mfMaxFrameTime;
	};

	struct OhplPeGfxIn
	{
		bool mbHpl3Color, mbAddBlend, mbFadeAtDistance, mbInvertsCull;
		cColor mPsColor;
		float mfBrightness;
		cColor mLightColor;
		float mfLightBrightness;
		cVector3f mvFadeCenter, mvOrigin;
		float mfMinFadeStart, mfMinFadeEnd, mfMaxFadeStart, mfMaxFadeEnd;
		cVector2f mvDrawSize;
		cMatrixf m_mtxWorld, m_mtxView, m_mtxPsWorld;
		cVector3f mvAxisRight, mvAxisForward;
		int32_t mlPosStride;
	};

	struct OhplParticle
	{
		cVector3f mvPos, mvLastPos, mvLastCollidePos, mvAcc, mvVel;
		float mfSpeedMul, mfMaxSpeed;
		cColor mStartColor, mColor;
		cVector2f mvStartSize, mvSize;
		float mfStartLife, mfLife, mfLifeSize_MiddleStart, mfLifeSize_MiddleEnd,
				mfLifeColor_MiddleStart, mfLifeColor_MiddleEnd;
		int32_t mlSubDivNum;
		float mfBounceAmount;
		int32_t mlBounceCount;
		float mfSpin, mfSpinVel, mfSpinFactor;
		cVector3f mvRevolutionVel;
	};

	struct OhplBbModelIn
	{
		int32_t mlKind;
		float mfForwardOffset;
		cMatrixf m_mtxWorld;
		cVector3f mvCamOrigin, mvViewUp, mvAxis;
	};

	struct OhplEnvVelIn
	{
		cVector3f mvGravityVel, mvWindVel, mvRotateVel;
		float mfGravitySpeedRand, mfWindSpeedRand, mfWindDirRand, mfRotateSpeedRand;
		bool mbRotateBothDirs;
	};

	static_assert(sizeof(OhplPeParams) == 404 && sizeof(OhplPeGfxIn) == 312 && sizeof(OhplParticle) == 176
				  && sizeof(OhplBbModelIn) == 108 && sizeof(OhplEnvVelIn) == 56, "particle layout");

extern "C" {

typedef bool (*OhplPeCollideFn)(void *apCtx, const cVector3f *apStart, const cVector3f *apEnd,
								cVector3f *apNormal, cVector3f *apPoint);

OhplEmitter* ohpl_pe_new(const OhplPeParams*, uint32_t alMax, bool abRespawn, const cVector2l *apSubDiv, uint32_t alMaterials);
void ohpl_pe_free(OhplEmitter*);
void ohpl_pe_update_motion(OhplEmitter*, float afDt, const cMatrixf *a_mtxStart, OhplPeCollideFn, void *apCtx);
uint32_t ohpl_pe_num_particles(const OhplEmitter*);
bool ohpl_pe_particle(const OhplEmitter*, uint32_t alIdx, OhplParticle *apOut);

}
}

#endif
