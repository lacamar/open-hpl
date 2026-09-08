/*
 * Native, hand-ported reimplementation of ONE part of SOMA's real
 * 00_01_apartment.hps map script: the David Munshi phone call that opens the
 * apartment scene (real script confirmed by reading
 * maps/chapter00/00_01_apartment/00_01_apartment.hps in full - OnStart(),
 * IntroSequence(), TimerRingTelephone(), AnswerPhone(), HandleTelephoneCall()
 * and the real dialogue data in the sibling 00_01_apartment.voice file).
 *
 * This is NOT a step toward a general AngelScript/.hps interpreter - there is
 * still zero real script execution anywhere in this codebase (see
 * PORTING_NOTES.md and cSomaIntroSequence.h's own identical disclaimer, which
 * this class's shape directly copies). This is a narrow, one-map, hardcoded
 * reimplementation of exactly this one phone call, nothing else in
 * 00_01_apartment.hps's ~1760 lines (lighting setup, drapes, the tracer-fluid
 * reminder loop, the answering machine, the bathroom/kitchen interactions,
 * the end-of-level timer, etc. are all still unimplemented and out of scope).
 *
 * Real timeline this reproduces (00_01_apartment.hps line numbers as read):
 *  - OnStart() (line 63): "if (cLux_ScriptDebugOn() == false)" branch (this
 *    port has no script-debug concept, so always taken - this is the normal
 *    player-facing path) does `Map_AddTimer("timer_introtext", 1.5,
 *    "TimerStartNarration")` (line 110).
 *  - TimerStartNarration() (line 230) calls IntroSequence("") (line 549)
 *    immediately (no further delay).
 *  - IntroSequence()'s first Sequence_DoStepAndWait(0.01) step (line 555)
 *    runs its body immediately and, among other wake-up-sequence actions,
 *    calls `Map_AddTimer("Timer_PhoneRing", 0.05, "TimerRingTelephone")`
 *    (line 562).
 *  - So the real ring starts ~1.5 + 0.05 = 1.55s after the map's OnStart()
 *    runs - this class uses that exact real total delay (kRingDelaySecs).
 *  - TimerRingTelephone() (line 669) starts the real ring sound
 *    ("Entities_Urban/tech/cellphone/vibrating_wood", looping) and enables
 *    interaction with the "Simon_Phone" entity.
 *  - AnswerPhone() (line 702), reached only via a real player interaction
 *    - via the real "InteractCellPhone_Dummy" Trigger-type Area
 *    (PlayerInteractCallback="OnInteractCellPhone" -> PhoneInteraction() ->
 *    AnswerPhone("Simon_Phone",1); NOT the "Simon_Phone" entity itself,
 *    which is a separate, later-game landline reused for the answering-
 *    machine feature at a different location - see SomaApartmentIntroCall.cpp's
 *    kPhoneWorldPos citation for the full real-source disambiguation) -
 *    stops the ring 0.1s later and opens
 *    `Dialog_AddBranchAndSubject("1_PhoneCall", ...)` (line 727), whose real
 *    line-by-line content lives in 00_01_apartment.voice's own
 *    Subject Name="1_PhoneCall" (11 lines, Simon/Munshi alternating). This
 *    port now reproduces the real interact gate for real (see below) -
 *    Munshi's dialogue does not start until cSomaPlayer reports the player
 *    genuinely pressed the real interact key while looking at the real
 *    phone interact point (see SomaPlayer.h's RegisterInteractPoint()/
 *    WasInteractedWith()/GetCurrentLookTarget() and SomaPlayer.cpp's
 *    UpdateLookTarget()).
 *
 * Known, deliberate honesty gaps vs the real sequence (kept rather than
 * faked - see PORTING_NOTES.md for the project-wide convention this
 * follows):
 *  - The interact system this class now uses (cSomaPlayer's
 *    RegisterInteractPoint()/WasInteractedWith()) is a small, hand-authored
 *    point+cone+line-of-sight test, NOT a general Area/trigger-volume system
 *    or a rebindable interact cAction - see SomaPlayer.h's own scope note.
 *    It reproduces the real "InteractCellPhone_Dummy" trigger's center point
 *    and real config/game.cfg DefaultMaxInteractDistance (2.0m), but not its
 *    real OBB extent/rotation (this engine has no compiled-Area reader - see
 *    SomaLoaders.h) or the real default interact key (SOMA's own interact
 *    binding isn't a plain single key in the default config data this
 *    engine reads - "E" was chosen as a plain, sensible default, consistent
 *    with cSomaPlayer's existing raw-keyboard-check pattern for Escape).
 *  - No real ring/pickup SFX: both real sounds
 *    ("Entities_Urban/tech/cellphone/vibrating_wood" and
 *    "00_05_apartment2/SFX/phone/pickup_counter") are FMOD Designer
 *    soundbank events (confirmed by reading sounds/entities/Entities_Urban.fdp
 *    directly - real underlying waveforms live inside
 *    sounds/entities/entities_urban.fsb, an FMOD-compiled bank) - this engine
 *    has no FMOD reader anywhere (see SomaIntroSequence.h's identical citation
 *    for the intro's own missing ambient cue). A silent ring/pickup is used
 *    instead; an on-screen "(phone ringing...)" line stands in for the real
 *    audio cue so the moment is still externally observable/verifiable.
 *  - The real IntroSequence() also drives a whole "waking up" camera
 *    animation/crouch-collision swap/HUD-disable sequence
 *    (CameraAnimation_Begin("CamAnim_WakeUp"), Player_SetJumpDisabled(true),
 *    etc.) unrelated to the phone call itself - not reproduced here; the
 *    player keeps normal control throughout. Also out of scope: everything
 *    else OnStart() does (lighting, drapes, tracer fluid, answering machine,
 *    end-of-level timer) - none of that is this class's concern.
 *  - The real 11-line "1_PhoneCall" dialogue's actual per-line voice-over
 *    .ogg files (lang/eng/voices/00_01_apartment/phonecall_1_phonecall_0NN_
 *    <player|david>_001.ogg) are real, plain (non-FMOD-banked) files this
 *    engine CAN play - these play for real, using the same
 *    cSoundHandler::PlayGui()-based, real-audio-completion-gated
 *    advance-to-next-line pattern used elsewhere in this codebase for
 *    dialogue playback (see the .cpp for exactly which API) rather than a
 *    fixed timer, so lines cannot overlap or feel stilted regardless of a
 *    given machine's real audio-decode latency.
 *
 * Modeled directly on cSomaIntroSequence's shape (a small self-contained
 * iUpdateable owning its own GUI-only overlay viewport/cGuiSet, registered
 * once with cUpdater::AddGlobalUpdate() and never destroyed - see
 * SomaIntroSequence.h for why this codebase's global iUpdateable objects are
 * never removed) - but unlike the intro sequence, this one does NOT own a
 * full-screen opaque background: the apartment is real, playable 3D
 * gameplay throughout, so only a small bottom-of-screen subtitle overlay is
 * drawn on top of the normal game viewport, exactly like a HUD element would
 * be.
 */

#ifndef SOMA_APARTMENT_INTRO_CALL_H
#define SOMA_APARTMENT_INTRO_CALL_H

#include "hpl.h"

#include <vector>

using namespace hpl;

class cSomaBase;

//----------------------------------------------

struct cApartmentCallLine
{
	tString msFile;       // real .ogg filename (lang/eng/voices/00_01_apartment/, bare name)
	tString msSpeaker;    // real Character DisplayName (00_01_apartment.voice)
	tString msText;       // real Sound Text=".." from the .voice Subject
	float mfFallbackHoldTime; // real probed .ogg duration + reading tail - safety net only,
	                          // see the .cpp's use of cSoundHandler::IsPlaying()
};

//----------------------------------------------

class cSomaApartmentIntroCall : public iUpdateable
{
public:
	cSomaApartmentIntroCall(cEngine *apEngine, cSomaBase *apBase);
	~cSomaApartmentIntroCall();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

private:
	enum eCallPhase
	{
		eCallPhase_WaitingForRing,
		eCallPhase_Ringing,
		eCallPhase_InCall,
		eCallPhase_Finished
	};

	void BuildDialogue();

	void EnterRinging();
	void AnswerCall();
	void AdvanceCall(float afTimeStep);
	void PlayLine(const cApartmentCallLine &aLine);
	void FinishCall();

	void DrawSubtitle();
	void DrawWrappedText(const tString &asText, iFontData *apFont, const cVector2f &avFontSize,
						  const cVector3f &avCenterPos, float afMaxWidth, const cColor &aColor,
						  eFontAlign aAlign, bool abGrowUpward);

	cEngine *mpEngine;
	cSomaBase *mpBase;

	cGui *mpGui;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;
	cViewport *mpViewport;

	cVector2f mvScreenSize;

	iFontData *mpSubtitleFont;

	std::vector<cApartmentCallLine> mvLines;

	eCallPhase mPhase;
	float mfTimer; // seconds since construction

	int mlCurrentLineIndex;
	float mfLineFallbackTimer; // counts down mfFallbackHoldTime; a safety net, not the primary gate
	tString msPlayingFile;     // "" once no line's audio is being waited on
	tString msCurrentSpeaker;
	tString msCurrentSubtitle;
};

//----------------------------------------------

#endif // SOMA_APARTMENT_INTRO_CALL_H
