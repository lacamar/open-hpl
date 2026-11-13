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

#include "physics/PhysicsMaterial.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
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

	iPhysicsMaterial::iPhysicsMaterial(const tString &asName, iPhysicsWorld *apWorld,int alMatId)
		: mpWorld(apWorld), msName(asName), mbPreloaded(false), mpSurfaceData(NULL)
	{
		iPhysicsWorld *pNWorld = static_cast<iPhysicsWorld*>(mpWorld);

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


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iPhysicsMaterial::SetElasticity(float afElasticity)
	{
		mfElasticity = afElasticity;

		UpdateMaterials();
	}

	float iPhysicsMaterial::GetElasticity() const
	{
		return mfElasticity;
	}

	//-----------------------------------------------------------------------

	void iPhysicsMaterial::SetStaticFriction(float afElasticity)
	{
		mfStaticFriction = afElasticity;

		UpdateMaterials();
	}

	float iPhysicsMaterial::GetStaticFriction() const
	{
		return mfStaticFriction;
	}

	//-----------------------------------------------------------------------

	void iPhysicsMaterial::SetKineticFriction(float afElasticity)
	{
		mfKineticFriction = afElasticity;

		UpdateMaterials();
	}

	float iPhysicsMaterial::GetKineticFriction() const
	{
		return mfKineticFriction;
	}

	//-----------------------------------------------------------------------

	void iPhysicsMaterial::SetFrictionCombMode(ePhysicsMaterialCombMode aMode)
	{
		mFrictionMode = aMode;

		UpdateMaterials();
	}

	ePhysicsMaterialCombMode iPhysicsMaterial::GetFrictionCombMode() const
	{
		return mFrictionMode;
	}

	//-----------------------------------------------------------------------

	void iPhysicsMaterial::SetElasticityCombMode(ePhysicsMaterialCombMode aMode)
	{
		mElasticityMode = aMode;

		UpdateMaterials();
	}

	//-----------------------------------------------------------------------

	ePhysicsMaterialCombMode iPhysicsMaterial::GetElasticityCombMode() const
	{
		return mElasticityMode;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iPhysicsMaterial::UpdateMaterials()
	{
		cPhysicsMaterialIterator MatIt = mpWorld->GetMaterialIterator();

		while(MatIt.HasNext())
		{
			iPhysicsMaterial* pMat = static_cast<iPhysicsMaterial*>(MatIt.Next());

			OhplNewtonMatProps propsA{mFrictionMode, mElasticityMode, mfElasticity, mfStaticFriction, mfKineticFriction};
			OhplNewtonMatProps propsB{pMat->mFrictionMode, pMat->mElasticityMode, pMat->mfElasticity,
										pMat->mfStaticFriction, pMat->mfKineticFriction};
			ohpl_ntm_update_pair(static_cast<iPhysicsWorld*>(mpWorld)->RustHandle(), mlMaterialId, pMat->mlMaterialId,
								&propsA, &propsB, pMat == this);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// STATIC NEWTON CALLBACKS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iPhysicsMaterial::DispatchContacts(iPhysicsBody* pContactBody1, iPhysicsBody* pContactBody2,
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
