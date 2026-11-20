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

#ifndef HPL_FRUSTUM_H
#define HPL_FRUSTUM_H

#include "math/MathTypes.h"
#include "math/RustBv.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "math/BoundingVolume.h"

namespace hpl {

	class iLowLevelGraphics;
	class iRenderableContainerNode;

	struct OhplFrustumLayout;

	//-----------------------------------------------

	// Logic lives in rust/src/math/bv.rs; the field order mirrors its Frustum.
	class cFrustum
	{
	friend struct OhplFrustumLayout;
	public:
		cFrustum(){ ohpl_frustum_init(Rs()); }

		void SetupPerspectiveProj( const cMatrixf& a_mtxProj, const cMatrixf& a_mtxView,
									float afFarPlane,float afNearPlane,float afFOV, float afAspect,
									const cVector3f &avOrigin, bool abInfFarPlane=false,
									cMatrixf* apCustomFarProjMtx=NULL,
									bool abObliqueNearPlane=false)
		{
			ohpl_frustum_setup_perspective(Rs(), &a_mtxProj, &a_mtxView, afFarPlane, afNearPlane, afFOV, afAspect,
											&avOrigin, abInfFarPlane, apCustomFarProjMtx, abObliqueNearPlane);
		}

		void SetupOrthoProj( const cMatrixf& a_mtxProj, const cMatrixf& a_mtxView,
							 float afFarPlane,float afNearPlane,const cVector2f &avViewSize,
							 const cVector3f &avOrigin, bool abInfFarPlane=false)
		{
			ohpl_frustum_setup_ortho(Rs(), &a_mtxProj, &a_mtxView, afFarPlane, afNearPlane, &avViewSize, &avOrigin, abInfFarPlane);
		}

		const cPlanef& GetPlane(eFrustumPlane aType){ return mPlane[aType];}
		const cVector3f& GetVertex(int alIdx){ return mvVertices[alIdx];}

		bool CollidePoint(const cVector3f& avPoint){ return ohpl_frustum_collide_point(Rs(), &avPoint); }
		eCollision CollideBoundingVolume(cBoundingVolume* apBV){ return (eCollision)ohpl_frustum_collide_bv(Rs(), apBV->Rs()); }
		template<class tNode>
		eCollision CollideNode(tNode* apNode)
		{
			const cVector3f vCenter = apNode->GetCenter();
			return (eCollision)ohpl_frustum_collide_node(Rs(), &vCenter, apNode->GetRadius(), &apNode->GetMin(), &apNode->GetMax());
		}
		eCollision CollideFrustum(cFrustum *apFrustum){ return (eCollision)ohpl_frustum_collide_frustum(Rs(), apFrustum->Rs()); }

		inline const cMatrixf& GetProjectionMatrix() const { return m_mtxProj;}
		inline const cMatrixf& GetViewMatrix()const { return m_mtxView;}

		inline float GetFarPlane()const { return mfFarPlane;}
		inline float GetNearPlane()const { return mfNearPlane;}
		inline float GetAspect() const { return mfAspect;}
		inline float GetFOV() const { return mfFOV;}

		bool GetInfFarPlane(){ return mbInfFarPlane;}

		void SetInvertsCullMode(bool abX){ mbInvertsCullMode=abX;}
		bool GetInvertsCullMode(){ return mbInvertsCullMode;}

		inline eProjectionType GetProjectionType() const { return mProjectionType; }

		inline const cVector2f& GetOrthoViewSize() const{ return mvViewSize;}

		bool CheckSphereNearPlaneIntersection(const cVector3f &avCenter, float afRadius){ return ohpl_frustum_check_sphere_near_plane(Rs(), &avCenter, afRadius); }
		bool CheckAABBNearPlaneIntersection(const cVector3f &avMin, const cVector3f &avMax){ return ohpl_frustum_check_aabb_near_plane(Rs(), &avMin, &avMax); }
		bool CheckBVNearPlaneIntersection(cBoundingVolume* apBV){ return ohpl_frustum_check_bv_near_plane(Rs(), apBV->Rs()); }
		bool CheckFrustumNearPlaneIntersection(cFrustum* apFrustum){ return ohpl_frustum_check_frustum_near_plane(Rs(), apFrustum->Rs()); }

		bool CheckLineIntersection(const cVector3f& avStart, const cVector3f &avEnd, cVector3f &avIntersection)
		{
			return ohpl_frustum_check_line(Rs(), &avStart, &avEnd, &avIntersection);
		}

		cSpheref GetBoundingSphere(){ return mBoundingSphere;}
		const cBoundingVolume& GetBoundingVolume(){ return mBoundingVolume;}

		const cVector3f& GetOrigin(){ return mvOrigin;}
		cBoundingVolume* GetOriginBV(){ return &mOriginBV;}

		cVector3f GetForward(){ return m_mtxView.GetForward();}

		template<class tGfx>
		void Draw(tGfx *apLowLevelGraphics, const cColor &aColor)
		{
			for(int i=0; i<4; ++i)
				apLowLevelGraphics->DrawLine(mvVertices[i==0?3:i-1], mvVertices[i],aColor);
			for(int i=4; i<8; ++i)
				apLowLevelGraphics->DrawLine(mvVertices[i==4?7:i-1], mvVertices[i],aColor);
			for(int i=0; i<4; ++i)
				apLowLevelGraphics->DrawLine(mvVertices[i], mvVertices[i+4],aColor);
		}

		eCollision CollideFustrumSphere(const cVector3f& avCenter, float afRadius){ return (eCollision)ohpl_frustum_collide_frustum_sphere(Rs(), &avCenter, afRadius); }

		eCollision CollideSphere(const cVector3f& avCenter, float afRadius, int alMaxPlanes=6){ return (eCollision)ohpl_frustum_collide_sphere(Rs(), &avCenter, afRadius, alMaxPlanes); }
		eCollision CollideAABB(const cVector3f& avMin,const cVector3f& avMax, int alMaxPlanes=6){ return (eCollision)ohpl_frustum_collide_aabb(Rs(), &avMin, &avMax); }

	private:
		OhplFrustum* Rs(){ return reinterpret_cast<OhplFrustum*>(this); }

		float mfFarPlane;
		float mfNearPlane;
		float mfAspect;
		float mfFOV;

		cVector2f mvViewSize;

		bool mbInfFarPlane;
		bool mbInvertsCullMode;
		bool mbObliqueNearPlane;

		eProjectionType mProjectionType;

		cVector3f mvOrigin;
		cBoundingVolume mOriginBV;

		cMatrixf m_mtxProj;
		cMatrixf m_mtxViewProj;
		cMatrixf m_mtxView;
		cPlanef mPlane[6];
		cSpheref mBoundingSphere;
		cBoundingVolume mBoundingVolume;

		cVector3f mvVertices[8];
	};

	struct OhplFrustumLayout
	{
		static_assert(sizeof(cFrustum) == 848 && __builtin_offsetof(cFrustum, mvViewSize) == 16 && __builtin_offsetof(cFrustum, mbInfFarPlane) == 24
			&& __builtin_offsetof(cFrustum, mProjectionType) == 28 && __builtin_offsetof(cFrustum, mvOrigin) == 32
			&& __builtin_offsetof(cFrustum, mOriginBV) == 48 && __builtin_offsetof(cFrustum, m_mtxProj) == 248
			&& __builtin_offsetof(cFrustum, m_mtxView) == 376 && __builtin_offsetof(cFrustum, mPlane) == 440
			&& __builtin_offsetof(cFrustum, mBoundingSphere) == 536 && __builtin_offsetof(cFrustum, mBoundingVolume) == 552
			&& __builtin_offsetof(cFrustum, mvVertices) == 752 && sizeof(eProjectionType) == 4
			&& __builtin_offsetof(cSpheref, center) == 4, "cFrustum layout");
	};
};
#endif // HPL_FRUSTUM_H
