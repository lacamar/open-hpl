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

#include "resources/EntityLoader_Object.h"

#include "system/String.h"
#include "scene/World.h"
#include "system/LowLevelSystem.h"


#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsJoint.h"
#include "physics/PhysicsController.h"
#include "physics/CollideShape.h"
#include "physics/PhysicsJointBall.h"
#include "physics/PhysicsJointHinge.h"
#include "physics/PhysicsJointScrew.h"
#include "physics/PhysicsJointSlider.h"

#include "math/Math.h"

#include "resources/MeshManager.h"
#include "resources/MaterialManager.h"
#include "resources/AnimationManager.h"
#include "resources/MeshLoaderHandler.h"
#include "resources/FileSearcher.h"
#include "resources/XmlDocument.h"
#include "resources/EngineFileLoading.h"

#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Animation.h"
#include "graphics/BoneState.h"
#include "scene/AnimationState.h"
#include "scene/MeshEntity.h"
#include "scene/Node3D.h"
#include "scene/SoundEntity.h"
#include "scene/BillBoard.h"
#include "scene/LensFlare.h"
#include "scene/ParticleSystem.h"

#include "scene/Light.h"


#include "resources/EntityObjectBuilder.h"

namespace hpl {

	bool cEntityLoader_Object::mbSubMeshScaleIncludesModelScale = false;
	bool cEntityLoader_Object::mbSubMeshInstanceScaleAfterRotation = false;
	bool cEntityLoader_Object::mbSubMeshMaterials = false;
	bool cEntityLoader_Object::mbShareBodyShapes = false;

	//-----------------------------------------------------------------------

	eCollideShapeType ToCollideShape(const tString& asType)
	{
		tString sLowType = cString::ToLowerCase(asType);

		if(sLowType == "box") return eCollideShapeType_Box;
		if(sLowType == "cylinder") return eCollideShapeType_Cylinder;
		if(sLowType == "sphere") return eCollideShapeType_Sphere;
		if(sLowType == "capsule") return eCollideShapeType_Capsule;

		Error("CollideShape '%s' does not exist!\n", asType.c_str());

		return eCollideShapeType_Null;
	}

	iCollideShape* CreateCollideShape(cXmlElement *apShapeElem, iPhysicsWorld *apPhysicsWorld, const cVector3f &avScale, const cMatrixf &a_mtxParent)
	{
		eCollideShapeType type = ToCollideShape(apShapeElem->GetAttributeString("ShapeType"));
		cVector3f vSize = apShapeElem->GetAttributeVector3f("Scale") * avScale;
		cVector3f vPos = apShapeElem->GetAttributeVector3f("RelativeTranslation") * avScale;
		cVector3f vRot = apShapeElem->GetAttributeVector3f("RelativeRotation");

		cMatrixf mtxOffset = cMath::MatrixRotate(vRot,eEulerRotationOrder_XYZ);
		mtxOffset.SetTranslation(vPos);
		mtxOffset = cMath::MatrixMul(a_mtxParent, mtxOffset);

		switch(type)
		{
		case eCollideShapeType_Box: 
			return apPhysicsWorld->CreateBoxShape(vSize,&mtxOffset);
		case eCollideShapeType_Sphere: 
			return apPhysicsWorld->CreateSphereShape(vSize,&mtxOffset);
		case eCollideShapeType_Cylinder: 
			mtxOffset = cMath::MatrixMul(mtxOffset, cMath::MatrixRotateZ(kPi2f));
			return apPhysicsWorld->CreateCylinderShape(vSize.x,vSize.y,&mtxOffset);
		case eCollideShapeType_Capsule: 
			mtxOffset = cMath::MatrixMul(mtxOffset, cMath::MatrixRotateZ(kPi2f));
			return apPhysicsWorld->CreateCapsuleShape(vSize.x,vSize.y,&mtxOffset);
		}

		return NULL;
	}


	//-----------------------------------------------------------------------

	class cEobImpl : public iEntityObjectBuilder
	{
	public:
		cEobImpl(cEntityLoader_Object *apLoader, const tString &asName, cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld,
				cResourceVarsObject *apInstanceVars)
			: mpL(apLoader), msName(asName), mpRoot(apRootElem), m_mtxTransform(a_mtxTransform), mpWorld(apWorld), mpVars(apInstanceVars) {}

		static tEob H(iEntity3D *apEnt){ return (tEob)apEnt; }
		static iEntity3D* E(tEob aH){ return (iEntity3D*)aH; }
		static iPhysicsBody* B(tEob aH){ return static_cast<iPhysicsBody*>((iEntity3D*)aH); }
		static iPhysicsJoint* J(tEob aH){ return (iPhysicsJoint*)aH; }
		static cBoneState* S(tEob aH){ return (cBoneState*)aH; }

		void Error(const tString& asMsg){ hpl::Error("%s", asMsg.c_str()); }
		void Warning(const tString& asMsg){ hpl::Warning("%s", asMsg.c_str()); }

		void BeforeLoad(){ mpL->BeforeLoad(mpRoot, m_mtxTransform, mpWorld, mpVars); }
		void AfterLoad(){ mpL->AfterLoad(mpRoot, m_mtxTransform, mpWorld, mpVars); }
		bool BeforeJoints(){ return mpL->BeforeJoints(); }

		cEobFlags Flags()
		{
			cEobFlags f;
			f.mbAnimations = mpL->mbLoadAnimations;
			f.mbParticleSystems = mpL->mbLoadParticleSystems;
			f.mbBillboards = mpL->mbLoadBillboards;
			f.mbSounds = mpL->mbLoadSounds;
			f.mbLights = mpL->mbLoadLights;
			f.mbSubMeshScaleIncludesModelScale = cEntityLoader_Object::mbSubMeshScaleIncludesModelScale;
			f.mbSubMeshInstanceScaleAfterRotation = cEntityLoader_Object::mbSubMeshInstanceScaleAfterRotation;
			f.mbSubMeshMaterials = cEntityLoader_Object::mbSubMeshMaterials;
			f.mbShareBodyShapes = cEntityLoader_Object::mbShareBodyShapes;
			return f;
		}

		bool CreateMesh(const tString& asFile, bool &abSkeleton, int &alAnimNum)
		{
			mpL->mpMesh = mpWorld->GetResources()->GetMeshManager()->CreateMesh(asFile);
			if(mpL->mpMesh==NULL) return false;
			abSkeleton = mpL->mpMesh->GetSkeleton()!=NULL;
			alAnimNum = mpL->mpMesh->GetAnimationNum();
			return true;
		}

		tEob CreateMeshEntity(const tString& asName)
		{
			mpL->mpEntity = mpWorld->CreateMeshEntity(asName, mpL->mpMesh, mpL->mbLoadAsStatic && mpL->mpMesh->GetAnimationNum() == 0);
			return H(mpL->mpEntity);
		}

		void SetMatrix(tEob aEnt, const cMatrixf& a_mtx){ E(aEnt)->SetMatrix(a_mtx); }
		void SetShadowCaster(tEob aEnt){ mpL->mpEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster,true); }
		void UpdateLogic(tEob aEnt){ E(aEnt)->UpdateLogic(0); }

		tEob GetSubMeshEntity(const tString& asName)
		{
			return H(mpL->mpEntity->GetSubMeshEntityName(asName));
		}

		void WarnMissingSubMesh(const tString& asName)
		{
			hpl::Warning("Sub mesh '%s' does not exist in mesh '%s'!\n",asName.c_str(), mpL->mpMesh->GetName().c_str());
		}

		cVector3f GetSubMeshModelScale(tEob aSub){ return static_cast<cSubMeshEntity*>(E(aSub))->GetSubMesh()->GetModelScale(); }
		void SetWorldMatrix(tEob aEnt, const cMatrixf& a_mtx){ E(aEnt)->SetWorldMatrix(a_mtx); }
		void SetUniqueID(tEob aEnt, int alID){ E(aEnt)->SetUniqueID(alID); }

		void SetSubMeshMaterial(tEob aSub, const tString& asFile)
		{
			cMaterial *pMaterial = mpWorld->GetResources()->GetMaterialManager()->CreateMaterial(asFile);
			if(pMaterial) static_cast<cSubMeshEntity*>(E(aSub))->SetCustomMaterial(pMaterial);
		}

		tEob AddAnimation(const tString& asFile, const tString& asName, float afSpeed)
		{
			cAnimation *pAnim = mpWorld->GetResources()->GetAnimationManager()->CreateAnimation(asFile);
			if(pAnim==NULL) return 0;
			return (tEob)mpL->mpEntity->AddAnimation(pAnim, asName, afSpeed);
		}

		void SetSpecialEventTime(tEob aState, float afTime){ ((cAnimationState*)aState)->SetSpecialEventTime(afTime); }

		void AddAnimationEvent(tEob aState, float afTime, int alType, const tString& asValue, const tString& asName, const tString& asDestSocket)
		{
			cAnimationEvent *pEvent = ((cAnimationState*)aState)->CreateEvent();
			pEvent->mfTime = afTime;
			pEvent->mType = (eAnimationEventType)alType;
			pEvent->msValue = asValue;
			pEvent->msName = asName;
			pEvent->msDestSocket = asDestSocket;
		}

		tEob LoadWorldEntity(eEobWorldEntity aKind, cXmlElement *apElem, const tString& asPrefix)
		{
			cEntityLoader_Object &l = *mpL;
			switch(aKind)
			{
			case eEobWorldEntity_ParticleSystem:
			{
				cParticleSystem *pPS = cEngineFileLoading::LoadParticleSystem(apElem,asPrefix, mpWorld);
				if(pPS)	l.mvParticleSystems.push_back(pPS);
				return H(pPS);
			}
			case eEobWorldEntity_Billboard:
			{
				cBillboard *pBillboard = cEngineFileLoading::LoadBillboard(apElem,asPrefix, mpWorld, mpWorld->GetResources(), l.mbLoadAsStatic,
																			&mlstConnections);
				if(pBillboard)	l.mvBillboards.push_back(pBillboard);
				return H(pBillboard);
			}
			case eEobWorldEntity_LensFlare:
			{
				cLensFlare *pFlare = cEngineFileLoading::LoadLensFlare(apElem,asPrefix, mpWorld, mpWorld->GetResources(), l.mbLoadAsStatic);
				if(pFlare) l.mvLensFlares.push_back(pFlare);
				return H(pFlare);
			}
			case eEobWorldEntity_Sound:
			{
				cSoundEntity *pSound = cEngineFileLoading::LoadSound(apElem,asPrefix, mpWorld);
				if(pSound)	l.mvSoundEntities.push_back(pSound);
				return H(pSound);
			}
			case eEobWorldEntity_Light:
			{
				iLight *pLight = cEngineFileLoading::LoadLight(apElem,asPrefix, mpWorld, mpWorld->GetResources(), l.mbLoadAsStatic);
				if(pLight)	l.mvLights.push_back(pLight);
				return H(pLight);
			}
			}
			return 0;
		}

		cVector3f GetLocalPosition(tEob aEnt){ return E(aEnt)->GetLocalPosition(); }
		void SetPosition(tEob aEnt, const cVector3f& avPos){ E(aEnt)->SetPosition(avPos); }

		void LinkLightBillboards()
		{
			tEFL_LightBillboardConnectionListIt connIt = mlstConnections.begin();
			for(; connIt != mlstConnections.end(); ++connIt)
			{
				cEFL_LightBillboardConnection& lightConnect = *connIt;

				cBillboard *pBB = mpL->GetBillboardFromID(lightConnect.msBillboardID);
				iLight *pLight = mpL->GetLightFromName(lightConnect.msLightName);

				if(pLight==NULL)
				{
					hpl::Warning("Light with name '%s' does not exist!",lightConnect.msLightName.c_str());
					continue;
				}
				if(pBB==NULL)
				{
					hpl::Warning("Billboard with id '%d' does not exist!",lightConnect.msBillboardID);
					continue;
				}

				pLight->AttachBillboard(pBB, pBB->GetColor());
			}
		}

		tEob GetBoneState(const tString& asName){ return (tEob)mpL->mpEntity->GetBoneStateFromName(asName); }
		int GetBoneStateNum(){ return mpL->mpEntity->GetBoneStateNum(); }

		void WarnSkeletonMismatch()
		{
			hpl::Warning("Loading entity %s: Skeletons in mesh file (%ls) and .ent file (%ls) differ\n",
				msName.c_str(),
				mpL->mpMesh->GetFullPath().c_str(),
				static_cast<iXmlDocument*>(mpRoot)->GetPath().c_str());
		}

		cMatrixf GetBoneWorldMatrix(tEob aBone){ return S(aBone)->GetWorldMatrix(); }
		tString GetBoneName(tEob aBone){ return S(aBone)->GetName(); }

		int GetUniqueID(tEob aEnt){ return E(aEnt)->GetUniqueID(); }
		bool IsSubMesh(tEob aEnt){ return E(aEnt)->GetEntityType() == "SubMesh"; }
		cMatrixf GetLocalMatrix(tEob aEnt){ return E(aEnt)->GetLocalMatrix(); }
		cMatrixf GetWorldMatrix(tEob aEnt){ return E(aEnt)->GetWorldMatrix(); }
		tString GetEntityName(tEob aEnt){ return E(aEnt)->GetName(); }
		void SetNodeAnimation(){ mpL->mbNodeAnimation = true; }
		void AttachEntityChild(tEob aParent, const cMatrixf& a_mtxInvParent, tEob aChild){ mpL->AttachEntityChild(E(aParent), a_mtxInvParent, E(aChild)); }
		void AttachBoneChild(tEob aBone, const cMatrixf& a_mtxInvParent, tEob aChild){ mpL->AttachBoneChild(S(aBone), a_mtxInvParent, E(aChild)); }
		void AttachBoneToBody(tEob aBody, const cMatrixf& a_mtxInvParent, tEob aBone){ mpL->AttachBoneToBody(B(aBody), a_mtxInvParent, S(aBone)); }

		tEob CreateBoxShape(const cVector3f& avSize, const cMatrixf& a_mtxOffset)
		{
			cMatrixf m = a_mtxOffset;
			return (tEob)mpWorld->GetPhysicsWorld()->CreateBoxShape(avSize,&m);
		}
		tEob CreateSphereShape(const cVector3f& avSize, const cMatrixf& a_mtxOffset)
		{
			cMatrixf m = a_mtxOffset;
			return (tEob)mpWorld->GetPhysicsWorld()->CreateSphereShape(avSize,&m);
		}
		tEob CreateCylinderShape(float afRadius, float afHeight, const cMatrixf& a_mtxOffset)
		{
			cMatrixf m = a_mtxOffset;
			return (tEob)mpWorld->GetPhysicsWorld()->CreateCylinderShape(afRadius,afHeight,&m);
		}
		tEob CreateCapsuleShape(float afRadius, float afHeight, const cMatrixf& a_mtxOffset)
		{
			cMatrixf m = a_mtxOffset;
			return (tEob)mpWorld->GetPhysicsWorld()->CreateCapsuleShape(afRadius,afHeight,&m);
		}
		tEob CreateCompoundShape(const std::vector<tEob>& avShapes)
		{
			tCollideShapeVec vShapes;
			for(size_t i=0; i<avShapes.size(); ++i) vShapes.push_back((iCollideShape*)avShapes[i]);
			return (tEob)mpWorld->GetPhysicsWorld()->CreateCompundShape(vShapes);
		}
		void IncShapeUserCount(tEob aShape){ ((iCollideShape*)aShape)->IncUserCount(); }
		void DestroyShape(tEob aShape){ mpWorld->GetPhysicsWorld()->DestroyShape((iCollideShape*)aShape); }

		tEob CreateBody(const tString& asName, tEob aShape)
		{
			return H(mpWorld->GetPhysicsWorld()->CreateBody(asName,(iCollideShape*)aShape));
		}

		void SetBodyProperties(tEob aBody, const cEobBodyProps& p)
		{
			iPhysicsBody *apBody = B(aBody);
			apBody->SetMatrix(p.m_mtxMatrix);
			apBody->SetMass(p.mfMass);
			apBody->SetAngularDamping(p.mfAngularDamping);
			apBody->SetLinearDamping(p.mfLinearDamping);
			apBody->SetBlocksSound(p.mbBlocksSound);
			apBody->SetCollideCharacter(p.mbCollideCharacter);
			apBody->SetCollide(p.mbCollide);
			apBody->SetGravity(p.mbGravity);
			apBody->SetBuoyancyDensityMul(p.mfBuoyancyDensityMul);
			apBody->SetMaxAngularSpeed(p.mfMaxAngularSpeed);
			apBody->SetMaxLinearSpeed(p.mfMaxLinearSpeed);
			apBody->SetContinuousCollision(p.mbContinuousCollision);
			apBody->SetPushedByCharacterGravity(p.mbPushedByCharacterGravity);
			apBody->SetVolatile(p.mbVolatile);
			apBody->SetUseSurfaceEffects(p.mbUseSurfaceEffects);
			apBody->SetGravityCanAttachCharacter(p.mbCanAttachCharacter);
			apBody->SetUniqueID(p.mlID);
		}

		void SetBodyMaterial(tEob aBody, const tString& asName)
		{
			iPhysicsMaterial *pPhysicsMat = mpWorld->GetPhysicsWorld()->GetMaterialFromName(asName);
			if(pPhysicsMat) B(aBody)->SetMaterial(pPhysicsMat);
		}

		void RegisterBody(tEob aBody)
		{
			iPhysicsBody *pBody = B(aBody);
			mpL->mvBodies.push_back(pBody);
			mpL->mvBodyExtraData.push_back(cEntityBodyExtraData());
			mpL->mvBodyExtraData[mpL->mvBodies.size() -1].m_mtxLocalTransform = pBody->GetLocalMatrix();
		}

		tEob CreateJointHinge(const tString& asName, const cVector3f& avPivot, const cVector3f& avPin, tEob aParent, tEob aChild)
		{
			return (tEob)(iPhysicsJoint*)mpWorld->GetPhysicsWorld()->CreateJointHinge(asName,avPivot,avPin,aParent ? B(aParent) : NULL,B(aChild));
		}
		tEob CreateJointBall(const tString& asName, const cVector3f& avPivot, const cVector3f& avPin, tEob aParent, tEob aChild)
		{
			return (tEob)(iPhysicsJoint*)mpWorld->GetPhysicsWorld()->CreateJointBall(asName,avPivot,avPin,aParent ? B(aParent) : NULL,B(aChild));
		}
		tEob CreateJointSlider(const tString& asName, const cVector3f& avPivot, const cVector3f& avPin, tEob aParent, tEob aChild)
		{
			return (tEob)(iPhysicsJoint*)mpWorld->GetPhysicsWorld()->CreateJointSlider(asName,avPivot,avPin,aParent ? B(aParent) : NULL,B(aChild));
		}
		tEob CreateJointScrew(const tString& asName, const cVector3f& avPivot, const cVector3f& avPin, tEob aParent, tEob aChild)
		{
			return (tEob)(iPhysicsJoint*)mpWorld->GetPhysicsWorld()->CreateJointScrew(asName,avPivot,avPin,aParent ? B(aParent) : NULL,B(aChild));
		}

		void SetHingeLimits(tEob aJoint, float afMin, float afMax)
		{
			iPhysicsJointHinge *pJoint = static_cast<iPhysicsJointHinge*>(J(aJoint));
			pJoint->SetMinAngle(afMin);
			pJoint->SetMaxAngle(afMax);
		}
		void SetBallLimits(tEob aJoint, float afCone, float afTwist){ static_cast<iPhysicsJointBall*>(J(aJoint))->SetConeLimits(afCone, afTwist); }
		void SetSliderLimits(tEob aJoint, float afMin, float afMax)
		{
			iPhysicsJointSlider *pJoint = static_cast<iPhysicsJointSlider*>(J(aJoint));
			pJoint->SetMinDistance(afMin);
			pJoint->SetMaxDistance(afMax);
		}
		void SetScrewLimits(tEob aJoint, float afMin, float afMax)
		{
			iPhysicsJointScrew *pJoint = static_cast<iPhysicsJointScrew*>(J(aJoint));
			pJoint->SetMinDistance(afMin);
			pJoint->SetMaxDistance(afMax);
		}

		void SetJointProperties(tEob aJoint, const cEobJointProps& p)
		{
			iPhysicsJoint *pJoint = J(aJoint);
			tString sMoveSound = p.msMoveSound;
			pJoint->SetMoveSound(sMoveSound);
			pJoint->SetMinMoveSpeed(p.mfMinMoveSpeed);
			pJoint->SetMinMoveFreq(p.mfMinMoveFreq);
			pJoint->SetMinMoveVolume(p.mfMinMoveVolume);
			pJoint->SetMinMoveFreqSpeed(p.mfMinMoveFreqSpeed);
			pJoint->SetMaxMoveFreq(p.mfMaxMoveFreq);
			pJoint->SetMaxMoveVolume(p.mfMaxMoveVolume);
			pJoint->SetMaxMoveFreqSpeed(p.mfMaxMoveFreqSpeed);
			pJoint->SetMiddleMoveSpeed(p.mfMiddleMoveSpeed);
			pJoint->SetMiddleMoveVolume(p.mfMiddleMoveVolume);
			pJoint->SetMoveSpeedType(p.mbAngularMove ? ePhysicsJointSpeed_Angular : ePhysicsJointSpeed_Linear);

			pJoint->SetStickyMinLimit(p.mbStickyMinLimit);
			pJoint->SetStickyMaxLimit(p.mbStickyMaxLimit);

			pJoint->SetBreakable(p.mbBreakable);
			pJoint->SetBreakForce(p.mfBreakForce);
			pJoint->SetBreakSound(p.msBreakSound);

			pJoint->SetLimitAutoSleep(p.mbLimitAutoSleep);
			pJoint->SetLimitAutoSleepDist(p.mfLimitAutoSleepDist);
			pJoint->SetLimitAutoSleepNumSteps(p.mlLimitAutoSleepNumSteps);

			pJoint->SetCollideBodies(p.mbCollideBodies);

			pJoint->GetMaxLimit()->msSound = p.msMaxLimitSound;
			pJoint->GetMaxLimit()->mfMaxSpeed = p.mfMaxLimitMaxSpeed;
			pJoint->GetMaxLimit()->mfMinSpeed = p.mfMaxLimitMinSpeed;

			pJoint->GetMinLimit()->msSound = p.msMinLimitSound;
			pJoint->GetMinLimit()->mfMaxSpeed = p.mfMinLimitMaxSpeed;
			pJoint->GetMinLimit()->mfMinSpeed = p.mfMinLimitMinSpeed;

			pJoint->SetUniqueID(p.mlID);
		}

		void RegisterJoint(tEob aJoint){ mpL->mvJoints.push_back(J(aJoint)); }

		int GetAnimationStateNum(){ return mpL->mpEntity->GetAnimationStateNum(); }
		void PlayFirstAnimation(){ mpL->mpEntity->Play(0, true, true); }
		void SetSkeletonPhysicsActive(){ mpL->mpEntity->SetSkeletonPhysicsActive(true); }
		void LoadUserVariables(){ mpL->LoadUserVariables(mpRoot); }

	private:
		cEntityLoader_Object *mpL;
		tString msName;
		cXmlElement *mpRoot;
		cMatrixf m_mtxTransform;
		cWorld *mpWorld;
		cResourceVarsObject *mpVars;
		tEFL_LightBillboardConnectionList mlstConnections;
	};

	//-----------------------------------------------------------------------

	iEntity3D* cEntityLoader_Object::Load(	const tString &asName, int alID, bool abActive, cXmlElement *apRootElem,
											const cMatrixf &a_mtxTransform, const cVector3f &avScale,
											cWorld *apWorld, const tString &asFileName, const tWString &asFullPath, cResourceVarsObject *apInstanceVars)
	{
		mvBodies.clear();
		mvJoints.clear();

		mvBodyExtraData.clear();

		mvParticleSystems.clear();
		mvBillboards.clear();
		mvLensFlares.clear();
		mvSoundEntities.clear();
		mvLights.clear();
		mvBeams.clear();

		mpEntity = NULL;
		mpMesh = NULL;

		msFileName = asFileName;
		mlID = alID;
		mbActive = abActive;
		mvScale = avScale;

		mbNodeAnimation = false;

		cEobParams params;
		params.msName = asName;
		params.msFileName = asFileName;
		params.msFullPath = cString::To8Char(asFullPath);
		params.msFullPathDir = cString::To8Char(cString::GetFilePathW(asFullPath));
		params.m_mtxTransform = a_mtxTransform;
		params.mvScale = avScale;
		params.mbPhysicsWorld = apWorld->GetPhysicsWorld() != NULL;

		cEobImpl builder(this, asName, apRootElem, a_mtxTransform, apWorld, apInstanceVars);

		if(ParseEntityObject(builder, params, apRootElem) == false) return NULL;
		return mpEntity;
	}

	//-----------------------------------------------------------------------

	cBillboard* cEntityLoader_Object::GetBillboardFromID(int alID)
	{
		for(size_t i=0; i<mvBillboards.size(); ++i)
		{
			cBillboard *pBB = mvBillboards[i];
			if(pBB->GetUniqueID() == alID) return pBB;
		}
		return NULL;
	}

	iLight* cEntityLoader_Object::GetLightFromName(const tString& asName)
	{
		for(size_t i=0; i<mvLights.size(); ++i)
		{
			iLight *pLight = mvLights[i];
			if(pLight->GetName() == asName) return pLight;
		}
		return NULL;
	}

	//-----------------------------------------------------------------------

	
	void cEntityLoader_Object::AttachEntityChild(iEntity3D *apParent, const cMatrixf& a_mtxInvParent, iEntity3D *apChild)
	{
		//Calculate local matrix
		cMatrixf mtxLocalTransform = cMath::MatrixMul(a_mtxInvParent, apChild->GetWorldMatrix());
		apChild->SetMatrix(mtxLocalTransform);

		//Add the entity to parent and remove it from any other hierarchy it might be in.
		if(apChild->GetEntityParent()) 
			apChild->GetEntityParent()->RemoveChild(apChild);
		apParent->AddChild(apChild);
	}

	//-----------------------------------------------------------------------

	void cEntityLoader_Object::AttachBoneChild(cBoneState *apBoneState, const cMatrixf& a_mtxInvParent, iEntity3D *apChild)
	{
		//Calculate local matrix
		cMatrixf mtxLocalTransform = cMath::MatrixMul(a_mtxInvParent, apChild->GetWorldMatrix());

		apChild->SetMatrix(mtxLocalTransform);

		//Add the entity to parent and remove it from any other hierarchy it might be in.
		if(apChild->GetParent()) 
			apChild->GetParent()->RemoveEntity(apChild);
		apBoneState->AddEntity(apChild);	
	}

	//-----------------------------------------------------------------------

	void cEntityLoader_Object::AttachBoneToBody(iPhysicsBody *apParentBody, const cMatrixf& a_mtxInvParent, cBoneState *apBoneState)
	{
		cMatrixf mtxInvBone = cMath::MatrixInverse(apBoneState->GetWorldMatrix());
		cMatrixf mtxBoneToBody = cMath::MatrixMul(mtxInvBone, apParentBody->GetWorldMatrix());

		//Attaching work differently for bones since they rely on base matrix and animations.
		//This sets up the basic properties and then the skeletal physics needs to be turned on the mesh entity.
		apBoneState->SetBody(apParentBody);
		apBoneState->SetBodyMatrix(mtxBoneToBody);

		apParentBody->SetIsRagDoll(true);
		//apParentBody->SetCollideRagDoll(false);

		/////////////////////////////////////
		//Create collider body
		/*
		iPhysicsBody *pColliderBody = mpPhysicsWorld->CreateBody(apEntity->GetName()+"_collider_"+pBoneState->GetName(),pShape);
		pColliderBody->SetMass(0);
		pColliderBody->SetActive(false);
		pColliderBody->SetCollideCharacter(false);

		pBoneState->SetColliderBody(pColliderBody);
		*/
	}
	
	//-----------------------------------------------------------------------

	
	ePhysicsControllerType GetControllerType(const char* apString)
	{
		if(apString == NULL) return ePhysicsControllerType_LastEnum;

		tString sName = apString;

		if(sName == "Pid") return ePhysicsControllerType_Pid;
		else if(sName == "Spring") return ePhysicsControllerType_Spring;
		
		return ePhysicsControllerType_LastEnum;
	}

	/////////////////////////

	static ePhysicsControllerInput GetControllerInput(const char* apString)
	{
		if(apString == NULL) return ePhysicsControllerInput_LastEnum;

		tString sName = apString;

		if(sName == "JointAngle") return ePhysicsControllerInput_JointAngle;
		else if(sName == "JointDist") return ePhysicsControllerInput_JointDist; 
		else if(sName == "LinearSpeed") return ePhysicsControllerInput_LinearSpeed;
		else if(sName == "AngularSpeed")  return ePhysicsControllerInput_AngularSpeed;
		
		return ePhysicsControllerInput_LastEnum;
	}

	/////////////////////////

	static ePhysicsControllerOutput GetControllerOutput(const char* apString)
	{
		if(apString == NULL) return ePhysicsControllerOutput_LastEnum;

		tString sName = apString;

		if(sName == "Force") return ePhysicsControllerOutput_Force;
		else if(sName == "Torque") return ePhysicsControllerOutput_Torque; 
		
		return ePhysicsControllerOutput_LastEnum;
	}

	/////////////////////////

	static ePhysicsControllerAxis GetControllerAxis(const char* apString)
	{
		if(apString == NULL) return ePhysicsControllerAxis_LastEnum;

		tString sName = apString;

		if(sName == "X") return ePhysicsControllerAxis_X;
		else if(sName == "Y") return ePhysicsControllerAxis_Y; 
		else if(sName == "Z") return ePhysicsControllerAxis_Z; 

		return ePhysicsControllerAxis_LastEnum;
	}

	/////////////////////////

	static ePhysicsControllerEnd GetControllerEnd(const char* apString)
	{
		if(apString == NULL) return ePhysicsControllerEnd_Null;

		tString sName = apString;

		if(sName == "OnMax") return ePhysicsControllerEnd_OnMax;
		else if(sName == "OnMin") return ePhysicsControllerEnd_OnMin; 
		else if(sName == "OnDest") return ePhysicsControllerEnd_OnDest; 

		return ePhysicsControllerEnd_Null;
	}

	/////////////////////////


	void cEntityLoader_Object::LoadController(iPhysicsJoint* apJoint,iPhysicsWorld *apPhysicsWorld, 
												TiXmlElement *apElem)
	{
		//////////////////////////////
		// Get the properties
		/*tString sName = cString::ToString(apElem->Attribute("Name"),"");
		bool bActive = cString::ToBool(apElem->Attribute("Active"),false);

		ePhysicsControllerType CtrlType = GetControllerType(apElem->Attribute("Type"));
		float fA= cString::ToFloat(apElem->Attribute("A"),0);
		float fB= cString::ToFloat(apElem->Attribute("B"),0);
		float fC= cString::ToFloat(apElem->Attribute("C"),0);
		int lIntegralSize = cString::ToInt(apElem->Attribute("IntegralSize"),1);

		ePhysicsControllerInput CtrlInput = GetControllerInput(apElem->Attribute("Input"));
		ePhysicsControllerAxis CtrlInputAxis = GetControllerAxis(apElem->Attribute("InputAxis"));
		float fDestValue = cString::ToFloat(apElem->Attribute("DestValue"),0);
		float fMaxOutput = cString::ToFloat(apElem->Attribute("MaxOutput"),0);

		ePhysicsControllerOutput CtrlOutput = GetControllerOutput(apElem->Attribute("Output"));
		ePhysicsControllerAxis CtrlOutputAxis = GetControllerAxis(apElem->Attribute("OutputAxis"));
		bool bMulMassWithOutput = cString::ToBool(apElem->Attribute("MulMassWithOutput"),false);

		ePhysicsControllerEnd CtrlEnd = GetControllerEnd(apElem->Attribute("EndType"));
		tString sNextCtrl = cString::ToString(apElem->Attribute("NextController"),"");
		
		bool bLogInfo = cString::ToBool(apElem->Attribute("LogInfo"),false);

		//Convert degrees to radians.
		if(CtrlInput == ePhysicsControllerInput_JointAngle) {
			fDestValue = cMath::ToRad(fDestValue);
		}

		//////////////////////////////
		// Create the controller
		iPhysicsController *pController =apPhysicsWorld->CreateController(sName);

		pController->SetType(CtrlType);

		pController->SetA(fA);
		pController->SetB(fB);
		pController->SetC(fC);

		pController->SetPidIntegralSize(lIntegralSize);

		pController->SetActive(bActive);
		pController->SetInputType(CtrlInput,CtrlInputAxis);
		pController->SetDestValue(fDestValue);
        
		pController->SetOutputType(CtrlOutput,CtrlOutputAxis);
		pController->SetMaxOutput(fMaxOutput);
		pController->SetMulMassWithOutput(bMulMassWithOutput);

		pController->SetEndType(CtrlEnd);
		pController->SetNextController(sNextCtrl);

		pController->SetLogInfo(bLogInfo);

		apJoint->AddController(pController);

		//Log("Controller: %s active: %d val: %f %f %f input: %d %d output: %d %d\n",
		//	sName.c_str(),bActive, fA,fB,fC, (int)CtrlInput, (int)CtrlInputAxis, (int)CtrlOutput,(int)CtrlOutputAxis);*/
	}

	//-----------------------------------------------------------------------

	eAnimationEventType cEntityLoader_Object::GetAnimationEventType(const char* apString)
	{
		if(apString == NULL) return eAnimationEventType_LastEnum;

        tString sName = apString;
		sName = cString::ToLowerCase(sName);

		if(sName == "playsound")
		{
			return eAnimationEventType_PlaySound;
		}

		Warning("Animation event type '%s' does not exist!\n",apString);
		return eAnimationEventType_LastEnum;
	}
	
	//-----------------------------------------------------------------------

	void cEntityLoader_Object::LoadUserVariables(cXmlElement *apRootElem)
	{
		cXmlElement *pVarRootElem = apRootElem->GetFirstElement("UserDefinedVariables");
		if(pVarRootElem==NULL){
			Warning("Can not find a use variable root element!\n");
			return;
		}

		msEntityType = pVarRootElem->GetAttributeString("EntityType");
		msEntitySubType = pVarRootElem->GetAttributeString("EntitySubType");

		LoadVariables(pVarRootElem);
	}
	
	//-----------------------------------------------------------------------
}