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
#include "graphics/Bitmap.h"


#include "graphics/FrameSubImage.h"
#include "graphics/FrameBitmap.h"
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
	   ohpl_h3f_image_init(mpRs, &mOps, mvFrameSize.x, mvFrameSize.y);
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

	void* cImageManager::H3Create(OhplH3Create &aCreate)
	{
		switch(aCreate.kind)
		{
		case OHPL_H3_IMAGE:
			return mpBitmapLoaderHandler->LoadBitmap(H3Wide(aCreate.path), eBitmapLoadFlag_ForceNoCompression);
		case OHPL_H3_IMG_BITMAP_FRAME:
		{
			iTexture *pTex = mpLowLevelGraphics->CreateTexture("ImageFrame",eTextureType_2D,eTextureUsage_Normal);
			cFrameTexture *pTFrame = hplNew( cFrameTexture, (pTex,aCreate.i[2],this,false) );
			cBitmap *pBmp = hplNew(cBitmap, () );
			pBmp->CreateData(cVector3l(aCreate.i[0], aCreate.i[1],1),ePixelFormat_RGBA,0,0);
			cFrameBitmap *pBFrame = hplNew(  cFrameBitmap, (pBmp,pTFrame,aCreate.i[2]) );
			return pBFrame;
		}
		case OHPL_H3_IMG_CUSTOM_FRAME:
			return hplNew( cFrameTexture, (static_cast<iTexture*>(aCreate.ptr),aCreate.i[0],this,true) );
		case OHPL_H3_IMG_ADD_BITMAP:
			return static_cast<iResourceBase*>(static_cast<cFrameBitmap*>(aCreate.ptr)->AddBitmap(static_cast<cBitmap*>(aCreate.ptr2), H3Wide(aCreate.path), NULL));
		}
		return NULL;
	}

	int64_t cImageManager::H3Call(int alWhat, void *apObj, int64_t alArg)
	{
		cFrameBitmap *pBmpFrame = static_cast<cFrameBitmap*>(apObj);
		cFrameTexture *pFrame = static_cast<cFrameTexture*>(apObj);
		switch(alWhat)
		{
		case OHPL_H3C_IMG_FRAME_TEX: return (int64_t)(intptr_t)H3Res<cFrameSubImage>(apObj)->GetFrameTexture();
		case OHPL_H3C_IMG_FRAME_BMP: return (int64_t)(intptr_t)H3Res<cFrameSubImage>(apObj)->GetFrameBitmap();
		case OHPL_H3C_FRAME_DEC_PIC: pFrame->DecPicCount(); return 0;
		case OHPL_H3C_BMPFRAME_DEC_PIC: pBmpFrame->DecPicCount(); return 0;
		case OHPL_H3C_FRAME_IS_EMPTY: return pFrame->IsEmpty();
		case OHPL_H3C_FRAME_IS_CUSTOM: return pFrame->IsCustom();
		case OHPL_H3C_BMPFRAME_HANDLE: return pBmpFrame->GetHandle();
		case OHPL_H3C_BMPFRAME_TEX: return (int64_t)(intptr_t)pBmpFrame->GetFrameTexture();
		case OHPL_H3C_BMPFRAME_IS_FULL: return pBmpFrame->IsFull();
		case OHPL_H3C_BMPFRAME_IS_LOCKED: return pBmpFrame->IsLocked();
		case OHPL_H3C_BMPFRAME_ADDITIONS: return pBmpFrame->GetAdditionsSinceReorganization();
		case OHPL_H3C_BMPFRAME_REORGANIZE: pBmpFrame->Reorganize(); return 0;
		case OHPL_H3C_BMPFRAME_FLUSH: return pBmpFrame->FlushToTexture();
		case OHPL_H3C_BMPFRAME_SET_LOCKED: pBmpFrame->SetLocked(alArg != 0); return 0;
		case OHPL_H3C_BMPFRAME_DELETE: hplDelete(pBmpFrame); return 0;
		case OHPL_H3C_FRAME_DELETE: hplDelete(pFrame); return 0;
		case OHPL_H3C_FRAME_HANDLE: return pFrame->GetHandle();
		case OHPL_H3C_BMP_WIDTH: return static_cast<cBitmap*>(apObj)->GetWidth();
		case OHPL_H3C_BMP_HEIGHT: return static_cast<cBitmap*>(apObj)->GetHeight();
		case OHPL_H3C_BMP_DELETE: hplDelete(static_cast<cBitmap*>(apObj)); return 0;
		}
		return 0;
	}
}
