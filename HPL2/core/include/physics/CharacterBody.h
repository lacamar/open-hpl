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

	//------------------------------------------------

	class iCharacterBodyCallback
	{
	public:
		virtual void OnGravityCollide(iCharacterBody *apCharBody, iPhysicsBody *apBody, cCollideData *apCollideData)=0;
		virtual void OnHitGround(iCharacterBody *apCharBody,const cVector3f &avVel)=0;
	};

	//------------------------------------------------

	enum eCharDir
	{
		eCharDir_Forward=0,
		eCharDir_Right=1,
		eCharDir_LastEnum=2
	};

	//------------------------------------------------

	class cCharacterBodyCollideGravity : public iPhysicsWorldCollisionCallback
	{
	public:
		cCharacterBodyCollideGravity(iCharacterBody *apCharBody);
		virtual ~cCharacterBodyCollideGravity(){}

		void OnCollision(iPhysicsBody *apBody, cCollideData *apCollideData);

		iCharacterBody *mpCharBody;
	};

	//------------------------------------------------

	class cCharacterBodyCollidePush : public iPhysicsWorldCollisionCallback
	{
	public:
		cCharacterBodyCollidePush(iCharacterBody *apCharBody);
		virtual ~cCharacterBodyCollidePush(){}

		void OnCollision(iPhysicsBody *apBody, cCollideData *apCollideData);

		iCharacterBody *mpCharBody;
	};

	//------------------------------------------------

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

	//------------------------------------------------


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
	public:
		iCharacterBody(const tString &asName, iPhysicsWorld *apWorld, const cVector3f avSize);
		virtual ~iCharacterBody();

		const tString& GetName(){ return msName;}

		///////////////////////////////////////
		//Actions
		void SetForce(const cVector3f &avForce);
		void AddForce(const cVector3f &avForce);
		cVector3f GetForce();

		void Move(eCharDir aDir, float afMul);
		void StopMovement();

		void ConnectToBody(	iPhysicsBody *apBody, const cVector3f& avConnectionPos, tPhysicsConnectionFlag alFlags);
		void DisconnectBody();

		int AddExtraSize(const cVector3f& avSize);
		void SetActiveSize(int alNum);
		int GetActiveSize(){ return mS.currentShapeIdx; }

		void Update(float afTimeStep);

		static void SetHpl3(bool abX){ mbHpl3 = abX; }

		///////////////////////////////////////
		//Helpers

		bool CheckCharacterFits(const cVector3f &avPosition, bool abFeetPosition=false, int alSizeIdx = -1, cVector3f *apPushBackVec=NULL, float afEpsilon = 0.0f);
		bool CheckRayIntersection(const cVector3f &avStart, const cVector3f &avEnd, float *apDistance, cVector3f *apNormalVec);

		///////////////////////////////////////
		// General Properties

		void SetActive(bool abX);
		bool IsActive(){ return mS.active;}


		///////////////////////////////////////
		// Physical Properties

		float GetMass();
		void SetMass(float afMass);

		cVector3f GetSize();

		iCollideShape *GetCurrentShape();
		iCollideShape *GetShape(int alIdx);
		int GetShapeNum(){ return (int)mvShapes.size();}
		iPhysicsBody* GetCurrentBody();

		void SetCollideCharacter(bool abX);
		bool GetCollideCharacter(){ return mbCollideCharacter;}

		void SetTestCollision(bool abX);
		void SetPhysicsBodyActive(bool abX);
		bool GetPhysicsBodyActive(){ return mbPhysicsBodyActive;}
		bool GetTestCollision(){return mS.testCollision;}

		void SetMaxPositiveMoveSpeed(eCharDir aDir, float afX);
		float GetMaxPositiveMoveSpeed(eCharDir aDir);
		void SetMaxNegativeMoveSpeed(eCharDir aDir, float afX);
		float GetMaxNegativeMoveSpeed(eCharDir aDir);

		void SetMoveSpeed(eCharDir aDir, float afX);
		float GetMoveSpeed(eCharDir aDir);
		void SetMoveSpeedMul(eCharDir aDir, float afX){ mS.moveSpeedMul[aDir] = afX;}
		float GetMoveSpeedMul(eCharDir aDir){ return mS.moveSpeedMul[aDir];}
		void SetMoveAcc(eCharDir aDir, float afX);
		float GetMoveAcc(eCharDir aDir);
		void SetMoveDeacc(eCharDir aDir, float afX);
		float GetMoveDeacc(eCharDir aDir);

		void SetMoveOppositeDirAccMul(eCharDir aDir, float afX){mS.moveOppositeDirAccMul[aDir] = afX;}
		float GetMoveOppositeDirAccMul(eCharDir aDir){ return mS.moveOppositeDirAccMul[aDir];}


		void SetDeaccelerateMoveSpeedInAir(bool abX){ mS.deaccelerateMoveSpeedInAir = abX;}
		bool GetDeaccelerateMoveSpeedInAir(){ return mS.deaccelerateMoveSpeedInAir;}

		cVector3f GetVelocity(float afFrameTime);

		void SetPosition(const cVector3f& avPos, bool abSmooth=false);
		void SetupSafePositions(float afSaveInterval, float afBufferTime);
		void TeleportToSafePosition(float afElapsedTime, bool abSmooth=false);
		const cVector3f& GetPosition();
		const cVector3f& GetLastPosition();
		void SetFeetPosition(const cVector3f& avPos, bool abSmooth=false);
		cVector3f GetFeetPosition();

		void SetYaw(float afX);
		void AddYaw(float afX);
		float GetYaw();
		void SetPitch(float afX);
		void AddPitch(float afX);
		float GetPitch();

		const cVector3f& GetForward();
		const cVector3f& GetRight();
		const cVector3f& GetUp();

		const cMatrixf& GetMoveMatrix(){ return mS.mtxMove;}

		void SetGravityActive(bool abX);
		bool GravityIsActive();
		void SetMaxGravitySpeed(float afX);
		float GetMaxGravitySpeed();

		bool GetCustomGravityActive();
		void SetCustomGravityActive(bool abX);
		void SetCustomGravity(const cVector3f& avCustomGravity);
		cVector3f GetCustomGravity();

		void AddForceVelocity(const cVector3f& avVel){ mS.velocity += avVel;}
		void SetForceVelocity(const cVector3f& avVel){ mS.velocity = avVel;}
		cVector3f GetForceVelocity(){ return mS.velocity;}

		bool IsOnGround();
		void SetMaxOnGroundCount(int alX) { mS.maxOnGroundCount = alX;}
		int GetMaxOnGroundCount() { return mS.maxOnGroundCount;}

		void SetCollideFlags(tFlag alX);
		inline tFlag GetCollideFlags() const { return mS.collideFlags;}

		void SetGroundFriction(float afX){ mS.groundFriction = afX;}
		float GetGroundFriction(){ return mS.groundFriction;}

		void SetAirFriction(float afX){ mS.airFriction = afX;}
		float GetAirFriction(){ return mS.airFriction;}

		void SetDisableDiagSpeedBoost(bool abX){ mS.disableDiagSpeedBoost = abX;}
		bool GetDisableDiagSpeedBoost(){ return mS.disableDiagSpeedBoost;}

		void SetMoveDelay(float afX){mS.moveDelayCount=afX;}
		float GetMoveDelay(){ return mS.moveDelayCount;}

		///////////////////////////////////////
		// Interaction Properties

		bool GetMovedLastUpdate(){ return mS.active && mS.movedLastUpdate;}

		void SetMaxPushMass(float afX){ mfMaxPushMass = afX;}
		void SetPushForce(float afX){ mfPushForce = afX;}
		float GetMaxPushMass(){ return mfMaxPushMass;}
		float GetPushForce(){ return mfPushForce;}
		void SetPushImpulse(float afX){ mfPushImpulse = afX;}
		float GetPushImpulse(){ return mfPushImpulse;}
		bool GetPushIn2D(){ return mbPushIn2D;}
		void SetPushIn2D(bool abX){ mbPushIn2D = abX;}

		void SetCharacterMaxPushMass(float afX){ mfCharacterMaxPushMass = afX;}
		void SetCharacterPushForce(float afX){ mfCharacterPushForce = afX;}
		float GetCharacterMaxPushMass(){ return mfCharacterMaxPushMass;}
		float GetCharacterPushForce(){ return mfCharacterPushForce;}
		bool GetCharacterPushIn2D(){ return mbCharacterPushIn2D;}
		void SetCharacterPushIn2D(bool abX){ mbCharacterPushIn2D = abX;}

		void SetConstantContactForceMul(float afX){ mfConstantContactForceMul = afX;}
		float GetConstantContactForceMul(){ return mfConstantContactForceMul;}
		void SetVelocityContactForceMul(float afX){ mfVelocityContactForceMul = afX;}
		float GetVelocityContactForceMul(){ return mfVelocityContactForceMul;}
		void SetMaxContactForcePerMassUnit(float afX){ mfMaxContactForcePerMassUnit = afX;}
		float GetMaxContactForcePerMassUnit(){ return mfMaxContactForcePerMassUnit;}

		void SetMaxNoSlideSlopeAngle(float afAngle);
		float GetMaxNoSlideSlopeAngle();
		void SetGroundAngleMin(float afAngle){ mS.groundAngleMinCos = cos(afAngle); }
		float GetGroundAngleMin(){ return acos(mS.groundAngleMinCos); }

		void SetStickToSlope(bool abX){ mbStickToSlope = abX;}
		bool GetStickToSlope(){ return mbStickToSlope;}

		float GetMaxStepSize(){ return mS.maxStepHeight;}
		void SetMaxStepSize(float afSize){ mS.maxStepHeight = afSize;}

		float GetMaxStepSizeInAir(){ return mS.maxStepHeightInAir;}
		void SetMaxStepSizeInAir(float afSize){ mS.maxStepHeightInAir = afSize;}

		void SetStepClimbSpeed(float afX){ mS.stepClimbSpeed = afX;}
		float GetStepClimbSpeed(){ return mS.stepClimbSpeed;}

		void SetAccurateClimbing(bool abX){ mS.accurateClimbing = abX;}
		bool GetAccurateClimbing(){ return mS.accurateClimbing;}

		void SetClimbForwardMul(float afX){ mS.climbForwardMul= afX;}
		float GetClimbForwardMul(){ return mS.climbForwardMul;}

		void SetClimbHeightAdd(float afX){ mS.climbHeightAdd = afX;}
		float GetClimbHeightAdd(){ return mS.climbHeightAdd;}

		bool IsClimbing(){ return mS.climbing;}
		void ResetClimbing();

		const cVector3f& GetLastGroundNormal(){ return mS.lastGroundNormal;}

		void SetRotateYawWhenGravityAttached(bool abX){ mbRotateYawWhenGravityAttached = abX;}
		bool GetRotateYawWhenGravityAttached(){ return mbRotateYawWhenGravityAttached;}

		void SetGravityAttachedBody(iPhysicsBody* apBody){ mpGravityAttachedBody = apBody;}
		iPhysicsBody* GetGravityAttachedBody(){ return mpGravityAttachedBody;}

		iPhysicsMaterial * GetGravityCollideMaterial(){ return mpGravityCollideMaterial;}

		///////////////////////////////////////
		// Connection Properties

		iPhysicsBody * GetConnectedBody();
		cVector3f GetBodyConnectionPos();
		cVector3f GetCharConnectionPos();

		const cVector3f& GetConnectionPosLocalToBody(){ return mvConnectionPosLocalToBody; }
		const cVector3f& GetConnectionPosLocalToChar(){ return mvConnectionPosLocalToChar; }
		void SetConnectionPosLocalToBody(const cVector3f& avPos){ mvConnectionPosLocalToBody = avPos; }
		void SetConnectionPosLocalToChar(const cVector3f& avPos){ mvConnectionPosLocalToChar = avPos; }

		const cVector3f& GetConnectionLocalUpBody(){ return mvConnectionLocalUpBody;}
		void SetConnectionLocalUpBody(const cVector3f& avVec){ mvConnectionLocalUpBody = avVec;}

		void SetMaxConnectionForce(float afX){ mfMaxConnectionForce = afX; }
		void SetMaxConnectionTorque(float afX){ mfMaxConnectionTorque = afX; }
		float GetMaxConnectionForce(){ return mfMaxConnectionForce; }
		float GetMaxConnectionTorque(){ return mfMaxConnectionTorque; }

		///////////////////////////////////////
		// User Properties

		void SetCamera(cCamera *apCam);
        cCamera* GetCamera();
		void SetCameraPosAdd(const cVector3f &avAdd);
		cVector3f GetCameraPosAdd();
		void SetCameraSmoothPosNum(int alNum){ mlCameraSmoothPosNum = alNum;}
		int GetCameraSmoothPosNum(){ return mlCameraSmoothPosNum;}
		void SetCameraUseSmoothing(bool abX){ mbCameraUseSmoothing = abX; mlstCameraPos.clear(); if(mpAux) ohpl_h14_clear_camera(mpAux); UpdateCamera(); }
		bool GetCameraUseSmoothing(){ return mbCameraUseSmoothing;}
		void SetCameraUpdateActive(bool abX){ mbCameraUpdateActive = abX;}
		bool GetCameraUpdateActive(){ return mbCameraUpdateActive;}

		void SetEntity(iEntity3D *apEntity);
		iEntity3D* GetEntity();

		void SetEntityOffset(const cMatrixf &a_mtxOffset);
		const cMatrixf & GetEntityOffset();

		void SetEntityPostOffset(const cMatrixf &a_mtxOffset);
		const cMatrixf & GetEntityPostOffset();

		void SetEntitySmoothPosNum(int alNum){ mlEntitySmoothPosNum = alNum;}
		int GetEntitySmoothPosNum(){ return mlEntitySmoothPosNum;}

		void SetUseEntitySmoothYPos(bool abX){ mbEntitySmoothYPos = abX;}
		void SetEntitySmoothYPosNum(int alX){ mlEntitySmoothYPosNum = alX;}
		bool GetUseEntitySmoothYPos(){ return mbEntitySmoothYPos;}
		void SetEntityPitchAmount(float afX){ mfEntityPitchAmount = afX;}
		float GetEntityPitchAmount(){ return mfEntityPitchAmount;}
		int GetEntitySmoothYPosNum(){ return mlEntitySmoothYPosNum;}


		void SetUserData(void* apUserData){ mpUserData = apUserData;}
		void* GetUserData(){ return mpUserData;}

		void SetCallback(iCharacterBodyCallback *apCallback){ mpCallback = apCallback;}

		///////////////////////////////////////
		//Debug:
		const cVector3f& GetLastMovePosAdd(){ return mS.lastMovePosAdd;}

	protected:
		void UpdateMoveMatrix();

		void PreUpdateConnection(float afTimeStep);
		void PostUpdateConnection(float afTimeStep);
		void UpdateSafePositions(float afTimeStep);
		void UpdateBodyConnection(float afTimeStep);
		void UpdateCharacterConnection(float afTimeStep);




		static bool mbHpl3;
		static size_t HostOp(int32_t alOp, OhplH15Wa *a);




		void UpdateForcePushing(float afTimeStep);

		void UpdateCamera();
		void UpdateBodyActive();
		void UpdateEntity();

		void UpdateBody();

		void ClearGravityAttachment();
		void UpdateGravityAttachment();

		void EnableBodiesAroundCharacter();

		bool CheckCollision(cVector3f *apPushBackVector, const cVector3f& avPos, iPhysicsWorldCollisionCallback *apCallback,int alShapeIdx=-1);

		OhplCharState mS;
		OhplCharExt mE = OhplCharExt();
		OhplH14Aux *mpAux;
		OhplH14Conn MakeConn();
		OhplCharVt MakeVt();

		tString msName;



		bool &mbCollideCharacter = mE.collideCharacter;

		bool &mbPhysicsBodyActive = mE.physicsBodyActive;

		bool &mbStickToSlope = mE.stickToSlope;










		float &mfMaxPushMass = mE.maxPushMass;
		float &mfPushForce = mE.pushForce;
		float &mfPushImpulse = mE.pushImpulse;
		bool &mbPushIn2D = mE.pushIn2D;

		float &mfCharacterMaxPushMass = mE.charMaxPushMass;
		float &mfCharacterPushForce = mE.charPushForce;
		bool &mbCharacterPushIn2D = mE.charPushIn2D;

		int &mlMinBodyPushStrength = mE.minBodyPushStrength;



		std::vector<cVector3f> mvSafePositions;
		cVector3f mvSafePosHistory[4];
		float mfSafePosTimer, mfSafePosInterval, mfSafePosBuffer;



		float &mfConstantContactForceMul = mE.constantContactForceMul;
		float &mfVelocityContactForceMul = mE.velocityContactForceMul;
		float &mfMaxContactForcePerMassUnit = mE.maxContactForcePerMassUnit;

		cCamera *&mpCamera = mE.camera;
		cVector3f &mvCameraPosAdd = mE.cameraPosAdd;
		int &mlCameraSmoothPosNum = mE.cameraSmoothPosNum;
		bool &mbCameraUseSmoothing = mE.cameraUseSmoothing;
		bool &mbCameraUpdateActive = mE.cameraUpdateActive;
		tVector3fList mlstCameraPos;

		iEntity3D *&mpEntity = mE.entity;
		cMatrixf &m_mtxEntityOffset = mE.entityOffset;
		cMatrixf &m_mtxEntityPostOffset = mE.entityPostOffset;
		int &mlEntitySmoothPosNum = mE.entitySmoothPosNum;
		tVector3fList mlstEntityPos;

		bool &mbEntitySmoothYPos = mE.entitySmoothYPos;
		float &mfEntityPitchAmount = mE.entityPitchAmount;
		int &mlEntitySmoothYPosNum = mE.entitySmoothYPosNum;
		tFloatList mlstEntityYPositions;



		void *&mpUserData = mE.userData;

		float &mfMaxConnectionTorque = mE.maxConnectionTorque;
		float &mfMaxConnectionForce = mE.maxConnectionForce;
		iPhysicsBody *&mpConnectedBody = mE.connectedBody;
		bool &mbConnectedBodyHadGravity = mE.connectedBodyHadGravity;
		bool &mbConnectionDependOnBodyRotation = mE.connDependOnBodyRotation;
		cVector3f &mvConnectionPosLocalToBody = mE.connPosLocalToBody;
		cVector3f &mvConnectionPosLocalToChar = mE.connPosLocalToChar;
		cVector3f &mvConnectionNormalLocalToBody = mE.connNormalLocalToBody;
		cVector3f &mvConnectionLocalUpBody = mE.connLocalUpBody;
		cVector3f &mvConnectionBodyStartRotation = mE.connBodyStartRotation;
		float &mfConnectionDistanceToBody = mE.connDistanceToBody;
		bool &mbConnectionAlignCenter = mE.connAlignCenter;
		bool &mbConnectionAlignRotation = mE.connAlignRotation;
		bool &mbConnectionCollision = mE.connCollision;
		bool &mbConnectedBodyIsActive = mE.connectedBodyIsActive;

		bool &mbConnectionAffectChar = mE.connAffectChar;
		bool &mbConnectionDependOnCharRotation = mE.connDependOnCharRotation;
		bool &mbConnectionAlignCharacterRotation = mE.connAlignCharRotation;
		cVector3f &mvConnectionCharLocalForward = mE.connCharLocalForward;

		cPidControllerVec3 mBodyConnectionPointPid;
		cPidControllerVec3 mBodyCenterPid;
		cPidControllerf mBodyOneAxisAngularPid;
		cPidControllerVec3 mBodyAngularPid;

		cPidControllerVec3 mCharConnectionPointPid;
		cPidControllerVec3 mCharRotatePid;

		bool &mbRotateYawWhenGravityAttached = mE.rotateYawWhenGravityAttached;
		iPhysicsBody *&mpGravityAttachedBody = mE.gravityAttachedBody;
		iPhysicsBody *&mpGravityLastAttachedBody = mE.gravityLastAttachedBody;
		cMatrixf &m_mtxGravityAttachedPrevMatrix = mE.gravityAttachedPrevMatrix;
		int &mlGravityAttachmentContacts = mE.gravityAttachmentContacts;
		cVector3f &mvGravityAttachmentBodyVelocity = mE.gravityAttachBodyVelocity;


		iCharacterBodyCallback *&mpCallback = mE.callback;

		iPhysicsWorld *&mpWorld = mE.world;

		cCharacterBodyCollideGravity *mpCollideGravityCallback;
		cCharacterBodyCollidePush *mpCollidePushCallback;
		cCharacterBodyRay *mpRayCallback;

		iPhysicsMaterial *&mpGravityCollideMaterial = mE.gravityCollideMaterial;

		iCollideShape *mpCurrentShape;
		iPhysicsBody *mpCurrentBody;
		std::vector<iCollideShape*> mvShapes;
		std::vector<iPhysicsBody*> mvBodies;

		static std::vector<iPhysicsBody*> mvTempBodies;
	};
};
#endif // HPL_CHARACTER_BODY_H
