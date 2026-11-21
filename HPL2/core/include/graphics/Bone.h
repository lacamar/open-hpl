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

#ifndef HPL_BONE_H
#define HPL_BONE_H

#include <map>

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "system/MemoryManager.h"
#include "graphics/RustGraphicsH6.h"

namespace hpl {

	class cSkeleton;
	class cBone;

	typedef std::list<cBone*> tBoneList;
	typedef tBoneList::iterator tBoneListIt;

	typedef cSTLIterator<cBone*,tBoneList,tBoneListIt> cBoneIterator;

	class cBone
	{
	friend class cSkeleton;
	public:
		cBone(const tString &asName,const tString &asSid, OhplH6Skel *apRs, cBone *apParent) : msName(asName), msSid(asSid), mpRs(apRs), mpParent(apParent)
		{
			mlRsId = apParent ? ohpl_h6_skel_create_child(apRs, apParent->mlRsId, asName.data(), asName.size(), asSid.data(), asSid.size()) : 0;
			ohpl_h6_bone_set_owner(apRs, mlRsId, this);
		}
		~cBone(){ for(cBone *pBone : mlstChildren) hplDelete(pBone); }

		cBone* CreateChildBone(const tString &asName,const tString &asId)
		{
			cBone *pBone = hplNew(cBone,(asName,asId, mpRs, this));
			mlstChildren.push_back(pBone);
			return pBone;
		}

		void SetTransform(const cMatrixf &a_mtxTransform){ ohpl_h6_bone_set_transform(mpRs, mlRsId, &a_mtxTransform);}
		const cMatrixf& GetLocalTransform(){ return *ohpl_h6_bone_local(mpRs, mlRsId);}
		const cMatrixf& GetWorldTransform(){ return *ohpl_h6_bone_world(mpRs, mlRsId);}
		const cMatrixf& GetInvWorldTransform(){ return *ohpl_h6_bone_inv_world(mpRs, mlRsId);}

		void SetInvBindTransform(const cMatrixf &a_mtxInvBind){ ohpl_h6_bone_set_inv_bind(mpRs, mlRsId, &a_mtxInvBind);}
		const cMatrixf& GetInvBindTransform(){ return *ohpl_h6_bone_inv_bind(mpRs, mlRsId);}

		void SetLocalUnitScale(float afX){ mfLocalUnitScale = afX; }
		float GetLocalUnitScale(){ return mfLocalUnitScale; }

		void SetTransformUnscaled(const cMatrixf &a_mtxTransform){ ohpl_h6_bone_set_unscaled(mpRs, mlRsId, &a_mtxTransform);}
		const cMatrixf& GetWorldTransformUnscaled(){ return *ohpl_h6_bone_world_unscaled(mpRs, mlRsId);}
		const cMatrixf& GetLocalTransformUnscaled(){ return *ohpl_h6_bone_local_unscaled(mpRs, mlRsId);}
		void NeedsUpdateUnscaled(){ ohpl_h6_bone_needs_update_unscaled(mpRs, mlRsId);}

		const tString& GetName(){ return msName;}
		const tString& GetSid(){ return msSid;}

		cBoneIterator GetChildIterator(){ return cBoneIterator(&mlstChildren);}
		tBoneList* GetChildList(){ return &mlstChildren; }

		void Detach()
		{
			if(mpParent == NULL) return;
			mpParent->mlstChildren.remove(this);
			ohpl_h6_bone_detach(mpRs, mlRsId);
		}

		cBone* GetParent(){ return mpParent;}

		int GetValue(){ return mlValue;}
		void SetValue(int alVal){ mlValue = alVal;}

	private:
		tString msName;
		tString msSid;
		float mfLocalUnitScale = 1;
		OhplH6Skel *mpRs;
		cBone *mpParent;
		tBoneList mlstChildren;
		int mlValue = 0;
		int mlRsId;
	};
};
#endif // HPL_BONE_H
