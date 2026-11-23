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

#include "resources/ImageManager.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/Texture.h"
#include "graphics/Bitmap.h"


#include "graphics/FrameSubImage.h"
#include "graphics/FrameTexture.h"
#include "resources/LowLevelResources.h"
#include "resources/Resources.h"
#include "resources/BitmapLoaderHandler.h"
#include "resources/FileSearcher.h"
#include "resources/RustResMgr.h"


namespace hpl {

	cVector2l cImageManager::mvDefaultFrameSize(512,512);

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cImageManager::cImageManager(	cResources *mpResources, iLowLevelGraphics *apLowLevelGraphics,
									iLowLevelSystem *apLowLevelSystem)
	: iResourceManager(mpResources->GetFileSearcher(), mpResources->GetLowLevel(),apLowLevelSystem)
	{
       mpLowLevelGraphics = apLowLevelGraphics;

	   mpBitmapLoaderHandler = mpResources->GetBitmapLoaderHandler();

	   mvFrameSize = mvDefaultFrameSize;
	   InitFrameHost();
	   ohpl_h3f_image_init(mpRs, &mOps, &mFrHost, mvFrameSize.x, mvFrameSize.y);
	}

	cImageManager::~cImageManager()
	{
		ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_IMAGE);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iResourceBase* cImageManager::CreateInFrame(const tString& asName, int alFrameHandle)
	{
		cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
		return static_cast<iResourceBase*>(ohpl_h3f_image_create_in_frame(mpRs, H3Ops(), RM_STR(asName), alFrameHandle, exts.h3(), exts.size()));
	}
	
	//-----------------------------------------------------------------------

	cFrameSubImage* cImageManager::CreateImage(const tString& asName, int alFrameHandle)
	{
		iResourceBase* pRes = CreateInFrame(asName, alFrameHandle);
		cFrameSubImage* pImage = static_cast<cFrameSubImage*>(pRes);

		return pImage;
	}

	//-----------------------------------------------------------------------

	cFrameSubImage* cImageManager::CreateFromBitmap(const tString &asName,cBitmap* apBmp, int alFrameHandle)
	{
		return H3Res<cFrameSubImage>(ohpl_h3f_image_from_bitmap(mpRs, H3Ops(), apBmp, alFrameHandle));
	}

	//-----------------------------------------------------------------------

	cFrameTexture* cImageManager::CreateCustomFrame(iTexture *apTexture)
	{
		return static_cast<cFrameTexture*>(ohpl_h3f_image_custom_frame(mpRs, H3Ops(), apTexture));
	}
	//-----------------------------------------------------------------------
	
	void cImageManager::Unload(iResourceBase* apResource)
	{

	}

	void cImageManager::Destroy(iResourceBase* apResource)
	{
		ohpl_h3f_image_destroy(mpRs, H3Ops(), apResource);
	}

	//-----------------------------------------------------------------------

	void cImageManager::ReorganizeAll()
	{
		ohpl_h3f_image_reorganize_all(mpRs, H3Ops());
	}
	
	//-----------------------------------------------------------------------
	
	int cImageManager::FlushAll()
	{
		return ohpl_h3f_image_flush_all(mpRs, H3Ops());
	}

	//-----------------------------------------------------------------------

	cFrameTexture* cImageManager::GetFrameTexture(int alHandle)
	{
		return static_cast<cFrameTexture*>(ohpl_h3f_image_frame_texture(mpRs, H3Ops(), alHandle));
	}

	//-----------------------------------------------------------------------

	int cImageManager::CreateFrame(cVector2l avSize)
	{
		return ohpl_h3f_image_create_frame(mpRs, H3Ops(), avSize.x, avSize.y);
	}
	
	//-----------------------------------------------------------------------

	void cImageManager::SetFrameLocked(int alHandle, bool abLocked)
	{
		ohpl_h3f_image_set_frame_locked(mpRs, H3Ops(), alHandle, abLocked);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------
	


	//-----------------------------------------------------------------------

	void cImageManager::InitFrameHost()
	{
		memset(&mFrHost, 0, sizeof(mFrHost));
		mFrHost.ctx = this;
		mFrHost.log = [](void*, int alLevel, const uint8_t *apMsg, size_t alLen)
		{
			tString sMsg(reinterpret_cast<const char*>(apMsg), alLen);
			if(alLevel == 2) Error("%s", sMsg.c_str());
			else if(alLevel == 1) Warning("%s", sMsg.c_str());
			else Log("%s", sMsg.c_str());
		};
		mFrHost.texNew = [](void *c) -> void*
		{
			return static_cast<cImageManager*>(c)->mpLowLevelGraphics->CreateTexture("ImageFrame",eTextureType_2D,eTextureUsage_Normal);
		};
		mFrHost.texDelete = [](void*, void *apTex){ hplDelete(static_cast<iTexture*>(apTex)); };
		mFrHost.texUpload = [](void*, void *apTex, void *apBmp)
		{
			iTexture *pTex = static_cast<iTexture*>(apTex);
			pTex->CreateFromBitmap(static_cast<cBitmap*>(apBmp));
			pTex->SetWrapS(eTextureWrap_ClampToEdge);
			pTex->SetWrapT(eTextureWrap_ClampToEdge);
		};
		mFrHost.texSize = [](void*, void *apTex, cVector3l *apSize){ *apSize = static_cast<iTexture*>(apTex)->GetSize(); };
		mFrHost.bmpNew = [](void*, int alW, int alH) -> void*
		{
			cBitmap *pBmp = hplNew(cBitmap, () );
			pBmp->CreateData(cVector3l(alW, alH, 1),ePixelFormat_RGBA,0,0);
			return pBmp;
		};
		mFrHost.bmpDelete = [](void*, void *apBmp){ hplDelete(static_cast<cBitmap*>(apBmp)); };
		mFrHost.bmpInfo = [](void *c, void *apBmp, OhplFrBmpInfo *apInfo)
		{
			cImageManager *pThis = static_cast<cImageManager*>(c);
			cBitmap *pBmp = static_cast<cBitmap*>(apBmp);
			apInfo->size = pBmp->GetSize();
			apInfo->format = pBmp->GetPixelFormat();
			apInfo->bytesPerPixel = pBmp->GetBytesPerPixel();
			cBitmapData *pData = pBmp->GetData(0,0);
			apInfo->data = pData ? pData->Data() : NULL;
			apInfo->len = (pData && pData->Data()) ? (size_t)pData->Size() : 0;
			apInfo->compressed = pBmp->IsCompressed();
			pThis->mFrScratch = cString::To8Char(pBmp->GetFileName());
			apInfo->name = OhplH3Str{reinterpret_cast<const uint8_t*>(pThis->mFrScratch.data()), pThis->mFrScratch.size()};
		};
		mFrHost.bmpClear = [](void*, void *apBmp){ static_cast<cBitmap*>(apBmp)->Clear(cColor(1,0,1,0),0,0); };
		mFrHost.subNew = [](void*, OhplH3Str aName, OhplH3WStr aPath, void *apRs, bool abIncUser) -> void*
		{
			cFrameSubImage *pImage = hplNew( cFrameSubImage, (H3Name(aName), H3Wide(aPath), static_cast<OhplFrSub*>(apRs)) );
			if(abIncUser) pImage->IncUserCount();
			return static_cast<iResourceBase*>(pImage);
		};
		mFrHost.subRs = [](void*, void *apShell) -> void*
		{
			return static_cast<cFrameSubImage*>(static_cast<iResourceBase*>(apShell))->Rs();
		};
	}

	cFrameSubImage::~cFrameSubImage()
	{
		ohpl_fr_sub_free(mpRs, GetUserCount());
	}

	//-----------------------------------------------------------------------

	void* cImageManager::H3Create(OhplH3Create &aCreate)
	{
		return mpBitmapLoaderHandler->LoadBitmap(H3Wide(aCreate.path), eBitmapLoadFlag_ForceNoCompression);
	}
}
