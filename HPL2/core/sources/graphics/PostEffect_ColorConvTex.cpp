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

#include "graphics/PostEffect_ColorConvTex.h"

#include "graphics/Graphics.h"
#include "resources/Resources.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Texture.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"

#include "resources/TextureManager.h"

#include "system/PreprocessParser.h"
#include "system/String.h"

#include "math/Math.h"

namespace hpl {
	
	//////////////////////////////////////////////////////////////////////////
	// PROGRAM VARS
	//////////////////////////////////////////////////////////////////////////

	#define kVar_afFadeAlpha	0

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT BASE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cPostEffectType_ColorConvTex::cPostEffectType_ColorConvTex(cGraphics *apGraphics, cResources *apResources) : iPostEffectType("ColorConvTex",apGraphics,apResources)
	{
		///////////////////////////
		// Load programs
		for(int i=0; i<2; ++i)
		{
			cParserVarContainer vars;
			vars.Add("UseUv");
			if(i==1) vars.Add("UseFadeAlpha");

			mpProgram[i] = mpGraphics->CreateGpuProgramFromShaders("BloomBlur"+cString::ToString(i),"deferred_base_vtx.glsl",
																	"posteffect_color_conv_tex_frag.glsl", &vars);
			if(i==1)
			{
				if(mpProgram[i])
				{
					mpProgram[i]->GetVariableAsId("afFadeAlpha",kVar_afFadeAlpha);
				}
			}
		}
	}
	
	//-----------------------------------------------------------------------

	cPostEffectType_ColorConvTex::~cPostEffectType_ColorConvTex()
	{

	}

	//-----------------------------------------------------------------------

	iPostEffect * cPostEffectType_ColorConvTex::CreatePostEffect(iPostEffectParams *apParams)
	{
		cPostEffect_ColorConvTex *pEffect = hplNew(cPostEffect_ColorConvTex, (mpGraphics,mpResources,this));
		cPostEffectParams_ColorConvTex *pBloomParams = static_cast<cPostEffectParams_ColorConvTex*>(apParams);

		return pEffect;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPostEffect_ColorConvTex::cPostEffect_ColorConvTex(cGraphics *apGraphics,cResources *apResources, iPostEffectType *apType) : iPostEffect(apGraphics,apResources,apType)
	{
		mpSpecificType = static_cast<cPostEffectType_ColorConvTex*>(mpType);

		mpColorConvTex = NULL;
	}

	//-----------------------------------------------------------------------

	cPostEffect_ColorConvTex::~cPostEffect_ColorConvTex()
	{

	}

	//-----------------------------------------------------------------------

	void cPostEffect_ColorConvTex::OnSetParams()
	{
		if(mParams.msTextureFile == "") return;

		if(mpColorConvTex)
		{
			mpResources->GetTextureManager()->Destroy(mpColorConvTex);
		}

		mpColorConvTex = mpResources->GetTextureManager()->Create1D(mParams.msTextureFile,false);
		mpColorConvTex->SetWrapSTR(eTextureWrap_ClampToEdge);
	}

	//-----------------------------------------------------------------------


	iTexture* cPostEffect_ColorConvTex::RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
	{
		iTexture *vTextures[3] = {apInputTexture, mpColorConvTex, apFinalTempBuffer->GetColorBuffer(0)->ToTexture()};
		iGpuProgram *vPrograms[2] = {mpSpecificType->mpProgram[0], mpSpecificType->mpProgram[1]};

		OhplPePlan *pPlan = ohpl_pe_plan_color_conv(mParams.mfFadeAlpha, vPrograms[0] != NULL, vPrograms[1] != NULL);
		return RunPlan(pPlan, vTextures, NULL, vPrograms, apFinalTempBuffer);
	}

	//-----------------------------------------------------------------------

}
