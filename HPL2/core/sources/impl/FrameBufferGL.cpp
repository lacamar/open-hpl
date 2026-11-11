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

#include "impl/FrameBufferGL.h"

#include "system/LowLevelSystem.h"
#include "math/Math.h"

#include "impl/SDLTexture.h"
#include "impl/LowLevelGraphicsSDL.h"
#include "impl/RustGl.h"



namespace hpl {
	

	//////////////////////////////////////////////////////////////////////////
	// DEPTH AND STENCIL BUFFER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cDepthStencilBufferGL::cDepthStencilBufferGL(const cVector2l& avSize, int alDepthBits, int alStencilBits)
		: iDepthStencilBuffer(avSize,alDepthBits,alStencilBits)
	{
		mlHandle =0;

		;
		
		OhplGlDsPlan plan;
		ohpl_gl_fbo_plan_depth_stencil(alDepthBits, alStencilBits, GLEW_EXT_packed_depth_stencil, &plan);

		switch(plan.kind)
		{
		case kOhplGlDsErrNoPacked:
			Error("Drivers does not support packed depth and stencil!\n");
			return;
		case kOhplGlDsErrPackedBits:
			Error("Depth-stencil has %d depth bits and %d stencil bits! Only a combination of 24 depth bits and 8 stencil bits is supported!\n",
					mlDepthBits, mlStencilBits);
			return;
		case kOhplGlDsErrDepth:
			Error("Invalid depth format: %d bits! Could not create depth buffer\n",alDepthBits);
			break;
		case kOhplGlDsErrStencil:
			Error("Invalid stencil format: %d bits! Could not create stencil buffer\n",alStencilBits);
			break;
		case kOhplGlDsPacked:
		case kOhplGlDsDepth:
		case kOhplGlDsStencil:
			glGenRenderbuffersEXT(1, &mlHandle);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, mlHandle);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, plan.format, avSize.x, avSize.y);
			break;
		}
	}

	//-----------------------------------------------------------------------
	
	cDepthStencilBufferGL::~cDepthStencilBufferGL()
	{
		;
		if(mlHandle != 0)	glDeleteRenderbuffersEXT(1, &mlHandle);
	}
	
	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cFrameBufferGL::cFrameBufferGL(const tString& asName, iLowLevelGraphics* apLowLevelGraphics) : iFrameBuffer(asName, apLowLevelGraphics)
	{
		;
		glGenFramebuffersEXT(1, &mlHandle);

		mbIsUpdated = true;
		mbExplicitMipLevel = false;
	}

	//-----------------------------------------------------------------------

	cFrameBufferGL::~cFrameBufferGL()
	{
		;
		glDeleteFramebuffersEXT(1, &mlHandle);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cFrameBufferGL::SetTexture2D(int alColorIdx, iTexture *apTexture, int alMipmapLevel)
	{
		if(CheckIfNullTexture(GL_COLOR_ATTACHMENT0_EXT,alColorIdx,apTexture)) return;

		if(!ohpl_gl_fbo_entry_accepts(0, apTexture->GetType())) return;

		AttachTexture(GL_COLOR_ATTACHMENT0_EXT,alColorIdx,apTexture,alMipmapLevel,0);
	}

	//-----------------------------------------------------------------------

	void cFrameBufferGL::SetTexture3D(int alColorIdx, iTexture *apTexture, int alZ, int alMipmapLevel)
	{
		if(CheckIfNullTexture(GL_COLOR_ATTACHMENT0_EXT,alColorIdx,apTexture)) return;

		if(!ohpl_gl_fbo_entry_accepts(1, apTexture->GetType())) return;

		AttachTexture(GL_COLOR_ATTACHMENT0_EXT,alColorIdx,apTexture,alMipmapLevel,alZ);
	}

	//-----------------------------------------------------------------------

	void cFrameBufferGL::SetTextureCubeMap(int alColorIdx, iTexture *apTexture, int alFace, int alMipmapLevel)
	{
		if(CheckIfNullTexture(GL_COLOR_ATTACHMENT0_EXT,alColorIdx,apTexture)) return;

		if(!ohpl_gl_fbo_entry_accepts(2, apTexture->GetType())) return;

		AttachTexture(GL_COLOR_ATTACHMENT0_EXT,alColorIdx,apTexture,alMipmapLevel,alFace);
	}

	//-----------------------------------------------------------------------

	void cFrameBufferGL::SetDepthTexture2D(iTexture *apTexture, int alMipmapLevel)
	{
		if(CheckIfNullTexture(GL_DEPTH_ATTACHMENT_EXT,0,apTexture)) return;

		if(!ohpl_gl_fbo_entry_accepts(0, apTexture->GetType())) return;
		//TODO: Check so it is a depth texture.
		
		AttachTexture(GL_DEPTH_ATTACHMENT_EXT,0,apTexture,alMipmapLevel,0);
	}
	
	void cFrameBufferGL::SetDepthTextureCubeMap(iTexture *apTexture, int alFace, int alMipmapLevel)
	{
		if(CheckIfNullTexture(GL_DEPTH_ATTACHMENT_EXT,0,apTexture)) return;

		if(!ohpl_gl_fbo_entry_accepts(2, apTexture->GetType())) return;
		//TODO: Check so it is a depth texture.

		AttachTexture(GL_DEPTH_ATTACHMENT_EXT,0,apTexture,alMipmapLevel,alFace);
	}

	//-----------------------------------------------------------------------

	void cFrameBufferGL::SetDepthStencilBuffer(iDepthStencilBuffer* apBuffer)
	{
		;

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mlHandle);

		////////////////////////////
		//Check if a NULL value was passed, if so remove attachments
		if(apBuffer == NULL)
		{
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,0);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,0);
			return;
		}

		cDepthStencilBufferGL *pBufferSDL = static_cast<cDepthStencilBufferGL*>(apBuffer);

		//////////////////////////////////
		//Attach depth buffer
		if(pBufferSDL->GetDepthBits() > 0)
		{
			glFramebufferRenderbufferEXT(	GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, 
											GL_RENDERBUFFER_EXT, pBufferSDL->GetHandle());

			mpDepthBuffer = apBuffer;
		}

		//////////////////////////////////
		//Attach stencil buffer
		if(pBufferSDL->GetStencilBits() > 0)
		{
			glFramebufferRenderbufferEXT(	GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, 
											GL_RENDERBUFFER_EXT, pBufferSDL->GetHandle());
			mpStencilBuffer = apBuffer;
		}

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		mbIsUpdated = true;

		//If this is the first thing set to the frame buffer, use that size.
		SetFirstSize(apBuffer->GetSize());
	}

	//-----------------------------------------------------------------------

	bool cFrameBufferGL::CompileAndValidate()
	{
		mbIsUpdated = true;

		bool bRet = false;

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mlHandle);

		unsigned int lOccupied = 0;
		for(int i=0; i<kMaxDrawColorBuffers; ++i)
		{
			if(mpColorBuffer[i] != NULL) lOccupied |= 1u << i;
		}

		OhplGlDrawPlan plan;
		ohpl_gl_fbo_plan_draw_buffers(lOccupied, &plan);

		switch(plan.mode)
		{
		case kOhplGlDrawNone:
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
			break;
		case kOhplGlDrawSingle:
			glDrawBuffer(plan.buffers[0]);
			glReadBuffer(plan.buffers[0]);
			break;
		default:
			glDrawBuffers(plan.count, plan.buffers);
			break;
		}

		GLenum GLStatus = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		switch(ohpl_gl_fbo_status_class(GLStatus))
		{
		case kOhplGlStatusComplete:		bRet = true; break;
		case kOhplGlStatusUnsupported:	Error("Frame buffer '%s' is not complete! Format is unsupported!\n", msName.c_str()); break;
		case kOhplGlStatusAttachment:	Error("Frame buffer '%s' is not complete! Incomplete attachments!\n", msName.c_str()); break;
		case kOhplGlStatusMissing:		Error("Frame buffer '%s' is not complete! Missing attachments!\n", msName.c_str()); break;
		case kOhplGlStatusDimensions:	Error("Frame buffer '%s' is not complete! Incomplete dimensions!\n", msName.c_str()); break;
		case kOhplGlStatusFormats:		Error("Frame buffer '%s' is not complete! Incomplete formats!\n", msName.c_str()); break;
		case kOhplGlStatusDrawBuffer:	Error("Frame buffer '%s' is not complete! Incomplete draw buffer!\n", msName.c_str()); break;
		case kOhplGlStatusReadBuffer:	Error("Frame buffer '%s' is not complete! Incomplete read buffer!\n", msName.c_str()); break;
		default:						Error("Frame buffer '%s' is not complete! Unknown error!\n", msName.c_str()); break;
		}

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		return bRet;
	}

	//-----------------------------------------------------------------------

	void cFrameBufferGL::PostBindUpdate()
	{
		if(mbExplicitMipLevel) return;
		for(int i=0; i<kMaxDrawColorBuffers; ++i)
		{
			if(mpColorBuffer[i]) PostBindUpdateAttachment(mpColorBuffer[i]);
		}
        if(mpDepthBuffer) PostBindUpdateAttachment(mpDepthBuffer);
		if(mpStencilBuffer) PostBindUpdateAttachment(mpStencilBuffer);
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cFrameBufferGL::PostBindUpdateAttachment(iFrameBufferAttachment *apAttachment)
	{
		if(apAttachment->GetFrameBufferAttachmentType() == eFrameBufferAttachment_RenderBuffer) return;

		iTexture *pTexture = static_cast<iTexture*>(apAttachment);
		if(pTexture->UsesMipMaps())
			pTexture->AutoGenerateMipmaps();
	}

	//-----------------------------------------------------------------------

	bool cFrameBufferGL::CheckIfNullTexture(int alAttachmentType, int alAttachmentIdx,iTexture *apTexture)
	{
		if(apTexture) return false;

		;

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mlHandle);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, alAttachmentType + alAttachmentIdx,GL_TEXTURE_2D, 0,0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		
		return true;
	}

	//-----------------------------------------------------------------------
	void cFrameBufferGL::AttachTexture(int alAttachmentType,int alAttachmentIdx,iTexture *apTexture, int alMipmapLevel, int alExtra)
	{
		cVector3l vTexSize = apTexture->GetSize();
		OhplGlAttach at;
		ohpl_gl_fbo_plan_attach(alAttachmentType, alAttachmentIdx, apTexture->GetType(), alMipmapLevel, alExtra,
								OhplGlV2{vTexSize.x, vTexSize.y}, &at);

		if(at.explicitMip) mbExplicitMipLevel = true;

		cSDLTexture *pTextureSDL = static_cast<cSDLTexture*>(apTexture);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mlHandle);

		if(at.call == kOhplGlCallTexture2D)
		{
			glFramebufferTexture2DEXT(	GL_FRAMEBUFFER_EXT, at.attachment, at.target,
										pTextureSDL->GetTextureHandle(),alMipmapLevel);
		}
		else if(at.call == kOhplGlCallTexture3D)
		{
			glFramebufferTexture3DEXT(	GL_FRAMEBUFFER_EXT, at.attachment, at.target,
										pTextureSDL->GetTextureHandle(),alMipmapLevel,
										alExtra);
		}

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		switch(at.slot)
		{
		case kOhplGlSlotColor:
			if(at.colorIndex >= 0) mpColorBuffer[at.colorIndex] = apTexture;
			break;
		case kOhplGlSlotDepth:
			mpDepthBuffer = apTexture;
			break;
		case kOhplGlSlotStencil:
			mpStencilBuffer = apTexture;
			break;
		}

		SetFirstSize(at.firstSize);

		mbIsUpdated = true;
	}

	//-----------------------------------------------------------------------

	void cFrameBufferGL::SetFirstSize(const cVector2l &avSize)
	{	
		if(mvSize.x > -1) return;

        mvSize = avSize;
	}
	
	//-----------------------------------------------------------------------
}
