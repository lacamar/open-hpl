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

#ifndef HPL_FRAMETEXTURE_H
#define HPL_FRAMETEXTURE_H

#include "graphics/FrameSubImage.h"
#include "resources/RustFrames.h"

namespace hpl {

	class iTexture;

	// Opaque: a Rust-owned frame texture; the pointer is its address.
	class cFrameTexture
	{
	public:
		cFrameTexture() = delete;

		iTexture* GetTexture(){ return static_cast<iTexture*>(ohpl_fr_ftex_texture(this)); }

		/**
		* Creates a subimage in frame at pos and with size messured in pixel. Only works on custom frames!
		*/
		cFrameSubImage* CreateCustomImage(const cVector2l& avPixelPos,const cVector2l& avPixelSize)
		{
			return static_cast<cFrameSubImage*>(static_cast<iResourceBase*>(ohpl_fr_ftex_custom_image(this, &avPixelPos, &avPixelSize)));
		}
	};

};
#endif // HPL_FRAMETEXTURE_H
