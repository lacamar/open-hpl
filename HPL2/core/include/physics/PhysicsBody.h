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

#ifndef HPL_PHYSICS_BODY_H
#define HPL_PHYSICS_BODY_H

#include "scene/Entity3D.h"
#include "graphics/GraphicsTypes.h"
#include "physics/PhysicsTypes.h"
#include "physics/RustPhysBook.h"

namespace hpl {

	class iPhysicsWorld;
	class iCollideShape;
	class iPhysicsMaterial;
	class iLowLevelGraphics;
	class cNode3D;
	class cSoundEntity;
	class iPhysicsJoint;
	class cPhysicsContactData;
	class iCharacterBody;
	class iVerletParticleContainer;

	//------------------------------------------

	class iPhysicsBody;
	class iPhysicsWorld;
	class iCollideShape;
	class iPhysicsMaterial;
	class iCharacterBody;
	class cSoundEntity;

	// Scalar and pointer state of iPhysicsBody, owned by rust/src/physics/shell.rs; the members are references into it.
	struct OhplBodyState
	{
		uint64_t shell;
		iPhysicsWorld *world;
		iCollideShape *shape;
		iPhysicsMaterial *material;
		iCharacterBody *charBody;
		iPhysicsBody *scrapeBody;
		cSoundEntity *scrapeSound;
		cSoundEntity *rollSound;
		void *userData;
		cMatrixf prevScrape;
		int32_t scrapeSoundId, rollSoundId, impactCount;
		tFlag collideFlags;
		float buoyancyDensityMul, massMulApplied;
		int32_t pushStrength, freezeCount;
		eVelocityAxes gravityAxes;
		bool inUpdateList, pushedByCharGravity, blocksSound, blocksLight, isCharacter, collideCharacter, isRagDoll, collideRagDoll,
			isVolatile, useSurfaceEffects, gravityCanAttach, gravityAttachRotation, gravityAttachVelocity, collide, destroying;
	};
	static_assert(sizeof(OhplBodyState) == 192, "body state layout");

	class iPhysicsBodyCallback
	{
	public:
		virtual bool OnAABBCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody)=0;
		virtual void OnBodyCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody,cPhysicsContactData* apContactData)=0;
	};

	typedef std::list<iPhysicsBodyCallback*> tPhysicsBodyCallbackList;
	typedef tPhysicsBodyCallbackList::iterator tPhysicsBodyCallbackListIt;

	//------------------------------------------

	struct cPhysicsBody_Buoyancy
	{
		cPhysicsBody_Buoyancy() : mbActive(false), mfDensity(1), 
								mfLinearViscosity(1), mfAngularViscosity(1){}

		bool mbActive;

		float mfDensity;
		float mfLinearViscosity;
		float mfAngularViscosity;

		cPlanef mSurface;

		cVector3f mvPrevLinearVel = 0;
		cVector3f mvPrevAngularVel = 0;
		float mfDragDensityMul = 1;
		bool mbNoPrevVel = true;
	};

	//------------------------------------------

	class iPhysicsBody : public iEntity3D
	{
	friend struct cRustPhysicsAccess;
	#ifdef __GNUC__
		typedef iEntity3D __super;
	#endif
	public:
		iPhysicsBody(const tString &asName, iPhysicsWorld *apWorld,iCollideShape *apShape);
		virtual ~iPhysicsBody();

		void Destroy();

		virtual void SetMaterial(iPhysicsMaterial* apMaterial)=0;
		iPhysicsMaterial* GetMaterial();

		iCollideShape* GetShape();

		void AddJoint(iPhysicsJoint *apJoint);
		iPhysicsJoint* GetJoint(int alIndex);
		int GetJointNum();
		void RemoveJoint(iPhysicsJoint *apJoint);

		virtual void SetLinearVelocity(const cVector3f &avVel)=0;
		virtual cVector3f GetLinearVelocity() const=0;
		virtual void SetAngularVelocity(const cVector3f &avVel)=0;
		virtual cVector3f GetAngularVelocity() const=0;
		virtual void SetLinearDamping(float afDamping)=0;
		virtual float GetLinearDamping() const=0;
		virtual void SetAngularDamping(float afDamping)=0;
		virtual float GetAngularDamping() const=0;
		virtual void SetMaxLinearSpeed(float afSpeed)=0;
		virtual float GetMaxLinearSpeed() const=0;
		virtual void SetMaxAngularSpeed(float afDamping)=0;
		virtual float GetMaxAngularSpeed() const=0;
		virtual cVector3f GetInertiaVector() =0;
		virtual cMatrixf GetInertiaMatrix() =0;

		cVector3f GetVelocityAtPosition(const cVector3f& avPos);
		cVector3f GetTorqueFromForceAtPosition(const cVector3f& avForce, const cVector3f& avPos);

		virtual void  SetMass(float afMass)=0;
		virtual float GetMass() const=0;
		virtual void  SetMassCentre(const cVector3f& avCentre)=0;
		virtual cVector3f GetMassCentre() const=0;

		virtual void AddForce(const cVector3f &avForce)=0;
		virtual void AddForceAtPosition(const cVector3f &avForce, const cVector3f &avPos)=0;
		virtual void AddTorque(const cVector3f &avTorque)=0;
		virtual void AddImpulse(const cVector3f &avImpulse)=0;
		virtual void AddImpulseAtPosition(const cVector3f &avImpulse, const cVector3f &avPos)=0;

		virtual void Enable()=0;
		virtual void Sleep()=0;
		void Freeze(){ mlFreezeCount = 3;}
		static void SetHpl3Buoyancy(bool abX){ mbHpl3Buoyancy = abX;}
		bool GetFrozen(){ return mlFreezeCount > 0;}
		void SetNoGravityWhenUnderwater(bool abX){ mbNoGravityWhenUnderwater = abX;}
		bool GetNoGravityWhenUnderwater(){ return mbNoGravityWhenUnderwater;}
		void SetIsUnderwater(bool abX){ mbIsUnderwater = abX;}
		bool GetIsUnderwater(){ return mbIsUnderwater;}
		virtual bool GetEnabled() const=0;
		virtual void SetAutoDisable(bool abEnabled)=0;
		virtual bool GetAutoDisable() const=0;
		virtual void SetAutoDisableLinearThreshold(float afThresold)=0;
		virtual float GetAutoDisableLinearThreshold() const=0;
		virtual void SetAutoDisableAngularThreshold(float afThresold)=0;
		virtual float GetAutoDisableAngularThreshold() const=0;
		virtual void SetAutoDisableNumSteps(int alNum)=0;
		virtual int GetAutoDisableNumSteps() const=0;
		virtual void SetContinuousCollision(bool abOn)=0;
		virtual bool GetContinuousCollision()=0;

		virtual void SetGravity(bool abEnabled)=0;
		virtual bool GetGravity() const=0;

		virtual void RenderDebugGeometry(iLowLevelGraphics *apLowLevel,const cColor &aColor)=0;

		bool UpdateBeforeSimulate(float afTimeStep);
		void UpdateAfterSimulate(float afTimeStep);

		void StaticLinearMove(const cVector3f& avVelocity);
		void StaticAngularMove(const cVector3f& avVelocity);

		void SetBlocksSound(bool abX){ mbBlocksSound = abX;}
		bool GetBlocksSound(){ return mbBlocksSound;}

		void SetBlocksLight(bool abX){ mbBlocksLight = abX;}
		bool GetBlocksLight(){ return mbBlocksLight;}

		/**
		 * Must be true with both bodies in a collision if any effect is to be played.
		 */
		void SetUseSurfaceEffects(bool abX){ mbUseSurfaceEffects = abX;}
		bool GetUseSurfaceEffects(){ return mbUseSurfaceEffects;}
		

		void SetScrapeSoundEntity(cSoundEntity *apEntity);
		cSoundEntity* GetScrapeSoundEntity(){ return mpScrapeSoundEntity;}
		void SetScrapeSoundEntityID(int alID){ mlScrapeSoundEntityID = alID;}
		int GetScrapeSoundEntityID(){ return mlScrapeSoundEntityID;}
		void SetScrapeBody(iPhysicsBody *apBody){ mpScrapeBody =apBody;}
		iPhysicsBody * GetScrapeBody(){ return mpScrapeBody;}
		const cMatrixf& GetPreveScrapeMatrix(){ return m_mtxPrevScrapeMatrix;}
		void SetPreveScrapeMatrix(const cMatrixf& a_mtxMtx){ m_mtxPrevScrapeMatrix = a_mtxMtx;}

		void SetRollSoundEntity(cSoundEntity *apEntity);
		cSoundEntity* GetRollSoundEntity(){ return mpRollSoundEntity;}
		int GetRollSoundEntityID(){ return mlRollSoundEntityID;}

		void SetHasSlide(bool abX){ if(mRsBook) ohpl_h15_body_flag_set(mRsBook, OhplH15_HasSlide, abX); else mbHasSlide = abX;}
		bool HasSlide(){ return mRsBook ? ohpl_h15_body_flag_get(mRsBook, OhplH15_HasSlide) : mbHasSlide;}

		bool HasCollision(){ return mRsBook ? ohpl_h15_body_flag_get(mRsBook, OhplH15_HasCollision) : mbHasCollision;}

		void SetUserData(void* apUserData){ mpUserData = apUserData;}
		void* GetUserData(){ return mpUserData;}

		void AddBodyCallback(iPhysicsBodyCallback *apCallback);
		void RemoveBodyCallback(iPhysicsBodyCallback *apCallback);

		bool OnAABBCollision(iPhysicsBody *apBody);
		void OnCollide(iPhysicsBody *apBody,cPhysicsContactData* apContactData);

		void SetCollide(bool abX){mbCollide = abX;}
		bool GetCollide(){ return mbCollide;}

		void SetIsCharacter(bool abX){ mbIsCharacter = abX;}
		bool IsCharacter(){ return mbIsCharacter;}

		void SetCollideCharacter(bool abX){ mbCollideCharacter = abX;}
		bool GetCollideCharacter(){ return mbCollideCharacter;}

		void SetCharacterBody(iCharacterBody *apCharBody){ mpCharacterBody = apCharBody;}
		iCharacterBody* GetCharacterBody(){ return mpCharacterBody;}

		void SetPushStrength(int alX){ mlPushStrength = alX;}
		int GetPushStrength(){ return mlPushStrength;}

		void SetCollideFlags(tFlag alX) { mlCollideFlags = alX;}
		inline tFlag GetCollideFlags() const { return mlCollideFlags;}
		static bool (*mpCollideFlagsMatch)(tFlag alA, tFlag alB);
		static bool CollideFlagsMatch(tFlag alA, tFlag alB) { return mpCollideFlagsMatch ? mpCollideFlagsMatch(alA, alB) : (alA & alB) != 0; }
		
		void SetIsRagDoll(bool abX){ mbIsRagDoll = abX;}
		bool IsRagDoll(){ return mbIsRagDoll;}

		void SetCollideRagDoll(bool abX){ mbCollideRagDoll = abX;}
		bool GetCollideRagDoll(){ return mbCollideRagDoll;}

		void SetVolatile(bool abX){ mbVolatile = abX;}
		bool IsVolatile(){ return mbVolatile;}

		void SetPushedByCharacterGravity(bool abX){ mbPushedByCharacterGravity = abX;}
		bool GetPushedByCharacterGravity(){ return mbPushedByCharacterGravity;}

		void SetBuoyancyActive(bool abX)
		{
			if(abX != mBuoyancy.mbActive)
			{
				mBuoyancy.mvPrevLinearVel = mBuoyancy.mvPrevAngularVel = 0;
				mBuoyancy.mbNoPrevVel = true;
			}
			mBuoyancy.mbActive = abX;
		}
		void SetBuoyancyDensity(float afX){ mBuoyancy.mfDensity = afX;}
		void SetBuoyancyLinearViscosity(float afX){ mBuoyancy.mfLinearViscosity = afX;}
		void SetBuoyancyAngularViscosity(float afX){ mBuoyancy.mfAngularViscosity = afX;}
		void SetBuoyancySurface(const cPlanef &aP){ mBuoyancy.mSurface = aP;}

		bool GetBuoyancyActive(){ return mBuoyancy.mbActive;}
		float GetBuoyancyDensity(){ return mBuoyancy.mfDensity;}
		float GetBuoyancyLinearViscosity(){ return mBuoyancy.mfLinearViscosity;}
		float GetBuoyancyAngularViscosity(){ return mBuoyancy.mfAngularViscosity;}
		cPlanef SetBuoyancySurface(){ return mBuoyancy.mSurface;}

		float GetBuoyancyDensityMul(){ return mfBuoyancyDensityMul;}
		void SetBuoyancyDensityMul(float afX){ mfBuoyancyDensityMul = afX;}
		float GetMassMulApplied(){ return mfMassMulApplied;}
		void SetMassMulApplied(float afX){ mfMassMulApplied = afX;}

		void SetGravityCanAttachCharacter(bool abX){ mbGravityCanAttachCharacter = abX;}
		void SetGravityAttachmentRotation(bool abX){ mbGravityAttachmentRotation = abX;}
		void SetGravityAttachmentVelocity(bool abX){ mbGravityAttachmentVelocity = abX;}
		void SetGravityAttachmentVelocityAxes(eVelocityAxes aAxes){ mGravityAttachmentVelocityAxes = aAxes;}

		bool GetGravityCanAttachCharacter(){ return mbGravityCanAttachCharacter;}
		bool GetGravityAttachmentRotation(){ return mbGravityAttachmentRotation;}
		bool GetGravityAttachmentVelocity(){ return mbGravityAttachmentVelocity;}
		eVelocityAxes GetGravityAttachmentVelocityAxes(){ return mGravityAttachmentVelocityAxes;}
		
		void AddConnectedCharacter(iCharacterBody *apCharBody);
		void RemoveConnectedCharacter(iCharacterBody *apCharBody);

		bool IsInUpdateList(){ return mbInUpdateList;}
		void SetInUpdateList(bool abX){ mbInUpdateList = abX;}

		void AddAttachedVerletContainer(iVerletParticleContainer *apContainer);
		void RemoveAttachedVerletContainer(iVerletParticleContainer *apContainer);
		
		iPhysicsWorld *GetWorld(){ return mpWorld;}

		void DisableAfterSimulation(){ if(mRsBook) ohpl_h15_body_flag_set(mRsBook, OhplH15_DisableAfterSim, true); else mbDisableAfterSimulation = true;}

		//Entity implementation
		tString GetEntityType(){ return "Body";}

		virtual void DeleteLowLevel()=0;
	protected:
		OhplBodyState mS = OhplBodyState();
        iPhysicsWorld *&mpWorld = mS.world;
		iCollideShape *&mpShape = mS.shape;
		iPhysicsMaterial *&mpMaterial = mS.material;
		
		iCharacterBody *&mpCharacterBody = mS.charBody;

		std::vector<iPhysicsJoint*> mvJoints;

		bool &mbDestroying = mS.destroying;
        
		std::list<iCharacterBody*> mlstConnectedCharBodies;
		        		
		iPhysicsBody *&mpScrapeBody = mS.scrapeBody;
		cSoundEntity *&mpScrapeSoundEntity = mS.scrapeSound;
		int &mlScrapeSoundEntityID = mS.scrapeSoundId;
		cSoundEntity *&mpRollSoundEntity = mS.rollSound;
		int &mlRollSoundEntityID = mS.rollSoundId;
		cMatrixf &m_mtxPrevScrapeMatrix = mS.prevScrape;
		uint64_t mRsBook;
		static size_t HostOp(int32_t alOp, OhplH15Wa *a);
		bool mbHasSlide;
		int mlSlideCount;
		int &mlImpactCount = mS.impactCount;
		bool &mbInUpdateList = mS.inUpdateList;

		float &mfBuoyancyDensityMul = mS.buoyancyDensityMul;
		float &mfMassMulApplied = mS.massMulApplied;

		bool mbStaticMovement;
		cVector3f mvTotalStaticLinearVel;
		cVector3f mvTotalStaticAngularVel;

		tFlag &mlCollideFlags = mS.collideFlags;

		bool &mbPushedByCharacterGravity = mS.pushedByCharGravity;

		bool &mbBlocksSound = mS.blocksSound;
		bool &mbBlocksLight = mS.blocksLight;
		bool &mbIsCharacter = mS.isCharacter;
		bool &mbCollideCharacter = mS.collideCharacter;
		bool &mbIsRagDoll = mS.isRagDoll;
		bool &mbCollideRagDoll = mS.collideRagDoll;
		bool &mbVolatile = mS.isVolatile;
		bool &mbUseSurfaceEffects = mS.useSurfaceEffects;

		bool &mbGravityCanAttachCharacter = mS.gravityCanAttach;
		bool &mbGravityAttachmentRotation = mS.gravityAttachRotation;
		bool &mbGravityAttachmentVelocity = mS.gravityAttachVelocity;
		eVelocityAxes &mGravityAttachmentVelocityAxes = mS.gravityAxes;

		int &mlPushStrength = mS.pushStrength;

		cPhysicsBody_Buoyancy mBuoyancy;

		bool mbDisableAfterSimulation;
		int &mlFreezeCount = mS.freezeCount;
		static bool mbHpl3Buoyancy;
		bool mbNoGravityWhenUnderwater = false;
		bool mbIsUnderwater = false;

		bool mbHasCollision;

		tPhysicsBodyCallbackList mlstBodyCallbacks;

		void *&mpUserData = mS.userData;

		tVerletParticleContainerList mlstAttachedVerletContainers;

		bool &mbCollide = mS.collide;
	};
};
#endif // HPL_PHYSICS_BODY_H
