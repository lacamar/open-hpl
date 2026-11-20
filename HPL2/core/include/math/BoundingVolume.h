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

#ifndef HPL_BOUNDING_VOLUME_H
#define HPL_BOUNDING_VOLUME_H

#include "math/MathTypes.h"
#include "math/RustBv.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "system/SerializeClass.h"

namespace hpl {

	enum eBVCollision
	{
		eBVCollision_Inside,
		eBVCollision_Outside,
		eBVCollision_Intersect,
		eBVCollision_LastEnum
	};

	struct OhplBvLayout;

	// Logic and the update flags live in rust/src/math/bv.rs; the field order mirrors its Bv.
	class cBoundingVolume : public iSerializable
	{
	friend class cMath;
	friend struct OhplBvLayout;
		kSerializableClassInit(cBoundingVolume)
	public:
		cBoundingVolume(){ ohpl_bv_init(Rs()); }
		cBoundingVolume(const cBoundingVolume& aBV) : iSerializable(aBV){ ohpl_bv_copy(Rs(), aBV.Rs()); }
		cBoundingVolume& operator=(const cBoundingVolume& aBV){ ohpl_bv_assign(Rs(), aBV.Rs()); return *this; }
		virtual ~cBoundingVolume(){ ohpl_bv_drop(Rs()); }

		const cVector3f& GetMax(){ UpdateSize(); return mvWorldMax; }
		const cVector3f& GetMin(){ UpdateSize(); return mvWorldMin; }

		const cVector3f& GetLocalMax(){ return mvLocalMax; }
		const cVector3f& GetLocalMin(){ return mvLocalMin; }

		void SetLocalMinMax(const cVector3f& mvMin,const cVector3f& mvMax){ ohpl_bv_set_local_min_max(Rs(), &mvMin, &mvMax); }

		void SetPosition(const cVector3f& avPos){ ohpl_bv_set_position(Rs(), &avPos); }
		cVector3f GetPosition(){ return m_mtxTransform.GetTranslation(); }

		void SetSize(const cVector3f& avSize){ ohpl_bv_set_size(Rs(), &avSize); }
		cVector3f GetSize(){ UpdateSize(); return mvSize; }

		void SetTransform(const cMatrixf &a_mtxTransform){ ohpl_bv_set_transform(Rs(), &a_mtxTransform); }
		const cMatrixf& GetTransform(){ return m_mtxTransform; }

		cVector3f GetLocalCenter(){ cVector3f v; ohpl_bv_local_center(Rs(), &v); return v; }
		cVector3f GetWorldCenter(){ cVector3f v; ohpl_bv_world_center(Rs(), &v); return v; }

		float GetRadius(){ UpdateSize(); return mfRadius; }

		void AddArrayPoints(const float *apArray, int alNumOfVectors){ ohpl_bv_add_array_points(Rs(), apArray, alNumOfVectors); }
		void CreateFromPoints(int alStride){ ohpl_bv_create_from_points(Rs(), alStride); }

		void UpdateSize(){ ohpl_bv_update_size(Rs()); }

		OhplBv* Rs() const { return reinterpret_cast<OhplBv*>(const_cast<cMatrixf*>(&m_mtxTransform)); }

		cMatrixf m_mtxTransform;

		cVector3f mvLocalMax;
		cVector3f mvLocalMin;

		cVector3f mvMax;
		cVector3f mvMin;

		cVector3f mvPivot;

		cVector3f mvWorldMax;
		cVector3f mvWorldMin;

		cVector3f mvPosition;
		cVector3f mvSize;
		float mfRadius;

	private:
		bool mbPositionUpdated;
		bool mbSizeUpdated;
		void* mpPending;
	};

	struct OhplBvLayout
	{
		static_assert(sizeof(cBoundingVolume) == 200 && __builtin_offsetof(cBoundingVolume, m_mtxTransform) == 8
			&& __builtin_offsetof(cBoundingVolume, mvLocalMax) == 72 && __builtin_offsetof(cBoundingVolume, mvSize) == 168
			&& __builtin_offsetof(cBoundingVolume, mfRadius) == 180 && __builtin_offsetof(cBoundingVolume, mbPositionUpdated) == 184
			&& __builtin_offsetof(cBoundingVolume, mpPending) == 192, "cBoundingVolume layout");
	};
};
#endif // HPL_BOUNDING_VOLUME_H
