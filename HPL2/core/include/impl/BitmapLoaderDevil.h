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
#ifndef HPL_BITMAP_LOADER_DEVIL_H
#define HPL_BITMAP_LOADER_DEVIL_H

#include "resources/BitmapLoader.h"
#include "graphics/Bitmap.h"
#include "impl/RustL5.h"

static_assert(hpl::ePixelFormat_RGBA32 == 26, "ePixelFormat crosses the ABI");
static_assert(hpl::ePixelFormat_Alpha == 1 && hpl::ePixelFormat_Luminance == 2 && hpl::ePixelFormat_RGB == 4 &&
	hpl::ePixelFormat_RGBA == 5 && hpl::ePixelFormat_BGR == 6 && hpl::ePixelFormat_BGRA == 7 && hpl::ePixelFormat_DXT1 == 8 &&
	hpl::ePixelFormat_DXT3 == 10 && hpl::ePixelFormat_DXT5 == 12 && hpl::ePixelFormat_RGTC2 == 13 &&
	hpl::ePixelFormat_RGTC1 == 27 && hpl::ePixelFormat_BC6H == 28 && hpl::ePixelFormat_BC7 == 29, "ePixelFormat crosses the ABI");

namespace hpl {

	class iBitmapLoaderDevil : public iBitmapLoader
	{
	public:
		bool SaveBitmap(cBitmap* apBitmap, const tWString& asFile, tBitmapSaveFlag)
		{
			OhplL5InstallLog();
			cBitmap& b = *apBitmap;
			return ohpl_l5_bitmap_save((const uint32_t*)asFile.c_str(), asFile.size(), b.GetWidth(), b.GetHeight(), b.GetDepth(),
									   (uint8_t)b.GetBytesPerPixel(), (uint32_t)b.GetPixelFormat(), b.GetData(0,0)->Data());
		}

	protected:
		cBitmap* LoadBitmapRust(const tWString& asFile, unsigned alKind, tBitmapLoadFlag aFlags)
		{
			OhplL5InstallLog();
			static_assert(sizeof(wchar_t) == sizeof(uint32_t), "wide text crosses the ABI as u32");
			OhplL5Bitmap* pSrc = ohpl_l5_bitmap_load((const uint32_t*)asFile.c_str(), asFile.size(), alKind,
													 (aFlags & eBitmapLoadFlag_ForceNoCompression) != 0);
			if(pSrc == NULL) return NULL;

			OhplL5BitmapInfo i;
			ohpl_l5_bitmap_info(pSrc, &i);

			cBitmap* pBmp = hplNew(cBitmap, ());
			if(i.setup_data) pBmp->SetUpData(i.images, i.mips);
			pBmp->SetSize(cVector3l(i.width, i.height, i.depth));
			pBmp->SetBytesPerPixel(i.bytes_per_pixel);
			pBmp->SetIsCompressed(i.compressed);
			pBmp->SetPixelFormat((ePixelFormat)i.format);
			for(int img = 0; img < i.images; ++img)
				for(int mip = 0; mip < i.mips; ++mip)
				{
					int lBytes = 0;
					if(const uint8_t* p = ohpl_l5_bitmap_plane(pSrc, img, mip, &lBytes)) pBmp->GetData(img, mip)->SetData(p, lBytes);
				}
			ohpl_l5_bitmap_free(pSrc);
			return pBmp;
		}
	};

};
#endif // HPL_BITMAP_LOADER_DEVIL_H
