/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HPL_LIGHT_BOX_H
#define HPL_LIGHT_BOX_H

#include "scene/Light.h"
#include "scene/SceneTypes.h"

namespace hpl {

	//------------------------------------------

	class cLightBox : public iLight
	{
	public:
		cLightBox(tString asName, cResources *apResources);

		void SetSize(const cVector3f& avSize);
		inline const cVector3f& GetSize(){ return mvSize;}

		void SetBlendFunc(eLightBoxBlendFunc aFunc){ mBlendFunc = aFunc; }
		eLightBoxBlendFunc GetBlendFunc(){ return mBlendFunc; }

		bool IsVisible();

		void SetBoxLightPrio(int alX){ mlBoxLightPrio = alX;}
		inline int GetBoxLightPrio()const{ return mlBoxLightPrio;}

		void SetAmbientColorSky(const cColor& aX){ mAmbientColorSky = aX;}
		void SetAmbientColorGround(const cColor& aX){ mAmbientColorGround = aX;}
		const cColor& GetAmbientColorSky(){ return mAmbientColorSky;}
		const cColor& GetAmbientColorGround(){ return mAmbientColorGround;}
		void SetWeight(float afX){ mfWeight = afX;}
		float GetWeight(){ return mfWeight;}
		void SetBevel(float afX){ mfBevel = afX;}
		float GetBevel(){ return mfBevel;}
		void SetFalloffPow(float afX){ mfFalloffPow = afX;}
		float GetFalloffPow(){ return mfFalloffPow;}
		void SetUseSphericalHarmonics(bool abX){ mbUseSphericalHarmonics = abX;}
		bool GetUseSphericalHarmonics(){ return mbUseSphericalHarmonics;}
		void SetProbeOffset(const cVector3f& avX){ mvProbeOffset = avX;}
		const cVector3f& GetProbeOffset(){ return mvProbeOffset;}

		void AddIrradianceSet(const tString& asName, const std::vector<cVector3f>& avBands);
		void FadeIrradianceSet(const tString& asName, float afTime);
		const cVector3f* GetIrradianceBands(){ return mvBands;}
		void UpdateLogic(float afTimeStep);

	private:
		void UpdateBoundingVolume();

		cVector3f mvSize;
		eLightBoxBlendFunc mBlendFunc;
		int mlBoxLightPrio;

		cColor mAmbientColorSky;
		cColor mAmbientColorGround;
		float mfWeight;
		float mfBevel;
		float mfFalloffPow;
		bool mbUseSphericalHarmonics;
		cVector3f mvProbeOffset;
		std::map<tString, std::vector<cVector3f> > m_mapIrradianceSets;
		cVector3f mvBands[9];
		cVector3f mvFadeFrom[9];
		const std::vector<cVector3f> *mpFadeTarget;
		float mfFadeT;
		float mfFadeSpeed;
	};

};
#endif // HPL_LIGHT_BOX_H
