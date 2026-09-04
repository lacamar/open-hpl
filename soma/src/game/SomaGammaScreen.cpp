/*
 * See SomaGammaScreen.h for scope notes.
 */

#include "SomaGammaScreen.h"
#include "SomaBase.h"

#include <fstream>

//---------------------------------------

cSomaGammaScreen::cSomaGammaScreen(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaGammaScreen")
{
	mpEngine = apEngine;
	mpBase = apBase;

	mbFinished = false;
	mbMouseWasDown = false;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("GammaScreen", mpGuiSkin);
	mpGuiSet->SetDrawMouse(true);

	// GUI-only viewport, same idiom as cSomaSplash - no camera/world exists
	// yet at this point in boot.
	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, true);
	mpViewport->AddGuiSet(mpGuiSet);

	mpGuiSet->SetActive(true);
	mpGui->SetFocus(mpGuiSet);

	// Real assets, confirmed present at graphics/startmenu/misc/ in a real
	// SOMA install - gamma_background.tga is full-bleed, gamma.tga is the
	// checkerboard test pattern the user adjusts until barely visible.
	mpBackgroundGfx = mpGui->CreateGfxTexture("gamma_background.tga", eGuiMaterial_Diffuse, eTextureType_2D);
	mpCheckerboardGfx = mpGui->CreateGfxTexture("gamma.tga", eGuiMaterial_Alpha, eTextureType_2D);

	if (mpCheckerboardGfx)
	{
		mvCheckerboardSize = mpCheckerboardGfx->GetImageSize();
		mvCheckerboardPos = cVector2f((mvScreenSize.x - mvCheckerboardSize.x) * 0.5f,
									   (mvScreenSize.y - mvCheckerboardSize.y) * 0.5f - 40);
	}

	////////////////////////////////////
	// Gamma slider - same mechanism as Dark Descent's cLuxPreMenu
	// (amnesia/src/game/LuxPreMenu.cpp), same min/max range.
	mfGammaMinValue = 0.3f;
	mfGammaMaxValue = 2.0f;

	cVector2f vSliderSize(300, 25);
	cVector3f vSliderPos((mvScreenSize.x - vSliderSize.x) * 0.5f,
						  mvCheckerboardPos.y + mvCheckerboardSize.y + 30, 0.1f);

	mpSlider = mpGuiSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal, vSliderPos, vSliderSize, 100, NULL);
	mpSlider->AddCallback(eGuiMessage_SliderMove, this, &cSomaGammaScreen::GammaSliderMoved_static_gui);

	// Initialize the slider to the engine's current gamma value, same
	// clamp/round math as cLuxPreMenu::SetGammaValueToInput().
	{
		float fCurrentGamma = mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
		fCurrentGamma = cMath::Clamp(fCurrentGamma, mfGammaMinValue, mfGammaMaxValue);
		int lValue = cMath::RoundToInt((fCurrentGamma - mfGammaMinValue) * 100.0f / (mfGammaMaxValue - mfGammaMinValue));
		mpSlider->SetValue(lValue, false);
	}

	////////////////////////////////////
	// Continue button - deliberately a real widget (not a global "any
	// click continues" like cSomaSplash uses) so dragging the slider can
	// never be misread as "continue" - only Enter/Escape or clicking this
	// specific button advance past the screen.
	cVector2f vButtonSize(120, 30);
	cVector3f vButtonPos((mvScreenSize.x - vButtonSize.x) * 0.5f,
						  vSliderPos.y + vSliderSize.y + 30, 0.1f);
	mpContinueButton = mpGuiSet->CreateWidgetButton(vButtonPos, vButtonSize, _W("Continue"), NULL);
	mpContinueButton->AddCallback(eGuiMessage_ButtonPressed, this, &cSomaGammaScreen::ContinuePressed_static_gui);

	mpGuiSet->SetDefaultFocusNavWidget(mpContinueButton);
	mpGuiSet->SetFocusedWidget(mpContinueButton);
}

//-----------------------------------------------------------------------

cSomaGammaScreen::~cSomaGammaScreen()
{
}

//-----------------------------------------------------------------------

bool cSomaGammaScreen::ShouldShowAndMarkSeen()
{
	tWString sStateRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome);
	tWString sStateDir = sStateRoot + _W("open-hpl/soma/");
	if (cPlatform::FolderExists(sStateDir) == false)
		cPlatform::CreateFolder(sStateDir);

	tWString sMarkerFile = sStateDir + _W("gamma_screen_seen");

	if (cPlatform::FileExists(sMarkerFile))
		return false;

	std::ofstream markerStream(cString::To8Char(sMarkerFile).c_str());
	if (markerStream.is_open())
	{
		markerStream << "1\n";
		markerStream.close();
	}

	return true;
}

//-----------------------------------------------------------------------

void cSomaGammaScreen::Finish()
{
	if (mbFinished)
		return;

	mbFinished = true;

	mpViewport->SetActive(false);

	// Deactivating the viewport alone doesn't stop this - a GuiSet's own
	// SetActive()/focus state (here: true/this-set, set in the
	// constructor) is tracked separately from whatever viewport it's
	// attached to, and cGuiSet's real cWidgetSlider/cWidgetButton (unlike
	// cSomaSplash's plain OnDraw()-gated DrawGfx calls, which this
	// mbFinished check already handles) render themselves via cGui's own
	// widget pass regardless of viewport activity. Without this, the
	// slider/Continue button kept rendering on top of every later scene
	// (main menu, then real gameplay maps) - confirmed live, screenshots
	// showing them still present well after Finish() ran. Same two calls
	// cSomaMainMenu::SetVisible(false) already makes for its own GuiSet.
	mpGuiSet->SetActive(false);
	if (mpGui->GetFocusedSet() == mpGuiSet)
		mpGui->SetFocus(NULL);

	// Persist whatever gamma the user landed on, so it survives past this
	// process - without this, the calibration would only ever affect the
	// live cLowLevelGraphics state for the current run (SetGammaCorrection()
	// above in GammaSliderMoved() has no config-writing side effect of its
	// own), and the real Options screen's own Gamma slider (see
	// SomaMainMenu.cpp/SomaConfig.h) would silently forget it on next boot.
	if (mpBase)
	{
		mpBase->GetConfig()->mfGamma = mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
		mpBase->GetConfig()->Save();

		mpBase->OnGammaScreenFinished();
	}
}

//-----------------------------------------------------------------------

bool cSomaGammaScreen::AnyContinueInputThisFrame()
{
	cInput *pInput = mpEngine->GetInput();
	if (pInput == NULL)
		return false;

	iKeyboard *pKeyboard = pInput->GetKeyboard();
	if (pKeyboard && pKeyboard->KeyIsPressed())
	{
		// Same drain-one-event pattern as cSomaSplash::AnySkipInputThisFrame()
		// - only reacts to real, distinct key presses, and only Enter/
		// Escape/Space specifically (not "any key", since typing isn't
		// possible on this screen anyway, but being explicit matches the
		// real intent - confirm/skip keys, not e.g. arrow keys which a
		// keyboard-driven slider-focus-nav might otherwise want).
		eKey key = pKeyboard->GetKey().mKey;
		if (key == eKey_Return || key == eKey_Escape || key == eKey_Space)
			return true;
		return false;
	}

	return false;
}

//-----------------------------------------------------------------------

void cSomaGammaScreen::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	// cGui does not poll iMouse on its own anywhere in this engine - the
	// real game's own LuxInputHandler.cpp does this manually every frame
	// for Dark Descent (see its UpdateGlobalInput()), and cSomaMainMenu.cpp
	// does the same locally for its own hand-rolled hit-testing. This
	// screen uses real cWidgetSlider/cWidgetButton widgets instead (unlike
	// the main menu), which route through cGui's own internal widget click
	// handling - SendMousePos()+SendMouseClickDown()/Up() (the same calls
	// LuxInputHandler.cpp makes) is the minimal real pump those need.
	iMouse *pMouse = mpEngine->GetInput()->GetMouse();
	if (pMouse)
	{
		mpGui->SendMousePos(pMouse->GetAbsPosition(), pMouse->GetRelPosition());

		bool bDown = pMouse->ButtonIsDown(eMouseButton_Left);
		if (bDown && mbMouseWasDown == false)
			mpGui->SendMouseClickDown(eGuiMouseButton_Left);
		if (bDown == false && mbMouseWasDown)
			mpGui->SendMouseClickUp(eGuiMouseButton_Left);
		mbMouseWasDown = bDown;
	}

	if (AnyContinueInputThisFrame())
		Finish();
}

//-----------------------------------------------------------------------

void cSomaGammaScreen::OnDraw(float afFrameTime)
{
	if (mbFinished)
		return;

	if (mpBackgroundGfx)
		mpGuiSet->DrawGfx(mpBackgroundGfx, cVector3f(0, 0, 0), mvScreenSize);

	if (mpCheckerboardGfx)
		mpGuiSet->DrawGfx(mpCheckerboardGfx, cVector3f(mvCheckerboardPos.x, mvCheckerboardPos.y, 0.1f), mvCheckerboardSize);
}

//-----------------------------------------------------------------------

bool cSomaGammaScreen::GammaSliderMoved_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData)
{
	return ((cSomaGammaScreen *)apObject)->GammaSliderMoved(apWidget, aData);
}

bool cSomaGammaScreen::GammaSliderMoved(iWidget *apWidget, const cGuiMessageData &aData)
{
	float fSliderRelValue = ((float)mpSlider->GetValue()) / (float)mpSlider->GetMaxValue();
	float fGamma = mfGammaMinValue + (mfGammaMaxValue - mfGammaMinValue) * fSliderRelValue;

	mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(fGamma);

	return true;
}

//-----------------------------------------------------------------------

bool cSomaGammaScreen::ContinuePressed_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData)
{
	return ((cSomaGammaScreen *)apObject)->ContinuePressed(apWidget, aData);
}

bool cSomaGammaScreen::ContinuePressed(iWidget *apWidget, const cGuiMessageData &aData)
{
	Finish();
	return true;
}

//-----------------------------------------------------------------------
