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

#ifndef LUX_MESSAGE_HANDLER_H
#define LUX_MESSAGE_HANDLER_H

#include "LuxBase.h"
#include "RustLuxUiA11b.h"

class cLuxMessageHandler : public iLuxUpdateable
{
public:
	cLuxMessageHandler() : iLuxUpdateable("LuxMusicHandler")
	{
		OhplA11bInitHost();
		mlH = ohpl_a11b_msg_new();
	}
	~cLuxMessageHandler() { ohpl_a11b_msg_free(mlH); }

	void LoadFonts() { ohpl_a11b_msg_font(mlH, (size_t)LoadFont("game_default.fnt")); }
	void OnStart(){}
	void Update(float afTimeStep) { ohpl_a11b_msg_on_update(mlH, afTimeStep); }
	void Reset() { ohpl_a11b_msg_reset(mlH); }

	void LoadUserConfig() { ohpl_a11b_msg_load_user_config(mlH); }
	void SaveUserConfig() { ohpl_a11b_msg_save_user_config(mlH); }

	void OnMapEnter(cLuxMap *apMap) { Reset(); }
	void OnMapLeave(cLuxMap *apMap){}

	void StarQuestAddedMessage() { ohpl_a11b_msg_start_quest(mlH); }

	void StartPauseMessage(const tWString& asText, bool abYesNo, iLuxMessageCallback *apCallback)
	{
		ohpl_a11b_msg_start_pause(mlH, OhplA11bW(asText), asText.size(), abYesNo, (size_t)apCallback);
	}

	/**
	* if time is <=0 then the life time is calculated based on string length.
	*/
	void SetMessage(const tWString& asText, float afTime)
	{
		ohpl_a11b_msg_set_message(mlH, OhplA11bW(asText), asText.size(), afTime);
	}
	bool IsMessageActive(){ return ohpl_a11b_msg_flag(mlH, 0) != 0; }
	uint64_t GetHandle(){ return mlH; }

	void OnDraw(float afFrameTime) { ohpl_a11b_msg_on_draw(mlH); }

	void DoAction(eLuxPlayerAction aAction, bool abPressed)
	{
		ohpl_a11b_msg_do_action(mlH, aAction == eLuxPlayerAction_Interact ? 0 : (aAction == eLuxPlayerAction_Attack ? 1 : -1), abPressed);
	}

	bool IsPauseMessageActive(){ return ohpl_a11b_msg_flag(mlH, 1) != 0; }
	void SetPauseMessageActive(bool abX) { ohpl_a11b_msg_set_pause(mlH, abX); }

	bool ShowSubtitles(){ return ohpl_a11b_msg_flag(mlH, 2) != 0;}
	void SetShowSubtitles(bool abX){ ohpl_a11b_msg_set_flag(mlH, 2, abX);}

	bool ShowEffectSubtitles(){ return ohpl_a11b_msg_flag(mlH, 3) != 0; }
	void SetShowEffectSubtitles(bool abX){ ohpl_a11b_msg_set_flag(mlH, 3, abX); }

private:
	uint64_t mlH;
};

#endif // LUX_MESSAGE_HANDLER_H
