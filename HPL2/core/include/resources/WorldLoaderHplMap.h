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

#ifndef HPL_WORLD_LOADER_HPL_MAP_H
#define HPL_WORLD_LOADER_HPL_MAP_H

#include "resources/WorldLoader.h"

#include "resources/ResourcesTypes.h"
#include "scene/SceneTypes.h"
#include "graphics/GraphicsTypes.h"
#include "physics/PhysicsTypes.h"
#include "resources/HplMapBuilder.h"

namespace hpl {

	class iEntity3D;
	class cXmlElement;
	class iPhysicsWorld;
	class cRenderableContainer_BoxTree;
	class cSubMeshEntity;
	class iPhysicsMaterial;
	class cResourceVarsObject;
	class iPhysicsBody;
	class cBinaryBuffer;

	//----------------------------------------

#if (defined(__ppc__) || defined(__PPC__)) && !defined(__LP64__)
	// PPC here
	#define MAP_CACHE_FORMAT_MAGIC_NUMBER		0xF4414550
#else
	// Only need to specialize PPC as 64bit and 32bit newton data is the same
	#define MAP_CACHE_FORMAT_MAGIC_NUMBER		0xF441451F
#endif

	#define MAP_CACHE_FORMAT_VERSION			13
	
	//----------------------------------------
	
	class cHplMapStaticUserData
	{
	public:
		cVector3f mvScale;
		bool mbCollides;
		bool mbCharCollider;
		bool mbCombine;
		bool mbVisible;
	};

	typedef std::list<cHplMapStaticUserData*> tHplMapStaticUserDataList;
	typedef tHplMapStaticUserDataList::iterator tHplMapStaticUserDataListIt;

	
	//----------------------------------------
	
	class cHplMapPhysicsObject
	{
	public:
		cSubMeshEntity *mpObject;
		iPhysicsMaterial *mpPhysicsMaterial;
		cHplMapStaticUserData *mpUserData;
		bool mbCharCollider;
	};

	//----------------------------------------

	class cHplMapShape
	{
	public:
		eCollideShapeType mType;
		cVector3f mvSize;
		cMatrixf m_mtxOffset;
	};

	typedef std::vector<cHplMapShape*> tHplMapShapeVec;
	typedef tHplMapShapeVec::iterator tHplMapShapeVecIt;

	//----------------------------------------
	
	class cHplMapShapeBody
	{
	public:
		cHplMapShapeBody();
		~cHplMapShapeBody();

		tString msMaterial;
		bool mbCharCollider;
		bool mbBlocksLight;
		cMatrixf m_mtxTransform;
		
		tHplMapShapeVec mvColliders;
	};
	
	typedef std::list<cHplMapShapeBody*> tHplMapShapeBodyList;
	typedef tHplMapShapeBodyList::iterator tHplMapShapeBodyListIt;

	//----------------------------------------

	
	class cWorldLoaderHplMap : public iWorldLoader
	{
	public:
		cWorldLoaderHplMap();
		~cWorldLoaderHplMap();

			
		cWorld* LoadWorld(const tWString& asFile, tWorldLoadFlag aFlags);

	private:
		void LoadCacheFile(const tWString& asFile);
		void SaveCacheFile(const tWString& asFile);

		void FillBuilder(OhplMapBuilder *apBuilder);
		void OnPhase(int alPhase, bool abBegin);

		void StaticBegin();
		void StaticCompile();
		void BuildCombinedStaticMeshes(cRenderableContainer_BoxTree *apContainer);
		void CreateStaticObjectEntity(	const tString& asName, const tString& asFileName, const cVector3f& avPosition,
										const cVector3f& avRotation, const cVector3f& avScale, bool abCollides,
										bool abCastsShadows, int alID,
										tMeshEntityList& alstMeshEntities, cRenderableContainer_BoxTree *apContainer);
		void CreatePlane(	const tString& asName, const tString& asMaterial, bool abCastsShadows, bool abCollides, int alID,
							const cVector3f& avPosition, const cVector3f& avScale, const cVector3f& avRotation,
							const cVector3f& avStartCorner, const cVector3f& avEndCorner, const cVector2f *apUV,
							tMeshEntityList& alstMeshEntities, cRenderableContainer_BoxTree *apContainer);
		void CreateDecal(	const tString& asName, const cColor& aDecalColor, int alID, const tString& asMaterial,
							cXmlElement* apDecalMeshElem, tMeshEntityList& alstMeshEntities,
							cRenderableContainer_BoxTree *apDecalContainer);
		void CreateStaticObjectCombo(	int alGroupID, const int *apObjIds, size_t alObjIdNum, tMeshEntityList& alstMeshEntities,
										cRenderableContainer_BoxTree *apContainer);

		void IterateLeafNodesAndBuildMeshes(iRenderableContainerNode *apNode);
		void CombineAndCreateMeshesAndPhysics(tRenderableList *apObjectList);
		void CombineObjectsAndCreateMeshEntity(tRenderableVec &avObjects, int alFirstIdx, int alLastIdx);
		void CombineObjectsAndCreatePhysics(std::vector<cHplMapPhysicsObject> &avObjects, int alFirstIdx, int alLastIdx);

		void EntitiesBegin();
		void EntitiesEnd();
		void CreateSubMeshShapeBodies(cSubMeshEntity *apSubEnt, const cMatrixf &a_mtxTransform, const cVector3f& avScale);
		void CreateShapeBody(cHplMapShapeBody* apShapeBody);

		void LoadEntity(const tString& asName, int alID, bool abActive,const cVector3f& avPos, const cVector3f& avRot, const cVector3f& avScale, const tString& asFilename, cXmlElement* apUserVars);
		void LoadArea(const tString& asName, int alID, bool abActive,const cVector3f& avPos, const cVector3f& avRot,const cVector3f& avScale, const tString& asType, cXmlElement* apUserVars);

		int mlCombinedMeshNameCount;
		int mlCombinedBodyNameCount;

		bool mbLoadedCache;

		unsigned long mlPhaseStart[6];

		int mlSortingTimeTotal;
		int mlCombineMeshTimeTotal;
		int mlCombineBodyTimeTotal;

		cWorld* mpCurrentWorld;
		iPhysicsWorld *mpCurrentPhysicsWorld;
		cBinaryBuffer *mpCacheBuffer;

		cRenderableContainer_BoxTree *mpStaticContainer;
		tMeshEntityList mlstStaticTempEntities;
		tEFL_LightBillboardConnectionList mlstLightBillboardConnections;

		std::list<iPhysicsBody*> mlstStaticMeshBodies;
		tHplMapShapeBodyList mlstStaticShapeBodies;
		tMeshEntityList mlstStaticMeshEntities;
		
		int mlStaticMeshBodiesCreated;
		int mlStaticMeshEntitiesCreated;

		tWString msCacheFileExt;
		tWString msCurrentFile;

		tWorldLoadFlag mlCurrentFlags;
		tHplMapStaticUserDataList mlstTempStaticUserData;
	};

};
#endif // HPL_WORLD_LOADER_HPL_MAP_H
