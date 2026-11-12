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

#include "LuxMainMenu_KeyConfig.h"

#include "LuxInputHandler.h"
#include "RustLuxMenu.h"

static_assert(sizeof(cSubActionWrapper) == sizeof(OhplGa7aSub), "cSubActionWrapper layout");
static_assert(offsetof(cSubActionWrapper, mType) == offsetof(OhplGa7aSub, kind), "mType");
static_assert(offsetof(cSubActionWrapper, mGamepadInputType) == offsetof(OhplGa7aSub, gtype), "mGamepadInputType");
static_assert(offsetof(cSubActionWrapper, mlInputId) == offsetof(OhplGa7aSub, id), "mlInputId");
static_assert(offsetof(cSubActionWrapper, mfInputValue) == offsetof(OhplGa7aSub, value), "mfInputValue");

#define OHPL_SUBS(v) ((OhplGa7aSub*)(v).data()), (v).size()
//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::mbGamepadButtonPress = false;

cLuxInputMenuEntry::cLuxInputMenuEntry(cLuxMainMenu_KeyConfig* apWindow, cLuxAction* apAction, iWidget* apParent, const cVector3f& avPos)
{
	mpWindow = apWindow;
	mlActionId = apAction->mlId;

	mbChanged = false;

	mpLAction = mpWindow->mpGuiSet->CreateWidgetLabel(avPos, -1, kTranslate("Actions", apAction->msName), apParent);
	mpLAction->AddCallback(eGuiMessage_MouseEnter, this, kGuiCallback(InputEntryMouseEnter));
	mpLAction->AddCallback(eGuiMessage_MouseLeave, this, kGuiCallback(InputEntryMouseLeave));
	mpLAction->AddCallback(eGuiMessage_GetUINavFocus, this, kGuiCallback(InputEntryMouseEnter));
	mpLAction->AddCallback(eGuiMessage_LoseUINavFocus, this, kGuiCallback(InputEntryMouseLeave));
	//mpLAction->AddCallback(eGuiMessage_GetUINavFocus, this, kGuiCallback(InputEntryMouseMove));
	//mpLAction->AddCallback(eGuiMessage_MouseMove, this, kGuiCallback(InputEntryMouseMove));
	mpLAction->AddCallback(eGuiMessage_TextChange, this, kGuiCallback(InputEntryChange));
	//mpLAction->AddCallback(eGuiMessage_MouseDown, this, kGuiCallback(InputEntryClick));
	//mpLAction->AddCallback(eGuiMessage_KeyPress, this, kGuiCallback(InputEntryKeyPress));

	if(ohpl_ga7a_kc_binds_pad((const unsigned char*)apAction->msName.data(), apAction->msName.size()))
	{
		mpLAction->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(InputEntryUIButtonPress));
	}

	mpLAction->AddCallback(eGuiMessage_UIArrowPress, this, kGuiCallback(InputEntryUIArrowPress));

	mpLAction->SetUserData(this);
	
	///////////////////////////////////////////
	// Set up primary and secondary Key boxes
	for(int i=0; i<eInputMenuEntryPos_LastEnum; ++i)
	{
		cWidgetLabel* pLabel = mpWindow->mpGuiSet->CreateWidgetLabel(cVector3f(mpWindow->mvHeaderPositions.v[i],0,0), mpWindow->mvEntrySize,_W(""), mpLAction);
		pLabel->SetDefaultFontColor(cColor(1,1));

		pLabel->SetTextAlign(eFontAlign_Center);

		if(i == 0)
		{
			////////////
			// It should only be possible to bind mouse to primary
			pLabel->AddCallback(eGuiMessage_MouseEnter, this, kGuiCallback(InputEntryMouseOver));
			pLabel->AddCallback(eGuiMessage_MouseLeave, this, kGuiCallback(InputEntryMouseOver));
			pLabel->AddCallback(eGuiMessage_MouseEnter, this, kGuiCallback(InputEntryMouseEnter));
			pLabel->AddCallback(eGuiMessage_MouseLeave, this, kGuiCallback(InputEntryMouseLeave));
			pLabel->AddCallback(eGuiMessage_MouseDown, this, kGuiCallback(InputEntryClick));
		}
		//pLabel->AddCallback(eGuiMessage_MouseMove, this, kGuiCallback(InputEntryMouseMove));
		pLabel->AddCallback(eGuiMessage_TextChange, this, kGuiCallback(InputEntryChange));
		pLabel->AddCallback(eGuiMessage_KeyPress, this, kGuiCallback(InputEntryKeyPress));
		
		if(ohpl_ga7a_kc_binds_pad((const unsigned char*)apAction->msName.data(), apAction->msName.size()))
		{
			//////////////
			// Not possible to bind lean to gamepad
			pLabel->AddCallback(eGuiMessage_GamepadInput, this, kGuiCallback(InputEntryGamepadButtonPress));
		}
		pLabel->SetUserData(this);
		pLabel->SetUserValue(i);

		// Not drawing bg anymore
		//pLabel->SetDrawBackGround(true);

		mvLKeyInputs.push_back(pLabel);
		mvInputColors.push_back(cColor(1,1));
		mvSubActions.push_back(cSubActionWrapper(eInputDeviceType_LastEnum, -1));
	}
}

cLuxInputMenuEntry::~cLuxInputMenuEntry()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxInputMenuEntry::RetrieveDefaultValue()
{
	tLuxInputVec vDefaultInputs = gpBase->mpInputHandler->GetDefaultInputsByActionId(mlActionId);

	std::vector<OhplGa7aStr> vNames(vDefaultInputs.size());
	std::vector<int> vValues(vDefaultInputs.size());
	for(size_t i=0; i<vDefaultInputs.size(); ++i)
	{
		OhplGa7aStr name = { (const unsigned char*)vDefaultInputs[i]->msInputType.data(), vDefaultInputs[i]->msInputType.size() };
		vNames[i] = name;
		vValues[i] = vDefaultInputs[i]->mlValue;
	}

	int bMacKey = 0;
#ifdef __APPLE__
	bMacKey = mlActionId == eLuxAction_Attack;
#endif
	ohpl_ga7a_kc_fill_default(vNames.data(), vValues.data(), vNames.size(), OHPL_SUBS(mvSubActions), bMacKey, (int)eKey_LeftMeta);

	///////////////////////////////////////////////////////////
	// Set changed to true, as we might have changed stuff
	mbChanged = true;
}

//-----------------------------------------------------------------------

void cLuxInputMenuEntry::RetrieveInitialValue()
{
	cInput* pInput = gpBase->mpEngine->GetInput();
	cAction* pAction = pInput->GetAction(mlActionId);

	std::vector<tString> vStrings;
	for(int i=0;i<pAction->GetSubActionNum();++i)
	{
		if(i>=eInputMenuEntryPos_LastEnum)
			break;
		iSubAction* pSubAction = pAction->GetSubAction(i);
		vStrings.push_back(pSubAction->GetInputType()+"."+pSubAction->GetInputName());
	}
	std::vector<OhplGa7aStr> vNames(vStrings.size());
	for(size_t i=0; i<vStrings.size(); ++i)
	{
		OhplGa7aStr name = { (const unsigned char*)vStrings[i].data(), vStrings[i].size() };
		vNames[i] = name;
	}
	ohpl_ga7a_kc_fill_initial(vNames.data(), vNames.size(), OHPL_SUBS(mvSubActions));
}

//-----------------------------------------------------------------------

void cLuxInputMenuEntry::UpdateAction()
{
	OhplGa7aOp vOps[eInputMenuEntryPos_LastEnum];
	int lOps = ohpl_ga7a_kc_update(OHPL_SUBS(mvSubActions), mbChanged, vOps, eInputMenuEntryPos_LastEnum);
	if(lOps<0)
		return;

	cInput* pInp = gpBase->mpEngine->GetInput();
	cAction* pAction = pInp->GetAction(mlActionId);
	pAction->ClearSubActions();

	for(int i=0;i<lOps;++i)
	{
		const OhplGa7aOp& op = vOps[i];
		switch(op.kind)
		{
		case 1:
			pAction->AddKey((eKey)op.a);
			break;
		case 2:
			pAction->AddMouseButton((eMouseButton)op.a);
			break;
		case 3:
			pAction->AddGamepadButton(0, (eGamepadButton)op.a);
			break;
		case 4:
			pAction->AddGamepadHat(0, (eGamepadHat)op.a, (eGamepadHatState)op.b);
			break;
		case 5:
			pAction->AddGamepadAxis(0, (eGamepadAxis)op.a, (eGamepadAxisRange)op.b, 0.25f);
			break;
		}
	}

	mbChanged = false;
}

//-----------------------------------------------------------------------

void cLuxInputMenuEntry::UpdateEntry()
{
	mpWindow->CheckEmptyAndDuplicateInputs();

	tWStringVec vInputStrings;
	vInputStrings.resize(eInputMenuEntryPos_LastEnum);

	
	for(int i=0;i<eInputMenuEntryPos_LastEnum;++i)
	{
		const cSubActionWrapper& subAction = mvSubActions[i];
		cWidgetLabel* pInput = mvLKeyInputs[i];

		//pInput->SetText(kTranslate("ButtonNames", GetStringFromSubAction(subAction)));
		pInput->SetText(kTranslate("ButtonNames", GetStringFromSubAction(subAction)));
	}
}

//-----------------------------------------------------------------------

void cLuxInputMenuEntry::SetInputKeypress(eInputMenuEntryPos aPos, eKey aKey)
{
	ohpl_ga7a_kc_set(OHPL_SUBS(mvSubActions), aPos, 0, (int)aKey, (int)0);
}

void cLuxInputMenuEntry::SetInputMouseButton(eInputMenuEntryPos aPos, eMouseButton aButton)
{
	ohpl_ga7a_kc_set(OHPL_SUBS(mvSubActions), aPos, 1, (int)aButton, (int)0);
}

void cLuxInputMenuEntry::SetInputGamepadButton(eInputMenuEntryPos aPos, eGamepadButton aButton)
{
	ohpl_ga7a_kc_set(OHPL_SUBS(mvSubActions), aPos, 2, (int)aButton, (int)0);
}

void cLuxInputMenuEntry::SetInputGamepadHat(eInputMenuEntryPos aPos, eGamepadHat aHat, eGamepadHatState aState)
{
	ohpl_ga7a_kc_set(OHPL_SUBS(mvSubActions), aPos, 3, (int)aHat, (int)aState);
}

void cLuxInputMenuEntry::SetInputGamepadAxis(eInputMenuEntryPos aPos, eGamepadAxis aAxis, eGamepadAxisRange aRange)
{
	ohpl_ga7a_kc_set(OHPL_SUBS(mvSubActions), aPos, 4, (int)aAxis, (int)aRange);
}

//-----------------------------------------------------------------------

eInputMenuEntryPos cLuxInputMenuEntry::GetKeyPos(eKey aKey)
{
	return (eInputMenuEntryPos)ohpl_ga7a_kc_key_pos(OHPL_SUBS(mvSubActions), (int)aKey);
}

eInputMenuEntryPos cLuxInputMenuEntry::GetMouseButtonPos(eMouseButton aButton)
{
	return (eInputMenuEntryPos)ohpl_ga7a_kc_mouse_pos(OHPL_SUBS(mvSubActions), (int)aButton);
}

eInputMenuEntryPos cLuxInputMenuEntry::GetGamepadButtonPos(eGamepadButton aButton)
{
	return (eInputMenuEntryPos)ohpl_ga7a_kc_pad_button_pos(OHPL_SUBS(mvSubActions), (int)aButton);
}

eInputMenuEntryPos cLuxInputMenuEntry::GetGamepadHatStatePos(eGamepadHat aHat, eGamepadHatState aState)
{
	return (eInputMenuEntryPos)ohpl_ga7a_kc_pad_hat_pos(OHPL_SUBS(mvSubActions), (int)aHat, (int)aState);
}

eInputMenuEntryPos cLuxInputMenuEntry::GetGamepadAxisPos(eGamepadAxis aAxis, eGamepadAxisRange aRange)
{
	return (eInputMenuEntryPos)ohpl_ga7a_kc_pad_axis_pos(OHPL_SUBS(mvSubActions), (int)aAxis, (int)aRange);
}

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::HasKey(eKey aKey)
{
	return GetKeyPos(aKey)!=eInputMenuEntryPos_LastEnum;
}

bool cLuxInputMenuEntry::HasMouseButton(eMouseButton aButton)
{
	return GetMouseButtonPos(aButton)!=eInputMenuEntryPos_LastEnum;
}

bool cLuxInputMenuEntry::HasGamepadButton(eGamepadButton aButton)
{
	return GetGamepadButtonPos(aButton)!=eInputMenuEntryPos_LastEnum;
}

bool cLuxInputMenuEntry::HasGamepadHatState(eGamepadHat aHat, eGamepadHatState aState)
{
	return GetGamepadHatStatePos(aHat,aState)!=eInputMenuEntryPos_LastEnum;
}

bool cLuxInputMenuEntry::HasGamepadAxis(eGamepadAxis aAxis, eGamepadAxisRange aRange)
{
	return GetGamepadAxisPos(aAxis,aRange)!=eInputMenuEntryPos_LastEnum;
}

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::HasSubActions()
{
	return ohpl_ga7a_kc_has_sub_actions(OHPL_SUBS(mvSubActions)) != 0;
}

//-----------------------------------------------------------------------

tMenuEntryPosVec cLuxInputMenuEntry::GetPosSharingSubAction(cLuxInputMenuEntry* apEntry)
{
	int vPos[eInputMenuEntryPos_LastEnum];
	size_t lNum = ohpl_ga7a_kc_sharing(OHPL_SUBS(mvSubActions), OHPL_SUBS(apEntry->mvSubActions), vPos);

	tMenuEntryPosVec vEntryPositions;
	for(size_t i=0;i<lNum;++i)
		vEntryPositions.push_back((eInputMenuEntryPos)vPos[i]);

	return vEntryPositions;
}

//-----------------------------------------------------------------------

void cLuxInputMenuEntry::SetColor(eInputMenuEntryPos aPos, const cColor& aCol)
{
	if(aPos==eInputMenuEntryPos_LastEnum)
		return;

	mvInputColors[aPos] = aCol;
	mvLKeyInputs[aPos]->SetDefaultFontColor(aCol);
	//mvLKeyInputs[aPos]->SetBackGroundColor(aCol);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

tString cLuxInputMenuEntry::GetStringFromSubAction(const cSubActionWrapper& aSubAction)
{
	tString sLabel;
	ohpl_ga7a_kc_label((const OhplGa7aSub*)&aSubAction, OhplGa7aStrSink, &sLabel);
	return sLabel;
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

eInputMenuEntryPos cLuxInputMenuEntry::GetPosFromInput(iWidget* apWidget)
{
	if(apWidget==NULL) return eInputMenuEntryPos_LastEnum;

	return (eInputMenuEntryPos)apWidget->GetUserValue();
}

//-----------------------------------------------------------------------

void cLuxInputMenuEntry::SaveSubAction(eInputMenuEntryPos aPos, eInputDeviceType aType, int alInputId)
{
	if(aPos==eInputMenuEntryPos_LastEnum)
		return;

	switch(aType)
	{
	case eInputDeviceType_Keyboard:
		SetInputKeypress(aPos, (eKey)alInputId);
		break;
	case eInputDeviceType_Mouse:
		SetInputMouseButton(aPos, (eMouseButton)alInputId);
		break;
	}

	UpdateEntry();
	SetWaiting(false, aPos);
}

void cLuxInputMenuEntry::SaveGamepadSubAction(eInputMenuEntryPos aPos, eGamepadInputType aType, int alInputId, float afValue)
{
	if(aPos==eInputMenuEntryPos_LastEnum)
		return;

	switch(aType)
	{
	case eGamepadInputType_Button:
		SetInputGamepadButton(aPos, (eGamepadButton)alInputId);
		break;
	case eGamepadInputType_Hat:
		{
			int lState = (int)afValue;
			SetInputGamepadHat(aPos, (eGamepadHat)alInputId, (eGamepadHatState)lState);
			break;
		}
	case eGamepadInputType_Axis:
		{
			SetInputGamepadAxis(aPos, (eGamepadAxis)alInputId, iGamepad::GetAxisRangeFromValue(afValue));
			break;
		}
	}

	UpdateEntry();
	SetWaiting(false, aPos);
}

//-----------------------------------------------------------------------

void cLuxInputMenuEntry::SetWaiting(bool abX, eInputMenuEntryPos aPos)
{
	if(aPos==eInputMenuEntryPos_LastEnum)
		return;

	cWidgetLabel* pWaitingInput = abX ? mvLKeyInputs[aPos]: NULL;
	
	mpWindow->SetWaitingInput(pWaitingInput);

	if(pWaitingInput)
		pWaitingInput->SetText(_W("..."));
}

bool cLuxInputMenuEntry::IsWaiting()
{
	if(GetPosFromInput(mpWindow->GetWaitingInput())!=eInputMenuEntryPos_LastEnum)
		return true;

	return false;
}

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::InputEntryMouseOver(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(apWidget->GetSet()->PopUpIsActive())
		return true;

	cColor color = apWidget->GetMouseIsOver()? cColor(1,1,0,1) : mvInputColors[GetPosFromInput(apWidget)];

	cWidgetLabel* pLabel = (cWidgetLabel*)apWidget;
	pLabel->SetDefaultFontColor(color);

	//This worked when bg was visible
	//pLabel->SetBackGroundColor(color);

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryMouseOver);

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::InputEntryMouseEnter(iWidget* apWidget, const cGuiMessageData& aData)
{
	cLuxInputMenuEntry* pEntry = static_cast<cLuxInputMenuEntry*>(apWidget->GetUserData());
	mpWindow->SetCurrentToolTipEntry(this);

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryMouseEnter);


//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::InputEntryMouseLeave(iWidget* apWidget, const cGuiMessageData& aData)
{
	cLuxInputMenuEntry* pEntry = static_cast<cLuxInputMenuEntry*>(apWidget->GetUserData());

	if(pEntry==mpWindow->GetCurrentToolTipEntry())
		mpWindow->SetCurrentToolTipEntry(NULL);
	
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryMouseLeave);


//-----------------------------------------------------------------------

/*
bool cLuxInputMenuEntry::InputEntryMouseMove(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(apWidget->GetMouseIsOver()==false)
		return true;

	cInput* pInput = gpBase->mpEngine->GetInput();
	cAction* pAction = pInput->GetAction(mlActionId);
	mpWindow->SetCurrentToolTipText(kTranslate("Actions", pAction->GetName() + "Tooltip"));

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryMouseMove);
*/

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::InputEntryChange(iWidget* apWidget, const cGuiMessageData& aData)
{
	mbChanged = true;
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryChange);

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::InputEntryClick(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lClick = ohpl_ga7a_kc_click(mpWindow->GetWaitingInput()==NULL, aData.mlVal==eGuiMouseButton_Left);
	if(lClick==1)
	{
		SetWaiting(true, GetPosFromInput(apWidget));
	}
	else if(lClick==2)
	{
		SaveSubAction(GetPosFromInput(apWidget),
					  eInputDeviceType_Mouse,
					  gpBase->mpEngine->GetGui()->TranslateGuiMouseToMouse((eGuiMouseButton)aData.mlVal));
	}
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryClick);

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::InputEntryKeyPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lAct = ohpl_ga7a_kc_key_press(IsWaiting(), aData.mKeyPress.mKey, eKey_Escape);
	if(lAct==1)
	{
		SaveSubAction(GetPosFromInput(apWidget),
				  eInputDeviceType_Keyboard,
				  aData.mKeyPress.mKey);
	}
	else if(lAct==2)
	{
		UpdateEntry();
		SetWaiting(false, GetPosFromInput(apWidget));
		mpWindow->CancelInputSetting();
	}
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryKeyPress);

//-----------------------------------------------------------------------

bool cLuxInputMenuEntry::InputEntryUIArrowPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lPress = ohpl_ga7a_kc_ui_arrow(mbGamepadButtonPress);
	if(lPress & 2) mbGamepadButtonPress = false;
	return (lPress & 1) != 0;
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryUIArrowPress);

bool cLuxInputMenuEntry::InputEntryUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lPress = ohpl_ga7a_kc_ui_button(aData.mlVal==eUIButton_Primary, mbGamepadButtonPress, mpWindow->GetWaitingInput()==NULL);
	if(lPress & 2) mbGamepadButtonPress = false;
	if(lPress & 4) SetWaiting(true, eInputMenuEntryPos_Secondary);
	return (lPress & 1) != 0;
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryUIButtonPress);

//-----------------------------------------------------------------------


bool cLuxInputMenuEntry::InputEntryGamepadButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
{
#if USE_SDL2
	int lCancelButton = eGamepadButton_B;
#else
	int lCancelButton = eGamepadButton_1;
#endif
	int lAct = ohpl_ga7a_kc_pad_press(IsWaiting(), aData.mGamepadInputData.mfInputValue, aData.mGamepadInputData.mInputType == eGamepadInputType_Button,
										aData.mGamepadInputData.mlInputId, lCancelButton);
	if(lAct==1) return false;
	if(lAct==2)
	{
		//////////
		// Cancel when pressing B
		UpdateEntry();
		SetWaiting(false, GetPosFromInput(apWidget));
		mpWindow->CancelInputSetting();
		return true;
	}
	if(lAct==3)
	{
		mbGamepadButtonPress = true;
		SaveGamepadSubAction(GetPosFromInput(apWidget),
								aData.mGamepadInputData.mInputType,
								aData.mGamepadInputData.mlInputId,
								aData.mGamepadInputData.mfInputValue);
	}
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxInputMenuEntry, InputEntryGamepadButtonPress);

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMainMenu_KeyConfig::cLuxMainMenu_KeyConfig(cGuiSet *apGuiSet, cGuiSkin *apGuiSkin) : iLuxMainMenuWindow(apGuiSet, apGuiSkin)
{
	mvWindowSize = cVector2f(620,460);
	mpWaitingInput = NULL;

	mpCurrentTipEntry = NULL;
	mbTipFadeOut = false;
	mbTipFadeRestore = false;
}

//-----------------------------------------------------------------------

cLuxMainMenu_KeyConfig::~cLuxMainMenu_KeyConfig()
{
	for(size_t i=0; i<mvInputs.size(); ++i)
		STLDeleteAll(mvInputs[i]);
	mvInputs.clear();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_KeyConfig::CreateGui()
{
	mbSettingInitialValues = false;

	//////////////////////////
	//Window
	mpWindow = mpGuiSet->CreateWidgetWindow(eWidgetWindowButtonFlag_None,cVector3f(0,0,5),mvWindowSize,kTranslate("KeyConfig","KeyConfigTitle"));
	mpWindow->AddCallback(eGuiMessage_OnUpdate, this, kGuiCallback(WindowOnUpdate));

	float fHeaderSize = 70;
	float fBorderSize = 15;
	float fHeaderFontMul = 1.25f;
	mvEntrySize = cVector2f(140.0f, 16.0f);
	mfEntrySep = 10.0f;
	mvHeaderPositions.x = 240.0f;
	mvHeaderPositions.y = mvHeaderPositions.x +mvEntrySize.x+mfEntrySep;

	//////////////////////////
	//Category selector
	cVector3f vPos = cVector3f(fBorderSize*2,fHeaderSize+fBorderSize,0.1f);
	cWidgetDummy* pGroup = mpGuiSet->CreateWidgetDummy(vPos, mpWindow);
	cWidgetLabel* pLabel = mpGuiSet->CreateWidgetLabel(0, -1, kTranslate("KeyConfig","Category"), pGroup);
	pLabel->SetDefaultFontSize(pLabel->GetDefaultFontSize()*fHeaderFontMul);
	pLabel->SetAutogenerateSize(true);

	mpCBCategory = mpGuiSet->CreateWidgetComboBox(cVector3f(pLabel->GetSize().x + 10, 0 ,0), cVector2f(175,25), _W(""), pGroup);
	mpCBCategory->AddItem(kTranslate("KeyConfig","Movement"));
	mpCBCategory->AddItem(kTranslate("KeyConfig","Actions"));
	mpCBCategory->AddItem(kTranslate("KeyConfig","Misc"));
	mpCBCategory->AddCallback(eGuiMessage_SelectionChange, this, kGuiCallback(CategoryChange));
	mpCBCategory->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(CategorySelector_OnUIButtonPress));
	mpCBCategory->SetGlobalUIInputListener(true);

	/////////////////////////////////////////////////////
	// Input panes
	vPos = pGroup->GetLocalPosition() + cVector3f(fBorderSize, pGroup->GetSize().y + 5.0f, 0);

	pLabel = mpGuiSet->CreateWidgetLabel(vPos, -1, kTranslate("KeyConfig", "HeaderAction"), mpWindow);
	pLabel->SetDefaultFontSize(pLabel->GetDefaultFontSize()*fHeaderFontMul);

	pLabel = mpGuiSet->CreateWidgetLabel(vPos + cVector3f(mvHeaderPositions.x,0,0), mvEntrySize, kTranslate("KeyConfig", "HeaderPrimary"), mpWindow);
	pLabel->SetDefaultFontSize(pLabel->GetDefaultFontSize()*fHeaderFontMul);
	pLabel->SetTextAlign(eFontAlign_Center);

	pLabel = mpGuiSet->CreateWidgetLabel(vPos + cVector3f(mvHeaderPositions.y,0,0), mvEntrySize, kTranslate("KeyConfig", "HeaderSecondary"), mpWindow);
	pLabel->SetDefaultFontSize(pLabel->GetDefaultFontSize()*fHeaderFontMul);
	pLabel->SetTextAlign(eFontAlign_Center);

	vPos.y += pLabel->GetSize().y+10;

	/////////////////////////
	//Key groups
	mvDKeyGroups.resize(eLuxActionCategory_LastEnum);
	for(size_t i=0; i<mvInputs.size(); ++i)
		STLDeleteAll(mvInputs[i]);
	mvInputs.clear();

	//For each category
	for(int cat = eLuxActionCategory_Movement; cat!= eLuxActionCategory_LastEnum; ++cat)
	{
		tInputEntryVec vInputs;

		cWidgetDummy* pGroup = mpGuiSet->CreateWidgetDummy(vPos, mpWindow);
		tLuxActionVec vActions = gpBase->mpInputHandler->GetActionsByCategory((eLuxActionCategory)cat);

		cVector3f vPosInGroup = cVector3f(0,0,0.1f);

		// Iterate every action in category
		for(size_t i=0;i<vActions.size();++i)
		{
			cLuxAction* pLuxAction = vActions[i];
			if(pLuxAction->mbConfigurable==false)
				continue;

			cLuxInputMenuEntry* pEntry = CreateInputEntry(pLuxAction, pGroup, vPosInGroup);
			vInputs.push_back(pEntry);

			vPosInGroup.y += pEntry->mvLKeyInputs[0]->GetSize().y + 10;
		}

		// Set up focus navigation
		for(size_t i=0;i<vInputs.size();++i)
		{
			cLuxInputMenuEntry* pEntry = vInputs[i];
			cLuxInputMenuEntry* pPrevEntry = NULL;
			cLuxInputMenuEntry* pNextEntry = NULL;

			int lPrev = i-1;
			int lNext = i+1;

			if(lPrev>=0)
				pPrevEntry = vInputs[lPrev];
			if(lNext<(int)vInputs.size())
				pNextEntry = vInputs[lNext];

			if(pPrevEntry)
			{
				pEntry->mpLAction->SetFocusNavigation(eUIArrow_Up, pPrevEntry->mpLAction);
			}

			if(pNextEntry)
			{
				pEntry->mpLAction->SetFocusNavigation(eUIArrow_Down, pNextEntry->mpLAction);
			}
		}

		mvDKeyGroups[cat] = pGroup;
		mvInputs.push_back(vInputs);
	}

	//////////////////////////
	//Buttons
	float fButtonWidth = 80;
	float fButtonSepp = 3;
	vPos.x = 30;
	vPos.y = mpWindow->GetSize().y - 25 - 10;

	//Tip Label
	mpLTip = mpGuiSet->CreateWidgetLabel(vPos + cVector3f(10,0,0), cVector2f(400,30), _W(""), mpWindow);
	mpLTip->SetDefaultFontColor(cColor(1,1));
	mpLTip->SetWordWrap(true);
	mpLTip->SetClipActive(true);
	mpLTip->SetScrollSpeedMul(4.0f);
	mpLTip->SetDrawBackGround(true);
	mpLTip->SetBackGroundColor(cColor(0, 0.5f));

	//Set default keys
	cWidgetButton* pButton = mpGuiSet->CreateWidgetButton(vPos + cVector3f(10,-35,0), cVector2f(fButtonWidth,25), kTranslate("KeyConfig","SetDefault"), mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(PressSetDefault));
	float fSetDefaultsWidth = pButton->GetDefaultFontType()->GetLength(pButton->GetDefaultFontSize(), pButton->GetText().c_str());
	pButton->SetSize(cVector2f(fSetDefaultsWidth+20, pButton->GetSize().y));
	mpBDefaultKeys = pButton;


	//Save changes
	vPos.x = mpWindow->GetSize().x - fButtonWidth*2-fButtonSepp-5;
	mpBOK = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,30),kTranslate("Global","OK"),mpWindow);
	mpBOK->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressOK));

	//Cancel
	vPos.x += fButtonWidth + fButtonSepp;
	mpBCancel = mpGuiSet->CreateWidgetButton(vPos,cVector2f(fButtonWidth,30),kTranslate("Global","Cancel"),mpWindow);
	mpBCancel->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressCancel));
	mpBCancel->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(UIPressCancel));
	mpBCancel->SetGlobalUIInputListener(true);

	mpCBCategory->SetFocusNavigation(eUIArrow_Down, mvInputs[0].front()->mpLAction);

	////////////////////////////////////////////////////////
	// Last focus nav settings
	// Set focus nav linking to DefaultKeys button
	for(size_t cat=0; cat<mvInputs.size(); ++cat)
	{
		iWidget* pWidget = mvInputs[cat].back()->mpLAction;

		pWidget->SetFocusNavigation(eUIArrow_Down, mpBDefaultKeys);

		pWidget = mvInputs[cat].front()->mpLAction;

		pWidget->SetFocusNavigation(eUIArrow_Up, mpCBCategory);
	}
	
	mpBDefaultKeys->SetFocusNavigation(eUIArrow_Up, mpCBCategory);
	mpBDefaultKeys->SetFocusNavigation(eUIArrow_Down, mpBOK);
	mpBOK->SetFocusNavigation(eUIArrow_Up, mpBDefaultKeys);
	mpBOK->SetFocusNavigation(eUIArrow_Right, mpBCancel);
	mpBCancel->SetFocusNavigation(eUIArrow_Up, mpBDefaultKeys);
	mpBCancel->SetFocusNavigation(eUIArrow_Left, mpBOK);
}

//-----------------------------------------------------------------------

void cLuxMainMenu_KeyConfig::ExitPressed()
{
	int lExit = ohpl_ga7a_kc_exit(mpGuiSet->PopUpIsActive(), mbInputSettingCancelled);
	if(lExit==0) return;
	if(lExit==1)
	{
		mbInputSettingCancelled = false;
		return;
	}
	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_Options);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_KeyConfig::OnSetActive(bool abX)
{
	if(abX)
	{
		mpGuiSet->PushAttentionWidget();
		mpGuiSet->PushFocusedWidget();
		mpGuiSet->PushDefaultFocusNavWidget();

		SetInitialValues();

		mpCBCategory->SetSelectedItem(-1);
		mpCBCategory->SetSelectedItem(0);
	}
	else
	{
		//mpGuiSet->SetDrawMouse(true);
#ifdef USE_GAMEPAD
		if(gpBase->mpInputHandler->IsGamepadPresent() == false)
			mpGuiSet->SetDrawMouse(true);
#endif
	}

	mpGuiSet->SetAttentionWidget(abX? mpWindow : NULL);
}

//-----------------------------------------------------------------------

cLuxInputMenuEntry* cLuxMainMenu_KeyConfig::CreateInputEntry(cLuxAction* apAction, iWidget* apParent, const cVector3f& avPos)
{
	cLuxInputMenuEntry* pEntry = hplNew(cLuxInputMenuEntry,(this, apAction, apParent, avPos));

	return pEntry;
}

//-----------------------------------------------------------------------

void cLuxMainMenu_KeyConfig::SetCurrentToolTipEntry(cLuxInputMenuEntry* apEntry)
{
	if(apEntry==mpCurrentTipEntry)
	{
		return;
	}

	mpCurrentTipEntry = apEntry;

	tWString sTip;

	if(mpCurrentTipEntry)
	{
		cInput* pInput = gpBase->mpEngine->GetInput();
		cAction* pAction = pInput->GetAction(apEntry->mlActionId);
		sTip = kTranslate("Actions", pAction->GetName() + "Tooltip");

		mpLTip->SetText(sTip);
	}

	int lFlags = ohpl_ga7a_kc_tip_flags(sTip.empty());
	mbTipFadeOut = (lFlags & 1) != 0;
	mbTipFadeRestore = (lFlags & 2) != 0;

	//mpLTip->SetText(sTip);
}

//-----------------------------------------------------------------------

void cLuxMainMenu_KeyConfig::ApplyChanges()
{
	cInput* pInput = gpBase->mpEngine->GetInput();
	for(size_t i=0; i<mvInputs.size(); ++i)
	{
		tInputEntryVecIt it = mvInputs[i].begin();
		tString sSeparator = ",";
		for(;it!=mvInputs[i].end();++it)
		{
			cLuxInputMenuEntry* pEntry = *it;
			pEntry->UpdateAction();
		}
	}
}

//-----------------------------------------------------------------------

void cLuxMainMenu_KeyConfig::SetDefaultValues()
{
	mbSettingInitialValues = true;

	for(size_t i=0; i<mvInputs.size(); ++i)
	{
		
		tInputEntryVecIt it = mvInputs[i].begin();
		for(;it!=mvInputs[i].end();++it)
		{
			cLuxInputMenuEntry* pEntry = *it;
			pEntry->RetrieveDefaultValue();
			pEntry->UpdateEntry();
		}

	}

	mbSettingInitialValues = false;

	CheckEmptyAndDuplicateInputs();
}

//-----------------------------------------------------------------------

void cLuxMainMenu_KeyConfig::SetInitialValues()
{
	mbSettingInitialValues = true;

	for(size_t i=0; i<mvInputs.size(); ++i)
	{
		tInputEntryVecIt it = mvInputs[i].begin();
		for(;it!=mvInputs[i].end();++it)
		{
			cLuxInputMenuEntry* pEntry = *it;
			pEntry->RetrieveInitialValue();
			pEntry->UpdateEntry();
		}
	}

	mbSettingInitialValues = false;

	CheckEmptyAndDuplicateInputs();
}

//-----------------------------------------------------------------------

void cLuxMainMenu_KeyConfig::SetWaitingInput(iWidget *apWidget)
{
	if(ohpl_ga7a_kc_set_waiting(apWidget!=NULL, mpWaitingInput!=NULL)==0)
		return;

	mpWaitingInput = apWidget;
	mbWasWaitingOnInput = true;

	if(apWidget)
	{
		mpGuiSet->PushAttentionWidget();
		mpGuiSet->PushFocusedWidget();

		mpGuiSet->SetAttentionWidget(apWidget);
	}
	else
	{
		mpGuiSet->PopAttentionWidget();
		mpGuiSet->PopFocusedWidget();
	}
	
	mpGuiSet->SetMouseMovementEnabled(apWidget==NULL);
}

//-----------------------------------------------------------------------

static void OhplGa7aSetEntryColor(void *apCtx, size_t alEntry, int alPos, int abRed)
{
	cLuxInputMenuEntry *pEntry = ((cLuxInputMenuEntry**)apCtx)[alEntry];
	pEntry->SetColor((eInputMenuEntryPos)alPos, abRed ? cColor(1,0,0,1) : cColor(1,1));
}

void cLuxMainMenu_KeyConfig::CheckEmptyAndDuplicateInputs()
{
	if(mbSettingInitialValues)
		return;

	std::vector<cLuxInputMenuEntry*> vEntries;
	std::vector<size_t> vCounts;
	std::vector<OhplGa7aSub> vSubs;
	for(size_t cat=0; cat<mvInputs.size(); ++cat)
	{
		vCounts.push_back(mvInputs[cat].size());
		for(size_t i=0; i<mvInputs[cat].size(); ++i)
		{
			vEntries.push_back(mvInputs[cat][i]);
			const std::vector<cSubActionWrapper>& vEntrySubs = mvInputs[cat][i]->mvSubActions;
			for(size_t j=0; j<vEntrySubs.size(); ++j)
				vSubs.push_back(((const OhplGa7aSub*)vEntrySubs.data())[j]);
		}
	}

	mbInvalidInputs = false;
	mbInvalidInputs = ohpl_ga7a_kc_check(vSubs.data(), vSubs.size(), vCounts.data(), vCounts.size(), OhplGa7aSetEntryColor, vEntries.data()) != 0;
}

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::WindowOnUpdate(iWidget* apWidget, const cGuiMessageData& aData)
{
	//mpGuiSet->SetDrawMouse(mpWaitingInput==NULL);
	//gpBase->SetDrawOnLiveCursor(mpWaitingInput==NULL);
	
	if(mpWaitingInput == NULL)
		mbWasWaitingOnInput = false;

	///////////////////////////////////////////////////
	// If there is a popup active, dont update tips
	if(mpGuiSet->PopUpIsActive())
		return true;

	////////////////////////////////////////////////////
	// Update Tip label fade
	cColor labelCol = mpLTip->GetDefaultFontColor();
	int lFlags = (mbTipFadeOut ? 1 : 0) | (mbTipFadeRestore ? 2 : 0);
	float vAlphas[2];
	size_t lSets = ohpl_ga7a_kc_tip_fade(labelCol.a, &lFlags, aData.mfVal, vAlphas);
	mbTipFadeOut = (lFlags & 1) != 0;
	mbTipFadeRestore = (lFlags & 2) != 0;
	for(size_t i=0; i<lSets; ++i)
	{
		labelCol.a = vAlphas[i];
		mpLTip->SetDefaultFontColor(labelCol);
	}

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, WindowOnUpdate);

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::CategoryChange(iWidget* apWidget, const cGuiMessageData& aData)
{
	for(int i=eLuxActionCategory_Movement;i!=eLuxActionCategory_LastEnum;++i)
	{
		bool bActive = ohpl_ga7a_kc_category_active(aData.mlVal, i) != 0;
        mvDKeyGroups[i]->SetVisible(bActive);
		mvDKeyGroups[i]->SetEnabled(bActive);

		if(bActive)
		{
			iWidget* pFocus = mvInputs[i-1].front()->mpLAction;
			iWidget* pTailWidget = mvInputs[i-1].back()->mpLAction;

			mpGuiSet->SetDefaultFocusNavWidget(pFocus);
			mpGuiSet->SetFocusedWidget(pFocus);

			mpCBCategory->SetFocusNavigation(eUIArrow_Down, pFocus);
			mpBDefaultKeys->SetFocusNavigation(eUIArrow_Up, pTailWidget);
		}
	}
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, CategoryChange);

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::CategorySelector_OnUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData)
{
	int lCat = ohpl_ga7a_kc_category_button(mbWasWaitingOnInput, aData.mlVal, eUIButton_PrevPage, eUIButton_NextPage,
											mpCBCategory->GetSelectedItem(), mpCBCategory->GetItemNum());
	if(lCat<0)
		return false;
	mpCBCategory->SetSelectedItem(lCat);
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, CategorySelector_OnUIButtonPress);

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::PressSetDefault(iWidget* apWidget, const cGuiMessageData& aData)
{
	mpGuiSet->CreatePopUpMessageBox(kTranslate("Global", "Warning"), 
											kTranslate("KeyConfig", "LoadDefaultsMessage"), 
											kTranslate("Global", "OK"), 
											kTranslate("Global", "Cancel"), 
											this, kGuiCallback(LoadDefaultsCallback));
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, PressSetDefault);

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::PressOK(iWidget* apWidget, const cGuiMessageData& aData)
{
	if(mbInvalidInputs)
	{
		mpGuiSet->CreatePopUpMessageBox(kTranslate("Global", "Warning"), 
										kTranslate("KeyConfig", "InvalidInputMessage"), 
										kTranslate("Global", "OK"), 
										kTranslate("Global", "Cancel"), 
										this, kGuiCallback(ExitCallback));
	}
	else
	{
		ExitCallback(NULL, cGuiMessageData(0));
	}

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, PressOK);

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::PressCancel(iWidget* apWidget, const cGuiMessageData& aData)
{
	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_Options);

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, PressCancel);

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::UIPressCancel(iWidget* apWidget, const cGuiMessageData& aData)
{
	///////////////
	// Check if this windows is or has been waiting for input his frame
	if(ohpl_ga7a_kc_ui_cancel(mbWasWaitingOnInput, aData.mlVal == eUIButton_Secondary))
	{
		gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_Options);
		return true;
	}
	return false; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, UIPressCancel);

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::LoadDefaultsCallback(iWidget* apWidget, const cGuiMessageData& aData)
{
	bool bOkPressed = aData.mlVal==0? true : false;
	if(bOkPressed)
		SetDefaultValues();
	
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, LoadDefaultsCallback);

//-----------------------------------------------------------------------

bool cLuxMainMenu_KeyConfig::ExitCallback(iWidget* apWidget, const cGuiMessageData& aData)
{
	bool bOkPressed = aData.mlVal==0? true : false;
	if(bOkPressed)
	{
		ApplyChanges();

		gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_Options);
	}

	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_KeyConfig, ExitCallback);

//-----------------------------------------------------------------------
