/*
 * Native, hardcoded port of SOMA's real 00_00_intro.hpm opening sequence -
 * Simon's flashback of the car crash with Ashley - shown as a fixed
 * timeline of 2D still images cross-fading over a black background, plus a
 * real dialogue subtitle track and voice-over, ending with the same map
 * change to 00_01_apartment.hpm the real game makes.
 *
 * This is NOT a generic script interpreter - there is still zero real
 * AngelScript/.hps execution anywhere in this codebase (see PORTING_NOTES.md).
 * This is a one-off reimplementation of exactly what
 * maps/chapter00/00_00_intro/00_00_intro.hps's cScrMap does - confirmed by
 * reading that file's whole OnStart()/OnGui()/Update() (1141 lines): it
 * builds a purely 2D "slideshow" out of its own iSlideShowElement/cSlide/
 * cSlideShowVoice helper classes (AddSlide()/AddCustomSlide()/AddVoice()),
 * renders it entirely through ImGui image/text draws over a hardcoded black
 * background ("Black background (Needed since scene is not being rendered,
 * thus screen is never cleared)" - OnGui()'s own comment), and disables the
 * player for the whole map (OnEnter(): Player_SetActive(false)) - i.e. this
 * really is "a slideshow and dialogue" as reported, not scripted 3D gameplay,
 * making it a tractable native-port candidate unlike most other .hps content.
 *
 * BuildTimeline() (see the .cpp) ports OnStart()'s AddSlide()/AddCustomSlide()/
 * AddVoice() call sequence call-for-call, with the exact same literal
 * arguments in the exact same order, accumulating a running clock the same
 * way the real mfSlideShowLength/AddTimeToSlideShow() does - so the same
 * real slide/voice start times fall out of the same arithmetic the real
 * script runs, rather than being hand-derived (error-prone: this file's own
 * commit history includes an early hand-computed timeline that silently
 * drifted from two of the real script's own inline comments after ~30
 * chained float additions).
 *
 * Real content sources (all confirmed present in a real SOMA install,
 * read directly, not guessed):
 *  - The 6 real slide images: graphics/intro/0{1..6}_*.jpg.
 *  - The opening quote ("Reality is that which, when you stop believing in
 *    it, doesn't go away." - Philip K. Dick): config/lang_main/english.lang,
 *    CATEGORY Name="00_00_intro", Entries "Quote"/"Signature" - this is what
 *    OnStart()'s AddCustomSlide("DrawTextSlide",...) draws via
 *    ImGui_DoTextFrameExt("Quote",...)/ImGui_DoLabelExt("Signature",...),
 *    which resolve through the current translation category (see
 *    helper_imgui.hps's ImGui_DoLabelExt() doc comment).
 *  - Real dialogue text + voice-over filenames: 00_00_intro.voice (its
 *    <Subject Name="Intro_3".."Intro_7"> entries - the only ones OnStart()
 *    actually calls AddVoice() for; Intro_0/1/2/8/9 exist in the same voice
 *    bank but belong to other content, not this map's OnStart()). Real
 *    per-line .ogg files ship under lang/eng/voices/00_00_intro/ as plain
 *    (non-FMOD-banked) audio, same convention as the main menu's music - see
 *    SomaMainMenu.cpp's comment on Menu_Music.ogg - so these play through
 *    this engine's existing cSoundHandler::PlayGui() with no new backend.
 *
 * Known, deliberate gaps vs the real sequence (kept, rather than attempting
 * a fragile approximation of things this engine cannot yet do faithfully):
 *  - No Ken-Burns camera pan/zoom (real script's AddCamMovement()/
 *    AddCamMovementReset() calls) - slides are shown static. Purely
 *    cosmetic; omitted to keep this port's timeline logic simple and
 *    correct rather than also chasing a coordinate-space match for a subtle
 *    effect.
 *  - No ambient SFX cue: the real one ("00_05_apartment2/SFX/game_intro_seq")
 *    lives inside sounds/level/00_05_apartment2_sfx.fsb, an FMOD Studio
 *    soundbank - this engine has no FMOD reader (see PORTING_NOTES.md's FMOD
 *    section), so it cannot be played back at all, plain-file re-encode or
 *    otherwise, without that separate reverse-engineering effort.
 *  - Real per-line VoiceOffset/EndPadding fields (00_00_intro.voice) that
 *    fine-tune subtitle-to-audio sync are not reproduced bit-for-bit; each
 *    line's on-screen hold time here is its real probed .ogg duration plus a
 *    fixed small reading tail (see the .cpp), not an exact frame match.
 *
 * Modeled on cSomaSplash's shape (a small self-contained iUpdateable driving
 * its own cGuiSet/cViewport, calling back into cSomaBase when done) - see
 * SomaSplash.h for why this codebase's objects, once registered with
 * cUpdater::AddGlobalUpdate(), are never destroyed.
 */

#ifndef SOMA_INTRO_SEQUENCE_H
#define SOMA_INTRO_SEQUENCE_H

#include "hpl.h"

#include <vector>
#include <map>

using namespace hpl;

class cSomaBase;

//----------------------------------------------

struct cIntroSlide
{
	tString msFile;   // "" = nothing drawn (blank/black spacer), unless mbTextSlide
	float mfStartTime;
	float mfFadeTime;
	bool mbTextSlide; // true only for the opening quote card
};

struct cIntroVoiceLine
{
	tString msFile;    // real .ogg filename (with extension), "" = no audio (one real line has none)
	tString msSpeaker;
	tString msText;
	float mfHoldTime;  // real probed .ogg duration + a small reading tail (see .cpp)
};

struct cIntroVoiceEvent
{
	float mfStartTime;
	std::vector<cIntroVoiceLine> mLines;
};

//----------------------------------------------

class cSomaIntroSequence : public iUpdateable
{
public:
	cSomaIntroSequence(cEngine *apEngine, cSomaBase *apBase);
	~cSomaIntroSequence();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

private:
	void BuildTimeline();

	// Mirrors the real script's AddTimeToSlideShow()/AddSlide()/AddCustomSlide()/
	// AddVoice() - see .cpp - accumulating mfLength exactly like the real
	// mfSlideShowLength.
	float Accumulate(float afOffset);
	void AddSlideBlank(float afOffset, float afFadeTime);
	void AddSlideImage(const tString &asFile, float afOffset, float afFadeTime);
	void AddSlideCustom(float afOffset, float afFadeTime);
	void AddVoiceEvent(float afOffset, const std::vector<cIntroVoiceLine> &aLines);

	cGuiGfxElement *GetOrLoadSlideGfx(const tString &asFile);

	void AdvanceSlides(float afTimeStep);
	void AdvanceVoice(float afTimeStep);
	void PlayLine(const cIntroVoiceLine &aLine);

	void DrawSlide(const cIntroSlide &aSlide, float afAlpha);
	void DrawTextSlide(float afAlpha);
	void DrawSubtitle();
	void DrawWrappedText(const tString &asText, iFontData *apFont, const cVector2f &avFontSize,
						  const cVector3f &avCenterPos, float afMaxWidth, const cColor &aColor,
						  eFontAlign aAlign, bool abGrowUpward);

	void Finish();

	cEngine *mpEngine;
	cSomaBase *mpBase;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;
	cViewport *mpViewport;

	cVector2f mvScreenSize;

	cGuiGfxElement *mpBlackBg;
	std::map<tString, cGuiGfxElement*> mmSlideGfxCache;

	iFontData *mpQuoteFont;
	iFontData *mpSignatureFont;
	iFontData *mpSubtitleFont;

	////////////////////////////////////
	// Timeline data, built once by BuildTimeline()
	std::vector<cIntroSlide> mvSlides;
	std::vector<cIntroVoiceEvent> mvVoiceEvents;
	float mfLength;   // == real script's final mfSlideShowLength
	float mfEndTime;  // == real script's Event_EndSlideShow timer (mfLength + 1.16f)

	////////////////////////////////////
	// Runtime playback state
	float mfTimer;

	int mlCurrentSlideIndex;
	float mfSlideFadeTimer;

	int mlCurrentSubjectIndex;
	int mlCurrentLineIndex;
	float mfLineTimer;
	tString msCurrentSpeaker;
	tString msCurrentSubtitle;

	bool mbFinished;
};

//----------------------------------------------

#endif // SOMA_INTRO_SEQUENCE_H
