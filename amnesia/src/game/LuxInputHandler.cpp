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
#include "LuxA12bDump.h"
#include "LuxInputHost.h"

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
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t n, OhplGa9Ret *r)
	{
		OhplA12bLogCall("input", op, a, n);
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

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t n, OhplGa9Ret *r)
	{
		OhplA12bLogCall("input", op, a, n);
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
	if(OhplInDumpEnabled() && OhplInScripted() == false) return;

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
	OhplInDumpTick();

	//Turn of logging so it only happens one frame!
	OhplInFx(eInFx_RenderLog, 0);
}

//-----------------------------------------------------------------------

tWString gsInvalidActionString = _W("InvalidAction");

tWString cLuxInputHandler::GetInputName(const tString& asActionName)
{
	tWString sName;
	if(OhplInInputName(asActionName, sName) == false) return gsInvalidActionString;

	return sName;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::ChangeState(eLuxInputState aState)
{
	mState = aState;


	//This rests all actions so changing states does not give unwanted clicks!
	OhplInFx(eInFx_ResetActions);

	//Reset some stuff
	ohpl_ga9_smooth_reset(mpSmooth);	//Every state needs new smoothing!

	mMouseAct.last_x = (int)OhplInVal(eInVal_MouseAbsX, 0);
	mMouseAct.last_y = (int)OhplInVal(eInVal_MouseAbsY, 0);
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
	return OhplInVal(eInVal_PadPresent, 0) != 0;
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

#define BECAME(a) OhplInTrig(0, a)
#define WAS(a) OhplInTrig(1, a)
#define IS(a) OhplInTrig(2, a)
#define DOUBLE(a) OhplInTrig(3, a)

void cLuxInputHandler::UpdateGlobalInput()
{
	/////////////////
	// ScreenShot
	if(BECAME(eLuxAction_ScreenShot))
	{
		tWString sFileName;
		tWString sScreenShotDir = OhplInShotDir();

		tString sShotExt = OhplInShotExt();
		auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r) { r->i = OhplInFileExists(OhplGa9GetW(a[0])); };
		auto emit = [&](uint32_t tag, const void *p, size_t n) { sFileName = OhplGa9EmitW(p, n); };
		ohpl_ga9_screenshot_name((const uint32_t*)sScreenShotDir.data(), sScreenShotDir.size(),
								(const unsigned char*)sShotExt.data(), sShotExt.size(),
								&call, OhplGa9CallOf<decltype(call)>::Fn(), OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

		OhplInShotSave(sFileName);
	}

	/////////////////
	// Debug output
	if(BECAME(eLuxAction_PrintInfo))
	{
		OhplInFx(eInFx_RenderLog, 1);
	}

	/////////////////
	// Send to GUI
	if(CurrentStateSendsInputToGui())
	{
		//Key presses
		while(OhplInQ(eInQ_KeyPop))
		{
			// sections with a widget handle their own input
			if(mState==eLuxInputState_PreMenu && OhplInQ(eInQ_PreContinue)==false)
			{
				OhplInFx(eInFx_PreMenuButton);
			}
		}

		//Mouse movement
		OhplInFx(eInFx_GuiMouse);


		//Mouse click
		static const int vClicks[][2] = { {eLuxAction_LeftClick, eGuiMouseButton_Left}, {eLuxAction_MiddleClick, eGuiMouseButton_Middle},
										  {eLuxAction_RightClick, eGuiMouseButton_Right} };
		for(int i=0; i<3; ++i)
		{
			if(BECAME(vClicks[i][0])) OhplInFx(eInFx_GuiClick, 0, vClicks[i][1]);
			if(WAS(vClicks[i][0])) OhplInFx(eInFx_GuiClick, 1, vClicks[i][1]);
			if(DOUBLE(vClicks[i][0])) OhplInFx(eInFx_GuiClick, 2, vClicks[i][1]);
		}

		if(BECAME(eLuxAction_ScrollUp)) OhplInFx(eInFx_GuiClick, 0, eGuiMouseButton_WheelUp);
		if(WAS(eLuxAction_ScrollUp)) OhplInFx(eInFx_GuiClick, 1, eGuiMouseButton_WheelUp);

		if(BECAME(eLuxAction_ScrollDown)) OhplInFx(eInFx_GuiClick, 0, eGuiMouseButton_WheelDown);
		if(WAS(eLuxAction_ScrollDown)) OhplInFx(eInFx_GuiClick, 1, eGuiMouseButton_WheelDown);

		static const int vExtra[][2] = { {eLuxAction_MouseButton6Click, eGuiMouseButton_Button6}, {eLuxAction_MouseButton7Click, eGuiMouseButton_Button7},
										 {eLuxAction_MouseButton8Click, eGuiMouseButton_Button8}, {eLuxAction_MouseButton9Click, eGuiMouseButton_Button9} };
		for(int i=0; i<4; ++i)
		{
			if(BECAME(vExtra[i][0])) OhplInFx(eInFx_GuiClick, 0, vExtra[i][1]);
			if(WAS(vExtra[i][0])) OhplInFx(eInFx_GuiClick, 1, vExtra[i][1]);
			if(DOUBLE(vExtra[i][0])) OhplInFx(eInFx_GuiClick, 2, vExtra[i][1]);
		}

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

	//Key presses
	if(IsGamepadPresent())
	{
		OhplInFx(eInFx_PadDrain);
	}

	int lAction, lInput;
	for(size_t i=0; ohpl_ga9_ui_map(0, i, &lAction, &lInput); ++i)
	{
		if(BECAME(lAction))
		{
			OhplInFx(eInFx_UiSend, 0, lInput);
			bRet = true;
		}
		if(WAS(lAction))
		{
			OhplInFx(eInFx_UiSend, 1, lInput);
			bRet = true;
		}
	}

	for(size_t i=0; ohpl_ga9_ui_map(1, i, &lAction, &lInput); ++i)
	{
		if(BECAME(lAction))
		{
			OhplInFx(eInFx_UiSend, 4, lInput);
			bRet = true;
		}
		if(WAS(lAction))
		{
			OhplInFx(eInFx_UiSend, 5, lInput);
			bRet = true;
		}
		if(DOUBLE(lAction))
		{
			OhplInFx(eInFx_UiSend, 6, lInput);
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
	if(BECAME(eLuxAction_OpenDebug))
	{
		OhplInFx(eInFx_DebugWindow, 1);
	}
	if(BECAME(eLuxAction_ReloadMap) && OhplInQ(eInQ_DebugMenu))
	{
		OhplInFx(eInFx_DebugReload);
	}
	if(BECAME(eLuxAction_FastForward) && OhplInQ(eInQ_DebugMenu))
	{
		bool bActivate = !OhplInQ(eInQ_FastForward);

		OhplInFx(eInFx_FastForward, bActivate);
	}

	if(OhplInQ(eInQ_PlayerDead)==false && OhplInQ(eInQ_QuickSave) && OhplInQ(eInQ_PTest)==false)
	{
		if(BECAME(eLuxAction_QuickSave))
		{
			OhplInFx(eInFx_AutoSave);
		}
		if(BECAME(eLuxAction_QuickLoad))
		{
			OhplInFx(eInFx_AutoLoad);
		}
	}

	////////////////////
	//Exit
	if(BECAME(eLuxAction_Exit))
	{
		OhplInFx(eInFx_Container, 0);
	}

	////////////////////
	//Toggle Crosshair
	if(BECAME(eLuxAction_CrosshairToggle))
	{
		OhplInFx(eInFx_Crosshair, !OhplInQ(eInQ_Crosshair));
	}

	////////////////////
	// Player or other
	if(OhplInQ(eInQ_MsgPause))
	{
		UpdateGameMessageInput();
	}
	else if(OhplInQ(eInQ_EffectPaused))
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
	if(OhplInQ(eInQ_PlayerDead))
	{
		if(	BECAME(eLuxAction_Attack) ||
			BECAME(eLuxAction_Interact) ||
			BECAME(eLuxAction_Ignite) ||
			BECAME(eLuxAction_Jump) ||
			BECAME(eLuxAction_Crouch) )
		{
			OhplInFx(eInFx_DeathPress);
		}
		return;
	}
	if(OhplInQ(eInQ_PlayerActive)==false) return;

	////////////////////
	// High level
	if(OhplInQ(eInQ_AllowMenus))
	{
		if(BECAME(eLuxAction_Inventory))
		{
			if(OhplInQ(eInQ_InvDisabled)==false)
				OhplInFx(eInFx_Container, 1);
		}
		if(BECAME(eLuxAction_Journal))
		{
			if(OhplInQ(eInQ_InvDisabled)==false)
				OhplInFx(eInFx_Container, 2);
		}
		if(BECAME(eLuxAction_QuestLog))
		{
			if(OhplInQ(eInQ_InvDisabled)==false)
			{
				OhplInFx(eInFx_JournalForce);
				OhplInFx(eInFx_Container, 2);
				OhplInFx(eInFx_JournalQuestLog);
			}
		}
		if(BECAME(eLuxAction_RecentText))
		{
			if(OhplInQ(eInQ_InvDisabled)==false)
			{
				OhplInFx(eInFx_JournalRecent);
			}
		}
	}

	/////////////////
	// Movement Direction
	if(IS(eLuxAction_Forward))
	{
		OhplInFx(eInFx_PlMove, eCharDir_Forward, 1);
	}
	if(IS(eLuxAction_Backward))
	{
		OhplInFx(eInFx_PlMove, eCharDir_Forward, -1);
	}
	if(IS(eLuxAction_Right))
	{
		OhplInFx(eInFx_PlMove, eCharDir_Right, 1);
	}
	if(IS(eLuxAction_Left))
	{
		OhplInFx(eInFx_PlMove, eCharDir_Right, -1);
	}

	/////////////////
	// Lean
	if(IS(eLuxAction_LeanRight))
	{
		OhplInFx(eInFx_PlLeanSet, 0, 0, 1);
	}
	if(IS(eLuxAction_LeanLeft))
	{
		OhplInFx(eInFx_PlLeanSet, 0, 0, -1);
	}

	/////////////////
	// Actions

	//Attack
	if(BECAME(eLuxAction_Attack))	OhplInFx(eInFx_PlAction, eLuxPlayerAction_Attack, true);
	if(WAS(eLuxAction_Attack))		OhplInFx(eInFx_PlAction, eLuxPlayerAction_Attack, false);

	//Interact
	if(BECAME(eLuxAction_Interact))
	{
		BECAME(eLuxAction_LeftClick);
		OhplInFx(eInFx_PlAction, eLuxPlayerAction_Interact, true);
	}
	if(WAS(eLuxAction_Interact))
	{
		WAS(eLuxAction_LeftClick);
		OhplInFx(eInFx_PlAction, eLuxPlayerAction_Interact, false);
	}

	//Ignite
	if(BECAME(eLuxAction_Ignite))	OhplInFx(eInFx_PlAction, eLuxPlayerAction_Ignite, true);
	if(WAS(eLuxAction_Ignite))		OhplInFx(eInFx_PlAction, eLuxPlayerAction_Ignite, false);

	//Holster
	if(BECAME(eLuxAction_Holster))
	{
		if(OhplInQ(eInQ_DebugMenu))
		{
			OhplInFx(eInFx_PlHolster);
		}
	}

	//Lantern
	if(BECAME(eLuxAction_Lantern))	OhplInFx(eInFx_PlAction, eLuxPlayerAction_Lantern, true);
	if(WAS(eLuxAction_Lantern))	OhplInFx(eInFx_PlAction, eLuxPlayerAction_Lantern, false);


	//Scroll
	if(BECAME(eLuxAction_ScrollUp))	OhplInFx(eInFx_PlScroll, 0, 0, 1.0f);
	if(BECAME(eLuxAction_ScrollDown))	OhplInFx(eInFx_PlScroll, 0, 0, -1.0f);


	/////////////////
	// Movements

	//Run
	if(BECAME(eLuxAction_Run))	OhplInFx(eInFx_PlRun, true);
	if(WAS(eLuxAction_Run))	OhplInFx(eInFx_PlRun, false);

	//Jump
	if(BECAME(eLuxAction_Jump))OhplInFx(eInFx_PlJump, true);
	if(WAS(eLuxAction_Jump))	OhplInFx(eInFx_PlJump, false);

	//Crouch
	if(BECAME(eLuxAction_Crouch))OhplInFx(eInFx_PlCrouch, true);
	if(WAS(eLuxAction_Crouch))	OhplInFx(eInFx_PlCrouch, false);

	/////////////////
	// Head

	// Mouse and gamepad
	OhplGa9LookIn lookIn;
	lookIn.mouse_rel_x = (int)OhplInVal(eInVal_MouseRelX, 0);
	lookIn.mouse_rel_y = (int)OhplInVal(eInVal_MouseRelY, 0);
	lookIn.mouse_sensitivity = mfMouseSensitivity;
	lookIn.screen_height = (float)OhplInVal(eInVal_ScreenH, 0);
	lookIn.smooth_mouse = mbSmoothMouse;
	lookIn.invert_mouse = mbInvertMouse;
	lookIn.pad_present = IsGamepadPresent();
	lookIn.look_sensitivity = mfGamepadLookSensitivity;
	lookIn.look_invert = mbGamepadLookInvert;

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		switch(op)
		{
		case eOhplGa9Op_LookTriggered: r->i = IS((int)a[0].i); break;
		case eOhplGa9Op_LookAddLean: OhplInFx(eInFx_PlLeanAdd, 0, 0, a[0].f); break;
		case eOhplGa9Op_LookSetLean: OhplInFx(eInFx_PlLeanSet, 0, 0, a[0].f); break;
		case eOhplGa9Op_LookScroll: OhplInFx(eInFx_PlScroll, 0, 0, a[0].f); break;
		case eOhplGa9Op_LookStop: OhplInFx(eInFx_PlStop); break;
		case eOhplGa9Op_LookButton: r->i = OhplInVal(eInVal_PadButton, (int)a[0].i) != 0; break;
		case eOhplGa9Op_LookAxis: r->f = (float)OhplInVal(eInVal_PadAxis, (int)a[0].i); break;
		case eOhplGa9Op_LookDead: r->f = (float)OhplInVal(eInVal_PadDead, 0); break;
		case eOhplGa9Op_LookFrame: r->f = (float)OhplInVal(eInVal_FrameTime, 0); break;
		case eOhplGa9Op_LookStep: r->f = (float)OhplInVal(eInVal_StepSize, 0); break;
		}
	};
	float vLook[2];
	ohpl_ga9_look(mpSmooth, &lookIn, &call, OhplGa9CallOf<decltype(call)>::Fn(), vLook);

	OhplInFx(eInFx_PlYaw, 0, 0, -vLook[0]);
	OhplInFx(eInFx_PlPitch, 0, 0, -vLook[1]);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGameMessageInput()
{
	//Attack
	if(BECAME(eLuxAction_Attack))	OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Attack, true);
	if(WAS(eLuxAction_Attack))		OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Attack, false);

	//Interact
	if(BECAME(eLuxAction_Interact))OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Interact, true);
	if(WAS(eLuxAction_Interact))	OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Interact, false);

#ifdef USE_GAMEPAD
	////////////
	// Use the UI input from the gamepad to do the same thing
	if(BECAME(eLuxAction_UIPrimary))	OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Interact, true);
	if(WAS(eLuxAction_UIPrimary))		OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Interact, false);

	if(BECAME(eLuxAction_UISecondary))	OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Attack, true);
	if(WAS(eLuxAction_UISecondary))	OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Attack, false);
#endif

}

//-----------------------------------------------------------------------

void cLuxInputHandler:: UpdateGameEffectInput()
{
	//Attack
	if(BECAME(eLuxAction_Attack))	OhplInFx(eInFx_EffectAction, eLuxPlayerAction_Attack, true);
	if(WAS(eLuxAction_Attack))		OhplInFx(eInFx_EffectAction, eLuxPlayerAction_Attack, false);

	//Interact
	if(BECAME(eLuxAction_Interact))OhplInFx(eInFx_EffectAction, eLuxPlayerAction_Interact, true);
	if(WAS(eLuxAction_Interact))	OhplInFx(eInFx_EffectAction, eLuxPlayerAction_Interact, false);

#ifdef USE_GAMEPAD
	////////////
	// Use the UI input from the gamepad to do the same thing
	if(BECAME(eLuxAction_UIPrimary))	OhplInFx(eInFx_EffectAction, eLuxPlayerAction_Interact, true);
	if(WAS(eLuxAction_UIPrimary))		OhplInFx(eInFx_EffectAction, eLuxPlayerAction_Interact, false);

	if(BECAME(eLuxAction_UISecondary))	OhplInFx(eInFx_MsgAction, eLuxPlayerAction_Attack, true);
	if(WAS(eLuxAction_UISecondary))	OhplInFx(eInFx_EffectAction, eLuxPlayerAction_Attack, false);
#endif
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateInventoryInput()
{
	if(ShowMouseOnMouseInput())
	{
		OhplInFx(eInFx_MenuMouse, 0);
	}

	////////////////////
	//Exit
	if(	BECAME(eLuxAction_Exit) ||
		BECAME(eLuxAction_Inventory))
	{
		OhplInFx(eInFx_InvExit);
	}

	////////////////////
	//Journal
	if(BECAME(eLuxAction_Journal))
	{
		OhplInFx(eInFx_InvJournal);
	}

	////////////////////
	//If a message is active, we use special input
	if(OhplInQ(eInQ_InvMsg))
	{
		OhplInFx(eInFx_GuiMouse);

		if(	BECAME(eLuxAction_LeftClick) || BECAME(eLuxAction_MiddleClick) ||
			BECAME(eLuxAction_RightClick) ||
			BECAME(eLuxAction_UIPrimary) ||
			BECAME(eLuxAction_UISecondary))
		{
			OhplInFx(eInFx_InvExit); //This just exits message!
		}
	}

}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdatePreMenuInput()
{
	if(ShowMouseOnMouseInput())
	{
		OhplInFx(eInFx_MenuMouse, 1);
	}

	////////////////////
	if(BECAME(eLuxAction_Exit) || BECAME(eLuxAction_UIPrimary))
	{
		OhplInFx(eInFx_PreMenuButton);
	}
	// sections with a widget handle their own input
	else if(OhplInQ(eInQ_PreContinue)==false &&
			(BECAME(eLuxAction_LeftClick) ||
			 BECAME(eLuxAction_MiddleClick) ||
			 BECAME(eLuxAction_RightClick)))
	{
		OhplInFx(eInFx_PreMenuButton);
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateMainMenuInput()
{
	if(ShowMouseOnMouseInput())
	{
		OhplInFx(eInFx_MenuMouse, 2);
	}

	////////////////////
	//Exit
	if(BECAME(eLuxAction_Exit))
	{
		OhplInFx(eInFx_MainMenuExit);
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateJournalInput()
{
	if(ShowMouseOnMouseInput())
	{
		OhplInFx(eInFx_MenuMouse, 3);
	}

	////////////////////
	//Exit
	if(	BECAME(eLuxAction_Exit))
	{
		OhplInFx(eInFx_JournalExit, 0);
	}
	else if(BECAME(eLuxAction_Journal) ||
			BECAME(eLuxAction_QuestLog) ||
			BECAME(eLuxAction_Inventory) ||
			BECAME(eLuxAction_RecentText))
	{
#ifdef USE_GAMEPAD
		if(mbGamepadUIInput==false) OhplInFx(eInFx_JournalExit, 1);
#else
		OhplInFx(eInFx_JournalExit, 1);
#endif
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateDebugInput()
{
	if(BECAME(eLuxAction_OpenDebug))
	{
		OhplInFx(eInFx_DebugWindow, 0);
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateCreditsInput()
{
	if(	BECAME(eLuxAction_Exit) ||
		BECAME(eLuxAction_Jump) ||
		BECAME(eLuxAction_Attack) ||
		BECAME(eLuxAction_Interact) ||
		BECAME(eLuxAction_Inventory))
	{
		OhplInFx(eInFx_CreditsExit);
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateDemoEndInput()
{
	if(OhplInQ(eInQ_DemoEnd))
	{
		if(BECAME(eLuxAction_Exit))
		{
			OhplInFx(eInFx_DemoEndExit);
		}
	}
}

//-----------------------------------------------------------------------


void cLuxInputHandler::UpdateLoadScreenInput()
{
	if(	BECAME(eLuxAction_Exit) ||
		BECAME(eLuxAction_Jump) ||
		BECAME(eLuxAction_Attack) ||
		BECAME(eLuxAction_Interact) ||
		BECAME(eLuxAction_Inventory))
	{
		OhplInFx(eInFx_LoadScreenExit);
	}
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::CurrentStateSendsInputToGui()
{
	return ohpl_ga9_state_sends_to_gui(mState, mState==eLuxInputState_Inventory && OhplInQ(eInQ_InvMsg));
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
		case eOhplGa9Op_MouseAnyButton: r->i = OhplInVal(eInVal_MouseAnyButton, 0) != 0; break;
		case eOhplGa9Op_MouseAbs:
			{
				int x = (int)OhplInVal(eInVal_MouseAbsX, 0), y = (int)OhplInVal(eInVal_MouseAbsY, 0);
				r->i = (int64_t)(uint32_t)x | ((int64_t)y << 32);
			}
			break;
		case eOhplGa9Op_MouseTime: r->d = OhplInVal(eInVal_GameTime, 0); break;
		case eOhplGa9Op_MousePad: r->i = IsGamepadPresent(); break;
		case eOhplGa9Op_MouseUiDir:
			r->i = IS(eLuxAction_UIArrowUp) ||
					IS(eLuxAction_UIArrowDown) ||
					IS(eLuxAction_UIArrowLeft) ||
					IS(eLuxAction_UIArrowRight);
			break;
		case eOhplGa9Op_MouseScreen:
			{
				int x = (int)OhplInVal(eInVal_ScreenW, 0), y = (int)OhplInVal(eInVal_ScreenH, 0);
				r->i = (int64_t)(uint32_t)x | ((int64_t)y << 32);
			}
			break;
		case eOhplGa9Op_MouseHide: OhplInFx(eInFx_HideMouseUis); break;
		}
	};

	return ohpl_ga9_mouse_step(&mMouseAct, &call, OhplGa9CallOf<decltype(call)>::Fn());
}

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
void cLuxInputHandler::SetUpGamepad()
{
	OhplInQ(eInQ_PadSetup);
}
#endif

//-----------------------------------------------------------------------
