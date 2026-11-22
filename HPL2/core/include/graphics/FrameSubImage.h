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

#ifndef HPL_RESOURCE_IMAGE_H
#define HPL_RESOURCE_IMAGE_H

#include <vector>

#include "resources/ResourceBase.h"
#include "resources/RustFrames.h"

#include "math/MathTypes.h"

namespace hpl {

	class iTexture;

	//----------------------------------------------------

	// Shell: the state lives in Rust (OhplFrSub prefix is read in place).
	class cFrameSubImage : public iResourceBase
	{
	public:
		cFrameSubImage(const tString& asName,const tWString& asFullPath, ::OhplFrSub *apRs)
			: iResourceBase(asName, asFullPath, 0), mpRs(apRs) {}
		~cFrameSubImage();

		bool Reload(){ return false; }
		void Unload(){}
		void Destroy(){}

		int GetHeight()const{return mpRs->rect.h;}
		int GetWidth()const{return mpRs->rect.w;}
		cVector2l GetSize()const{return cVector2l(mpRs->rect.w,mpRs->rect.h);}
		cVector2l GetPosition()const{return cVector2l(mpRs->rect.x,mpRs->rect.y);}

		int GetSourceWidth()const{return mpRs->src.x;}
		int GetSourceHeight()const{return mpRs->src.y;}

		iTexture *GetTexture()const{ return static_cast<iTexture*>(ohpl_fr_sub_texture(mpRs)); }

		int GetUpdateCount(){ return mpRs->updateCount;}

		void Flush(){ ohpl_fr_sub_flush(mpRs); }

		const OhplFrSub* Rs()const{ return mpRs; }
		OhplFrSub* Rs(){ return mpRs; }

	private:
		::OhplFrSub *mpRs;
	};

	typedef std::vector<cFrameSubImage*> tResourceImageVec;
	typedef tResourceImageVec::iterator tResourceImageVecIt;

};

#endif // HPL_RESOURCE_IMAGE_H
