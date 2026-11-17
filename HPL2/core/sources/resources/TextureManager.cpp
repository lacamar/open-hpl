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

		
	}

	cTextureManager::~cTextureManager()
	{
		STLMapDeleteAll(m_mapAttenuationTextures);
		ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_TEXTURE);
		return;
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
		cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
		return H3Res<iTexture>(ohpl_h3f_texture_anim(mpRs, H3Ops(), RM_STR(asName), abUseMipMaps, (int32_t)aType, (int32_t)aUsage, (int32_t)alTextureSizeLevel, exts.h3(), exts.size()));
	}

	//-----------------------------------------------------------------------

	iTexture* cTextureManager::CreateCubeMap(const tString& asPathName,bool abUseMipMaps,
											eTextureUsage aUsage,
											unsigned int alTextureSizeLevel)
	{
		cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
		return H3Res<iTexture>(ohpl_h3f_texture_cube(mpRs, H3Ops(), RM_STR(asPathName), abUseMipMaps, (int32_t)aUsage, (int32_t)alTextureSizeLevel, (int32_t)eTextureType_CubeMap, exts.h3(), exts.size()));
	}

	//-----------------------------------------------------------------------

	void cTextureManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cTextureManager::Destroy(iResourceBase* apResource)
	{
		ohpl_h3f_texture_destroy(mpRs, H3Ops(), apResource);
	}

	//-----------------------------------------------------------------------

	void cTextureManager::Update(float afTimeStep)
	{
		ohpl_h3f_texture_update(mpRs, H3Ops(), H3Bits(afTimeStep));
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
		cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
		return H3Res<iTexture>(ohpl_h3f_texture_simple(mpRs, H3Ops(), RM_STR(asName), abUseMipMaps, (int32_t)aUsage, (int32_t)aType, (int32_t)alTextureSizeLevel, exts.h3(), exts.size()));
	}

	//-----------------------------------------------------------------------
	


	void* cTextureManager::H3Create(OhplH3Create &aCreate)
	{
		std::vector<cBitmap*> vBitmaps;
		auto DeleteBitmaps = [&](){ for(cBitmap *pBmp : vBitmaps) hplDelete(pBmp); };
		size_t lBitmaps = aCreate.kind == OHPL_H3_TEX_SIMPLE ? 1 : aCreate.npaths;
		for(size_t i=0; i<lBitmaps; ++i)
		{
			cBitmap *pBmp = mpBitmapLoaderHandler->LoadBitmap(aCreate.kind == OHPL_H3_TEX_SIMPLE ? H3Wide(aCreate.path) : H3Wide(aCreate.paths[i]), 0);
			if(pBmp==NULL)
			{
				DeleteBitmaps();
				aCreate.status = 1;
				aCreate.idx = (int32_t)i;
				return NULL;
			}
			vBitmaps.push_back(pBmp);
		}
		iTexture *pTexture = mpGraphics->GetLowLevel()->CreateTexture(H3Name(aCreate.name), (eTextureType)aCreate.i[2], (eTextureUsage)aCreate.i[1]);
		pTexture->SetsRGB(mbSRGB);
		pTexture->SetFullPath(H3Wide(aCreate.path));
		if(aCreate.kind != OHPL_H3_TEX_ANIM) pTexture->SetUseMipMaps(aCreate.i[0] != 0);
		pTexture->SetSizeDownScaleLevel(aCreate.i[3]);
		bool bOk;
		if(aCreate.kind == OHPL_H3_TEX_SIMPLE) bOk = pTexture->CreateFromBitmap(vBitmaps[0]);
		else if(aCreate.kind == OHPL_H3_TEX_ANIM) bOk = pTexture->CreateAnimFromBitmapVec(&vBitmaps);
		else bOk = pTexture->CreateCubeFromBitmapVec(&vBitmaps);
		DeleteBitmaps();
		if(bOk==false)
		{
			hplDelete(pTexture);
			aCreate.status = 2;
			return NULL;
		}
		return static_cast<iResourceBase*>(pTexture);
	}

	int64_t cTextureManager::H3Call(int alWhat, void *apObj, int64_t alArg)
	{
		iTexture *pTexture = H3Res<iTexture>(apObj);
		if(alWhat == OHPL_H3C_MEM_SIZE) return pTexture->GetMemorySize();
		if(alWhat == OHPL_H3C_UPDATE) pTexture->Update(H3Float(alArg));
		return 0;
	}
}
