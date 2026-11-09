#ifndef HPL_ENTITY_OBJECT_BUILDER_H
#define HPL_ENTITY_OBJECT_BUILDER_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"

#include <cstdint>
#include <vector>

namespace hpl {

	class cXmlElement;

	typedef uintptr_t tEob;

	enum eEobWorldEntity
	{
		eEobWorldEntity_ParticleSystem,
		eEobWorldEntity_Billboard,
		eEobWorldEntity_LensFlare,
		eEobWorldEntity_Sound,
		eEobWorldEntity_Light,
	};

	struct cEobParams
	{
		tString msName;
		tString msFileName;
		tString msFullPath;
		tString msFullPathDir;
		cMatrixf m_mtxTransform;
		cVector3f mvScale;
		bool mbPhysicsWorld;
	};

	struct cEobFlags
	{
		bool mbAnimations, mbParticleSystems, mbBillboards, mbSounds, mbLights;
		bool mbSubMeshScaleIncludesModelScale, mbSubMeshInstanceScaleAfterRotation, mbSubMeshMaterials, mbShareBodyShapes;
	};

	struct cEobBodyProps
	{
		cMatrixf m_mtxMatrix;
		float mfMass, mfAngularDamping, mfLinearDamping, mfBuoyancyDensityMul, mfMaxAngularSpeed, mfMaxLinearSpeed;
		bool mbBlocksSound, mbCollideCharacter, mbCollide, mbGravity, mbContinuousCollision, mbPushedByCharacterGravity;
		bool mbVolatile, mbUseSurfaceEffects, mbCanAttachCharacter;
		int mlID;
	};

	struct cEobJointProps
	{
		tString msMoveSound, msBreakSound, msMaxLimitSound, msMinLimitSound;
		float mfMinMoveSpeed, mfMinMoveFreq, mfMinMoveVolume, mfMinMoveFreqSpeed, mfMaxMoveFreq, mfMaxMoveVolume;
		float mfMaxMoveFreqSpeed, mfMiddleMoveSpeed, mfMiddleMoveVolume, mfBreakForce, mfLimitAutoSleepDist;
		float mfMaxLimitMaxSpeed, mfMaxLimitMinSpeed, mfMinLimitMaxSpeed, mfMinLimitMinSpeed;
		bool mbAngularMove, mbStickyMinLimit, mbStickyMaxLimit, mbBreakable, mbLimitAutoSleep, mbCollideBodies;
		int mlLimitAutoSleepNumSteps, mlID;
	};

	// Every engine object the .ent parser creates or inspects goes through here.
	class iEntityObjectBuilder
	{
	public:
		virtual ~iEntityObjectBuilder(){}

		virtual void Error(const tString& asMsg)=0;
		virtual void Warning(const tString& asMsg)=0;

		virtual void BeforeLoad()=0;
		virtual void AfterLoad()=0;
		virtual bool BeforeJoints()=0;
		virtual cEobFlags Flags()=0;

		virtual bool CreateMesh(const tString& asFile, bool &abSkeleton, int &alAnimNum)=0;
		virtual tEob CreateMeshEntity(const tString& asName)=0;
		virtual void SetMatrix(tEob aEnt, const cMatrixf& a_mtx)=0;
		virtual void SetShadowCaster(tEob aEnt)=0;
		virtual void UpdateLogic(tEob aEnt)=0;
		virtual tEob GetSubMeshEntity(const tString& asName)=0;
		virtual void WarnMissingSubMesh(const tString& asName)=0;
		virtual cVector3f GetSubMeshModelScale(tEob aSub)=0;
		virtual void SetWorldMatrix(tEob aEnt, const cMatrixf& a_mtx)=0;
		virtual void SetUniqueID(tEob aEnt, int alID)=0;
		virtual void SetSubMeshMaterial(tEob aSub, const tString& asFile)=0;

		virtual tEob AddAnimation(const tString& asFile, const tString& asName, float afSpeed)=0;
		virtual void SetSpecialEventTime(tEob aState, float afTime)=0;
		virtual void AddAnimationEvent(tEob aState, float afTime, int alType, const tString& asValue, const tString& asName, const tString& asDestSocket)=0;

		virtual tEob LoadWorldEntity(eEobWorldEntity aKind, cXmlElement *apElem, const tString& asPrefix)=0;
		virtual cVector3f GetLocalPosition(tEob aEnt)=0;
		virtual void SetPosition(tEob aEnt, const cVector3f& avPos)=0;
		virtual void LinkLightBillboards()=0;

		virtual tEob GetBoneState(const tString& asName)=0;
		virtual int GetBoneStateNum()=0;
		virtual void WarnSkeletonMismatch()=0;
		virtual cMatrixf GetBoneWorldMatrix(tEob aBone)=0;
		virtual tString GetBoneName(tEob aBone)=0;

		virtual int GetUniqueID(tEob aEnt)=0;
		virtual bool IsSubMesh(tEob aEnt)=0;
		virtual cMatrixf GetLocalMatrix(tEob aEnt)=0;
		virtual cMatrixf GetWorldMatrix(tEob aEnt)=0;
		virtual tString GetEntityName(tEob aEnt)=0;
		virtual void SetNodeAnimation()=0;
		virtual void AttachEntityChild(tEob aParent, const cMatrixf& a_mtxInvParent, tEob aChild)=0;
		virtual void AttachBoneChild(tEob aBone, const cMatrixf& a_mtxInvParent, tEob aChild)=0;
		virtual void AttachBoneToBody(tEob aBody, const cMatrixf& a_mtxInvParent, tEob aBone)=0;

		virtual tEob CreateBoxShape(const cVector3f& avSize, const cMatrixf& a_mtxOffset)=0;
		virtual tEob CreateSphereShape(const cVector3f& avSize, const cMatrixf& a_mtxOffset)=0;
		virtual tEob CreateCylinderShape(float afRadius, float afHeight, const cMatrixf& a_mtxOffset)=0;
		virtual tEob CreateCapsuleShape(float afRadius, float afHeight, const cMatrixf& a_mtxOffset)=0;
		virtual tEob CreateCompoundShape(const std::vector<tEob>& avShapes)=0;
		virtual void IncShapeUserCount(tEob aShape)=0;
		virtual void DestroyShape(tEob aShape)=0;

		virtual tEob CreateBody(const tString& asName, tEob aShape)=0;
		virtual void SetBodyProperties(tEob aBody, const cEobBodyProps& aProps)=0;
		virtual void SetBodyMaterial(tEob aBody, const tString& asName)=0;
		virtual void RegisterBody(tEob aBody)=0;

		virtual tEob CreateJointHinge(const tString& asName, const cVector3f& avPivot, const cVector3f& avPin, tEob aParent, tEob aChild)=0;
		virtual tEob CreateJointBall(const tString& asName, const cVector3f& avPivot, const cVector3f& avPin, tEob aParent, tEob aChild)=0;
		virtual tEob CreateJointSlider(const tString& asName, const cVector3f& avPivot, const cVector3f& avPin, tEob aParent, tEob aChild)=0;
		virtual tEob CreateJointScrew(const tString& asName, const cVector3f& avPivot, const cVector3f& avPin, tEob aParent, tEob aChild)=0;
		virtual void SetHingeLimits(tEob aJoint, float afMin, float afMax)=0;
		virtual void SetBallLimits(tEob aJoint, float afCone, float afTwist)=0;
		virtual void SetSliderLimits(tEob aJoint, float afMin, float afMax)=0;
		virtual void SetScrewLimits(tEob aJoint, float afMin, float afMax)=0;
		virtual void SetJointProperties(tEob aJoint, const cEobJointProps& aProps)=0;
		virtual void RegisterJoint(tEob aJoint)=0;

		virtual int GetAnimationStateNum()=0;
		virtual void PlayFirstAnimation()=0;
		virtual void SetSkeletonPhysicsActive()=0;
		virtual void LoadUserVariables()=0;
	};

	// Runs the .ent parse; true when it reached the end, false when it bailed out early.
	bool ParseEntityObject(iEntityObjectBuilder &aBuilder, const cEobParams &aParams, cXmlElement *apRootElem);

};
#endif // HPL_ENTITY_OBJECT_BUILDER_H
