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
#include "impl/RustNewtonBackend.h"

class NewtonBody;
class NewtonWorld;

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

	class iPhysicsBody;
	class iPhysicsWorld;
	class iCollideShape;
	class iPhysicsMaterial;
	class iCharacterBody;
	class cSoundEntity;
	class cPhysicsBodyNewtonCallback;

	// Scalar and pointer state of iPhysicsBody, owned by rust/src/physics/shell.rs; the members are references into it.
	struct OhplBodyState
	{
		uint64_t shell, rust, book;
		NewtonBody *newtonBody;
		cPhysicsBodyNewtonCallback *callback;
		iPhysicsWorld *world;
		iCollideShape *shape;
		iPhysicsMaterial *material;
		iCharacterBody *charBody;
		iPhysicsBody *scrapeBody;
		cSoundEntity *scrapeSound;
		cSoundEntity *rollSound;
		void *userData;
		cMatrixf prevScrape;
		cVector3f totalStaticLinear, totalStaticAngular;
		int32_t slideCount, scrapeSoundId, rollSoundId, impactCount;
		tFlag collideFlags;
		float buoyancyDensityMul, massMulApplied;
		int32_t pushStrength, freezeCount;
		eVelocityAxes gravityAxes;
		bool inUpdateList, pushedByCharGravity, blocksSound, blocksLight, isCharacter, collideCharacter, isRagDoll, collideRagDoll,
			isVolatile, useSurfaceEffects, gravityCanAttach, gravityAttachRotation, gravityAttachVelocity, collide, destroying,
			noGravityUnderwater, isUnderwater;
	};
	static_assert(sizeof(OhplBodyState) == 256, "body state layout");

	class iPhysicsBodyCallback
	{
	public:
		virtual bool OnAABBCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody)=0;
		virtual void OnBodyCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody,cPhysicsContactData* apContactData)=0;
	};

	typedef std::list<iPhysicsBodyCallback*> tPhysicsBodyCallbackList;
	typedef tPhysicsBodyCallbackList::iterator tPhysicsBodyCallbackListIt;

	class cPhysicsBodyNewtonCallback : public iEntityCallback
	{
	public:
		virtual ~cPhysicsBodyNewtonCallback(){}
	private:
		void OnTransformUpdate(iEntity3D * apEntity);
	};

	class iPhysicsBody : public iEntity3D
	{
	friend struct cRustPhysicsAccess;
	#ifdef __GNUC__
		typedef iEntity3D __super;
	#endif
	friend class cPhysicsBodyNewtonCallback;
	friend struct OhplL4HostImpl;
	public:
		iPhysicsBody(const tString &asName, iPhysicsWorld *apWorld,iCollideShape *apShape);
		virtual ~iPhysicsBody();

		void Destroy();

		void SetMaterial(iPhysicsMaterial* apMaterial);
		iPhysicsMaterial* GetMaterial() { return mS->material; }

		iCollideShape* GetShape() { return mS->shape; }

		void AddJoint(iPhysicsJoint *apJoint);
		iPhysicsJoint* GetJoint(int alIndex);
		int GetJointNum();
		void RemoveJoint(iPhysicsJoint *apJoint);

		void SetLinearVelocity(const cVector3f &avVel);
		cVector3f GetLinearVelocity() const;
		void SetAngularVelocity(const cVector3f &avVel);
		cVector3f GetAngularVelocity() const;
		void SetLinearDamping(float afDamping);
		float GetLinearDamping() const;
		void SetAngularDamping(float afDamping);
		float GetAngularDamping() const;
		void SetMaxLinearSpeed(float afSpeed);
		float GetMaxLinearSpeed() const;
		void SetMaxAngularSpeed(float afDamping);
		float GetMaxAngularSpeed() const;
		cVector3f GetInertiaVector();
		cMatrixf GetInertiaMatrix();

		cVector3f GetVelocityAtPosition(const cVector3f& avPos);
		cVector3f GetTorqueFromForceAtPosition(const cVector3f& avForce, const cVector3f& avPos);

		void  SetMass(float afMass);
		float GetMass() const;
		void  SetMassCentre(const cVector3f& avCentre);
		cVector3f GetMassCentre() const;

		void AddForce(const cVector3f &avForce);
		void AddForceAtPosition(const cVector3f &avForce, const cVector3f &avPos);
		void AddTorque(const cVector3f &avTorque);
		void AddImpulse(const cVector3f &avImpulse);
		void AddImpulseAtPosition(const cVector3f &avImpulse, const cVector3f &avPos);

		void Enable();
		void Sleep();
		void Freeze(){ mS->freezeCount = 3;}
		static void SetHpl3Buoyancy(bool abX){ mbHpl3Buoyancy = abX;}
		bool GetFrozen(){ return mS->freezeCount > 0;}
		void SetNoGravityWhenUnderwater(bool abX){ mS->noGravityUnderwater = abX;}
		bool GetNoGravityWhenUnderwater(){ return mS->noGravityUnderwater;}
		void SetIsUnderwater(bool abX){ mS->isUnderwater = abX;}
		bool GetIsUnderwater(){ return mS->isUnderwater;}
		bool GetEnabled() const;
		void SetAutoDisable(bool abEnabled);
		bool GetAutoDisable() const;
		void SetAutoDisableLinearThreshold(float afThresold);
		float GetAutoDisableLinearThreshold() const;
		void SetAutoDisableAngularThreshold(float afThresold);
		float GetAutoDisableAngularThreshold() const;
		void SetAutoDisableNumSteps(int alNum);
		int GetAutoDisableNumSteps() const;
		void SetContinuousCollision(bool abOn);
		bool GetContinuousCollision();

		void SetGravity(bool abEnabled);
		bool GetGravity() const;

		void RenderDebugGeometry(iLowLevelGraphics *apLowLevel,const cColor &aColor);

		bool UpdateBeforeSimulate(float afTimeStep);
		void UpdateAfterSimulate(float afTimeStep);

		void StaticLinearMove(const cVector3f& avVelocity);
		void StaticAngularMove(const cVector3f& avVelocity);

		void SetBlocksSound(bool abX){ mS->blocksSound = abX;}
		bool GetBlocksSound(){ return mS->blocksSound;}

		void SetBlocksLight(bool abX){ mS->blocksLight = abX;}
		bool GetBlocksLight(){ return mS->blocksLight;}

		/**
		 * Must be true with both bodies in a collision if any effect is to be played.
		 */
		void SetUseSurfaceEffects(bool abX){ mS->useSurfaceEffects = abX;}
		bool GetUseSurfaceEffects(){ return mS->useSurfaceEffects;}

		void SetScrapeSoundEntity(cSoundEntity *apEntity);
		cSoundEntity* GetScrapeSoundEntity(){ return mS->scrapeSound;}
		void SetScrapeSoundEntityID(int alID){ mS->scrapeSoundId = alID;}
		int GetScrapeSoundEntityID(){ return mS->scrapeSoundId;}
		void SetScrapeBody(iPhysicsBody *apBody){ mS->scrapeBody =apBody;}
		iPhysicsBody * GetScrapeBody(){ return mS->scrapeBody;}
		const cMatrixf& GetPreveScrapeMatrix(){ return mS->prevScrape;}
		void SetPreveScrapeMatrix(const cMatrixf& a_mtxMtx){ mS->prevScrape = a_mtxMtx;}

		void SetRollSoundEntity(cSoundEntity *apEntity);
		cSoundEntity* GetRollSoundEntity(){ return mS->rollSound;}
		int GetRollSoundEntityID(){ return mS->rollSoundId;}

		void SetHasSlide(bool abX){ ohpl_h15_body_flag_set(mS->book, OhplH15_HasSlide, abX);}
		bool HasSlide(){ return ohpl_h15_body_flag_get(mS->book, OhplH15_HasSlide);}

		bool HasCollision(){ return ohpl_h15_body_flag_get(mS->book, OhplH15_HasCollision);}

		void SetUserData(void* apUserData){ mS->userData = apUserData;}
		void* GetUserData(){ return mS->userData;}

		void AddBodyCallback(iPhysicsBodyCallback *apCallback);
		void RemoveBodyCallback(iPhysicsBodyCallback *apCallback);

		bool OnAABBCollision(iPhysicsBody *apBody);
		void OnCollide(iPhysicsBody *apBody,cPhysicsContactData* apContactData);

		void SetCollide(bool abX){mS->collide = abX;}
		bool GetCollide(){ return mS->collide;}

		void SetIsCharacter(bool abX){ mS->isCharacter = abX;}
		bool IsCharacter(){ return mS->isCharacter;}

		void SetCollideCharacter(bool abX){ mS->collideCharacter = abX;}
		bool GetCollideCharacter(){ return mS->collideCharacter;}

		void SetCharacterBody(iCharacterBody *apCharBody){ mS->charBody = apCharBody;}
		iCharacterBody* GetCharacterBody(){ return mS->charBody;}

		void SetPushStrength(int alX){ mS->pushStrength = alX;}
		int GetPushStrength(){ return mS->pushStrength;}

		void SetCollideFlags(tFlag alX) { mS->collideFlags = alX;}
		inline tFlag GetCollideFlags() const { return mS->collideFlags;}
		static bool (*mpCollideFlagsMatch)(tFlag alA, tFlag alB);
		static bool CollideFlagsMatch(tFlag alA, tFlag alB) { return mpCollideFlagsMatch ? mpCollideFlagsMatch(alA, alB) : (alA & alB) != 0; }

		void SetIsRagDoll(bool abX){ mS->isRagDoll = abX;}
		bool IsRagDoll(){ return mS->isRagDoll;}

		void SetCollideRagDoll(bool abX){ mS->collideRagDoll = abX;}
		bool GetCollideRagDoll(){ return mS->collideRagDoll;}

		void SetVolatile(bool abX){ mS->isVolatile = abX;}
		bool IsVolatile(){ return mS->isVolatile;}

		void SetPushedByCharacterGravity(bool abX){ mS->pushedByCharGravity = abX;}
		bool GetPushedByCharacterGravity(){ return mS->pushedByCharGravity;}

		void SetBuoyancyActive(bool abX){ ohpl_ntb_set_buoy_active(mS->rust, abX);}
		void SetBuoyancyDensity(float afX){ ohpl_ntb_set_buoy_density(mS->rust, afX);}
		void SetBuoyancyLinearViscosity(float afX){ ohpl_ntb_set_buoy_linear_viscosity(mS->rust, afX);}
		void SetBuoyancyAngularViscosity(float afX){ ohpl_ntb_set_buoy_angular_viscosity(mS->rust, afX);}
		void SetBuoyancySurface(const cPlanef &aP){ ohpl_ntb_set_buoy_surface(mS->rust, &aP);}

		bool GetBuoyancyActive(){ return BuoyState().active;}
		float GetBuoyancyDensity(){ return BuoyState().density;}
		float GetBuoyancyLinearViscosity(){ return BuoyState().linearViscosity;}
		float GetBuoyancyAngularViscosity(){ return BuoyState().angularViscosity;}
		cPlanef SetBuoyancySurface(){ return BuoyState().surface;}

		float GetBuoyancyDensityMul(){ return mS->buoyancyDensityMul;}
		void SetBuoyancyDensityMul(float afX){ mS->buoyancyDensityMul = afX;}
		float GetMassMulApplied(){ return mS->massMulApplied;}
		void SetMassMulApplied(float afX){ mS->massMulApplied = afX;}

		void SetGravityCanAttachCharacter(bool abX){ mS->gravityCanAttach = abX;}
		void SetGravityAttachmentRotation(bool abX){ mS->gravityAttachRotation = abX;}
		void SetGravityAttachmentVelocity(bool abX){ mS->gravityAttachVelocity = abX;}
		void SetGravityAttachmentVelocityAxes(eVelocityAxes aAxes){ mS->gravityAxes = aAxes;}

		bool GetGravityCanAttachCharacter(){ return mS->gravityCanAttach;}
		bool GetGravityAttachmentRotation(){ return mS->gravityAttachRotation;}
		bool GetGravityAttachmentVelocity(){ return mS->gravityAttachVelocity;}
		eVelocityAxes GetGravityAttachmentVelocityAxes(){ return mS->gravityAxes;}

		void AddConnectedCharacter(iCharacterBody *apCharBody);
		void RemoveConnectedCharacter(iCharacterBody *apCharBody);

		bool IsInUpdateList(){ return mS->inUpdateList;}
		void SetInUpdateList(bool abX){ mS->inUpdateList = abX;}

		void AddAttachedVerletContainer(iVerletParticleContainer *apContainer);
		void RemoveAttachedVerletContainer(iVerletParticleContainer *apContainer);

		iPhysicsWorld *GetWorld(){ return mS->world;}

		void DisableAfterSimulation(){ ohpl_h15_body_flag_set(mS->book, OhplH15_DisableAfterSim, true);}

		//Entity implementation
		tString GetEntityType(){ return "Body";}

		NewtonBody *GetNewtonBody(){ return mS->newtonBody;}

		void DeleteLowLevel();

		static void SetUseCallback(bool abX){ mbUseCallback = abX;}
	protected:
		bool CalcHpl3Buoyancy(const cVector3f &avGravity, float afTimeStep, cVector3f *apForce, cVector3f *apTorque);
		OhplL4Buoy BuoyState() const { OhplL4Buoy b; ohpl_ntb_buoy(mS->rust, &b); return b;}

		static bool mbUseCallback;

		OhplBodyState *mS;
		static size_t HostOp(int32_t alOp, OhplH15Wa *a);
		static bool mbHpl3Buoyancy;
	};
};
#endif // HPL_PHYSICS_BODY_H
