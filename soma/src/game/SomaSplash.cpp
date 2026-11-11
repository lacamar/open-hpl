#include "SomaSplash.h"
#include "SomaImGui.h"
#include "SomaBase.h"
#include "RustSomaUi.h"
#include "system/HeadlessControl.h"
#include <SDL2/SDL.h>

cSomaImGui *SomaHudImGui();
void SomaRenderImGuis();

cSomaSplash::cSomaSplash(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaSplash")
{
	mpEngine = apEngine;
	mpBase = apBase;

	mfElapsed = 0;
	mbFinished = false;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	mfVirtualWidth = ohpl_gs7_splash_virtual_width(mvScreenSize.v);

	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("Splash", mpGuiSkin);

	mpBlackBg = mpGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);
	mpWhite = mpGui->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);
	mpPremenuBg = mpGui->CreateGfxTexture("Premenu.png", eGuiMaterial_Alpha, eTextureType_2D);
	mpLoadingBar = mpGui->CreateGfxTexture("loading_bar.dds", eGuiMaterial_Alpha, eTextureType_2D);
	mpLoadingFrame = mpGui->CreateGfxTexture("loading_frame.dds", eGuiMaterial_Alpha, eTextureType_2D);
	for (int i = 0; i < mlBrainFrameCount; ++i)
		mvBrainFrames[i] = mpGui->CreateGfxTexture("brain_" + cString::ToString(i + 1, 2) + ".dds", eGuiMaterial_Additive, eTextureType_2D);

	mpBarClipRegion = hplNew(cGuiClipRegion, ());

	// game.cfg SplashScreenMusic / SplashScreenMusicVol
	mpEngine->GetSound()->GetMusicHandler()->Play("loadscreen_background.ogg", 0.15f, 0.3f, true, false);

	mlBootStart = cPlatform::GetApplicationTime();
	static cSomaSplash *pBootSplash;
	pBootSplash = this;
	iResourceManager::mpLoadTickCallback = [] { pBootSplash->DrawLoadingScreen(true); };
	mpBase->LoadScriptMainMenu();
	iResourceManager::mpLoadTickCallback = NULL;
	// Draw after the map and HUD viewports
	SomaHudImGui();
	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
	mpViewport->AddGuiSet(mpGuiSet);
}

cSomaSplash::~cSomaSplash()
{
	hplDelete(mpBarClipRegion);
}

void cSomaSplash::Finish()
{
	if (mbFinished)
		return;
	mbFinished = true;

	mpGui->DestroyGfx(mpPremenuBg);
	mpGui->DestroyGfx(mpLoadingBar);
	mpGui->DestroyGfx(mpLoadingFrame);
	mpPremenuBg = mpLoadingBar = mpLoadingFrame = NULL;

	mpViewport->SetActive(false);

	mpBase->OnSplashFinished();
}

void cSomaSplash::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	if (ohpl_gs7_splash_update(&mfElapsed, afTimeStep))
		Finish();
}

bool cSomaSplash::ScriptsMayRun() { return ohpl_gs7_splash_scripts_may_run(mbFinished, mfElapsed); }

cVector3f cSomaSplash::VirtualToScreen(const cVector2f &avPos, float afZ)
{
	cVector3f vOut;
	ohpl_gs7_splash_virtual_to_screen(avPos.v, afZ, mfVirtualWidth, mvScreenSize.v, vOut.v);
	return vOut;
}

cVector2f cSomaSplash::VirtualSizeToScreen(const cVector2f &avSize)
{
	cVector2f vOut;
	ohpl_gs7_splash_virtual_size_to_screen(avSize.v, mfVirtualWidth, mvScreenSize.v, vOut.v);
	return vOut;
}

void cSomaSplash::DrawBrainIcon(float afAlpha, float afTime)
{
	if (afAlpha <= 0)
		return;
	int lFrame = ohpl_gs7_splash_brain_frame(afTime);
	cGuiGfxElement *pFrame = lFrame >= 0 ? mvBrainFrames[lFrame] : NULL;
	if (pFrame == NULL)
		return;

	cVector3f vPos;
	cVector2f vSize;
	ohpl_gs7_splash_brain_rect(mfVirtualWidth, mvScreenSize.v, vPos.v, vSize.v);
	mpGuiSet->DrawGfx(pFrame, vPos, vSize, cColor(afAlpha, afAlpha, afAlpha, 1));
}

void cSomaSplash::DrawLoadingScreen(bool abBoot)
{
	static unsigned long lLastDraw = 0;
	unsigned long lNow = cPlatform::GetApplicationTime();
	if (ohpl_gs7_splash_load_draw_due(lNow, lLastDraw) == false)
		return;
	lLastDraw = lNow;
	// Lets the compositor's fullscreen configure through (niri keeps the window tiled until acked)
	SDL_PumpEvents();
	iLowLevelGraphics *pLowGfx = mpEngine->GetGraphics()->GetLowLevel();
	pLowGfx->SetCurrentFrameBuffer(NULL);
	pLowGfx->SetClearColor(cColor(0, 1));
	pLowGfx->ClearFrameBuffer(eClearFrameBufferFlag_Color);
	if (abBoot)
	{
		// Hold before the fade-out until the menu has loaded
		mfElapsed = ohpl_gs7_splash_boot_elapsed(lNow, mlBootStart);
		OnDraw(0);
	}
	else
	{
		// Script loading screen (MenuHandler cLastOnSomaScreen): keep its last frame, bar where it asked
		if (mvLoadBarSize.x > 0)
		{
			SomaRenderImGuis();
			// ponytail: time-based progress, no resource count known up front
			float fFrac = ohpl_gs7_splash_load_fraction(lNow, mlLoadStart);
			mpGuiSet->DrawGfx(mpWhite, VirtualToScreen(mvLoadBarPos, 1), VirtualSizeToScreen(cVector2f(mvLoadBarSize.x * fFrac, mvLoadBarSize.y)));
		}
		DrawBrainIcon(1, lNow / 1000.0f);
	}
	mpGuiSet->Render(NULL);
	mpGuiSet->ClearRenderObjects();
	pLowGfx->FlushRendering();
	if (mpEngine->GetHeadlessControl())
		mpEngine->GetHeadlessControl()->Update(true);
	pLowGfx->SwapBuffers();
}

void cSomaSplash::OnDraw(float afFrameTime)
{
	if (mbFinished)
		return;

	mpGuiSet->DrawGfx(mpBlackBg, cVector3f(0, 0, 0), mvScreenSize);

	float t = mfElapsed;
	OhplGs7SplashPlan plan;
	cVector2f vImgSize = mpPremenuBg ? mpPremenuBg->GetImageSize() : cVector2f(0);
	ohpl_gs7_splash_plan(mvScreenSize.v, mfVirtualWidth, t, mpPremenuBg ? vImgSize.v : NULL, &plan);
	float fAlpha = plan.alpha;

	if (mpPremenuBg)
		mpGuiSet->DrawGfx(mpPremenuBg, cVector3f(plan.premenu_pos[0], plan.premenu_pos[1], plan.premenu_pos[2]), cVector2f(plan.premenu_size[0], plan.premenu_size[1]),
						  cColor(1, 1, 1, fAlpha));

	if (mpLoadingFrame && mpLoadingBar)
	{
		cVector3f vBarPos(plan.bar_pos[0], plan.bar_pos[1], plan.bar_pos[2]);
		cVector2f vBarSize(plan.bar_size[0], plan.bar_size[1]);
		mpGuiSet->DrawGfx(mpLoadingFrame, vBarPos, vBarSize, cColor(1, 1, 1, fAlpha));

		mpBarClipRegion->mRect = cRect2f(plan.bar_clip[0], plan.bar_clip[1], plan.bar_clip[2], plan.bar_clip[3]);
		cGuiClipRegion *pPrevRegion = mpGuiSet->GetCurrentClipRegion();
		mpGuiSet->SetCurrentClipRegion(mpBarClipRegion);
		mpGuiSet->DrawGfx(mpLoadingBar, vBarPos, vBarSize, cColor(1, 1, 1, fAlpha));
		mpGuiSet->SetCurrentClipRegion(pPrevRegion);
	}

	DrawBrainIcon(plan.brain_alpha, t);
}
