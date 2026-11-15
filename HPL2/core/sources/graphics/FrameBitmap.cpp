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

#include "graphics/FrameBitmap.h"
#include "graphics/RustGraphics.h"
#include "graphics/RustGraphicsH5.h"

#include "math/Math.h"

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"

#include "graphics/FrameTexture.h"
#include "graphics/FrameSubImage.h"
#include "graphics/Bitmap.h"
#include "graphics/Texture.h"

#include <memory>


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// FRAME BITMAP IMAGE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cFBitmapImage::~cFBitmapImage()
	{
		if(mpSubImage) mpSubImage->mpFrameBitmapImage = NULL;
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cFrameBitmap::cFrameBitmap(cBitmap *apBitmap,  cFrameTexture *apFrmTex, int alHandle) : iFrameBase()
	{
		mpBitmap = apBitmap;
		mpFrameTexture = apFrmTex;
		//TODO: Make a filling method
		mpBitmap->Clear(cColor(1,0,1,0),0,0);
		mlHandle = alHandle;
		mlPicCount =0;

		//Root node in rect tree
		mpAtlas = ohpl_h5_atlas_new(mpBitmap->GetWidth(), mpBitmap->GetHeight(), alHandle);
	}

	cFrameBitmap::~cFrameBitmap()
	{
		ohpl_h5_atlas_free(mpAtlas);
		STLDeleteAll(mlstImages);
		hplDelete(mpBitmap);
		mpBitmap = NULL;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static void BitmapPix(cBitmap *apBitmap, OhplH5Surface &aSurface, unsigned char *&apData, size_t &alLen, bool abDest)
	{
		aSurface.size = apBitmap->GetSize();
		aSurface.format = apBitmap->GetPixelFormat();
		aSurface.bytes_per_pixel = apBitmap->GetBytesPerPixel();
		cBitmapData *pData = apBitmap->GetData(0,0);
		apData = (pData && !(abDest && apBitmap->IsCompressed())) ? pData->mpData : NULL;
		alLen = (pData && apData) ? (size_t)pData->mlSize : 0;
	}

	cFrameSubImage *cFrameBitmap::AddBitmap(cBitmap *apSrc, const tWString& asFullPath, cFrameSubImage *apSubImageCreated, bool *apFoundNode)
	{
		OhplH5Surface dst, src;
		unsigned char *pDst, *pSrc;
		size_t lDstLen, lSrcLen;
		BitmapPix(mpBitmap, dst, pDst, lDstLen, true);
		BitmapPix(apSrc, src, pSrc, lSrcLen, false);

		OhplH5Added added;
		bool bFoundNode = ohpl_h5_atlas_add(mpAtlas, &dst, pDst, lDstLen, &src, pSrc, lSrcLen, apSubImageCreated!=NULL, &added);
		cFrameSubImage *pImage = NULL;

		if(bFoundNode)
		{
			cFBitmapImage *pBitmapImage = hplNew( cFBitmapImage, () );
			if(apSubImageCreated){
				pBitmapImage->mpSubImage = apSubImageCreated;
				apSubImageCreated->mpFrameBitmapImage = pBitmapImage;
			}
			pBitmapImage->mRect = added.rect;
			mlstImages.push_back(pBitmapImage);

			mlPicCount = ohpl_h5_atlas_pic_count(mpAtlas);
			mpFrameTexture->SetPicCount(mlPicCount);

			if(apSubImageCreated == NULL)
			{
				pImage = hplNew( cFrameSubImage, (cString::To8Char(apSrc->GetFileName()),asFullPath,
								mpFrameTexture, this,
								added.rect,
								cVector2l(mpBitmap->GetWidth(),mpBitmap->GetHeight()),
								ohpl_h5_atlas_handle(mpAtlas), pBitmapImage) );
				pBitmapImage->mpSubImage = pImage;
			}
		}

		if(apFoundNode) *apFoundNode = bFoundNode;
		return pImage;
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	bool cFrameBitmap::IsUpdated()
	{
		return ohpl_h5_atlas_updated(mpAtlas);
	}
    
	//-----------------------------------------------------------------------

	bool cFrameBitmap::IsFull()
	{
		return ohpl_h5_atlas_full(mpAtlas);
	}

	//-----------------------------------------------------------------------

	bool cFrameBitmap::IsLocked()const
	{
		return ohpl_h5_atlas_locked(mpAtlas);
	}

	void cFrameBitmap::SetLocked(bool abX)
	{
		ohpl_h5_atlas_set_locked(mpAtlas, abX);
	}

	int cFrameBitmap::GetAdditionsSinceReorganization()
	{
		return ohpl_h5_atlas_additions(mpAtlas);
	}

	void cFrameBitmap::SetNeedReorganisation()
	{
		ohpl_h5_atlas_set_need_reorg(mpAtlas, true);
	}

	//-----------------------------------------------------------------------

	void cFrameBitmap::UpdateBeforeDraw()
	{
		if(ohpl_h5_atlas_take_need_reorg(mpAtlas)) Reorganize();
		FlushToTexture();
	}

	//-----------------------------------------------------------------------

	void cFrameBitmap::Reorganize()
	{
		std::vector<cFrameSubImage*> vSubs;
		std::vector<cRect2l> vRects;
		for(cFBitmapImageListIt it = mlstImages.begin(); it != mlstImages.end(); ++it)
		{
			if((*it)->mpSubImage==NULL) continue;
			vSubs.push_back((*it)->mpSubImage);
			vRects.push_back((*it)->mRect);
		}
		size_t lNum = vSubs.size();

		OhplH5Surface dst;
		unsigned char *pDst;
		size_t lDstLen;
		BitmapPix(mpBitmap, dst, pDst, lDstLen, true);

		std::unique_ptr<bool[]> vFit(new bool[lNum+1]);
		std::vector<cRect2l> vOut(lNum+1);
		std::vector<size_t> vOrder(lNum+1);
		size_t lCount = 0;
		ohpl_h5_atlas_reorganize(mpAtlas, &dst, pDst, lDstLen, vRects.data(), lNum, vOut.data(), vFit.get(), vOrder.data(), &lCount);

		//Deleting the old images unlinks their subimages
		STLDeleteAll(mlstImages);
		mlstImages.clear();

		for(size_t i=0; i<lNum; ++i)
		{
			if(vFit[i]==false)
				Error("When reorganizing images according to previous order an image still does not fit!!!\n");
		}

		for(size_t i=0; i<lCount; ++i)
		{
			size_t lIdx = vOrder[i];
			cFBitmapImage *pImage = hplNew( cFBitmapImage, () );
			pImage->mRect = vOut[lIdx];
			pImage->mpSubImage = vSubs[lIdx];
			vSubs[lIdx]->mpFrameBitmapImage = pImage;
			mlstImages.push_back(pImage);
		}

		mlPicCount = ohpl_h5_atlas_pic_count(mpAtlas);
		if(lCount > 0) mpFrameTexture->SetPicCount(mlPicCount);

		for(cFBitmapImageListIt it = mlstImages.begin(); it != mlstImages.end(); ++it)
			(*it)->mpSubImage->SetNeedUpdateUvs();
	}

	//-----------------------------------------------------------------------

	bool cFrameBitmap::FlushToTexture()
	{
		if(ohpl_h5_atlas_take_updated(mpAtlas))
		{
			mpFrameTexture->GetTexture()->CreateFromBitmap(mpBitmap);
			mpFrameTexture->GetTexture()->SetWrapS(eTextureWrap_ClampToEdge);
			mpFrameTexture->GetTexture()->SetWrapT(eTextureWrap_ClampToEdge);

			//mpFrameTexture->SetPicCount(mlPicCount);
			return true;
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}
