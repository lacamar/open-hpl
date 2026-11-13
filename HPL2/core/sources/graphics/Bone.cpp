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

#include "graphics/Bone.h"

#include "graphics/Skeleton.h"
#include "math/Math.h"

#include "system/LowLevelSystem.h"
#include "graphics/RustGraphicsH6.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cBone::cBone(const tString &asName,const tString &asSid, cSkeleton* apSkeleton)
	{
		msName = asName;
		msSid = asSid;
		mpSkeleton = apSkeleton;
		mpParent = NULL;

		mfLocalUnitScale = 1;

		mlValue = 0;
		mlRsId = -1;
	}

	//-----------------------------------------------------------------------

	cBone::~cBone()
	{
		//STLDeleteAll(mlstChildren);
		tBoneListIt it = mlstChildren.begin();
		for(;it != mlstChildren.end(); it++)
		{
			cBone *pBone = *it;
			
			hplDelete( pBone );
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cBone* cBone::CreateChildBone(const tString &asName,const tString &asId)
	{
		cBone *pBone = hplNew(cBone,(asName,asId, mpSkeleton) );
		pBone->mpParent = this;
		mlstChildren.push_back(pBone);
		pBone->mlRsId = ohpl_h6_skel_create_child(Rs(), mlRsId, asName.data(), asName.size(), asId.data(), asId.size());
		mpSkeleton->RsRegister(pBone);
		return pBone;
	}

	//-----------------------------------------------------------------------

	void cBone::SetTransform(const cMatrixf &a_mtxTransform)
	{
		ohpl_h6_bone_set_transform(Rs(), mlRsId, &a_mtxTransform);
	}

	//-----------------------------------------------------------------------

	//// -- <UNSCALED> -- ///

	void cBone::NeedsUpdateUnscaled()
	{
		ohpl_h6_bone_needs_update_unscaled(Rs(), mlRsId);
	}

	void cBone::SetTransformUnscaled(const cMatrixf &a_mtxTransformUnscaled)
	{
		ohpl_h6_bone_set_unscaled(Rs(), mlRsId, &a_mtxTransformUnscaled);
	}

	const cMatrixf& cBone::GetLocalTransformUnscaled()
	{
		return *ohpl_h6_bone_local_unscaled(Rs(), mlRsId);
	}

	const cMatrixf& cBone::GetWorldTransformUnscaled()
	{
		return *ohpl_h6_bone_world_unscaled(Rs(), mlRsId);
	}

	//// -- </UNSCALED> -- ///

	
	//-----------------------------------------------------------------------

	const cMatrixf& cBone::GetLocalTransform()
	{
		return *ohpl_h6_bone_local(Rs(), mlRsId);
	}

	const cMatrixf& cBone::GetWorldTransform()
	{
		return *ohpl_h6_bone_world(Rs(), mlRsId);
	}

	const cMatrixf& cBone::GetInvWorldTransform()
	{
		return *ohpl_h6_bone_inv_world(Rs(), mlRsId);
	}

	//-----------------------------------------------------------------------

	void cBone::Detach()
	{
		if(mpParent == NULL) return;
		
        tBoneListIt it = mpParent->mlstChildren.begin();
		for(;it != mpParent->mlstChildren.end(); it++)
		{
			if(*it == this)
			{
				mpParent->mlstChildren.erase(it);
				break;
			}
		}

		ohpl_h6_bone_detach(Rs(), mlRsId);
	}

	//-----------------------------------------------------------------------

	void cBone::SetInvBindTransform(const cMatrixf &a_mtxInvBind)
	{
		ohpl_h6_bone_set_inv_bind(Rs(), mlRsId, &a_mtxInvBind);
	}

	const cMatrixf& cBone::GetInvBindTransform()
	{
		return *ohpl_h6_bone_inv_bind(Rs(), mlRsId);
	}

	OhplH6Skel* cBone::Rs() const
	{
		return mpSkeleton->mpRs;
	}

	cBoneIterator cBone::GetChildIterator()
	{
		return cBoneIterator(&mlstChildren);
	}

	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	//-----------------------------------------------------------------------
}
