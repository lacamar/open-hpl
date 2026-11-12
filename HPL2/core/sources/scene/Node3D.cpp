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

#include "scene/Node3D.h"
#include "scene/Entity3D.h"
#include "system/LowLevelSystem.h"
#include "math/Math.h"
#include "scene/RustNode.h"
#include "scene/RustSceneH10.h"
#include "impl/RustBackends.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cNode3D::cNode3D(const tString &asName, bool abAutoDeleteChildren)
	{
		msName = asName;

		mbActive = true;

		mbAutoDeleteChildren = abAutoDeleteChildren;

		mS.m_mtxLocal = cMatrixf::Identity;
		mS.m_mtxWorld = cMatrixf::Identity;

		mvWorldPosition = cVector3f(0,0,0);

		mS.mbTransformUpdated = true;

		mS.mbUsePre = false;
		mS.mbUsePost = false;

		mS.m_mtxPre  = cMatrixf::Identity;
		mS.m_mtxPost = cMatrixf::Identity;

		mS.mpParent = NULL;
		mS.mpEntityParent = NULL;
		
		mS.mqRotation = cQuaternion::Identity;
		mS.mvScale = cVector3f(1,1,1);
		mS.mvTranslation = cVector3f(0,0,0);

		mlCustomFlags =0;
	}

	//-----------------------------------------------------------------------

	cNode3D::~cNode3D()
	{
		if(mS.mpEntityParent)
		{
			mS.mpEntityParent->RemoveNodeChild(this);
		}
		else if(mS.mpParent)
		{
			mS.mpParent->RemoveChild(this);
		}

		
		for(tEntity3DListIt EIt = mlstEntity.begin(); EIt != mlstEntity.end();++EIt)
		{
			iEntity3D* pEntity = *EIt;
			pEntity->SetParent(NULL);
		}

		for(tNode3DListIt NIt = mlstNode.begin(); NIt != mlstNode.end();++NIt)
		{
			cNode3D* pNode = *NIt;
			pNode->mS.mpParent = NULL;
		}
		
		if(mbAutoDeleteChildren)
		{
			STLDeleteAll(mlstNode);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	int cNode3D::SetVisible(bool abX, bool abCascade)
	{
		if(abCascade)
		{
			for(tNode3DListIt NIt = mlstNode.begin(); NIt != mlstNode.end(); NIt++)
			{
				(*NIt)->SetVisible(abX,abCascade);
			}
		}
		return 0;
	}

	//-----------------------------------------------------------------------

	bool cNode3D::AddEntity(iEntity3D* apEntity)
	{
		if(apEntity->HasParent())return false;

		mlstEntity.push_back(apEntity);
		apEntity->SetParent(this);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cNode3D::RemoveEntity(iEntity3D* apEntity)
	{
		for(tEntity3DListIt it = mlstEntity.begin();it != mlstEntity.end();it++)
		{
			if(*it == apEntity){
				apEntity->SetParent(NULL);
				mlstEntity.erase(it);
				return true;
			}
		}

		return false;
	}

	//-----------------------------------------------------------------------

	void cNode3D::ClearEntities()
	{
		mlstEntity.clear();
	}

	//-----------------------------------------------------------------------

	cNode3DIterator cNode3D::GetChildIterator()
	{
		return cNode3DIterator(&mlstNode);
	}

	cEntity3DIterator cNode3D::GetEntityIterator()
	{
		return cEntity3DIterator(&mlstEntity);
	}	
	

	//-----------------------------------------------------------------------

	cNode3D* cNode3D::CreateChild(const tString &asName, bool abAutoDeleteChildren)
	{
		cNode3D *pNode = hplNew( cNode3D, (asName,abAutoDeleteChildren) );

		pNode->mS.mpParent = this;
		mlstNode.push_back(pNode);

		return pNode;
	}

	//-----------------------------------------------------------------------

	void cNode3D::RemoveChild(cNode3D* apNode)
	{
		for(tNode3DListIt NIt = mlstNode.begin(); NIt != mlstNode.end();++NIt)
		{
			if(*NIt == apNode)
			{
				mlstNode.erase(NIt);
				return;
			}
		}
	}

	//-----------------------------------------------------------------------

	cVector3f cNode3D::GetLocalPosition()
	{
		return mS.m_mtxLocal.GetTranslation();
	}
	
	//-----------------------------------------------------------------------

	cMatrixf& cNode3D::GetLocalMatrix()
	{
		return mS.m_mtxLocal;
	}
	
	//-----------------------------------------------------------------------

	cVector3f cNode3D::GetWorldPosition()
	{

		UpdateWorldTransform();

		return mS.m_mtxWorld.GetTranslation();
	}

	//-----------------------------------------------------------------------

	cMatrixf& cNode3D::GetWorldMatrix()
	{
		UpdateWorldTransform();

		return mS.m_mtxWorld;
	}

	//-----------------------------------------------------------------------


	void cNode3D::SetPosition(const cVector3f& avPos)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_set_position(OhplH10GetHost(), (uintptr_t)this, &avPos); return; }

		mS.m_mtxLocal.m[0][3] = avPos.x;
		mS.m_mtxLocal.m[1][3] = avPos.y;
		mS.m_mtxLocal.m[2][3] = avPos.z;

		SetWorldTransformUpdated();
	}

	void cNode3D::SetMatrix(const cMatrixf& a_mtxTransform, bool abSetChildrenUpdated)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_set_matrix(OhplH10GetHost(), (uintptr_t)this, &a_mtxTransform, abSetChildrenUpdated); return; }

		mS.m_mtxLocal = a_mtxTransform;

		if(abSetChildrenUpdated)
			SetWorldTransformUpdated();
		else
			mS.mbTransformUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cNode3D::SetWorldPosition(const cVector3f& avWorldPos)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_set_world_position(OhplH10GetHost(), (uintptr_t)this, &avWorldPos); return; }

		if(mS.mpParent)
		{
			SetPosition(avWorldPos - mS.mpParent->GetWorldPosition());
		}
		else
		{
			SetPosition(avWorldPos);
		}
	}
	
	//-----------------------------------------------------------------------


	void cNode3D::SetWorldMatrix(const cMatrixf& a_mtxWorldTransform)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_set_world_matrix(OhplH10GetHost(), (uintptr_t)this, &a_mtxWorldTransform); return; }

		if(mS.mpParent)
		{
			cMatrixf mtxLocal;
			ohpl_scene_world_to_local(&mS.mpParent->GetWorldMatrix(), &a_mtxWorldTransform, &mtxLocal);
			SetMatrix(mtxLocal);
		}
		else
		{
			SetMatrix(a_mtxWorldTransform);
		}
	}

	//-----------------------------------------------------------------------
	
	const tString& cNode3D::GetName()
	{
		return msName;
	}

	//-----------------------------------------------------------------------

	cNode3D* cNode3D::GetParent()
	{
		return mS.mpParent;
	}
	
	//-----------------------------------------------------------------------

	void cNode3D::SetParent(cNode3D* apNode)
	{
		mS.mpParent = apNode;

		if(mS.mpParent)
			mS.mpParent->mlstNode.push_back(this);
	}

	void cNode3D::AddChild(cNode3D* apChild)
	{
		mlstNode.push_back(apChild);
	}

	//-----------------------------------------------------------------------

	void cNode3D::AddRotation(const cVector3f& avRot, eEulerRotationOrder aOrder)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_add_rotation_euler(&mS, &avRot, (int)aOrder); return; }

		cQuaternion qNew;
		ohpl_scene_node_add_rotation(&avRot, (int)aOrder, &mS.mqRotation, &qNew);
		mS.mqRotation = qNew;
	}
	
	void cNode3D::AddRotation(const cQuaternion& aqRotation)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_add_rotation(&mS, &aqRotation); return; }

		mS.mqRotation = cMath::QuaternionMul(aqRotation, mS.mqRotation);
	}

	void cNode3D::AddScale(const cVector3f& avScale)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_add_scale(&mS, &avScale); return; }

		mS.mvScale *= avScale;
	}

	void cNode3D::AddTranslation(const cVector3f& avTrans)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_add_translation(&mS, &avTrans); return; }

		mS.mvTranslation += avTrans;
	}

	void cNode3D::UpdateMatrix(bool abSetChildrenUpdated)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_update_matrix(OhplH10GetHost(), (uintptr_t)this, abSetChildrenUpdated); return; }

		cMatrixf mtxTransform;
		ohpl_scene_node_update_matrix(&GetLocalMatrix(), &mS.mqRotation, &mS.mvTranslation, &mtxTransform);

		SetMatrix(mtxTransform,abSetChildrenUpdated);

		//Log("World pos: %s\n",GetWorldMatrix().GetTranslation().ToString().c_str());
		
		//Reset values
		mS.mqRotation = cQuaternion::Identity;
		mS.mvScale = cVector3f(1,1,1);
		mS.mvTranslation = cVector3f(0,0,0);
	}

	//-----------------------------------------------------------------------

	void cNode3D::ApplyPreAnimTransform(bool abSetChildrenUpdated)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_apply_anim(OhplH10GetHost(), (uintptr_t)this, false, abSetChildrenUpdated); return; }

		if(mS.mbUsePre==false) return;

		cMatrixf mtxTransform = GetLocalMatrix();
		mtxTransform = cMath::MatrixMul(mtxTransform, mS.m_mtxPre);

		SetMatrix(mtxTransform,abSetChildrenUpdated);
	}

	void cNode3D::ApplyPostAnimTransform(bool abSetChildrenUpdated)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_apply_anim(OhplH10GetHost(), (uintptr_t)this, true, abSetChildrenUpdated); return; }

		if(mS.mbUsePost==false) return;

		cMatrixf mtxTransform = GetLocalMatrix();
		mtxTransform = cMath::MatrixMul(mS.m_mtxPost, mtxTransform);

		SetMatrix(mtxTransform,abSetChildrenUpdated);
	}
	
	//-----------------------------------------------------------------------

	void cNode3D::UpdateEntityChildren()
	{
		//Set all entities as updated
		tEntity3DListIt EIt = mlstEntity.begin();
		for(;EIt != mlstEntity.end();++EIt)
		{
			iEntity3D* pEntity = static_cast<iEntity3D*>(*EIt);

			pEntity->SetTransformUpdated();
		}
	}
	
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	void cNode3D::UpdateWorldTransform()
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_update_world(OhplH10GetHost(), (uintptr_t)this); return; }

		if(mS.mbTransformUpdated)
		{
			//if(msName == "WeaponJoint") LogUpdate("  update world transform!\n");

			mS.mbTransformUpdated = false;

			if(mS.mpParent)
			{
				mS.m_mtxWorld = cMath::MatrixMul(mS.mpParent->GetWorldMatrix(),mS.m_mtxLocal);
			}
			else if(mS.mpEntityParent)
			{
				mS.m_mtxWorld = cMath::MatrixMul(mS.mpEntityParent->GetWorldMatrix(),mS.m_mtxLocal);
			}
			else
			{
				mS.m_mtxWorld = mS.m_mtxLocal;
			}
		}
	}
	//-----------------------------------------------------------------------
	
	void cNode3D::SetWorldTransformUpdated()
	{
		if(OhplBackendRustRequested()) { ohpl_h10_node_set_world_transform_updated(OhplH10GetHost(), (uintptr_t)this); return; }

		//if(msName == "WeaponJoint") LogUpdate("  setworldtransform updated!\n");

		mS.mbTransformUpdated = true;

		//Update entity childre
		UpdateEntityChildren();

		//Set all child nodes as updated
		tNode3DListIt NIt = mlstNode.begin();
		for(;NIt != mlstNode.end();++NIt)
		{
			cNode3D* pNode = static_cast<cNode3D*>(*NIt);

        	pNode->SetWorldTransformUpdated();
		}
	}

	//-----------------------------------------------------------------------
}
