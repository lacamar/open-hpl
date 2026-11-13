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

#include "physics/CollideShape.h"

#include "physics/PhysicsWorld.h"
#include "system/LowLevelSystem.h"
#include "impl/RustNewton.h"
#include "impl/RustNewtonBackend.h"
#include "physics/PhysicsWorld.h"
#include "system/Platform.h"
#include "resources/BinaryBuffer.h"
#include "resources/Resources.h"
#include "system/String.h"
#include "math/Math.h"
#include <algorithm>
#include <cstdio>
#include <unistd.h>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iCollideShape::iCollideShape(eCollideShapeType aType, const cVector3f &avSize,
											cMatrixf* apOffsetMtx, NewtonWorld* apNewtonWorld,
											iPhysicsWorld *apWorld)
	: mlUserCount(0), mpWorld(apWorld)
	{
		mpNewtonCollision = NULL;
		mpNewtonWorld = apNewtonWorld;
		if(apWorld) mhWorld = static_cast<iPhysicsWorld*>(apWorld)->RustHandle();
		mvSize = avSize;
		mType = aType;

		mfVolume = 0;

		if(apOffsetMtx)
			m_mtxOffset = *apOffsetMtx;
		else
			m_mtxOffset = cMatrixf::Identity;

		////////////////////////////////////////////
		// Create Newton collision

		if(aType >= eCollideShapeType_Null && aType <= eCollideShapeType_Capsule)
			mpNewtonCollision = ohpl_nts_create(mhWorld, (int32_t)aType, &mvSize, apOffsetMtx);

		////////////////////////////////////////////
		// Calculate Bounding volume and volume.
		if(mType == eCollideShapeType_Box)
		{
			mBoundingVolume.SetSize(mvSize);

			mfVolume = mvSize.x * mvSize.y *mvSize.z;
		}
		else if(mType == eCollideShapeType_Sphere)
		{
			mBoundingVolume.SetSize(mvSize.x*2);

			mfVolume = (4.0f / 3.0f) * kPif * (mvSize.x*mvSize.x*mvSize.x);
		}
		else if(mType == eCollideShapeType_Cylinder ||
				mType == eCollideShapeType_Capsule)
		{
			mBoundingVolume.SetSize(cVector3f(mvSize.y,mvSize.x*2,mvSize.x*2));

			//Not gonna be correct for capsule...
			if(mType == eCollideShapeType_Cylinder)
				mfVolume = kPif * (mvSize.x*mvSize.x)*mvSize.y;
			else
			{
				//Height of the cylinder part.
                float fCylHeight = mvSize.y - (mvSize.x*2);
				mfVolume =0;

				//The volume of the cylinder part.
				if(fCylHeight>0)
					mfVolume += kPif * (mvSize.x*mvSize.x)*fCylHeight;

				//The volume of the sphere part.
				mfVolume += (4.0f / 3.0f) * kPif * (mvSize.x*mvSize.x*mvSize.x);
			}
		}

		mBoundingVolume.SetTransform(m_mtxOffset);
	}

	//-----------------------------------------------------------------------

	iCollideShape::~iCollideShape()
	{
		//Release Newton Collision
		if(mpNewtonCollision)
		{
			ohpl_nts_release(mhWorld, mpNewtonCollision);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iCollideShape* iCollideShape::GetSubShape(int alIdx)
	{
		if(mType == eCollideShapeType_Compound)
			return mvSubShapes[alIdx];
		else
			return this;
	}

	int iCollideShape::GetSubShapeNum()
	{
		if(mType == eCollideShapeType_Compound)
			return (int)mvSubShapes.size();
		else
			return 1;
	}

	//-----------------------------------------------------------------------

	const tVector3fVec& iCollideShape::GetBuoyancyPoints(float &afRadius)
	{
		if(mfBuoyancyPointRadius < 0)
		{
			cVector3f vPoints[512];
			size_t lNum = ohpl_newton_buoyancy_points(mType, &mvSize, vPoints, 512, &mfBuoyancyPointRadius);
			for(size_t i=0; i<lNum; ++i)
				mvBuoyancyPoints.push_back(vPoints[i]);
			for(cVector3f &vP : mvBuoyancyPoints)
				vP = cMath::MatrixMul(m_mtxOffset, vP);
		}
		afRadius = mfBuoyancyPointRadius;
		return mvBuoyancyPoints;
	}

	//-----------------------------------------------------------------------

	cVector3f iCollideShape::GetInertia(float afMass)
	{
		cVector3f vInertia(1,1,1);

		// Box
		if(mType == eCollideShapeType_Box)
		{
			float fInv = 1.0f / 12.0f;
			vInertia = cVector3f(
						afMass * (mvSize.y * mvSize.y + mvSize.z * mvSize.z) * fInv,
						afMass * (mvSize.x * mvSize.x + mvSize.z * mvSize.z) * fInv,
						afMass * (mvSize.x * mvSize.x + mvSize.y * mvSize.y) * fInv
						);
		}
		// Sphere
		else if(mType == eCollideShapeType_Sphere)
		{
			float fI = 0.4f * afMass * mvSize.x * mvSize.x;
			vInertia = cVector3f(fI, fI,fI);
		}
		//Cylinder
		else if(mType == eCollideShapeType_Cylinder)
		{
			float fRadius = mvSize.x;
			vInertia.x = afMass * (fRadius*fRadius) * (1.0f/4.0f) + afMass * (mvSize.y*mvSize.y) * (1.0f / 12.0f);
			vInertia.y = vInertia.x;
			vInertia.z = afMass * (fRadius*fRadius) * (1.0f/2.0f);
		}
		//Capsule
		else if(mType == eCollideShapeType_Capsule)
		{
			float fRadius = mvSize.x;
			vInertia.x = afMass * (fRadius*fRadius) * (1.0f/4.0f) + afMass * (mvSize.y*mvSize.y) * (1.0f / 12.0f);
			vInertia.y = vInertia.x;
			vInertia.z = afMass * (fRadius*fRadius) * (1.0f/2.0f);
		}
		//Compound
		//This is only a very bad approximation.
		else if(mType == eCollideShapeType_Compound)
		{
			cVector3f vBoxSize = mBoundingVolume.GetSize();
			float fBoxVolume = vBoxSize.x * vBoxSize.y * vBoxSize.z;

			float fInv = 1.0f / 12.0f;
			vInertia = cVector3f(
				afMass * (vBoxSize.y * vBoxSize.y + vBoxSize.z * vBoxSize.z) * fInv,
				afMass * (vBoxSize.x * vBoxSize.x + vBoxSize.z * vBoxSize.z) * fInv,
				afMass * (vBoxSize.x * vBoxSize.x + vBoxSize.y * vBoxSize.y) * fInv
				);
			//Scale of a bit of the inertia since the compound is not a 100% solid box
			vInertia = vInertia * (1.0f - ((fBoxVolume / mfVolume)*0.3f));
		}

		return vInertia;
	}

	//-----------------------------------------------------------------------

	void iCollideShape::CreateStaticSceneFromShapeVec(tCollideShapeVec &avShapes, tMatrixfVec *apMatrices)
	{
		std::vector<NewtonCollision*> vColliders;
		for(size_t i=0; i<avShapes.size(); ++i)
		{
			mvSubShapes.push_back(avShapes[i]);
			vColliders.push_back(static_cast<iCollideShape*>(avShapes[i])->GetNewtonCollision());
		}
		mpNewtonCollision = ohpl_nts_static_scene(mhWorld, vColliders.data(), vColliders.size());
	}


	//-----------------------------------------------------------------------


	void iCollideShape::CreateCompoundFromShapeVec(tCollideShapeVec &avShapes)
	{
		std::vector<NewtonCollision*> vNewtonColliders;

		vNewtonColliders.reserve(avShapes.size());
		mvSubShapes.reserve(avShapes.size());

		mfVolume =0;

		for(size_t i=0; i< avShapes.size(); i++)
		{
			mvSubShapes.push_back(avShapes[i]);

			iCollideShape *pNewtonShape = static_cast<iCollideShape*>(avShapes[i]);
			vNewtonColliders.push_back(pNewtonShape->GetNewtonCollision());

			mfVolume += pNewtonShape->GetVolume();
		}

		mpNewtonCollision = ohpl_nts_compound(mhWorld, vNewtonColliders.data(), vNewtonColliders.size());

		// Create bounding volume
		cVector3f vFinalMax = avShapes[0]->GetBoundingVolume().GetMax();
		cVector3f vFinalMin = avShapes[0]->GetBoundingVolume().GetMin();

		for(size_t i=1; i< avShapes.size(); i++)
		{
			cVector3f vMax = avShapes[i]->GetBoundingVolume().GetMax();
			cVector3f vMin = avShapes[i]->GetBoundingVolume().GetMin();

			if(vFinalMax.x < vMax.x) vFinalMax.x = vMax.x;
			if(vFinalMin.x > vMin.x) vFinalMin.x = vMin.x;

			if(vFinalMax.y < vMax.y) vFinalMax.y = vMax.y;
			if(vFinalMin.y > vMin.y) vFinalMin.y = vMin.y;

			if(vFinalMax.z < vMax.z) vFinalMax.z = vMax.z;
			if(vFinalMin.z > vMin.z) vFinalMin.z = vMin.z;
		}

		mBoundingVolume.SetLocalMinMax(vFinalMin, vFinalMax);

	}

	//-----------------------------------------------------------------------

	void iCollideShape::CreateFromVertices(	const unsigned int* apIndexArray, int alIndexNum,
													const float *apVertexArray, int alVtxStride, int alVtxNum)
	{
		tWString sCacheFile;
		if(alIndexNum >= 6000)
		{
			uint64_t lHash = 0xcbf29ce484222325ULL ^ 1;
			auto Mix = [&lHash](const void* apData, size_t alSize) {
				for(size_t i = 0; i < alSize; ++i) lHash = (lHash ^ ((const unsigned char*)apData)[i]) * 0x100000001b3ULL;
			};
			Mix(apIndexArray, sizeof(unsigned int) * alIndexNum);
			Mix(apVertexArray, sizeof(float) * alVtxStride * alVtxNum);
			char sHash[17];
			snprintf(sHash, sizeof(sHash), "%016llx", (unsigned long long)lHash);
			sCacheFile = cResources::GetCacheFile(_W("collision/") + cString::To16Char(sHash), _W("ncol"));

			cBinaryBuffer cacheBuf;
			if(cPlatform::FileExists(sCacheFile) && cacheBuf.Load(sCacheFile))
			{
				CreateFromSerializedData(&cacheBuf);
				if(mpNewtonCollision) return;
			}
		}

		mpNewtonCollision = ohpl_nts_tree(mhWorld, apIndexArray, alIndexNum, apVertexArray, alVtxStride, alVtxNum);


		//Set bounding box size
		mBoundingVolume.AddArrayPoints(apVertexArray, alVtxNum);
		mBoundingVolume.CreateFromPoints(alVtxStride);

		if(sCacheFile.empty() == false)
		{
			cBinaryBuffer cacheBuf;
			SaveToSerializedData(&cacheBuf);
			tWString sTmp = sCacheFile + _W(".") + cString::ToStringW((int)getpid());
			if(cacheBuf.Save(sTmp)) std::rename(cString::To8Char(sTmp).c_str(), cString::To8Char(sCacheFile).c_str());
		}
	}

	//-----------------------------------------------------------------------

	void iCollideShape::CreateHeightField(int alSize, const unsigned short* apElevation, float afUnitSize, float afVerticalScale)
	{
		mpNewtonCollision = ohpl_nts_height_field(mhWorld, alSize, apElevation, afUnitSize, afVerticalScale);
		const unsigned short* pEnd = apElevation + (size_t)alSize * alSize;
		mBoundingVolume.SetLocalMinMax(cVector3f(0, *std::min_element(apElevation, pEnd) * afVerticalScale, 0),
									   cVector3f((alSize - 1) * afUnitSize, *std::max_element(apElevation, pEnd) * afVerticalScale, (alSize - 1) * afUnitSize));
	}

	//-----------------------------------------------------------------------

	void iCollideShape::SaveToSerializedData(cBinaryBuffer* apBinBuffer)
	{
		apBinBuffer->AddVector3f(mBoundingVolume.GetLocalMin());
		apBinBuffer->AddVector3f(mBoundingVolume.GetLocalMax());
		ohpl_nts_serialize(mhWorld, mpNewtonCollision, (size_t)apBinBuffer);
	}

	//-----------------------------------------------------------------------

	void iCollideShape::CreateFromSerializedData(cBinaryBuffer* apBinBuffer)
	{
		cVector3f vMin, vMax;
		apBinBuffer->GetVector3f(&vMin);
		apBinBuffer->GetVector3f(&vMax);

		mpNewtonCollision = ohpl_nts_deserialize(mhWorld, (size_t)apBinBuffer);

		mBoundingVolume.SetLocalMinMax(vMin, vMax);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

}
