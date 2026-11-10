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

#include "scene/SubMeshEntity.h"

#include "scene/MeshEntity.h"

#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"

#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"

#include "scene/AnimationState.h"
#include "scene/NodeState.h"

#include "physics/PhysicsBody.h"

#include "math/Math.h"

namespace hpl {
	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	cSubMeshEntity::cSubMeshEntity(const tString &asName, cMeshEntity *apMeshEntity, cSubMesh * apSubMesh,
								cMaterialManager* apMaterialManager) : iRenderable(asName)
	{
		mpMeshEntity = apMeshEntity;
		mpSubMesh = apSubMesh;

		mbIsOneSided = mpSubMesh->GetIsOneSided();
		mvOneSidedNormal = mpSubMesh->GetOneSidedNormal();

		mpMaterialManager = apMaterialManager;

		mbGraphicsUpdated = false;
		mlSkinnedBoneMatricesVersion = -1;

		if(mpMeshEntity->GetMesh()->GetSkeleton())
		{
			mpDynVtxBuffer = mpSubMesh->GetVertexBuffer()->CreateCopy(eVertexBufferType_Hardware,eVertexBufferUsageType_Dynamic,eFlagBit_All);
			mvDynTriangles = *mpSubMesh->GetTriangleVecPtr();
		}
		else
		{
			mpDynVtxBuffer = NULL;
		}

		mpLocalNode = NULL;

		mpEntityCallback = hplNew( cSubMeshEntityBodyUpdate, () );
		mbUpdateBody = false;

		mpMaterial = NULL;

		mpUserData = NULL;

		//This is used to see if null should be returned.
		// 0 = no check made, test if matrix is identity
		// -1 = Matrix was not identity
		// 1 = matrix was identiy
		mlStaticNullMatrixCount =0;
	}

	cSubMeshEntity::~cSubMeshEntity()
	{
		hplDelete(mpEntityCallback);

		if(mpDynVtxBuffer) hplDelete(mpDynVtxBuffer);

		/* Clear any custom textures here*/	
		if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// BODY CALLBACK
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------
	
	void cSubMeshEntityBodyUpdate::OnTransformUpdate(iEntity3D* apEntity)
	{
		/*cSubMeshEntity *pSubEntity = static_cast<cSubMeshEntity*>(apEntity);

		if(pSubEntity->GetBody())
		{
			if(apEntity->GetWorldMatrix() != pSubEntity->GetBody()->GetLocalMatrix())
			{
				Log("Setting matrix on %s from\n",pSubEntity->GetBody()->GetName().c_str());
				Log("  %s\n",apEntity->GetWorldMatrix().ToString().c_str());
				Log("  %s\n",pSubEntity->GetBody()->GetLocalMatrix().ToString().c_str());

				pSubEntity->GetBody()->SetMatrix(apEntity->GetWorldMatrix());
			}
		}*/
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cSubMeshEntity::UpdateLogic(float afTimeStep)
	{
		
	}

	//-----------------------------------------------------------------------


	cMaterial* cSubMeshEntity::GetMaterial()
	{
		if(mpMaterial==NULL && mpSubMesh->GetMaterial()==NULL)
		{
			//Error("Materials for sub entity %s are NULL!\n",GetName().c_str());
		}

		if(mpMaterial) 
			return mpMaterial;
		else 
			return mpSubMesh->GetMaterial();
	}

	//-----------------------------------------------------------------------

	void cSubMeshEntity::UpdateGraphicsForFrame(float afFrameTime)
	{
		////////////////////////////////////
		//Update things in parent first.
		mpMeshEntity->UpdateGraphicsForFrame(afFrameTime);

		////////////////////////////////////
		if(mpDynVtxBuffer && mpSubMesh->mpVertexWeights)
		{
			if(mbGraphicsUpdated && (mpMeshEntity->mbSkeletonPhysicsSleeping ||
				mlSkinnedBoneMatricesVersion == mpMeshEntity->mlBoneMatricesVersion))
			{
				return;
			}
			
			mbGraphicsUpdated = true;
			mlSkinnedBoneMatricesVersion = mpMeshEntity->mlBoneMatricesVersion;

			const float *pBindPos = mpSubMesh->GetVertexBuffer()->GetFloatArray(eVertexBufferElement_Position);
			const float *pBindNormal = mpSubMesh->GetVertexBuffer()->GetFloatArray(eVertexBufferElement_Normal);
			const float *pBindTangent = mpSubMesh->GetVertexBuffer()->GetFloatArray(eVertexBufferElement_Texture1Tangent);

			float *pSkinPos = mpDynVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
			float *pSkinNormal = mpDynVtxBuffer->GetFloatArray(eVertexBufferElement_Normal);
			float *pSkinTangent = mpDynVtxBuffer->GetFloatArray(eVertexBufferElement_Texture1Tangent);

			const int lVtxStride = mpDynVtxBuffer->GetElementNum(eVertexBufferElement_Position);
			const int lVtxNum = mpDynVtxBuffer->GetVertexNum();

			ohpl_submesh_skin(pBindPos, pBindNormal, pBindTangent, pSkinPos, pSkinNormal, pSkinTangent,
							  mpSubMesh->mpVertexWeights, mpSubMesh->mpVertexBones,
							  mpMeshEntity->mvBoneMatrices.data(), lVtxNum, lVtxStride, (int)mpMeshEntity->mvBoneMatrices.size());

			//No stencil shadows:
			/*float *pSkinPosArray = mpDynVtxBuffer->GetArray(eVertexElementFlag_Position);
			if(mpMeshEntity->GetRenderFlagBit(eRenderableFlag_ShadowCaster))
			{
				//Update the shadow double
				memcpy(&pSkinPosArray[lVtxStride*lVtxNum],pSkinPosArray,sizeof(float)*lVtxStride*lVtxNum);
				for(int vtx=lVtxStride*lVtxNum + lVtxStride-1; vtx < lVtxStride*lVtxNum*2; vtx+=lVtxStride)
				{
					pSkinPosArray[vtx] = 0;
				}
			}*/

			//Update buffer
			mpDynVtxBuffer->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Normal | eVertexElementFlag_Texture1,false);
			
			//No stencil shadows:
			/*if(mpMeshEntity->GetRenderFlagBit(eRenderableFlag_ShadowCaster))
			{
				//Update triangles
				cMath::CreateTriangleData(mvDynTriangles,
					mpDynVtxBuffer->GetIndices(), mpDynVtxBuffer->GetIndexNum(),
					pSkinPosArray, lVtxStride, lVtxNum);
			}*/
		}
		
	}

	//-----------------------------------------------------------------------


	iVertexBuffer* cSubMeshEntity::GetVertexBuffer()
	{
		if(mpDynVtxBuffer)
		{
			return mpDynVtxBuffer;
		}
		else
		{
			return mpSubMesh->GetVertexBuffer();
		}
	}

	//-----------------------------------------------------------------------


	cBoundingVolume* cSubMeshEntity::GetBoundingVolume()
	{
		if(mpMeshEntity->GetMesh()->GetSkeleton())
		{
			return mpMeshEntity->GetBoundingVolume();
		}	
		else
		{
			if(mbUpdateBoundingVolume)
			{
				mBoundingVolume.SetTransform(GetWorldMatrix());
				mbUpdateBoundingVolume = false;
			}

			return &mBoundingVolume;
		}
	}

	//-----------------------------------------------------------------------


	int cSubMeshEntity::GetMatrixUpdateCount()
	{
		return GetTransformUpdateCount();
	}

	//-----------------------------------------------------------------------

	cMatrixf* cSubMeshEntity::GetModelMatrix(cFrustum *apFrustum)
	{
		////////////////////////
		// Static entity
		if(IsStatic() && mlStaticNullMatrixCount>=0)
		{
			if(mlStaticNullMatrixCount==0)
			{
				if(GetWorldMatrix() == cMatrixf::Identity) 	mlStaticNullMatrixCount = 1;
				else										mlStaticNullMatrixCount = -1;

				if(mlStaticNullMatrixCount == 1)return NULL;
				else							return &GetWorldMatrix();
					
			}
			else
			{
				return NULL;
			}
		}
		////////////////////////
		// Dynamic
		else
		{
			return &GetWorldMatrix();
		}
	}

	//-----------------------------------------------------------------------

	void cSubMeshEntity::SetLocalNode(cNode3D *apNode)
	{
		mpLocalNode = apNode;

		mpLocalNode->AddEntity(this);
	}

	//-----------------------------------------------------------------------

	cNode3D* cSubMeshEntity::GetLocalNode()
	{
		return mpLocalNode;
	}

	//-----------------------------------------------------------------------

	void cSubMeshEntity::SetUpdateBody(bool abX)
	{
		mbUpdateBody = abX;

		/*if(mbUpdateBody)
		{
			AddCallback(mpEntityCallback);
		}
		else
		{
			RemoveCallback(mpEntityCallback);
		}*/
	}
	
	bool cSubMeshEntity::GetUpdateBody()
	{
		return mbUpdateBody;
	}

	//-----------------------------------------------------------------------

	cTriangleData& cSubMeshEntity::GetTriangle(int alIndex)
	{ 
		if(mpDynVtxBuffer)
			return mvDynTriangles[alIndex];
		else
			return (*mpSubMesh->GetTriangleVecPtr())[alIndex];
	}
	int cSubMeshEntity::GetTriangleNum()
	{ 
		if(mpDynVtxBuffer)
			return (int)mvDynTriangles.size();
		else
			return (int)mpSubMesh->GetTriangleVecPtr()->size();
	}

	tTriangleDataVec* cSubMeshEntity::GetTriangleVecPtr()
	{ 
		if(mpDynVtxBuffer)
			return &mvDynTriangles;
		else
			return mpSubMesh->GetTriangleVecPtr();
	}
	
	//-----------------------------------------------------------------------
	
	void cSubMeshEntity::SetCustomMaterial(cMaterial *apMaterial, bool abDestroyOldCustom)
	{
		if(abDestroyOldCustom)
		{
			if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
		}

        mpMaterial = apMaterial;
	}
	
	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cSubMeshEntity::OnTransformUpdated()
	{
		mpMeshEntity->mbUpdateBoundingVolume = true;
	}

	//-----------------------------------------------------------------------

}
