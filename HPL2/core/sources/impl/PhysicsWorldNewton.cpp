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

#include "physics/PhysicsWorld.h"
#include "physics/CharacterBody.h"
#include "physics/PhysicsRope.h"
#include "physics/PhysicsController.h"

#include "physics/CollideShape.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsMaterial.h"

#include "physics/PhysicsJointBall.h"
#include "physics/PhysicsJointHinge.h"
#include "physics/PhysicsJointScrew.h"
#include "physics/PhysicsJointSlider.h"

#include "impl/RustNewtonBackend.h"

#include "scene/World.h"

#include "system/LowLevelSystem.h"
#include "graphics/VertexBuffer.h"
#include "graphics/LowLevelGraphics.h"
#include "math/Math.h"
#include "resources/BinaryBuffer.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iPhysicsWorld::Simulate(float afTimeStep)
	{
		ohpl_ntw_simulate(mhRust, afTimeStep);
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::SetMaxTimeStep(float afTimeStep)
	{
		mfMaxTimeStep = afTimeStep;
		ohpl_ntw_set_max_step(mhRust, afTimeStep);
	}

	float iPhysicsWorld::GetMaxTimeStep()
	{
		return mfMaxTimeStep;
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::SetWorldSize(const cVector3f &avMin,const cVector3f &avMax)
	{
		mvWorldSizeMin = avMin;
		mvWorldSizeMax = avMax;

		ohpl_ntw_set_size(mhRust, avMin.v, avMax.v);
	}

	cVector3f iPhysicsWorld::GetWorldSizeMin()
	{
		return mvWorldSizeMin;
	}

	cVector3f iPhysicsWorld::GetWorldSizeMax()
	{
		return mvWorldSizeMax;
	}


	//-----------------------------------------------------------------------

	void iPhysicsWorld::SetGravity(const cVector3f& avGravity)
	{
		mvGravity = avGravity;
	}

	//-----------------------------------------------------------------------

	cVector3f iPhysicsWorld::GetGravity()
	{
		return mvGravity;
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::SetAccuracyLevel(ePhysicsAccuracy aAccuracy)
	{
		mAccuracy = aAccuracy;

		ohpl_ntw_set_accuracy(mhRust, (int32_t)aAccuracy);
	}

	//-----------------------------------------------------------------------

	ePhysicsAccuracy iPhysicsWorld::GetAccuracyLevel()
	{
		return mAccuracy;
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::SetNumberOfThreads(int alThreads)
	{
		ohpl_ntw_set_threads(mhRust, alThreads);
	}

	int iPhysicsWorld::GetNumberOfThreads()
	{
		return ohpl_ntw_threads(mhRust);
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateNullShape()
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Null, 0, NULL,
													mpNewtonWorld, this) );
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateBoxShape(const cVector3f &avSize, cMatrixf* apOffsetMtx)
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Box, avSize, apOffsetMtx,
													mpNewtonWorld, this) );
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateSphereShape(const cVector3f &avRadii, cMatrixf* apOffsetMtx)
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Sphere, avRadii, apOffsetMtx,
														mpNewtonWorld, this) );
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateCylinderShape(float afRadius, float afHeight, cMatrixf* apOffsetMtx)
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Cylinder,
													cVector3f(afRadius,afHeight,afRadius),
													apOffsetMtx,
													mpNewtonWorld, this) );
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateCapsuleShape(float afRadius, float afHeight, cMatrixf* apOffsetMtx)
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Capsule,
													cVector3f(afRadius,afHeight,afRadius),
													apOffsetMtx,
													mpNewtonWorld, this) );
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateMeshShape(iVertexBuffer *apVtxBuffer)
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Mesh, 0, NULL, mpNewtonWorld,this) );

		pShape->CreateFromVertices(apVtxBuffer->GetIndices(),apVtxBuffer->GetIndexNum(),
									apVtxBuffer->GetFloatArray(eVertexBufferElement_Position),
									apVtxBuffer->GetElementNum(eVertexBufferElement_Position),
									apVtxBuffer->GetVertexNum());
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateHeightFieldShape(int alSize, const unsigned short* apElevation, float afUnitSize, float afVerticalScale)
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Mesh, 0, NULL, mpNewtonWorld,this) );
		pShape->CreateHeightField(alSize, apElevation, afUnitSize, afVerticalScale);
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::LoadMeshShapeFromBuffer(cBinaryBuffer *apBuffer)
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Mesh,0, NULL, mpNewtonWorld,this) );

		pShape->CreateFromSerializedData(apBuffer);

		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------


	void iPhysicsWorld::SaveMeshShapeToBuffer(iCollideShape* apMeshShape, cBinaryBuffer *apBuffer)
	{
		iCollideShape *pNewtonMeshShape = static_cast<iCollideShape*>(apMeshShape);
		pNewtonMeshShape->SaveToSerializedData(apBuffer);
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateCompundShape(tCollideShapeVec &avShapes)
	{
		iCollideShape *pShape = hplNew( iCollideShape, (eCollideShapeType_Compound,0, NULL, mpNewtonWorld,this) );
		pShape->CreateCompoundFromShapeVec(avShapes);
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* iPhysicsWorld::CreateStaticSceneShape(tCollideShapeVec &avShapes, tMatrixfVec *apMatrices)
	{
		iCollideShape *pShape = hplNew(	iCollideShape, (eCollideShapeType_StaticScene,0, NULL, mpNewtonWorld,this) );

        pShape->CreateStaticSceneFromShapeVec(avShapes, apMatrices);
		RegisterShape(pShape);
		return pShape;
	}

	//-----------------------------------------------------------------------

	iPhysicsJointBall* iPhysicsWorld::CreateJointBall(const tString &asName,
										const cVector3f& avPivotPoint,const cVector3f& avPinDir,
										iPhysicsBody* apParentBody, iPhysicsBody *apChildBody)
	{
		iPhysicsJointBall *pJoint = hplNew( iPhysicsJointBall, (asName,apParentBody,apChildBody,this,
														avPivotPoint,avPinDir) );
		RegisterJoint(pJoint);
		return pJoint;
	}

	iPhysicsJointHinge* iPhysicsWorld::CreateJointHinge(const tString &asName,
										const cVector3f& avPivotPoint,const cVector3f& avPinDir,
										iPhysicsBody* apParentBody, iPhysicsBody *apChildBody)
	{
		iPhysicsJointHinge *pJoint = hplNew( iPhysicsJointHinge, (asName,apParentBody,apChildBody,this,
										avPivotPoint,avPinDir) );
		RegisterJoint(pJoint);
		return pJoint;
	}

	iPhysicsJointSlider* iPhysicsWorld::CreateJointSlider(const tString &asName,
										const cVector3f& avPivotPoint,const cVector3f& avPinDir,
										iPhysicsBody* apParentBody, iPhysicsBody *apChildBody)
	{
		iPhysicsJointSlider *pJoint = hplNew( iPhysicsJointSlider, (asName,apParentBody,apChildBody,this,
											avPivotPoint,avPinDir) );
		RegisterJoint(pJoint);
		return pJoint;
	}

	iPhysicsJointScrew* iPhysicsWorld::CreateJointScrew(const tString &asName,
										const cVector3f& avPivotPoint,const cVector3f& avPinDir,
										iPhysicsBody* apParentBody, iPhysicsBody *apChildBody)
	{
		iPhysicsJointScrew *pJoint = hplNew( iPhysicsJointScrew, (asName,apParentBody,apChildBody,this,
											avPivotPoint,avPinDir) );
		RegisterJoint(pJoint);
		return pJoint;
	}

	//-----------------------------------------------------------------------

	iPhysicsBody* iPhysicsWorld::CreateBody(const tString &asName,iCollideShape *apShape)
	{
		iPhysicsBody *pBody = hplNew( iPhysicsBody, (asName,this, apShape) );

		RegisterBody(pBody);

		return pBody;
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::GetBodiesInBV(cBoundingVolume *apBV, std::vector<iPhysicsBody*> *apBodyVec)
	{
		ohpl_ntw_bodies_in_aabb(mhRust, apBV->GetMin().v, apBV->GetMax().v, (size_t)apBodyVec);
	}

	//-----------------------------------------------------------------------

	iCharacterBody* iPhysicsWorld::CreateCharacterBody(const tString &asName, const cVector3f &avSize)
	{
		iCharacterBody *pChar = hplNew( iCharacterBody, (asName,this,avSize) );

		RegisterCharBody(pChar);

		return pChar;
	}

	//-----------------------------------------------------------------------

	iPhysicsMaterial* iPhysicsWorld::CreateMaterial(const tString &asName)
	{
		iPhysicsMaterial *pMaterial = hplNew( iPhysicsMaterial, (asName,this) );

		RegisterMaterial(asName, pMaterial);

		pMaterial->UpdateMaterials();

		return pMaterial;
	}

	//-----------------------------------------------------------------------

	iPhysicsController* iPhysicsWorld::CreateController(const tString &asName)
	{
		iPhysicsController* pController = hplNew( iPhysicsController, (asName, this) );

		RegisterController(pController);

		return pController;
	}

	//-----------------------------------------------------------------------

	iPhysicsRope* iPhysicsWorld::CreateRope(const tString &asName, const cVector3f &avStartPos, const cVector3f &avEndPos)
	{
		iPhysicsRope *pRope = hplNew( iPhysicsRope, (asName, this,avStartPos, avEndPos));

		RegisterRope(pRope);

		return pRope;
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::CastRay(iPhysicsRayCallback *apCallback,
								const cVector3f &avOrigin, const cVector3f& avEnd,
								bool abCalcDist, bool abCalcNormal,bool abCalcPoint,
								bool abUsePrefilter)
	{
		ohpl_ntw_cast_ray(mhRust, (size_t)apCallback, &avOrigin, &avEnd, abCalcDist, abCalcNormal, abCalcPoint, abUsePrefilter);
	}

	//-----------------------------------------------------------------------

	float iPhysicsWorld::CastRayOnShape(iCollideShape* apShape, const cMatrixf& a_mtxTransform, const cVector3f& avOrigin, const cVector3f& avEnd,
											cVector3f& avOutNormal, bool abLocalRayPositions)
	{
		return ohpl_ntw_cast_ray_on_shape(static_cast<iCollideShape*>(apShape)->GetNewtonCollision(), &a_mtxTransform,
										&avOrigin, &avEnd, &avOutNormal, abLocalRayPositions);
	}

	bool iPhysicsWorld::CheckShapeCollision(	iCollideShape* apShapeA, const cMatrixf& a_mtxA,
										iCollideShape* apShapeB, const cMatrixf& a_mtxB,
										cCollideData & aCollideData, int alMaxPoints,
										bool abCorrectNormalDirection)
	{
		iCollideShape *pNewtonShapeA = static_cast<iCollideShape*>(apShapeA);
		iCollideShape *pNewtonShapeB = static_cast<iCollideShape*>(apShapeB);

		bool bCompound = pNewtonShapeA->GetType() == eCollideShapeType_Compound ||
						pNewtonShapeB->GetType() == eCollideShapeType_Compound;
		std::vector<NewtonCollision*> vSubsA, vSubsB;
		if(bCompound)
		{
			for(int i=0; i<pNewtonShapeA->GetSubShapeNum(); ++i)
				vSubsA.push_back(static_cast<iCollideShape*>(pNewtonShapeA->GetSubShape(i))->GetNewtonCollision());
			for(int i=0; i<pNewtonShapeB->GetSubShapeNum(); ++i)
				vSubsB.push_back(static_cast<iCollideShape*>(pNewtonShapeB->GetSubShape(i))->GetNewtonCollision());
		}
		else
		{
			vSubsA.push_back(pNewtonShapeA->GetNewtonCollision());
			vSubsB.push_back(pNewtonShapeB->GetNewtonCollision());
		}
		int32_t lCount = aCollideData.mlNumOfPoints;
		bool bHit = ohpl_ntw_check_collision(mhRust, vSubsA.data(), vSubsA.size(), vSubsB.data(), vSubsB.size(), bCompound,
							&a_mtxA, &a_mtxB, alMaxPoints, abCorrectNormalDirection, apShapeA->GetType() == eCollideShapeType_Mesh,
							reinterpret_cast<OhplL4CollPoint*>(aCollideData.mvContactPoints.data()),
							aCollideData.mvContactPoints.size(), &lCount);
		aCollideData.mlNumOfPoints = lCount;
		return bHit;
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::RenderShapeDebugGeometry(	iCollideShape *apShape, const cMatrixf& a_mtxTransform,
														iLowLevelGraphics *apLowLevel, const cColor& aColor)
	{
		ohpl_ntw_debug_shape(static_cast<iCollideShape*>(apShape)->GetNewtonCollision(), &a_mtxTransform,
							(size_t)apLowLevel, (size_t)&aColor);
	}

	//-----------------------------------------------------------------------

	void iPhysicsWorld::RenderDebugGeometry(iLowLevelGraphics *apLowLevel,const cColor &aColor)
	{
		tPhysicsBodyList &lstBodies = BodyList();
		tPhysicsBodyListIt it = lstBodies.begin();
		for(;it != lstBodies.end(); ++it)
		{
			iPhysicsBody *pBody = *it;
			pBody->RenderDebugGeometry(apLowLevel,aColor);
		}
	}

	//-----------------------------------------------------------------------
}
