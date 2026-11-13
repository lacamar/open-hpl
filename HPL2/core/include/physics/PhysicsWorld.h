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

#ifndef HPL_PHYSICS_WORLD_H
#define HPL_PHYSICS_WORLD_H

#include <map>
#include <cstdint>
#include "system/SystemTypes.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "physics/PhysicsTypes.h"

#include "physics/CollideData.h"

#include "engine/SaveGame.h"

class NewtonWorld;

namespace hpl {

	class iCollideShape;
	class iVertexBuffer;
	class iPhysicsBody;
	class iLowLevelGraphics;
	class iPhysicsMaterial;
	class iCharacterBody;
	class iPhysicsJoint;
	class iPhysicsJointBall;
	class iPhysicsJointHinge;
	class iPhysicsJointScrew;
	class iPhysicsJointSlider;
	class iPhysicsController;
	class iPhysicsRope;
	class cBinaryBuffer;

	class cWorld;
	class cBoundingVolume;

	typedef std::list<iCollideShape*> tCollideShapeList;
	typedef tCollideShapeList::iterator tCollideShapeListIt;

	typedef std::vector<iCollideShape*> tCollideShapeVec;
	typedef tCollideShapeVec::iterator tCollideShapeVecIt;

	typedef std::list<iPhysicsBody*> tPhysicsBodyList;
	typedef tPhysicsBodyList::iterator tPhysicsBodyListIt;

	typedef std::set<iPhysicsBody*> tPhysicsBodySet;
	typedef tPhysicsBodySet::iterator tPhysicsBodySetIt;

	typedef std::list<iPhysicsJoint*> tPhysicsJointList;
	typedef tPhysicsJointList::iterator tPhysicsJointListIt;

	typedef std::list<iPhysicsController*> tPhysicsControllerList;
	typedef tPhysicsControllerList::iterator tPhysicsControllerListIt;

	typedef std::list<iCharacterBody*> tCharacterBodyList;
	typedef tCharacterBodyList::iterator tCharacterBodyListIt;

	typedef std::list<iPhysicsRope*> tPhysicsRopeList;
	typedef tPhysicsRopeList::iterator tPhysicsRopeListIt;

	typedef std::map<tString, iPhysicsMaterial*> tPhysicsMaterialMap;
	typedef tPhysicsMaterialMap::iterator tPhysicsMaterialMapIt;

	typedef cSTLMapIterator<iPhysicsMaterial*, tPhysicsMaterialMap, tPhysicsMaterialMapIt> cPhysicsMaterialIterator;

	typedef cSTLIterator<iPhysicsBody*, tPhysicsBodyList, tPhysicsBodyListIt> cPhysicsBodyIterator;
	typedef cSTLIterator<iPhysicsJoint*, tPhysicsJointList, tPhysicsJointListIt> cPhysicsJointIterator;

	//----------------------------------------------------

	class iPhysicsWorld
	{
	public:
		iPhysicsWorld();
		virtual ~iPhysicsWorld();

		//########################################################################################
		//! \name General
		//########################################################################################
		//! @{

		void Update(float afTimeStep);
		void Simulate(float afTimeStep);

		void  SetMaxTimeStep(float afTimeStep);
		float GetMaxTimeStep();

		void SetWorldSize(const cVector3f &avMin,const cVector3f &avMax);
		cVector3f GetWorldSizeMin();
		cVector3f GetWorldSizeMax();

		void SetGravity(const cVector3f& avGravity);
		cVector3f GetGravity();

		void SetAccuracyLevel(ePhysicsAccuracy aAccuracy);
		ePhysicsAccuracy GetAccuracyLevel();

		void SetNumberOfThreads(int alThreads);
		int GetNumberOfThreads();

		//! @}

		//########################################################################################
		//! \name Shapes
		//########################################################################################
		//! @{

		iCollideShape* CreateNullShape();
		iCollideShape* CreateBoxShape(const cVector3f &avSize, cMatrixf* apOffsetMtx);
		iCollideShape* CreateSphereShape(const cVector3f &avRadii, cMatrixf* apOffsetMtx);
		iCollideShape* CreateCylinderShape(float afRadius, float afHeight, cMatrixf* apOffsetMtx);
		iCollideShape* CreateCapsuleShape(float afRadius, float afHeight, cMatrixf* apOffsetMtx);

		iCollideShape* CreateMeshShape(iVertexBuffer *apVtxBuffer);
		iCollideShape* CreateHeightFieldShape(int alSize, const unsigned short* apElevation, float afUnitSize, float afVerticalScale);
		/**
		 * The buffer position must be pointing to where the data is saved!
		 */
		iCollideShape* LoadMeshShapeFromBuffer(cBinaryBuffer *apBuffer);
		/**
		 * The shape must be a mesh shape!
		 */
		void SaveMeshShapeToBuffer(iCollideShape* apMeshShape, cBinaryBuffer *apBuffer);

		iCollideShape* CreateCompundShape(tCollideShapeVec &avShapes);
		iCollideShape* CreateStaticSceneShape(tCollideShapeVec &avShapes, tMatrixfVec *apMatrices);
		void DestroyShape(iCollideShape *apShape);

		//! @}

		//########################################################################################
		//! \name Joints
		//########################################################################################
		//! @{

		iPhysicsJointBall* CreateJointBall(const tString &asName,
											const cVector3f& avPivotPoint,const cVector3f& avPinDir,
											iPhysicsBody* apParentBody, iPhysicsBody *apChildBody);
		iPhysicsJointHinge* CreateJointHinge(const tString &asName,
											const cVector3f& avPivotPoint,const cVector3f& avPinDir,
											iPhysicsBody* apParentBody, iPhysicsBody *apChildBody);
		iPhysicsJointSlider* CreateJointSlider(const tString &asName,
											const cVector3f& avPivotPoint,const cVector3f& avPinDir,
											iPhysicsBody* apParentBody, iPhysicsBody *apChildBody);
		iPhysicsJointScrew* CreateJointScrew(const tString &asName,
											const cVector3f& avPivotPoint,const cVector3f& avPinDir,
											iPhysicsBody* apParentBody, iPhysicsBody *apChildBody);
		void DestroyJoint(iPhysicsJoint* apJoint);
		iPhysicsJoint *GetJoint(const tString &asName);
		bool JointExists(iPhysicsJoint* apJoint);
		cPhysicsJointIterator GetJointIterator();

		//! @}

		//########################################################################################
		//! \name Materials
		//########################################################################################
		//! @{

		iPhysicsMaterial* CreateMaterial(const tString &asName);
		iPhysicsMaterial* GetMaterialFromName(const tString &asName);
		cPhysicsMaterialIterator GetMaterialIterator();

		//! @}

		//########################################################################################
		//! \name Bodies
		//########################################################################################
		//! @{

		iPhysicsBody* CreateBody(const tString &asName,iCollideShape *apShape);
		void DestroyBody(iPhysicsBody* apBody);
		iPhysicsBody *GetBody(const tString &asName);
		cPhysicsBodyIterator GetBodyIterator();

		iCharacterBody *CreateCharacterBody(const tString &asName, const cVector3f &avSize);
		void DestroyCharacterBody(iCharacterBody* apBody);
		iPhysicsBody *GetCharacterBody(const tString &asName);

		void GetBodiesInBV(cBoundingVolume *apBV, std::vector<iPhysicsBody*> *apBodyVec);
		void EnableBodiesInBV(cBoundingVolume *apBV, bool abEnabled);

		void AddBodyToUpdateList(iPhysicsBody *apBody);
		void RemoveBodyFromUpdateList(iPhysicsBody *apBody, bool abDestroyingBody);


		//! @}

		//########################################################################################
		//! \name Misc
		//########################################################################################
		//! @{

		iPhysicsRope* CreateRope(const tString &asName, const cVector3f &avStartPos, const cVector3f &avEndPos);
		iPhysicsRope* GetRope(const tString &asName);
		iPhysicsRope* GetRopeFromUniqueID(int alID);
		void DestroyRope(iPhysicsRope* apRope);


		//! @}

		//########################################################################################
		//! \name Tools
		//########################################################################################
		//! @{

		void SetLogDebug(bool abX){ mbLogDebug = abX;}
		bool GetLogDebug(){ return mbLogDebug;}

		iPhysicsController *CreateController(const tString &asName);
		void DestroyController(iPhysicsController *apController);

		void FadeoutAllLoopSounds(float afFadeSpeed);

		tCollidePointVec* GetContactPoints(){ return &mvContactPoints;}
		void SetSaveContactPoints(bool abX){ mbSaveContactPoints = abX;}
		bool GetSaveContactPoints(){ return mbSaveContactPoints;}
		void RenderContactPoints(iLowLevelGraphics *apLowLevel, const cColor& aPointColor, const cColor& aLineColor);

		void CastRay(iPhysicsRayCallback *apCallback,
							const cVector3f &avOrigin, const cVector3f& avEnd,
							bool abCalcDist, bool abCalcNormal, bool abCalcPoint,
							bool abUsePrefilter=false);

		void RenderShapeDebugGeometry(	iCollideShape *apShape, const cMatrixf& a_mtxTransform,
												iLowLevelGraphics *apLowLevel, const cColor& aColor);

		void RenderDebugGeometry(iLowLevelGraphics *apLowLevel, const cColor& aColor);

		bool CheckShapeCollision(	iCollideShape* apShapeA, const cMatrixf& a_mtxA,
										iCollideShape* apShapeB, const cMatrixf& a_mtxB,
										cCollideData & aCollideData, int alMaxPoints,
										bool abCorrectNormalDirection);
		float CastRayOnShape(iCollideShape* apShape, const cMatrixf& a_mtxTransform, const cVector3f& avOrigin, const cVector3f& avEnd,
									cVector3f& avOutNormal, bool abLocalRayPositions);

		bool CheckShapeWorldCollision(	cVector3f *apPushVector,
										iCollideShape* apShape, const cMatrixf& a_mtxTransform,
										iPhysicsBody *apSkipBody=NULL, bool abSkipStatic=false,
										bool abIsCharacter=false,
										iPhysicsWorldCollisionCallback *apCallback=NULL,
										bool abCollideCharacter=true,
										int alMinPushStrength=0,
										tFlag alCollideFlags = eFlagBit_All,
										bool abDebug=false);

		void DestroyAll();

		std::vector<iPhysicsBody*>& GetTempBodies(){ return mvTempBodies;}
		void FadeoutBodySounds(iPhysicsBody *apBody, float afFadeSpeed);
		void FadeoutJointSound(iPhysicsJoint *apJoint, float afFadeSpeed);
		void RenderContactPoint(iLowLevelGraphics *apLowLevel, size_t alIdx, const cColor& aPointColor, const cColor& aLineColor);

		NewtonWorld* GetNewtonWorld(){ return mpNewtonWorld;}
		uint64_t RustHandle(){ return mhRust;}
		cWorld* GetWorld(){ return mpWorld;}
		void SetWorld(cWorld *apWorld){ mpWorld = apWorld;}
		//! @}

	protected:
		void RegisterShape(iCollideShape *apShape);
		void RegisterBody(iPhysicsBody *apBody);
		void RegisterCharBody(iCharacterBody *apBody);
		void RegisterJoint(iPhysicsJoint *apJoint);
		void RegisterController(iPhysicsController *apController);
		void RegisterRope(iPhysicsRope *apRope);
		void RegisterMaterial(const tString &asName, iPhysicsMaterial *apMaterial);
		tPhysicsBodyList& BodyList();

		tPhysicsBodyList mlstBodies;
		uint64_t mRsBook;
		tPhysicsMaterialMap m_mapMaterials;
		tPhysicsJointList mlstJoints;
		cWorld *mpWorld;

		std::vector<iPhysicsBody*> mvTempBodies;

		bool mbLogDebug;

		tCollidePointVec mvContactPoints;
		bool mbSaveContactPoints;
		NewtonWorld *mpNewtonWorld;
		uint64_t mhRust = 0;
		cVector3f mvWorldSizeMin;
		cVector3f mvWorldSizeMax;
		cVector3f mvGravity;
		float mfMaxTimeStep;
		ePhysicsAccuracy mAccuracy;
	};
};
#endif // HPL_PHYSICS_WORLD_H

