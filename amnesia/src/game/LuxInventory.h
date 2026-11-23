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

#ifndef LUX_INVENTORY_H
#define LUX_INVENTORY_H

#include "LuxBase.h"
#include "RustLuxInventory.h"

class cLuxInventory : public iLuxUpdateable
{
public:
	cLuxInventory();
	~cLuxInventory();

	void LoadFonts(){ ohpl_inv_load_fonts(); }
	void Reset(){ ohpl_inv_reset(); }
	void OnGameStart(){ ohpl_inv_on_game_start(); }
	void Update(float afTimeStep){ ohpl_inv_update(afTimeStep); }
	void OnEnterContainer(const tString& asOldContainer){ ohpl_inv_on_enter(); }
	void OnLeaveContainer(const tString& asNewContainer){ ohpl_inv_on_leave(); }
	void OnDraw(float afFrameTime){ ohpl_inv_on_draw(afFrameTime); }
#ifdef USE_GAMEPAD
	void AppDeviceWasPlugged(){ ohpl_inv_gamepad_changed(); }
	void AppDeviceWasRemoved(){ ohpl_inv_gamepad_changed(); }
#endif

	cGuiSet* GetSet(){ return (cGuiSet*)ohpl_inv_get_set(); }

	void ExitPressed(){ ohpl_inv_exit_pressed(); }
	void LoadScript(){ ohpl_inv_load_script(); }

	cLuxInventory_Item* AddItem(const tString& asName, eLuxItemType aType, const tString& asSubTypeName, const tString& asImageName,
								float afAmount, const tString& asVal, const tString& asExtraVal, bool* apRemoveItemProp=NULL)
	{
		int lRemove = 0;
		uintptr_t pItem = ohpl_inv_add_item(OhplA11aS(asName), aType, OhplA11aS(asSubTypeName), OhplA11aS(asImageName), afAmount,
											OhplA11aS(asVal), OhplA11aS(asExtraVal), &lRemove);
		if(apRemoveItemProp) *apRemoveItemProp = lRemove != 0;
		return (cLuxInventory_Item*)pItem;
	}
	void RemoveItem(const tString& asName){ ohpl_inv_remove_by_name(OhplA11aS(asName)); }
	void RemoveItem(cLuxInventory_Item *apItem){ ohpl_inv_remove((uintptr_t)apItem); }
	void RemoveItemOfType(eLuxItemType aType){ ohpl_inv_remove_of_type(aType); }
	int GetItemIndex(cLuxInventory_Item *apItem){ return ohpl_inv_item_index((uintptr_t)apItem); }
	cLuxInventory_Item* GetItem(const tString& asName){ return (cLuxInventory_Item*)ohpl_inv_get_by_name(OhplA11aS(asName)); }
	cLuxInventory_Item* GetItem(int alIdx){ return (cLuxInventory_Item*)ohpl_inv_item_at(alIdx); }
	cLuxInventory_Item* GetItemFromSubType(const tString& asSubType){ return (cLuxInventory_Item*)ohpl_inv_get_by_subtype(OhplA11aS(asSubType)); }
	int GetItemNum(){ return ohpl_inv_item_num(); }
	bool HasItemOfType(eLuxItemType aType){ return ohpl_inv_has_of_type(aType) != 0; }

	void AddCombineCallback(const tString& asName, const tString& asItemA, const tString& asItemB, const tString& asFunction, bool abAutoDestroy)
	{
		ohpl_inv_add_combine(OhplA11aS(asName), OhplA11aS(asItemA), OhplA11aS(asItemB), OhplA11aS(asFunction), abAutoDestroy);
	}
	void RemoveCombineCallback(const tString& asName){ ohpl_inv_remove_combine(OhplA11aS(asName)); }

	bool RecompileScript(tString *apOutput)
	{
		tString sOut;
		bool bOk = ohpl_inv_recompile(OhplA11aStrSink, &sOut) != 0;
		if(apOutput) *apOutput = sOut;
		return bOk;
	}

	void SetMessageText(const tWString &asText, float afLifeTime){ ohpl_inv_set_message((const unsigned int*)asText.data(), asText.size(), afLifeTime); }
	bool GetMessageActive(){ return ohpl_inv_message_active() != 0; }

	void OpenJournal(){ ohpl_inv_open_journal(); }
	void SetDisabled(bool abX){ ohpl_inv_set_disabled(abX); }
	bool GetDisabled(){ return ohpl_inv_disabled() != 0; }

	cLuxInventory_Item* GetPickedItem(){ return (cLuxInventory_Item*)ohpl_inv_picked(); }
	void SetEnterFromJournal(bool abX){ ohpl_inv_set_enter_from_journal(abX); }

	void SetEquippedHandItem(cLuxInventory_Item *apItem){ ohpl_inv_set_equipped((uintptr_t)apItem); }
	cLuxInventory_Item* GetEquippedHandItem(){ return (cLuxInventory_Item*)ohpl_inv_equipped(); }

	eLuxItemType GetItemTypeFromString(const tString& asType){ return (eLuxItemType)ohpl_inv_type_from_string(OhplA11aS(asType)); }

};

inline cGuiGfxElement* OhplInvImage(cLuxInventory_Item *p) { return (cGuiGfxElement*)ohpl_inv_item_image((uintptr_t)p); }

#endif // LUX_INVENTORY_H
