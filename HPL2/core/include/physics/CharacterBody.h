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

#ifndef HPL_CHARACTER_BODY_H
#define HPL_CHARACTER_BODY_H

#include "math/MathTypes.h"
#include "physics/PhysicsWorld.h"
#include "math/PidController.h"
#include "physics/RustCharacter.h"
#include "physics/RustPhysicsH14.h"

namespace hpl {

	class iPhysicsWorld;
	class iCollideShape;
	class iPhysicsBody;
	class cCamera;
	class iCharacterBody;
	class iEntity3D;

	class iCharacterBodyCallback
	{
	public:
		virtual void OnGravityCollide(iCharacterBody *apCharBody, iPhysicsBody *apBody, cCollideData *apCollideData)=0;
		virtual void OnHitGround(iCharacterBody *apCharBody,const cVector3f &avVel)=0;
	};

	enum eCharDir
	{
		eCharDir_Forward=0,
		eCharDir_Right=1,
		eCharDir_LastEnum=2
	};

	class cCharacterBodyCollideGravity : public iPhysicsWorldCollisionCallback
	{
	public:
		cCharacterBodyCollideGravity(iCharacterBody *apCharBody);
		virtual ~cCharacterBodyCollideGravity(){}

		void OnCollision(iPhysicsBody *apBody, cCollideData *apCollideData);

		iCharacterBody *mpCharBody;
	};

	class cCharacterBodyCollidePush : public iPhysicsWorldCollisionCallback
	{
	public:
		cCharacterBodyCollidePush(iCharacterBody *apCharBody);
		virtual ~cCharacterBodyCollidePush(){}

		void OnCollision(iPhysicsBody *apBody, cCollideData *apCollideData);

		iCharacterBody *mpCharBody;
	};

	class cCharacterBodyRay : public iPhysicsRayCallback
	{
	public:
		cCharacterBodyRay();
		virtual ~cCharacterBodyRay(){}

		void Clear();
		bool OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams);

		float mfMinDist;
		cVector3f mvNormal;
		bool mbCollide;
		tFlag mlCollideFlags;
	};

	class iPhysicsWorld;
	class iPhysicsMaterial;
	class cCamera;
	class iEntity3D;

	// Remaining scalar and pointer state of iCharacterBody, owned by rust/src/physics/char_conn.rs; the members are references into it.
	struct OhplCharExt
	{
		iPhysicsWorld *world;
		iCharacterBodyCallback *callback;
		void *userData;
		cCamera *camera;
		iEntity3D *entity;
		iPhysicsBody *connectedBody;
		iPhysicsBody *gravityAttachedBody;
		iPhysicsBody *gravityLastAttachedBody;
		iPhysicsMaterial *gravityCollideMaterial;
		cVector3f cameraPosAdd, connPosLocalToBody, connPosLocalToChar, connNormalLocalToBody, connLocalUpBody, connBodyStartRotation,
			connCharLocalForward, gravityAttachBodyVelocity;
		cMatrixf entityOffset, entityPostOffset, gravityAttachedPrevMatrix;
		float maxPushMass, pushForce, pushImpulse, charMaxPushMass, charPushForce, constantContactForceMul, velocityContactForceMul,
			maxContactForcePerMassUnit, entityPitchAmount, maxConnectionTorque, maxConnectionForce, connDistanceToBody;
		int32_t minBodyPushStrength, cameraSmoothPosNum, entitySmoothPosNum, entitySmoothYPosNum, gravityAttachmentContacts;
		bool collideCharacter, physicsBodyActive, stickToSlope, pushIn2D, charPushIn2D, cameraUseSmoothing, cameraUpdateActive, entitySmoothYPos,
			connectedBodyHadGravity, connDependOnBodyRotation, connAlignCenter, connAlignRotation, connCollision, connectedBodyIsActive,
			connAffectChar, connDependOnCharRotation, connAlignCharRotation, rotateYawWhenGravityAttached;
	};
	static_assert(sizeof(OhplCharExt) == 448, "char ext layout");

	class iCharacterBody
	{
	friend class cCharacterBodyCollideGravity;
	friend class cCharacterBodyCollidePush;
	public:
		iCharacterBody(const tString &asName, iPhysicsWorld *apWorld, const cVector3f avSize);
		virtual ~iCharacterBody();

		const tString& GetName(){ return msName;}

		//Actions
		void SetForce(const cVector3f &avForce) { mS->force = avForce; }
		void AddForce(const cVector3f &avForce) { mS->force += avForce; }
		cVector3f GetForce() { return mS->force; }

		void Move(eCharDir aDir, float afMul);
		void StopMovement();

		void ConnectToBody(	iPhysicsBody *apBody, const cVector3f& avConnectionPos, tPhysicsConnectionFlag alFlags);
		void DisconnectBody() { ohpl_h14_disconnect_body(HostOp, reinterpret_cast<size_t>(this), mE); }

		int AddExtraSize(const cVector3f& avSize);
		void SetActiveSize(int alNum);
		int GetActiveSize(){ return mS->currentShapeIdx; }

		void Update(float afTimeStep) { OhplCharVt vt = MakeVt(); ohpl_char_update(&vt, mS, afTimeStep, mbHpl3); }

		static void SetHpl3(bool abX){ mbHpl3 = abX; }

		//Helpers

		bool CheckCharacterFits(const cVector3f &avPosition, bool abFeetPosition=false, int alSizeIdx = -1, cVector3f *apPushBackVec=NULL, float afEpsilon = 0.0f);
		bool CheckRayIntersection(const cVector3f &avStart, const cVector3f &avEnd, float *apDistance, cVector3f *apNormalVec);

		// General Properties

		void SetActive(bool abX);
		bool IsActive(){ return mS->active;}

		// Physical Properties

		float GetMass() { return mS->mass; }
		void SetMass(float afMass);

		cVector3f GetSize() { return mS->size; }

		iCollideShape *GetCurrentShape() { return mpCurrentShape; }
		iCollideShape *GetShape(int alIdx) { return mvShapes[alIdx]; }
		int GetShapeNum(){ return (int)mvShapes.size();}
		iPhysicsBody* GetCurrentBody() { return mpCurrentBody; }

		void SetCollideCharacter(bool abX);
		bool GetCollideCharacter(){ return mE->collideCharacter;}

		void SetTestCollision(bool abX);
		void SetPhysicsBodyActive(bool abX) { mE->physicsBodyActive = abX; UpdateBodyActive(); }
		bool GetPhysicsBodyActive(){ return mE->physicsBodyActive;}
		bool GetTestCollision(){return mS->testCollision;}

		void SetMaxPositiveMoveSpeed(eCharDir aDir, float afX) { mS->maxPosMoveSpeed[aDir] = afX; }
		float GetMaxPositiveMoveSpeed(eCharDir aDir);
		void SetMaxNegativeMoveSpeed(eCharDir aDir, float afX) { mS->maxNegMoveSpeed[aDir] = afX; }
		float GetMaxNegativeMoveSpeed(eCharDir aDir);

		void SetMoveSpeed(eCharDir aDir, float afX) { mS->moveSpeed[aDir] = afX; }
		float GetMoveSpeed(eCharDir aDir);
		void SetMoveSpeedMul(eCharDir aDir, float afX){ mS->moveSpeedMul[aDir] = afX;}
		float GetMoveSpeedMul(eCharDir aDir){ return mS->moveSpeedMul[aDir];}
		void SetMoveAcc(eCharDir aDir, float afX) { mS->moveAcc[aDir] = afX; }
		float GetMoveAcc(eCharDir aDir);
		void SetMoveDeacc(eCharDir aDir, float afX) { mS->moveDeacc[aDir] = afX; }
		float GetMoveDeacc(eCharDir aDir);

		void SetMoveOppositeDirAccMul(eCharDir aDir, float afX){mS->moveOppositeDirAccMul[aDir] = afX;}
		float GetMoveOppositeDirAccMul(eCharDir aDir){ return mS->moveOppositeDirAccMul[aDir];}

		void SetDeaccelerateMoveSpeedInAir(bool abX){ mS->deaccelerateMoveSpeedInAir = abX;}
		bool GetDeaccelerateMoveSpeedInAir(){ return mS->deaccelerateMoveSpeedInAir;}

		cVector3f GetVelocity(float afFrameTime);

		void SetPosition(const cVector3f& avPos, bool abSmooth=false);
		void SetupSafePositions(float afSaveInterval, float afBufferTime) { ohpl_h14_setup_safe_positions(mpAux, afSaveInterval, afBufferTime); }
		void TeleportToSafePosition(float afElapsedTime, bool abSmooth=false);
		const cVector3f& GetPosition();
		const cVector3f& GetLastPosition() { return mS->lastPosition; }
		void SetFeetPosition(const cVector3f& avPos, bool abSmooth=false) { SetPosition(avPos + cVector3f(0,mS->size.y/2,0),abSmooth); }
		cVector3f GetFeetPosition() { return mS->position - cVector3f(0,mS->size.y/2,0); }

		void SetYaw(float afX);
		void AddYaw(float afX) { mS->yaw += afX; }
		float GetYaw() { return mS->yaw; }
		void SetPitch(float afX);
		void AddPitch(float afX) { mS->pitch += afX; }
		float GetPitch() { return mS->pitch; }

		const cVector3f& GetForward() { return mS->forward; }
		const cVector3f& GetRight();
		const cVector3f& GetUp() { return mS->up; }

		const cMatrixf& GetMoveMatrix(){ return mS->mtxMove;}

		void SetGravityActive(bool abX) { mS->gravityActive = abX; }
		bool GravityIsActive() { return mS->gravityActive; }
		void SetMaxGravitySpeed(float afX) { mS->maxGravitySpeed = afX; }
		float GetMaxGravitySpeed();

		bool GetCustomGravityActive() { return mS->customGravityActive; }
		void SetCustomGravityActive(bool abX) { mS->customGravityActive = abX; }
		void SetCustomGravity(const cVector3f& avCustomGravity) { mS->customGravity = avCustomGravity; }
		cVector3f GetCustomGravity() { return mS->customGravity; }

		void AddForceVelocity(const cVector3f& avVel){ mS->velocity += avVel;}
		void SetForceVelocity(const cVector3f& avVel){ mS->velocity = avVel;}
		cVector3f GetForceVelocity(){ return mS->velocity;}

		bool IsOnGround() { return mS->onGroundCount >0; }
		void SetMaxOnGroundCount(int alX) { mS->maxOnGroundCount = alX;}
		int GetMaxOnGroundCount() { return mS->maxOnGroundCount;}

		void SetCollideFlags(tFlag alX);
		inline tFlag GetCollideFlags() const { return mS->collideFlags;}

		void SetGroundFriction(float afX){ mS->groundFriction = afX;}
		float GetGroundFriction(){ return mS->groundFriction;}

		void SetAirFriction(float afX){ mS->airFriction = afX;}
		float GetAirFriction(){ return mS->airFriction;}

		void SetDisableDiagSpeedBoost(bool abX){ mS->disableDiagSpeedBoost = abX;}
		bool GetDisableDiagSpeedBoost(){ return mS->disableDiagSpeedBoost;}

		void SetMoveDelay(float afX){mS->moveDelayCount=afX;}
		float GetMoveDelay(){ return mS->moveDelayCount;}

		// Interaction Properties

		bool GetMovedLastUpdate(){ return mS->active && mS->movedLastUpdate;}

		void SetMaxPushMass(float afX){ mE->maxPushMass = afX;}
		void SetPushForce(float afX){ mE->pushForce = afX;}
		float GetMaxPushMass(){ return mE->maxPushMass;}
		float GetPushForce(){ return mE->pushForce;}
		void SetPushImpulse(float afX){ mE->pushImpulse = afX;}
		float GetPushImpulse(){ return mE->pushImpulse;}
		bool GetPushIn2D(){ return mE->pushIn2D;}
		void SetPushIn2D(bool abX){ mE->pushIn2D = abX;}

		void SetCharacterMaxPushMass(float afX){ mE->charMaxPushMass = afX;}
		void SetCharacterPushForce(float afX){ mE->charPushForce = afX;}
		float GetCharacterMaxPushMass(){ return mE->charMaxPushMass;}
		float GetCharacterPushForce(){ return mE->charPushForce;}
		bool GetCharacterPushIn2D(){ return mE->charPushIn2D;}
		void SetCharacterPushIn2D(bool abX){ mE->charPushIn2D = abX;}

		void SetConstantContactForceMul(float afX){ mE->constantContactForceMul = afX;}
		float GetConstantContactForceMul(){ return mE->constantContactForceMul;}
		void SetVelocityContactForceMul(float afX){ mE->velocityContactForceMul = afX;}
		float GetVelocityContactForceMul(){ return mE->velocityContactForceMul;}
		void SetMaxContactForcePerMassUnit(float afX){ mE->maxContactForcePerMassUnit = afX;}
		float GetMaxContactForcePerMassUnit(){ return mE->maxContactForcePerMassUnit;}

		void SetMaxNoSlideSlopeAngle(float afAngle) { mS->maxNoSlideSlopeCos = cos(afAngle); }
		float GetMaxNoSlideSlopeAngle() { return acos(mS->maxNoSlideSlopeCos); }
		void SetGroundAngleMin(float afAngle){ mS->groundAngleMinCos = cos(afAngle); }
		float GetGroundAngleMin(){ return acos(mS->groundAngleMinCos); }

		void SetStickToSlope(bool abX){ mE->stickToSlope = abX;}
		bool GetStickToSlope(){ return mE->stickToSlope;}

		float GetMaxStepSize(){ return mS->maxStepHeight;}
		void SetMaxStepSize(float afSize){ mS->maxStepHeight = afSize;}

		float GetMaxStepSizeInAir(){ return mS->maxStepHeightInAir;}
		void SetMaxStepSizeInAir(float afSize){ mS->maxStepHeightInAir = afSize;}

		void SetStepClimbSpeed(float afX){ mS->stepClimbSpeed = afX;}
		float GetStepClimbSpeed(){ return mS->stepClimbSpeed;}

		void SetAccurateClimbing(bool abX){ mS->accurateClimbing = abX;}
		bool GetAccurateClimbing(){ return mS->accurateClimbing;}

		void SetClimbForwardMul(float afX){ mS->climbForwardMul= afX;}
		float GetClimbForwardMul(){ return mS->climbForwardMul;}

		void SetClimbHeightAdd(float afX){ mS->climbHeightAdd = afX;}
		float GetClimbHeightAdd(){ return mS->climbHeightAdd;}

		bool IsClimbing(){ return mS->climbing;}
		void ResetClimbing() { mS->climbing = false; }

		const cVector3f& GetLastGroundNormal(){ return mS->lastGroundNormal;}

		void SetRotateYawWhenGravityAttached(bool abX){ mE->rotateYawWhenGravityAttached = abX;}
		bool GetRotateYawWhenGravityAttached(){ return mE->rotateYawWhenGravityAttached;}

		void SetGravityAttachedBody(iPhysicsBody* apBody){ mE->gravityAttachedBody = apBody;}
		iPhysicsBody* GetGravityAttachedBody(){ return mE->gravityAttachedBody;}

		iPhysicsMaterial * GetGravityCollideMaterial(){ return mE->gravityCollideMaterial;}

		// Connection Properties

		iPhysicsBody * GetConnectedBody() { return mE->connectedBody; }
		cVector3f GetBodyConnectionPos();
		cVector3f GetCharConnectionPos();

		const cVector3f& GetConnectionPosLocalToBody(){ return mE->connPosLocalToBody; }
		const cVector3f& GetConnectionPosLocalToChar(){ return mE->connPosLocalToChar; }
		void SetConnectionPosLocalToBody(const cVector3f& avPos){ mE->connPosLocalToBody = avPos; }
		void SetConnectionPosLocalToChar(const cVector3f& avPos){ mE->connPosLocalToChar = avPos; }

		const cVector3f& GetConnectionLocalUpBody(){ return mE->connLocalUpBody;}
		void SetConnectionLocalUpBody(const cVector3f& avVec){ mE->connLocalUpBody = avVec;}

		void SetMaxConnectionForce(float afX){ mE->maxConnectionForce = afX; }
		void SetMaxConnectionTorque(float afX){ mE->maxConnectionTorque = afX; }
		float GetMaxConnectionForce(){ return mE->maxConnectionForce; }
		float GetMaxConnectionTorque(){ return mE->maxConnectionTorque; }

		// User Properties

		void SetCamera(cCamera *apCam) { mE->camera = apCam; }
        cCamera* GetCamera() { return mE->camera; }
		void SetCameraPosAdd(const cVector3f &avAdd) { mE->cameraPosAdd = avAdd; }
		cVector3f GetCameraPosAdd();
		void SetCameraSmoothPosNum(int alNum){ mE->cameraSmoothPosNum = alNum;}
		int GetCameraSmoothPosNum(){ return mE->cameraSmoothPosNum;}
		void SetCameraUseSmoothing(bool abX){ mE->cameraUseSmoothing = abX; ohpl_h14_clear_camera(mpAux); UpdateCamera(); }
		bool GetCameraUseSmoothing(){ return mE->cameraUseSmoothing;}
		void SetCameraUpdateActive(bool abX){ mE->cameraUpdateActive = abX;}
		bool GetCameraUpdateActive(){ return mE->cameraUpdateActive;}

		void SetEntity(iEntity3D *apEntity) { mE->entity = apEntity; }
		iEntity3D* GetEntity();

		void SetEntityOffset(const cMatrixf &a_mtxOffset) { mE->entityOffset = a_mtxOffset; }
		const cMatrixf & GetEntityOffset();

		void SetEntityPostOffset(const cMatrixf &a_mtxOffset) { mE->entityPostOffset = a_mtxOffset; }
		const cMatrixf & GetEntityPostOffset();

		void SetEntitySmoothPosNum(int alNum){ mE->entitySmoothPosNum = alNum;}
		int GetEntitySmoothPosNum(){ return mE->entitySmoothPosNum;}

		void SetUseEntitySmoothYPos(bool abX){ mE->entitySmoothYPos = abX;}
		void SetEntitySmoothYPosNum(int alX){ mE->entitySmoothYPosNum = alX;}
		bool GetUseEntitySmoothYPos(){ return mE->entitySmoothYPos;}
		void SetEntityPitchAmount(float afX){ mE->entityPitchAmount = afX;}
		float GetEntityPitchAmount(){ return mE->entityPitchAmount;}
		int GetEntitySmoothYPosNum(){ return mE->entitySmoothYPosNum;}

		void SetUserData(void* apUserData){ mE->userData = apUserData;}
		void* GetUserData(){ return mE->userData;}

		void SetCallback(iCharacterBodyCallback *apCallback){ mE->callback = apCallback;}

		//Debug:
		const cVector3f& GetLastMovePosAdd(){ return mS->lastMovePosAdd;}

	protected:
		void UpdateMoveMatrix() { ohpl_char_update_move_matrix(mS); }

		void PreUpdateConnection(float afTimeStep);
		void PostUpdateConnection(float afTimeStep);
		void UpdateSafePositions(float afTimeStep);
		void UpdateBodyConnection(float afTimeStep);
		void UpdateCharacterConnection(float afTimeStep);

		static bool mbHpl3;
		static size_t HostOp(int32_t alOp, OhplH15Wa *a);

		void UpdateCamera();
		void UpdateBodyActive();
		void UpdateEntity();

		void UpdateBody();

		void ClearGravityAttachment() { mE->gravityLastAttachedBody = mE->gravityAttachedBody; mE->gravityAttachedBody = NULL; mE->gravityAttachmentContacts = 0; }
		void UpdateGravityAttachment();

		bool CheckCollision(cVector3f *apPushBackVector, const cVector3f& avPos, iPhysicsWorldCollisionCallback *apCallback,int alShapeIdx=-1);

		OhplCharState *mS;
		OhplCharExt *mE;
		OhplH14Aux *mpAux;
		OhplH14Conn MakeConn();
		OhplCharVt MakeVt();

		tString msName;

		cCharacterBodyCollideGravity *mpCollideGravityCallback;
		cCharacterBodyCollidePush *mpCollidePushCallback;
		cCharacterBodyRay *mpRayCallback;

		iCollideShape *mpCurrentShape;
		iPhysicsBody *mpCurrentBody;
		std::vector<iCollideShape*> mvShapes;
		std::vector<iPhysicsBody*> mvBodies;

	};
};
#endif // HPL_CHARACTER_BODY_H
