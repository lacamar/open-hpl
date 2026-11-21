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

#ifndef HPL_SKELETON_H
#define HPL_SKELETON_H

#include <map>

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "graphics/Bone.h"

namespace hpl {

	typedef std::vector<cBone*> tBoneVec;
	typedef tBoneVec::iterator tBoneVecIt;

	typedef std::map<tString,int> tBoneIdxNameMap;
	typedef tBoneIdxNameMap::iterator tBoneIdxNameMapIt;

	class cSkeleton
	{
	public:
		cSkeleton() : mpRs(ohpl_h6_skel_new()), mpRootBone(hplNew(cBone,("__root_bone","",mpRs,(cBone*)NULL)))
		{
			mpRootBone->SetTransform(cMatrixf::Identity);
			mpRootBone->SetTransformUnscaled(cMatrixf::Identity);
		}
		~cSkeleton(){ hplDelete(mpRootBone); ohpl_h6_skel_free(mpRs);}

		void RemoveBone(cBone* apBone){ ohpl_h6_bone_detach(mpRs, apBone->mlRsId);}

		cBone* GetRootBone(){ return mpRootBone;}
		cBone* GetBoneByIndex(int alIndex){ return (cBone*)ohpl_h6_skel_owner_at(mpRs, alIndex);}
		cBone* GetBoneByName(const tString &asName){ return GetBoneByIndex(GetBoneIndexByName(asName));}
		cBone* GetBoneBySid(const tString &asSid){ return GetBoneByIndex(GetBoneIndexBySid(asSid));}

		int GetBoneIndexBySid(const tString &asSid){ return ohpl_h6_skel_index_by_sid(mpRs, asSid.data(), asSid.size());}
		int GetBoneIndexByName(const tString &asName){ return ohpl_h6_skel_index_by_name(mpRs, asName.data(), asName.size());}

		int GetBoneNum(){ return ohpl_h6_skel_num(mpRs);}

		OhplH6Skel* Rs(){ return mpRs;}

		bool mbFileUnits = false;

	private:
		OhplH6Skel *mpRs;
		cBone *mpRootBone;
	};
};
#endif // HPL_SKELETON_H
