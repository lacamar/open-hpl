/*
 * See SomaSplash.h for scope notes.
 */

#include "SomaSplash.h"
#include "SomaBase.h"

//---------------------------------------

const float cSomaSplash::mfFadeTime = 0.4f;

//---------------------------------------

cSomaSplash::cSomaSplash(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaSplash")
{
	mpEngine = apEngine;
	mpBase = apBase;

	mpCurrentImage = NULL;
	mlCurrentIndex = 0;
	mfTimer = 0;
	mbFinished = false;
	mbMouseWasDown = false;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	// Real SOMA install ships this - see SomaSplash.h for why these two
	// files specifically.
	mvImages.push_back(cSomaSplashImage());
	mvImages.back().msFile = "frictional_games_logo.dds";
	mvImages.back().mfHoldTime = 2.4f;

	mvImages.push_back(cSomaSplashImage());
	mvImages.back().msFile = "soma_logo_splash_static.dds";
	mvImages.back().mfHoldTime = 2.4f;

	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("Splash", mpGuiSkin);

	// GUI-only viewport - no camera, no world, just here to give the GUI
	// set somewhere to attach to and draw through before any real scene
	// exists.
	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, true);
	mpViewport->AddGuiSet(mpGuiSet);

	mpBlackBg = mpGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);

	LoadCurrentImage();
}

//-----------------------------------------------------------------------

cSomaSplash::~cSomaSplash()
{
}

//-----------------------------------------------------------------------

void cSomaSplash::LoadCurrentImage()
{
	if (mpCurrentImage)
	{
		mpGui->DestroyGfx(mpCurrentImage);
		mpCurrentImage = NULL;
	}

	if (mlCurrentIndex >= mvImages.size())
		return;

	mpCurrentImage = mpGui->CreateGfxTexture(
		mvImages[mlCurrentIndex].msFile, eGuiMaterial_Alpha, eTextureType_Rect);

	if (mpCurrentImage == NULL)
	{
		// Missing/unloadable image - don't get stuck showing nothing for
		// the full hold time, just skip straight to the next entry.
		Log("SOMA splash: could not load '%s', skipping\n",
			mvImages[mlCurrentIndex].msFile.c_str());
		AdvanceToNextImage();
		return;
	}

	mfTimer = mvImages[mlCurrentIndex].mfHoldTime;
}

//-----------------------------------------------------------------------

void cSomaSplash::AdvanceToNextImage()
{
	mlCurrentIndex++;

	if (mlCurrentIndex >= mvImages.size())
	{
		Finish();
		return;
	}

	LoadCurrentImage();
}

//-----------------------------------------------------------------------

void cSomaSplash::Finish()
{
	if (mbFinished)
		return;

	mbFinished = true;

	if (mpCurrentImage)
	{
		mpGui->DestroyGfx(mpCurrentImage);
		mpCurrentImage = NULL;
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

float cSomaSplash::ComputeFadeAlpha() const
{
	if (mlCurrentIndex >= mvImages.size())
		return 0;

	float fHold = mvImages[mlCurrentIndex].mfHoldTime;
	float fElapsed = fHold - mfTimer;

	if (fElapsed < mfFadeTime)
		return cMath::Clamp(fElapsed / mfFadeTime, 0.0f, 1.0f);
	if (mfTimer < mfFadeTime)
		return cMath::Clamp(mfTimer / mfFadeTime, 0.0f, 1.0f);

	return 1.0f;
}

//-----------------------------------------------------------------------

void cSomaSplash::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	mfTimer -= afTimeStep;

	if (AnySkipInputThisFrame() || mfTimer <= 0.0f)
	{
		AdvanceToNextImage();
	}
}

//-----------------------------------------------------------------------

void cSomaSplash::OnDraw(float afFrameTime)
{
	if (mbFinished)
		return;

	mpGuiSet->DrawGfx(mpBlackBg, cVector3f(0, 0, 0), mvScreenSize);

	if (mpCurrentImage)
	{
		cVector2f vImgSize = mpCurrentImage->GetImageSize();
		cVector3f vPos((mvScreenSize.x - vImgSize.x) * 0.5f,
						(mvScreenSize.y - vImgSize.y) * 0.5f, 1);

		float fAlpha = ComputeFadeAlpha();

		mpGuiSet->DrawGfx(mpCurrentImage, vPos, vImgSize, cColor(1, 1, 1, fAlpha));
	}
}

//-----------------------------------------------------------------------
