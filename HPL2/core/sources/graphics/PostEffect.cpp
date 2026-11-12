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

#include "graphics/PostEffect.h"
#include "impl/RustBackends.h"

#include "graphics/Graphics.h"

#include "graphics/PostEffectComposite.h"
#include "graphics/FrameBuffer.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Texture.h"
#include "graphics/GPUProgram.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT BASE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	iPostEffectType::iPostEffectType(const tString& asName, cGraphics *apGraphics, cResources *apResources)
	{
		mpGraphics = apGraphics;
		mpResources = apResources;

		msName = asName;
	}
	
	//-----------------------------------------------------------------------

	iPostEffectType::~iPostEffectType()
	{

	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// POST EFFECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iPostEffect::iPostEffect(cGraphics *apGraphics,cResources *apResources, iPostEffectType *apType)
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		mpType = apType;

		mbFinalFrameBufferUsed = false;

		mpLowLevelGraphics = mpGraphics->GetLowLevel();

		mbActive = true;
		mbDisabled = false;

		mpRs = NULL;
		if(OhplBackendRustRequested())
		{
			const tString sName = mpType ? mpType->GetName() : tString();
			mpRs = ohpl_h7_pe_new(mpType != NULL, sName.data(), sName.size());
		}
	}

	//-----------------------------------------------------------------------

	iPostEffect::~iPostEffect()
	{
		if(mpRs) ohpl_h7_pe_free(mpRs);
	}

	void iPostEffect::SetDisabled(bool abX)
	{
		mbDisabled = abX;
		if(mpRs) ohpl_h7_pe_set_disabled(mpRs, abX);
	}

	//-----------------------------------------------------------------------

	iTexture* iPostEffect::Render(cPostEffectComposite *apComposite, iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer, bool abLastEffect)
	{
		///////////////////////////
		// Set up variables and data
		mpCurrentComposite = apComposite;
		mbIsLastEffect = abLastEffect;

		mbFinalFrameBufferUsed = false;
		if(mpRs) ohpl_h7_pe_begin_render(mpRs);

		///////////////////////////
		// Render
		iTexture *pOutputTex = RenderEffect(apInputTexture, apFinalTempBuffer);
		
		//////////////////////////
		// If last effect and final frame buffer has not been called, copy o rendertarget
		if(mpRs ? ohpl_h7_pe_needs_final_copy(mpRs, mbIsLastEffect) : (mbIsLastEffect && mbFinalFrameBufferUsed == false))
		{
			iTexture *vTextures[1] = {pOutputTex};
			iFrameBuffer *vFrameBuffers[1] = {mpCurrentComposite->GetCurrentRenderTarget()->mpFrameBuffer};
			RunPlan(ohpl_pe_plan_final_copy(), vTextures, vFrameBuffers, NULL, NULL);
		}

		return pOutputTex;
	}
	
	//-----------------------------------------------------------------------

	void iPostEffect::SetActive(bool abX)
	{
		if(mpRs){ if(ohpl_h7_pe_set_active(mpRs, abX) == false) return; }
		else if(mbActive == abX) return;

		mbActive = abX;

		OnSetActive(abX);
	}
	
	//-----------------------------------------------------------------------

	void iPostEffect::SetParams(iPostEffectParams *apSrcParams)
	{
		if(mpType==NULL) return;

		//Make sure the type is correct!
		if(mpRs){ if(ohpl_h7_pe_accepts(mpRs, apSrcParams->GetName().data(), apSrcParams->GetName().size()) == false) return; }
		else if(apSrcParams->GetName() != mpType->GetName()) return;

        GetTypeSpecificParams()->LoadFrom(apSrcParams);
		OnSetParams();
	}
	
	void iPostEffect::GetParams(iPostEffectParams *apDestParams)
	{
		if(mpType==NULL) return;

		//Make sure the type is correct!
		if(mpRs){ if(ohpl_h7_pe_accepts(mpRs, apDestParams->GetName().data(), apDestParams->GetName().size()) == false) return; }
		else if(apDestParams->GetName() != mpType->GetName()) return;

		GetTypeSpecificParams()->CopyTo(apDestParams);
	}

	//-----------------------------------------------------------------------

	void iPostEffect::SetFinalFrameBuffer(iFrameBuffer *apOutputBuffer)
	{
		mbFinalFrameBufferUsed = true;
		if(mpRs) ohpl_h7_pe_mark_final_used(mpRs);

		///////////////////////
		// Set the frame buffer
		if(mbIsLastEffect)
		{
			cRenderTarget *pRenderTarget = mpCurrentComposite->GetCurrentRenderTarget();
			mpCurrentComposite->SetFrameBuffer(pRenderTarget->mpFrameBuffer, true);
		}
		else
		{
			mpCurrentComposite->SetFrameBuffer(apOutputBuffer, true);
		}
	}

	//-----------------------------------------------------------------------

	void iPostEffect::GetTextureUvPosAndSize(const cVector2f& avTexSize, cVector2f& avUvPos,  cVector2f& avUvSize)
	{
		cVector2f vScreenSizeFloat = mpLowLevelGraphics->GetScreenSizeFloat();
		cRenderTarget *pRenderTarget = mpCurrentComposite->GetCurrentRenderTarget();
		cVector2l vRenderTargetSize = mpCurrentComposite->GetRenderTargetSize();

		OhplVec2f vPos, vSize;
		ohpl_pe_uv_pos_size(OhplVec2f{avTexSize.x, avTexSize.y}, OhplVec2f{vScreenSizeFloat.x, vScreenSizeFloat.y},
							OhplVec2l{pRenderTarget->mvPos.x, pRenderTarget->mvPos.y},
							OhplVec2l{vRenderTargetSize.x, vRenderTargetSize.y}, &vPos, &vSize);
		avUvPos = cVector2f(vPos.x, vPos.y);
		avUvSize = cVector2f(vSize.x, vSize.y);
	}

	//-----------------------------------------------------------------------

	void iPostEffect::SetFrameBuffer(iFrameBuffer *apFrameBuffer)
	{
		iTexture *pTex = apFrameBuffer->GetColorBuffer(0)->ToTexture();

		/////////////////////
		//Check if texture is same size as screen, if so no need do any extra calcs
		if(pTex->GetSizeInt2D() == mpLowLevelGraphics->GetScreenSizeInt())
		{
			mpCurrentComposite->SetFrameBuffer(apFrameBuffer,true);
		}
		/////////////////////
		//Texture does not have same size as screen, need to do extra calculations
		else
		{
			cVector2f vTexSize = pTex->GetSizeFloat2D();
			cVector2f vUvPos, vUvSize;
			GetTextureUvPosAndSize(vTexSize,vUvPos, vUvSize);

			OhplVec2l vTargetPos, vTargetSize;
			ohpl_pe_round_target(OhplVec2f{vUvPos.x, vUvPos.y}, OhplVec2f{vUvSize.x, vUvSize.y}, &vTargetPos, &vTargetSize);

			mpLowLevelGraphics->SetCurrentFrameBuffer(apFrameBuffer, cVector2l(vTargetPos.x, vTargetPos.y),
													  cVector2l(vTargetSize.x, vTargetSize.y));
		}
	}

	//-----------------------------------------------------------------------

	void iPostEffect::QuadUv(iTexture *apTexture, cVector2f& avMin, cVector2f& avMax)
	{
		cVector2f vTexSize = apTexture->GetSizeFloat2D();
		cVector2f vUvPos, vUvSize;
		GetTextureUvPosAndSize(vTexSize,vUvPos,vUvSize);

		OhplVec2f vMin, vMax;
		ohpl_pe_quad_uv(OhplVec2f{vTexSize.x, vTexSize.y}, apTexture->GetType() == eTextureType_Rect,
						OhplVec2f{vUvPos.x, vUvPos.y}, OhplVec2f{vUvSize.x, vUvSize.y}, &vMin, &vMax);
		avMin = cVector2f(vMin.x, vMin.y);
		avMax = cVector2f(vMax.x, vMax.y);
	}

	//-----------------------------------------------------------------------

	void iPostEffect::DrawQuad(const cVector3f& avPos,  const cVector2f& avSize, iTexture *apTexture, bool abFlipY)
	{
		cVector2f vMin, vMax;
		QuadUv(apTexture, vMin, vMax);
		mpCurrentComposite->DrawQuad(avPos,avSize,vMin,vMax,abFlipY);
	}

	//-----------------------------------------------------------------------

	void iPostEffect::DrawQuad(	const cVector3f& avPos,  const cVector2f& avSize, iTexture *apTexture0, iTexture *apTexture1,
								bool abFlipY0,bool abFlipY1)
	{
		cVector2f vTexMin[2], vTexMax[2];
		QuadUv(apTexture0, vTexMin[0], vTexMax[0]);
		QuadUv(apTexture1, vTexMin[1], vTexMax[1]);

		mpCurrentComposite->DrawQuad(cVector2f(0,0),1,vTexMin[0],vTexMax[0],vTexMin[1],vTexMax[1],abFlipY0,abFlipY1);
	}

	//-----------------------------------------------------------------------

	iTexture* iPostEffect::RunPlan(OhplPePlan *apPlan, iTexture **apTextures, iFrameBuffer **apFrameBuffers,
								   iGpuProgram **apPrograms, iFrameBuffer *apFinalTempBuffer)
	{
		cPostEffectComposite *c = mpCurrentComposite;
		auto Tex = [&](int i) { return i >= 0 ? apTextures[i] : (iTexture*)NULL; };
		auto Fb = [&](int i) { return i >= 0 ? apFrameBuffers[i] : (iFrameBuffer*)NULL; };
		auto Prog = [&](int i) { return i >= 0 ? apPrograms[i] : (iGpuProgram*)NULL; };
		OhplPeOp op;
		while(ohpl_pe_plan_next(apPlan, &op))
		{
			switch(op.kind)
			{
			case eOhplPeOp_FlatProjection:		c->SetFlatProjection(); break;
			case eOhplPeOp_BlendNone:			c->SetBlendMode(eMaterialBlendMode_None); break;
			case eOhplPeOp_BlendAlpha:			c->SetBlendMode(eMaterialBlendMode_Alpha); break;
			case eOhplPeOp_ChannelRgba:			c->SetChannelMode(eMaterialChannelMode_RGBA); break;
			case eOhplPeOp_TextureRangeNull:	c->SetTextureRange(NULL, op.a); break;
			case eOhplPeOp_CompFb:				c->SetFrameBuffer(Fb(op.a)); break;
			case eOhplPeOp_CompFbPos:			c->SetFrameBuffer(Fb(op.a), true); break;
			case eOhplPeOp_FinalFb:				SetFinalFrameBuffer(apFinalTempBuffer); break;
			case eOhplPeOp_BaseFb:				SetFrameBuffer(Fb(op.a)); break;
			case eOhplPeOp_Program:				c->SetProgram(Prog(op.a)); break;
			case eOhplPeOp_ClearColor:			c->ClearFrameBuffer(eClearFrameBufferFlag_Color, true); break;
			case eOhplPeOp_Tex:					c->SetTexture(op.a, Tex(op.b)); break;
			case eOhplPeOp_Float:				Prog(op.a)->SetFloat(op.b, op.f[0]); break;
			case eOhplPeOp_Vec2:				Prog(op.a)->SetVec2f(op.b, cVector2f(op.f[0], op.f[1])); break;
			case eOhplPeOp_Vec3:				Prog(op.a)->SetVec3f(op.b, cVector3f(op.f[0], op.f[1], op.f[2])); break;
			case eOhplPeOp_Quad1:				DrawQuad(0, 1, Tex(op.a), op.b != 0); break;
			case eOhplPeOp_Quad2:				DrawQuad(0, 1, Tex(op.a), Tex(op.b), (op.c & 1) != 0, (op.c & 2) != 0); break;
			}
		}
		int lResult = ohpl_pe_plan_result(apPlan);
		iTexture *pResult = lResult >= 0 ? apTextures[lResult] : NULL;
		ohpl_pe_plan_free(apPlan);
		return pResult;
	}

	//-----------------------------------------------------------------------

}
