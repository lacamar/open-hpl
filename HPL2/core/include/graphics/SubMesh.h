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
#ifndef HPL_SUB_MESH_H
#define HPL_SUB_MESH_H

#include <cstddef>
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "math/MeshTypes.h"
#include "physics/PhysicsTypes.h"
#include "graphics/RustMeshMm.h"

namespace hpl {

	class cMaterial;
	class iVertexBuffer;
	class cMesh;
	class iPhysicsWorld;
	class iCollideShape;
	class cMaterialManager;

	//--------------------------------------------------

	// Same layout as rust/src/graphics/mm.rs Collider; allocated and owned by the sub-mesh box.
	class cMeshCollider
	{
	public:
		eCollideShapeType Type() const { return mType; }
		const cVector3f& Size() const { return mvSize; }
		void SetSize(const cVector3f& avSize){ mvSize = avSize; }
		const cMatrixf& Offset() const { return m_mtxOffset; }
		cMatrixf& Offset(){ return m_mtxOffset; }
		bool IsCharCollider() const { return mbCharCollider; }
		void SetCharCollider(bool abX){ mbCharCollider = abX; }
	private:
		eCollideShapeType mType;
		cVector3f mvSize;
		cMatrixf m_mtxOffset;
		bool mbCharCollider;
	};

	static_assert(sizeof(cMeshCollider) == 84, "cMeshCollider layout");

	typedef std::vector<cMeshCollider*> tMeshColliderVec;
	typedef tMeshColliderVec::iterator tMeshColliderVecIt;

	//--------------------------------------------------

	// Sub-mesh state owned by rust/src/graphics/mm.rs (SubPub); read in place.
	struct OhplMmSub
	{
		cMaterial *mpMaterial;
		iVertexBuffer *mpVtxBuffer;
		float *mpVertexWeights;
		unsigned char *mpVertexBones;
		cMatrixf m_mtxLocalTransform;
		cVector3f mvModelScale, mvOneSidedNormal, mvOneSidedPoint;
		bool mbDoubleSided, mbCollideShape, mbIsOneSided;
	};

	static_assert(offsetof(OhplMmSub, m_mtxLocalTransform) == 32 && offsetof(OhplMmSub, mbDoubleSided) == 132 && sizeof(cVertexBonePair) == sizeof(OhplMmPair), "OhplMmSub layout");

	//--------------------------------------------------

	class cSubMesh
	{
	friend class cMesh;
	friend class cSubMeshEntity;
	public:
		cSubMesh(const tString &asName,cMaterialManager* apMaterialManager) :
			msName(asName),
			mpS(reinterpret_cast<OhplMmSub*>(ohpl_mm_sub_new(OhplMmHostGet(), apMaterialManager, asName.data(), asName.size()))),
			mpVertexWeights(mpS->mpVertexWeights), mpVertexBones(mpS->mpVertexBones) {}
		~cSubMesh(){ ohpl_mm_sub_free(Rs()); }

		void SetMaterial(cMaterial* apMaterial){ ohpl_mm_sub_set_material(Rs(), apMaterial); }
		void SetVertexBuffer(iVertexBuffer* apVtxBuffer){ mpS->mpVtxBuffer = apVtxBuffer; }

		//Renderable implementation.
		cMaterial *GetMaterial(){ return mpS->mpMaterial; }
		iVertexBuffer* GetVertexBuffer(){ return mpS->mpVtxBuffer; }

		const tString& GetName(){ return msName;}

		//Vertex-Bone pairs
		void ResizeVertexBonePairs(int alSize){ ohpl_mm_sub_resize_pairs(Rs(), alSize); }
		int GetVertexBonePairNum(){ return ohpl_mm_sub_pair_num(Rs()); }
		cVertexBonePair& GetVertexBonePair(int alNum){ return *reinterpret_cast<cVertexBonePair*>(ohpl_mm_sub_pair(Rs(), alNum)); }
		void AddVertexBonePair(const cVertexBonePair &aPair){ ohpl_mm_sub_add_pair(Rs(), aPair.vtxIdx, aPair.boneIdx, aPair.weight); }
		void ClearVertexBonePairs(){ ohpl_mm_sub_clear_pairs(Rs()); }

		//Colliders
		cMeshCollider* CreateCollider(eCollideShapeType aType){ return reinterpret_cast<cMeshCollider*>(ohpl_mm_sub_create_collider(Rs(), aType)); }
		cMeshCollider* GetCollider(int alIdx){ return reinterpret_cast<cMeshCollider*>(ohpl_mm_sub_collider(Rs(), alIdx)); }
		int GetColliderNum(){ return ohpl_mm_sub_collider_num(Rs()); }

		iCollideShape* CreateCollideShape(iPhysicsWorld *apWorld){ return static_cast<iCollideShape*>(ohpl_mm_sub_create_collide_shape(Rs(), apWorld)); }
		static iCollideShape* CreateCollideShapeFromCollider(cMeshCollider *pCollider, iPhysicsWorld *apWorld, const cVector3f& avSizeMul, cMatrixf *apMtxOffset)
		{
			return static_cast<iCollideShape*>(ohpl_mm_collide_shape(OhplMmHostGet(), reinterpret_cast<OhplMmCollider*>(pCollider), apWorld, &avSizeMul, apMtxOffset));
		}

		void SetIsCollideShape(bool abX){mpS->mbCollideShape = abX;}
		bool IsCollideShape(){ return mpS->mbCollideShape;}

		void SetDoubleSided(bool abX){ mpS->mbDoubleSided = abX;}
		bool GetDoubleSided(){ return mpS->mbDoubleSided;}

		void SetModelScale(const cVector3f& avScale){ mpS->mvModelScale = avScale;}
		cVector3f GetModelScale(){ return mpS->mvModelScale;}

		const cMatrixf& GetLocalTransform(){ return mpS->m_mtxLocalTransform;}
		void SetLocalTransform(const cMatrixf& a_mtxTrans){ mpS->m_mtxLocalTransform = a_mtxTrans;}

		bool GetIsOneSided(){ return mpS->mbIsOneSided;}
		const cVector3f& GetOneSidedNormal(){ return mpS->mvOneSidedNormal;}
		const cVector3f& GetOneSidedPoint(){ return mpS->mvOneSidedPoint;}

		void SetMaterialName(const tString& asName){msMaterialName =asName;}
		const tString& GetMaterialName(){ return msMaterialName;}

		void Compile(){ ohpl_mm_sub_compile(Rs()); }

	private:
		OhplMmSub* Rs(){ return mpS; }

		tString msName;
		tString msMaterialName;
		OhplMmSub *mpS;
		float *&mpVertexWeights;
		unsigned char *&mpVertexBones;
	};

};
#endif // HPL_SUB_MESH_H
