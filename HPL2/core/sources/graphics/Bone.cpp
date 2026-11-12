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
#include "impl/RustBackends.h"
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

		mbNeedsUpdate = true;
		mbHasInvBind = false;
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
		if(Rs())
		{
			pBone->mlRsId = ohpl_h6_skel_create_child(Rs(), mlRsId, asName.data(), asName.size(), asId.data(), asId.size());
			mpSkeleton->RsRegister(pBone);
		}
		else mpSkeleton->AddBone(pBone);
		return pBone;
	}

	//-----------------------------------------------------------------------

	void cBone::SetTransform(const cMatrixf &a_mtxTransform)
	{
		if(Rs()) { ohpl_h6_bone_set_transform(Rs(), mlRsId, &a_mtxTransform); return; }
		m_mtxTransform = a_mtxTransform;
		NeedsUpdate();
	}

	//-----------------------------------------------------------------------

	//// -- <UNSCALED> -- ///

	void cBone::NeedsUpdateUnscaled()
	{
		if(Rs()) { ohpl_h6_bone_needs_update_unscaled(Rs(), mlRsId); return; }
		mbNeedsUnscaledUpdate = true;

		tBoneListIt it = mlstChildren.begin();
		for(;it != mlstChildren.end(); it++)
		{
			cBone* pBone = *it;

			pBone->NeedsUpdateUnscaled();
		}
	}

	void cBone::SetTransformUnscaled(const cMatrixf &a_mtxTransformUnscaled)
	{
		if(Rs()) { ohpl_h6_bone_set_unscaled(Rs(), mlRsId, &a_mtxTransformUnscaled); return; }
		m_mtxTransformUnscaled = a_mtxTransformUnscaled;
		NeedsUpdateUnscaled();
	}

	const cMatrixf& cBone::GetLocalTransformUnscaled()
	{
		if(Rs()) return *ohpl_h6_bone_local_unscaled(Rs(), mlRsId);
		return m_mtxTransformUnscaled;
	}

	const cMatrixf& cBone::GetWorldTransformUnscaled()
	{
		if(Rs()) return *ohpl_h6_bone_world_unscaled(Rs(), mlRsId);
		UpdateUnscaledMatrix();
		
        return m_mtxWorldTransformUnscaled;
	}

	void cBone::UpdateUnscaledMatrix()
	{
		if(mbNeedsUnscaledUpdate == false) return;

		if(mpParent==NULL)
		{
			m_mtxWorldTransformUnscaled = m_mtxTransformUnscaled;
		}
		else
		{
			m_mtxWorldTransformUnscaled = cMath::MatrixMul(mpParent->GetWorldTransformUnscaled(),m_mtxTransformUnscaled);
		}

		//m_mtxInvWorldTransform = cMath::MatrixInverse(m_mtxWorldTransform);

		mbNeedsUnscaledUpdate = false;
	}

	//// -- </UNSCALED> -- ///

	
	//-----------------------------------------------------------------------

	const cMatrixf& cBone::GetLocalTransform()
	{
		if(Rs()) return *ohpl_h6_bone_local(Rs(), mlRsId);
		return m_mtxTransform;
	}

	const cMatrixf& cBone::GetWorldTransform()
	{
		if(Rs()) return *ohpl_h6_bone_world(Rs(), mlRsId);
		UpdateMatrix();
		
        return m_mtxWorldTransform;
	}

	const cMatrixf& cBone::GetInvWorldTransform()
	{
		if(Rs()) return *ohpl_h6_bone_inv_world(Rs(), mlRsId);
		UpdateMatrix();

		return m_mtxInvWorldTransform;
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

		if(Rs()) ohpl_h6_bone_detach(Rs(), mlRsId);
		else mpSkeleton->RemoveBone(this);
	}

	//-----------------------------------------------------------------------

	void cBone::SetInvBindTransform(const cMatrixf &a_mtxInvBind)
	{
		if(Rs()) ohpl_h6_bone_set_inv_bind(Rs(), mlRsId, &a_mtxInvBind);
		else m_mtxInvBind = a_mtxInvBind;
		mbHasInvBind = true;
	}

	const cMatrixf& cBone::GetInvBindTransform()
	{
		if(Rs()) return *ohpl_h6_bone_inv_bind(Rs(), mlRsId);
		return mbHasInvBind ? m_mtxInvBind : GetInvWorldTransform();
	}

	OhplH6Skel* cBone::Rs() const
	{
		return mpSkeleton ? mpSkeleton->mpRs : NULL;
	}

	cBoneIterator cBone::GetChildIterator()
	{
		return cBoneIterator(&mlstChildren);
	}

	//-----------------------------------------------------------------------

	void cBone::UpdateMatrix()
	{
		if(mbNeedsUpdate == false) return;

		if(mpParent==NULL)
		{
			m_mtxWorldTransform = m_mtxTransform;
		}
		else
		{
			m_mtxWorldTransform = cMath::MatrixMul(mpParent->GetWorldTransform(),m_mtxTransform);
		}

		m_mtxInvWorldTransform = cMath::MatrixInverse(m_mtxWorldTransform);

		mbNeedsUpdate = false;
	}


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cBone::NeedsUpdate()
	{
		mbNeedsUpdate = true;

		tBoneListIt it = mlstChildren.begin();
		for(;it != mlstChildren.end(); it++)
		{
			cBone* pBone = *it;

			pBone->NeedsUpdate();
		}
	}
	//-----------------------------------------------------------------------
}
