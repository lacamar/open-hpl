#include "SomaSplash.h"
#include "SomaImGui.h"
#include "SomaBase.h"
#include "RustSomaUi.h"
#include "SomaS8Trace.h"
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
	s8::Tr("screen", {}, {mvScreenSize.x, mvScreenSize.y});
	mpGuiSet = mpGui->CreateSet("Splash", mpGuiSkin);
	s8::Tr("set_new", {"Splash"}, {mpGuiSet});

	mpBlackBg = mpGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);
	s8::Tr("gfx_rect", {0.0f, 0.0f, 0.0f, 1.0f, (int)eGuiMaterial_Alpha}, {mpBlackBg});
	mpWhite = mpGui->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);
	s8::Tr("gfx_rect", {1.0f, 1.0f, 1.0f, 1.0f, (int)eGuiMaterial_Alpha}, {mpWhite});
	mpPremenuBg = mpGui->CreateGfxTexture("Premenu.png", eGuiMaterial_Alpha, eTextureType_2D);
	s8::Tr("gfx_tex", {"Premenu.png", (int)eGuiMaterial_Alpha}, {mpPremenuBg});
	mpLoadingBar = mpGui->CreateGfxTexture("loading_bar.dds", eGuiMaterial_Alpha, eTextureType_2D);
	s8::Tr("gfx_tex", {"loading_bar.dds", (int)eGuiMaterial_Alpha}, {mpLoadingBar});
	mpLoadingFrame = mpGui->CreateGfxTexture("loading_frame.dds", eGuiMaterial_Alpha, eTextureType_2D);
	s8::Tr("gfx_tex", {"loading_frame.dds", (int)eGuiMaterial_Alpha}, {mpLoadingFrame});
	for (int i = 0; i < mlBrainFrameCount; ++i)
	{
		mvBrainFrames[i] = mpGui->CreateGfxTexture("brain_" + cString::ToString(i + 1, 2) + ".dds", eGuiMaterial_Additive, eTextureType_2D);
		s8::Tr("gfx_tex", {"brain_" + cString::ToString(i + 1, 2) + ".dds", (int)eGuiMaterial_Additive}, {mvBrainFrames[i]});
	}

	mpBarClipRegion = hplNew(cGuiClipRegion, ());
	s8::Tr("clip_new", {}, {mpBarClipRegion});

	// game.cfg SplashScreenMusic / SplashScreenMusicVol
	s8::Tr("music", {"loadscreen_background.ogg", 0.15f, 0.3f, true, false});
	mpEngine->GetSound()->GetMusicHandler()->Play("loadscreen_background.ogg", 0.15f, 0.3f, true, false);

	mlBootStart = s8::Now(cPlatform::GetApplicationTime());
	s8::Tr("now", {}, {mlBootStart});
	static cSomaSplash *pBootSplash;
	pBootSplash = this;
	iResourceManager::mpLoadTickCallback = [] { s8::Tr("load_tick", {}); pBootSplash->DrawLoadingScreen(true); };
	s8::Tr("load_begin", {});
	mpBase->LoadScriptMainMenu();
	s8::Tr("load_end", {});
	iResourceManager::mpLoadTickCallback = NULL;
	// Draw after the map and HUD viewports
	s8::Tr("hud_imgui", {});
	SomaHudImGui();
	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
	s8::Tr("viewport_new", {false}, {mpViewport});
	mpViewport->AddGuiSet(mpGuiSet);
	s8::Tr("viewport_add_set", {mpViewport, mpGuiSet});
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

	s8::Tr("gfx_destroy", {mpPremenuBg});
	mpGui->DestroyGfx(mpPremenuBg);
	s8::Tr("gfx_destroy", {mpLoadingBar});
	mpGui->DestroyGfx(mpLoadingBar);
	s8::Tr("gfx_destroy", {mpLoadingFrame});
	mpGui->DestroyGfx(mpLoadingFrame);
	mpPremenuBg = mpLoadingBar = mpLoadingFrame = NULL;

	s8::Tr("viewport_active", {mpViewport, false});
	mpViewport->SetActive(false);

	s8::Tr("base_splash_done", {});
	mpBase->OnSplashFinished();
}

void cSomaSplash::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	if (s8::On())
		afTimeStep = 1.0f / 60;
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
	s8::Tr("draw_gfx", {mpGuiSet, pFrame, vPos.x, vPos.y, vPos.z, vSize.x, vSize.y, afAlpha, afAlpha, afAlpha, 1.0f});
	mpGuiSet->DrawGfx(pFrame, vPos, vSize, cColor(afAlpha, afAlpha, afAlpha, 1));
}

void cSomaSplash::DrawLoadingScreen(bool abBoot)
{
	static unsigned long lLastDraw = 0;
	unsigned long lNow = s8::Now(cPlatform::GetApplicationTime());
	s8::Tr("now", {}, {lNow});
	if (ohpl_gs7_splash_load_draw_due(lNow, lLastDraw) == false)
		return;
	lLastDraw = lNow;
	// Lets the compositor's fullscreen configure through (niri keeps the window tiled until acked)
	s8::Tr("frame_begin", {});
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
			s8::Tr("imgui_render", {});
			SomaRenderImGuis();
			// ponytail: time-based progress, no resource count known up front
			float fFrac = ohpl_gs7_splash_load_fraction(lNow, mlLoadStart);
			{
				cVector3f vP = VirtualToScreen(mvLoadBarPos, 1);
				cVector2f vS = VirtualSizeToScreen(cVector2f(mvLoadBarSize.x * fFrac, mvLoadBarSize.y));
				s8::Tr("draw_gfx", {mpGuiSet, mpWhite, vP.x, vP.y, vP.z, vS.x, vS.y, 1.0f, 1.0f, 1.0f, 1.0f});
				mpGuiSet->DrawGfx(mpWhite, vP, vS);
			}
		}
		DrawBrainIcon(1, lNow / 1000.0f);
	}
	s8::Tr("frame_end", {mpGuiSet});
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

	s8::Tr("draw_gfx", {mpGuiSet, mpBlackBg, 0.0f, 0.0f, 0.0f, mvScreenSize.x, mvScreenSize.y, 1.0f, 1.0f, 1.0f, 1.0f});
	mpGuiSet->DrawGfx(mpBlackBg, cVector3f(0, 0, 0), mvScreenSize);

	float t = mfElapsed;
	OhplGs7SplashPlan plan;
	cVector2f vImgSize = mpPremenuBg ? mpPremenuBg->GetImageSize() : cVector2f(0);
	if (mpPremenuBg)
		s8::Tr("gfx_size", {mpPremenuBg}, {vImgSize.x, vImgSize.y});
	ohpl_gs7_splash_plan(mvScreenSize.v, mfVirtualWidth, t, mpPremenuBg ? vImgSize.v : NULL, &plan);
	float fAlpha = plan.alpha;

	if (mpPremenuBg)
	{
		s8::Tr("draw_gfx", {mpGuiSet, mpPremenuBg, plan.premenu_pos[0], plan.premenu_pos[1], plan.premenu_pos[2], plan.premenu_size[0], plan.premenu_size[1], 1.0f, 1.0f, 1.0f, fAlpha});
		mpGuiSet->DrawGfx(mpPremenuBg, cVector3f(plan.premenu_pos[0], plan.premenu_pos[1], plan.premenu_pos[2]), cVector2f(plan.premenu_size[0], plan.premenu_size[1]),
						  cColor(1, 1, 1, fAlpha));
	}

	if (mpLoadingFrame && mpLoadingBar)
	{
		cVector3f vBarPos(plan.bar_pos[0], plan.bar_pos[1], plan.bar_pos[2]);
		cVector2f vBarSize(plan.bar_size[0], plan.bar_size[1]);
		s8::Tr("draw_gfx", {mpGuiSet, mpLoadingFrame, vBarPos.x, vBarPos.y, vBarPos.z, vBarSize.x, vBarSize.y, 1.0f, 1.0f, 1.0f, fAlpha});
		mpGuiSet->DrawGfx(mpLoadingFrame, vBarPos, vBarSize, cColor(1, 1, 1, fAlpha));

		s8::Tr("clip_rect", {mpBarClipRegion, plan.bar_clip[0], plan.bar_clip[1], plan.bar_clip[2], plan.bar_clip[3]});
		mpBarClipRegion->mRect = cRect2f(plan.bar_clip[0], plan.bar_clip[1], plan.bar_clip[2], plan.bar_clip[3]);
		cGuiClipRegion *pPrevRegion = mpGuiSet->GetCurrentClipRegion();
		s8::Tr("clip_get", {mpGuiSet}, {pPrevRegion});
		s8::Tr("clip_set", {mpGuiSet, mpBarClipRegion});
		mpGuiSet->SetCurrentClipRegion(mpBarClipRegion);
		s8::Tr("draw_gfx", {mpGuiSet, mpLoadingBar, vBarPos.x, vBarPos.y, vBarPos.z, vBarSize.x, vBarSize.y, 1.0f, 1.0f, 1.0f, fAlpha});
		mpGuiSet->DrawGfx(mpLoadingBar, vBarPos, vBarSize, cColor(1, 1, 1, fAlpha));
		s8::Tr("clip_set", {mpGuiSet, pPrevRegion});
		mpGuiSet->SetCurrentClipRegion(pPrevRegion);
	}

	DrawBrainIcon(plan.brain_alpha, t);
}
