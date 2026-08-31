/*
 * Phase 1: a minimal debug free-fly (no-clip) camera controller for
 * cAmfpBase's main loop, so a loaded Machine for Pigs map can actually be
 * looked at without any player controller or scripts running.
 *
 * Identical in shape to soma/src/game/DebugFreeCamera.h - see that file for
 * the rationale (no reusable no-clip camera exists elsewhere in HPL2/core or
 * HPL2/tools). Duplicated rather than shared because amfp/ and soma/ are
 * independent sibling game modules with no shared code of their own, same as
 * amnesia/ and soma/ today.
 */

#ifndef AMFP_DEBUG_FREE_CAMERA_H
#define AMFP_DEBUG_FREE_CAMERA_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cAmfpDebugFreeCamera : public iUpdateable
{
public:
	cAmfpDebugFreeCamera(cCamera *apCamera, cInput *apInput);
	~cAmfpDebugFreeCamera();

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

#endif // AMFP_DEBUG_FREE_CAMERA_H
