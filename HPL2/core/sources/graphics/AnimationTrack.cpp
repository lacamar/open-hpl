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

#include "graphics/AnimationTrack.h"

#include "math/Math.h"
#include "graphics/Animation.h"
#include "graphics/RustMesh.h"
#include "system/LowLevelSystem.h"
#include "scene/Node3D.h"
#include "impl/RustBackends.h"
#include "graphics/RustGraphicsH6.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cAnimationTrack::cAnimationTrack(const tString &asName, tAnimTransformFlag aTransformFlags, 
									cAnimation *apParent)
	{
		msName = asName;
		mTransformFlags = aTransformFlags;
		mpParent = apParent;

		mfMaxFrameTime = 0;

		mlNodeIdx = -1;
		mpRs = OhplBackendRustRequested() ? ohpl_h6_track_new() : NULL;
	}

	//-----------------------------------------------------------------------

	cAnimationTrack::~cAnimationTrack()
	{
		if(mpRs) ohpl_h6_track_free(mpRs);
		STLDeleteAll(mvKeyFrames);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cAnimationTrack::ResizeKeyFrames(int alSize)
	{
		if(mpRs) { ohpl_h6_track_reserve(mpRs, alSize); return; }
		mvKeyFrames.reserve(alSize);
	}
	
	//-----------------------------------------------------------------------

	cKeyFrame* cAnimationTrack::CreateKeyFrame(float afTime)
	{
		if(mpRs) return ohpl_h6_track_create(mpRs, afTime);
		cKeyFrame* pFrame = hplNew( cKeyFrame,());
		pFrame->time = afTime;

		bool bUpdateMax = false;
		size_t lPos = ohpl_anim_insert_position(mvKeyFrames.data(), mvKeyFrames.size(), mfMaxFrameTime, afTime, &bUpdateMax);
		mvKeyFrames.insert(mvKeyFrames.begin() + lPos, pFrame);
		if(bUpdateMax) mfMaxFrameTime = afTime;
		
        return pFrame;
	}

	//-----------------------------------------------------------------------

	void cAnimationTrack::ClearKeyFrames()
	{
		if(mpRs) { ohpl_h6_track_clear(mpRs); return; }
		STLDeleteAll(mvKeyFrames);
		mvKeyFrames.clear();
	}

	//-----------------------------------------------------------------------

	void cAnimationTrack::ApplyToNode(cNode3D* apNode, float afTime, float afWeight, bool bLoop, float afTransScale)
	{
		if(GetKeyFrameNum() == 0) return;

		cKeyFrame Frame = GetInterpolatedKeyFrame(afTime);
        		
		//Scale
		//Skip this for now...
		/*cVector3f vOne(1,1,1);
		cVector3f vScale = (Frame.scale - vOne)*afWeight + vOne;
		apNode->AddScale(vScale);*/

		//Rotation
		cQuaternion qRot = cMath::QuaternionSlerp(afWeight, cQuaternion::Identity, Frame.rotation, true);
		apNode->AddRotation(qRot);
		
		//Translation
		cVector3f vTrans = Frame.trans * (afWeight * afTransScale);
		apNode->AddTranslation(vTrans);
	}

	//-----------------------------------------------------------------------

	cKeyFrame cAnimationTrack::GetInterpolatedKeyFrame(float afTime, bool bLoop)
	{
		cKeyFrame ResultKeyFrame;
		if(mpRs)
		{
			ohpl_h6_track_interpolate(mpRs, mpParent->GetLength(), afTime, &ResultKeyFrame);
			return ResultKeyFrame;
		}
		ohpl_anim_interpolate(mvKeyFrames.data(), mvKeyFrames.size(), mpParent->GetLength(), mfMaxFrameTime, afTime, &ResultKeyFrame);

		return ResultKeyFrame;
	}

	//-----------------------------------------------------------------------
	
	float cAnimationTrack::GetKeyFramesAtTime(float afTime, cKeyFrame** apKeyFrameA,cKeyFrame** apKeyFrameB, bool bLoop)
	{
		size_t lIdxA = 0, lIdxB = 0;
		if(mpRs)
		{
			float fW = ohpl_h6_track_frames_at_time(mpRs, mpParent->GetLength(), afTime, &lIdxA, &lIdxB);
			if(GetKeyFrameNum() == 0)
			{
				*apKeyFrameA = NULL;
				*apKeyFrameB = NULL;
				return fW;
			}
			*apKeyFrameA = GetKeyFrame((int)lIdxA);
			*apKeyFrameB = GetKeyFrame((int)lIdxB);
			return fW;
		}
		float fT = ohpl_anim_frames_at_time(mvKeyFrames.data(), mvKeyFrames.size(), mpParent->GetLength(), mfMaxFrameTime, afTime, &lIdxA, &lIdxB);

		if(mvKeyFrames.empty())
		{
			*apKeyFrameA = NULL;
			*apKeyFrameB = NULL;
			return fT;
		}

		*apKeyFrameA = mvKeyFrames[lIdxA];
		*apKeyFrameB = mvKeyFrames[lIdxB];
		return fT;
	}

	//-----------------------------------------------------------------------



	void cAnimationTrack::Smooth(float afAmount,float afPow,  int alSamples, bool abTranslation, bool abRotation)
	{
		/*/////////////////////////////////
		// Create temp key frames
		tKeyFramePtrVec vTempKeyFrames;
		vTempKeyFrames.resize(mvKeyFrames.size());
		for(size_t i=0; i<vTempKeyFrames.size(); ++i)
		{
			cKeyFrame* pFrame = hplNew( cKeyFrame,());
			*pFrame = *mvKeyFrames[i];
            vTempKeyFrames[i] = pFrame;
		}

		/////////////////////////////////
		// Go through all keyframes and calculate a smooth version
		// TODO: Add translation and also be able to turn rot / trans off
		for(size_t i=0; i<mvKeyFrames.size(); ++i)
		{
			cKeyFrame* pFrame = vTempKeyFrames[i];
			Log(" KeyFrame %d\n", i);
			
			float fTime = pFrame->time;
            float fMulSum =0;
			cVector3f vRotSum(0);
            			
			for(int sample=0; sample<alSamples;++sample)
			{
				float fT = ( ((float)sample) / ((float)alSamples));
				float fMul = powf(1.0f - fT, afPow);

				Log("   Sample %d t: %f mul: %f\n", sample, fT, fMul);
				
				float fTimeAdd = fT * afAmount;
				
				///////////////////
				//Rotation
				if(abRotation)
				{
					if(fTime + fTimeAdd <= mpParent->GetLength())
					{
						vRotSum = vRotSum + GetBaseAnglesAtTime(fTime + fTimeAdd) * fMul;
						fMulSum += fMul;
						Log("    Rot: %s\n", cMath::Vector3ToDeg(GetBaseAnglesAtTime(fTime + fTimeAdd)).ToString().c_str());
					}
					
					if(sample != 0 && fTime - fTimeAdd >= 0)
					{
						vRotSum = vRotSum + GetBaseAnglesAtTime(fTime - fTimeAdd) * fMul;
						fMulSum += fMul;
						Log("    Rot: %s\n", cMath::Vector3ToDeg(GetBaseAnglesAtTime(fTime - fTimeAdd)).ToString().c_str());
					}
				}
			}
			
			if(abRotation)
			{
				cVector3f vRot = vRotSum *(1.0f / fMulSum);
				Log("  Result: %s\n", cMath::Vector3ToDeg(vRot).ToString().c_str());
				cMatrixf mtxRot = cMath::MatrixRotate(vRot, eEulerRotationOrder_XYZ);
				cQuaternion qRot;
				qRot.FromRotationMatrix(mtxRot);

				pFrame->rotation = qRot;
			}
		}

		//////////////////////////////
		// Set temp data to normal
		for(size_t i=0; i<vTempKeyFrames.size(); ++i)
		{
			cKeyFrame* pTempFrame = vTempKeyFrames[i];
			cKeyFrame* pFrame = mvKeyFrames[i];
			*pFrame = *pTempFrame;
		}
		

		/////////////////////////////////
		// Delete temp key frames
		STLDeleteAll(vTempKeyFrames);*/
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}