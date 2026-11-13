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

#include "physics/PhysicsBody.h"

#include "physics/CollideShape.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsJoint.h"
#include "system/LowLevelSystem.h"
#include "physics/PhysicsMaterial.h"
#include "physics/SurfaceData.h"
#include "physics/CharacterBody.h"
#include "physics/VerletParticle.h"

#include "scene/World.h"
#include "scene/Scene.h"
#include "engine/Engine.h"

#include "scene/SoundEntity.h"
#include "sound/SoundHandler.h"
#include "sound/SoundChannel.h"

#include "math/Math.h"
#include "physics/RustPhysics.h"

namespace hpl {

	bool (*iPhysicsBody::mpCollideFlagsMatch)(tFlag, tFlag) = NULL;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool iPhysicsBody::mbHpl3Buoyancy = false;

	size_t iPhysicsBody::HostOp(int32_t alOp, OhplH15Wa *a)
	{
		switch(alOp)
		{
		case 301:
		{
			iPhysicsBody *pB = reinterpret_cast<iPhysicsBody*>(a->p[0]);
			pB->mpShape->IncUserCount();
			pB->mBoundingVolume.SetLocalMinMax(pB->mpShape->GetBoundingVolume().GetMin(), pB->mpShape->GetBoundingVolume().GetMax());
			pB->mpMaterial = pB->mpWorld->GetMaterialFromName("Default");
			return 0;
		}
		case 302: reinterpret_cast<iPhysicsWorld*>(a->p[1])->DestroyShape(reinterpret_cast<iCollideShape*>(a->p[2])); return 0;
		case 303: reinterpret_cast<iCharacterBody*>(a->p[0])->DisconnectBody(); return 0;
		case 304: reinterpret_cast<iVerletParticleContainer*>(a->p[0])->RemoveAttachedBody(reinterpret_cast<iPhysicsBody*>(a->p[1]), false); return 0;
		case 305: reinterpret_cast<iPhysicsJoint*>(a->p[0])->RemoveBody(reinterpret_cast<iPhysicsBody*>(a->p[1])); return 0;
		case 306: { iPhysicsJoint *pJ = reinterpret_cast<iPhysicsJoint*>(a->p[0]); return pJ->GetParentBody()==NULL && pJ->GetChildBody()==NULL; }
		case 307: reinterpret_cast<iPhysicsWorld*>(a->p[0])->DestroyJoint(reinterpret_cast<iPhysicsJoint*>(a->p[1])); return 0;
		case 308:
		{
			iPhysicsBody *pB = reinterpret_cast<iPhysicsBody*>(a->p[0]);
			cSoundEntity *pEnt = a->p[1] == 0 ? pB->mpScrapeSoundEntity : pB->mpRollSoundEntity;
			int lId = a->p[1] == 0 ? pB->mlScrapeSoundEntityID : pB->mlRollSoundEntityID;
			if(pEnt && pB->mpWorld->GetWorld()->SoundEntityExists(pEnt, lId)) pB->mpWorld->GetWorld()->DestroySoundEntity(pEnt);
			return 0;
		}
		case 309: reinterpret_cast<iPhysicsBody*>(a->p[0])->DeleteLowLevel(); return 0;
		case 310: return reinterpret_cast<iPhysicsBodyCallback*>(a->p[0])->OnAABBCollide(reinterpret_cast<iPhysicsBody*>(a->p[1]), reinterpret_cast<iPhysicsBody*>(a->p[2]));
		case 311: reinterpret_cast<iPhysicsBodyCallback*>(a->p[0])->OnBodyCollide(reinterpret_cast<iPhysicsBody*>(a->p[1]), reinterpret_cast<iPhysicsBody*>(a->p[2]), reinterpret_cast<cPhysicsContactData*>(a->p[3])); return 0;
		}
		return 0;
	}

	iPhysicsBody::iPhysicsBody(const tString &asName,iPhysicsWorld *apWorld,iCollideShape *apShape)
		: iEntity3D(asName)
	{
		if(OhplBackendRustRequested())
		{
			mRsBook = ohpl_h15_body_new();
			ohpl_h15b_new(HostOp, reinterpret_cast<size_t>(this), &mS, reinterpret_cast<size_t>(apWorld), reinterpret_cast<size_t>(apShape));
			return;
		}

		mpWorld = apWorld;
		mpShape = apShape;

		//Increment user count for the shape
		apShape->IncUserCount();

		mBoundingVolume.SetLocalMinMax(apShape->GetBoundingVolume().GetMin(),
										apShape->GetBoundingVolume().GetMax());

		//Set the default material so that body always has a material.
		mpMaterial = mpWorld->GetMaterialFromName("Default");

		mpCharacterBody = NULL;

		mbBlocksSound = false;
		mbBlocksLight = true;
		mbUseSurfaceEffects = true;

		mpScrapeBody = NULL;
		mpScrapeSoundEntity = NULL;
		mlScrapeSoundEntityID = -1;
		mpRollSoundEntity = NULL;
		mlRollSoundEntityID = -1;
		mRsBook = OhplBackendRustRequested() ? ohpl_h15_body_new() : 0;
		mbHasSlide = false;

		mfBuoyancyDensityMul = 1.0f;
		mfMassMulApplied = 1.0f;

		mlSlideCount = 0;
		mlImpactCount = 0;

		mbInUpdateList = false;

		mlPushStrength = 0;

		mbGravityCanAttachCharacter = true;
		mbGravityAttachmentRotation = true;
		mbGravityAttachmentVelocity= true;
		mGravityAttachmentVelocityAxes = eVelocityAxes_XYZ;

		mpUserData = NULL;

		mbPushedByCharacterGravity = true;

		mbIsCharacter = false;
		mbCollideCharacter = true;

		mbIsRagDoll = false;
		mbCollideRagDoll = true;

		mbCollide = true;

		mbVolatile = false;

		mlFreezeCount = 0;
		mbDisableAfterSimulation = false;

		mbHasCollision = false;

		mbDestroying = false;

		m_mtxPrevScrapeMatrix = cMatrixf::Identity;

		mlCollideFlags = eFlagBit_All;

		mvTotalStaticLinearVel = 0;
		mvTotalStaticAngularVel = 0;
		mbStaticMovement = false;

	}

	//-----------------------------------------------------------------------

	iPhysicsBody::~iPhysicsBody()
	{
		if(mRsBook) ohpl_h15_body_free(mRsBook);
		if(mS.shell) ohpl_h15b_free(mS.shell);
	}

	//-----------------------------------------------------------------------


	void iPhysicsBody::Destroy()
	{
		if(mS.shell){ ohpl_h15b_destroy(mS.shell, reinterpret_cast<size_t>(this)); return; }

		mbDestroying = true;

		mpWorld->DestroyShape(mpShape);

		///////////////////////////
		//Remove connected characters
		tCharacterBodyListIt it = mlstConnectedCharBodies.begin();
		for(; it != mlstConnectedCharBodies.end(); ++it)
		{
			iCharacterBody *pCharBody = *it;
			pCharBody->DisconnectBody();
		}

		///////////////////////////
		//Remove from verlet container
		tVerletParticleContainerListIt conIt = mlstAttachedVerletContainers.begin();
		for(; conIt != mlstAttachedVerletContainers.end(); ++conIt)
		{
			iVerletParticleContainer *pCont = *conIt;
			pCont->RemoveAttachedBody(this, false);
		}
		mlstAttachedVerletContainers.clear();

		///////////////////////////
		//Remove Joints
		for(int i=0; i<(int)mvJoints.size(); i++)
		{
			iPhysicsJoint *pJoint = mvJoints[i];

			pJoint->RemoveBody(this);

			if(pJoint->GetParentBody()==NULL && pJoint->GetChildBody()==NULL)
			{
				mpWorld->DestroyJoint(pJoint);
			}

			//Skip removing for now, just makes things messy...
			/*if(	pJoint->GetParentBody() == this ||
				(pJoint->GetParentBody() == NULL && pJoint->GetChildBody()== this) )
			{
				//Log("  Destroy joint %d\n",(size_t)pJoint);
				mpWorld->DestroyJoint(pJoint);
			}
			else if(pJoint->GetParentBody() == this)
			{
				//Remove this body from the joint
				pJoint->RemoveBody(this);
			}*/
		}

		//Log("Deleted body '%s'\n",msName.c_str());

		////////////////////////
		// Remove sounds
		if(mpScrapeSoundEntity && mpWorld->GetWorld()->SoundEntityExists(mpScrapeSoundEntity, mlScrapeSoundEntityID))
		{
			mpWorld->GetWorld()->DestroySoundEntity(mpScrapeSoundEntity);
		}
		if(mpRollSoundEntity  && mpWorld->GetWorld()->SoundEntityExists(mpRollSoundEntity,mlRollSoundEntityID))
		{
			mpWorld->GetWorld()->DestroySoundEntity(mpRollSoundEntity);
		}

		////////////////////////
		// Lowlevel stuff
		DeleteLowLevel();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVector3f iPhysicsBody::GetVelocityAtPosition(const cVector3f& avPos)
	{
		cVector3f vVel;
		ohpl_phys_body_velocity_at_position(OhplPhysHost(), reinterpret_cast<size_t>(this), &avPos, &vVel);
		return vVel;
	}

	//-----------------------------------------------------------------------

	cVector3f iPhysicsBody::GetTorqueFromForceAtPosition(const cVector3f& avForce, const cVector3f& avPos)
	{
		cVector3f vTorque;
		ohpl_phys_body_torque_from_force(OhplPhysHost(), reinterpret_cast<size_t>(this), &avForce, &avPos, &vTorque);
		return vTorque;
	}

	//-----------------------------------------------------------------------

	void iPhysicsBody::AddJoint(iPhysicsJoint *apJoint)
	{
		if(mS.shell){ ohpl_h15b_add_joint(mS.shell, reinterpret_cast<size_t>(apJoint)); return; }
		mvJoints.push_back(apJoint);
	}

	iPhysicsJoint* iPhysicsBody::GetJoint(int alIndex)
	{
		if(mS.shell) return reinterpret_cast<iPhysicsJoint*>(ohpl_h15b_joint_at(mS.shell, alIndex));
		return mvJoints[alIndex];
	}

	int iPhysicsBody::GetJointNum()
	{
		if(mS.shell) return ohpl_h15b_joint_num(mS.shell);
		return (int)mvJoints.size();
	}

	void iPhysicsBody::RemoveJoint(iPhysicsJoint *apJoint)
	{
		if(mS.shell){ ohpl_h15b_remove_joint(mS.shell, reinterpret_cast<size_t>(apJoint)); return; }
		std::vector<iPhysicsJoint*>::iterator it = mvJoints.begin();
		for(; it != mvJoints.end(); ++it)
		{
			if(*it == apJoint){
				mvJoints.erase(it);
				break;
			}
		}
	}

	//-----------------------------------------------------------------------

	bool iPhysicsBody::UpdateBeforeSimulate(float afTimeStep)
	{
		return ohpl_phys_body_update_before(OhplPhysHost(), reinterpret_cast<size_t>(this), mRsBook);
	}

	//-----------------------------------------------------------------------

	void iPhysicsBody::UpdateAfterSimulate(float afTimeStep)
	{
		ohpl_phys_body_update_after(OhplPhysHost(), reinterpret_cast<size_t>(this), afTimeStep, mRsBook);
	}

	//-----------------------------------------------------------------------

	void iPhysicsBody::StaticLinearMove(const cVector3f& avVelocity)
	{
		ohpl_phys_body_static_linear_move(OhplPhysHost(), reinterpret_cast<size_t>(this), &avVelocity, mRsBook);
	}

	void iPhysicsBody::StaticAngularMove(const cVector3f& avVelocity)
	{
		ohpl_phys_body_static_angular_move(OhplPhysHost(), reinterpret_cast<size_t>(this), &avVelocity, mRsBook);
	}

	//-----------------------------------------------------------------------

	void iPhysicsBody::AddBodyCallback(iPhysicsBodyCallback *apCallback)
	{
		if(mS.shell){ ohpl_h15b_add_callback(mS.shell, reinterpret_cast<size_t>(apCallback)); return; }
		mlstBodyCallbacks.push_back(apCallback);
	}

	void iPhysicsBody::RemoveBodyCallback(iPhysicsBodyCallback *apCallback)
	{
		if(mS.shell){ ohpl_h15b_remove_callback(mS.shell, reinterpret_cast<size_t>(apCallback)); return; }
		tPhysicsBodyCallbackListIt it = mlstBodyCallbacks.begin();
		for(; it != mlstBodyCallbacks.end(); ++it)
		{
			if(apCallback == *it)
			{
				mlstBodyCallbacks.erase(it);
				break;
			}
		}
	}

	//-----------------------------------------------------------------------

	void iPhysicsBody::SetScrapeSoundEntity(cSoundEntity *apEntity)
	{
		mpScrapeSoundEntity = apEntity;
		if(apEntity)
			mlScrapeSoundEntityID = apEntity->GetCreationID();
		else
			mlScrapeSoundEntityID = -1;
	}

	//-----------------------------------------------------------------------


	void iPhysicsBody::SetRollSoundEntity(cSoundEntity *apEntity)
	{
		mpRollSoundEntity = apEntity;
		if(apEntity)
			mlRollSoundEntityID = apEntity->GetCreationID();
		else
			mlRollSoundEntityID = -1;
	}

	//-----------------------------------------------------------------------

	bool iPhysicsBody::OnAABBCollision(iPhysicsBody *apBody)
	{
		if(mS.shell) return ohpl_h15b_on_aabb(mS.shell, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(apBody));

		if(mlstBodyCallbacks.empty()) return true;

		bool bReturn = true;

		tPhysicsBodyCallbackListIt it = mlstBodyCallbacks.begin();
		for(; it != mlstBodyCallbacks.end(); ++it)
		{
			iPhysicsBodyCallback *pCallback = *it;

			if(pCallback->OnAABBCollide(this, apBody)==false) bReturn = false;
		}

		return bReturn;
	}

	//-----------------------------------------------------------------------

	void iPhysicsBody::OnCollide(iPhysicsBody *apBody, cPhysicsContactData* apContactData)
	{
		if(mRsBook) ohpl_h15_body_flag_set(mRsBook, OhplH15_HasCollision, true);
		else mbHasCollision = true;

		if(mS.shell){ ohpl_h15b_on_collide(mS.shell, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(apBody), reinterpret_cast<size_t>(apContactData)); return; }

		if(mlstBodyCallbacks.empty()) return;

		tPhysicsBodyCallbackListIt it = mlstBodyCallbacks.begin();
		for(; it != mlstBodyCallbacks.end(); ++it)
		{
			iPhysicsBodyCallback *pCallback = *it;

			pCallback->OnBodyCollide(this, apBody,apContactData);
		}
	}

	//-----------------------------------------------------------------------

	iPhysicsMaterial* iPhysicsBody::GetMaterial()
	{
		return mpMaterial;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsBody::GetShape()
	{
		return mpShape;
	}

	//-----------------------------------------------------------------------

	void iPhysicsBody::AddConnectedCharacter(iCharacterBody *apCharBody)
	{
		if(mS.shell){ ohpl_h15b_add_char(mS.shell, reinterpret_cast<size_t>(apCharBody)); return; }
		if(mbDestroying) return;

		mlstConnectedCharBodies.push_back(apCharBody);
	}

	void iPhysicsBody::RemoveConnectedCharacter(iCharacterBody *apCharBody)
	{
		if(mS.shell){ ohpl_h15b_remove_char(mS.shell, reinterpret_cast<size_t>(apCharBody)); return; }
		if(mbDestroying) return;

		STLFindAndRemove(mlstConnectedCharBodies,apCharBody);
	}

	//-----------------------------------------------------------------------

	void iPhysicsBody::AddAttachedVerletContainer(iVerletParticleContainer *apContainer)
	{
		if(mS.shell){ ohpl_h15b_add_verlet(mS.shell, reinterpret_cast<size_t>(apContainer)); return; }
		mlstAttachedVerletContainers.push_back(apContainer);
	}

	//-----------------------------------------------------------------------


	void iPhysicsBody::RemoveAttachedVerletContainer(iVerletParticleContainer *apContainer)
	{
		if(mS.shell){ ohpl_h15b_remove_verlet(mS.shell, reinterpret_cast<size_t>(apContainer)); return; }
		tVerletParticleContainerListIt it = mlstAttachedVerletContainers.begin();
		for(; it != mlstAttachedVerletContainers.end(); )
		{
			iVerletParticleContainer *pCont = *it;
            if(pCont == apContainer)
			{
				it = mlstAttachedVerletContainers.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	//-----------------------------------------------------------------------
}
