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

#include "resources/EngineFileLoading.h"

#include "resources/XmlDocument.h"
#include "resources/Resources.h"
#include "scene/SoundEntity.h"
#include "scene/RustSceneH10.h"
#include "scene/ParticleSystem.h"
#include "scene/Light.h"
#include "scene/LensFlare.h"
#include "scene/FogArea.h"
#include "scene/BillBoard.h"
#include "resources/TextureManager.h"
#include "resources/RustResMgr.h"
#include "resources/MaterialManager.h"

#include "system/String.h"




#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"


extern "C" void ohpl_efl_set_host(const void*);

namespace hpl {


	namespace {
		typedef void (*tAttrSink)(void*, const uint8_t*, size_t, const uint8_t*, size_t);
		typedef void (*tChildSink)(void*, void*);

		struct OhplEflHost
		{
			void (*pods)(uint32_t, void*, void**);
			void* (*tex_create)(void*, int32_t, const uint8_t*, size_t, bool);
			void (*tex_frame_time)(void*, float);
			const uint8_t* (*el_name)(void*, size_t*);
			void (*el_attrs)(void*, tAttrSink, void*);
			void (*el_children)(void*, tChildSink, void*);
			const OhplH10Host *h10;
		};

		template<class T> void Ent(T *apObj, void **apOut){ apOut[1] = apObj->EntPod(); }
		template<class T> void Rend(T *apObj, void **apOut){ Ent(apObj, apOut); apOut[2] = apObj->Rs(); }

		void Pods(uint32_t alKind, void *apObj, void **apOut)
		{
			apOut[0] = apOut[1] = apOut[2] = NULL;
			switch(alKind)
			{
			case 0: apOut[0] = static_cast<cFogArea*>(apObj)->Pod(); Rend(static_cast<cFogArea*>(apObj), apOut); break;
			case 1: apOut[0] = static_cast<iLight*>(apObj)->Pod(); Rend(static_cast<iLight*>(apObj), apOut); break;
			case 2: apOut[0] = static_cast<cBillboard*>(apObj)->Pod(); Rend(static_cast<cBillboard*>(apObj), apOut); break;
			case 3: apOut[0] = static_cast<cLensFlare*>(apObj)->Pod(); Rend(static_cast<cLensFlare*>(apObj), apOut); break;
			case 4: apOut[0] = static_cast<cParticleSystem*>(apObj)->RsPod(); Ent(static_cast<cParticleSystem*>(apObj), apOut); break;
			case 5: apOut[0] = static_cast<cSoundEntity*>(apObj)->RsPod(); Ent(static_cast<cSoundEntity*>(apObj), apOut); break;
			}
		}

		void* TexCreate(void *apRes, int32_t alKind, const uint8_t *apName, size_t alLen, bool abSRGB)
		{
			cTextureManager *pMgr = static_cast<cResources*>(apRes)->GetTextureManager();
			const tString sName(reinterpret_cast<const char*>(apName), alLen);
			if(abSRGB) pMgr->SetSRGB(true);
			iTexture *pTex = NULL;
			switch(alKind)
			{
			case 0: pTex = pMgr->Create1D(sName, true); break;
			case 1: pTex = pMgr->Create2D(sName, true); break;
			case 2: pTex = pMgr->CreateCubeMap(sName, true); break;
			case 3: pTex = pMgr->CreateAnim(sName, true, eTextureType_2D); break;
			case 4: pTex = pMgr->CreateAnim(sName, true, eTextureType_CubeMap); break;
			}
			if(abSRGB) pMgr->SetSRGB(false);
			return pTex;
		}

		void TexFrameTime(void *apTex, float afTime){ static_cast<iTexture*>(apTex)->SetFrameTime(afTime); }

		const uint8_t* ElName(void *apEl, size_t *apLen)
		{
			const tString &sName = static_cast<cXmlElement*>(apEl)->GetValue();
			*apLen = sName.size();
			return reinterpret_cast<const uint8_t*>(sName.data());
		}

		void ElAttrs(void *apEl, tAttrSink apSink, void *apCtx)
		{
			tAttributeMap *pMap = static_cast<cXmlElement*>(apEl)->GetAttributeMap();
			for(tAttributeMapIt it = pMap->begin(); it != pMap->end(); ++it)
				apSink(apCtx, reinterpret_cast<const uint8_t*>(it->first.data()), it->first.size(), reinterpret_cast<const uint8_t*>(it->second.data()), it->second.size());
		}

		void ElChildren(void *apEl, tChildSink apSink, void *apCtx)
		{
			cXmlNodeListIterator it = static_cast<cXmlElement*>(apEl)->GetChildIterator();
			while(it.HasNext())
				if(cXmlElement *pChild = it.Next()->ToElement()) apSink(apCtx, pChild);
		}

		struct cRegister
		{
			cRegister()
			{
				static const OhplEflHost h = { Pods, TexCreate, TexFrameTime, ElName, ElAttrs, ElChildren, OhplH10GetHost() };
				ohpl_efl_set_host(&h);
			}
		} gRegister;
	}


	int glDecalNumOfElements[4] = {4,3,3,4};
	eVertexBufferElement glDecalElementType[4] = {	eVertexBufferElement_Position, 
													eVertexBufferElement_Normal,
													eVertexBufferElement_Texture0,
													eVertexBufferElement_Texture1Tangent};
	cMesh* cEngineFileLoading::LoadDecalMeshHelper(cXmlElement* apElement, cGraphics* apGraphics, cResources* apResources, const tString& asName, const tString& asMaterial, const cColor& aColor)
	{
		////////////////////////////////
		//Load Vertex data
		if(apElement==NULL)return NULL;

		int lNumOfVtx = apElement->GetAttributeInt("NumVerts", 0);
		int lNumOfIdx = apElement->GetAttributeInt("NumInds", 0);

		if(lNumOfIdx <=0 || lNumOfVtx<=0)
		{
			Warning("Decal %s is missing geometry, skipping!\n", asName.c_str());
			return NULL;
		}
			
		cXmlElement *pDataArrayElem[4];
		pDataArrayElem[0] = apElement->GetFirstElement("Positions");
		pDataArrayElem[1] = apElement->GetFirstElement("Normals");
		pDataArrayElem[2] = apElement->GetFirstElement("TexCoords");
		pDataArrayElem[3] = apElement->GetFirstElement("Tangents");
		cXmlElement *pIndicesElem = apElement->GetFirstElement("Indices");

		tFloatVec vDataArrays[4];
		tIntVec vIdxArray;
		tString sSepp=" ";
		//A missing array element or short array used to read past the data.
		if(pIndicesElem==NULL)
		{
			Warning("Decal %s is missing its index array, skipping!\n", asName.c_str());
			return NULL;
		}
		for(int i=0; i<4; ++i)
		{
			if(pDataArrayElem[i]==NULL)
			{
				Warning("Decal %s is missing a vertex array, skipping!\n", asName.c_str());
				return NULL;
			}
			cString::GetFloatVec(pDataArrayElem[i]->GetAttributeString("Array"), vDataArrays[i],&sSepp);
		}
		cString::GetIntVec(pIndicesElem->GetAttributeString("Array"), vIdxArray,&sSepp);

		const float *pArrays[4] = { vDataArrays[0].data(), vDataArrays[1].data(), vDataArrays[2].data(), vDataArrays[3].data() };
		size_t lArrayLens[4] = { vDataArrays[0].size(), vDataArrays[1].size(), vDataArrays[2].size(), vDataArrays[3].size() };
		if(ohpl_rm_world_decal_vertices(pArrays, lArrayLens, (size_t)lNumOfVtx, NULL)==false || vIdxArray.size() < (size_t)lNumOfIdx)
		{
			Warning("Decal %s has too little geometry data, skipping!\n", asName.c_str());
			return NULL;
		}
		std::vector<float> vPacked((size_t)lNumOfVtx * 16);
		ohpl_rm_world_decal_vertices(pArrays, lArrayLens, (size_t)lNumOfVtx, vPacked.data());
	
		//////////////////////////////////
		// Create vertex buffer
		iVertexBuffer *pVtxBuffer = apGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Software, eVertexBufferDrawType_Tri, 
																					eVertexBufferUsageType_Static,lNumOfVtx, lNumOfIdx);

		//Create arrays	
		for(int i=0; i<4; ++i)
			pVtxBuffer->CreateElementArray(glDecalElementType[i],eVertexBufferElementFormat_Float, glDecalNumOfElements[i]);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		
		//Copy the data!
		// TODO: This needs to be made faster so that data is loaded directly into mesh!
		for(int vtx=0; vtx<lNumOfVtx; ++vtx)
		{
			for(int i=0; i<4; ++i)
			{
				const float *pData = &vPacked[((size_t)vtx*4 + i)*4];

				if(glDecalNumOfElements[i]==2)
					pVtxBuffer->AddVertexVec3f(glDecalElementType[i], cVector3f(pData[0],pData[1],0) );
				else if(glDecalNumOfElements[i]==3)
					pVtxBuffer->AddVertexVec3f(glDecalElementType[i], cVector3f(pData[0],pData[1],pData[2]) );
				else if(glDecalNumOfElements[i]==4)
					pVtxBuffer->AddVertexVec4f(glDecalElementType[i], cVector3f(pData[0],pData[1],pData[2]),pData[3]);
			}

			pVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, aColor);
		}

		for(int i=0; i<lNumOfIdx; ++i)
			pVtxBuffer->AddIndex(vIdxArray[i]);

		//Compile
		pVtxBuffer->Compile(0);
		
		/////////////////////////
		// Create the mesh
		cMesh *pMesh = hplNew( cMesh, (asName, _W(""), apResources->GetMaterialManager(), apResources->GetAnimationManager()) );

		cSubMesh *pSubMesh = pMesh->CreateSubMesh("Main");
		
		cMaterial *pMat = apResources->GetMaterialManager()->CreateMaterial(asMaterial);
		pSubMesh->SetMaterial(pMat);
		pSubMesh->SetVertexBuffer(pVtxBuffer);
		pSubMesh->SetMaterialName(asMaterial);


		return pMesh;
	}
}
