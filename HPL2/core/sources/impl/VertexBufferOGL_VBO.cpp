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

#include "impl/VertexBufferOGL_VBO.h"
#include "system/EngineDiagnostics.h"

#include "system/LowLevelSystem.h"
#include "math/Math.h"

#include "impl/LowLevelGraphicsSDL.h"
#include "impl/GlOps.h"

#include <memory.h>

#include <GL/glew.h>


namespace hpl {

#define BUFFER_OFFSET(i) ((void*)(i*sizeof(float)))

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVertexBufferOGL_VBO::cVertexBufferOGL_VBO(iLowLevelGraphics* apLowLevelGraphics,
												eVertexBufferDrawType aDrawType,eVertexBufferUsageType aUsageType,
												int alReserveVtxSize,int alReserveIdxSize) :
	iVertexBufferOpenGL(apLowLevelGraphics,eVertexBufferType_Hardware,  aDrawType,aUsageType, alReserveVtxSize, alReserveIdxSize)
	{
		mlRs = ohpl_glvb_new(true);
	}

	//-----------------------------------------------------------------------

	cVertexBufferOGL_VBO::~cVertexBufferOGL_VBO()
	{
		ohpl_glvb_free(mlRs);
		mlRs = 0;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::UpdateData(tVertexElementFlag aTypes, bool abIndices)
	{
		if(ohpl_glvb_needs_compile(mlRs, mvIndexArray.size())) CompileSpecific();

		OhplVbData vData[eVertexBufferElement_LastEnum];
		BuildVbData(vData, aTypes);
		ohpl_glvb_update(mlRs, mUsageType, aTypes, abIndices, vData, abIndices ? &mvIndexArray[0] : NULL, GetIndexNum());
	}
	
	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::Draw(eVertexBufferDrawType aDrawType)
	{
		cEngineDiagnostics::CountDrawCall();
		ohpl_glvb_draw(mlRs, mDrawType, aDrawType, mlElementNum, GetIndexNum(), NULL);
	}

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::DrawIndices(unsigned int *apIndices, int alCount,eVertexBufferDrawType aDrawType)
	{
		cEngineDiagnostics::CountDrawCall();
		ohpl_glvb_draw_indices(mDrawType, aDrawType, apIndices, alCount);
	}


	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::Bind()
	{
		;

		if(ohpl_glvb_needs_compile(mlRs, mvIndexArray.size())) CompileSpecific();

		SetVertexStates();
	}

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::UnBind()
	{
		ohpl_glvb_unbind(mlRs);
	}

	//-----------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	/////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::CompileSpecific()
	{
		OhplVbData vData[eVertexBufferElement_LastEnum];
		BuildVbData(vData, ~0u);
		ohpl_glvb_compile(mlRs, mUsageType, vData, &mvIndexArray[0], GetIndexNum());
	}

	//-----------------------------------------------------------------------

	iVertexBufferOpenGL* cVertexBufferOGL_VBO::CreateDataCopy(tVertexElementFlag aFlags, eVertexBufferDrawType aDrawType,
																eVertexBufferUsageType aUsageType,
																int alReserveVtxSize,int alReserveIdxSize)
	{
		return hplNew(cVertexBufferOGL_VBO, (mpLowLevelGraphics,aDrawType,aUsageType,alReserveVtxSize,alReserveIdxSize));
	}

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::SetVertexStates()
	{
		ohpl_glvb_bind(mlRs, NULL);
	}

	//-----------------------------------------------------------------------

}
