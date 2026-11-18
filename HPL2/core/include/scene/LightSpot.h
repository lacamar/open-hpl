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

#ifndef HPL_LIGHT_SPOT_H
#define HPL_LIGHT_SPOT_H

#include "scene/Light.h"

namespace hpl {

	class cResources;
	class iTexture;
	class cFrustum;

	//------------------------------------------

	class cLightSpot : public iLight
	{
	#ifdef __GNUC__
		typedef iLight __super;
	#endif
	public:
		cLightSpot(tString asName, cResources *apResources);
		~cLightSpot();

		const cMatrixf& GetViewMatrix();
		const cMatrixf& GetProjectionMatrix();
		const cMatrixf& GetViewProjMatrix();

		void SetFOV(float afAngle);
		inline float GetFOV() const { return Spot()->fov;}
		
		inline float GetTanHalfFOV() const{return Spot()->tanHalfFov; }
		inline float GetCosHalfFOV() const{return Spot()->cosHalfFov;}

		void SetAspect(float afAngle) { Spot()->aspect = afAngle; Spot()->ctl.projection_updated = true;}
		float GetAspect() { return Spot()->aspect;}

		void SetNearClipPlane(float afX) { Spot()->nearClip = afX; Spot()->ctl.projection_updated = true;}
		float GetNearClipPlane() { return Spot()->nearClip;}

		static void SetShadowNearClip(float afX){ ohpl_sl_spot_set_shadow_near(afX); }
		static float GetShadowNearClip(){ return ohpl_sl_spot_shadow_near(); }

		void SetRadius(float afX);
		
		cFrustum* GetFrustum();

		iTexture *GetSpotFalloffMap(){ return (iTexture*)Spot()->spotFalloffMap; }
		void SetSpotFalloffMap(iTexture* apTexture);

		bool CollidesWithBV(cBoundingVolume *apBV);
		bool CollidesWithFrustum(cFrustum *apFrustum);

	private:
		OhplSLSpot* Spot() const { return (OhplSLSpot*)mpS; }
		void ExtraXMLProperties(cXmlElement *apMainElem);
		void UpdateBoundingVolume();
	};

};
#endif // HPL_LIGHT_SPOT_H
