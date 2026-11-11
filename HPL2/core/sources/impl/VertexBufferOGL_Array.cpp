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
		GLenum mode = ohpl_gl_vb_draw_mode(mDrawType, aDrawType);

		int lSize = ohpl_gl_vb_draw_count(mlElementNum, GetIndexNum());

		cEngineDiagnostics::CountDrawCall();
		glDrawElements(mode,lSize,GL_UNSIGNED_INT, &mvIndexArray[0]);
	}

	void cVertexBufferOGL_Array::DrawIndices(	unsigned int *apIndices, int alCount,
												eVertexBufferDrawType aDrawType)
	{
		GLenum mode = ohpl_gl_vb_draw_mode(mDrawType, aDrawType);

		cEngineDiagnostics::CountDrawCall();
		glDrawElements(mode, alCount, GL_UNSIGNED_INT, apIndices);
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
		OhplGlOp vOps[kOhplGlMaxOps];
		size_t lCount = ohpl_gl_vb_unbind(mvElemDesc, mvElementArrays.size(), vOps);
		OhplGlOpSources src = {NULL, NULL, NULL};
		RunGlOps(vOps, lCount, src);
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
		OhplGlOp vOps[kOhplGlMaxOps];
		size_t lCount = ohpl_gl_vb_bind_array(mvElemDesc, mvElementArrays.size(), vOps);

		const void *vPtrs[eVertexBufferElement_LastEnum];
		for(size_t i=0; i<mvElementArrays.size(); ++i) vPtrs[i] = mvElementArrays[i]->GetArrayPtr();

		OhplGlOpSources src = {vPtrs, NULL, NULL};
		RunGlOps(vOps, lCount, src);
	}

	//-----------------------------------------------------------------------

}
