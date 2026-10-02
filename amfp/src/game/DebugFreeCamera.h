
#ifndef AMFP_DEBUG_FREE_CAMERA_H
#define AMFP_DEBUG_FREE_CAMERA_H

#include "hpl.h"

using namespace hpl;

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

#endif // AMFP_DEBUG_FREE_CAMERA_H
