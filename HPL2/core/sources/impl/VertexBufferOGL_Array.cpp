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

#include "impl/VertexBufferOGL_Array.h"
#include "system/EngineDiagnostics.h"

#include "system/LowLevelSystem.h"
#include "math/Math.h"

#include "impl/LowLevelGraphicsSDL.h"
#include "impl/GlOps.h"

#include <GL/glew.h>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVertexBufferOGL_Array::cVertexBufferOGL_Array(	iLowLevelGraphics* apLowLevelGraphics,
										eVertexBufferDrawType aDrawType,eVertexBufferUsageType aUsageType,
										int alReserveVtxSize,int alReserveIdxSize) :
	iVertexBufferOpenGL(apLowLevelGraphics,eVertexBufferType_Software, aDrawType,aUsageType, alReserveVtxSize, alReserveIdxSize)
	{
		mlRs = ohpl_glvb_new(false);
	}

	cVertexBufferOGL_Array::~cVertexBufferOGL_Array()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_Array::UpdateData(tVertexElementFlag aTypes, bool abIndices)
	{
		
	}
	//-----------------------------------------------------------------------

	void cVertexBufferOGL_Array::Draw(eVertexBufferDrawType aDrawType)
	{
		cEngineDiagnostics::CountDrawCall();
		ohpl_glvb_draw(mlRs, mDrawType, aDrawType, mlElementNum, GetIndexNum(), &mvIndexArray[0]);
	}

	void cVertexBufferOGL_Array::DrawIndices(	unsigned int *apIndices, int alCount,
												eVertexBufferDrawType aDrawType)
	{
		cEngineDiagnostics::CountDrawCall();
		ohpl_glvb_draw_indices(mDrawType, aDrawType, apIndices, alCount);
	}


	//-----------------------------------------------------------------------

	void cVertexBufferOGL_Array::Bind()
	{
		;

		SetVertexStates();
	}

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_Array::UnBind()
	{
		ohpl_glvb_unbind(mlRs);
	}

	//-----------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	/////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_Array::CompileSpecific()
	{
	}

	//-----------------------------------------------------------------------

	iVertexBufferOpenGL* cVertexBufferOGL_Array::CreateDataCopy(tVertexElementFlag aFlags, eVertexBufferDrawType aDrawType,
																eVertexBufferUsageType aUsageType,
																int alReserveVtxSize,int alReserveIdxSize)
	{
		return hplNew(cVertexBufferOGL_Array, (mpLowLevelGraphics,aDrawType,aUsageType,alReserveVtxSize,alReserveIdxSize));
	}

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_Array::SetVertexStates()
	{
		const void *vRsPtrs[eVertexBufferElement_LastEnum];
		for(size_t i=0; i<mvElementArrays.size(); ++i) vRsPtrs[i] = mvElementArrays[i]->GetArrayPtr();
		ohpl_glvb_bind(mlRs, vRsPtrs);
	}

	//-----------------------------------------------------------------------

}
