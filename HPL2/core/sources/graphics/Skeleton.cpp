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
#include "impl/RustBackends.h"
#include "graphics/RustGraphicsH6.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSkeleton::cSkeleton()
	{
		mpRs = OhplBackendRustRequested() ? ohpl_h6_skel_new() : NULL;
		mpRootBone = hplNew( cBone, ("__root_bone","",this) );
		if(mpRs)
		{
			mpRootBone->mlRsId = 0;
			mvRsById.push_back(mpRootBone);
		}
		mpRootBone->SetTransform(cMatrixf::Identity);
		mpRootBone->SetTransformUnscaled(cMatrixf::Identity);
	}

	//-----------------------------------------------------------------------

	cSkeleton::~cSkeleton()
	{
		//All bones are deleted in this call. 
		//No need to delete the bones in the containers.
		hplDelete(mpRootBone);
		if(mpRs) ohpl_h6_skel_free(mpRs);
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

	void cSkeleton::AddBone(cBone* apBone)
	{
		if(mpRs) return;
		mvBones.push_back(apBone);

		m_mapBonesIdxByName.insert(tBoneIdxNameMap::value_type(apBone->GetName(), (int)mvBones.size()-1));
	}

	void cSkeleton::RemoveBone(cBone* apBone)
	{
		if(mpRs) { ohpl_h6_bone_detach(mpRs, apBone->mlRsId); return; }
        tBoneVecIt vecIt = mvBones.begin();
		for(;vecIt != mvBones.end(); ++vecIt)
		{
			if(*vecIt == apBone)
			{
				mvBones.erase(vecIt);
				break;
			}
		}

		//Rebuild the map
		m_mapBonesIdxByName.clear();

		for(int i=0;i< (int) mvBones.size(); i++)
		{
			m_mapBonesIdxByName.insert(tBoneIdxNameMap::value_type(
								mvBones[i]->GetName(),i));
		}
	}
	
	//-----------------------------------------------------------------------

	cBone* cSkeleton::GetRootBone()
	{
		return mpRootBone;
	}

	//-----------------------------------------------------------------------

	cBone* cSkeleton::GetBoneByIndex(int alIndex)
	{
		if(mpRs)
		{
			int lId = ohpl_h6_skel_by_index(mpRs, alIndex);
			return lId < 0 ? NULL : mvRsById[lId];
		}
		return mvBones[alIndex];
	}
	
	cBone* cSkeleton::GetBoneByName(const tString &asName)
	{
		if(mpRs)
		{
			int lIdx = ohpl_h6_skel_index_by_name(mpRs, asName.data(), asName.size());
			return lIdx < 0 ? NULL : mvRsById[ohpl_h6_skel_by_index(mpRs, lIdx)];
		}
		int alIdx = GetBoneIndexByName(asName);
		if(alIdx <0) return NULL;
		
		return mvBones[alIdx];
	}
	cBone* cSkeleton::GetBoneBySid(const tString &asSid)
	{
		if(mpRs)
		{
			int lIdx = ohpl_h6_skel_index_by_sid(mpRs, asSid.data(), asSid.size());
			return lIdx < 0 ? NULL : mvRsById[ohpl_h6_skel_by_index(mpRs, lIdx)];
		}
		for(size_t i=0; i<mvBones.size();++i)
		{
			if(mvBones[i]->GetSid() == asSid) return mvBones[i];
		}
		return NULL;
	}
	
	int cSkeleton::GetBoneIndexByName(const tString &asName)
	{
		if(mpRs) return ohpl_h6_skel_index_by_name(mpRs, asName.data(), asName.size());
		tBoneIdxNameMapIt it = m_mapBonesIdxByName.find(asName);
		if(it == m_mapBonesIdxByName.end()) return -1;

		return it->second;
	}
	int cSkeleton::GetBoneIndexBySid(const tString &asSid)
	{
		if(mpRs) return ohpl_h6_skel_index_by_sid(mpRs, asSid.data(), asSid.size());
		for(size_t i=0; i<mvBones.size();++i)
		{
			if(mvBones[i]->GetSid() == asSid) return (int)i;
		}
		return-1;
	}
	int cSkeleton::GetBoneNum()
	{
		if(mpRs) return ohpl_h6_skel_num(mpRs);
		return (int)mvBones.size();
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
	}
