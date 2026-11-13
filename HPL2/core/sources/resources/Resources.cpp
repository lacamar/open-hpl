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

#include "resources/Resources.h"

#include "resources/LowLevelResources.h"
#include "resources/FileSearcher.h"
#include "resources/ImageManager.h"
#include "resources/GpuShaderManager.h"
#include "resources/ParticleManager.h"
#include "resources/SoundManager.h"
#include "resources/FontManager.h"
#include "resources/ScriptManager.h"
#include "resources/TextureManager.h"
#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"
#include "resources/MeshLoaderHandler.h"
#include "resources/SoundEntityManager.h"
#include "resources/AnimationManager.h"
#include "resources/VideoManager.h"
#include "resources/EntFileManager.h"
#include "resources/ConfigFile.h"
#include "resources/LanguageFile.h"
#include "resources/XmlDocument.h"
#include "resources/BitmapLoaderHandler.h"
#include "resources/WorldLoaderHandler.h"
#include "resources/VideoLoaderHandler.h"
#include "resources/BinaryBuffer.h"

#include "resources/WorldLoaderHplMap.h"
#include "resources/WorldLoaderHpm.h"

#include "system/System.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"

#include "impl/tinyXML/tinyxml.h"
#include "resources/RustResMgr.h"
#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cResources::mbForceCacheLoadingAndSkipSaving = false;
	tWString cResources::msMeshCacheDir = _W("");
	bool cResources::mbCreateAndLoadCompressedMaps= false; 

	//-----------------------------------------------------------------------

	cResources::cResources(iLowLevelResources *apLowLevelResources,iLowLevelGraphics *apLowLevelGraphics)
		: iUpdateable("Resources"), mpRs(NULL), mpInit(NULL)
	{
		mpLowLevelResources = apLowLevelResources;
		mpLowLevelGraphics = apLowLevelGraphics;

		mpFileSearcher = hplNew( cFileSearcher, () );

		mpLanguageFile = NULL;
		H4Setup();
	}

	//-----------------------------------------------------------------------

	tWString cResources::GetCacheFile(const tWString& asFile, const tWString& asExt)
	{
		tWString sHome = cPlatform::GetSystemSpecialPath(eSystemPath_XDGCacheHome);
		cRmOut out;
		ohpl_rm_cache_file(reinterpret_cast<const uint32_t*>(sHome.data()), sHome.size(), reinterpret_cast<const uint32_t*>(asFile.data()), asFile.size(),
							reinterpret_cast<const uint32_t*>(asExt.data()), asExt.size(), out.raw());
		for(const tWString& sDir : out.mvItems)
		{
			if(cPlatform::FolderExists(sDir) == false) cPlatform::CreateFolder(sDir);
		}
		return out.msPath;
	}

	//-----------------------------------------------------------------------

	cResources::~cResources()
	{
		ohpl_h4_res_shutdown(mpRs);
		ohpl_h4_res_free(mpRs);
		mpRs = NULL;
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// MAP DATA VARS OBJECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	cResourceVarsObject::cResourceVarsObject() : mpVarsRs(ohpl_h2_vars_new())
	{
	}

	cResourceVarsObject::cResourceVarsObject(const cResourceVarsObject &aOther) : mpVarsRs(ohpl_h2_vars_clone(aOther.mpVarsRs))
	{
	}

	cResourceVarsObject& cResourceVarsObject::operator=(const cResourceVarsObject &aOther)
	{
		if(this == &aOther) return *this;
		ohpl_h2_vars_free(mpVarsRs);
		mpVarsRs = ohpl_h2_vars_clone(aOther.mpVarsRs);
		return *this;
	}

	cResourceVarsObject::~cResourceVarsObject()
	{
		ohpl_h2_vars_free(mpVarsRs);
	}

	void cResourceVarsObject::LoadVariables(cXmlElement *apRootElem)
	{
		ohpl_h2_vars_clear(mpVarsRs);

		////////////////////
		//Iterate variables and add them to map.
		cXmlNodeListIterator varIt = apRootElem->GetChildIterator();
		while(varIt.HasNext())
		{
			cXmlElement *pVarElem = varIt.Next()->ToElement();

			tString sName = pVarElem->GetAttributeString("Name");
			tString sValue = pVarElem->GetAttributeString("Value");

			ohpl_h2_vars_insert(mpVarsRs, RM_STR(sName), RM_STR(sValue));
		}
	}

	//-----------------------------------------------------------------------

	void cResourceVarsObject::SetUserVariable(const tString& asName, const tString& asValue)
	{
		ohpl_h2_vars_set(mpVarsRs, RM_STR(asName), RM_STR(asValue));
	}

	//-----------------------------------------------------------------------

	tString* cResourceVarsObject::GetUserVariable(const tString& asName)
	{
		msVarScratch.clear();
		if(ohpl_h2_vars_get(mpVarsRs, RM_STR(asName), [](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &msVarScratch) == false) return NULL;
		return &msVarScratch;
	}
	//-----------------------------------------------------------------------

	void cResourceVarsObject::AddVarBool(const tString& asName, bool abDefault)
	{
		ohpl_h2_vars_add_bool(mpVarsRs, RM_STR(asName), abDefault);
	}

	void cResourceVarsObject::AddVarInt(const tString& asName, int alDefault)
	{
		ohpl_h2_vars_add_int(mpVarsRs, RM_STR(asName), alDefault);
	}

	void cResourceVarsObject::AddVarFloat(const tString& asName, float afDefault)
	{
		ohpl_h2_vars_add_float(mpVarsRs, RM_STR(asName), afDefault);
	}

	void cResourceVarsObject::AddVarVector2f(const tString& asName, const cVector2f& avDefault)
	{
		float vF[2] = { avDefault.x, avDefault.y };
		ohpl_h2_vars_add_floats(mpVarsRs, RM_STR(asName), vF, 2);
	}

	void cResourceVarsObject::AddVarVector3f(const tString& asName, const cVector3f& avDefault)
	{
		float vF[3] = { avDefault.x, avDefault.y, avDefault.z };
		ohpl_h2_vars_add_floats(mpVarsRs, RM_STR(asName), vF, 3);
	}

	void cResourceVarsObject::AddVarColor(const tString& asName, const cColor& aDefault)
	{
		float vF[4] = { aDefault.r, aDefault.g, aDefault.b, aDefault.a };
		ohpl_h2_vars_add_floats(mpVarsRs, RM_STR(asName), vF, 4);
	}

	void cResourceVarsObject::AddVarString(const tString& asName, const tString& asDefault)
	{
		ohpl_h2_vars_set(mpVarsRs, RM_STR(asName), RM_STR(asDefault));
	}

	//-----------------------------------------------------------------------

	bool cResourceVarsObject::GetVarBool(const tString& asName, bool abDefault)
	{
		return ohpl_h2_vars_get_bool(mpVarsRs, RM_STR(asName), abDefault);
	}

	int cResourceVarsObject::GetVarInt(const tString& asName, int alDefault)
	{
		return ohpl_h2_vars_get_int(mpVarsRs, RM_STR(asName), alDefault);
	}

	float cResourceVarsObject::GetVarFloat(const tString& asName, float afDefault)
	{
		return ohpl_h2_vars_get_float(mpVarsRs, RM_STR(asName), afDefault);
	}

	cVector2f cResourceVarsObject::GetVarVector2f(const tString& asName, const cVector2f& avDefault)
	{
		float vF[2];
		return ohpl_h2_vars_get_floats(mpVarsRs, RM_STR(asName), vF, 2) ? cVector2f(vF[0], vF[1]) : avDefault;
	}

	cVector3f cResourceVarsObject::GetVarVector3f(const tString& asName, const cVector3f& avDefault)
	{
		float vF[3];
		return ohpl_h2_vars_get_floats(mpVarsRs, RM_STR(asName), vF, 3) ? cVector3f(vF[0], vF[1], vF[2]) : avDefault;
	}

	cColor cResourceVarsObject::GetVarColor(const tString& asName, const cColor& aDefault)
	{
		float vF[4];
		return ohpl_h2_vars_get_floats(mpVarsRs, RM_STR(asName), vF, 4) ? cColor(vF[0], vF[1], vF[2], vF[3]) : aDefault;
	}

	tString cResourceVarsObject::GetVarString(const tString& asName, const tString& asDefault)
	{
		tString sRsVal;
		return ohpl_h2_vars_get(mpVarsRs, RM_STR(asName), [](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &sRsVal) ? sRsVal : asDefault;
	}

	//-----------------------------------------------------------------------
	

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cResources::Init(	cGraphics* apGraphics,cSystem *apSystem, cSound* apSound, cScene *apScene,
							cGui *apGui, cPhysics *apPhysics)
	{
		cInitArgs args = { apGraphics, apSystem, apSound, apScene, apGui, apPhysics };
		mpInit = &args;
		ohpl_h4_res_init(mpRs);
		mpInit = NULL;
	}
	
	//-----------------------------------------------------------------------

	void cResources::Update(float afTimeStep)
	{
		int32_t lBits;
		memcpy(&lBits, &afTimeStep, sizeof(lBits));
		ohpl_h4_res_update(mpRs, lBits);
	}

	//-----------------------------------------------------------------------

	cFileSearcher* cResources::GetFileSearcher()
	{
		return mpFileSearcher;
	}

	//-----------------------------------------------------------------------

	/**
	 * \todo File searcher should check so if the dir is allready added and if so return false and not add
	 * \param &asDir 
	 * \param &asMask 
	 * \return 
	 */
	bool cResources::AddResourceDir(const tWString &asDir, bool abAddSubDirectories, const tString &asMask)
	{
		return ohpl_h4_res_add_dir(mpRs, reinterpret_cast<const uint32_t*>(asDir.data()), asDir.size(), abAddSubDirectories, RM_STR(asMask));
	}

	void cResources::ClearResourceDirs()
	{	
		ohpl_h4_res_clear_dirs(mpRs);
	}

	//-----------------------------------------------------------------------

	bool cResources::AddLanguageFile(const tString &asFilePath, bool abAddResourceDirs, const tWString &asAltPath)
	{
		if(mpLanguageFile == NULL)
		{
			mpLanguageFile = hplNew( cLanguageFile, (this) );
		}

		return mpLanguageFile->AddFromFile(cString::To16Char(asFilePath), abAddResourceDirs, asAltPath);
	}

	void cResources::ClearTranslations()
	{
		if(mpLanguageFile)
		{
			hplDelete(mpLanguageFile);
			mpLanguageFile = NULL;
		}
	}

	const tWString& cResources::Translate(const tString& asCat, const tString& asName)
	{
		if(mpLanguageFile)
		{
			return mpLanguageFile->Translate(asCat,asName);
		}
		else
		{
			return mwsEmptyString;
		}
	}

	//-----------------------------------------------------------------------

	void cResources::AddEntityLoader(iEntityLoader* apLoader, bool abSetAsDefault)
	{
		ohpl_h4_res_add_loader(mpRs, false, RM_STR(apLoader->GetName()), apLoader, abSetAsDefault);
	}

	iEntityLoader* cResources::GetEntityLoader(const tString& asName)
	{
		return static_cast<iEntityLoader*>(ohpl_h4_res_get_loader(mpRs, false, RM_STR(asName)));
	}

	//-----------------------------------------------------------------------

	void cResources::AddAreaLoader(iAreaLoader* apLoader, bool abSetAsDefault)
	{
		ohpl_h4_res_add_loader(mpRs, true, RM_STR(apLoader->GetName()), apLoader, abSetAsDefault);
	}

	iAreaLoader* cResources::GetAreaLoader(const tString& asName)
	{
		return static_cast<iAreaLoader*>(ohpl_h4_res_get_loader(mpRs, true, RM_STR(asName)));
	}

	//-----------------------------------------------------------------------

	bool cResources::LoadResourceDirsFile(const tString &asFile, const tWString &asAltPath)
	{
		return ohpl_h4_res_dirs_file(mpRs, RM_STR(asFile), reinterpret_cast<const uint32_t*>(asAltPath.data()), asAltPath.size());
	}

	//-----------------------------------------------------------------------

	iXmlDocument* cResources::LoadXmlDocument(const tString& asFile)
	{
		return static_cast<iXmlDocument*>(ohpl_h4_res_load_xml(mpRs, RM_STR(asFile)));
	}
	
	void cResources::DestroyXmlDocument(iXmlDocument* apDoc)
	{
		ohpl_h4_res_destroy_xml(mpRs, apDoc);
	}

	//-----------------------------------------------------------------------

	cBinaryBuffer* cResources::LoadBinaryBuffer(const tString& asFile)
	{
		return static_cast<cBinaryBuffer*>(ohpl_h4_res_load_bin(mpRs, RM_STR(asFile)));
	}
	
	void cResources::DestroyBinaryBuffer(cBinaryBuffer* apFile)
	{
		ohpl_h4_res_destroy_bin(mpRs, apFile);
	}

	//-----------------------------------------------------------------------
	
	iLowLevelResources* cResources::GetLowLevel()
	{
		return mpLowLevelResources;
	}

	//-----------------------------------------------------------------------


	void cResources::H4Setup()
	{
		OhplH4ResOps ops;
		memset(&ops, 0, sizeof(ops));
		ops.ctx = this;
		ops.log = [](void*, int32_t alLevel, const uint8_t *apMsg, size_t alLen)
		{
			tString sMsg(reinterpret_cast<const char*>(apMsg), alLen);
			if(alLevel == 2) Error("%s", sMsg.c_str());
			else if(alLevel == 1) Warning("%s", sMsg.c_str());
			else Log("%s", sMsg.c_str());
		};
		ops.create = [](void *c, int32_t alWhat, OhplH3WStr aArg) -> void*
		{
			cResources *p = static_cast<cResources*>(c);
			cInitArgs *a = p->mpInit;
			tWString sArg(reinterpret_cast<const wchar_t*>(aArg.p), aArg.n);
			switch(alWhat)
			{
			case OHPL_H4W_MESH_HANDLER: p->mpMeshLoaderHandler = hplNew( cMeshLoaderHandler,(p, a->mpScene) ); return p->mpMeshLoaderHandler;
			case OHPL_H4W_BITMAP_HANDLER: p->mpBitmapLoaderHandler = hplNew( cBitmapLoaderHandler,(p, a->mpGraphics) ); return p->mpBitmapLoaderHandler;
			case OHPL_H4W_WORLD_HANDLER: p->mpWorldLoaderHandler = hplNew( cWorldLoaderHandler,(p, a->mpGraphics, a->mpScene, a->mpPhysics) ); return p->mpWorldLoaderHandler;
			case OHPL_H4W_VIDEO_HANDLER: p->mpVideoLoaderHandler = hplNew( cVideoLoaderHandler,(p, a->mpGraphics) ); return p->mpVideoLoaderHandler;
			case OHPL_H4W_IMAGE: p->mpImageManager = hplNew( cImageManager,(p, p->mpLowLevelGraphics, p->mpLowLevelSystem) ); return static_cast<iResourceManager*>(p->mpImageManager);
			case OHPL_H4W_GPU_SHADER: p->mpGpuShaderManager = hplNew( cGpuShaderManager,(p->mpFileSearcher, p->mpLowLevelGraphics, p->mpLowLevelResources, p->mpLowLevelSystem) ); return static_cast<iResourceManager*>(p->mpGpuShaderManager);
			case OHPL_H4W_PARTICLE: p->mpParticleManager = hplNew( cParticleManager,(a->mpGraphics, p) ); return static_cast<iResourceManager*>(p->mpParticleManager);
			case OHPL_H4W_SOUND: p->mpSoundManager = hplNew( cSoundManager,(a->mpSound, p) ); return static_cast<iResourceManager*>(p->mpSoundManager);
			case OHPL_H4W_FONT: p->mpFontManager = hplNew( cFontManager,(a->mpGraphics, a->mpGui, p) ); return static_cast<iResourceManager*>(p->mpFontManager);
			case OHPL_H4W_SCRIPT: p->mpScriptManager = hplNew( cScriptManager,(a->mpSystem, p) ); return static_cast<iResourceManager*>(p->mpScriptManager);
			case OHPL_H4W_TEXTURE: p->mpTextureManager = hplNew( cTextureManager,(a->mpGraphics, p) ); return static_cast<iResourceManager*>(p->mpTextureManager);
			case OHPL_H4W_MATERIAL: p->mpMaterialManager = hplNew( cMaterialManager,(a->mpGraphics, p) ); return static_cast<iResourceManager*>(p->mpMaterialManager);
			case OHPL_H4W_MESH: p->mpMeshManager = hplNew( cMeshManager,(a->mpGraphics, p) ); return static_cast<iResourceManager*>(p->mpMeshManager);
			case OHPL_H4W_SOUND_ENTITY: p->mpSoundEntityManager = hplNew( cSoundEntityManager,(a->mpSound, p) ); return static_cast<iResourceManager*>(p->mpSoundEntityManager);
			case OHPL_H4W_ANIMATION: p->mpAnimationManager = hplNew( cAnimationManager,(a->mpGraphics, p) ); return static_cast<iResourceManager*>(p->mpAnimationManager);
			case OHPL_H4W_VIDEO: p->mpVideoManager = hplNew( cVideoManager,(a->mpGraphics, p) ); return static_cast<iResourceManager*>(p->mpVideoManager);
			case OHPL_H4W_ENT_FILE: p->mpEntFileManager = hplNew( cEntFileManager,(p) ); return static_cast<iResourceManager*>(p->mpEntFileManager);
			case OHPL_H4W_XML_DOC:
			{
				iXmlDocument *pDoc = p->mpLowLevelResources->CreateXmlDocument();
				if(pDoc->CreateFromFile(sArg)==false) { hplDelete(pDoc); return NULL; }
				return pDoc;
			}
			case OHPL_H4W_BIN_BUFFER:
			{
				cBinaryBuffer *pFile = hplNew(cBinaryBuffer, (sArg) );
				if(pFile->Load()==false) { hplDelete(pFile); return NULL; }
				return pFile;
			}
			}
			return NULL;
		};
		ops.destroy = [](void *c, int32_t alWhat, void *apObj)
		{
			cResources *p = static_cast<cResources*>(c);
			switch(alWhat)
			{
			case OHPL_H4W_MESH_HANDLER: hplDelete(p->mpMeshLoaderHandler); break;
			case OHPL_H4W_BITMAP_HANDLER: hplDelete(p->mpBitmapLoaderHandler); break;
			case OHPL_H4W_WORLD_HANDLER: hplDelete(p->mpWorldLoaderHandler); break;
			case OHPL_H4W_VIDEO_HANDLER: hplDelete(p->mpVideoLoaderHandler); break;
			case OHPL_H4W_IMAGE: hplDelete(p->mpImageManager); break;
			case OHPL_H4W_GPU_SHADER: hplDelete(p->mpGpuShaderManager); break;
			case OHPL_H4W_PARTICLE: hplDelete(p->mpParticleManager); break;
			case OHPL_H4W_SOUND: hplDelete(p->mpSoundManager); break;
			case OHPL_H4W_FONT: hplDelete(p->mpFontManager); break;
			case OHPL_H4W_SCRIPT: hplDelete(p->mpScriptManager); break;
			case OHPL_H4W_TEXTURE: hplDelete(p->mpTextureManager); break;
			case OHPL_H4W_MATERIAL: hplDelete(p->mpMaterialManager); break;
			case OHPL_H4W_MESH: hplDelete(p->mpMeshManager); break;
			case OHPL_H4W_SOUND_ENTITY: hplDelete(p->mpSoundEntityManager); break;
			case OHPL_H4W_ANIMATION: hplDelete(p->mpAnimationManager); break;
			case OHPL_H4W_VIDEO: hplDelete(p->mpVideoManager); break;
			case OHPL_H4W_ENT_FILE: hplDelete(p->mpEntFileManager); break;
			case OHPL_H4W_FILE_SEARCHER: hplDelete(p->mpFileSearcher); break;
			case OHPL_H4W_LANGUAGE: hplDelete(p->mpLanguageFile); break;
			case OHPL_H4W_XML_DOC: hplDelete(static_cast<iXmlDocument*>(apObj)); break;
			case OHPL_H4W_BIN_BUFFER: hplDelete(static_cast<cBinaryBuffer*>(apObj)); break;
			case OHPL_H4W_ENTITY_LOADER: hplDelete(static_cast<iEntityLoader*>(apObj)); break;
			case OHPL_H4W_AREA_LOADER: hplDelete(static_cast<iAreaLoader*>(apObj)); break;
			}
		};
		ops.call = [](void *c, int32_t alWhat, void *apObj, int64_t alArg) -> int64_t
		{
			cResources *p = static_cast<cResources*>(c);
			switch(alWhat)
			{
			case OHPL_H4W_LOW_LEVEL_SYSTEM: p->mpLowLevelSystem = p->mpInit->mpSystem->GetLowLevel(); break;
			case OHPL_H4W_LOADERS_ADD:
				p->mpLowLevelResources->AddBitmapLoaders(p->mpBitmapLoaderHandler);
				p->mpLowLevelResources->AddMeshLoaders(p->mpMeshLoaderHandler);
				p->mpLowLevelResources->AddVideoLoaders(p->mpVideoLoaderHandler);
				p->mpWorldLoaderHandler->AddLoader(hplNew(cWorldLoaderHplMap, () ));
				p->mpWorldLoaderHandler->AddLoader(hplNew(cWorldLoaderHpm, () ));
				break;
			case OHPL_H4W_UPDATE_MANAGER:
			{
				int32_t lBits = (int32_t)alArg;
				float fTime;
				memcpy(&fTime, &lBits, sizeof(fTime));
				static_cast<iResourceManager*>(apObj)->Update(fTime);
				break;
			}
			case OHPL_H4W_HAS_LANGUAGE: return p->mpLanguageFile != NULL;
			}
			return 0;
		};
		ops.file_path = [](void *c, const uint8_t *apName, size_t alLen, int32_t *apCount, size_t *apLen) -> const uint32_t*
		{
			const tWString &sPath = static_cast<cResources*>(c)->mpFileSearcher->GetFilePath(tString(reinterpret_cast<const char*>(apName), alLen));
			*apLen = sPath.size();
			return reinterpret_cast<const uint32_t*>(sPath.data());
		};
		ops.add_dir = [](void *c, OhplH3WStr aDir, OhplH3Str aMask, bool abSubDirs)
		{
			static_cast<cResources*>(c)->mpFileSearcher->AddDirectory(tWString(reinterpret_cast<const wchar_t*>(aDir.p), aDir.n),
																	tString(reinterpret_cast<const char*>(aMask.p), aMask.n), abSubDirs);
		};
		ops.clear_dirs = [](void *c){ static_cast<cResources*>(c)->mpFileSearcher->ClearDirectories(); };
		ops.read_dirs = [](void *c, OhplH3Str aFile, void (*apEach)(void*, OhplH3Str, bool), void *apCtx) -> bool
		{
			cResources *p = static_cast<cResources*>(c);
			iXmlDocument* pDoc = p->mpLowLevelResources->CreateXmlDocument();
			if(pDoc->CreateFromFile(cString::To16Char(tString(reinterpret_cast<const char*>(aFile.p), aFile.n)))==false)
			{
				hplDelete(pDoc);
				return false;
			}
			cXmlNodeListIterator it = pDoc->GetChildIterator();
			while(it.HasNext())
			{
				cXmlElement *pChildElem = it.Next()->ToElement();
				tString sPath = pChildElem->GetAttributeString("Path");
				if(sPath==""){
					continue;
				}
				apEach(apCtx, OhplH3Str{reinterpret_cast<const uint8_t*>(sPath.data()), sPath.size()}, pChildElem->GetAttributeBool("AddSubDirs",false));
			}
			hplDelete(pDoc);
			return true;
		};
		mpRs = ohpl_h4_res_new(&ops);
	}
}
