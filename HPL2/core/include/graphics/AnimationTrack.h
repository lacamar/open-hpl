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

#ifndef HPL_ANIMATION_TRACK_H
#define HPL_ANIMATION_TRACK_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "graphics/RustGraphicsH6.h"
#include "scene/Node3D.h"
#include "math/Math.h"

namespace hpl {

	class cAnimationTrack
	{
	public:
		cAnimationTrack(const tString &asName, tAnimTransformFlag aTransformFlags, const float *apLength)
			: msName(asName), mTransformFlags(aTransformFlags), mpLength(apLength), mpRs(ohpl_h6_track_new()){}
		~cAnimationTrack(){ ohpl_h6_track_free(mpRs);}

		void ResizeKeyFrames(int alSize){ ohpl_h6_track_reserve(mpRs, alSize);}
		cKeyFrame* CreateKeyFrame(float afTime){ return ohpl_h6_track_create(mpRs, afTime);}
		void ClearKeyFrames(){ ohpl_h6_track_clear(mpRs);}

		cKeyFrame* GetKeyFrame(int alIndex){ return ohpl_h6_track_frame(mpRs, alIndex);}
		int GetKeyFrameNum(){ return ohpl_h6_track_num(mpRs);}

		tAnimTransformFlag GetTransformFlags(){ return mTransformFlags;}

		float GetUnitScale(){ return mfUnitScale;}
		void SetUnitScale(float afX){ mfUnitScale = afX;}

		void ApplyToNode(cNode3D* apNode, float afTime, float afWeight,bool bLoop=true, float afTransScale=1.0f)
		{
			if(GetKeyFrameNum() == 0) return;
			cKeyFrame Frame = GetInterpolatedKeyFrame(afTime);
			apNode->AddRotation(cMath::QuaternionSlerp(afWeight, cQuaternion::Identity, Frame.rotation, true));
			apNode->AddTranslation(Frame.trans * (afWeight * afTransScale));
		}

		cKeyFrame GetInterpolatedKeyFrame(float afTime,bool bLoop=true)
		{
			cKeyFrame ResultKeyFrame;
			ohpl_h6_track_interpolate(mpRs, *mpLength, afTime, &ResultKeyFrame);
			return ResultKeyFrame;
		}

		float GetKeyFramesAtTime(float afTime, cKeyFrame** apKeyFrameA,cKeyFrame** apKeyFrameB,bool bLoop=true)
		{
			size_t lIdxA = 0, lIdxB = 0;
			float fW = ohpl_h6_track_frames_at_time(mpRs, *mpLength, afTime, &lIdxA, &lIdxB);
			const bool bEmpty = GetKeyFrameNum() == 0;
			*apKeyFrameA = bEmpty ? NULL : GetKeyFrame((int)lIdxA);
			*apKeyFrameB = bEmpty ? NULL : GetKeyFrame((int)lIdxB);
			return fW;
		}

		void Smooth(float afAmount, float afPow, int alSamples,bool abTranslation, bool abRotation){}

		const tString& GetName(){ return msName;}

		void SetNodeIndex(int alIndex){ mlNodeIdx = alIndex;}
		int GetNodeIndex(){ return mlNodeIdx;}

	private:
		tString msName;
		int mlNodeIdx = -1;
		tAnimTransformFlag mTransformFlags;
		float mfUnitScale = 1.0f;
		const float *mpLength;
		OhplH6Track *mpRs;
	};
};
#endif // HPL_ANIMATION_TRACK_H
