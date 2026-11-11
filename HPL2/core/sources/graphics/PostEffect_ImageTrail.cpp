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

#include "graphics/PostEffect_ImageTrail.h"

#include "graphics/Graphics.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Texture.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"

#include "system/PreprocessParser.h"

namespace hpl {
	
	//////////////////////////////////////////////////////////////////////////
	// PROGRAM VARS
	//////////////////////////////////////////////////////////////////////////

	#define kVar_afAlpha	0

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT BASE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cPostEffectType_ImageTrail::cPostEffectType_ImageTrail(cGraphics *apGraphics, cResources *apResources) : iPostEffectType("ImageTrail",apGraphics,apResources)
	{
		cParserVarContainer vars;
		vars.Add("UseUv");

		mpProgram = mpGraphics->CreateGpuProgramFromShaders("ImageTrail","deferred_base_vtx.glsl", "posteffect_image_trail_frag.glsl", &vars);
		if(mpProgram)
		{
			mpProgram->GetVariableAsId("afAlpha",kVar_afAlpha);
		}
	}
	
	//-----------------------------------------------------------------------

	cPostEffectType_ImageTrail::~cPostEffectType_ImageTrail()
	{

	}

	//-----------------------------------------------------------------------

	iPostEffect * cPostEffectType_ImageTrail::CreatePostEffect(iPostEffectParams *apParams)
	{
		cPostEffect_ImageTrail *pEffect = hplNew(cPostEffect_ImageTrail, (mpGraphics,mpResources,this));
		cPostEffectParams_ImageTrail *pImageTrailParams = static_cast<cPostEffectParams_ImageTrail*>(apParams);

		return pEffect;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPostEffect_ImageTrail::cPostEffect_ImageTrail(cGraphics *apGraphics, cResources *apResources, iPostEffectType *apType) : iPostEffect(apGraphics,apResources,apType)
	{
		cVector2l vSize = mpLowLevelGraphics->GetScreenSizeInt();
		
		mpAccumTexture = mpGraphics->CreateTexture("ImageTrailTexture", eTextureType_Rect, eTextureUsage_RenderTarget);	
		mpAccumTexture->CreateFromRawData(cVector3l(vSize.x, vSize.y,1), ePixelFormat_RGB, NULL);

		mpAccumBuffer = mpGraphics->CreateFrameBuffer("ImageTrailBuffer");
		mpAccumBuffer->SetTexture2D(0, mpAccumTexture);
		if(mpAccumBuffer->CompileAndValidate()==false)
		{
			Error("Could not compile and validate image trail frame buffer!\n");
		}

		mpImageTrailType = static_cast<cPostEffectType_ImageTrail*>(mpType);

		mbClearFrameBuffer = true;
	}

	//-----------------------------------------------------------------------

	cPostEffect_ImageTrail::~cPostEffect_ImageTrail()
	{

	}

	//-----------------------------------------------------------------------

	void cPostEffect_ImageTrail::Reset()
	{
		mbClearFrameBuffer = true;
	}

	//-----------------------------------------------------------------------

	void cPostEffect_ImageTrail::OnSetParams()
	{
		
	}

	//-----------------------------------------------------------------------

	void cPostEffect_ImageTrail::OnSetActive(bool abX)
	{
		if(abX == false)
		{
			Reset();
		}
	}

	//-----------------------------------------------------------------------


	iTexture* cPostEffect_ImageTrail::RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
	{
		iTexture *vTextures[2] = {apInputTexture, mpAccumTexture};
		iFrameBuffer *vFrameBuffers[1] = {mpAccumBuffer};
		iGpuProgram *vPrograms[1] = {mpImageTrailType->mpProgram};

		OhplPePlan *pPlan = ohpl_pe_plan_image_trail(mParams.mfAmount, mpCurrentComposite->GetCurrentFrameTime(),
													 mpImageTrailType->mpProgram != NULL, &mbClearFrameBuffer);
		return RunPlan(pPlan, vTextures, vFrameBuffers, vPrograms, apFinalTempBuffer);
	}

	//-----------------------------------------------------------------------

}
