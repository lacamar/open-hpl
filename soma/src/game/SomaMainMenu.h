/*
 * A real, interactive main menu for the SOMA Phase 0/1 scaffold.
 *
 * SOMA's actual main menu is ImGui + an AngelScript-driven main_menu.hps -
 * a toolkit this engine port has no integration for at all (confirmed, not
 * attempted - see PORTING_NOTES.md). This is a plain native GuiSet menu
 * built the same way amnesia/src/game/LuxMainMenu.{h,cpp} builds Dark
 * Descent's menu (cGuiSet + cWidgetButton, skin-drawn, callback-driven),
 * trimmed to what this scaffold actually needs: a "New Game" button that
 * loads a real map via cSomaBase::LoadMap(), and a "Quit" button. No
 * profiles, custom stories, or key config - none of that exists here.
 *
 * Owned by cSomaBase, created once by InitMainMenuScene() and attached to
 * the same real camera+world viewport (mpDebugViewport) that scene already
 * creates - not a separate GUI-only viewport like cSomaSplash uses, since
 * there's a real scene behind this menu (even though main_menu.hpm itself
 * is empty).
 *
 * iUpdateable (registered via cUpdater::AddGlobalUpdate(), same idiom as
 * cSomaSplash/cSomaDebugFreeCamera) because cGui does NOT automatically
 * route mouse input to widgets - nothing in HPL2/core polls iMouse and
 * calls cGui::SendMousePos()/SendMouseClickDown()/Up() for you; the real
 * Dark Descent game module does that itself every frame in
 * amnesia/src/game/LuxInputHandler.cpp, which this Phase 0 scaffold has no
 * equivalent of. Found live: buttons were visible and the GUI set was
 * focused, but clicks were silently dropped and the mouse cursor never
 * left (0,0) until this class started pumping mouse state into cGui itself.
 */

#ifndef SOMA_MAIN_MENU_H
#define SOMA_MAIN_MENU_H

#include "hpl.h"

using namespace hpl;

class cSomaBase;

//----------------------------------------------

class cSomaMainMenu : public iUpdateable
{
public:
	cSomaMainMenu(cEngine *apEngine, cSomaBase *apBase, cViewport *apViewport);
	~cSomaMainMenu();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

	// Hides the menu and stops it from eating mouse input, without
	// destroying it - used once "New Game" has actually loaded a map, so
	// the buttons don't keep drawing/intercepting clicks over gameplay.
	void SetVisible(bool abVisible);

private:
	void CreateGui();

	bool ButtonNewGame(iWidget *apWidget, const cGuiMessageData &aData);
	kGuiCallbackDeclarationEnd(ButtonNewGame);

	bool ButtonQuit(iWidget *apWidget, const cGuiMessageData &aData);
	kGuiCallbackDeclarationEnd(ButtonQuit);

	cEngine *mpEngine;
	cSomaBase *mpBase;
	cViewport *mpViewport;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;

	cVector2f mvScreenSize;

	// Real SOMA menu art (graphics/startmenu/) - drawn directly via
	// DrawGfx() in OnDraw(), the same mechanism cSomaSplash already proved
	// reliable, rather than cWidgetImage: cWidgetImage's CreateGfxImage()
	// path goes through cImageManager, which failed to find these exact
	// same files (real cause not fully chased down - not worth another
	// investigation given a known-working alternative existed).
	cGuiGfxElement *mpBackgroundGfx;
	cGuiGfxElement *mpTitleGfx;

	bool mbVisible;
	bool mbMouseWasDown;
};

//----------------------------------------------

#endif // SOMA_MAIN_MENU_H
