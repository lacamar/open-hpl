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

static_assert(hpl::ePixelFormat_RGBA32 == 26, "ePixelFormat crosses the ABI");
static_assert(hpl::ePixelFormat_Alpha == 1 && hpl::ePixelFormat_Luminance == 2 && hpl::ePixelFormat_RGB == 4 &&
	hpl::ePixelFormat_RGBA == 5 && hpl::ePixelFormat_BGR == 6 && hpl::ePixelFormat_BGRA == 7 && hpl::ePixelFormat_DXT1 == 8 &&
	hpl::ePixelFormat_DXT3 == 10 && hpl::ePixelFormat_DXT5 == 12 && hpl::ePixelFormat_RGTC2 == 13 &&
	hpl::ePixelFormat_RGTC1 == 27 && hpl::ePixelFormat_BC6H == 28 && hpl::ePixelFormat_BC7 == 29, "ePixelFormat crosses the ABI");

namespace hpl {

	iBitmapLoaderDevil::iBitmapLoaderDevil()
	{
	}

	iBitmapLoaderDevil::~iBitmapLoaderDevil()
	{
	}

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

	bool iBitmapLoaderDevil::SaveBitmap(cBitmap* apBitmap,const tWString& asFile, tBitmapSaveFlag aFlags)
	{
		OhplL5InstallLog();
		return ohpl_l5_bitmap_save((const uint32_t*)asFile.c_str(), asFile.size(), apBitmap->GetWidth(), apBitmap->GetHeight(),
								   apBitmap->GetDepth(), (uint8_t)apBitmap->GetBytesPerPixel(), (uint32_t)apBitmap->GetPixelFormat(),
								   apBitmap->GetData(0,0)->mpData);
	}
}
