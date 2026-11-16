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

#ifndef LUX_HINT_HANDLER_H
#define LUX_HINT_HANDLER_H

#include "LuxBase.h"
#include "LuxA11bDump.h"
#include "RustLuxUiA11b.h"


class cLuxHintHandler : public iLuxUpdateable
{
public:
	cLuxHintHandler() : iLuxUpdateable("LuxHintHandler")
	{
		OhplA11bInitHost();
		mlH = ohpl_a11b_hint_new();
	}
	~cLuxHintHandler() { ohpl_a11b_hint_free(mlH); }

	void LoadFonts()
	{
		ohpl_a11b_hint_font(mlH, (size_t)LoadFont(gpBase->mpMenuCfg->GetString("Hints","DefaultFont","")));
	}
	void OnStart(){}
	void Update(float afTimeStep)
	{
		if(OhplA11Enabled() == false) UpdateHintText(afTimeStep);
	}
	void LoadUserConfig() { ohpl_a11b_hint_load_user_config(mlH); }
	void SaveUserConfig() { ohpl_a11b_hint_save_user_config(mlH); }
	void Reset() { ohpl_a11b_hint_reset(mlH); }
	void OnDraw(float afFrameTime)
	{
		if(OhplA11Enabled()) UpdateHintText(OhplA11Dt());
		DrawHintText(afFrameTime, gpBase->mpGameHudSet);
	}

	void DrawHintText(float afFrameTime, cGuiSet *apGuiSet) { ohpl_a11b_hint_draw(mlH, (size_t)apGuiSet); }
	void UpdateHintText(float afTimeStep) { ohpl_a11b_hint_update(mlH, afTimeStep); }

	void SetActive(bool abX) { ohpl_a11b_hint_set_active(mlH, abX); }
	bool IsActive() { return ohpl_a11b_hint_is_active(mlH) != 0; }
	uint64_t GetHandle(){ return mlH; }

	/**
	* if time shown is <=0 then the life time is calculated based on string length.
	*/
	bool Add(const tString &asName, const tWString &asMessage, float afTimeShown)
	{
		return ohpl_a11b_hint_add(mlH, asName.data(), asName.size(), OhplA11bW(asMessage), asMessage.size(), afTimeShown) != 0;
	}
	bool Remove(const tString &asName) { return ohpl_a11b_hint_remove(mlH, asName.data(), asName.size()) != 0; }

	void Block(const tString &asName) { ohpl_a11b_hint_block(mlH, asName.data(), asName.size(), 1); }
	void UnBlock(const tString &asName) { ohpl_a11b_hint_block(mlH, asName.data(), asName.size(), 0); }

	void GetNames(bool abBlocked, tStringVec &avNames)
	{
		ohpl_a11b_hint_names(mlH, abBlocked, PushName, &avNames);
	}
	void SetNames(bool abBlocked, const tStringVec &avNames)
	{
		ohpl_a11b_hint_names_clear(mlH, abBlocked);
		for(size_t i=0; i<avNames.size(); ++i) ohpl_a11b_hint_names_add(mlH, abBlocked, avNames[i].data(), avNames[i].size());
	}

private:
	static void PushName(void *apCtx, const unsigned char *apName, size_t alLen)
	{
		((tStringVec*)apCtx)->push_back(tString((const char*)apName, alLen));
	}

	uint64_t mlH;
};

#endif // LUX_HINT_HANDLER_H
