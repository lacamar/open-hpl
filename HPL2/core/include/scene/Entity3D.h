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

#ifndef HPL_ENTITY3D_H
#define HPL_ENTITY3D_H

#include <list>
#include "math/MathTypes.h"
#include "system/SystemTypes.h"
#include "scene/SceneTypes.h"
#include "system/Container.h"

#include "math/BoundingVolume.h"
#include "scene/RustSceneH10.h"

namespace hpl {

	//------------------------------------

	//Used for the render container to add specific data to
	//the object.
	class iRenderContainerData
	{

	};

	typedef std::list<iRenderContainerData*> tRenderContainerDataList;
	typedef tRenderContainerDataList::iterator tRenderContainerDataListIt;
	
	//-----------------------------------------

	class cNode3D;
	struct OhplH10Hooks;

	//-----------------------------------------

	class iEntity3D
	{
	friend struct OhplH10Hooks;
	friend class cNode3D;
	public:
		iEntity3D(tString asName);
		virtual ~iEntity3D();

		virtual tString GetEntityType()=0;

		virtual void UpdateLogic(float afTimeStep){}

		tString& GetName(){return msName;}
		void SetName(const tString& asName){msName = asName;}

		cNode3D* GetParent(){ return mpE->parentNode ? (cNode3D*)mpE->parentNode->owner : NULL;}
		bool HasParent(){ return mpE->parentNode!=NULL;}

		bool IsActive(){ return mpE->active; }
		void SetActive(bool abActive){ mpE->active = abActive; }

		cVector3f GetLocalPosition(){ return mpE->local.GetTranslation();}
		cMatrixf& GetLocalMatrix(){ return mpE->local;}

		cVector3f GetWorldPosition(){ UpdateWorldTransform(); return mpE->world.GetTranslation();}
		cMatrixf& GetWorldMatrix(){ UpdateWorldTransform(); return mpE->world;}

		void SetPosition(const cVector3f& avPos){ ohpl_h10_ent_set_position(OhplH10GetHost(), mpE, &avPos);}
		void SetMatrix(const cMatrixf& a_mtxTransform){ ohpl_h10_ent_set_matrix(OhplH10GetHost(), mpE, &a_mtxTransform);}

		void SetWorldPosition(const cVector3f& avWorldPos){ ohpl_h10_ent_set_world_position(OhplH10GetHost(), mpE, &avWorldPos);}
		void SetWorldMatrix(const cMatrixf& a_mtxWorldTransform){ ohpl_h10_ent_set_world_matrix(OhplH10GetHost(), mpE, &a_mtxWorldTransform);}

		void SetTransformUpdated(bool abUpdateCallbacks = true){ ohpl_h10_ent_set_transform_updated(OhplH10GetHost(), mpE, abUpdateCallbacks);}
		bool GetTransformUpdated(){ return mpE->transformUpdated;}

		int GetTransformUpdateCount(){ return mpE->count;}

		void AddCallback(iEntityCallback *apCallback);
		void RemoveCallback(iEntityCallback *apCallback);

		void SetSourceFile(const tString& asFile){ msSourceFile = asFile;}
		const tString& GetSourceFile(){ return msSourceFile;}

		virtual cBoundingVolume* GetBoundingVolume();

		bool IsSaved(){ return mpE->saved; }
		void SetIsSaved(bool abX){ mpE->saved = abX; }

		void SetUniqueID(int alX){ mpE->uniqueId = alX;}
		int GetUniqueID(){ return mpE->uniqueId;}

        /**
		 * The entity3d child hierarchy will only work if the child has no node parent.
		 **/
		void AddChild(iEntity3D *apEntity){ ohpl_ent_add_child(OhplH10GetHost(), mpE, apEntity ? apEntity->mpE : NULL);}
		void RemoveChild(iEntity3D *apEntity){ ohpl_ent_remove_child(mpE, apEntity ? apEntity->mpE : NULL);}
		bool IsChild(iEntity3D *apEntity){ return ohpl_ent_is_child(mpE, apEntity ? apEntity->mpE : NULL);}
		iEntity3D *GetEntityParent(){ return mpE->parentEntity ? (iEntity3D*)mpE->parentEntity->owner : NULL;}
		cEntity3DIterator GetChildIterator();
		
		/**
		* The node3d child hierarchy will only work if the node child has no node parent.
		**/
		void AddNodeChild(cNode3D *apNode);
		void RemoveNodeChild(cNode3D *apNode);
		bool IsNodeChild(cNode3D *apNode);

		inline int GetIteratorCount(){ return mpE->iteratorCount;}
		inline void SetIteratorCount(const int alX){ mpE->iteratorCount = alX;}

		size_t GetNodeChildCount(){ return ohpl_ent_node_child_count(mpE);}
		cNode3D *GetNodeChildAt(size_t alIdx){ return (cNode3D*)ohpl_ent_node_child_at(mpE, alIdx);}

		void SetApplyTransformToBV(bool abX){ mpE->applyBV = abX;}
		bool GetUpdateBoundingVolume(){ return mpE->updateBV;}
		void SetUpdateBoundingVolume(bool abX){ mpE->updateBV = abX;}

	protected:
		virtual void OnTransformUpdated(){}

		OhplEntPod *mpE;

		tString msName;
		cBoundingVolume mBoundingVolume;

		tString msSourceFile;

		tEntityCallbackList mlstCallbacks;
	private:
		void UpdateWorldTransform(){ ohpl_h10_ent_update_world(OhplH10GetHost(), mpE);}
	};

};
#endif // HPL_ENTITY3D_H
