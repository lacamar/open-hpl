/*
 * See SomaPlayer.h for scope notes.
 */

#include "SomaPlayer.h"
#include "SomaBase.h"

//---------------------------------------

// Real config/game.cfg <Game DefaultMaxInteractDistance="2.0" .../> - used
// by RegisterInteractPoint() callers as the real default max-look distance
// (see SomaApartmentIntroCall.cpp's own use for the wake-up phone).
static const float kDefaultMaxInteractDistance = 2.0f;

// Not a real script constant - real SOMA's own "am I looking at this" test
// (PlayerLookAtCheckCenterOfScreen) is a compiled-Area/Entity flag this
// engine's world loader doesn't parse (see SomaLoaders.h), so a registered
// interact point here is just a plain world-space point with no known real
// on-screen extent. A generous view-cone half-angle (~32 degrees) keeps a
// point comfortably "hittable" near screen center without requiring the
// player to aim with pixel precision at an invisible point.
static const float kInteractConeCosine = 0.85f;

// Line-of-sight raycast tolerance (see UpdateLookTarget()'s use of
// cSomaInteractRayCallback) - a solid hit strictly closer than the target
// point, by more than this margin, is treated as "blocking the view".
// Without a small margin, the point itself (or geometry it's touching)
// could register as its own blocker due to float precision.
static const float kLineOfSightMargin = 0.1f;

//---------------------------------------

// Modeled on amnesia/src/game/LuxMapHelper.cpp's own
// cLuxLineOfSightCallback (read directly, not guessed) - finds the closest
// real solid hit along a ray, ignoring the player's own character-collider
// bodies (matching that file's apBody->IsCharacter() skip) so
// UpdateLookTarget() below can tell whether something solid stands between
// the camera and a registered interact point.
class cSomaInteractRayCallback : public iPhysicsRayCallback
{
public:
	cSomaInteractRayCallback() : mbHit(false), mfClosestDist(0) {}

	bool BeforeIntersect(iPhysicsBody *apBody)
	{
		return apBody->IsCharacter() == false && apBody->GetCollide();
	}

	bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams)
	{
		if (mbHit == false || apParams->mfDist < mfClosestDist)
		{
			mbHit = true;
			mfClosestDist = apParams->mfDist;
		}
		return true;
	}

	bool mbHit;
	float mfClosestDist;
};

//---------------------------------------

cSomaPlayer::cSomaPlayer(cCamera *apCamera, cInput *apInput) : iUpdateable("SomaPlayer")
{
	mpCamera = apCamera;
	mpInput = apInput;
	mpPhysicsWorld = NULL;
	mpCharBody = NULL;

	// Real values from SOMA's own script/player/Player_Types.hps (gvBodySize /
	// gvBodyCrouchSize consts) - crouch size isn't used for anything yet
	// (no crouch input wired up), but AddExtraSize() is called with it in
	// CreateCharacterBody() to match CreateWorldEntities()'s real shape list
	// in Player.hps, in case future code queries GetActiveSize()/switches to it.
	mvBodySize = cVector3f(0.7f, 2.0f, 0.7f);
	mvBodyCrouchSize = cVector3f(0.7f, 1.2f, 0.7f);

	// LuxInputHandler.cpp's own mouse-to-radians conversion divides by
	// screen height and multiplies by a user sensitivity around 1.0 - this
	// is a fixed, reasonable BASE constant tuned by feel (same idiom as
	// cSomaDebugFreeCamera's own mfMouseSensitivity), separate from the real,
	// live cSomaConfig::mfMouseSensitivity multiplier applied in Update()
	// below (real key "Input"/"MouseSensitivity", real default 1.0 - see
	// SomaConfig.h).
	mfMouseSensitivity = 0.003f;

	mbJumpButtonWasDown = false;
	mbActive = true;

	// v = sqrt(2 * g * h) for a ~0.8m hop under SOMA's real gravity
	// magnitude (12 m/s^2 - see ResetForNewMap() below) - not from a real
	// script constant (SOMA's own gfJumpStartForce=200 is a *force*
	// integrated over gfJumpDuration=0.32s inside MoveState_Normal.hps'
	// UpdateJumping(), a curve this scaffold doesn't reproduce), just a
	// plain instantaneous upward velocity that feels like a jump.
	mfJumpSpeed = 4.5f;

	// Real limits from SOMA's own config/game.cfg <Player CameraPitchLimit_Min/Max>.
	mpCamera->SetPitchLimits(cMath::ToRad(-70.0f), cMath::ToRad(70.0f));

	////////////////////////////////////////
	// Tasks 2/3 - interact-point registry + HUD overlay. gpSomaBase is
	// already valid here (Main.cpp assigns it immediately after
	// construction, long before any LoadMap() call could construct this
	// object - see SomaBase.cpp's own identical assumption for
	// gpSomaBase->GetConfig() in Update() below).
	msCurrentLookTarget = "";
	mbInteractKeyPressedThisFrame = false;

	cEngine *pEngine = gpSomaBase->mpEngine;
	mpGui = pEngine->GetGui();
	mpGuiSkin = mpGui->CreateSkin("gui_default.skin");
	mpGuiSet = mpGui->CreateSet("PlayerHud", mpGuiSkin);

	// GUI-only overlay viewport on top of the real gameplay viewport -
	// abPushFront=false puts this at the back of the render list (drawn
	// LAST/on top), same reasoning/citation as cSomaApartmentIntroCall's own
	// identical subtitle overlay viewport (see its .cpp).
	mpHudViewport = pEngine->GetScene()->CreateViewport(NULL, NULL, false);
	mpHudViewport->AddGuiSet(mpGuiSet);

	mpCrosshairGfx = mpGui->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);
	mpPromptFont = pEngine->GetResources()->GetFontManager()->CreateFontData("sansation_medium_bold.fnt");
}

//-----------------------------------------------------------------------

cSomaPlayer::~cSomaPlayer()
{
	DestroyCharacterBody();
}

//-----------------------------------------------------------------------

void cSomaPlayer::DestroyCharacterBody()
{
	// mpCharBody is owned by mpPhysicsWorld (in turn owned by the cWorld
	// SomaBase::LoadMap() is about to destroy) - destroy it explicitly
	// before that world goes away, since iPhysicsWorld has no "destroy
	// everything" call SomaBase relies on elsewhere.
	if(mpCharBody && mpPhysicsWorld)
		mpPhysicsWorld->DestroyCharacterBody(mpCharBody);
	mpCharBody = NULL;
}

//-----------------------------------------------------------------------

void cSomaPlayer::ResetForNewMap(iPhysicsWorld *apPhysicsWorld, const cVector3f &avFeetPos, float afYawRad)
{
	// NOTE: does NOT call DestroyCharacterBody() here - the caller (see
	// SomaBase::LoadMap()) must already have done so, before destroying the
	// old world/physics world this body belonged to. See the DestroyCharacterBody()
	// declaration in SomaPlayer.h for why (a real use-after-free otherwise).
	mpPhysicsWorld = apPhysicsWorld;
	CreateCharacterBody();

	// NOTE: deliberately no StopMovement() call here (unlike LuxPlayer::
	// PlaceAtStartNode(), which this was originally modeled on) - the body
	// was JUST created fresh above, so there is no residual movement to
	// clear, and StopMovement() has a real gotcha: it zeroes mfMoveAcc/
	// mfMoveDeacc (see CharacterBody.cpp) - the persistent per-body
	// ACCELERATION-RATE constants set by CreateCharacterBody() below, not
	// just the transient move state its name implies. That's harmless for
	// Dark Descent's own cLuxPlayer only because its move-state machine
	// (LuxMoveState.cpp's OnEnterState()) unconditionally re-applies
	// SetMoveAcc()/SetMoveDeacc() every time a move state activates, so the
	// wipe self-heals immediately - this scaffold has no such state machine,
	// so calling StopMovement() here left mfMoveAcc permanently 0 forever
	// (found live: WASD registered as pressed - confirmed via a real
	// input_debug headless probe - and iCharacterBody::Update() ran every
	// frame - confirmed via working gravity/landing - yet a direct
	// synchronous 60x Move()+Update() headless probe still produced exactly
	// zero displacement; GetMoveAcc(eCharDir_Forward) read back as 0 instead
	// of the expected default of 20, which is what led here).
	mpCharBody->SetFeetPosition(avFeetPos);
	mpCharBody->SetYaw(afYawRad);
	mpCharBody->Update(0.001f);

	mpCamera->SetYaw(afYawRad);
	mpCamera->SetPitch(0);

	mbJumpButtonWasDown = false;
}

//-----------------------------------------------------------------------

void cSomaPlayer::CreateCharacterBody()
{
	mpCharBody = mpPhysicsWorld->CreateCharacterBody("Player", mvBodySize);

	// Real gravity vector from script/player/Player_Types.hps' mvGravity
	// default (cVector3f(0,-12,0) - see Player.hps' member init) and real
	// mass from gfPlayer_BodyDefaultMass=70 (both in Player_Types.hps).
	mpCharBody->SetCustomGravity(cVector3f(0, -12.0f, 0));
	mpCharBody->SetCustomGravityActive(true);
	mpCharBody->SetGravityActive(true);
	mpCharBody->SetMass(70.0f);

	// Real values from Player.hps' SetCharacterBodyDefaults().
	mpCharBody->SetAccurateClimbing(true);
	mpCharBody->SetMaxNoSlideSlopeAngle(cMath::ToRad(46.0f));
	mpCharBody->SetMaxPushMass(10.0f);
	mpCharBody->SetCharacterMaxPushMass(10.0f);
	mpCharBody->SetCharacterPushForce(100.0f);
	mpCharBody->SetMaxStepSize(0.4f);
	mpCharBody->SetMaxStepSizeInAir(0.1f);
	mpCharBody->SetStepClimbSpeed(3.5f);
	mpCharBody->SetStickToSlope(false);
	mpCharBody->SetDeaccelerateMoveSpeedInAir(false);

	// Real per-axis walk speeds from MoveState_Normal.hps' OnEnterState():
	// mfMaxForwardSpeed=2.5, mfMaxBackwardSpeed=2, mfMaxSidwaySpeed=2.25.
	// iCharacterBody's own eCharDir only has Forward/Right (no separate
	// backward axis - negative Forward speed *is* backward), so the negative
	// forward limit uses the real backward constant.
	mpCharBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, 2.5f);
	mpCharBody->SetMaxNegativeMoveSpeed(eCharDir_Forward, -2.0f);
	mpCharBody->SetMaxPositiveMoveSpeed(eCharDir_Right, 2.25f);
	mpCharBody->SetMaxNegativeMoveSpeed(eCharDir_Right, -2.25f);

	mpCharBody->SetCamera(mpCamera);
	// Real value straight from Player.hps' SetBaseCameraPosAdd(cVector3f(0,-0.1f,0)).
	// iCharacterBody::UpdateCamera() already places the un-adjusted camera
	// position at feet + the primary shape's own height (i.e. the top of the
	// collision capsule, see CharacterBody.cpp) *before* adding
	// CameraPosAdd - so this is a small downward nudge from the very top of
	// the body, not an additional "add the body height" term. Getting this
	// wrong (e.g. re-adding mvBodySize.y here) would put the camera almost a
	// full body-height too high.
	mpCharBody->SetCameraPosAdd(cVector3f(0, -0.1f, 0));
	mpCharBody->SetCameraSmoothPosNum(10);

	// Real extra shapes from Player.hps' CreateWorldEntities() (crouch and
	// climb sizes) - only the crouch one is reachable right now (no crouch
	// input), the climb/special sizes are added for parity but unused.
	mpCharBody->AddExtraSize(mvBodyCrouchSize);
	mpCharBody->AddExtraSize(cVector3f(mvBodySize.x*0.7f, mvBodySize.y*1.1f, mvBodySize.z*0.7f));
	mpCharBody->AddExtraSize(cVector3f(0.7f));
}

//-----------------------------------------------------------------------

void cSomaPlayer::Update(float afTimeStep)
{
	if(mpCharBody == NULL || mpInput == NULL) return;

	//////////////////////////
	// ESC pause menu (task 3) + real interact key (task 2/3) - both checked
	// even while mbActive is false (i.e. already paused), so a second
	// Escape press can close the menu it just opened. Neither has a real
	// cAction bound (see cSomaBase::CreateInputActions() - only the 5
	// movement/jump actions exist), so this reads the raw keyboard event
	// queue directly instead, same "drain one distinct press" pattern as
	// cSomaGammaScreen::AnyContinueInputThisFrame(). Both checks share ONE
	// drain loop rather than two separate KeyIsPressed()+GetKey() calls -
	// iKeyboard::GetKey() (see KeyboardSDL.cpp) pops the front of a real
	// FIFO queue every call, so two independent single-pop checks in the
	// same frame could each consume a *different* queued key and neither
	// would see the other's press. Draining the whole queue once here
	// checks every key actually pressed this frame against both actions.
	//
	// Escape is routed through cSomaBase::SetGameplayPaused() (which also
	// calls this object's own SetActive()) rather than calling
	// SetActive()/the menu directly here - see its comment in SomaBase.h.
	// The interact key (E - a real default per this task's own guidance;
	// this scaffold has no rebindable interact cAction, see SomaPlayer.h's
	// own scope note) is only latched into mbInteractKeyPressedThisFrame
	// here; WasInteractedWith() (checked later, once UpdateLookTarget() has
	// run for this frame) is what actually turns it into an edge-triggered
	// "did the player just interact with X" answer.
	mbInteractKeyPressedThisFrame = false;
	{
		iKeyboard *pKeyboard = mpInput->GetKeyboard();
		while(pKeyboard && pKeyboard->KeyIsPressed())
		{
			eKey key = pKeyboard->GetKey().mKey;

			if(key == eKey_Escape && gpSomaBase)
				gpSomaBase->SetGameplayPaused(gpSomaBase->IsGameplayPaused() == false);
			else if(key == eKey_E)
				mbInteractKeyPressedThisFrame = true;
		}
	}

	if(mbActive == false)
	{
		// No fresh look-target while paused/inactive - avoid reporting a
		// stale target from the last active frame (see WasInteractedWith()).
		msCurrentLookTarget = "";
		return;
	}

	// Real Horizontal FOV/MouseSensitivity/InvertMouseY settings - all three
	// are live, so just re-read the config every frame rather than caching a
	// stale copy (cCamera::SetFOV() is a cheap early-return-if-unchanged
	// call, see Camera.cpp - no cost to calling it unconditionally here).
	cSomaConfig *pCfg = gpSomaBase ? gpSomaBase->GetConfig() : NULL;
	if(pCfg)
		mpCamera->SetFOV(cMath::ToRad(pCfg->mfFOV));

	iMouse *pMouse = mpInput->GetMouse();

	//////////////////////////
	// Mouse look - always active (unlike cSomaDebugFreeCamera's right-button
	// hold, which exists purely so a free-fly debug session doesn't fight
	// the desktop's cursor; a real player controller should look like a
	// real FPS). GetRelPosition() already reports the frame's raw motion
	// delta (see LuxInputHandler.cpp's identical use, no relative-mouse-mode
	// toggle exists anywhere in this codebase's SDL input layer).
	if(pMouse)
	{
		float fSensitivity = mfMouseSensitivity * (pCfg ? pCfg->mfMouseSensitivity : 1.0f);
		float fInvert = (pCfg && pCfg->mbInvertMouseY) ? 1.0f : -1.0f;

		cVector2l vRel = pMouse->GetRelPosition();
		mpCamera->AddYaw(-(float)vRel.x * fSensitivity);
		mpCamera->AddPitch((float)vRel.y * fSensitivity * fInvert);

		// Character body yaw must track the camera's yaw every frame (same
		// as LuxPlayer::AddYaw() does) so Move()'s Forward/Right axes stay
		// aligned with where the player is actually looking.
		mpCharBody->SetYaw(mpCamera->GetYaw());
	}

	//////////////////////////
	// WASD movement, relative to the body's own (camera-synced) facing.
	// Real HPL2 cAction/cInput system (HPL2/core/include/input/Action.h) -
	// keys are looked up by name every call rather than hardcoded eKey_W/S/
	// A/D checks, so a real rebind from SomaMainMenu.cpp's KEYBINDINGS
	// screen (via cSomaBase::RebindPlayerAction()) takes effect immediately,
	// same as every other live Options setting. The actions themselves are
	// created once by cSomaBase::CreateInputActions() (called from
	// InitEngine(), before any player/menu exists), not here - this scaffold
	// intentionally has no gamepad support to fall back to.
	{
		if(mpInput->IsTriggerd(cSomaBase::GetPlayerActionName(cSomaBase::eSomaPlayerAction_Forward))) mpCharBody->Move(eCharDir_Forward, 1);
		if(mpInput->IsTriggerd(cSomaBase::GetPlayerActionName(cSomaBase::eSomaPlayerAction_Backward))) mpCharBody->Move(eCharDir_Forward, -1);
		if(mpInput->IsTriggerd(cSomaBase::GetPlayerActionName(cSomaBase::eSomaPlayerAction_Right))) mpCharBody->Move(eCharDir_Right, 1);
		if(mpInput->IsTriggerd(cSomaBase::GetPlayerActionName(cSomaBase::eSomaPlayerAction_Left))) mpCharBody->Move(eCharDir_Right, -1);

		//////////////////////////
		// Jump - a simple instantaneous upward velocity on the down-stroke,
		// only while grounded (real SOMA's own jump additionally blocks on
		// crouch/underwater/a fatigue timer - see MoveState_Normal.hps'
		// Jump() - none of which exist in this scaffold yet).
		bool bJumpDown = mpInput->IsTriggerd(cSomaBase::GetPlayerActionName(cSomaBase::eSomaPlayerAction_Jump));
		if(bJumpDown && mbJumpButtonWasDown == false && mpCharBody->IsOnGround())
		{
			mpCharBody->AddForceVelocity(cVector3f(0, mfJumpSpeed, 0));
		}
		mbJumpButtonWasDown = bJumpDown;
	}

	mpCharBody->Update(afTimeStep);

	// Run after mpCharBody->Update() so mpCamera's position (driven by the
	// character body's own SetCamera()/CameraPosAdd - see CreateCharacterBody())
	// is this frame's up-to-date eye position, not last frame's.
	UpdateLookTarget();
}

//-----------------------------------------------------------------------

void cSomaPlayer::RegisterInteractPoint(const tString &asName, const cVector3f &avWorldPos, float afMaxDistance)
{
	// Re-registering an existing name (e.g. a map reload) replaces it rather
	// than accumulating duplicates.
	for(size_t i = 0; i < mvInteractPoints.size(); ++i)
	{
		if(mvInteractPoints[i].msName == asName)
		{
			mvInteractPoints[i].mvWorldPos = avWorldPos;
			mvInteractPoints[i].mfMaxDistance = afMaxDistance;
			return;
		}
	}

	cSomaInteractPoint point;
	point.msName = asName;
	point.mvWorldPos = avWorldPos;
	point.mfMaxDistance = afMaxDistance;
	mvInteractPoints.push_back(point);
}

//-----------------------------------------------------------------------

bool cSomaPlayer::WasInteractedWith(const tString &asName) const
{
	return mbInteractKeyPressedThisFrame && msCurrentLookTarget == asName;
}

//-----------------------------------------------------------------------

// See SomaPlayer.h's RegisterInteractPoint() doc comment and
// cSomaInteractRayCallback above for the real citations this leans on.
void cSomaPlayer::UpdateLookTarget()
{
	msCurrentLookTarget = "";

	if(mvInteractPoints.empty() || mpCharBody == NULL)
		return;

	cVector3f vCamPos = mpCamera->GetPosition();
	cVector3f vFwd = mpCamera->GetForward();

	tString sClosestName = "";
	float fClosestDist = 0;

	for(size_t i = 0; i < mvInteractPoints.size(); ++i)
	{
		const cSomaInteractPoint &point = mvInteractPoints[i];

		cVector3f vToPoint = point.mvWorldPos - vCamPos;
		float fDist = vToPoint.Length();
		if(fDist > point.mfMaxDistance || fDist < 0.0001f)
			continue;

		cVector3f vDir = vToPoint / fDist;
		float fDot = cMath::Vector3Dot(vFwd, vDir);
		if(fDot < kInteractConeCosine)
			continue;

		if(mpPhysicsWorld)
		{
			cSomaInteractRayCallback rayCallback;
			mpPhysicsWorld->CastRay(&rayCallback, vCamPos, point.mvWorldPos, true, false, false, true);
			if(rayCallback.mbHit && rayCallback.mfClosestDist < fDist - kLineOfSightMargin)
				continue; // something solid stands between the camera and the point
		}

		if(sClosestName == "" || fDist < fClosestDist)
		{
			sClosestName = point.msName;
			fClosestDist = fDist;
		}
	}

	msCurrentLookTarget = sClosestName;
}

//-----------------------------------------------------------------------

// Task 3 - a real crosshair, shown during normal gameplay only (not while
// paused/inactive, matching the real game's own HUD hiding for its pause
// menu). Screen size is queried fresh here rather than cached at
// construction - this object (unlike the short-lived splash/gamma screens)
// lives for the rest of the process, so a cached size would go stale across
// a real window resize (see SomaMainMenu.cpp's own equivalent fix, cited in
// PORTING_NOTES.md's "screen-size staleness" entry).
void cSomaPlayer::DrawCrosshair()
{
	cVector2f vScreenSize = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	cVector2f vCenter = vScreenSize * 0.5f;

	const float fLength = 8.0f;
	const float fThickness = 2.0f;
	cColor col(1, 1, 1, 0.85f);

	// Horizontal bar, then vertical bar - together a real "+" reticle.
	mpGuiSet->DrawGfx(mpCrosshairGfx,
					   cVector3f(vCenter.x - fLength, vCenter.y - fThickness * 0.5f, 10),
					   cVector2f(fLength * 2.0f, fThickness), col);
	mpGuiSet->DrawGfx(mpCrosshairGfx,
					   cVector3f(vCenter.x - fThickness * 0.5f, vCenter.y - fLength, 10),
					   cVector2f(fThickness, fLength * 2.0f), col);
}

//-----------------------------------------------------------------------

// Task 3 - real interact hint, shown only while actually looking at a
// registered interactable (reuses task 2's own UpdateLookTarget() - one
// shared "what am I looking at" system, not two separate ones, per this
// task's own guidance). Mirrors real base_english.lang's
// CATEGORY="MainMenu"/Entry="Interact" wording ("Interact") rather than
// inventing new copy.
void cSomaPlayer::DrawInteractPrompt()
{
	if(msCurrentLookTarget == "")
		return;

	cVector2f vScreenSize = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

	tString sPrompt = "[E] Interact";
	cVector3f vPos(vScreenSize.x * 0.5f, vScreenSize.y * 0.5f + 28.0f, 10);

	mpGuiSet->DrawFont(cString::To16Char(sPrompt), mpPromptFont, vPos,
						cVector2f(20, 20), cColor(1, 1), eFontAlign_Center);
}

//-----------------------------------------------------------------------

void cSomaPlayer::OnDraw(float afFrameTime)
{
	// No HUD while paused/inactive (SetActive(false)) - matches the real
	// game hiding its own HUD behind the pause menu, and keeps this from
	// drawing over cSomaIntroSequence's opaque slideshow (which also drives
	// SetActive(false) - see SomaBase.cpp's LoadMap()) or cSomaMainMenu's
	// paused-menu overlay.
	if(mbActive == false)
		return;

	DrawCrosshair();
	DrawInteractPrompt();
}

//-----------------------------------------------------------------------
