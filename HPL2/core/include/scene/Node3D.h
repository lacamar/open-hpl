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

#ifndef HPL_NODE3D_H
#define HPL_NODE3D_H

#include "math/MathTypes.h"
#include "scene/SceneTypes.h"
#include "scene/RustSceneH10.h"

namespace hpl {

	struct OhplH10Hooks;

	class cNode3D
	{
		friend class iEntity3D;
		friend struct OhplH10Hooks;
	public:
		cNode3D(const tString &asName="", bool abAutoDeleteChildren = true);
		virtual ~cNode3D();

		///////////////////////
		// Hierarchy
		bool AddEntity(iEntity3D* apEntity);
		bool RemoveEntity(iEntity3D* apEntity);
		void ClearEntities(){ ohpl_node_clear_entities(mpN);}
		
		cNode3D* GetParent(){ return mpN->mpParent ? (cNode3D*)mpN->mpParent->owner : NULL;}
		
		cNode3DIterator GetChildIterator();

		size_t GetChildCount(){ return ohpl_node_child_count(mpN);}
		cNode3D* GetChildAt(size_t alIdx){ return (cNode3D*)ohpl_node_child_at(mpN, alIdx);}

		cEntity3DIterator GetEntityIterator();

		cNode3D* CreateChild(const tString &asName="", bool abAutoDeleteChildren = true);
		void RemoveChild(cNode3D* apNode){ ohpl_node_remove_child(mpN, apNode->mpN);}


		//Extra stuff that shouldn't be used externally
		void SetParent(cNode3D* apNode){ ohpl_node_set_parent(mpN, apNode ? apNode->mpN : NULL);}
		void AddChild(cNode3D* apChild){ ohpl_node_add_child(mpN, apChild->mpN);}
		
		///////////////////////
		//Properties
		const tString& GetName(){ return msName;}

		void SetActive(bool abX){mpN->active = abX;}
		bool IsActive(){return mpN->active;}

		int SetVisible(bool abX, bool abCascade){ return 0;}
		
		cVector3f GetLocalPosition(){ return mpN->m_mtxLocal.GetTranslation();}
		cMatrixf& GetLocalMatrix(){ return mpN->m_mtxLocal;}

		cVector3f GetWorldPosition(){ UpdateWorldTransform(); return mpN->m_mtxWorld.GetTranslation();}
		cMatrixf& GetWorldMatrix(){ UpdateWorldTransform(); return mpN->m_mtxWorld;}

		void SetPosition(const cVector3f& avPos){ ohpl_h10_node_set_position(OhplH10GetHost(), mpN, &avPos);}
		void SetMatrix(const cMatrixf& a_mtxTransform, bool abSetChildrenUpdated=true){ ohpl_h10_node_set_matrix(OhplH10GetHost(), mpN, &a_mtxTransform, abSetChildrenUpdated);}

		void SetWorldPosition(const cVector3f& avWorldPos){ ohpl_h10_node_set_world_position(OhplH10GetHost(), mpN, &avWorldPos);}
		void SetWorldMatrix(const cMatrixf& a_mtxWorldTransform){ ohpl_h10_node_set_world_matrix(OhplH10GetHost(), mpN, &a_mtxWorldTransform);}

		void AddRotation(const cVector3f& avRot, eEulerRotationOrder aOrder){ ohpl_h10_node_add_rotation_euler(mpN, &avRot, (int)aOrder);}
		void AddRotation(const cQuaternion& aqRotation){ ohpl_h10_node_add_rotation(mpN, &aqRotation);}

		void AddScale(const cVector3f& avScale){ ohpl_h10_node_add_scale(mpN, &avScale);}

		void AddTranslation(const cVector3f& avTrans){ ohpl_h10_node_add_translation(mpN, &avTrans);}

		bool GetUsePreAnimTransform(){ return mpN->mbUsePost;}
		bool GetUsePostAnimTransform(){ return mpN->mbUsePost;}
		const cMatrixf& GetPreAnimTransform(){ return mpN->m_mtxPost;}
		const cMatrixf& GetPostAnimTransform(){ return mpN->m_mtxPost;}
		
		void SetUsePreTransform(bool abX){ mpN->mbUsePre = abX;}
		void SetUsePostTransform(bool abX){ mpN->mbUsePost = abX;}
		void SetPreTransform(const cMatrixf& a_mtxTransform){ mpN->m_mtxPre = a_mtxTransform;}
		void SetPostTransform(const cMatrixf& a_mtxTransform){ mpN->m_mtxPost = a_mtxTransform;}
		
		/**
		 * Applies the pre and post transforms to current matrix
		 */
		void ApplyPreAnimTransform(bool abSetChildrenUpdated){ ohpl_h10_node_apply_anim(OhplH10GetHost(), mpN, false, abSetChildrenUpdated);}
		void ApplyPostAnimTransform(bool abSetChildrenUpdated){ ohpl_h10_node_apply_anim(OhplH10GetHost(), mpN, true, abSetChildrenUpdated);}

		void SetCustomFlags(int alX){ mpN->customFlags = alX;}
		int GetCustomFlags(){ return mpN->customFlags;}
		
		/**
		 * Updates the matrix with the added scales, translations and rotation. It also resets these values.
		 */
		void UpdateMatrix(bool abSetChildrenUpdated){ ohpl_h10_node_update_matrix(OhplH10GetHost(), mpN, abSetChildrenUpdated);}

		void UpdateWorldTransform(){ ohpl_h10_node_update_world(OhplH10GetHost(), mpN);}
		void SetWorldTransformUpdated(){ ohpl_h10_node_set_world_transform_updated(OhplH10GetHost(), mpN);}

		void UpdateEntityChildren(){ ohpl_node_update_entity_children(OhplH10GetHost(), mpN);}
	private:
		tString msName;
		OhplNodePod *mpN;
	};

};
#endif // HPL_NODE3D_H
