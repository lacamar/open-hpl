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

#include "impl/BitmapLoaderDevil.h"

#include "graphics/Bitmap.h"
#include "system/LowLevelSystem.h"

#include "system/String.h"
#include "system/Platform.h"
#include "impl/RustImpl.h"

#ifdef WIN32
#include <io.h>
#endif

static_assert(IL_RGB == 0x1907 && IL_RGBA == 0x1908 && IL_BGR == 0x80E0 && IL_BGRA == 0x80E1 && IL_LUMINANCE == 0x1909, "IL pixel formats cross the ABI");
static_assert(IL_BMP == 0x0420 && IL_JPG == 0x0425 && IL_PCX == 0x0428 && IL_PNG == 0x042A && IL_SGI == 0x042C &&
	IL_TGA == 0x042D && IL_TIF == 0x042E && IL_DDS == 0x0437 && IL_PSD == 0x0439, "IL file types cross the ABI");
static_assert(IL_DXT1 == 0x0706 && IL_DXT3 == 0x0708 && IL_DXT5 == 0x070A && IL_DXT_NO_COMP == 0x070B, "IL DXT formats cross the ABI");
static_assert(hpl::ePixelFormat_RGBA32 == 26, "ePixelFormat crosses the ABI");
static_assert(hpl::ePixelFormat_Alpha == 1 && hpl::ePixelFormat_Luminance == 2 && hpl::ePixelFormat_RGB == 4 &&
	hpl::ePixelFormat_RGBA == 5 && hpl::ePixelFormat_BGR == 6 && hpl::ePixelFormat_BGRA == 7 && hpl::ePixelFormat_DXT1 == 8 &&
	hpl::ePixelFormat_DXT3 == 10 && hpl::ePixelFormat_DXT5 == 12 && hpl::ePixelFormat_RGTC2 == 13 &&
	hpl::ePixelFormat_RGTC1 == 27 && hpl::ePixelFormat_BC6H == 28 && hpl::ePixelFormat_BC7 == 29, "ePixelFormat crosses the ABI");

namespace hpl {

	bool iBitmapLoaderDevil::mbIsInitialized = false;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iBitmapLoaderDevil::iBitmapLoaderDevil()
	{
	}
	
	iBitmapLoaderDevil::~iBitmapLoaderDevil()
	{
		
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cBitmap* iBitmapLoaderDevil::LoadBitmapRust(const tWString& asFile, unsigned alKind, tBitmapLoadFlag aFlags)
	{
		OhplL5InstallLog();
		static_assert(sizeof(wchar_t) == sizeof(uint32_t), "wide text crosses the ABI as u32");
		OhplL5Bitmap* pRust = ohpl_l5_bitmap_load((const uint32_t*)asFile.c_str(), asFile.size(), alKind,
												  (aFlags & eBitmapLoadFlag_ForceNoCompression) != 0);
		if(pRust==NULL) return NULL;

		OhplL5BitmapInfo info;
		ohpl_l5_bitmap_info(pRust, &info);

		cBitmap *pBitmap = hplNew(cBitmap, ());
		if(info.setup_data) pBitmap->SetUpData(info.images, info.mips);
		pBitmap->SetSize(cVector3l(info.width, info.height, info.depth));
		pBitmap->SetBytesPerPixel(info.bytes_per_pixel);
		pBitmap->SetIsCompressed(info.compressed);
		pBitmap->SetPixelFormat((ePixelFormat)info.format);
		for(int image=0; image<info.images; ++image)
		for(int mip=0; mip<info.mips; ++mip)
		{
			int lSize = 0;
			const uint8_t* pData = ohpl_l5_bitmap_plane(pRust, image, mip, &lSize);
			if(pData) pBitmap->GetData(image,mip)->SetData(pData, lSize);
		}
		ohpl_l5_bitmap_free(pRust);
		return pBitmap;
	}

	//-----------------------------------------------------------------------

	bool iBitmapLoaderDevil::SaveBitmap(cBitmap* apBitmap,const tWString& asFile, tBitmapSaveFlag aFlags)
	{
		if(OhplBackendRustRequested())
		{
			OhplL5InstallLog();
			return ohpl_l5_bitmap_save((const uint32_t*)asFile.c_str(), asFile.size(), apBitmap->GetWidth(), apBitmap->GetHeight(),
									   apBitmap->GetDepth(), (uint8_t)apBitmap->GetBytesPerPixel(), (uint32_t)apBitmap->GetPixelFormat(),
									   apBitmap->GetData(0,0)->mpData);
		}

		Initialize();

		//create image id
		unsigned int lImageId;
		ilGenImages(1,&lImageId);

		//Bind image
		ilBindImage(lImageId);

		////////////////////////////////////////
		//Set the data from bitmap
		ILubyte lBytesPerPixel = apBitmap->GetBytesPerPixel();
		ILenum lILFormat = HPLPixelFormatToDevil(apBitmap->GetPixelFormat());	

		if(lILFormat ==0)
		{
			Error("Could not save image to '%s'! Pixel format of bitmap was not valid!\n",cString::To8Char(asFile).c_str());
			ilDeleteImages(1,&lImageId);
			return false;
		}
		
		ilTexImage(	apBitmap->GetWidth(), apBitmap->GetHeight(),apBitmap->GetDepth(),lBytesPerPixel,
					lILFormat,IL_UNSIGNED_BYTE, apBitmap->GetData(0,0)->mpData);
		
		
		////////////////////////////////////////
		//Save to file
		//Get the type of file
		ILenum lType = FileNameToDevilTypeW(asFile);
		if(lType == 0) {
			Error("Filename %s has not a valid image extension!\n",cString::To8Char(asFile).c_str());
			ilDeleteImages(1,&lImageId);
			return false;
		}
		//Get a pointer to the file
		FILE *pFile = cPlatform::OpenFile(asFile, _W("w+b"));
		if(pFile == NULL){
			Error("Could not open file %s for writing!\n",cString::To8Char(asFile).c_str());
			ilDeleteImages(1,&lImageId);
			return false;
		}

		//Save the image to file
		int	lRet = ilSaveF(lType, pFile);
		if(lRet ==0) Error("Error when saving image '%s' to file!\n",cString::To8Char(asFile).c_str());
		
		//Close file pointer
		fclose(pFile);

		////////////////////////////////////////
		//Exit
		ilDeleteImages(1,&lImageId);
		

		return lRet != 0;
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	bool iBitmapLoaderDevil::LoadDevilImageW(const tWString& asFile)
	{
		ILenum lType = FileNameToDevilTypeW(asFile);
		if(lType == 0) {
			Error("Filename %s has not a valid image extension!\n",cString::To8Char(asFile).c_str());
			return false;
		}

		FILE *pFile = cPlatform::OpenFile(asFile, _W("rb"));
		if(pFile == NULL){
			Error("Could not open file %s for reading!\n",cString::To8Char(asFile).c_str());
			return false;
		}
		
		int lRet = ilLoadF(lType, pFile);

		fclose(pFile);

		return lRet;
	}

	//-----------------------------------------------------------------------

	static void ILAPIENTRY   DevilClose(ILHANDLE apHandle)
	{
		fclose((FILE*)apHandle);
	}
	static ILHANDLE ILAPIENTRY DevilOpen(const ILstring apFile)
	{
		return fopen(apFile, "wb");
	}
	static ILint ILAPIENTRY    DevilPutc(ILubyte alData, ILHANDLE apHandle)
	{
		return fputc(alData, (FILE*)apHandle);
	}
	static ILint ILAPIENTRY    DevilSeek(ILHANDLE apHandle, ILint alOffset, ILint alMode)
	{
		return fseek((FILE*)apHandle,alOffset,alMode);
	}
	static ILint  ILAPIENTRY   DevilTell(ILHANDLE apHandle)
	{
		return ftell((FILE*)apHandle);
	}
	static ILint ILAPIENTRY    DevilWrite(const void* apBuffer, ILuint alSize, ILuint alNum, ILHANDLE apHandle)
	{
		return (ILint)fwrite(apBuffer,alSize,alNum,(FILE*)apHandle);
	}
	static ILboolean ILAPIENTRY DevilEof(ILHANDLE apHandle)
	{
		ILuint OrigPos, FileSize;

		// Find out the filesize for checking for the end of file
		OrigPos = DevilTell(apHandle);
		DevilSeek(apHandle, 0, IL_SEEK_END);
		FileSize = DevilTell(apHandle);
		DevilSeek(apHandle, OrigPos, IL_SEEK_SET);

		if (DevilTell(apHandle) >= FileSize) return IL_TRUE;
		return IL_FALSE;
	}

	ILint ILAPIENTRY DevilGetC(ILHANDLE Handle)
	{
		return fgetc((FILE*)Handle);
	}

	ILint ILAPIENTRY DevilRead(void *Buffer, ILuint Size, ILuint Number, ILHANDLE Handle)
	{
		return fread(Buffer, Size, Number, (FILE*)Handle);
	}

	//-----------------------------------------------------------------------

	void iBitmapLoaderDevil::Initialize()
	{
		if(mbIsInitialized==true) return;

		Log(" Initializing DevIL\n");
		ilInit();

		Log("  Vendor String: %s\n",ilGetString(IL_VENDOR));
		Log("  Version String: %s\n", ilGetString(IL_VERSION_NUM));
		int libVersion = ilGetInteger(IL_VERSION_NUM);
		Log("  Version Number: %d\n", libVersion);
		
		if (libVersion != IL_VERSION) {
			Log("  ***** WARNING *****\n");
			Log("  Expecting IL version %d and found version %d\n",IL_VERSION,libVersion);
			Log("  Cube Maps may not load correctly in this version and will not render correctly in game\n");
			Log("  Please use the included version of libIL and not a custom build if you have issues\n");
			Log("  ***** WARNING *****\n");
		}

		ilSetWrite(DevilOpen,DevilClose,DevilPutc,DevilSeek,DevilTell,DevilWrite);
		ilSetRead(DevilOpen,DevilClose,DevilEof,DevilGetC,DevilRead, DevilSeek,DevilTell);

		ilEnable(IL_ORIGIN_SET);
		ilSetInteger(IL_ORIGIN_MODE, IL_ORIGIN_UPPER_LEFT);

		mbIsInitialized = true;
	}

	//-----------------------------------------------------------------------

	ePixelFormat iBitmapLoaderDevil::DevilPixelFormatToHPL(int alFormat)
	{
		return (ePixelFormat)ohpl_devil_pixel_format_to_hpl((unsigned)alFormat);
	}

	//-----------------------------------------------------------------------

	ILenum iBitmapLoaderDevil::HPLPixelFormatToDevil(ePixelFormat aFormat)
	{
		return ohpl_devil_pixel_format_from_hpl((unsigned)aFormat);
	}

	//-----------------------------------------------------------------------
	
	ILenum iBitmapLoaderDevil::FileNameToDevilTypeW(const tWString& asFile)
	{
		tWString sExt = cString::ToLowerCaseW(cString::GetFileExtW(asFile));
		static_assert(sizeof(wchar_t) == sizeof(unsigned), "wide text crosses the ABI as u32");
		return ohpl_devil_type_from_extension((const unsigned*)sExt.c_str(), sExt.size());
	}

	//-----------------------------------------------------------------------
}
