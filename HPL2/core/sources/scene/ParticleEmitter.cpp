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

#include "scene/ParticleEmitter.h"

#include "system/LowLevelSystem.h"

#include "resources/Resources.h"
#include "resources/MaterialManager.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Renderer.h"
#include "graphics/Material.h"

#include "scene/Camera.h"
#include "math/Math.h"

#include "engine/Engine.h"
#include "scene/Scene.h"
#include "scene/World.h"

#include "scene/ParticleSystem.h"
#include "impl/RustBackends.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// DATA LOADER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iParticleEmitterData::iParticleEmitterData(const tString &asName,cResources* apResources,
		cGraphics *apGraphics)
	{
		msName = asName;
		mpResources = apResources;
		mpGraphics = apGraphics;

		mfWarmUpTime =0;
		mfWarmUpStepsPerSec = 20;
	}

	//-----------------------------------------------------------------------

	iParticleEmitterData::~iParticleEmitterData()
	{
		for(int i=0;i<(int)mvMaterials.size();i++)
		{
			if(mvMaterials[i]) mpResources->GetMaterialManager()->Destroy(mvMaterials[i]);
		}
	}
	//-----------------------------------------------------------------------

	void iParticleEmitterData::AddMaterial(cMaterial *apMaterial)
	{
		mvMaterials.push_back(apMaterial);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iParticleEmitter::iParticleEmitter(tString asName,tMaterialVec *avMaterials,
										unsigned int alMaxParticles, cVector3f avSize,
										cGraphics *apGraphics,cResources *apResources) 
		:iRenderable(asName)
	{

		mpGraphics = apGraphics;
		mpResources = apResources;

		mlPoolSize = alMaxParticles;
		mpRust = NULL;

		mvMaterials = avMaterials;

		//////////////////////////////////
		//Create vertex buffer
		mpVtxBuffer = apGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware,
																	eVertexBufferDrawType_Tri, eVertexBufferUsageType_Stream,
																	alMaxParticles*4, alMaxParticles*6);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Texture1Tangent,eVertexBufferElementFormat_Float,4);
		
		//////////////////////////////////
		//Fill the indices with quads
		for(int i=0;i<(int)alMaxParticles;i++)
		{
			int lStart = i*4;
			for(int j=0;j<3;j++) mpVtxBuffer->AddIndex(lStart + j);
			for(int j=2;j<5;j++) mpVtxBuffer->AddIndex(lStart + (j==4?0:j));
		}

		//////////////////////////////////
		//Fill with texture coords (will do for most particle systems)
		for(int i=0;i<(int)alMaxParticles;i++)
		{
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(1,1,0));
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(0,1,0));
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(0,0,0));
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(1,0,0));
		}

		//////////////////////////////////
		//Set default values for pos and col
		for(int i=0;i<(int)alMaxParticles*4;i++){
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, 0);
			mpVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1));
			mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal, cVector3f(0,0,1));
			mpVtxBuffer->AddVertexVec4f(eVertexBufferElement_Texture1Tangent, cVector3f(1,0,0), 1);
		}
		
		////////////////////////////////////
		// Compile vertex buffer
		mpVtxBuffer->Compile(0);

		
		////////////////////////////////////
		//Setup vars
		//mCtl starts with a high sleep count to make sure a looping particle system reaches equilibrium. 5 secs should eb enough!
		mvDirection = cVector3f(0,0,0);

		mvMaxDrawSize =0;

		mbApplyTransformToBV = false; 

		mBoundingVolume.SetSize(0);
		mBoundingVolume.SetPosition(0);

		mDrawType = eParticleEmitterType_FixedPoint;
		mCoordSystem = eParticleEmitterCoordSystem_World;
		
		mbUsesDirection = false;	//If Direction should be udpdated
		
	}

	//-----------------------------------------------------------------------

	iParticleEmitter::~iParticleEmitter()
	{
		ohpl_pe_free(mpRust);

		hplDelete(mpVtxBuffer);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//Seems like this fucntion is never called any more...
	void iParticleEmitter::UpdateLogic(float afTimeStep)
	{
		if(IsActive()==false) return;

		if(OhplBackendRustRequested())
		{
			uint32_t lGate = ohpl_h12b_pe_update_gate(&mCtl, true, IsDying(), iRenderer::GetRenderFrameCount() == mlRenderFrameCount,
													mbUsesDirection, GetMatrixUpdateCount());
			if(lGate == 0) return;
			if(lGate & kOhplH12GateDirection)
			{
				cMatrixf mtxInv = cMath::MatrixInverse(GetWorldMatrix());
				mvDirection = mtxInv.GetUp();
			}
			UpdateMotion(afTimeStep);
			SetTransformUpdated();
			return;
		}

		//////////////////////////////
		// Update sleep
		if(IsDying()==false && iRenderer::GetRenderFrameCount() != mlRenderFrameCount)
		{
			if(mCtl.sleep_count <= 0)return;
			mCtl.sleep_count--;
		}
		else
		{
			if(mCtl.sleep_count<10) mCtl.sleep_count =10;
		}

		//////////////////////////////
		// Update vars
		mCtl.update_gfx = true;
		mCtl.update_bv = true;

		//////////////////////////////
		// Update direction
		if(mbUsesDirection)
		{
			if(mCtl.direction_update_count != GetMatrixUpdateCount())
			{
				mCtl.direction_update_count = GetMatrixUpdateCount();
				cMatrixf mtxInv = cMath::MatrixInverse(GetWorldMatrix());
				mvDirection = mtxInv.GetUp();
			}
		}

		UpdateMotion(afTimeStep);

		SetTransformUpdated();
	}

	//-----------------------------------------------------------------------

	cMaterial* iParticleEmitter::GetMaterial()
	{
		if(mvMaterials->empty()) return NULL;
		return (*mvMaterials)[(int)ohpl_pe_frame(mpRust)];
	}

	//-----------------------------------------------------------------------

	static inline void SetCol(float *apCol, const cColor &aCol)
	{
		apCol[0] = aCol.r;
		apCol[1] = aCol.g;
		apCol[2] = aCol.b;
		apCol[3] = aCol.a;
	}

	static inline void SetPos(float *apPos, const cVector3f &aPos)
	{
		apPos[0] = aPos.x;
		apPos[1] = aPos.y;
		apPos[2] = aPos.z;
	}

	static inline cColor LinearColor(const cColor &aC)
	{
		return aC * cColor(aC.r, aC.g, aC.b, 1);
	}

	static inline void SetTex(float *apTex, const cVector3f &aPos)
	{
		apTex[0] = aPos.x;
		apTex[1] = aPos.y;
		apTex[2] = aPos.z;
	}

	bool iParticleEmitter::UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime)
	{
		const bool bLinear = cParticleSystem::mbHpl3Color;
		OhplPeGfxIn in;
		in.mbHpl3Color = bLinear;
		in.mbAddBlend = false;
		in.mbInvertsCull = apFrustum->GetInvertsCullMode();
		in.mPsColor = mpParentSystem->mColor;
		in.mfBrightness = mpParentSystem->mfBrightness;
		in.mLightColor = mpParentSystem->mLightColor;
		in.mfLightBrightness = mpParentSystem->mfLightBrightness;
		in.mbFadeAtDistance = mpParentSystem->mbFadeAtDistance;
		in.mvOrigin = apFrustum->GetOrigin();
		in.mfMinFadeStart = mpParentSystem->mfMinFadeDistanceStart;
		in.mfMinFadeEnd = mpParentSystem->mfMinFadeDistanceEnd;
		in.mfMaxFadeStart = mpParentSystem->mfMaxFadeDistanceStart;
		in.mfMaxFadeEnd = mpParentSystem->mfMaxFadeDistanceEnd;
		in.mvDrawSize = mvDrawSize;
		in.m_mtxView = apFrustum->GetViewMatrix();
		in.mlPosStride = mpVtxBuffer->GetElementNum(eVertexBufferElement_Position);

		if(bLinear)
		{
			cMaterial *pMat = GetMaterial();
			in.mbAddBlend = pMat && pMat->GetBlendMode() == eMaterialBlendMode_Add;
			in.m_mtxWorld = GetWorldMatrix();
		}
		if(in.mbFadeAtDistance)
			in.mvFadeCenter = mpParentSystem->GetBoundingVolume()->GetWorldCenter();
		if(mCoordSystem == eParticleEmitterCoordSystem_Local)
			in.m_mtxPsWorld = mpParentSystem->GetWorldMatrix();
		if(mDrawType == eParticleEmitterType_Axis)
		{
			const bool bAxisUpdate = OhplBackendRustRequested() ?
				ohpl_h12b_pe_axis_gate(&mCtl, true, GetMatrixUpdateCount()) : mCtl.axis_update_count != GetMatrixUpdateCount();
			if(bAxisUpdate)
			{
				mCtl.axis_update_count = GetMatrixUpdateCount();
				cMatrixf mtxInv = cMath::MatrixInverse(GetWorldMatrix());
				mvRight = mtxInv.GetRight();
				mvForward = mtxInv.GetForward();
			}
			in.mvAxisRight = mvRight;
			in.mvAxisForward = mvForward;
		}

		const size_t lQuads = mlPoolSize;
		const bool bSubDiv = ohpl_pe_sub_div_count(mpRust) > 1;
		float *pPosArray = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
		float *pColArray = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Color0);
		float *pTexArray = bSubDiv ? mpVtxBuffer->GetFloatArray(eVertexBufferElement_Texture0) : NULL;

		int lRes = ohpl_pe_build_vertices(mpRust, &in, pPosArray, lQuads*4*in.mlPosStride,
										  pColArray, lQuads*16, pTexArray, bSubDiv ? lQuads*12 : 0);
		if(lRes == 0)
		{
			mpVtxBuffer->SetElementNum(0);
			return false;
		}
		if(lRes == 2)
		{
			mpVtxBuffer->SetElementNum(ohpl_pe_num_particles(mpRust) * 6);

			if(bSubDiv)
				mpVtxBuffer->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0, false);
			else
				mpVtxBuffer->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Color0, false);
		}
		return true;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* iParticleEmitter::GetVertexBuffer()
	{
		return mpVtxBuffer;
	}

	//-----------------------------------------------------------------------

	bool iParticleEmitter::IsVisible()
	{ 
		if(IsActive()==false) return false;
		return mbIsVisible; 
	}

	//-----------------------------------------------------------------------

	cBoundingVolume* iParticleEmitter::GetBoundingVolume()
	{
		bool bUpdate, bAxisUpdate;
		if(OhplBackendRustRequested())
		{
			const uint32_t lGate = ohpl_h12b_pe_bv_gate(&mCtl, mDrawType == eParticleEmitterType_Axis, GetMatrixUpdateCount());
			bUpdate = lGate & 1;
			bAxisUpdate = lGate & 2;
		}
		else
		{
			bUpdate = mCtl.update_bv;
			bAxisUpdate = bUpdate && mDrawType == eParticleEmitterType_Axis && mCtl.axis_update_count != GetMatrixUpdateCount();
			if(bAxisUpdate) mCtl.axis_update_count = GetMatrixUpdateCount();
		}

		if(bUpdate)
		{
			if(bAxisUpdate)
			{
				cMatrixf mtxInv = cMath::MatrixInverse(GetWorldMatrix());
				mvRight = mtxInv.GetRight();
				mvForward = mtxInv.GetForward();
			}

			cVector3f vWorldPos = GetWorldPosition();
			cVector3f vMin, vMax;
			ohpl_pe_bounds(mpRust, &vWorldPos, &mvRight, &mvForward, &mvMaxDrawSize, &vMin, &vMax);

			mBoundingVolume.SetLocalMinMax(vMin, vMax);

			if(mCoordSystem == eParticleEmitterCoordSystem_Local)
			{
				mBoundingVolume.SetTransform(mpParentSystem->GetWorldMatrix());
			}

			mCtl.update_bv = false;
		}

		return &mBoundingVolume;
	}

	//-----------------------------------------------------------------------

	cMatrixf* iParticleEmitter::GetModelMatrix(cFrustum *apFrustum)
	{
		if(OhplBackendRustRequested())
		{
			switch(ohpl_h12b_pe_model_kind(apFrustum != NULL, mDrawType == eParticleEmitterType_Axis))
			{
			case 0: return &GetWorldMatrix();
			case 1: m_mtxTemp = cMatrixf::Identity; break;
			default: m_mtxTemp = cMath::MatrixInverse(apFrustum->GetViewMatrix()); break;
			}
			return &m_mtxTemp;
		}

		if(apFrustum)
		{
			if(mDrawType == eParticleEmitterType_Axis)
			{
				m_mtxTemp = cMatrixf::Identity;
			}
			//This is really not good...
			else if(mCoordSystem == eParticleEmitterCoordSystem_World)
			{
				m_mtxTemp = cMath::MatrixInverse(apFrustum->GetViewMatrix());
			}
			else
			{
				m_mtxTemp = cMath::MatrixInverse(apFrustum->GetViewMatrix());
				//m_mtxTemp = cMath::MatrixMul(cMath::MatrixInverse(apCamera->GetViewMatrix()), 
				//								GetWorldMatrix());
			}

			//m_mtxTemp.SetTranslation(cVector3f(0,0,0));//GetWorldMatrix().GetTranslation());

			//m_mtxTemp = cMatrixf::Identity;

			//cMatrixf mtxCam = apCamera->GetViewMatrix();
			//Log("MATRIX: %s\n",mtxCam.ToString().c_str());

			return &m_mtxTemp;

		}
		else
		{
			return &GetWorldMatrix();
		}
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}
