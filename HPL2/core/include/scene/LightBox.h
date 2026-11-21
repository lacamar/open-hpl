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
		cLightBox(tString asName, cResources *apResources) : iLight(asName,apResources,eLightType_Box){}

		void SetSize(const cVector3f& avSize){ Box()->size = avSize; mpE->updateBV = true; SetTransformUpdated();}
		inline const cVector3f& GetSize(){ return Box()->size;}

		void SetBlendFunc(eLightBoxBlendFunc aFunc){ Box()->blend = aFunc; }
		eLightBoxBlendFunc GetBlendFunc(){ return (eLightBoxBlendFunc)Box()->blend; }

		bool IsVisible(){ return IsLit() && mbIsVisible;}

		void SetBoxLightPrio(int alX){ Box()->prio = alX;}
		inline int GetBoxLightPrio()const{ return Box()->prio;}

		void SetAmbientColorSky(const cColor& aX){ Box()->ambientSky = aX;}
		void SetAmbientColorGround(const cColor& aX){ Box()->ambientGround = aX;}
		const cColor& GetAmbientColorSky(){ return Box()->ambientSky;}
		const cColor& GetAmbientColorGround(){ return Box()->ambientGround;}
		void SetWeight(float afX){ Box()->weight = afX;}
		float GetWeight(){ return Box()->weight;}
		void SetBevel(float afX){ Box()->bevel = afX;}
		float GetBevel(){ return Box()->bevel;}
		void SetFalloffPow(float afX){ Box()->falloffPow = afX;}
		float GetFalloffPow(){ return Box()->falloffPow;}
		void SetUseSphericalHarmonics(bool abX){ Box()->useSh = abX;}
		bool GetUseSphericalHarmonics(){ return Box()->useSh;}
		void SetProbeOffset(const cVector3f& avX){ Box()->probeOffset = avX;}
		const cVector3f& GetProbeOffset(){ return Box()->probeOffset;}

		void AddIrradianceSet(const tString& asName, const std::vector<cVector3f>& avBands){ ohpl_sl_box_add_set(Box(), asName.data(), asName.size(), avBands.data(), avBands.size());}
		void FadeIrradianceSet(const tString& asName, float afTime){ ohpl_ls_box_fade_set(mpS, asName.data(), asName.size(), afTime);}
		const cVector3f* GetIrradianceBands(){ return Box()->bands;}
		void UpdateLogic(float afTimeStep){ ohpl_ls_light_update_logic(mpS, afTimeStep);}

	private:
		OhplSLBox* Box() const { return (OhplSLBox*)mpS; }
	};

};
#endif // HPL_LIGHT_BOX_H
