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

#include "resources/ImageManager.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/Bitmap.h"


#include "graphics/FrameSubImage.h"
#include "graphics/FrameBitmap.h"
#include "graphics/FrameTexture.h"
#include "resources/LowLevelResources.h"
#include "resources/Resources.h"
#include "resources/BitmapLoaderHandler.h"
#include "resources/FileSearcher.h"
#include "resources/RustResMgr.h"


namespace hpl {

	cVector2l cImageManager::mvDefaultFrameSize(512,512);

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cImageManager::cImageManager(	cResources *mpResources, iLowLevelGraphics *apLowLevelGraphics,
									iLowLevelSystem *apLowLevelSystem)
	: iResourceManager(mpResources->GetFileSearcher(), mpResources->GetLowLevel(),apLowLevelSystem)
	{
       mpLowLevelGraphics = apLowLevelGraphics;

	   mpBitmapLoaderHandler = mpResources->GetBitmapLoaderHandler();

	   mvFrameSize = mvDefaultFrameSize;
	   mlFrameHandle = 0;
	   if(mpRs) ohpl_h3f_image_init(mpRs, &mOps, mvFrameSize.x, mvFrameSize.y);
	}

	cImageManager::~cImageManager()
	{
		if(mpRs) { ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_IMAGE); return; }
		//DeleteAllBitmapFrames();
		DestroyAll();

		Log(" Done with images\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iResourceBase* cImageManager::CreateInFrame(const tString& asName, int alFrameHandle)
	{
		if(mpRs)
		{
			cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
			return static_cast<iResourceBase*>(ohpl_h3f_image_create_in_frame(mpRs, H3Ops(), RM_STR(asName), alFrameHandle, exts.h3(), exts.size()));
		}
		cFrameSubImage *pImage = NULL;
		tWString sPath;

		BeginLoad(asName);

		pImage = FindImage(asName, sPath);

		//debug:
		/*if(pImage)
		{
			pImage=NULL;
			sPath = mpFileSearcher->GetFilePath(asName);
		}*/

		if(!pImage)
		{
			if(sPath != _W(""))
			{
				cBitmap *pBmp = mpBitmapLoaderHandler->LoadBitmap(sPath, eBitmapLoadFlag_ForceNoCompression);
				if(pBmp==NULL){
					Error("Imagemanager Couldn't load bitmap '%s'\n", cString::To8Char(sPath).c_str());
					EndLoad();
					return NULL;
				}
				
				pImage = AddToFrame(pBmp, sPath, alFrameHandle);

				hplDelete(pBmp);

				if(pImage==NULL){
					Error("Imagemanager couldn't create image '%s'\n", asName.c_str());
				}
				
				if(pImage) AddResource(pImage);
 			}
		}
		else
		{
			//Log("Found '%s' in stock!\n",asName.c_str());
		}

		if(pImage)pImage->IncUserCount();
		else Error("Couldn't load image '%s'\n",asName.c_str());

		//Log("Loaded image %s, it has %d users!\n", pImage->GetName().c_str(),pImage->GetUserCount());
		//Log(" frame has %d pics\n", pImage->GetFrameTexture()->GetPicCount());
		
        EndLoad();
        return pImage;
	}
	
	//-----------------------------------------------------------------------

	cFrameSubImage* cImageManager::CreateImage(const tString& asName, int alFrameHandle)
	{
		iResourceBase* pRes = CreateInFrame(asName, alFrameHandle);
		cFrameSubImage* pImage = static_cast<cFrameSubImage*>(pRes);

		return pImage;
	}

	//-----------------------------------------------------------------------

	cFrameSubImage* cImageManager::CreateFromBitmap(const tString &asName,cBitmap* apBmp, int alFrameHandle)
	{
		if(mpRs) return H3Res<cFrameSubImage>(ohpl_h3f_image_from_bitmap(mpRs, H3Ops(), apBmp, alFrameHandle));
		if(apBmp==NULL)return NULL;

		cFrameSubImage *pImage = AddToFrame(apBmp,_W(""), alFrameHandle);

		if(pImage){
			AddResource(pImage, false);
			pImage->IncUserCount();
		}

		return pImage;
	}

	//-----------------------------------------------------------------------

	cFrameTexture* cImageManager::CreateCustomFrame(iTexture *apTexture)
	{
		if(mpRs) return static_cast<cFrameTexture*>(ohpl_h3f_image_custom_frame(mpRs, H3Ops(), apTexture));
		cFrameTexture *pTFrame = hplNew( cFrameTexture, (apTexture,mlFrameHandle,this,true) );
		
		m_mapTextureFrames.insert(tFrameTextureMap::value_type(mlFrameHandle, pTFrame));
		++mlFrameHandle;

		return pTFrame;
	}
	//-----------------------------------------------------------------------
	
	void cImageManager::Unload(iResourceBase* apResource)
	{

	}

	void cImageManager::Destroy(iResourceBase* apResource)
	{
		if(mpRs) { ohpl_h3f_image_destroy(mpRs, H3Ops(), apResource); return; }
		//Lower the user num for the the resource. If it is 0 then lower the
		//user num for the TextureFrame and delete the resource. If the Texture
		//frame reaches 0 it is deleted as well.
		cFrameSubImage *pImage = static_cast<cFrameSubImage*>(apResource);
		cFrameTexture *pFrame = pImage->GetFrameTexture();
		cFrameBitmap *pBmpFrame = pImage->GetFrameBitmap();

		//pImage->GetFrameBitmap()->FlushToTexture(); Not needed?
		
		
		//Log("  Users Before: %d\n",pImage->GetUserCount());
		//Log("  Framepics Before: %d\n",pFrame->GetPicCount());

		bool bDelete = ohpl_rm_destroy_standard(pImage->GetMeta());//dec frame count as well.. is that ok?
		
		//Log("---\n");
		//Log("  Destroyed Image: '%s' Users: %d\n",pImage->GetName().c_str(),pImage->GetUserCount());
		//Log("  Frame %d has left Pics: %d\n",pFrame,pFrame->GetPicCount());
		
		if(bDelete)
		{
			pFrame->DecPicCount(); // Doing it here now instead.
			if(pBmpFrame) pBmpFrame->DecPicCount();
			RemoveResource(apResource);
			hplDelete(pImage);
			
			//Log("  deleting image and dec frame to %d images!\n",pFrame->GetPicCount());
		}
		
		uint32_t lAfter = ohpl_rm_destroy_image_frame(pFrame->IsEmpty(), pFrame->IsCustom(), pBmpFrame != NULL);
		if(lAfter & 2)
		{
			//Log("  Deleting frame...\n");

			//Delete the bitmap frame that has this this frame.
			if(lAfter & 1)
			{
				for(tFrameBitmapListIt it=mlstBitmapFrames.begin();it!=mlstBitmapFrames.end();++it)
				{
					cFrameBitmap *pTestBmpFrame = *it;
					//Log(" %d vs %d\n", pBmpFrame, pTestBmpFrame);
					if(pTestBmpFrame->GetFrameTexture() == pFrame)
					{
						//Log("and bitmap...");
						//Log("   Destroying bmp frame %d", pTestBmpFrame);
						mlstBitmapFrames.erase(it);
						hplDelete(pTestBmpFrame);
						break;
					}
				}
			}
			

			//delete from list
			m_mapTextureFrames.erase(pFrame->GetHandle());
			hplDelete(pFrame);
			//Log("  Deleted frame!\n");
		}
		//Log("---\n");
		
	}

	//-----------------------------------------------------------------------

	void cImageManager::ReorganizeAll()
	{
		if(mpRs) { ohpl_h3f_image_reorganize_all(mpRs, H3Ops()); return; }
		for(tFrameBitmapListIt it=mlstBitmapFrames.begin();it!=mlstBitmapFrames.end();++it)
		{
			cFrameBitmap* pFrameBmp = *it;
			pFrameBmp->Reorganize();
			
		}
	}
	
	//-----------------------------------------------------------------------
	
	int cImageManager::FlushAll()
	{
		if(mpRs) return ohpl_h3f_image_flush_all(mpRs, H3Ops());
		//Log("Flushing...");
		int lNum =0;
		for(tFrameBitmapListIt it=mlstBitmapFrames.begin();it!=mlstBitmapFrames.end();++it)
		{
			if((*it)->FlushToTexture()) lNum++;
		}

		//Log("Done!\n");

		return lNum;
	}

	//-----------------------------------------------------------------------

	cFrameTexture* cImageManager::GetFrameTexture(int alHandle)
	{
		if(mpRs) return static_cast<cFrameTexture*>(ohpl_h3f_image_frame_texture(mpRs, H3Ops(), alHandle));
		tFrameTextureMapIt it = m_mapTextureFrames.find(alHandle);
		if(it == m_mapTextureFrames.end()) return NULL;
		
		return it->second;
	}

	//-----------------------------------------------------------------------

	int cImageManager::CreateFrame(cVector2l avSize)
	{
		if(mpRs) return ohpl_h3f_image_create_frame(mpRs, H3Ops(), avSize.x, avSize.y);
		cFrameBitmap *pBFrame = CreateBitmapFrame(avSize);
		
		if(pBFrame==NULL) return -1;
		
		return pBFrame->GetHandle();
	}
	
	//-----------------------------------------------------------------------

	void cImageManager::SetFrameLocked(int alHandle, bool abLocked)
	{
		if(mpRs) { ohpl_h3f_image_set_frame_locked(mpRs, H3Ops(), alHandle, abLocked); return; }
		tFrameBitmapListIt it = mlstBitmapFrames.begin();
		while(it != mlstBitmapFrames.end())
		{	
			if((*it)->GetHandle() == alHandle){
				(*it)->SetLocked(abLocked);
				break;
			}
			it++;
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cFrameSubImage *cImageManager::FindImage(const tString &asName, tWString &asFilePath)
	{
		cRmEnv env(mpFileSearcher, this);
		cRmExts exts(*mpBitmapLoaderHandler->GetSupportedTypes());
		cRmOut out;
		ohpl_rm_find_with_formats(env.raw(), RM_STR(asName), exts.data(), exts.size(), out.raw());
		asFilePath = out.msPath;
		return static_cast<cFrameSubImage*>(out.Res());
	}

	//-----------------------------------------------------------------------

	cFrameSubImage *cImageManager::AddToFrame(cBitmap *apBmp, const tWString& asFullPath, int alFrameHandle)
	{
		bool bFound = false;
		cFrameSubImage *pImage=NULL;

		if(mlstBitmapFrames.size()==0){
			CreateBitmapFrame(mvFrameSize);
		}
		
		if(alFrameHandle<0)
		{
			//Search the frames til one is find that fits the bitmap
			for(tFrameBitmapListIt it=mlstBitmapFrames.begin();it!=mlstBitmapFrames.end();it++)
			{
				cFrameBitmap * pFrame = *it;
				if(	pFrame->IsFull()==false && pFrame->IsLocked()==false)
				{
					pImage = pFrame->AddBitmap(apBmp, asFullPath, NULL);
					//if not fit, reorganize and see if that helps
					if(pImage==NULL && pFrame->GetAdditionsSinceReorganization() > 1)
					{	
						pFrame->Reorganize();
						pImage = pFrame->AddBitmap(apBmp, asFullPath, NULL);
					}
					if(pImage!=NULL)
					{
						bFound = true;
						break;
					}
				}
			}

			//If it fitted in none of the frames, create a new and put it in that.
			if(!bFound)
			{
				//Log("No fit!\n");
				//not 100% it fits in this one...if so maybe the bitmap size of the frame
				//should be changed? Nahh.. 510x510 is a good upper size
				
				//pImage = CreateBitmapFrame(mvFrameSize)->AddBitmap(apBmp);
				cVector2l vSize = mvFrameSize;
				ohpl_rm_image_frame_size(vSize.x, vSize.y, apBmp->GetWidth(), apBmp->GetHeight(), &vSize.x, &vSize.y);
				cFrameBitmap * pFrame = CreateBitmapFrame(vSize);
				if(pFrame)
				{
					pImage = pFrame->AddBitmap(apBmp, asFullPath, NULL);
					if(pImage==NULL)
					{
						Log("No fit in new frame!\n");
					}
				}
			}
		}
		else
		{
			tFrameBitmapListIt it = mlstBitmapFrames.begin();
			while(it != mlstBitmapFrames.end())
			{	
				if((*it)->GetHandle() == alFrameHandle)
				{
					pImage = (*it)->AddBitmap(apBmp, asFullPath, NULL);			
					break;
				}
				it++;
			}
			if(pImage==NULL)
				Error("Image didn't fit frame %d!\n", alFrameHandle);
		}

		return pImage;
	}

	//-----------------------------------------------------------------------
	
	cFrameBitmap *cImageManager::CreateBitmapFrame(cVector2l avSize)
	{
		iTexture *pTex = mpLowLevelGraphics->CreateTexture("ImageFrame",eTextureType_2D,eTextureUsage_Normal);
		cFrameTexture *pTFrame = hplNew( cFrameTexture, (pTex,mlFrameHandle,this,false) );
		cBitmap *pBmp = hplNew(cBitmap, () );
		pBmp->CreateData(cVector3l(avSize.x, avSize.y,1),ePixelFormat_RGBA,0,0);

		cFrameBitmap *pBFrame = hplNew(  cFrameBitmap, (pBmp,pTFrame,mlFrameHandle) );

		mlstBitmapFrames.push_back(pBFrame);
		
		std::pair<tFrameTextureMap::iterator, bool> ret = m_mapTextureFrames.insert(tFrameTextureMap::value_type(mlFrameHandle, pTFrame));
		if(ret.second == false)
		{
			Error("Could not add texture frame %d with handle %d! Handle already exist!\n",pTFrame, mlFrameHandle);
		}
		else
		{
			//Log("Added texture frame: %d\n",pTFrame);
		}

		mlFrameHandle++;
        return pBFrame;
	}


	//-----------------------------------------------------------------------

	void* cImageManager::H3Create(OhplH3Create &aCreate)
	{
		switch(aCreate.kind)
		{
		case OHPL_H3_IMAGE:
			return mpBitmapLoaderHandler->LoadBitmap(H3Wide(aCreate.path), eBitmapLoadFlag_ForceNoCompression);
		case OHPL_H3_IMG_BITMAP_FRAME:
		{
			iTexture *pTex = mpLowLevelGraphics->CreateTexture("ImageFrame",eTextureType_2D,eTextureUsage_Normal);
			cFrameTexture *pTFrame = hplNew( cFrameTexture, (pTex,aCreate.i[2],this,false) );
			cBitmap *pBmp = hplNew(cBitmap, () );
			pBmp->CreateData(cVector3l(aCreate.i[0], aCreate.i[1],1),ePixelFormat_RGBA,0,0);
			cFrameBitmap *pBFrame = hplNew(  cFrameBitmap, (pBmp,pTFrame,aCreate.i[2]) );
			return pBFrame;
		}
		case OHPL_H3_IMG_CUSTOM_FRAME:
			return hplNew( cFrameTexture, (static_cast<iTexture*>(aCreate.ptr),aCreate.i[0],this,true) );
		case OHPL_H3_IMG_ADD_BITMAP:
			return static_cast<iResourceBase*>(static_cast<cFrameBitmap*>(aCreate.ptr)->AddBitmap(static_cast<cBitmap*>(aCreate.ptr2), H3Wide(aCreate.path), NULL));
		}
		return NULL;
	}

	int64_t cImageManager::H3Call(int alWhat, void *apObj, int64_t alArg)
	{
		cFrameBitmap *pBmpFrame = static_cast<cFrameBitmap*>(apObj);
		cFrameTexture *pFrame = static_cast<cFrameTexture*>(apObj);
		switch(alWhat)
		{
		case OHPL_H3C_IMG_FRAME_TEX: return (int64_t)(intptr_t)H3Res<cFrameSubImage>(apObj)->GetFrameTexture();
		case OHPL_H3C_IMG_FRAME_BMP: return (int64_t)(intptr_t)H3Res<cFrameSubImage>(apObj)->GetFrameBitmap();
		case OHPL_H3C_FRAME_DEC_PIC: pFrame->DecPicCount(); return 0;
		case OHPL_H3C_BMPFRAME_DEC_PIC: pBmpFrame->DecPicCount(); return 0;
		case OHPL_H3C_FRAME_IS_EMPTY: return pFrame->IsEmpty();
		case OHPL_H3C_FRAME_IS_CUSTOM: return pFrame->IsCustom();
		case OHPL_H3C_BMPFRAME_HANDLE: return pBmpFrame->GetHandle();
		case OHPL_H3C_BMPFRAME_TEX: return (int64_t)(intptr_t)pBmpFrame->GetFrameTexture();
		case OHPL_H3C_BMPFRAME_IS_FULL: return pBmpFrame->IsFull();
		case OHPL_H3C_BMPFRAME_IS_LOCKED: return pBmpFrame->IsLocked();
		case OHPL_H3C_BMPFRAME_ADDITIONS: return pBmpFrame->GetAdditionsSinceReorganization();
		case OHPL_H3C_BMPFRAME_REORGANIZE: pBmpFrame->Reorganize(); return 0;
		case OHPL_H3C_BMPFRAME_FLUSH: return pBmpFrame->FlushToTexture();
		case OHPL_H3C_BMPFRAME_SET_LOCKED: pBmpFrame->SetLocked(alArg != 0); return 0;
		case OHPL_H3C_BMPFRAME_DELETE: hplDelete(pBmpFrame); return 0;
		case OHPL_H3C_FRAME_DELETE: hplDelete(pFrame); return 0;
		case OHPL_H3C_FRAME_HANDLE: return pFrame->GetHandle();
		case OHPL_H3C_BMP_WIDTH: return static_cast<cBitmap*>(apObj)->GetWidth();
		case OHPL_H3C_BMP_HEIGHT: return static_cast<cBitmap*>(apObj)->GetHeight();
		case OHPL_H3C_BMP_DELETE: hplDelete(static_cast<cBitmap*>(apObj)); return 0;
		}
		return 0;
	}
}
