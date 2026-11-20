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

#ifndef HPL_RENDERABLE_H
#define HPL_RENDERABLE_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "scene/SceneTypes.h"

#include "graphics/RustRenderable.h"
#include "scene/Entity3D.h"


namespace hpl {

	class cMaterial;
	class iVertexBuffer;
	class cBoundingVolume;
	class cFrustum;
	class iLight;
	class cCamera;
	class iRenderableCallback;
	class cRenderList;
	class iRenderableContainerNode;
	class iPhysicsBody;
	class iRenderer;

	//------------------------------------------

	class iRenderable : public iEntity3D
	{
	#ifdef __GNUC__
		typedef iEntity3D __super;
	#endif
	public:
		iRenderable(const tString &asName) : iEntity3D(asName), mpR(ohpl_rnd_new()){}
		virtual ~iRenderable();

		OhplRnd* Rs() const { return mpR;}

		virtual cMaterial *GetMaterial()=0;
		virtual iVertexBuffer* GetVertexBuffer()=0;

		virtual bool CollidesWithBV(cBoundingVolume *apBV);
		virtual bool CollidesWithFrustum(cFrustum *apFrustum);

		virtual cMatrixf* GetModelMatrix(cFrustum *apFrustum)=0;

		virtual eRenderableType GetRenderType()=0;

		virtual void UpdateGraphicsForFrame(float afFrameTime){}
		virtual bool UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime){ return true;}

		virtual bool UsesOcclusionQuery(){ return false; }
		virtual void AssignOcclusionQuery(iRenderer *apRenderer){}
		virtual bool RetrieveOcculsionQuery(iRenderer *apRenderer){ return true;}

		virtual void SetRenderFlagBit(tRenderableFlag alFlagBit, bool abSet);
		bool GetRenderFlagBit(tRenderableFlag alFlagBit){ return (mpR->mlRenderFlags & alFlagBit)!=0;}
		inline tRenderableFlag GetRenderFlags() const { return mpR->mlRenderFlags;}

		virtual bool IsVisible(){ return mbIsVisible && mpR->mfCoverageAmount >0; }
		void SetVisible(bool abVisible);
		/**
		 * This is needed since IsVisible does not return the actual var value!
		 */
		bool GetVisibleVar(){ return mbIsVisible;}
		virtual void OnChangeVisible(){}
		
		virtual void SetIlluminationAmount(float afX){ mpR->mfIlluminationAmount = afX;}
		inline float GetIlluminationAmount()const { return mpR->mfIlluminationAmount;}
		void SetIlluminationColor(const cColor& aX){ mpR->mIlluminationColor = aX;}
		inline const cColor& GetIlluminationColor()const { return mpR->mIlluminationColor;}
		void SetColorMul(const cColor& aX){ mpR->mColorMul = aX;}
		inline const cColor& GetColorMul()const { return mpR->mColorMul;}
		
		void SetCoverageAmount(float afX);
		inline float GetCoverageAmount()const { return mpR->mfCoverageAmount;}

		void SetLiquidAmount(float afX){ mpR->mfLiquidAmount = afX > 0 ? afX : 0; }
		void UseAutomaticLiquidAmount(float afTime=0){ mpR->mfLiquidAmount = -1 - std::fabs(afTime); }
		inline float GetLiquidAmount()const { return mpR->mfLiquidAmount;}

		virtual void SetCulledByDistance(bool abX){ mpR->mbCulledByDistance = abX; mpR->mfDistanceCullFade = 1; }
		bool IsCulledByDistance() const { return mpR->mbCulledByDistance;}
		float& DistanceCullFade(){ return mpR->mfDistanceCullFade;}
		void SetCulledByFog(bool abX){ mpR->mbCulledByFog = abX; }
		bool IsCulledByFog() const { return mpR->mbCulledByFog;}

		void SetLargePlaneSurfacePlacement(int alX){ mpR->mlLargePlaneSurfacePlacement = alX;};
		inline int GetLargePlaneSurfacePlacement(){ return mpR->mlLargePlaneSurfacePlacement;};

		/**
		 * Should return a different number each time the renderable model matrix is updated. never -1
		 * \return
		 */
		virtual int GetMatrixUpdateCount()=0;

		inline void SetModelMatrixPtr(cMatrixf *apMtx){ mpR->mpModelMatrix = apMtx;}
		inline cMatrixf* GetModelMatrixPtr() const { return mpR->mpModelMatrix;}

		inline float GetViewSpaceZ() const { return mpR->mfViewSpaceZ;}
		inline void SetViewSpaceZ(float afZ){ mpR->mfViewSpaceZ = afZ;}

		cMatrixf* GetInvModelMatrix();

		inline void SetPrevMatrix(const cMatrixf& a_mtxPrev){mpR->m_mtxPrevious = a_mtxPrev;}
		inline cMatrixf& GetPrevMatrix(){ return mpR->m_mtxPrevious;}

		const cVector3f& GetCalcScale();

		void SetStatic(bool abX){ mpR->mbStatic = abX;}
		bool IsStatic() const { return mpR->mbStatic;}

		inline int GetRenderFrameCount() const { return mlRenderFrameCount;}
		inline void SetRenderFrameCount(const int alCount) { mlRenderFrameCount = alCount;}
		
		bool GetIsOneSided(){ return mbIsOneSided;}
		const cVector3f& GetOneSidedNormal(){ return mvOneSidedNormal;}

		void SetRenderCallback(iRenderableCallback *apCallback){ mpRenderCallback = apCallback; }

		inline iRenderableContainerNode* GetRenderContainerNode(){ return mpR->mpRenderContainerNode; }
		inline void SetRenderContainerNode(iRenderableContainerNode* apNode){ mpR->mpRenderContainerNode = apNode; }

		void SetRenderableUserData(void* apData) { mpR->mpRenderableUserData = apData; }
		void* GetRenderableUserData() { return mpR->mpRenderableUserData; }

	protected:
		// Rust-owned state (rust/src/scene/rnd.rs); the references keep the names subclasses use.
		OhplRnd *mpR;
		bool &mbIsVisible = mpR->mbIsVisible;
		iRenderableCallback *&mpRenderCallback = mpR->mpRenderCallback;
		int &mlRenderFrameCount = mpR->mlRenderFrameCount;
		bool &mbIsOneSided = mpR->mbIsOneSided;
		cVector3f &mvOneSidedNormal = mpR->mvOneSidedNormal;
	};
};
#endif // HPL_RENDERABLE_H
