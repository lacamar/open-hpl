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

#ifndef HPL_VERTEXBUFFER_OPENGL_H
#define HPL_VERTEXBUFFER_OPENGL_H

#include "graphics/VertexBuffer.h"
#include "system/EngineDiagnostics.h"
#include "system/LowLevelSystem.h"
#include "impl/RustGl.h"
#include "impl/RustGlRes.h"
#include "graphics/LowLevelGraphics.h"

namespace hpl {

	//---------------------------------------------------------

	// Element arrays, indices and GL names live in Rust (rust/src/backend/gl_res/vbdata.rs); mlRs is the handle.
	class iVertexBufferOpenGL : public iVertexBuffer
	{
	public:
		iVertexBufferOpenGL(	iLowLevelGraphics* apLowLevelGraphics,
							eVertexBufferType aType,
							eVertexBufferDrawType aDrawType,eVertexBufferUsageType aUsageType,
							int alReserveVtxSize,int alReserveIdxSize, bool abVbo=false) :
		iVertexBuffer(apLowLevelGraphics, aType, aDrawType,aUsageType, alReserveVtxSize, alReserveIdxSize),
		mlRs(ohpl_vb_new(abVbo, alReserveVtxSize, alReserveIdxSize)) {}
		~iVertexBufferOpenGL(){ ohpl_vb_free(mlRs); }

		void CreateElementArray(	eVertexBufferElement aType, eVertexBufferElementFormat aFormat,
									int alElementNum, int alProgramVarIndex=0)
		{
			unsigned lFlag = ohpl_vb_create_array(mlRs, aType, aFormat, alElementNum, alProgramVarIndex);
			if(lFlag == 0){ Error("Vertex element of type %d already present in buffer %d!\n",aType,this); return; }
			mVertexFlags |= lFlag;
		}
		void AddVertexVec3f(eVertexBufferElement aElement,const cVector3f& avVtx){ ohpl_vb_add_vec3(mlRs, aElement, avVtx.x, avVtx.y, avVtx.z); }
		void AddVertexVec4f(eVertexBufferElement aElement,const cVector3f& avVtx, float afW){ ohpl_vb_add_vec4(mlRs, aElement, avVtx.x, avVtx.y, avVtx.z, afW); }
		void AddVertexColor(eVertexBufferElement aElement,const cColor& aColor){ ohpl_vb_add_color(mlRs, aElement, aColor.r, aColor.g, aColor.b, aColor.a); }
		void AddIndex(unsigned int alIndex){ ohpl_vb_add_index(mlRs, alIndex); }

		bool Compile(tVertexCompileFlag aFlags)
		{
			if(aFlags & eVertexCompileFlag_CreateTangents)
			{
				CreateElementArray(eVertexBufferElement_Texture1Tangent, eVertexBufferElementFormat_Float, 4);
				ohpl_vb_fill_tangents(mlRs);
			}
			CompileSpecific();
			return true;
		}
		void CreateShadowDouble(bool abUpdateData)
		{
			if(ohpl_vb_shadow_double(mlRs) && abUpdateData) UpdateData(eVertexElementFlag_Position, false);
		}
		void Transform(const cMatrixf &a_mtxTransform)
		{
			tVertexElementFlag vtxFlag = ohpl_vb_transform(mlRs, &a_mtxTransform);
			if(vtxFlag) UpdateData(vtxFlag, false);
		}
		iVertexBuffer* CreateCopy(	eVertexBufferType aType, eVertexBufferUsageType aUsageType,
									tVertexElementFlag alVtxToCopy)
		{
			auto *p = static_cast<iVertexBufferOpenGL*>(
				mpLowLevelGraphics->CreateVertexBuffer(aType, mDrawType, aUsageType, GetIndexNum(), GetVertexNum()));
			p->mVertexFlags |= ohpl_vb_copy_into(mlRs, p->mlRs, alVtxToCopy);
			p->Compile(0);
			return p;
		}
		cBoundingVolume CreateBoundingVolume()
		{
			cBoundingVolume bv;
			int lCheck = ohpl_vb_bounding_check(mlRs);
			if(lCheck == 1){ Warning("Could not create bounding volume from buffer %d  because no position element was present!\n",this); return bv; }
			if(lCheck == 2){ Warning("Could not breate bounding volume since postion was not for format float in buffer %d!\n",this); return bv; }
			bv.AddArrayPoints(GetFloatArray(eVertexBufferElement_Position), GetVertexNum());
			bv.CreateFromPoints(GetElementNum(eVertexBufferElement_Position));
			return bv;
		}

		int GetVertexNum(){ return ohpl_vb_vertex_num(mlRs); }
		int GetIndexNum(){ return ohpl_vb_index_num(mlRs); }
		int GetElementNum(eVertexBufferElement aElement){ int v[3]; ohpl_vb_element_desc(mlRs, aElement, v); return v[0]; }
		eVertexBufferElementFormat GetElementFormat(eVertexBufferElement aElement){ int v[3]; ohpl_vb_element_desc(mlRs, aElement, v); return (eVertexBufferElementFormat)v[1]; }
		int GetElementProgramVarIndex(eVertexBufferElement aElement){ int v[3]; ohpl_vb_element_desc(mlRs, aElement, v); return v[2]; }
		float* GetFloatArray(eVertexBufferElement aElement){ return (float*)ohpl_vb_array_ptr(mlRs, aElement); }
		int* GetIntArray(eVertexBufferElement aElement){ return (int*)ohpl_vb_array_ptr(mlRs, aElement); }
		unsigned char* GetByteArray(eVertexBufferElement aElement){ return (unsigned char*)ohpl_vb_array_ptr(mlRs, aElement); }
		unsigned int* GetIndices(){ return ohpl_vb_indices(mlRs); }
		void ResizeArray(eVertexBufferElement aElement, int alSize){ ohpl_vb_resize_array(mlRs, aElement, alSize); }
		void ResizeIndices(int alSize){ ohpl_vb_resize_indices(mlRs, alSize); }

		void UpdateData(tVertexElementFlag aTypes, bool abIndices){ ohpl_vb_update(mlRs, mUsageType, aTypes, abIndices); }
		void Draw(eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum)
		{
			cEngineDiagnostics::CountDrawCall();
			ohpl_vb_draw(mlRs, mDrawType, aDrawType, mlElementNum);
		}
		void DrawIndices(unsigned int *apIndices, int alCount, eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum)
		{
			cEngineDiagnostics::CountDrawCall();
			ohpl_glvb_draw_indices(mDrawType, aDrawType, apIndices, alCount);
		}
		void Bind(){ ohpl_vb_bind(mlRs, mUsageType); }
		void UnBind(){ ohpl_vb_unbind(mlRs); }

	protected:
		virtual void CompileSpecific(){ ohpl_vb_compile(mlRs, mUsageType); }

		uint64_t mlRs;
	};

};
#endif // HPL_VERTEXBUFFER_OPENGL_H
