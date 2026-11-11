#include "SomaLoaders.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "RustSomaUi.h"

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
	cXmlElement *pModel = apRootElem->GetFirstElement("ModelData");
	cXmlElement *pAnims = pModel ? pModel->GetFirstElement("Animations") : NULL;
	cXmlElement *pProcAnims = pModel ? pModel->GetFirstElement("ProcAnimations") : NULL;
	cXmlElement *pBodies = pModel ? pModel->GetFirstElement("Bodies") : NULL;
	std::vector<float> vMasses;
	if (pBodies)
	{
		cXmlNodeListIterator it = pBodies->GetChildIterator();
		while (it.HasNext())
			if (cXmlElement *pBody = it.Next()->ToElement())
				vMasses.push_back(pBody->GetAttributeFloat("Mass", 0));
	}
	bool bDynamic = ohpl_gs7_load_is_dynamic(gsSomaSpawnName != "", pAnims && pAnims->GetFirstElement("Animation"), pProcAnims && pProcAnims->GetFirstElement("Animation"),
											 vMasses.data(), vMasses.size());
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
		OhplGs7ElementId r;
		ohpl_gs7_load_element_id(v.data(), v.size(), pElem->GetAttributeInt("ID", 0), &r);
		id.mA = r.a;
		id.mB = r.b;
		id.mC = r.c;
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
	switch (ohpl_gs7_load_kind((const unsigned char *)asType.data(), asType.size()))
	{
	case 1: return eSomaLuxEntityType_Agent;
	case 2: return eSomaLuxEntityType_Critter;
	}
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
bool cSomaGenericEntityLoader::BeforeJoints()
{
	cXmlElement *pElem = cWorldLoaderHpm::GetCurrentElement();
	cXmlElement *pPose = pElem ? pElem->GetFirstElement("Pose") : NULL;
	cSkeleton *pSkeleton = mpEntity && mpEntity->GetMesh() ? mpEntity->GetMesh()->GetSkeleton() : NULL;
	if (ohpl_gs7_load_before_joints(pPose != NULL, pSkeleton != NULL) == false) return false;

	tFloatVec vVals;
	cString::GetFloatVec(pPose->GetAttributeString("_Text"), vVals, NULL);
	if (ohpl_gs7_load_align_bodies(mpEntity->SetBoneRestPoseFromMapValues(vVals)))
		mpEntity->AlignBodiesToSkeleton(false);
	return true;
}

void cSomaGenericEntityLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	// HPL2 only attaches these to bodies; HPL3 parents them to the mesh entity otherwise
	if (ohpl_gs7_load_reparent_children(mpEntity != NULL, mvBodies.size()))
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
			{
				cVector3f vLocal = pChild->GetLocalPosition(), vOffset;
				ohpl_gs7_load_scaled_offset(vLocal.v, mvScale.v, vOffset.v);
				pChild->SetPosition(vOffset);
				mpEntity->AddChild(pChild);
			}
			}
	}
	float fMeanScale = ohpl_gs7_load_mean_scale(mvScale.v);
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
		if (mpEntity && mpEntity->GetAnimationStateNum() > 0 && mpEntity->GetAnimationState(0)->IsActive())
		{
			cAnimationState *pAnim = mpEntity->GetAnimationState(0);
			if (ohpl_gs7_load_random_anim_start(pEnt->meType == eSomaLuxEntityType_Prop, GetVarBool("RandomizeAnimationStart", true)))
				pAnim->SetTimePosition(cMath::RandRectf(0, pAnim->GetLength()));
			pEnt->mlCurrentAnim = 0;
		}
		pEnt->mbShowMesh = GetVarBool("ShowMesh", ohpl_gs7_load_default_show_mesh((const unsigned char *)msEntityType.data(), msEntityType.size()));
		pEnt->mvBodies = mvBodies;
		for (iPhysicsBody *pBody : mvBodies)
			pEnt->mvDefaultCollideCharacter.push_back(pBody->GetCollideCharacter());
		pEnt->mvBodyExtraData = mvBodyExtraData;
		if (apInstanceVars)
			pEnt->mbEffectsActive = apInstanceVars->GetVarBool("EffectsActive", true);
		if (apInstanceVars && (apInstanceVars->GetVarBool("Static", false) || apInstanceVars->GetVarBool("StaticPhysics", false)))
			pEnt->SetStaticPhysics(true);
		// cLuxPropLoader::AfterLoad: gravity-free first steps let Newton put resting props to sleep
		if (ohpl_gs7_load_freeze_props(pEnt->meType == eSomaLuxEntityType_Prop, mbActive, GetVarBool("DisableFreezeAtStart", false), mpEntity && mpEntity->GetSkeletonPhysicsActive()))
			for (iPhysicsBody *pBody : mvBodies)
				if (pBody->GetMass() > 0)
					pBody->Freeze();
		if (ohpl_gs7_load_no_gravity_underwater(pEnt->meType == eSomaLuxEntityType_Prop, GetVarBool("NoGravityWhenUnderwater", false)))
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
		for (size_t i = 0; i < mvBodies.size() && i < mvBodyExtraData.size(); ++i)
			if (pEnt->GetMainBody() == mvBodies[i])
				pEnt->mvBodyOffset = mvBodyExtraData[i].m_mtxLocalTransform.GetTranslation();
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
			{
				const tString &sBone = pBone->GetName();
				if (ptrdiff_t lOffset = ohpl_gs7_load_socket_offset((const unsigned char *)sBone.data(), sBone.size()); lOffset >= 0)
					pEnt->mvSockets.push_back({cString::Sub(sBone, (int)lOffset), pBone, cMatrixf::Identity});
			}
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
			ohpl_gs7_load_color_mul(col.v, effectMul.v, col.v);
			pLight->SetDiffuseColor(col);
			pLight->SetDefaultDiffuseColor(col);
			pLight->SetBrightness(pLight->GetBrightness() * fBrightnessMul);
			pLight->SetDefaultBrightness(pLight->GetBrightness());
		}
		for (cBillboard *pBB : mvBillboards)
		{
			cColor bbCol = pBB->GetColor();
			ohpl_gs7_load_color_mul_all(bbCol.v, effectMul.v, bbCol.v);
			pBB->SetColor(bbCol);
		}
	}

	// ShowMesh=false marks collision-only meshes; StaticCollider has no such var but is never visible
	if (mpEntity)
	{
		bool bDefaultShowMesh = ohpl_gs7_load_default_show_mesh((const unsigned char *)msEntityType.data(), msEntityType.size());
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
	// types the level editor writes that EntityTypes.cfg doesn't list
	std::set<tString> setEntities = {"StaticCollider", "StaticProp"};
	std::set<tString> setAreas = {"EnvironmentParticleClip", "PathNode", "PathNodeExcluder", "PathNodeGenerator", "PathNodeInterestPoint", "PathNodePatrol", "SoundOcclusion", "SoundOcclusionConnection", "Soundscape", "VisibilityArea", "VisibilityPortal"};
	iXmlDocument *pTypes = apResources->LoadXmlDocument("config/EntityTypes.cfg");
	if (pTypes)
	{
		cXmlNodeListIterator itGroup = pTypes->GetChildIterator();
		while (itGroup.HasNext())
		{
			cXmlElement *pGroup = itGroup.Next()->ToElement();
			if (pGroup == NULL || pGroup->GetValue() == "ComponentsTypes")
				continue;
			bool bArea = cString::GetLastStringPos(pGroup->GetValue(), "AreaTypes") >= 0;
			cXmlNodeListIterator itType = pGroup->GetChildIterator();
			while (itType.HasNext())
				if (cXmlElement *pType = itType.Next()->ToElement())
					(bArea ? setAreas : setEntities).insert(pType->GetAttributeString("Name", ""));
		}
		apResources->DestroyXmlDocument(pTypes);
	}
	for (const tString &sName : setEntities)
		apResources->AddEntityLoader(hplNew(cSomaGenericEntityLoader, (sName)));

	apResources->AddAreaLoader(hplNew(cSomaAreaLoader_PlayerStart, ("PlayerStart")));
	for (const tString &sName : setAreas)
		apResources->AddAreaLoader(hplNew(cSomaAreaLoader_Noop, (sName)));
}
