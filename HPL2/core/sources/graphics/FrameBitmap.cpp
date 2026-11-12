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
#include "impl/RustBackends.h"

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
		mlMinHole = 6;
		mlHandle = alHandle;
		mbIsFull = false;
		mbIsLocked = false;
		mlPicCount =0;
		mlAdditionsSinceReorganization =0;

		mbNeedNeedReorganisation = false;

		//Root node in rect tree
		mpAtlas = NULL;
		mpPacker = NULL;
		if(OhplBackendRustRequested())
			mpAtlas = ohpl_h5_atlas_new(mpBitmap->GetWidth(), mpBitmap->GetHeight(), alHandle);
		else
			mpPacker = ohpl_packer_new(mpBitmap->GetWidth(), mpBitmap->GetHeight());
	}

	cFrameBitmap::~cFrameBitmap()
	{
		ohpl_packer_free(mpPacker);
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

	cFrameSubImage *cFrameBitmap::AddBitmapRust(cBitmap *apSrc, const tWString& asFullPath, cFrameSubImage *apSubImageCreated, bool *apFoundNode)
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

	cFrameSubImage *cFrameBitmap::AddBitmap(cBitmap *apSrc, const tWString& asFullPath, cFrameSubImage *apSubImageCreated, bool *apFoundNode)
	{
		if(mpAtlas) return AddBitmapRust(apSrc, asFullPath, apSubImageCreated, apFoundNode);

		cFBitmapImage *pBitmapImage=NULL;
		cFrameSubImage *pImage=NULL;
		//source size
		//+2 because we are gonna have a border to get rid if some antialiasing problems
		int lSW = apSrc->GetWidth()+2; 
		int lSH = apSrc->GetHeight()+2;

		cVector2l vPos;
		cRect2l NewRect;
		bool bFoundEmptyNode = false;
		bool bFoundNode = ohpl_packer_insert(mpPacker, apSrc->GetWidth(), apSrc->GetHeight(), &NewRect, &bFoundEmptyNode);

		if(bFoundNode)
		{
			vPos = cVector2l(NewRect.x+1,NewRect.y+1);//+1 for the right pos

			//Draw corners for border
			mpBitmap->Blit(	apSrc,cVector3l(NewRect.x, NewRect.y, 0),
							cVector3l(1,1,1),0);
			mpBitmap->Blit(	apSrc,cVector3l(NewRect.x + apSrc->GetWidth()+1, NewRect.y,0),
							cVector3l(1,1,1), cVector3l(apSrc->GetWidth()-1,0,0) );
			mpBitmap->Blit(	apSrc,cVector3l(NewRect.x + apSrc->GetWidth()+1, NewRect.y + apSrc->GetHeight()+1,0),
							cVector3l(1,1,1), cVector3l(apSrc->GetWidth()-1,apSrc->GetHeight()-1,0) );
			mpBitmap->Blit(	apSrc,cVector3l(NewRect.x, NewRect.y + apSrc->GetHeight()+1,0),
							cVector3l(1,1,1), cVector3l(0,apSrc->GetHeight()-1,0) );
			
			//Draw sides for border
			mpBitmap->Blit(	apSrc,cVector3l(NewRect.x+1, NewRect.y, 0),
							cVector3l(apSrc->GetWidth(),1,1),0);
			mpBitmap->Blit(	apSrc,cVector3l(NewRect.x+1, NewRect.y+apSrc->GetHeight()+1, 0),
							cVector3l(apSrc->GetWidth(),1,1), cVector3l(0,apSrc->GetHeight()-1,0));
			mpBitmap->Blit(	apSrc,cVector3l(NewRect.x, NewRect.y+1, 0),
							cVector3l(1,apSrc->GetHeight(),1),0);
			mpBitmap->Blit(	apSrc,cVector3l(NewRect.x+apSrc->GetWidth()+1, NewRect.y+1, 0),
							cVector3l(1,apSrc->GetHeight(),1),cVector3l(apSrc->GetWidth()-1,0,0));

			//Draw the final
			mpBitmap->Blit(apSrc,cVector3l(NewRect.x+1,NewRect.y+1,0), apSrc->GetSize(),0);

			//Add image data
			pBitmapImage = hplNew( cFBitmapImage, () );
			//Connect the subimage with the frame image.
			if(apSubImageCreated){
				pBitmapImage->mpSubImage = apSubImageCreated;
				apSubImageCreated->mpFrameBitmapImage = pBitmapImage;
			}
			pBitmapImage->mRect.x = NewRect.x+1; pBitmapImage->mRect.y = NewRect.y+1;
			pBitmapImage->mRect.w = apSrc->GetWidth(); pBitmapImage->mRect.h = apSrc->GetHeight();
			mlstImages.push_back(pBitmapImage);

			mlAdditionsSinceReorganization++;
			mlPicCount++;
			mpFrameTexture->SetPicCount(mlPicCount);
		}

		if(bFoundNode && apSubImageCreated == NULL)
		{
			//Create the image resource
			pImage = hplNew( cFrameSubImage, (cString::To8Char(apSrc->GetFileName()),asFullPath, 
							mpFrameTexture, this,
							cRect2l(vPos,cVector2l(lSW-2,lSH-2)),//-2 to get the correct size.
							cVector2l(mpBitmap->GetWidth(),mpBitmap->GetHeight()),
							mlHandle, pBitmapImage) );
			
			pBitmapImage->mpSubImage = pImage;

			if(!bFoundEmptyNode)
			{
				mbIsFull = true;
			}

			mbIsUpdated = true;
		}

		if(apFoundNode) *apFoundNode = bFoundNode;

		return pImage;
	}

	//-----------------------------------------------------------------------

	bool cFrameBitmap::MinimumFit(cRect2l aSrc,cRect2l aDest)
	{
		if(mpAtlas) return ohpl_h5_atlas_minimum_fit(&aSrc, &aDest);
		if(aDest.w-aSrc.w<mlMinHole && aDest.h-aSrc.h<mlMinHole) return true;

		return false;
	}

	//-----------------------------------------------------------------------

	bool cFrameBitmap::IsUpdated()
	{
		if(mpAtlas) return ohpl_h5_atlas_updated(mpAtlas);
		return mbIsUpdated;
	}
    
	//-----------------------------------------------------------------------

	bool cFrameBitmap::IsFull()
	{
		if(mpAtlas) return ohpl_h5_atlas_full(mpAtlas);
		return mbIsFull;
	}

	//-----------------------------------------------------------------------

	bool cFrameBitmap::IsLocked()const
	{
		return mpAtlas ? ohpl_h5_atlas_locked(mpAtlas) : mbIsLocked;
	}

	void cFrameBitmap::SetLocked(bool abX)
	{
		if(mpAtlas) ohpl_h5_atlas_set_locked(mpAtlas, abX);
		else mbIsLocked = abX;
	}

	int cFrameBitmap::GetAdditionsSinceReorganization()
	{
		return mpAtlas ? ohpl_h5_atlas_additions(mpAtlas) : mlAdditionsSinceReorganization;
	}

	void cFrameBitmap::SetNeedReorganisation()
	{
		if(mpAtlas){ ohpl_h5_atlas_set_need_reorg(mpAtlas, true); return; }
		mbNeedNeedReorganisation = true;
	}

	//-----------------------------------------------------------------------

	void cFrameBitmap::UpdateBeforeDraw()
	{
		if(mpAtlas)
		{
			if(ohpl_h5_atlas_take_need_reorg(mpAtlas)) Reorganize();
			FlushToTexture();
			return;
		}
		if(mbNeedNeedReorganisation)
		{
			Reorganize();
			mbNeedNeedReorganisation = false;
		}

		FlushToTexture();
	}

	//-----------------------------------------------------------------------

	class cBitmapSubImageComb
	{
	public: 
		cBitmapSubImageComb(cBitmap *apBitmap, cFrameSubImage *apSubImage){
			mpBitmap = apBitmap;
			mpSubImage = apSubImage;
		}
		~cBitmapSubImageComb(){
			hplDelete( mpBitmap );
		}

		cBitmap *mpBitmap;
		cFrameSubImage *mpSubImage;
	};

	typedef std::list<cBitmapSubImageComb*> tFrameBitmapCombList;
	typedef tFrameBitmapCombList::iterator tFrameBitmapCombListIt;

	//-----------------------------------------------------------------------


	void cFrameBitmap::ReorganizeRust()
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

	void cFrameBitmap::Reorganize()
	{
		if(mpAtlas){ ReorganizeRust(); return; }

		tFrameBitmapCombList lstBitmaps;

		bool mbLogTime = false;

		///////////////////////////////////////
		//Iterate nodes and retrive all bitmaps
		unsigned long lStartTime = cPlatform::GetApplicationTime();
		
		cFBitmapImageListIt imageIt = mlstImages.begin();
		for(; imageIt != mlstImages.end(); ++imageIt)
		{
			cFBitmapImage* pImage = *imageIt;
			if(pImage->mpSubImage==NULL) continue;
			
			//Log("Node: %dx%d (%d, %d)\n",pRect->mRect.w,pRect->mRect.h,pRect->mRect.x,pRect->mRect.y);

			//Create bitmap according to node rect
            cBitmap *pBitmap = hplNew( cBitmap, ());
			//-2 since we do not want borders
			pBitmap->CreateData(cVector3l(pImage->mRect.w,pImage->mRect.h,1),ePixelFormat_RGBA,0,0);

			//Copy from from bitmap to temp bitmap. +1 because we do not want borders.
			pBitmap->Blit(mpBitmap, 0, pBitmap->GetSize(),cVector3l(pImage->mRect.x, pImage->mRect.y,0));

			//Add bitmap to list
			cBitmapSubImageComb *pBmpComb = hplNew( cBitmapSubImageComb,(pBitmap,pImage->mpSubImage) );
			lstBitmaps.push_back(pBmpComb);
		}
		if(mbLogTime)Log(" getting image data took: %dms\n",cPlatform::GetApplicationTime()-lStartTime);
        

		///////////////////////////////////////
		//Clear Frame data
		lStartTime = cPlatform::GetApplicationTime();

		ClearAddedImages();//Clears the tree and image list.

		if(mbLogTime)Log(" clearing data took: %dms\n",cPlatform::GetApplicationTime()-lStartTime);


		lStartTime = cPlatform::GetApplicationTime();

		///////////////////////////////////////
		//Add all bitmaps again
		bool bAllNodesFit = false;
		std::vector<cBitmapSubImageComb*> vComb(lstBitmaps.begin(), lstBitmaps.end());
		std::vector<int> vSizes;
		for(size_t i=0; i<vComb.size(); ++i)
		{
			vSizes.push_back(vComb[i]->mpBitmap->GetSize().x);
			vSizes.push_back(vComb[i]->mpBitmap->GetSize().y);
		}
		std::vector<size_t> vOrder(vComb.size());
		ohpl_packer_reorganize_order(vSizes.data(), vComb.size(), vOrder.data());
		for(size_t i=0; i<vOrder.size(); ++i)
		{
			cBitmapSubImageComb *pComb = vComb[vOrder[i]];
			
			AddBitmap(pComb->mpBitmap, pComb->mpSubImage->GetFullPath(), pComb->mpSubImage, &bAllNodesFit);
			if(bAllNodesFit==false) break;
		}
		
		///////////////////////////////////////
		// Check if all could be added
		// If no room for bitmap was found, need to add again, according to previous order
		if(bAllNodesFit==false)
		{
			ClearAddedImages();
            
			for(tFrameBitmapCombListIt it = lstBitmaps.begin(); it != lstBitmaps.end(); ++it)
			{
				cBitmapSubImageComb *pComb = *it;

				bool bFit = false;
				AddBitmap(pComb->mpBitmap, pComb->mpSubImage->GetFullPath(), pComb->mpSubImage, &bFit);
				if(bFit==false){
					Error("When reorganizing images according to previous order an image still does not fit!!!\n");
					pComb->mpSubImage->mpFrameBitmapImage = NULL;
				}
			}
		}


		if(mbLogTime)Log(" adding again took: %dms\n",cPlatform::GetApplicationTime()-lStartTime);

		
		//Do this on demand instead...
		imageIt = mlstImages.begin();
		for(; imageIt != mlstImages.end(); ++imageIt)
		{
			cFBitmapImage* pImage = *imageIt;
			if(pImage->mpSubImage) pImage->mpSubImage->SetNeedUpdateUvs();
		}

		
		///////////////////////////////////////
		//Clean up and exit
		STLDeleteAll(lstBitmaps);
		
		mlAdditionsSinceReorganization =0;
		//Set as updated!
		mbIsUpdated = true;
	}
	
	//-----------------------------------------------------------------------

	bool cFrameBitmap::FlushToTexture()
	{
		if(mpAtlas ? ohpl_h5_atlas_take_updated(mpAtlas) : mbIsUpdated)
		{
			mpFrameTexture->GetTexture()->CreateFromBitmap(mpBitmap);
			mpFrameTexture->GetTexture()->SetWrapS(eTextureWrap_ClampToEdge);
			mpFrameTexture->GetTexture()->SetWrapT(eTextureWrap_ClampToEdge);

			//mpFrameTexture->SetPicCount(mlPicCount);
			mbIsUpdated = false;
			return true;
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------

	void cFrameBitmap::ClearAddedImages()
	{
		mpBitmap->Clear(cColor(1,0,1,0),0,0);

		mlPicCount =0;
		STLDeleteAll(mlstImages);
		mlstImages.clear();

		ohpl_packer_clear(mpPacker, mpBitmap->GetWidth(), mpBitmap->GetHeight());
	}
	
	//-----------------------------------------------------------------------
}
