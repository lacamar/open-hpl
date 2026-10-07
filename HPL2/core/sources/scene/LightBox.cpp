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

#include "scene/LightBox.h"

#include "graphics/LowLevelGraphics.h"
#include "scene/Camera.h"
#include "math/Math.h"
#include "system/String.h"

#include "scene/World.h"
#include "scene/Scene.h"
#include "engine/Engine.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLightBox::cLightBox(tString asName, cResources *apResources) : iLight(asName,apResources)
	{
		mLightType = eLightType_Box;

		mvSize = 1;
		mBlendFunc = eLightBoxBlendFunc_Replace;
		mlBoxLightPrio =0;
		mAmbientColorSky = cColor(1,1);
		mAmbientColorGround = cColor(1,1);
		mfWeight = 1;
		mfBevel = 0;
		mfFalloffPow = 0;
		mbUseSphericalHarmonics = false;
		mvProbeOffset = 0;
		mpFadeTarget = NULL;
		mfFadeT = 1;
		mfFadeSpeed = 0;
		for(int i=0; i<9; ++i) mvBands[i] = 0;

		UpdateBoundingVolume();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cLightBox::SetSize(const cVector3f& avSize)
	{ 
		mvSize = avSize;

		mbUpdateBoundingVolume = true;

		//This is so that the render container is updated.
		SetTransformUpdated();
	}

	//-----------------------------------------------------------------------

	void cLightBox::AddIrradianceSet(const tString& asName, const std::vector<cVector3f>& avBands)
	{
		bool bFirst = m_mapIrradianceSets.empty();
		std::vector<cVector3f> &vBands = m_mapIrradianceSets[cString::ToLowerCase(asName)];
		vBands = avBands;
		vBands.resize(9, cVector3f(0));
		if(bFirst) for(int i=0; i<9; ++i) mvBands[i] = vBands[i];
	}

	void cLightBox::FadeIrradianceSet(const tString& asName, float afTime)
	{
		std::map<tString, std::vector<cVector3f> >::iterator it = m_mapIrradianceSets.find(cString::ToLowerCase(asName));
		if(it == m_mapIrradianceSets.end()) return;
		mpFadeTarget = &it->second;
		for(int i=0; i<9; ++i) mvFadeFrom[i] = mvBands[i];
		mfFadeT = 0;
		mfFadeSpeed = afTime > 0 ? 1.0f / afTime : 1e9f;
		UpdateLogic(0);
	}

	void cLightBox::UpdateLogic(float afTimeStep)
	{
		iLight::UpdateLogic(afTimeStep);
		if(mpFadeTarget==NULL) return;
		mfFadeT = cMath::Min(mfFadeT + mfFadeSpeed * afTimeStep, 1.0f);
		if(mfFadeSpeed >= 1e9f) mfFadeT = 1;
		for(int i=0; i<9; ++i) mvBands[i] = mvFadeFrom[i] + ((*mpFadeTarget)[i] - mvFadeFrom[i]) * mfFadeT;
		if(mfFadeT >= 1) mpFadeTarget = NULL;
	}

	bool cLightBox::IsVisible()
	{
		if(!IsLit()) return false;
		
		return mbIsVisible; 
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------
	
	void cLightBox::UpdateBoundingVolume()
	{
		mBoundingVolume.SetSize(mvSize);
		mBoundingVolume.SetPosition(GetWorldPosition());
	}
	//-----------------------------------------------------------------------

}
