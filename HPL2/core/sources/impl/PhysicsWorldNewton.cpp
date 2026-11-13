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

#include "impl/PhysicsWorldNewton.h"

#include "impl/CollideShapeNewton.h"
#include "impl/PhysicsBodyNewton.h"
#include "impl/PhysicsMaterialNewton.h"
#include "impl/CharacterBodyNewton.h"
#include "impl/PhysicsRopeNewton.h"

#include "impl/PhysicsJointBallNewton.h"
#include "impl/PhysicsJointHingeNewton.h"
#include "impl/PhysicsJointScrewNewton.h"
#include "impl/PhysicsJointSliderNewton.h"

#include "impl/PhysicsControllerNewton.h"
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

	cPhysicsWorldNewton::cPhysicsWorldNewton()
		: iPhysicsWorld()
	{
		OhplL4InstallHost();
		mhRust = ohpl_ntw_create(&mpNewtonWorld);

		if(mpNewtonWorld==NULL){
			Warning("Couldn't create newton world!\n");
		}
		
		/////////////////////////////////
        //Set default values to properties
		mvWorldSizeMin = cVector3f(0,0,0);
		mvWorldSizeMax = cVector3f(0,0,0);

		mvGravity = cVector3f(0,-9.81f,0);
		mfMaxTimeStep = 1.0f/60.0f;
		
		/////////////////////////////////
		//Create default material.
		int lDefaultMatId = 0;//NewtonMaterialGetDefaultGroupID(mpNewtonWorld);
		cPhysicsMaterialNewton *pMaterial = hplNew( cPhysicsMaterialNewton, ("Default",this,lDefaultMatId) );
		RegisterMaterial("Default", pMaterial);
		pMaterial->UpdateMaterials();
	}

	//-----------------------------------------------------------------------

	cPhysicsWorldNewton::~cPhysicsWorldNewton()
	{
		DestroyAll();
		ohpl_ntw_destroy(mhRust);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::Simulate(float afTimeStep)
	{
		ohpl_ntw_simulate(mhRust, afTimeStep);
	}

	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::SetMaxTimeStep(float afTimeStep)
	{
		mfMaxTimeStep = afTimeStep;
		ohpl_ntw_set_max_step(mhRust, afTimeStep);
	}
	
	float cPhysicsWorldNewton::GetMaxTimeStep()
	{
		return mfMaxTimeStep;
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::SetWorldSize(const cVector3f &avMin,const cVector3f &avMax)
	{
		mvWorldSizeMin = avMin;
		mvWorldSizeMax = avMax;

		ohpl_ntw_set_size(mhRust, avMin.v, avMax.v);
	}

	cVector3f cPhysicsWorldNewton::GetWorldSizeMin()
	{
		return mvWorldSizeMin;
	}

	cVector3f cPhysicsWorldNewton::GetWorldSizeMax()
	{
		return mvWorldSizeMax;
	}

	
	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::SetGravity(const cVector3f& avGravity)
	{
		mvGravity = avGravity;
	}
	
	//-----------------------------------------------------------------------

	cVector3f cPhysicsWorldNewton::GetGravity()
	{
		return mvGravity;
	}

	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::SetAccuracyLevel(ePhysicsAccuracy aAccuracy)
	{
		mAccuracy = aAccuracy;

		ohpl_ntw_set_accuracy(mhRust, (int32_t)aAccuracy);
	}

	//-----------------------------------------------------------------------

	ePhysicsAccuracy cPhysicsWorldNewton::GetAccuracyLevel()
	{
		return mAccuracy;
	}

	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::SetNumberOfThreads(int alThreads)
	{
		ohpl_ntw_set_threads(mhRust, alThreads);
	}
	
	int cPhysicsWorldNewton::GetNumberOfThreads()
	{
		return ohpl_ntw_threads(mhRust);
	}
	
	//-----------------------------------------------------------------------
	
	iCollideShape* cPhysicsWorldNewton::CreateNullShape()
	{
		iCollideShape *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Null, 0, NULL, 
													mpNewtonWorld, this) );
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* cPhysicsWorldNewton::CreateBoxShape(const cVector3f &avSize, cMatrixf* apOffsetMtx)
	{
		iCollideShape *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Box, avSize, apOffsetMtx, 
													mpNewtonWorld, this) );
		RegisterShape(pShape);
		
		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* cPhysicsWorldNewton::CreateSphereShape(const cVector3f &avRadii, cMatrixf* apOffsetMtx)
	{
		iCollideShape *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Sphere, avRadii, apOffsetMtx, 
														mpNewtonWorld, this) ); 
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* cPhysicsWorldNewton::CreateCylinderShape(float afRadius, float afHeight, cMatrixf* apOffsetMtx)
	{
		iCollideShape *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Cylinder, 
													cVector3f(afRadius,afHeight,afRadius), 
													apOffsetMtx, 
													mpNewtonWorld, this) );
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* cPhysicsWorldNewton::CreateCapsuleShape(float afRadius, float afHeight, cMatrixf* apOffsetMtx)
	{
		iCollideShape *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Capsule, 
													cVector3f(afRadius,afHeight,afRadius), 
													apOffsetMtx, 
													mpNewtonWorld, this) );
		RegisterShape(pShape);

		return pShape;
	}
	
	//-----------------------------------------------------------------------

	iCollideShape* cPhysicsWorldNewton::CreateMeshShape(iVertexBuffer *apVtxBuffer)
	{
		cCollideShapeNewton *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Mesh, 0, NULL, mpNewtonWorld,this) );
		
		pShape->CreateFromVertices(apVtxBuffer->GetIndices(),apVtxBuffer->GetIndexNum(),
									apVtxBuffer->GetFloatArray(eVertexBufferElement_Position),
									apVtxBuffer->GetElementNum(eVertexBufferElement_Position),
									apVtxBuffer->GetVertexNum());
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* cPhysicsWorldNewton::CreateHeightFieldShape(int alSize, const unsigned short* apElevation, float afUnitSize, float afVerticalScale)
	{
		cCollideShapeNewton *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Mesh, 0, NULL, mpNewtonWorld,this) );
		pShape->CreateHeightField(alSize, apElevation, afUnitSize, afVerticalScale);
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* cPhysicsWorldNewton::LoadMeshShapeFromBuffer(cBinaryBuffer *apBuffer)
	{
		cCollideShapeNewton *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Mesh,0, NULL, mpNewtonWorld,this) );
		
		pShape->CreateFromSerializedData(apBuffer);
		
		RegisterShape(pShape);

		return pShape;
	}
	
	//-----------------------------------------------------------------------

	
	void cPhysicsWorldNewton::SaveMeshShapeToBuffer(iCollideShape* apMeshShape, cBinaryBuffer *apBuffer)
	{
		cCollideShapeNewton *pNewtonMeshShape = static_cast<cCollideShapeNewton*>(apMeshShape);
		pNewtonMeshShape->SaveToSerializedData(apBuffer);
	}
	
	//-----------------------------------------------------------------------
	
	iCollideShape* cPhysicsWorldNewton::CreateCompundShape(tCollideShapeVec &avShapes)
	{
		cCollideShapeNewton *pShape = hplNew( cCollideShapeNewton, (eCollideShapeType_Compound,0, NULL, mpNewtonWorld,this) );
		pShape->CreateCompoundFromShapeVec(avShapes);
		RegisterShape(pShape);

		return pShape;
	}

	//-----------------------------------------------------------------------

	iCollideShape* cPhysicsWorldNewton::CreateStaticSceneShape(tCollideShapeVec &avShapes, tMatrixfVec *apMatrices)
	{
		cCollideShapeNewton *pShape = hplNew(	cCollideShapeNewton, (eCollideShapeType_StaticScene,0, NULL, mpNewtonWorld,this) );

        pShape->CreateStaticSceneFromShapeVec(avShapes, apMatrices);
		RegisterShape(pShape);
		return pShape;
	}

	//-----------------------------------------------------------------------

	iPhysicsJointBall* cPhysicsWorldNewton::CreateJointBall(const tString &asName, 
										const cVector3f& avPivotPoint,const cVector3f& avPinDir,
										iPhysicsBody* apParentBody, iPhysicsBody *apChildBody)
	{
		iPhysicsJointBall *pJoint = hplNew( cPhysicsJointBallNewton, (asName,apParentBody,apChildBody,this,
														avPivotPoint,avPinDir) );
		RegisterJoint(pJoint);
		return pJoint;
	}

	iPhysicsJointHinge* cPhysicsWorldNewton::CreateJointHinge(const tString &asName,
										const cVector3f& avPivotPoint,const cVector3f& avPinDir,
										iPhysicsBody* apParentBody, iPhysicsBody *apChildBody)
	{
		iPhysicsJointHinge *pJoint = hplNew( cPhysicsJointHingeNewton, (asName,apParentBody,apChildBody,this,
										avPivotPoint,avPinDir) );
		RegisterJoint(pJoint);
		return pJoint;
	}

	iPhysicsJointSlider* cPhysicsWorldNewton::CreateJointSlider(const tString &asName,
										const cVector3f& avPivotPoint,const cVector3f& avPinDir,			
										iPhysicsBody* apParentBody, iPhysicsBody *apChildBody)
	{
		iPhysicsJointSlider *pJoint = hplNew( cPhysicsJointSliderNewton, (asName,apParentBody,apChildBody,this,
											avPivotPoint,avPinDir) );
		RegisterJoint(pJoint);
		return pJoint;
	}

	iPhysicsJointScrew* cPhysicsWorldNewton::CreateJointScrew(const tString &asName,
										const cVector3f& avPivotPoint,const cVector3f& avPinDir,
										iPhysicsBody* apParentBody, iPhysicsBody *apChildBody)
	{
		iPhysicsJointScrew *pJoint = hplNew( cPhysicsJointScrewNewton, (asName,apParentBody,apChildBody,this,
											avPivotPoint,avPinDir) );
		RegisterJoint(pJoint);
		return pJoint;
	}

	//-----------------------------------------------------------------------

	iPhysicsBody* cPhysicsWorldNewton::CreateBody(const tString &asName,iCollideShape *apShape)
	{
		cPhysicsBodyNewton *pBody = hplNew( cPhysicsBodyNewton, (asName,this, apShape) );

		RegisterBody(pBody);

		return pBody;
	}

	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::GetBodiesInBV(cBoundingVolume *apBV, std::vector<iPhysicsBody*> *apBodyVec)
	{
		ohpl_ntw_bodies_in_aabb(mhRust, apBV->GetMin().v, apBV->GetMax().v, (size_t)apBodyVec);
	}
	
	//-----------------------------------------------------------------------

	iCharacterBody* cPhysicsWorldNewton::CreateCharacterBody(const tString &asName, const cVector3f &avSize)
	{
		cCharacterBodyNewton *pChar = hplNew( cCharacterBodyNewton, (asName,this,avSize) );

		RegisterCharBody(pChar);

		return pChar;
	}

	//-----------------------------------------------------------------------

	iPhysicsMaterial* cPhysicsWorldNewton::CreateMaterial(const tString &asName)
	{
		cPhysicsMaterialNewton *pMaterial = hplNew( cPhysicsMaterialNewton, (asName,this) );
		
		RegisterMaterial(asName, pMaterial);
		
		pMaterial->UpdateMaterials();

		return pMaterial;
	}

	//-----------------------------------------------------------------------

	iPhysicsController* cPhysicsWorldNewton::CreateController(const tString &asName)
	{
		iPhysicsController* pController = hplNew( cPhysicsControllerNewton, (asName, this) );	

		RegisterController(pController);

		return pController;
	}

	//-----------------------------------------------------------------------

	iPhysicsRope* cPhysicsWorldNewton::CreateRope(const tString &asName, const cVector3f &avStartPos, const cVector3f &avEndPos)
	{
		iPhysicsRope *pRope = hplNew( cPhysicsRopeNewton, (asName, this,avStartPos, avEndPos));
		
		RegisterRope(pRope);

		return pRope;
	}

	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::CastRay(iPhysicsRayCallback *apCallback, 
								const cVector3f &avOrigin, const cVector3f& avEnd, 
								bool abCalcDist, bool abCalcNormal,bool abCalcPoint,
								bool abUsePrefilter)
	{
		ohpl_ntw_cast_ray(mhRust, (size_t)apCallback, &avOrigin, &avEnd, abCalcDist, abCalcNormal, abCalcPoint, abUsePrefilter);
	}

	//-----------------------------------------------------------------------

	float cPhysicsWorldNewton::CastRayOnShape(iCollideShape* apShape, const cMatrixf& a_mtxTransform, const cVector3f& avOrigin, const cVector3f& avEnd,
											cVector3f& avOutNormal, bool abLocalRayPositions)
	{
		return ohpl_ntw_cast_ray_on_shape(static_cast<cCollideShapeNewton*>(apShape)->GetNewtonCollision(), &a_mtxTransform,
										&avOrigin, &avEnd, &avOutNormal, abLocalRayPositions);
	}

	bool cPhysicsWorldNewton::CheckShapeCollision(	iCollideShape* apShapeA, const cMatrixf& a_mtxA,
										iCollideShape* apShapeB, const cMatrixf& a_mtxB,
										cCollideData & aCollideData, int alMaxPoints,
										bool abCorrectNormalDirection)
	{
		cCollideShapeNewton *pNewtonShapeA = static_cast<cCollideShapeNewton*>(apShapeA);
		cCollideShapeNewton *pNewtonShapeB = static_cast<cCollideShapeNewton*>(apShapeB);

		bool bCompound = pNewtonShapeA->GetType() == eCollideShapeType_Compound ||
						pNewtonShapeB->GetType() == eCollideShapeType_Compound;
		std::vector<NewtonCollision*> vSubsA, vSubsB;
		if(bCompound)
		{
			for(int i=0; i<pNewtonShapeA->GetSubShapeNum(); ++i)
				vSubsA.push_back(static_cast<cCollideShapeNewton*>(pNewtonShapeA->GetSubShape(i))->GetNewtonCollision());
			for(int i=0; i<pNewtonShapeB->GetSubShapeNum(); ++i)
				vSubsB.push_back(static_cast<cCollideShapeNewton*>(pNewtonShapeB->GetSubShape(i))->GetNewtonCollision());
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

	void cPhysicsWorldNewton::RenderShapeDebugGeometry(	iCollideShape *apShape, const cMatrixf& a_mtxTransform, 
														iLowLevelGraphics *apLowLevel, const cColor& aColor)
	{
		ohpl_ntw_debug_shape(static_cast<cCollideShapeNewton*>(apShape)->GetNewtonCollision(), &a_mtxTransform,
							(size_t)apLowLevel, (size_t)&aColor);
	}
	
	//-----------------------------------------------------------------------

	void cPhysicsWorldNewton::RenderDebugGeometry(iLowLevelGraphics *apLowLevel,const cColor &aColor)
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
