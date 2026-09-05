/*
 * See SomaIntroSequence.h for scope notes and real-source citations.
 */

#include "SomaIntroSequence.h"
#include "SomaBase.h"

//---------------------------------------

// Real English quote card - config/lang_main/english.lang, CATEGORY
// Name="00_00_intro", Entries "Quote"/"Signature" (see the real
// AddCustomSlide("DrawTextSlide",...)/DrawTextSlide() in 00_00_intro.hps).
static const char *kQuoteText = "\"Reality is that which, when you stop believing in it, doesn't go away.\"";
static const char *kSignatureText = "- Philip K. Dick";

// Small fixed reading tail added to each line's real probed .ogg duration
// (see BuildTimeline()) so on-screen text does not vanish the instant audio
// stops - the real engine's own EndPadding/VoiceOffset fields do something
// similar but are not reproduced bit-for-bit here (see SomaIntroSequence.h).
static const float kSubtitleReadingTail = 0.4f;

// Real "Intro_7" subject's 4th line (00_00_intro.voice) has real dialogue
// text ("For what?") but no FileName attribute at all - i.e. no real
// voice-over line exists for it. Held on screen for this long, silent.
static const float kSilentLineHoldTime = 1.1f;

//---------------------------------------

cSomaIntroSequence::cSomaIntroSequence(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaIntroSequence")
{
	mpEngine = apEngine;
	mpBase = apBase;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("IntroSequence", mpGuiSkin);

	// GUI-only viewport, same idiom as cSomaSplash - the real 00_00_intro.hpm
	// 3D scene is loaded behind this (LoadMap() already ran by the time this
	// is constructed - see cSomaBase::StartNewGame()) but never needs to be
	// seen: the real script's own OnGui() draws an opaque black background
	// "since scene is not being rendered" (see the .h's citation), and this
	// port does the same by drawing on a dedicated always-on-top GUI set
	// rather than trying to suppress the world viewport underneath.
	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, true);
	mpViewport->AddGuiSet(mpGuiSet);

	mpBlackBg = mpGui->CreateGfxFilledRect(cColor(0, 1), eGuiMaterial_Alpha);

	mpQuoteFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_large_bold.fnt");
	mpSignatureFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_large.fnt");
	mpSubtitleFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_medium_bold.fnt");

	mfLength = 0;
	mfEndTime = 0;
	BuildTimeline();

	mfTimer = 0;

	mlCurrentSlideIndex = -1;
	mfSlideFadeTimer = 0;

	mlCurrentSubjectIndex = -1;
	mlCurrentLineIndex = -1;
	mfLineTimer = 0;

	mbFinished = false;
}

//-----------------------------------------------------------------------

cSomaIntroSequence::~cSomaIntroSequence()
{
}

//-----------------------------------------------------------------------

float cSomaIntroSequence::Accumulate(float afOffset)
{
	mfLength += afOffset;
	return mfLength;
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::AddSlideBlank(float afOffset, float afFadeTime)
{
	cIntroSlide slide;
	slide.msFile = "";
	slide.mfStartTime = Accumulate(afOffset);
	slide.mfFadeTime = afFadeTime;
	slide.mbTextSlide = false;
	mvSlides.push_back(slide);
}

void cSomaIntroSequence::AddSlideImage(const tString &asFile, float afOffset, float afFadeTime)
{
	cIntroSlide slide;
	slide.msFile = asFile;
	slide.mfStartTime = Accumulate(afOffset);
	slide.mfFadeTime = afFadeTime;
	slide.mbTextSlide = false;
	mvSlides.push_back(slide);
}

void cSomaIntroSequence::AddSlideCustom(float afOffset, float afFadeTime)
{
	cIntroSlide slide;
	slide.msFile = "";
	slide.mfStartTime = Accumulate(afOffset);
	slide.mfFadeTime = afFadeTime;
	slide.mbTextSlide = true;
	mvSlides.push_back(slide);
}

void cSomaIntroSequence::AddVoiceEvent(float afOffset, const std::vector<cIntroVoiceLine> &aLines)
{
	cIntroVoiceEvent event;
	event.mfStartTime = Accumulate(afOffset);
	event.mLines = aLines;
	mvVoiceEvents.push_back(event);
}

//-----------------------------------------------------------------------

// Ported call-for-call from 00_00_intro.hps's OnStart() - same literal
// arguments, same order, same running-total arithmetic (Accumulate() here ==
// the real AddTimeToSlideShow()). Real script's AddCamMovement()/
// AddCamMovementReset() calls (Ken-Burns pan/zoom) are intentionally not
// ported - see SomaIntroSequence.h's "known gaps".
void cSomaIntroSequence::BuildTimeline()
{
	std::vector<cIntroVoiceLine> lines;

	AddSlideBlank(0.0f, 0.0f);
	AddSlideCustom(0.5f, 1.0f); // "DrawTextSlide" - the Philip K. Dick quote card
	AddSlideBlank(4.5f, 1.0f);

	// Intro_3 (00_00_intro.voice)
	lines.clear();
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_3_001_ashley_001.ogg";
	lines.back().msSpeaker = "Ashley";
	lines.back().msText = "Are you okay, Simon? I think you're bleeding.";
	lines.back().mfHoldTime = 2.935958f + kSubtitleReadingTail;
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_3_002_simon_001.ogg";
	lines.back().msSpeaker = "Simon";
	lines.back().msText = "Oh, that's nothing -- it's just my brain can't stop bleeding from the accident.";
	lines.back().mfHoldTime = 4.565104f + kSubtitleReadingTail;
	AddVoiceEvent(3.75f, lines);

	AddSlideImage("01_SimonMirror.jpg", 2.55f, 0.0f);
	AddSlideBlank(0.9f, 1.5f);

	// Intro_4
	lines.clear();
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_4_001_ashley_001.ogg";
	lines.back().msSpeaker = "Ashley";
	lines.back().msText = "Here, take this.";
	lines.back().mfHoldTime = 1.001021f + kSubtitleReadingTail;
	AddVoiceEvent(2.5f, lines);

	// Intro_5
	lines.clear();
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_5_001_simon_001.ogg";
	lines.back().msSpeaker = "Simon";
	lines.back().msText = "No, that's for later -- for the scan.";
	lines.back().mfHoldTime = 2.010417f + kSubtitleReadingTail;
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_5_002_ashley_001.ogg";
	lines.back().msSpeaker = "Ashley";
	lines.back().msText = "It's green.";
	lines.back().mfHoldTime = 0.700354f + kSubtitleReadingTail;
	AddVoiceEvent(4.1f, lines);

	AddSlideImage("02_Bottle.jpg", 0.4f, 1.5f);
	AddSlideBlank(2.0f, 1.0f);

	// Intro_6
	lines.clear();
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_6_001_simon_001.ogg";
	lines.back().msSpeaker = "Simon";
	lines.back().msText = "Ashley, I need to tell you something.";
	lines.back().mfHoldTime = 2.210417f + kSubtitleReadingTail;
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_6_002_ashley_001.ogg";
	lines.back().msSpeaker = "Ashley";
	lines.back().msText = "Simon, please don't make this weird--";
	lines.back().mfHoldTime = 2.141938f + kSubtitleReadingTail;
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_6_003_simon_001.ogg";
	lines.back().msSpeaker = "Simon";
	lines.back().msText = "No, no, it's not like that.";
	lines.back().mfHoldTime = 1.650521f + kSubtitleReadingTail;
	AddVoiceEvent(4.7f, lines);

	AddSlideImage("03_AshleyPortrait.jpg", 3.17f, 0.5f);
	AddSlideBlank(0.33f, 4.0f);

	// Intro_7 - last line ("For what?") has real dialogue text but no real
	// voice-over file at all (see 00_00_intro.voice's own Intro_7 Subject).
	lines.clear();
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_7_001_simon_001.ogg";
	lines.back().msSpeaker = "Simon";
	lines.back().msText = "Why now?";
	lines.back().mfHoldTime = 1.031771f + kSubtitleReadingTail;
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_7_002_ashley_001.ogg";
	lines.back().msSpeaker = "Ashley";
	lines.back().msText = "Who's David Munshi?";
	lines.back().mfHoldTime = 1.732438f + kSubtitleReadingTail;
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "intro_intro_7_003_simon_001.ogg";
	lines.back().msSpeaker = "Simon";
	lines.back().msText = "Why is there never enough time?!";
	lines.back().mfHoldTime = 1.477000f + kSubtitleReadingTail;
	lines.push_back(cIntroVoiceLine());
	lines.back().msFile = "";
	lines.back().msSpeaker = "Ashley";
	lines.back().msText = "For what?";
	lines.back().mfHoldTime = kSilentLineHoldTime;
	AddVoiceEvent(1.0f, lines);

	AddSlideImage("04_PhoneClose.jpg", 3.6f, 0.27f);
	AddSlideBlank(0.27f, 1.16f);
	AddSlideImage("03_AshleyPortrait.jpg", 1.8f, 0.27f);
	AddSlideBlank(0.27f, 1.0f);
	AddSlideImage("05_InCar.jpg", 1.8f, 0.27f);
	AddSlideBlank(0.27f, 1.0f);
	AddSlideImage("04_PhoneClose.jpg", 1.8f, 0.27f);
	AddSlideBlank(0.27f, 1.16f);
	AddSlideImage("06_AshClose.jpg", 1.8f, 0.27f);
	AddSlideBlank(0.27f, 1.16f);

	// Real script: AddEvent("Event_FadeOut", 0.0) then
	// AddEvent("Event_EndSlideShow", +1.16f) - both accumulate into
	// mfSlideShowLength the same way slides/voices do.
	Accumulate(0.0f);
	mfEndTime = Accumulate(1.16f);
}

//-----------------------------------------------------------------------

cGuiGfxElement *cSomaIntroSequence::GetOrLoadSlideGfx(const tString &asFile)
{
	std::map<tString, cGuiGfxElement*>::iterator it = mmSlideGfxCache.find(asFile);
	if (it != mmSlideGfxCache.end())
		return it->second;

	cGuiGfxElement *pGfx = mpGui->CreateGfxTexture(asFile, eGuiMaterial_Alpha, eTextureType_2D);
	if (pGfx == NULL)
		Log("SOMA intro sequence: could not load slide image '%s'\n", asFile.c_str());

	mmSlideGfxCache[asFile] = pGfx;
	return pGfx;
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::AdvanceSlides(float afTimeStep)
{
	if (mfSlideFadeTimer > 0.0f)
		mfSlideFadeTimer -= afTimeStep;

	while (mlCurrentSlideIndex + 1 < (int)mvSlides.size() &&
		   mfTimer >= mvSlides[mlCurrentSlideIndex + 1].mfStartTime)
	{
		++mlCurrentSlideIndex;
		const cIntroSlide &curSlide = mvSlides[mlCurrentSlideIndex];

		mfSlideFadeTimer = curSlide.mfFadeTime;
		if (mlCurrentSlideIndex + 1 < (int)mvSlides.size())
		{
			float fGap = mvSlides[mlCurrentSlideIndex + 1].mfStartTime - curSlide.mfStartTime;
			mfSlideFadeTimer = cMath::Min(mfSlideFadeTimer, fGap);
		}

		// Preload eagerly on transition so the very first OnDraw() of a new
		// slide never has to wait on a texture load - all 6 real slide
		// images are tiny enough that upfront loading isn't worth the extra
		// bookkeeping cSomaSplash's lazy variant needed for a splash of
		// arbitrary/unknown images.
		if (curSlide.msFile != "")
			GetOrLoadSlideGfx(curSlide.msFile);
	}
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::PlayLine(const cIntroVoiceLine &aLine)
{
	msCurrentSpeaker = aLine.msSpeaker;
	msCurrentSubtitle = aLine.msText;

	if (aLine.msFile != "")
	{
		mpEngine->GetSound()->GetSoundHandler()->PlayGui(aLine.msFile, false, 1.0f);
	}
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::AdvanceVoice(float afTimeStep)
{
	if (mlCurrentSubjectIndex + 1 < (int)mvVoiceEvents.size() &&
		mfTimer >= mvVoiceEvents[mlCurrentSubjectIndex + 1].mfStartTime)
	{
		++mlCurrentSubjectIndex;
		mlCurrentLineIndex = -1;
		mfLineTimer = 0;
	}

	if (mlCurrentSubjectIndex < 0)
		return;

	const cIntroVoiceEvent &subject = mvVoiceEvents[mlCurrentSubjectIndex];

	if (mfLineTimer > 0.0f)
	{
		mfLineTimer -= afTimeStep;
		return;
	}

	if (mlCurrentLineIndex + 1 < (int)subject.mLines.size())
	{
		++mlCurrentLineIndex;
		const cIntroVoiceLine &line = subject.mLines[mlCurrentLineIndex];
		mfLineTimer = line.mfHoldTime;
		PlayLine(line);
	}
	else
	{
		msCurrentSubtitle = "";
	}
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::Update(float afTimeStep)
{
	if (mbFinished)
		return;

	mfTimer += afTimeStep;

	AdvanceSlides(afTimeStep);
	AdvanceVoice(afTimeStep);

	if (mfTimer >= mfEndTime)
		Finish();
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::Finish()
{
	if (mbFinished)
		return;

	mbFinished = true;

	mpViewport->SetActive(false);

	if (mpBase)
		mpBase->OnIntroSequenceFinished();
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::DrawWrappedText(const tString &asText, iFontData *apFont, const cVector2f &avFontSize,
										  const cVector3f &avCenterPos, float afMaxWidth, const cColor &aColor,
										  eFontAlign aAlign, bool abGrowUpward)
{
	if (apFont == NULL)
		return;

	tWString sWideText = cString::To16Char(asText);

	tWStringVec vRows;
	apFont->GetWordWrapRows(afMaxWidth, avFontSize.y, avFontSize, sWideText, &vRows);

	float fTotalHeight = (float)vRows.size() * avFontSize.y;
	float fStartY = avCenterPos.y - (abGrowUpward ? fTotalHeight : fTotalHeight * 0.5f);

	for (size_t i = 0; i < vRows.size(); ++i)
	{
		cVector3f vRowPos(avCenterPos.x, fStartY + (float)i * avFontSize.y, avCenterPos.z);
		mpGuiSet->DrawFont(vRows[i], apFont, vRowPos, avFontSize, aColor, aAlign);
	}
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::DrawTextSlide(float afAlpha)
{
	cColor col(1, 1, 1, afAlpha);

	float fCenterX = mvScreenSize.x * 0.5f;
	float fCenterY = mvScreenSize.y * 0.42f;
	float fMaxWidth = mvScreenSize.x * 0.6f;

	DrawWrappedText(kQuoteText, mpQuoteFont, cVector2f(30, 30), cVector3f(fCenterX, fCenterY, 5),
					fMaxWidth, col, eFontAlign_Center, false);

	mpGuiSet->DrawFont(cString::To16Char(kSignatureText), mpSignatureFont,
						cVector3f(fCenterX + fMaxWidth * 0.5f, fCenterY + 60, 5),
						cVector2f(24, 24), col, eFontAlign_Right);
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::DrawSlide(const cIntroSlide &aSlide, float afAlpha)
{
	if (afAlpha <= 0.0f)
		return;

	if (aSlide.mbTextSlide)
	{
		DrawTextSlide(afAlpha);
		return;
	}

	if (aSlide.msFile == "")
		return; // blank/black spacer slide - nothing to draw

	cGuiGfxElement *pGfx = GetOrLoadSlideGfx(aSlide.msFile);
	if (pGfx == NULL)
		return;

	// Scale to fill the screen height, preserving aspect ratio and cropping
	// width if needed (real slides are landscape stills meant to fill frame,
	// unlike the splash logos which must show their whole extent) - centered
	// horizontally.
	cVector2f vImgSize = pGfx->GetImageSize();
	float fScale = mvScreenSize.y / vImgSize.y;
	vImgSize = vImgSize * fScale;

	cVector3f vPos((mvScreenSize.x - vImgSize.x) * 0.5f, 0, 1);

	mpGuiSet->DrawGfx(pGfx, vPos, vImgSize, cColor(1, 1, 1, afAlpha));
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::DrawSubtitle()
{
	if (msCurrentSubtitle == "")
		return;

	tString sLine = msCurrentSpeaker + ": " + msCurrentSubtitle;

	float fCenterX = mvScreenSize.x * 0.5f;
	float fBottomY = mvScreenSize.y * 0.86f;
	float fMaxWidth = mvScreenSize.x * 0.7f;

	DrawWrappedText(sLine, mpSubtitleFont, cVector2f(26, 26), cVector3f(fCenterX, fBottomY, 5),
					fMaxWidth, cColor(1, 1), eFontAlign_Center, true);
}

//-----------------------------------------------------------------------

void cSomaIntroSequence::OnDraw(float afFrameTime)
{
	if (mbFinished)
		return;

	mpGuiSet->DrawGfx(mpBlackBg, cVector3f(0, 0, 0), mvScreenSize);

	if (mlCurrentSlideIndex >= 0)
	{
		const cIntroSlide &curSlide = mvSlides[mlCurrentSlideIndex];

		float fAlpha = 1.0f;
		if (mfSlideFadeTimer > 0.0f && curSlide.mfFadeTime > 0.0f)
		{
			fAlpha = cMath::Clamp(mfSlideFadeTimer / curSlide.mfFadeTime, 0.0f, 1.0f);

			if (mlCurrentSlideIndex > 0)
				DrawSlide(mvSlides[mlCurrentSlideIndex - 1], fAlpha);

			fAlpha = 1.0f - fAlpha;
		}

		DrawSlide(curSlide, fAlpha);
	}

	DrawSubtitle();
}
