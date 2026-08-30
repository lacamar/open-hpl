/*
 * Phase 1: a minimal debug free-fly (no-clip) camera controller for
 * cSomaBase's main loop, so a loaded SOMA map can actually be looked at
 * without any player controller or scripts running.
 *
 * HPL2's cCamera (scene/Camera.h) already implements fly-mode movement
 * (MoveForward/MoveRight/MoveUp honour eCameraMoveMode_Fly, i.e. movement
 * follows the camera's own facing direction rather than staying level) and
 * euler-angle rotation (AddPitch/AddYaw) - there is no separate "no-clip
 * camera" class anywhere in HPL2/core or HPL2/tools to reuse (the level
 * editor's EdCamera in HPL2/tools/NewEditors is Qt-based and not usable
 * from a standalone game loop). This class is just the small amount of
 * input-polling glue on top of cCamera: WASD + Q/E to move, mouse-look
 * while the right mouse button is held, Shift to move faster, Escape to
 * quit.
 */

#ifndef SOMA_DEBUG_FREE_CAMERA_H
#define SOMA_DEBUG_FREE_CAMERA_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cSomaDebugFreeCamera : public iUpdateable
{
public:
	cSomaDebugFreeCamera(cCamera *apCamera, cInput *apInput);
	~cSomaDebugFreeCamera();

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

#endif // SOMA_DEBUG_FREE_CAMERA_H
