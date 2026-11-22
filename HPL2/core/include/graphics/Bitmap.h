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

#ifndef HPL_BITMAP_H
#define HPL_BITMAP_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/RustGraphics.h"
#include <cstring>
#include <vector>

namespace hpl {

	//-----------------------------------------

	class cBitmapData
	{
	public:
		cBitmapData() : mpData(NULL), mlSize(0) {}
		~cBitmapData() { delete [] mpData; }

		void SetData(const unsigned char* apData, int alSize)
		{
			if(mpData && mlSize != alSize) { delete [] mpData; mpData = NULL; }
			if(mpData == NULL) mpData = new unsigned char[alSize];
			mlSize = alSize;
			memcpy(mpData, apData, alSize);
		}

		void Alloc(int alSize) { mpData = new unsigned char[alSize]; mlSize = alSize; }

		unsigned char* Data() const { return mpData; }
		int Size() const { return mlSize; }

	private:
		// Layout is read by Rust as OhplTexLevel { ptr, int }.
		unsigned char *mpData;
		int mlSize;
	};

	//-----------------------------------------

	class cBitmap
	{
	public:
		cBitmap() : mvImages(1), mbDataIsCompressed(false), mvSize(0), mPixelFormat(ePixelFormat_Unknown),
					mlBytesPerPixel(0), mlNumOfImages(1), mlNumOfMipMaps(1) {}

		inline const cVector3l& GetSize() const { return mvSize;}
		inline int GetWidth() const { return mvSize.x;}
		inline int GetHeight() const { return mvSize.y;}
		inline int GetDepth() const { return mvSize.z;}

		void SetSize(const cVector3l& avSize){ mvSize = avSize; }
        
		inline ePixelFormat GetPixelFormat() const { return mPixelFormat;}
		inline char GetBytesPerPixel() const { return mlBytesPerPixel; }
		inline int GetNumOfMipMaps() const { return mlNumOfMipMaps;}
		inline int GetNumOfImages() const { return mlNumOfImages;}

		void SetPixelFormat(ePixelFormat aFormat){ mPixelFormat = aFormat;}
		void SetBytesPerPixel(char alBpp){ mlBytesPerPixel = alBpp;}
		
		cBitmapData* GetData(int alImage, int alMipMapLevel)
		{
			return (alImage < mlNumOfImages && alMipMapLevel < mlNumOfMipMaps) ? &mvImages[alImage * mlNumOfMipMaps + alMipMapLevel] : NULL;
		}

		void SetUpData(int alNumOfImages, int alNumOfMipmaps)
		{
			mvImages.resize((mlNumOfImages = alNumOfImages) * (mlNumOfMipMaps = alNumOfMipmaps));
		}

		inline bool IsCompressed() const { return mbDataIsCompressed;}
		void SetIsCompressed(bool abX) { mbDataIsCompressed = abX;}

		inline const tWString& GetFileName()const { return msFileName;}
		void SetFileName(const tWString& asFileName) { msFileName = asFileName;}

		void CreateData(const cVector3l& avSize, ePixelFormat aFormat, int alImage, int alMipMap)
		{
			mvSize = cVector3l(avSize.x > 0 ? avSize.x : 1, avSize.y > 0 ? avSize.y : 1, avSize.z > 0 ? avSize.z : 1);
			mPixelFormat = aFormat;
			mlBytesPerPixel = (char)GetChannelsInPixelFormat(aFormat);
			GetData(alImage, alMipMap)->Alloc(mvSize.x * mvSize.y * mvSize.z * mlBytesPerPixel);
		}
	
		void Clear( const cColor& aColor,int alImage, int alMipMap)
		{
			if(mbDataIsCompressed) return;

			cBitmapData* pLevel = GetData(alImage, alMipMap);
			if(pLevel==NULL || pLevel->Data()==NULL) return;

			const float vF[4] = { aColor.r, aColor.g, aColor.b, aColor.a };
			unsigned char vClearColor[4];
			for(int i=0; i<4; ++i) vClearColor[i] = (unsigned char)FloatColorToUChar(vF[i]);

			ohpl_bitmap_clear(pLevel->Data(), pLevel->Size(), (long long)mvSize.x * mvSize.y * mvSize.z,
							  mlBytesPerPixel, mPixelFormat, vClearColor);
		}

		void Blit(	cBitmap *apSrc,
					const cVector3l& avDestPosition,const cVector3l& avSrcSize,
					const cVector3l& avSrcPosition,
					int alDestImage=0, int alDestMipMap=0,
					int alSrcImage=0, int alSrcMipMap=0)
		{
			if(mbDataIsCompressed) return;

			// Destination is looked up with the source image/mip, as in the original.
			cBitmapData *pFrom = apSrc->GetData(alSrcImage, alSrcMipMap), *pTo = GetData(alSrcImage, alSrcMipMap);
			if(pFrom==NULL || pTo==NULL || pFrom->Data()==NULL || pTo->Data()==NULL) return;

			const OhplSurface vTo = { mvSize, mPixelFormat, GetBytesPerPixel() };
			const OhplSurface vFrom = { apSrc->GetSize(), apSrc->GetPixelFormat(), apSrc->GetBytesPerPixel() };

			ohpl_bitmap_blit(&vTo, pTo->Data(), pTo->Size(), &vFrom, pFrom->Data(), pFrom->Size(),
							 &avDestPosition, &avSrcSize, &avSrcPosition);
		}

	private:
		std::vector<cBitmapData> mvImages;
		bool mbDataIsCompressed;

		tWString msFileName;

		cVector3l mvSize;
		ePixelFormat mPixelFormat;

		char mlBytesPerPixel;
		
		int mlNumOfImages;
		int mlNumOfMipMaps;
	};
};
#endif // HPL_BITMAP_H
