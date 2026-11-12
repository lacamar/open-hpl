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

#include "scene/Entity3D.h"
#include "scene/Node3D.h"
#include "math/Math.h"
#include "scene/RustNode.h"
#include "scene/RustSceneH10.h"
#include "impl/RustBackends.h"

#include "system/LowLevelSystem.h"

namespace hpl {


	//////////////////////////////////////////////////////////////////////////
	// RUST HOST
	//////////////////////////////////////////////////////////////////////////

	struct OhplH10Hooks
	{
		static iEntity3D* E(uintptr_t alEnt){ return (iEntity3D*)alEnt;}
		static cNode3D* N(uintptr_t alNode){ return (cNode3D*)alNode;}

		template<class T> static size_t Items(const std::list<T*> &alstItems, uintptr_t *apOut, size_t alCap)
		{
			size_t lCount =0;
			for(T *pItem : alstItems)
			{
				if(lCount < alCap) apOut[lCount] = (uintptr_t)pItem;
				++lCount;
			}
			return lCount;
		}

		static void EntView(uintptr_t alEnt, OhplH10EntView *apOut)
		{
			iEntity3D *pEnt = E(alEnt);
			apOut->mpLocal = &pEnt->m_mtxLocalTransform;
			apOut->mpWorld = &pEnt->m_mtxWorldTransform;
			apOut->mpTransformUpdated = &pEnt->mbTransformUpdated;
			apOut->mpUpdateBV = &pEnt->mbUpdateBoundingVolume;
			apOut->mpApplyBV = &pEnt->mbApplyTransformToBV;
			apOut->mpCount = &pEnt->mlCount;
			apOut->mlParentNode = (uintptr_t)pEnt->mpParentNode;
			apOut->mlParentEntity = (uintptr_t)pEnt->mpParent;
		}
		static size_t EntChildren(uintptr_t alEnt, uintptr_t *apOut, size_t alCap){ return Items(E(alEnt)->mlstChildren, apOut, alCap);}
		static size_t EntNodeChildren(uintptr_t alEnt, uintptr_t *apOut, size_t alCap){ return Items(E(alEnt)->mlstNodeChildren, apOut, alCap);}
		static void EntOnUpdated(uintptr_t alEnt){ E(alEnt)->OnTransformUpdated();}
		static void EntCallbacks(uintptr_t alEnt)
		{
			iEntity3D *pEnt = E(alEnt);
			if(pEnt->mlstCallbacks.empty()) return;
			for(tEntityCallbackListIt it = pEnt->mlstCallbacks.begin(); it != pEnt->mlstCallbacks.end(); ++it)
			{
				(*it)->OnTransformUpdate(pEnt);
			}
		}
		static OhplH10Node* NodeState(uintptr_t alNode){ return &N(alNode)->mS;}
		static size_t NodeChildren(uintptr_t alNode, uintptr_t *apOut, size_t alCap){ return Items(N(alNode)->mlstNode, apOut, alCap);}
		static size_t NodeEntities(uintptr_t alNode, uintptr_t *apOut, size_t alCap){ return Items(N(alNode)->mlstEntity, apOut, alCap);}
	};

	const OhplH10Host* OhplH10GetHost()
	{
		static const OhplH10Host host = {
			OhplH10Hooks::EntView, OhplH10Hooks::EntChildren, OhplH10Hooks::EntNodeChildren, OhplH10Hooks::EntOnUpdated,
			OhplH10Hooks::EntCallbacks, OhplH10Hooks::NodeState, OhplH10Hooks::NodeChildren, OhplH10Hooks::NodeEntities};
		return &host;
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	iEntity3D::iEntity3D(tString asName)
	{
		msName = asName;
		mbIsActive = true;
		
		mpParentNode = NULL;

		m_mtxLocalTransform = cMatrixf::Identity;
		m_mtxWorldTransform = cMatrixf::Identity;

		mbTransformUpdated = true;

		mlCount = 0;

		msSourceFile = "";

		mbApplyTransformToBV = true;
		mbUpdateBoundingVolume = true;

		mpParent = NULL;

		mlIteratorCount =-1;

		mbIsSaved = true;
		mlUniqueID = -1;
	}

	iEntity3D::~iEntity3D()
	{
		if(mpParentNode)
			mpParentNode->RemoveEntity(this);
		else if(mpParent) 
			mpParent->RemoveChild(this);

		for(tEntity3DListIt it = mlstChildren.begin(); it != mlstChildren.end();++it)
		{
			iEntity3D *pChild = *it;
			pChild->mpParent = NULL;
		}

		for(tNode3DListIt it = mlstNodeChildren.begin(); it != mlstNodeChildren.end();++it)
		{
			cNode3D *pNode = *it;
			pNode->mS.mpEntityParent = NULL;
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cVector3f iEntity3D::GetLocalPosition()
	{
		return m_mtxLocalTransform.GetTranslation();
	}

	//-----------------------------------------------------------------------

	cMatrixf& iEntity3D::GetLocalMatrix()
	{
		return m_mtxLocalTransform;
	}

	//-----------------------------------------------------------------------

	cVector3f iEntity3D::GetWorldPosition()
	{
		UpdateWorldTransform();

		return m_mtxWorldTransform.GetTranslation();
	}
	
	//-----------------------------------------------------------------------

	cMatrixf& iEntity3D::GetWorldMatrix()
	{
		UpdateWorldTransform();
		
		return m_mtxWorldTransform;
	}
	
	//-----------------------------------------------------------------------

	void iEntity3D::SetPosition(const cVector3f& avPos)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_ent_set_position(OhplH10GetHost(), (uintptr_t)this, &avPos); return; }

		m_mtxLocalTransform.m[0][3] = avPos.x;
		m_mtxLocalTransform.m[1][3] = avPos.y;
		m_mtxLocalTransform.m[2][3] = avPos.z;

		SetTransformUpdated();
	}
	
	//-----------------------------------------------------------------------

	void iEntity3D::SetMatrix(const cMatrixf& a_mtxTransform)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_ent_set_matrix(OhplH10GetHost(), (uintptr_t)this, &a_mtxTransform); return; }

		m_mtxLocalTransform = a_mtxTransform;

		SetTransformUpdated();
	}

	//-----------------------------------------------------------------------

	void iEntity3D::SetWorldPosition(const cVector3f& avWorldPos)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_ent_set_world_position(OhplH10GetHost(), (uintptr_t)this, &avWorldPos); return; }

		if(mpParent)
		{
			SetPosition(avWorldPos - mpParent->GetWorldPosition());
		}
		else
		{
			SetPosition(avWorldPos);
		}
	}

	//-----------------------------------------------------------------------

	void iEntity3D::SetWorldMatrix(const cMatrixf& a_mtxWorldTransform)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_ent_set_world_matrix(OhplH10GetHost(), (uintptr_t)this, &a_mtxWorldTransform); return; }

		if(mpParent)
		{
			cMatrixf mtxLocal;
			ohpl_scene_world_to_local(&mpParent->GetWorldMatrix(), &a_mtxWorldTransform, &mtxLocal);
			SetMatrix(mtxLocal);
		}
		else
		{
			SetMatrix(a_mtxWorldTransform);
		}
	}
	
	//-----------------------------------------------------------------------
	
	void iEntity3D::SetTransformUpdated(bool abUpdateCallbacks)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_ent_set_transform_updated(OhplH10GetHost(), (uintptr_t)this, abUpdateCallbacks); return; }

		mbTransformUpdated = true;
		mlCount++;

		mbUpdateBoundingVolume = true;

		OnTransformUpdated();
		
		//Update children
		for(tEntity3DListIt EntIt = mlstChildren.begin(); EntIt != mlstChildren.end();++EntIt)
		{
			iEntity3D *pChild = *EntIt;
			pChild->SetTransformUpdated(true);
		}
		
		//Update node children
		for(tNode3DListIt nodeIt = mlstNodeChildren.begin(); nodeIt != mlstNodeChildren.end();++nodeIt)
		{
			cNode3D *pNode = *nodeIt;
			pNode->SetWorldTransformUpdated();
		}

		//Update callbacks
		if(mlstCallbacks.empty() || abUpdateCallbacks==false) return;

		tEntityCallbackListIt it = mlstCallbacks.begin();
		for(; it!= mlstCallbacks.end(); ++it)
		{
			iEntityCallback* pCallback = *it;
			pCallback->OnTransformUpdate(this);
		}
	}

	//-----------------------------------------------------------------------

	bool iEntity3D::GetTransformUpdated()
	{
		return mbTransformUpdated;
	}
	
	//-----------------------------------------------------------------------
	
	int iEntity3D::GetTransformUpdateCount()
	{
		return mlCount;
	}

	//-----------------------------------------------------------------------

	cBoundingVolume* iEntity3D::GetBoundingVolume()
	{
		if(OhplBackendRustRequested())
		{
			if(ohpl_h10_ent_bv_dirty(OhplH10GetHost(), (uintptr_t)this)) mBoundingVolume.SetTransform(m_mtxWorldTransform);
			return &mBoundingVolume;
		}

		if(mbApplyTransformToBV && mbUpdateBoundingVolume)
		{
			mBoundingVolume.SetTransform(GetWorldMatrix());
			mbUpdateBoundingVolume = false;
		}

		return &mBoundingVolume;
	}

	//-----------------------------------------------------------------------

	void iEntity3D::AddCallback(iEntityCallback *apCallback)
	{
		mlstCallbacks.push_back(apCallback);
	}
	
	//-----------------------------------------------------------------------

	void iEntity3D::RemoveCallback(iEntityCallback *apCallback)
	{
		STLFindAndRemove(mlstCallbacks, apCallback);
	}

	//-----------------------------------------------------------------------

	void iEntity3D::AddChild(iEntity3D *apEntity)
	{
		if(apEntity==NULL)return;
		if(apEntity->mpParent != NULL)
		{
			apEntity->mpParent->RemoveChild(apEntity);
		}

	    mlstChildren.push_back(apEntity);
		apEntity->mpParent = this;

		apEntity->SetTransformUpdated(true);
	}
	
	void iEntity3D::RemoveChild(iEntity3D *apEntity)
	{
		for(tEntity3DListIt it = mlstChildren.begin(); it != mlstChildren.end();)
		{
			iEntity3D *pChild = *it;
			if(pChild == apEntity)
			{
				pChild->mpParent = NULL;
				it = mlstChildren.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	bool iEntity3D::IsChild(iEntity3D *apEntity)
	{
		for(tEntity3DListIt it = mlstChildren.begin(); it != mlstChildren.end();++it)
		{
			iEntity3D *pChild = *it;
			if(pChild == apEntity) return true;
		}
		return false;
	}

	iEntity3D * iEntity3D::GetEntityParent()
	{
		return mpParent;
	}

	cEntity3DIterator iEntity3D::GetChildIterator()
	{
		return cEntity3DIterator(&mlstChildren);
	}
	
	//-----------------------------------------------------------------------

	void iEntity3D::AddNodeChild(cNode3D *apNode)
	{
		if(apNode->mS.mpEntityParent != NULL)
		{
			apNode->mS.mpEntityParent->RemoveNodeChild(apNode);
		}

		mlstNodeChildren.push_back(apNode);
		apNode->mS.mpEntityParent = this;

		apNode->SetWorldTransformUpdated();
	}
	
	void iEntity3D::RemoveNodeChild(cNode3D *apNode)
	{
		for(tNode3DListIt it = mlstNodeChildren.begin(); it != mlstNodeChildren.end();)
		{
			cNode3D *pNode = *it;
			if(pNode == apNode)
			{
				pNode->mS.mpEntityParent = NULL;
				it = mlstNodeChildren.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
	
	bool iEntity3D::IsNodeChild(cNode3D *apNode)
	{
		for(tNode3DListIt it = mlstNodeChildren.begin(); it != mlstNodeChildren.end();++it)
		{
			cNode3D *pNode = *it;
			if(pNode == apNode) return true;
		}
		return false;
	}
	
	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	void iEntity3D::UpdateWorldTransform()
	{
		if(OhplBackendRustRequested()) { ohpl_h10_ent_update_world(OhplH10GetHost(), (uintptr_t)this); return; }

		if(mbTransformUpdated)
		{
			//Log("CREATING Entity '%s' world transform!\n",msName.c_str());

			mbTransformUpdated = false;
			
			//first check if there is a node parent
			if(mpParentNode)
			{
				cNode3D* pNode3D = static_cast<cNode3D*>(mpParentNode);

				m_mtxWorldTransform = cMath::MatrixMul(pNode3D->GetWorldMatrix(), m_mtxLocalTransform);
			}
			//If there is no node parent check for entity parent
			else if(mpParent)
			{
				m_mtxWorldTransform = cMath::MatrixMul(mpParent->GetWorldMatrix(), m_mtxLocalTransform);
			}
			else
			{
				m_mtxWorldTransform = m_mtxLocalTransform;
			}
		}
	}
	
	//-----------------------------------------------------------------------
}
