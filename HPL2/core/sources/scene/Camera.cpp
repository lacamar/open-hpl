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
#include "impl/RustBackends.h"

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
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_position(&mS, &avPos); mNode.SetPosition(mS.mvPosition); return; }

		mS.mvPosition = avPos;
		mS.mbViewUpdated = true;
		mS.mbFrustumUpdated = true;

		mNode.SetPosition(mS.mvPosition);
	}

	//-----------------------------------------------------------------------

	void cCamera::SetPitch(float afAngle)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_pitch(&mS, afAngle); return; }

		mS.mfPitch = afAngle;
		
		if(mS.mfPitchLimitMin!=0 || mS.mfPitchLimitMax!=0)
		{
			if(mS.mfPitch> mS.mfPitchLimitMax) mS.mfPitch = mS.mfPitchLimitMax;
			if(mS.mfPitch< mS.mfPitchLimitMin) mS.mfPitch = mS.mfPitchLimitMin;
		}

		mS.mbViewUpdated = true; mS.mbMoveUpdated = true;
		mS.mbFrustumUpdated = true;
	}
	void cCamera::SetYaw(float afAngle)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_yaw(&mS, afAngle); return; }

		mS.mfYaw = afAngle;

		if(mS.mfYawLimitMin!=0 || mS.mfYawLimitMax!=0)
		{
			if(mS.mfYaw> mS.mfYawLimitMax) mS.mfYaw = mS.mfYawLimitMax;
			if(mS.mfYaw< mS.mfYawLimitMin) mS.mfYaw = mS.mfYawLimitMin;
		}
		
		mS.mbViewUpdated = true; mS.mbMoveUpdated = true;
		mS.mbFrustumUpdated = true;
	}
	void cCamera::SetRoll(float afAngle)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_roll(&mS, afAngle); return; }

		mS.mfRoll = afAngle;
		mS.mbViewUpdated = true; mS.mbMoveUpdated = true;
		mS.mbFrustumUpdated = true;
	}
	
	//-----------------------------------------------------------------------

	void cCamera::AddPitch(float afAngle)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_add_pitch(&mS, afAngle); return; }

		mS.mfPitch += afAngle;
		
		SetPitch(mS.mfPitch);
}	
	void cCamera::AddYaw(float afAngle)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_add_yaw(&mS, afAngle); return; }

		mS.mfYaw += afAngle;
		
		SetYaw(mS.mfYaw);
	}
	void cCamera::AddRoll(float afAngle)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_add_roll(&mS, afAngle); return; }

		mS.mfRoll += afAngle;
		mS.mbViewUpdated = true; mS.mbMoveUpdated = true;
		mS.mbFrustumUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera::MoveForward(float afDist)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_move(&mS, 0, afDist); mNode.SetPosition(mS.mvPosition); return; }

		UpdateMoveMatrix();
		
		mS.mvPosition += mS.m_mtxMove.GetForward()*-afDist;

		mS.mbViewUpdated = true;
		mS.mbFrustumUpdated = true;

		mNode.SetPosition(mS.mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera::MoveRight(float afDist)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_move(&mS, 1, afDist); mNode.SetPosition(mS.mvPosition); return; }

		UpdateMoveMatrix();

		mS.mvPosition += mS.m_mtxMove.GetRight()*afDist;
		
		mS.mbViewUpdated = true;
		mS.mbFrustumUpdated = true;

		mNode.SetPosition(mS.mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera::MoveUp(float afDist)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_move(&mS, 2, afDist); mNode.SetPosition(mS.mvPosition); return; }

		UpdateMoveMatrix();

		mS.mvPosition += mS.m_mtxMove.GetUp()*afDist;

		mS.mbViewUpdated = true;
		mS.mbFrustumUpdated = true;

		mNode.SetPosition(mS.mvPosition);
	}

	//-----------------------------------------------------------------------

	void cCamera::SetFOV(float afAngle)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_fov(&mS, afAngle); return; }

		if(mS.mfFOV==afAngle)
			return;

		mS.mfFOV = afAngle;

		mS.mbProjectionUpdated = true;
		mS.mbFrustumUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera::SetAspect(float afAspect)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_aspect(&mS, afAspect); return; }

		if(mS.mfAspect==afAspect)
			return;
		
		mS.mfAspect = afAspect;

		mS.mbProjectionUpdated = true;
		mS.mbFrustumUpdated = true;
	}

	void cCamera::SetFarClipPlane(float afX)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_far(&mS, afX); return; }

		if(mS.mfFarClipPlane==afX)
			return;
		
		mS.mfFarClipPlane = afX;

		mS.mbProjectionUpdated = true;
		mS.mbFrustumUpdated = true;
	}

	void cCamera::SetNearClipPlane(float afX)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_near(&mS, afX); return; }

		if(mS.mfNearClipPlane==afX)
			return;
		
		mS.mfNearClipPlane = afX;

		mS.mbProjectionUpdated = true;
		mS.mbFrustumUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera::SetProjectionType(eProjectionType aType)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_projection_type(&mS, (int)aType); return; }

		if(mS.mlProjectionType == aType) return;

		mS.mlProjectionType = aType;

		mS.mbProjectionUpdated = true;
		mS.mbFrustumUpdated = true;
	}
	
	void cCamera::SetOrthoViewSize(const cVector2f &avSize)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_ortho_view_size(&mS, &avSize); return; }

		mS.mvViewSize = avSize;

		if(mS.mlProjectionType == eProjectionType_Orthographic)
		{
			mS.mbProjectionUpdated = true;
			mS.mbFrustumUpdated = true;
		}
	}
	
	//-----------------------------------------------------------------------

	void cCamera::SetRotateMode(eCameraRotateMode aMode)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_rotate_mode(&mS, (int)aMode); return; }

		mS.mlRotateMode = aMode;
		mS.mbViewUpdated = true; mS.mbMoveUpdated = true;
		mS.mbFrustumUpdated = true;
	}	
	
	//-----------------------------------------------------------------------

	void cCamera::SetMoveMode(eCameraMoveMode aMode)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_move_mode(&mS, (int)aMode); return; }

		mS.mlMoveMode = aMode;
		mS.mbMoveUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera::ResetRotation()
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_reset_rotation(&mS); return; }

		mS.mbViewUpdated = false;
		mS.mbMoveUpdated = false;
		mS.mbFrustumUpdated = true;
		mS.m_mtxMove = cMatrixf::Identity;
		mS.m_mtxView = cMatrixf::Identity;

		mS.mfRoll =0;
		mS.mfYaw =0;
		mS.mfPitch =0;
	}

	//-----------------------------------------------------------------------

	cFrustum* cCamera::GetFrustum()
	{
		if(mS.mbFrustumUpdated)
		{
			//If the far plane is infinite, still have to use a number on far plane
			//to calculate the near plane in the frustm.
			bool bWasInf=false;
			if(OhplBackendRustRequested())
			{
				bWasInf = (ohpl_h10_cam_frustum_begin(&mS) & 2) != 0;
			}
			else if(mS.mbInfFarPlane){
				SetInifintiveFarPlane(false);
				bWasInf=true;
			}
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
			
			if(OhplBackendRustRequested())
			{
				ohpl_h10_cam_frustum_end(&mS, bWasInf);
			}
			else
			{
				if(bWasInf){
					SetInifintiveFarPlane(true);
				}

				mS.mbFrustumUpdated = false;
			}
		}

		return &mFrustum;
	}

	//-----------------------------------------------------------------------
	
	const cMatrixf& cCamera::GetViewMatrix()
	{
		if(OhplBackendRustRequested()) { return *ohpl_h10_cam_matrix(&mS, 0); }

		if(mS.mbViewUpdated)
		{
			ohpl_scene_camera_view_matrix((int)mS.mlRotateMode, &mS.mvPosition, mS.mfYaw, mS.mfPitch, mS.mfRoll, &mS.m_mtxMatrixRotation, &mS.m_mtxView);

			mS.mbViewUpdated = false;
		}
		return mS.m_mtxView;
	}
	
	//-----------------------------------------------------------------------

	const cMatrixf& cCamera::GetProjectionMatrix()
	{
		if(OhplBackendRustRequested()) { return *ohpl_h10_cam_matrix(&mS, 1); }

		if(mS.mbProjectionUpdated)
		{
			ohpl_scene_camera_projection((int)mS.mlProjectionType, mS.mfNearClipPlane, mS.mfFarClipPlane, mS.mfFOV, mS.mfAspect, mS.mbInfFarPlane, &mS.mvViewSize, &mS.m_mtxProjection);

			mS.mbProjectionUpdated = false;
		}
		
		return mS.m_mtxProjection;
	}
	
	//-----------------------------------------------------------------------

	const cMatrixf& cCamera::GetMoveMatrix()
	{
		if(OhplBackendRustRequested()) { return *ohpl_h10_cam_matrix(&mS, 2); }

		UpdateMoveMatrix();

		return mS.m_mtxMove;
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
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_rotation_row(&mS, 2, &avX); return; }

		mS.m_mtxMatrixRotation.SetForward(avX);

		mS.mbViewUpdated = true;
		mS.mbFrustumUpdated = true;
	}
	void cCamera::SetRight(const cVector3f& avX)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_rotation_row(&mS, 0, &avX); return; }

		mS.m_mtxMatrixRotation.SetRight(avX);

		mS.mbViewUpdated = true;
		mS.mbFrustumUpdated = true;
	}
	void cCamera::SetUp(const cVector3f& avX)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_rotation_row(&mS, 1, &avX); return; }

		mS.m_mtxMatrixRotation.SetUp(avX);

		mS.mbViewUpdated = true;
		mS.mbFrustumUpdated = true;
	}

	void cCamera::SetRotationMatrix(const cMatrixf& a_mtxRot)
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_set_rotation_matrix(&mS, &a_mtxRot); return; }

		mS.m_mtxMatrixRotation = a_mtxRot;

		mS.mbViewUpdated = true;
		mS.mbFrustumUpdated = true;
	}

	//-----------------------------------------------------------------------

	
	cVector3f cCamera::GetForward()
	{
		if(OhplBackendRustRequested()) { cVector3f vRow; ohpl_h10_cam_view_row(&mS, 2, true, &vRow); return vRow; }

		return GetViewMatrix().GetForward()*-1.0f;
	}
	cVector3f cCamera::GetRight()
	{
		if(OhplBackendRustRequested()) { cVector3f vRow; ohpl_h10_cam_view_row(&mS, 0, false, &vRow); return vRow; }

		return GetViewMatrix().GetRight();
	}
	cVector3f cCamera::GetUp()
	{
		if(OhplBackendRustRequested()) { cVector3f vRow; ohpl_h10_cam_view_row(&mS, 1, false, &vRow); return vRow; }

		return GetViewMatrix().GetUp();
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cCamera::UpdateMoveMatrix()
	{
		if(OhplBackendRustRequested()) { ohpl_h10_cam_matrix(&mS, 2); return; }

		if(mS.mbMoveUpdated)
		{
			ohpl_scene_camera_move_matrix((int)mS.mlRotateMode, (int)mS.mlMoveMode, mS.mfYaw, mS.mfPitch, &mS.m_mtxMove);

			mS.mbMoveUpdated = false;
		}
	}

	//-----------------------------------------------------------------------

}
