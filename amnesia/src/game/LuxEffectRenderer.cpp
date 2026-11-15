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
#include <cstring>

#define kVar_afBlurSize			0
#define kVar_afColorMul			1

cLuxEffectRenderer::cLuxEffectRenderer() : iLuxUpdateable("LuxEffectRenderer")
{
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cVector2l vScreenSize = pGraphics->GetLowLevel()->GetScreenSizeInt();

	cRendererDeferred *pRendererDeferred = static_cast<cRendererDeferred*>(pGraphics->GetRenderer(eRenderer_Main));

	mpDeferredAccumBuffer = pRendererDeferred->GetAccumBuffer();

	mpOutlineColorTexture = pGraphics->GetTempFrameBuffer(vScreenSize,ePixelFormat_RGBA,0)->GetColorBuffer(0)->ToTexture();

	mpFrameBufferColor = pGraphics->CreateFrameBuffer("OutlineColor");
	mpFrameBufferColor->SetDepthStencilBuffer(pRendererDeferred->GetDepthStencilBuffer());
	mpFrameBufferColor->SetTexture2D(0,mpOutlineColorTexture);

	mpFrameBufferColor->CompileAndValidate();

	cParserVarContainer programVars;

	programVars.Add("UseUv");
	programVars.Add("UseNormals");
	mpFlashProgram = pGraphics->CreateGpuProgramFromShaders("GameOutline","deferred_base_vtx.glsl", "game_object_flash_frag.glsl",&programVars);
	mpFlashProgram->GetVariableAsId("afColorMul",kVar_afColorMul);
	programVars.Clear();

	programVars.Add("UseUv");
	programVars.Add("UseNormals");
	mpEnemyGlowProgram = pGraphics->CreateGpuProgramFromShaders("EnemyGlow","deferred_base_vtx.glsl", "game_enemy_darkness_glow_frag.glsl",&programVars);
	mpEnemyGlowProgram->GetVariableAsId("afColorMul",kVar_afColorMul);
	programVars.Clear();

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

	uint32_t lPrograms = 0;
	if(mpFlashProgram) lPrograms |= 1u << kOhplGa9bProgFlash;
	if(mpEnemyGlowProgram) lPrograms |= 1u << kOhplGa9bProgGlow;
	if(mpOutlineColorProgram[0]) lPrograms |= 1u << kOhplGa9bProgOutlineColor0;
	if(mpOutlineColorProgram[1]) lPrograms |= 1u << kOhplGa9bProgOutlineColor1;
	if(mpOutlineStencilProgram) lPrograms |= 1u << kOhplGa9bProgOutlineStencil;
	if(mpOutlineStencilAlphaProgram) lPrograms |= 1u << kOhplGa9bProgOutlineStencilAlpha;
	if(mpBlurProgram[0]) lPrograms |= 1u << kOhplGa9bProgBlur0;
	if(mpBlurProgram[1]) lPrograms |= 1u << kOhplGa9bProgBlur1;
	OhplA12aInitHost();
	mlH = ohpl_a12a_rend_new(lPrograms);
}

void cLuxEffectRenderer::Replay(cRendererCallbackFunctions* apFunctions, const size_t *apObjects, const OhplGa9bROp *apOps, size_t alOps, const cRect2l &aClipRect)
{
	iGpuProgram *vPrograms[8] = { mpFlashProgram, mpEnemyGlowProgram, mpOutlineColorProgram[0], mpOutlineColorProgram[1],
									mpOutlineStencilProgram, mpOutlineStencilAlphaProgram, mpBlurProgram[0], mpBlurProgram[1] };
	iTexture *vTextures[3] = { mpOutlineColorTexture, mpBlurTexture[0], mpBlurTexture[1] };
	iFrameBuffer *vFrameBuffers[4] = { mpFrameBufferColor, mpBlurBuffer[0], mpBlurBuffer[1], mpDeferredAccumBuffer };
	iRenderable* const *vObjects = (iRenderable* const*)apObjects;

	for(size_t n=0; n<alOps; ++n)
	{
		const OhplGa9bROp &op = apOps[n];
		if(OhplA12Enabled() && OhplA12Frame() >= 10)
		{
			std::string sOp = "R " + std::to_string(op.kind) + " " + std::to_string(op.i) + " " + std::to_string(op.j);
			for(int k=0; k<16; ++k) sOp += " " + OhplA12F(op.f[k]);
			OhplA12Log("%s", sOp.c_str());
		}
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
		case kOhplGa9bRTexDiffuse:		apFunctions->SetTexture(0, vObjects[op.i]->GetMaterial()->GetTexture(eMaterialTexture_Diffuse)); break;
		case kOhplGa9bRTexAlpha:		apFunctions->SetTexture(0, vObjects[op.i]->GetMaterial()->GetTexture(eMaterialTexture_Alpha)); break;
		case kOhplGa9bRTexNull:			apFunctions->SetTexture(0, NULL); break;
		case kOhplGa9bRTexSlot:			apFunctions->SetTexture(0, vTextures[op.i]); break;
		case kOhplGa9bRVertexObj:		apFunctions->SetVertexBuffer(vObjects[op.i]->GetVertexBuffer()); break;
		case kOhplGa9bRMatrixObj:		apFunctions->SetMatrix(vObjects[op.i]->GetModelMatrixPtr()); break;
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
}
