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
		cLightSpot(tString asName, cResources *apResources) : iLight(asName,apResources,eLightType_Spot){}

		const cMatrixf& GetViewMatrix(){ return *ohpl_ls_spot_view(mpS);}
		const cMatrixf& GetProjectionMatrix(){ return *ohpl_ls_spot_projection(mpS);}
		const cMatrixf& GetViewProjMatrix(){ return *ohpl_ls_spot_view_proj(mpS);}

		void SetFOV(float afAngle){ ohpl_ls_spot_set_fov(mpS, afAngle);}
		inline float GetFOV() const { return Spot()->fov;}
		
		inline float GetTanHalfFOV() const{return Spot()->tanHalfFov; }
		inline float GetCosHalfFOV() const{return Spot()->cosHalfFov;}

		void SetAspect(float afAngle) { Spot()->aspect = afAngle; Spot()->ctl.projection_updated = true;}
		float GetAspect() { return Spot()->aspect;}

		void SetNearClipPlane(float afX) { Spot()->nearClip = afX; Spot()->ctl.projection_updated = true;}
		float GetNearClipPlane() { return Spot()->nearClip;}

		static void SetShadowNearClip(float afX){ ohpl_sl_spot_set_shadow_near(afX); }
		static float GetShadowNearClip(){ return ohpl_sl_spot_shadow_near(); }

		cFrustum* GetFrustum(){ return (cFrustum*)ohpl_ls_spot_frustum(mpS);}

		iTexture *GetSpotFalloffMap(){ return (iTexture*)Spot()->spotFalloffMap; }
		void SetSpotFalloffMap(iTexture* apTexture){ ohpl_ls_spot_set_falloff_map(mpS, (uintptr_t)apTexture);}

		bool CollidesWithBV(cBoundingVolume *apBV){ return ohpl_ls_spot_collides_bv(mpS, apBV->Rs());}
		bool CollidesWithFrustum(cFrustum *apFrustum){ return ohpl_ls_spot_collides_frustum(mpS, OhplLsFrustum(apFrustum));}

	private:
		OhplSLSpot* Spot() const { return (OhplSLSpot*)mpS; }
	};

};
#endif // HPL_LIGHT_SPOT_H
