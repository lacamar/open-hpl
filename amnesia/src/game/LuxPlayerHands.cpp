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

#include "LuxPlayerHands.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"

#include "LuxHandObject_Melee.h"
#include "LuxHandObject_LightSource.h"
#include "RustLuxPlayer.h"


/////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHands::cLuxPlayerHands(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerHands")
{
	////////////////////
	//Init data

	//Add special entity type for loading the hands.
	gpBase->mpEngine->GetResources()->AddEntityLoader( hplNew( cLuxPlayerHandsLoader, ("PlayerHands", this) ) );
	
	//Add entity loader for hand objects
	mpHandObjectLoader = hplNew( cLuxHandObjectLoader, ("HandObject") );
	gpBase->mpEngine->GetResources()->AddEntityLoader(mpHandObjectLoader);

	ohpl_a2d_hands_init(&mD, gpBase->mpGameCfg->GetInt("Player_General", "HandsRotationSmoothNum", 0), gpBase->mpGameCfg->GetFloat("Player_General", "HandsPosAddMul", 0));

	mpHandsMesh = NULL;

}

cLuxPlayerHands::~cLuxPlayerHands()
{
	if(mpHandsMesh) 
	{
		gpBase->mpEngine->GetResources()->GetMeshManager()->Destroy(mpHandsMesh);
	}

	for(size_t i=0; i<mvHandAnimations.size(); ++i)
	{
		gpBase->mpEngine->GetResources()->GetAnimationManager()->Destroy(mvHandAnimations[i]);
	}
	
	STLDeleteAll(mvHandObjects);
	ohpl_a2d_hands_free(&mD);

	//Resources delete hand object loader by itself!
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// ENTITY LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHandsLoader::cLuxPlayerHandsLoader(const tString& asName, cLuxPlayerHands *apPlayerHands) : cEntityLoader_Object(asName)
{
	mpPlayerHands = apPlayerHands;
}

//-----------------------------------------------------------------------

void cLuxPlayerHandsLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	
}

//-----------------------------------------------------------------------

void cLuxPlayerHandsLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	mpPlayerHands->mpHandsEntity = mpEntity;
	if(mpEntity) mpEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster,false);
	
	if(mpMesh && mpPlayerHands->mpHandsMesh==NULL)
	{
		mpMesh->IncUserCount();
		mpPlayerHands->mpHandsMesh = mpMesh;
	}

	if(mpPlayerHands->mvHandAnimations.empty() && mpEntity)
	{
		for(int i=0; i<mpEntity->GetAnimationStateNum(); ++i)
		{
			cAnimationState *pAnimState = mpEntity->GetAnimationState(i);
            cAnimation *pAnim = pAnimState->GetAnimation();
			pAnim->IncUserCount();
			mpPlayerHands->mvHandAnimations.push_back(pAnim);
		}

	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerHands::OnStart()
{
	
}

void cLuxPlayerHands::Reset()
{
	OhplA2dHost dh = HandsHost();
	ohpl_a2d_hands_reset(&mD, &dh);
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::Update(float afTimeStep)
{
	OhplA2dHost dh = HandsHost();
	ohpl_a2d_hands_update(&mD, &dh, afTimeStep);
}


//-----------------------------------------------------------------------

void cLuxPlayerHands::PostUpdate(float afTimeStep)
{
	OhplA2dHost dh = HandsHost();
	ohpl_a2d_hands_post_update(&mD, &dh);
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::OnMapEnter(cLuxMap *apMap)
{
	//////////////////////////
	// If a hand object is loaded, create it.
	if(mpCurrentHandObject)
	{
		CreateAndAttachHandObject(apMap, mpCurrentHandObject);
		
		mpHandsEntity->SetVisible(true);
		mpHandsEntity->PlayName(mpCurrentHandObject->GetAnimIdle(),true, true);
	}

	ohpl_a2d_hands_clear_rotations(&mD);
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::OnMapLeave(cLuxMap *apMap)
{
	mpHandsEntity = NULL;
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *pHandObject = mvHandObjects[i];
		pHandObject->ResetEntityContainers();
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::CreateWorldEntities(cLuxMap *apMap)
{
	//////////////////////////
	// Create the hand entity
	CreateHandEntity(apMap);
}

void cLuxPlayerHands::DestroyWorldEntities(cLuxMap *apMap)
{
	/////////////////////////
	// Destroy the hand entity
	DestroyHandEntity(apMap);

	///////////////////////////////////
	// Destroy all hand object entities
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *pHandObject = mvHandObjects[i];
		pHandObject->DestroyEntity(apMap);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	if(mpCurrentHandObject) mpCurrentHandObject->RenderSolid(apFunctions);
}

void cLuxPlayerHands::RenderTrans(cRendererCallbackFunctions* apFunctions)
{
	if(mpCurrentHandObject) mpCurrentHandObject->RenderTrans(apFunctions);
}

//-----------------------------------------------------------------------

float cLuxPlayerHands::HostCall(void *apCtx, uint32_t alKind, int32_t alI, float afA, float afB, void *apP)
{
	cLuxPlayerHands *p = (cLuxPlayerHands*)apCtx;
	switch(alKind)
	{
	case kOhplA2dHHasObject: return p->mpCurrentHandObject != NULL;
	case kOhplA2dHAnimOver: return p->AnimOver();
	case kOhplA2dHObjAnimOver: return p->mpCurrentHandObject->AnimationIsOver();
	case kOhplA2dHAttachDraw:
		p->CreateAndAttachHandObject(gpBase->mpMapHandler->GetCurrentMap(), p->mpCurrentHandObject);
		p->PlayAnim(p->mpCurrentHandObject->GetAnimDraw(),false);
		break;
	case kOhplA2dHDisable:
		p->HideAllHandObjects();
		p->mpHandsEntity->SetVisible(false);
		break;
	case kOhplA2dHIdle: p->PlayAnim(p->mpCurrentHandObject->GetAnimIdle(),true); break;
	case kOhplA2dHUpdateObjects:
		// need to do like this, since current might not be drawn yet!
		for(size_t i=0; i<p->mvHandObjects.size(); ++i)
		{
			iLuxHandObject *pHandObject = p->mvHandObjects[i];
			if(pHandObject->GetMeshEntity() && pHandObject->GetMeshEntity()->IsActive()) pHandObject->Update(afA);
		}
		break;
	case kOhplA2dHObjDoAction: return p->mpCurrentHandObject->DoAction((eLuxPlayerAction)(int)afA, afB != 0);
	case kOhplA2dHSameObject: return p->mpCurrentHandObject == (iLuxHandObject*)apP;
	case kOhplA2dHHolster: p->PlayAnim(p->mpCurrentHandObject->GetAnimHolster(),false); break;
	case kOhplA2dHDrawNew:
	{
		iLuxHandObject *pObject = (iLuxHandObject*)apP;
		p->CreateAndAttachHandObject(gpBase->mpMapHandler->GetCurrentMap(), pObject);
		p->PlayAnim(pObject->GetAnimDraw(),false);
		p->mpHandsEntity->SetVisible(true);
		break;
	}
	case kOhplA2dHSetCurrent: p->mpCurrentHandObject = (iLuxHandObject*)apP; break;
	case kOhplA2dHResetPtrs:
		p->mpCurrentHandObject = NULL;
		p->mpHandsEntity = NULL;
		p->msCurrentAnim = "";
		for(size_t i=0; i<p->mvHandObjects.size(); ++i) p->mvHandObjects[i]->Reset();
		break;
	case kOhplA2dHHasEntity: return p->mpHandsEntity != NULL;
	case kOhplA2dHFillRot:
	{
		cCamera *pCam = p->mpPlayer->GetCamera();
		float *f = (float*)apP;
		f[0] = pCam->GetPitch(); f[1] = pCam->GetYaw(); f[2] = pCam->GetRoll();
		break;
	}
	case kOhplA2dHFillPos:
	{
		cCamera *pCam = p->mpPlayer->GetCamera();
		float *f = (float*)apP;
		const cVector3f vs[7] = {pCam->GetPosition(), pCam->GetRight(), pCam->GetUp(), pCam->GetForward(),
			p->mpPlayer->GetHeadPosAdd(eLuxHeadPosAdd_Bob), p->mpPlayer->GetHeadPosAdd(eLuxHeadPosAdd_ScreenShake), p->mpPlayer->GetHeadPosAdd(eLuxHeadPosAdd_Hurt)};
		for(int i=0; i<7; ++i) { f[i*3] = vs[i].x; f[i*3+1] = vs[i].y; f[i*3+2] = vs[i].z; }
		break;
	}
	case kOhplA2dHSetMatrix:
	{
		const float *f = (const float*)apP;
		cMatrixf mtxHands = cMath::MatrixRotate(cVector3f(f[0], f[1], f[2]), eEulerRotationOrder_ZXY);
		mtxHands.SetTranslation(cVector3f(f[3], f[4], f[5]));
		p->mpHandsEntity->SetMatrix(mtxHands);
		break;
	}
	}
	return 0;
}

void cLuxPlayerHands::PlayAnim(const tString& asAnim, bool abLoop)
{
	mpHandsEntity->PlayFadeToName(asAnim, abLoop, 0.2f);
	msCurrentAnim = asAnim;
}

bool cLuxPlayerHands::AnimOver()
{
	return mpHandsEntity->AnimationIsOver(msCurrentAnim);
}

//-----------------------------------------------------------------------

bool cLuxPlayerHands::CheckAnimationEvent(float afRelTime)
{
	cAnimationState *pAnim = mpHandsEntity->GetAnimationStateFromName(msCurrentAnim);
	if(pAnim == NULL) return false;

	float fPrevPos = pAnim->GetPreviousTimePosition() / pAnim->GetLength();
	float fPos = pAnim->GetRelativeTimePosition();
    
	return afRelTime > fPrevPos && afRelTime <= fPos;
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	OhplA2dHost dh = HandsHost();
	ohpl_a2d_hands_do_action(&mD, &dh, (int)aAction, abPressed);
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::SetActiveHandObject(const tString& asName)
{
	if(mpHandsEntity==NULL) return; //Make sure the 

	if(asName != "")
	{
		iLuxHandObject* pObject = GetHandObject(asName);
		if(pObject) SetCurrentHandObject(pObject);
	}
	else
	{
		SetCurrentHandObject(NULL);
	}
}

//-----------------------------------------------------------------------

iLuxHandObject* cLuxPlayerHands::GetHandObject(const tString& asName)
{
	//////////////////////////////////////
	// Search in already loaded objects
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *apObject = mvHandObjects[i];
        if(asName == apObject->GetName()) return apObject;
	}

	//////////////////////////////////////
	// If not found try loading
	return LoadHandObject(asName);
}


//-----------------------------------------------------------------------

void cLuxPlayerHands::SetCurrentHandObject(iLuxHandObject *apObject)
{
	OhplA2dHost dh = HandsHost();
	ohpl_a2d_hands_set_object(&mD, &dh, apObject);
}


//-----------------------------------------------------------------------

void cLuxPlayerHands::SetState(eLuxHandsState aState)
{ 
	mHandState = aState; 
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerHands::CreateHandEntity(cLuxMap *apMap)
{
	if(mpHandsEntity) return;

	apMap->GetWorld()->CreateEntity("PlayerHands", cMatrixf::Identity, "models/player/hands/hands.ent");
	mpHandsEntity->SetVisible(false);
	mpHandsEntity->Stop();
	mpHandsEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster,false);
	mpHandsEntity->SetRenderFlagBit(eRenderableFlag_VisibleInReflection,false);
}

void cLuxPlayerHands::DestroyHandEntity(cLuxMap *apMap)
{
	if(mpHandsEntity)
	{
		apMap->GetWorld()->DestroyMeshEntity(mpHandsEntity);
		mpHandsEntity = NULL;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::CreateAndAttachHandObject(cLuxMap *apMap, iLuxHandObject *apHandObject)
{
	HideAllHandObjects();

	///////////////////////
	//Load object
	mpHandObjectLoader->SetHandObject(apHandObject);
	apHandObject->CreateEntity(apMap);

	///////////////////////
	//Attach object
	cMeshEntity *pMeshEntity = apHandObject->GetMeshEntity();
	if(pMeshEntity)
	{
		cBoneState *pBone = mpHandsEntity->GetBoneStateFromName(apHandObject->GetAttachBoneName());
		if(pBone==NULL)
		{
			Error("Could not find bone '%s' in player hands model!\n", apHandObject->GetAttachBoneName().c_str());
			return;
		}
				
		pBone->AddEntity(pMeshEntity);
	}
	apHandObject->SetSetEntitiesVisible(true);

	mfHandObjectAlpha =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::HideAllHandObjects()
{
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *pHandObject = mvHandObjects[i];
		pHandObject->SetSetEntitiesVisible(false);
	}
}

//-----------------------------------------------------------------------

iLuxHandObject* cLuxPlayerHands::LoadHandObject(const tString& asName)
{
	/////////////////////
	// Init variables
	cResources *pResources = gpBase->mpEngine->GetResources();

	/////////////////////
	// Get file name
	tString sFile = cString::SetFileExt(asName, "ho");
	sFile = "/models/hand_objects/"+asName+"/"+sFile;

	/////////////////////
	// Load XML document
	iXmlDocument *pDoc = pResources->LoadXmlDocument(sFile);
	if(pDoc==NULL)
	{
		Error("Could not load hand object file: '%s'\n", sFile.c_str());
		return NULL;
	}

	// Main element
	cXmlElement *pMainElem = pDoc->GetFirstElement("Main");
	if(pMainElem == NULL){
		Error("No main element found in hand object file '%s'\n", sFile.c_str());
		pResources->DestroyXmlDocument(pDoc);
		return NULL;
	}

	// Settings element
	cXmlElement *pSettingsElem = pDoc->GetFirstElement("Settings");
	if(pSettingsElem == NULL){
		Error("No settings element found in hand object file '%s'\n", sFile.c_str());
		pResources->DestroyXmlDocument(pDoc);
		return NULL;
	}


	/////////////////////
	// Create the object

	//Get the type
	eLuxHandObjectType aType = ToHandObjectType(pMainElem->GetAttributeString("Type",""));
	if(aType == eLuxHandObjectType_LastEnum)
	{
		pResources->DestroyXmlDocument(pDoc);
		return NULL;
	}

	//Create object and load data
	iLuxHandObject *pObject = CreateObjectFromType(asName,aType);
	if(aType == eLuxHandObjectType_LastEnum)
	{
		pResources->DestroyXmlDocument(pDoc);
		return NULL;
	}

	//Load the main data (model file name, etc)
	if(pObject->LoadMainData(pMainElem)==false)
	{
		pResources->DestroyXmlDocument(pDoc);
		return NULL;	
	}

	//Load the settings
	pObject->LoadSettings(pSettingsElem);


	/////////////////////
	// Add object

	mvHandObjects.push_back(pObject);

	return pObject;
}

//-----------------------------------------------------------------------

iLuxHandObject* cLuxPlayerHands::CreateObjectFromType(const tString& asName, eLuxHandObjectType aType)
{
	switch(aType)
	{
	case eLuxHandObjectType_Melee:			return hplNew(cLuxHandObject_Melee, (asName, this));
	//case eLuxHandObjectType_Ranged:			return NULL;
	case eLuxHandObjectType_LightSource:	return hplNew(cLuxHandObject_LightSource, (asName, this));
	}

	Error("HandObject '%s' of type %d could not be created! Type is not implemented in code!",asName.c_str(), aType);

	return NULL;
}

//-----------------------------------------------------------------------

eLuxHandObjectType cLuxPlayerHands::ToHandObjectType(const tString& asType)
{
	tString sLowType = cString::ToLowerCase(asType);

	if(sLowType == "melee") return eLuxHandObjectType_Melee;
	if(sLowType == "ranged") return eLuxHandObjectType_Ranged;
	if(sLowType == "lightsource") return eLuxHandObjectType_LightSource;

	Error("Hand object of type '%s' does not exist!", asType.c_str());
	return eLuxHandObjectType_LastEnum;
}

//-----------------------------------------------------------------------

