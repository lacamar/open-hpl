#include "scene/DebugFreeCamera.h"

#include "input/Input.h"
#include "input/Keyboard.h"
#include "input/Mouse.h"
#include "scene/Camera.h"

namespace hpl {

	void cDebugFreeCamera::Update(float afTimeStep)
	{
		if(mpCamera == NULL || mpInput == NULL) return;

		iKeyboard *pKeyboard = mpInput->GetKeyboard();
		iMouse *pMouse = mpInput->GetMouse();
		const float fMouseSensitivity = 0.003f;

		if(pMouse && pMouse->ButtonIsDown(eMouseButton_Right))
		{
			cVector2l vMousePos = pMouse->GetAbsPosition();
			if(mbFirstUpdate == false)
			{
				mpCamera->AddYaw(-(float)(vMousePos.x - mvLastMousePos.x) * fMouseSensitivity);
				mpCamera->AddPitch(-(float)(vMousePos.y - mvLastMousePos.y) * fMouseSensitivity);
			}
			mvLastMousePos = vMousePos;
			mbFirstUpdate = false;
		}
		else
		{
			mbFirstUpdate = true;
		}

		if(pKeyboard)
		{
			float fDist = 3.0f * afTimeStep;
			if(pKeyboard->KeyIsDown(eKey_LeftShift) || pKeyboard->KeyIsDown(eKey_RightShift))
				fDist *= 4.0f;

			if(pKeyboard->KeyIsDown(eKey_W)) mpCamera->MoveForward(fDist);
			if(pKeyboard->KeyIsDown(eKey_S)) mpCamera->MoveForward(-fDist);
			if(pKeyboard->KeyIsDown(eKey_D)) mpCamera->MoveRight(fDist);
			if(pKeyboard->KeyIsDown(eKey_A)) mpCamera->MoveRight(-fDist);
			if(pKeyboard->KeyIsDown(eKey_E)) mpCamera->MoveUp(fDist);
			if(pKeyboard->KeyIsDown(eKey_Q)) mpCamera->MoveUp(-fDist);
		}
	}

}
