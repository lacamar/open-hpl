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

#ifndef HPL_FRAMEBITMAP_H
#define HPL_FRAMEBITMAP_H

#include <list>
#include <vector>

#include "graphics/FrameBase.h"
#include "math/MathTypes.h"

namespace hpl {

	class cFrameTexture;
	class cFrameSubImage;
	class cBitmap;
	
	class cFBitmapImage
	{
	public:
		~cFBitmapImage();

		cRect2l mRect;
		cFrameSubImage *mpSubImage;
	};

	//----------------------------------------
	
	typedef std::list<cFBitmapImage*> cFBitmapImageList;
	typedef cFBitmapImageList::iterator cFBitmapImageListIt;
		
	class cFrameBitmap : public iFrameBase
	{
	public:
		cFrameBitmap(cBitmap *apBitmap, cFrameTexture *apFrmTex, int alHandle);
		~cFrameBitmap();

		cFrameSubImage * AddBitmap(cBitmap *apSrc, const tWString& asFullPath, cFrameSubImage *apSubImageCreated, bool *apFoundNode=NULL);
		bool MinimumFit(cRect2l aSrc,cRect2l aDest);
		bool IsFull();
		bool IsUpdated();

		bool IsLocked()const;
		void SetLocked(bool abX);

		void SetNeedReorganisation();

		void UpdateBeforeDraw();
		
		void Reorganize();
		bool FlushToTexture();

		cFrameTexture* GetFrameTexture(){ return mpFrameTexture;}

		int GetHandle()const{ return mlHandle; }

		int GetAdditionsSinceReorganization();
	private:
		void ClearAddedImages();
		void ReorganizeRust();
		cFrameSubImage* AddBitmapRust(cBitmap *apSrc, const tWString& asFullPath, cFrameSubImage *apSubImageCreated, bool *apFoundNode);

		cBitmap* mpBitmap;
		cFrameTexture* mpFrameTexture;
		
		void* mpPacker;
		void* mpAtlas;

		cFBitmapImageList mlstImages;
		
		int mlMinHole;
		int mlHandle;
		bool mbIsFull;
		bool mbIsUpdated;
		bool mbIsLocked;
		bool mbNeedNeedReorganisation;

		int mlAdditionsSinceReorganization;
	};

};
#endif // HPL_FRAMEBITMAP_H
