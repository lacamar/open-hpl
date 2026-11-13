/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "scene/Camera.h"

#include "system/LowLevelSystem.h"

#include "graphics/LowLevelGraphics.h"

#include "math/Math.h"
#include "scene/RustNode.h"

#include "scene/Entity3D.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cCamera::cCamera()
	{
		mS.mvPosition = cVector3f(0);

		mS.mfFOV = cMath::ToRad(70.0f);
		mS.mfAspect = 4.0f/3.0f;
		mS.mfFarClipPlane = 1000.0f;
		mS.mfNearClipPlane = 0.05f;

		mS.mvViewSize = cVector2f(2 * mS.mfAspect, 2);

		mS.mlProjectionType = eProjectionType_Perspective;

		mS.mfPitch = 0;
		mS.mfYaw = 0;
		mS.mfRoll = 0;

		mS.m_mtxMatrixRotation = cMatrixf::Identity;

		mS.mlRotateMode = eCameraRotateMode_EulerAngles;
		mS.mlMoveMode = eCameraMoveMode_Fly;

		mS.m_mtxView = cMatrixf::Identity;
		mS.m_mtxProjection = cMatrixf::Identity;
		mS.m_mtxMove = cMatrixf::Identity;

		mS.mbViewUpdated = true;
		mS.mbProjectionUpdated = true;
		mS.mbMoveUpdated = true;
		mS.mbFrustumUpdated = true;

		mS.mbInfFarPlane = false;

		mS.mfPitchLimitMin = -kPi2f;
		mS.mfPitchLimitMax = kPi2f;

		mS.mfYawLimitMin =0;
		mS.mfYawLimitMax =0;
		
	}

	//-----------------------------------------------------------------------

	cCamera::~cCamera()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cCamera::SetPosition(const cVector3f &avPos)
	{
		ohpl_h10_cam_set_position(&mS, &avPos); mNode.SetPosition(mS.mvPosition);
	}

	//-----------------------------------------------------------------------

	void cCamera::SetPitch(float afAngle)
	{
		ohpl_h10_cam_set_pitch(&mS, afAngle);
	}
	void cCamera::SetYaw(float afAngle)
	{
		ohpl_h10_cam_set_yaw(&mS, afAngle);
	}
	void cCamera::SetRoll(float afAngle)
	{
		ohpl_h10_cam_set_roll(&mS, afAngle);
	}
	
	//-----------------------------------------------------------------------

	void cCamera::AddPitch(float afAngle)
	{
		ohpl_h10_cam_add_pitch(&mS, afAngle);
}	
	void cCamera::AddYaw(float afAngle)
	{
		ohpl_h10_cam_add_yaw(&mS, afAngle);
	}
	void cCamera::AddRoll(float afAngle)
	{
		ohpl_h10_cam_add_roll(&mS, afAngle);
	}

	//-----------------------------------------------------------------------

	void cCamera::MoveForward(float afDist)
	{
		ohpl_h10_cam_move(&mS, 0, afDist); mNode.SetPosition(mS.mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera::MoveRight(float afDist)
	{
		ohpl_h10_cam_move(&mS, 1, afDist); mNode.SetPosition(mS.mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera::MoveUp(float afDist)
	{
		ohpl_h10_cam_move(&mS, 2, afDist); mNode.SetPosition(mS.mvPosition);
	}

	//-----------------------------------------------------------------------

	void cCamera::SetFOV(float afAngle)
	{
		ohpl_h10_cam_set_fov(&mS, afAngle);
	}

	//-----------------------------------------------------------------------

	void cCamera::SetAspect(float afAspect)
	{
		ohpl_h10_cam_set_aspect(&mS, afAspect);
	}

	void cCamera::SetFarClipPlane(float afX)
	{
		ohpl_h10_cam_set_far(&mS, afX);
	}

	void cCamera::SetNearClipPlane(float afX)
	{
		ohpl_h10_cam_set_near(&mS, afX);
	}

	//-----------------------------------------------------------------------

	void cCamera::SetProjectionType(eProjectionType aType)
	{
		ohpl_h10_cam_set_projection_type(&mS, (int)aType);
	}
	
	void cCamera::SetOrthoViewSize(const cVector2f &avSize)
	{
		ohpl_h10_cam_set_ortho_view_size(&mS, &avSize);
	}
	
	//-----------------------------------------------------------------------

	void cCamera::SetRotateMode(eCameraRotateMode aMode)
	{
		ohpl_h10_cam_set_rotate_mode(&mS, (int)aMode);
	}	
	
	//-----------------------------------------------------------------------

	void cCamera::SetMoveMode(eCameraMoveMode aMode)
	{
		ohpl_h10_cam_set_move_mode(&mS, (int)aMode);
	}

	//-----------------------------------------------------------------------

	void cCamera::ResetRotation()
	{
		ohpl_h10_cam_reset_rotation(&mS);
	}

	//-----------------------------------------------------------------------

	cFrustum* cCamera::GetFrustum()
	{
		if(mS.mbFrustumUpdated)
		{
			//If the far plane is infinite, still have to use a number on far plane
			//to calculate the near plane in the frustm.
			bool bWasInf = (ohpl_h10_cam_frustum_begin(&mS) & 2) != 0;
			//////////////////////////////////////////////
			// Perspective projection
			if(mS.mlProjectionType == eProjectionType_Perspective)
			{
				mFrustum.SetupPerspectiveProj(	GetProjectionMatrix(),
												GetViewMatrix(),
												GetFarClipPlane(),GetNearClipPlane(),
												GetFOV(),GetAspect(),GetPosition(),mS.mbInfFarPlane);
			}
			//////////////////////////////////////////////
			// Orthographic projection
			else
			{
				mFrustum.SetupOrthoProj(	GetProjectionMatrix(),
											GetViewMatrix(),
											GetFarClipPlane(),GetNearClipPlane(),
											mS.mvViewSize,GetPosition(),mS.mbInfFarPlane);
			}
			
			ohpl_h10_cam_frustum_end(&mS, bWasInf);
		}

		return &mFrustum;
	}

	//-----------------------------------------------------------------------
	
	const cMatrixf& cCamera::GetViewMatrix()
	{
		return *ohpl_h10_cam_matrix(&mS, 0);
	}
	
	//-----------------------------------------------------------------------

	const cMatrixf& cCamera::GetProjectionMatrix()
	{
		return *ohpl_h10_cam_matrix(&mS, 1);
	}
	
	//-----------------------------------------------------------------------

	const cMatrixf& cCamera::GetMoveMatrix()
	{
		return *ohpl_h10_cam_matrix(&mS, 2);
	}


	//-----------------------------------------------------------------------

	cVector3f cCamera::GetEyePosition()
	{
		return mS.mvPosition;
	}

	//-----------------------------------------------------------------------

	void cCamera::SetModelViewMatrix(iLowLevelGraphics* apLowLevel)
	{

	}

	//-----------------------------------------------------------------------

	void cCamera::SetProjectionMatrix(iLowLevelGraphics* apLowLevel)
	{

	}
	
	//-----------------------------------------------------------------------

	void cCamera::UnProject(cVector3f *apPosition, cVector3f *apDirection, const cVector2f& avScreenPos, const cVector2f& avVirtualScreenSize)
	{
		UnProjectHelper(apPosition, apDirection, avScreenPos, avVirtualScreenSize, 
						GetViewMatrix().GetRotation(),
						GetPosition(), GetForward(), GetUp(), GetRight());
	}

	void cCamera::UnProjectHelper(cVector3f *apPosition, cVector3f *apDirection, const cVector2f& avScreenPos, const cVector2f& avVirtualScreenSize,
								  const cMatrixf& amtxCameraRotation,
								  const cVector3f& avCameraPos, const cVector3f& avCameraFwd, const cVector3f& avCameraUp, const cVector3f& avCameraRgt)
	{
		cVector3f vPos, vDir;
		if(ohpl_scene_camera_unproject((int)mS.mlProjectionType, mS.mfNearClipPlane, mS.mfFOV, mS.mfAspect, &mS.mvViewSize, &avScreenPos, &avVirtualScreenSize,
									   &amtxCameraRotation, &avCameraPos, &avCameraFwd, &avCameraUp, &avCameraRgt, &vPos, &vDir))
		{
			if(apDirection) *apDirection = vDir;
			if(apPosition) *apPosition = vPos;
		}
	}

	//-----------------------------------------------------------------------

	void cCamera::AttachEntity(iEntity3D *apEntity)
	{
		mNode.AddEntity(apEntity);
	}

	void cCamera::RemoveEntity(iEntity3D *apEntity)
	{
		mNode.RemoveEntity(apEntity);
	}

	void cCamera::ClearAttachedEntities()
	{
		mNode.ClearEntities();
	}

	//-----------------------------------------------------------------------

	void cCamera::SetForward(const cVector3f& avX)
	{
		ohpl_h10_cam_set_rotation_row(&mS, 2, &avX);
	}
	void cCamera::SetRight(const cVector3f& avX)
	{
		ohpl_h10_cam_set_rotation_row(&mS, 0, &avX);
	}
	void cCamera::SetUp(const cVector3f& avX)
	{
		ohpl_h10_cam_set_rotation_row(&mS, 1, &avX);
	}

	void cCamera::SetRotationMatrix(const cMatrixf& a_mtxRot)
	{
		ohpl_h10_cam_set_rotation_matrix(&mS, &a_mtxRot);
	}

	//-----------------------------------------------------------------------

	
	cVector3f cCamera::GetForward()
	{
		cVector3f vRow; ohpl_h10_cam_view_row(&mS, 2, true, &vRow); return vRow;
	}
	cVector3f cCamera::GetRight()
	{
		cVector3f vRow; ohpl_h10_cam_view_row(&mS, 0, false, &vRow); return vRow;
	}
	cVector3f cCamera::GetUp()
	{
		cVector3f vRow; ohpl_h10_cam_view_row(&mS, 1, false, &vRow); return vRow;
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cCamera::UpdateMoveMatrix()
	{
		ohpl_h10_cam_matrix(&mS, 2);
	}

	//-----------------------------------------------------------------------

}
