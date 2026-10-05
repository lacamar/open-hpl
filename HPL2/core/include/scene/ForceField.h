#ifndef HPL_FORCE_FIELD_H
#define HPL_FORCE_FIELD_H

#include "graphics/Renderable.h"

namespace hpl {

	// HPL3 force field: bends sway/undergrowth vertices around it
	class cForceField : public iRenderable
	{
	public:
		cForceField(const tString& asName, bool abAutoRemove) : iRenderable(asName), mbAutoRemove(abAutoRemove) {}

		void UpdateLogic(float afTimeStep)
		{
			mfT += GetFinalFreq() * afTimeStep;
			if(mfFade < mfFadeGoal) mfFade = std::min(mfFade + mfFadeSpeed * afTimeStep, mfFadeGoal);
			else mfFade = std::max(mfFade - mfFadeSpeed * afTimeStep, mfFadeGoal);
		}

		void FadeTo(float afAmount, float afTime){ mfFadeGoal = afAmount; mfFadeSpeed = afTime > 0 ? 1 / afTime : 1000; }
		void FadeOut(float afTime){ FadeTo(0, afTime); }
		bool IsDead(){ return mfFade <= 0; }

		void SetRadius(float afX){ mfRadius = afX; mBoundingVolume.SetSize(afX * 2); SetTransformUpdated(); }
		float GetRadius(){ return mfRadius; }
		float GetFinalRadius(){ return mfRadius; }
		void SetFalloffStartRadius(float afX){ mfFalloffStart = afX; }
		float GetFalloffStartRadius(){ return mfFalloffStart; }
		float GetFinalFalloffStartRadius(){ return mfFalloffStart; }
		void SetForce(float afX){ mfForce = afX; }
		float GetForce(){ return mfForce; }
		float GetFinalForce(){ return mfForce * mfFade; }
		void SetFreq(float afX){ mfFreq = afX; }
		float GetFreq(){ return mfFreq; }
		float GetFinalFreq(){ return mfFreq * mfFade; }
		void SetAutoRemove(bool abX){ mbAutoRemove = abX; }
		bool GetAutoRemove(){ return mbAutoRemove; }
		float GetT(){ return mfT; }

		bool IsVisible(){ return GetFinalForce() > 0 && mfRadius > 0 && mbIsVisible; }

		tString GetEntityType(){ return "cForceField"; }
		cMaterial *GetMaterial(){ return NULL; }
		iVertexBuffer* GetVertexBuffer(){ return NULL; }
		eRenderableType GetRenderType(){ return eRenderableType_Dummy; }
		int GetMatrixUpdateCount(){ return GetTransformUpdateCount(); }
		cMatrixf* GetModelMatrix(cFrustum* apFrustum){ return NULL; }

	private:
		float mfRadius = 0, mfFalloffStart = 0, mfForce = 0, mfFreq = 0, mfT = 0;
		float mfFade = 1, mfFadeGoal = 1, mfFadeSpeed = 1;
		bool mbAutoRemove;
	};

};
#endif // HPL_FORCE_FIELD_H
