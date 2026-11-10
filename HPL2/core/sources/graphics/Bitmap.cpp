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

#include "graphics/Bitmap.h"
#include "graphics/RustGraphics.h"

#include "system/LowLevelSystem.h"

#include <memory>
#include <cstring>


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// BITMAP DATA
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cBitmapData::cBitmapData()
	{
		mpData = NULL;
		mlSize =0;
	}
	
	//-----------------------------------------------------------------------

	cBitmapData::~cBitmapData()
	{
		if(mpData) hplDeleteArray( mpData );
	}
	

	//-----------------------------------------------------------------------

	void cBitmapData::SetData(const unsigned char* apData, int alSize)
	{
		if(mpData && mlSize != alSize) hplDelete( mpData );
		
		mlSize = alSize;
		mpData = hplNewArray(unsigned char, mlSize);
        
		memcpy(mpData, apData, mlSize);
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cBitmap::cBitmap()
	{
		mvImages.resize(1);
		
		mbDataIsCompressed = false;

		mvSize = 0;
		mPixelFormat = ePixelFormat_Unknown;

		mlBytesPerPixel =0;

		msFileName = _W("");

		mlNumOfMipMaps =1;
		mlNumOfImages =1;
	}

	//-----------------------------------------------------------------------

	cBitmap::~cBitmap()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cBitmapData* cBitmap::GetData(int alImage, int alMipMapLevel)
	{
		if(alImage >= mlNumOfImages) return NULL;
		if(alMipMapLevel >= mlNumOfMipMaps) return NULL;

		return &mvImages[alImage * mlNumOfMipMaps + alMipMapLevel];
	}
	
	//-----------------------------------------------------------------------

	void cBitmap::SetUpData(int alNumOfImages, int alNumOfMipmaps)
	{
		mlNumOfImages = alNumOfImages;
		mlNumOfMipMaps = alNumOfMipmaps;

		mvImages.resize(mlNumOfImages * mlNumOfMipMaps);
	}

	//-----------------------------------------------------------------------

	void cBitmap::CreateData(const cVector3l& avSize, ePixelFormat aFormat, int alImage, int alMipMap)
	{
		mvSize = avSize;
		if(mvSize.x<=0)mvSize.x =1;
		if(mvSize.y<=0)mvSize.y =1;
		if(mvSize.z<=0)mvSize.z =1;

		mPixelFormat = aFormat;
		mlBytesPerPixel = GetChannelsInPixelFormat(aFormat);

        int lDataSize = mvSize.x * mvSize.y * mvSize.z * mlBytesPerPixel;
		
		cBitmapData* pData = GetData(alImage, alMipMap);
		pData->mpData = hplNewArray(unsigned char, lDataSize);
		pData->mlSize = lDataSize;
	}

	//-----------------------------------------------------------------------

	void cBitmap::Clear( const cColor& aColor,int alImage, int alMipMap)
	{
		if(mbDataIsCompressed) return; //Cannot draw to compressed data!

		cBitmapData* pData = GetData(alImage, alMipMap);
		if(pData==NULL || pData->mpData==NULL) return;

		unsigned char vClearColor[] = {	(unsigned char)FloatColorToUChar(aColor.r),
										(unsigned char)FloatColorToUChar(aColor.g),
										(unsigned char)FloatColorToUChar(aColor.b),
										(unsigned char)FloatColorToUChar(aColor.a)
										};

		ohpl_bitmap_clear(pData->mpData, pData->mlSize, (long long)mvSize.x * mvSize.y * mvSize.z,
						  mlBytesPerPixel, mPixelFormat, vClearColor);
	}

	//-----------------------------------------------------------------------

	void cBitmap::Blit(	cBitmap *apSrcBmp, const cVector3l& avDestPosition,const cVector3l& avSrcSize,
						const cVector3l& avSrcPosition,
						int alDestImage, int alDestMipMap,
						int alSrcImage, int alSrcMipMap)
	{
		if(mbDataIsCompressed) return; //Cannot blit compressed data!

		cBitmapData* pSrcData = apSrcBmp->GetData(alSrcImage, alSrcMipMap);
		//The destination is looked up with the source image/mip (as in the original).
		cBitmapData* pDestData = GetData(alSrcImage, alSrcMipMap);
		if(pSrcData==NULL || pDestData==NULL || pSrcData->mpData==NULL || pDestData->mpData==NULL) return;

		OhplSurface dest = { mvSize, mPixelFormat, GetBytesPerPixel() };
		OhplSurface src = { apSrcBmp->GetSize(), apSrcBmp->GetPixelFormat(), apSrcBmp->GetBytesPerPixel() };

		ohpl_bitmap_blit(&dest, pDestData->mpData, pDestData->mlSize,
						 &src, pSrcData->mpData, pSrcData->mlSize,
						 &avDestPosition, &avSrcSize, &avSrcPosition);
	}

	//-----------------------------------------------------------------------
}
