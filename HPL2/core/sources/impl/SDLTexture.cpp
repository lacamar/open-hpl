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
		mbContainsData = false;
				
		mpGfxSDL = static_cast<cLowLevelGraphicsSDL*>(mpLowLevelGraphics);

		mlTextureIndex = 0;
		mClock.count = 0;
		mClock.dir = 1;
	}

	cSDLTexture::~cSDLTexture()
	{
		;

		for(size_t i=0; i<mvTextureHandles.size(); ++i)
		{
			glDeleteTextures(1,(GLuint *)&mvTextureHandles[i]);
		}
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
		for(size_t i=0; i< mvTextureHandles.size(); ++i)
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

		/////////////////////////////
		//Create the faces on the cube
		for(int i=0; i< 6; i++)
		{
			cBitmap *pBmp = (*avBitmaps)[i];

			bool bRet = CreateTexture(	mvTextureHandles[0],
										pBmp->GetData(0,0), pBmp->GetNumOfMipMaps(),
										pBmp->GetSize(), pBmp->GetPixelFormat(),i,
										mbUseMipMaps,true);
			if(bRet==false) return false;
		}

		SetupProperties(mvTextureHandles[0]);

		return true;
	}


	//-----------------------------------------------------------------------

	bool cSDLTexture::CreateFromRawData(const cVector3l &avSize,ePixelFormat aPixelFormat, unsigned char *apData)
	{
		GenerateHandles(1);

		int lDataSize;
		OhplGlV3 vRaw = ohpl_gl_tex_raw_size(OhplGl(avSize), aPixelFormat, &lDataSize);
		mvSize = cVector3l(vRaw.x, vRaw.y, vRaw.z);
		mPixelFormat = aPixelFormat;

		GLenum GLTarget = TextureTypeToGLTarget(mType);

		glEnable(GLTarget);
		glBindTexture(GLTarget, mvTextureHandles[0]);

		bool bRet = CopyTextureDataToGL(mvTextureHandles[0],0,apData, lDataSize, avSize, aPixelFormat,0);
		if(bRet==false)
		{
			glDisable(GLTarget);
			return false;
		}

		mbContainsData = true;

		if(mbUseMipMaps)
		{
			GenerateMipMaps(GLTarget,aPixelFormat,avSize,apData,lDataSize,0);
		}

		SetupProperties(mvTextureHandles[0]);

		glDisable(GLTarget);

		return true;
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetRawData(	int alLevel, const cVector3l& avOffset, const cVector3l& avSize, 
									ePixelFormat aPixelFormat, void *apData)
	{
		;

		if(mbContainsData==false) return;
		if(apData==NULL) return;

		GLenum GLTarget = TextureTypeToGLTarget(mType);
		GLenum GLFormat = PixelFormatToGLFormat(aPixelFormat);

		glEnable(GLTarget);
		glBindTexture(GLTarget, mvTextureHandles[0]);

		if(mType == eTextureType_1D)
		{
			glTexSubImage1D(GLTarget,alLevel,avOffset.x, avSize.x,
							GLFormat,GL_UNSIGNED_BYTE,apData);
		}
		else if(mType == eTextureType_2D || mType == eTextureType_Rect)
		{
			glTexSubImage2D(GLTarget,alLevel,avOffset.x,avOffset.y, avSize.x,avSize.y,
							GLFormat,GL_UNSIGNED_BYTE,apData);
		}
		else if(mType == eTextureType_3D)
		{
			glTexSubImage3D(GLTarget,alLevel,avOffset.x,avOffset.y,avOffset.z, 
							avSize.x,avSize.y,avSize.z,
							GLFormat,GL_UNSIGNED_BYTE,apData);
		}

		glDisable(GLTarget);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::Update(float afTimeStep)
	{
		ohpl_gl_clock_update(&mClock, mvTextureHandles.size(), afTimeStep, mfFrameTime, mAnimMode);
	}

	//-----------------------------------------------------------------------

	bool cSDLTexture::HasAnimation()
	{
		return mvTextureHandles.size() > 1;
	}
	
	void cSDLTexture::NextFrame()
	{
		ohpl_gl_clock_next(&mClock, mvTextureHandles.size(), mAnimMode);
	}

	void cSDLTexture::PrevFrame()
	{
		ohpl_gl_clock_prev(&mClock, mvTextureHandles.size(), mAnimMode);
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

		GLenum GLTarget = GetGLTextureTargetEnum(mType);

		avOut.resize(mvSize.x * mvSize.y * 4);

		glBindTexture(GLTarget, lHandle);
		if(mPixelFormat == ePixelFormat_Depth16 || mPixelFormat == ePixelFormat_Depth24 || mPixelFormat == ePixelFormat_Depth32)
		{
			std::vector<float> vDepth(mvSize.x * mvSize.y);
			glGetTexImage(GLTarget, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &vDepth[0]);
			for(size_t i=0; i<vDepth.size(); ++i) { avOut[i*4] = avOut[i*4+1] = avOut[i*4+2] = vDepth[i]; avOut[i*4+3] = 1; }
		}
		else
			glGetTexImage(GLTarget, 0, GL_RGBA, GL_FLOAT, &avOut[0]);
		glBindTexture(GLTarget, 0);

		return true;
	}

	void cSDLTexture::SetFilter(eTextureFilter aFilter)
	{
		if(mFilter == aFilter) return;
		mFilter = aFilter;

		if(mbContainsData)
		{
			GLenum GLTarget = GetGLTextureTargetEnum(mType);
			unsigned int vFilter[2];
			ohpl_gl_tex_filter_params(mFilter, mbUseMipMaps, mType, vFilter);

			glEnable(GLTarget);
			for(size_t i=0; i < mvTextureHandles.size(); ++i)
			{
				glBindTexture(GLTarget, mvTextureHandles[i]);
				if(vFilter[0])
				{
					glTexParameteri(GLTarget, GL_TEXTURE_MIN_FILTER, vFilter[0]);
					glTexParameteri(GLTarget, GL_TEXTURE_MAG_FILTER, vFilter[1]);
				}
			}
			glDisable(GLTarget);
		}
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetAnisotropyDegree(float afX)
	{
		if(mbContainsData==false) return;
		if(!mpLowLevelGraphics->GetCaps(eGraphicCaps_AnisotropicFiltering)) return;
		if(afX < 1.0f) return;
		if(!ohpl_gl_tex_aniso_apply(true, true, mpLowLevelGraphics->GetCaps(eGraphicCaps_MaxAnisotropicFiltering), afX, mfAnisotropyDegree)) return;

		mfAnisotropyDegree = afX;

		GLenum GLTarget = GetGLTextureTargetEnum(mType);

		glEnable(GLTarget);
		for(size_t i=0; i < mvTextureHandles.size(); ++i)
		{
			glBindTexture(GLTarget, mvTextureHandles[i]);
			glTexParameterf(GLTarget,GL_TEXTURE_MAX_ANISOTROPY_EXT ,mfAnisotropyDegree);
		}
		glDisable(GLTarget);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetWrapS(eTextureWrap aMode)
	{
		if(mType == eTextureType_Rect) return; //Rect only has one mode! (clamp to edge)

		;

		mWrapS = aMode;

		if(mbContainsData==false) return;
		
		GLenum GLTarget = GetGLTextureTargetEnum(mType);

		glEnable(GLTarget);
		for(size_t i=0; i < mvTextureHandles.size(); ++i)
		{
			glBindTexture(GLTarget, mvTextureHandles[i]);

			glTexParameteri(GLTarget,GL_TEXTURE_WRAP_S,GetGLWrapEnum(aMode));
		}

		glDisable(GLTarget);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetWrapT(eTextureWrap aMode)
	{
		if(mType == eTextureType_Rect) return; //Rect only has one mode! (clamp to edge)

		;

		mWrapT = aMode;

		if(mbContainsData==false) return;
		
		GLenum GLTarget = GetGLTextureTargetEnum(mType);

		glEnable(GLTarget);
		for(size_t i=0; i < mvTextureHandles.size(); ++i)
		{
			glBindTexture(GLTarget, mvTextureHandles[i]);

			glTexParameteri(GLTarget,GL_TEXTURE_WRAP_T,GetGLWrapEnum(aMode));
		}

		glDisable(GLTarget);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetWrapR(eTextureWrap aMode)
	{
		if(mType == eTextureType_Rect) return; //Rect only has one mode! (clamp to edge)

		;

		mWrapR = aMode;

		if(mbContainsData==false) return;
		
		GLenum GLTarget = GetGLTextureTargetEnum(mType);

		glEnable(GLTarget);
		for(size_t i=0; i < mvTextureHandles.size(); ++i)
		{
			glBindTexture(GLTarget, mvTextureHandles[i]);

			glTexParameteri(GLTarget,GL_TEXTURE_WRAP_R,GetGLWrapEnum(aMode));
		}

		glDisable(GLTarget);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetWrapSTR(eTextureWrap aMode)
	{
		if(mType == eTextureType_Rect) return; //Rect only has one mode! (clamp to edge)

		;

		mWrapS = aMode;
		mWrapT = aMode;
		mWrapR = aMode;

		if(mbContainsData==false) return;

		GLenum GLTarget = GetGLTextureTargetEnum(mType);

		glEnable(GLTarget);
		for(size_t i=0; i < mvTextureHandles.size(); ++i)
		{
			glBindTexture(GLTarget, mvTextureHandles[i]);

			glTexParameteri(GLTarget,GL_TEXTURE_WRAP_S,GetGLWrapEnum(aMode));
			glTexParameteri(GLTarget,GL_TEXTURE_WRAP_T,GetGLWrapEnum(aMode));
			glTexParameteri(GLTarget,GL_TEXTURE_WRAP_R,GetGLWrapEnum(aMode));
		}

		glDisable(GLTarget);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetCompareMode(eTextureCompareMode aMode)
	{
		mCompareMode = aMode;

		if(mbContainsData==false) return;

		;

		GLenum GLTarget = GetGLTextureTargetEnum(mType);
		GLenum GLCompareMode = GetGLTextureCompareMode(mCompareMode);
		
		glEnable(GLTarget);
		for(size_t i=0; i < mvTextureHandles.size(); ++i)
		{
			glBindTexture(GLTarget, mvTextureHandles[i]);

			glTexParameteri(GLTarget,GL_TEXTURE_COMPARE_MODE,GLCompareMode);			
		}
		glDisable(GLTarget);
	}
	
	
	void cSDLTexture::SetCompareFunc(eTextureCompareFunc aFunc)
	{
		mCompareFunc = aFunc;

		if(mbContainsData==false) return;

		;

		GLenum GLTarget = GetGLTextureTargetEnum(mType);
		GLenum GLCompareFunc = GetGLTextureCompareFunc(mCompareFunc);

		glEnable(GLTarget);
		for(size_t i=0; i < mvTextureHandles.size(); ++i)
		{
			glBindTexture(GLTarget, mvTextureHandles[i]);

			glTexParameteri(GLTarget,GL_TEXTURE_COMPARE_FUNC,GLCompareFunc);			
		}
		glDisable(GLTarget);
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetRedFromAlpha(bool abX)
	{
		if(mbContainsData==false || mType != eTextureType_2D) return;

		GLint lPrevBound = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &lPrevBound);
		for(size_t i=0; i < mvTextureHandles.size(); ++i)
		{
			glBindTexture(GL_TEXTURE_2D, mvTextureHandles[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, abX ? GL_ALPHA : GL_RED);
		}
		glBindTexture(GL_TEXTURE_2D, lPrevBound);
	}

	void cSDLTexture::AutoGenerateMipmaps()
	{
		if(mbUseMipMaps==false) return;

		;

		if(GLEW_EXT_framebuffer_object)
		{
			GLenum GLTarget = GetGLTextureTargetEnum(mType);

			GLenum GLBinding = ohpl_gl_texture_binding(GLTarget);
			GLint lPrevBound = 0;
			glGetIntegerv(GLBinding, &lPrevBound);
			glEnable(GLTarget);
			for(size_t i=0; i < mvTextureHandles.size(); ++i)
			{
				glBindTexture(GLTarget, mvTextureHandles[i]);
				glGenerateMipmapEXT(GLTarget);
			}

			glDisable(GLTarget);
			glBindTexture(GLTarget, lPrevBound);
		}
	}
	
	//-----------------------------------------------------------------------

	unsigned int cSDLTexture::GetTextureHandle()
	{
		int64_t lSlot = ohpl_gl_tex_handle_slot(mbContainsData, mvTextureHandles.size(), mClock.count);
		if(lSlot < 0) return 0;
		return mvTextureHandles[lSlot];
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cSDLTexture::GenerateHandles(int alNumOfHandles)
	{
		if( (int)mvTextureHandles.size() < alNumOfHandles)
		{
			int lNewHandles = alNumOfHandles - (int)mvTextureHandles.size();
			mvTextureHandles.resize(alNumOfHandles);
			glGenTextures(lNewHandles,(GLuint *)&mvTextureHandles[alNumOfHandles-lNewHandles]);
		}
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

			for(int i=0; i<6; ++i)
			{
				bool bRet = CreateTexture(	mvTextureHandles[alIdx],
											apBmp->GetData(i,0),apBmp->GetNumOfMipMaps(),
											apBmp->GetSize(), apBmp->GetPixelFormat(),
											i,mbUseMipMaps,	true);
				if(bRet == false) return false;
			}


		}
		////////////////////////////
		//Create 1d, 2d or 3d texture
		else
		{
			bool bRet = CreateTexture(	mvTextureHandles[alIdx],
										apBmp->GetData(0,0),apBmp->GetNumOfMipMaps(),
										apBmp->GetSize(), apBmp->GetPixelFormat(),
										0,mbUseMipMaps,true);
			if(bRet == false) return false;
			
		}

		SetupProperties(mvTextureHandles[alIdx]);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cSDLTexture::CreateTexture(	int alTextureHandle,
										cBitmapData* apBitmapImage, int alNumOfMipMaps,
										const cVector3l avSize, ePixelFormat aPixelFormat,
										int alFaceNum,bool abGenerateMipMaps,
										bool abCheckForResize)
	{
		OhplGlCreateIn in = { (int)mType, (int)aPixelFormat, avSize, mvMinDownScaleSize, alNumOfMipMaps,
							  (unsigned)mlSizeDownScaleLevel, abGenerateMipMaps, abCheckForResize };
		OhplGlCreatePlan plan;
		ohpl_gl_tex_plan_create(&in, &plan);

		if(plan.warnPow2)
		{
			Warning("Texture '%s' does not have a pow2 size!\n",msName.c_str());
		}

		mvSize = plan.size;
		mPixelFormat = aPixelFormat;
		mbIsCompressed = plan.compressed;

		GLenum GLTarget = TextureTypeToGLTarget(mType);
		GLenum GLFormat = PixelFormatToGLFormat(aPixelFormat);

		unsigned char *pResizeData = NULL;
		int lResizeDataSize = 0;
		if(plan.resize)
		{
			lResizeDataSize = plan.resizeBytes;
			pResizeData = hplNewArray(unsigned char, lResizeDataSize);

			gluScaleImage(	GLFormat,avSize.x, avSize.y, GL_UNSIGNED_BYTE, apBitmapImage->mpData,
							plan.size.x, plan.size.y,GL_UNSIGNED_BYTE,pResizeData);
		}

		glEnable(GLTarget);
		glBindTexture(GLTarget, alTextureHandle);

		bool bRet = true;
		int lMipMapCount = plan.endLevel;
		for(int i=plan.startLevel; i<lMipMapCount; ++i)
		{
			int lCount = i-plan.startLevel;
			OhplGlV3 vLevel = ohpl_gl_tex_level_size(OhplGl(plan.size), lCount);
			cVector3l vSize(vLevel.x, vLevel.y, vLevel.z);

			unsigned char *pData = apBitmapImage[i].mpData;
			int lSize = apBitmapImage[i].mlSize;

			if(lCount==0 && pResizeData)
			{
				pData = pResizeData;
				lSize = lResizeDataSize;
			}

			if(CopyTextureDataToGL(	alTextureHandle,lCount,pData,lSize, vSize,aPixelFormat,alFaceNum) == false)
			{
				bRet = false;
				break;
			}
		}

		if(bRet==false)
		{
			glDisable(GLTarget);
			Error("Could not create GL texture %s\n",msName.c_str());
			return false;
		}

		mbContainsData = true;

		if(plan.maxLevel >= 0)
			glTexParameteri(GLTarget, GL_TEXTURE_MAX_LEVEL, plan.maxLevel);

		if(plan.buildMips)
		{
			if(pResizeData)
				GenerateMipMaps(GLTarget,aPixelFormat,mvSize,pResizeData,lResizeDataSize,alFaceNum);
			else
				GenerateMipMaps(GLTarget,aPixelFormat,mvSize,apBitmapImage->mpData,apBitmapImage->mlSize,alFaceNum);
		}

		if(pResizeData) hplDeleteArray(pResizeData);

		glDisable(GLTarget);

		return true;
	}

	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------

	void cSDLTexture::GenerateMipMaps(	GLenum aGLTarget, ePixelFormat aPixelFormat,const cVector3l avSize,
										unsigned char *apData,int alDataSize, int alFaceNum)
	{
		OhplGlMipPlan plan;
		ohpl_gl_tex_plan_mips(mType, mUsage, GLEW_EXT_framebuffer_object, mbsRGB, aPixelFormat, OhplGl(avSize), alFaceNum, &plan);

		switch(plan.action)
		{
		case kOhplGlMipGenerate:
			glGenerateMipmapEXT(aGLTarget);
			break;
		case kOhplGlMipBuild1D:
			gluBuild1DMipmaps(	aGLTarget,plan.internal,avSize.x,
								plan.format, GL_UNSIGNED_BYTE,
								apData);
			break;
		case kOhplGlMipBuild2D:
			gluBuild2DMipmaps(	plan.target,
								plan.internal,avSize.x,avSize.y,
								plan.format, GL_UNSIGNED_BYTE,
								apData);
			break;
		}

		mlMemorySize += plan.memory;
		if(plan.disableMips) mbUseMipMaps = false;
	}

	//-----------------------------------------------------------------------

	bool cSDLTexture::CopyTextureDataToGL(	int alTextureHandle, int alLevel,unsigned char *apData,int alDataSize,
											const cVector3l avSize, ePixelFormat aPixelFormat,int alFaceNum)
	{
		OhplGlUpload up;
		ohpl_gl_tex_plan_upload(mType, mbsRGB, aPixelFormat, alLevel, alDataSize, OhplGl(avSize), alFaceNum, &up);

		while(glGetError()!=GL_NO_ERROR);

		mlMemorySize += alDataSize;

		switch(up.kind)
		{
		case 1:
			glCompressedTexImage1DARB(up.target, up.level, up.internal, up.size.x, 0, up.bytes, apData);
			break;
		case 2:
			glCompressedTexImage2DARB(up.target, up.level, up.internal, up.size.x, up.size.y, 0, up.bytes, apData);
			if(up.swizzle)
			{
				// SOMA uploads these as LATC2
				GLint vSwizzle[4] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
				glTexParameteriv(TextureTypeToGLTarget(mType), GL_TEXTURE_SWIZZLE_RGBA, vSwizzle);
			}
			break;
		case 3:
			glCompressedTexImage3DARB(up.target, up.level, up.internal, up.size.x, up.size.y, up.size.z, 0, up.bytes, apData);
			break;
		case 4:
			glTexImage1D(up.target, up.level, up.internal, up.size.x, 0, up.format, up.glType, apData);
			break;
		case 5:
			glTexImage2D(up.target, up.level, up.internal, up.size.x, up.size.y, 0, up.format, up.glType, apData);
			break;
		case 6:
			glTexImage3D(up.target, up.level, up.internal, up.size.x, up.size.y, up.size.z, 0, up.format, up.glType, apData);
			break;
		}

		if(glGetError()!=GL_NO_ERROR) return false;

		return true;
	}

	//-----------------------------------------------------------------------

	void cSDLTexture::SetupProperties(int alTextureHandle)
	{
		if(mbContainsData==false) return;

		GLenum GLTarget = TextureTypeToGLTarget(mType);

		glEnable(GLTarget);
		glBindTexture(GLTarget, alTextureHandle);

		int vWrap[3] = { (int)mWrapS, (int)mWrapT, (int)mWrapR };
		OhplGlProps props;
		ohpl_gl_tex_plan_props(mType, mbUseMipMaps, mFilter, vWrap, &props);

		glTexParameteri(GLTarget, GL_TEXTURE_MIN_FILTER, props.minFilter);
		glTexParameteri(GLTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if(props.rect)
		{
			mWrapS = eTextureWrap_ClampToEdge;
			mWrapT = eTextureWrap_ClampToEdge;
			mWrapR = eTextureWrap_ClampToEdge;
		}

		glTexParameteri(GLTarget,GL_TEXTURE_WRAP_S,props.wrap[0]);
		glTexParameteri(GLTarget,GL_TEXTURE_WRAP_T,props.wrap[1]);
		glTexParameteri(GLTarget,GL_TEXTURE_WRAP_R,props.wrap[2]);

		if(props.rect == false)
		{
			if(	mpLowLevelGraphics->GetCaps(eGraphicCaps_AnisotropicFiltering) &&
				ohpl_gl_tex_aniso_fits(mpLowLevelGraphics->GetCaps(eGraphicCaps_MaxAnisotropicFiltering), mfAnisotropyDegree))
			{
				glTexParameterf(GLTarget,GL_TEXTURE_MAX_ANISOTROPY_EXT ,mfAnisotropyDegree);
			}
		}

		glDisable(GLTarget);
	}

	//-----------------------------------------------------------------------

	unsigned char* cSDLTexture::ResizePixelData(unsigned char *apData, int alBytesPerPixel)
	{
		if(mlSizeDownScaleLevel<=0 || mvSize.x <= mvMinDownScaleSize.x) return NULL;

		unsigned char *pNewSrc = NULL;
		
		int lOldW = mvSize.x;
		int lOldH = mvSize.y;

		int lSizeDiv = (int)powf((int)2,(int)mlSizeDownScaleLevel);

		mvSize.x /= lSizeDiv;
		mvSize.y /= lSizeDiv;

		while(mvSize.x < mvMinDownScaleSize.x)
		{
			mvSize.x*=2;
			mvSize.y*=2;
			lSizeDiv/=2;
		}

		pNewSrc = hplNewArray( unsigned char, alBytesPerPixel * mvSize.x * mvSize.y);

		int lWidthCount = mvSize.x;
		int lHeightCount = mvSize.y;
		int lOldAdd = alBytesPerPixel*lSizeDiv;
		int lOldHeightAdd = alBytesPerPixel*lOldW*(lSizeDiv-1);

		unsigned char *pOldPixel = apData;
		unsigned char *pNewPixel = pNewSrc;

		while(lHeightCount)
		{
			memcpy(pNewPixel, pOldPixel,alBytesPerPixel);

			pOldPixel += lOldAdd;
			pNewPixel += alBytesPerPixel;

			lWidthCount--;
			if(!lWidthCount)
			{
				lWidthCount = mvSize.x;
				lHeightCount--;
				pOldPixel += lOldHeightAdd;
			}
		}

		return pNewSrc;
	}

	//-----------------------------------------------------------------------
}
