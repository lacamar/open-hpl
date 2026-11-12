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

#include "LuxHintHandler.h"

#include "LuxHelpFuncs.h"
#include "input/Gamepad.h"

#include <stdarg.h>
#include <stdlib.h>

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxHintHandler::cLuxHintHandler() : iLuxUpdateable("LuxHintHandler")
{
	mpFont = NULL;

	mS.active = 0;
	mS.alpha = 0;
	mS.show_count = 0;
	cVector2f vFontSize = gpBase->mpMenuCfg->GetVector2f("Hints","FontSize",0);
	ohpl_ga8_hint_init(&mS, gpBase->mpMenuCfg->GetFloat("Hints","YPos",0), vFontSize.x, vFontSize.y,
						gpBase->mpMenuCfg->GetFloat("Hints","TextDurationMul",0));

	//////////////
	// Set up icons for each gamepad button
	cGuiSet* pGuiSet = gpBase->mpGameHudSet;
	cGui* pGui = pGuiSet->GetGui();
#ifdef USE_GAMEPAD
	for(int i = 0; i < eGamepadButton_LastEnum; ++i)
	{
		//////////
		// Reset all the buttons
		mpGamepadButtonIcon[i] = NULL;
	}

# if USE_SDL2
    mpGamepadButtonIcon[eGamepadButton_A]             = pGui->CreateGfxTexture("gamepad_a.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_B]             = pGui->CreateGfxTexture("gamepad_b.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_X]             = pGui->CreateGfxTexture("gamepad_x.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_Y]             = pGui->CreateGfxTexture("gamepad_y.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_LeftShoulder]  = pGui->CreateGfxTexture("gamepad_lb.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_RightShoulder] = pGui->CreateGfxTexture("gamepad_rb.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_Back]          = pGui->CreateGfxTexture("gamepad_select.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_Start]         = pGui->CreateGfxTexture("gamepad_start.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_LeftStick]     = pGui->CreateGfxTexture("gamepad_ls.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_RightStick]    = pGui->CreateGfxTexture("gamepad_rs.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_DpadUp]        = pGui->CreateGfxTexture("gamepad_dpad_u.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_DpadRight]     = pGui->CreateGfxTexture("gamepad_dpad_r.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_DpadLeft]      = pGui->CreateGfxTexture("gamepad_dpad_l.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_DpadDown]      = pGui->CreateGfxTexture("gamepad_dpad_d.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);

	//////////
	// Dpad
	mpGamepadHatIcon[cMath::Log2ToInt(eGamepadHatState_Up)]    = pGui->CreateGfxTexture("gamepad_dpad_u.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadHatIcon[cMath::Log2ToInt(eGamepadHatState_Right)] = pGui->CreateGfxTexture("gamepad_dpad_r.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadHatIcon[cMath::Log2ToInt(eGamepadHatState_Left)]  = pGui->CreateGfxTexture("gamepad_dpad_l.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadHatIcon[cMath::Log2ToInt(eGamepadHatState_Down)]  = pGui->CreateGfxTexture("gamepad_dpad_d.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
# else
	//////////////
	// Set up the buttons
	mpGamepadButtonIcon[eGamepadButton_0] = pGui->CreateGfxTexture("gamepad_a.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_1] = pGui->CreateGfxTexture("gamepad_b.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_2] = pGui->CreateGfxTexture("gamepad_x.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_3] = pGui->CreateGfxTexture("gamepad_y.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_4] = pGui->CreateGfxTexture("gamepad_lb.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_5] = pGui->CreateGfxTexture("gamepad_rb.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_6] = pGui->CreateGfxTexture("gamepad_select.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_7] = pGui->CreateGfxTexture("gamepad_start.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_8] = pGui->CreateGfxTexture("gamepad_ls.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadButtonIcon[eGamepadButton_9] = pGui->CreateGfxTexture("gamepad_rs.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);

	//////////
	// Dpad
	mpGamepadHatIcon[cMath::Log2ToInt(eGamepadHatState_Up)]    = pGui->CreateGfxTexture("gamepad_dpad_u.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadHatIcon[cMath::Log2ToInt(eGamepadHatState_Right)] = pGui->CreateGfxTexture("gamepad_dpad_r.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadHatIcon[cMath::Log2ToInt(eGamepadHatState_Left)]  = pGui->CreateGfxTexture("gamepad_dpad_l.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGamepadHatIcon[cMath::Log2ToInt(eGamepadHatState_Down)]  = pGui->CreateGfxTexture("gamepad_dpad_d.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
# endif

	////////
	// Triggers
	for(int i = 0; i < eGamepadAxis_LastEnum; i++)
	{
		//////////
		// Reset all the sticks
		mpGampadAxisIcon[i * 2 + 0] = NULL;
		mpGampadAxisIcon[i * 2 + 1] = NULL;
	}

# if USE_SDL2
	mpGampadAxisIcon[eGamepadAxis_LeftTrigger * 2 + eGamepadAxisRange_Positive - 1]  = pGui->CreateGfxTexture("gamepad_lt.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGampadAxisIcon[eGamepadAxis_RightTrigger * 2 + eGamepadAxisRange_Positive - 1] = pGui->CreateGfxTexture("gamepad_rt.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
#else
	mpGampadAxisIcon[eGamepadAxis_2 * 2 + eGamepadAxisRange_Positive - 1] = pGui->CreateGfxTexture("gamepad_lt.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
	mpGampadAxisIcon[eGamepadAxis_2 * 2 + eGamepadAxisRange_Negative - 1] = pGui->CreateGfxTexture("gamepad_rt.tga", eGuiMaterial_Alpha, eTextureType_2D, cColor(1,1), false);
#endif

#endif
}

//-----------------------------------------------------------------------

cLuxHintHandler::~cLuxHintHandler()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxHintHandler::LoadFonts()
{
	tString sFontFile = gpBase->mpMenuCfg->GetString("Hints","DefaultFont","");
	mpFont = LoadFont(sFontFile);
}

//-----------------------------------------------------------------------

void cLuxHintHandler::OnStart()
{
	
}

//-----------------------------------------------------------------------

void cLuxHintHandler::Reset()
{
	ohpl_ga8_hint_reset(&mS);
	msCurrentText = _W("");
	m_setHintsGiven.clear();
	m_setHintsBlocked.clear();
}

//-----------------------------------------------------------------------

void cLuxHintHandler::LoadUserConfig()
{
	mS.active = gpBase->mpUserConfig->GetBool("Game", "ShowHints", true);
}

//-----------------------------------------------------------------------

void cLuxHintHandler::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game", "ShowHints", mS.active != 0);
}

//-----------------------------------------------------------------------

void cLuxHintHandler::Update(float afTimeStep)
{
	UpdateHintText(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxHintHandler::OnDraw(float afFrameTime)
{
	DrawHintText(afFrameTime, gpBase->mpGameHudSet);
}

//-----------------------------------------------------------------------

void cLuxHintHandler::DrawHintText(float afFrameTime, cGuiSet *apGuiSet)
{
	if(ohpl_ga8_hint_drawable(&mS, gpBase->mbPTestActivated) == 0) return;

	cVector2f vFontSize(mS.font_size[0], mS.font_size[1]);
	float vLabel[7];
	ohpl_ga8_hint_label(&mS, vLabel);
	cColor hintCol(vLabel[3], vLabel[4], vLabel[5], vLabel[6]);
	apGuiSet->DrawFont(mpFont, cVector3f(vLabel[0],vLabel[1],vLabel[2]),vFontSize,hintCol, eFontAlign_Center, eGuiMaterial_FontNormal,
		_W(" %ls"), kTranslate("Hints", "HINT:").c_str());

	tWStringVec vRows;
	mpFont->GetWordWrapRows(ohpl_ga8_hint_wrap_width(),ohpl_ga8_hint_wrap_line(), vFontSize, msCurrentText, &vRows);
	for(size_t i=0; i<vRows.size(); ++i)
	{
		float vPos[4];
		ohpl_ga8_hint_row(&mS, vRows.size(), i, vPos);
		apGuiSet->DrawFont(mpFont, cVector3f(vPos[0], vPos[1], vPos[2]),vFontSize,cColor(1,mS.alpha), vPos[3] != 0 ? eFontAlign_Center : eFontAlign_Left, eGuiMaterial_FontNormal,
							_W(" %ls"), vRows.size()==1 ? msCurrentText.c_str() : vRows[i].c_str());
	}

#ifdef USE_GAMEPAD
	/////////////
	// Draw all the icons
	for(size_t i = 0; i < mvHintIcons.size(); ++i)
	{
		apGuiSet->DrawGfx(mvHintIcons[i].mpGuiIcon, mvHintIcons[i].mvPosition, mvHintIcons[i].mvSize, cColor(1,mS.alpha), eGuiMaterial_Alpha);
	}
#endif
}

void cLuxHintHandler::UpdateHintText(float afTimeStep)
{
	ohpl_ga8_hint_update(&mS, afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxHintHandler::SetActive(bool abX)
{
	if(ohpl_ga8_hint_set_active(&mS, abX))
	{
		msCurrentText = _W("");
	}
}

//-----------------------------------------------------------------------

bool cLuxHintHandler::Add(const tString &asName, const tWString &asMessage, float afTimeShown)
{
	if(ohpl_ga8_hint_can_add(&mS, m_setHintsBlocked.count(asName)>0, m_setHintsGiven.find(asName) != m_setHintsGiven.end()) == 0) return false;

	m_setHintsGiven.insert(asName);

    msCurrentText = gpBase->mpHelpFuncs->ParseString(asMessage);
	ohpl_ga8_hint_start(&mS, afTimeShown, afTimeShown <=0 ? gpBase->mpHelpFuncs->GetStringDuration(msCurrentText) : 0);

#ifdef USE_GAMEPAD
	ParseStringForGamepadIcons();
#endif

	return true;
}

//-----------------------------------------------------------------------

bool cLuxHintHandler::Remove(const tString &asName)
{
	tStringSetIt it = m_setHintsGiven.find(asName);
	if(it == m_setHintsGiven.end()) return false;

    m_setHintsGiven.erase(it);
	return true;
}

//-----------------------------------------------------------------------

void cLuxHintHandler::Block(const tString &asName)
{
	m_setHintsBlocked.insert(asName);
}

void cLuxHintHandler::UnBlock(const tString &asName)
{
	m_setHintsBlocked.erase(asName);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
void cLuxHintHandler::ParseStringForGamepadIcons()
{
	mvHintIcons.clear();

	int lPosition = 0;
	tWString sOutput;
	ohpl_ga8_strip_icons((const unsigned*)msCurrentText.data(), msCurrentText.size(), &cLuxHintHandler::IconReplace, this, OhplGa8WideSink, &sOutput);

	msCurrentText = sOutput;

	cVector2f vFontSize(mS.font_size[0], mS.font_size[1]);
	if(mvHintIcons.size() > 0)
	{
		size_t lIconIdx = 0;
		lPosition = 0;

		///////////////////
		// Get the correct size and position for each icon
		tWStringVec vRows;
		float fMaxWidth = ohpl_ga8_hint_wrap_width();
		float fStartY = mS.y_pos + vFontSize.y + 3.0f;
		mpFont->GetWordWrapRows(fMaxWidth,ohpl_ga8_hint_wrap_line(), vFontSize, msCurrentText, &vRows);
		wchar_t sRowText[1024];

		for(size_t i=0; i<vRows.size(); ++i)
		{
			///////////////
			// Convert the string
			swprintf(sRowText, 1023, _W(" %ls"), vRows[i].c_str());

			int lCount = 0;
			cVector3f vPos;

			if(vRows.size()==1)
			{
				vPos = cVector3f(400,fStartY,20);
				vPos.x -= mpFont->GetLength(vFontSize, sRowText)/2;
			}
			else
			{
				vPos = cVector3f(400-fMaxWidth*0.5f, fStartY + (float)i* (vFontSize.y+2.0f), 20);
			}

			//////////////////////////////////////////////////////
			// Iterate the characters in string until NULL is found
			while(sRowText[lCount] != 0)
			{
				wchar_t lGlyphNum = ((wchar_t)sRowText[lCount]);
			
				//Check if the glyph is valid (in range)
				if(	lGlyphNum < mpFont->GetFirstChar() || 
					lGlyphNum > mpFont->GetLastChar())
				{
					lCount++;
					continue;
				}
				//Get actual number of the glyph in the font.
				lGlyphNum -= mpFont->GetFirstChar();
			
				//Get glyph data and draw.
				cGlyph *pGlyph = mpFont->GetGlyph(lGlyphNum);

				if(pGlyph)
				{
					cVector2f vSize(pGlyph->mvSize * vFontSize);
					vPos.x += pGlyph->mfAdvance*vFontSize.x;
				}

				if(lPosition == mvHintIcons[lIconIdx].mlCharacterPosition)
				{
					///////////////
					// Place the icon here
					mvHintIcons[lIconIdx].mvPosition = vPos;
					////////////
					// Set the size and aspect ratio
					mvHintIcons[lIconIdx].mvSize = mvHintIcons[lIconIdx].mpGuiIcon->GetImageSize();
					mvHintIcons[lIconIdx].mvSize /= mvHintIcons[lIconIdx].mvSize.y;
					mvHintIcons[lIconIdx].mvSize *= vFontSize;

					if(++lIconIdx == mvHintIcons.size())
					{
						return;
					}
				}

				lPosition++;
				lCount++;
			}
		}
	}
}

//-----------------------------------------------------------------------

void cLuxHintHandler::IconReplace(void *apCtx, const unsigned *apCmd, size_t alLen, int alPos, OhplGa8Sink32 apSink, void *apSinkCtx)
{
	tWString sReplace = ((cLuxHintHandler*)apCtx)->AddGamepadIconAtPosition(tWString((const wchar_t*)apCmd, alLen), alPos);
	apSink(apSinkCtx, (const unsigned*)sReplace.data(), sReplace.size());
}

//-----------------------------------------------------------------------

tWString cLuxHintHandler::AddGamepadIconAtPosition(const tWString& asCommand, int alPosition)
{
	////////////
	// Convert the command from wString to tString
	tString sCommand;
	sCommand.resize(asCommand.size());
	std::copy(asCommand.begin(), asCommand.end(), sCommand.begin());

	////////////////
	// Get icon from command string
	tString sSep = ".";
	tStringVec vInputParts;
	cString::GetStringVec(sCommand, vInputParts, &sSep);

	cLuxHintIcon hintIcon;
	hintIcon.mlCharacterPosition = alPosition;

	if(vInputParts[0] == "GamepadButton")
	{
		//////////
		// See if we have a icon for this button
		eGamepadButton button = iGamepad::StringToButton(vInputParts[1]);

		if(button < eGamepadButton_LastEnum)
		{
			if(mpGamepadButtonIcon[button])
			{
				hintIcon.mpGuiIcon = mpGamepadButtonIcon[button];
				mvHintIcons.push_back(hintIcon);

				if (gpBase->msCurrentLanguage == "chinese.lang")
					return _W("                ");
				return _W("    ");
			}
			else
			{
				/////////////
				// Return the translated command for buttons with no icon
				return kTranslate("ButtonNames", vInputParts[1]);
			}
		}
		else
		{
			return _W(" ");
		}
	}
	else
	{
		eGamepadHat hat = iGamepad::StringToHat(vInputParts[1]);
		eGamepadAxis axis = iGamepad::StringToAxis(vInputParts[1]);
								
		if(hat!=eGamepadHat_LastEnum)
		{
			eGamepadHatState state = iGamepad::StringToHatState(vInputParts[2]);

			if(mpGamepadHatIcon[cMath::Log2ToInt(state)])
			{
				hintIcon.mpGuiIcon = mpGamepadHatIcon[cMath::Log2ToInt(state)];
				mvHintIcons.push_back(hintIcon);
				return _W("    ");
			}
			else
			{
				/////////////
				// Return the translated command for hats with no icon
				return kTranslate("ButtonNames", vInputParts[1]+"."+vInputParts[2]);
			}
		}
		else if(axis!=eGamepadAxis_LastEnum)
		{
			eGamepadAxisRange range = iGamepad::StringToAxisRange(vInputParts[2]);

			if(mpGampadAxisIcon[axis * 2 + range - 1])
			{
				hintIcon.mpGuiIcon = mpGampadAxisIcon[axis * 2 + range - 1];
				mvHintIcons.push_back(hintIcon);
				return _W("      ");
			}
			else
			{
				/////////////
				// Return the translated command for hats with no icon
				return kTranslate("ButtonNames", vInputParts[1]+"."+vInputParts[2]);
			}
		}
		else
		{
			return _W(" ");
		}
	}

	return _W(" ");
}
#endif

//-----------------------------------------------------------------------
