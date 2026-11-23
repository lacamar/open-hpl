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
// RUST-BACKED AREAS (liquid, sticky, slime, insanity, flashback)
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

kSerializeRustClassP(cLuxArea_Liquid_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Sticky_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_SlimeDamage_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Insanity_SaveData, iLuxArea_SaveData)
kSerializeRustClassP(cLuxArea_Flashback_SaveData, iLuxArea_SaveData)

namespace
{
	struct OhplAreaCtx
	{
		cLuxAreaRs *mpArea;
		cLuxAreaLoaderRs *mpLoader;
		cWorld *mpWorld;
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
	OhplA7Sv VarS(void *c, const char *p, size_t n){ return Lend(C(c)->mpLoader->GetVarString(tString(p, n), "")); }

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
		cLuxAreaRs *pArea = C(c)->mpArea;
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
		OhplA7Vt vt = { apCtx, VarF, VarI, VarB, VarS, CfgF, CfgS, PreloadSnd, PreloadPS, MatFind, MatName, BodyIds, FindBody };
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

void cLuxAreaRs::LoadVars(cLuxAreaLoaderRs *apLoader, cWorld *apWorld)
{
	OhplAreaCtx ctx = { this, apLoader, apWorld };
	OhplA7Vt vt = MakeVt(&ctx);
	ohpl_a7_area_load_vars(mpA7Area, &vt);
}

void cLuxAreaRs::SetupAfterLoad(cWorld *apWorld)
{
	if(GetAreaType() != eLuxAreaType_Liquid) return;

	float fHeight = mpBody->GetShape()->GetSize().y;
	cVector3f vPos = mpBody->GetWorldPosition();
	ohpl_a7_area_liquid_setup(mpA7Area, vPos.y, fHeight);
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
