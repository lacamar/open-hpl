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

#include "LuxEffectRenderer.h"
#include "RustLuxFx.h"
#include <cstring>


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PROGRAM VARIABLES
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

#define kVar_afBlurSize			0
#define kVar_afColorMul			1

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffectRenderer::cLuxEffectRenderer() : iLuxUpdateable("LuxEffectRenderer")
{
	/////////////////////////////
	//Setup vars
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cVector2l vScreenSize = pGraphics->GetLowLevel()->GetScreenSizeInt();

	/////////////////////////////
	//Get deferred renderer stuff
	cRendererDeferred *pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));

	mpDeferredAccumBuffer = pRendererDeferred->GetAccumBuffer();

	/////////////////////////////
	//Create Outline color buffer
	mpOutlineColorTexture = pGraphics->GetTempFrameBuffer(vScreenSize,ePixelFormat_RGBA,0)->GetColorBuffer(0)->ToTexture();

	mpFrameBufferColor = pGraphics->CreateFrameBuffer("OutlineColor");
	mpFrameBufferColor->SetDepthStencilBuffer(pRendererDeferred->GetDepthStencilBuffer());
	mpFrameBufferColor->SetTexture2D(0,mpOutlineColorTexture);

	mpFrameBufferColor->CompileAndValidate();	

	cParserVarContainer programVars;

	/////////////////////////////
	//Load Flash programs

	programVars.Add("UseUv");
	programVars.Add("UseNormals");
	mpFlashProgram = pGraphics->CreateGpuProgramFromShaders("GameOutline","deferred_base_vtx.glsl", "game_object_flash_frag.glsl",&programVars);
	mpFlashProgram->GetVariableAsId("afColorMul",kVar_afColorMul);
	programVars.Clear();

	/////////////////////////////
	//Load Enemy Glow programs

	programVars.Add("UseUv");
	programVars.Add("UseNormals");
	mpEnemyGlowProgram = pGraphics->CreateGpuProgramFromShaders("EnemyGlow","deferred_base_vtx.glsl", "game_enemy_darkness_glow_frag.glsl",&programVars);
	mpEnemyGlowProgram->GetVariableAsId("afColorMul",kVar_afColorMul);
	programVars.Clear();
		
	/////////////////////////////
	//Load Outline programs

	//TODO: Could perhaps use deferred_base_vtx for color? Since the scale can be done to matrix.
	for(int i=0; i<2; ++i)
	{
		if(i==1) programVars.Add("UseAlpha");
		mpOutlineColorProgram[i] = pGraphics->CreateGpuProgramFromShaders("GameOutline","game_outline_vtx.glsl", "game_outline_frag.glsl",&programVars);
		programVars.Clear();
	}

	mpOutlineStencilProgram = pGraphics->CreateGpuProgramFromShaders("GameOutline","deferred_base_vtx.glsl", "deferred_base_frag.glsl",&programVars);
	
	programVars.Add("UseUv");
	programVars.Add("UseDiffuse");
	mpOutlineStencilAlphaProgram = pGraphics->CreateGpuProgramFromShaders("GameOutline","deferred_base_vtx.glsl", "deferred_base_frag.glsl",&programVars);
	programVars.Clear();

	

	///////////////////////////
	// Load Blur Programs
	mlBlurSizeDiv = 4;
	for(int i=0; i<2; ++i)
	{
		cParserVarContainer vars;
		if(i==1) vars.Add("BlurHorisontal");
		mpBlurProgram[i] = pGraphics->CreateGpuProgramFromShaders("BloomBlur","posteffect_bloom_blur_vtx.glsl", "posteffect_bloom_blur_frag.glsl", &vars);

		if(mpBlurProgram[i])
			mpBlurProgram[i]->GetVariableAsId("afBlurSize",kVar_afBlurSize);

		mpBlurBuffer[i] = pGraphics->GetTempFrameBuffer(vScreenSize/mlBlurSizeDiv,ePixelFormat_RGBA,i);
		if(mpBlurBuffer[i])
			mpBlurTexture[i] = mpBlurBuffer[i]->GetColorBuffer(0)->ToTexture();
	}


	///////////////////////////
	// Reset variables
	mFlashOscill.SetUp(0,1,0,1,1);

	///////////////////////////
	// Reset variables
	Reset();
}

//-----------------------------------------------------------------------

cLuxEffectRenderer::~cLuxEffectRenderer()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEffectRenderer::Reset()
{
	mvOutlineObjects.clear();
	ClearRenderLists();
}

void cLuxEffectRenderer::ClearRenderLists()
{
	mvFlashObjects.clear();
	mvEnemyGlowObjects.clear();
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::Update(float afTimeStep)
{
	mFlashOscill.Update(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	if(apFunctions->GetSettings()->mbIsReflection) return;

}

void cLuxEffectRenderer::RenderTrans(cRendererCallbackFunctions* apFunctions)
{
	/////////////////////////////////////
	// Normal and Reflection
	RenderFlashObjects(apFunctions);
	RenderEnemyGlow(apFunctions);

	
	/////////////////////////////////////
	// Only Normal
	if(apFunctions->GetSettings()->mbIsReflection==false)
	{
		RenderOutline(apFunctions);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::AddOutlineObject(iRenderable *apObject)
{
	mvOutlineObjects.push_back(apObject);
}

void cLuxEffectRenderer::ClearOutlineObjects()
{
	mvOutlineObjects.clear();
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::AddFlashObject(iRenderable *apObject, float afAlpha)
{
	mvFlashObjects.push_back(cGlowObject(apObject, afAlpha) );
}

void cLuxEffectRenderer::AddEnemyGlow(iRenderable *apObject, float afAlpha)
{
	mvEnemyGlowObjects.push_back(cGlowObject(apObject, afAlpha) );
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderFlashObjects(cRendererCallbackFunctions* apFunctions)
{
	if(mvFlashObjects.empty()) return;
	if(mpFlashProgram==NULL) return;

	cFrustum *pFrustum = apFunctions->GetFrustum();
	std::vector<iRenderable*> vObjects(mvFlashObjects.size());
	std::vector<OhplGa9bGlow> vGlow(mvFlashObjects.size());
	for(size_t i=0; i<mvFlashObjects.size(); ++i)
	{
		vObjects[i] = mvFlashObjects[i].mpObject;
		vGlow[i].collides = vObjects[i]->CollidesWithFrustum(pFrustum);
		vGlow[i].alpha = mvFlashObjects[i].mfAlpha;
	}
	OhplGa9bRPlan *pPlan = ohpl_ga9b_plan_flash(&vGlow[0], vGlow.size(), mFlashOscill.val, true);
	Replay(pPlan, apFunctions, vObjects, cRect2l());
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderEnemyGlow(cRendererCallbackFunctions* apFunctions)
{
	if(mvEnemyGlowObjects.empty()) return;
	if(mpEnemyGlowProgram==NULL) return;

	cFrustum *pFrustum = apFunctions->GetFrustum();
	std::vector<iRenderable*> vObjects(mvEnemyGlowObjects.size());
	std::vector<OhplGa9bGlow> vGlow(mvEnemyGlowObjects.size());
	for(size_t i=0; i<mvEnemyGlowObjects.size(); ++i)
	{
		vObjects[i] = mvEnemyGlowObjects[i].mpObject;
		vGlow[i].collides = vObjects[i]->CollidesWithFrustum(pFrustum);
		vGlow[i].alpha = mvEnemyGlowObjects[i].mfAlpha;
	}
	OhplGa9bRPlan *pPlan = ohpl_ga9b_plan_glow(&vGlow[0], vGlow.size(), true);
	Replay(pPlan, apFunctions, vObjects, cRect2l());
}

//-----------------------------------------------------------------------

void cLuxEffectRenderer::RenderOutline(cRendererCallbackFunctions* apFunctions)
{
	if(mvOutlineObjects.empty()) return;

	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cVector2l vScreenSize = pGraphics->GetLowLevel()->GetScreenSizeInt();

	cFrustum *pFrustum = apFunctions->GetFrustum();

	std::vector<OhplGa9bOutline> vOutline(mvOutlineObjects.size());
	for(size_t i=0; i<mvOutlineObjects.size(); ++i)
	{
		iRenderable *pObject = mvOutlineObjects[i];
		OhplGa9bOutline &out = vOutline[i];
		memset(&out, 0, sizeof(out));
		out.collides = pObject->CollidesWithFrustum(pFrustum);
		if(out.collides == 0) continue;

		cBoundingVolume *pBV = pObject->GetBoundingVolume();
		out.hasAlpha = pObject->GetMaterial()->GetTexture(eMaterialTexture_Alpha) != NULL;
		const cVector3f vMin = pBV->GetMin(), vMax = pBV->GetMax();
		const cVector3f vLocalMin = pBV->GetLocalMin(), vLocalMax = pBV->GetLocalMax(), vLocalCenter = pBV->GetLocalCenter();
		out.min[0] = vMin.x; out.min[1] = vMin.y; out.min[2] = vMin.z;
		out.max[0] = vMax.x; out.max[1] = vMax.y; out.max[2] = vMax.z;
		out.localMin[0] = vLocalMin.x; out.localMin[1] = vLocalMin.y; out.localMin[2] = vLocalMin.z;
		out.localMax[0] = vLocalMax.x; out.localMax[1] = vLocalMax.y; out.localMax[2] = vLocalMax.z;
		out.localCenter[0] = vLocalCenter.x; out.localCenter[1] = vLocalCenter.y; out.localCenter[2] = vLocalCenter.z;
		const cMatrixf &mtxWorld = pObject->GetWorldMatrix();
		for(int r=0; r<4; ++r)
			for(int c=0; c<4; ++c)
				out.world[r*4+c] = mtxWorld.m[r][c];
	}

	uint32_t lProgramsOk = 0;
	if(mpOutlineColorProgram[0]) lProgramsOk |= 1u << kOhplGa9bProgOutlineColor0;
	if(mpOutlineColorProgram[1]) lProgramsOk |= 1u << kOhplGa9bProgOutlineColor1;
	if(mpOutlineStencilProgram) lProgramsOk |= 1u << kOhplGa9bProgOutlineStencil;
	if(mpOutlineStencilAlphaProgram) lProgramsOk |= 1u << kOhplGa9bProgOutlineStencilAlpha;
	if(mpBlurProgram[0]) lProgramsOk |= 1u << kOhplGa9bProgBlur0;
	if(mpBlurProgram[1]) lProgramsOk |= 1u << kOhplGa9bProgBlur1;

	OhplGa9bRPlan *pPlan = ohpl_ga9b_plan_outline(&vOutline[0], vOutline.size(), lProgramsOk);

	float vClipMin[3], vClipMax[3];
	ohpl_ga9b_rplan_clip(pPlan, vClipMin, vClipMax);
	cBoundingVolume totalBV;
	totalBV.SetLocalMinMax(cVector3f(vClipMin[0], vClipMin[1], vClipMin[2]), cVector3f(vClipMax[0], vClipMax[1], vClipMax[2]));
	cRect2l clipRect;
	cMath::GetClipRectFromBV(clipRect,totalBV, apFunctions->GetFrustum(), vScreenSize,-1);

	Replay(pPlan, apFunctions, mvOutlineObjects, clipRect);
}

void cLuxEffectRenderer::Replay(OhplGa9bRPlan *apPlan, cRendererCallbackFunctions* apFunctions, const std::vector<iRenderable*> &avObjects, const cRect2l &aClipRect)
{
	iGpuProgram *vPrograms[8] = { mpFlashProgram, mpEnemyGlowProgram, mpOutlineColorProgram[0], mpOutlineColorProgram[1],
									mpOutlineStencilProgram, mpOutlineStencilAlphaProgram, mpBlurProgram[0], mpBlurProgram[1] };
	iTexture *vTextures[3] = { mpOutlineColorTexture, mpBlurTexture[0], mpBlurTexture[1] };
	iFrameBuffer *vFrameBuffers[4] = { mpFrameBufferColor, mpBlurBuffer[0], mpBlurBuffer[1], mpDeferredAccumBuffer };

	OhplGa9bROp op;
	while(ohpl_ga9b_rplan_next(apPlan, &op))
	{
		switch(op.kind)
		{
		case kOhplGa9bRDepthFunc:		apFunctions->SetDepthTestFunc(op.i == kOhplGa9bDepthEqual ? eDepthTestFunc_Equal : eDepthTestFunc_LessOrEqual); break;
		case kOhplGa9bRDepthTest:		apFunctions->SetDepthTest(op.i != 0); break;
		case kOhplGa9bRDepthWrite:		apFunctions->SetDepthWrite(op.i != 0); break;
		case kOhplGa9bRBlend:			apFunctions->SetBlendMode(op.i == kOhplGa9bBlendAdd ? eMaterialBlendMode_Add : eMaterialBlendMode_None); break;
		case kOhplGa9bRAlphaMode:		apFunctions->SetAlphaMode(op.i == kOhplGa9bAlphaTrans ? eMaterialAlphaMode_Trans : eMaterialAlphaMode_Solid); break;
		case kOhplGa9bRChannel:			apFunctions->SetChannelMode(op.i == kOhplGa9bChannelNone ? eMaterialChannelMode_None : eMaterialChannelMode_RGBA); break;
		case kOhplGa9bRProgram:			apFunctions->SetProgram(op.i >= 0 ? vPrograms[op.i] : NULL); break;
		case kOhplGa9bRProgFloat:		vPrograms[op.i]->SetFloat(op.j, op.f[0]); break;
		case kOhplGa9bRProgColor3:		vPrograms[op.i]->SetColor3f(vPrograms[op.i]->GetVariableId("gvColor"), cColor(op.f[0], op.f[1], op.f[2], op.f[3])); break;
		case kOhplGa9bRTexDiffuse:		apFunctions->SetTexture(0, avObjects[op.i]->GetMaterial()->GetTexture(eMaterialTexture_Diffuse)); break;
		case kOhplGa9bRTexAlpha:		apFunctions->SetTexture(0, avObjects[op.i]->GetMaterial()->GetTexture(eMaterialTexture_Alpha)); break;
		case kOhplGa9bRTexNull:			apFunctions->SetTexture(0, NULL); break;
		case kOhplGa9bRTexSlot:			apFunctions->SetTexture(0, vTextures[op.i]); break;
		case kOhplGa9bRVertexObj:		apFunctions->SetVertexBuffer(avObjects[op.i]->GetVertexBuffer()); break;
		case kOhplGa9bRMatrixObj:		apFunctions->SetMatrix(avObjects[op.i]->GetModelMatrixPtr()); break;
		case kOhplGa9bRMatrixNull:		apFunctions->SetMatrix(NULL); break;
		case kOhplGa9bRMatrixTemp:
			for(int r=0; r<4; ++r)
				for(int c=0; c<4; ++c)
					m_mtxTemp.m[r][c] = op.f[r*4+c];
			apFunctions->SetMatrix(&m_mtxTemp);
			break;
		case kOhplGa9bRDraw:			apFunctions->DrawCurrent(); break;
		case kOhplGa9bRFrameBuffer:		apFunctions->SetFrameBuffer(vFrameBuffers[op.i]); break;
		case kOhplGa9bRClearStencilColor:	apFunctions->ClearFrameBuffer(eClearFrameBufferFlag_Stencil | eClearFrameBufferFlag_Color,true); break;
		case kOhplGa9bRStencilActive:	apFunctions->SetStencilActive(op.i != 0); break;
		case kOhplGa9bRStencilAlwaysReplace:	apFunctions->GetLowLevelGfx()->SetStencil(eStencilFunc_Always,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Keep,eStencilOp_Replace); break;
		case kOhplGa9bRStencilNotEqualKeep:		apFunctions->GetLowLevelGfx()->SetStencil(eStencilFunc_NotEqual,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Keep,eStencilOp_Keep); break;
		case kOhplGa9bRFlatProjection:	apFunctions->SetFlatProjection(); break;
		case kOhplGa9bRNormalProjection:	apFunctions->SetNormalFrustumProjection(); break;
		case kOhplGa9bRTextureRangeNull:	apFunctions->SetTextureRange(NULL, op.i); break;
		case kOhplGa9bRScissorActive:	apFunctions->SetScissorActive(op.i != 0); break;
		case kOhplGa9bRScissorRect:		apFunctions->SetScissorRect(aClipRect,false); break;
		case kOhplGa9bRQuadMaxUv:		apFunctions->DrawQuad(0,1,vTextures[op.i]->GetSizeFloat2D(),true); break;
		case kOhplGa9bRQuadMinMax:		apFunctions->DrawQuad(0,1,0,vTextures[op.i]->GetSizeFloat2D(),true); break;
		}
	}
	ohpl_ga9b_rplan_free(apPlan);
}



//-----------------------------------------------------------------------



