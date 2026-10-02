#include "SomaGammaScreen.h"
#include "SomaBase.h"

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
	{
		mvCheckerboardSize = mpCheckerboardGfx->GetImageSize();
		mvCheckerboardPos = cVector2f((mvScreenSize.x - mvCheckerboardSize.x) * 0.5f,
									   (mvScreenSize.y - mvCheckerboardSize.y) * 0.5f - 40);
	}

	mpInstructionsFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_large_bold.fnt");
	mfInstructionsFontHeight = 24.0f;
	mfInstructionsRowHeight = mfInstructionsFontHeight + 6.0f;

	if (mpInstructionsFont)
	{
		float fWrapWidth = cMath::Min(700.0f, mvScreenSize.x - 160.0f);
		if (fWrapWidth < 200.0f)
			fWrapWidth = mvScreenSize.x; // degenerate tiny headless resolution - don't wrap into nothing

		mpInstructionsFont->GetWordWrapRows(fWrapWidth, mfInstructionsRowHeight,
											 cVector2f(mfInstructionsFontHeight, mfInstructionsFontHeight),
											 _W("Adjust gamma so you can barely make out the details on the robot poster on the left."),
											 &mvInstructionRows);

		float fBlockHeight = mfInstructionsRowHeight * (float)mvInstructionRows.size();

		mvInstructionsPos = cVector2f((mvScreenSize.x - fWrapWidth) * 0.5f,
									   cMath::Max(20.0f, mvCheckerboardPos.y - 20.0f - fBlockHeight));
	}

	mfGammaMinValue = 0.3f;
	mfGammaMaxValue = 2.0f;

	cVector2f vSliderSize(300, 25);
	cVector3f vSliderPos((mvScreenSize.x - vSliderSize.x) * 0.5f,
						  mvCheckerboardPos.y + mvCheckerboardSize.y + 30, 0.1f);

	mpSlider = mpGuiSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal, vSliderPos, vSliderSize, 100, NULL);
	mpSlider->AddCallback(eGuiMessage_SliderMove, this, &cSomaGammaScreen::GammaSliderMoved_static_gui);

	{
		float fCurrentGamma = mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
		fCurrentGamma = cMath::Clamp(fCurrentGamma, mfGammaMinValue, mfGammaMaxValue);
		int lValue = cMath::RoundToInt((fCurrentGamma - mfGammaMinValue) * 100.0f / (mfGammaMaxValue - mfGammaMinValue));
		mpSlider->SetValue(lValue, false);
	}

	// Real button: dragging the slider must not count as continue
	cVector2f vButtonSize(120, 30);
	cVector3f vButtonPos((mvScreenSize.x - vButtonSize.x) * 0.5f,
						  vSliderPos.y + vSliderSize.y + 30, 0.1f);
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
		mpBase->GetConfig()->mfGamma = mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
		mpBase->GetConfig()->Save();

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
		if (bDown && mbMouseWasDown == false)
			mpGui->SendMouseClickDown(eGuiMouseButton_Left);
		if (bDown == false && mbMouseWasDown)
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
	float fSliderRelValue = ((float)mpSlider->GetValue()) / (float)mpSlider->GetMaxValue();
	float fGamma = mfGammaMinValue + (mfGammaMaxValue - mfGammaMinValue) * fSliderRelValue;

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
