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
			mlMaterialId = OhplL4UseRust() ? ohpl_ntm_create_group(pNWorld->RustHandle()) : NewtonMaterialCreateGroupID(mpNewtonWorld);
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
			if(OhplL4UseRust())
			{
				ohpl_ntm_update_pair(static_cast<cPhysicsWorldNewton*>(mpWorld)->RustHandle(), mlMaterialId, pMat->mlMaterialId,
									&propsA, &propsB, pMat == this);
				continue;
			}
			OhplNewtonPairParams pair;
			ohpl_newton_pair_params(&propsA, &propsB, pMat == this, &pair);

			NewtonMaterialSetDefaultElasticity(mpNewtonWorld,mlMaterialId,pMat->mlMaterialId, pair.elasticity);

			NewtonMaterialSetDefaultFriction(mpNewtonWorld,mlMaterialId,pMat->mlMaterialId,
				pair.staticFriction, pair.kineticFriction);

			NewtonMaterialSetContinuousCollisionMode(mpNewtonWorld,mlMaterialId,pMat->mlMaterialId,
													1);

			NewtonMaterialSetCollisionCallback(mpNewtonWorld,mlMaterialId,pMat->mlMaterialId,
												(void*)NULL,OnAABBOverlapCallback,ContactsProcessCallback);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// NEWTON LOCK CLASS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cNewtonLockBodyUntilReturn::cNewtonLockBodyUntilReturn(const NewtonBody* apNewtonBody)
	{
		mpNewtonBody = apNewtonBody;
		NewtonWorldCriticalSectionLock (NewtonBodyGetWorld (mpNewtonBody));
	}
	
	//-----------------------------------------------------------------------

	cNewtonLockBodyUntilReturn::~cNewtonLockBodyUntilReturn()
	{
		NewtonWorldCriticalSectionUnlock (NewtonBodyGetWorld (mpNewtonBody));
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// STATIC NEWTON CALLBACKS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	int cPhysicsMaterialNewton::OnAABBOverlapCallback(	const NewtonMaterial* apMaterial,
														const NewtonBody* apBody1, const NewtonBody* apBody2,
														int alThreadIndex)
	{
		cPhysicsBodyNewton* pContactBody1 = (cPhysicsBodyNewton*) NewtonBodyGetUserData(apBody1);
		cPhysicsBodyNewton* pContactBody2 = (cPhysicsBodyNewton*) NewtonBodyGetUserData(apBody2);

		OhplNewtonGateBody gate1{pContactBody1->GetCollide(), pContactBody1->IsActive(), pContactBody1->IsRagDoll(),
								pContactBody1->GetCollideRagDoll(), pContactBody1->IsCharacter(), pContactBody1->GetCollideCharacter()};
		OhplNewtonGateBody gate2{pContactBody2->GetCollide(), pContactBody2->IsActive(), pContactBody2->IsRagDoll(),
								pContactBody2->GetCollideRagDoll(), pContactBody2->IsCharacter(), pContactBody2->GetCollideCharacter()};
		if(ohpl_newton_aabb_gate(iPhysicsBody::CollideFlagsMatch(pContactBody1->GetCollideFlags(), pContactBody2->GetCollideFlags()),
								&gate1, &gate2)==false) return 0;
		
		//TODO: Not using this in character body any more!
		//if(pContactBody1->IsCharacter() || pContactBody2->IsCharacter()) return 0;

		//Log("----- Begin contact between body '%s' and '%s'.\n",mpContactBody1->GetName().c_str(),
		//													mpContactBody2->GetName().c_str());

		//Thread lock
		cNewtonLockBodyUntilReturn criticalLock1(apBody1);
		cNewtonLockBodyUntilReturn criticalLock2(apBody2);
		
		//Call the callbacks
		if(pContactBody1->OnAABBCollision(pContactBody2)==false) return 0;
		if(pContactBody2->OnAABBCollision(pContactBody1)==false) return 0;

		
		return 1;
	}

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

	void cPhysicsMaterialNewton::ContactsProcessCallback(const NewtonJoint* apContactJoint, dFloat afTimestep, int alThreadIndex)
	{
		////////////////////////////////
		//Get bodies
		NewtonBody *pBody0 = NewtonJointGetBody0(apContactJoint);
		NewtonBody *pBody1 = NewtonJointGetBody1(apContactJoint);

		cPhysicsBodyNewton* pContactBody1 = (cPhysicsBodyNewton*) NewtonBodyGetUserData(pBody0);
		cPhysicsBodyNewton* pContactBody2 = (cPhysicsBodyNewton*) NewtonBodyGetUserData(pBody1);

		////////////////////////////////
		//Set up variables

		int lContactNum =0;
		cPhysicsContactData contactData;

		////////////////////////////////
		//Iterate all contacts
		OhplNewtonContact stackContacts[64];
		std::vector<OhplNewtonContact> heapContacts;
		void* pContact = NewtonContactJointGetFirstContact (apContactJoint);
		for(; pContact != NULL; pContact = NewtonContactJointGetNextContact (apContactJoint, pContact)) 
		{
			NewtonMaterial* pMaterial =  NewtonContactGetMaterial (pContact);

			OhplNewtonContact contact;
			contact.normalSpeed = NewtonMaterialGetContactNormalSpeed(pMaterial);
			contact.tangentSpeed0 = NewtonMaterialGetContactTangentSpeed(pMaterial,0);
			contact.tangentSpeed1 = NewtonMaterialGetContactTangentSpeed(pMaterial,1);
			NewtonMaterialGetContactForce(pMaterial,(NewtonBody*)pBody0,contact.force.v);
			NewtonMaterialGetContactPositionAndNormal(pMaterial,(NewtonBody*)pBody0,contact.position.v, contact.normal.v);

			if(lContactNum < 64) stackContacts[lContactNum] = contact;
			else
			{
				if(lContactNum == 64) heapContacts.assign(stackContacts, stackContacts + 64);
				heapContacts.push_back(contact);
			}

			if(pContactBody1->GetWorld()->GetSaveContactPoints())
			{
				//Thread lock
				NewtonWorldCriticalSectionLock (NewtonBodyGetWorld (pBody0));

				cCollidePoint collidePoint;
				collidePoint.mfDepth = 1;
				collidePoint.mvPoint = contact.position;
				collidePoint.mvNormal = contact.normal;

				pContactBody1->GetWorld()->GetContactPoints()->push_back(collidePoint);

				//Thread unlock
				NewtonWorldCriticalSectionUnlock (NewtonBodyGetWorld (pBody0));
			}

			lContactNum++;
		}

		OhplNewtonContactAgg agg;
		ohpl_newton_aggregate_contacts(lContactNum > 64 ? heapContacts.data() : stackContacts, lContactNum, &agg);
		contactData.mfMaxContactNormalSpeed = agg.maxNormalSpeed;
		contactData.mfMaxContactTangentSpeed = agg.maxTangentSpeed;
		contactData.mvForce = agg.force;
		contactData.mvContactNormal = agg.normal;
		contactData.mvContactPosition = agg.position;

		////////////////////////////////
		//End contact process
		//Thread lock
		NewtonWorldCriticalSectionLock (NewtonBodyGetWorld (pBody0));
		NewtonWorldCriticalSectionLock (NewtonBodyGetWorld (pBody1));

		DispatchContacts(pContactBody1, pContactBody2, contactData, lContactNum);

		//Thread unlock
		NewtonWorldCriticalSectionUnlock (NewtonBodyGetWorld (pBody0));
		NewtonWorldCriticalSectionUnlock (NewtonBodyGetWorld (pBody1));


	}

	//-----------------------------------------------------------------------

}
