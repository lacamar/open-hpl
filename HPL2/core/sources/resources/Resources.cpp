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
#include "impl/RustBackends.h"
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

		mpEntityLoaders = ohpl_rm_loaders_new();
		mpAreaLoaders = ohpl_rm_loaders_new();

		mpLanguageFile = NULL;
		if(OhplBackendRustRequested()) H4Setup();
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
		if(mpRs)
		{
			ohpl_h4_res_shutdown(mpRs);
			ohpl_h4_res_free(mpRs);
			mpRs = NULL;
			ohpl_rm_loaders_free(mpEntityLoaders);
			ohpl_rm_loaders_free(mpAreaLoaders);
			return;
		}
		Log("Exiting Resources Module\n");
		Log("--------------------------------------------------------\n");

		ohpl_rm_loaders_each(mpEntityLoaders, [](void*, void* apLoader){ hplDelete(static_cast<iEntityLoader*>(apLoader)); }, NULL);
		ohpl_rm_loaders_each(mpAreaLoaders, [](void*, void* apLoader){ hplDelete(static_cast<iAreaLoader*>(apLoader)); }, NULL);
		ohpl_rm_loaders_free(mpEntityLoaders);
		ohpl_rm_loaders_free(mpAreaLoaders);

		STLDeleteAll(mlstXmlDocuments);
		STLDeleteAll(mlstBinBuffers);
		
		hplDelete(mpFontManager);
		hplDelete(mpScriptManager);
		hplDelete(mpParticleManager);
		hplDelete(mpSoundManager);
		hplDelete(mpMeshManager);
		hplDelete(mpMaterialManager);
		hplDelete(mpGpuShaderManager);
		hplDelete(mpImageManager);
		hplDelete(mpTextureManager);
		hplDelete(mpSoundEntityManager);
		hplDelete(mpAnimationManager);
		hplDelete(mpVideoManager);
		hplDelete(mpEntFileManager);
		
		Log(" All resources deleted\n");

		hplDelete(mpFileSearcher);
		
		hplDelete(mpMeshLoaderHandler);
		hplDelete(mpBitmapLoaderHandler);
		hplDelete(mpWorldLoaderHandler);
		hplDelete(mpVideoLoaderHandler);

		if(mpLanguageFile) hplDelete(mpLanguageFile);
		
		mlstManagers.clear();
		Log("--------------------------------------------------------\n\n");
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// MAP DATA VARS OBJECT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	cResourceVarsObject::cResourceVarsObject() : mpVarsRs(OhplBackendRustRequested() ? ohpl_h2_vars_new() : NULL)
	{
	}

	cResourceVarsObject::cResourceVarsObject(const cResourceVarsObject &aOther) : m_mapVars(aOther.m_mapVars), mpVarsRs(aOther.mpVarsRs ? ohpl_h2_vars_clone(aOther.mpVarsRs) : NULL)
	{
	}

	cResourceVarsObject& cResourceVarsObject::operator=(const cResourceVarsObject &aOther)
	{
		if(this == &aOther) return *this;
		m_mapVars = aOther.m_mapVars;
		if(mpVarsRs) ohpl_h2_vars_free(mpVarsRs);
		mpVarsRs = aOther.mpVarsRs ? ohpl_h2_vars_clone(aOther.mpVarsRs) : NULL;
		return *this;
	}

	cResourceVarsObject::~cResourceVarsObject()
	{
		if(mpVarsRs) ohpl_h2_vars_free(mpVarsRs);
	}

	void cResourceVarsObject::LoadVariables(cXmlElement *apRootElem)
	{
		if(mpVarsRs) ohpl_h2_vars_clear(mpVarsRs);
		//////////////////////
		//Clear any existing variables
		m_mapVars.clear();

		////////////////////
		//Iterate variables and add them to map.
		cXmlNodeListIterator varIt = apRootElem->GetChildIterator();
		while(varIt.HasNext())
		{
			cXmlElement *pVarElem = varIt.Next()->ToElement();

			tString sName = pVarElem->GetAttributeString("Name");
			tString sValue = pVarElem->GetAttributeString("Value");

			if(mpVarsRs) ohpl_h2_vars_insert(mpVarsRs, RM_STR(sName), RM_STR(sValue));
			else m_mapVars.insert(tResourceVarMap::value_type(sName, sValue));
		}
	}

	//-----------------------------------------------------------------------

	void cResourceVarsObject::SetUserVariable(const tString& asName, const tString& asValue)
	{
		if(mpVarsRs) { ohpl_h2_vars_set(mpVarsRs, RM_STR(asName), RM_STR(asValue)); return; }
		m_mapVars[asName] = asValue;
	}

	//-----------------------------------------------------------------------

	tString* cResourceVarsObject::GetUserVariable(const tString& asName)
	{
		if(mpVarsRs)
		{
			msVarScratch.clear();
			if(ohpl_h2_vars_get(mpVarsRs, RM_STR(asName), [](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &msVarScratch) == false) return NULL;
			return &msVarScratch;
		}
		tResourceVarMapIt it = m_mapVars.find(asName);
		if(it == m_mapVars.end()) {
			//Warning("Did not find user variable '%s'\n", asName.c_str());
			return NULL;
		}

		return &(it->second);
	}
	//-----------------------------------------------------------------------

	void cResourceVarsObject::AddVarBool(const tString& asName, bool abDefault)
	{
		if(mpVarsRs) { ohpl_h2_vars_add_bool(mpVarsRs, RM_STR(asName), abDefault); return; }
		SetUserVariable(asName, abDefault? "true" : "false");
	}

	void cResourceVarsObject::AddVarInt(const tString& asName, int alDefault)
	{
		if(mpVarsRs) { ohpl_h2_vars_add_int(mpVarsRs, RM_STR(asName), alDefault); return; }
		SetUserVariable(asName, cString::ToString(alDefault));
	}

	void cResourceVarsObject::AddVarFloat(const tString& asName, float afDefault)
	{
		if(mpVarsRs) { ohpl_h2_vars_add_float(mpVarsRs, RM_STR(asName), afDefault); return; }
		SetUserVariable(asName, cString::ToString(afDefault,-1,true));
	}

	void cResourceVarsObject::AddVarVector2f(const tString& asName, const cVector2f& avDefault)
	{
		if(mpVarsRs) { float vF[2] = { avDefault.x, avDefault.y }; ohpl_h2_vars_add_floats(mpVarsRs, RM_STR(asName), vF, 2); return; }
		tString sVector = avDefault.ToString();
		sVector = cString::ReplaceCharTo(sVector,":"," ");
		SetUserVariable(asName, sVector);
	}

	void cResourceVarsObject::AddVarVector3f(const tString& asName, const cVector3f& avDefault)
	{
		if(mpVarsRs) { float vF[3] = { avDefault.x, avDefault.y, avDefault.z }; ohpl_h2_vars_add_floats(mpVarsRs, RM_STR(asName), vF, 3); return; }
		tString sVector = avDefault.ToString();
		sVector = cString::ReplaceCharTo(sVector,":"," ");
		SetUserVariable(asName, sVector);
	}

	void cResourceVarsObject::AddVarColor(const tString& asName, const cColor& aDefault)
	{
		if(mpVarsRs) { float vF[4] = { aDefault.r, aDefault.g, aDefault.b, aDefault.a }; ohpl_h2_vars_add_floats(mpVarsRs, RM_STR(asName), vF, 4); return; }
		tString sColor = aDefault.ToString();
		sColor = cString::ReplaceCharTo(sColor,":"," ");
		SetUserVariable(asName, sColor);
	}

	void cResourceVarsObject::AddVarString(const tString& asName, const tString& asDefault)
	{
		if(mpVarsRs) { ohpl_h2_vars_set(mpVarsRs, RM_STR(asName), RM_STR(asDefault)); return; }
		SetUserVariable(asName, asDefault);
	}

	//-----------------------------------------------------------------------

	bool cResourceVarsObject::GetVarBool(const tString& asName, bool abDefault)
	{
		if(mpVarsRs) return ohpl_h2_vars_get_bool(mpVarsRs, RM_STR(asName), abDefault);
		tString *pVal = GetUserVariable(asName);
		return cString::ToBool(pVal ? pVal->c_str() : NULL,abDefault);
	}

	int cResourceVarsObject::GetVarInt(const tString& asName, int alDefault)
	{
		if(mpVarsRs) return ohpl_h2_vars_get_int(mpVarsRs, RM_STR(asName), alDefault);
		tString *pVal = GetUserVariable(asName);
		return cString::ToInt(pVal ? pVal->c_str() : NULL,alDefault);
	}

	float cResourceVarsObject::GetVarFloat(const tString& asName, float afDefault)
	{
		if(mpVarsRs) return ohpl_h2_vars_get_float(mpVarsRs, RM_STR(asName), afDefault);
		tString *pVal = GetUserVariable(asName);
		return cString::ToFloat(pVal ? pVal->c_str() : NULL,afDefault);
	}

	cVector2f cResourceVarsObject::GetVarVector2f(const tString& asName, const cVector2f& avDefault)
	{
		if(mpVarsRs) { float vF[2]; return ohpl_h2_vars_get_floats(mpVarsRs, RM_STR(asName), vF, 2) ? cVector2f(vF[0], vF[1]) : avDefault; }
		tString *pVal = GetUserVariable(asName);
		return cString::ToVector2f(pVal ? pVal->c_str() : NULL,avDefault);
	}

	cVector3f cResourceVarsObject::GetVarVector3f(const tString& asName, const cVector3f& avDefault)
	{
		if(mpVarsRs) { float vF[3]; return ohpl_h2_vars_get_floats(mpVarsRs, RM_STR(asName), vF, 3) ? cVector3f(vF[0], vF[1], vF[2]) : avDefault; }
		tString *pVal = GetUserVariable(asName);
		return cString::ToVector3f(pVal ? pVal->c_str() : NULL,avDefault);
	}

	cColor cResourceVarsObject::GetVarColor(const tString& asName, const cColor& aDefault)
	{
		if(mpVarsRs) { float vF[4]; return ohpl_h2_vars_get_floats(mpVarsRs, RM_STR(asName), vF, 4) ? cColor(vF[0], vF[1], vF[2], vF[3]) : aDefault; }
		tString *pVal = GetUserVariable(asName);
		return cString::ToColor(pVal ? pVal->c_str() : NULL,aDefault);
	}

	tString cResourceVarsObject::GetVarString(const tString& asName, const tString& asDefault)
	{
		if(mpVarsRs)
		{
			tString sRsVal;
			return ohpl_h2_vars_get(mpVarsRs, RM_STR(asName), [](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &sRsVal) ? sRsVal : asDefault;
		}
		tString *pVal = GetUserVariable(asName);
		return cString::ToString(pVal ? pVal->c_str() : NULL,asDefault);
	}

	//-----------------------------------------------------------------------
	

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cResources::Init(	cGraphics* apGraphics,cSystem *apSystem, cSound* apSound, cScene *apScene,
							cGui *apGui, cPhysics *apPhysics)
	{
		if(mpRs)
		{
			cInitArgs args = { apGraphics, apSystem, apSound, apScene, apGui, apPhysics };
			mpInit = &args;
			ohpl_h4_res_init(mpRs);
			mpInit = NULL;
			return;
		}
		Log("Initializing Resources Module\n");
		Log("--------------------------------------------------------\n");

		mpLowLevelSystem = apSystem->GetLowLevel();

		Log(" Creating loader handlers \n");

		mpMeshLoaderHandler = hplNew( cMeshLoaderHandler,(this, apScene) );
		mpBitmapLoaderHandler = hplNew( cBitmapLoaderHandler,(this, apGraphics) );
		mpWorldLoaderHandler = hplNew( cWorldLoaderHandler,(this, apGraphics,apScene,apPhysics) );
		mpVideoLoaderHandler = hplNew( cVideoLoaderHandler,(this, apGraphics) );

		Log(" Creating resource managers\n");

		mpImageManager = hplNew( cImageManager,(this,mpLowLevelGraphics,mpLowLevelSystem) );
		mlstManagers.push_back(mpImageManager);
		mpGpuShaderManager = hplNew( cGpuShaderManager,(mpFileSearcher,mpLowLevelGraphics,mpLowLevelResources,mpLowLevelSystem) );
		mlstManagers.push_back(mpGpuShaderManager);
		mpParticleManager = hplNew( cParticleManager,(apGraphics, this) );
		mlstManagers.push_back(mpParticleManager);
		mpSoundManager = hplNew( cSoundManager,(apSound, this) );
		mlstManagers.push_back(mpParticleManager);
		mpFontManager = hplNew( cFontManager,(apGraphics,apGui, this) );
		mlstManagers.push_back(mpFontManager);
		mpScriptManager = hplNew( cScriptManager,(apSystem, this) );
		mlstManagers.push_back(mpScriptManager);
		mpTextureManager = hplNew( cTextureManager,(apGraphics, this) );
		mlstManagers.push_back(mpTextureManager);
		mpMaterialManager = hplNew( cMaterialManager,(apGraphics, this) );
		mlstManagers.push_back(mpMaterialManager);
		mpMeshManager = hplNew( cMeshManager,(apGraphics, this) );
		mlstManagers.push_back(mpMeshManager);
		mpSoundEntityManager = hplNew( cSoundEntityManager,(apSound, this) );
		mlstManagers.push_back(mpSoundEntityManager);
		mpAnimationManager = hplNew( cAnimationManager,(apGraphics, this) );
		mlstManagers.push_back(mpAnimationManager);
		mpVideoManager = hplNew( cVideoManager,(apGraphics, this) );
		mlstManagers.push_back(mpVideoManager);
		mpEntFileManager = hplNew( cEntFileManager,(this) );
		mlstManagers.push_back(mpEntFileManager);

		Log(" Adding loaders to handlers \n");

		//Low level resources will load non-propitary formats.
		mpLowLevelResources->AddBitmapLoaders(mpBitmapLoaderHandler);
		mpLowLevelResources->AddMeshLoaders(mpMeshLoaderHandler);
		mpLowLevelResources->AddVideoLoaders(mpVideoLoaderHandler);
		
		//Add properitary formats directly
        mpWorldLoaderHandler->AddLoader(hplNew(cWorldLoaderHplMap, () ));

		mpWorldLoaderHandler->AddLoader(hplNew(cWorldLoaderHpm, () ));

		Log("--------------------------------------------------------\n\n");
	}
	
	//-----------------------------------------------------------------------

	void cResources::Update(float afTimeStep)
	{
		if(mpRs)
		{
			int32_t lBits;
			memcpy(&lBits, &afTimeStep, sizeof(lBits));
			ohpl_h4_res_update(mpRs, lBits);
			return;
		}
		tResourceManagerListIt it = mlstManagers.begin();
		for(; it != mlstManagers.end(); ++it)
		{
			iResourceManager *pManager = *it;

			pManager->Update(afTimeStep);
		}
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
		if(mpRs) return ohpl_h4_res_add_dir(mpRs, reinterpret_cast<const uint32_t*>(asDir.data()), asDir.size(), abAddSubDirectories, RM_STR(asMask));
		mpFileSearcher->AddDirectory(asDir, asMask, abAddSubDirectories);
		if(iResourceBase::GetLogCreateAndDelete())
			Log(" Added resource directory '%s'\n",cString::To8Char(asDir).c_str());
		return true;
	}

	void cResources::ClearResourceDirs()
	{	
		if(mpRs) { ohpl_h4_res_clear_dirs(mpRs); return; }
		mpFileSearcher->ClearDirectories();
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
		if(mpRs) { ohpl_h4_res_add_loader(mpRs, false, RM_STR(apLoader->GetName()), apLoader, abSetAsDefault); return; }
		ohpl_rm_loaders_add(mpEntityLoaders, RM_STR(apLoader->GetName()), apLoader, abSetAsDefault);
	}

	iEntityLoader* cResources::GetEntityLoader(const tString& asName)
	{
		if(mpRs) return static_cast<iEntityLoader*>(ohpl_h4_res_get_loader(mpRs, false, RM_STR(asName)));
		bool bMissing = false;
		iEntityLoader* pLoader = static_cast<iEntityLoader*>(ohpl_rm_loaders_get(mpEntityLoaders, RM_STR(asName), &bMissing));
		if(bMissing)
		{
			Warning("No loader for type '%s' found!\n",asName.c_str());
			if(pLoader) Log("Using default loader!\n");
		}
		return pLoader;
	}

	//-----------------------------------------------------------------------

	void cResources::AddAreaLoader(iAreaLoader* apLoader, bool abSetAsDefault)
	{
		if(mpRs) { ohpl_h4_res_add_loader(mpRs, true, RM_STR(apLoader->GetName()), apLoader, abSetAsDefault); return; }
		ohpl_rm_loaders_add(mpAreaLoaders, RM_STR(apLoader->GetName()), apLoader, abSetAsDefault);
	}

	iAreaLoader* cResources::GetAreaLoader(const tString& asName)
	{
		if(mpRs) return static_cast<iAreaLoader*>(ohpl_h4_res_get_loader(mpRs, true, RM_STR(asName)));
		bool bMissing = false;
		iAreaLoader* pLoader = static_cast<iAreaLoader*>(ohpl_rm_loaders_get(mpAreaLoaders, RM_STR(asName), &bMissing));
		if(bMissing)
		{
			Warning("No loader for area type '%s' found!\n",asName.c_str());
			if(pLoader) Log("Using default loader!\n");
		}
		return pLoader;
	}

	//-----------------------------------------------------------------------

	bool cResources::LoadResourceDirsFile(const tString &asFile, const tWString &asAltPath)
	{
		if(mpRs) return ohpl_h4_res_dirs_file(mpRs, RM_STR(asFile), reinterpret_cast<const uint32_t*>(asAltPath.data()), asAltPath.size());
		iXmlDocument* pDoc = mpLowLevelResources->CreateXmlDocument();
		if(pDoc->CreateFromFile(cString::To16Char(asFile))==false)
		{
			Error("Couldn't load XML file '%s'!\n",asFile.c_str());
			hplDelete( pDoc);
			return false;
		}

		//Get the root.
		cXmlNodeListIterator it = pDoc->GetChildIterator();
		while(it.HasNext())
		{
			cXmlElement *pChildElem = it.Next()->ToElement();

			tString sPath = pChildElem->GetAttributeString("Path");
			if(sPath==""){
				continue;
			}

			bool bAddSubDirs = pChildElem->GetAttributeBool("AddSubDirs",false);

			cRmOut dirs;
			ohpl_rm_resource_dirs(RM_STR(sPath), reinterpret_cast<const uint32_t*>(asAltPath.data()), asAltPath.size(), dirs.raw());
			for(const tWString& sDir : dirs.mvItems) AddResourceDir(sDir,bAddSubDirs);
		}

		hplDelete( pDoc);
		return true;
	}

	//-----------------------------------------------------------------------

	iXmlDocument* cResources::LoadXmlDocument(const tString& asFile)
	{
		if(mpRs) return static_cast<iXmlDocument*>(ohpl_h4_res_load_xml(mpRs, RM_STR(asFile)));
		tWString sPath = mpFileSearcher->GetFilePath(asFile);
		if(sPath == _W("")){
			Error("Could not load XML document '%s'\n",asFile.c_str());
			return NULL;
		}
		
        iXmlDocument *pDoc = mpLowLevelResources->CreateXmlDocument();
		if(pDoc->CreateFromFile(sPath)==false)
		{
			Error("Could not parse/load XML from %s\n",cString::To8Char(sPath).c_str());
			hplDelete( pDoc );
			return NULL;
		}

		mlstXmlDocuments.push_back(pDoc);

		return pDoc;
	}
	
	void cResources::DestroyXmlDocument(iXmlDocument* apDoc)
	{
		if(mpRs) { ohpl_h4_res_destroy_xml(mpRs, apDoc); return; }
		STLFindAndDelete(mlstXmlDocuments,apDoc);
	}

	//-----------------------------------------------------------------------

	cBinaryBuffer* cResources::LoadBinaryBuffer(const tString& asFile)
	{
		if(mpRs) return static_cast<cBinaryBuffer*>(ohpl_h4_res_load_bin(mpRs, RM_STR(asFile)));
		tWString sPath = mpFileSearcher->GetFilePath(asFile);
		if(sPath == _W("")){
			Error("Could not find filepath for binary buffer '%s'\n",asFile.c_str());
			return NULL;
		}

		cBinaryBuffer *pFile = hplNew(cBinaryBuffer, (sPath) );
		if(pFile->Load()==false)
		{
			Error("Could not load binary file from %s\n",cString::To8Char(sPath).c_str());
			hplDelete( pFile );
			return NULL;
		}

		mlstBinBuffers.push_back(pFile);

		return pFile;
	}
	
	void cResources::DestroyBinaryBuffer(cBinaryBuffer* apFile)
	{
		if(mpRs) { ohpl_h4_res_destroy_bin(mpRs, apFile); return; }
		STLFindAndDelete(mlstBinBuffers,apFile);
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
