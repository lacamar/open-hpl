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

#include "graphics/RenderFunctions.h"

#include "graphics/RenderList.h"
#include "graphics/FrameBuffer.h"
#include "graphics/GPUProgram.h"
#include "graphics/VertexBuffer.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Texture.h"
#include "graphics/Graphics.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"
#include "math/Frustum.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// RUST BACKEND CALLBACKS
	//////////////////////////////////////////////////////////////////////////

	static inline uintptr_t PtrId(const void *apPtr){ return (uintptr_t)apPtr; }
	static inline OhplVec2l ToOhpl(const cVector2l& avVec){ OhplVec2l v = {avVec.x, avVec.y}; return v; }
	static inline OhplVec2f ToOhpl(const cVector2f& avVec){ OhplVec2f v = {avVec.x, avVec.y}; return v; }

	struct cRenderFunctionsBackend
	{
		static iRenderFunctions* Self(void *apCtx){ return static_cast<iRenderFunctions*>(static_cast<OhplRenderState*>(apCtx)); }

		static void DepthTest(void *c, bool b){ Self(c)->mpLowLevelGraphics->SetDepthTestActive(b); }
		static void DepthWrite(void *c, bool b){ Self(c)->mpLowLevelGraphics->SetDepthWriteActive(b); }
		static void DepthFunc(void *c, unsigned f){ Self(c)->mpLowLevelGraphics->SetDepthTestFunc((eDepthTestFunc)f); }
		static void CullActive(void *c, bool b){ Self(c)->mpLowLevelGraphics->SetCullActive(b); }
		static void CullMode(void *c, unsigned m){ Self(c)->mpLowLevelGraphics->SetCullMode((eCullMode)m); }
		static void StencilActive(void *c, bool b){ Self(c)->mpLowLevelGraphics->SetStencilActive(b); }
		static void ScissorActive(void *c, bool b){ Self(c)->mpLowLevelGraphics->SetScissorActive(b); }
		static void ScissorRect(void *c, OhplVec2l p, OhplVec2l s){ Self(c)->mpLowLevelGraphics->SetScissorRect(cVector2l(p.x, p.y), cVector2l(s.x, s.y)); }
		static void ColorWrite(void *c, bool r, bool g, bool b, bool a){ Self(c)->mpLowLevelGraphics->SetColorWriteActive(r, g, b, a); }
		static void AlphaTestActive(void *c, bool b){ Self(c)->mpLowLevelGraphics->SetAlphaTestActive(b); }
		static void AlphaTestGreaterOrEqual(void *c, float l){ Self(c)->mpLowLevelGraphics->SetAlphaTestFunc(eAlphaTestFunc_GreaterOrEqual, l); }
		static void BlendActive(void *c, bool b){ Self(c)->mpLowLevelGraphics->SetBlendActive(b); }
		static void BlendFunc(void *c, unsigned s, unsigned d){ Self(c)->mpLowLevelGraphics->SetBlendFunc((eBlendFunc)s, (eBlendFunc)d); }
		static void ProgramBind(void*, uintptr_t p){ reinterpret_cast<iGpuProgram*>(p)->Bind(); }
		static void ProgramUnbind(void*, uintptr_t p){ reinterpret_cast<iGpuProgram*>(p)->UnBind(); }
		static void Texture(void *c, int u, uintptr_t t){ Self(c)->mpLowLevelGraphics->SetTexture(u, reinterpret_cast<iTexture*>(t)); }
		static void VertexBufferUnbind(void*, uintptr_t v){ reinterpret_cast<iVertexBuffer*>(v)->UnBind(); }
		static void VertexBufferBind(void*, uintptr_t v){ reinterpret_cast<iVertexBuffer*>(v)->Bind(); }
		static void SetProjection(void *c, uintptr_t m){ Self(c)->mpLowLevelGraphics->SetMatrix(eMatrix_Projection, *reinterpret_cast<const cMatrixf*>(m)); }

		static void SetModelMatrix(void *c, uintptr_t m)
		{
			iRenderFunctions *pSelf = Self(c);
			if(m)
			{
				cMatrixf mtxModel = cMath::MatrixMul(pSelf->mpCurrentFrustum->GetViewMatrix(), *reinterpret_cast<cMatrixf*>(m));
				pSelf->mpLowLevelGraphics->SetMatrix(eMatrix_ModelView,mtxModel);
			}
			else
			{
				pSelf->mpLowLevelGraphics->SetMatrix(eMatrix_ModelView,pSelf->mpCurrentFrustum->GetViewMatrix());
			}
		}

		static void SetFrameBuffer(void *c, uintptr_t fb, bool abUsePosAndSize, OhplVec2l p, OhplVec2l s)
		{
			iLowLevelGraphics *pLlg = Self(c)->mpLowLevelGraphics;
			if(abUsePosAndSize)	pLlg->SetCurrentFrameBuffer(reinterpret_cast<iFrameBuffer*>(fb), cVector2l(p.x, p.y), cVector2l(s.x, s.y));
			else				pLlg->SetCurrentFrameBuffer(reinterpret_cast<iFrameBuffer*>(fb));
		}

		static void FrameBufferSize(void*, uintptr_t fb, OhplVec2l *apOut)
		{
			cVector2l vSize = reinterpret_cast<iFrameBuffer*>(fb)->GetSize();
			apOut->x = vSize.x;
			apOut->y = vSize.y;
		}

		static void ClearFrameBuffer(void *c, unsigned f){ Self(c)->mpLowLevelGraphics->ClearFrameBuffer(f); }

		static void LogEvent(void *c, int alEvent, int64_t a, int64_t b, int64_t cc, int64_t d, float f)
		{
			iRenderFunctions *pSelf = Self(c);
			switch(alEvent)
			{
			case eOhplRfLog_Projection:
				{
					const cMatrixf *pMtx = reinterpret_cast<const cMatrixf*>(a);
					Log(" Setting projection matrix: %d  %s.\n",(int)a, pMtx->ToString().c_str());
				}
				break;
			case eOhplRfLog_DepthTest:		Log("  Setting depth test active: %d\n", (int)a); break;
			case eOhplRfLog_DepthWrite:		Log("  Setting depth write active: %d\n", (int)a); break;
			case eOhplRfLog_DepthFunc:
				{
					tString sFunc="Unknown";
					switch((eDepthTestFunc)a)
					{
					case eDepthTestFunc_Never:			sFunc = "Never"; break;
					case eDepthTestFunc_Less:			sFunc = "Less"; break;
					case eDepthTestFunc_LessOrEqual:	sFunc = "LessOrEqual"; break;
					case eDepthTestFunc_Greater:		sFunc = "Greater"; break;
					case eDepthTestFunc_GreaterOrEqual:	sFunc = "GreaterOrEqual"; break;
					case eDepthTestFunc_Equal:			sFunc = "Equal"; break;
					case eDepthTestFunc_NotEqual:		sFunc = "NotEqual"; break;
					case eDepthTestFunc_Always:			sFunc = "Always"; break;
					default: break;
					}
					Log("  Setting depth func: %s\n", sFunc.c_str());
				}
				break;
			case eOhplRfLog_CullActive:		Log("  Setting cull active: %d\n",(int)a); break;
			case eOhplRfLog_CullMode:
				{
					tString sMode="Unknown";
					switch((eCullMode)a)
					{
					case eCullMode_Clockwise:			sMode = "Clockwise"; break;
					case eCullMode_CounterClockwise:	sMode = "CounterClockwise"; break;
					default: break;
					}
					Log("  Setting cull mode: %s\n", sMode.c_str());
				}
				break;
			case eOhplRfLog_StencilActive:	Log("  Setting stencil active: %d\n",(int)a); break;
			case eOhplRfLog_ScissorActive:	Log("  Setting scissor active: %d\n",(int)a); break;
			case eOhplRfLog_ScissorRect:	Log("  Setting scissor rect: %d, %d, %dx%d\n", (int)a, (int)b, (int)cc, (int)d); break;
			case eOhplRfLog_ChannelMode:
				switch((eMaterialChannelMode)a)
				{
				case eMaterialChannelMode_RGBA:	Log("  Setting channel mode: RGBA\n"); break;
				case eMaterialChannelMode_RGB:	Log("  Setting channel mode: RGB\n"); break;
				case eMaterialChannelMode_A:	Log("  Setting channel mode: A\n"); break;
				case eMaterialChannelMode_None:	Log("  Setting channel mode: None\n"); break;
				default: break;
				}
				break;
			case eOhplRfLog_AlphaMode:		Log(a ? "  Setting alpha mode: Solid\n" : "  Setting alpha mode: Trans\n"); break;
			case eOhplRfLog_AlphaLimit:		Log("  Setting alpha limit: %f\n", f); break;
			case eOhplRfLog_BlendMode:
				switch((eMaterialBlendMode)a)
				{
				case eMaterialBlendMode_None:			Log("  Setting blend mode: None\n"); break;
				case eMaterialBlendMode_Add:			Log("  Setting blend mode: Add\n"); break;
				case eMaterialBlendMode_Mul:			Log("  Setting blend mode: Mul\n"); break;
				case eMaterialBlendMode_MulX2:			Log("  Setting blend mode: MulX2\n"); break;
				case eMaterialBlendMode_Alpha:			Log("  Setting blend mode: Alpha\n"); break;
				case eMaterialBlendMode_PremulAlpha:	Log("  Setting blend mode: PremulAlpha\n"); break;
				case eMaterialBlendMode_ModulateSource:	Log("  Setting blend mode: ModulateSource\n"); break;
				default: break;
				}
				break;
			case eOhplRfLog_Program:		Log("  Setting gpu program %d : '%s'\n", (int)a, reinterpret_cast<iGpuProgram*>(a)->GetName().c_str()); break;
			case eOhplRfLog_ProgramNull:	Log("  Setting gpu program NULL\n"); break;
			case eOhplRfLog_Texture:
				if(b)	Log("  Setting texture unit: %d, %d/'%s'\n",(int)a,(int)b,reinterpret_cast<iTexture*>(b)->GetName().c_str());
				else	Log("  Setting texture unit: %d, 'NULL\n",(int)a);
				break;
			case eOhplRfLog_VertexBuffer:		Log("  Setting vertex buffer: %d\n",(int)a); break;
			case eOhplRfLog_VertexBufferNull:	Log("  Setting vertex buffer: NULL\n"); break;
			case eOhplRfLog_ModelMatrix:		Log("  Setting model matrix: %d / %s\n",(int)a, reinterpret_cast<cMatrixf*>(a)->ToString().c_str()); break;
			case eOhplRfLog_ModelMatrixNull:	Log("  Setting model matrix: NULL\n"); break;
			case eOhplRfLog_InvertCull:			Log(" Inverting Cull Modes!\n"); break;
			case eOhplRfLog_FrameBufferPos:
				{
					iFrameBuffer *pFb = reinterpret_cast<iFrameBuffer*>(a);
					Log(" Setting FrameBuffer %d '%s', pos: %d,%d size: %dx%d\n",	(int)a, pFb ? pFb->GetName().c_str() : "",
						(int)b, (int)cc, pSelf->mvRenderTargetSize.x, pSelf->mvRenderTargetSize.y);
				}
				break;
			case eOhplRfLog_FrameBufferDefault:
				{
					iFrameBuffer *pFb = reinterpret_cast<iFrameBuffer*>(a);
					Log(" Setting FrameBuffer %d '%s', default size\n", (int)a, pFb ? pFb->GetName().c_str() : "");
				}
				break;
			case eOhplRfLog_ClearPortionGlobal:
			case eOhplRfLog_ClearPortionTarget:
				Log(" Clearing portion of frame buffer. Pos: %d,%d Size: %dx%d\n", (int)a, (int)b, (int)cc, (int)d);
				break;
			case eOhplRfLog_ClearAll:		Log(" Clearing entire frame buffer\n"); break;
			}
		}
	};

	static void SetupBackend(OhplRfBackend& aBackend, OhplRenderState *apState)
	{
		aBackend.ctx = apState;
		aBackend.depthTest = cRenderFunctionsBackend::DepthTest;
		aBackend.depthWrite = cRenderFunctionsBackend::DepthWrite;
		aBackend.depthFunc = cRenderFunctionsBackend::DepthFunc;
		aBackend.cullActive = cRenderFunctionsBackend::CullActive;
		aBackend.cullMode = cRenderFunctionsBackend::CullMode;
		aBackend.stencilActive = cRenderFunctionsBackend::StencilActive;
		aBackend.scissorActive = cRenderFunctionsBackend::ScissorActive;
		aBackend.scissorRect = cRenderFunctionsBackend::ScissorRect;
		aBackend.colorWrite = cRenderFunctionsBackend::ColorWrite;
		aBackend.alphaTestActive = cRenderFunctionsBackend::AlphaTestActive;
		aBackend.alphaTestGreaterOrEqual = cRenderFunctionsBackend::AlphaTestGreaterOrEqual;
		aBackend.blendActive = cRenderFunctionsBackend::BlendActive;
		aBackend.blendFunc = cRenderFunctionsBackend::BlendFunc;
		aBackend.programBind = cRenderFunctionsBackend::ProgramBind;
		aBackend.programUnbind = cRenderFunctionsBackend::ProgramUnbind;
		aBackend.texture = cRenderFunctionsBackend::Texture;
		aBackend.vertexBufferUnbind = cRenderFunctionsBackend::VertexBufferUnbind;
		aBackend.vertexBufferBind = cRenderFunctionsBackend::VertexBufferBind;
		aBackend.setProjection = cRenderFunctionsBackend::SetProjection;
		aBackend.setModelMatrix = cRenderFunctionsBackend::SetModelMatrix;
		aBackend.setFrameBuffer = cRenderFunctionsBackend::SetFrameBuffer;
		aBackend.frameBufferSize = cRenderFunctionsBackend::FrameBufferSize;
		aBackend.clearFrameBuffer = cRenderFunctionsBackend::ClearFrameBuffer;
		aBackend.log = cRenderFunctionsBackend::LogEvent;
	}

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	void iRenderFunctions::SetupRenderFunctions(iLowLevelGraphics *apLowLevelGraphics)
	{
		mpLowLevelGraphics = apLowLevelGraphics;
		SetupBackend(mBackend, this);

		mvScreenSize = mpLowLevelGraphics->GetScreenSizeInt();
		mvScreenSizeFloat = mpLowLevelGraphics->GetScreenSizeFloat();
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::InitAndResetRenderFunctions(	cFrustum *apFrustum, cRenderTarget *apRenderTarget, bool abLog,
														bool abUseGlobalScissorRect,
														const cVector2l& avGlobalScissorRectPos, const cVector2l& avGlobalScissorRectSize)
	{
		mpCurrentFrustum = apFrustum;
		mpCurrentRenderTarget = apRenderTarget;
		SetupBackend(mBackend, this);

		cVector2l vScreenSize = mpLowLevelGraphics->GetScreenSizeInt();
		cVector2f vScreenSizeFloat = mpLowLevelGraphics->GetScreenSizeFloat();
		cVector2l vFrameBufferSize = mpCurrentRenderTarget->mpFrameBuffer ? mpCurrentRenderTarget->mpFrameBuffer->GetSize() : vScreenSize;

		ohpl_rf_init(this, PtrId(&m_mtxNULL), abLog, abUseGlobalScissorRect, ToOhpl(avGlobalScissorRectPos), ToOhpl(avGlobalScissorRectSize),
					 ToOhpl(vScreenSize), ToOhpl(vScreenSizeFloat), ToOhpl(vFrameBufferSize), ToOhpl(mpCurrentRenderTarget->mvSize));
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::ExitAndCleanUpRenderFunctions()
	{
		ohpl_rf_exit(this, &mBackend);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	void iRenderFunctions::SetFlatProjection(const cVector2f &avSize,float afMin,float afMax)
	{
		if(mbLog) Log(" Setting Ortho Projection size: %fx%f, minz: %f maxz: %f\n",avSize.x, avSize.y, afMin,afMax);

		mpLowLevelGraphics->SetOrthoProjection(avSize,afMin,afMax);
		mpLowLevelGraphics->SetIdentityMatrix(eMatrix_ModelView);
		ohpl_rf_flat_projection_set(this, &mBackend, PtrId(&m_mtxNULL));
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::SetFlatProjectionMinMax(const cVector3f &avMin,const cVector3f &avMax)
	{
		if(mbLog) Log(" Setting Ortho Projection min: %s, max: %s\n",avMin.ToString().c_str(), avMax.ToString().c_str());

		mpLowLevelGraphics->SetOrthoProjection(avMin,avMax);
		mpLowLevelGraphics->SetIdentityMatrix(eMatrix_ModelView);
		ohpl_rf_flat_projection_set(this, &mBackend, PtrId(&m_mtxNULL));
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::SetNormalFrustumProjection()
	{
		SetFrustumProjection(mpCurrentFrustum);
	}

	void iRenderFunctions::SetFrustumProjection(cFrustum *apFrustum)
	{
		SetProjectionMatrix(&apFrustum->GetProjectionMatrix());

		SetInvertCullMode(mpCurrentFrustum->GetInvertsCullMode());
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::SetProjectionMatrix(const cMatrixf *apProjMatrix)
	{
		ohpl_rf_set_projection_matrix(this, &mBackend, PtrId(&m_mtxNULL), PtrId(apProjMatrix));
	}

	//-----------------------------------------------------------------------

	bool iRenderFunctions::SetDepthTest(bool abX){ return ohpl_rf_set_depth_test(this, &mBackend, abX); }
	bool iRenderFunctions::SetDepthWrite(bool abX){ return ohpl_rf_set_depth_write(this, &mBackend, abX); }
	bool iRenderFunctions::SetDepthTestFunc(eDepthTestFunc aFunc){ return ohpl_rf_set_depth_test_func(this, &mBackend, aFunc); }
	bool iRenderFunctions::SetCullActive(bool abX){ return ohpl_rf_set_cull_active(this, &mBackend, abX); }
	bool iRenderFunctions::SetCullMode(eCullMode aMode, bool abCheckIfInverted){ return ohpl_rf_set_cull_mode(this, &mBackend, aMode, abCheckIfInverted); }
	bool iRenderFunctions::SetStencilActive(bool abX){ return ohpl_rf_set_stencil_active(this, &mBackend, abX); }

	//-----------------------------------------------------------------------

	void iRenderFunctions::SetStencilWriteMask(unsigned int alMask)
	{
		mpLowLevelGraphics->SetStencilWriteMask(alMask);
	}

	void iRenderFunctions::SetStencil(	eStencilFunc aFunc,int alRef, unsigned int aMask,
										eStencilOp aFailOp,eStencilOp aZFailOp,eStencilOp aZPassOp)
	{
		mpLowLevelGraphics->SetStencil(aFunc, alRef, aMask, aFailOp, aZFailOp, aZPassOp);
	}

	void iRenderFunctions::SetStencilTwoSide(	eStencilFunc aFrontFunc,eStencilFunc aBackFunc,
												int alRef, unsigned int aMask,
												eStencilOp aFrontFailOp,eStencilOp aFrontZFailOp,eStencilOp aFrontZPassOp,
												eStencilOp aBackFailOp,eStencilOp aBackZFailOp,eStencilOp aBackZPassOp)
	{
		mpLowLevelGraphics->SetStencilTwoSide(	aFrontFunc, aBackFunc,
												alRef, aMask,
												aFrontFailOp, aFrontZFailOp, aFrontZPassOp,
												aBackFailOp, aBackZFailOp, aBackZPassOp);
	}

	//-----------------------------------------------------------------------

	bool iRenderFunctions::SetScissorActive(bool abX)
	{
		return ohpl_rf_set_scissor_active(this, &mBackend, abX, ToOhpl(mpCurrentRenderTarget->mvPos));
	}

	//-----------------------------------------------------------------------

	bool iRenderFunctions::SetScissorRect(const cRect2l& aClipRect, bool abAutoEnabling)
	{
		return SetScissorRect(cVector2l(aClipRect.x, aClipRect.y), cVector2l(aClipRect.w, aClipRect.h), abAutoEnabling);
	}

	bool iRenderFunctions::SetScissorRect(const cVector2l& avPos, const cVector2l& avSize, bool abAutoEnabling)
	{
		return ohpl_rf_set_scissor_rect(this, &mBackend, ToOhpl(avPos), ToOhpl(avSize), abAutoEnabling, ToOhpl(mpCurrentRenderTarget->mvPos));
	}

	//-----------------------------------------------------------------------

	bool iRenderFunctions::SetChannelMode(eMaterialChannelMode aMode){ return ohpl_rf_set_channel_mode(this, &mBackend, aMode); }
	bool iRenderFunctions::SetAlphaMode(eMaterialAlphaMode aMode){ return ohpl_rf_set_alpha_mode(this, &mBackend, aMode); }
	bool iRenderFunctions::SetAlphaLimit(float afLimit){ return ohpl_rf_set_alpha_limit(this, &mBackend, afLimit); }
	bool iRenderFunctions::SetBlendMode(eMaterialBlendMode aMode){ return ohpl_rf_set_blend_mode(this, &mBackend, aMode); }
	bool iRenderFunctions::SetProgram(iGpuProgram *apProgram){ return ohpl_rf_set_program(this, &mBackend, PtrId(apProgram)); }
	void iRenderFunctions::SetTexture(int alUnit, iTexture *apTexture){ ohpl_rf_set_texture(this, &mBackend, alUnit, PtrId(apTexture)); }

	void iRenderFunctions::SetTextureRange(iTexture *apTexture, int alFirstUnit, int alLastUnit)
	{
		ohpl_rf_set_texture_range(this, &mBackend, PtrId(apTexture), alFirstUnit, alLastUnit);
	}

	void iRenderFunctions::SetVertexBuffer(iVertexBuffer *apVtxBuffer){ ohpl_rf_set_vertex_buffer(this, &mBackend, PtrId(apVtxBuffer)); }
	void iRenderFunctions::SetMatrix(cMatrixf *apMatrix){ ohpl_rf_set_matrix(this, &mBackend, PtrId(apMatrix)); }

	//-----------------------------------------------------------------------

	void iRenderFunctions::SetModelViewMatrix(const cMatrixf& a_mtxModelView)
	{
		//No test here...
		if(mbLog) Log("  Setting view matrix: %s\n",cMath::MatrixToChar(a_mtxModelView));

		mpLowLevelGraphics->SetMatrix(eMatrix_ModelView,a_mtxModelView);
		ohpl_rf_model_view_set(this, PtrId(&m_mtxNULL));
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::SetInvertCullMode(bool abX){ ohpl_rf_set_invert_cull_mode(this, &mBackend, abX); }

	//-----------------------------------------------------------------------

	void iRenderFunctions::SetFrameBuffer(iFrameBuffer *apFrameBuffer, bool abUsePosAndSize, bool abUseGlobalScissor)
	{
		ohpl_rf_set_frame_buffer(this, &mBackend, PtrId(apFrameBuffer), abUsePosAndSize, abUseGlobalScissor, ToOhpl(mpCurrentRenderTarget->mvPos));
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::ClearFrameBuffer(tClearFrameBufferFlag aFlags, bool abUsePosAndSize)
	{
		ohpl_rf_clear_frame_buffer(this, &mBackend, aFlags, abUsePosAndSize, ToOhpl(mpCurrentRenderTarget->mvPos));
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::DrawQuad(	const cVector3f& aPos, const cVector2f& avSize, const cVector2f& avMinUV, const cVector2f& avMaxUV,
										bool abInvertY, const cColor& aColor)
	{
		if(mbLog){
			Log("   Drawing quad to: pos (%s) size (%s) uv: (%s)-(%s)\n",	aPos.ToString().c_str(), 
				avSize.ToString().c_str(),
				avMinUV.ToString().c_str(),
				avMaxUV.ToString().c_str());
		}
		if(abInvertY)
		{
			mpLowLevelGraphics->DrawQuad(	aPos, avSize,cVector2f(avMinUV.x, avMaxUV.y),
				cVector2f(avMaxUV.x, avMinUV.y),aColor);
		}
		else
		{
			mpLowLevelGraphics->DrawQuad(aPos, avSize,avMinUV,avMaxUV,aColor);
		}
	}
	
	//-----------------------------------------------------------------------

	void iRenderFunctions::DrawQuad(	const cVector3f& aPos, const cVector2f& avSize, 
										const cVector2f& avMinUV0, const cVector2f& avMaxUV0, const cVector2f& avMinUV1, const cVector2f& avMaxUV1,
										bool abInvertY0,bool abInvertY1, const cColor& aColor)
	{
		if(mbLog){
			Log("   Drawing quad to: pos (%s) size (%s) uv0: (%s)-(%s) uv1: (%s)-(%s)\n",	aPos.ToString().c_str(), 
				avSize.ToString().c_str(),
				avMinUV0.ToString().c_str(),
				avMaxUV0.ToString().c_str(),
				avMinUV1.ToString().c_str(),
				avMaxUV0.ToString().c_str());
		}
		cVector2f vFinalMinUv0 = avMinUV0;
		cVector2f vFinalMaxUv0 = avMaxUV0;
		if(abInvertY0){
			vFinalMaxUv0.y = avMinUV0.y;
			vFinalMinUv0.y = avMaxUV0.y;
		}

		cVector2f vFinalMinUv1 = avMinUV1;
		cVector2f vFinalMaxUv1 = avMaxUV1;
		if(abInvertY0){
			vFinalMaxUv1.y = avMinUV1.y;
			vFinalMinUv1.y = avMaxUV1.y;
		}

		mpLowLevelGraphics->DrawQuad(aPos, avSize,vFinalMinUv0,vFinalMaxUv0,vFinalMinUv1,vFinalMaxUv1,aColor);
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::DrawCurrent(eVertexBufferDrawType aDrawType)
	{
		if(mbLog) Log("   Drawing vertex buffer\n");

		if(mpCurrentVtxBuffer) mpCurrentVtxBuffer->Draw(aDrawType);

	}
	
	//-----------------------------------------------------------------------

	void iRenderFunctions::DrawWireFrame(iVertexBuffer *apVtxBuffer, const cColor &aColor)
	{
		///////////////////////////////////////
		//Set up variables
		int lIndexNum = apVtxBuffer->GetElementNum();
		if(lIndexNum<0) lIndexNum = apVtxBuffer->GetIndexNum();
		unsigned int* pIndexArray = apVtxBuffer->GetIndices();

		float *pVertexArray = apVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
		int lVertexStride = apVtxBuffer->GetElementNum(eVertexBufferElement_Position);

		cVector3f vTriPos[3];
	
		///////////////////////////////////////
		//Iterate through each triangle and draw it as 3 lines
		for(int tri = 0; tri < lIndexNum; tri+=3)
		{
			////////////////////////
			//Set the vector with positions of the lines
			for(int idx =0; idx < 3; idx++)
			{
				int lVtx = pIndexArray[tri + 2-idx]*lVertexStride;

				vTriPos[idx].x = pVertexArray[lVtx + 0];
				vTriPos[idx].y = pVertexArray[lVtx + 1];
				vTriPos[idx].z = pVertexArray[lVtx + 2];
			}

			////////////////////////
			//Draw the three lines
			for(int i=0; i<3; ++i)
			{
				int lNext = i==2 ? 0 : i+1;
				mpLowLevelGraphics->DrawLine(vTriPos[i],vTriPos[lNext], aColor);
			}
		}

	}
	
	//-----------------------------------------------------------------------

	iTexture* iRenderFunctions::CreateRenderTexture(const tString& asName, const cVector2l& avSize, ePixelFormat aPixelFormat,
													eTextureFilter aFilter, eTextureType aType)
	{
		iTexture *pTexture =NULL;
		pTexture = mpGraphics->CreateTexture(asName,aType,eTextureUsage_RenderTarget);
		if(pTexture->CreateFromRawData(cVector3l(avSize.x, avSize.y,0),aPixelFormat, NULL)==false)
		{
			Error("Could not create texture '%s'\n", asName.c_str());
			return pTexture;
		}
				
		pTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
		pTexture->SetFilter(aFilter);

		return pTexture;
	}

	//-----------------------------------------------------------------------

	void iRenderFunctions::CopyFrameBufferToTexure(	iTexture *apTexture, const cVector2l& avPos, const cVector2l& avSize, const cVector2l& avTextureOffset,
													bool abTextureOffsetUsesRenderTargetPos)
	{
		if(mbLog)
		{
			if(abTextureOffsetUsesRenderTargetPos)
				Log(" Copying current to texture '%s' (%d). Pos: %d:%d Size: %dx%d TextureOffset: %d:%d\n",
								apTexture->GetName().c_str(), apTexture,
								avPos.x, avPos.y, avSize.x, avSize.y, 
								avTextureOffset.x+mpCurrentRenderTarget->mvPos.x, 
								avTextureOffset.y+mpCurrentRenderTarget->mvPos.y);
			else
				Log(" Copying current to texture '%s' (%d). Pos: %d:%d Size: %dx%d TextureOffset: %d:%d\n",
								apTexture->GetName().c_str(), apTexture,
								avPos.x, avPos.y, avSize.x, avSize.y, avTextureOffset.x, avTextureOffset.y);
								

		}
		if(abTextureOffsetUsesRenderTargetPos)
			mpLowLevelGraphics->CopyFrameBufferToTexure(apTexture, avPos, avSize, avTextureOffset + mpCurrentRenderTarget->mvPos);
		else
			mpLowLevelGraphics->CopyFrameBufferToTexure(apTexture, avPos, avSize, avTextureOffset);
		mvCurrentTexture[0] = apTexture;
	}

	//-----------------------------------------------------------------------

}
