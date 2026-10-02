
#ifndef REBIRTH_DEBUG_FREE_CAMERA_H
#define REBIRTH_DEBUG_FREE_CAMERA_H

#include "hpl.h"

using namespace hpl;

class cRebirthDebugFreeCamera : public iUpdateable
{
public:
	cRebirthDebugFreeCamera(cCamera *apCamera, cInput *apInput);
	~cRebirthDebugFreeCamera();

	void Update(float afTimeStep);

private:
	cCamera *mpCamera;
	cInput *mpInput;

	float mfMoveSpeed;
	float mfMouseSensitivity;

	bool mbFirstUpdate;
	cVector2l mvLastMousePos;
};

#endif // REBIRTH_DEBUG_FREE_CAMERA_H
