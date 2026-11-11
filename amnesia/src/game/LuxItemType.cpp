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

#include "LuxItemType.h"

#include "LuxInventory.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxPlayerState.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxEntity.h"
#include "LuxJournal.h"
#include "LuxCompletionCountHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxHintHandler.h"


//////////////////////////////////////////////////////////////////////////
// INTERFACE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxItemType::iLuxItemType(eLuxItemType aType)
{
	OhplGa8ItInfo info = {};
	ohpl_ga8_it_info(aType, &info);

	msName.assign((const char*)info.name.p, info.name.n);
	mType = aType;
	mbHasCount = info.has_count != 0;
	mlMaxCount = info.max_count;
	mbShowPickUpMessage = info.show_pickup != 0;
}

//-----------------------------------------------------------------------

iLuxItemType::~iLuxItemType()
{

}

//-----------------------------------------------------------------------

void iLuxItemType::AddCompletionAmount(int alAmount)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return;

	pMap->AddCompletionAmount(alAmount);
}

//-----------------------------------------------------------------------

void iLuxItemType::FillItem(cLuxInventory_Item *apItem, OhplGa8ItItem &aItem)
{
	aItem.name = OhplGa8S(apItem->GetName());
	aItem.sval = OhplGa8S(apItem->GetStringVal());
	aItem.extra = OhplGa8S(apItem->GetExtraStringVal());
	aItem.amount = apItem->GetAmount();
}

void iLuxItemType::FillPlayer(OhplGa8ItPlayer &aPlayer)
{
	memset(&aPlayer, 0, sizeof(aPlayer));

	switch(mType)
	{
	case eLuxItemType_Puzzle:
		aPlayer.use_item_state = gpBase->mpPlayer->GetCurrentState()==eLuxPlayerState_UseItem;
		break;
	case eLuxItemType_Coins:
		aPlayer.coins = gpBase->mpPlayer->GetCoins();
		break;
	case eLuxItemType_Lantern:
		aPlayer.lantern_active = gpBase->mpPlayer->GetHelperLantern()->IsActive();
		break;
	case eLuxItemType_Health:
		aPlayer.health = gpBase->mpPlayer->GetHealth();
		break;
	case eLuxItemType_Sanity:
		aPlayer.sanity = gpBase->mpPlayer->GetSanity();
		break;
	case eLuxItemType_LampOil:
		aPlayer.has_lantern = gpBase->mpInventory->HasItemOfType(eLuxItemType_Lantern);
		if(aPlayer.has_lantern) aPlayer.lamp_oil = gpBase->mpPlayer->GetLampOil();
		break;
	default:
		break;
	}
}

void iLuxItemType::RunEvents(const std::vector<OhplGa8Event> &avEvents, cLuxInventory_Item *apItem, cLuxNote *apNote, cLuxDiary *apDiary)
{
	for(size_t i=0; i<avEvents.size(); ++i)
	{
		const OhplGa8Event &e = avEvents[i];
		switch(e.kind)
		{
		case eOhplGa8It_ProgLog:
			ProgLog(eLuxProgressLogLevel_Medium, e.s0);
			break;
		case eOhplGa8It_Completion:
			if(e.a == eOhplGa8Completion_Item)
				AddCompletionAmount(gpBase->mpCompletionCountHandler->mlItemCompletionValue);
			else if(e.a == eOhplGa8Completion_Note)
				AddCompletionAmount(gpBase->mpCompletionCountHandler->mlNoteCompletionValue);
			else
				AddCompletionAmount(gpBase->mpCompletionCountHandler->mlDiaryCompletionValue);
			break;
		case eOhplGa8It_SetCoins:
			gpBase->mpPlayer->SetCoins(e.a);
			break;
		case eOhplGa8It_Hint:
			gpBase->mpHintHandler->Add(e.s0, kTranslate("Hints", e.s0), 0);
			break;
		case eOhplGa8It_SetContainerJournal:
			gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
			break;
		case eOhplGa8It_ForceInstantExit:
			gpBase->mpJournal->SetForceInstantExit(e.a != 0);
			break;
		case eOhplGa8It_OpenNote:
			gpBase->mpJournal->OpenNote(apNote, e.a != 0);
			break;
		case eOhplGa8It_RunMapScript:
			gpBase->mpMapHandler->GetCurrentMap()->RunScript(e.s0);
			break;
		case eOhplGa8It_OpenDiary:
			gpBase->mpJournal->OpenDiary(apDiary, e.a != 0);
			break;
		case eOhplGa8It_DiaryLastRead:
			gpBase->mpJournal->SetDiaryAsLastRead(apDiary);
			break;
		case eOhplGa8It_LanternToggle:
			gpBase->mpPlayer->GetHelperLantern()->SetActive(e.a != 0, true);
			break;
		case eOhplGa8It_SetHealth:
			gpBase->mpPlayer->SetHealth(e.v[0]);
			break;
		case eOhplGa8It_AddSanity:
			gpBase->mpPlayer->AddSanity(e.v[0]);
			break;
		case eOhplGa8It_SetLampOil:
			gpBase->mpPlayer->SetLampOil(e.v[0]);
			break;
		case eOhplGa8It_RemoveItem:
			gpBase->mpInventory->RemoveItem(apItem);
			break;
		case eOhplGa8It_Sound:
			gpBase->mpHelpFuncs->PlayGuiSoundData(e.s0, eSoundEntryType_Gui);
			break;
		case eOhplGa8It_MessageKey:
			gpBase->mpInventory->SetMessageText(kTranslate("Inventory", e.s0), 0);
			break;
		case eOhplGa8It_UseItemStateReset:
			gpBase->mpPlayer->ChangeState(eLuxPlayerState_Normal);
			break;
		case eOhplGa8It_UseItemEnter:
			cLuxPlayerStateVars::SetupUseItem(apItem);
			gpBase->mpPlayer->ChangeState(eLuxPlayerState_UseItem);
			break;
		case eOhplGa8It_ExitPressed:
			gpBase->mpInventory->ExitPressed();
			break;
		case eOhplGa8It_Equip:
			gpBase->mpInventory->SetEquippedHandItem(apItem);
			break;
		case eOhplGa8It_AddTinderbox:
			gpBase->mpPlayer->AddTinderboxes(e.a);
			break;
		}
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// TYPES
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

#define LUX_ITEM_TYPE_BASIC(CLASS, TYPE) \
CLASS::CLASS() : iLuxItemType(TYPE) \
{ \
}

#define LUX_ITEM_TYPE_BEFORE_ADD(CLASS, TYPE) \
bool CLASS::BeforeAddItem(cLuxInventory_Item *apItem) \
{ \
	OhplGa8ItItem item; \
	OhplGa8ItPlayer player; \
	OhplGa8Buf buf; \
	int lConsumed = 0; \
	FillItem(apItem, item); \
	FillPlayer(player); \
	size_t lNum = ohpl_ga8_it_before_add(TYPE, &item, &player, &lConsumed, buf.mvRaw, OHPL_GA8_EVMAX); \
	RunEvents(OhplGa8Take(buf.mvRaw, lNum), apItem, NULL, NULL); \
	return lConsumed != 0; \
}

#define LUX_ITEM_TYPE_ON_USE(CLASS, TYPE) \
void CLASS::OnUse(cLuxInventory_Item *apItem, int alSlotIndex) \
{ \
	OhplGa8ItItem item; \
	OhplGa8ItPlayer player; \
	OhplGa8Buf buf; \
	FillItem(apItem, item); \
	FillPlayer(player); \
	size_t lNum = ohpl_ga8_it_on_use(TYPE, &item, &player, buf.mvRaw, OHPL_GA8_EVMAX); \
	RunEvents(OhplGa8Take(buf.mvRaw, lNum), apItem, NULL, NULL); \
}

LUX_ITEM_TYPE_BASIC(cLuxItemType_Puzzle, eLuxItemType_Puzzle)
LUX_ITEM_TYPE_BEFORE_ADD(cLuxItemType_Puzzle, eLuxItemType_Puzzle)
LUX_ITEM_TYPE_ON_USE(cLuxItemType_Puzzle, eLuxItemType_Puzzle)

LUX_ITEM_TYPE_BASIC(cLuxItemType_Coins, eLuxItemType_Coins)
LUX_ITEM_TYPE_BEFORE_ADD(cLuxItemType_Coins, eLuxItemType_Coins)
void cLuxItemType_Coins::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{

}

LUX_ITEM_TYPE_BASIC(cLuxItemType_Note, eLuxItemType_Note)

bool cLuxItemType_Note::BeforeAddItem(cLuxInventory_Item *apItem)
{
	cLuxNote *pNote = gpBase->mpJournal->AddNote(apItem->GetStringVal(), apItem->GetImageName());

	OhplGa8ItItem item;
	OhplGa8Buf buf;
	FillItem(apItem, item);
	size_t lNum = ohpl_ga8_it_note_before_add(pNote==NULL, &item, buf.mvRaw, OHPL_GA8_EVMAX);
	RunEvents(OhplGa8Take(buf.mvRaw, lNum), apItem, pNote, NULL);

	return true;
}

void cLuxItemType_Note::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{

}

//-----------------------------------------------------------------------

bool cLuxItemType_Diary::mbShowJournalOnPickup = true;

LUX_ITEM_TYPE_BASIC(cLuxItemType_Diary, eLuxItemType_Diary)

bool cLuxItemType_Diary::BeforeAddItem(cLuxInventory_Item *apItem)
{
	int lDiaryIdx;
	cLuxDiary *pDiary = gpBase->mpJournal->AddDiary(apItem->GetStringVal(), apItem->GetImageName(), lDiaryIdx);

	OhplGa8ItItem item;
	OhplGa8Buf buf;
	int lGo = 0;
	FillItem(apItem, item);
	size_t lNum = ohpl_ga8_it_diary_before_add(pDiary==NULL, lDiaryIdx, &item, &lGo, buf.mvRaw, OHPL_GA8_EVMAX);
	if(lGo == 0) return true;

	mbShowJournalOnPickup = true;
	RunEvents(OhplGa8Take(buf.mvRaw, lNum), apItem, NULL, pDiary);

	lNum = ohpl_ga8_it_diary_after(mbShowJournalOnPickup, buf.mvRaw, OHPL_GA8_EVMAX);
	RunEvents(OhplGa8Take(buf.mvRaw, lNum), apItem, NULL, pDiary);

	return true;
}

void cLuxItemType_Diary::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{

}

//-----------------------------------------------------------------------

LUX_ITEM_TYPE_BASIC(cLuxItemType_Lantern, eLuxItemType_Lantern)
LUX_ITEM_TYPE_BEFORE_ADD(cLuxItemType_Lantern, eLuxItemType_Lantern)
LUX_ITEM_TYPE_ON_USE(cLuxItemType_Lantern, eLuxItemType_Lantern)

tWString cLuxItemType_Lantern::GetDisplayedNameAdd(cLuxInventory_Item *apItem)
{
	cLuxPlayerLantern *pLantern = gpBase->mpPlayer->GetHelperLantern();
	OhplGa8Str key = ohpl_ga8_it_lantern_key(pLantern->IsActive());
	tWString sStr = kTranslate("Inventory", tString((const char*)key.p, key.n));
	return _W(" (") +sStr + _W(")");
}

//-----------------------------------------------------------------------

LUX_ITEM_TYPE_BASIC(cLuxItemType_Health, eLuxItemType_Health)
LUX_ITEM_TYPE_BEFORE_ADD(cLuxItemType_Health, eLuxItemType_Health)
LUX_ITEM_TYPE_ON_USE(cLuxItemType_Health, eLuxItemType_Health)

LUX_ITEM_TYPE_BASIC(cLuxItemType_Sanity, eLuxItemType_Sanity)
LUX_ITEM_TYPE_BEFORE_ADD(cLuxItemType_Sanity, eLuxItemType_Sanity)
LUX_ITEM_TYPE_ON_USE(cLuxItemType_Sanity, eLuxItemType_Sanity)

LUX_ITEM_TYPE_BASIC(cLuxItemType_LampOil, eLuxItemType_LampOil)
LUX_ITEM_TYPE_BEFORE_ADD(cLuxItemType_LampOil, eLuxItemType_LampOil)
LUX_ITEM_TYPE_ON_USE(cLuxItemType_LampOil, eLuxItemType_LampOil)

LUX_ITEM_TYPE_BASIC(cLuxItemType_Tinderbox, eLuxItemType_Tinderbox)
LUX_ITEM_TYPE_BEFORE_ADD(cLuxItemType_Tinderbox, eLuxItemType_Tinderbox)
LUX_ITEM_TYPE_ON_USE(cLuxItemType_Tinderbox, eLuxItemType_Tinderbox)

//-----------------------------------------------------------------------

cLuxItemType_HandObject::cLuxItemType_HandObject() : iLuxItemType(eLuxItemType_HandObject)
{

}

bool cLuxItemType_HandObject::BeforeAddItem(cLuxInventory_Item *apItem)
{
	if(gpBase->mpInventory->GetEquippedHandItem() == NULL)
	{
		gpBase->mpInventory->SetEquippedHandItem(apItem);
	}

    return false;
}

LUX_ITEM_TYPE_ON_USE(cLuxItemType_HandObject, eLuxItemType_HandObject)

//-----------------------------------------------------------------------
