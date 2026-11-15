#include "SomaGammaScreen.h"
#include "SomaBase.h"
#include "SomaLuxGame.h"
#include "RustSomaUi.h"
#include "SomaS8Trace.h"

#include <fstream>

cSomaGammaScreen::cSomaGammaScreen(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaGammaScreen")
{
	mpEngine = apEngine;
	mpBase = apBase;

	mbFinished = false;
	mbMouseWasDown = false;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	s8::Tr("screen", {}, {mvScreenSize.x, mvScreenSize.y});
	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("GammaScreen", mpGuiSkin);
	s8::Tr("set_new", {"GammaScreen"}, {mpGuiSet});
	s8::Tr("set_draw_mouse", {mpGuiSet, true});
	mpGuiSet->SetDrawMouse(true);

	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, true);
	s8::Tr("viewport_new", {true}, {mpViewport});
	mpViewport->AddGuiSet(mpGuiSet);
	s8::Tr("viewport_add_set", {mpViewport, mpGuiSet});

	s8::Tr("set_active", {mpGuiSet, true});
	mpGuiSet->SetActive(true);
	s8::Tr("gui_focus", {mpGuiSet});
	mpGui->SetFocus(mpGuiSet);

	mpBackgroundGfx = mpGui->CreateGfxTexture("gamma_background.tga", eGuiMaterial_Diffuse, eTextureType_2D);
	s8::Tr("gfx_tex", {"gamma_background.tga", (int)eGuiMaterial_Diffuse}, {mpBackgroundGfx});
	mpCheckerboardGfx = mpGui->CreateGfxTexture("gamma.tga", eGuiMaterial_Alpha, eTextureType_2D);
	s8::Tr("gfx_tex", {"gamma.tga", (int)eGuiMaterial_Alpha}, {mpCheckerboardGfx});

	if (mpCheckerboardGfx)
	{
		mvCheckerboardSize = mpCheckerboardGfx->GetImageSize();
		s8::Tr("gfx_size", {mpCheckerboardGfx}, {mvCheckerboardSize.x, mvCheckerboardSize.y});
	}

	mpInstructionsFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_large_bold.fnt");
	s8::Tr("font_new", {"sansation_large_bold.fnt"}, {mpInstructionsFont});
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
		{
			std::vector<s8::A> vRows;
			for (const tWString &r : mvInstructionRows)
				vRows.emplace_back(r);
			s8::Tr("font_wrap", {mpInstructionsFont, fWrapWidth, mfInstructionsRowHeight, mfInstructionsFontHeight,
								 _W("Adjust gamma so you can barely make out the details on the robot poster on the left.")}, vRows);
		}
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
	s8::Tr("slider_new", {mpGuiSet, vSliderPos.x, vSliderPos.y, vSliderPos.z, vSliderSize.x, vSliderSize.y, 100, 0}, {mpSlider});
	mpSlider->AddCallback(eGuiMessage_SliderMove, this, &cSomaGammaScreen::GammaSliderMoved_static_gui);

	{
		float fCurrentGamma = mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
		s8::Tr("gamma_get", {}, {fCurrentGamma});
		s8::Tr("slider_set", {mpSlider, ohpl_gs7_gamma_to_slider(fCurrentGamma)});
		mpSlider->SetValue(ohpl_gs7_gamma_to_slider(fCurrentGamma), false);
	}

	// Real button: dragging the slider must not count as continue
	cVector2f vButtonSize(layout.button_size[0], layout.button_size[1]);
	cVector3f vButtonPos(layout.button_pos[0], layout.button_pos[1], layout.button_pos[2]);
	mpContinueButton = mpGuiSet->CreateWidgetButton(vButtonPos, vButtonSize, _W("Continue"), NULL);
	s8::Tr("button_new", {mpGuiSet, vButtonPos.x, vButtonPos.y, vButtonPos.z, vButtonSize.x, vButtonSize.y, _W("Continue"), 1}, {mpContinueButton});
	mpContinueButton->AddCallback(eGuiMessage_ButtonPressed, this, &cSomaGammaScreen::ContinuePressed_static_gui);

	s8::Tr("default_focus_nav", {mpGuiSet, mpContinueButton});
	mpGuiSet->SetDefaultFocusNavWidget(mpContinueButton);
	s8::Tr("focused_widget", {mpGuiSet, mpContinueButton});
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

	s8::Tr("viewport_active", {mpViewport, false});
	mpViewport->SetActive(false);

	// cGui draws widgets regardless of viewport activity
	s8::Tr("set_active", {mpGuiSet, false});
	mpGuiSet->SetActive(false);
	{
		bool bFocused = mpGui->GetFocusedSet() == mpGuiSet;
		s8::Tr("gui_focused_is", {mpGuiSet}, {bFocused});
		if (bFocused)
		{
			s8::Tr("gui_focus", {(const void *)NULL});
			mpGui->SetFocus(NULL);
		}
	}

	if (mpBase)
	{
		float fGamma = mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection();
		s8::Tr("gamma_get", {}, {fGamma});
		s8::Tr("save_gamma", {fGamma});
		mpBase->GetConfig()->mfGamma = fGamma;
		mpBase->GetConfig()->Save();
		if (cConfigFile *pUser = SomaUserConfig())
		{
			pUser->SetFloat("Graphics", "Brightness", fGamma);
			pUser->Save();
		}

		s8::Tr("base_gamma_done", {});
		mpBase->OnGammaScreenFinished();
	}
}

bool cSomaGammaScreen::AnyContinueInputThisFrame()
{
	int lCode = -1;
	if (s8::AutoKey())
		lCode = eKey_Return;
	else if (cInput *pInput = mpEngine->GetInput())
	{
		iKeyboard *pKeyboard = pInput->GetKeyboard();
		if (pKeyboard && pKeyboard->KeyIsPressed())
			lCode = pKeyboard->GetKey().mKey;
	}
	s8::Tr("key_poll", {}, {lCode});
	return lCode == eKey_Return || lCode == eKey_Escape || lCode == eKey_Space;
}

void cSomaGammaScreen::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	// cGui does not poll iMouse itself
	iMouse *pMouse = mpEngine->GetInput()->GetMouse();
	if (pMouse)
	{
		{
			bool bD = pMouse->ButtonIsDown(eMouseButton_Left);
			s8::Tr("mouse_poll", {}, {1, (int)pMouse->GetAbsPosition().x, (int)pMouse->GetAbsPosition().y,
									  (int)pMouse->GetRelPosition().x, (int)pMouse->GetRelPosition().y, bD});
			s8::Tr("gui_mouse_pos", {(int)pMouse->GetAbsPosition().x, (int)pMouse->GetAbsPosition().y,
									 (int)pMouse->GetRelPosition().x, (int)pMouse->GetRelPosition().y});
		}
		mpGui->SendMousePos(pMouse->GetAbsPosition(), pMouse->GetRelPosition());

		bool bDown = pMouse->ButtonIsDown(eMouseButton_Left);
		int lEdge = ohpl_gs7_mouse_edge(bDown, mbMouseWasDown);
		if (lEdge == 1)
		{
			s8::Tr("click", {true});
			mpGui->SendMouseClickDown(eGuiMouseButton_Left);
		}
		if (lEdge == 2)
		{
			s8::Tr("click", {false});
			mpGui->SendMouseClickUp(eGuiMouseButton_Left);
		}
		mbMouseWasDown = bDown;
	}
	else
		s8::Tr("mouse_poll", {}, {0, 0, 0, 0, 0, false});

	if (AnyContinueInputThisFrame())
		Finish();
}

void cSomaGammaScreen::OnDraw(float afFrameTime)
{
	if (mbFinished)
		return;

	if (mpBackgroundGfx)
	{
		s8::Tr("draw_gfx", {mpGuiSet, mpBackgroundGfx, 0.0f, 0.0f, 0.0f, mvScreenSize.x, mvScreenSize.y, 1.0f, 1.0f, 1.0f, 1.0f});
		mpGuiSet->DrawGfx(mpBackgroundGfx, cVector3f(0, 0, 0), mvScreenSize);
	}

	if (mpCheckerboardGfx)
	{
		s8::Tr("draw_gfx", {mpGuiSet, mpCheckerboardGfx, mvCheckerboardPos.x, mvCheckerboardPos.y, 0.1f, mvCheckerboardSize.x, mvCheckerboardSize.y, 1.0f, 1.0f, 1.0f, 1.0f});
		mpGuiSet->DrawGfx(mpCheckerboardGfx, cVector3f(mvCheckerboardPos.x, mvCheckerboardPos.y, 0.1f), mvCheckerboardSize);
	}

	// The logo is baked into gamma_background.tga
	if (mpInstructionsFont)
	{
		for (size_t i = 0; i < mvInstructionRows.size(); ++i)
		{
			cVector3f vRowPos(mvInstructionsPos.x, mvInstructionsPos.y + mfInstructionsRowHeight * (float)i, 0.2f);
			s8::Tr("draw_font", {mpGuiSet, mpInstructionsFont, vRowPos.x, vRowPos.y, vRowPos.z, mfInstructionsFontHeight, 1.0f, 1.0f, 1.0f, 1.0f, mvInstructionRows[i]});
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
	s8::Tr("cb_slider", {});
	s8::Tr("slider_get", {mpSlider}, {mpSlider->GetValue(), mpSlider->GetMaxValue()});
	float fGamma = ohpl_gs7_gamma_from_slider(mpSlider->GetValue(), mpSlider->GetMaxValue());

	s8::Tr("gamma_set", {fGamma});
	mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(fGamma);

	return true;
}

bool cSomaGammaScreen::ContinuePressed_static_gui(void *apObject, iWidget *apWidget, const cGuiMessageData &aData)
{
	return ((cSomaGammaScreen *)apObject)->ContinuePressed(apWidget, aData);
}

bool cSomaGammaScreen::ContinuePressed(iWidget *apWidget, const cGuiMessageData &aData)
{
	s8::Tr("cb_continue", {});
	Finish();
	return true;
}
