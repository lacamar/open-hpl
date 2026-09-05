/*
 * A real physics-based player controller for SOMA game maps, replacing the
 * free-fly debug camera (see DebugFreeCamera.h) once a real game map is
 * loaded (main menu scenes keep using the free-fly camera - see SomaBase.cpp).
 *
 * Modeled on amnesia/src/game/LuxPlayer.cpp's CreateCharacterBody()/Move()/
 * AddYaw()/AddPitch()/Jump() - NOT copied verbatim, since Dark Descent's
 * version is wrapped in a large player-state-machine (crouch/lean/insanity/
 * interact states) this scaffold doesn't have. This is the minimum slice:
 * a real iCharacterBody (HPL2/core/include/physics/CharacterBody.h) for
 * gravity/collision/ground detection, WASD movement relative to camera
 * facing, mouse-look, and a simple jump - no crouch, lean, or state machine.
 *
 * Real body/movement constants (size, mass, gravity, camera offset, walk
 * speeds, jump force) are reverse-engineered from SOMA's actual script
 * source, not guessed - see the .cpp file's comments for exact file/line
 * citations (script/player/Player_Types.hps, Player.hps,
 * MoveState_Normal.hps, config/game.cfg).
 */

#ifndef SOMA_PLAYER_H
#define SOMA_PLAYER_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cSomaPlayer : public iUpdateable
{
public:
	// apCamera/apInput must outlive this object (owned by the caller, same
	// convention as cSomaDebugFreeCamera). No character body is created yet
	// here - call ResetForNewMap() once a world/physics world is available.
	cSomaPlayer(cCamera *apCamera, cInput *apInput);
	~cSomaPlayer();

	// Must be called BEFORE the cWorld owning the current character body's
	// physics world is destroyed (see SomaBase::LoadMap(), which calls this
	// right before cScene::DestroyWorld()) - destroying the body is only
	// safe while its iPhysicsWorld is still alive. A no-op the first time
	// (no body created yet). ResetForNewMap() below does NOT do this itself
	// for exactly that reason: by the time it runs, LoadMap() has already
	// destroyed the old world, so the old mpPhysicsWorld pointer this class
	// was holding is already dangling - calling iPhysicsWorld::
	// DestroyCharacterBody() on it then is a real use-after-free (caught
	// live via a real SIGSEGV/coredumpctl backtrace during verification,
	// not just reasoned about - see PORTING_NOTES.md).
	void DestroyCharacterBody();

	// cUpdater (see SomaBase.cpp's own comments on this) has no "remove"
	// counterpart to AddGlobalUpdate() - so, like cSomaDebugFreeCamera, one
	// cSomaPlayer instance is created once and reused for every subsequent
	// SomaBase::LoadMap() call rather than replaced. A real iCharacterBody
	// belongs to one specific iPhysicsWorld though (destroyed along with the
	// old cWorld on every map load), so this destroys the old one (if any)
	// and creates a fresh one in the new world/position - everything else
	// (camera/input pointers, tuning constants) is unaffected. avFeetPos/
	// afYawRad are the real PlayerStart Area's position/yaw.
	void ResetForNewMap(iPhysicsWorld *apPhysicsWorld, const cVector3f &avFeetPos, float afYawRad);

	void Update(float afTimeStep);

	iCharacterBody* GetCharacterBody(){ return mpCharBody; }

	// Gates Update() - used by cSomaIntroSequence (see SomaIntroSequence.h)
	// to freeze input/movement while its non-interactive 2D slideshow plays
	// over the (currently unlit/black) 00_00_intro.hpm scene, matching the
	// real script's own Player_SetActive(false) in OnEnter(). Defaults to
	// true; does not touch the character body itself (gravity/collision
	// still apply, only WASD/mouse-look/jump input is ignored while false).
	void SetActive(bool abActive){ mbActive = abActive; }
	bool IsActive() const { return mbActive; }

private:
	void CreateCharacterBody();

	cCamera *mpCamera;
	cInput *mpInput;
	iPhysicsWorld *mpPhysicsWorld;

	iCharacterBody *mpCharBody;

	cVector3f mvBodySize;
	cVector3f mvBodyCrouchSize;

	float mfMouseSensitivity;

	bool mbJumpButtonWasDown;
	float mfJumpSpeed;

	bool mbActive;
};

//----------------------------------------------

#endif // SOMA_PLAYER_H
