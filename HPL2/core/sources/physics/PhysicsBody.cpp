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
		mRsBook = ohpl_h15_body_new();
		ohpl_h15b_new(HostOp, reinterpret_cast<size_t>(this), &mS, reinterpret_cast<size_t>(apWorld), reinterpret_cast<size_t>(apShape));

		iPhysicsWorld *pWorldNewton = static_cast<iPhysicsWorld*>(apWorld);
		iCollideShape *pShapeNewton = static_cast<iCollideShape*>(apShape);

		mpNewtonWorld = pWorldNewton->GetNewtonWorld();
		cMatrixf mtxIdentity = cMatrixf::Identity;
		mhRust = ohpl_ntb_create(pWorldNewton->RustHandle(), pShapeNewton->GetNewtonCollision(), (size_t)this, &mpNewtonBody);

		mpCallback = hplNew( cPhysicsBodyNewtonCallback, () );

		AddCallback(mpCallback);

		//Set default property settings
		mbGravity = true;

		mfMaxLinearSpeed =0;
		mfMaxAngularSpeed =0;
		mfMass =0;

		mfAutoDisableLinearThreshold = 0.01f;
		mfAutoDisableAngularThreshold = 0.01f;
		mlAutoDisableNumSteps = 10;

		//Log("Creating newton body '%s' %d\n",msName.c_str(),(size_t)this);
	}

	iPhysicsBody::~iPhysicsBody()
	{
		ohpl_h15_body_free(mRsBook);
		ohpl_h15b_free(mS.shell);
	}

	void iPhysicsBody::Destroy()
	{
		ohpl_h15b_destroy(mS.shell, reinterpret_cast<size_t>(this));
	}

	// PUBLIC METHODS

	cVector3f iPhysicsBody::GetVelocityAtPosition(const cVector3f& avPos)
	{
		cVector3f vVel;
		ohpl_phys_body_velocity_at_position(OhplPhysHost(), reinterpret_cast<size_t>(this), &avPos, &vVel);
		return vVel;
	}

	cVector3f iPhysicsBody::GetTorqueFromForceAtPosition(const cVector3f& avForce, const cVector3f& avPos)
	{
		cVector3f vTorque;
		ohpl_phys_body_torque_from_force(OhplPhysHost(), reinterpret_cast<size_t>(this), &avForce, &avPos, &vTorque);
		return vTorque;
	}

	void iPhysicsBody::AddJoint(iPhysicsJoint *apJoint)
	{
		ohpl_h15b_add_joint(mS.shell, reinterpret_cast<size_t>(apJoint));
	}

	iPhysicsJoint* iPhysicsBody::GetJoint(int alIndex)
	{
		return reinterpret_cast<iPhysicsJoint*>(ohpl_h15b_joint_at(mS.shell, alIndex));
	}

	int iPhysicsBody::GetJointNum()
	{
		return ohpl_h15b_joint_num(mS.shell);
	}

	void iPhysicsBody::RemoveJoint(iPhysicsJoint *apJoint)
	{
		ohpl_h15b_remove_joint(mS.shell, reinterpret_cast<size_t>(apJoint));
	}

	bool iPhysicsBody::UpdateBeforeSimulate(float afTimeStep)
	{
		return ohpl_phys_body_update_before(OhplPhysHost(), reinterpret_cast<size_t>(this), mRsBook);
	}

	void iPhysicsBody::UpdateAfterSimulate(float afTimeStep)
	{
		ohpl_phys_body_update_after(OhplPhysHost(), reinterpret_cast<size_t>(this), afTimeStep, mRsBook);
	}

	void iPhysicsBody::StaticLinearMove(const cVector3f& avVelocity)
	{
		ohpl_phys_body_static_linear_move(OhplPhysHost(), reinterpret_cast<size_t>(this), &avVelocity, mRsBook);
	}

	void iPhysicsBody::StaticAngularMove(const cVector3f& avVelocity)
	{
		ohpl_phys_body_static_angular_move(OhplPhysHost(), reinterpret_cast<size_t>(this), &avVelocity, mRsBook);
	}

	void iPhysicsBody::AddBodyCallback(iPhysicsBodyCallback *apCallback)
	{
		ohpl_h15b_add_callback(mS.shell, reinterpret_cast<size_t>(apCallback));
	}

	void iPhysicsBody::RemoveBodyCallback(iPhysicsBodyCallback *apCallback)
	{
		ohpl_h15b_remove_callback(mS.shell, reinterpret_cast<size_t>(apCallback));
	}

	void iPhysicsBody::SetScrapeSoundEntity(cSoundEntity *apEntity)
	{
		mpScrapeSoundEntity = apEntity;
		if(apEntity)
			mlScrapeSoundEntityID = apEntity->GetCreationID();
		else
			mlScrapeSoundEntityID = -1;
	}

	void iPhysicsBody::SetRollSoundEntity(cSoundEntity *apEntity)
	{
		mpRollSoundEntity = apEntity;
		if(apEntity)
			mlRollSoundEntityID = apEntity->GetCreationID();
		else
			mlRollSoundEntityID = -1;
	}

	bool iPhysicsBody::OnAABBCollision(iPhysicsBody *apBody)
	{
		return ohpl_h15b_on_aabb(mS.shell, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(apBody));
	}

	void iPhysicsBody::OnCollide(iPhysicsBody *apBody, cPhysicsContactData* apContactData)
	{
		ohpl_h15_body_flag_set(mRsBook, OhplH15_HasCollision, true);

		ohpl_h15b_on_collide(mS.shell, reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(apBody), reinterpret_cast<size_t>(apContactData));
	}

	void iPhysicsBody::AddConnectedCharacter(iCharacterBody *apCharBody)
	{
		ohpl_h15b_add_char(mS.shell, reinterpret_cast<size_t>(apCharBody));
	}

	void iPhysicsBody::RemoveConnectedCharacter(iCharacterBody *apCharBody)
	{
		ohpl_h15b_remove_char(mS.shell, reinterpret_cast<size_t>(apCharBody));
	}

	void iPhysicsBody::AddAttachedVerletContainer(iVerletParticleContainer *apContainer)
	{
		ohpl_h15b_add_verlet(mS.shell, reinterpret_cast<size_t>(apContainer));
	}

	void iPhysicsBody::RemoveAttachedVerletContainer(iVerletParticleContainer *apContainer)
	{
		ohpl_h15b_remove_verlet(mS.shell, reinterpret_cast<size_t>(apContainer));
	}

}
