/*
 * See DebugFreeCamera.h for scope notes.
 */

#include "DebugFreeCamera.h"

//---------------------------------------

cRebirthDebugFreeCamera::cRebirthDebugFreeCamera(cCamera *apCamera, cInput *apInput) : iUpdateable("RebirthDebugFreeCamera")
{
	mpCamera = apCamera;
	mpInput = apInput;

	mfMoveSpeed = 3.0f; // m/s, x4 while holding shift
	mfMouseSensitivity = 0.003f;

	mbFirstUpdate = true;
	mvLastMousePos = cVector2l(0, 0);
}

//-----------------------------------------------------------------------

cRebirthDebugFreeCamera::~cRebirthDebugFreeCamera()
{
}

//-----------------------------------------------------------------------

void cRebirthDebugFreeCamera::Update(float afTimeStep)
{
	if (mpCamera == NULL || mpInput == NULL) return;

	iKeyboard *pKeyboard = mpInput->GetKeyboard();
	iMouse *pMouse = mpInput->GetMouse();

	//////////////////////////
	// Mouse look, only while right mouse button is held (so the cursor is
	// still free to use for anything else, and so this doesn't fight with
	// a window manager / desktop that owns the pointer).
	if (pMouse && pMouse->ButtonIsDown(eMouseButton_Right))
	{
		cVector2l vMousePos = pMouse->GetAbsPosition();

		if (mbFirstUpdate == false)
		{
			float fDX = (float)(vMousePos.x - mvLastMousePos.x);
			float fDY = (float)(vMousePos.y - mvLastMousePos.y);

			mpCamera->AddYaw(-fDX * mfMouseSensitivity);
			mpCamera->AddPitch(-fDY * mfMouseSensitivity);
		}

		mvLastMousePos = vMousePos;
		mbFirstUpdate = false;
	}
	else
	{
		mbFirstUpdate = true;
	}

	//////////////////////////
	// WASD + Q/E movement, Shift to move faster.
	if (pKeyboard)
	{
		float fSpeed = mfMoveSpeed;
		if (pKeyboard->KeyIsDown(eKey_LeftShift) || pKeyboard->KeyIsDown(eKey_RightShift))
			fSpeed *= 4.0f;

		float fDist = fSpeed * afTimeStep;

		if (pKeyboard->KeyIsDown(eKey_W)) mpCamera->MoveForward(fDist);
		if (pKeyboard->KeyIsDown(eKey_S)) mpCamera->MoveForward(-fDist);
		if (pKeyboard->KeyIsDown(eKey_D)) mpCamera->MoveRight(fDist);
		if (pKeyboard->KeyIsDown(eKey_A)) mpCamera->MoveRight(-fDist);
		if (pKeyboard->KeyIsDown(eKey_E)) mpCamera->MoveUp(fDist);
		if (pKeyboard->KeyIsDown(eKey_Q)) mpCamera->MoveUp(-fDist);
	}
}

//-----------------------------------------------------------------------
