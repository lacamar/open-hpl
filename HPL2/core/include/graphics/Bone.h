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

namespace hpl {
	
	class cSkeleton;
	class cBone;
	struct OhplH6Skel;

	typedef std::list<cBone*> tBoneList;
	typedef tBoneList::iterator tBoneListIt;

	typedef cSTLIterator<cBone*,tBoneList,tBoneListIt> cBoneIterator;
    
	class cBone
	{
	friend class cSkeleton;
	public:
		cBone(const tString &asName,const tString &asSid, cSkeleton* apSkeleton);
		~cBone();

		cBone* CreateChildBone(const tString &asName,const tString &asId);

		void SetTransform(const cMatrixf &a_mtxTransform);
		const cMatrixf& GetLocalTransform();
		const cMatrixf& GetWorldTransform();
		const cMatrixf& GetInvWorldTransform();

		void SetInvBindTransform(const cMatrixf &a_mtxInvBind);
		const cMatrixf& GetInvBindTransform();
		void SetLocalUnitScale(float afX){ mfLocalUnitScale = afX; }
		float GetLocalUnitScale(){ return mfLocalUnitScale; }

		//// -- <UNSCALED> -- ///
		void SetTransformUnscaled(const cMatrixf &a_mtxTransform);
		const cMatrixf& GetWorldTransformUnscaled();
		const cMatrixf& GetLocalTransformUnscaled();
		void NeedsUpdateUnscaled();
		//// -- </UNSCALED> -- ///

		const tString& GetName(){ return msName;}
		const tString& GetSid(){ return msSid;}

		cBoneIterator GetChildIterator();
		tBoneList* GetChildList(){ return &mlstChildren; }

		void Detach();

		cBone* GetParent(){ return mpParent;}
		
		//Needed for some loading stuff..
		int GetValue(){ return mlValue;}
		void SetValue(int alVal){ mlValue = alVal;}
		
	private:
		OhplH6Skel* Rs() const;
		tString msName;
		tString msSid;

		float mfLocalUnitScale;
		
		//// -- <UNSCALED> -- ///
		//// -- </UNSCALED> -- ///

		cBone* mpParent;
		tBoneList mlstChildren;

		cSkeleton* mpSkeleton;

		int mlValue;
		int mlRsId;
	};

};
#endif // HPL_BONE_H
