/*
 * See SomaApartmentIntroCall.h for scope notes and real-source citations.
 */

#include "SomaApartmentIntroCall.h"
#include "SomaBase.h"

//---------------------------------------

// Real total delay from 00_01_apartment.hps's own OnStart() to the ring
// actually starting: Map_AddTimer("timer_introtext", 1.5, ...) then, once
// that fires, IntroSequence()'s own Map_AddTimer("Timer_PhoneRing", 0.05,
// ...) - see the .h's line-numbered citation.
static const float kRingDelaySecs = 1.5f + 0.05f;

// Real name for the interact point registered on cSomaPlayer (task 2) - not
// a real SOMA entity/area name itself (that's "InteractCellPhone_Dummy",
// see kPhoneWorldPos's own citation below), just this port's own internal
// lookup key.
static const char *kPhoneInteractName = "ApartmentPhone";

// Real WorldPos of the "InteractCellPhone_Dummy" Trigger-type Area (real
// 00_01_apartment.hpm_Area cache, read directly) - the actual entity real
// 00_01_apartment.hps wires the wake-up phone interaction to
// (PlayerInteractCallback="OnInteractCellPhone" -> PhoneInteraction() ->
// AnswerPhone("Simon_Phone",1)). This is NOT the "Simon_Phone" entity itself
// (WorldPos -13.2609 1.12571 7.06526) - that's a separate, later-game
// landline reused for the answering-machine feature (InteractPhone() in the
// same .hps, a different callback entirely) at a different apartment
// location; the actual wake-up interaction point is this small trigger box
// near the ringing cell phone prop ("CellPhone", WorldPos -12.9389 0.7888
// 8.81662, ~0.9m away). This port has no general compiled-Area/OBB reader
// (see SomaLoaders.h) so the trigger's own real box extent/rotation aren't
// reproduced - just its real center point, checked via cSomaPlayer's own
// point+cone+line-of-sight test (see SomaPlayer.cpp).
static const cVector3f kPhoneWorldPos(-12.6146f, 1.17574f, 9.28202f);

// Real config/game.cfg <Game DefaultMaxInteractDistance="2.0" .../> - no
// real Entity_SetMaxInteractionDistance() override exists for this
// particular trigger in 00_01_apartment.hps, so the real engine's own
// default applies.
static const float kPhoneMaxInteractDistance = 2.0f;

// Same reading-tail convention as SomaIntroSequence.cpp's own
// kSubtitleReadingTail - a safety-net fallback only; the real gate is
// cSoundHandler::IsPlaying() below, not this timer (see AdvanceCall()).
static const float kFallbackReadingTail = 0.4f;

// How long the final line's subtitle is held before this object goes quiet.
static const float kEndHoldSecs = 1.0f;

//---------------------------------------

cSomaApartmentIntroCall::cSomaApartmentIntroCall(cEngine *apEngine, cSomaBase *apBase) : iUpdateable("SomaApartmentIntroCall")
{
	mpEngine = apEngine;
	mpBase = apBase;

	mpGui = mpEngine->GetGui();
	mvScreenSize = mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("ApartmentIntroCall", mpGuiSkin);

	// GUI-only overlay viewport on top of the real apartment gameplay
	// viewport (already created by cSomaBase::LoadMap() before this object
	// is constructed - see the wiring there). abPushFront=false puts this
	// at the back of the render list, i.e. drawn LAST/on top - same
	// reasoning as cSomaIntroSequence's own identical choice (see its .cpp
	// comment for the full "front vs back" citation), except here the
	// world underneath is real, live, playable gameplay, not something to
	// paint over - this viewport draws nothing but a small subtitle line,
	// never a background rect.
	mpViewport = mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
	mpViewport->AddGuiSet(mpGuiSet);

	mpSubtitleFont = mpEngine->GetResources()->GetFontManager()->CreateFontData("sansation_medium_bold.fnt");

	BuildDialogue();

	mpRingWorld = NULL;
	mpRing = NULL;
	mlRingCreationID = -1;
	mPhase = eCallPhase_Finished;
	mpViewport->SetActive(false);
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::Restart()
{
	Cancel();

	if (mpBase && mpBase->GetPlayer())
		mpBase->GetPlayer()->RegisterInteractPoint(kPhoneInteractName, kPhoneWorldPos, kPhoneMaxInteractDistance);

	mPhase = eCallPhase_WaitingForRing;
	mfTimer = 0;
	mlCurrentLineIndex = -1;
	mfLineFallbackTimer = 0;
	msPlayingFile = "";
	msCurrentSpeaker = "";
	msCurrentSubtitle = "";
	mpViewport->SetActive(true);
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::Cancel()
{
	StopRing();
	if (msPlayingFile != "")
		mpEngine->GetSound()->GetSoundHandler()->Stop(msPlayingFile);
	msPlayingFile = "";
	mPhase = eCallPhase_Finished;
	mpViewport->SetActive(false);
}

//-----------------------------------------------------------------------

cSomaApartmentIntroCall::~cSomaApartmentIntroCall()
{
	StopRing();
}

//-----------------------------------------------------------------------

// Real content: 00_01_apartment.voice's Subject Name="1_PhoneCall" (11
// lines, Character ID 0 = "Simon" / ID 1 = "Munshi" per that file's own
// <Character> DisplayName attributes) for msText, and the real, plain
// (non-FMOD) .ogg files shipped at lang/eng/voices/00_01_apartment/
// phonecall_1_phonecall_0NN_<player|david>_001.ogg for msFile - confirmed to
// exist and match the .voice text/order 1-for-1 by directly listing that
// directory. mfFallbackHoldTime is each real file's own probed duration
// (ffprobe) plus kFallbackReadingTail - a safety net only, see AdvanceCall().
void cSomaApartmentIntroCall::BuildDialogue()
{
	mvLines.clear();

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_001_player_001.ogg";
	mvLines.back().msSpeaker = "Simon";
	mvLines.back().msText = "Yeah, I'm up!";
	mvLines.back().mfFallbackHoldTime = 0.891667f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_002_david_001.ogg";
	mvLines.back().msSpeaker = "Munshi";
	mvLines.back().msText = "Hi, Simon Jarrett?";
	mvLines.back().mfFallbackHoldTime = 1.318667f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_003_player_001.ogg";
	mvLines.back().msSpeaker = "Simon";
	mvLines.back().msText = "Yeah, that's me.";
	mvLines.back().mfFallbackHoldTime = 0.978125f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_004_david_001.ogg";
	mvLines.back().msSpeaker = "Munshi";
	mvLines.back().msText = "My name is David Munshi. We spoke earlier--";
	mvLines.back().mfFallbackHoldTime = 2.196000f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_005_player_001.ogg";
	mvLines.back().msSpeaker = "Simon";
	mvLines.back().msText = "The brain scan! I remember.";
	mvLines.back().mfFallbackHoldTime = 1.745833f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_006_david_001.ogg";
	mvLines.back().msSpeaker = "Munshi";
	mvLines.back().msText = "Are you all right?";
	mvLines.back().mfFallbackHoldTime = 0.752000f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_007_player_001.ogg";
	mvLines.back().msSpeaker = "Simon";
	mvLines.back().msText = "Yeah, yeah -- just a bad dream. Are we still on for today?";
	mvLines.back().mfFallbackHoldTime = 3.301563f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_008_david_001.ogg";
	mvLines.back().msSpeaker = "Munshi";
	mvLines.back().msText = "Yeah, that's why I'm calling. I wanted to remind you to drink the tracer fluid I sent you. It will help me capture a better image of the damages.";
	mvLines.back().mfFallbackHoldTime = 7.125333f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_009_player_001.ogg";
	mvLines.back().msSpeaker = "Simon";
	mvLines.back().msText = "Don't worry, I got it somewhere.";
	mvLines.back().mfFallbackHoldTime = 1.838542f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_010_david_001.ogg";
	mvLines.back().msSpeaker = "Munshi";
	mvLines.back().msText = "Okay, great. Well, see you in a couple of hours then.";
	mvLines.back().mfFallbackHoldTime = 2.898667f + kFallbackReadingTail;

	mvLines.push_back(cApartmentCallLine());
	mvLines.back().msFile = "phonecall_1_phonecall_011_player_001.ogg";
	mvLines.back().msSpeaker = "Simon";
	mvLines.back().msText = "Okay -- see you soon.";
	mvLines.back().mfFallbackHoldTime = 1.167188f + kFallbackReadingTail;
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::EnterRinging()
{
	mPhase = eCallPhase_Ringing;

	// Entities_Urban/tech/cellphone/vibrating_wood, synthesized by cSomaAmbientSfx
	mpRingWorld = mpBase ? mpBase->GetCurrentWorld() : NULL;
	mpRing = mpRingWorld ? mpRingWorld->CreateSoundEntity("PhoneRing", "vibrating_wood", false) : NULL;
	if (mpRing)
	{
		mlRingCreationID = mpRing->GetCreationID();
		mpRing->SetPosition(kPhoneWorldPos);
	}
	else
	{
		Log("SOMA apartment intro call: ring sound unavailable\n");
	}
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::StopRing()
{
	if (mpRing && mpBase && mpBase->GetCurrentWorld() == mpRingWorld && mpRingWorld->SoundEntityExists(mpRing, mlRingCreationID))
		mpRingWorld->DestroySoundEntity(mpRing);
	mpRing = NULL;
	mpRingWorld = NULL;
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::AnswerCall()
{
	mPhase = eCallPhase_InCall;
	mlCurrentLineIndex = -1;
	msPlayingFile = "";
	mfLineFallbackTimer = 0;

	StopRing();
	mpEngine->GetSound()->GetSoundHandler()->PlayGui("pickup_phone_counter_01.ogg", false, 1.0f);

	Log("SOMA apartment intro call: real interact detected while looking at the phone - "
		"starting real '1_PhoneCall' dialogue\n");
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::PlayLine(const cApartmentCallLine &aLine)
{
	msCurrentSpeaker = aLine.msSpeaker;
	msCurrentSubtitle = aLine.msText;

	mpEngine->GetSound()->GetSoundHandler()->PlayGui(aLine.msFile, false, 1.0f);
	msPlayingFile = aLine.msFile;
	mfLineFallbackTimer = aLine.mfFallbackHoldTime;
}

//-----------------------------------------------------------------------

// Real-audio-completion-gated advance, same pattern used elsewhere in this
// codebase for dialogue playback (cSoundHandler::IsPlaying() polled each
// frame against the currently-playing line's own filename) rather than a
// fixed timer - fixed timers were found to cause overlap/stilted pacing for
// this exact kind of sequential voice-line playback (see this file's own
// header comment). mfLineFallbackTimer is a safety net only, in case a line's
// audio fails to open a channel at all (e.g. a missing/corrupt file) - without
// it, such a failure would silently hang the whole call forever.
void cSomaApartmentIntroCall::AdvanceCall(float afTimeStep)
{
	if (mfLineFallbackTimer > 0.0f)
		mfLineFallbackTimer -= afTimeStep;

	bool bLineDone = false;
	if (msPlayingFile != "")
	{
		bool bStillPlaying = mpEngine->GetSound()->GetSoundHandler()->IsPlaying(msPlayingFile);
		if (bStillPlaying == false || mfLineFallbackTimer <= 0.0f)
			bLineDone = true;
	}
	else
	{
		// Nothing played yet (mlCurrentLineIndex == -1, right after
		// AnswerCall()) - fall through immediately to start the first line.
		bLineDone = true;
	}

	if (bLineDone == false)
		return;

	msPlayingFile = "";
	++mlCurrentLineIndex;

	if (mlCurrentLineIndex < (int)mvLines.size())
	{
		PlayLine(mvLines[mlCurrentLineIndex]);
	}
	else
	{
		// Real HandleTelephoneCall(asSubject, abStartOfSubject=false) marks
		// mbDavidCallDone/writes LastOnSoma_SetText() here - both are state
		// for later-in-map script logic this port doesn't run (no OnStart()
		// execution at all - see SomaLoaders.h), so there is nothing
		// equivalent to port; just end the call.
		msCurrentSubtitle = "";
		mfLineFallbackTimer = kEndHoldSecs;
		mPhase = eCallPhase_Finished;
		Log("SOMA apartment intro call: real '1_PhoneCall' dialogue finished (11/11 lines)\n");
		FinishCall();
	}
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::FinishCall()
{
	mpViewport->SetActive(false);
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::Update(float afTimeStep)
{
	if (mPhase == eCallPhase_Finished)
		return;

	mfTimer += afTimeStep;

	if (mPhase == eCallPhase_WaitingForRing)
	{
		if (mfTimer >= kRingDelaySecs)
			EnterRinging();
	}
	else if (mPhase == eCallPhase_Ringing)
	{
		// Task 2's real fix: no fixed auto-answer timer any more (real
		// 00_01_apartment.hps's own TimerRingTelephone()/AnswerPhone() waits
		// indefinitely for a real player interaction too, matching this) -
		// the ring plays until cSomaPlayer reports a real interact keypress
		// while the player is looking at the real phone interact point (see
		// the constructor's RegisterInteractPoint() call and kPhoneWorldPos's
		// citation above).
		cSomaPlayer *pPlayer = mpBase ? mpBase->GetPlayer() : NULL;
		if (pPlayer && pPlayer->WasInteractedWith(kPhoneInteractName))
			AnswerCall();
	}
	else if (mPhase == eCallPhase_InCall)
	{
		AdvanceCall(afTimeStep);
	}
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::DrawWrappedText(const tString &asText, iFontData *apFont, const cVector2f &avFontSize,
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

void cSomaApartmentIntroCall::DrawSubtitle()
{
	// Same real Sound/ShowSubtitles toggle SomaIntroSequence.cpp's own
	// DrawSubtitle() gates on - see that file's comment.
	if (gpSomaBase && gpSomaBase->GetConfig()->mbShowSubtitles == false)
		return;

	tString sLine;
	if (mPhase == eCallPhase_Ringing)
	{
		sLine = "(phone ringing...)";
	}
	else if (mPhase == eCallPhase_InCall && msCurrentSubtitle != "")
	{
		sLine = msCurrentSpeaker + ": " + msCurrentSubtitle;
	}
	else
	{
		return;
	}

	float fCenterX = mvScreenSize.x * 0.5f;
	float fBottomY = mvScreenSize.y * 0.86f;
	float fMaxWidth = mvScreenSize.x * 0.7f;

	DrawWrappedText(sLine, mpSubtitleFont, cVector2f(26, 26), cVector3f(fCenterX, fBottomY, 5),
					fMaxWidth, cColor(1, 1), eFontAlign_Center, true);
}

//-----------------------------------------------------------------------

void cSomaApartmentIntroCall::OnDraw(float afFrameTime)
{
	if (mPhase == eCallPhase_Finished)
		return;

	DrawSubtitle();
}
