#ifndef HPL_ENVIRONMENT_PARTICLES_H
#define HPL_ENVIRONMENT_PARTICLES_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"

namespace hpl {

	class cWorld;
	class cGraphics;
	class cResources;
	class iTexture;
	class iVertexBuffer;
	class iGpuProgram;
	class iEntity3D;
	class cLightSpot;
	class cFrustum;

	#define kMaxEnvParticleIterations 30

	class cEnvironmentParticles
	{
	friend class cRendererDeferred;
	public:
		cEnvironmentParticles(const tString& asName, cWorld *apWorld, cGraphics *apGraphics, cResources *apResources);
		~cEnvironmentParticles();

		void Setup(float afBoxSize, int alNum, const cVector2f& avParticleSize, const cVector2l& avSubDivUV,
				   bool abAffectedByLight, iTexture *apTexture);
		void Update(float afTimeStep);
		bool IsVisible(cFrustum *apFrustum);

		const tString& GetName(){ return msName; }
		bool SetVisible(bool abX){ mbVisible = abX; return abX; }

		void SetBoxDistance(float afX){ mfBoxDistance = afX; }
		void SetFadeInStart(float afX){ mfFadeInStart = afX; }
		void SetFadeInEnd(float afX){ mfFadeInEnd = afX; }
		void SetFadeOutStart(float afX){ mfFadeOutStart = afX; }
		void SetFadeOutEnd(float afX){ mfFadeOutEnd = afX; }
		void SetIterationNum(float afNum);
		float GetIterationNum(){ return mfIterationNum; }
		int GetIterationNumInt(){ return mlIterationNum; }

		void SetColor(const cColor& aCol){ mColor = aCol; }
		void SetBrightness(float afX){ mfBrightness = afX; }
		cColor GetColor(){ return mColor; }
		float GetBrightness(){ return mfBrightness; }

		void SetGravityVelocity(const cVector3f& avVel){ mvGravityVel = avVel; mbVelDirty = true; }
		void SetWindVelocity(const cVector3f& avVel){ mvWindVel = avVel; mbVelDirty = true; }
		void SetRotateVelocity(const cVector3f& avVel){ mvRotateVel = avVel; mbVelDirty = true; }
		cVector3f GetGravityVelocity(){ return mvGravityVel; }
		cVector3f GetWindVelocity(){ return mvWindVel; }
		cVector3f GetRotateVelocity(){ return mvRotateVel; }
		void SetGravitySpeedRandomAmount(float afX){ mfGravitySpeedRand = afX; mbVelDirty = true; }
		void SetWindSpeedRandomAmount(float afX){ mfWindSpeedRand = afX; mbVelDirty = true; }
		void SetWindDirectionRandomAmount(float afX){ mfWindDirRand = afX; mbVelDirty = true; }
		void SetRotateSpeedRandomAmount(float afX){ mfRotateSpeedRand = afX; mbVelDirty = true; }
		void SetRotateSpeedRandomBothDirs(bool abX){ mbRotateBothDirs = abX; mbVelDirty = true; }

		void SetClipActive(bool abX){ mbClipActive = abX; }
		bool GetClipActive(){ return mbClipActive; }
		void AddClipArea(iEntity3D *apClipEntity, bool abSubtractive);
		void RemoveClipArea(iEntity3D *apClipEntity);

		// ponytail: raw pointer, the flashlight lives as long as the world
		void SetSpotLight(cLightSpot *apSpotLight, float afMul){ mpSpotLight = apSpotLight; mfSpotLightMul = afMul; }

		static float (*mpLightLevelFunc)(const cVector3f& avPos);

	private:
		struct cClipArea
		{
			iEntity3D *mpEntity;
			bool mbSubtractive;
			cMatrixf m_mtxClip;
		};

		iGpuProgram* GetProgram(bool abDepthOfField);
		void SetupProgramBase(iGpuProgram *apProg, cFrustum *apFrustum, const float *afDofParams);
		void SetupProgramIteration(iGpuProgram *apProg, int alIdx);
		void UpdateVelocityVectors();

		tString msName;
		cWorld *mpWorld;
		cGraphics *mpGraphics;
		cResources *mpResources;

		iVertexBuffer *mpVtxBuffer = NULL;
		iTexture *mpTexture = NULL;
		iGpuProgram *mpProgram[2] = {NULL, NULL};
		int mlProgramKey[2] = {-1, -1};

		bool mbVisible = true;
		float mfBoxSize = 1;
		cVector2f mvParticleSize = 1;
		bool mbAffectedByLight = false;

		float mfBoxDistance = 2;
		float mfFadeInStart = 0.2f, mfFadeInEnd = 1, mfFadeOutStart = 10, mfFadeOutEnd = 20;
		float mfIterationNum = 1;
		int mlIterationNum = 1;
		cColor mColor = cColor(1, 1);
		float mfBrightness = 1;

		cVector3f mvGravityVel = cVector3f(0, -0.1f, 0), mvWindVel = 0, mvRotateVel = 0;
		float mfGravitySpeedRand = 0, mfWindSpeedRand = 0, mfWindDirRand = 0, mfRotateSpeedRand = 0;
		bool mbRotateBothDirs = false;
		bool mbVelDirty = true;

		cVector3f mvIterGravityVel[kMaxEnvParticleIterations];
		cVector3f mvIterWindVel[kMaxEnvParticleIterations];
		cVector3f mvIterRotateVel[kMaxEnvParticleIterations];
		cVector3f mvIterOffset[kMaxEnvParticleIterations];
		cVector3f mvIterRotation[kMaxEnvParticleIterations];

		bool mbClipActive = true;
		std::vector<cClipArea> mvClipAreas;

		cLightSpot *mpSpotLight = NULL;
		float mfSpotLightMul = 1;
	};

	typedef std::vector<cEnvironmentParticles*> tEnvironmentParticlesVec;

};
#endif // HPL_ENVIRONMENT_PARTICLES_H
