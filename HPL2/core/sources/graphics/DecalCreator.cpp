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

#include "graphics/DecalCreator.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "resources/Resources.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Renderer.h"
#include "graphics/LowLevelGraphics.h"
#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"
#include "resources/AnimationManager.h"
#include "scene/MeshEntity.h"
#include "math/Math.h"
#include "graphics/RustDecal.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cDecalCreator::cDecalCreator(iLowLevelGraphics *apLowLevelGraphics, cResources *apResources) : mvDecalSize(0)
	{
		mpLowLevelGraphics = apLowLevelGraphics;
		mpResources = apResources;

		mvDecalRight = 0;
		mvDecalUp = 0;
		mvDecalForward = 0;
		mfDecalOffset = 0;

		SetDecalRight(	cVector3f(1,0,0),false);
		SetDecalUp(		cVector3f(0,1,0),false);
		SetDecalForward(cVector3f(0,0,1),false);
		SetDecalPosition(0);
		SetDecalOffset(0);
		SetDecalSize(1);
		SetUVSubDivisions(1);
		SetCurrentSubDiv(0);
		SetColor(1);

		mpDecalMaterial = NULL;
		mpDecalVB = NULL;

		mlMaxDecalTriangleCount = 250;

		mbCompiled = false;
	}

	//-----------------------------------------------------------------------

	cDecalCreator::~cDecalCreator()
	{
		if(mpDecalVB) hplDelete(mpDecalVB);
		if(mpDecalMaterial) mpResources->GetMaterialManager()->Destroy(mpDecalMaterial);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	bool cDecalCreator::AddSubMesh(cSubMeshEntity* apSubMesh)
	{
		if(apSubMesh==NULL)
			return false;

		cBoundingVolume* pMeshBV = apSubMesh->GetBoundingVolume();
		cBoundingVolume* pDecalBV = GetDecalBoundingVolume();

		if(cMath::CheckBVIntersection(*pMeshBV, *pDecalBV))
		{
			AddAffectedSubMesh(apSubMesh);
			return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::AddAffectedSubMesh(cSubMeshEntity* apSubMesh)
	{
		if(apSubMesh==NULL)
			return;

		mvMeshes.push_back(apSubMesh);

		mbUpdated = true;
	}

	void cDecalCreator::ClearMeshes()
	{
		mvMeshes.clear();

		mbUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetDecalPosition(const cVector3f& avPosition)
	{
		if(mvDecalPosition==avPosition)
			return;

		mvDecalPosition = avPosition;

		mbUpdated = true;
		mbBVUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetDecalUp(const cVector3f& avUp, bool abComputeBasis)
	{
		if(mvDecalUp==avUp)
			return;

		mvDecalUp = avUp;
		mvDecalUp.Normalize();

		if(abComputeBasis)	ComputeBasis();

		mbUpdated = true;
		mbBVUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetDecalRight(const cVector3f& avRight, bool abComputeBasis)
	{
		if(mvDecalRight==avRight)
			return;

		mvDecalRight = avRight;
		mvDecalRight.Normalize();

		if(abComputeBasis)	ComputeBasis();

		mbUpdated = true;
		mbBVUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetDecalForward(const cVector3f& avForward, bool abComputeBasis)
	{
		if(mvDecalForward==avForward)
			return;

		mvDecalForward = avForward;
		mvDecalForward.Normalize();

		if(abComputeBasis)	ComputeBasis();

		mbUpdated = true;
		mbBVUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetDecalOffset(float afOffset)
	{
		if(mfDecalOffset==afOffset)
			return;

		mfDecalOffset = afOffset;

		mbUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetDecalSize(const cVector3f& avSize)
	{
		if(mvDecalSize==avSize)
			return;

		mvDecalSize = avSize;

		mbUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetMaterial(const tString& asMat)
	{
		if(msMaterial==asMat)
			return;

		cMaterialManager* pManager = mpResources->GetMaterialManager();
		cMaterial* pMat = pManager->CreateMaterial(asMat);
		if(pMat)
		{
			if(mpDecalMaterial==pMat)
				pManager->Destroy(pMat);
			else
			{
				if(mpDecalMaterial)
					pManager->Destroy(mpDecalMaterial);

				mpDecalMaterial = pMat;
				msMaterial = asMat;
			}
		}
		else
		{
			if(mpDecalMaterial)
					pManager->Destroy(mpDecalMaterial);
			mpDecalMaterial = NULL;
			msMaterial = "";
		}

		mbUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetColor(const cColor& aCol)
	{
		if(mColor==aCol)
			return;

		mColor = aCol;

		mbUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetUVSubDivisions(const cVector2l& avSubDiv)
	{
		if(mvSubDiv==avSubDiv)
			return;

		mvSubDiv = avSubDiv;

		mbUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::SetCurrentSubDiv(int alSubDiv)
	{
		if(mlCurrentSubDiv==alSubDiv)
			return;

		mlCurrentSubDiv = alSubDiv;

		mbUpdated = true;
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	bool cDecalCreator::Compile()
	{
		if(mbUpdated)
		{
			mbCompiled = false;

			if(mvMeshes.empty())
				return false;
			if(cMath::Abs(mvDecalSize.x)<kEpsilonf || cMath::Abs(mvDecalSize.y)<kEpsilonf)
				return false;

			mbUpdated = false;

			mlDecalVertexCount = 0;
			mlDecalTriangleCount = 0;

			cVector3f vDecalHalfSize = mvDecalSize*0.5f;

			cVector3f vAxes[] = { mvDecalRight, mvDecalUp, mvDecalForward };
			float vSign[] = { 1.0f, -1.0f };

			
			mvMatrices.clear();
			mvTransformedBases.clear();
			mvClipPlanes.clear();
			for(int i=0;i<3;++i)
			{
				cVector3f vAdd = vAxes[i]*vDecalHalfSize.v[i];
				for(int j=0;j<2;++j)
				{
					mvClipPlanes.push_back(cPlanef(vAxes[i]*vSign[j], mvDecalPosition - vAdd*vSign[j]));
					//Log("ClipPlane %i %s %f\n",i+j, mvClipPlanes.back().GetNormal().ToFileString().c_str(), mvClipPlanes.back().d);
				}
			}
			
			if(mpDecalVB)
			{
				hplDelete(mpDecalVB);
				mpDecalVB = NULL;
			}
			
			mpDecalVB = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware,
																eVertexBufferDrawType_Tri,
																eVertexBufferUsageType_Static);
			mpDecalVB->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
			mpDecalVB->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
			mpDecalVB->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
			mpDecalVB->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
						
			///////////////////////////////////////////
			// Clip near triangles
			for(size_t i=0;i<mvMeshes.size();++i)
			{
				cSubMeshEntity* pAffectedMesh = mvMeshes[i];
				ClipMesh(pAffectedMesh, mpDecalVB);
			}

			const int lVtxNum = mpDecalVB->GetVertexNum();
			const int lPosStride = mpDecalVB->GetElementNum(eVertexBufferElement_Position);
			const float *pVertexPositions = mpDecalVB->GetFloatArray(eVertexBufferElement_Position);

			OhplDecalUv uvParams = { mvDecalPosition, mvDecalRight, mvDecalForward, mvDecalSize.x, mvDecalSize.z, mvSubDiv, mlCurrentSubDiv };
			tVector3fVec vUvs(lVtxNum);
			ohpl_decal_uvs(pVertexPositions, (size_t)lVtxNum*lPosStride, lPosStride, lVtxNum, &uvParams, vUvs.data());
			for(int i=0;i<lVtxNum;++i)
				mpDecalVB->AddVertexVec3f(eVertexBufferElement_Texture0, vUvs[i]);
			mbCompiled = mpDecalVB->Compile(eVertexCompileFlag_CreateTangents);
		}

		return mbCompiled;
	}
	
	bool cDecalCreator::CanCreateDecal()
	{
		mbCanCreate = Compile();

		return mbCanCreate;
	}

	//-----------------------------------------------------------------------

	cMesh* cDecalCreator::CreateDecalMesh()
	{
		if(CanCreateDecal()==false)
			return NULL;

		cMaterial* pMat = mpResources->GetMaterialManager()->CreateMaterial(msMaterial);
		if(pMat==NULL)
			return NULL;

		cMesh *pMesh = hplNew( cMesh, ("", _W(""), mpResources->GetMaterialManager(), mpResources->GetAnimationManager()) ); 
		cSubMesh* pSubMesh = pMesh->CreateSubMesh("Decal");
		pSubMesh->SetVertexBuffer(mpDecalVB->CreateCopy(eVertexBufferType_Hardware, eVertexBufferUsageType_Static, mpDecalVB->GetVertexElementFlags()));
		pSubMesh->SetMaterial(pMat);

		return pMesh;
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::DrawDebug(cRendererCallbackFunctions* apFunctions, bool abDrawAxes, bool abDrawWireframe)
	{
		apFunctions->SetMatrix(NULL);

		if(mpDecalVB)
		{
			if(mpDecalMaterial)
			{
				apFunctions->SetDepthTest(true);
				apFunctions->SetDepthWrite(false);
				apFunctions->SetBlendMode(eMaterialBlendMode_Alpha);
				apFunctions->SetAlphaMode(eMaterialAlphaMode_Solid);

				apFunctions->SetProgram(NULL);

				for(int i=0; i<kMaxTextureUnits; ++i)
				{
					iTexture *pTexture = mpDecalMaterial->GetTextureInUnit(eMaterialRenderMode_Diffuse,i);
					apFunctions->SetTexture(i, pTexture);

				}
				
				apFunctions->SetVertexBuffer(mpDecalVB);

				apFunctions->DrawCurrent();

				apFunctions->SetTextureRange(NULL,0);
				apFunctions->SetBlendMode(eMaterialBlendMode_None);
			}
			if(abDrawWireframe)	apFunctions->DrawWireFrame(mpDecalVB, cColor(1));
		}

		if(abDrawAxes)
		{
			cVector3f vHalfDecalSize = mvDecalSize*0.5f;
			cMatrixf mtxTransform = cMath::MatrixUnitVectors(mvDecalRight, mvDecalUp, mvDecalForward, mvDecalPosition);
			//cMatrixf mtxTransform = cMatrixf::Identity;
			mtxTransform.SetTranslation(mvDecalPosition);

			apFunctions->SetMatrix(&mtxTransform);
			//apFunctions->GetLowLevelGfx()->DrawSphere(0, 0.01f, 1);
			apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(vHalfDecalSize.x,0,0), cColor(1,0,0,1));
			apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(0,vHalfDecalSize.y,0), cColor(0,1,0,1));
			apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(0,0,vHalfDecalSize.z), cColor(0,0,1,1));
			apFunctions->GetLowLevelGfx()->DrawBoxMinMax(vHalfDecalSize*-1, vHalfDecalSize, 1);

			for(size_t i=0;i<mvTransformedBases.size();i+=4)
			{
				const cVector3f& vRight = mvTransformedBases[i+1];
				const cVector3f& vUp = mvTransformedBases[i+2];
				const cVector3f& vFwd = mvTransformedBases[i+3];

				cMatrixf mtxBasis = cMath::MatrixUnitVectors(vRight, vUp, vFwd, mvTransformedBases[i]);
				apFunctions->SetMatrix(&mtxBasis);
				apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(vHalfDecalSize.x,0,0), cColor(1,0,0,1));
				apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(0,vHalfDecalSize.y,0), cColor(0,1,0,1));
				apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(0,0,vHalfDecalSize.z), cColor(0,0,1,1));
				apFunctions->GetLowLevelGfx()->DrawBoxMinMax(vHalfDecalSize*-1, vHalfDecalSize, 1);
			}

			//for(int i=0;i<(int)mvMatrices.size();++i)
			//{
			//	cMatrixf& mtxTransform = mvMatrices[i];

			//	apFunctions->SetMatrix(&mtxTransform);
			//	apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(1,0,0), cColor(1,0,0,1));
			//	apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(0,1,0), cColor(0,1,0,1));
			//	apFunctions->GetLowLevelGfx()->DrawLine(0,cVector3f(0,0,1), cColor(0,0,1,1));
			//}

			apFunctions->SetMatrix(NULL);
		}

		apFunctions->SetBlendMode(eMaterialBlendMode_None);
		apFunctions->SetDepthTest(false);
	}

	cBoundingVolume* cDecalCreator::GetDecalBoundingVolume()
	{
		if(mbBVUpdated)
		{
			mbBVUpdated=false;

			cMatrixf mtxTransform = cMath::MatrixUnitVectors(mvDecalRight, mvDecalUp, mvDecalForward, 0).GetTranspose();
			mtxTransform = cMath::MatrixInverse(mtxTransform);

			mDecalBV.SetTransform(mtxTransform);
			mDecalBV.SetPosition(mvDecalPosition);
			mDecalBV.SetSize(mvDecalSize);
		}
			
		return &mDecalBV; 
	}


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cDecalCreator::ComputeBasis()
	{
		cVector3f vUp = mvDecalUp;
		cVector3f vRgt = mvDecalRight;
		cVector3f vFwd = mvDecalForward;
		cMath::Vector3OrthonormalizeBasis(vUp, vRgt, vFwd, mvDecalUp, mvDecalRight, mvDecalForward);
	}

	//-----------------------------------------------------------------------

	void cDecalCreator::ClipMesh(cSubMeshEntity* apSubMesh,iVertexBuffer* apDecalVB)
	{
		//Log("Clipping mesh %s\n", apSubMesh->GetName().c_str());

		cMatrixf mtxSubMeshWorldMatrix = apSubMesh->GetWorldMatrix();
		cMatrixf mtxInvSubMeshWorldMatrix = cMath::MatrixInverse(mtxSubMeshWorldMatrix);
		cMatrixf mtxSubMeshWorldNormalRot = mtxInvSubMeshWorldMatrix.GetRotation().GetTranspose();
		

		//////////////////////////////////////////////////
		// Transform clip planes to local submesh coordinates
		tPlanefVec vTransformedPlanes;
		for(int i=0;i<(int)mvClipPlanes.size();++i)
		{
			vTransformedPlanes.push_back(cMath::TransformPlane(mtxInvSubMeshWorldMatrix, mvClipPlanes[i]));
			//Log("Plane %d transformed normal:(%s) d:%f\n", i, vTransformedPlanes.back().GetNormal().ToFileString().c_str(), vTransformedPlanes.back().d);
		}
		OhplDecalClip clip;
		for(int i=0;i<6;++i)
			clip.planes[i] = vTransformedPlanes[i];
		clip.world = mtxSubMeshWorldMatrix;
		clip.normalRot = mtxSubMeshWorldNormalRot;
		clip.offset = mfDecalOffset;
		clip.maxTris = mlMaxDecalTriangleCount;

		iVertexBuffer* pSubMeshVB = apSubMesh->GetVertexBuffer();
		const int lVtxNum = pSubMeshVB->GetVertexNum();
		const int lPosStride = pSubMeshVB->GetElementNum(eVertexBufferElement_Position);
		const int lNrmStride = pSubMeshVB->GetElementNum(eVertexBufferElement_Normal);

		OhplDecalMesh mesh = { pSubMeshVB->GetFloatArray(eVertexBufferElement_Position), (size_t)lVtxNum*lPosStride, (uint32_t)lPosStride,
								pSubMeshVB->GetFloatArray(eVertexBufferElement_Normal), (size_t)lVtxNum*lNrmStride, (uint32_t)lNrmStride,
								pSubMeshVB->GetIndices(), (uint32_t)pSubMeshVB->GetIndexNum() };
		OhplDecalCounts counts = { mlDecalVertexCount, mlDecalTriangleCount };
		OhplDecalOut* pOut = ohpl_decal_clip_mesh(&clip, &mesh, &counts);
		mlDecalVertexCount = counts.vertices;
		mlDecalTriangleCount = counts.triangles;

		for(uint32_t i=0;i<pOut->indexCount;++i)
			apDecalVB->AddIndex(pOut->indices[i]);
		for(uint32_t i=0;i<pOut->vertexCount;++i)
		{
			apDecalVB->AddVertexVec3f(eVertexBufferElement_Position, pOut->positions[i]);
			apDecalVB->AddVertexVec3f(eVertexBufferElement_Normal, pOut->normals[i]);
			apDecalVB->AddVertexColor(eVertexBufferElement_Color0, cColor(mColor.r,mColor.g,mColor.b,mColor.a));
		}
		ohpl_decal_out_free(pOut);
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

}
