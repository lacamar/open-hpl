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

#include "graphics/GraphicsTypes.h"
#include "graphics/RustGraphics.h"

#include "graphics/Texture.h"
#include "graphics/FrameBuffer.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// GLOBAL FUNCTIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	tVertexElementFlag GetVertexElementFlagFromEnum(eVertexBufferElement aElement) { return ohpl_gfx_vertex_element_flag(aElement); }
	int GetVertexFormatByteSize(eVertexBufferElementFormat aFormat) { return ohpl_gfx_vertex_format_byte_size(aFormat); }
	int GetVertexElementTextureUnit(eVertexBufferElement aElement) { return ohpl_gfx_vertex_element_texture_unit(aElement); }
	int GetChannelsInPixelFormat(ePixelFormat aFormat) { return ohpl_gfx_channels(aFormat); }
	int GetBytesPerPixel(ePixelFormat aFormat) { return ohpl_gfx_bytes_per_pixel(aFormat); }
	bool PixelFormatIsCompressed(ePixelFormat aFormat) { return ohpl_gfx_is_compressed(aFormat); }
	bool PixelFormatIsDepth(ePixelFormat aFormat) { return ohpl_gfx_is_depth(aFormat); }
	bool PixelFormatIsFloatingPoint(ePixelFormat aFormat) { return ohpl_gfx_is_floating_point(aFormat); }

	//////////////////////////////////////////////////////////////////////////
	// VIDEO MODE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVideoMode::cVideoMode(const cVector2l& avScreenSize, int alBitsPerPixel, int alRefreshRate) : mvScreenSize(avScreenSize),
																								   mlBitsPerPixel(alBitsPerPixel),
																								   mlRefreshRate(alRefreshRate),
																								   mlDisplay(0),
																								   mbCustom(false)
	{
	}

	cVideoMode::cVideoMode(int alDisplay, const cVector2l& avScreenSize, int alBitsPerPixel, int alRefreshRate) : mvScreenSize(avScreenSize),
																												  mlBitsPerPixel(alBitsPerPixel),
																												  mlRefreshRate(alRefreshRate),
																												  mlDisplay(alDisplay),
																												  mbCustom(false)
	{
	}

	cVideoMode::cVideoMode(const cVector2l& avScreenSize) : mvScreenSize(avScreenSize),
															mlBitsPerPixel(-1),
															mlRefreshRate(-1),
															mlDisplay(0),
															mbCustom(true)
	{
	}

	cVideoMode::cVideoMode() : mvScreenSize(cVector2l(-1,-1)),
							   mlBitsPerPixel(-1),
							   mlRefreshRate(-1),
							   mlDisplay(0),
							   mbCustom(true)
	{
	}

	bool cVideoMode::operator==(const hpl::cVideoMode &o) const
	{
		return mlDisplay == o.mlDisplay && mvScreenSize == o.mvScreenSize;
	}

	//-----------------------------------------------------------------------	

	//////////////////////////////////////////////////////////////////////////
	// FRAME BUFFER ATTACHMENT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	iTexture* iFrameBufferAttachment::ToTexture()
	{
		if(GetFrameBufferAttachmentType() != eFrameBufferAttachment_Texture) return NULL;

		return static_cast<iTexture*>(this);
	}

	//-----------------------------------------------------------------------

	iDepthStencilBuffer* iFrameBufferAttachment::ToDepthStencilBuffer()
	{
		if(GetFrameBufferAttachmentType() != eFrameBufferAttachment_RenderBuffer) return NULL;

		return static_cast<iDepthStencilBuffer*>(this);
	}
	
	//-----------------------------------------------------------------------

}
