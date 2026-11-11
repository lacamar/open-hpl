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

#include "scene/RopeEntity.h"
#include "scene/RustLight.h"

#include "math/Math.h"

#include "graphics/Graphics.h"
#include "graphics/Material.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"

#include "resources/Resources.h"
#include "resources/MaterialManager.h"

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/Scene.h"

#include "physics/PhysicsRope.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cRopeEntity::cRopeEntity(const tString& asName, cResources *apResources,cGraphics *apGraphics,
								iPhysicsRope *apRope, int alMaxSegments) :	iRenderable(asName)
	{
		mpMaterialManager = apResources->GetMaterialManager();
		mpLowLevelGraphics = apGraphics->GetLowLevel();
		
		mColor = cColor(1,1,1,1);

		mpMaterial = NULL;
		
		mpRope = apRope;
		mlMaxSegments = alMaxSegments;

		mfRadius = mpRope->GetParticleRadius();
		mfLengthTileAmount = 1;
		mfLengthTileSize = 1;

		mpVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(	eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Dynamic,
																4 * mlMaxSegments, 6 * mlMaxSegments);
		
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		mpVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);

		for(int i=0; i<mlMaxSegments; ++i)
		{
			cVector3f vTexCoords[4] = {cVector3f(1,1,0),	//Bottom left
										cVector3f(-1,1,0),	//Bottom right
										cVector3f(-1,-1,0),	//Top left
										cVector3f(1,-1,0)};	//Top right

			for(int j=0;j<4;j++)
			{
				mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position,	0);
				mpVtxBuffer->AddVertexColor(eVertexBufferElement_Color0,	mColor);
				mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, 0);
				mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal,	cVector3f(0,0,1));
			}

			for(int j=0;j<3;j++) mpVtxBuffer->AddIndex(j + i*4);
			for(int j=2;j<5;j++) mpVtxBuffer->AddIndex( (j==4?0:j)  + i*4);
		}

		mpVtxBuffer->Compile(eVertexCompileFlag_CreateTangents);

		mbApplyTransformToBV = false;

		mlLastUpdateCount = -1;
		mbMultiplyAlphaWithColor = false;
	}

	//-----------------------------------------------------------------------

	cRopeEntity::~cRopeEntity()
	{
		if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
		if(mpVtxBuffer) hplDelete(mpVtxBuffer);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cRopeEntity::SetMultiplyAlphaWithColor(bool abX)
	{
		if(mbMultiplyAlphaWithColor == abX) return;

		mbMultiplyAlphaWithColor = abX;
	}

	//-----------------------------------------------------------------------

	void cRopeEntity::SetColor(const cColor &aColor)
	{
		if(mColor == aColor) return;

		mColor = aColor;

		float *pColors = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Color0);

		ohpl_rope_set_colors(&mColor, mbMultiplyAlphaWithColor, pColors, mlMaxSegments * 4);

		mpVtxBuffer->UpdateData(eVertexElementFlag_Color0,false);
	}

	//-----------------------------------------------------------------------

	void cRopeEntity::SetMaterial(cMaterial * apMaterial)
	{
		mpMaterial = apMaterial;
	}

	//-----------------------------------------------------------------------

	cBoundingVolume* cRopeEntity::GetBoundingVolume()
	{
		if(mlLastUpdateCount != mpRope->GetUpdateCount())
		{
			mvPoints.clear();
			cVerletParticleIterator it = mpRope->GetParticleIterator();
			while(it.HasNext()) mvPoints.push_back(it.Next()->GetPosition());

			cVector3f vMin, vMax;
			ohpl_rope_bounds(mvPoints.data(), (int)mvPoints.size(), mfRadius, &vMin, &vMax);
			mBoundingVolume.SetLocalMinMax(vMin, vMax);

			mlLastUpdateCount = mpRope->GetUpdateCount();
		}

		return &mBoundingVolume;
	}

	//-----------------------------------------------------------------------

	void cRopeEntity::UpdateGraphicsForFrame(float afFrameTime)
	{
		
	}

	//-----------------------------------------------------------------------

	bool cRopeEntity::UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime)
	{
		float *pPosArray = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
		float *pUvArray = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Texture0);
		float *pNrmArray = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Normal);
		float *pTanArray = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Texture1Tangent);

		mvPoints.clear();
		cVerletParticleIterator it = mpRope->GetParticleIterator();
		while(it.HasNext())
		{
			if((int)mvPoints.size() >= mlMaxSegments) break;
			cVerletParticle *pPart = it.Next();
			mvPoints.push_back(mvPoints.empty() ? pPart->GetPosition() : pPart->GetSmoothPosition());
		}
		int lCount = (int)mvPoints.size();

		OhplRopeParams params;
		params.forward = apFrustum->GetForward();
		params.radius = mfRadius;
		params.segment_length = mpRope->GetSegmentLength();
		params.tile_amount = mfLengthTileAmount;
		params.tile_size = mfLengthTileSize;
		ohpl_rope_quads(mvPoints.data(), lCount, &params, pPosArray, pUvArray, pNrmArray, pTanArray, mlMaxSegments);

		mpVtxBuffer->SetElementNum((lCount-1) * 6);

		mpVtxBuffer->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Texture0 | eVertexElementFlag_Texture1 | eVertexElementFlag_Normal, false);

		return true;
	}

	//-----------------------------------------------------------------------

	cMatrixf* cRopeEntity::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL)return &GetWorldMatrix();
		
		return NULL;
	}

	//-----------------------------------------------------------------------

	int cRopeEntity::GetMatrixUpdateCount()
	{
		return GetTransformUpdateCount();
	}

	//-----------------------------------------------------------------------

	bool cRopeEntity::IsVisible()
	{
		if(mColor.r <= 0 && mColor.g <= 0 && mColor.b <= 0) return false;

		return mbIsVisible;
	}

	
	//-----------------------------------------------------------------------


	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}
