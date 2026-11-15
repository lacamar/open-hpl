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

#include "LuxMapHandler.h"

cLuxPostEffect_Insanity::cLuxPostEffect_Insanity(cGraphics *apGraphics, cResources *apResources) : iLuxPostEffect(apGraphics, apResources)
{
	cParserVarContainer vars;
	vars.Add("UseUv");
	mpProgram = mpGraphics->CreateGpuProgramFromShaders("LuxInsanity","deferred_base_vtx.glsl", "posteffect_insanity_frag.glsl", &vars);
	if(mpProgram)
	{
		mpProgram->GetVariableAsId("afAlpha",0);
		mpProgram->GetVariableAsId("afT",1);
		mpProgram->GetVariableAsId("avScreenSize",2);
		mpProgram->GetVariableAsId("afAmpT",3);
		mpProgram->GetVariableAsId("afWaveAlpha",4);
		mpProgram->GetVariableAsId("afZoomAlpha",5);
	}

	mvAmpMaps.resize(3);

	for(size_t i=0; i<mvAmpMaps.size(); ++i)
		mvAmpMaps[i] = mpResources->GetTextureManager()->Create2D("posteffect_insanity_ampmap"+cString::ToString((int)i), false);

	mpZoomMap = mpResources->GetTextureManager()->Create2D("posteffect_insanity_zoom.jpg", false);

	OhplA12aInitHost();
	mlH = ohpl_a12a_ins_new();
}

iTexture* cLuxPostEffect_Insanity::RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
{
	cVector2f vScreenSize(0);
	if(mpProgram) vScreenSize = mpLowLevelGraphics->GetScreenSizeFloat();
	if(OhplA12Enabled() && OhplA12Frame() >= 10)
		OhplA12Log("p_in %s %s %d", OhplA12F(vScreenSize.x).c_str(), OhplA12F(vScreenSize.y).c_str(), mpProgram != NULL ? 1 : 0);
	ohpl_a12a_ins_render(mlH, vScreenSize.x, vScreenSize.y, mpProgram != NULL, (size_t)this, (size_t)apInputTexture, (size_t)apFinalTempBuffer);

	return apFinalTempBuffer->GetColorBuffer(0)->ToTexture();
}

void cLuxPostEffect_Insanity::Replay(const OhplGa9bPOp *apOps, size_t alOps, iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
{
	iTexture *vTextures[5] = { apInputTexture, mvAmpMaps[0], mvAmpMaps[1], mvAmpMaps[2], mpZoomMap };
	auto Tex = [&](int i) { return i >= 0 ? vTextures[i] : (iTexture*)NULL; };

	for(size_t n=0; n<alOps; ++n)
	{
		const OhplGa9bPOp &op = apOps[n];
		if(OhplA12Enabled() && OhplA12Frame() >= 10)
			OhplA12Log("P %u %d %d %s %s", op.kind, op.a, op.b, op.kind == kOhplGa9bPFloat && (op.b == 1 || op.b == 3) ? "~" : OhplA12F(op.f[0]).c_str(), OhplA12F(op.f[1]).c_str());
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
}

cLuxPostEffectHandler::cLuxPostEffectHandler() : iLuxUpdateable("LuxPostEffectHandler")
{
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cResources *pResources = gpBase->mpEngine->GetResources();

	mpInsanity = hplNew(cLuxPostEffect_Insanity, (pGraphics, pResources) );
	mpInsanity->SetActive(false);
	gpBase->mpMapHandler->GetViewport()->GetPostEffectComposite()->AddPostEffect(mpInsanity, 25);
	mpInsanity->SetActive(true);
}

void cLuxPostEffectHandler::LoadMainConfig()
{
	cConfigFile *pMainCfg = gpBase->mpMainConfig;

	mpInsanity->SetDisabled(pMainCfg->GetBool("Graphics", "PostEffectInsanity", true)==false);
}

void cLuxPostEffectHandler::SaveMainConfig()
{
	cConfigFile *pMainCfg = gpBase->mpMainConfig;

	pMainCfg->SetBool("Graphics", "PostEffectInsanity", mpInsanity->IsDisabled()==false);
}
