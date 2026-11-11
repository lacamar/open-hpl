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

#include "resources/TextureManager.h"
#include "system/String.h"
#include "graphics/Graphics.h"
#include "resources/Resources.h"
#include "graphics/Texture.h"
#include "graphics/LowLevelGraphics.h"
#include "resources/LowLevelResources.h"
#include "system/LowLevelSystem.h"
#include "resources/FileSearcher.h"
#include "graphics/Bitmap.h"
#include "resources/BitmapLoaderHandler.h"
#include "resources/RustResMgr.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cTextureManager::mbHardwareSRGB = false;

	cTextureManager::cTextureManager(cGraphics* apGraphics,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		
		mpBitmapLoaderHandler = mpResources->GetBitmapLoaderHandler();

		mlMemoryUsage =0;
		
		mvCubeSideSuffixes.push_back("_pos_x");
		mvCubeSideSuffixes.push_back("_neg_x");
		mvCubeSideSuffixes.push_back("_pos_y");
		mvCubeSideSuffixes.push_back("_neg_y");
		mvCubeSideSuffixes.push_back("_pos_z");
		mvCubeSideSuffixes.push_back("_neg_z");
	}

	cTextureManager::~cTextureManager()
	{
		STLMapDeleteAll(m_mapAttenuationTextures);
		DestroyAll();
		Log(" Destroyed all textures\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iTexture* cTextureManager::Create1D(const tString& asName,bool abUseMipMaps, 
										eTextureUsage aUsage, unsigned int alTextureSizeLevel)
	{
		return CreateSimpleTexture(asName,abUseMipMaps,aUsage, eTextureType_1D,alTextureSizeLevel);
	}

	//-----------------------------------------------------------------------

	iTexture* cTextureManager::Create2D(const tString& asName,bool abUseMipMaps, eTextureType aType,
										eTextureUsage aUsage, unsigned int alTextureSizeLevel)
	{
		return CreateSimpleTexture(asName,abUseMipMaps,aUsage, aType,alTextureSizeLevel);
	}

	//-----------------------------------------------------------------------

	iTexture* cTextureManager::Create3D(const tString& asName,bool abUseMipMaps, eTextureUsage aUsage,
										unsigned int alTextureSizeLevel)
	{
		return CreateSimpleTexture(asName,abUseMipMaps,aUsage, eTextureType_3D,alTextureSizeLevel);
	}

	//-----------------------------------------------------------------------

	iTexture* cTextureManager::CreateAnim(const tString& asName,bool abUseMipMaps,eTextureType aType,
											eTextureUsage aUsage, unsigned int alTextureSizeLevel)
	{
		cRmEnv env(mpFileSearcher, this);
		cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
		cRmOut firstFrame;
		ohpl_rm_anim_first_frame(env.raw(), RM_STR(asName), exts.data(), exts.size(), firstFrame.raw());
		const tString& asFirstFrameName = firstFrame.msName;

		BeginLoad(asFirstFrameName);

		cRmOut plan;
		int lPlan = ohpl_rm_anim_plan(env.raw(), RM_STR(asFirstFrameName), plan.raw());

		//The error returns below leave the load tab open, as before.
		if(lPlan == -1)
		{
			Error("First frame of animation '%s' must contain '01'!\n", asFirstFrameName.c_str());
			return NULL;
		}
		if(lPlan == -2)
		{
			Error("First frame of animation '%s' must contain '01' before extension!\n", asFirstFrameName.c_str());
			return NULL;
		}
		if(lPlan == -3)
		{
			Error("First frame of animation '%s' could not be found!\n", asFirstFrameName.c_str());
			return NULL;
		}
		if(lPlan == -4)
		{
			Error("No textures found for animation %s\n",plan.msName.c_str());
			EndLoad();
			return NULL;
		}

		iTexture* pTexture = static_cast<iTexture*>(plan.Res());

		if(lPlan == 1)
		{
			const tString& sBaseName = plan.msName;
			const tWString& sFakeFullPath = plan.msPath;
			const std::vector<tWString>& vPaths = plan.mvItems;

			std::vector<cBitmap*> vBitmaps;
			for(size_t i =0; i< vPaths.size(); ++i)
			{
				cBitmap* pBmp = mpBitmapLoaderHandler->LoadBitmap(vPaths[i],0);
				if(pBmp==NULL){
					Error("Couldn't load bitmap '%s'!\n",cString::To8Char(vPaths[i]).c_str());
					for(int j=0;j<(int)vBitmaps.size();j++) hplDelete(vBitmaps[j]);
					EndLoad();
					return NULL;
				}
				vBitmaps.push_back(pBmp);
			}

			//Create the animated texture
			pTexture = mpGraphics->GetLowLevel()->CreateTexture(sBaseName, aType, aUsage);
			pTexture->SetsRGB(mbSRGB);
			pTexture->SetFullPath(sFakeFullPath);
			pTexture->SetSizeDownScaleLevel(alTextureSizeLevel);

			if(pTexture->CreateAnimFromBitmapVec(&vBitmaps)==false)
			{
				Error("Couldn't create animated texture '%s'!\n", sBaseName.c_str());
				hplDelete(pTexture);
				for(int j=0;j<(int)vBitmaps.size();j++) hplDelete(vBitmaps[j]);
				EndLoad();
				return NULL;
			}

			//Bitmaps no longer needed.
			for(int j=0;j<(int)vBitmaps.size();j++) hplDelete(vBitmaps[j]);

			mlMemoryUsage += pTexture->GetMemorySize();
			AddResource(pTexture);
			m_setAnimTextures.insert(pTexture);
		}

		if(pTexture)pTexture->IncUserCount();
		else Error("Couldn't texture '%s'\n",asFirstFrameName.c_str());

		EndLoad();
		return pTexture;
	}

	//-----------------------------------------------------------------------

	iTexture* cTextureManager::CreateCubeMap(const tString& asPathName,bool abUseMipMaps,
											eTextureUsage aUsage,
											unsigned int alTextureSizeLevel)
	{
		cRmEnv env(mpFileSearcher, this);
		cRmOut pre;

		/////////////////////////////////////////////////////////
		// Load Cubemap from single file
		if(ohpl_rm_cube_pre(env.raw(), RM_STR(asPathName), pre.raw()) == 0)
		{
			return CreateSimpleTexture(asPathName,abUseMipMaps,aUsage,eTextureType_CubeMap,alTextureSizeLevel);
		}
		/////////////////////////////////////////////////////////
		// Load Cubemap from multiple files
		else
		{
			const tString& sName = pre.msName;
			const tWString& sFakeFullPath = pre.msPath;
			iTexture* pTexture = static_cast<iTexture*>(pre.Res());

			BeginLoad(asPathName);

			if(pTexture==NULL)
			{
				//See if files for all faces exist
				cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
				cRmOut faces;
				int lFace = ohpl_rm_cube_faces(env.raw(), RM_STR(sName), exts.data(), exts.size(), faces.raw());
				if(lFace != 0)
				{
					int i = lFace-1;
					tString sNewName = sName + mvCubeSideSuffixes[i];
					//The load tab stays open, as before.
					Error("Couldn't find %d-face '%s', for cubemap '%s' in path: '%s'\n",i,sNewName.c_str(),sName.c_str(), asPathName.c_str());
					return NULL;
				}
				const std::vector<tWString>& vPaths = faces.mvItems;

				//Load bitmaps for all faces
				std::vector<cBitmap*> vBitmaps;
				for(int i=0;i<6; i++)
				{
					cBitmap* pBmp = mpBitmapLoaderHandler->LoadBitmap(vPaths[i],0);
					if(pBmp==NULL){
						Error("Couldn't load bitmap '%s'!\n",cString::To8Char(vPaths[i]).c_str());
						for(int j=0;j<(int)vBitmaps.size();j++) hplDelete(vBitmaps[j]);
						EndLoad();
						return NULL;
					}
					vBitmaps.push_back(pBmp);
				}

				//Create the cubemap
				pTexture = mpGraphics->GetLowLevel()->CreateTexture(sName,eTextureType_CubeMap, aUsage);
				pTexture->SetsRGB(mbSRGB);
				pTexture->SetFullPath(sFakeFullPath);
				pTexture->SetUseMipMaps(abUseMipMaps);
				pTexture->SetSizeDownScaleLevel(alTextureSizeLevel);

				if(pTexture->CreateCubeFromBitmapVec(&vBitmaps)==false)
				{
					Error("Couldn't create cubemap '%s'!\n", sName.c_str());
					hplDelete(pTexture);
					for(int j=0;j<(int)vBitmaps.size();j++) hplDelete(vBitmaps[j]);
					EndLoad();
					return NULL;
				}

				//Bitmaps no longer needed.
				for(int j=0;j<(int)vBitmaps.size();j++)	hplDelete(vBitmaps[j]);

				mlMemoryUsage += pTexture->GetMemorySize();
				AddResource(pTexture);
			}

			if(pTexture)pTexture->IncUserCount();
			else Error("Couldn't texture '%s'\n",sName.c_str());

			EndLoad();
			return pTexture;
		}
	}

	//-----------------------------------------------------------------------

	void cTextureManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cTextureManager::Destroy(iResourceBase* apResource)
	{
		if(ohpl_rm_destroy_standard(apResource->GetMeta()))
		{
			mlMemoryUsage -= static_cast<iTexture*>(apResource)->GetMemorySize();
			m_setAnimTextures.erase(static_cast<iTexture*>(apResource));

			RemoveResource(apResource);
			hplDelete(apResource);
		}
	}

	//-----------------------------------------------------------------------

	void cTextureManager::Update(float afTimeStep)
	{
		for(iTexture *pTexture : m_setAnimTextures)
			pTexture->Update(afTimeStep);
	}

	//-----------------------------------------------------------------------

	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iTexture* cTextureManager::CreateSimpleTexture(	const tString& asName,bool abUseMipMaps, 
													eTextureUsage aUsage, eTextureType aType,
													unsigned int alTextureSizeLevel)
	{
		tWString sPath;
		iTexture* pTexture;
		
		BeginLoad(asName);

		pTexture = FindTexture2D(asName,sPath);

		if(pTexture==NULL && sPath!=_W(""))
		{
			//Load the bitmap
			cBitmap *pBmp;
			pBmp = mpBitmapLoaderHandler->LoadBitmap(sPath,0);
			if(pBmp==NULL)
			{
				Error("Texture manager Couldn't load bitmap '%s'\n", cString::To8Char(sPath).c_str());
				EndLoad();
				return NULL;
			}

			//Create the texture and load from bitmap
			pTexture = mpGraphics->GetLowLevel()->CreateTexture(asName,aType,aUsage);
			pTexture->SetsRGB(mbSRGB);
			pTexture->SetFullPath(sPath);
			
			pTexture->SetUseMipMaps(abUseMipMaps);
			pTexture->SetSizeDownScaleLevel(alTextureSizeLevel);
			
			if(pTexture->CreateFromBitmap(pBmp)==false)
			{
				hplDelete(pTexture);
				hplDelete(pBmp);
				EndLoad();
				return NULL;
			}

			//Bitmap is no longer needed so delete it.
			hplDelete(pBmp);
			
			mlMemoryUsage += pTexture->GetMemorySize();
			AddResource(pTexture);
		}

		if(pTexture)pTexture->IncUserCount();
		else Error("Couldn't texture '%s'\n",asName.c_str());
		
		EndLoad();
		return pTexture;
	}

	//-----------------------------------------------------------------------
	
	iTexture* cTextureManager::FindTexture2D(const tString &asName, tWString &asFilePath)
	{
		cRmEnv env(mpFileSearcher, this);
		cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
		cRmOut out;
		ohpl_rm_find_with_formats(env.raw(), RM_STR(asName), exts.data(), exts.size(), out.raw());
		asFilePath = out.msPath;
		return static_cast<iTexture*>(out.Res());
	}

}
