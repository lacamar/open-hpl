/*
 * Phase 0: a minimal debug free-fly (no-clip) camera controller for
 * cBunkerBase's main loop, so a loaded Amnesia: The Bunker map can actually
 * be looked at without any player controller or scripts running. Identical
 * to soma/src/game/DebugFreeCamera.h - see that file's header for why
 * there's no shared no-clip camera class to reuse instead of duplicating
 * this.
 */

#ifndef BUNKER_DEBUG_FREE_CAMERA_H
#define BUNKER_DEBUG_FREE_CAMERA_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cBunkerDebugFreeCamera : public iUpdateable
{
public:
	cBunkerDebugFreeCamera(cCamera *apCamera, cInput *apInput);
	~cBunkerDebugFreeCamera();

	void Update(float afTimeStep);

private:
	cCamera *mpCamera;
	cInput *mpInput;

	float mfMoveSpeed;
	float mfMouseSensitivity;

	bool mbFirstUpdate;
	cVector2l mvLastMousePos;
};

//----------------------------------------------

#endif // BUNKER_DEBUG_FREE_CAMERA_H
