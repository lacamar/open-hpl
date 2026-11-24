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

#include "resources/WorldLoaderHplMap.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"

#include "resources/Resources.h"
#include "resources/MeshManager.h"
#include "resources/MaterialManager.h"
#include "resources/TextureManager.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"
#include "resources/EngineFileLoading.h"
#include "resources/BinaryBuffer.h"
#include "resources/HplMapParse.h"

#include "scene/Scene.h"
#include "scene/World.h"
#include "scene/MeshEntity.h"
#include "scene/SubMeshEntity.h"
#include "scene/LightPoint.h"
#include "scene/LightSpot.h"
#include "scene/LightBox.h"
#include "scene/BillBoard.h"
#include "scene/SoundEntity.h"
#include "scene/ParticleSystem.h"
#include "scene/RenderableContainer_BoxTree.h"

#include "graphics/Graphics.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/MeshCreator.h"

#include "physics/Physics.h"
#include "physics/PhysicsWorld.h"
#include "physics/CollideShape.h"
#include "physics/PhysicsBody.h"

#include "math/Math.h"
#include "scene/RustSceneH13.h"
#include "impl/RustBackends.h"

#include <algorithm>
#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC DATA
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool gbLogCacheLoad = false;
	
	static bool gbLog = false;
	static bool gbLogTiming = true;

	#define kEncryptKey 0x4E5F16F0

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cHplMapShapeBody::cHplMapShapeBody()
	{

	}
	
	cHplMapShapeBody::~cHplMapShapeBody()
	{
		STLDeleteAll(mvColliders);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cWorldLoaderHplMap::cWorldLoaderHplMap()
	{
		AddSupportedExtension("map");
		AddSupportedExtension("cmap");

		mpCurrentWorld = NULL;
		mpCurrentPhysicsWorld = NULL; 
		mpCacheBuffer = NULL;
		mpStaticContainer = NULL;
	}
	
	//-----------------------------------------------------------------------

	cWorldLoaderHplMap::~cWorldLoaderHplMap()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cWorld* cWorldLoaderHplMap::LoadWorld(const tWString& asFile,tWorldLoadFlag aFlags)
	{
		unsigned long lLoadStartTime = cPlatform::GetApplicationTime();
		mlCurrentFlags = aFlags;
		bool bLoadedFromNormalFile=false;

		///////////////////////
		//Load the map file
		iXmlDocument* pDoc = mpResources->GetLowLevel()->CreateXmlDocument();
		tWString sExt = cString::ToLowerCaseW(cString::GetFileExtW(asFile));
		if(sExt != _W("cmap"))
		{
			if(pDoc->CreateFromFile(asFile)==false)
			{
				hplDelete(pDoc);

				return NULL;
			}
			bLoadedFromNormalFile = true;
		}
		///////////////////////
		//Load compressed
		else if(cResources::GetCreateAndLoadCompressedMaps())
		{
			cBinaryBuffer compBuffer;
			if(compBuffer.Load(asFile)==false)
			{
				//Log("Could not load compressed map!\n");
				return NULL;
			}

			int lKey = kEncryptKey;
			compBuffer.XorTransform((char*)&lKey, sizeof(lKey));

			cBinaryBuffer textBuff;
			if(textBuff.DecompressAndAddFromBuffer(&compBuffer, false)==false)
			{
				//Log("Could not decompress map!\n");
				return NULL;
			}

			if(pDoc->CreateFromString(textBuff.GetDataPointer())==false)
			{
				hplDelete(pDoc);
				//Log("Could not parse map!\n");
				return NULL;
			}
		}

		///////////////////////
		//Save to compressed
		if(cResources::GetCreateAndLoadCompressedMaps() && bLoadedFromNormalFile)
		{
			tWString sCompFile = cString::SetFileExtW(asFile,_W("cmap"));

			//Only recreate if file does not exist or if out of date.
			if(	cPlatform::FileExists(sCompFile)==false || 
				cPlatform::FileModifiedDate(sCompFile) < cPlatform::FileModifiedDate(asFile))
			{
				tString sData;
				pDoc->SaveToString(&sData);
				
				cBinaryBuffer textBuff;
				textBuff.AddCharArray(sData.c_str(), sData.size()+1);

				cBinaryBuffer compBuff;
				compBuff.CompressAndAdd(textBuff.GetDataPointer(), textBuff.GetSize());
				
				int lKey = kEncryptKey;
				compBuff.XorTransform((char*)&lKey, sizeof(lKey));

				compBuff.Save(sCompFile);
			}
		}

		////////////////////////////////
		// Init general vars
		mbLoadedCache= false;
		mlstStaticMeshBodies.clear();
		mlstStaticMeshEntities.clear();
		mlStaticMeshBodiesCreated = 0;
		mlStaticMeshEntitiesCreated =0;

		msCacheFileExt = _W("map_cache");
		if( (mlCurrentFlags & eWorldLoadFlag_FastPhysicsLoad) || (mlCurrentFlags & eWorldLoadFlag_FastStaticLoad) )
		{
			msCacheFileExt = _W("map_cache_fastload");
		}
		
		
		////////////////////////////////
		// Init some timing variables
		unsigned long lStartTime;
		unsigned long lDeltaTime;

		mlSortingTimeTotal =0;
		mlCombineMeshTimeTotal=0;
		mlCombineBodyTimeTotal=0;
		
		
		if(gbLogTiming) Log(" -------- Loading map '%s' ---------\n", cString::To8Char(cString::GetFileNameW(asFile)).c_str());

		///////////////////////
		//Create world and set up physics world with default values
		mpCurrentWorld = mpScene->CreateWorld(cString::To8Char(cString::GetFileNameW(asFile)));
		mpCurrentWorld->SetFilePath(asFile);

		mpCurrentPhysicsWorld = mpPhysics->CreateWorld(true);
        
		//Set up some default values.
		mpCurrentPhysicsWorld->SetAccuracyLevel(ePhysicsAccuracy_Medium);
		mpCurrentPhysicsWorld->SetWorldSize(-300,300);
		mpCurrentPhysicsWorld->SetMaxTimeStep(1.0f / 60.0f);

		mpCurrentWorld->SetPhysicsWorld(mpCurrentPhysicsWorld);
		
		////////////////////////////////////
		// Try loading cache
		LoadCacheFile(asFile);

		////////////////////////////////////
		// Parse the map and build its contents
		OhplMapBuilder builder;
		FillBuilder(&builder);
		tString sFile8 = cString::To8Char(asFile);
		OhplMapOptions mapOptions;
		mapOptions.flags = mlCurrentFlags;
		mapOptions.logCache = gbLogCacheLoad;
		mapOptions.file.p = sFile8.data();
		mapOptions.file.n = sFile8.size();

		if(HplMapParse(pDoc, &builder, &mapOptions, mbLoadedCache) != OHPL_MAP_OK)
		{
			hplDelete(pDoc);
			return NULL;
		}
			
		//////////////////////////////
		// Compile
		lStartTime = cPlatform::GetApplicationTime();
		mpCurrentWorld->Compile(true);
		lDeltaTime = cPlatform::GetApplicationTime() - lStartTime;
		if(gbLogTiming) Log("  Compilation: %d ms\n", lDeltaTime);

		//////////////////////////////
		// Save cache
		SaveCacheFile(asFile);

		//////////////////////////////
		// Final clean up
		STLDeleteAll(mlstStaticShapeBodies);

		hplDelete(pDoc);
		
		lDeltaTime = cPlatform::GetApplicationTime() - lLoadStartTime;
		if(gbLogTiming) Log("  Total: %d ms\n", lDeltaTime);

		if(gbLogTiming) Log("  Meshes created: %d\n", mlStaticMeshEntitiesCreated);
		if(gbLogTiming) Log("  Bodies created: %d\n", mlStaticMeshBodiesCreated);
	
		if(gbLogTiming) Log(" -------- Loading complete ---------\n");

		if(const char *sDump = getenv("OHPL_H13_MAPDUMP"))
			mpCurrentWorld->DumpH13(sDump, cString::To8Char(cString::GetFileNameW(asFile)).c_str());

		return mpCurrentWorld;
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static tString S(OhplStr aStr)
	{
		return tString(aStr.p, aStr.n);
	}

	static cVector3f V3(const OhplF3& aVec)
	{
		return cVector3f(aVec.x, aVec.y, aVec.z);
	}

	static cColor C4(const OhplF4& aCol)
	{
		return cColor(aCol.r, aCol.g, aCol.b, aCol.a);
	}

	static cMatrixf M4(const OhplMat4& aMtx)
	{
		cMatrixf mtx;
		for(int i=0; i<16; ++i) mtx.v[i] = aMtx.v[i];
		return mtx;
	}

	static OhplStr Str(const tString& asStr)
	{
		OhplStr s = { asStr.data(), asStr.size() };
		return s;
	}

	static void* GetVertexBufferWithFormat(iVertexBuffer *apVtxBuffer, eVertexBufferElement aElement, eVertexBufferElementFormat aFormat)
	{
		switch(aFormat)
		{
		case eVertexBufferElementFormat_Int:		return (void*)apVtxBuffer->GetIntArray(aElement);
		case eVertexBufferElementFormat_Float:		return (void*)apVtxBuffer->GetFloatArray(aElement);
		case eVertexBufferElementFormat_Byte:		return (void*)apVtxBuffer->GetByteArray(aElement);
		}

		return NULL;
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::OnPhase(int alPhase, bool abBegin)
	{
		static const char *vTexts[] =
		{
			"  Static Objects: %d ms\n",
			"    MeshEntity Loading: %d ms\n",
			"    Primitive Loading: %d ms\n",
			"    Decal Loading: %d ms\n",
			"    Object Combining: %d ms\n",
			"  Entities: %d ms\n"
		};

		if(abBegin)
		{
			mlPhaseStart[alPhase] = cPlatform::GetApplicationTime();
		}
		else
		{
			unsigned long lDeltaTime = cPlatform::GetApplicationTime() - mlPhaseStart[alPhase];
			if(gbLogTiming) Log(vTexts[alPhase], (int)lDeltaTime);
		}
	}

	void cWorldLoaderHplMap::FillBuilder(OhplMapBuilder *apB)
	{
		typedef cWorldLoaderHplMap T;
		apB->ctx = this;

		apB->log = [](void*, OhplStr s) noexcept{ Log("%s", S(s).c_str()); };
		apB->warning = [](void*, OhplStr s) noexcept{ Warning("%s", S(s).c_str()); };
		apB->error = [](void*, OhplStr s) noexcept{ Error("%s", S(s).c_str()); };
		apB->phase = [](void *c, int lPhase, bool bBegin) noexcept{ ((T*)c)->OnPhase(lPhase, bBegin); };

		apB->set_fog = [](void *c, bool bActive, OhplF4 color, float fFalloffExp, float fStart, float fEnd, bool bCulling) noexcept
		{
			cWorld *pWorld = ((T*)c)->mpCurrentWorld;
			pWorld->SetFogActive(bActive);
			pWorld->SetFogColor(C4(color));
			pWorld->SetFogFalloffExp(fFalloffExp);
			pWorld->SetFogStart(fStart);
			pWorld->SetFogEnd(fEnd);
			pWorld->SetFogCulling(bCulling);
		};
		apB->set_skybox = [](void *c, bool bActive, OhplF4 color, OhplStr texture) noexcept
		{
			T *t = (T*)c;
			t->mpCurrentWorld->SetSkyBoxActive(bActive);
			t->mpCurrentWorld->SetSkyBoxColor(C4(color));

			tString sSkyBoxTexture = S(texture);
			if(sSkyBoxTexture!="")
			{
				iTexture *pSkyBoxTexture = t->mpResources->GetTextureManager()->CreateCubeMap(sSkyBoxTexture,false);
				if(pSkyBoxTexture) t->mpCurrentWorld->SetSkyBox(pSkyBoxTexture, true);
			}
		};

		apB->static_begin = [](void *c) noexcept{ ((T*)c)->StaticBegin(); };
		apB->static_object = [](void *c, OhplStr name, OhplStr file, OhplF3 pos, OhplF3 rot, OhplF3 scale, bool bCollides, bool bCasts, int lID) noexcept
		{
			T *t = (T*)c;
			t->CreateStaticObjectEntity(S(name), S(file), V3(pos), V3(rot), V3(scale), bCollides, bCasts, lID,
										t->mlstStaticTempEntities, t->mpStaticContainer);
		};
		apB->static_plane = [](void *c, OhplStr name, OhplStr material, bool bCasts, bool bCollides, int lID,
								OhplF3 pos, OhplF3 scale, OhplF3 rot, OhplF3 start, OhplF3 end, const OhplF2 *uv) noexcept
		{
			T *t = (T*)c;
			cVector2f vUV[4];
			for(int i=0; i<4; ++i) vUV[i] = cVector2f(uv[i].x, uv[i].y);
			t->CreatePlane(S(name), S(material), bCasts, bCollides, lID, V3(pos), V3(scale), V3(rot), V3(start), V3(end), vUV,
							t->mlstStaticTempEntities, t->mpStaticContainer);
		};
		apB->static_decal = [](void *c, OhplStr name, OhplF4 color, int lID, OhplStr material, OhplXmlNode decalMesh) noexcept
		{
			T *t = (T*)c;
			t->CreateDecal(S(name), C4(color), lID, S(material), (cXmlElement*)decalMesh,
							t->mlstStaticTempEntities, t->mpStaticContainer);
		};
		apB->static_loading_done = [](void *c) noexcept
		{
			T *t = (T*)c;
			if(t->mlCurrentFlags & eWorldLoadFlag_FastStaticLoad)
				t->mpResources->GetMeshManager()->SetUseFastloadMaterial(false);
		};
		apB->static_combo = [](void *c, int lGroupID, const int *pIds, size_t lNum) noexcept
		{
			T *t = (T*)c;
			t->CreateStaticObjectCombo(lGroupID, pIds, lNum, t->mlstStaticTempEntities, t->mpStaticContainer);
		};
		apB->static_compile = [](void *c) noexcept{ ((T*)c)->StaticCompile(); };

		apB->entities_begin = [](void *c) noexcept{ ((T*)c)->EntitiesBegin(); };
		apB->entities_end = [](void *c) noexcept{ ((T*)c)->EntitiesEnd(); };
		apB->entity = [](void *c, OhplStr name, int lID, bool bActive, OhplF3 pos, OhplF3 rot, OhplF3 scale, OhplStr file, OhplXmlNode userVars) noexcept
		{
			((T*)c)->LoadEntity(S(name), lID, bActive, V3(pos), V3(rot), V3(scale), S(file), (cXmlElement*)userVars);
		};
		apB->area = [](void *c, OhplStr name, int lID, bool bActive, OhplF3 pos, OhplF3 rot, OhplF3 scale, OhplStr type, OhplXmlNode userVars) noexcept
		{
			((T*)c)->LoadArea(S(name), lID, bActive, V3(pos), V3(rot), V3(scale), S(type), (cXmlElement*)userVars);
		};
		apB->fog_area = [](void *c, OhplXmlNode n) noexcept
		{
			cEngineFileLoading::LoadFogArea((cXmlElement*)n,"", ((T*)c)->mpCurrentWorld, true);
		};
		apB->particle_system = [](void *c, OhplXmlNode n) noexcept
		{
			cEngineFileLoading::LoadParticleSystem((cXmlElement*)n,"", ((T*)c)->mpCurrentWorld);
		};
		apB->sound = [](void *c, OhplXmlNode n) noexcept
		{
			cEngineFileLoading::LoadSound((cXmlElement*)n,"", ((T*)c)->mpCurrentWorld);
		};
		apB->billboard = [](void *c, OhplXmlNode n) noexcept
		{
			T *t = (T*)c;
			cEngineFileLoading::LoadBillboard((cXmlElement*)n,"", t->mpCurrentWorld, t->mpResources, true, &t->mlstLightBillboardConnections);
		};
		apB->light = [](void *c, OhplXmlNode n) noexcept
		{
			T *t = (T*)c;
			cEngineFileLoading::LoadLight((cXmlElement*)n,"", t->mpCurrentWorld, t->mpResources, true);
		};

		apB->cache_header = [](void *c, int lBodies, int lShapeBodies, int lMeshes) noexcept
		{
			T *t = (T*)c;
			t->mlStaticMeshBodiesCreated = lBodies;
			t->mlStaticMeshEntitiesCreated = lMeshes;

			if(gbLogTiming) Log("    Cache header: %d mesh bodies, %d shape bodies, %d mesh entities\n",
				lBodies, lShapeBodies, lMeshes);
		};
		apB->cache_mesh_body = [](void *c, OhplStr name, OhplStr material, bool bBlocksLight, bool bColliderCharacer) noexcept
		{
			T *t = (T*)c;

			iCollideShape *pShape = t->mpCurrentPhysicsWorld->LoadMeshShapeFromBuffer(t->mpCacheBuffer);

			iPhysicsBody *pBody = t->mpCurrentPhysicsWorld->CreateBody(S(name), pShape);

			pBody->SetMaterial(t->mpCurrentPhysicsWorld->GetMaterialFromName(S(material)));
			pBody->SetBlocksLight(bBlocksLight);
			pBody->SetCollide(!bColliderCharacer); //A character collider only collides with characters
		};
		apB->cache_shape_body = [](void *c, OhplStr material, const OhplMat4 *pTransform, bool bCharCollider, bool bBlocksLight,
									const OhplCacheShape *pShapes, size_t lNum) noexcept
		{
			cHplMapShapeBody shapeBody;

			shapeBody.msMaterial = S(material);
			shapeBody.m_mtxTransform = M4(*pTransform);
			shapeBody.mbCharCollider = bCharCollider;
			shapeBody.mbBlocksLight = bBlocksLight;

			shapeBody.mvColliders.resize(lNum);
			for(size_t i=0; i<lNum; ++i)
			{
				cHplMapShape *pShape = hplNew(cHplMapShape, ());
				shapeBody.mvColliders[i] = pShape;

				pShape->mType = (eCollideShapeType)pShapes[i].type;
				pShape->mvSize = V3(pShapes[i].size);
				pShape->m_mtxOffset = M4(pShapes[i].offset);
			}

			((T*)c)->CreateShapeBody(&shapeBody);
		};
		apB->cache_mesh = [](void *c, OhplStr name, OhplStr material, bool bCastShadows, int lVtxNum,
							const OhplCacheArray *pArrays, size_t lArrayNum, const unsigned *pIndices, size_t lIdxNum) noexcept
		{
			T *t = (T*)c;
			tString sName = S(name);
			tString sMaterial = S(material);

			cMesh* pMesh = hplNew( cMesh, (sName, t->msCurrentFile, t->mpResources->GetMaterialManager(), t->mpResources->GetAnimationManager()) );
			cSubMesh *pSubMesh = pMesh->CreateSubMesh("SubMesh");

			pSubMesh->SetMaterialName(sMaterial);
			if(sMaterial != "")
			{
				if((t->mlCurrentFlags & eWorldLoadFlag_FastStaticLoad))
					sMaterial = t->mpResources->GetMeshManager()->GetFastloadMaterial();

				cMaterial *pMaterial = t->mpResources->GetMaterialManager()->CreateMaterial(sMaterial);
				pSubMesh->SetMaterial(pMaterial);
			}			
			
			iVertexBuffer* pVtxBuff = t->mpGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																					eVertexBufferUsageType_Static, 0, 0);
			for(size_t i=0; i<lArrayNum; ++i)
			{
				const OhplCacheArray& arr = pArrays[i];
				eVertexBufferElement arrayType = (eVertexBufferElement)arr.type;
				eVertexBufferElementFormat elementFormat = (eVertexBufferElementFormat)arr.format;

				pVtxBuff->CreateElementArray(arrayType, elementFormat, arr.elemNum, arr.progVar);
				pVtxBuff->ResizeArray(arrayType, lVtxNum * arr.elemNum);

				if(arr.compression ==0)
				{
					void *pData = GetVertexBufferWithFormat(pVtxBuff, arrayType, elementFormat);
					memcpy(pData, arr.data, arr.count * (elementFormat == eVertexBufferElementFormat_Byte ? 1 : 4));
				}
				else
				{
					memcpy(pVtxBuff->GetFloatArray(arrayType), arr.data, arr.count * sizeof(float));
				}
			}

			pVtxBuff->ResizeIndices((int)lIdxNum);
			memcpy(pVtxBuff->GetIndices(), pIndices, lIdxNum * sizeof(unsigned));
			
			pVtxBuff->Compile(0);

			pSubMesh->SetVertexBuffer(pVtxBuff);
			pSubMesh->Compile();

            cMeshEntity *pMeshEntity = t->mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
			pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bCastShadows);
		};
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::LoadCacheFile(const tWString& asFile)
	{
#if (defined(__PPC__) || defined(__ppc__))
		return;
#endif
#if defined(__linux__)
		bool bOwnCache = true;
		tWString sCacheFile = cResources::GetCacheFile(asFile, msCacheFileExt);
		if(cPlatform::FileExists(sCacheFile) == false)
		{
			tWString sLegacyCacheFile = cString::SetFileExtW(asFile, msCacheFileExt);
			if(cPlatform::FileExists(sLegacyCacheFile)) { sCacheFile = sLegacyCacheFile; bOwnCache = false; }
		}
#else
		bool bOwnCache = false;
		tWString sCacheFile = cString::SetFileExtW(asFile, msCacheFileExt);
#endif

		////////////////////////////////////////
		// Check if there is a cache file
		cDate currentDate = cPlatform::FileModifiedDate(asFile);
		cDate cacheDate = cPlatform::FileModifiedDate(sCacheFile);

		if(bOwnCache || cResources::GetForceCacheLoadingAndSkipSaving()==false)
		{
			if(cacheDate < currentDate || cPlatform::FileExists(sCacheFile)==false)
			{
				return;
			}
		}

		////////////////////////////////////////
		// Load file
		cBinaryBuffer binBuff(sCacheFile);
		if(binBuff.Load()==false)
		{
			Error("Could not map cache file '%s'.", cString::To8Char(asFile).c_str());
			return;
		}

		unsigned long lStartTime = cPlatform::GetApplicationTime();

		OhplMapBuilder builder;
		FillBuilder(&builder);
		tString sFile8 = cString::To8Char(asFile);
		OhplMapOptions mapOptions;
		mapOptions.flags = mlCurrentFlags;
		mapOptions.logCache = gbLogCacheLoad;
		mapOptions.file.p = sFile8.data();
		mapOptions.file.n = sFile8.size();

		msCurrentFile = asFile;
		mpCacheBuffer = &binBuff;
		mbLoadedCache = HplMapCacheRead(binBuff, &builder, &mapOptions);
		mpCacheBuffer = NULL;

		if(mbLoadedCache) Log("    Cache Loading: %d ms\n", cPlatform::GetApplicationTime() - lStartTime);
	}
	
	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::SaveCacheFile(const tWString& asFile)
	{
#if (defined(__PPC__) || defined(__ppc__))
		return;
#endif
		if(mbLoadedCache) return; //No need to save if cache was loaded!
#if !defined(__linux__)
		if(cResources::GetForceCacheLoadingAndSkipSaving()) return;
#endif

		Log("Saving cache file for '%s'\n", cString::To8Char(asFile).c_str());

#if defined(__linux__)
		tWString sCacheFile = cResources::GetCacheFile(asFile, msCacheFileExt);
#else
		tWString sCacheFile = cString::SetFileExtW(asFile, msCacheFileExt);
#endif
		cBinaryBuffer binBuff(sCacheFile);

		struct cSource
		{
			cWorldLoaderHplMap *mpLoader;
			cBinaryBuffer *mpBuffer;
			std::list<iPhysicsBody*>::iterator mBodyIt;
			tHplMapShapeBodyListIt mShapeBodyIt;
			tMeshEntityListIt mMeshIt;

			iPhysicsBody *mpBody;
			tString msName, msMaterial;
			std::vector<OhplCacheShape> mvShapes;
			std::vector<OhplCacheArray> mvArrays;

			static void Counts(void *c, int *apBodies, int *apShapes, int *apMeshes) noexcept
			{
				cSource *s = (cSource*)c;
				*apBodies = (int)s->mpLoader->mlstStaticMeshBodies.size();
				*apShapes = (int)s->mpLoader->mlstStaticShapeBodies.size();
				*apMeshes = (int)s->mpLoader->mlstStaticMeshEntities.size();
			}

			static void NextBody(void *c, OhplCacheBodyOut *apOut) noexcept
			{
				cSource *s = (cSource*)c;
				s->mpBody = *s->mBodyIt++;
				s->msName = s->mpBody->GetName();
				s->msMaterial = s->mpBody->GetMaterial() ? s->mpBody->GetMaterial()->GetName() : "";
				apOut->name = Str(s->msName);
				apOut->material = Str(s->msMaterial);
				apOut->blocksLight = s->mpBody->GetBlocksLight();
				apOut->charCollider = s->mpBody->GetCollide()==false;
			}

			static void WriteBodyShape(void *c) noexcept
			{
				cSource *s = (cSource*)c;
				s->mpLoader->mpCurrentPhysicsWorld->SaveMeshShapeToBuffer(s->mpBody->GetShape(), s->mpBuffer);
			}

			static void NextShapeBody(void *c, OhplCacheShapeBodyOut *apOut) noexcept
			{
				cSource *s = (cSource*)c;
				cHplMapShapeBody *pShapeBody = *s->mShapeBodyIt++;

				s->msMaterial = pShapeBody->msMaterial;
				apOut->material = Str(s->msMaterial);
				for(int i=0; i<16; ++i) apOut->transform.v[i] = pShapeBody->m_mtxTransform.v[i];
				apOut->charCollider = pShapeBody->mbCharCollider;
				apOut->blocksLight = pShapeBody->mbBlocksLight;

				s->mvShapes.resize(pShapeBody->mvColliders.size());
				for(size_t i=0; i<s->mvShapes.size(); ++i)
				{
					cHplMapShape *pShape = pShapeBody->mvColliders[i];
					s->mvShapes[i].type = pShape->mType;
					s->mvShapes[i].size.x = pShape->mvSize.x;
					s->mvShapes[i].size.y = pShape->mvSize.y;
					s->mvShapes[i].size.z = pShape->mvSize.z;
					for(int j=0; j<16; ++j) s->mvShapes[i].offset.v[j] = pShape->m_mtxOffset.v[j];
				}
				apOut->colliders = s->mvShapes.data();
				apOut->nColliders = s->mvShapes.size();
			}

			static void NextMesh(void *c, OhplCacheMeshOut *apOut) noexcept
			{
				cSource *s = (cSource*)c;
				cMeshEntity *pEntity = *s->mMeshIt++;
				cSubMeshEntity *pSubEnt = pEntity->GetSubMeshEntity(0);
				cSubMesh *pSubMesh = pSubEnt->GetSubMesh();
				iVertexBuffer *pVtxBuff = pSubMesh->GetVertexBuffer();

				s->msName = pEntity->GetName();
				s->msMaterial = pSubMesh->GetMaterialName();
				apOut->name = Str(s->msName);
				apOut->material = Str(s->msMaterial);
				apOut->castShadows = pSubEnt->GetRenderFlagBit(eRenderableFlag_ShadowCaster);
				apOut->vertexNum = pVtxBuff->GetVertexNum();

				s->mvArrays.clear();
				for(int i=0; i < eVertexBufferElement_LastEnum;i++)
				{
					eVertexBufferElement arrayType = (eVertexBufferElement)i;
					if(pVtxBuff->GetElementNum(arrayType) <= 0) continue;

					eVertexBufferElementFormat elementFormat = pVtxBuff->GetElementFormat(arrayType);

					OhplCacheArray arr;
					arr.type = i;
					arr.format = elementFormat;
					arr.progVar = pVtxBuff->GetElementProgramVarIndex(arrayType);
					arr.elemNum = pVtxBuff->GetElementNum(arrayType);
					arr.compression = 0;
					arr.count = (size_t)(pVtxBuff->GetVertexNum() * arr.elemNum);

					if(arrayType == eVertexBufferElement_Color0 || arrayType == eVertexBufferElement_Normal ||
						arrayType == eVertexBufferElement_Texture1Tangent)
						arr.data = pVtxBuff->GetFloatArray(arrayType);
					else
						arr.data = GetVertexBufferWithFormat(pVtxBuff, arrayType, elementFormat);

					s->mvArrays.push_back(arr);
				}
				apOut->arrays = s->mvArrays.data();
				apOut->nArrays = s->mvArrays.size();
				apOut->indices = pVtxBuff->GetIndices();
				apOut->nIndices = pVtxBuff->GetIndexNum();
			}
		};

		cSource source;
		source.mpLoader = this;
		source.mpBuffer = &binBuff;
		source.mBodyIt = mlstStaticMeshBodies.begin();
		source.mShapeBodyIt = mlstStaticShapeBodies.begin();
		source.mMeshIt = mlstStaticMeshEntities.begin();
		source.mpBody = NULL;

		OhplMapCacheSource src;
		src.ctx = &source;
		src.error = [](void*, OhplStr s) noexcept{ Error("%s", S(s).c_str()); };
		src.counts = cSource::Counts;
		src.next_body = cSource::NextBody;
		src.write_body_shape = cSource::WriteBodyShape;
		src.next_shape_body = cSource::NextShapeBody;
		src.next_mesh = cSource::NextMesh;

		HplMapCacheWrite(binBuff, &src);

		bool bRet = binBuff.Save();
		if(bRet==false) 	Error("Couldn't save map cache to '%s'", cString::To8Char(sCacheFile).c_str());
	}
	
	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::StaticBegin()
	{
		mpStaticContainer = hplNew( cRenderableContainer_BoxTree, () );

		//TODO: These vars need to be tweaked!
		mpStaticContainer->SetMinLeafObjects(200);//12);
		mpStaticContainer->SetMinSideLength(3);//3.0f);
		mpStaticContainer->SetMaxSideLength(30);//3.0f);
		mpStaticContainer->SetMaxIntersectionAmount(0.03f);
		mpStaticContainer->SetMaxVolumeCalcObjects(400);
		mpStaticContainer->SetMinForceIntersectionRelativeSize(0.8f);

		mlstStaticTempEntities.clear();

		mlCombinedMeshNameCount =0;
		mlCombinedBodyNameCount =0;

		if(mlCurrentFlags & eWorldLoadFlag_FastStaticLoad)
			mpResources->GetMeshManager()->SetUseFastloadMaterial(true);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::StaticCompile()
	{
		unsigned long lStartTime;
		unsigned long lDeltaTime;

		///////////////////////////////////////
		//Compile the container
		lStartTime = cPlatform::GetApplicationTime();
		mpStaticContainer->Compile();
		lDeltaTime = cPlatform::GetApplicationTime() - lStartTime;
		if(gbLogTiming) Log("    Compilation: %d ms\n", lDeltaTime);

		/////////////////////////////////
		//Go through all leaves of the container, and combine objects found there.
		lStartTime = cPlatform::GetApplicationTime();
		IterateLeafNodesAndBuildMeshes(mpStaticContainer->GetRoot());
		lDeltaTime = cPlatform::GetApplicationTime() - lStartTime;
		if(gbLogTiming)
		{
			Log("    Combining: %d ms\n", lDeltaTime);
			Log("     Sorting: %d ms\n", mlSortingTimeTotal);
			Log("     Meshes: %d ms\n", mlCombineMeshTimeTotal);
			Log("     Bodies: %d ms\n", mlCombineBodyTimeTotal);
		}
		
		/////////////////////////////////
		//Clean up
        STLDeleteAll(mlstStaticTempEntities);
		STLDeleteAll(mlstTempStaticUserData);
		hplDelete(mpStaticContainer);
		mpStaticContainer = NULL;
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	static bool SortStaticSubMeshesForMeshes(iRenderable* apObjectDataA, iRenderable* apObjectDataB)
	{
		//Is shadow caster check
		if(	apObjectDataA->GetRenderFlagBit(eRenderableFlag_ShadowCaster) !=
			apObjectDataB->GetRenderFlagBit(eRenderableFlag_ShadowCaster))
		{
			return	apObjectDataA->GetRenderFlagBit(eRenderableFlag_ShadowCaster) <
					apObjectDataB->GetRenderFlagBit(eRenderableFlag_ShadowCaster);
		}
		//Material check
		if( apObjectDataA->GetMaterial() != apObjectDataB->GetMaterial())
		{
			return apObjectDataA->GetMaterial() < apObjectDataB->GetMaterial();
		}

		return apObjectDataA->GetVertexBuffer() < apObjectDataB->GetVertexBuffer();
	}

	//-----------------------------------------------------------------------

	static bool SortStaticSubMeshesForBodies(const cHplMapPhysicsObject& aObjectDataA, const cHplMapPhysicsObject& aObjectDataB)
	{
		//Is shadow caster check
		if(	aObjectDataA.mpObject->GetRenderFlagBit(eRenderableFlag_ShadowCaster) !=
			aObjectDataB.mpObject->GetRenderFlagBit(eRenderableFlag_ShadowCaster))
		{
			return	aObjectDataA.mpObject->GetRenderFlagBit(eRenderableFlag_ShadowCaster) <
					aObjectDataB.mpObject->GetRenderFlagBit(eRenderableFlag_ShadowCaster);
		}
		//Material check
		//Check if next object is not part of sequence, if so combine current sequence.
		if( aObjectDataA.mpPhysicsMaterial != aObjectDataB.mpPhysicsMaterial)
		{
			return aObjectDataA.mpPhysicsMaterial < aObjectDataB.mpPhysicsMaterial;
		}

		//Char collider or not
		if( aObjectDataA.mbCharCollider  != aObjectDataB.mbCharCollider)
		{
			return aObjectDataA.mbCharCollider < aObjectDataB.mbCharCollider;
		}

		return aObjectDataA.mpObject->GetVertexBuffer() < aObjectDataA.mpObject->GetVertexBuffer();
	}

	//-----------------------------------------------------------------------
	
	void cWorldLoaderHplMap::IterateLeafNodesAndBuildMeshes(iRenderableContainerNode *apNode)
	{
		////////////////////////
		//Iterate children
		if(apNode->HasChildNodes())
		{
			tRenderableContainerNodeListIt childIt = apNode->GetChildNodeList()->begin();
			for(; childIt != apNode->GetChildNodeList()->end(); ++childIt)
			{
				iRenderableContainerNode *pChildNode = *childIt;
				IterateLeafNodesAndBuildMeshes(pChildNode);
			}
		}

		/////////////////////////////
		//Iterate objects
		if(apNode->HasObjects())
		{
			CombineAndCreateMeshesAndPhysics(apNode->GetObjectList());
		}	
	}
	
	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::CombineAndCreateMeshesAndPhysics(tRenderableList *apObjectList)
	{
		unsigned long lStartTime;
		lStartTime = cPlatform::GetApplicationTime();

		bool bCreateBodies = !mbLoadedCache;

		////////////////////
		//Set up vectors
		tRenderableVec vMeshObjects;
		std::vector<cHplMapPhysicsObject> vPhysicsObjects;

		vMeshObjects.resize(apObjectList->size());
		if(bCreateBodies)
			vPhysicsObjects.resize(apObjectList->size());

		if(gbLog) Log("Trying to combine %d objects for list %d\n",vMeshObjects.size(),apObjectList);

		////////////////////
		//Iterate the objects and add to vector
		tRenderableListIt it = apObjectList->begin();
		for(int i=0; it != apObjectList->end(); ++it, ++i)
		{
			iRenderable *pObject = *it;

			if(gbLog) Log("  Adding '%s'\n",pObject->GetName().c_str());

			/////////////////////
			//Preload and save physics material
			iPhysicsMaterial *pPhysicsMat = NULL;
			if(pObject->GetMaterial())
			{
				const tString& sMat = pObject->GetMaterial()->GetPhysicsMaterial();
				if(sMat != "")	pPhysicsMat = mpCurrentPhysicsWorld->GetMaterialFromName(sMat);
			}

			/////////////////////
			//Set object to arrays	
			vMeshObjects[i] = pObject;

			if(bCreateBodies)
			{
				cSubMeshEntity *pSubEnt =static_cast<cSubMeshEntity*>(pObject);
				cHplMapStaticUserData* pUserData = (cHplMapStaticUserData*)pSubEnt->GetUserData();

				vPhysicsObjects[i].mpObject = pSubEnt;
				vPhysicsObjects[i].mpPhysicsMaterial = pPhysicsMat;
				vPhysicsObjects[i].mpUserData = pUserData;
				vPhysicsObjects[i].mbCharCollider = pUserData->mbCharCollider;

				//Log("'%s': charcollider: %d\n", vPhysicsObjects[i].mpObject->GetName().c_str(), vPhysicsObjects[i].mbCharCollider);
			}
		}

		////////////////////
		//Sort the objects
		if(gbLog) Log(" Sorting objects!\n");
		std::sort(vMeshObjects.begin(), vMeshObjects.end(), SortStaticSubMeshesForMeshes);
		std::sort(vPhysicsObjects.begin(), vPhysicsObjects.end(), SortStaticSubMeshesForBodies);

		mlSortingTimeTotal += cPlatform::GetApplicationTime() - lStartTime;

		////////////////////
		// Create Meshes
		//  Iterate and combine when a found of combinable objects are found
		lStartTime = cPlatform::GetApplicationTime();
		if(gbLog) Log(" Check for mesh combination sequences!\n");

		std::vector<OhplH13MeshItem> vMeshItems(vMeshObjects.size());
		for(size_t i=0; i< vMeshObjects.size(); ++i)
		{
			cHplMapStaticUserData *pStaticData = (cHplMapStaticUserData*)static_cast<cSubMeshEntity*>(vMeshObjects[i])->GetUserData();
			vMeshItems[i].mMaterial = (size_t)vMeshObjects[i]->GetMaterial();
			vMeshItems[i].mbShadow = vMeshObjects[i]->GetRenderFlagBit(eRenderableFlag_ShadowCaster);
			vMeshItems[i].mbCombine = pStaticData ? pStaticData->mbCombine : true;
		}
		std::vector<int32_t> vMeshPlan(2 * vMeshItems.size());
		const size_t lMeshRuns = ohpl_h13_plan_meshes(vMeshItems.data(), vMeshItems.size(), vMeshPlan.data(), vMeshItems.size());
		for(size_t i=0; i<lMeshRuns; ++i)
			CombineObjectsAndCreateMeshEntity(vMeshObjects, vMeshPlan[2*i], vMeshPlan[2*i+1]);
		mlCombineMeshTimeTotal += cPlatform::GetApplicationTime() - lStartTime;

		////////////////////
		// Create Physics Bodies
		//  Iterate physics, combine and create!
		if(bCreateBodies==false) return;

		lStartTime = cPlatform::GetApplicationTime();
		if(gbLog) Log(" Check for body combination sequences!\n");
		{
			std::vector<OhplH13BodyItem> vBodyItems(vPhysicsObjects.size());
			for(size_t i=0; i< vPhysicsObjects.size(); ++i)
			{
				cHplMapPhysicsObject& physObj = vPhysicsObjects[i];
				vBodyItems[i].mPhysMaterial = (size_t)physObj.mpPhysicsMaterial;
				vBodyItems[i].mbShadow = physObj.mpObject->GetRenderFlagBit(eRenderableFlag_ShadowCaster);
				vBodyItems[i].mbCharCollider = physObj.mbCharCollider;
				vBodyItems[i].mbCollides = physObj.mpUserData->mbCollides;
				vBodyItems[i].mlIndexNum = vBodyItems[i].mbCollides ? physObj.mpObject->GetVertexBuffer()->GetIndexNum() : 0;
			}
			std::vector<int32_t> vBodyPlan(2 * vBodyItems.size());
			const size_t lBodyRuns = ohpl_h13_plan_bodies(vBodyItems.data(), vBodyItems.size(), vMeshObjects.size(), vBodyPlan.data(), vBodyItems.size());
			for(size_t i=0; i<lBodyRuns; ++i)
				CombineObjectsAndCreatePhysics(vPhysicsObjects, vBodyPlan[2*i], vBodyPlan[2*i+1]);
		}
		mlCombineBodyTimeTotal += cPlatform::GetApplicationTime() - lStartTime;
	}
	
	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::CombineObjectsAndCreateMeshEntity(tRenderableVec &avObjects, int alFirstIdx, int alLastIdx)
	{
		if(gbLog) Log("  Combining objects %d -> %d\n", alFirstIdx, alLastIdx);

		///////////////////////////////////////////
		//Iterate objects to get the total amount of vertex data
		int lTotalVtxAmount =0;
		int lTotalIdxAmount =0;
		tString sName = "CombinedObjects"+cString::ToString(mlCombinedMeshNameCount);
		std::vector<OhplMapCombineSrc> vSrcs;
		static const eVertexBufferElement vTypes[5] = { eVertexBufferElement_Position, eVertexBufferElement_Normal, eVertexBufferElement_Color0,
														eVertexBufferElement_Texture0, eVertexBufferElement_Texture1Tangent };
		for(int i=alFirstIdx; i<=alLastIdx; ++i)
		{
			//Check if the sub mesh is visible, else skip
			cHplMapStaticUserData*pUserData = (cHplMapStaticUserData*)static_cast<cSubMeshEntity*>(avObjects[i])->GetUserData();
			if(pUserData->mbVisible==false) continue;

			//Check add the vertex num and index num from vertex buffer
			iVertexBuffer *pVtxBuffer = avObjects[i]->GetVertexBuffer();

			OhplMapCombineSrc src;
			src.vtxNum = pVtxBuffer->GetVertexNum();
			src.idxNum = pVtxBuffer->GetIndexNum();
			memcpy(src.matrix, avObjects[i]->GetWorldMatrix().v, sizeof(src.matrix));
			for(int k=0; k<5; ++k) src.arrays[k] = pVtxBuffer->GetFloatArray(vTypes[k]);
			src.indices = pVtxBuffer->GetIndices();
			vSrcs.push_back(src);

			lTotalVtxAmount += pVtxBuffer->GetVertexNum();
			lTotalIdxAmount += pVtxBuffer->GetIndexNum();

			if(gbLog) Log("   '%s' has %d vtx and %d idx\n",avObjects[i]->GetName().c_str(),pVtxBuffer->GetVertexNum(),pVtxBuffer->GetIndexNum());
		}
		if(gbLog) Log("   Total amount %d vtx and %d idx\n",lTotalVtxAmount, lTotalIdxAmount);

		///////////////////////////////////////////
		//If no vertices, return and skip creation
		if(lTotalVtxAmount<=0 || lTotalIdxAmount <=0)
		{
			return;
		}

		//Increase the name count
		mlCombinedMeshNameCount++; 

		///////////////////////////////////////////
		//Create the vertex buffer (skipping color!)
		iVertexBuffer *pVtxBuffer = mpGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																					eVertexBufferUsageType_Static,lTotalVtxAmount, lTotalIdxAmount);

		//Set up what data arrays to use
		static const int vElementNums[5] = { 4, 3, 4, 3, 4 };
		OhplMapCombineDst dst;
		for(int i=0;i<5; ++i)
		{
			pVtxBuffer->CreateElementArray(vTypes[i],eVertexBufferElementFormat_Float, vElementNums[i]);
			pVtxBuffer->ResizeArray(vTypes[i], lTotalVtxAmount * vElementNums[i]);
			dst.arrays[i] = pVtxBuffer->GetFloatArray(vTypes[i]);
		}
				
		pVtxBuffer->ResizeIndices(lTotalIdxAmount);
		dst.indices = pVtxBuffer->GetIndices();
		dst.vtxTotal = lTotalVtxAmount;
		dst.idxTotal = lTotalIdxAmount;
		ohpl_map_combine(vSrcs.data(), vSrcs.size(), true, &dst);

		///////////////////////
		// All meshes batched into one buffer, compile it.
		pVtxBuffer->Compile(0);

		///////////////////////////////////////////
		//Create the mesh
		iRenderable *pFirstObject = avObjects[alFirstIdx];
		cSubMeshEntity *pFirstSubEnt = static_cast<cSubMeshEntity*>(pFirstObject);
		cMesh *pMesh = hplNew( cMesh, (sName, _W("") ,mpResources->GetMaterialManager(),mpResources->GetAnimationManager()) );

		cSubMesh *pSubMesh = pMesh->CreateSubMesh("SubMesh");
		
		//Set the vertex buffer
		pSubMesh->SetVertexBuffer(pVtxBuffer);

		//Set material
		cMaterial *pMaterial = pFirstObject->GetMaterial();
		if(pMaterial)
		{
			pMaterial->IncUserCount();
			pSubMesh->SetMaterial(pMaterial);
		}
		pSubMesh->SetMaterialName(pFirstSubEnt->GetSubMesh()->GetMaterialName());
		
		//Compile
		pSubMesh->Compile();
		
		///////////////////////////////////////////
		//Create the mesh entity
		cMeshEntity *pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName,pMesh, true);
		
		//Set up variables
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, pFirstObject->GetRenderFlagBit(eRenderableFlag_ShadowCaster));

		//Add to list
		mlstStaticMeshEntities.push_back(pMeshEntity);
		mlStaticMeshEntitiesCreated++;
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::CombineObjectsAndCreatePhysics(std::vector<cHplMapPhysicsObject> &avObjects, int alFirstIdx, int alLastIdx)
	{
		if(gbLog) Log("  Combining objects %d -> %d\n", alFirstIdx, alLastIdx);
		const int lMaxIndices= 30;

		///////////////////////////////////////////
		//Iterate objects to get the total amount of vertex data
		int lTotalVtxAmount =0;
		int lTotalIdxAmount =0;
		tString sName = "CombinedObjects"+cString::ToString(mlCombinedBodyNameCount);
		std::vector<OhplMapCombineSrc> vSrcs;
		for(int i=alFirstIdx; i<=alLastIdx; ++i)
		{
			if(avObjects[i].mpUserData->mbCollides==false) continue;

			iVertexBuffer *pVtxBuffer = avObjects[i].mpObject->GetVertexBuffer();
			
			//Do a special debug test and skip highpoly entities, loading the map faster.
            if((mlCurrentFlags & eWorldLoadFlag_FastPhysicsLoad) && pVtxBuffer->GetIndexNum() > lMaxIndices)	continue;

			OhplMapCombineSrc src;
			src.vtxNum = pVtxBuffer->GetVertexNum();
			src.idxNum = pVtxBuffer->GetIndexNum();
			memcpy(src.matrix, avObjects[i].mpObject->GetWorldMatrix().v, sizeof(src.matrix));
			src.arrays[0] = pVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
			src.indices = pVtxBuffer->GetIndices();
			vSrcs.push_back(src);


			lTotalVtxAmount += pVtxBuffer->GetVertexNum();
			lTotalIdxAmount += pVtxBuffer->GetIndexNum();

			if(gbLog) Log("   '%s' has %d vtx and %d idx\n",avObjects[i].mpObject->GetName().c_str(),pVtxBuffer->GetVertexNum(),pVtxBuffer->GetIndexNum());
		}
		if(gbLog) Log("   Total amount %d vtx and %d idx\n",lTotalVtxAmount, lTotalIdxAmount);

		///////////////////////////////////////////
		//If no vertex buffers, then just exit
		if(lTotalVtxAmount<=0 || lTotalIdxAmount<=0)
		{
			return;
		}
		mlCombinedBodyNameCount++;


		///////////////////////////////////////////
		//Create the vertex buffer (skipping color!)
		iVertexBuffer *pVtxBuffer = mpGraphics->GetLowLevel()->CreateVertexBuffer(	eVertexBufferType_Software, eVertexBufferDrawType_Tri,
																					eVertexBufferUsageType_Dynamic,lTotalVtxAmount, lTotalIdxAmount);

		pVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float, 4);
		pVtxBuffer->ResizeArray(eVertexBufferElement_Position, lTotalVtxAmount * 4);
		OhplMapCombineDst dst;
		dst.arrays[0] = pVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
		
		//Set up and get indices
		pVtxBuffer->ResizeIndices(lTotalIdxAmount);
		dst.indices = pVtxBuffer->GetIndices();
		dst.vtxTotal = lTotalVtxAmount;
		dst.idxTotal = lTotalIdxAmount;
		ohpl_map_combine(vSrcs.data(), vSrcs.size(), false, &dst);

		pVtxBuffer->Compile(0);


		///////////////////////////////////////////
		//Create the mesh physics body
		iRenderable *pFirstObject = avObjects[alFirstIdx].mpObject;
		
		iCollideShape *pShape = mpCurrentPhysicsWorld->CreateMeshShape(pVtxBuffer);
		hplDelete(pVtxBuffer);
		
		iPhysicsBody *pBody = mpCurrentPhysicsWorld->CreateBody(sName,pShape);
		pBody->SetMass(0);

		bool bCastShadows = pFirstObject->GetRenderFlagBit(eRenderableFlag_ShadowCaster);
		pBody->SetBlocksLight(bCastShadows);

		pBody->SetCollide(!avObjects[alFirstIdx].mbCharCollider);

		mlstStaticMeshBodies.push_back(pBody);
		mlStaticMeshBodiesCreated++;

		if(avObjects[alFirstIdx].mpPhysicsMaterial) pBody->SetMaterial(avObjects[alFirstIdx].mpPhysicsMaterial);
	}


	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::CreateStaticObjectEntity(	const tString& sName, const tString& sFileName, const cVector3f& vPosition,
														const cVector3f& vRotation, const cVector3f& vScale, bool bCollides,
														bool bCastsShadows, int lID,
														tMeshEntityList& alstMeshEntities, cRenderableContainer_BoxTree *apContainer)
	{
		////////////////////////////////
		//Create mesh entity
		cMesh* pMesh = mpResources->GetMeshManager()->CreateMesh(sFileName);
		if(pMesh == NULL){
			Error("Could not load mesh '%s'\n", sFileName.c_str());
			return;
		}
		
		//////////////////////
		//RENDER_DEBUG
		/*cMeshEntity* pDebugMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh);
		pDebugMeshEntity->SetWorldMatrix(cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ),cMath::MatrixScale(vScale)));
		pDebugMeshEntity->SetPosition(vPosition);
		return;*/
		
		
		cMeshEntity* pMeshEntity = hplNew( cMeshEntity, (sName,pMesh,
														mpResources->GetMaterialManager(),
														mpResources->GetMeshManager(), 
														mpResources->GetAnimationManager()) );
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bCastsShadows);
		pMeshEntity->SetUniqueID(lID);

		alstMeshEntities.push_back(pMeshEntity);


		///////////////////////
		// Check if there is a collider (non character collider!)
		// NOTE: Even though cache is loaded, we still want to add all of these to keep the tree the same!
		bool bHasNormalColliderShapeMesh = false;
		if(bCollides)
		{
			for(int i=0; i<pMeshEntity->GetSubMeshEntityNum(); ++i)
			{
				cSubMeshEntity *pSubEnt = pMeshEntity->GetSubMeshEntity(i);
				cSubMesh *pSubMesh = pSubEnt->GetSubMesh();
				if( cString::Sub(pSubMesh->GetName(),0,1)== "_")
				{
					if( cString::Sub(pSubMesh->GetName(),0,9)== "_collider")
					{
						bHasNormalColliderShapeMesh = true;
						break;
					}
				}
			}
		}

		///////////////////////
		// Add all sub meshes to a new vector
		for(int i=0; i<pMeshEntity->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity *pSubEnt = pMeshEntity->GetSubMeshEntity(i);
			cSubMesh *pSubMesh = pSubEnt->GetSubMesh();
			cMaterial *pMaterial = pSubEnt->GetMaterial();

            ////////////////////////////////
			//If there are (non-char) collide shapes, do not create body from mesh! 
			bool bCreateMeshCollider = bCollides; 
			if(bCreateMeshCollider)
			{
				for(int j=0; j<pSubMesh->GetColliderNum(); ++j)
				{
					if(pSubMesh->GetCollider(j)->IsCharCollider()==false)
						bCreateMeshCollider = false;
				}
			}

			////////////////////////////////
			//Is this a collider shape mesh?
			bool bIsColliderMeshShape = false;
			bool bIsCharCollider = false;
			{
				//All non mesh shapes shall skip creating a collider if there is a normal collider shape.
				if(bHasNormalColliderShapeMesh)	bCreateMeshCollider = false; 

				if( cString::Sub(pSubMesh->GetName(),0,1)== "_")
				{
					if(cString::Sub(pSubMesh->GetName(),0,9)== "_collider")
					{
						bCreateMeshCollider = true;
						bIsColliderMeshShape = true;
					}
					else if(cString::Sub(pSubMesh->GetName(),0,13)== "_charcollider")
					{
						bCreateMeshCollider = true;
						bIsColliderMeshShape = true;
						bIsCharCollider = true;
					}
				}
			}

			//No need to create mesh since it would be non-colliding and invisible!
			if(bCollides == false && bIsColliderMeshShape) continue;
			
			////////////////////////////////
			//Create user data
			cHplMapStaticUserData *pUserData = hplNew(cHplMapStaticUserData, ());
			pUserData->mvScale = vScale;
			pUserData->mbCollides = bCreateMeshCollider;
			pUserData->mbVisible = !bIsColliderMeshShape;
			pUserData->mbCharCollider = bIsCharCollider;

			//If one sided and world reflector, do not combine!
			if(pSubMesh->GetIsOneSided() && pMaterial && pMaterial->HasWorldReflection())
				pUserData->mbCombine = false;
			else
				pUserData->mbCombine = true;

			
			pSubEnt->SetUserData(pUserData);
			mlstTempStaticUserData.push_back(pUserData);

			apContainer->Add(pSubEnt);
		}
		
		////////////////////////////////
		//Create shape bodies
		if(bCollides)
		{
			cMatrixf mtxBodyTransform = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
			mtxBodyTransform.SetTranslation(vPosition);

			for(int i=0; i<pMeshEntity->GetSubMeshEntityNum(); ++i)
			{
				cSubMeshEntity *pSubEnt = pMeshEntity->GetSubMeshEntity(i);
				CreateSubMeshShapeBodies(pSubEnt, mtxBodyTransform, vScale);
			}
		}

		
		////////////////////////////////
		//Setup mesh entity
		pMeshEntity->SetWorldMatrix(cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ),cMath::MatrixScale(vScale)));
		pMeshEntity->SetPosition(vPosition);
	}

	//-----------------------------------------------------------------------
	
	void cWorldLoaderHplMap::CreateSubMeshShapeBodies(cSubMeshEntity *apSubEnt, const cMatrixf &a_mtxTransform, const cVector3f& avScale)
	{
		cSubMesh *pSubMesh = apSubEnt->GetSubMesh();
		if(pSubMesh->GetColliderNum() <= 0) return;
		
		std::vector<cHplMapShape*> vCharColliders;
		std::vector<cHplMapShape*> vNormalColliders;

		vCharColliders.reserve(pSubMesh->GetColliderNum());
		vNormalColliders.reserve(pSubMesh->GetColliderNum());

		//////////////////////////////////
		//Iterate colliders and create shapes
		for(int i=0; i<pSubMesh->GetColliderNum(); ++i)
		{
			cMeshCollider *pMeshCollider = pSubMesh->GetCollider(i);

			cHplMapShape *pMapShape = hplNew(cHplMapShape, ());
			
			pMapShape->m_mtxOffset = pMeshCollider->Offset();
			pMapShape->m_mtxOffset.SetTranslation(pMeshCollider->Offset().GetTranslation() * avScale);
			pMapShape->mType = pMeshCollider->Type();
			pMapShape->mvSize = pMeshCollider->Size() * avScale;

			if(pMeshCollider->IsCharCollider())
				vCharColliders.push_back(pMapShape);
			else
				vNormalColliders.push_back(pMapShape);
		}

		//////////////////////////////////
		//Iterate shapes and create bodies
		for(int i=0; i<2; ++i)
		{
			if(i==0 && vNormalColliders.empty()) continue;
			if(i==1 && vCharColliders.empty()) continue;

			std::vector<cHplMapShape*> *pColliderVec = i==0 ? &vNormalColliders : &vCharColliders;
			
			/////////////////////////////////////
			// Create the saved data (to save in cache(
			cHplMapShapeBody *pShapeBody = hplNew(cHplMapShapeBody, () );
            pShapeBody->mbCharCollider = (i==1);			
			pShapeBody->m_mtxTransform = a_mtxTransform;
			pShapeBody->mvColliders = *pColliderVec;
			pShapeBody->mbBlocksLight = apSubEnt->GetRenderFlagBit(eRenderableFlag_ShadowCaster);
			pShapeBody->msMaterial = apSubEnt->GetMaterial()->GetPhysicsMaterial();
			
			mlstStaticShapeBodies.push_back(pShapeBody);

			/////////////////////////////////////
			// Create Shapes Body
			CreateShapeBody(pShapeBody);	
		}
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::CreateShapeBody(cHplMapShapeBody* apShapeBody)
	{
		////////////////////////////////////
		// Create sub shapes
		std::vector<iCollideShape*> vShapes;
		vShapes.resize(apShapeBody->mvColliders.size());
		
		for(size_t i=0; i<vShapes.size(); ++i)
		{
			cHplMapShape* pMapShape = apShapeBody->mvColliders[i];
			cMatrixf *pOffset = &pMapShape->m_mtxOffset;

			switch(pMapShape->mType)
			{
			case eCollideShapeType_Box: 
				vShapes[i] = mpCurrentPhysicsWorld->CreateBoxShape(pMapShape->mvSize,pOffset);
				break;
			case eCollideShapeType_Sphere: 
				vShapes[i] = mpCurrentPhysicsWorld->CreateSphereShape(pMapShape->mvSize,pOffset);
				break;
			case eCollideShapeType_Cylinder: 
				vShapes[i] = mpCurrentPhysicsWorld->CreateCylinderShape(pMapShape->mvSize.x,pMapShape->mvSize.y,pOffset);
				break;
			case eCollideShapeType_Capsule: 
				vShapes[i] = mpCurrentPhysicsWorld->CreateCapsuleShape(pMapShape->mvSize.x,pMapShape->mvSize.y,pOffset);
				break;
			}
		}

		/////////////////////////////////////
		// Create final shape
		iCollideShape *pShape = NULL;
		if(vShapes.size() > 1)	pShape = mpCurrentPhysicsWorld->CreateCompundShape(vShapes);
		else					pShape = vShapes[0];

		/////////////////////////////////////
		// Create Body
		iPhysicsBody *pBody = mpCurrentPhysicsWorld->CreateBody("ShapeBody",pShape);
		pBody->SetCollide(!apShapeBody->mbCharCollider);

		pBody->SetBlocksLight(apShapeBody->mbBlocksLight);

		iPhysicsMaterial *pPhysicsMat = mpCurrentPhysicsWorld->GetMaterialFromName(apShapeBody->msMaterial);
		pBody->SetMaterial(pPhysicsMat);
		pBody->SetMatrix(apShapeBody->m_mtxTransform);
	}

	//-----------------------------------------------------------------------
	void cWorldLoaderHplMap::CreatePlane(	const tString& sName, const tString& sMaterialRaw, bool bCastsShadows, bool bCollides, int lID,
											const cVector3f& vPosition, const cVector3f& vScale, const cVector3f& vRotation,
											const cVector3f& vStartCorner, const cVector3f& vEndCorner, const cVector2f *vUVCorners,
											tMeshEntityList& alstMeshEntities, cRenderableContainer_BoxTree *apContainer)
	{
		cMeshEntity *pMeshEntity = NULL;

		tString sMaterial = sMaterialRaw;
		tString sMaterialName = sMaterial;

		if((mlCurrentFlags & eWorldLoadFlag_FastStaticLoad))
			sMaterial = mpResources->GetMeshManager()->GetFastloadMaterial();

		{
			//Create the mesh
			cMesh *pMesh = mpGraphics->GetMeshCreator()->CreatePlane(sName,vStartCorner,vEndCorner,
																	 vUVCorners[0],vUVCorners[1], vUVCorners[2], vUVCorners[3], 
																	 sMaterial);

			//Create mesh entity
			pMeshEntity = hplNew( cMeshEntity, (sName,pMesh,
												mpResources->GetMaterialManager(),
												mpResources->GetMeshManager(), 
												mpResources->GetAnimationManager()) );
			pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bCastsShadows);
			pMeshEntity->GetSubMeshEntity(0)->GetSubMesh()->SetMaterialName(sMaterialName);
		}

		//////////////////////////////////
		// General Final Stuff
		if(pMeshEntity)
		{
			//Set the transform
			pMeshEntity->SetWorldMatrix(cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ),cMath::MatrixScale(vScale)));
			pMeshEntity->SetPosition(vPosition);

			//Id
			pMeshEntity->SetUniqueID(lID);

			//Add the mesh entity.
			alstMeshEntities.push_back(pMeshEntity);

			// Add all sub meshes to a new vector
			for(int i=0; i<pMeshEntity->GetSubMeshEntityNum(); ++i)
			{
				cSubMeshEntity *pSubEnt = pMeshEntity->GetSubMeshEntity(i);
				cSubMesh *pSubMesh = pSubEnt->GetSubMesh();
				cMaterial *pMaterial = pSubEnt->GetMaterial();

				cHplMapStaticUserData *pUserData = hplNew(cHplMapStaticUserData, ());
				pUserData->mvScale = 1;
				pUserData->mbCollides = bCollides;
				pUserData->mbVisible = true;
				pUserData->mbCharCollider = false;
				
				//If one sided and world reflector, do not combine!
				if(pSubMesh->GetIsOneSided() && pMaterial->HasWorldReflection())
					pUserData->mbCombine = false;
				else
					pUserData->mbCombine = true;
				

				pSubEnt->SetUserData(pUserData);
				mlstTempStaticUserData.push_back(pUserData);
				
				apContainer->Add(pSubEnt);
			}
		}

	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::CreateDecal(	const tString& sName, const cColor& decalColor, int lID, const tString& sMaterial,
											cXmlElement* pDecalMeshElem, tMeshEntityList& alstMeshEntities,
											cRenderableContainer_BoxTree *apDecalContainer)
	{
		cMeshEntity *pMeshEntity = NULL;

		////////////////////////////////
		//Load Vertex data
		cMesh* pMesh = cEngineFileLoading::LoadDecalMeshHelper(pDecalMeshElem, mpGraphics, mpResources, sName, sMaterial, decalColor);
		if(pMesh==NULL)	return;
		
		//////////////////////////
		// Create mesh entity
		pMeshEntity = hplNew( cMeshEntity, (sName,pMesh,
											mpResources->GetMaterialManager(),
											mpResources->GetMeshManager(), 
											mpResources->GetAnimationManager()) );
		
		//////////////////////////////////
		// General Final Stuff
		if(pMeshEntity)
		{
			//Add the mesh entity.
			alstMeshEntities.push_back(pMeshEntity);

			//Id
			pMeshEntity->SetUniqueID(lID);
            
			// Add all sub meshes to a new vector
			for(int i=0; i<pMeshEntity->GetSubMeshEntityNum(); ++i)
			{
				cSubMeshEntity *pSubEnt = pMeshEntity->GetSubMeshEntity(i);

				cHplMapStaticUserData *pUserData = hplNew(cHplMapStaticUserData, ());
				pUserData->mvScale = 1;
				pUserData->mbCollides = false;
				pUserData->mbVisible = true;
				pUserData->mbCharCollider = false;

				pSubEnt->SetUserData(pUserData);
				mlstTempStaticUserData.push_back(pUserData);

				apDecalContainer->Add(pSubEnt);
			}
		}

	}
	
	//-----------------------------------------------------------------------

	static cMeshEntity* GetAndRemoveMeshEntity(tMeshEntityList& alstMeshEntities, int alID)
	{
		for(tMeshEntityListIt it = alstMeshEntities.begin(); it != alstMeshEntities.end(); ++it)
		{
			cMeshEntity *pEntity = *it;
			if(pEntity->GetUniqueID() == alID)
			{
				alstMeshEntities.erase(it);
				return pEntity;
			}
		}
		return NULL;
	}

	void cWorldLoaderHplMap::CreateStaticObjectCombo(	int lGroupID, const int *apObjIds, size_t alObjIdNum, tMeshEntityList& alstMeshEntities,
														cRenderableContainer_BoxTree *apContainer)
	{
		tMeshEntityList lstCombineMeshes;
		tRenderableList lstCombineSubMeshes;
		
		tIntVec vObjIds(apObjIds, apObjIds + alObjIdNum);

		////////////////////////////
		// Iterate object ids
        for(size_t i=0; i<vObjIds.size(); ++i)
		{
			int lID = vObjIds[i];

			///////////////////////////
			//Get the mesh entity
			cMeshEntity *pMeshEnt = GetAndRemoveMeshEntity(alstMeshEntities, lID);
			if(pMeshEnt==NULL)
			{
				Warning(" Object id %d in group %d does not exist!\n", lID, lGroupID);
				continue;
			}
		
			lstCombineMeshes.push_back(pMeshEnt);
			for(int i=0; i<pMeshEnt->GetSubMeshEntityNum(); ++i)
			{
				lstCombineSubMeshes.push_back(pMeshEnt->GetSubMeshEntity(i));
			}
		}

		if(lstCombineMeshes.empty()) return;

		////////////////////////////
		// Combine the sub meshes and create meshes and bodies
		CombineAndCreateMeshesAndPhysics(&lstCombineSubMeshes);
		
		////////////////////////////
		// Destroy all the mesh entities
		for(tMeshEntityListIt it = lstCombineMeshes.begin(); it != lstCombineMeshes.end(); ++it)
		{
			cMeshEntity *pMeshEnt = *it;
            
			for(int i=0; i<pMeshEnt->GetSubMeshEntityNum(); ++i)
			{
				apContainer->Remove(pMeshEnt->GetSubMeshEntity(i));
			}
			hplDelete(pMeshEnt);
		}
	}
	
	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::EntitiesBegin()
	{
		if(mlCurrentFlags & eWorldLoadFlag_FastEntityLoad)
			mpResources->GetMeshManager()->SetUseFastloadMaterial(true);

		mlstLightBillboardConnections.clear();
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::EntitiesEnd()
	{
		/////////////////////////////////////
		//Set up light and billboard connections
		tEFL_LightBillboardConnectionListIt connIt = mlstLightBillboardConnections.begin();
		for(; connIt != mlstLightBillboardConnections.end(); ++connIt)
		{
			cEFL_LightBillboardConnection& lightConnect = *connIt;

			cBillboard *pBB = mpCurrentWorld->GetBillboardFromUniqueID(lightConnect.msBillboardID);
			iLight *pLight = mpCurrentWorld->GetLight(lightConnect.msLightName);

			if(pLight==NULL)
			{
				Warning("Light with name '%s' does not exist!",lightConnect.msLightName.c_str());
				continue;
			}
			if(pBB==NULL)
			{
				Warning("Billboard with id '%d' does not exist!",lightConnect.msBillboardID);
				continue;
			}

			pLight->AttachBillboard(pBB, pBB->GetColor());
		}
		
		
		//Set fast entity load
		if(mlCurrentFlags & eWorldLoadFlag_FastEntityLoad)
			mpResources->GetMeshManager()->SetUseFastloadMaterial(false);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::LoadEntity(const tString& asName, int alID, bool abActive, const cVector3f& avPos, const cVector3f& avRot, const cVector3f& avScale, const tString& sFilename, cXmlElement* pUserVarsElem)
	{
		cMatrixf mtxTransform = cMath::MatrixRotate(avRot,eEulerRotationOrder_XYZ);
		mtxTransform.SetTranslation(avPos);

		//User variables
		cResourceVarsObject userVars;
		if(pUserVarsElem) userVars.LoadVariables(pUserVarsElem);
		
        //Create in world
		bool bSkipNonStatic = (mlCurrentFlags & eWorldLoadFlag_NoDynamicGameEntities)!=0;
		mpCurrentWorld->CreateEntity(asName, mtxTransform, sFilename,alID, abActive, avScale, &userVars, bSkipNonStatic);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHplMap::LoadArea(const tString& asName, int alID, bool abActive,const cVector3f& avPos, const cVector3f& avRot,const cVector3f& avScale, const tString& sType, cXmlElement* pVarRootElem)
	{
		cMatrixf mtxTransform = cMath::MatrixRotate(avRot,eEulerRotationOrder_XYZ);
		mtxTransform.SetTranslation(avPos);

		iAreaLoader *pLoader  = mpResources->GetAreaLoader(sType);
		if(pLoader==NULL) return;

		//Skip any dynamic area if a flag is set.
		if( (mlCurrentFlags & eWorldLoadFlag_NoDynamicGameEntities)!=0 && pLoader->GetCreatesStaticArea()==false) return;

		//Load variables
		if(pVarRootElem) pLoader->LoadVariables(pVarRootElem);

		//Create the area
		pLoader->Load(asName,alID, abActive,avScale,mtxTransform,mpCurrentWorld);
	}

	//-----------------------------------------------------------------------
}
