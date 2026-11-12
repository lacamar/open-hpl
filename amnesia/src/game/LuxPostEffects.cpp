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

#include "LuxPostEffects.h"
#include "RustLuxFx.h"

#include "LuxMapHandler.h"

//////////////////////////////////////////////////////////////////////////
// VARIABLES
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

#define kVar_afAlpha			0
#define kVar_afT				1
#define kVar_avScreenSize		2
#define kVar_afAmpT				3
#define kVar_afWaveAlpha		4
#define kVar_afZoomAlpha		5

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// INSANITY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPostEffect_Insanity::cLuxPostEffect_Insanity(cGraphics *apGraphics, cResources *apResources) : iLuxPostEffect(apGraphics, apResources)
{
	//////////////////////////////
	// Create program
	cParserVarContainer vars;
	vars.Add("UseUv");
	mpProgram = mpGraphics->CreateGpuProgramFromShaders("LuxInsanity","deferred_base_vtx.glsl", "posteffect_insanity_frag.glsl", &vars);
	if(mpProgram)
	{
		mpProgram->GetVariableAsId("afAlpha",kVar_afAlpha);
		mpProgram->GetVariableAsId("afT",kVar_afT);
		mpProgram->GetVariableAsId("avScreenSize",kVar_avScreenSize);
		mpProgram->GetVariableAsId("afAmpT",kVar_afAmpT);
		mpProgram->GetVariableAsId("afWaveAlpha",kVar_afWaveAlpha);
		mpProgram->GetVariableAsId("afZoomAlpha",kVar_afZoomAlpha);
	}


	//////////////////////////////
	// Textures
	mvAmpMaps.resize(3);
	
	for(size_t i=0; i<mvAmpMaps.size(); ++i)
		mvAmpMaps[i] = mpResources->GetTextureManager()->Create2D("posteffect_insanity_ampmap"+cString::ToString((int)i), false);

	mpZoomMap = mpResources->GetTextureManager()->Create2D("posteffect_insanity_zoom.jpg", false);

	//////////////////////////////
	// Init vars
	mfT =0;
	mfAnimCount =0;
	mfWaveAlpha = 0.0f;
	mfZoomAlpha = 0.0f;
	mfWaveSpeed =0.0f;
}

//-----------------------------------------------------------------------

cLuxPostEffect_Insanity::~cLuxPostEffect_Insanity()
{

}

//-----------------------------------------------------------------------

void cLuxPostEffect_Insanity::Update(float afTimeStep)
{
	OhplGa9bClock clock = { mfT, mfAnimCount, mfWaveAlpha, mfZoomAlpha, mfWaveSpeed };
	ohpl_ga9b_post_update(&clock, afTimeStep);
	mfT = clock.t;
	mfAnimCount = clock.animCount;
}

//-----------------------------------------------------------------------


iTexture* cLuxPostEffect_Insanity::RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
{
	OhplGa9bClock clock = { mfT, mfAnimCount, mfWaveAlpha, mfZoomAlpha, mfWaveSpeed };
	cVector2f vScreenSize(0);
	if(mpProgram) vScreenSize = mpLowLevelGraphics->GetScreenSizeFloat();
	OhplGa9bPPlan *pPlan = ohpl_ga9b_post_plan(&clock, vScreenSize.x, vScreenSize.y, mpProgram != NULL);

	iTexture *vTextures[5] = { apInputTexture, mvAmpMaps[0], mvAmpMaps[1], mvAmpMaps[2], mpZoomMap };
	auto Tex = [&](int i) { return i >= 0 ? vTextures[i] : (iTexture*)NULL; };

	OhplGa9bPOp op;
	while(ohpl_ga9b_pplan_next(pPlan, &op))
	{
		switch(op.kind)
		{
		case kOhplGa9bPFlatProjection:		mpCurrentComposite->SetFlatProjection(); break;
		case kOhplGa9bPBlendNone:			mpCurrentComposite->SetBlendMode(eMaterialBlendMode_None); break;
		case kOhplGa9bPChannelRgba:			mpCurrentComposite->SetChannelMode(eMaterialChannelMode_RGBA); break;
		case kOhplGa9bPFinalFb:				SetFinalFrameBuffer(apFinalTempBuffer); break;
		case kOhplGa9bPTex:					mpCurrentComposite->SetTexture(op.a, Tex(op.b)); break;
		case kOhplGa9bPProgram:				mpCurrentComposite->SetProgram(op.a >= 0 ? mpProgram : NULL); break;
		case kOhplGa9bPFloat:				mpProgram->SetFloat(op.b, op.f[0]); break;
		case kOhplGa9bPVec2:				mpProgram->SetVec2f(op.b, cVector2f(op.f[0], op.f[1])); break;
		case kOhplGa9bPQuad1:				DrawQuad(0,1,Tex(op.a), op.b != 0); break;
		case kOhplGa9bPTextureRangeNull:	mpCurrentComposite->SetTextureRange(NULL, op.a); break;
		}
	}
	ohpl_ga9b_pplan_free(pPlan);

	return apFinalTempBuffer->GetColorBuffer(0)->ToTexture();
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// POST EFFECT HANDLER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPostEffectHandler::cLuxPostEffectHandler() : iLuxUpdateable("LuxPostEffectHandler")
{
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cResources *pResources = gpBase->mpEngine->GetResources();

	///////////////////////
	// Create post effects
	mpInsanity = hplNew(cLuxPostEffect_Insanity, (pGraphics, pResources) );
	AddEffect(mpInsanity, 25);
	mpInsanity->SetActive(true);
}

//-----------------------------------------------------------------------

cLuxPostEffectHandler::~cLuxPostEffectHandler()
{
	STLDeleteAll(mvPostEffects);
}

//-----------------------------------------------------------------------

void cLuxPostEffectHandler::OnStart()
{

}

//-----------------------------------------------------------------------

void cLuxPostEffectHandler::Update(float afTimeStep)
{
	for(size_t i=0; i<mvPostEffects.size(); ++i)
	{
		iLuxPostEffect *pPostEffect = mvPostEffects[i];

        if(pPostEffect->IsActive()) pPostEffect->Update(afTimeStep);
	}
}

//-----------------------------------------------------------------------

void cLuxPostEffectHandler::Reset()
{

}

//-----------------------------------------------------------------------

void cLuxPostEffectHandler::LoadMainConfig()
{
	cConfigFile *pMainCfg = gpBase->mpMainConfig;

	mpInsanity->SetDisabled(pMainCfg->GetBool("Graphics", "PostEffectInsanity", true)==false);

}

//-----------------------------------------------------------------------

void cLuxPostEffectHandler::SaveMainConfig()
{
	cConfigFile *pMainCfg = gpBase->mpMainConfig;

	pMainCfg->SetBool("Graphics", "PostEffectInsanity", mpInsanity->IsDisabled()==false);
}

//-----------------------------------------------------------------------

void cLuxPostEffectHandler::AddEffect(iLuxPostEffect *apPostEffect, int alPrio)
{
	mvPostEffects.push_back(apPostEffect);
	apPostEffect->SetActive(false);
	gpBase->mpMapHandler->GetViewport()->GetPostEffectComposite()->AddPostEffect(apPostEffect, alPrio);
}

//-----------------------------------------------------------------------
