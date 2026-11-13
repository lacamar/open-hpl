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

#include "impl/PhysicsMaterialNewton.h"

#include "impl/PhysicsWorldNewton.h"
#include "impl/PhysicsBodyNewton.h"
#include "physics/SurfaceData.h"
#include "impl/RustNewton.h"
#include "impl/RustNewtonBackend.h"

#include "system/LowLevelSystem.h"
#include <algorithm>
#include <vector>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cPhysicsMaterialNewton::cPhysicsMaterialNewton(const tString &asName, iPhysicsWorld *apWorld,int alMatId)
		: iPhysicsMaterial(asName,apWorld)
	{
		cPhysicsWorldNewton *pNWorld = static_cast<cPhysicsWorldNewton*>(mpWorld);

		mpNewtonWorld = pNWorld->GetNewtonWorld();

		if(alMatId==-1)
		{
			mlMaterialId = ohpl_ntm_create_group(pNWorld->RustHandle());
		}
		else
		{
			mlMaterialId = alMatId;
		}

		//Setup default properties
		mFrictionMode = ePhysicsMaterialCombMode_Average;
		mElasticityMode  = ePhysicsMaterialCombMode_Average;

		mfElasticity = 0.5f;
		mfStaticFriction = 0.3f;
		mfKineticFriction = 0.3f;

		//Log(" Created physics material '%s' with Newton id %d\n",asName.c_str(),mlMaterialId);
	}

	//-----------------------------------------------------------------------

	cPhysicsMaterialNewton::~cPhysicsMaterialNewton()
	{
		/*Might be just as well to let newton handle this*/
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cPhysicsMaterialNewton::SetElasticity(float afElasticity)
	{
		mfElasticity = afElasticity;

		UpdateMaterials();
	}

	float cPhysicsMaterialNewton::GetElasticity() const
	{
		return mfElasticity;
	}

	//-----------------------------------------------------------------------

	void cPhysicsMaterialNewton::SetStaticFriction(float afElasticity)
	{
		mfStaticFriction = afElasticity;

		UpdateMaterials();
	}

	float cPhysicsMaterialNewton::GetStaticFriction() const
	{
		return mfStaticFriction;
	}

	//-----------------------------------------------------------------------

	void cPhysicsMaterialNewton::SetKineticFriction(float afElasticity)
	{
		mfKineticFriction = afElasticity;

		UpdateMaterials();
	}

	float cPhysicsMaterialNewton::GetKineticFriction() const
	{
		return mfKineticFriction;
	}

	//-----------------------------------------------------------------------

	void cPhysicsMaterialNewton::SetFrictionCombMode(ePhysicsMaterialCombMode aMode)
	{
		mFrictionMode = aMode;

		UpdateMaterials();
	}

	ePhysicsMaterialCombMode cPhysicsMaterialNewton::GetFrictionCombMode() const
	{
		return mFrictionMode;
	}

	//-----------------------------------------------------------------------

	void cPhysicsMaterialNewton::SetElasticityCombMode(ePhysicsMaterialCombMode aMode)
	{
		mElasticityMode = aMode;

		UpdateMaterials();
	}

	//-----------------------------------------------------------------------

	ePhysicsMaterialCombMode cPhysicsMaterialNewton::GetElasticityCombMode() const
	{
		return mElasticityMode;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cPhysicsMaterialNewton::UpdateMaterials()
	{
		cPhysicsMaterialIterator MatIt = mpWorld->GetMaterialIterator();

		while(MatIt.HasNext())
		{
			cPhysicsMaterialNewton* pMat = static_cast<cPhysicsMaterialNewton*>(MatIt.Next());

			OhplNewtonMatProps propsA{mFrictionMode, mElasticityMode, mfElasticity, mfStaticFriction, mfKineticFriction};
			OhplNewtonMatProps propsB{pMat->mFrictionMode, pMat->mElasticityMode, pMat->mfElasticity,
										pMat->mfStaticFriction, pMat->mfKineticFriction};
			ohpl_ntm_update_pair(static_cast<cPhysicsWorldNewton*>(mpWorld)->RustHandle(), mlMaterialId, pMat->mlMaterialId,
								&propsA, &propsB, pMat == this);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// STATIC NEWTON CALLBACKS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cPhysicsMaterialNewton::DispatchContacts(cPhysicsBodyNewton* pContactBody1, cPhysicsBodyNewton* pContactBody2,
												cPhysicsContactData &contactData, int lContactNum)
	{
		iPhysicsMaterial *pMaterial1 = pContactBody1->GetMaterial();
		iPhysicsMaterial *pMaterial2 = pContactBody2->GetMaterial();

		////////////////////////////
		//Surface data stuff
		//Only do the effects if both bodies uses surfaces effects!
		if(	pMaterial1->GetSurfaceData() && pMaterial2->GetSurfaceData() &&
			pContactBody1->GetUseSurfaceEffects() && pContactBody2->GetUseSurfaceEffects() &&
			pContactBody1->GetBuoyancyActive()==false && pContactBody2->GetBuoyancyActive()==false)
		{
			pMaterial1->GetSurfaceData()->CreateImpactEffect(contactData.mfMaxContactNormalSpeed,
																contactData.mvContactPosition,
																lContactNum,pMaterial2->GetSurfaceData(),
																pContactBody1->GetWorld());

			OhplNewtonSurfaceCalls calls;
			ohpl_newton_surface_calls(pMaterial1->GetSurfaceData()->GetPriority(), pMaterial2->GetSurfaceData()->GetPriority(),
										pMaterial2 == pMaterial1, contactData.mfMaxContactNormalSpeed,
										contactData.mfMaxContactTangentSpeed, &calls);

			if(calls.impact1)
				pMaterial1->GetSurfaceData()->OnImpact(contactData.mfMaxContactNormalSpeed,
														contactData.mvContactPosition,
														lContactNum,pContactBody1);
			if(calls.slide1)
				pMaterial1->GetSurfaceData()->OnSlide(contactData.mfMaxContactTangentSpeed,
														contactData.mvContactPosition,
														lContactNum,pContactBody1,pContactBody2);
			if(calls.impact2)
				pMaterial2->GetSurfaceData()->OnImpact(contactData.mfMaxContactNormalSpeed,
														contactData.mvContactPosition,
														lContactNum,pContactBody2);
			if(calls.slide2)
				pMaterial2->GetSurfaceData()->OnSlide(contactData.mfMaxContactTangentSpeed,
														contactData.mvContactPosition,
														lContactNum,pContactBody2,pContactBody1);
		}

		pContactBody1->OnCollide(pContactBody2,&contactData);
		pContactBody2->OnCollide(pContactBody1,&contactData);
	}

	//-----------------------------------------------------------------------

}
