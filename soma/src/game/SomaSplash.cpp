/*
 * See SomaSplash.h for scope notes and citations for every real value used
 * below.
 */

#include "SomaSplash.h"
#include "SomaImGui.h"

cSomaImGui *SomaHudImGui();
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

// Brainscan loading-icon animation rate - real evidence this pass (see
// SomaSplash.h point 7): cLuxLoadHandler::OnDraw() disassembles to an
// elapsed-time accumulator multiplied by a literal double constant 15.0,
// compared against 2x the icon's real frame count in a ping-pong (bounce,
// not wraparound-loop) pattern. DrawBrainIcon() below now replicates both.
const float cSomaSplash::mfBrainFrameRate = 15.0f;

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
	mbRealBootWorkDone = false;

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
		mvBrainFrames[i] = mpGui->CreateGfxTexture(sFrameFile, eGuiMaterial_Additive, eTextureType_2D);
	}

	// Owned outright (not a child of the set's base clip region) so its
	// lifetime is entirely this class's responsibility - see
	// SomaSplash.h's comment on mpBarClipRegion for why this is allocated
	// once here rather than via CreateChild() every frame.
	mpBarClipRegion = hplNew(cGuiClipRegion, ());

	// Real order: boot-init (native cLuxLoadHandler) first, FG logo
	// (scripted GuiPreMenu(), part of the already-loaded main menu's own
	// update) second - see the file-top comment in SomaSplash.h for the
	// disassembly/script evidence this was corrected from.
	EnterPhase(eSomaSplashPhase_BootInit);
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

		// Task 3: real boot work, triggered for real right here instead of
		// only after this whole splash sequence finishes (the old
		// cSomaBase::OnSplashFinished()->ProceedPastBoot() path) - see
		// SomaSplash.h's "Two real phases" comment and
		// cSomaBase::PreloadMainMenuWorld()'s own comment in SomaBase.cpp.
		// This is a real, synchronous, blocking call (loads the actual
		// main_init.cfg <MainMenu> world) - not a synthetic delay - and
		// mbRealBootWorkDone genuinely can't become true before it returns.
		if (mpBase)
		{
			if (mpBase->UsesScriptMenu())
			{
				mpBase->LoadScriptMainMenu();
				// Draw after the map and HUD viewports
				SomaHudImGui();
				mpEngine->GetScene()->DestroyViewport(mpViewport);
				mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
				mpViewport->AddGuiSet(mpGuiSet);
			}
			else
				mpBase->PreloadMainMenuWorld();
		}
		mbRealBootWorkDone = true;
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
	if (mPhase == eSomaSplashPhase_BootInit && mpBase->UsesScriptMenu() == false)
	{
		EnterPhase(eSomaSplashPhase_FGLogo);
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
		fPhaseDuration = BootDuration();

	// Task 3a: never let eSomaSplashPhase_BootInit advance - by skip input
	// OR by its own cosmetic duration expiring - until the real main menu
	// world load it triggers in EnterPhase() has actually finished. See
	// mbRealBootWorkDone's own comment in SomaSplash.h for why this is
	// (currently) always already true by the time this runs, and why the
	// explicit check still matters. AnySkipInputThisFrame() is still called
	// unconditionally so a stray key/click during this window is consumed
	// (see that method's own comment on why) rather than left queued up to
	// incorrectly skip the FG logo phase right after this one opens.
	bool bSkip = AnySkipInputThisFrame();
	bool bBootWorkGate = (mPhase != eSomaSplashPhase_BootInit) || mbRealBootWorkDone;

	if (bBootWorkGate && (bSkip || mfPhaseElapsed >= fPhaseDuration))
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

// Timings and layout measured from the official game (scripts/soma-compare.py boot): linear fade-in,
// hold, long fade-out to black, then a cut; the brain icon fades in once and stays lit.
const float kBootFadeIn = 2.2f, kBootFadeOutStart = 3.6f, kBootFadeOut = 4.0f;
const float kBrainStart = 2.05f, kBrainFadeIn = 1.0f;
const float kScriptStart = 3.7f, kBootBlackHold = 0.1f;

float cSomaSplash::BootDuration() { return kBootFadeOutStart + kBootFadeOut + kBootBlackHold; }
bool cSomaSplash::ScriptsMayRun() { return mbFinished || mPhase != eSomaSplashPhase_BootInit || mfPhaseElapsed >= kScriptStart; }

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

// Official HUD virtual space: 1024x768 centre, widened by 4/3 of the extra width
cVector3f cSomaSplash::VirtualToScreen(const cVector2f &avPos, float afZ)
{
	float fW = (4.0f * 768.0f * mvScreenSize.x / mvScreenSize.y - 1024.0f) / 3.0f;
	return cVector3f((avPos.x + (fW - 1024.0f) * 0.5f) * mvScreenSize.x / fW, avPos.y * mvScreenSize.y / 768.0f, afZ);
}

cVector2f cSomaSplash::VirtualSizeToScreen(const cVector2f &avSize)
{
	float fW = (4.0f * 768.0f * mvScreenSize.x / mvScreenSize.y - 1024.0f) / 3.0f;
	return cVector2f(avSize.x * mvScreenSize.x / fW, avSize.y * mvScreenSize.y / 768.0f);
}

void cSomaSplash::DrawBootInitPhase()
{
	float t = mfPhaseElapsed;
	float fAlpha = t < kBootFadeOutStart ? cMath::Clamp(t / kBootFadeIn, 0.0f, 1.0f)
										 : cMath::Clamp(1.0f - (t - kBootFadeOutStart) / kBootFadeOut, 0.0f, 1.0f);

	if (mpPremenuBg)
	{
		cVector2f vImgSize = mpPremenuBg->GetImageSize();
		float fScale = cMath::Min(mvScreenSize.x / vImgSize.x, mvScreenSize.y / vImgSize.y);
		vImgSize = vImgSize * fScale;
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


void cSomaSplash::DrawBrainIcon(float afAlpha)
{
	if (afAlpha <= 0)
		return;
	float fRaw = mfPhaseElapsed * mfBrainFrameRate;
	int lPeriod = 2 * (mlBrainFrameCount - 1);
	int lStep = ((int)fRaw) % lPeriod;
	int lFrame = (lStep <= (mlBrainFrameCount - 1)) ? lStep : (lPeriod - lStep);

	cGuiGfxElement *pFrame = mvBrainFrames[lFrame];
	if (pFrame == NULL)
		return;

	float fW = (4.0f * 768.0f * mvScreenSize.x / mvScreenSize.y - 1024.0f) / 3.0f;
	cVector3f vPos = VirtualToScreen(cVector2f(1024.0f + (fW - 1024.0f) * 0.5f - 150.0f, 648.0f), 3);
	mpGuiSet->DrawGfx(pFrame, vPos, VirtualSizeToScreen(cVector2f(70, 70)), cColor(afAlpha, afAlpha, afAlpha, 1));
}


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
