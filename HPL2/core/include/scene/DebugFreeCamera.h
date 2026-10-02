#ifndef HPL_DEBUG_FREE_CAMERA_H
#define HPL_DEBUG_FREE_CAMERA_H

#include "engine/Updateable.h"
#include "math/MathTypes.h"

namespace hpl {

	class cCamera;
	class cInput;

	class cDebugFreeCamera : public iUpdateable
	{
	public:
		cDebugFreeCamera(cCamera *apCamera, cInput *apInput) : iUpdateable("DebugFreeCamera"), mpCamera(apCamera), mpInput(apInput) {}

		void Update(float afTimeStep);

	private:
		cCamera *mpCamera;
		cInput *mpInput;
		bool mbFirstUpdate = true;
		cVector2l mvLastMousePos = cVector2l(0, 0);
	};

}

#endif // HPL_DEBUG_FREE_CAMERA_H
