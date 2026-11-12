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
#include "impl/RustBackends.h"

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
		mlElementHandle =0;
		mlRs = OhplBackendRustRequested() ? ohpl_glvb_new(true) : 0;
	}

	//-----------------------------------------------------------------------

	cVertexBufferOGL_VBO::~cVertexBufferOGL_VBO()
	{
		if(mlRs)
		{
			ohpl_glvb_free(mlRs);
			mlRs = 0;
			return;
		}

		for(size_t i=0; i<mvElementArrays.size(); ++i)
		{
			cVtxBufferGLElementArray *pElement = mvElementArrays[i];
			
			glDeleteBuffersARB(1,(GLuint *)&pElement->mlGLHandle);
		}

		glDeleteBuffersARB(1,(GLuint *)&mlElementHandle);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::UpdateData(tVertexElementFlag aTypes, bool abIndices)
	{
		if(mlRs)
		{
			if(ohpl_glvb_needs_compile(mlRs, mvIndexArray.size())) CompileSpecific();

			OhplVbData vData[eVertexBufferElement_LastEnum];
			BuildVbData(vData, aTypes);
			ohpl_glvb_update(mlRs, mUsageType, aTypes, abIndices, vData, abIndices ? &mvIndexArray[0] : NULL, GetIndexNum());
			return;
		}

		if(mlElementHandle == 0 && mvIndexArray.empty()==false) CompileSpecific();

		GLenum usageType = ohpl_gl_usage(mUsageType);

		//Create the VBO vertex arrays
		for(size_t i=0; i<mvElementArrays.size(); ++i)
		{
			cVtxBufferGLElementArray *pElement = mvElementArrays[i];

			if( (aTypes & pElement->mFlag) && pElement->mlGLHandle >0)
			{
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, pElement->mlGLHandle);


				int lSize = (int)pElement->Size() * GetVertexFormatByteSize(pElement->mFormat);
				glBufferDataARB(GL_ARRAY_BUFFER_ARB, lSize,	NULL, usageType);
				glBufferDataARB(GL_ARRAY_BUFFER_ARB, lSize, pElement->GetArrayPtr(), usageType);
			}
		}
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		//Create the VBO index array
		if(abIndices)
		{
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,mlElementHandle);

			//glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,GetIndexNum()*sizeof(unsigned int),
			//	NULL, usageType);

			glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GetIndexNum()*sizeof(unsigned int),
				&mvIndexArray[0], usageType);

			//TODO: Same as with vertex, for stream and dynamic.

			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
		}
	}
	
	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::Draw(eVertexBufferDrawType aDrawType)
	{
		if(mlRs)
		{
			cEngineDiagnostics::CountDrawCall();
			ohpl_glvb_draw(mlRs, mDrawType, aDrawType, mlElementNum, GetIndexNum(), NULL);
			return;
		}

		GLenum mode = ohpl_gl_vb_draw_mode(mDrawType, aDrawType);

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,mlElementHandle);

		int lSize = ohpl_gl_vb_draw_count(mlElementNum, GetIndexNum());

		cEngineDiagnostics::CountDrawCall();
		glDrawElements(mode,lSize,GL_UNSIGNED_INT, (char*) NULL);

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	}

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::DrawIndices(unsigned int *apIndices, int alCount,eVertexBufferDrawType aDrawType)
	{
		if(mlRs)
		{
			cEngineDiagnostics::CountDrawCall();
			ohpl_glvb_draw_indices(mDrawType, aDrawType, apIndices, alCount);
			return;
		}

		GLenum mode = ohpl_gl_vb_draw_mode(mDrawType, aDrawType);

		cEngineDiagnostics::CountDrawCall();
		glDrawElements(mode, alCount, GL_UNSIGNED_INT, apIndices);
	}


	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::Bind()
	{
		;

		if((mlRs ? ohpl_glvb_needs_compile(mlRs, mvIndexArray.size()) : (mlElementHandle == 0 && mvIndexArray.empty()==false))) CompileSpecific();

		SetVertexStates();
	}

	//-----------------------------------------------------------------------

	void cVertexBufferOGL_VBO::UnBind()
	{
		if(mlRs)
		{
			ohpl_glvb_unbind(mlRs);
			return;
		}

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

	void cVertexBufferOGL_VBO::CompileSpecific()
	{
		if(mlRs)
		{
			OhplVbData vData[eVertexBufferElement_LastEnum];
			BuildVbData(vData, ~0u);
			ohpl_glvb_compile(mlRs, mUsageType, vData, &mvIndexArray[0], GetIndexNum());
			return;
		}

		GLenum usageType = ohpl_gl_usage(mUsageType);

		//Create the VBO vertex arrays
		for(size_t i=0; i<mvElementArrays.size(); ++i)
		{
			cVtxBufferGLElementArray *pElement = mvElementArrays[i];
            
			glGenBuffersARB(1,(GLuint *)&pElement->mlGLHandle);
			mvElemDesc[i].handle = pElement->mlGLHandle;
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, pElement->mlGLHandle);

			glBufferDataARB(GL_ARRAY_BUFFER_ARB, pElement->Size() * GetVertexFormatByteSize(pElement->mFormat),
							pElement->GetArrayPtr(), usageType);

			//Log("Compiling handle %d, type: %d element num: %d\n",pElement->mlGLHandle, pElement->mType, pElement->mlElementNum);

			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		}

		//Create the VBO index array
		glGenBuffersARB(1,(GLuint *)&mlElementHandle);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,mlElementHandle);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GetIndexNum()*sizeof(unsigned int),
			&mvIndexArray[0], usageType);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);

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
		if(mlRs)
		{
			ohpl_glvb_bind(mlRs, NULL);
			return;
		}

		OhplGlOp vOps[kOhplGlMaxOps];
		size_t lCount = ohpl_gl_vb_bind_vbo(mvElemDesc, mvElementArrays.size(), vOps);
		OhplGlOpSources src = {NULL, NULL, NULL};
		RunGlOps(vOps, lCount, src);
	}

	//-----------------------------------------------------------------------

}

//Old code for compiling all to one array.
//Log("Compiling VBO..\n");

/*int lNum = 0;
int lSize = 0;
int lNumSize = 0;
//Deterimine the number of arrays and the size.
Log("calc size: ");
for(int i=0;i< klNumOfVertexFlags; i++)
{
if(mVertexFlags & kvVertexFlags[i]){
lNum++;

//if(lSize != -1 && lSize != mvTempVertexArray[i].size()){
//	return false;
//}

lSize += (int)mvTempVertexArray[i].size();

lNumSize = (int)mvTempVertexArray[i].size()/GetElementNum(kvVertexFlags[i]);

Log(" %d (%d * %d),",lSize,lNumSize, GetElementNum(kvVertexFlags[i]));
}
}
Log("\n");

//Get memory for the main array
mvVertexArray.resize(lSize);

//Copy the temp arrays into the main one
int lPos =0;

Log("Copy: \n");
for(int i=0;i< klNumOfVertexFlags; i++)
{
if(mVertexFlags & kvVertexFlags[i])
{
int lArraySize = (int)mvTempVertexArray[i].size();

Log(" %d (%d) -> %d \n",i,lArraySize,lPos );

memcpy(&mvVertexArray[lPos], &(mvTempVertexArray[i][0]), sizeof(float)*lArraySize);

mvArrayOffset[i] = lPos;

lPos += lArraySize;
mvTempVertexArray[i].clear();
}
}

Log("Array:\n");
for(int i=0;i< klNumOfVertexFlags; i++)
{
if(mVertexFlags & kvVertexFlags[i])
{
int lOffset =  mvArrayOffset[i];
int lElemNum = GetElementNum(kvVertexFlags[i]);

for(int j=0;j<lNumSize;j++)
{
Log("(");

for(int k=0;k<lElemNum;k++)
{
Log(" %.1f,",mvVertexArray[lOffset + j*lElemNum + k]);
}

Log(") ");
}

Log("\n");
}
}*/



