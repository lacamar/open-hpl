#ifndef HPL_LIGHT_DIRECTIONAL_H
#define HPL_LIGHT_DIRECTIONAL_H

#include "scene/Light.h"

namespace hpl {

	class cLightDirectional : public iLight
	{
	public:
		cLightDirectional(tString asName, cResources *apResources) : iLight(asName, apResources) { mLightType = eLightType_Directional; }

		void SetDirection(const cVector3f& avDir){ mvDirection = avDir; }
		const cVector3f& GetDirection(){ return mvDirection; }
		void SetAmbientColorSky(const cColor& aX){ mAmbientColorSky = aX; }
		void SetAmbientColorGround(const cColor& aX){ mAmbientColorGround = aX; }
		const cColor& GetAmbientColorSky(){ return mAmbientColorSky; }
		const cColor& GetAmbientColorGround(){ return mAmbientColorGround; }
		void SetShadowCasterDistance(float afX){ mfShadowCasterDistance = afX; }
		float GetShadowCasterDistance(){ return mfShadowCasterDistance; }
		void SetAutoShadowSliceSettings(bool abX){ mbAutoShadowSliceSettings = abX; }
		bool GetAutoShadowSliceSettings(){ return mbAutoShadowSliceSettings; }
		void SetAutoShadowSliceLogTerm(float afX){ mfAutoShadowSliceLogTerm = afX; }
		float GetAutoShadowSliceLogTerm(){ return mfAutoShadowSliceLogTerm; }

	private:
		void UpdateBoundingVolume(){}

		cVector3f mvDirection = cVector3f(0,-1,0);
		cColor mAmbientColorSky = cColor(0,0);
		cColor mAmbientColorGround = cColor(0,0);
		float mfShadowCasterDistance = 40;
		bool mbAutoShadowSliceSettings = true;
		float mfAutoShadowSliceLogTerm = 0.9f;
	};

};
#endif
