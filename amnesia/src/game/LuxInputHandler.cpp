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

#include "LuxInputHandler.h"

#include "RustLuxBase.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxPlayerHelpers.h"

#include "LuxPreMenu.h"
#include "LuxMainMenu.h"
#include "LuxCredits.h"
#include "LuxDemoEnd.h"

#include "LuxInventory.h"
#include "LuxJournal.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"
#include "LuxSaveHandler.h"
#include "LuxEffectHandler.h"
#include "LuxConfigHandler.h"
#include "LuxLoadScreenHandler.h"

#include "LuxDebugHandler.h"

//////////////////////////////////////////////////////////////////////////
// ACTION LISTS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

// The action table and the default bindings live in Rust; these copies hand out stable pointers.
static std::vector<cLuxAction> gvLuxActions;
static std::vector<cLuxInput> gvLuxInputs;

static void LuxBuildInputTables()
{
	if(gvLuxActions.empty() == false) return;

	tString sName;
	auto emit = [&](uint32_t tag, const void *p, size_t n) { sName = OhplGa9EmitS(p, n); };

	gvLuxActions.reserve(ohpl_ga9_action_count());
	OhplGa9Action action;
	for(size_t i=0; ohpl_ga9_action(i, &action, OhplGa9EmitOf<decltype(emit)>::Fn(), &emit); ++i)
		gvLuxActions.push_back(cLuxAction(sName, action.id, action.configurable, (eLuxActionCategory)action.cat));

	gvLuxInputs.reserve(ohpl_ga9_input_count());
	int lValue, lActionId;
	for(size_t i=0; ohpl_ga9_input(i, &lValue, &lActionId, OhplGa9EmitOf<decltype(emit)>::Fn(), &emit); ++i)
		gvLuxInputs.push_back(cLuxInput(sName, lValue, lActionId));
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxInputHandler::cLuxInputHandler() : iLuxUpdateable("LuxInputHandler")
{
	////////////////////////////////////
	// Get needed engine modules
	mpInput = gpBase->mpEngine->GetInput();
	mpGraphics = gpBase->mpEngine->GetGraphics();

	////////////////////////////////////
	// Game settings init
	mlMaxSmoothMousePos = gpBase->mpGameCfg->GetInt("Input","MaxSmoothMousePos",0);
	mfPrevSmoothMousePosMul = gpBase->mpGameCfg->GetFloat("Input","PrevSmoothMousePosMul",0);
	mpSmooth = ohpl_ga9_smooth_new(mlMaxSmoothMousePos, mfPrevSmoothMousePosMul);
	mMouseAct.last_x = 0;
	mMouseAct.last_y = 0;
	mMouseAct.active_at = -1;

#ifdef USE_GAMEPAD
	////////////////////////////////////
	// Set up gamepad
	SetUpGamepad();
#endif

	////////////////////////////////////
	// Create the actions
	CreateActions();

	////////////////////////////////////
	// Variable init
	mState = eLuxInputState_Game;
}

//-----------------------------------------------------------------------

cLuxInputHandler::~cLuxInputHandler()
{
	ohpl_ga9_smooth_free(mpSmooth);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxInputHandler::LoadUserConfig()
{
	OhplGa9Settings settings;
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		if(op >= eOhplGa9Op_ActionCreate) ActionCall(op, a, r);
		else if(op >= eOhplGa9Op_Cfg2) OhplGa9CfgCall(gpBase->mpUserKeyConfig, op - eOhplGa9Op_Cfg2, a, r);
		else OhplGa9CfgCall(gpBase->mpUserConfig, op, a, r);
	};
	ohpl_ga9_load_user_config(&call, OhplGa9CallOf<decltype(call)>::Fn(), &settings);

	mbInvertMouse = settings.invert_mouse;
	mbSmoothMouse = settings.smooth_mouse;
	mfMouseSensitivity = settings.mouse_sensitivity;
	mfGamepadWalkSensitivity = settings.gamepad_walk_sensitivity;
	mfGamepadLookSensitivity = settings.gamepad_look_sensitivity;
	mbGamepadLookInvert = settings.gamepad_look_invert;
}

void cLuxInputHandler::ActionCall(uint32_t op, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	switch(op)
	{
	case eOhplGa9Op_ActionCreate: mpInput->CreateAction(OhplGa9GetS(a[0]), (int)a[1].i); break;
	case eOhplGa9Op_ActionClear: mpInput->GetAction((int)a[0].i)->ClearSubActions(); break;
	case eOhplGa9Op_ActionAdd:
		{
			cAction *pAction = mpInput->GetAction((int)a[0].i);
			int lA = (int)a[2].i, lB = (int)a[3].i;
			switch(a[1].i)
			{
			case 0: pAction->AddKey((eKey)lA); break;
			case 1: pAction->AddMouseButton((eMouseButton)lA); break;
			case 2: pAction->AddGamepadButton(0, (eGamepadButton)lA); break;
			case 3: pAction->AddGamepadHat(0, (eGamepadHat)lA, (eGamepadHatState)lB); break;
			case 4: pAction->AddGamepadAxis(0, (eGamepadAxis)lA, (eGamepadAxisRange)lB, 0.25f); break;
			}
		}
		break;
	case eOhplGa9Op_ActionCount: r->i = mpInput->GetAction((int)a[0].i)->GetSubActionNum(); break;
	case eOhplGa9Op_ActionType: OhplGa9RetS(r, mpInput->GetAction((int)a[0].i)->GetSubAction((size_t)a[1].i)->GetInputType()); break;
	case eOhplGa9Op_ActionInput: OhplGa9RetS(r, mpInput->GetAction((int)a[0].i)->GetSubAction((size_t)a[1].i)->GetInputName()); break;
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::SaveUserConfig()
{
	OhplGa9Settings settings;
	settings.invert_mouse = mbInvertMouse;
	settings.smooth_mouse = mbSmoothMouse;
	settings.gamepad_look_invert = mbGamepadLookInvert;
	settings.mouse_sensitivity = mfMouseSensitivity;
	settings.gamepad_walk_sensitivity = mfGamepadWalkSensitivity;
	settings.gamepad_look_sensitivity = mfGamepadLookSensitivity;

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		if(op >= eOhplGa9Op_ActionCreate) ActionCall(op, a, r);
		else if(op >= eOhplGa9Op_Cfg2) OhplGa9CfgCall(gpBase->mpUserKeyConfig, op - eOhplGa9Op_Cfg2, a, r);
		else OhplGa9CfgCall(gpBase->mpUserConfig, op, a, r);
	};
	ohpl_ga9_save_user_config(&call, OhplGa9CallOf<decltype(call)>::Fn(), &settings);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::OnStart()
{
	mpPlayer = gpBase->mpPlayer;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::Update(float afTimeStep)
{
	///////////////////////////////////
	// Update input for current state
	UpdateGlobalInput();

	switch(mState)
	{
	//Game
	case eLuxInputState_Game: UpdateGameInput(); break;
	//Main Menu
	case eLuxInputState_MainMenu: UpdateMainMenuInput(); break;
	//Pre Menu
	case eLuxInputState_PreMenu: UpdatePreMenuInput(); break;
	//Inventory
	case eLuxInputState_Inventory: UpdateInventoryInput(); break;
	//Journal
	case eLuxInputState_Journal: UpdateJournalInput(); break;
	//Debug
	case eLuxInputState_Debug: UpdateDebugInput(); break;
	//Credits
	case eLuxInputState_Credits: UpdateCreditsInput(); break;
	//Demo End
	case eLuxInputState_DemoEnd: UpdateDemoEndInput(); break;
	//Load Screen
	case eLuxInputState_LoadScreen: UpdateLoadScreenInput(); break;
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::Reset()
{

}

//-----------------------------------------------------------------------

void cLuxInputHandler::OnPostRender(float afFrameTime)
{
	//Turn of logging so it only happens one frame!
	gpBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbLog = false;
}

//-----------------------------------------------------------------------

tWString gsInvalidActionString = _W("InvalidAction");

tWString cLuxInputHandler::GetInputName(const tString& asActionName)
{
	cAction *pAction = mpInput->GetAction(asActionName);
	if(pAction == NULL) return gsInvalidActionString;

    iSubAction *pSubAction = pAction->GetSubAction(0);

	return cString::To16Char(pSubAction->GetInputName());
}

//-----------------------------------------------------------------------

void cLuxInputHandler::ChangeState(eLuxInputState aState)
{
	mState = aState;
	

	//This rests all actions so changing states does not give unwanted clicks!
	mpInput->ResetActionsToCurrentState();

	//Reset some stuff
	ohpl_ga9_smooth_reset(mpSmooth);	//Every state needs new smoothing!

	cVector2l vAbsPos = mpInput->GetMouse()->GetAbsPosition();
	mMouseAct.last_x = vAbsPos.x;
	mMouseAct.last_y = vAbsPos.y;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::SetMouseSensitivity(float afX)
{
	if(mfMouseSensitivity==afX)
		return;

	mfMouseSensitivity = afX;
}

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
void cLuxInputHandler::SetGamepadLookSensitivity(float afX)
{
	if(mfGamepadLookSensitivity==afX)
		return;

	mfGamepadLookSensitivity = afX;
}
#endif

//-----------------------------------------------------------------------

cLuxAction* cLuxInputHandler::GetActionByName(const tString& asName)
{
	LuxBuildInputTables();

	int lIdx = ohpl_ga9_action_by_name((const unsigned char*)asName.data(), asName.size());

	return lIdx < 0 ? NULL : &gvLuxActions[lIdx];
}

//-----------------------------------------------------------------------

cLuxAction* cLuxInputHandler::GetActionById(int alId)
{
	LuxBuildInputTables();

	int lIdx = ohpl_ga9_action_by_id(alId);

	return lIdx < 0 ? NULL : &gvLuxActions[lIdx];
}

//-----------------------------------------------------------------------

tLuxActionVec cLuxInputHandler::GetActionsByCategory(eLuxActionCategory aCat)
{
	LuxBuildInputTables();

	std::vector<int> vIdx(gvLuxActions.size());
	size_t lNum = ohpl_ga9_actions_by_category(aCat, vIdx.data(), vIdx.size());

	tLuxActionVec vActions;
	for(size_t i=0; i<lNum; ++i)
		vActions.push_back(&gvLuxActions[vIdx[i]]);

	return vActions;
}

//-----------------------------------------------------------------------

tLuxInputVec cLuxInputHandler::GetDefaultInputsByActionId(int alId)
{
	LuxBuildInputTables();

	std::vector<int> vIdx(gvLuxInputs.size());
	size_t lNum = ohpl_ga9_default_inputs(alId, vIdx.data(), vIdx.size());

	tLuxInputVec vInputs;
	for(size_t i=0; i<lNum; ++i)
		vInputs.push_back(&gvLuxInputs[vIdx[i]]);

	return vInputs;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::ResetSmoothMousePos()
{
	ohpl_ga9_smooth_reset(mpSmooth);
}

//-----------------------------------------------------------------------

cVector2f cLuxInputHandler::GetSmoothMousePos(const cVector2f& avRelPosMouse)
{
	float vOut[2];
	ohpl_ga9_smooth_push(mpSmooth, avRelPosMouse.x, avRelPosMouse.y, vOut);

	return cVector2f(vOut[0], vOut[1]);
}

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
bool cLuxInputHandler::IsGamepadPresent()
{
	return mpPad!=NULL;
}
#endif

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
void cLuxInputHandler::AppDeviceWasPlugged()
{
	SetUpGamepad();
}
#endif

#ifdef USE_GAMEPAD
void cLuxInputHandler::AppDeviceWasRemoved()
{
	SetUpGamepad();
}
#endif

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGlobalInput()
{
	/////////////////
	// ScreenShot
	if(mpInput->BecameTriggerd(eLuxAction_ScreenShot))
	{
		tWString sFileName;
		tWString sScreenShotDir = cString::AddSlashAtEndW(cPlatform::GetSystemSpecialPath(eSystemPath_XDGPictures)) + _W("OpenHPL");
		if (cPlatform::FolderExists(sScreenShotDir) == false) cPlatform::CreateFolder(sScreenShotDir);

		auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r) { r->i = cPlatform::FileExists(OhplGa9GetW(a[0])); };
		auto emit = [&](uint32_t tag, const void *p, size_t n) { sFileName = OhplGa9EmitW(p, n); };
		ohpl_ga9_screenshot_name((const uint32_t*)sScreenShotDir.data(), sScreenShotDir.size(),
								(const unsigned char*)gpBase->mpConfigHandler->msScreenShotExt.data(), gpBase->mpConfigHandler->msScreenShotExt.size(),
								&call, OhplGa9CallOf<decltype(call)>::Fn(), OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

		cEngine *pEngine = gpBase->mpEngine;
		
		cBitmap *pBmp = pEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
		pEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(pBmp,sFileName,0);
		hplDelete(pBmp);
	}

	/////////////////
	// Debug output
	if(mpInput->BecameTriggerd(eLuxAction_PrintInfo))
	{
		gpBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbLog = true;
	}

	/////////////////
	// Send to GUI
	if(CurrentStateSendsInputToGui())
	{
		cGui *pGui = gpBase->mpEngine->GetGui();

		//Key presses
		while(mpInput->GetKeyboard()->KeyIsPressed())
		{
			cKeyPress key = mpInput->GetKeyboard()->GetKey();
			pGui->SendKeyPress(key);

			// sections with a widget handle their own input
			if(mState==eLuxInputState_PreMenu && gpBase->mpPreMenu->IsContinueButtonVisible()==false)
			{
				gpBase->mpPreMenu->ButtonPressed();
			}
		}

		//Mouse movement
		pGui->SendMousePos(mpInput->GetMouse()->GetAbsPosition(), mpInput->GetMouse()->GetRelPosition());

		
		//Mouse click
		if(mpInput->BecameTriggerd(eLuxAction_LeftClick))		pGui->SendMouseClickDown(eGuiMouseButton_Left);
		if(mpInput->WasTriggerd(eLuxAction_LeftClick))			pGui->SendMouseClickUp(eGuiMouseButton_Left);
		if(mpInput->DoubleTriggerd(eLuxAction_LeftClick, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Left);

		if(mpInput->BecameTriggerd(eLuxAction_MiddleClick))		
			pGui->SendMouseClickDown(eGuiMouseButton_Middle);
		if(mpInput->WasTriggerd(eLuxAction_MiddleClick))			pGui->SendMouseClickUp(eGuiMouseButton_Middle);
		if(mpInput->DoubleTriggerd(eLuxAction_MiddleClick, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Middle);

		if(mpInput->BecameTriggerd(eLuxAction_RightClick))		pGui->SendMouseClickDown(eGuiMouseButton_Right);
		if(mpInput->WasTriggerd(eLuxAction_RightClick))			pGui->SendMouseClickUp(eGuiMouseButton_Right);
		if(mpInput->DoubleTriggerd(eLuxAction_RightClick, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Right);

		if(mpInput->BecameTriggerd(eLuxAction_ScrollUp))		pGui->SendMouseClickDown(eGuiMouseButton_WheelUp);
		if(mpInput->WasTriggerd(eLuxAction_ScrollUp))		pGui->SendMouseClickUp(eGuiMouseButton_WheelUp);

		if(mpInput->BecameTriggerd(eLuxAction_ScrollDown))		pGui->SendMouseClickDown(eGuiMouseButton_WheelDown);
		if(mpInput->WasTriggerd(eLuxAction_ScrollDown))		pGui->SendMouseClickUp(eGuiMouseButton_WheelDown);

		if(mpInput->BecameTriggerd(eLuxAction_MouseButton6Click))		pGui->SendMouseClickDown(eGuiMouseButton_Button6);
		if(mpInput->WasTriggerd(eLuxAction_MouseButton6Click))		pGui->SendMouseClickUp(eGuiMouseButton_Button6);
		if(mpInput->DoubleTriggerd(eLuxAction_MouseButton6Click, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Button6);

		if(mpInput->BecameTriggerd(eLuxAction_MouseButton7Click))		pGui->SendMouseClickDown(eGuiMouseButton_Button7);
		if(mpInput->WasTriggerd(eLuxAction_MouseButton7Click))		pGui->SendMouseClickUp(eGuiMouseButton_Button7);
		if(mpInput->DoubleTriggerd(eLuxAction_MouseButton7Click, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Button7);
		
		if(mpInput->BecameTriggerd(eLuxAction_MouseButton8Click))		pGui->SendMouseClickDown(eGuiMouseButton_Button8);
		if(mpInput->WasTriggerd(eLuxAction_MouseButton8Click))		pGui->SendMouseClickUp(eGuiMouseButton_Button8);
		if(mpInput->DoubleTriggerd(eLuxAction_MouseButton8Click, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Button8);

		if(mpInput->BecameTriggerd(eLuxAction_MouseButton9Click))		pGui->SendMouseClickDown(eGuiMouseButton_Button9);
		if(mpInput->WasTriggerd(eLuxAction_MouseButton9Click))		pGui->SendMouseClickUp(eGuiMouseButton_Button9);
		if(mpInput->DoubleTriggerd(eLuxAction_MouseButton9Click, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Button9);

#ifdef USE_GAMEPAD
		mbGamepadUIInput = false;
		mbGamepadUIInput = UpdateGamepadUIInput();
#endif
	}
}

#ifdef USE_GAMEPAD
bool cLuxInputHandler::UpdateGamepadUIInput()
{
	bool bRet = false;
	cGui* pGui = gpBase->mpEngine->GetGui();

	//Key presses
	if(IsGamepadPresent())
	{
		while(mpPad->HasInputUpdates())
		{
			pGui->SendGamepadInput(mpPad->GetInputUpdate());
		}
	}

	int lAction, lInput;
	for(size_t i=0; ohpl_ga9_ui_map(0, i, &lAction, &lInput); ++i)
	{
		if(mpInput->BecameTriggerd(lAction))
		{
			pGui->SendUIArrowPress((eUIArrow)lInput);
			bRet = true;
		}
		if(mpInput->WasTriggerd(lAction))
		{
			pGui->SendUIArrowRelease((eUIArrow)lInput);
			bRet = true;
		}
	}

	for(size_t i=0; ohpl_ga9_ui_map(1, i, &lAction, &lInput); ++i)
	{
		if(mpInput->BecameTriggerd(lAction))
		{
			pGui->SendUIButtonPress((eUIButton)lInput);
			bRet = true;
		}
		if(mpInput->WasTriggerd(lAction))
		{
			pGui->SendUIButtonRelease((eUIButton)lInput);
			bRet = true;
		}
		if(mpInput->DoubleTriggerd(lAction, 0.3f))
		{
			pGui->SendUIButtonDoublePress((eUIButton)lInput);
			bRet = true;
		}
	}

	return bRet;
}
#endif

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGameInput()
{
	/////////////////
	// Debug
	if(mpInput->BecameTriggerd(eLuxAction_OpenDebug))
	{
		gpBase->mpDebugHandler->SetDebugWindowActive(true);
	}
	if(mpInput->BecameTriggerd(eLuxAction_ReloadMap) && gpBase->mpConfigHandler->mbLoadDebugMenu)
	{
		gpBase->mpDebugHandler->QuickReloadMap();
	}
	if(mpInput->BecameTriggerd(eLuxAction_FastForward) && gpBase->mpConfigHandler->mbLoadDebugMenu)
	{
		bool bActivate = !gpBase->mpDebugHandler->GetFastForward();

		gpBase->mpDebugHandler->SetFastForward(bActivate);
	}

	if(mpPlayer->IsDead()==false && gpBase->mpDebugHandler->GetAllowQuickSave() && gpBase->mbPTestActivated==false)
	{
		if(mpInput->BecameTriggerd(eLuxAction_QuickSave))
		{
			gpBase->mpSaveHandler->AutoSave();
		}
		if(mpInput->BecameTriggerd(eLuxAction_QuickLoad))
		{
			gpBase->mpSaveHandler->AutoLoad(false);
		}
	}

	////////////////////
	//Exit
	if(mpInput->BecameTriggerd(eLuxAction_Exit))
	{
		gpBase->mpEngine->GetUpdater()->SetContainer("MainMenu");
	}

	////////////////////
	//Toggle Crosshair
	if(mpInput->BecameTriggerd(eLuxAction_CrosshairToggle))
	{
		gpBase->mpPlayer->SetShowCrosshair(!gpBase->mpPlayer->GetShowCrosshair());
	}

	////////////////////
	// Player or other
	if(gpBase->mpMessageHandler->IsPauseMessageActive())
	{
		UpdateGameMessageInput();
	}
	else if(gpBase->mpEffectHandler->GetPlayerIsPaused())
	{
		UpdateGameEffectInput();
	}
	else
	{
		UpdateGamePlayerInput();
	}
	
}
//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGamePlayerInput()
{
	/////////////////
	// Check if player is dead
	if(mpPlayer->IsDead())
	{
		if(	mpInput->BecameTriggerd(eLuxAction_Attack) ||
			mpInput->BecameTriggerd(eLuxAction_Interact) ||
			mpInput->BecameTriggerd(eLuxAction_Ignite) ||
			mpInput->BecameTriggerd(eLuxAction_Jump) ||
			mpInput->BecameTriggerd(eLuxAction_Crouch) )
		{
			mpPlayer->GetHelperDeath()->OnPressButton();
		}
		return;
	}
	if(mpPlayer->IsActive()==false) return;

	////////////////////
	// High level
	if(mpPlayer->GetCurrentStateData()->AllowPlayerMenus())
	{
		if(mpInput->BecameTriggerd(eLuxAction_Inventory))
		{
			if(gpBase->mpInventory->GetDisabled()==false)
				gpBase->mpEngine->GetUpdater()->SetContainer("Inventory");
		}
		if(mpInput->BecameTriggerd(eLuxAction_Journal))
		{
			if(gpBase->mpInventory->GetDisabled()==false)
				gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		}
		if(mpInput->BecameTriggerd(eLuxAction_QuestLog))
		{
			if(gpBase->mpInventory->GetDisabled()==false)
			{
				gpBase->mpJournal->SetForceInstantExit(true);
				gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
				gpBase->mpJournal->ChangeState(eLuxJournalState_QuestLog);
			}
		}
		if(mpInput->BecameTriggerd(eLuxAction_RecentText))
		{
			if(gpBase->mpInventory->GetDisabled()==false)
			{
				gpBase->mpJournal->OpenLastReadText();
			}
		}
	}

	/////////////////
	// Movement Direction
	if(mpInput->IsTriggerd(eLuxAction_Forward))
	{
		mpPlayer->Move(eCharDir_Forward, 1);
	}
	if(mpInput->IsTriggerd(eLuxAction_Backward))
	{
		mpPlayer->Move(eCharDir_Forward, -1);
	}
	if(mpInput->IsTriggerd(eLuxAction_Right))
	{
		mpPlayer->Move(eCharDir_Right, 1);
	}
	if(mpInput->IsTriggerd(eLuxAction_Left))
	{
		mpPlayer->Move(eCharDir_Right, -1);
	}

	/////////////////
	// Lean
	if(mpInput->IsTriggerd(eLuxAction_LeanRight))
	{
		mpPlayer->SetLean(1);
	}
	if(mpInput->IsTriggerd(eLuxAction_LeanLeft))
	{
		mpPlayer->SetLean(-1);
	}

	/////////////////
	// Actions

	//Attack
	if(mpInput->BecameTriggerd(eLuxAction_Attack))	mpPlayer->DoAction(eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_Attack))		mpPlayer->DoAction(eLuxPlayerAction_Attack, false);

	//Interact
	if(mpInput->BecameTriggerd(eLuxAction_Interact))
	{
		mpInput->BecameTriggerd(eLuxAction_LeftClick); 
		mpPlayer->DoAction(eLuxPlayerAction_Interact ,true);
	}
	if(mpInput->WasTriggerd(eLuxAction_Interact))
	{
		mpInput->WasTriggerd(eLuxAction_LeftClick); 
		mpPlayer->DoAction(eLuxPlayerAction_Interact, false);
	}

	//Ignite
	if(mpInput->BecameTriggerd(eLuxAction_Ignite))	mpPlayer->DoAction(eLuxPlayerAction_Ignite ,true);
	if(mpInput->WasTriggerd(eLuxAction_Ignite))		mpPlayer->DoAction(eLuxPlayerAction_Ignite, false);

	//Holster
	if(mpInput->BecameTriggerd(eLuxAction_Holster))	
	{
		if(gpBase->mpConfigHandler->mbLoadDebugMenu)
		{
			mpPlayer->GiveSanityDamage(10);
			mpPlayer->LowerSanity(3, true);//1.0f / 60.0f);
			mpPlayer->GiveDamage(10, 10, eLuxDamageType_BloodSplat, true, true);
		}	
	}
		//mpPlayer->DoAction(eLuxPlayerAction_Holster ,true);
	//if(mpInput->WasTriggerd(eLuxAction_Holster))	mpPlayer->DoAction(eLuxPlayerAction_Holster, false);
	
	//Lantern
	if(mpInput->BecameTriggerd(eLuxAction_Lantern))	mpPlayer->DoAction(eLuxPlayerAction_Lantern ,true);
	if(mpInput->WasTriggerd(eLuxAction_Lantern))	mpPlayer->DoAction(eLuxPlayerAction_Lantern, false);


	//Scroll
	if(mpInput->BecameTriggerd(eLuxAction_ScrollUp))	mpPlayer->Scroll(1.0f);
	if(mpInput->BecameTriggerd(eLuxAction_ScrollDown))	mpPlayer->Scroll(-1.0f);


	/////////////////
	// Movements

	//Run
	if(mpInput->BecameTriggerd(eLuxAction_Run))	mpPlayer->Run(true);
	if(mpInput->WasTriggerd(eLuxAction_Run))	mpPlayer->Run(false);

	//Jump
	if(mpInput->BecameTriggerd(eLuxAction_Jump))mpPlayer->Jump(true);
	if(mpInput->WasTriggerd(eLuxAction_Jump))	mpPlayer->Jump(false);

	//Crouch
	if(mpInput->BecameTriggerd(eLuxAction_Crouch))mpPlayer->Crouch(true);
	if(mpInput->WasTriggerd(eLuxAction_Crouch))	mpPlayer->Crouch(false);

	/////////////////
	// Head

	// Mouse and gamepad
	cVector2l vMouseRelPos = mpInput->GetMouse()->GetRelPosition();
	OhplGa9LookIn lookIn;
	lookIn.mouse_rel_x = vMouseRelPos.x;
	lookIn.mouse_rel_y = vMouseRelPos.y;
	lookIn.mouse_sensitivity = mfMouseSensitivity;
	lookIn.screen_height = mpGraphics->GetLowLevel()->GetScreenSizeFloat().y;
	lookIn.smooth_mouse = mbSmoothMouse;
	lookIn.invert_mouse = mbInvertMouse;
	lookIn.pad_present = IsGamepadPresent();
	lookIn.look_sensitivity = mfGamepadLookSensitivity;
	lookIn.look_invert = mbGamepadLookInvert;

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		switch(op)
		{
		case eOhplGa9Op_LookTriggered: r->i = mpInput->IsTriggerd((int)a[0].i); break;
		case eOhplGa9Op_LookAddLean: mpPlayer->AddLean(a[0].f); break;
		case eOhplGa9Op_LookSetLean: mpPlayer->SetLean(a[0].f); break;
		case eOhplGa9Op_LookScroll: mpPlayer->Scroll(a[0].f); break;
		case eOhplGa9Op_LookStop: mpPlayer->GetCharacterBody()->StopMovement(); break;
		case eOhplGa9Op_LookButton: r->i = mpPad->ButtonIsDown((eGamepadButton)a[0].i); break;
		case eOhplGa9Op_LookAxis: r->f = mpPad->GetAxisValue((eGamepadAxis)a[0].i); break;
		case eOhplGa9Op_LookDead: r->f = mpPad->GetAxisDeadZoneRadiusValue(); break;
		case eOhplGa9Op_LookFrame: r->f = gpBase->mpEngine->GetFrameTime(); break;
		case eOhplGa9Op_LookStep: r->f = gpBase->mpEngine->GetStepSize(); break;
		}
	};
	float vLook[2];
	ohpl_ga9_look(mpSmooth, &lookIn, &call, OhplGa9CallOf<decltype(call)>::Fn(), vLook);

	mpPlayer->AddYaw(-vLook[0]);
	mpPlayer->AddPitch(-vLook[1]);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGameMessageInput()
{
	//Attack
	if(mpInput->BecameTriggerd(eLuxAction_Attack))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_Attack))		gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, false);

	//Interact
	if(mpInput->BecameTriggerd(eLuxAction_Interact))gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Interact ,true);
	if(mpInput->WasTriggerd(eLuxAction_Interact))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Interact, false);

#ifdef USE_GAMEPAD
	////////////
	// Use the UI input from the gamepad to do the same thing
	if(mpInput->BecameTriggerd(eLuxAction_UIPrimary))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Interact, true);
	if(mpInput->WasTriggerd(eLuxAction_UIPrimary))		gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Interact, false);

	if(mpInput->BecameTriggerd(eLuxAction_UISecondary))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_UISecondary))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, false);
#endif
	
}

//-----------------------------------------------------------------------

void cLuxInputHandler:: UpdateGameEffectInput()
{
	//Attack
	if(mpInput->BecameTriggerd(eLuxAction_Attack))	gpBase->mpEffectHandler->DoAction(eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_Attack))		gpBase->mpEffectHandler->DoAction(eLuxPlayerAction_Attack, false);

	//Interact
	if(mpInput->BecameTriggerd(eLuxAction_Interact))gpBase->mpEffectHandler->DoAction(eLuxPlayerAction_Interact ,true);
	if(mpInput->WasTriggerd(eLuxAction_Interact))	gpBase->mpEffectHandler->DoAction(eLuxPlayerAction_Interact, false);

#ifdef USE_GAMEPAD
	////////////
	// Use the UI input from the gamepad to do the same thing
	if(mpInput->BecameTriggerd(eLuxAction_UIPrimary))	gpBase->mpEffectHandler->DoAction(eLuxPlayerAction_Interact, true);
	if(mpInput->WasTriggerd(eLuxAction_UIPrimary))		gpBase->mpEffectHandler->DoAction(eLuxPlayerAction_Interact, false);

	if(mpInput->BecameTriggerd(eLuxAction_UISecondary))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_UISecondary))	gpBase->mpEffectHandler->DoAction(eLuxPlayerAction_Attack, false);
#endif
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateInventoryInput()
{
	if(ShowMouseOnMouseInput())
	{
		gpBase->mpInventory->GetSet()->SetMouseMovementEnabled(true);
		gpBase->mpInventory->GetSet()->SetDrawMouse(true);
		gpBase->mpInventory->GetSet()->SetDrawFocus(false);
	}

	////////////////////
	//Exit
	if(	mpInput->BecameTriggerd(eLuxAction_Exit) ||
		mpInput->BecameTriggerd(eLuxAction_Inventory))
	{
		gpBase->mpInventory->ExitPressed();
	}

	////////////////////
	//Journal
	if(mpInput->BecameTriggerd(eLuxAction_Journal))
	{
		gpBase->mpInventory->OpenJournal();
	}

	////////////////////
	//If a message is active, we use special input
	if(gpBase->mpInventory->GetMessageActive())
	{
		gpBase->mpEngine->GetGui()->SendMousePos(mpInput->GetMouse()->GetAbsPosition(), mpInput->GetMouse()->GetRelPosition());

		if(	mpInput->BecameTriggerd(eLuxAction_LeftClick) || mpInput->BecameTriggerd(eLuxAction_MiddleClick) ||
			mpInput->BecameTriggerd(eLuxAction_RightClick) ||
			mpInput->BecameTriggerd(eLuxAction_UIPrimary) ||
			mpInput->BecameTriggerd(eLuxAction_UISecondary))
		{
			gpBase->mpInventory->ExitPressed(); //This just exits message!		
		}
	}

}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdatePreMenuInput()
{
	if(ShowMouseOnMouseInput())
	{
		gpBase->mpPreMenu->GetSet()->SetMouseMovementEnabled(true);
		gpBase->mpPreMenu->GetSet()->SetDrawMouse(true);
	}

	////////////////////
	if(mpInput->BecameTriggerd(eLuxAction_Exit) || mpInput->BecameTriggerd(eLuxAction_UIPrimary))
	{
		gpBase->mpPreMenu->ButtonPressed();
	}
	// sections with a widget handle their own input
	else if(gpBase->mpPreMenu->IsContinueButtonVisible()==false &&
			(mpInput->BecameTriggerd(eLuxAction_LeftClick) ||
			 mpInput->BecameTriggerd(eLuxAction_MiddleClick) ||
			 mpInput->BecameTriggerd(eLuxAction_RightClick)))
	{
		gpBase->mpPreMenu->ButtonPressed();
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateMainMenuInput()
{
	if(ShowMouseOnMouseInput())
	{
		gpBase->mpMainMenu->GetSet()->SetMouseMovementEnabled(true);
		gpBase->mpMainMenu->GetSet()->SetDrawMouse(true);
	}

	////////////////////
	//Exit
	if(mpInput->BecameTriggerd(eLuxAction_Exit))
	{
		gpBase->mpMainMenu->ExitPressed();
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateJournalInput()
{
	if(ShowMouseOnMouseInput())
	{
		gpBase->mpJournal->GetSet()->SetMouseMovementEnabled(true);
		gpBase->mpJournal->GetSet()->SetDrawMouse(true);
	}

	////////////////////
	//Exit
	if(	mpInput->BecameTriggerd(eLuxAction_Exit))
	{
		gpBase->mpJournal->ExitPressed(false);
	}
	else if(mpInput->BecameTriggerd(eLuxAction_Journal) || 
			mpInput->BecameTriggerd(eLuxAction_QuestLog) || 
			mpInput->BecameTriggerd(eLuxAction_Inventory) ||
			mpInput->BecameTriggerd(eLuxAction_RecentText))
	{
#ifdef USE_GAMEPAD
		if(mbGamepadUIInput==false) gpBase->mpJournal->ExitPressed(true);
#else
		gpBase->mpJournal->ExitPressed(true);
#endif
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateDebugInput()
{
	if(mpInput->BecameTriggerd(eLuxAction_OpenDebug))
	{
		gpBase->mpDebugHandler->SetDebugWindowActive(false);
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateCreditsInput()
{
	if(	mpInput->BecameTriggerd(eLuxAction_Exit) ||
		mpInput->BecameTriggerd(eLuxAction_Jump) ||
		mpInput->BecameTriggerd(eLuxAction_Attack) ||
		mpInput->BecameTriggerd(eLuxAction_Interact) ||
		mpInput->BecameTriggerd(eLuxAction_Inventory))
	{
		gpBase->mpCredits->ExitPressed();
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateDemoEndInput()
{
	if(gpBase->mpDemoEnd)
	{
		if(mpInput->BecameTriggerd(eLuxAction_Exit))
		{
			gpBase->mpDemoEnd->Exit(false);
		}
	}
}

//-----------------------------------------------------------------------


void cLuxInputHandler::UpdateLoadScreenInput()
{
	if(	mpInput->BecameTriggerd(eLuxAction_Exit) ||
		mpInput->BecameTriggerd(eLuxAction_Jump) ||
		mpInput->BecameTriggerd(eLuxAction_Attack) ||
		mpInput->BecameTriggerd(eLuxAction_Interact) ||
		mpInput->BecameTriggerd(eLuxAction_Inventory))
	{
		gpBase->mpLoadScreenHandler->ExitPressed();
	}
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::CurrentStateSendsInputToGui()
{
	return ohpl_ga9_state_sends_to_gui(mState, mState==eLuxInputState_Inventory && gpBase->mpInventory->GetMessageActive());
}

//-----------------------------------------------------------------------

void cLuxInputHandler::CreateActions()
{
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r) { ActionCall(op, a, r); };
	ohpl_ga9_create_actions(&call, OhplGa9CallOf<decltype(call)>::Fn());
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::ShowMouseOnMouseInput()
{
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		switch(op)
		{
		case eOhplGa9Op_MouseAnyButton:
			{
				bool bActive = false;
				for(int i = 0; i < eMouseButton_LastEnum; ++i)
					bActive = bActive || mpInput->GetMouse()->ButtonIsDown(eMouseButton(i));
				r->i = bActive;
			}
			break;
		case eOhplGa9Op_MouseAbs:
			{
				cVector2l vAbsPos = mpInput->GetMouse()->GetAbsPosition();
				r->i = (int64_t)(uint32_t)vAbsPos.x | ((int64_t)vAbsPos.y << 32);
			}
			break;
		case eOhplGa9Op_MouseTime: r->d = gpBase->mpEngine->GetGameTime(); break;
		case eOhplGa9Op_MousePad: r->i = IsGamepadPresent(); break;
		case eOhplGa9Op_MouseUiDir:
			r->i = mpInput->IsTriggerd(eLuxAction_UIArrowUp) ||
					mpInput->IsTriggerd(eLuxAction_UIArrowDown) ||
					mpInput->IsTriggerd(eLuxAction_UIArrowLeft) ||
					mpInput->IsTriggerd(eLuxAction_UIArrowRight);
			break;
		case eOhplGa9Op_MouseScreen:
			{
				cVector2l vScreen = mpGraphics->GetLowLevel()->GetScreenSizeInt();
				r->i = (int64_t)(uint32_t)vScreen.x | ((int64_t)vScreen.y << 32);
			}
			break;
		case eOhplGa9Op_MouseHide:
			gpBase->mpInventory->GetSet()->SetDrawMouse(false);
			gpBase->mpInventory->GetSet()->SetMouseMovementEnabled(false);
			gpBase->mpInventory->GetSet()->SetDrawFocus(true);
			gpBase->mpMainMenu->GetSet()->SetMouseMovementEnabled(false);
			gpBase->mpMainMenu->GetSet()->SetDrawMouse(false);
			gpBase->mpJournal->GetSet()->SetMouseMovementEnabled(false);
			gpBase->mpJournal->GetSet()->SetDrawMouse(false);
			gpBase->mpPreMenu->GetSet()->SetMouseMovementEnabled(false);
			gpBase->mpPreMenu->GetSet()->SetDrawMouse(false);
			break;
		}
	};

	return ohpl_ga9_mouse_step(&mMouseAct, &call, OhplGa9CallOf<decltype(call)>::Fn());
}

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
void cLuxInputHandler::SetUpGamepad()
{
	mpPad = mpInput->GetGamepad(0);
	if(IsGamepadPresent())
	{
		mpPad->SetAxisDeadZoneRadiusValue(0.15f);
	}
}
#endif

//-----------------------------------------------------------------------
