#include "SomaLoaders.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"

#include "resources/WorldLoaderHpm.h"

cSomaGenericEntityLoader::cSomaGenericEntityLoader(const tString &asName) : cEntityLoader_Object(asName)
{
	mbLoadAsStatic = true;
	mbCreatesStaticEntity = true;
}

extern tString gsSomaSpawnName;

// Static meshes are never animated or moved by HPL2's world update
void cSomaGenericEntityLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	bool bDynamic = gsSomaSpawnName != "";
	cXmlElement *pModel = apRootElem->GetFirstElement("ModelData");
	cXmlElement *pAnims = pModel ? pModel->GetFirstElement("Animations") : NULL;
	if (pAnims && pAnims->GetFirstElement("Animation"))
		bDynamic = true;
	cXmlElement *pProcAnims = pModel ? pModel->GetFirstElement("ProcAnimations") : NULL;
	if (pProcAnims && pProcAnims->GetFirstElement("Animation"))
		bDynamic = true;
	cXmlElement *pBodies = pModel ? pModel->GetFirstElement("Bodies") : NULL;
	if (pBodies)
	{
		cXmlNodeListIterator it = pBodies->GetChildIterator();
		while (it.HasNext() && bDynamic == false)
			if (cXmlElement *pBody = it.Next()->ToElement())
				bDynamic = pBody->GetAttributeFloat("Mass", 0) > 0;
	}
	mbLoadAsStatic = bDynamic == false;
}

// UID="a b c" of the element being created (the map's hpl::cID)
static cSomaID ElementID()
{
	cSomaID id;
	cXmlElement *pElem = cWorldLoaderHpm::GetCurrentElement();
	if (pElem)
	{
		tIntVec v;
		cString::GetIntVec(pElem->GetAttributeString("UID", ""), v, NULL);
		if (v.size() == 3)
		{
			id.mA = (uint8_t)v[0];
			id.mB = v[1];
			id.mC = v[2];
		}
		else
			id.mC = pElem->GetAttributeInt("ID", 0);
	}
	return id;
}

static void LoadInstanceVars(cResourceVarsObject &aVars)
{
	cXmlElement *pElem = cWorldLoaderHpm::GetCurrentElement();
	cXmlElement *pVars = pElem ? pElem->GetFirstElement("UserVariables") : NULL;
	if (pVars)
		aVars.LoadVariables(pVars);
}

static eSomaLuxEntityType TypeFromEntityType(const tString &asType)
{
	if (asType.compare(0, 6, "Agent_") == 0)
		return eSomaLuxEntityType_Agent;
	if (asType.compare(0, 8, "Critter_") == 0 || asType.compare(0, 8, "critter_") == 0)
		return eSomaLuxEntityType_Critter;
	return eSomaLuxEntityType_Prop;
}

static void CreateAreaEntity(const tString &asName, const tString &asType, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform)
{
	cSomaLuxEntity *pEnt = new cSomaLuxEntity();
	pEnt->msName = asName;
	pEnt->msClassName = asType;
	pEnt->meType = asType == "Liquid" ? eSomaLuxEntityType_LiquidArea : eSomaLuxEntityType_Area;
	pEnt->mID = ElementID();
	pEnt->mbActive = abActive;
	pEnt->mvSize = avSize;
	pEnt->m_mtxOnLoad = a_mtxTransform;
	LoadInstanceVars(pEnt->mInstanceVars);
	cSomaLuxEntity::Pending().push_back(pEnt);
}

// Name of an entity created at runtime by cLuxMap::CreateEntity
tString gsSomaSpawnName;

static void LoadSockets(cXmlElement *apElem, const tString &asBone, cSomaLuxEntity *apEnt)
{
	cXmlNodeListIterator it = apElem->GetChildIterator();
	while (it.HasNext())
	{
		cXmlElement *pChild = it.Next()->ToElement();
		if (pChild == NULL)
			continue;
		tString sValue = pChild->GetValue();
		if (sValue == "Socket")
		{
			cMatrixf mtxSocket = cMath::MatrixRotate(pChild->GetAttributeVector3f("Rotation", 0), eEulerRotationOrder_XYZ);
			mtxSocket.SetTranslation(pChild->GetAttributeVector3f("WorldPos", 0));
			cSomaLuxEntity::cSocket sock{pChild->GetAttributeString("Name", ""), NULL, mtxSocket};
			cSkeleton *pSkel = apEnt->mpMesh && apEnt->mpMesh->GetMesh() ? apEnt->mpMesh->GetMesh()->GetSkeleton() : NULL;
			for (const tString &sBone : {pChild->GetAttributeString("SourceBoneName", ""), asBone})
			{
				cBone *pBone = pSkel && sBone != "" ? pSkel->GetBoneByName(sBone) : NULL;
				if (pBone == NULL)
					continue;
				sock.mpBone = apEnt->mpMesh->GetBoneStateFromName(sBone);
				sock.m_mtxOffset = cMath::MatrixMul(cMath::MatrixInverse(pBone->GetWorldTransform()), mtxSocket);
				break;
			}
			apEnt->mvSockets.push_back(sock);
		}
		else
			LoadSockets(pChild, sValue == "Bone" ? pChild->GetAttributeString("Name", "") : asBone, apEnt);
	}
}

static void LoadProcAnimations(cXmlElement *apElem, cSomaLuxEntity *apEnt)
{
	cXmlNodeListIterator it = apElem->GetChildIterator();
	while (it.HasNext())
	{
		cXmlElement *pAnimElem = it.Next()->ToElement();
		if (pAnimElem == NULL)
			continue;
		cSomaLuxEntity::cProcAnim anim;
		anim.msName = pAnimElem->GetAttributeString("Name", "");
		cXmlNodeListIterator trackIt = pAnimElem->GetChildIterator();
		while (trackIt.HasNext())
		{
			cXmlElement *pTrack = trackIt.Next()->ToElement();
			cSubMeshEntity *pSub = pTrack ? apEnt->mpMesh->GetSubMeshEntityName(pTrack->GetAttributeString("SubMesh", "")) : NULL;
			if (pSub == NULL)
				continue;
			tString sAxes = pTrack->GetAttributeString("Axes", "");
			cVector3f vAxes(sAxes.find('X') != tString::npos, sAxes.find('Y') != tString::npos, sAxes.find('Z') != tString::npos);
			anim.mvTracks.push_back({pSub, pSub->GetLocalMatrix(), pTrack->GetAttributeString("Type", "") == "rotate", pTrack->GetAttributeBool("ReverseMotion", false), vAxes,
									 pTrack->GetAttributeFloat("OffsetMin", 0), pTrack->GetAttributeFloat("OffsetMax", 0), pTrack->GetAttributeInt("Cycles", 1),
									 pTrack->GetAttributeString("Easing", "")});
		}
		apEnt->mvProcAnims.push_back(anim);
	}
}

// Before joints so they are created in the posed frame. Map placements carry a bone pose: per bone quaternion (w x y z) + translation in FBX units
void cSomaGenericEntityLoader::BeforeJoints()
{
	cXmlElement *pElem = cWorldLoaderHpm::GetCurrentElement();
	cXmlElement *pPose = pElem ? pElem->GetFirstElement("Pose") : NULL;
	cSkeleton *pSkeleton = mpEntity && mpEntity->GetMesh() ? mpEntity->GetMesh()->GetSkeleton() : NULL;
	if (pPose == NULL || pSkeleton == NULL) return;

	tFloatVec vVals;
	cString::GetFloatVec(pPose->GetAttributeString("_Text"), vVals, NULL);
	if ((int)vVals.size() != pSkeleton->GetBoneNum() * 7) return;

	std::vector<cMatrixf> vLocal(pSkeleton->GetBoneNum());
	for (int i = 0; i < pSkeleton->GetBoneNum(); ++i)
	{
		const float *v = &vVals[i * 7];
		cBone *pBone = pSkeleton->GetBoneByIndex(i);
		vLocal[i] = cMath::MatrixQuaternion(cQuaternion(v[0], v[1], v[2], v[3]));
		// Unit-scaled roots (subway girls) carry a scale the pose omits
		const cMatrixf &mtxRest = pBone->GetLocalTransform();
		vLocal[i] = cMath::MatrixMul(vLocal[i], cMath::MatrixScale(cVector3f(mtxRest.GetRight().Length(), mtxRest.GetUp().Length(), mtxRest.GetForward().Length())));
		vLocal[i].SetTranslation(cVector3f(v[4], v[5], v[6]) * pBone->GetLocalUnitScale());
	}
	mpEntity->SetBoneRestPose(vLocal);
	mpEntity->AlignBodiesToSkeleton(false);
}

void cSomaGenericEntityLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	// HPL2 only attaches these to bodies; HPL3 parents them to the mesh entity otherwise
	if (mpEntity && mvBodies.empty())
	{
		std::vector<iEntity3D *> vChildren(mvLights.begin(), mvLights.end());
		vChildren.insert(vChildren.end(), mvBillboards.begin(), mvBillboards.end());
		vChildren.insert(vChildren.end(), mvLensFlares.begin(), mvLensFlares.end());
		vChildren.insert(vChildren.end(), mvParticleSystems.begin(), mvParticleSystems.end());
		vChildren.insert(vChildren.end(), mvSoundEntities.begin(), mvSoundEntities.end());
		// The mesh entity carries the scale there, on top of the loader's scaled offsets
		for (iEntity3D *pChild : vChildren)
			if (pChild->GetEntityParent() == NULL && pChild->GetParent() == NULL)
			{
				pChild->SetPosition(pChild->GetLocalPosition() * mvScale);
				mpEntity->AddChild(pChild);
			}
	}
	float fMeanScale = (mvScale.x + mvScale.y + mvScale.z) / 3.0f;
	for (iLight *pLight : mvLights)
		pLight->SetRadius(pLight->GetRadius() * fMeanScale);
	for (cBillboard *pBB : mvBillboards)
		pBB->SetSize(pBB->GetSize() * fMeanScale);

	if (cWorldLoaderHpm::GetCurrentElement() || gsSomaSpawnName != "")
	{
		cSomaLuxEntity *pEnt = new cSomaLuxEntity();
		pEnt->msName = cWorldLoaderHpm::GetCurrentElement() ? cWorldLoaderHpm::GetCurrentElement()->GetAttributeString("Name", "") : gsSomaSpawnName;
		pEnt->msClassName = msEntityType;
		pEnt->msFileName = msFileName;
		pEnt->meType = TypeFromEntityType(msEntityType);
		pEnt->mID = ElementID();
		pEnt->mbActive = mbActive;
		pEnt->m_mtxOnLoad = a_mtxTransform;
		pEnt->mvScale = mvScale;
		pEnt->mpMesh = mpEntity;
		if (mpEntity)
			mpEntity->SetCallback(pEnt);
		if (mpEntity && mpEntity->GetAnimationStateNum() > 0)
		{
			cAnimationState *pAnim = mpEntity->GetAnimationState(0);
			if (pEnt->meType == eSomaLuxEntityType_Prop && GetVarBool("RandomizeAnimationStart", true))
				pAnim->SetTimePosition(cMath::RandRectf(0, pAnim->GetLength()));
			pEnt->mlCurrentAnim = 0;
		}
		pEnt->mbShowMesh = GetVarBool("ShowMesh", msEntityType != "StaticCollider");
		pEnt->mvBodies = mvBodies;
		for (iPhysicsBody *pBody : mvBodies)
			pEnt->mvDefaultCollideCharacter.push_back(pBody->GetCollideCharacter());
		pEnt->mvBodyExtraData = mvBodyExtraData;
		if (apInstanceVars)
			pEnt->mbEffectsActive = apInstanceVars->GetVarBool("EffectsActive", true);
		if (apInstanceVars && apInstanceVars->GetVarBool("StaticPhysics", false))
			pEnt->SetStaticPhysics(true);
		// cLuxPropLoader::AfterLoad: gravity-free first steps let Newton put resting props to sleep
		if (pEnt->meType == eSomaLuxEntityType_Prop && mbActive && !GetVarBool("DisableFreezeAtStart", false) && !(mpEntity && mpEntity->GetSkeletonPhysicsActive()))
			for (iPhysicsBody *pBody : mvBodies)
				if (pBody->GetMass() > 0)
					pBody->Freeze();
		if (pEnt->meType == eSomaLuxEntityType_Prop && GetVarBool("NoGravityWhenUnderwater", false))
			for (iPhysicsBody *pBody : mvBodies)
				pBody->SetNoGravityWhenUnderwater(true);
		if (apInstanceVars)
			if (unsigned int lFlags = SomaCollideFlag(apInstanceVars->GetVarString("CollideGroup", "")))
				for (iPhysicsBody *pBody : mvBodies)
					pBody->SetCollideFlags(lFlags);
		pEnt->mvJoints = mvJoints;
		pEnt->mvLights = mvLights;
		pEnt->mvParticleSystems = mvParticleSystems;
		pEnt->mvBillboards = mvBillboards;
		pEnt->mvLensFlares = mvLensFlares;
		pEnt->mvSoundEntities = mvSoundEntities;
		pEnt->mVars.LoadVariables(apRootElem->GetFirstElement("UserDefinedVariables"));
		tString sMainBody = pEnt->mVars.GetVarString("MainPhysicsBody", "");
		for (iPhysicsBody *pBody : mvBodies)
			if (pBody->GetName() == pEnt->msName + "_" + sMainBody)
				pEnt->mpMainBody = pBody;
		if (sMainBody != "" && pEnt->mpMainBody == NULL)
			Warning("Could not find main physics body '%s'\n", pEnt->msName.c_str());
		if (cXmlElement *pModel = apRootElem->GetFirstElement("ModelData"))
		{
			LoadSockets(pModel, "", pEnt);
			cXmlElement *pProcAnims = pModel->GetFirstElement("ProcAnimations");
			if (pProcAnims && mpEntity)
				LoadProcAnimations(pProcAnims, pEnt);
		}
		if (mpEntity)
			for (int i = 0; i < mpEntity->GetBoneStateNum(); ++i)
			{
				cBoneState *pBone = mpEntity->GetBoneState(i);
				if (cString::ToLowerCase(cString::Sub(pBone->GetName(), 0, 7)) == "socket_")
					pEnt->mvSockets.push_back({cString::Sub(pBone->GetName(), 7), pBone, cMatrixf::Identity});
			}
		LoadInstanceVars(pEnt->mInstanceVars);
		cSomaLuxEntity::Pending().push_back(pEnt);
	}

	if (apInstanceVars && mpEntity)
	{
		mpEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, apInstanceVars->GetVarBool("CastShadows", true));
		mpEntity->SetIlluminationAmount(apInstanceVars->GetVarFloat("IllumBrightness", 1));
		mpEntity->SetColorMul(apInstanceVars->GetVarColor("ColorMul", cColor(1, 1)));
	}
	if (apInstanceVars)
	{
		cColor effectMul = apInstanceVars->GetVarColor("EffectColorMul", cColor(1, 1));
		float fBrightnessMul = apInstanceVars->GetVarFloat("EffectBrightnessMul", 1);
		for (iLight *pLight : mvLights)
		{
			cColor col = pLight->GetDiffuseColor();
			col = cColor(col.r * effectMul.r, col.g * effectMul.g, col.b * effectMul.b, col.a);
			pLight->SetDiffuseColor(col);
			pLight->SetDefaultDiffuseColor(col);
			pLight->SetBrightness(pLight->GetBrightness() * fBrightnessMul);
		}
		for (cBillboard *pBB : mvBillboards)
			pBB->SetColor(pBB->GetColor() * effectMul);
	}

	// ShowMesh=false marks collision-only meshes; StaticCollider has no such var but is never visible
	if (mpEntity)
	{
		bool bDefaultShowMesh = (msEntityType != "StaticCollider");
		mpEntity->SetVisible(GetVarBool("ShowMesh", bDefaultShowMesh));
	}

	// Map Active="false": script-activated later (e.g. the apartment's Legs), as iLuxProp::OnSetActive.
	if (mbActive) return;
	for (iPhysicsBody *pBody : mvBodies)
		pBody->SetActive(false);
	if (mpEntity)
	{
		mpEntity->SetActive(false);
		mpEntity->SetVisible(false);
	}
	for (size_t i = 0; i < mvLights.size(); ++i)
	{
		mvLights[i]->SetVisible(false);
		mvLights[i]->SetActive(false);
	}
	for (size_t i = 0; i < mvParticleSystems.size(); ++i)
	{
		if (mvParticleSystems[i] == NULL) continue;
		mvParticleSystems[i]->SetVisible(false);
		mvParticleSystems[i]->SetActive(false);
	}
	for (size_t i = 0; i < mvBillboards.size(); ++i)
	{
		mvBillboards[i]->SetActive(false);
		mvBillboards[i]->SetVisible(false);
	}
	for (cLensFlare *pFlare : mvLensFlares)
	{
		pFlare->SetActive(false);
		pFlare->SetVisible(false);
	}
	for (size_t i = 0; i < mvBeams.size(); ++i)
	{
		mvBeams[i]->SetActive(false);
		mvBeams[i]->SetVisible(false);
	}
	for (size_t i = 0; i < mvSoundEntities.size(); ++i) mvSoundEntities[i]->Stop(false);
}

cSomaAreaLoader_PlayerStart::cSomaAreaLoader_PlayerStart(const tString &asName) : iAreaLoader(asName)
{
	mbCreatesStaticArea = true;
}

void cSomaAreaLoader_PlayerStart::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld)
{
	cStartPosEntity *pStartPos = apWorld->CreateStartPos(asName);
	pStartPos->SetMatrix(a_mtxTransform);
	cResourceVarsObject vars;
	LoadInstanceVars(vars);
	pStartPos->mbCrouching = vars.GetVarBool("Crouching", false);
}

cSomaAreaLoader_Noop::cSomaAreaLoader_Noop(const tString &asName) : iAreaLoader(asName)
{
	mbCreatesStaticArea = true;
}

void cSomaAreaLoader_Noop::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld)
{
	CreateAreaEntity(asName, GetName(), abActive, avSize, a_mtxTransform);
}

void RegisterSomaLoaders(cResources *apResources)
{
	static const char* apEntityTypeNames[] = {
		"Agent_Anglerfish",
		"Agent_Construct_Crawler",
		"Agent_Construct_Worker",
		"Agent_DeepseaSuit",
		"Agent_Flesher",
		"Agent_Humanoid",
		"Agent_Humanoid_NPC",
		"Agent_Infected_Robot",
		"Agent_Puppet",
		"Agent_Remade",
		"Agent_Roomba",
		"Agent_Swarm",
		"Agent_SwimBot",
		"Agent_Viperfish",
		"Critter_CaveSpider",
		"Critter_CrabSmall",
		"Critter_CrabSpider",
		"Critter_FishSmall",
		"Critter_Nautilus",
		"critter_wau_swarm_agent_fish",
		"Prop_Button",
		"Prop_ConstructLure",
		"Prop_Datamine",
		"Prop_EnergySource",
		"Prop_Grab",
		"Prop_HandheldTerminal",
		"Prop_HudObject",
		"Prop_Lamp",
		"Prop_LevelDoor",
		"Prop_Lever",
		"Prop_Meter",
		"Prop_MoveObject",
		"Prop_MovingButton",
		"Prop_OmniSlot",
		"Prop_PhysicsSlideDoor",
		"Prop_PlayerHands",
		"Prop_Push",
		"Prop_Readable",
		"Prop_Rigid",
		"Prop_Slide",
		"Prop_SlideDoor",
		"Prop_SwingDoor",
		"Prop_Tear",
		"Prop_Terminal",
		"Prop_Tool",
		"Prop_Wheel",
		"StaticCollider",
		"StaticProp",
	};
	for (size_t i = 0; i < sizeof(apEntityTypeNames) / sizeof(apEntityTypeNames[0]); ++i)
	{
		apResources->AddEntityLoader(hplNew(cSomaGenericEntityLoader, (apEntityTypeNames[i])));
	}

	apResources->AddAreaLoader(hplNew(cSomaAreaLoader_PlayerStart, ("PlayerStart")));

	static const char* apNoopAreaTypeNames[] = {
		"AgentRepel",
		"AmbientLight",
		"CameraAnimation",
		"Climb",
		"Crawl",
		"Datamine",
		"Description",
		"Distortion",
		"DoorwayTrigger",
		"EyeTrackingZoom",
		"Hide",
		"InteractAux",
		"Ladder",
		"Liquid",
		"MapTransfer",
		"PathNode",
		"Sit",
		"Soundscape",
		"Sticky",
		"Tool",
		"Trigger",
		"VisibilityArea",
		"VisibilityPortal",
		"Zoom",
	};
	for (size_t i = 0; i < sizeof(apNoopAreaTypeNames) / sizeof(apNoopAreaTypeNames[0]); ++i)
	{
		apResources->AddAreaLoader(hplNew(cSomaAreaLoader_Noop, (apNoopAreaTypeNames[i])));
	}
}
