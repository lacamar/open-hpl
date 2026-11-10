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
#include "impl/RustImpl.h"

#include "graphics/Bitmap.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"
#include "graphics/LowLevelGraphics.h"

#include <cstring>

namespace hpl {

	static bool TryLoadBlockCompressedDDS(const tWString& asFile, tBitmapLoadFlag aFlags, cBitmap** apBitmapOut)
	{
		FILE* pFile = cPlatform::OpenFile(asFile, _W("rb"));
		if (pFile == NULL) return false;

		unsigned char vHeader[148] = {};
		size_t lRead = fread(vHeader, 1, sizeof(vHeader), pFile);
		long lFileSize = fseek(pFile, 0, SEEK_END) == 0 ? ftell(pFile) : -1;
		OhplDdsBlockPlan plan;
		if (lFileSize < 0 || !ohpl_dds_plan_block(vHeader, lRead, (uint64_t)lFileSize, (aFlags & eBitmapLoadFlag_ForceNoCompression) != 0, &plan) ||
			fseek(pFile, (long)plan.data_offset, SEEK_SET) != 0)
		{
			fclose(pFile);
			return false;
		}

		cBitmap* pBitmap = hplNew(cBitmap, ());
		if (plan.images > 1 || plan.mips > 1) pBitmap->SetUpData((int)plan.images, (int)plan.mips);
		pBitmap->SetSize(cVector3l((int)plan.width, (int)plan.height, 1));
		pBitmap->SetBytesPerPixel(plan.bytes_per_pixel);
		pBitmap->SetIsCompressed(plan.block_size != 0);
		pBitmap->SetPixelFormat((ePixelFormat)plan.format);

		for (unsigned image = 0; image < plan.images; ++image)
		{
			for (unsigned mip = 0; mip < plan.mips; ++mip)
			{
				int lSize = ohpl_dds_mip_size(&plan, mip);
				cBitmapData* pImage = pBitmap->GetData((int)image, (int)mip);
				pImage->mlSize = lSize;
				pImage->mpData = hplNewArray(unsigned char, lSize);
				if (fread(pImage->mpData, 1, (size_t)lSize, pFile) != (size_t)lSize)
				{
					fclose(pFile);
					hplDelete(pBitmap);
					return false;
				}
			}
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
		long lFileSize = bOk && fseek(pFile, 0, SEEK_END) == 0 ? ftell(pFile) : -1;
		OhplDdsAlphaPlan plan;
		// Rebirth font pages: DDPF_ALPHAPIXELS alone
		if (lFileSize < 0 || !ohpl_dds_plan_alpha(vHeader, sizeof(vHeader), (uint64_t)lFileSize, &plan))
		{
			fclose(pFile);
			return false;
		}
		unsigned int lWidth = plan.width, lHeight = plan.height, lRowBytes = plan.row_bytes;

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
			cBitmap* pBlockBitmap = NULL;
			if (TryLoadBlockCompressedDDS(asFile, aFlags, &pBlockBitmap))
				return pBlockBitmap;
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
		//IL counts are one less than the number of images / mipmaps
		int lCubeFlags = ilGetInteger(IL_IMAGE_CUBEFLAGS);
		OhplDdsLayout layout;
		ohpl_devil_dds_layout(ilGetInteger(IL_NUM_IMAGES), ilGetInteger(IL_NUM_MIPMAPS), lCubeFlags, &layout);
		int lNumOfImages = layout.images;
		int lNumOfMipMaps = layout.mips;

		//No need to setup if only one image and mipmap, data is already setup for that.
		if(layout.setup_data)
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
		if (ilGetInteger(IL_IMAGE_TYPE) == IL_FLOAT && pixelFormat == ePixelFormat_RGBA)
			pixelFormat = ePixelFormat_RGBA32;

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
					if(lNumOfImages > 1)	lCubeFlags ? ilActiveFace(image) : ilActiveImage(image);
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
					if(lNumOfImages > 1)	lCubeFlags ? ilActiveFace(image) : ilActiveImage(image);
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
		return (ePixelFormat)ohpl_devil_dxt_to_hpl((unsigned)alDxtFormat);
	}

	//-----------------------------------------------------------------------
}
