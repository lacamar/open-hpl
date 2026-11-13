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

#include "impl/SDLTexture.h"

#include "system/LowLevelSystem.h"
#include "math/Math.h"
#include "graphics/Bitmap.h"

#include "impl/LowLevelGraphicsSDL.h"

#include <cmath>
#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSDLTexture::cSDLTexture(const tString& asName, eTextureType aType, eTextureUsage aUsage, iLowLevelGraphics* apLowLevelGraphics)
				: iTexture(asName,_W(""),aType, aUsage, apLowLevelGraphics)
	{
				
		mpGfxSDL = static_cast<cLowLevelGraphicsSDL*>(mpLowLevelGraphics);

		mlRs = ohpl_gltex_new();

		mlTextureIndex = 0;
		mClock.count = 0;
		mClock.dir = 1;
	}

	cSDLTexture::~cSDLTexture()
	{
		ohpl_gltex_free(mlRs);
	}


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cSDLTexture::CreateFromBitmap(cBitmap* apBmp)
	{
		;

		GenerateHandles(1);

        return CreateFromBitmapToIndex(apBmp,0);
	}

	//-----------------------------------------------------------------------

	bool cSDLTexture::CreateAnimFromBitmapVec(std::vector<cBitmap*> *avBitmaps)
	{
		;

		GenerateHandles((int)avBitmaps->size());

		//////////////////////////////////
		//Create textures
		for(size_t i=0; i< HandleCount(); ++i)
		{
			if(CreateFromBitmapToIndex( (*avBitmaps)[i], (int)i)==false)
			{
				return false;
			}
		}

		return true;
	}

	//-----------------------------------------------------------------------

	bool cSDLTexture::CreateCubeFromBitmapVec(std::vector<cBitmap*> *avBitmaps)
	{
		;

		/////////////////////////////
		//Check so everything is correct
		if(mUsage == eTextureUsage_RenderTarget || mType != eTextureType_CubeMap)
		{
			return false;
		}

		if(avBitmaps->size()<6){
			Error("Only %d bitmaps supplied for creation of cube map, 6 needed.",avBitmaps->size());
			return false;
		}

		GenerateHandles(1);

		OhplTexFace vFaces[6];
		for(int i=0; i<6; ++i)
		{
			cBitmap *pBmp = (*avBitmaps)[i];
			OhplTexFace f = { reinterpret_cast<const OhplTexLevel*>(pBmp->GetData(0,0)), pBmp->GetNumOfMipMaps(), pBmp->GetSize(), (int)pBmp->GetPixelFormat() };
			vFaces[i] = f;
		}
		if(CreateFacesRs(0, vFaces, 6) == false) return false;
		SetupPropertiesRs(0);
		return true;
	}


	//-----------------------------------------------------------------------

	bool cSDLTexture::CreateFromRawData(const cVector3l &avSize,ePixelFormat aPixelFormat, unsigned char *apData)
	{
		OhplTexIo io;
		LoadIo(io);
		bool bRs = ohpl_gltex_create_raw(mlRs, &io, OhplGl(avSize), aPixelFormat, apData);
		StoreIo(io);
		if(bRs == false) return false;
		SetupPropertiesRs(0);
		ohpl_gltex_create_raw_end(mlRs, &io);
		return true;
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetRawData(	int alLevel, const cVector3l& avOffset, const cVector3l& avSize, 
									ePixelFormat aPixelFormat, void *apData)
	{
		;

		OhplTexIo io;
		LoadIo(io);
		ohpl_gltex_set_raw_data(mlRs, &io, alLevel, OhplGl(avOffset), OhplGl(avSize), aPixelFormat, apData);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::Update(float afTimeStep)
	{
		ohpl_gl_clock_update(&mClock, HandleCount(), afTimeStep, mfFrameTime, mAnimMode);
	}

	//-----------------------------------------------------------------------

	bool cSDLTexture::HasAnimation()
	{
		return HandleCount() > 1;
	}
	
	void cSDLTexture::NextFrame()
	{
		ohpl_gl_clock_next(&mClock, HandleCount(), mAnimMode);
	}

	void cSDLTexture::PrevFrame()
	{
		ohpl_gl_clock_prev(&mClock, HandleCount(), mAnimMode);
	}
	
	float cSDLTexture::GetT()
	{
		return cMath::Modulus(mClock.count,1.0f);
	}

	float cSDLTexture::GetTimeCount()
	{
		return mClock.count;
	}
	void cSDLTexture::SetTimeCount(float afX)
	{
		mClock.count = afX;
	}
	int cSDLTexture::GetCurrentLowlevelHandle()
	{
		return GetTextureHandle();
	}

	//-----------------------------------------------------------------------

	bool cSDLTexture::GetRawPixelsRGBAFloat(std::vector<float> &avOut)
	{
		unsigned int lHandle = GetTextureHandle();
		if(lHandle == 0) return false;

		OhplTexIo io;
		LoadIo(io);
		avOut.resize(mvSize.x * mvSize.y * 4);
		bool bDepth = mPixelFormat == ePixelFormat_Depth16 || mPixelFormat == ePixelFormat_Depth24 || mPixelFormat == ePixelFormat_Depth32;
		return ohpl_gltex_read_pixels(mlRs, &io, mClock.count, bDepth, avOut.data());
	}

	void cSDLTexture::SetFilter(eTextureFilter aFilter)
	{
		OhplTexIo io;
		LoadIo(io);
		ohpl_gltex_set_filter(mlRs, &io, aFilter);
		StoreIo(io);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetAnisotropyDegree(float afX)
	{
		if(ContainsData()==false) return;
		if(!mpLowLevelGraphics->GetCaps(eGraphicCaps_AnisotropicFiltering)) return;
		if(afX < 1.0f) return;
		if(!ohpl_gl_tex_aniso_apply(true, true, mpLowLevelGraphics->GetCaps(eGraphicCaps_MaxAnisotropicFiltering), afX, mfAnisotropyDegree)) return;

		mfAnisotropyDegree = afX;

		OhplTexIo io;
		LoadIo(io);
		ohpl_gltex_apply_aniso(mlRs, &io, afX);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetWrapS(eTextureWrap aMode)
	{
		if(mType == eTextureType_Rect) return; //Rect only has one mode! (clamp to edge)

		WrapRs(1, aMode);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetWrapT(eTextureWrap aMode)
	{
		if(mType == eTextureType_Rect) return; //Rect only has one mode! (clamp to edge)

		WrapRs(2, aMode);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetWrapR(eTextureWrap aMode)
	{
		if(mType == eTextureType_Rect) return; //Rect only has one mode! (clamp to edge)

		WrapRs(4, aMode);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetWrapSTR(eTextureWrap aMode)
	{
		if(mType == eTextureType_Rect) return; //Rect only has one mode! (clamp to edge)

		;

		WrapRs(7, aMode);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetCompareMode(eTextureCompareMode aMode)
	{
		CompareRs(false, aMode);
	}
	
	
	void cSDLTexture::SetCompareFunc(eTextureCompareFunc aFunc)
	{
		CompareRs(true, aFunc);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetRedFromAlpha(bool abX)
	{
		OhplTexIo io;
		LoadIo(io);
		ohpl_gltex_set_red_from_alpha(mlRs, &io, abX);
	}

	void cSDLTexture::AutoGenerateMipmaps()
	{
		if(mbUseMipMaps==false) return;

		OhplTexIo io;
		LoadIo(io);
		ohpl_gltex_auto_mipmaps(mlRs, &io);
	}
	
	//-----------------------------------------------------------------------

	unsigned int cSDLTexture::GetTextureHandle()
	{
		return ohpl_gltex_handle(mlRs, mClock.count);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	size_t cSDLTexture::HandleCount()
	{
		return ohpl_gltex_handle_count(mlRs);
	}

	bool cSDLTexture::ContainsData()
	{
		return ohpl_gltex_contains_data(mlRs);
	}

	void cSDLTexture::LoadIo(OhplTexIo &aIo)
	{
		aIo.type = mType; aIo.usage = mUsage; aIo.filter = mFilter;
		aIo.wrap[0] = mWrapS; aIo.wrap[1] = mWrapT; aIo.wrap[2] = mWrapR;
		aIo.compareMode = mCompareMode; aIo.compareFunc = mCompareFunc;
		aIo.animMode = mAnimMode; aIo.pixelFormat = mPixelFormat;
		aIo.downLevel = mlSizeDownScaleLevel; aIo.memory = mlMemorySize;
		aIo.size = mvSize; aIo.minSize = mvMinDownScaleSize;
		aIo.frameTime = mfFrameTime; aIo.aniso = mfAnisotropyDegree;
		aIo.useMips = mbUseMipMaps; aIo.srgb = mbsRGB; aIo.compressed = mbIsCompressed;
	}

	void cSDLTexture::StoreIo(const OhplTexIo &aIo)
	{
		mFilter = (eTextureFilter)aIo.filter;
		mWrapS = (eTextureWrap)aIo.wrap[0]; mWrapT = (eTextureWrap)aIo.wrap[1]; mWrapR = (eTextureWrap)aIo.wrap[2];
		mCompareMode = (eTextureCompareMode)aIo.compareMode; mCompareFunc = (eTextureCompareFunc)aIo.compareFunc;
		mPixelFormat = (ePixelFormat)aIo.pixelFormat;
		mlMemorySize = aIo.memory; mvSize = aIo.size;
		mbUseMipMaps = aIo.useMips; mbIsCompressed = aIo.compressed;
	}

	void cSDLTexture::WrapRs(unsigned alMask, eTextureWrap aMode)
	{
		OhplTexIo io;
		LoadIo(io);
		ohpl_gltex_set_wrap(mlRs, &io, alMask, aMode);
		StoreIo(io);
	}

	void cSDLTexture::CompareRs(bool abFunc, int alValue)
	{
		OhplTexIo io;
		LoadIo(io);
		ohpl_gltex_set_compare(mlRs, &io, abFunc, alValue);
		StoreIo(io);
	}

	bool cSDLTexture::CreateFacesRs(int alIdx, OhplTexFace *apFaces, int alNum)
	{
		OhplTexIo io;
		LoadIo(io);
		OhplTexRes res = {0};
		bool bRet = ohpl_gltex_create_faces(mlRs, &io, alIdx, apFaces, alNum, &res);
		StoreIo(io);
		for(int i=0; i<res.warnPow2; ++i)
		{
			Warning("Texture '%s' does not have a pow2 size!\n",msName.c_str());
		}
		if(bRet == false) Error("Could not create GL texture %s\n",msName.c_str());
		return bRet;
	}

	void cSDLTexture::SetupPropertiesRs(int alIdx)
	{
		OhplTexIo io;
		LoadIo(io);
		int lState = ohpl_gltex_props_begin(mlRs, &io, alIdx);
		StoreIo(io);
		if(lState == 0) return;
		if(lState == 1 &&
			mpLowLevelGraphics->GetCaps(eGraphicCaps_AnisotropicFiltering) &&
			ohpl_gl_tex_aniso_fits(mpLowLevelGraphics->GetCaps(eGraphicCaps_MaxAnisotropicFiltering), mfAnisotropyDegree))
		{
			ohpl_gltex_props_aniso(mlRs, &io);
		}
		ohpl_gltex_props_end(mlRs, &io);
	}

	void cSDLTexture::GenerateHandles(int alNumOfHandles)
	{
		ohpl_gltex_generate_handles(mlRs, alNumOfHandles);
	}
	
	//-----------------------------------------------------------------------

	bool cSDLTexture::CreateFromBitmapToIndex(cBitmap* apBmp, int alIdx)
	{
		////////////////////////////
		//Create Cubemap texture
		if(mType == eTextureType_CubeMap)
		{
			//Check so there are at least 6 images.
			if(apBmp->GetNumOfImages()<6)
			{
				Error("Bitmap has to few images (%d) to create cubmap texture '%s'\n", apBmp->GetNumOfImages(),msName.c_str());
				return false;
			}

			OhplTexFace vFaces[6];
			for(int i=0; i<6; ++i)
			{
				OhplTexFace f = { reinterpret_cast<const OhplTexLevel*>(apBmp->GetData(i,0)), apBmp->GetNumOfMipMaps(), apBmp->GetSize(), (int)apBmp->GetPixelFormat() };
				vFaces[i] = f;
			}
			if(CreateFacesRs(alIdx, vFaces, 6) == false) return false;
			SetupPropertiesRs(alIdx);
			return true;
		}
		////////////////////////////
		//Create 1d, 2d or 3d texture
		OhplTexFace f = { reinterpret_cast<const OhplTexLevel*>(apBmp->GetData(0,0)), apBmp->GetNumOfMipMaps(), apBmp->GetSize(), (int)apBmp->GetPixelFormat() };
		if(CreateFacesRs(alIdx, &f, 1) == false) return false;

		SetupPropertiesRs(alIdx);

		return true;
	}

	//-----------------------------------------------------------------------

}
