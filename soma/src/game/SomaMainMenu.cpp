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

	mpBackgroundGfx = NULL;
	mpTitleGfx = NULL;

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

	////////////////////////////////////
	// Real background/title art - SOMA ships a full 2D menu art set at
	// graphics/startmenu/ (background/title/gfx/icons) that this port
	// wasn't using at all (this class originally drew a plain text title on
	// a black backdrop). None of it is ImGui-rendered - these are ordinary
	// .tga textures meant to be composited by *some* 2D UI system. Drawn
	// directly in OnDraw() via CreateGfxTexture()+DrawGfx() - the same
	// mechanism cSomaSplash already uses successfully - rather than
	// cWidgetImage, whose CreateGfxImage()->cImageManager path failed to
	// find these exact files (real cause not chased down further given a
	// known-working alternative). Real background file is 1920x1080, same
	// 16:9 aspect as this engine's own screen sizes, so a stretch-to-fill
	// has no visible distortion.
	mpBackgroundGfx = mpGui->CreateGfxTexture("startmenu_background.tga", eGuiMaterial_Diffuse, eTextureType_2D);
	// Real title graphic (startmenu_title.tga, 1024x256 - the real
	// glitch-styled "SOMA" logo with its reticle-over-the-O detail). The
	// real asset set also ships 4 numbered "_flicker" variants clearly
	// meant for a cycling glitch-flicker animation, not used here yet - a
	// real follow-up, not attempted this pass (static is still a large
	// step up from a plain text label).
	mpTitleGfx = mpGui->CreateGfxTexture("startmenu_title.tga", eGuiMaterial_Alpha, eTextureType_2D);

	////////////////////////////////////
	// New Game / Quit - still the generic skin's default button graphics,
	// not SOMA's real startmenu_button_long.tga (+ jitter-state variants) -
	// real button art needs a custom iGuiMaterial/skin entry to actually
	// use, not just a texture swap, left as a follow-up. Positioned to
	// sit under the title, same dark-background region.
	cVector3f vNewGamePos(60, 240, 0.1f);
	cWidgetButton *pNewGame = mpGuiSet->CreateWidgetButton(
		vNewGamePos, cVector2f(fButtonWidth, fButtonHeight), _W("New Game"), NULL, false, "NewGame");
	pNewGame->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(ButtonNewGame));

	////////////////////////////////////
	// Quit
	cVector3f vQuitPos(60, 240 + fButtonHeight + fButtonSep, 0.1f);
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

void cSomaMainMenu::OnDraw(float afFrameTime)
{
	if (mbVisible == false)
		return;

	// Drawn here (queued into mpGuiSet, same DrawGfx() mechanism
	// cSomaSplash uses) rather than as widgets so background paints first
	// and title second, both behind the button/label widgets' own draws -
	// see the CreateGui() comment for why cWidgetImage wasn't used.
	if (mpBackgroundGfx)
		mpGuiSet->DrawGfx(mpBackgroundGfx, cVector3f(0, 0, 0), mvScreenSize);

	if (mpTitleGfx)
	{
		const cVector2f vTitleSize(512, 128);
		mpGuiSet->DrawGfx(mpTitleGfx, cVector3f(60, 70, 0.05f), vTitleSize);
	}
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
