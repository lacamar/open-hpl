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

//----------------------------------------------

#include "LuxBase.h"
#include "RustLuxUi.h"

//----------------------------------------


class cLuxMessageHandler : public iLuxUpdateable
{
friend class cLuxMusicHandler_SaveData;
public:	
	cLuxMessageHandler();
	~cLuxMessageHandler();
	
	void LoadFonts();
	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	void LoadUserConfig();
	void SaveUserConfig();

	void OnMapEnter(cLuxMap *apMap);
	void OnMapLeave(cLuxMap *apMap);

	void StarQuestAddedMessage();

	void StartPauseMessage(const tWString& asText, bool abYesNo, iLuxMessageCallback *apCallback);
	
	/**
	* if time is <=0 then the life time is calculated based on string length.
	*/
	void SetMessage(const tWString& asText, float afTime);
	bool IsMessageActive(){ return mS.message_time>0; }

	void OnDraw(float afFrameTime);

	void DoAction(eLuxPlayerAction aAction, bool abPressed);

	bool IsPauseMessageActive(){ return mS.pause_active != 0; }
	void SetPauseMessageActive(bool abX);

	bool ShowSubtitles(){ return mS.show_subtitles != 0;}
	void SetShowSubtitles(bool abX){ mS.show_subtitles=abX;}

	bool ShowEffectSubtitles(){ return mS.show_effect_subtitles != 0; }
	void SetShowEffectSubtitles(bool abX){ mS.show_effect_subtitles=abX; }
private:
	void RunEvents(const std::vector<OhplGa8Event> &avEvents);
	void DrawQuestAdded();
	void DrawMessage();
	void DrawPauseMessage();
	
	
	//////////////////
	// Data
	cGuiGfxElement *mpBlackGfx;
	iFontData *mpFont;

	cGuiGfxElement *mpQuestAddedIcon;
	tString msQuestAddedSound;
	
	OhplGa8MessageState mS;

	//////////////////
	// Variables
	tWStringVec mvMessageRows;

	tWStringVec mvLines;
	iLuxMessageCallback* mpCallback;
};

//----------------------------------------------


#endif // LUX_MESSAGE_HANDLER_H
