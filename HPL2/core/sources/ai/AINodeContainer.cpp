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

#include "ai/AINodeContainer.h"

#include "scene/World.h"
#include "physics/PhysicsBody.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// RAY INTERSECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cAINodeRayCallback::Reset()
	{
		mbIntersected = false;
		mpCallback = NULL;
	}
	
	//-----------------------------------------------------------------------

	bool cAINodeRayCallback::Intersected()
	{
		return mbIntersected;
	}

	//-----------------------------------------------------------------------
	
	bool cAINodeRayCallback::BeforeIntersect(iPhysicsBody *pBody)
	{
		if(pBody->GetCollideCharacter()==false) return false;

		if( (mFlags & eAIFreePathFlag_SkipStatic) && pBody->GetMass() == 0) return false;

		if( (mFlags & eAIFreePathFlag_SkipDynamic) && 
			(pBody->GetMass() > 0 || pBody->IsCharacter()) ) return false;

		if( (mFlags & eAIFreePathFlag_SkipVolatile) && pBody->IsVolatile()) return false;

		return true;
	}

	//-----------------------------------------------------------------------

	bool cAINodeRayCallback::OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams)
	{
		if(mpCallback)
		{
			if(mpCallback->Intersects(pBody,apParams))
			{
				mbIntersected = true;
				return false;
			}
			else
			{
				return true;
			}
		}
		else
		{
			mbIntersected = true;
			return false;
		}
	}

	//-----------------------------------------------------------------------

	bool (*cAINodeContainer::mpTestCast)(unsigned int, const cVector3f*, const cVector3f*) = NULL;

	cAINodeContainer::cAINodeContainer(	const tString& asName, const tString &asNodeName, 
										cWorld *apWorld, const cVector3f &avCollideSize)
	{
		mpWorld = apWorld;
		mvSize = avCollideSize;
		msName = asName;
		msNodeName = asNodeName;

		mpRayCallback = hplNew( cAINodeRayCallback, () );
		mpFreePathCallback = NULL;
		mpGraph = ohpl_ai_graph_new(CastRay, this, &mvSize);

		mlMaxNodeEnds = 5;
		mlMinNodeEnds = 2;
		mfMaxEndDistance = 3.0f;
		mfMaxHeight = 0.1f;

		mbNodeIsAtCenter = true;

		SyncParams();
	}

	//-----------------------------------------------------------------------

	cAINodeContainer::~cAINodeContainer()
	{
		hplDelete(mpRayCallback);
		ohpl_ai_graph_free(mpGraph);
	}	

	//-----------------------------------------------------------------------

	bool cAINodeContainer::CastRay(void *apContainer, unsigned int alFlags, const cVector3f *apStart, const cVector3f *apEnd)
	{
		cAINodeContainer *pContainer = (cAINodeContainer*)apContainer;
		if(pContainer->mpWorld==NULL) return mpTestCast ? mpTestCast(alFlags, apStart, apEnd) : false;
		iPhysicsWorld *pPhysicsWorld = pContainer->mpWorld->GetPhysicsWorld();
		if(pPhysicsWorld==NULL) return false;

		cAINodeRayCallback *pRayCallback = pContainer->mpRayCallback;
		pRayCallback->SetFlags(alFlags);
		pRayCallback->Reset();
		pRayCallback->mpCallback = pContainer->mpFreePathCallback;

		pPhysicsWorld->CastRay(pRayCallback,*apStart,*apEnd,false,false,false,true);

		return pRayCallback->Intersected();
	}

	//-----------------------------------------------------------------------

}
