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

#include "LuxProp_Item.h"

#include "LuxPlayer.h"
#include "LuxMap.h"
#include "LuxHelpFuncs.h"
#include "LuxInventory.h"
#include "LuxItemType.h"
#include "LuxMessageHandler.h"
#include "LuxEffectRenderer.h"


//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPropLoader_Item::cLuxPropLoader_Item(const tString& asName) : iLuxPropLoader(asName)
{
	mfDefaultMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction","Item_DefaultMaxFocusDist",0);
}

//-----------------------------------------------------------------------

iLuxProp *cLuxPropLoader_Item::CreateProp(const tString& asName, int alID, cLuxMap *apMap)
{
	return hplNew(cLuxProp_Item, (asName, alID,apMap) );
}

//-----------------------------------------------------------------------

static int LuxItemKind(eLuxItemType aType)
{
	switch(aType)
	{
	case eLuxItemType_Coins:	return kOhplGa4cItem_Coins;
	case eLuxItemType_Health:	return kOhplGa4cItem_Health;
	case eLuxItemType_Sanity:	return kOhplGa4cItem_Sanity;
	case eLuxItemType_LampOil:	return kOhplGa4cItem_LampOil;
	case eLuxItemType_Note:		return kOhplGa4cItem_Note;
	case eLuxItemType_Diary:	return kOhplGa4cItem_Diary;
	default:					return kOhplGa4cItem_Other;
	}
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Item::LoadVariables(iLuxProp *apProp, cXmlElement *apRootElem)
{
	cLuxProp_Item  *pItem = static_cast<cLuxProp_Item*>(apProp);

	pItem->mItemType = gpBase->mpInventory->GetItemTypeFromString(msEntitySubType);

	OhplGa4VarsAdapter vars(this);
	vars.Slot(kOhplGa4cSlot_ItemImage, pItem->msImageFile)
		.Slot(kOhplGa4cSlot_ItemPickSound, pItem->msPickSound)
		.Slot(kOhplGa4cSlot_ItemSubType, pItem->msSubItemTypeName);
	OhplGa4cItemAmount amount;
	ohpl_ga4c_item_load(vars.Vars(), LuxItemKind(pItem->mItemType), &amount);
	if(amount.set) pItem->mfAmount = amount.amount;
}

//-----------------------------------------------------------------------

void cLuxPropLoader_Item::LoadInstanceVariables(iLuxProp *apProp, cResourceVarsObject *apInstanceVars)
{
	cLuxProp_Item  *pItem = static_cast<cLuxProp_Item*>(apProp);

	OhplGa4VarsAdapter vars(apInstanceVars);
	vars.Slot(kOhplGa4cSlot_ItemSubType, pItem->msSubItemTypeName)
		.Slot(kOhplGa4cSlot_ItemVal, pItem->msVal)
		.Slot(kOhplGa4cSlot_ItemExtraVal, pItem->msExtraVal);
	OhplGa4cItemAmount amount;
	ohpl_ga4c_item_load_instance(vars.Vars(), LuxItemKind(pItem->mItemType), &amount);
	if(amount.set) pItem->mfAmount = amount.amount;
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxProp_Item::cLuxProp_Item(const tString &asName,int alID, cLuxMap *apMap) : iLuxProp(asName,alID,apMap, eLuxPropType_Item)
{
	mfAmount = 1.0f;
	mlSpawnContainerID =-1;
	mS.remove_prop = false;
	mS.flash_alpha =0;
}

//-----------------------------------------------------------------------

cLuxProp_Item::~cLuxProp_Item()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxProp_Item::CanInteract(iPhysicsBody *apBody)
{
	return ohpl_ga4c_item_can_interact(mbDestroyMe);
}

class cLuxItemHost
{
public:
	cLuxItemHost(cLuxProp_Item *apItem, cCamera *apCamera) : mpItem(apItem), mpCamera(apCamera)
	{
		mHost.ctx = this; mHost.show_pickup = &ShowPickup; mHost.sub_mesh_num = &SubMeshNum; mHost.sub_mesh_visible = &SubMeshVisible;
	}
	const OhplGa4cItemHost *Host() const { return &mHost; }

private:
	static cLuxItemHost *Me(void *c) { return (cLuxItemHost*)c; }
	static bool ShowPickup(void *c)
	{
		iLuxItemType *pType = gpBase->mpInventory->GetItemTypeData(Me(c)->mpItem->mItemType);
		return pType->ShowPickUpMessage();
	}
	static int SubMeshNum(void *c) { return Me(c)->mpItem->mpMeshEntity->GetSubMeshEntityNum(); }
	static bool SubMeshVisible(void *c, int i)
	{
		cSubMeshEntity *pSubEnt = Me(c)->mpItem->mpMeshEntity->GetSubMeshEntity(i);
		return Me(c)->mpCamera->GetFrustum()->CollideBoundingVolume(pSubEnt->GetBoundingVolume()) != eCollision_Outside;
	}

	cLuxProp_Item *mpItem;
	cCamera *mpCamera;
	OhplGa4cItemHost mHost;
};

//-----------------------------------------------------------------------

bool cLuxProp_Item::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	cLuxItemHost host(this, gpBase->mpPlayer->GetCamera());
	OhplGa4Run run(this);
	ohpl_ga4c_item_interact(&mS, host.Host(), run.Sink());
	run.Finish();

	return true;
}

//-----------------------------------------------------------------------

void cLuxProp_Item::Ga4ApplyOp(const OhplGa4Op &aOp)
{
	switch(aOp.code)
	{
	case kOhplGa4cOp_ItemAdd:
		gpBase->mpInventory->AddItem(msName, mItemType, msSubItemTypeName, msImageFile, mfAmount, msVal,msExtraVal ,&mS.remove_prop);
		break;
	case kOhplGa4cOp_ItemDestroy:
		mpMap->DestroyEntity(this);
		break;
	case kOhplGa4cOp_ItemPickSound:
		gpBase->mpHelpFuncs->PlayGuiSoundData("ui/pick_"+msPickSound, eSoundEntryType_World);
		break;
	case kOhplGa4cOp_ItemPickMessage:
		{
			tString sEntry = "ItemName_"+msSubItemTypeName;
			gpBase->mpMessageHandler->SetMessage(kTranslate("Inventory", "PickedUp")+_W(" ")+kTranslate("Inventory",sEntry), 0);
		}
		break;
	case kOhplGa4cOp_ItemCallback:
		RunCallbackFunc("OnPickup");
		break;
	case kOhplGa4cOp_ItemFlash:
		gpBase->mpEffectRenderer->AddFlashObject(mpMeshEntity->GetSubMeshEntity(aOp.i), aOp.f[0]);
		break;
	default:
		iLuxProp::Ga4ApplyOp(aOp);
	}
}

//-----------------------------------------------------------------------

void cLuxProp_Item::OnResetProperties()
{

}

//-----------------------------------------------------------------------

void cLuxProp_Item::OnSetupAfterLoad(cWorld *apWorld)
{
	
}

//-----------------------------------------------------------------------

void cLuxProp_Item::UpdatePropSpecific(float afTimeStep)
{
	if(mbInteractionDisabled) return;

	cCamera *pCam =  gpBase->mpPlayer->GetCamera();
	cVector3f vCameraPos = pCam->GetPosition();
	cVector3f vBodyPos = mvBodies[0]->GetLocalPosition();
	vCameraPos.y=0; 
	vBodyPos.y =0;

	cLuxItemHost host(this, pCam);
	OhplGa4Run run(this);
	ohpl_ga4c_item_update(&mS, afTimeStep, vCameraPos.v, vBodyPos.v, host.Host(), run.Sink());
	run.Finish();
}

//-----------------------------------------------------------------------

void cLuxProp_Item::BeforePropDestruction()
{
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxProp_Item::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_Pick;
}

//-----------------------------------------------------------------------

void cLuxProp_Item::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{
	//TODO!
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxProp_Item_SaveData, iLuxProp_SaveData)
kSerializeVar(msVal, eSerializeType_String)
kSerializeVar(msExtraVal, eSerializeType_String)
kSerializeVar(mlSpawnContainerID, eSerializeType_Int32)
kSerializeVar(mfAmount, eSerializeType_Float32)
kSerializeVar(msSubItemTypeName, eSerializeType_String)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxProp_Item::CreateSaveData()
{
	return hplNew(cLuxProp_Item_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxProp_Item::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::SaveToSaveData(apSaveData);
	cLuxProp_Item_SaveData *pData = static_cast<cLuxProp_Item_SaveData*>(apSaveData);

	//////////////////
	//Set variables
	kCopyToVar(pData,msVal);
	kCopyToVar(pData,msExtraVal);
	kCopyToVar(pData,mlSpawnContainerID);
	kCopyToVar(pData,mfAmount);
	kCopyToVar(pData,msSubItemTypeName);
}

//-----------------------------------------------------------------------

void cLuxProp_Item::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	//////////////////
	//Init
	super_class::LoadFromSaveData(apSaveData);
	cLuxProp_Item_SaveData *pData = static_cast<cLuxProp_Item_SaveData*>(apSaveData);
	
	//////////////////
	//Set variables
	kCopyFromVar(pData,msVal);
	kCopyFromVar(pData,msExtraVal);
	kCopyFromVar(pData,mlSpawnContainerID);
	kCopyFromVar(pData,mfAmount);
	kCopyFromVar(pData,msSubItemTypeName);
}

//-----------------------------------------------------------------------

void cLuxProp_Item::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
}

//-----------------------------------------------------------------------
