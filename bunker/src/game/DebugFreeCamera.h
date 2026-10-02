
#ifndef BUNKER_DEBUG_FREE_CAMERA_H
#define BUNKER_DEBUG_FREE_CAMERA_H

#include "hpl.h"

using namespace hpl;

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

#endif // BUNKER_DEBUG_FREE_CAMERA_H
