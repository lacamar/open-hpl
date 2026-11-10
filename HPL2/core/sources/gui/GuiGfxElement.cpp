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

#include "gui/GuiGfxElement.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/ImageManager.h"
#include "graphics/FrameSubImage.h"
#include "graphics/FrameBitmap.h"

#include "graphics/Texture.h"

#include "gui/Gui.h"
#include "gui/RustGuiSkin.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// ANIMATION
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cGuiGfxAnimation::AddFrame(int alNum)
	{
		mvFrames.push_back(alNum);
	}
	void cGuiGfxAnimation::SetType(eGuiGfxAnimationType aType)
	{
		mType = aType;
	}
	void cGuiGfxAnimation::SetFrameLength(float afLength)
	{
		mfFrameLength = afLength;
	}
		
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cGuiGfxElement::cGuiGfxElement(cGui* apGui)
	{
		mpGui = apGui;

		mpResources = mpGui->GetResources();

		/////////////////////////////
		// Set up vertices
		mvVtx.resize(4);

		for(int i=0; i<4; ++i)
		{
			mvVtx[i].pos =0;
			mvVtx[i].tex =0;
			mvVtx[i].col = cColor(1,1);
		}

		//Position
		mvVtx[1].pos.x = 1;
		mvVtx[2].pos.x = 1;
		mvVtx[2].pos.y = 1;
		mvVtx[3].pos.y = 1;

		//Texture coordinate
		mvVtx[1].tex.x = 1;
		mvVtx[2].tex.x = 1;
		mvVtx[2].tex.y = 1;
		mvVtx[3].tex.y = 1;

		mvImageSize = 0;

		mvOffset =0;
		mvActiveSize =0;

		mbDestroyTexture = true;

		////////////////////////////
		//Set up textures
		for(int i=0; i<kMaxGuiTextures; ++i)
		{
			mvTextures[i] = NULL;
			mvImages[i] = NULL;
		}

		mlTextureNum =0;
		mlCurrentAnimation = 0;
		mfCurrentFrame =0;
		mbForwardAnim = true;
		mlActiveImage =0;
		mbAnimationPaused = false;

		mpMaterial = NULL;

		mbFlipUvYAxis = false;

		mbFlushed = false;

		mlImageUpdateCount = -1;
	}

	//---------------------------------------------------

	cGuiGfxElement::~cGuiGfxElement()
	{
		STLDeleteAll(mvAnimations);

		////////////////////////////////
		// Delete all textures / Images
		if(mvImageBufferVec.size()>0)
		{
			for(int i=0; i< (int)mvImageBufferVec.size(); ++i)
			{
				mpResources->GetImageManager()->Destroy(mvImageBufferVec[i]);
			}
		}
		else
		{
			for(int i=0; i<mlTextureNum; ++i)
			{
				if(mvImages[i])
				{
					mpResources->GetImageManager()->Destroy(mvImages[i]);
				}
				else if(mvTextures[i] && mbDestroyTexture)
				{
					mpResources->GetTextureManager()->Destroy(mvTextures[i]);
				}
			}
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cGuiGfxElement::Update(float afTimeStep)
	{
		OhplGuiAnimState state = {mfCurrentFrame, mlActiveImage, mbForwardAnim};
		OhplGuiAnimInput input = {};
		input.animationCount = mvAnimations.size();
		input.bufferLen = mvImageBufferVec.size();
		input.currentAnimation = mlCurrentAnimation;
		input.paused = mbAnimationPaused;
		input.timeStep = afTimeStep;
		if(mlCurrentAnimation >= 0 && (size_t)mlCurrentAnimation < mvAnimations.size())
		{
			input.type = mvAnimations[mlCurrentAnimation]->mType;
			input.frameLength = mvAnimations[mlCurrentAnimation]->mfFrameLength;
		}

		int64_t lImage = ohpl_gui_gfx_update(&state, &input);

		mfCurrentFrame = state.currentFrame;
		mlActiveImage = state.activeImage;
		mbForwardAnim = state.forward;

		if(lImage >= 0) SetImage(mvImageBufferVec[lImage],0);
	}
	
	//-----------------------------------------------------------------------
	
	void cGuiGfxElement::AddImage(cFrameSubImage* apImage)
	{
		SetImage(apImage, mlTextureNum);
		
		mvActiveSize = GetImageSize();

		++mlTextureNum;
	}

	//---------------------------------------------------

	void cGuiGfxElement::AddTexture(iTexture* apTexture, const cVector2f& avStartUV, const cVector2f& avEndUV)
	{
		mvTextures[mlTextureNum] = apTexture;

		if(mlTextureNum==0)
		{
			float vTex[8];
			float vSize[2];
			ohpl_gui_gfx_texture_uv(vTex, (float)apTexture->GetWidth(), (float)apTexture->GetHeight(),
									apTexture->GetType() == eTextureType_Rect, avStartUV.x, avStartUV.y, avEndUV.x, avEndUV.y, vSize);

			mvImageSize.x = vSize[0];
			mvImageSize.y = vSize[1];
			for(int i=0; i<4; ++i)
			{
				mvVtx[i].tex.x = vTex[i*2];
				mvVtx[i].tex.y = vTex[i*2+1];
			}

			if(mbFlipUvYAxis) SetFlipUvYAxis(mbFlipUvYAxis);
		}

		mvActiveSize = GetImageSize();

		++mlTextureNum;
	}

	//---------------------------------------------------

	void cGuiGfxElement::AddImageToBuffer(cFrameSubImage* apImage)
	{
		if(mvImageBufferVec.size()==0)
		{
			SetImage(apImage,0);
		}

		mvImageBufferVec.push_back(apImage);
	}

	//---------------------------------------------------

	cGuiGfxAnimation* cGuiGfxElement::CreateAnimtion(const tString& asName)
	{
		cGuiGfxAnimation *pAnimation = hplNew( cGuiGfxAnimation, () );
		pAnimation->msName = asName;

		mvAnimations.push_back(pAnimation);
        		
		return pAnimation;
	}

	//---------------------------------------------------

	void cGuiGfxElement::PlayAnimation(int alNum)
	{
		if(mlCurrentAnimation == alNum) return;

		mlCurrentAnimation = alNum;

		OhplGuiAnimState state = {mfCurrentFrame, mlActiveImage, mbForwardAnim};
		ohpl_gui_gfx_restart(&state);
		mfCurrentFrame = state.currentFrame;
		mlActiveImage = state.activeImage;
		mbForwardAnim = state.forward;

		if(mvImageBufferVec.empty()==false) SetImage(mvImageBufferVec[mlActiveImage],0);
	}

	void cGuiGfxElement::SetAnimationTime(float afTime)
	{
		bool bHasAnim = mlCurrentAnimation>=0 && (size_t)mlCurrentAnimation < mvAnimations.size();
		if(mlCurrentAnimation>=0 && bHasAnim==false) return;

		mfCurrentFrame = ohpl_gui_gfx_animation_time(afTime, bHasAnim, bHasAnim ? mvAnimations[mlCurrentAnimation]->mfFrameLength : 0);
	}

	//---------------------------------------------------

	void cGuiGfxElement::SetMaterial(iGuiMaterial *apMat)
	{
		mpMaterial = apMat;
	}

	//---------------------------------------------------

	void cGuiGfxElement::SetColor(const cColor &aColor)
	{
		for(int i=0; i<4; ++i)	mvVtx[i].col = aColor;
	}

	//-----------------------------------------------------------------------

	void cGuiGfxElement::SetFlipUvYAxis(bool abX)
	{
		mbFlipUvYAxis = abX;

		float vTex[8], vImageTex[8];
		for(int i=0; i<4; ++i)
		{
			vTex[i*2] = mvVtx[i].tex.x;
			vTex[i*2+1] = mvVtx[i].tex.y;
		}

		int lSource = 2;
		if(mvImages[0])
		{
			lSource = 0;
			const tVertexVec& vImageVtx = mvImages[0]->GetVertexVec();
			for(int i=0; i<4; ++i)
			{
				vImageTex[i*2] = vImageVtx[i].tex.x;
				vImageTex[i*2+1] = vImageVtx[i].tex.y;
			}
		}
		else if(mvTextures[0]) lSource = 1;

		ohpl_gui_gfx_flip_uv(vTex, abX, lSource, vImageTex);

		for(int i=0; i<4; ++i)
		{
			mvVtx[i].tex.x = vTex[i*2];
			mvVtx[i].tex.y = vTex[i*2+1];
		}
	}

	//-----------------------------------------------------------------------

	cVector2f cGuiGfxElement::GetImageSize()
	{
		return mvImageSize;
	}

	//-----------------------------------------------------------------------

	void cGuiGfxElement::Flush()
	{
		//Check if element uses images.
		cFrameSubImage *pMainImage = mvImages[0];
		if(pMainImage == NULL) return;

		//Flush all images
		for(int i=0; i<mlTextureNum; ++i)
		{
			cFrameSubImage *pImage = mvImages[i];
			if(pImage) pImage->Flush();

		}
		
		//Check if the image has been changed, if so update UV.
		if(ohpl_gui_gfx_flush_changed(&mlImageUpdateCount, pMainImage->GetUpdateCount()))
		{
			const tVertexVec& vImageVtx = pMainImage->GetVertexVec();
			for(int i=0; i<4; ++i) mvVtx[i].tex = vImageVtx[i].tex;	
			if(mbFlipUvYAxis) SetFlipUvYAxis(mbFlipUvYAxis);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cGuiGfxElement::SetImage(cFrameSubImage* apImage, int alNum)
	{
		//Set image and texture (for sorting)
		mvImages[alNum] = apImage;
		mvTextures[alNum] = apImage->GetTexture();

		//Get texture coords
		//Log("Image tex: '%s' Texture: %d\n", apImage->GetName().c_str(), mvTextures[alNum]);
		const tVertexVec& vImageVtx = apImage->GetVertexVec();
		for(int i=0; i<4; ++i) {
			mvVtx[i].tex = vImageVtx[i].tex;
			//Log("  (%s)", mvVtx[i].tex.ToString().c_str());
		}
		//Log("\n");

		if(alNum==0)
		{
			mvImageSize.x = (float)apImage->GetWidth();
			mvImageSize.y = (float)apImage->GetHeight();

			if(mbFlipUvYAxis) SetFlipUvYAxis(mbFlipUvYAxis);
		}
	}

	//-----------------------------------------------------------------------

}
