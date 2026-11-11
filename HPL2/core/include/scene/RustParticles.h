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
int ohpl_pe_build_vertices(const OhplEmitter*, const OhplPeGfxIn*, float *apPos, size_t alPosLen,
						   float *apCol, size_t alColLen, float *apTex, size_t alTexLen);
void ohpl_pe_bounds(const OhplEmitter*, const cVector3f *apWorldPos, const cVector3f *apRight,
					const cVector3f *apForward, const cVector2f *apMaxDraw, cVector3f *apMin, cVector3f *apMax);
uint32_t ohpl_pe_num_particles(const OhplEmitter*);
bool ohpl_pe_is_dying(const OhplEmitter*);
bool ohpl_pe_is_dead(const OhplEmitter*);
void ohpl_pe_kill(OhplEmitter*);
void ohpl_pe_kill_instantly(OhplEmitter*);
float ohpl_pe_frame(const OhplEmitter*);
size_t ohpl_pe_sub_div_count(const OhplEmitter*);
bool ohpl_pe_particle(const OhplEmitter*, uint32_t alIdx, OhplParticle *apOut);
void ohpl_pe_debug_state(const OhplEmitter*, float *apOut8);

bool ohpl_bb_model_matrix(const OhplBbModelIn*, cMatrixf *apOut);
void ohpl_bb_vertex_colors(const cColor*, float afBrightness, float afHaloAlpha, bool abHdr, float *apOut16);
void ohpl_bb_vertex_positions(const cVector2f*, float *apOut16);

void ohpl_env_velocity_vectors(const OhplEnvVelIn*, cVector3f *apGrav30, cVector3f *apWind30, cVector3f *apRot30);
void ohpl_env_update(int alN, float afDt, const cVector3f *apGrav30, const cVector3f *apWind30, const cVector3f *apRot30,
					 cVector3f *apOffset30, cVector3f *apRotation30);
void ohpl_env_setup(float afBoxSize, int alNum, const cVector2f *apParticleSize, const cVector2l *apSubDiv,
					float *apPos, float *apUv0, float *apUv1);

}
}

#endif
