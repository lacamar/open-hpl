#ifndef HPL_ENVIRONMENT_PARTICLES_H
#define HPL_ENVIRONMENT_PARTICLES_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "scene/RustLs.h"

namespace hpl {

	class cWorld;
	class cGraphics;
	class cResources;
	class iTexture;
	class iVertexBuffer;
	class iGpuProgram;
	class iEntity3D;
	class cLightSpot;

	#define kMaxEnvParticleIterations 30

	class cEnvironmentParticles
	{
	friend class cRendererDeferred;
	public:
		cEnvironmentParticles(const tString& asName, cWorld *apWorld, cGraphics *apGraphics, cResources *apResources)
			: msName(asName), mpL(ohpl_ls_env_new((uintptr_t)apResources, (uintptr_t)apGraphics)){}
		~cEnvironmentParticles();

		void Setup(float afBoxSize, int alNum, const cVector2f& avParticleSize, const cVector2l& avSubDivUV,
				   bool abAffectedByLight, iTexture *apTexture)
		{
			ohpl_ls_env_setup(mpL, afBoxSize, alNum, &avParticleSize, &avSubDivUV, abAffectedByLight, (uintptr_t)apTexture);
		}
		void Update(float afTimeStep){ ohpl_ls_env_update(mpL, afTimeStep); }
		bool IsVisible(cFrustum *apFrustum){ return ohpl_ls_env_is_visible(mpL, OhplLsFrustum(apFrustum)); }

		const tString& GetName(){ return msName; }
		bool SetVisible(bool abX){ mpL->visible = abX; return abX; }

		void SetBoxDistance(float afX){ mpL->boxDistance = afX; }
		void SetFadeInStart(float afX){ mpL->fadeInStart = afX; }
		void SetFadeInEnd(float afX){ mpL->fadeInEnd = afX; }
		void SetFadeOutStart(float afX){ mpL->fadeOutStart = afX; }
		void SetFadeOutEnd(float afX){ mpL->fadeOutEnd = afX; }
		void SetIterationNum(float afNum){ ohpl_ls_env_set_iteration_num(mpL, afNum); }
		float GetIterationNum(){ return mpL->iterationNum; }
		int GetIterationNumInt(){ return mpL->iterationInt; }

		void SetColor(const cColor& aCol){ mpL->color = aCol; }
		void SetBrightness(float afX){ mpL->brightness = afX; }
		cColor GetColor(){ return mpL->color; }
		float GetBrightness(){ return mpL->brightness; }

		void SetGravityVelocity(const cVector3f& avVel){ mpL->gravityVel = avVel; mpL->velDirty = true; }
		void SetWindVelocity(const cVector3f& avVel){ mpL->windVel = avVel; mpL->velDirty = true; }
		void SetRotateVelocity(const cVector3f& avVel){ mpL->rotateVel = avVel; mpL->velDirty = true; }
		cVector3f GetGravityVelocity(){ return mpL->gravityVel; }
		cVector3f GetWindVelocity(){ return mpL->windVel; }
		cVector3f GetRotateVelocity(){ return mpL->rotateVel; }
		void SetGravitySpeedRandomAmount(float afX){ mpL->gravitySpeedRand = afX; mpL->velDirty = true; }
		void SetWindSpeedRandomAmount(float afX){ mpL->windSpeedRand = afX; mpL->velDirty = true; }
		void SetWindDirectionRandomAmount(float afX){ mpL->windDirRand = afX; mpL->velDirty = true; }
		void SetRotateSpeedRandomAmount(float afX){ mpL->rotateSpeedRand = afX; mpL->velDirty = true; }
		void SetRotateSpeedRandomBothDirs(bool abX){ mpL->rotateBothDirs = abX; mpL->velDirty = true; }

		void SetClipActive(bool abX){ mpL->clipActive = abX; }
		bool GetClipActive(){ return mpL->clipActive; }
		void AddClipArea(iEntity3D *apClipEntity, bool abSubtractive){ ohpl_ls_env_add_clip_area(mpL, (uintptr_t)apClipEntity, abSubtractive); }
		void RemoveClipArea(iEntity3D *apClipEntity){ ohpl_ls_env_remove_clip_area(mpL, (uintptr_t)apClipEntity); }

		// ponytail: raw pointer, the flashlight lives as long as the world
		void SetSpotLight(cLightSpot *apSpotLight, float afMul){ mpL->spot = (uintptr_t)apSpotLight; mpL->spotMul = afMul; }

		inline static float (*mpLightLevelFunc)(const cVector3f& avPos) = NULL;

	private:
		iVertexBuffer* GetVertexBuffer(){ return (iVertexBuffer*)mpL->vtx; }
		iTexture* GetTexture(){ return (iTexture*)mpL->texture; }
		iGpuProgram* GetProgram(bool abDepthOfField){ return (iGpuProgram*)ohpl_ls_env_program(mpL, abDepthOfField); }
		void SetupProgramBase(iGpuProgram *apProg, cFrustum *apFrustum, const float *afDofParams)
		{
			ohpl_ls_env_setup_base(mpL, (uintptr_t)apProg, OhplLsFrustum(apFrustum), afDofParams);
		}
		void SetupProgramIteration(iGpuProgram *apProg, int alIdx){ ohpl_ls_env_setup_iteration(mpL, (uintptr_t)apProg, alIdx); }

		tString msName;
		OhplLsEnv *mpL;
	};

	typedef std::vector<cEnvironmentParticles*> tEnvironmentParticlesVec;

};
#endif // HPL_ENVIRONMENT_PARTICLES_H
