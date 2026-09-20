#include "impl/MeshLoaderAssimp.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Material.h"

#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace hpl {

	//-----------------------------------------------------------------------

	cMeshLoaderAssimp::cMeshLoaderAssimp(iLowLevelGraphics *apLowLevelGraphics) : iMeshLoader(apLowLevelGraphics)
	{
		AddSupportedExtension("fbx");
	}

	//-----------------------------------------------------------------------

	static tString GetMaterialFile(const aiMaterial *apMaterial)
	{
		// Same convention as the Collada loader: <diffuse texture name>.mat
		aiString sTexture;
		if(apMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &sTexture) == AI_SUCCESS && sTexture.length > 0)
		{
			tString sPath = cString::ReplaceCharTo(tString(sTexture.C_Str()), "\\", "/");
			return cString::SetFileExt(cString::GetFileName(sPath), "mat");
		}
		aiString sName;
		if(apMaterial->Get(AI_MATKEY_NAME, sName) == AI_SUCCESS && sName.length > 0)
			return cString::SetFileExt(tString(sName.C_Str()), "mat");
		return "";
	}

	//-----------------------------------------------------------------------

	static void AddNodeMeshes(	const aiScene *apScene, const aiNode *apNode, const aiMatrix4x4 &a_mtxParent,
								cMesh *apMesh, iLowLevelGraphics *apLowLevelGraphics,
								cMaterialManager *apMaterialManager, const tString& asFallbackMaterial)
	{
		aiMatrix4x4 mtxWorld = a_mtxParent * apNode->mTransformation;
		aiMatrix3x3 mtxNormal(mtxWorld);
		mtxNormal.Inverse().Transpose();

		for(unsigned int m=0; m<apNode->mNumMeshes; ++m)
		{
			const aiMesh *pSrc = apScene->mMeshes[apNode->mMeshes[m]];
			if(pSrc->mPrimitiveTypes != aiPrimitiveType_TRIANGLE || pSrc->mNumVertices == 0 || pSrc->mNormals == NULL) continue;

			iVertexBuffer *pVtxBuff = apLowLevelGraphics->CreateVertexBuffer(
					eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static,
					(int)pSrc->mNumVertices, (int)pSrc->mNumFaces*3);
			pVtxBuff->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
			pVtxBuff->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
			pVtxBuff->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
			pVtxBuff->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
			pVtxBuff->CreateElementArray(eVertexBufferElement_Texture1Tangent, eVertexBufferElementFormat_Float, 4);

			for(unsigned int v=0; v<pSrc->mNumVertices; ++v)
			{
				aiVector3D vPos = mtxWorld * pSrc->mVertices[v];
				aiVector3D vNrm = (mtxNormal * pSrc->mNormals[v]).NormalizeSafe();

				pVtxBuff->AddVertexVec3f(eVertexBufferElement_Position, cVector3f(vPos.x, vPos.y, vPos.z));
				pVtxBuff->AddVertexVec3f(eVertexBufferElement_Normal, cVector3f(vNrm.x, vNrm.y, vNrm.z));

				cVector3f vUV(0);
				if(pSrc->mTextureCoords[0]) vUV = cVector3f(pSrc->mTextureCoords[0][v].x, pSrc->mTextureCoords[0][v].y, 0);
				pVtxBuff->AddVertexVec3f(eVertexBufferElement_Texture0, vUV);

				pVtxBuff->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1));

				cVector3f vTan(1,0,0);
				float fHandedness = 1;
				if(pSrc->mTangents && pSrc->mBitangents)
				{
					aiVector3D vT = (mtxNormal * pSrc->mTangents[v]).NormalizeSafe();
					aiVector3D vB = mtxNormal * pSrc->mBitangents[v];
					vTan = cVector3f(vT.x, vT.y, vT.z);
					fHandedness = ((vNrm ^ vT) * vB) < 0 ? -1.0f : 1.0f;
				}
				pVtxBuff->AddVertexVec4f(eVertexBufferElement_Texture1Tangent, vTan, fHandedness);
			}

			for(unsigned int f=0; f<pSrc->mNumFaces; ++f)
				for(int i=0; i<3; ++i) pVtxBuff->AddIndex(pSrc->mFaces[f].mIndices[i]);

			pVtxBuff->Compile(0);

			// .ent files reference sub meshes by the FBX node name.
			tString sName = apNode->mName.C_Str();
			if(apNode->mNumMeshes > 1) sName += "_" + cString::ToString((int)m);

			cSubMesh *pSubMesh = apMesh->CreateSubMesh(sName);
			pSubMesh->SetVertexBuffer(pVtxBuff);

			tString sMaterial = GetMaterialFile(apScene->mMaterials[pSrc->mMaterialIndex]);
			cMaterial *pMaterial = sMaterial != "" ? apMaterialManager->CreateMaterial(sMaterial) : NULL;
			if(pMaterial == NULL && asFallbackMaterial != "")
			{
				sMaterial = asFallbackMaterial;
				pMaterial = apMaterialManager->CreateMaterial(sMaterial);
			}
			pSubMesh->SetMaterialName(sMaterial);
			if(pMaterial) pSubMesh->SetMaterial(pMaterial);

			pSubMesh->Compile();
		}

		for(unsigned int c=0; c<apNode->mNumChildren; ++c)
			AddNodeMeshes(apScene, apNode->mChildren[c], mtxWorld, apMesh, apLowLevelGraphics, apMaterialManager, asFallbackMaterial);
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshLoaderAssimp::LoadMesh(const tWString& asFile, tMeshLoadFlag aFlags)
	{
		Assimp::Importer importer;
		const aiScene *pScene = importer.ReadFile(cString::To8Char(asFile),
				aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
				aiProcess_CalcTangentSpace | aiProcess_FlipUVs | aiProcess_SortByPType | aiProcess_GlobalScale);
		if(pScene == NULL || pScene->mRootNode == NULL)
		{
			Error("Could not load '%s': %s\n", cString::To8Char(asFile).c_str(), importer.GetErrorString());
			return NULL;
		}

		tString sMeshName = cString::GetFileName(cString::To8Char(asFile));
		cMesh *pMesh = hplNew( cMesh, (sMeshName, asFile, mpMaterialManager, mpAnimationManager) );

		// Frictional's convention: <mesh name>.mat next to the mesh.
		tString sFallbackMaterial = cString::SetFileExt(sMeshName, "mat");

		AddNodeMeshes(pScene, pScene->mRootNode, aiMatrix4x4(), pMesh, mpLowLevelGraphics, mpMaterialManager, sFallbackMaterial);

		if(pMesh->GetSubMeshNum() == 0)
		{
			Error("No triangle geometry in '%s'\n", cString::To8Char(asFile).c_str());
			hplDelete(pMesh);
			return NULL;
		}
		return pMesh;
	}

	//-----------------------------------------------------------------------
}
