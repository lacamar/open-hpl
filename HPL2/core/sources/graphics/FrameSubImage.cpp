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

#include "graphics/FrameSubImage.h"
#include "graphics/RustGraphics.h"
#include "graphics/FrameTexture.h"
#include "graphics/FrameBitmap.h"
#include "graphics/Texture.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cFrameSubImage::cFrameSubImage(	const tString& asName,const tWString& asFullPath, cFrameTexture *apFrameTex, 
									cFrameBitmap *apFrameBmp, cRect2l aRect,
									cVector2l avSrcSize, int alHandle,cFBitmapImage *apFrameBitmapImage) 
									: iResourceBase(asName, asFullPath,0)
	{
		mpFrameTexture = apFrameTex;
		mpFrameBitmap = apFrameBmp;
		mpFrameBitmapImage = apFrameBitmapImage;
		mRect = aRect;
		mvSourceSize = avSrcSize;
		mlHandle = alHandle;
		mlUpdateCount =0;
		mbNeedUvUpdate = false;

		cVector2f vUv[4];
		ohpl_subimage_uvs(&mRect, &mvSourceSize, vUv);

		mvVtx.push_back(cVertex(cVector3f(0,0,0), cVector3f(vUv[0].x, vUv[0].y,0), cColor(1)));
		mvVtx.push_back(cVertex(cVector3f((float)mRect.w,0,0), cVector3f(vUv[1].x, vUv[1].y,0), cColor(1)));
		mvVtx.push_back(cVertex(cVector3f((float)mRect.w,(float)mRect.h,0), cVector3f(vUv[2].x, vUv[2].y,0), cColor(1)));
		mvVtx.push_back(cVertex(cVector3f(0,(float)mRect.h,0), cVector3f(vUv[3].x, vUv[3].y,0), cColor(1)));
	}

	//-----------------------------------------------------------------------

	cFrameSubImage::~cFrameSubImage()
	{
		if(mpFrameBitmap) mpFrameBitmap->SetNeedReorganisation();
		if(mpFrameBitmapImage) mpFrameBitmapImage->mpSubImage = NULL; //Since we are deleting, it is no longer valid.

		mvVtx.clear();
		//mpFrameTexture->DecPicCount();
		mpFrameTexture = NULL;
		mlHandle = -1;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	iTexture *cFrameSubImage::GetTexture()const{return mpFrameTexture->GetTexture();}

	//-----------------------------------------------------------------------
	
	tVertexVec cFrameSubImage::GetVertexVecCopy(const cVector2f &avPos, const cVector2f &avSize)
	{
		tVertexVec vTmpVtx = mvVtx;
		
		if(avSize == cVector2f(-1,-1)) {
			vTmpVtx[1].pos.x = mvVtx[0].pos.x + mRect.w;
			vTmpVtx[2].pos.x = mvVtx[0].pos.x + mRect.w;
			vTmpVtx[2].pos.y = mvVtx[0].pos.y + mRect.h;
			vTmpVtx[3].pos.y = mvVtx[0].pos.y + mRect.h;
		}
		else {
			vTmpVtx[1].pos.x = mvVtx[0].pos.x + avSize.x;
			vTmpVtx[2].pos.x = mvVtx[0].pos.x + avSize.x;
			vTmpVtx[2].pos.y = mvVtx[0].pos.y + avSize.y;
			vTmpVtx[3].pos.y = mvVtx[0].pos.y + avSize.y;
		}

		for(int i=0;i<4;i++)
			vTmpVtx[i].pos+=avPos;

		return vTmpVtx;
	}

	//-----------------------------------------------------------------------

	void cFrameSubImage::Flush()
	{
		if(mpFrameBitmap)
		{
			mpFrameBitmap->UpdateBeforeDraw();
			if(mbNeedUvUpdate)
			{
				UpdateUvs();
				mbNeedUvUpdate = false;
			}
		}
	}

	//-----------------------------------------------------------------------

	void cFrameSubImage::SetNeedUpdateUvs()
	{
		mbNeedUvUpdate = true;
	}
	
	//-----------------------------------------------------------------------

	bool cFrameSubImage::Reload()
	{
		return false;
	}
	
	//-----------------------------------------------------------------------

	void cFrameSubImage::Unload()
	{
	}
	
	//-----------------------------------------------------------------------

	void cFrameSubImage::Destroy()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	void cFrameSubImage::UpdateUvs()
	{
		if(mpFrameBitmapImage==NULL) return;

		++mlUpdateCount;
		
		cVector2f vUv[4];
		ohpl_subimage_uvs(&mpFrameBitmapImage->mRect, &mvSourceSize, vUv);

		for(int i=0; i<4; ++i) mvVtx[i].tex = cVector3f(vUv[i].x, vUv[i].y, 0);
	}
	
	//-----------------------------------------------------------------------

}
