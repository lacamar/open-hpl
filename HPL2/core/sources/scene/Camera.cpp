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
		mvPosition = cVector3f(0);

		mfFOV = cMath::ToRad(70.0f);
		mfAspect = 4.0f/3.0f;
		mfFarClipPlane = 1000.0f;
		mfNearClipPlane = 0.05f;

		mvViewSize = cVector2f(2 * mfAspect, 2);

		mProjectionType = eProjectionType_Perspective;

		mfPitch = 0;
		mfYaw = 0;
		mfRoll = 0;

		m_mtxMatrixRotation = cMatrixf::Identity;

		mRotateMode = eCameraRotateMode_EulerAngles;
		mMoveMode = eCameraMoveMode_Fly;

		m_mtxView = cMatrixf::Identity;
		m_mtxProjection = cMatrixf::Identity;
		m_mtxMove = cMatrixf::Identity;

		mbViewUpdated = true;
		mbProjectionUpdated = true;
		mbMoveUpdated = true;
		mbFrustumUpdated = true;

		mbInfFarPlane = false;

		mfPitchLimitMin = -kPi2f;
		mfPitchLimitMax = kPi2f;

		mfYawLimitMin =0;
		mfYawLimitMax =0;
		
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
		mvPosition = avPos;
		mbViewUpdated = true;
		mbFrustumUpdated = true;

		mNode.SetPosition(mvPosition);
	}

	//-----------------------------------------------------------------------

	void cCamera::SetPitch(float afAngle)
	{ 
		mfPitch = afAngle;
		
		if(mfPitchLimitMin!=0 || mfPitchLimitMax!=0)
		{
			if(mfPitch> mfPitchLimitMax) mfPitch = mfPitchLimitMax;
			if(mfPitch< mfPitchLimitMin) mfPitch = mfPitchLimitMin;
		}

		mbViewUpdated = true; mbMoveUpdated = true;
		mbFrustumUpdated = true;
	}
	void cCamera::SetYaw(float afAngle)
	{ 
		mfYaw = afAngle;

		if(mfYawLimitMin!=0 || mfYawLimitMax!=0)
		{
			if(mfYaw> mfYawLimitMax) mfYaw = mfYawLimitMax;
			if(mfYaw< mfYawLimitMin) mfYaw = mfYawLimitMin;
		}
		
		mbViewUpdated = true; mbMoveUpdated = true;
		mbFrustumUpdated = true;
	}
	void cCamera::SetRoll(float afAngle)
	{ 
		mfRoll = afAngle;
		mbViewUpdated = true; mbMoveUpdated = true;
		mbFrustumUpdated = true;
	}
	
	//-----------------------------------------------------------------------

	void cCamera::AddPitch(float afAngle)
	{
		mfPitch += afAngle;
		
		SetPitch(mfPitch);
}	
	void cCamera::AddYaw(float afAngle)
	{ 
		mfYaw += afAngle;
		
		SetYaw(mfYaw);
	}
	void cCamera::AddRoll(float afAngle)
	{ 
		mfRoll += afAngle;
		mbViewUpdated = true; mbMoveUpdated = true;
		mbFrustumUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera::MoveForward(float afDist)
	{
		UpdateMoveMatrix();
		
		mvPosition += m_mtxMove.GetForward()*-afDist;

		mbViewUpdated = true;
		mbFrustumUpdated = true;

		mNode.SetPosition(mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera::MoveRight(float afDist)
	{
		UpdateMoveMatrix();

		mvPosition += m_mtxMove.GetRight()*afDist;
		
		mbViewUpdated = true;
		mbFrustumUpdated = true;

		mNode.SetPosition(mvPosition);
	}
	
	//-----------------------------------------------------------------------

	void cCamera::MoveUp(float afDist)
	{
		UpdateMoveMatrix();

		mvPosition += m_mtxMove.GetUp()*afDist;

		mbViewUpdated = true;
		mbFrustumUpdated = true;

		mNode.SetPosition(mvPosition);
	}

	//-----------------------------------------------------------------------

	void cCamera::SetFOV(float afAngle)
	{
		if(mfFOV==afAngle)
			return;

		mfFOV = afAngle;

		mbProjectionUpdated = true;
		mbFrustumUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera::SetAspect(float afAspect)
	{
		if(mfAspect==afAspect)
			return;
		
		mfAspect = afAspect;

		mbProjectionUpdated = true;
		mbFrustumUpdated = true;
	}

	void cCamera::SetFarClipPlane(float afX)
	{
		if(mfFarClipPlane==afX)
			return;
		
		mfFarClipPlane = afX;

		mbProjectionUpdated = true;
		mbFrustumUpdated = true;
	}

	void cCamera::SetNearClipPlane(float afX)
	{
		if(mfNearClipPlane==afX)
			return;
		
		mfNearClipPlane = afX;

		mbProjectionUpdated = true;
		mbFrustumUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera::SetProjectionType(eProjectionType aType)
	{
		if(mProjectionType == aType) return;

		mProjectionType = aType;

		mbProjectionUpdated = true;
		mbFrustumUpdated = true;
	}
	
	void cCamera::SetOrthoViewSize(const cVector2f &avSize)
	{
		mvViewSize = avSize;

		if(mProjectionType == eProjectionType_Orthographic)
		{
			mbProjectionUpdated = true;
			mbFrustumUpdated = true;
		}
	}
	
	//-----------------------------------------------------------------------

	void cCamera::SetRotateMode(eCameraRotateMode aMode)
	{
		mRotateMode = aMode;
		mbViewUpdated = true; mbMoveUpdated = true;
		mbFrustumUpdated = true;
	}	
	
	//-----------------------------------------------------------------------

	void cCamera::SetMoveMode(eCameraMoveMode aMode)
	{
		mMoveMode = aMode;
		mbMoveUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cCamera::ResetRotation()
	{
		mbViewUpdated = false;
		mbMoveUpdated = false;
		mbFrustumUpdated = true;
		m_mtxMove = cMatrixf::Identity;
		m_mtxView = cMatrixf::Identity;

		mfRoll =0;
		mfYaw =0;
		mfPitch =0;
	}

	//-----------------------------------------------------------------------

	cFrustum* cCamera::GetFrustum()
	{
		if(mbFrustumUpdated)
		{
			//If the far plane is infinite, still have to use a number on far plane
			//to calculate the near plane in the frustm.
			bool bWasInf=false;
			if(mbInfFarPlane){
				SetInifintiveFarPlane(false);
				bWasInf=true;
			}
			//////////////////////////////////////////////
			// Perspective projection
			if(mProjectionType == eProjectionType_Perspective)
			{
				mFrustum.SetupPerspectiveProj(	GetProjectionMatrix(),
												GetViewMatrix(),
												GetFarClipPlane(),GetNearClipPlane(),
												GetFOV(),GetAspect(),GetPosition(),mbInfFarPlane);
			}
			//////////////////////////////////////////////
			// Orthographic projection
			else
			{
				mFrustum.SetupOrthoProj(	GetProjectionMatrix(),
											GetViewMatrix(),
											GetFarClipPlane(),GetNearClipPlane(),
											mvViewSize,GetPosition(),mbInfFarPlane);
			}
			
			if(bWasInf){
				SetInifintiveFarPlane(true);
			}

			mbFrustumUpdated = false;
		}

		return &mFrustum;
	}

	//-----------------------------------------------------------------------
	
	const cMatrixf& cCamera::GetViewMatrix()
	{
		if(mbViewUpdated)
		{
			ohpl_scene_camera_view_matrix((int)mRotateMode, &mvPosition, mfYaw, mfPitch, mfRoll, &m_mtxMatrixRotation, &m_mtxView);

			mbViewUpdated = false;
		}
		return m_mtxView;
	}
	
	//-----------------------------------------------------------------------

	const cMatrixf& cCamera::GetProjectionMatrix()
	{
		if(mbProjectionUpdated)
		{
			ohpl_scene_camera_projection((int)mProjectionType, mfNearClipPlane, mfFarClipPlane, mfFOV, mfAspect, mbInfFarPlane, &mvViewSize, &m_mtxProjection);

			mbProjectionUpdated = false;
		}
		
		return m_mtxProjection;
	}
	
	//-----------------------------------------------------------------------

	const cMatrixf& cCamera::GetMoveMatrix()
	{
		UpdateMoveMatrix();

		return m_mtxMove;
	}


	//-----------------------------------------------------------------------

	cVector3f cCamera::GetEyePosition()
	{
		return mvPosition;
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
		if(ohpl_scene_camera_unproject((int)mProjectionType, mfNearClipPlane, mfFOV, mfAspect, &mvViewSize, &avScreenPos, &avVirtualScreenSize,
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
		m_mtxMatrixRotation.SetForward(avX);

		mbViewUpdated = true;
		mbFrustumUpdated = true;
	}
	void cCamera::SetRight(const cVector3f& avX)
	{
		m_mtxMatrixRotation.SetRight(avX);

		mbViewUpdated = true;
		mbFrustumUpdated = true;
	}
	void cCamera::SetUp(const cVector3f& avX)
	{
		m_mtxMatrixRotation.SetUp(avX);

		mbViewUpdated = true;
		mbFrustumUpdated = true;
	}

	void cCamera::SetRotationMatrix(const cMatrixf& a_mtxRot)
	{
		m_mtxMatrixRotation = a_mtxRot;

		mbViewUpdated = true;
		mbFrustumUpdated = true;
	}

	//-----------------------------------------------------------------------

	
	cVector3f cCamera::GetForward()
	{
		return GetViewMatrix().GetForward()*-1.0f;
	}
	cVector3f cCamera::GetRight()
	{
		return GetViewMatrix().GetRight();
	}
	cVector3f cCamera::GetUp()
	{
		return GetViewMatrix().GetUp();
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cCamera::UpdateMoveMatrix()
	{
		if(mbMoveUpdated)
		{
			ohpl_scene_camera_move_matrix((int)mRotateMode, (int)mMoveMode, mfYaw, mfPitch, &m_mtxMove);

			mbMoveUpdated = false;
		}
	}

	//-----------------------------------------------------------------------

}
