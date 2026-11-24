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

#include "LuxArea.h"
#include "RustLuxMap.h"

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxProp.h"
#include "LuxBase.h"
#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxHelpFuncs.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxAreaLoader::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform,cWorld *apWorld)
{
	cLuxMap *pMap = gpBase->mpCurrentMapLoading;
	if(pMap==NULL) return;

	iLuxArea *pArea = CreateArea(asName, alID,pMap);

	//////////////////////////////
	// Create and set body
	iPhysicsWorld *pPhysicsWorld = apWorld->GetPhysicsWorld();
	iCollideShape* pShape = pPhysicsWorld->CreateBoxShape(avSize, NULL);
	iPhysicsBody* pBody = pPhysicsWorld->CreateBody(asName,pShape);

	pBody->SetCollide(false);
	pBody->SetCollideCharacter(false);
	pBody->SetMatrix(a_mtxTransform);
	pBody->SetUserData(pArea);
	
	pArea->mpBody = pBody;

	//////////////////////////////
	// Load base properties
	pArea->mvSize = avSize;
	pArea->m_mtxTransform = a_mtxTransform;

	//////////////////
	//Load variables
	LoadVariables(pArea, apWorld);

	//////////////////////////////
	// Load type specific properties
	SetupArea(pArea, apWorld);

	pMap->AddEntity(pArea);

	pArea->SetActive(abActive);

	pArea->SetupAfterLoad(apWorld);
}

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

static int A7Kind(eLuxAreaType aType)
{
	switch(aType)
	{
	case eLuxAreaType_Script: return kOhplA7Script;
	case eLuxAreaType_Flashback: return kOhplA7Flashback;
	case eLuxAreaType_Ladder: return kOhplA7Ladder;
	case eLuxAreaType_Liquid: return kOhplA7Liquid;
	case eLuxAreaType_Sticky: return kOhplA7Sticky;
	case eLuxAreaType_Insanity: return kOhplA7Insanity;
	case eLuxAreaType_Examine: return kOhplA7Examine;
	case eLuxAreaType_Sign: return kOhplA7Sign;
	default: return kOhplA7Slime;
	}
}

static OhplA7Area *A7NewArea(cLuxMap *apMap, eLuxAreaType aType)
{
	return ohpl_a7_area_new(apMap->GetA7(), A7Kind(aType));
}

iLuxArea::iLuxArea(const tString &asName, int alID, cLuxMap *apMap, eLuxAreaType aAreaType)  : iLuxEntity(asName,alID,apMap, eLuxEntityType_Area),
	mAreaType(aAreaType),
	mpA7Area(A7NewArea(apMap, aAreaType)),
	mpA7Base(ohpl_a7_area_base(mpA7Area)),
	mpA7Pod(ohpl_a7_area_pod(mpA7Area)),
	mvSize(*reinterpret_cast<cVector3f*>(&mpA7Base->size)),
	m_mtxTransform(*reinterpret_cast<cMatrixf*>(&mpA7Base->mtx)),
	mvRelativeOffset(*reinterpret_cast<cVector3f*>(&mpA7Base->relOffset)),
	mlBodyId(mpA7Base->bodyId)
{
	mpA7Base->areaType = (int)aAreaType;

	mpBody = NULL;

	mpParentBody = NULL;
	mvRelativeOffset = 0;
}

//-----------------------------------------------------------------------

iLuxArea::~iLuxArea()
{
	if(mpBody)
	{
		GetMap()->GetPhysicsWorld()->DestroyBody(mpBody);
	}

	ohpl_a7_area_free(GetMap()->GetA7(), mpA7Area);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxArea::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
    /*if(mpBody==NULL) return;

	cBoundingVolume* pBV = mpBody->GetBoundingVolume();
	
	apFunctions->GetLowLevelGfx()->DrawBoxMinMax(pBV->GetMin(), pBV->GetMax(),cColor(1,1,1,1));*/
}

//-----------------------------------------------------------------------

bool iLuxArea::CanInteract(iPhysicsBody *apBody)
{
	return false;
}

//-----------------------------------------------------------------------

bool iLuxArea::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return false;
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair iLuxArea::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_LastEnum;
}

//-----------------------------------------------------------------------

iEntity3D* iLuxArea::GetAttachEntity()
{
	return mpBody;
}

//-----------------------------------------------------------------------

void iLuxArea::AttachToBody(const tString& asPropName, int alBodyId)
{
	iLuxEntity *apEntity = GetMap()->GetEntityByName(asPropName);
	if(apEntity && apEntity->GetEntityType() == eLuxEntityType_Prop)
	{
		iLuxProp* pProp = static_cast<iLuxProp*>(apEntity);
		if( pProp->GetBodyNum() > 0 )
		{
			iPhysicsBody* pPropBody = pProp->GetBody(alBodyId);
			if(pPropBody)
			{
				msPropName = asPropName;
				mlBodyId = alBodyId;
				mpParentBody = pPropBody;
				cVector3f vBodyPos = mpBody->GetWorldPosition();
				cVector3f vParentPos = mpParentBody->GetWorldPosition();
				ohpl_a7_area_attach(mpA7Area, reinterpret_cast<const OhplGa5Vec3*>(&vBodyPos), reinterpret_cast<const OhplGa5Vec3*>(&vParentPos));
				return;
			}
		}
	}

	/////////////
	// Failed
	DetachBody();
}

void iLuxArea::DetachBody()
{
	msPropName = "";
	mpParentBody = NULL;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

void iLuxArea::OnSetActive(bool abX)
{	
	///////////////
	//Bodies
	if(mpBody)
		mpBody->SetActive(abX);
}

//-----------------------------------------------------------------------

void iLuxArea::OnUpdate(float afTimeStep)
{
	if(mpParentBody)
	{
		cVector3f vParentPos = mpParentBody->GetWorldPosition();
		cVector3f vBodyPos = mpBody->GetWorldPosition();
		cVector3f vMove;
		ohpl_a7_area_follow(mpA7Area, reinterpret_cast<const OhplGa5Vec3*>(&vParentPos), reinterpret_cast<const OhplGa5Vec3*>(&vBodyPos), reinterpret_cast<OhplGa5Vec3*>(&vMove));
		mpBody->StaticLinearMove(vMove);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kSerializeRustClassV(iLuxArea_SaveData, iLuxEntity_SaveData)


//-----------------------------------------------------------------------

iLuxEntity* iLuxArea_SaveData::CreateEntity(cLuxMap *apMap)
{
	iLuxArea *pArea = CreateArea(apMap);
	apMap->AddEntity(pArea);

	return pArea;
}

//-----------------------------------------------------------------------

void iLuxArea::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);
	iLuxArea_SaveData *pData = static_cast<iLuxArea_SaveData*>(apSaveData);

    kCopyToVar(pData, mvSize);
	kCopyToVar(pData, m_mtxTransform);

	kCopyToVar(pData, mvRelativeOffset);
	kCopyToVar(pData, msPropName);
	kCopyToVar(pData, mlBodyId);
}

//-----------------------------------------------------------------------

void iLuxArea::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);
	iLuxArea_SaveData *pData = static_cast<iLuxArea_SaveData*>(apSaveData);

	kCopyFromVar(pData, mvSize);
	kCopyFromVar(pData, m_mtxTransform);

	kCopyFromVar(pData, mvRelativeOffset);
	kCopyFromVar(pData, msPropName);
	kCopyFromVar(pData, mlBodyId);

	//////////////////////////////
	// Create and set body
	iPhysicsWorld *pPhysicsWorld = GetMap()->GetPhysicsWorld();
	iCollideShape* pShape = pPhysicsWorld->CreateBoxShape(mvSize, NULL);
	iPhysicsBody* pBody = pPhysicsWorld->CreateBody(GetName(),pShape);

	pBody->SetCollide(false);
	pBody->SetCollideCharacter(false);
	pBody->SetMatrix(m_mtxTransform);
	pBody->SetUserData(this);
	pBody->SetActive(IsActive());

	mpBody = pBody;
	
	///////////////////
	//Do setup
	SetupAfterLoad(GetMap()->GetWorld());
}

//-----------------------------------------------------------------------

void iLuxArea::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
	iLuxArea_SaveData *pData = static_cast<iLuxArea_SaveData*>(apSaveData);

	if(msPropName != "")
	{
		cVector3f vOffset = mvRelativeOffset;
		AttachToBody(msPropName, mlBodyId);
		mvRelativeOffset = vOffset;
	}
}


//////////////////////////////////////////////////////////////////////////
// RUST-BACKED AREAS
//////////////////////////////////////////////////////////////////////////

// Member order is the layout rust/src/luxsave/layout_ar.rs mirrors.
class cLuxArea_Liquid_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_Liquid_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_Liquid)); }

	float mfDensity;
	float mfLinearViscosity;
	float mfAngularViscosity;
	tString msPhysicsMaterial;
	bool mbHasWaves;
	float mfWaveAmp;
	float mfWaveFreq;
	float mfPlayerSpeedMul;
	float mfMaxWaveDistanceSqr;
	float mfTimeCount;
};

class cLuxArea_Sticky_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_Sticky_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_Sticky)); }

	tString msAttachFunction;
	tString msDetachFunction;
	tString msAttachSound;
	tString msDetachSound;
	tString msAttachPS;
	tString msDetachPS;
	bool mbCanDetach;
	float mfPoseTime;
	bool mbCheckCenterInArea;
	tString msAttachableBodyName;
	int mlAttachedEntityID;
	int mlAttachedBodyID;
	float mfAttachedBodyMass;
	bool mbAttachedBodyGravity;
	bool mbAttachedEntityFullGameSaved;
	float mfSetMtxTime;
};

class cLuxArea_SlimeDamage_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_SlimeDamage_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_SlimeDamage)); }

	int mlSlimeType;
	float mfMinCheckAttackTime;
	float mfMaxCheckAttackTime;
	bool mbDisableAfterAttack;
	tString msCallback;
	float mfCheckCollisionCount;
	tString msAttackSound;
	tString msAttackPS;
	float mfMinAttackDamage;
	float mfMaxAttackDamage;
	float mfScreenShakeAmount;
};

class cLuxArea_Insanity_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_Insanity_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_Insanity)); }

	bool mbAutoDisable;
};

class cLuxArea_Flashback_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_Flashback_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_Flashback)); }

	tString msFlashbackFile;
	tString msCallback;
};

class cLuxArea_Script_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_Script_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_Script)); }

	bool mbItemInteraction;
};

class cLuxArea_Sign_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_Sign_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_Sign)); }

	tString msTextCat;
	tString msTextEntry;
};

class cLuxArea_Examine_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_Examine_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_Examine)); }

	tString msDescCat;
	tString msDescEntry;
	tString msDescInsaneCat;
	tString msDescInsaneEntry;
	tString msSound;
	tString msInsaneSound;
};

class cLuxArea_Ladder_SaveData : public iLuxArea_SaveData
{
	kSerializableClassInit(cLuxArea_Ladder_SaveData)
public:
	iLuxArea* CreateArea(cLuxMap *apMap){ return hplNew(cLuxAreaRs, (msName, mlID, apMap, eLuxAreaType_Ladder)); }

	tString msMaterial;
};

kSerializeRustClassP(cLuxArea_Script_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Sign_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Examine_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Ladder_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Liquid_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Sticky_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_SlimeDamage_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Insanity_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Flashback_SaveData, iLuxArea_SaveData)

namespace
{
	struct OhplAreaCtx
	{
		iLuxEntity *mpArea;
		iAreaLoader *mpLoader;
		cWorld *mpWorld;
		iXmlDocument *mpDoc;
	};

	// Lent to Rust until the next thunk call; Rust copies at once.
	tString gsLent;

	OhplA7Sv Lend(const tString &asStr)
	{
		gsLent = asStr;
		OhplA7Sv s = { gsLent.data(), gsLent.size() };
		return s;
	}

	OhplAreaCtx *C(void *apCtx){ return static_cast<OhplAreaCtx*>(apCtx); }

	float VarF(void *c, const char *p, size_t n, float d){ return C(c)->mpLoader->GetVarFloat(tString(p, n), d); }
	int VarI(void *c, const char *p, size_t n, int d){ return C(c)->mpLoader->GetVarInt(tString(p, n), d); }
	bool VarB(void *c, const char *p, size_t n, bool d){ return C(c)->mpLoader->GetVarBool(tString(p, n), d); }
	OhplA7Sv VarS(void *c, const char *p, size_t n, const char *d, size_t dn){ return Lend(C(c)->mpLoader->GetVarString(tString(p, n), tString(d, dn))); }

	bool DocOpen(void *c, const char *p, size_t n)
	{
		OhplAreaCtx *pCtx = C(c);
		tString sFile(p, n);
		pCtx->mpDoc = gpBase->mpEngine->GetResources()->LoadXmlDocument(sFile);
		if(pCtx->mpDoc == NULL)
			Error("Could not load '%s' for rope '%s'. Using set variables instead!\n", sFile.c_str(), pCtx->mpArea->GetName().c_str());
		return pCtx->mpDoc != NULL;
	}

	float DocF(void *c, const char *p, size_t n){ return C(c)->mpDoc->GetAttributeFloat(tString(p, n), 0); }
	OhplA7Sv DocS(void *c, const char *p, size_t n){ return Lend(C(c)->mpDoc->GetAttributeString(tString(p, n), "")); }

	void DocClose(void *c)
	{
		gpBase->mpEngine->GetResources()->DestroyXmlDocument(C(c)->mpDoc);
		C(c)->mpDoc = NULL;
	}

	void MaxFocus(void *c, float afDist){ C(c)->mpArea->SetMaxFocusDistance(afDist); }

	void Callback(void *c, int alWhich, const char *p, size_t n, bool abRemove)
	{
		if(alWhich == 0) C(c)->mpArea->SetPlayerLookAtCallback(tString(p, n), abRemove);
		else C(c)->mpArea->SetPlayerInteractCallback(tString(p, n), abRemove);
	}

	float CfgF(void *, const char *s, size_t sn, const char *k, size_t kn, float d)
	{
		return gpBase->mpGameCfg->GetFloat(tString(s, sn), tString(k, kn), d);
	}

	OhplA7Sv CfgS(void *, const char *s, size_t sn, const char *k, size_t kn)
	{
		return Lend(gpBase->mpGameCfg->GetString(tString(s, sn), tString(k, kn), ""));
	}

	void PreloadSnd(void *, const char *p, size_t n){ gpBase->PreloadSound(tString(p, n)); }
	void PreloadPS(void *, const char *p, size_t n){ gpBase->PreloadParticleSystem(tString(p, n)); }

	size_t MatFind(void *c, const char *p, size_t n)
	{
		cWorld *pWorld = C(c)->mpWorld;
		iLuxEntity *pArea = C(c)->mpArea;
		iPhysicsWorld *pPhysicsWorld = pWorld ? pWorld->GetPhysicsWorld() : pArea->GetMap()->GetPhysicsWorld();
		tString sName(p, n);
		iPhysicsMaterial *pMaterial = pPhysicsWorld->GetMaterialFromName(sName);
		if(pMaterial == NULL)
			Error("Could not load physics material '%s' for liquid area '%s'!\n", sName.c_str(), pArea->GetName().c_str());
		return reinterpret_cast<size_t>(pMaterial);
	}

	OhplA7Sv MatName(void *, size_t alMaterial){ return Lend(reinterpret_cast<iPhysicsMaterial*>(alMaterial)->GetName()); }

	void BodyIds(void *, size_t alBody, int *apOut)
	{
		iPhysicsBody *pBody = reinterpret_cast<iPhysicsBody*>(alBody);
		apOut[0] = static_cast<iLuxEntity*>(pBody->GetUserData())->GetID();
		apOut[1] = pBody->GetUniqueID();
	}

	size_t FindBody(void *c, int alEntityID, int alBodyID)
	{
		iLuxEntity *pEntity = C(c)->mpArea->GetMap()->GetEntityByID(alEntityID);
		iPhysicsBody *pBody = NULL;
		if(pEntity && pEntity->GetEntityType() == eLuxEntityType_Prop)
		{
			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
			pBody = pProp->GetBodyFromID(alBodyID);
			if(pBody == NULL)
				Error("Could not load body with id %d in prop '%s'\n", alBodyID, pProp->GetName().c_str());
		}
		else
			Error("Could not load prop with id %d\n", alEntityID);
		return reinterpret_cast<size_t>(pBody);
	}

	OhplA7Vt MakeVt(OhplAreaCtx *apCtx)
	{
		OhplA7Vt vt = { apCtx, VarF, VarI, VarB, VarS, CfgF, CfgS, PreloadSnd, PreloadPS, MatFind, MatName, BodyIds, FindBody, MaxFocus, Callback, DocOpen, DocF, DocS, DocClose };
		return vt;
	}
}

iLuxArea *cLuxAreaLoaderRs::CreateArea(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxAreaRs, (asName, alID, apMap, mAreaType));
}

void cLuxAreaLoaderRs::LoadVariables(iLuxArea *apArea, cWorld *apWorld)
{
	static_cast<cLuxAreaRs*>(apArea)->LoadVars(this, apWorld);
}

cLuxAreaRs::cLuxAreaRs(const tString &asName, int alID, cLuxMap *apMap, eLuxAreaType aAreaType) : iLuxArea(asName, alID, apMap, aAreaType)
{
	OhplAreaCtx ctx = { this, NULL, NULL };
	OhplA7Vt vt = MakeVt(&ctx);
	if(ohpl_a7_area_init(mpA7Area, asName.data(), asName.size(), &vt)) SetFullGameSave(true);
}

cLuxAreaRs::~cLuxAreaRs()
{
}

void cLuxAreaRs::LoadVars(iAreaLoader *apLoader, cWorld *apWorld)
{
	OhplAreaCtx ctx = { this, apLoader, apWorld, NULL };
	OhplA7Vt vt = MakeVt(&ctx);
	ohpl_a7_area_load_vars(mpA7Area, &vt);
}

void cLuxAreaRs::SetupAfterLoad(cWorld *apWorld)
{
	if(GetAreaType() == eLuxAreaType_Liquid)
	{
		float fHeight = mpBody->GetShape()->GetSize().y;
		cVector3f vPos = mpBody->GetWorldPosition();
		ohpl_a7_area_liquid_setup(mpA7Area, vPos.y, fHeight);
	}
	else if(GetAreaType() == eLuxAreaType_Ladder)
	{
		cMatrixf mtxWorld = mpBody->GetWorldMatrix();
		ohpl_a7_area_ladder_setup(mpA7Area, reinterpret_cast<const OhplGa5Mtx*>(&mtxWorld), mpBody->GetWorldPosition().y, mpBody->GetShape()->GetSize().y);
	}
}

tString cLuxAreaRs::Str(size_t alIdx)
{
	size_t lLen;
	const char *pStr = ohpl_a7_area_str(mpA7Area, alIdx, &lLen);
	return tString(pStr, lLen);
}

bool cLuxAreaRs::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_a7_area_can_interact(mpA7Area, GetInteractCallback()!="");
}

eLuxFocusCrosshair cLuxAreaRs::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	const int vTable[4] = { eLuxFocusCrosshair_Default, eLuxFocusCrosshair_Grab, eLuxFocusCrosshair_Ladder, eLuxFocusCrosshair_LastEnum };
	return (eLuxFocusCrosshair)ohpl_a7_area_crosshair(mpA7Area, GetInteractCallback()!="", GetCustomFocusCrossHair(), &vTable);
}

tWString cLuxAreaRs::GetFocusText()
{
	if(GetAreaType() != eLuxAreaType_Sign) return _W("");
	return kTranslate(Str(kOhplA7sSignCat), Str(kOhplA7sSignCat+1));
}

bool cLuxAreaRs::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	switch(GetAreaType())
	{
	case eLuxAreaType_Script: return ohpl_a7_area_on_interact(mpA7Area, GetInteractCallback()!="");
	case eLuxAreaType_Ladder: return LadderInteract();
	case eLuxAreaType_Examine:
	{
		OhplGa5ExamineOut out;
		ohpl_a7_area_examine(mpA7Area, gpBase->mpPlayer->GetSanity(), &out);
		if(out.showText)
		{
			size_t lCat = out.insaneText ? kOhplA7sExInsaneCat : kOhplA7sExDescCat;
			gpBase->mpMessageHandler->SetMessage(kTranslate(Str(lCat), Str(lCat+1)), 0);
		}
		if(out.playSound)
			gpBase->mpHelpFuncs->PlayGuiSoundData(Str(out.insaneSound ? kOhplA7sExInsaneSound : kOhplA7sExSound), eSoundEntryType_Gui);
		return true;
	}
	default: return false;
	}
}

cVector3f cLuxAreaRs::GetStartRotation()
{
	return cMath::GetAngleFromPoints3D(cVector3f(0,0,0), GetForward()*-1);
}

static bool LadderFits(void *apCtx, const OhplGa5Vec3 *apPos)
{
	iCharacterBody *pCharBody = static_cast<iCharacterBody*>(apCtx);
	return pCharBody->CheckCharacterFits(cVector3f(apPos->x, apPos->y, apPos->z), false,0,NULL,kEpsilonf);
}

bool cLuxAreaRs::LadderInteract()
{
	iCharacterBody *pCharBody = gpBase->mpPlayer->GetCharacterBody();
	cVector3f vCharSize = pCharBody->GetShape(0)->GetSize();
	cVector3f vCharPos = pCharBody->GetPosition();
	cVector3f vLadderPos = mpBody->GetWorldPosition();

	cVector3f vStartPos;
	ohpl_a7_area_ladder_start(mpA7Area, reinterpret_cast<const OhplGa5Vec3*>(&vLadderPos), reinterpret_cast<const OhplGa5Vec3*>(&vCharSize), vCharPos.y, reinterpret_cast<OhplGa5Vec3*>(&vStartPos));

	if(ohpl_ga5_ladder_find_start(reinterpret_cast<OhplGa5Vec3*>(&vStartPos), GetMaxY(), LadderFits, pCharBody)==false)
		return false;

	cLuxPlayerStateVars::SetupLadder(this, vStartPos);
	gpBase->mpPlayer->ChangeState(eLuxPlayerState_Ladder);

	return true;
}

void cLuxAreaRs::OnUpdate(float afTimeStep)
{
	ohpl_a7_area_update(mpA7Area, this, GetMap()->GetA7(), afTimeStep);
}

void cLuxAreaRs::StickyDetach()
{
	ohpl_a7_area_sticky_detach(mpA7Area, this, GetMap()->GetA7());
}

iLuxEntity_SaveData* cLuxAreaRs::CreateSaveData()
{
	switch(GetAreaType())
	{
	case eLuxAreaType_Script: return hplNew(cLuxArea_Script_SaveData, ());
	case eLuxAreaType_Sign: return hplNew(cLuxArea_Sign_SaveData, ());
	case eLuxAreaType_Examine: return hplNew(cLuxArea_Examine_SaveData, ());
	case eLuxAreaType_Ladder: return hplNew(cLuxArea_Ladder_SaveData, ());
	case eLuxAreaType_Liquid: return hplNew(cLuxArea_Liquid_SaveData, ());
	case eLuxAreaType_Sticky: return hplNew(cLuxArea_Sticky_SaveData, ());
	case eLuxAreaType_Insanity: return hplNew(cLuxArea_Insanity_SaveData, ());
	case eLuxAreaType_Flashback: return hplNew(cLuxArea_Flashback_SaveData, ());
	default: return hplNew(cLuxArea_SlimeDamage_SaveData, ());
	}
}

void cLuxAreaRs::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);

	OhplAreaCtx ctx = { this, NULL, NULL };
	OhplA7Vt vt = MakeVt(&ctx);
	ohpl_a7_area_save(mpA7Area, apSaveData, &vt);
}

void cLuxAreaRs::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);

	OhplAreaCtx ctx = { this, NULL, NULL };
	OhplA7Vt vt = MakeVt(&ctx);
	ohpl_a7_area_load(mpA7Area, apSaveData, &vt);
}

//////////////////////////////////////////////////////////////////////////
// ROPE
//////////////////////////////////////////////////////////////////////////

kSerializeRustClassP(cLuxRope_SaveData, iLuxEntity_SaveData)

iLuxEntity* cLuxRope_SaveData::CreateEntity(cLuxMap *apMap)
{
	cLuxRope *pRope = hplNew(cLuxRope, (msName, mlID,apMap));
	apMap->AddEntity(pRope);

	return pRope;
}

void cLuxAreaRopeLoader::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform,cWorld *apWorld)
{
	cLuxMap *pMap = gpBase->mpCurrentMapLoading;
	if(pMap==NULL) return;

	cLuxRope *pRope = hplNew(cLuxRope, (asName, alID,pMap));

	OhplAreaCtx ctx = { pRope, this, apWorld, NULL };
	OhplA7Vt vt = MakeVt(&ctx);
	cVector3f vStart = a_mtxTransform.GetTranslation();
	ohpl_a7_rope_load(pRope->mpA7Area, reinterpret_cast<const OhplGa5Vec3*>(&vStart), &vt);

	pMap->AddEntity(pRope);
	pRope->SetActive(abActive);
}

cLuxRope::cLuxRope(const tString& asName, int alID, cLuxMap *apMap)  : iLuxEntity(asName,alID,apMap, eLuxEntityType_Rope),
	mpA7Area(ohpl_a7_area_new(apMap->GetA7(), kOhplA7Rope)),
	mR(*static_cast<OhplA7Rope*>(ohpl_a7_area_pod(mpA7Area))),
	mpRope(NULL),
	mpRopeGfx(NULL)
{
}

cLuxRope::~cLuxRope()
{
	if(mpRopeGfx)
	{
		GetMap()->GetWorld()->DestroyRopeEntity(mpRopeGfx);
	}
	if(mpRope)
	{
		GetMap()->GetPhysicsWorld()->DestroyRope(mpRope);
	}
	ohpl_a7_area_free(GetMap()->GetA7(), mpA7Area);
}

tString cLuxRope::Str(size_t alIdx)
{
	size_t lLen;
	const char *pStr = ohpl_a7_area_str(mpA7Area, alIdx, &lLen);
	return tString(pStr, lLen);
}

void cLuxRope::AfterWorldLoad()
{
	int lRopeAction = ohpl_ga5_rope_after_world(mR.ropeCreated, mpRopeGfx==NULL && mpRope==NULL);
	if(lRopeAction != kOhplGa5RopeCreate)
	{
		if(lRopeAction == kOhplGa5RopeRelink)
		{
			mpRope = GetMap()->GetPhysicsWorld()->GetRopeFromUniqueID(GetID());
			if(mpRope==NULL) Error("Cannot find rope physics entity with ID %d for lux rope '%s'\n", GetID(), GetName().c_str());

			mpRopeGfx = GetMap()->GetWorld()->GetRopeEntityFromUniqueID(GetID());
			if(mpRopeGfx==NULL) Error("Cannot find rope gfx entity with ID %d for lux rope '%s'\n", GetID(), GetName().c_str());
		}

		return;
	}

	mR.ropeCreated = true;

	tString sEndPosNode = Str(kOhplA7sRopeEndPosNode);
	cLuxNode_Pos *pPosNode = GetMap()->GetPosNode(sEndPosNode);
	if(pPosNode == NULL)
	{
		Error("Could not find end pos '%s' for rope '%s'\n", sEndPosNode.c_str(), GetName().c_str());
		return;
	}

	cVector3f vEndPos = pPosNode->GetPosition();
	mR.endPos.x = vEndPos.x; mR.endPos.y = vEndPos.y; mR.endPos.z = vEndPos.z;

	tString sStartBody = Str(kOhplA7sRopeStartBody);
	tString sEndBody = Str(kOhplA7sRopeEndBody);
	iPhysicsBody *pStartBody = NULL;
	if(sStartBody != "") pStartBody = GetBody(sStartBody);

	iPhysicsBody *pEndBody = NULL;
	if(sEndBody != "") pEndBody = GetBody(sEndBody);

	mpRope = GetMap()->GetPhysicsWorld()->CreateRope(GetName(), cVector3f(mR.startPos.x, mR.startPos.y, mR.startPos.z), vEndPos);

	mpRope->SetMinTotalLength(mR.minTotalLength);
	mpRope->SetMaxTotalLength(mR.maxTotalLength);
	mpRope->SetSegmentLength(mR.segmentLength);
	mpRope->SetDamping(mR.damping);
	mpRope->SetStrength(mR.strength);
	mpRope->SetStiffness(mR.stiffness);
	mpRope->SetMotorSound(Str(kOhplA7sRopeSound));
	mpRope->SetMotorSoundStartSpeed(mR.soundStartSpeed);
	mpRope->SetMotorSoundStopSpeed(mR.soundStopSpeed);
	mpRope->SetAutoMoveActive(mR.autoMove);
	mpRope->SetAutoMoveAcc(mR.autoMoveAcc);
	mpRope->SetAutoMoveMaxSpeed(mR.autoMoveMaxSpeed);

	mpRope->SetUniqueID(GetID());

	if(pStartBody!=NULL)
	{
		mpRope->SetAttachedStartBody(pStartBody);
		if(pStartBody->GetMass() != 0)
			mpRope->GetStartParticle()->SetInvMass(1.0f);
	}

	if(pEndBody!=NULL)
	{
		mpRope->SetAttachedEndBody(pEndBody);
	}

	cResources *pResources = gpBase->mpEngine->GetResources();

	int lMaxSegments = ohpl_ga5_rope_max_segments(mR.maxTotalLength, mR.segmentLength);
	mpRopeGfx = GetMap()->GetWorld()->CreateRopeEntity(GetName(), mpRope, lMaxSegments);

	mpRopeGfx->SetMaterial(pResources->GetMaterialManager()->CreateMaterial(Str(kOhplA7sRopeMaterial)));
	mpRopeGfx->SetRadius(mR.radius);
	mpRopeGfx->SetLengthTileAmount(mR.lengthTileAmount);
	mpRopeGfx->SetLengthTileSize(mR.lengthTileSize);

	mpRopeGfx->SetUniqueID(GetID());
}

iPhysicsBody* cLuxRope::GetBody(const tString& asName)
{
	iPhysicsBody *pBody = GetMap()->GetPhysicsWorld()->GetBody(asName);
	if(pBody==NULL)
	{
		iLuxEntity *pEntity = GetMap()->GetEntityByName(asName, eLuxEntityType_Prop);
		if(pEntity==NULL)
		{
			Error("Could not find body or entity with the name '%s'\n", asName.c_str());
			return NULL;
		}
		pBody = static_cast<iLuxProp*>(pEntity)->GetMainBody();
	}
	return pBody;
}

void cLuxRope::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);

	OhplAreaCtx ctx = { this, NULL, NULL, NULL };
	OhplA7Vt vt = MakeVt(&ctx);
	ohpl_a7_area_save(mpA7Area, apSaveData, &vt);
}

void cLuxRope::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);

	OhplAreaCtx ctx = { this, NULL, NULL, NULL };
	OhplA7Vt vt = MakeVt(&ctx);
	ohpl_a7_area_load(mpA7Area, apSaveData, &vt);
}
