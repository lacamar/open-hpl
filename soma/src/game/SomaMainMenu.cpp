/*
 * See SomaMainMenu.h for scope notes.
 */

#include "SomaMainMenu.h"
#include "SomaBase.h"

//---------------------------------------

cSomaMainMenu::cSomaMainMenu(cEngine *apEngine, cSomaBase *apBase, cViewport *apViewport) : iUpdateable("SomaMainMenu")
{
	mpEngine = apEngine;
	mpBase = apBase;
	mpViewport = apViewport;

	mbVisible = true;
	mbMouseWasDown = false;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	// Same real skin file cSomaSplash already uses (SOMA ships no separate
	// "main menu" skin) - it has full Button/Window gfx + a "Default" font
	// (vera.fnt), both confirmed present in the real SOMA install.
	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("SomaMainMenu", mpGuiSkin);

	mpGuiSet->SetDrawMouse(true);
	mpViewport->AddGuiSet(mpGuiSet);
	mpGuiSet->SetActive(true);

	// cGui routes all mouse/keyboard input to a single global "focused" set
	// (cGui::mpSetInFocus, see SendMousePos()/SendMouseClickDown() etc. in
	// Gui.cpp) - nothing sets this automatically just from being active on a
	// viewport (cSomaSplash never needs it, since it polls iMouse/iKeyboard
	// directly rather than routing through widget click messages). Without
	// this, button clicks are silently dropped: cGuiSet::SendMessage() is
	// never even called. Same call cLuxMainMenu/cLuxPreMenu make in the real
	// Dark Descent menu code (LuxMainMenu.cpp/LuxPreMenu.cpp).
	mpGui->SetFocus(mpGuiSet);

	CreateGui();
}

//-----------------------------------------------------------------------

cSomaMainMenu::~cSomaMainMenu()
{
}

//-----------------------------------------------------------------------

void cSomaMainMenu::CreateGui()
{
	const float fButtonWidth = 260;
	const float fButtonHeight = 44;
	const float fButtonSep = 18;

	cVector2f vCenter = mvScreenSize * 0.5f;

	////////////////////////////////////
	// Title - plain, honest labelling of what this actually is (not a
	// recreation of SOMA's real ImGui menu, see SomaMainMenu.h).
	cWidgetLabel *pTitle = mpGuiSet->CreateWidgetLabel(
		cVector3f(0, vCenter.y - 140, 0.1f), cVector2f(mvScreenSize.x, 40),
		_W("SOMA (Open HPL scaffold)"), NULL, "Title");
	pTitle->SetTextAlign(eFontAlign_Center);
	pTitle->SetDefaultFontSize(cVector2f(28, 28));
	// The skin's default label font colour is near-black (matches window
	// backgrounds elsewhere in the skin) - invisible against this scene's
	// black backdrop (main_menu.hpm has no skybox/ambient light of its own).
	pTitle->SetDefaultFontColor(cColor(1, 1, 1, 1));

	////////////////////////////////////
	// New Game
	cVector3f vNewGamePos(vCenter.x - fButtonWidth * 0.5f, vCenter.y - fButtonHeight - fButtonSep * 0.5f, 0.1f);
	cWidgetButton *pNewGame = mpGuiSet->CreateWidgetButton(
		vNewGamePos, cVector2f(fButtonWidth, fButtonHeight), _W("New Game"), NULL, false, "NewGame");
	pNewGame->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(ButtonNewGame));

	////////////////////////////////////
	// Quit
	cVector3f vQuitPos(vCenter.x - fButtonWidth * 0.5f, vCenter.y + fButtonSep * 0.5f, 0.1f);
	cWidgetButton *pQuit = mpGuiSet->CreateWidgetButton(
		vQuitPos, cVector2f(fButtonWidth, fButtonHeight), _W("Quit"), NULL, false, "Quit");
	pQuit->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(ButtonQuit));

	mpGuiSet->SetDefaultFocusNavWidget(pNewGame);
	mpGuiSet->SetFocusedWidget(pNewGame);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::SetVisible(bool abVisible)
{
	mbVisible = abVisible;

	mpGuiSet->SetActive(abVisible);
	mpGuiSet->SetDrawMouse(abVisible);

	if (abVisible)
		mpGui->SetFocus(mpGuiSet);
	else if (mpGui->GetFocusedSet() == mpGuiSet)
		mpGui->SetFocus(NULL);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::Update(float afTimeStep)
{
	if (mbVisible == false)
		return;

	// See the class comment in SomaMainMenu.h: cGui does not poll iMouse on
	// its own, so this scaffold has to do what amnesia/src/game/
	// LuxInputHandler.cpp does for the real game every frame - push mouse
	// position and left-click edges into cGui by hand.
	iMouse *pMouse = mpEngine->GetInput()->GetMouse();
	if (pMouse == NULL)
		return;

	mpGui->SendMousePos(pMouse->GetAbsPosition(), pMouse->GetRelPosition());

	bool bDown = pMouse->ButtonIsDown(eMouseButton_Left);
	if (bDown && mbMouseWasDown == false)
		mpGui->SendMouseClickDown(eGuiMouseButton_Left);
	else if (bDown == false && mbMouseWasDown)
		mpGui->SendMouseClickUp(eGuiMouseButton_Left);

	mbMouseWasDown = bDown;
}

//-----------------------------------------------------------------------

bool cSomaMainMenu::ButtonNewGame(iWidget *apWidget, const cGuiMessageData &aData)
{
	// Reuses cSomaBase::LoadMap() - the same entry point the headless
	// "start_map" command already uses - rather than duplicating map-load
	// logic here. 00_01_apartment.hpm/PlayerStartArea_1 is the smallest,
	// earliest real SOMA map with a real PlayerStart Area (see
	// PORTING_NOTES.md - extensively tested this session).
	tString sError;
	if (mpBase->LoadMap("00_01_apartment.hpm", cVector3f(0, 1.7f, 0), sError, "PlayerStartArea_1") == false)
	{
		Log("SOMA main menu: New Game failed to load 00_01_apartment.hpm (%s)\n", sError.c_str());
		return true;
	}

	SetVisible(false);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cSomaMainMenu, ButtonNewGame);

//-----------------------------------------------------------------------

bool cSomaMainMenu::ButtonQuit(iWidget *apWidget, const cGuiMessageData &aData)
{
	mpEngine->Exit();
	return true;
}
kGuiCallbackDeclaredFuncEnd(cSomaMainMenu, ButtonQuit);

//-----------------------------------------------------------------------
