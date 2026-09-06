/*
 * See SomaPlayer.h for scope notes.
 */

#include "SomaPlayer.h"
#include "SomaBase.h"

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
	// ESC pause menu (task 3) - checked even while mbActive is false (i.e.
	// already paused), so a second Escape press can close the menu it just
	// opened. No cAction is bound to Escape (see cSomaBase::
	// CreateInputActions() - only the 5 movement/jump actions exist), so
	// this reads the raw keyboard event queue directly instead, same
	// "drain one distinct press" pattern as cSomaGammaScreen::
	// AnyContinueInputThisFrame(). Routed through cSomaBase::
	// SetGameplayPaused() (which also calls this object's own SetActive())
	// rather than calling SetActive()/the menu directly here - see its
	// comment in SomaBase.h.
	{
		iKeyboard *pKeyboard = mpInput->GetKeyboard();
		if(pKeyboard && pKeyboard->KeyIsPressed() && pKeyboard->GetKey().mKey == eKey_Escape && gpSomaBase)
			gpSomaBase->SetGameplayPaused(gpSomaBase->IsGameplayPaused() == false);
	}

	if(mbActive == false) return;

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
}

//-----------------------------------------------------------------------
