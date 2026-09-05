/*
 * See SomaSplash.h for scope notes and citations for every real value used
 * below.
 */

#include "SomaSplash.h"
#include "SomaBase.h"
#include "SomaMenuSfx.h"

//---------------------------------------

// Real script timing (script/modules/MenuHandler.hps::GuiPreMenu(),
// mlPreMenuState==0) - see SomaSplash.h point 5.
const float cSomaSplash::mfFGFadeInTime = 2.5f;	 // 1 / 0.4 fade-in rate
const float cSomaSplash::mfFGHoldTimerTotal = 3.0f; // ImGui_AddTimer("FGLogoOver", 3)
const float cSomaSplash::mfFGFadeOutTime = 2.0f;	 // 1 / 0.5 fade-out rate

// Boot/init phase (Premenu.png + loading bar) - native, closed-source
// timing; no real evidence recovered for exact numbers, so this reuses
// the previous version of this file's own established 0.4s fade
// convention and picks a plausible hold time. See SomaSplash.h point 6
// (ePhase_BootInit) for why the bar fill itself is honestly time-based
// rather than tied to fabricated "phases".
const float cSomaSplash::mfBootFadeTime = 0.4f;
const float cSomaSplash::mfBootHoldTime = 3.0f;

//---------------------------------------

cSomaSplash::cSomaSplash(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaSplash")
{
	mpEngine = apEngine;
	mpBase = apBase;

	mpFGLogo = NULL;
	mpPremenuBg = NULL;
	mpLoadingBar = NULL;
	mpLoadingFrame = NULL;
	mpBarClipRegion = NULL;

	mfPhaseElapsed = 0;
	mbFinished = false;
	mbMouseWasDown = false;
	mbSplashMusicStarted = false;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("Splash", mpGuiSkin);

	// GUI-only viewport - no camera, no world, just here to give the GUI
	// set somewhere to attach to and draw through before any real scene
	// exists.
	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, true);
	mpViewport->AddGuiSet(mpGuiSet);

	mpBlackBg = mpGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);

	// Real SOMA install ships these two - see SomaSplash.h for exactly
	// where each one is confirmed used.
	mpFGLogo = mpGui->CreateGfxTexture("frictional_games_logo.dds", eGuiMaterial_Alpha, eTextureType_2D);
	mpPremenuBg = mpGui->CreateGfxTexture("Premenu.png", eGuiMaterial_Alpha, eTextureType_2D);
	mpLoadingBar = mpGui->CreateGfxTexture("loading_bar.dds", eGuiMaterial_Alpha, eTextureType_2D);
	mpLoadingFrame = mpGui->CreateGfxTexture("loading_frame.dds", eGuiMaterial_Alpha, eTextureType_2D);

	// Owned outright (not a child of the set's base clip region) so its
	// lifetime is entirely this class's responsibility - see
	// SomaSplash.h's comment on mpBarClipRegion for why this is allocated
	// once here rather than via CreateChild() every frame.
	mpBarClipRegion = hplNew(cGuiClipRegion, ());

	// Idempotent (checks its own cache dir first) and safe to call again
	// even though soma/src/game/SomaMainMenu.cpp's cSomaMainMenu also calls
	// this - see SomaSplash.h point 6 for what this provides.
	cSomaMenuSfx::EnsureCached(mpEngine->GetResources());

	EnterPhase(eSomaSplashPhase_FGLogo);
}

//-----------------------------------------------------------------------

cSomaSplash::~cSomaSplash()
{
	if (mpBarClipRegion)
		hplDelete(mpBarClipRegion);
}

//-----------------------------------------------------------------------

void cSomaSplash::EnterPhase(eSomaSplashPhase aPhase)
{
	mPhase = aPhase;
	mfPhaseElapsed = 0;

	if (aPhase == eSomaSplashPhase_FGLogo)
	{
		// Real script/modules/MenuHandler.hps::GuiPreMenu(), mlPreMenuState==0
		// (see SomaSplash.h point 5): both of these real Sound_PlayGui()/
		// Sound_CreateAtEntity() calls fire the instant the FG logo phase
		// begins. Real sample names/extraction path documented in
		// SomaMenuSfx.cpp (cSomaMenuSfx::FGLogoSting()/MenuBgNoise() - added
		// this pass, reusing that file's existing FSB5 PCM16 reader rather
		// than duplicating it).
		tString sSting = cSomaMenuSfx::FGLogoSting();
		if (sSting.size() > 0)
			mpEngine->GetSound()->GetSoundHandler()->PlayGui(sSting, false, 1.0f);
		else
			Log("SOMA splash: FG_Menu_Sting sample not available (see SomaMenuSfx.cpp) - playing silently\n");

		tString sBgNoise = cSomaMenuSfx::MenuBgNoise();
		if (sBgNoise.size() > 0)
			mpEngine->GetSound()->GetSoundHandler()->PlayGui(sBgNoise, true, 1.0f);
		else
			Log("SOMA splash: main_menu_bg sample not available (see SomaMenuSfx.cpp) - playing silently\n");
	}

	if (aPhase == eSomaSplashPhase_BootInit && mbSplashMusicStarted == false)
	{
		// config/game.cfg: SplashScreenMusic="loadscreen_background",
		// SplashScreenMusicVol="0.15" - see SomaSplash.h point 6. Same
		// cMusicHandler::Play() call soma/src/game/SomaMainMenu.cpp
		// already uses for its own real "Menu_Music.ogg".
		mbSplashMusicStarted = true;
		mpEngine->GetSound()->GetMusicHandler()->Play("loadscreen_background.ogg", 0.15f, 0.3f, true, false);
	}
}

//-----------------------------------------------------------------------

void cSomaSplash::AdvanceToNextPhase()
{
	if (mPhase == eSomaSplashPhase_FGLogo)
	{
		EnterPhase(eSomaSplashPhase_BootInit);
		return;
	}

	Finish();
}

//-----------------------------------------------------------------------

void cSomaSplash::Finish()
{
	if (mbFinished)
		return;

	mbFinished = true;
	mPhase = eSomaSplashPhase_Done;

	if (mpFGLogo)
	{
		mpGui->DestroyGfx(mpFGLogo);
		mpFGLogo = NULL;
	}
	if (mpPremenuBg)
	{
		mpGui->DestroyGfx(mpPremenuBg);
		mpPremenuBg = NULL;
	}
	if (mpLoadingBar)
	{
		mpGui->DestroyGfx(mpLoadingBar);
		mpLoadingBar = NULL;
	}
	if (mpLoadingFrame)
	{
		mpGui->DestroyGfx(mpLoadingFrame);
		mpLoadingFrame = NULL;
	}

	// Stop this viewport rendering (clearing to black + drawing the now-
	// empty GUI set) once the real scene's own viewport takes over -
	// otherwise both would render every frame.
	mpViewport->SetActive(false);

	if (mpBase)
		mpBase->OnSplashFinished();
}

//-----------------------------------------------------------------------

bool cSomaSplash::AnySkipInputThisFrame()
{
	cInput *pInput = mpEngine->GetInput();
	if (pInput == NULL)
		return false;

	iKeyboard *pKeyboard = pInput->GetKeyboard();
	if (pKeyboard && pKeyboard->KeyIsPressed())
	{
		// KeyIsPressed() only reports whether the pressed-keys queue is
		// non-empty - it does NOT drain it (only GetKey() does, see
		// cKeyboardSDL::KeyIsPressed()/GetKey() in KeyboardSDL.cpp). Since
		// nothing else in this class calls GetKey(), a single stray key
		// event (e.g. from window creation/focus) would otherwise latch
		// this true forever and skip the whole sequence in one frame.
		// Drain exactly one event per call so this only fires on real,
		// distinct key presses.
		pKeyboard->GetKey();
		return true;
	}

	iMouse *pMouse = pInput->GetMouse();
	if (pMouse)
	{
		bool bDown = pMouse->ButtonIsDown(eMouseButton_Left) ||
					 pMouse->ButtonIsDown(eMouseButton_Right) ||
					 pMouse->ButtonIsDown(eMouseButton_Middle);

		if (bDown && mbMouseWasDown == false)
		{
			mbMouseWasDown = true;
			return true;
		}
		if (bDown == false)
			mbMouseWasDown = false;
	}

	return false;
}

//-----------------------------------------------------------------------

void cSomaSplash::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	mfPhaseElapsed += afTimeStep;

	float fPhaseDuration = 0;
	if (mPhase == eSomaSplashPhase_FGLogo)
		fPhaseDuration = mfFGHoldTimerTotal + mfFGFadeOutTime;
	else if (mPhase == eSomaSplashPhase_BootInit)
		fPhaseDuration = mfBootFadeTime + mfBootHoldTime + mfBootFadeTime;

	if (AnySkipInputThisFrame() || mfPhaseElapsed >= fPhaseDuration)
	{
		AdvanceToNextPhase();
	}
}

//-----------------------------------------------------------------------

void cSomaSplash::DrawFGLogoPhase()
{
	if (mpFGLogo == NULL)
		return;

	// Real rates: fade-in 0.4/s (~2.5s), hold until the 3s timer fires,
	// fade-out 0.5/s (~2s) - see SomaSplash.h point 5.
	float fAlpha;
	if (mfPhaseElapsed < mfFGHoldTimerTotal)
	{
		fAlpha = cMath::Clamp(mfPhaseElapsed / mfFGFadeInTime, 0.0f, 1.0f);
	}
	else
	{
		float fFadeOutElapsed = mfPhaseElapsed - mfFGHoldTimerTotal;
		fAlpha = cMath::Clamp(1.0f - (fFadeOutElapsed / mfFGFadeOutTime), 0.0f, 1.0f);
	}

	// Scale down (never up) to fit on screen, preserving aspect ratio -
	// kept from the previous version of this file (already visually
	// correct per user feedback), see SomaSplash.h point 5 for why the
	// real script's exact undocumented box size wasn't chased instead.
	cVector2f vImgSize = mpFGLogo->GetImageSize();
	float fScale = cMath::Min(1.0f, cMath::Min(mvScreenSize.x / vImgSize.x, mvScreenSize.y / vImgSize.y));
	vImgSize = vImgSize * fScale;

	cVector3f vPos((mvScreenSize.x - vImgSize.x) * 0.5f,
					(mvScreenSize.y - vImgSize.y) * 0.5f, 1);

	mpGuiSet->DrawGfx(mpFGLogo, vPos, vImgSize, cColor(1, 1, 1, fAlpha));
}

//-----------------------------------------------------------------------

void cSomaSplash::DrawBootInitPhase()
{
	float fPhaseDuration = mfBootFadeTime + mfBootHoldTime + mfBootFadeTime;

	float fAlpha = 1.0f;
	if (mfPhaseElapsed < mfBootFadeTime)
		fAlpha = cMath::Clamp(mfPhaseElapsed / mfBootFadeTime, 0.0f, 1.0f);
	else if (mfPhaseElapsed > fPhaseDuration - mfBootFadeTime)
		fAlpha = cMath::Clamp((fPhaseDuration - mfPhaseElapsed) / mfBootFadeTime, 0.0f, 1.0f);

	// Bar fill fraction - smooth time-based animation across the whole
	// phase, not a fabricated "phase" breakdown - see SomaSplash.h's
	// ePhase_BootInit note for why.
	float fBarFraction = cMath::Clamp(mfPhaseElapsed / fPhaseDuration, 0.0f, 1.0f);

	if (mpPremenuBg)
	{
		// Real asset is already a 1920x1080 full-bleed composite
		// (graphics/startmenu/premenu/Premenu.png) - scale-to-fit same as
		// the FG logo, in case this splash ever runs at a non-16:9
		// resolution.
		cVector2f vImgSize = mpPremenuBg->GetImageSize();
		float fScale = cMath::Min(mvScreenSize.x / vImgSize.x, mvScreenSize.y / vImgSize.y);
		vImgSize = vImgSize * fScale;

		cVector3f vPos((mvScreenSize.x - vImgSize.x) * 0.5f,
						(mvScreenSize.y - vImgSize.y) * 0.5f, 1);

		mpGuiSet->DrawGfx(mpPremenuBg, vPos, vImgSize, cColor(1, 1, 1, fAlpha));
	}

	if (mpLoadingFrame && mpLoadingBar)
	{
		// Placement approximation - see SomaSplash.h point 6 for why
		// there's no further real evidence for an exact position. Sized
		// relative to screen width, preserving the real 1024:128 (8:1)
		// asset aspect ratio, positioned just under where Premenu.png's
		// own baked "INITIALIZATION..." text sits (roughly 44% down the
		// 1920x1080 reference image).
		float fBarWidth = mvScreenSize.x * 0.32f;
		float fBarHeight = fBarWidth * (128.0f / 1024.0f);
		cVector3f vBarPos(mvScreenSize.x * 0.17f, mvScreenSize.y * 0.47f, 2);
		cVector2f vBarSize(fBarWidth, fBarHeight);

		// Static decoration - always fully visible.
		mpGuiSet->DrawGfx(mpLoadingFrame, vBarPos, vBarSize, cColor(1, 1, 1, fAlpha));

		// Fill - clipped horizontally to [0, fBarFraction] of vBarSize.x.
		// See SomaSplash.h's mpBarClipRegion comment for why this reuses
		// one persistent region rather than allocating a new child every
		// frame.
		mpBarClipRegion->mRect = cRect2f(vBarPos.x, vBarPos.y, vBarSize.x * fBarFraction, vBarSize.y);

		cGuiClipRegion *pPrevRegion = mpGuiSet->GetCurrentClipRegion();
		mpGuiSet->SetCurrentClipRegion(mpBarClipRegion);
		mpGuiSet->DrawGfx(mpLoadingBar, vBarPos, vBarSize, cColor(1, 1, 1, fAlpha));
		mpGuiSet->SetCurrentClipRegion(pPrevRegion);
	}
}

//-----------------------------------------------------------------------

void cSomaSplash::OnDraw(float afFrameTime)
{
	if (mbFinished)
		return;

	mpGuiSet->DrawGfx(mpBlackBg, cVector3f(0, 0, 0), mvScreenSize);

	switch (mPhase)
	{
	case eSomaSplashPhase_FGLogo:
		DrawFGLogoPhase();
		break;
	case eSomaSplashPhase_BootInit:
		DrawBootInitPhase();
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------
