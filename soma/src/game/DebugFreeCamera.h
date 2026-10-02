#ifndef SOMA_DEBUG_FREE_CAMERA_H
#define SOMA_DEBUG_FREE_CAMERA_H

#include "hpl.h"

using namespace hpl;

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

#endif // SOMA_DEBUG_FREE_CAMERA_H
