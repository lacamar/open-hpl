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

#include "system/LowLevelSystem.h"

#include "graphics/LowLevelGraphics.h"

#include "math/Math.h"

#include "impl/VertexBufferOGL_Array.h"
#include "impl/VertexBufferOGL_VBO.h"

#include <memory.h>

#include <GL/glew.h>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// GLOBAL FUNCTIONS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	unsigned int GetGLArrayFromVertexElement(eVertexBufferElement aType)
	{
		return ohpl_gl_vertex_array(aType);
	}
	
	unsigned int  GetGLTypeFromVertexFormat(eVertexBufferElementFormat aFormat)
	{
		return ohpl_gl_vertex_type(aFormat);
	}

	//-----------------------------------------------------------------------

	unsigned int GetDrawModeFromDrawType(eVertexBufferDrawType aDrawType)
	{
		return ohpl_gl_draw_type_mode(aDrawType);
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// ELEMENT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVtxBufferGLElementArray::cVtxBufferGLElementArray(eVertexBufferElementFormat aFormat)
	{
		mlGLHandle =0;

		mpByteArray = NULL;
		mpIntArray = NULL;
		mpFloatArray = NULL;

		switch(aFormat)
		{
		case eVertexBufferElementFormat_Float:	mpFloatArray = hplNew(tFloatVec, ()); break;
		case eVertexBufferElementFormat_Int:	mpIntArray = hplNew(tIntVec, ());	break;
		case eVertexBufferElementFormat_Byte:	mpByteArray = hplNew(tByteVec, ());	break;
		}
	}
	
	cVtxBufferGLElementArray::~cVtxBufferGLElementArray()
	{
		if(mpByteArray)	hplDelete(mpByteArray);
		if(mpIntArray)	hplDelete(mpIntArray);
		if(mpFloatArray)hplDelete(mpFloatArray);
	}

	//-----------------------------------------------------------------------

	void cVtxBufferGLElementArray::Reserve(size_t alSize)
	{
		switch(mFormat)
		{
		case eVertexBufferElementFormat_Float:	mpFloatArray->reserve(alSize); break;
		case eVertexBufferElementFormat_Int:	mpIntArray->reserve(alSize); break;
		case eVertexBufferElementFormat_Byte:	mpByteArray->reserve(alSize); break;
		}
	}
	void cVtxBufferGLElementArray::Resize(size_t alSize)
	{
		switch(mFormat)
		{
		case eVertexBufferElementFormat_Float:	mpFloatArray->resize(alSize); break;
		case eVertexBufferElementFormat_Int:	mpIntArray->resize(alSize); break;
		case eVertexBufferElementFormat_Byte:	mpByteArray->resize(alSize); break;
		}
	}
	void cVtxBufferGLElementArray::PushBack(const void *apData)
	{
		switch(mFormat)
		{
		case eVertexBufferElementFormat_Float:	mpFloatArray->push_back( *((const float*)apData) ); break;
		case eVertexBufferElementFormat_Int:	mpIntArray->push_back( *((const int*)apData) ); break;
		case eVertexBufferElementFormat_Byte:	mpByteArray->push_back( *((const unsigned char*)apData) ); break;
		}
	}

	void* cVtxBufferGLElementArray::GetArrayPtr()
	{
		switch(mFormat)
		{
		case eVertexBufferElementFormat_Float:	return &(*mpFloatArray)[0];
		case eVertexBufferElementFormat_Int:	return &(*mpIntArray)[0];
		case eVertexBufferElementFormat_Byte:	return &(*mpByteArray)[0];
		}
		return NULL;
	}

	size_t cVtxBufferGLElementArray::Size()
	{
		switch(mFormat)
		{
		case eVertexBufferElementFormat_Float:	return mpFloatArray->size();
		case eVertexBufferElementFormat_Int:	return mpIntArray->size();
		case eVertexBufferElementFormat_Byte:	return mpByteArray->size();
		}
		return 0;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iVertexBufferOpenGL::iVertexBufferOpenGL(	iLowLevelGraphics* apLowLevelGraphics,
												eVertexBufferType aType,
												eVertexBufferDrawType aDrawType,eVertexBufferUsageType aUsageType,
												int alReserveVtxSize,int alReserveIdxSize) :
	iVertexBuffer(apLowLevelGraphics, aType, aDrawType,aUsageType, alReserveVtxSize, alReserveIdxSize)
	{
		if(alReserveIdxSize>0)
			mvIndexArray.reserve(alReserveIdxSize);

		mbHasShadowDouble = false;

		mpLowLevelGraphics = apLowLevelGraphics;

		for(int i=0; i< eVertexBufferElement_LastEnum; ++i)	mvElementArrayIndex[i] =-1;
	}

	//----------------------------------------------------------------------

	iVertexBufferOpenGL::~iVertexBufferOpenGL()
	{
		STLDeleteAll(mvElementArrays);	
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	void iVertexBufferOpenGL::CreateElementArray(	eVertexBufferElement aType, eVertexBufferElementFormat aFormat,
													int alElementNum, int alProgramVarIndex)
	{
		tVertexElementFlag elementFlag = GetVertexElementFlagFromEnum(aType);
		if(elementFlag & mVertexFlags){
			Error("Vertex element of type %d already present in buffer %d!\n",aType,this);
			return;
		}
		mVertexFlags |= elementFlag;

		cVtxBufferGLElementArray *pElement = hplNew(cVtxBufferGLElementArray, (aFormat) );
		pElement->mType = aType;
		pElement->mFlag = elementFlag;
		pElement->mFormat = aFormat;
		pElement->mlElementNum = alElementNum;
		pElement->mlProgramVarIndex = alProgramVarIndex;
		 
		if(mlReservedVtxSize > 0) pElement->Reserve(alElementNum * mlReservedVtxSize);

		mvElementArrayIndex[aType] = (char)mvElementArrays.size();
		OhplGlElem &desc = mvElemDesc[mvElementArrays.size()];
		desc.kind = aType;
		desc.format = aFormat;
		desc.elementNum = alElementNum;
		desc.handle = 0;

		mvElementArrays.push_back(pElement);
	}
	//-----------------------------------------------------------------------

	void iVertexBufferOpenGL::AddVertexVec3f(eVertexBufferElement aType,const cVector3f& avVtx)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aType);

		float vValues[4];
		size_t lCount = ohpl_gl_vb_vec3_components(pElement->mlElementNum, avVtx.x, avVtx.y, avVtx.z, vValues);
		for(size_t i=0; i<lCount; ++i) pElement->PushBack(&vValues[i]);
	}

	//-----------------------------------------------------------------------

	void iVertexBufferOpenGL::AddVertexVec4f(eVertexBufferElement aType,const cVector3f& avVtx, float afW)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aType);

		pElement->PushBack(&avVtx.x);
		pElement->PushBack(&avVtx.y);
		pElement->PushBack(&avVtx.z);
		pElement->PushBack(&afW);
	}

	//-----------------------------------------------------------------------

	void iVertexBufferOpenGL::AddVertexColor(eVertexBufferElement aType,const cColor& aColor)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aType);

		if(ohpl_gl_vb_color_components(pElement->mFormat) == 4)
		{
			pElement->PushBack(&aColor.r);
			pElement->PushBack(&aColor.g);
			pElement->PushBack(&aColor.b);
			pElement->PushBack(&aColor.a);
		}
	}

	//-----------------------------------------------------------------------

	void iVertexBufferOpenGL::AddIndex(unsigned int alIndex)
	{
		mvIndexArray.push_back(alIndex);
	}

	//-----------------------------------------------------------------------

	cBoundingVolume iVertexBufferOpenGL::CreateBoundingVolume()
	{
		cBoundingVolume bv;

        int lCheck = ohpl_gl_vb_bounding_check(mVertexFlags, mvElementArrayIndex[eVertexBufferElement_Position] < 0 ? 0 : GetElementArray(eVertexBufferElement_Position)->mFormat);
		if(lCheck == 1)
		{
			Warning("Could not create bounding volume from buffer %d  because no position element was present!\n",this);
			return bv;
		}

		cVtxBufferGLElementArray *pElement = GetElementArray(eVertexBufferElement_Position);
		if(lCheck == 2)
		{
			Warning("Could not breate bounding volume since postion was not for format float in buffer %d!\n",this);
			return bv;
		}

		bv.AddArrayPoints((float*)pElement->GetArrayPtr(), GetVertexNum());
		bv.CreateFromPoints(pElement->mlElementNum);

		return bv;
	}

	//-----------------------------------------------------------------------

	bool iVertexBufferOpenGL::Compile(tVertexCompileFlag aFlags)
	{
		//////////////////////////////////////////
		//Create tangents
		if(aFlags & eVertexCompileFlag_CreateTangents)
		{
			CreateElementArray(eVertexBufferElement_Texture1Tangent, eVertexBufferElementFormat_Float, 4);

			cVtxBufferGLElementArray *pPosElement = GetElementArray(eVertexBufferElement_Position);
			cVtxBufferGLElementArray *pNormalElement = GetElementArray(eVertexBufferElement_Normal);
			cVtxBufferGLElementArray *pTextureElement = GetElementArray(eVertexBufferElement_Texture0);
			cVtxBufferGLElementArray *pTangentElement = GetElementArray(eVertexBufferElement_Texture1Tangent);

			if(pPosElement && pNormalElement && pTextureElement && pTangentElement)
			{

			pTangentElement->Resize(GetVertexNum()*4);
			
			cMath::CreateTriTangentVectors((float*)pTangentElement->GetArrayPtr(),
											&mvIndexArray[0], GetIndexNum(),

											(float*)pPosElement->GetArrayPtr(),
											pPosElement->mlElementNum,

											(float*)pTextureElement->GetArrayPtr(),
											(float*)pNormalElement->GetArrayPtr(),
											GetVertexNum()
				);
			}
		}

		//////////////////////////////////////////
		//Compile implementation specific
		CompileSpecific();

		return true;
	}

	//-----------------------------------------------------------------------

	void iVertexBufferOpenGL::CreateShadowDouble(bool abUpdateData)
	{
		cVtxBufferGLElementArray *pPosElement = GetElementArray(eVertexBufferElement_Position);

		size_t lSize = pPosElement->Size();
		pPosElement->Resize(lSize + ohpl_gl_vb_shadow_double_extra(lSize));
		ohpl_gl_vb_shadow_double_fill(pPosElement->mFormat, (unsigned*)pPosElement->GetArrayPtr(), pPosElement->Size(), lSize);

		mbHasShadowDouble = true;

		if(abUpdateData)
		{
			UpdateData(eVertexElementFlag_Position, false);
		}
	}

	//-----------------------------------------------------------------------

	void iVertexBufferOpenGL::Transform(const cMatrixf &a_mtxTransform)
	{
		cVtxBufferGLElementArray *pPosElement = GetElementArray(eVertexBufferElement_Position);
		cVtxBufferGLElementArray *pNormalElement = GetElementArray(eVertexBufferElement_Normal);
		cVtxBufferGLElementArray *pTangentElement = GetElementArray(eVertexBufferElement_Texture1Tangent);
		if(pPosElement == NULL || pPosElement->mFormat == eVertexBufferElementFormat_Byte) return;
		if(pNormalElement && pNormalElement->mFormat == eVertexBufferElementFormat_Byte) pNormalElement = NULL;
		if(pTangentElement && pTangentElement->mFormat == eVertexBufferElementFormat_Byte) pTangentElement = NULL;

		tVertexElementFlag vtxFlag = ohpl_gl_vb_transform(&a_mtxTransform,
			(float*)pPosElement->GetArrayPtr(), pPosElement->Size(),
			pNormalElement ? (float*)pNormalElement->GetArrayPtr() : NULL, pNormalElement ? pNormalElement->Size() : 0,
			pTangentElement ? (float*)pTangentElement->GetArrayPtr() : NULL, pTangentElement ? pTangentElement->Size() : 0,
			GetVertexNum(), pPosElement->mlElementNum, mbHasShadowDouble);

		UpdateData(vtxFlag ,false);
	}

	//-----------------------------------------------------------------------

	int iVertexBufferOpenGL::GetElementNum(eVertexBufferElement aElement)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aElement);
		if(pElement==NULL) return 0;	

		return pElement->mlElementNum;
	}

	//-----------------------------------------------------------------------

	eVertexBufferElementFormat iVertexBufferOpenGL::GetElementFormat(eVertexBufferElement aElement)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aElement);
		if(pElement==NULL) return eVertexBufferElementFormat_LastEnum;	

		return pElement->mFormat;
	}

	//-----------------------------------------------------------------------

	int iVertexBufferOpenGL::GetElementProgramVarIndex(eVertexBufferElement aElement)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aElement);
		if(pElement==NULL) return 0;	

		return pElement->mlProgramVarIndex;
	}

	//-----------------------------------------------------------------------

	float* iVertexBufferOpenGL::GetFloatArray(eVertexBufferElement aElement)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aElement);
        if(pElement==NULL) return NULL;	
		
		return (float*)pElement->GetArrayPtr();
	}
	
	int* iVertexBufferOpenGL::GetIntArray(eVertexBufferElement aElement)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aElement);
		if(pElement==NULL) return NULL;	

		return (int*)pElement->GetArrayPtr();
	}
	
	unsigned char* iVertexBufferOpenGL::GetByteArray(eVertexBufferElement aElement)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aElement);
		if(pElement==NULL) return NULL;	

		return (unsigned char*)pElement->GetArrayPtr();
	}

	//-----------------------------------------------------------------------

	unsigned int* iVertexBufferOpenGL::GetIndices()
	{
		return &mvIndexArray[0];
	}

	//-----------------------------------------------------------------------

	void iVertexBufferOpenGL::ResizeArray(eVertexBufferElement aElement, int alSize)
	{
		cVtxBufferGLElementArray *pElement = GetElementArray(aElement);
		if(pElement==NULL) return;

		pElement->Resize(alSize);
	}

	//-----------------------------------------------------------------------

	void iVertexBufferOpenGL::ResizeIndices(int alSize)
	{
		mvIndexArray.resize(alSize);
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* iVertexBufferOpenGL::CreateCopy(	eVertexBufferType aType, eVertexBufferUsageType aUsageType,
													tVertexElementFlag alVtxToCopy)
	{
		alVtxToCopy = ohpl_gl_vb_copy_flags(alVtxToCopy, mVertexFlags);

		iVertexBufferOpenGL *pVtxBuff = static_cast<iVertexBufferOpenGL*>(
			mpLowLevelGraphics->CreateVertexBuffer(aType, mDrawType, aUsageType, GetIndexNum(), GetVertexNum()));

		//Copy the vertices to the new buffer.
		for(size_t i=0; i<mvElementArrays.size(); ++i)
		{
			cVtxBufferGLElementArray *pSrcElement = mvElementArrays[i];
			if( (pSrcElement->mFlag & alVtxToCopy) == 0) continue;
			
			pVtxBuff->CreateElementArray(pSrcElement->mType,pSrcElement->mFormat,pSrcElement->mlElementNum,pSrcElement->mlProgramVarIndex);
			cVtxBufferGLElementArray *pDestElement = pVtxBuff->GetElementArray(pSrcElement->mType);

			pDestElement->Resize(pSrcElement->Size());

			memcpy(	pDestElement->GetArrayPtr(), pSrcElement->GetArrayPtr(), 
					pSrcElement->Size() * GetVertexFormatByteSize(pSrcElement->mFormat));
		}

        /*for(int i=0; i < klNumOfVertexFlags; i++)
		{
			if( (kvVertexFlags[i] & mVertexFlags) && (kvVertexFlags[i] & alVtxToCopy))
			{
				int lElements = kvVertexElements[i];
				if(mbTangents && kvVertexFlags[i] == eVertexElementFlag_Texture1)
					lElements=4;

				pVtxBuff->ResizeArray(kvVertexFlags[i], (int)mvVertexArray[i].size());

				memcpy(pVtxBuff->GetArray(kvVertexFlags[i]),
						&mvVertexArray[i][0], mvVertexArray[i].size() * sizeof(float));
			}
		}*/

		//Copy indices to the new buffer
        pVtxBuff->ResizeIndices(GetIndexNum());
		memcpy(pVtxBuff->GetIndices(), GetIndices(), GetIndexNum() * sizeof(unsigned int) );

		pVtxBuff->mbTangents = mbTangents;
		pVtxBuff->mbHasShadowDouble = mbHasShadowDouble;

		pVtxBuff->Compile(0);

		return pVtxBuff;
	}

	//-----------------------------------------------------------------------

	int iVertexBufferOpenGL::GetVertexNum()
	{
		cVtxBufferGLElementArray *pPosElement = GetElementArray(eVertexBufferElement_Position);
		return ohpl_gl_vb_vertex_num((int)pPosElement->Size(), pPosElement->mlElementNum, mbHasShadowDouble);
	}
	
	//-----------------------------------------------------------------------

	int iVertexBufferOpenGL::GetIndexNum()
	{
		return (int)mvIndexArray.size();
	}

	//-----------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	/////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	
	//-----------------------------------------------------------------------

}


