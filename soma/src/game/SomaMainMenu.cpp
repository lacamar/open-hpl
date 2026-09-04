/*
 * See SomaMainMenu.h for scope notes and where these real values came from
 * (script/modules/MenuHandler.hps + helper_imgui_options.hps, read directly
 * out of SOMA's own shipped game data).
 */

#include "SomaMainMenu.h"
#include "SomaBase.h"

#include <cmath>

//---------------------------------------
// Real design-space constants, verbatim from MenuHandler.hps/
// helper_imgui_options.hps ("Data for menu, values based on 1280x720
// resolution"). Used directly against a 1280x720 cGuiSet virtual size -
// see the class comment for why that's the real scaling mechanism.

static const cVector2f kVirtualCanvas(1280, 720);

static const cVector2f kMainMenuButtonPos(136, 275);
static const cVector2f kTitlePos(100, 67);
static const cVector2f kTitleSize(4 * 173, 173);
static const cColor kMainMenuButtonBgColor(69.0f / 255.0f, 139.0f / 255.0f, 139.0f / 255.0f, 1.0f);

static const cVector2f kOptionMenuButtonBgSize(16 * 42, 42);
static const float kOptionMenuButtonSpacing = 38;

static const cColor kDisabledColor(58.0f / 255.0f, 1.0f);
static const cColor kSelectedTextColor(0, 1);
static const cColor kDeselectedTextColor(1, 1);

static const float kFrontFaceScale = 0.7f;
static const float kBackFaceScale = 0.3f;

//---------------------------------------

cSomaMainMenu::cSomaMainMenu(cEngine *apEngine, cSomaBase *apBase, cViewport *apViewport) : iUpdateable("SomaMainMenu")
{
	mpEngine = apEngine;
	mpBase = apBase;
	mpViewport = apViewport;

	mpBackgroundGfx = NULL;
	mpCornerUL = mpCornerUR = mpCornerBL = mpCornerBR = NULL;
	mpCathLeft = mpCathRight = mpCathJaw = NULL;
	mpTitleGfx = NULL;
	for (int i = 0; i < 4; ++i)
		mpTitleFlickerGfx[i] = NULL;
	mpButtonBarGfx = NULL;
	for (int i = 0; i < 3; ++i)
		mpButtonBarJitterGfx[i] = NULL;
	mpButtonFont = NULL;

	mbVisible = true;
	mbMouseWasDown = false;

	mfTitleAlpha = 0;
	mfFaceAlpha = 0;
	mfBGAnimTime = 0;

	mfTitleGlitchWaitTimer = cMath::RandRectf(4.0f, 7.0f);
	mlTitleGlitchTimes = 0;
	mfTitleGlitchTimer = 0;
	mlTitleGlitchPic = 0;

	mfTitlePulseTimer = cMath::RandRectf(1.0f, 4.0f);
	mTitleColorStart = cColor(1, 1);
	mTitleColorGoal = cColor(1, 1);
	mfTitleColorFadeT = 1.0f;
	mfTitleColorFadeLen = 1.0f;

	mlHoveredItem = -1;
	mlClickedItem = -1;
	mfButtonClickedTimer = 0;
	mPendingAction = eSomaMainMenuAction_None;

	mpGui = mpEngine->GetGui();

	// Same real skin file cSomaSplash already uses (SOMA ships no separate
	// "main menu" skin) - only needed here for cGuiSet's mouse cursor
	// gfx/DrawGfx machinery, not for any skin-drawn widgets (this menu's
	// real text buttons are drawn directly, see DrawMenuItems()).
	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("SomaMainMenu", mpGuiSkin);

	// Real menu values are all in an 1280x720 design canvas ("Data for
	// menu, values based on 1280x720 resolution" - MenuHandler.hps) scaled
	// to the real screen; cGuiSet::SetVirtualSize() is this engine's own
	// existing equivalent of that (Dark Descent's menus use it too), so
	// use it here instead of hand-rolling the real script's aspect-ratio
	// math. cGui::SendMousePos() already converts incoming mouse
	// coordinates into this same virtual space, so mpGuiSet->GetMousePos()
	// below needs no extra conversion.
	mpGuiSet->SetVirtualSize(kVirtualCanvas, -1000, 1000);

	mpGuiSet->SetDrawMouse(true);
	mpViewport->AddGuiSet(mpGuiSet);
	mpGuiSet->SetActive(true);

	// cGui routes all mouse/keyboard input to a single global "focused" set
	// (cGui::mpSetInFocus, see SendMousePos()/SendMouseClickDown() etc. in
	// Gui.cpp) - nothing sets this automatically just from being active on
	// a viewport. Without this, mpGuiSet->GetMousePos() below never
	// updates. Same call cLuxMainMenu/cLuxPreMenu make in the real Dark
	// Descent menu code (LuxMainMenu.cpp/LuxPreMenu.cpp).
	mpGui->SetFocus(mpGuiSet);

	CreateGui();
}

//-----------------------------------------------------------------------

cSomaMainMenu::~cSomaMainMenu()
{
}

//-----------------------------------------------------------------------

cGuiGfxElement *cSomaMainMenu::CreateGfx(const tString &asFile, eGuiMaterial aMaterial)
{
	return mpGui->CreateGfxTexture(asFile, aMaterial, eTextureType_2D);
}

//-----------------------------------------------------------------------

void cSomaMainMenu::CreateGui()
{
	////////////////////////////////////
	// Real background/title/phase-1 art (graphics/startmenu/) - drawn
	// directly via DrawGfx() rather than cWidgetImage: cWidgetImage's
	// CreateGfxImage()->cImageManager path failed to find these exact
	// files (real cause not chased down further given a known-working
	// alternative). Real background file is 1280x720 native (matches the
	// virtual canvas 1:1).
	mpBackgroundGfx = CreateGfx("menu_background.tga", eGuiMaterial_Diffuse);

	// Phase-1 dirt corners (real mCurrentBGPhase default is
	// eMainMenuPhase_Initial_To_1_3 = 1, i.e. "p1_" - this scaffold has no
	// save/progress system to drive later phases).
	mpCornerUL = CreateGfx("p1_ul.tga", eGuiMaterial_Alpha);
	mpCornerUR = CreateGfx("p1_ur.tga", eGuiMaterial_Alpha);
	mpCornerBL = CreateGfx("p1_bl.tga", eGuiMaterial_Alpha);
	mpCornerBR = CreateGfx("p1_br.tga", eGuiMaterial_Alpha);

	// Real "cathedral" ghost-trail pieces (GuiBackground_DrawCathFacePart).
	mpCathLeft = CreateGfx("p1_left.tga", eGuiMaterial_Alpha);
	mpCathRight = CreateGfx("p1_right.tga", eGuiMaterial_Alpha);
	mpCathJaw = CreateGfx("p1_jaw.tga", eGuiMaterial_Alpha);

	// Real title graphic + its 4 glitch-flicker variants.
	mpTitleGfx = CreateGfx("startmenu_title.tga", eGuiMaterial_Alpha);
	mpTitleFlickerGfx[0] = CreateGfx("startmenu_title_flicker1.tga", eGuiMaterial_Alpha);
	mpTitleFlickerGfx[1] = CreateGfx("startmenu_title_flicker2.tga", eGuiMaterial_Alpha);
	mpTitleFlickerGfx[2] = CreateGfx("startmenu_title_flicker3.tga", eGuiMaterial_Alpha);
	mpTitleFlickerGfx[3] = CreateGfx("startmenu_title_flicker4.tga", eGuiMaterial_Alpha);

	// Real highlight bar behind the focused menu item + its click-flash
	// jitter frames (OptionMenu_ButtonBackground).
	mpButtonBarGfx = CreateGfx("startmenu_button_long.tga", eGuiMaterial_Alpha);
	mpButtonBarJitterGfx[0] = CreateGfx("startmenu_button_long_jitter2.tga", eGuiMaterial_Alpha);
	mpButtonBarJitterGfx[1] = CreateGfx("startmenu_button_long_jitter3.tga", eGuiMaterial_Alpha);
	mpButtonBarJitterGfx[2] = CreateGfx("startmenu_button_long_jitter4.tga", eGuiMaterial_Alpha);

	// Real main-menu font (Sansation Large Bold, 36px - see
	// OptionMenu_ButtonMainMenu()), not the generic skin's "Default" font.
	mpButtonFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_large_bold.fnt");

	////////////////////////////////////
	// Real menu item list/order/captions (MainMenu.Continue/NewGame/
	// LoadGame/Options/Exit in config/base_english.lang) and real
	// enable rule: with no save system in this scaffold, mbCanContinue is
	// always false, same as a real fresh install - Continue/LoadGame show
	// as disabled labels rather than buttons (GuiMainMenuSelection()).
	mItems[0].msLabel = _W("CONTINUE");
	mItems[0].mbEnabled = false;
	mItems[0].mAction = eSomaMainMenuAction_None;

	mItems[1].msLabel = _W("NEW GAME");
	mItems[1].mbEnabled = true;
	mItems[1].mAction = eSomaMainMenuAction_NewGame;

	mItems[2].msLabel = _W("LOAD GAME");
	mItems[2].mbEnabled = false;
	mItems[2].mAction = eSomaMainMenuAction_None;

	mItems[3].msLabel = _W("OPTIONS");
	mItems[3].mbEnabled = true;
	mItems[3].mAction = eSomaMainMenuAction_Options;

	mItems[4].msLabel = _W("EXIT");
	mItems[4].mbEnabled = true;
	mItems[4].mAction = eSomaMainMenuAction_Exit;

	for (int i = 0; i < 5; ++i)
		mItems[i].mfRowY = kMainMenuButtonPos.y + kOptionMenuButtonSpacing * i;
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

	UpdateMouseHitTest();

	bool bDown = pMouse->ButtonIsDown(eMouseButton_Left);
	if (bDown && mbMouseWasDown == false && mlHoveredItem != -1 && mlClickedItem == -1)
	{
		ClickItem(mItems[mlHoveredItem]);
	}
	mbMouseWasDown = bDown;

	if (mlClickedItem != -1)
	{
		mfButtonClickedTimer -= afTimeStep;
		if (mfButtonClickedTimer <= 0)
			RunPendingAction();
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::UpdateMouseHitTest()
{
	mlHoveredItem = -1;

	const cVector2f &vMouse = mpGuiSet->GetMousePos();

	// Real OptionMenu_ButtonMainMenu() hit-tests an 8000-unit-wide row
	// (ImGui_DoButtonExt with kOptionMenu_ButtonSize.x=8000) - effectively
	// "anywhere to the right of the label, to the edge of the screen".
	for (int i = 0; i < 5; ++i)
	{
		if (mItems[i].mbEnabled == false)
			continue;

		float fTop = mItems[i].mfRowY;
		float fBottom = fTop + kOptionMenuButtonSpacing;
		if (vMouse.x >= kMainMenuButtonPos.x && vMouse.x <= kVirtualCanvas.x && vMouse.y >= fTop && vMouse.y <= fBottom)
		{
			mlHoveredItem = i;
			break;
		}
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::ClickItem(cSomaMainMenuItem &aItem)
{
	// Real OptionMenu_ButtonMainMenu()/GuiMainMenuSelection(): a click
	// starts a 0.15s "ButtonClicked" flash (jitter background) and only
	// performs the actual action once that timer elapses.
	mlClickedItem = mlHoveredItem;
	mfButtonClickedTimer = 0.15f;
	mPendingAction = aItem.mAction;
}

//-----------------------------------------------------------------------

void cSomaMainMenu::RunPendingAction()
{
	eSomaMainMenuAction action = mPendingAction;
	mlClickedItem = -1;
	mPendingAction = eSomaMainMenuAction_None;

	switch (action)
	{
	case eSomaMainMenuAction_NewGame:
	{
		// Reuses cSomaBase::LoadMap() - the same entry point the headless
		// "start_map" command already uses - rather than duplicating
		// map-load logic here. 00_01_apartment.hpm/PlayerStartArea_1 is
		// the smallest, earliest real SOMA map with a real PlayerStart
		// Area (see PORTING_NOTES.md).
		tString sError;
		if (mpBase->LoadMap("00_01_apartment.hpm", cVector3f(0, 1.7f, 0), sError, "PlayerStartArea_1") == false)
		{
			Log("SOMA main menu: New Game failed to load 00_01_apartment.hpm (%s)\n", sError.c_str());
			return;
		}
		SetVisible(false);
		break;
	}
	case eSomaMainMenuAction_Options:
		// Real menu has a full Options screen (Audio/Video/Input/
		// Gameplay - eMainMenuGroup_Options*); not implemented in this
		// scaffold, honestly a no-op rather than a fake screen.
		Log("SOMA main menu: Options is not implemented in this scaffold yet\n");
		break;
	case eSomaMainMenuAction_Exit:
		// Real menu shows an "ARE YOU SURE YOU WANT TO EXIT?" confirm box
		// first (mbShowExit); not reproduced here, exits immediately.
		mpEngine->Exit();
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::OnDraw(float afFrameTime)
{
	if (mbVisible == false)
		return;

	DrawBackground(afFrameTime);
	DrawTitle(afFrameTime);
	DrawMenuItems();
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawBackground(float afTimeStep)
{
	// Z ordering below mirrors the real script's relative depth (it adds a
	// constant +20 to every one of these via OptionMenu_GetTopLeftOffset()
	// - only the relative order matters here, not the absolute values):
	// background(-1.0) < face(-0.005) < title(0.0) < title-ghost/buttons
	// (1.0) < corners(22.5), all offset down by 10 here to leave headroom.
	if (mpBackgroundGfx)
		mpGuiSet->DrawGfx(mpBackgroundGfx, cVector3f(0, 0, -10.0f), kVirtualCanvas);

	// Real dirt-corner overlays, alpha 0.3, aligned flush to each corner
	// of the canvas (GuiBackground()) - real fZ=22.5 sits above every
	// other element here (even the buttons), so this scaffold matches
	// that instead of assuming it's a mistake in the real menu.
	const cColor cornerCol(1.0f, 0.3f);
	const float fCornerZ = 12.5f;
	if (mpCornerUL)
		mpGuiSet->DrawGfx(mpCornerUL, cVector3f(0, 0, fCornerZ), mpCornerUL->GetImageSize(), cornerCol);
	if (mpCornerUR)
	{
		cVector2f vSize = mpCornerUR->GetImageSize();
		mpGuiSet->DrawGfx(mpCornerUR, cVector3f(kVirtualCanvas.x - vSize.x, 0, fCornerZ), vSize, cornerCol);
	}
	if (mpCornerBL)
	{
		cVector2f vSize = mpCornerBL->GetImageSize();
		mpGuiSet->DrawGfx(mpCornerBL, cVector3f(0, kVirtualCanvas.y - vSize.y, fCornerZ), vSize, cornerCol);
	}
	if (mpCornerBR)
	{
		cVector2f vSize = mpCornerBR->GetImageSize();
		mpGuiSet->DrawGfx(mpCornerBR, cVector3f(kVirtualCanvas.x - vSize.x, kVirtualCanvas.y - vSize.y, fCornerZ), vSize, cornerCol);
	}

	// Real "cath's face" 3-layer ghost trail, fading in over ~2s
	// (mfBackgroundFaceAlpha += afTimeStep*0.5).
	mfFaceAlpha = cMath::Clamp(mfFaceAlpha + afTimeStep * 0.5f, 0.0f, 1.0f);
	mfBGAnimTime += afTimeStep;

	if (mfFaceAlpha > 0 && mpCathLeft && mpCathRight && mpCathJaw)
	{
		cVector3f vFacePos(960, 400, -1.0f);
		cColor colorMul(mfFaceAlpha, mfFaceAlpha);
		float fT = mfBGAnimTime * 0.05f;

		DrawCathFacePart(mpCathLeft,
						  vFacePos + cVector3f(cosf(fT), -sinf(fT), 0) * 10.0f * cosf(mfBGAnimTime * 0.075f),
						  cVector3f(vFacePos.x, vFacePos.y, vFacePos.z - 0.002f),
						  kFrontFaceScale, kBackFaceScale,
						  cColor(1, 1) * colorMul, cColor(0.3f, 0.8f) * colorMul, 3);

		DrawCathFacePart(mpCathRight,
						  vFacePos + cVector3f(-cosf(fT), -sinf(fT), 0) * 8.0f * sinf(mfBGAnimTime * 0.075f),
						  cVector3f(vFacePos.x + 30, vFacePos.y, vFacePos.z - 0.002f),
						  kFrontFaceScale, kBackFaceScale,
						  cColor(1, 1) * colorMul, cColor(0.3f, 0.8f) * colorMul, 3);

		DrawCathFacePart(mpCathJaw,
						  vFacePos + cVector3f(-cosf(fT), sinf(fT / 2.0f), 0) * 10.0f,
						  cVector3f(vFacePos.x, vFacePos.y + 50, vFacePos.z - 0.002f),
						  kFrontFaceScale, kBackFaceScale,
						  cColor(1, 1) * colorMul, cColor(0.3f, 0.8f) * colorMul, 3);
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawCathFacePart(cGuiGfxElement *apGfx, const cVector3f &avFrontCenterPos, const cVector3f &avBackCenterPos,
									  float afFrontScale, float afBackScale, const cColor &aFrontCol, const cColor &aBackCol, int alLayers)
{
	if (alLayers <= 0)
		alLayers = 1;

	cVector2f vOriginalSize = apGfx->GetImageSize();
	cVector2f vSize = vOriginalSize * afFrontScale;

	mpGuiSet->DrawGfx(apGfx, avFrontCenterPos - cVector3f(vSize.x, vSize.y, 0) * 0.5f, vSize, aFrontCol);

	if (alLayers == 1)
		return;

	int lSteps = alLayers - 1;
	cVector3f vDir = avBackCenterPos - avFrontCenterPos;
	cVector3f vStep = vDir * (1.0f / (float)lSteps);

	float fScaleStep = (afBackScale - afFrontScale) / (float)lSteps;
	cColor colStep((aBackCol.r - aFrontCol.r) / lSteps, (aBackCol.g - aFrontCol.g) / lSteps,
					(aBackCol.b - aFrontCol.b) / lSteps, (aBackCol.a - aFrontCol.a) / lSteps);

	cVector3f vPos = avFrontCenterPos;
	float fScale = afFrontScale;
	cColor col = aFrontCol;

	for (int i = 0; i < lSteps; ++i)
	{
		vPos += vStep;
		fScale += fScaleStep;
		vSize = vOriginalSize * fScale;

		col.r += colStep.r;
		col.g += colStep.g;
		col.b += colStep.b;
		col.a += colStep.a;

		mpGuiSet->DrawGfx(apGfx, vPos - cVector3f(vSize.x, vSize.y, 0) * 0.5f, vSize, col);
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawTitle(float afTimeStep)
{
	mfTitleAlpha = cMath::Clamp(mfTitleAlpha + afTimeStep * 0.5f, 0.0f, 1.0f);
	if (mfTitleAlpha <= 0 || mpTitleGfx == NULL)
		return;

	////////////////////////////////////
	// Real glitch-flicker state machine (GuiBackground(), "TitleGlitchWait"
	// / "TitleGlitchTimes" / "TitleGlitch" / "TitleGlitchPic").
	mfTitleGlitchWaitTimer -= afTimeStep;
	if (mfTitleGlitchWaitTimer <= 0 && mlTitleGlitchTimes <= 0)
	{
		mlTitleGlitchTimes = cMath::RandRectl(3, 5);
		mfTitleGlitchTimer = 0;
	}

	if (mlTitleGlitchTimes > 0)
	{
		mfTitleGlitchTimer -= afTimeStep;
		if (mfTitleGlitchTimer <= 0)
		{
			mlTitleGlitchPic = cMath::RandRectl(1, 4);
			--mlTitleGlitchTimes;
			mfTitleGlitchTimer = cMath::RandRectf(0.01f, 0.1f);

			if (mlTitleGlitchTimes <= 0)
			{
				mlTitleGlitchPic = 0;
				mfTitleGlitchWaitTimer = cMath::RandRectf(4.0f, 7.0f);
			}
		}
	}

	////////////////////////////////////
	// Real slow colour pulse ("TitlePulse" timer + "TitleColor" fade).
	mfTitlePulseTimer -= afTimeStep;
	if (mfTitlePulseTimer <= 0)
	{
		float fComponent = cMath::RandRectf(0.5f, 3.0f);
		mTitleColorStart = mTitleColorGoal;
		mTitleColorGoal = cColor(fComponent, cMath::Clamp(fComponent, 0.0f, 1.0f));
		mfTitleColorFadeLen = cMath::RandRectf(0.05f, 0.25f);
		mfTitleColorFadeT = 0;
		mfTitlePulseTimer = cMath::RandRectf(1.0f, 4.0f);
	}
	if (mfTitleColorFadeT < mfTitleColorFadeLen)
		mfTitleColorFadeT = cMath::Min(mfTitleColorFadeT + afTimeStep, mfTitleColorFadeLen);

	float fFadeAlpha = mfTitleColorFadeLen > 0 ? mfTitleColorFadeT / mfTitleColorFadeLen : 1.0f;
	cColor titleCol;
	titleCol.r = mTitleColorStart.r + (mTitleColorGoal.r - mTitleColorStart.r) * fFadeAlpha;
	titleCol.g = mTitleColorStart.g + (mTitleColorGoal.g - mTitleColorStart.g) * fFadeAlpha;
	titleCol.b = mTitleColorStart.b + (mTitleColorGoal.b - mTitleColorStart.b) * fFadeAlpha;
	titleCol.a = mTitleColorStart.a + (mTitleColorGoal.a - mTitleColorStart.a) * fFadeAlpha;

	cColor colorMul(1, mfTitleAlpha);

	cGuiGfxElement *pTitleGfx = mpTitleGfx;
	bool bGlitching = false;
	if (mlTitleGlitchPic > 0 && mpTitleFlickerGfx[mlTitleGlitchPic - 1])
	{
		pTitleGfx = mpTitleFlickerGfx[mlTitleGlitchPic - 1];
		bGlitching = true;
	}

	mpGuiSet->DrawGfx(pTitleGfx, cVector3f(kTitlePos.x, kTitlePos.y, 0.0f), kTitleSize, titleCol * colorMul);

	if (bGlitching)
	{
		cVector2f vGhostPos = kTitlePos + cVector2f(20.0f, 20.0f);
		mpGuiSet->DrawGfx(mpTitleGfx, cVector3f(vGhostPos.x, vGhostPos.y, 1.0f), kTitleSize, cColor(1.0f, 0.5f) * colorMul);
	}
}

//-----------------------------------------------------------------------

void cSomaMainMenu::DrawMenuItems()
{
	for (int i = 0; i < 5; ++i)
	{
		cSomaMainMenuItem &item = mItems[i];
		cVector3f vPos(kMainMenuButtonPos.x, item.mfRowY, 1.0f);

		if (item.mbEnabled == false)
		{
			if (mpButtonFont)
				mpGuiSet->DrawFont(item.msLabel, mpButtonFont, vPos, cVector2f(36, 36), kDisabledColor, eFontAlign_Left);
			continue;
		}

		bool bSelected = (i == mlHoveredItem && mlClickedItem == -1) || i == mlClickedItem;

		if (bSelected)
		{
			cVector3f vBarPos(kMainMenuButtonPos.x - 22.0f, item.mfRowY, 0.5f);
			if (i == mlClickedItem)
			{
				// Real click-flash: a random jitter frame at full white,
				// only for the 0.15s "ButtonClicked" window.
				int lFrame = cMath::RandRectl(0, 2);
				if (mpButtonBarJitterGfx[lFrame])
					mpGuiSet->DrawGfx(mpButtonBarJitterGfx[lFrame], vBarPos, kOptionMenuButtonBgSize, cColor(1, 1));
			}
			else if (mpButtonBarGfx)
			{
				mpGuiSet->DrawGfx(mpButtonBarGfx, vBarPos, kOptionMenuButtonBgSize, kMainMenuButtonBgColor);
			}
		}

		if (mpButtonFont)
		{
			const cColor &textCol = bSelected ? kSelectedTextColor : kDeselectedTextColor;
			mpGuiSet->DrawFont(item.msLabel, mpButtonFont, vPos, cVector2f(36, 36), textCol, eFontAlign_Left);
		}
	}
}
