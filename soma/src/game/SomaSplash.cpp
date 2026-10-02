#include "SomaSplash.h"
#include "SomaImGui.h"
#include "SomaBase.h"

cSomaImGui *SomaHudImGui();

//---------------------------------------

// Timings and layout measured from the official game (scripts/soma-compare.py boot): linear fade-in,
// hold, long fade-out to black, then a cut; the brain icon fades in once and stays lit.
static const float kBootFadeIn = 2.2f, kBootFadeOutStart = 3.6f, kBootFadeOut = 4.0f;
static const float kBrainStart = 2.05f, kBrainFadeIn = 1.0f;
static const float kScriptStart = 3.7f, kBootBlackHold = 0.1f;
static const float kBootDuration = kBootFadeOutStart + kBootFadeOut + kBootBlackHold;
// cLuxLoadHandler::OnDraw: 15 frames/s, ping-pong
static const float kBrainFrameRate = 15.0f;

//---------------------------------------

cSomaSplash::cSomaSplash(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaSplash")
{
	mpEngine = apEngine;
	mpBase = apBase;

	mfElapsed = 0;
	mbFinished = false;
	mbMouseWasDown = false;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	// Official HUD virtual space: 1024x768 centre, widened by 4/3 of the extra width
	mfVirtualWidth = (4.0f * 768.0f * mvScreenSize.x / mvScreenSize.y - 1024.0f) / 3.0f;

	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("Splash", mpGuiSkin);

	mpBlackBg = mpGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);
	mpPremenuBg = mpGui->CreateGfxTexture("Premenu.png", eGuiMaterial_Alpha, eTextureType_2D);
	mpLoadingBar = mpGui->CreateGfxTexture("loading_bar.dds", eGuiMaterial_Alpha, eTextureType_2D);
	mpLoadingFrame = mpGui->CreateGfxTexture("loading_frame.dds", eGuiMaterial_Alpha, eTextureType_2D);
	for (int i = 0; i < mlBrainFrameCount; ++i)
		mvBrainFrames[i] = mpGui->CreateGfxTexture("brain_" + cString::ToString(i + 1, 2) + ".dds", eGuiMaterial_Additive, eTextureType_2D);

	mpBarClipRegion = hplNew(cGuiClipRegion, ());

	// game.cfg SplashScreenMusic / SplashScreenMusicVol
	mpEngine->GetSound()->GetMusicHandler()->Play("loadscreen_background.ogg", 0.15f, 0.3f, true, false);

	mpBase->LoadScriptMainMenu();
	// Draw after the map and HUD viewports
	SomaHudImGui();
	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
	mpViewport->AddGuiSet(mpGuiSet);
}

//-----------------------------------------------------------------------

cSomaSplash::~cSomaSplash()
{
	hplDelete(mpBarClipRegion);
}

//-----------------------------------------------------------------------

void cSomaSplash::Finish()
{
	if (mbFinished)
		return;
	mbFinished = true;

	mpGui->DestroyGfx(mpPremenuBg);
	mpGui->DestroyGfx(mpLoadingBar);
	mpGui->DestroyGfx(mpLoadingFrame);
	for (int i = 0; i < mlBrainFrameCount; ++i)
		mpGui->DestroyGfx(mvBrainFrames[i]);
	mpPremenuBg = mpLoadingBar = mpLoadingFrame = NULL;

	mpViewport->SetActive(false);

	mpBase->OnSplashFinished();
}

//-----------------------------------------------------------------------

bool cSomaSplash::AnySkipInputThisFrame()
{
	cInput *pInput = mpEngine->GetInput();

	iKeyboard *pKeyboard = pInput->GetKeyboard();
	if (pKeyboard->KeyIsPressed())
	{
		// KeyIsPressed() doesn't drain the queue; a stray event would latch
		pKeyboard->GetKey();
		return true;
	}

	iMouse *pMouse = pInput->GetMouse();
	bool bDown = pMouse->ButtonIsDown(eMouseButton_Left) || pMouse->ButtonIsDown(eMouseButton_Right) ||
				 pMouse->ButtonIsDown(eMouseButton_Middle);
	bool bClick = bDown && mbMouseWasDown == false;
	mbMouseWasDown = bDown;
	return bClick;
}

//-----------------------------------------------------------------------

void cSomaSplash::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	mfElapsed += afTimeStep;
	if (AnySkipInputThisFrame() || mfElapsed >= kBootDuration)
		Finish();
}

//-----------------------------------------------------------------------

bool cSomaSplash::ScriptsMayRun() { return mbFinished || mfElapsed >= kScriptStart; }

// Official bar progress on the reference machine; our main menu load finishes before the first frame
static float BootBarFraction(float afT)
{
	static const float vCurve[][2] = {{0, 0.60f}, {0.2f, 0.64f}, {0.46f, 0.68f}, {0.72f, 0.69f}, {0.98f, 0.71f}, {1.25f, 0.73f},
									  {1.51f, 0.77f}, {1.77f, 0.80f}, {2.03f, 0.82f}, {2.3f, 0.86f}, {2.56f, 0.95f}, {2.82f, 1.0f}};
	const int lNum = sizeof(vCurve) / sizeof(vCurve[0]);
	for (int i = 1; i < lNum; ++i)
		if (afT < vCurve[i][0])
			return vCurve[i - 1][1] + (vCurve[i][1] - vCurve[i - 1][1]) * (afT - vCurve[i - 1][0]) / (vCurve[i][0] - vCurve[i - 1][0]);
	return 1.0f;
}

cVector3f cSomaSplash::VirtualToScreen(const cVector2f &avPos, float afZ)
{
	return cVector3f((avPos.x + (mfVirtualWidth - 1024.0f) * 0.5f) * mvScreenSize.x / mfVirtualWidth, avPos.y * mvScreenSize.y / 768.0f, afZ);
}

cVector2f cSomaSplash::VirtualSizeToScreen(const cVector2f &avSize)
{
	return cVector2f(avSize.x * mvScreenSize.x / mfVirtualWidth, avSize.y * mvScreenSize.y / 768.0f);
}

void cSomaSplash::DrawBrainIcon(float afAlpha)
{
	if (afAlpha <= 0)
		return;
	int lPeriod = 2 * (mlBrainFrameCount - 1);
	int lStep = ((int)(mfElapsed * kBrainFrameRate)) % lPeriod;
	int lFrame = (lStep <= (mlBrainFrameCount - 1)) ? lStep : (lPeriod - lStep);

	cGuiGfxElement *pFrame = mvBrainFrames[lFrame];
	if (pFrame == NULL)
		return;

	cVector3f vPos = VirtualToScreen(cVector2f(1024.0f + (mfVirtualWidth - 1024.0f) * 0.5f - 150.0f, 648.0f), 3);
	mpGuiSet->DrawGfx(pFrame, vPos, VirtualSizeToScreen(cVector2f(70, 70)), cColor(afAlpha, afAlpha, afAlpha, 1));
}

void cSomaSplash::OnDraw(float afFrameTime)
{
	if (mbFinished)
		return;

	mpGuiSet->DrawGfx(mpBlackBg, cVector3f(0, 0, 0), mvScreenSize);

	float t = mfElapsed;
	float fAlpha = t < kBootFadeOutStart ? cMath::Clamp(t / kBootFadeIn, 0.0f, 1.0f)
										 : cMath::Clamp(1.0f - (t - kBootFadeOutStart) / kBootFadeOut, 0.0f, 1.0f);

	if (mpPremenuBg)
	{
		cVector2f vImgSize = mpPremenuBg->GetImageSize();
		vImgSize = vImgSize * cMath::Min(mvScreenSize.x / vImgSize.x, mvScreenSize.y / vImgSize.y);
		cVector3f vPos((mvScreenSize.x - vImgSize.x) * 0.5f, (mvScreenSize.y - vImgSize.y) * 0.5f, 1);
		mpGuiSet->DrawGfx(mpPremenuBg, vPos, vImgSize, cColor(1, 1, 1, fAlpha));
	}

	if (mpLoadingFrame && mpLoadingBar)
	{
		cVector3f vBarPos = VirtualToScreen(cVector2f(0, 512), 2);
		cVector2f vBarSize = VirtualSizeToScreen(cVector2f(1024, 128));
		mpGuiSet->DrawGfx(mpLoadingFrame, vBarPos, vBarSize, cColor(1, 1, 1, fAlpha));

		mpBarClipRegion->mRect = cRect2f(vBarPos.x, vBarPos.y, vBarSize.x * BootBarFraction(t), vBarSize.y);
		cGuiClipRegion *pPrevRegion = mpGuiSet->GetCurrentClipRegion();
		mpGuiSet->SetCurrentClipRegion(mpBarClipRegion);
		mpGuiSet->DrawGfx(mpLoadingBar, vBarPos, vBarSize, cColor(1, 1, 1, fAlpha));
		mpGuiSet->SetCurrentClipRegion(pPrevRegion);
	}

	DrawBrainIcon(cMath::Clamp((t - kBrainStart) / kBrainFadeIn, 0.0f, 1.0f));
}
