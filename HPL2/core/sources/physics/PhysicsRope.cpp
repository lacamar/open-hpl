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

#include "physics/PhysicsRope.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"

#include "scene/World.h"
#include "scene/SoundEntity.h"

#include "sound/Sound.h"
#include "sound/SoundHandler.h"

#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"

#include "math/Math.h"
#include "impl/RustBackends.h"
#include "physics/RustPhysicsH14.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iPhysicsRope::iPhysicsRope(const tString &asName,iPhysicsWorld *apWorld, const cVector3f &avStartPos , const cVector3f &avEndPos) 
		: iVerletParticleContainer(asName, apWorld)
	{
		//Add start and end particles.
		mlstParticles.push_front(	hplNew( cVerletParticle, (this,avStartPos, 0) ) );
		mlstParticles.push_back(	hplNew( cVerletParticle, (this,avEndPos, 1) ) );
		RefreshParticleCache();

		for(int i=0; i< 2;++i)
		{
			mS.attached[i].mpBody = NULL;
			mForcePid[i].SetErrorNum(10);
		}

		mS.totalLength = cMath::Vector3Dist(avStartPos, avEndPos);
		mS.segmentLength = 0; //0 = no segments!

		mS.motorActive = false;
		mS.motorWantedLength = mS.totalLength;
		mS.motorSpeedMul = 1.0f;
		mS.motorMinSpeed = -1.0f;
		mS.motorMaxSpeed = 1.0f;

		msMotorSound = "";
		mfMotorSoundStartSpeed = 0.1f;
		mfMotorSoundStopSpeed = 0.05f;
		mpMotorSoundEntity = NULL;
		mlMotorSoundEntityID = -1;

		mS.autoMoveActive = false;
		mS.autoMoveAcc = 1.0f;
		mS.autoMoveMaxSpeed = 1.0f;
		mS.autoMoveSpeed =0;

		mS.minTotalLength = mS.totalLength;
		mS.maxTotalLength = mS.totalLength;

		mS.collideAttachments = false;
		mS.collide = false;

		mS.strength = 100;
		mS.stiffness = 1;

		mS.firstSegmentLength = mS.totalLength;

		mS.maxIterations = 3;
		
		mS.hasUpdated = false;
	}


	//-----------------------------------------------------------------------

	iPhysicsRope::~iPhysicsRope()
	{
		for(size_t i=0;i<2;++i)
		{
			if(mS.attached[i].mpBody)
			{
				mS.attached[i].mpBody->RemoveAttachedVerletContainer(this);
			}
		}

		STLDeleteAll(mlstParticles);
		hplDelete(mpRayParticleCallback);
	}

	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	
	//-----------------------------------------------------------------------
	
	
	//-----------------------------------------------------------------------

	void iPhysicsRope::SetMotorActive(bool abX)
	{
		if(mS.motorActive == abX) return;

		mS.motorActive = abX;

		if(mS.motorActive)
		{
			mS.autoMoveSpeed = 0;
			SetSleeping(false);
		}
	}

	//-----------------------------------------------------------------------

	void iPhysicsRope::SetAutoMoveActive(bool abX)
	{
		mS.autoMoveActive = abX;

        if(mS.autoMoveActive)
		{
			SetSleeping(false);
		}
	}

	bool iPhysicsRope::GetAutoMoveActive()
	{
		return mS.autoMoveActive;
	}

	//-----------------------------------------------------------------------

	void iPhysicsRope::RemoveAttachedBody(iPhysicsBody *apBody, bool abRemoveContainerFromBody)
	{
		for(int i=0; i<2; ++i)
		{
			if(mS.attached[i].mpBody == apBody)
			{
				mS.attached[i].mpBody = NULL;
			}
		}
		if(abRemoveContainerFromBody)
			apBody->RemoveAttachedVerletContainer(this);
	}
	
	//-----------------------------------------------------------------------

	void iPhysicsRope::SetAttachedStartBody(iPhysicsBody *apBody)
	{
		SetAttachedBody(0, GetStartParticle(), apBody);

		//TODO: Need to change inverse mass of start particle, if body mass > 0?
	}
	
	void iPhysicsRope::SetAttachedEndBody(iPhysicsBody *apBody)
	{
		SetAttachedBody(1, GetEndParticle(), apBody);
	}
	
	//-----------------------------------------------------------------------

	void iPhysicsRope::SetTotalLength(float afX)
	{
		if(mS.totalLength == afX) return;

		mS.totalLength = afX;

		BuildRopeParticles();
		RefreshParticleCache();
		SetSleeping(false);
	}

	//-----------------------------------------------------------------------

	void iPhysicsRope::SetMinTotalLength(float afX)
	{
		mS.minTotalLength = afX;
	}

	void iPhysicsRope::SetMaxTotalLength(float afX)
	{
		mS.maxTotalLength = afX;
	}

	//-----------------------------------------------------------------------

	void iPhysicsRope::SetSegmentLength(float afX)
	{
		if(afX<0 || mS.segmentLength == afX) return;

        mS.segmentLength = afX;

		BuildRopeParticles();
		RefreshParticleCache();
		SetSleeping(false);
	}
	
	//-----------------------------------------------------------------------

	void iPhysicsRope::RenderDebug(iLowLevelGraphics *apLowLevel)
	{
		cVector3f vPrevPos =0;
		bool bFirst=true;
		int lCount =0;
		for(tVerletParticleListIt it = mlstParticles.begin(); it != mlstParticles.end(); ++it, ++lCount)
		{
			cVerletParticle *pPart = *it;

			apLowLevel->DrawSphere(pPart->GetPosition(), mS.particleRadius,cColor(1,0,0,1));

			if(bFirst==false)
			{
				apLowLevel->DrawLine(vPrevPos,pPart->GetPosition(),cColor(1,1));
			}

			bFirst=false;
			vPrevPos = pPart->GetPosition();
		}
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void iPhysicsRope::UpdateBeforeSimulate(float afTimeStep)
	{
		OhplRopeVt vt = MakeVt();
		ohpl_rope_update_before(&vt, &mS, afTimeStep);
	}

	//-----------------------------------------------------------------------

	void iPhysicsRope::UpdateAfterSimulate(float afTimeStep)
	{
		OhplRopeVt vt = MakeVt();
		ohpl_rope_update_after(&vt, &mS);
	}

	//-----------------------------------------------------------------------

	void iPhysicsRope::UpdateConstraints()
	{
		OhplRopeVt vt = MakeVt();
		ohpl_rope_update_constraints(&vt, &mS);
	}

	//-----------------------------------------------------------------------

	void iPhysicsRope::UpdateMotorSound(float fVel)
	{
		//////////////////
		// Update sound
		if(mpMotorSoundEntity==NULL && fVel==0) return;

		cWorld *pNormalWorld = mpWorld->GetWorld();
		cSoundHandler *pSoundHandler = pNormalWorld->GetSound()->GetSoundHandler();
		if(pSoundHandler->GetSilent()) return;

		if(mpMotorSoundEntity && pNormalWorld->SoundEntityExists(mpMotorSoundEntity, mlMotorSoundEntityID)==false)
		{
			mpMotorSoundEntity = NULL;	
		}
		
		///////////////////////
		//Sound is playing. Check if speed is under the stop speed
		if(mpMotorSoundEntity && cMath::Abs(fVel) < mfMotorSoundStopSpeed)
		{
			mpMotorSoundEntity->FadeOut(10.0f);
			mpMotorSoundEntity = NULL;
		}
		///////////////////////
		//Sound is NOT playing. See if one should be started.
		else if(msMotorSound != "" && mpMotorSoundEntity==NULL && cMath::Abs(fVel) > mfMotorSoundStartSpeed)
		{
			mpMotorSoundEntity = pNormalWorld->CreateSoundEntity(msName+"_MotorSound",msMotorSound,true);
			if(mpMotorSoundEntity)
			{
				mlMotorSoundEntityID = mpMotorSoundEntity->GetCreationID();

				mpMotorSoundEntity->SetIsSaved(false);
				mpMotorSoundEntity->SetPosition(GetStartParticle()->GetPosition());
				mpMotorSoundEntity->FadeIn(5.0f);
			}
		}
	}

	//-----------------------------------------------------------------------

	OhplRopeVt iPhysicsRope::MakeVt()
	{
		OhplRopeVt vt = {
			this,
			[](void *c, size_t *apNum) -> cVerletParticle* const* { iPhysicsRope *p = static_cast<iPhysicsRope*>(c); *apNum = p->mvParticleCache.size(); return p->mvParticleCache.data(); },
			[](void *c) { return static_cast<iPhysicsRope*>(c)->CheckSpecificDataSleeping(); },
			[](void *c) { return static_cast<iPhysicsRope*>(c)->CheckSpecificDataAwake(); },
			[](void *c, bool abX) { static_cast<iPhysicsRope*>(c)->SetSleeping(abX); },
			[](void *c, float afX) { static_cast<iPhysicsRope*>(c)->SetTotalLength(afX); },
			[](void *c, float afVel) { static_cast<iPhysicsRope*>(c)->UpdateMotorSound(afVel); },
			[](void *c, OhplV3 aStart, OhplV3 aEnd, OhplV3 *apPos, OhplV3 *apNormal)
			{
				return static_cast<iPhysicsRope*>(c)->CastParticleRay(cVector3f(aStart.x, aStart.y, aStart.z), cVector3f(aEnd.x, aEnd.y, aEnd.z),
					reinterpret_cast<cVector3f*>(apPos), reinterpret_cast<cVector3f*>(apNormal));
			},
			[](size_t b, cMatrixf *apOut) { *apOut = reinterpret_cast<iPhysicsBody*>(b)->GetLocalMatrix(); },
			[](size_t b) { cVector3f v = reinterpret_cast<iPhysicsBody*>(b)->GetLocalPosition(); return OhplV3{v.x, v.y, v.z}; },
			[](size_t b) { cVector3f v = reinterpret_cast<iPhysicsBody*>(b)->GetMassCentre(); return OhplV3{v.x, v.y, v.z}; },
			[](size_t b, cMatrixf *apOut) { *apOut = reinterpret_cast<iPhysicsBody*>(b)->GetInertiaMatrix(); },
			[](size_t b) { return reinterpret_cast<iPhysicsBody*>(b)->GetMass(); },
			[](size_t b, OhplV3 v) { reinterpret_cast<iPhysicsBody*>(b)->AddForce(cVector3f(v.x, v.y, v.z)); },
			[](size_t b, OhplV3 v) { reinterpret_cast<iPhysicsBody*>(b)->AddTorque(cVector3f(v.x, v.y, v.z)); },
			[](void *c, int32_t alIdx, float afP, float afD, OhplV3 aError, float afTimeStep)
			{
				cPidControllerVec3 &pid = static_cast<iPhysicsRope*>(c)->mForcePid[alIdx];
				pid.p = afP;
				pid.i = 0;
				pid.d = afD;
				cVector3f vForce = pid.Output(cVector3f(aError.x, aError.y, aError.z), afTimeStep);
				return OhplV3{vForce.x, vForce.y, vForce.z};
			},
		};
		return vt;
	}

	//-----------------------------------------------------------------------

	bool iPhysicsRope::CheckParticleBodyCollision(iPhysicsBody *apBody)
	{
		if(mS.collideAttachments) return true;

        bool bAttachment=false;
		for(int i=0; i<2; ++i)
		{
			if(mS.attached[i].mpBody == apBody) return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------

	bool iPhysicsRope::CheckSpecificDataSleeping()
	{
		if(mS.motorActive) return false;

		for(int i=0; i<2; ++i)
		{
			iPhysicsBody *pBody = mS.attached[i].mpBody;
			if(pBody)
			{
				if(pBody->GetAngularVelocity().SqrLength() > 0.001f*0.001f) return false;
				if(pBody->GetLinearVelocity().SqrLength() > 0.001f*0.001f) return false;
			}
		}

		return true;
	}

	bool iPhysicsRope::CheckSpecificDataAwake()
	{
		if(mS.motorActive) return true;

		for(int i=0; i<2; ++i)
		{
			iPhysicsBody *pBody = mS.attached[i].mpBody;
			if(pBody)
			{
				if(pBody->GetEnabled()) return true;
			}
		}
		return false;
	}

	void iPhysicsRope::SetSpecificDataSleeping(bool abSleeping)
	{
		for(int i=0; i<2; ++i)
		{
			iPhysicsBody *pBody = mS.attached[i].mpBody;
			if(pBody)
			{
				if(abSleeping==false) pBody->Enable();
			}
		}
	}

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------
	
	void iPhysicsRope::BuildRopeParticles()
	{
		////////////////////////
		//If not updated, clear data
		if(mS.hasUpdated==false)
		{
			int lRemoveCount = (int)mlstParticles.size() -2;
			tVerletParticleListIt it = mlstParticles.begin(); it++;
			for(; lRemoveCount>0; --lRemoveCount)
			{
				cVerletParticle* pPart = *it;
				it = mlstParticles.erase(it);
				hplDelete(pPart);
			}
		}

		////////////////////////
		//Set up
		int lParticleNum = (int)mlstParticles.size() - 2;

		////////////////////////
		//Calculate number of particles needed and update vars
		int lWantedNum = 0;
		if(OhplBackendRustRequested())
		{
			OhplH14RopePlan plan;
			ohpl_h14_rope_plan(mS.totalLength, mS.segmentLength, &plan);
			lWantedNum = plan.wanted;
			mS.firstSegmentLength = plan.firstSegment;
		}
		else if(mS.segmentLength > 0)
		{
			lWantedNum = (int)((mS.totalLength / mS.segmentLength)+0.999999f) - 1;

			mS.firstSegmentLength = cMath::Modulus(mS.totalLength, mS.segmentLength);
			if(mS.firstSegmentLength == 0) mS.firstSegmentLength = mS.segmentLength;
		}
		else
		{
			mS.firstSegmentLength = mS.totalLength;
		}
		
		//Log("Wanted num: %d | total len: %f | first set len: %f | seg len: %f \n", lWantedNum, mS.totalLength, mS.firstSegmentLength, mS.segmentLength);

		if(lWantedNum == lParticleNum) return;

		////////////////////////
		//Remove particles if needed
		if(lWantedNum < lParticleNum)
		{
			tVerletParticleListIt it = mlstParticles.begin();
			it++;
			int lRemoveCount= lParticleNum - lWantedNum;
			for(; lRemoveCount>0; --lRemoveCount)
			{
				cVerletParticle* pPart = *it;
				it = mlstParticles.erase(it);
				hplDelete(pPart);
			}
		}
		////////////////////////
		//Add particles if needed
		else if(lWantedNum > lParticleNum)
		{
			cVector3f vDir = GetStartDirection();
			int lAddCount= lWantedNum - lParticleNum;

			cVector3f vStartPos = GetStartParticle()->GetPosition();

			float fLength = mS.firstSegmentLength + mS.segmentLength*((float)lAddCount-1);

			tVerletParticleListIt it = mlstParticles.begin(); 
			it++;

			for(; lAddCount>0; --lAddCount)
			{
				cVector3f vPos;
				if(OhplBackendRustRequested())
				{
					OhplV3 r = ohpl_h14_rope_particle_pos(OhplV3{vStartPos.x, vStartPos.y, vStartPos.z}, OhplV3{vDir.x, vDir.y, vDir.z}, fLength);
					vPos = cVector3f(r.x, r.y, r.z);
				}
				else
					vPos = vStartPos + vDir*fLength;
				//Log("Adding %d pos: %s length: %f\n", lAddCount, vPos.ToString().c_str(), fLength);
				fLength -= mS.segmentLength;

				it = mlstParticles.insert(it, hplNew(cVerletParticle, (this,vPos, 1)) );
			}
		}
	}

	//-----------------------------------------------------------------------

	void iPhysicsRope::SetAttachedBody(int alIdx, cVerletParticle *apParticle, iPhysicsBody *apBody)
	{
		mForcePid[alIdx].Reset();
		
		if(mS.attached[alIdx].mpBody)
		{
			mS.attached[alIdx].mpBody->RemoveAttachedVerletContainer(this);
		}
		
		apBody->AddAttachedVerletContainer(this);

		mS.attached[alIdx].mpBody = apBody;
		mS.attached[alIdx].mpParticle = apParticle;

		cMatrixf mtxInvBody = cMath::MatrixInverse(apBody->GetLocalMatrix());
		mS.attached[alIdx].mvBodyLocalPos = cMath::MatrixMul(mtxInvBody, apParticle->GetPosition());
	}
	
	//-----------------------------------------------------------------------

	cVector3f iPhysicsRope::GetStartDirection()
	{
		tVerletParticleListIt it = mlstParticles.begin();
		cVerletParticle *pP1 = *it;
		it++;
		cVerletParticle *pP2 = *it;

		return cMath::Vector3Normalize(pP2->GetPosition() - pP1->GetPosition());
	}

	//-----------------------------------------------------------------------
}
