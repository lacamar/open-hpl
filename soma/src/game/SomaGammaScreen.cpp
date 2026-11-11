#include "SomaGammaScreen.h"
#include "SomaBase.h"
#include "SomaLuxGame.h"
#include "RustSomaUi.h"

#include <fstream>

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

	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, true);
	mpViewport->AddGuiSet(mpGuiSet);

	mpGuiSet->SetActive(true);
	mpGui->SetFocus(mpGuiSet);

	mpBackgroundGfx = mpGui->CreateGfxTexture("gamma_background.tga", eGuiMaterial_Diffuse, eTextureType_2D);
	mpCheckerboardGfx = mpGui->CreateGfxTexture("gamma.tga", eGuiMaterial_Alpha, eTextureType_2D);

	if (mpCheckerboardGfx)
		mvCheckerboardSize = mpCheckerboardGfx->GetImageSize();

	mpInstructionsFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_large_bold.fnt");
	mfInstructionsFontHeight = 24.0f;
	mfInstructionsRowHeight = mfInstructionsFontHeight + 6.0f;

	float fWrapWidth = 0;
	if (mpInstructionsFont)
	{
		fWrapWidth = ohpl_gs7_gamma_wrap_width(mvScreenSize.x);

		mpInstructionsFont->GetWordWrapRows(fWrapWidth, mfInstructionsRowHeight,
											 cVector2f(mfInstructionsFontHeight, mfInstructionsFontHeight),
											 _W("Adjust gamma so you can barely make out the details on the robot poster on the left."),
											 &mvInstructionRows);
	}

	OhplGs7GammaLayout layout;
	ohpl_gs7_gamma_layout(mvScreenSize.v, mpCheckerboardGfx ? mvCheckerboardSize.v : NULL, fWrapWidth, mvInstructionRows.size(), &layout);
	if (mpCheckerboardGfx)
		mvCheckerboardPos = cVector2f(layout.checker_pos[0], layout.checker_pos[1]);
	if (mpInstructionsFont)
		mvInstructionsPos = cVector2f(layout.instructions_pos[0], layout.instructions_pos[1]);

	mfGammaMinValue = 0.3f;
	mfGammaMaxValue = 2.0f;

	cVector2f vSliderSize(layout.slider_size[0], layout.slider_size[1]);
	cVector3f vSliderPos(layout.slider_pos[0], layout.slider_pos[1], layout.slider_pos[2]);

	mpSlider = mpGuiSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal, vSliderPos, vSliderSize, 100, NULL);
	mpSlider->AddCallback(eGuiMessage_SliderMove, this, &cSomaGammaScreen::GammaSliderMoved_static_gui);

	{
		float fCurrentGamma = mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
		mpSlider->SetValue(ohpl_gs7_gamma_to_slider(fCurrentGamma), false);
	}

	// Real button: dragging the slider must not count as continue
	cVector2f vButtonSize(layout.button_size[0], layout.button_size[1]);
	cVector3f vButtonPos(layout.button_pos[0], layout.button_pos[1], layout.button_pos[2]);
	mpContinueButton = mpGuiSet->CreateWidgetButton(vButtonPos, vButtonSize, _W("Continue"), NULL);
	mpContinueButton->AddCallback(eGuiMessage_ButtonPressed, this, &cSomaGammaScreen::ContinuePressed_static_gui);

	mpGuiSet->SetDefaultFocusNavWidget(mpContinueButton);
	mpGuiSet->SetFocusedWidget(mpContinueButton);
}

cSomaGammaScreen::~cSomaGammaScreen()
{
}

bool cSomaGammaScreen::ShouldShowAndMarkSeen()
{
	tWString sStateRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome);
	tWString sStateDir = sStateRoot + _W("open-hpl/") + SomaGameIdW();
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

void cSomaGammaScreen::Finish()
{
	if (mbFinished)
		return;

	mbFinished = true;

	mpViewport->SetActive(false);

	// cGui draws widgets regardless of viewport activity
	mpGuiSet->SetActive(false);
	if (mpGui->GetFocusedSet() == mpGuiSet)
		mpGui->SetFocus(NULL);

	if (mpBase)
	{
		float fGamma = mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
		mpBase->GetConfig()->mfGamma = fGamma;
		mpBase->GetConfig()->Save();
		if (cConfigFile *pUser = SomaUserConfig())
		{
			pUser->SetFloat("Graphics", "Brightness", fGamma);
			pUser->Save();
		}

		mpBase->OnGammaScreenFinished();
	}
}

bool cSomaGammaScreen::AnyContinueInputThisFrame()
{
	cInput *pInput = mpEngine->GetInput();
	if (pInput == NULL)
		return false;

	iKeyboard *pKeyboard = pInput->GetKeyboard();
	if (pKeyboard && pKeyboard->KeyIsPressed())
	{
		eKey key = pKeyboard->GetKey().mKey;
		if (key == eKey_Return || key == eKey_Escape || key == eKey_Space)
			return true;
		return false;
	}

	return false;
}

void cSomaGammaScreen::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	// cGui does not poll iMouse itself
	iMouse *pMouse = mpEngine->GetInput()->GetMouse();
	if (pMouse)
	{
		mpGui->SendMousePos(pMouse->GetAbsPosition(), pMouse->GetRelPosition());

		bool bDown = pMouse->ButtonIsDown(eMouseButton_Left);
		int lEdge = ohpl_gs7_mouse_edge(bDown, mbMouseWasDown);
		if (lEdge == 1)
			mpGui->SendMouseClickDown(eGuiMouseButton_Left);
		if (lEdge == 2)
			mpGui->SendMouseClickUp(eGuiMouseButton_Left);
		mbMouseWasDown = bDown;
	}

	if (AnyContinueInputThisFrame())
		Finish();
}

void cSomaGammaScreen::OnDraw(float afFrameTime)
{
	if (mbFinished)
		return;

	if (mpBackgroundGfx)
		mpGuiSet->DrawGfx(mpBackgroundGfx, cVector3f(0, 0, 0), mvScreenSize);

	if (mpCheckerboardGfx)
		mpGuiSet->DrawGfx(mpCheckerboardGfx, cVector3f(mvCheckerboardPos.x, mvCheckerboardPos.y, 0.1f), mvCheckerboardSize);

	// The logo is baked into gamma_background.tga
	if (mpInstructionsFont)
	{
		for (size_t i = 0; i < mvInstructionRows.size(); ++i)
		{
			cVector3f vRowPos(mvInstructionsPos.x, mvInstructionsPos.y + mfInstructionsRowHeight * (float)i, 0.2f);
			mpGuiSet->DrawFont(mvInstructionRows[i], mpInstructionsFont, vRowPos,
								cVector2f(mfInstructionsFontHeight, mfInstructionsFontHeight), cColor(1, 1), eFontAlign_Left);
		}
	}
}

bool cSomaGammaScreen::GammaSliderMoved_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData)
{
	return ((cSomaGammaScreen *)apObject)->GammaSliderMoved(apWidget, aData);
}

bool cSomaGammaScreen::GammaSliderMoved(iWidget *apWidget, const cGuiMessageData &aData)
{
	float fGamma = ohpl_gs7_gamma_from_slider(mpSlider->GetValue(), mpSlider->GetMaxValue());

	mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(fGamma);

	return true;
}

bool cSomaGammaScreen::ContinuePressed_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData)
{
	return ((cSomaGammaScreen *)apObject)->ContinuePressed(apWidget, aData);
}

bool cSomaGammaScreen::ContinuePressed(iWidget *apWidget, const cGuiMessageData &aData)
{
	Finish();
	return true;
}
