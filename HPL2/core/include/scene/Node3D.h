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
		void ClearEntities();
		
		cNode3D* GetParent();
		
		cNode3DIterator GetChildIterator();
		tNode3DList* GetChildList(){ return &mlstNode;}

		cEntity3DIterator GetEntityIterator();

		cNode3D* CreateChild(const tString &asName="", bool abAutoDeleteChildren = true);
		void RemoveChild(cNode3D* apNode);


		//Extra stuff that shouldn't be used externally
		void SetParent(cNode3D* apNode);
		void AddChild(cNode3D* apChild);
		
		///////////////////////
		//Properties
		const tString& GetName();

		void SetActive(bool abX){mbActive = abX;}
		bool IsActive(){return mbActive;}

		int SetVisible(bool abX, bool abCascade);
		
		cVector3f GetLocalPosition();
		cMatrixf& GetLocalMatrix();

		cVector3f GetWorldPosition();
		cMatrixf& GetWorldMatrix();

		void SetPosition(const cVector3f& avPos);
		void SetMatrix(const cMatrixf& a_mtxTransform, bool abSetChildrenUpdated=true);

		void SetWorldPosition(const cVector3f& avWorldPos);
		void SetWorldMatrix(const cMatrixf& a_mtxWorldTransform);

		void AddRotation(const cVector3f& avRot, eEulerRotationOrder aOrder);
		void AddRotation(const cQuaternion& aqRotation);

		void AddScale(const cVector3f& avScale);

		void AddTranslation(const cVector3f& avTrans);

		bool GetUsePreAnimTransform(){ return mS.mbUsePost;}
		bool GetUsePostAnimTransform(){ return mS.mbUsePost;}
		const cMatrixf& GetPreAnimTransform(){ return mS.m_mtxPost;}
		const cMatrixf& GetPostAnimTransform(){ return mS.m_mtxPost;}
		
		void SetUsePreTransform(bool abX){ mS.mbUsePre = abX;}
		void SetUsePostTransform(bool abX){ mS.mbUsePost = abX;}
		void SetPreTransform(const cMatrixf& a_mtxTransform){ mS.m_mtxPre = a_mtxTransform;}
		void SetPostTransform(const cMatrixf& a_mtxTransform){ mS.m_mtxPost = a_mtxTransform;}
		
		/**
		 * Applies the pre and post transforms to current matrix
		 */
		void ApplyPreAnimTransform(bool abSetChildrenUpdated);
		void ApplyPostAnimTransform(bool abSetChildrenUpdated);

		void SetCustomFlags(int alX){ mlCustomFlags = alX;}
		int GetCustomFlags(){ return mlCustomFlags;}
		
		/**
		 * Updates the matrix with the added scales, translations and rotation. It also resets these values.
		 */
		void UpdateMatrix(bool abSetChildrenUpdated);

		void UpdateWorldTransform();
		void SetWorldTransformUpdated();

		void UpdateEntityChildren();
	private:
		tString msName;
		bool mbActive;
		
		int mlCustomFlags;

		bool mbAutoDeleteChildren;

		OhplH10Node mS;

		cVector3f mvWorldPosition;

		tEntity3DList mlstEntity;
		tNode3DList mlstNode;
	};

};
#endif // HPL_NODE3D_H
