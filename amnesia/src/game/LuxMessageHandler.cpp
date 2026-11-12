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

#include "LuxMessageHandler.h"

#include "LuxMapHandler.h"
#include "LuxInputHandler.h"
#include "LuxHelpFuncs.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMessageHandler::cLuxMessageHandler() : iLuxUpdateable("LuxMusicHandler")
{
	cGui *pGui = gpBase->mpEngine->GetGui();

	mpBlackGfx 	= pGui->CreateGfxFilledRect(cColor(0,1),eGuiMaterial_Alpha);

	mpFont = NULL;

	msQuestAddedSound = gpBase->mpMenuCfg->GetString("Messages", "QuestAddedSound","");
	tString sQuestAddedImage = gpBase->mpMenuCfg->GetString("Messages", "QuestAddedIcon","");
	if(sQuestAddedImage != "")	mpQuestAddedIcon = pGui->CreateGfxImage(sQuestAddedImage, eGuiMaterial_Alpha);
	else					mpQuestAddedIcon = NULL;

	memset(&mS, 0, sizeof(mS));
	cVector2f vFontSize = gpBase->mpMenuCfg->GetVector2f("Messages", "GameMessageFontSize",0);
	mS.font_size[0] = vFontSize.x;
	mS.font_size[1] = vFontSize.y;
	mpCallback = NULL;
}

//-----------------------------------------------------------------------

cLuxMessageHandler::~cLuxMessageHandler()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMessageHandler::LoadFonts()
{
	tString sFontFile = "game_default.fnt";
	mpFont = LoadFont(sFontFile);
}

void cLuxMessageHandler::OnStart()
{
	
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::Reset()
{
	ohpl_ga8_msg_reset(&mS);

	mvLines.clear();
	mpCallback = NULL;
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::LoadUserConfig()
{
	mS.show_subtitles = gpBase->mpUserConfig->GetBool("Game", "ShowSubtitles", true);
	mS.show_effect_subtitles = gpBase->mpUserConfig->GetBool("Game", "ShowEffectSubtitles", false);
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game", "ShowSubtitles", mS.show_subtitles != 0);
	gpBase->mpUserConfig->SetBool("Game", "ShowEffectSubtitles", mS.show_effect_subtitles != 0);
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::OnMapEnter(cLuxMap *apMap)
{
	Reset();
}

void cLuxMessageHandler::OnMapLeave(cLuxMap *apMap)
{

}

//-----------------------------------------------------------------------

void cLuxMessageHandler::StarQuestAddedMessage()
{
	ohpl_ga8_msg_start_quest(&mS);
}


//-----------------------------------------------------------------------

void cLuxMessageHandler::StartPauseMessage(const tWString& asText, bool abYesNo, iLuxMessageCallback *apCallback)
{
	mvLines.clear();

	cVector2f vFontSize(mS.font_size[0], mS.font_size[1]);
    mpFont->GetWordWrapRows(ohpl_ga8_msg_wrap_width(1),ohpl_ga8_msg_line_height(vFontSize.y),vFontSize, gpBase->mpHelpFuncs->ParseString(asText),&mvLines);
	mpCallback = apCallback;

	OhplGa8Buf buf;
	RunEvents(OhplGa8Take(buf.mvRaw, ohpl_ga8_msg_start_pause(&mS, abYesNo, buf.mvRaw, OHPL_GA8_EVMAX)));
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::SetMessage(const tWString& asText, float afTime)
{
	tWString sParsedText = gpBase->mpHelpFuncs->ParseString(asText);
	ohpl_ga8_msg_set_message(&mS, afTime, afTime <=0 ? gpBase->mpHelpFuncs->GetStringDuration(sParsedText) : 0);

	mvMessageRows.clear();

	cVector2f vFontSize(mS.font_size[0], mS.font_size[1]);
	mpFont->GetWordWrapRows(ohpl_ga8_msg_wrap_width(0),ohpl_ga8_msg_line_height(vFontSize.y),vFontSize, sParsedText, &mvMessageRows);
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::Update(float afTimeStep)
{
	OhplGa8Buf buf;
	RunEvents(OhplGa8Take(buf.mvRaw, ohpl_ga8_msg_update(&mS, afTimeStep, buf.mvRaw, OHPL_GA8_EVMAX)));
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::RunEvents(const std::vector<OhplGa8Event> &avEvents)
{
	for(size_t i=0; i<avEvents.size(); ++i)
	{
		const OhplGa8Event &e = avEvents[i];
		switch(e.kind)
		{
		case eOhplGa8Msg_QuestSound:
			gpBase->mpHelpFuncs->PlayGuiSoundData(msQuestAddedSound, eSoundEntryType_Gui);
			break;
		case eOhplGa8Msg_SetUpdateActive:
			gpBase->mpMapHandler->SetUpdateActive(e.a != 0);
			break;
		case eOhplGa8Msg_Callback:
			if(mpCallback) mpCallback->OnPress(e.a != 0);
			break;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::OnDraw(float afFrameTime)
{
	DrawMessage();
	DrawPauseMessage();
	DrawQuestAdded();
}
//-----------------------------------------------------------------------

void cLuxMessageHandler::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	int lAction = aAction == eLuxPlayerAction_Interact ? eOhplGa8MsgAction_Interact :
					(aAction == eLuxPlayerAction_Attack ? eOhplGa8MsgAction_Attack : -1);

	OhplGa8Buf buf;
	RunEvents(OhplGa8Take(buf.mvRaw, ohpl_ga8_msg_do_action(&mS, lAction, abPressed, mpCallback != NULL, buf.mvRaw, OHPL_GA8_EVMAX)));
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::SetPauseMessageActive(bool abX)
{
	OhplGa8Buf buf;
	RunEvents(OhplGa8Take(buf.mvRaw, ohpl_ga8_msg_set_pause(&mS, abX, buf.mvRaw, OHPL_GA8_EVMAX)));
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMessageHandler::DrawQuestAdded()
{
	OhplGa8Buf buf;
	cVector2f vIconSize = mpQuestAddedIcon ? mpQuestAddedIcon->GetActiveSize() : cVector2f(0);
	float vCenter[2] = { gpBase->mvHudVirtualCenterSize.x, gpBase->mvHudVirtualCenterSize.y };
	float vStart[3] = { gpBase->mvHudVirtualStartPos.x, gpBase->mvHudVirtualStartPos.y, gpBase->mvHudVirtualStartPos.z };
	float vIcon[2] = { vIconSize.x, vIconSize.y };
	std::vector<OhplGa8Event> vEvents = OhplGa8Take(buf.mvRaw,
		ohpl_ga8_msg_draw_quest(&mS, mpQuestAddedIcon ? vIcon : NULL, vCenter, vStart, buf.mvRaw, OHPL_GA8_EVMAX));

	for(size_t i=0; i<vEvents.size(); ++i)
	{
		const OhplGa8Event &e = vEvents[i];
		if(e.kind == eOhplGa8Msg_QuestIcon)
			gpBase->mpGameHudSet->DrawGfx(mpQuestAddedIcon, cVector3f(e.v[0],e.v[1],e.v[2]), -1, cColor(1, e.v[8]));
		else
			gpBase->mpGameHudSet->DrawGfx(mpQuestAddedIcon, cVector3f(e.v[0],e.v[1],e.v[2]), -1, cColor(e.v[5], e.v[8]), eGuiMaterial_Additive);
	}
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::DrawMessage()
{
	OhplGa8Buf buf;
	cVector2f vFontSize(mS.font_size[0], mS.font_size[1]);
	std::vector<OhplGa8Event> vEvents = OhplGa8Take(buf.mvRaw, ohpl_ga8_msg_draw_message(&mS, mvMessageRows.size(), buf.mvRaw, OHPL_GA8_EVMAX));
	for(size_t i=0; i<vEvents.size(); ++i)
	{
		const OhplGa8Event &e = vEvents[i];
		gpBase->mpGameHudSet->DrawFont(mvMessageRows[e.a], mpFont,cVector3f(e.v[0],e.v[1],e.v[2]),vFontSize, cColor(1, e.v[8]),eFontAlign_Center);
	}
}

//-----------------------------------------------------------------------

void cLuxMessageHandler::DrawPauseMessage()
{
	OhplGa8Buf buf;
	cVector2f vFontSize(mS.font_size[0], mS.font_size[1]);
	std::vector<OhplGa8Event> vEvents = OhplGa8Take(buf.mvRaw, ohpl_ga8_msg_draw_pause(&mS, mvLines.size(), buf.mvRaw, OHPL_GA8_EVMAX));
	for(size_t i=0; i<vEvents.size(); ++i)
	{
		const OhplGa8Event &e = vEvents[i];
		cVector3f vPos(e.v[0],e.v[1],e.v[2]);
		switch(e.kind)
		{
		case eOhplGa8Msg_PauseBg:
			gpBase->mpGameHudSet->DrawGfx(mpBlackGfx,vPos,cVector2f(e.v[3],e.v[4]),cColor(1, e.v[8]));
			break;
		case eOhplGa8Msg_PauseRow:
			gpBase->mpGameHudSet->DrawFont(mvLines[e.a], mpFont,vPos,vFontSize, cColor(1, e.v[8]));
			break;
		case eOhplGa8Msg_Yes:
			gpBase->mpGameHudSet->DrawFont(mpFont,vPos,vFontSize, cColor(1, e.v[8]),_W("Yes"));
			break;
		case eOhplGa8Msg_YesKey:
			gpBase->mpGameHudSet->DrawFont(mpFont,vPos,cVector2f(e.v[3],e.v[4]), cColor(1, e.v[8]),_W("(%ls)"), gpBase->mpInputHandler->GetInputName("Interact").c_str());
			break;
		case eOhplGa8Msg_No:
			gpBase->mpGameHudSet->DrawFont(mpFont,vPos,vFontSize, cColor(1, e.v[8]),_W("No"));
			break;
		case eOhplGa8Msg_NoKey:
			gpBase->mpGameHudSet->DrawFont(mpFont,vPos,cVector2f(e.v[3],e.v[4]), cColor(1, e.v[8]),_W("(%ls)"), gpBase->mpInputHandler->GetInputName("Attack").c_str());
			break;
		}
	}
}
