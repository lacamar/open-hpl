#include "resources/EntityObjectBuilder.h"

#include "system/String.h"
#include "math/Math.h"
#include "resources/XmlDocument.h"
#include "graphics/GraphicsTypes.h"

#include <cstdarg>
#include <cstdio>
#include <list>
#include <map>
#include <set>

namespace hpl {

	namespace {

	tString Fmt(const char *asFmt, ...)
	{
		char buf[2048];
		va_list ap;
		va_start(ap, asFmt);
		vsnprintf(buf, sizeof(buf), asFmt, ap);
		va_end(ap);
		return buf;
	}

	typedef std::multimap<int,tEob> tShapeMap;
	typedef std::multimap<int,tEob> tBodyMap;

	tEob GetBodyShape(iEntityObjectBuilder &b, cXmlElement *apBodyElem, tShapeMap &a_mapShapes, std::set<tEob> &a_setUsed, bool abShare)
	{
		std::vector<tEob> vShapes;
		cXmlNodeListIterator boundShapeIt = apBodyElem->GetChildIterator();
		while(boundShapeIt.HasNext())
		{
			cXmlElement *pShapeElem = boundShapeIt.Next()->ToElement();
			int lShapeID = pShapeElem->GetAttributeInt("ID");

			tShapeMap::iterator it = a_mapShapes.find(lShapeID);
			if(it != a_mapShapes.end())
			{
				vShapes.push_back(it->second);
				if(abShare)	{ if(a_setUsed.insert(it->second).second) b.IncShapeUserCount(it->second); }
				else		a_mapShapes.erase(it);
			}
		}

		if(vShapes.empty()) return 0;
		if(vShapes.size()==1) return vShapes[0];

		return b.CreateCompoundShape(vShapes);
	}

	tEob CreateShape(iEntityObjectBuilder &b, cXmlElement *apShapeElem, const cVector3f &avScale)
	{
		tString sType = apShapeElem->GetAttributeString("ShapeType");
		tString sLowType = cString::ToLowerCase(sType);
		int type = -1;
		if(sLowType == "box") type = 0;
		else if(sLowType == "cylinder") type = 1;
		else if(sLowType == "sphere") type = 2;
		else if(sLowType == "capsule") type = 3;
		else b.Error(Fmt("CollideShape '%s' does not exist!\n", sType.c_str()));

		cVector3f vSize = apShapeElem->GetAttributeVector3f("Scale") * avScale;
		cVector3f vPos = apShapeElem->GetAttributeVector3f("RelativeTranslation") * avScale;
		cVector3f vRot = apShapeElem->GetAttributeVector3f("RelativeRotation");

		cMatrixf mtxOffset = cMath::MatrixRotate(vRot,eEulerRotationOrder_XYZ);
		mtxOffset.SetTranslation(vPos);
		mtxOffset = cMath::MatrixMul(cMatrixf::Identity, mtxOffset);

		switch(type)
		{
		case 0: return b.CreateBoxShape(vSize,mtxOffset);
		case 2: return b.CreateSphereShape(vSize,mtxOffset);
		case 1:
			mtxOffset = cMath::MatrixMul(mtxOffset, cMath::MatrixRotateZ(kPi2f));
			return b.CreateCylinderShape(vSize.x,vSize.y,mtxOffset);
		case 3:
			mtxOffset = cMath::MatrixMul(mtxOffset, cMath::MatrixRotateZ(kPi2f));
			return b.CreateCapsuleShape(vSize.x,vSize.y,mtxOffset);
		}
		return 0;
	}

	cMatrixf GetMatrixFromVectors(const cVector3f& avPos, const cVector3f& avRot, const cVector3f& avScale)
	{
		cMatrixf mtxOut = cMath::MatrixScale(avScale);
		mtxOut = cMath::MatrixMul(cMath::MatrixRotate(avRot, eEulerRotationOrder_XYZ), mtxOut);
		mtxOut.SetTranslation(avPos);
		return mtxOut;
	}

	int ToAnimEventType(iEntityObjectBuilder &b, const tString& asType)
	{
		tString sLowType = cString::ToLowerCase(asType);

		if(sLowType == "playsound")	return eAnimationEventType_PlaySound;
		if(sLowType == "createparticle") return eAnimationEventType_CreateParticle;
		if(sLowType == "step")		return eAnimationEventType_Step;
		if(sLowType == "message")	return eAnimationEventType_Message;
		if(sLowType == "playloopsound") return eAnimationEventType_PlayLoopSound;
		if(sLowType == "stoploopsound") return eAnimationEventType_StopLoopSound;
		if(sLowType == "" || sLowType == "ik") return eAnimationEventType_LastEnum;

		b.Error(Fmt("No animation event named '%s'\n", asType.c_str()));
		return eAnimationEventType_LastEnum;
	}

	struct cParse
	{
		iEntityObjectBuilder &b;
		const cEobParams &p;
		cEobFlags f;
		bool mbSkeleton;
		int mlAnimNum;
		tEob mEnt;
		bool mbNodeAnimation;

		cParse(iEntityObjectBuilder &aB, const cEobParams &aP) : b(aB), p(aP), mbNodeAnimation(false) {}

		void AttachChildren(cXmlElement *apMainElem, tEob aParent, tEob aBone,
							std::list<tEob>& a_lstChildList, std::map<int,tEob> &a_mapBoneStates, bool abRemoveAttachedChild, bool abIsBody)
		{
			cXmlElement *pChildrenElem = apMainElem->GetFirstElement("Children");
			if(pChildrenElem==NULL) return;

			cMatrixf mtxInvParent;
			if(aParent)
				mtxInvParent = cMath::MatrixInverse(b.GetLocalMatrix(aParent));
			else
				mtxInvParent = cMath::MatrixInverse(b.GetBoneWorldMatrix(aBone));

			cXmlNodeListIterator childIt = pChildrenElem->GetChildIterator();
			while(childIt.HasNext())
			{
				cXmlElement *pChildElem = childIt.Next()->ToElement();

				int lID = pChildElem->GetAttributeInt("ID");

				bool bFound = false;
				for(std::list<tEob>::iterator it = a_lstChildList.begin(); it != a_lstChildList.end(); ++it)
				{
					tEob pEntity = *it;
					if(b.GetUniqueID(pEntity) != lID) continue;

					if(aParent)
					{
						if(b.IsSubMesh(pEntity) && mbSkeleton == false && mlAnimNum > 0)
						{
							mbNodeAnimation = true;
							b.SetNodeAnimation();
							continue;
						}
						else
						{
							b.AttachEntityChild(aParent, mtxInvParent, pEntity);
						}
					}
					else
						b.AttachBoneChild(aBone, mtxInvParent, pEntity);

					if(abRemoveAttachedChild) a_lstChildList.erase(it);
					bFound = true;
					break;
				}

				if(abIsBody && bFound==false)
				{
					std::map<int,tEob>::iterator it = a_mapBoneStates.find(lID);
					if(it != a_mapBoneStates.end())
					{
						b.AttachBoneToBody(aParent, mtxInvParent, it->second);
						bFound = true;

						a_mapBoneStates.erase(it);
					}
				}

				if(bFound == false)
				{
					if(aParent)
						b.Warning(Fmt("Could not find child with unique ID %d to attach to entity '%s' in '%s'\n", lID, b.GetEntityName(aParent).c_str(), p.msFileName.c_str()));
					else
						b.Warning(Fmt("Could not find child with unique ID %d to attach to bone '%s' in '%s'\n", lID, b.GetBoneName(aBone).c_str(), p.msFileName.c_str()));
				}
			}
		}

		tEob CreateJoint(cXmlElement *apJointElem, tBodyMap &a_mapBodies, const std::map<tEob, cMatrixf>& a_mapRestMatrix)
		{
			cVector3f vPivot = apJointElem->GetAttributeVector3f("WorldPos") * p.mvScale;

			cVector3f vPinDir;
			if(apJointElem->GetAttributeString("PinDir","") != "")
			{
				vPinDir = apJointElem->GetAttributeVector3f("PinDir");
			}
			else
			{
				vPinDir = cMath::MatrixMul3x3(cMath::MatrixRotate(apJointElem->GetAttributeVector3f("Rotation"),
																	eEulerRotationOrder_XYZ), cVector3f(0,1,0));
			}

			vPivot = cMath::MatrixMul(p.m_mtxTransform, vPivot);
			vPinDir = cMath::MatrixMul3x3(p.m_mtxTransform, vPinDir);

			tString sJointName = p.msName + "_" + apJointElem->GetAttributeString("Name");

			tString sType = apJointElem->GetValue();
			tString sLowType = cString::ToLowerCase(sType);
			int jointType = 1;
			if(sLowType == "jointhinge")	jointType = 0;
			else if(sLowType == "jointball")		jointType = 1;
			else if(sLowType == "jointslider")	jointType = 2;
			else if(sLowType == "joinscrew")		jointType = 3;
			else b.Error(Fmt("Joint type '%s' does not exist!\n", sType.c_str()));

			int lParentID = apJointElem->GetAttributeInt("ConnectedParentBodyID");
			int lChildID = apJointElem->GetAttributeInt("ConnectedChildBodyID");

			tBodyMap::iterator parentIt = a_mapBodies.find(lParentID);
			tEob pParentBody = (lParentID > 0 && parentIt != a_mapBodies.end()) ? parentIt->second : 0;
			tBodyMap::iterator childIt = a_mapBodies.find(lChildID);
			tEob pChildBody = childIt != a_mapBodies.end() ? childIt->second : 0;

			if(pChildBody==0)
			{
				b.Error(Fmt("Could not find child body with ID %d for joint '%s'\n", lChildID, sJointName.c_str()));
				return 0;
			}

			std::map<tEob, cMatrixf>::const_iterator restIt = a_mapRestMatrix.find(pChildBody);
			if(restIt != a_mapRestMatrix.end())
			{
				cMatrixf mtxDelta = cMath::MatrixMul(b.GetWorldMatrix(pChildBody), cMath::MatrixInverse(restIt->second));
				vPivot = cMath::MatrixMul(mtxDelta, vPivot);
				vPinDir = cMath::MatrixMul3x3(mtxDelta, vPinDir);
			}

			if(jointType == 0)
			{
				tEob pJoint = b.CreateJointHinge(sJointName,vPivot,vPinDir,pParentBody,pChildBody);
				float fMin = cMath::ToRad(apJointElem->GetAttributeFloat("MinAngle"));
				float fMax = cMath::ToRad(apJointElem->GetAttributeFloat("MaxAngle"));
				b.SetHingeLimits(pJoint, fMin, fMax);
				return pJoint;
			}
			else if(jointType == 1)
			{
				tEob pJoint = b.CreateJointBall(sJointName,vPivot,vPinDir,pParentBody,pChildBody);
				float fCone = cMath::ToRad(apJointElem->GetAttributeFloat("MaxConeAngle"));
				float fTwist = cMath::ToRad(apJointElem->GetAttributeFloat("MaxTwistAngle"));
				b.SetBallLimits(pJoint, fCone, fTwist);
				return pJoint;
			}
			else if(jointType == 2)
			{
				tEob pJoint = b.CreateJointSlider(sJointName,vPivot,vPinDir,pParentBody,pChildBody);
				float fMin = apJointElem->GetAttributeFloat("MinDistance");
				float fMax = apJointElem->GetAttributeFloat("MaxDistance");
				b.SetSliderLimits(pJoint, fMin, fMax);
				return pJoint;
			}
			else
			{
				tEob pJoint = b.CreateJointScrew(sJointName,vPivot,vPinDir,pParentBody,pChildBody);
				float fMin = apJointElem->GetAttributeFloat("MinDistance");
				float fMax = apJointElem->GetAttributeFloat("MaxDistance");
				b.SetScrewLimits(pJoint, fMin, fMax);
				return pJoint;
			}
		}

		void JointProperties(tEob aJoint, cXmlElement *e)
		{
			cEobJointProps j;
			j.msMoveSound = e->GetAttributeString("MoveSound","");
			j.mfMinMoveSpeed = e->GetAttributeFloat("MinMoveSpeed",0.5f);
			j.mfMinMoveFreq = e->GetAttributeFloat("MinMoveFreq",0.9f);
			j.mfMinMoveVolume = e->GetAttributeFloat("MinMoveVolume",0.3f);
			j.mfMinMoveFreqSpeed = e->GetAttributeFloat("MinMoveFreqSpeed",0.9f);
			j.mfMaxMoveFreq = e->GetAttributeFloat("MaxMoveFreq",1.1f);
			j.mfMaxMoveVolume = e->GetAttributeFloat("MaxMoveVolume",1.0f);
			j.mfMaxMoveFreqSpeed = e->GetAttributeFloat("MaxMoveFreqSpeed",1.1f);
			j.mfMiddleMoveSpeed = e->GetAttributeFloat("MiddleMoveSpeed",1.0f);
			j.mfMiddleMoveVolume = e->GetAttributeFloat("MiddleMoveVolume",1.0f);
			j.mbAngularMove = cString::ToLowerCase(e->GetAttributeString("MoveType","Linear")) == "angular";

			j.mbStickyMinLimit = e->GetAttributeBool("StickyMinLimit",false);
			j.mbStickyMaxLimit = e->GetAttributeBool("StickyMaxLimit",false);

			j.mbBreakable = e->GetAttributeBool("Breakable",false);
			j.mfBreakForce = e->GetAttributeFloat("BreakForce",1000);
			j.msBreakSound = e->GetAttributeString("BreakSound","");

			j.mbLimitAutoSleep = e->GetAttributeBool("LimitAutoSleep",false);
			j.mfLimitAutoSleepDist = e->GetAttributeFloat("LimitAutoSleepDist",0.02f);
			j.mlLimitAutoSleepNumSteps = e->GetAttributeInt("LimitAutoSleepNumSteps",10);

			j.mbCollideBodies = e->GetAttributeBool("CollideBodies",true);

			j.msMaxLimitSound = e->GetAttributeString("MaxLimitSound","");
			j.mfMaxLimitMaxSpeed = e->GetAttributeFloat("MaxLimitMaxSpeed",10.0f);
			j.mfMaxLimitMinSpeed = e->GetAttributeFloat("MaxLimit_MinSpeed",20.0f);
			if(j.mfMaxLimitMaxSpeed <=0) j.mfMaxLimitMaxSpeed = 0.01f;

			j.msMinLimitSound = e->GetAttributeString("MinLimitSound","");
			j.mfMinLimitMaxSpeed = e->GetAttributeFloat("MinLimitMaxSpeed",10.0f);
			j.mfMinLimitMinSpeed = e->GetAttributeFloat("MinLimitMinSpeed",20.0f);
			if(j.mfMinLimitMaxSpeed <=0) j.mfMaxLimitMaxSpeed = 0.01f;

			j.mlID = e->GetAttributeInt("ID",-1);

			b.SetJointProperties(aJoint, j);
		}

		void BodyProperties(tEob aBody, cXmlElement *e)
		{
			cEobBodyProps p2;
			p2.m_mtxMatrix = GetMatrixFromVectors(e->GetAttributeVector3f("WorldPos") * p.mvScale,
													e->GetAttributeVector3f("Rotation"),
													1.0f);
			p2.mfMass = e->GetAttributeFloat("Mass",1.0f);
			p2.mfAngularDamping = e->GetAttributeFloat("AngularDamping");
			p2.mfLinearDamping = e->GetAttributeFloat("LinearDamping");
			p2.mbBlocksSound = e->GetAttributeBool("BlocksSound",false);
			p2.mbCollideCharacter = e->GetAttributeBool("CollideCharacter",true);
			p2.mbCollide = e->GetAttributeBool("CollideNonCharacter",true);
			p2.mbGravity = e->GetAttributeBool("HasGravity",true);
			p2.mfBuoyancyDensityMul = e->GetAttributeFloat("BuoyancyDensityMul",1.0);
			p2.mfMaxAngularSpeed = e->GetAttributeFloat("MaxAngularSpeed",0);
			p2.mfMaxLinearSpeed = e->GetAttributeFloat("MaxLinearSpeed",0);
			p2.mbContinuousCollision = e->GetAttributeBool("ContinuousCollision",true);
			p2.mbPushedByCharacterGravity = e->GetAttributeBool("PushedByCharacterGravity",false);
			p2.mbVolatile = e->GetAttributeBool("Volatile",false);
			p2.mbUseSurfaceEffects = e->GetAttributeBool("UseSurfaceEffects",true);
			p2.mbCanAttachCharacter = e->GetAttributeBool("CanAttachCharacter",false);
			p2.mlID = e->GetAttributeInt("ID",-1);
			b.SetBodyProperties(aBody, p2);
		}

		bool Run(cXmlElement *apRootElem)
		{
			const cVector3f &mvScale = p.mvScale;
			std::list<tEob> lstEntities;

			cXmlElement* pModelDataElem = apRootElem->GetFirstElement("ModelData");
			if(pModelDataElem==NULL){
				b.Error("Couldn't load element ModelData"); return false;
			}

			b.BeforeLoad();
			f = b.Flags();

			cXmlElement *pMeshElem = pModelDataElem->GetFirstElement("Mesh");
			if(pMeshElem==NULL){
				b.Error("Couldn't load element Mesh"); return false;
			}

			{
				tString sMeshFile = pMeshElem->GetAttributeString("Filename");
				if(cString::GetFilePath(sMeshFile).size() < 1)
				{
					sMeshFile = cString::SetFilePath(sMeshFile, p.msFullPathDir);
				}
				if(b.CreateMesh(sMeshFile, mbSkeleton, mlAnimNum)==false) return false;

				mEnt = b.CreateMeshEntity(p.msName);

				if(mbSkeleton)
					b.SetMatrix(mEnt, cMath::MatrixScale(mvScale));

				b.SetShadowCaster(mEnt);
			}

			{
				bool bHasSkeleton = mbSkeleton;
				cXmlNodeListIterator submeshIt = pMeshElem->GetChildIterator();
				while(submeshIt.HasNext())
				{
					cXmlElement *pSubMeshElem = submeshIt.Next()->ToElement();

					tString sName = pSubMeshElem->GetAttributeString("Name");

					tEob pSubEntity = b.GetSubMeshEntity(sName);
					if(pSubEntity==0)
					{
						b.WarnMissingSubMesh(sName);
						continue;
					}
					if(bHasSkeleton==false)
						lstEntities.push_back(pSubEntity);

					if(bHasSkeleton==false)
					{
						cVector3f vSubScale = pSubMeshElem->GetAttributeVector3f("Scale");
						if(f.mbSubMeshScaleIncludesModelScale)
						{
							cVector3f vModelScale = b.GetSubMeshModelScale(pSubEntity);
							for(int i=0; i<3; ++i) if(vModelScale.v[i] != 0) vSubScale.v[i] /= vModelScale.v[i];
						}
						cMatrixf mtxLocalTransform = f.mbSubMeshInstanceScaleAfterRotation ?
							cMath::MatrixMul(cMath::MatrixScale(mvScale), GetMatrixFromVectors(	pSubMeshElem->GetAttributeVector3f("WorldPos"),
																								pSubMeshElem->GetAttributeVector3f("Rotation"),
																								vSubScale)) :
							GetMatrixFromVectors(	pSubMeshElem->GetAttributeVector3f("WorldPos")*mvScale,
													pSubMeshElem->GetAttributeVector3f("Rotation"),
													vSubScale*mvScale);

						b.SetWorldMatrix(pSubEntity, mtxLocalTransform);
					}

					int lID = pSubMeshElem->GetAttributeInt("ID",-1);
					if(lID < 0) lID = pSubMeshElem->GetAttributeInt("SubMeshID");

					b.SetUniqueID(pSubEntity, lID);

					tString sMaterial = f.mbSubMeshMaterials ? pSubMeshElem->GetAttributeString("Material") : "";
					if(cString::GetFileExt(sMaterial) != "")
						b.SetSubMeshMaterial(pSubEntity, sMaterial);
				}
			}

			cXmlElement *pAnimationsElem = pModelDataElem->GetFirstElement("Animations");
			if(f.mbAnimations && mEnt && pAnimationsElem)
			{
				cXmlNodeListIterator animElemIt = pAnimationsElem->GetChildIterator();
				while(animElemIt.HasNext())
				{
					cXmlElement *pAnimElem = animElemIt.Next()->ToElement();

					tString sFile = pAnimElem->GetAttributeString("File");
					tString sName = pAnimElem->GetAttributeString("Name");
					float fSpeed = pAnimElem->GetAttributeFloat("Speed",1.0f);
					float fSpecialEventTime = pAnimElem->GetAttributeFloat("SpecialEventTime",0.0f);

					if(cString::GetFilePath(sFile).length() <= 1)
						sFile = cString::SetFilePath(sFile, p.msFullPathDir);

					tEob pState = b.AddAnimation(sFile, sName, fSpeed);

					if(pState)
					{
						b.SetSpecialEventTime(pState, fSpecialEventTime);

						cXmlNodeListIterator eventElemIt = pAnimElem->GetChildIterator();
						while(eventElemIt.HasNext())
						{
							cXmlElement *pEventElem = eventElemIt.Next()->ToElement();

							float fTime = pEventElem->GetAttributeFloat("Time");
							int lType = ToAnimEventType(b, pEventElem->GetAttributeString("Type"));
							b.AddAnimationEvent(pState, fTime, lType, pEventElem->GetAttributeString("Value"),
												pEventElem->GetAttributeString("Name"), pEventElem->GetAttributeString("DestSocket"));
						}
					}
				}
			}

			{
				cXmlElement *pEntitiesElem = pModelDataElem->GetFirstElement("Entities");
				if(pEntitiesElem)
				{
					tString sPrefix = p.msName + "_";
					cXmlNodeListIterator entityIt = pEntitiesElem->GetChildIterator();
					while(entityIt.HasNext())
					{
						cXmlElement *pEntityElem = entityIt.Next()->ToElement();
						const tString& sEntityType = pEntityElem->GetValue();
						tEob pEntity = 0;

						if(sEntityType == "ParticleSystem")
						{
							if(f.mbParticleSystems) pEntity = b.LoadWorldEntity(eEobWorldEntity_ParticleSystem, pEntityElem, sPrefix);
						}
						else if(sEntityType == "Billboard")
						{
							if(f.mbBillboards) pEntity = b.LoadWorldEntity(eEobWorldEntity_Billboard, pEntityElem, sPrefix);
						}
						else if(sEntityType == "LensFlare")
						{
							pEntity = b.LoadWorldEntity(eEobWorldEntity_LensFlare, pEntityElem, sPrefix);
						}
						else if(sEntityType == "Sound")
						{
							if(f.mbSounds) pEntity = b.LoadWorldEntity(eEobWorldEntity_Sound, pEntityElem, sPrefix);
						}
						else if(cString::GetLastStringPos(sEntityType,"Light")>0)
						{
							if(f.mbLights) pEntity = b.LoadWorldEntity(eEobWorldEntity_Light, pEntityElem, sPrefix);
						}
						else
						{
							b.Error(Fmt("Entity world entity type '%s' is unknown!\n", sEntityType.c_str()));
						}

						if(pEntity)
						{
							cVector3f vPos = b.GetLocalPosition(pEntity);
							b.SetPosition(pEntity, vPos * mvScale);

							lstEntities.push_back(pEntity);
						}
					}
				}

				b.LinkLightBillboards();
			}

			std::list<tEob> lstTempEntities = lstEntities;

			std::map<int,tEob> mapBoneStates;
			if(mbSkeleton)
			{
				cXmlElement *pBonesElem = pModelDataElem->GetFirstElement("Bones");
				if(pBonesElem)
				{
					cXmlNodeListIterator boneIt = pBonesElem->GetChildIterator();
					while(boneIt.HasNext())
					{
						cXmlElement *pBoneElem = boneIt.Next()->ToElement();

						int lID = pBoneElem->GetAttributeInt("ID");
						tString sName = pBoneElem->GetAttributeString("Name");

						tEob pBoneState = b.GetBoneState(sName);
						if(pBoneState==0){
							b.Error(Fmt("Could not find bone '%s' in model '%s'\n", sName.c_str(), p.msFullPath.c_str()));
							continue;
						}

						mapBoneStates.insert(std::map<int,tEob>::value_type(lID, pBoneState));

						AttachChildren(pBoneElem, 0, pBoneState, lstTempEntities, mapBoneStates, true, false);
					}
				}
			}

			tShapeMap mapShapes;
			std::set<tEob> setUsedShapes;

			if(p.mbPhysicsWorld)
			{
				cXmlElement *pShapesElem = pModelDataElem->GetFirstElement("Shapes");
				if(pShapesElem)
				{
					cXmlNodeListIterator shapeIt = pShapesElem->GetChildIterator();
					while(shapeIt.HasNext())
					{
						cXmlElement *pBodyShapeElem = shapeIt.Next()->ToElement();

						tEob pCollideShape = CreateShape(b, pBodyShapeElem, mvScale);
						if(pCollideShape == 0) continue;
						int lID = pBodyShapeElem->GetAttributeInt("ID");

						mapShapes.insert(tShapeMap::value_type(lID, pCollideShape));
					}
				}
			}

			tBodyMap mapBodies;
			std::vector<tEob> vBodies;

			if(p.mbPhysicsWorld)
			{
				cXmlElement *pBodiesElem = pModelDataElem->GetFirstElement("Bodies");
				if(pBodiesElem)
				{
					cXmlNodeListIterator bodyIt = pBodiesElem->GetChildIterator();
					while(bodyIt.HasNext())
					{
						cXmlElement *pBodyElem = bodyIt.Next()->ToElement();
						int lID = pBodyElem->GetAttributeInt("ID");
						tString sBodyName = pBodyElem->GetAttributeString("Name");

						tEob pShape = GetBodyShape(b, pBodyElem, mapShapes, setUsedShapes, f.mbShareBodyShapes);
						if(pShape==0){
							b.Error(Fmt("No shapes found for body '%s'\n", sBodyName.c_str()));
							continue;
						}

						tEob pBody = b.CreateBody(p.msName +"_"+ sBodyName, pShape);
						BodyProperties(pBody, pBodyElem);

						b.SetBodyMaterial(pBody, pBodyElem->GetAttributeString("Material"));

						mapBodies.insert(tBodyMap::value_type(lID, pBody));
						vBodies.push_back(pBody);
						b.RegisterBody(pBody);

						AttachChildren(pBodyElem, pBody, 0, lstTempEntities, mapBoneStates, true, true);
					}
				}

				if(mbSkeleton)
				{
					if(mapBoneStates.size() != (size_t)b.GetBoneStateNum())
						b.WarnSkeletonMismatch();
					lstTempEntities.push_back(mEnt);
				}

				for(tShapeMap::iterator shapeSetIt = mapShapes.begin(); shapeSetIt != mapShapes.end(); ++shapeSetIt)
				{
					if(setUsedShapes.count(shapeSetIt->second)==0)
						b.DestroyShape(shapeSetIt->second);
				}
			}

			if(vBodies.empty()==false)
			{
				tEob pMainBody = vBodies[0];
				cMatrixf mtxInvParent = cMath::MatrixInverse(b.GetLocalMatrix(pMainBody));

				for(std::list<tEob>::iterator it = lstTempEntities.begin(); it != lstTempEntities.end(); ++it)
				{
					tEob pEntity = *it;

					if(b.IsSubMesh(pEntity) && mbSkeleton == false && mlAnimNum > 0)
					{
						mbNodeAnimation = true;
						b.SetNodeAnimation();
						continue;
					}
					else
					{
						b.AttachEntityChild(pMainBody, mtxInvParent, pEntity);
					}
				}

				if(mbNodeAnimation)
					b.AttachEntityChild(pMainBody, mtxInvParent, mEnt);
			}

			if(vBodies.size()<=0)
			{
				if(mbSkeleton)
					b.SetMatrix(mEnt, cMath::MatrixMul(p.m_mtxTransform, cMath::MatrixScale(mvScale)) );
				else
					b.SetMatrix(mEnt, p.m_mtxTransform);

				b.UpdateLogic(mEnt);
			}
			else
			{
				for(size_t i=0;i<vBodies.size(); ++i)
				{
					tEob pBody = vBodies[i];
					b.SetMatrix(pBody, cMath::MatrixMul(p.m_mtxTransform, b.GetLocalMatrix(pBody)));
				}
			}

			std::map<tEob, cMatrixf> mapRestMatrix;
			for(size_t i=0; i<vBodies.size(); ++i)
				mapRestMatrix[vBodies[i]] = b.GetWorldMatrix(vBodies[i]);
			bool bPosed = b.BeforeJoints();
			if(!bPosed)
				mapRestMatrix.clear();

			if(p.mbPhysicsWorld)
			{
				cXmlElement *JointsElem = pModelDataElem->GetFirstElement("Joints");
				if(JointsElem)
				{
					cXmlNodeListIterator jointIt = JointsElem->GetChildIterator();
					while(jointIt.HasNext())
					{
						cXmlElement *pJointElem = jointIt.Next()->ToElement();

						tEob pJoint = CreateJoint(pJointElem, mapBodies, mapRestMatrix);
						if(pJoint)
						{
							JointProperties(pJoint, pJointElem);
							b.RegisterJoint(pJoint);
						}
					}
				}
			}

			for(std::list<tEob>::iterator it = lstEntities.begin(); it != lstEntities.end(); ++it)
				b.SetUniqueID(*it, -1);

			if(b.GetAnimationStateNum() >0 && !bPosed)
				b.PlayFirstAnimation();
			else if(mbSkeleton && vBodies.size() >0)
				b.SetSkeletonPhysicsActive();

			b.LoadUserVariables();
			b.AfterLoad();

			return true;
		}
	};

	}

	bool ParseEntityObject(iEntityObjectBuilder &aBuilder, const cEobParams &aParams, cXmlElement *apRootElem)
	{
		cParse parse(aBuilder, aParams);
		return parse.Run(apRootElem);
	}

};
