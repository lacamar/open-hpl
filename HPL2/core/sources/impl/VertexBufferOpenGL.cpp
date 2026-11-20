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

#include "impl/VertexBufferOpenGL.h"
#include "graphics/LowLevelGraphics.h"

namespace hpl {

	iVertexBufferOpenGL::~iVertexBufferOpenGL()
	{
		ohpl_vb_free(mlRs);
	}

	cBoundingVolume iVertexBufferOpenGL::CreateBoundingVolume()
	{
		cBoundingVolume bv;
		int lCheck = ohpl_vb_bounding_check(mlRs);
		if(lCheck == 1)
		{
			Warning("Could not create bounding volume from buffer %d  because no position element was present!\n",this);
			return bv;
		}
		if(lCheck == 2)
		{
			Warning("Could not breate bounding volume since postion was not for format float in buffer %d!\n",this);
			return bv;
		}
		bv.AddArrayPoints(GetFloatArray(eVertexBufferElement_Position), GetVertexNum());
		bv.CreateFromPoints(GetElementNum(eVertexBufferElement_Position));
		return bv;
	}

	iVertexBuffer* iVertexBufferOpenGL::CreateCopy(	eVertexBufferType aType, eVertexBufferUsageType aUsageType,
													tVertexElementFlag alVtxToCopy)
	{
		iVertexBufferOpenGL *pVtxBuff = static_cast<iVertexBufferOpenGL*>(
			mpLowLevelGraphics->CreateVertexBuffer(aType, mDrawType, aUsageType, GetIndexNum(), GetVertexNum()));
		pVtxBuff->mVertexFlags |= ohpl_vb_copy_into(mlRs, pVtxBuff->mlRs, alVtxToCopy);
		pVtxBuff->Compile(0);
		return pVtxBuff;
	}

}
