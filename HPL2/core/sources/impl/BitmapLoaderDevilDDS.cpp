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

#include "impl/BitmapLoaderDevilDDS.h"

#include "graphics/Bitmap.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"
#include "graphics/LowLevelGraphics.h"

#include <cstring>

namespace hpl {

	static bool TryLoadRgtc2DDS(const tWString& asFile, cBitmap** apBitmapOut)
	{
		FILE* pFile = cPlatform::OpenFile(asFile, _W("rb"));
		if (pFile == NULL) return false;

		unsigned char vHeader[128];
		bool bOk = fread(vHeader, 1, sizeof(vHeader), pFile) == sizeof(vHeader) && memcmp(vHeader, "DDS ", 4) == 0;
		bool bIsBC5U = bOk && memcmp(vHeader + 84, "BC5U", 4) == 0;
		bool bIsATI2 = bOk && memcmp(vHeader + 84, "ATI2", 4) == 0;
		if (bIsBC5U == false && bIsATI2 == false)
		{
			fclose(pFile);
			return false;
		}

		unsigned int lHeight = 0, lWidth = 0, lMipMaps = 0;
		memcpy(&lHeight, vHeader + 12, 4);
		memcpy(&lWidth, vHeader + 16, 4);
		memcpy(&lMipMaps, vHeader + 28, 4);
		if (lMipMaps == 0) lMipMaps = 1;

		cBitmap* pBitmap = hplNew(cBitmap, ());
		if (lMipMaps > 1) pBitmap->SetUpData(1, (int)lMipMaps);
		pBitmap->SetSize(cVector3l((int)lWidth, (int)lHeight, 1));
		pBitmap->SetBytesPerPixel(1);
		pBitmap->SetIsCompressed(true);
		pBitmap->SetPixelFormat(bIsBC5U ? ePixelFormat_RGTC2_XY : ePixelFormat_RGTC2_YX);

		unsigned int lW = lWidth, lH = lHeight;
		for (unsigned int mip = 0; mip < lMipMaps; ++mip)
		{
			int lSize = (int)(((lW + 3) / 4) * ((lH + 3) / 4) * 16);
			cBitmapData* pImage = pBitmap->GetData(0, (int)mip);
			pImage->mlSize = lSize;
			pImage->mpData = hplNewArray(unsigned char, lSize);
			if (fread(pImage->mpData, 1, (size_t)lSize, pFile) != (size_t)lSize)
			{
				fclose(pFile);
				hplDelete(pBitmap);
				return false;
			}
			lW = lW > 1 ? lW / 2 : 1;
			lH = lH > 1 ? lH / 2 : 1;
		}

		fclose(pFile);
		*apBitmapOut = pBitmap;
		return true;
	}

	static bool TryLoadUncompressedAlphaDDS(const tWString& asFile, cBitmap** apBitmapOut)
	{
		FILE* pFile = cPlatform::OpenFile(asFile, _W("rb"));
		if (pFile == NULL) return false;

		unsigned char vHeader[128];
		bool bOk = fread(vHeader, 1, sizeof(vHeader), pFile) == sizeof(vHeader);
		if (bOk && memcmp(vHeader, "DDS ", 4) != 0) bOk = false;

		unsigned int lHeaderFlags = 0, lHeight = 0, lWidth = 0, lPitch = 0;
		unsigned int lPixelFormatFlags = 0, lRgbBitCount = 0;
		if (bOk)
		{
			memcpy(&lHeaderFlags, vHeader + 8, 4);
			memcpy(&lHeight, vHeader + 12, 4);
			memcpy(&lWidth, vHeader + 16, 4);
			memcpy(&lPitch, vHeader + 20, 4);
			memcpy(&lPixelFormatFlags, vHeader + 80, 4);
			memcpy(&lRgbBitCount, vHeader + 88, 4);
		}

		const unsigned int klDdpfAlpha = 0x2;
		const unsigned int klDdsdPitch = 0x8;

		if (bOk == false || lPixelFormatFlags != klDdpfAlpha || lRgbBitCount != 8 ||
			lWidth == 0 || lHeight == 0)
		{
			fclose(pFile);
			return false;
		}

		unsigned int lRowBytes = ((lHeaderFlags & klDdsdPitch) != 0 && lPitch > 0) ? lPitch : lWidth;

		unsigned char* pPixels = hplNewArray(unsigned char, (size_t)lWidth * lHeight);
		bool bReadOk = true;
		for (unsigned int y = 0; y < lHeight; ++y)
		{
			if (fseek(pFile, 128 + (long)y * lRowBytes, SEEK_SET) != 0 ||
				fread(pPixels + (size_t)y * lWidth, 1, lWidth, pFile) != lWidth)
			{
				bReadOk = false;
				break;
			}
		}
		fclose(pFile);

		if (bReadOk == false)
		{
			hplDeleteArray(pPixels);
			return false;
		}

		cBitmap* pBitmap = hplNew(cBitmap, ());
		pBitmap->SetSize(cVector3l((int)lWidth, (int)lHeight, 1));
		pBitmap->SetBytesPerPixel(1);
		pBitmap->SetIsCompressed(false);
		pBitmap->SetPixelFormat(ePixelFormat_Alpha);
		pBitmap->GetData(0, 0)->SetData(pPixels, (int)(lWidth * lHeight));

		hplDeleteArray(pPixels);

		*apBitmapOut = pBitmap;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cBitmapLoaderDevilDDS::cBitmapLoaderDevilDDS() : iBitmapLoaderDevil()
	{
		AddSupportedExtension("dds");
	}
	
	cBitmapLoaderDevilDDS::~cBitmapLoaderDevilDDS()
	{
		
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cBitmap* cBitmapLoaderDevilDDS::LoadBitmap(const tWString& asFile, tBitmapLoadFlag aFlags)
	{
		Initialize();

		{
			cBitmap* pRgtc2Bitmap = NULL;
			if (TryLoadRgtc2DDS(asFile, &pRgtc2Bitmap))
				return pRgtc2Bitmap;
		}
		{
			cBitmap* pRawAlphaBitmap = NULL;
			if (TryLoadUncompressedAlphaDDS(asFile, &pRawAlphaBitmap))
				return pRawAlphaBitmap;
		}

		//create image id
		unsigned int lImageId;
		ilGenImages(1,&lImageId);

		//Bind image
		ilBindImage(lImageId);

		//Make sure compressed data is kept
		ilSetInteger(IL_KEEP_DXTC_DATA, IL_TRUE);

		//Try and load the file.
		if(LoadDevilImageW(asFile)==false)
		{
			ilDeleteImages(1,&lImageId);
			return NULL;
		}

		cBitmap *pBitmap = hplNew(cBitmap, () );

		////////////////////////////////////////
		//Get main image properties
		int lNumOfImages = ilGetInteger(IL_NUM_IMAGES) + 1; //Returns number of images - 1, so need to add 1
		int lNumOfMipMaps = ilGetInteger(IL_NUM_MIPMAPS) + 1; //We count first image as mimap too, so add 1
		int lCubeFlags = ilGetInteger(IL_IMAGE_CUBEFLAGS);

		if(lCubeFlags != 0) lNumOfImages = 6;

		//No need to setup if only one image and mipmap, data is already setup for that.
		if(lNumOfImages > 1 || lNumOfMipMaps > 1)
		{
			pBitmap->SetUpData(lNumOfImages, lNumOfMipMaps);
		}

		//Fet the size of image,
		cVector3l vSize;
		vSize.x = ilGetInteger(IL_IMAGE_WIDTH);
		vSize.y = ilGetInteger(IL_IMAGE_HEIGHT);
		vSize.z = ilGetInteger(IL_IMAGE_DEPTH);
		pBitmap->SetSize(vSize);

		//Det properties on the pixel data
		int lBytesPerPixel = ilGetInteger(IL_IMAGE_BYTES_PER_PIXEL);
		ePixelFormat pixelFormat = DevilPixelFormatToHPL(ilGetInteger(IL_IMAGE_FORMAT));

		//Get the compression format used (if any)
		int lDXTFormat = ilGetInteger( IL_DXTC_DATA_FORMAT );

		/*Log("Image: %s\n",cString::To8Char(asFile).c_str());
		Log(" Number of mipmaps: %d\n",lNumOfMipMaps);
		Log(" Number of image: %d\n",lNumOfImages);
		Log(" Compression: %d\n",lDXTFormat != IL_DXT_NO_COMP);*/

		//////////////////////////////////////////////////
		// Load compressed image
		if(	lDXTFormat != IL_DXT_NO_COMP && //mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureCompression_DXTC) &&
			(aFlags & eBitmapLoadFlag_ForceNoCompression)==0 )
		{
			ePixelFormat compressedPixelFormat = GetPixelFormatFromILDXT(lDXTFormat);
			pBitmap->SetBytesPerPixel(lBytesPerPixel);
			pBitmap->SetIsCompressed(true);
			pBitmap->SetPixelFormat(compressedPixelFormat);

			for(int image=0; image< lNumOfImages; ++image)
			for(int mip=0; mip< lNumOfMipMaps; ++mip)
			{
				if(lNumOfImages > 1 || lNumOfMipMaps > 1)
				{
					ilBindImage(lImageId); // For some reason this is needed....
					if(lNumOfImages > 1)	ilActiveImage(image);
					if(lNumOfMipMaps > 1)	ilActiveMipmap(mip);
				}

				cBitmapData *pImage = pBitmap->GetData(image,mip);

				int lSize = ilGetDXTCData(NULL, 0, lDXTFormat);
				pImage->mlSize = lSize;
				pImage->mpData = hplNewArray(unsigned char,lSize);

				ilGetDXTCData(pImage->mpData, lSize, lDXTFormat);

				//int format = ilGetInteger( IL_DXTC_DATA_FORMAT );
				//Log("  %d bounds: %dx%d size: %d\n",mip,ilGetInteger(IL_IMAGE_WIDTH),ilGetInteger(IL_IMAGE_HEIGHT),lSize);

			}
		}
		//////////////////////////////////////////////////
		// Load uncompressed image
		else
		{
			pBitmap->SetBytesPerPixel(lBytesPerPixel);
			pBitmap->SetIsCompressed(false);
			pBitmap->SetPixelFormat(pixelFormat);

			for(int image=0; image< lNumOfImages; ++image)
			for(int mip=0; mip< lNumOfMipMaps; ++mip)
			{
				if(lNumOfImages > 1 || lNumOfMipMaps > 1)
				{
					ilBindImage(lImageId); // For some reason this is needed....
					if(lNumOfImages > 1)	ilActiveImage(image);
					if(lNumOfMipMaps > 1)	ilActiveMipmap(mip);
				}

				cBitmapData *pImage = pBitmap->GetData(image,mip);
				int lSize = ilGetInteger(IL_IMAGE_SIZE_OF_DATA);
				pImage->SetData(ilGetData(), lSize);
			}
		}
	

		ilDeleteImages(1,&lImageId);

		return pBitmap;
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	ePixelFormat cBitmapLoaderDevilDDS::GetPixelFormatFromILDXT(int alDxtFormat)
	{
		switch(alDxtFormat)
		{
		case IL_DXT1:	return ePixelFormat_DXT1;
		case IL_DXT3:	return ePixelFormat_DXT3;
		case IL_DXT5:	return ePixelFormat_DXT5;
		}

		return ePixelFormat_Unknown;
	}

	//-----------------------------------------------------------------------
}
