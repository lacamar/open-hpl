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

// Brainscan loading-icon animation rate - no real evidence recovered for the
// exact native value (see SomaSplash.h point 7), 12fps is a plausible guess
// for a low-res EEG-style loop like this.
const float cSomaSplash::mfBrainFrameRate = 12.0f;

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

	for (int i = 0; i < mlBrainFrameCount; ++i)
		mvBrainFrames[i] = NULL;

	mpMenuAmbientSound = NULL;
	mlMenuAmbientSoundId = -1;

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

	// config/game.cfg: LoadingIcon = "brain_01.dds" - real 26-frame sequence,
	// see SomaSplash.h point 7. Resolved by filename alone, same as the
	// other textures above - resources.cfg's <Directory Path="/graphics"
	// AddSubDirs="true"/> covers graphics/general/loadscreen/brainAnim/.
	for (int i = 0; i < mlBrainFrameCount; ++i)
	{
		tString sFrameFile = "brain_" + cString::ToString(i + 1, 2) + ".dds";
		mvBrainFrames[i] = mpGui->CreateGfxTexture(sFrameFile, eGuiMaterial_Alpha, eTextureType_2D);
	}

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
		{
			// Stored so StopMenuAmbient() can stop this specific looping
			// instance later (see SomaSplash.h point 8 / that method's own
			// comment) - previously fire-and-forget, the real bug this pass
			// fixes.
			mpMenuAmbientSound = mpEngine->GetSound()->GetSoundHandler()->PlayGui(sBgNoise, true, 1.0f);
			if (mpMenuAmbientSound)
				mlMenuAmbientSoundId = mpMenuAmbientSound->GetId();
		}
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

void cSomaSplash::StopMenuAmbient()
{
	if (mpMenuAmbientSound == NULL)
		return;

	// cSoundHandler may have already destroyed/recycled this cSoundEntry*
	// by the time this runs (this class lives for the whole process, long
	// past the menu's own lifetime) - same IsValid(ptr, id) guard
	// amnesia/src/game/LuxEnemy_ManPig.cpp's mpMindFuckSound uses before
	// touching a stored cSoundEntry* again.
	cSoundHandler *pSoundHandler = mpEngine->GetSound()->GetSoundHandler();
	if (pSoundHandler->IsValid(mpMenuAmbientSound, mlMenuAmbientSoundId))
		mpMenuAmbientSound->Stop();

	mpMenuAmbientSound = NULL;
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
	for (int i = 0; i < mlBrainFrameCount; ++i)
	{
		if (mvBrainFrames[i])
		{
			mpGui->DestroyGfx(mvBrainFrames[i]);
			mvBrainFrames[i] = NULL;
		}
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

	// Reused for the loading bar/frame below (see their own comment) so
	// their on-screen size tracks Premenu.png's own scale at any resolution.
	float fPremenuScale = 1.0f;

	if (mpPremenuBg)
	{
		// Real asset is already a 1920x1080 full-bleed composite
		// (graphics/startmenu/premenu/Premenu.png) - scale-to-fit same as
		// the FG logo, in case this splash ever runs at a non-16:9
		// resolution.
		cVector2f vImgSize = mpPremenuBg->GetImageSize();
		fPremenuScale = cMath::Min(mvScreenSize.x / vImgSize.x, mvScreenSize.y / vImgSize.y);
		vImgSize = vImgSize * fPremenuScale;

		cVector3f vPos((mvScreenSize.x - vImgSize.x) * 0.5f,
						(mvScreenSize.y - vImgSize.y) * 0.5f, 1);

		mpGuiSet->DrawGfx(mpPremenuBg, vPos, vImgSize, cColor(1, 1, 1, fAlpha));
	}

	if (mpLoadingFrame && mpLoadingBar)
	{
		// Real evidence this pass (previous version of this block was a
		// pure visual-judgment guess - see the removed comment in git
		// history): `nm -C`/`objdump -d` on the real, unstripped
		// Soma.bin.x86_64 (it has debug_info - confirmed `file` output)
		// locates a native cLuxLoadHandler class whose constructor
		// (cLuxLoadHandler::cLuxLoadHandler(), file vaddr 0xb1a6f0)
		// disassembles to cConfigFile::GetString("General", ...) calls for
		// "LoadingIcon"/"SplashScreen"/"LoadingBar"/"LoadingFrame"/
		// "SplashScreenMusic" IN THAT EXACT ORDER - i.e. this is the real
		// native class config/game.cfg's <General> block belongs to, and
		// it groups the boot splash (SplashScreen) with LoadingBar/
		// LoadingFrame, confirming (not just assuming) they're meant to
		// composite together.
		//
		// Its OnDraw() (file vaddr 0xb1d190) disassembles to a
		// cGuiSet::DrawGfx() call for the loading bar (and a second, near-
		// identical one for the frame) with a LITERAL, HARDCODED size
		// immediate of (1024.0, 128.0) - i.e. the asset's own exact native
		// pixel dimensions (`identify` confirms both loading_bar.dds and
		// loading_frame.dds are 1024x128) - not a fraction of screen
		// width like the previous version of this block used. The same
		// call's position math includes a literal "-512.0" float
		// (sitting in .rodata right next to the "LoadingBar"/
		// "LoadingFrame" config-key strings themselves) - exactly half
		// that 1024 width - applied to a term that's otherwise built from
		// a "screen-width * 0.5" component; i.e. the real code converts a
		// horizontal-CENTER coordinate into a left-edge draw position,
		// confirming the bar is horizontally centered on screen (not
		// left-anchored at a fixed inset like the previous version
		// guessed). The analogous vertical term uses no such "* 0.5" on
		// its screen-size input and instead has a "-256.0" literal -
		// read here as the same pattern applied to a BOTTOM-edge anchor
		// instead of a center one (screen height minus a fixed inset),
		// which - unlike full vertical centering - keeps the bar clear of
		// Premenu.png's own baked "INITIALIZATION.../LOAD/OPTIONS" text
		// block (roughly 43%-53% down the 1080-tall reference image, per
		// direct pixel inspection this pass). This vertical reading is
		// this pass's best inference, not a runtime-confirmed value (the
		// exact fields the formula reads live in engine globals this pass
		// had no way to sample live without running the real closed
		// binary, which is out of bounds - see PORTING_NOTES.md); the
		// size and horizontal-centering findings above are the solid
		// part of this evidence.
		cVector2f vBarSize(1024.0f * fPremenuScale, 128.0f * fPremenuScale);
		cVector3f vBarPos((mvScreenSize.x - vBarSize.x) * 0.5f,
						   mvScreenSize.y - 256.0f * fPremenuScale, 2);

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

	DrawBrainIcon(fAlpha, fPremenuScale);
}

//-----------------------------------------------------------------------

void cSomaSplash::DrawBrainIcon(float afAlpha, float afPremenuScale)
{
	// config/game.cfg: LoadingIcon = "brain_01.dds" - real 26-frame animated
	// sequence (graphics/general/loadscreen/brainAnim/brain_01.dds ..
	// brain_26.dds, each a genuine distinct 512x512 DDS frame) - see
	// SomaSplash.h point 7 for the native cLuxLoadHandler evidence tying
	// this to the boot-init phase. No real evidence recovered for the
	// native on-screen size/position/frame rate, so a modest bottom-right
	// icon (matching the user's reference screenshot) and mfBrainFrameRate
	// are this pass's plausible values, not disassembly-confirmed.
	int lFrame = ((int)(mfPhaseElapsed * mfBrainFrameRate)) % mlBrainFrameCount;
	if (lFrame < 0)
		lFrame = 0;

	cGuiGfxElement *pFrame = mvBrainFrames[lFrame];
	if (pFrame == NULL)
		return;

	cVector2f vIconSize(140.0f * afPremenuScale, 140.0f * afPremenuScale);
	float fMargin = 40.0f * afPremenuScale;

	cVector3f vPos(mvScreenSize.x - vIconSize.x - fMargin,
				   mvScreenSize.y - vIconSize.y - fMargin, 2);

	mpGuiSet->DrawGfx(pFrame, vPos, vIconSize, cColor(1, 1, 1, afAlpha));
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
