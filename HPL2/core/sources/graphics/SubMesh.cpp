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

#include "graphics/SubMesh.h"

#include "system/String.h"

#include "graphics/Mesh.h"
#include "resources/MaterialManager.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"
#include "math/Math.h"
#include "graphics/RustMesh.h"

#include "physics/PhysicsWorld.h"

#include "system/MemoryManager.h"

#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSubMesh::cSubMesh(const tString &asName, cMaterialManager* apMaterialManager)
	{
		mpMaterialManager = apMaterialManager;

		msName = asName;

		mpMaterial = NULL;
		mpVtxBuffer = NULL;

		mbDoubleSided = false;

		mbCollideShape = false;

		mpVertexWeights = NULL;
		mpVertexBones = NULL;

		m_mtxLocalTransform = cMatrixf::Identity;

		mbIsOneSided = false;
		mvOneSidedNormal =0;
		mvOneSidedPoint =0;
	}

	//-----------------------------------------------------------------------

	cSubMesh::~cSubMesh()
	{
		if(mpMaterial)mpMaterialManager->Destroy(mpMaterial);
		if(mpVtxBuffer) hplDelete(mpVtxBuffer);
		if(mpVertexBones) hplDeleteArray(mpVertexBones);
		if(mpVertexWeights) hplDeleteArray(mpVertexWeights);

		STLDeleteAll(mvColliders);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cSubMesh::SetMaterial(cMaterial* apMaterial)
	{
		if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
		mpMaterial = apMaterial;
	}
	
	//-----------------------------------------------------------------------

	void cSubMesh::SetVertexBuffer(iVertexBuffer* apVtxBuffer)
	{
		if(mpVtxBuffer == apVtxBuffer) return;

		mpVtxBuffer = apVtxBuffer;
	}

	//-----------------------------------------------------------------------

	cMaterial *cSubMesh::GetMaterial()
	{
		return mpMaterial;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cSubMesh::GetVertexBuffer()
	{
		return mpVtxBuffer;
	}

	//-----------------------------------------------------------------------
	
	void cSubMesh::ResizeVertexBonePairs(int alSize)
	{
		mvVtxBonePairs.resize(alSize);
	}

	int cSubMesh::GetVertexBonePairNum()
	{
		return (int)mvVtxBonePairs.size();
	}
	cVertexBonePair& cSubMesh::GetVertexBonePair(int alNum)
	{
		return mvVtxBonePairs[alNum];
	}

	void cSubMesh::AddVertexBonePair(const cVertexBonePair &aPair)
	{
		mvVtxBonePairs.push_back(aPair);
	}

	void cSubMesh::ClearVertexBonePairs()
	{
		mvVtxBonePairs.clear();
	}


	
	//-----------------------------------------------------------------------

	cMeshCollider* cSubMesh::CreateCollider(eCollideShapeType aType)
	{
		cMeshCollider* pColl = hplNew( cMeshCollider, () );
		pColl->mType = aType;

		mvColliders.push_back(pColl);

		return pColl;
	}

	cMeshCollider* cSubMesh::GetCollider(int alIdx)
	{
		return mvColliders[alIdx];
	}

	int cSubMesh::GetColliderNum()
	{
		return (int) mvColliders.size();
	}

	iCollideShape* cSubMesh::CreateCollideShapeFromCollider(cMeshCollider *pCollider, iPhysicsWorld *apWorld, const cVector3f& avSizeMul, cMatrixf *apMtxOffset)
	{
		cMatrixf *pOffset = apMtxOffset ? apMtxOffset : &pCollider->m_mtxOffset;

		cVector3f vSize = pCollider->mvSize*avSizeMul;

		switch(pCollider->mType)
		{
		case eCollideShapeType_Box: 
			return apWorld->CreateBoxShape(vSize,pOffset);
		case eCollideShapeType_Sphere: 
			return apWorld->CreateSphereShape(vSize,pOffset);
		case eCollideShapeType_Cylinder: 
			return apWorld->CreateCylinderShape(vSize.x,vSize.y,pOffset);
		case eCollideShapeType_Capsule: 
			return apWorld->CreateCapsuleShape(vSize.x,vSize.y,pOffset);
		}

		return NULL;
	}

	iCollideShape* cSubMesh::CreateCollideShape(iPhysicsWorld *apWorld)
	{
		if(mvColliders.empty()) return NULL;

		//Create a single object
		if(mvColliders.size() == 1)
		{
			return CreateCollideShapeFromCollider(mvColliders[0],apWorld,1, NULL);
		}
		//Create compound object
		else
		{
			tCollideShapeVec vShapes;
			vShapes.reserve(mvColliders.size());

			for(size_t i=0; i<mvColliders.size(); ++i)
			{
				vShapes.push_back(CreateCollideShapeFromCollider(mvColliders[i],apWorld,1, NULL));
			}

			return apWorld->CreateCompundShape(vShapes);
		}
	}

	//-----------------------------------------------------------------------


	void cSubMesh::Compile()
	{
		CheckOneSided();
		CompileBonePairs();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIAVTE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cSubMesh::CheckOneSided()
	{
		//Log("--- %s\n",GetName().c_str());
		
		if(mpVtxBuffer==NULL) return;

		int lIdxNum = mpVtxBuffer->GetIndexNum();

		if(lIdxNum > 400*3) return; //Just skip larger buffers for now, they should never be planes.

		unsigned int* pIndices = mpVtxBuffer->GetIndices();
		float *pPositions = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
		const int lVtxStride = mpVtxBuffer->GetElementNum(eVertexBufferElement_Position);

		cVector3f vNormal, vPoint;
		if(ohpl_submesh_one_sided(pIndices, (size_t)lIdxNum, pPositions, (size_t)mpVtxBuffer->GetVertexNum() * (size_t)lVtxStride, lVtxStride, &vNormal, &vPoint) == false) return;

		mbIsOneSided = true;
		mvOneSidedNormal = vNormal;
		mvOneSidedPoint = vPoint;
	}

	//-----------------------------------------------------------------------

	void cSubMesh::CompileBonePairs()
	{
		if(mvVtxBonePairs.empty()) return;

		mpVertexWeights = hplNewArray( float, 4 * mpVtxBuffer->GetVertexNum());
		mpVertexBones = hplNewArray( unsigned char, 4 * mpVtxBuffer->GetVertexNum()) ;
		memset(mpVertexWeights,0,4 * mpVtxBuffer->GetVertexNum()*sizeof(float));
		memset(mpVertexBones,0,4 * mpVtxBuffer->GetVertexNum()*sizeof(unsigned char));

		unsigned int lFlags = ohpl_submesh_compile_bone_pairs(mvVtxBonePairs.data(), mvVtxBonePairs.size(), (size_t)mpVtxBuffer->GetVertexNum(), mpVertexWeights, mpVertexBones);

		if(lFlags & 1) Warning("More than 4 bones on a vertex in submesh '%s' in mesh '%s' !\n",GetName().c_str(), mpParent->GetName().c_str());

		if(lFlags & 2) Warning("Some vertices in sub mesh '%s' in mesh '%s' are not connected to a bone!\n",GetName().c_str(), mpParent->GetName().c_str());
	}

	//-----------------------------------------------------------------------
}
