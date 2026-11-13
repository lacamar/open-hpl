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

#include "scene/LightSpot.h"
#include "scene/RustLight.h"
#include "impl/RustBackends.h"

#include "impl/tinyXML/tinyxml.h"
#include "math/Math.h"
#include "math/Frustum.h"
#include "resources/TextureManager.h"
#include "resources/Resources.h"
#include "graphics/LowLevelGraphics.h"
#include "scene/Camera.h"

#include "scene/World.h"
#include "scene/Scene.h"
#include "engine/Engine.h"

#include "system/String.h"

namespace hpl {

	float cLightSpot::mfShadowNearClip = 0;

	
	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cLightSpot::cLightSpot(tString asName, cResources *apResources) : iLight(asName,apResources)
	{
        mLightType = eLightType_Spot;

		mpFrustum = hplNew( cFrustum, () );

		mSpot.view_proj_count =-1;
		mSpot.view_count =-1;
		mSpot.frustum_count =-1;

		mfFOV = cMath::ToRad(60.0f);
		mfAspect = 1.0f;
		mfNearClipPlane = 0.1f;
		mfRadius = 100.0f;

		ohpl_light_spot_half_fov(mfFOV, &mfTanHalfFOV, &mfCosHalfFOV);

		mbFovUpdated = true;
		
		m_mtxView = cMatrixf::Identity;
		m_mtxViewProj = cMatrixf::Identity;
		m_mtxProjection = cMatrixf::Identity;

		static bool bFalloffMissing = mbHpl3Visibility;
		mpSpotFalloffMap = bFalloffMissing ? NULL : mpTextureManager->Create1D("core_falloff_linear",false);
		bFalloffMissing = mpSpotFalloffMap == NULL;
		if(mpSpotFalloffMap)
		{
			mpSpotFalloffMap->SetWrapS(eTextureWrap_ClampToEdge);
			mpSpotFalloffMap->SetWrapT(eTextureWrap_ClampToEdge);
		}

		UpdateBoundingVolume();
	}
	
	cLightSpot::~cLightSpot()
	{
		if(mpSpotFalloffMap) mpTextureManager->Destroy(mpSpotFalloffMap);

		hplDelete(mpFrustum);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cLightSpot::SetRadius(float afX)
	{ 
		mfRadius = afX;

		UpdateBoundingVolume();

		//This is so that the render container is updated.
		SetTransformUpdated();
		mSpot.projection_updated = true;
	}

	void cLightSpot::SetFOV(float afAngle)
	{ 
		mfFOV = afAngle;
		mSpot.projection_updated = true;

		ohpl_light_spot_half_fov(mfFOV, &mfTanHalfFOV, &mfCosHalfFOV);
	}
	
	//-----------------------------------------------------------------------
	
	const cMatrixf& cLightSpot::GetViewMatrix()
	{
		if(OhplBackendRustRequested() ? ohpl_h11_spot_view_stale(&mSpot, GetTransformUpdateCount()) : mSpot.view_count != GetTransformUpdateCount())
		{
			mSpot.view_count = GetTransformUpdateCount();
			m_mtxView = cMath::MatrixInverse(GetWorldMatrix());
		}

		return m_mtxView;
	}

	//-----------------------------------------------------------------------


	const cMatrixf& cLightSpot::GetProjectionMatrix()
	{
		if(OhplBackendRustRequested() ? ohpl_h11_spot_projection_stale(&mSpot) : mSpot.projection_updated)
		{
			ohpl_light_spot_projection(mfFOV, mfAspect, mfNearClipPlane, mfShadowNearClip, mfRadius, &m_mtxProjection);

			if(!OhplBackendRustRequested())
			{
				mSpot.projection_updated = false;
				mSpot.view_proj_updated = true;
				mSpot.frustum_updated = true;
			}
		}

		return m_mtxProjection;
	}

	//-----------------------------------------------------------------------
	
	const cMatrixf& cLightSpot::GetViewProjMatrix()
	{
		if(OhplBackendRustRequested() ? ohpl_h11_spot_view_proj_stale(&mSpot, GetTransformUpdateCount())
									  : (mSpot.view_proj_count != GetTransformUpdateCount() || mSpot.view_proj_updated || mSpot.projection_updated))
		{
			const cMatrixf &mtxProj = GetProjectionMatrix();
			const cMatrixf &mtxView = GetViewMatrix();
			ohpl_light_spot_view_proj(&mtxProj, &mtxView, &m_mtxViewProj);
			
			if(OhplBackendRustRequested()) ohpl_h11_spot_view_proj_done(&mSpot, GetTransformUpdateCount());
			else
			{
				mSpot.view_proj_count = GetTransformUpdateCount();
				mSpot.view_proj_updated = false;
			}
		}

		return m_mtxViewProj;
	}

	//-----------------------------------------------------------------------

	cFrustum* cLightSpot::GetFrustum()
	{
		if(OhplBackendRustRequested() ? ohpl_h11_spot_frustum_stale(&mSpot, GetTransformUpdateCount())
									  : (mSpot.frustum_count != GetTransformUpdateCount() || mSpot.frustum_updated || mSpot.projection_updated))
		{
			mpFrustum->SetupPerspectiveProj(GetProjectionMatrix(),
											GetViewMatrix(),
											mfRadius,ohpl_light_spot_near(mfNearClipPlane, mfShadowNearClip),
											mfFOV,mfAspect,GetWorldPosition(),false);
			if(OhplBackendRustRequested()) ohpl_h11_spot_frustum_done(&mSpot, GetTransformUpdateCount());
			else
			{
				mSpot.frustum_updated = false;
				mSpot.frustum_count = GetTransformUpdateCount();
			}
		}

		return mpFrustum;
	}

	//-----------------------------------------------------------------------

	iTexture *cLightSpot::GetSpotFalloffMap()
	{
		return mpSpotFalloffMap;
	}

	void cLightSpot::SetSpotFalloffMap(iTexture* apTexture)
	{
		if(mpSpotFalloffMap) mpTextureManager->Destroy(mpSpotFalloffMap);

		mpSpotFalloffMap = apTexture;
		if(mpSpotFalloffMap)
		{
			mpSpotFalloffMap->SetWrapS(eTextureWrap_ClampToEdge);
			mpSpotFalloffMap->SetWrapT(eTextureWrap_ClampToEdge);
		}
	}

	//-----------------------------------------------------------------------


	bool cLightSpot::CollidesWithBV(cBoundingVolume *apBV)
	{
		if(cMath::CheckBVIntersection(*GetBoundingVolume(), *apBV)==false) return false;

		return GetFrustum()->CollideBoundingVolume(apBV)!= eCollision_Outside;
	}
	
	//-----------------------------------------------------------------------

	bool cLightSpot::CollidesWithFrustum(cFrustum *apFrustum)
	{
		return apFrustum->CollideFrustum(GetFrustum())!=eCollision_Outside;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	
	static eTextureAnimMode GetAnimMode(const tString& asType)
	{
		if(cString::ToLowerCase(asType) == "none") return eTextureAnimMode_None;
		else if(cString::ToLowerCase(asType) == "loop") return eTextureAnimMode_Loop;
		else if(cString::ToLowerCase(asType) == "oscillate") return eTextureAnimMode_Oscillate;

		return eTextureAnimMode_None;
	}

	void cLightSpot::ExtraXMLProperties(TiXmlElement *apMainElem)
	{
		tString sTexture = cString::ToString(apMainElem->Attribute("ProjectionImage"),"");

		eTextureAnimMode animMode = GetAnimMode(cString::ToString(apMainElem->Attribute("ProjectionAnimMode"),"None"));
		float fFrameTime = cString::ToFloat(apMainElem->Attribute("ProjectionFrameTime"),1.0f);
		iTexture *pTex = NULL;

        if(animMode != eTextureAnimMode_None)
		{
			pTex = mpTextureManager->CreateAnim(sTexture,true,eTextureType_2D);
			pTex->SetAnimMode(animMode);
			pTex->SetFrameTime(fFrameTime);
		}
		else
		{
			pTex = mpTextureManager->Create2D(sTexture,true);
		}
		
		
		if(pTex)
		{
			SetGoboTexture(pTex);
		}

		mfAspect = cString::ToFloat(apMainElem->Attribute("Aspect"),mfAspect);

		mfNearClipPlane = cString::ToFloat(apMainElem->Attribute("NearClipPlane"),mfNearClipPlane);
	}

	//-----------------------------------------------------------------------

	void cLightSpot::UpdateBoundingVolume()
	{
		mBoundingVolume = GetFrustum()->GetBoundingVolume();
	}

	//-----------------------------------------------------------------------
	
}
