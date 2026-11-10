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


#include "graphics/MeshCreator.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "resources/Resources.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/RustMesh.h"
#include "resources/MaterialManager.h"
#include "resources/AnimationManager.h"
#include "math/Math.h"

namespace hpl {

	//-----------------------------------------------------------------------

	namespace {
		void CreateStandardElements(iVertexBuffer *apVtxBuffer)
		{
			apVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
			apVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
			apVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
			apVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);
		}

		void FillVertexBuffer(iVertexBuffer *apVtxBuffer, const OhplMeshData *apData, bool abNormals)
		{
			const cColor col(1,1,1,1);
			for(uint32_t i=0; i<apData->vertexCount; ++i)
			{
				apVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, apData->positions[i]);
				if(abNormals) apVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal, apData->normals[i]);
				apVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, col);
				apVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, apData->texcoords[i]);
			}
			for(uint32_t i=0; i<apData->indexCount; ++i)
				apVtxBuffer->AddIndex(apData->indices[i]);
		}
	}

	//-----------------------------------------------------------------------

	cMeshCreator::cMeshCreator(iLowLevelGraphics *apLowLevelGraphics, cResources *apResources)
	{
		mpLowLevelGraphics = apLowLevelGraphics;
		mpResources = apResources;
	}

	//-----------------------------------------------------------------------

	cMeshCreator::~cMeshCreator()
	{
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cMeshCreator::CreateWireframeVertexBuffer(iVertexBuffer *apSrc)
	{
		iVertexBuffer *pDest = apSrc->CreateCopy(eVertexBufferType_Hardware, eVertexBufferUsageType_Static, apSrc->GetVertexElementFlags());

        pDest->ResizeIndices(apSrc->GetIndexNum()*4);
		unsigned int* pDestArray = pDest->GetIndices();

		OhplMeshData *pData = ohpl_meshgen_wireframe(apSrc->GetIndices(), (size_t)apSrc->GetIndexNum());
		for(uint32_t i=0; i<pData->indexCount; ++i) pDestArray[i] = pData->indices[i];
		ohpl_meshgen_free(pData);

		pDest->UpdateData(0, true);

		return pDest;
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshCreator::CreateMeshFromBuffer(const tString &asName, iVertexBuffer *apVtxBuffer, const tString &asMaterial, bool abSetMaterialName)
	{
		cMesh *pMesh = hplNew( cMesh, (asName, _W(""), mpResources->GetMaterialManager(), mpResources->GetAnimationManager()) );

		cSubMesh *pSubMesh = pMesh->CreateSubMesh("Main");

		cMaterial *pMat = mpResources->GetMaterialManager()->CreateMaterial(asMaterial);
		pSubMesh->SetMaterial(pMat);
		pSubMesh->SetVertexBuffer(apVtxBuffer);
		if(abSetMaterialName) pSubMesh->SetMaterialName(asMaterial);

		return pMesh;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cMeshCreator::CompileOrDelete(iVertexBuffer *apVtxBuffer)
	{
		if(!apVtxBuffer->Compile(eVertexCompileFlag_CreateTangents))
		{
			hplDelete(apVtxBuffer);
			return NULL;
		}
		return apVtxBuffer;
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshCreator::CreateBox(const tString &asName,cVector3f avSize, const tString &asMaterial)
	{
		cMesh *pMesh = hplNew( cMesh, (asName, _W(""), mpResources->GetMaterialManager(), mpResources->GetAnimationManager()) );

		cSubMesh *pSubMesh = pMesh->CreateSubMesh("Main");

		cMaterial *pMat = mpResources->GetMaterialManager()->CreateMaterial(asMaterial);
		pSubMesh->SetMaterial(pMat);
		iVertexBuffer *pVtxBuff = CreateBoxVertexBuffer(avSize);
		pSubMesh->SetVertexBuffer(pVtxBuff);

		return pMesh;
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshCreator::CreatePlane(const tString& asName, const cVector3f& avCorner1, const cVector3f& avCorner2,
									 const cVector2f& avCorner1UV, const cVector2f& avCorner2UV, const cVector2f& avCorner3UV, const cVector2f& avCorner4UV,
									 const tString& asMaterial)
	{
		const cVector2f vUVs[4] = { avCorner1UV, avCorner2UV, avCorner3UV, avCorner4UV };
		OhplMeshData *pData = NULL;
		int lError = ohpl_meshgen_plane(&avCorner1, &avCorner2, vUVs, &pData);
		if(lError == 1)
		{
			Error("CreatePlane failed: plane corners are coincident. Plane name: %s\n", asName.c_str());
			return NULL;
		}
		if(lError != 0)
		{
			Error("CreatePlane failed: plane corners are not coplanar. Plane name: %s\n", asName.c_str());
			return NULL;
		}

		iVertexBuffer* pVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(
								eVertexBufferType_Hardware,
								eVertexBufferDrawType_Tri, eVertexBufferUsageType_Dynamic,4,6);
		CreateStandardElements(pVtxBuffer);
		FillVertexBuffer(pVtxBuffer, pData, true);
		ohpl_meshgen_free(pData);

		pVtxBuffer->Compile(eVertexCompileFlag_CreateTangents);

		return CreateMeshFromBuffer(asName, pVtxBuffer, asMaterial, true);
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshCreator::CreateSphere(const tString& asName, float afRadius, int alSections, int alSlices, const tString &asMaterial)
	{
		iVertexBuffer* pVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																	eVertexBufferUsageType_Static);
		CreateStandardElements(pVtxBuffer);

		OhplMeshData *pData = ohpl_meshgen_sphere(afRadius, alSections, alSlices);
		FillVertexBuffer(pVtxBuffer, pData, true);
		ohpl_meshgen_free(pData);

		return CreateMeshFromBuffer(asName, CompileOrDelete(pVtxBuffer), asMaterial, false);
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshCreator::CreateCylinder(const tString& asName, const cVector2f &avSize, int alSections, const tString &asMaterial)
	{
		iVertexBuffer* pVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																	eVertexBufferUsageType_Static);
		CreateStandardElements(pVtxBuffer);

		OhplMeshData *pData = ohpl_meshgen_cylinder(&avSize, alSections);
		FillVertexBuffer(pVtxBuffer, pData, true);
		ohpl_meshgen_free(pData);

		return CreateMeshFromBuffer(asName, CompileOrDelete(pVtxBuffer), asMaterial, false);
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshCreator::CreateCapsule(const tString &asName, const cVector2f &avSize, int alSections, int alSlices, const tString &asMaterial)
	{
		iVertexBuffer* pVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																	eVertexBufferUsageType_Static);
		CreateStandardElements(pVtxBuffer);

		OhplMeshData *pData = ohpl_meshgen_capsule(&avSize, alSections, alSlices);
		FillVertexBuffer(pVtxBuffer, pData, true);
		ohpl_meshgen_free(pData);

		return CreateMeshFromBuffer(asName, CompileOrDelete(pVtxBuffer), asMaterial, false);
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshCreator::CreateCone(const tString &asName, const cVector2f &avSize, int alSections, const tString &asMaterial)
	{
		iVertexBuffer* pVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																	eVertexBufferUsageType_Static);
		CreateStandardElements(pVtxBuffer);

		OhplMeshData *pData = ohpl_meshgen_cone(&avSize, alSections);
		FillVertexBuffer(pVtxBuffer, pData, true);
		ohpl_meshgen_free(pData);

		return CreateMeshFromBuffer(asName, CompileOrDelete(pVtxBuffer), asMaterial, false);
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cMeshCreator::CreateSkyBoxVertexBuffer(float afSize)
	{
		iVertexBuffer* pSkyBox = mpLowLevelGraphics->CreateVertexBuffer(
										eVertexBufferType_Hardware,
										eVertexBufferDrawType_Quad,eVertexBufferUsageType_Static);
		pSkyBox->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		pSkyBox->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);
		pSkyBox->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);

		OhplMeshData *pData = ohpl_meshgen_sky_box(afSize);
		FillVertexBuffer(pSkyBox, pData, false);
		ohpl_meshgen_free(pData);

		if(!pSkyBox->Compile(0))
		{
			hplDelete(pSkyBox);
			return NULL;
		}
		return pSkyBox;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cMeshCreator::CreateBoxVertexBuffer(cVector3f avSize)
	{
		iVertexBuffer* pBox = mpLowLevelGraphics->CreateVertexBuffer(
			eVertexBufferType_Hardware,
			eVertexBufferDrawType_Tri,eVertexBufferUsageType_Static);
		CreateStandardElements(pBox);

		OhplMeshData *pData = ohpl_meshgen_box(&avSize);
		FillVertexBuffer(pBox, pData, true);
		ohpl_meshgen_free(pData);

		return CompileOrDelete(pBox);
	}

	//-----------------------------------------------------------------------
}
