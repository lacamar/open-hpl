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

#include "graphics/Skeleton.h"

#include "graphics/Bone.h"
#include "system/MemoryManager.h"
#include "system/LowLevelSystem.h"
#include "graphics/RustGraphicsH6.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSkeleton::cSkeleton()
	{
		mpRs = ohpl_h6_skel_new();
		mpRootBone = hplNew( cBone, ("__root_bone","",this) );
		mpRootBone->mlRsId = 0;
		mvRsById.push_back(mpRootBone);
		mpRootBone->SetTransform(cMatrixf::Identity);
		mpRootBone->SetTransformUnscaled(cMatrixf::Identity);
	}

	//-----------------------------------------------------------------------

	cSkeleton::~cSkeleton()
	{
		//All bones are deleted in this call. 
		//No need to delete the bones in the containers.
		hplDelete(mpRootBone);
		ohpl_h6_skel_free(mpRs);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cSkeleton::RsRegister(cBone* apBone)
	{
		if((size_t)apBone->mlRsId >= mvRsById.size()) mvRsById.resize((size_t)apBone->mlRsId + 1, NULL);
		mvRsById[apBone->mlRsId] = apBone;
	}

	void cSkeleton::RemoveBone(cBone* apBone)
	{
		ohpl_h6_bone_detach(mpRs, apBone->mlRsId);
	}
	
	//-----------------------------------------------------------------------

	cBone* cSkeleton::GetRootBone()
	{
		return mpRootBone;
	}

	//-----------------------------------------------------------------------

	cBone* cSkeleton::GetBoneByIndex(int alIndex)
	{
		int lId = ohpl_h6_skel_by_index(mpRs, alIndex);
		return lId < 0 ? NULL : mvRsById[lId];
	}
	
	cBone* cSkeleton::GetBoneByName(const tString &asName)
	{
		int lIdx = ohpl_h6_skel_index_by_name(mpRs, asName.data(), asName.size());
		return lIdx < 0 ? NULL : mvRsById[ohpl_h6_skel_by_index(mpRs, lIdx)];
	}
	cBone* cSkeleton::GetBoneBySid(const tString &asSid)
	{
		int lIdx = ohpl_h6_skel_index_by_sid(mpRs, asSid.data(), asSid.size());
		return lIdx < 0 ? NULL : mvRsById[ohpl_h6_skel_by_index(mpRs, lIdx)];
	}
	
	int cSkeleton::GetBoneIndexByName(const tString &asName)
	{
		return ohpl_h6_skel_index_by_name(mpRs, asName.data(), asName.size());
	}
	int cSkeleton::GetBoneIndexBySid(const tString &asSid)
	{
		return ohpl_h6_skel_index_by_sid(mpRs, asSid.data(), asSid.size());
	}
	int cSkeleton::GetBoneNum()
	{
		return ohpl_h6_skel_num(mpRs);
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
	}
