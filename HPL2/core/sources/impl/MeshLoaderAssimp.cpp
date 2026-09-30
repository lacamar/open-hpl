#include "impl/MeshLoaderAssimp.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Material.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"
#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"

#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"

#include <cstdio>
#include <cstring>
#include <map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace hpl {

	static cMatrixf ToMatrix(const aiMatrix4x4 &a_mtx)
	{
		return cMatrixf(a_mtx.a1, a_mtx.a2, a_mtx.a3, a_mtx.a4, a_mtx.b1, a_mtx.b2, a_mtx.b3, a_mtx.b4,
						a_mtx.c1, a_mtx.c2, a_mtx.c3, a_mtx.c4, a_mtx.d1, a_mtx.d2, a_mtx.d3, a_mtx.d4);
	}

	static bool SceneHasBones(const aiScene *apScene)
	{
		for(unsigned int i=0; i<apScene->mNumMeshes; ++i)
			if(apScene->mMeshes[i]->mNumBones > 0) return true;
		return false;
	}

	//-----------------------------------------------------------------------

	// Every node becomes a bone; bound bones take their skin bind pose.
	static void CreateBones(const aiNode *apNode, const aiMatrix4x4 &a_mtxParentNodeWorld, const aiMatrix4x4 &a_mtxParentBoneWorld,
							cBone *apParent, const std::map<tString, aiMatrix4x4> &amapBind)
	{
		tString sName = apNode->mName.C_Str();
		aiMatrix4x4 mtxWorld = a_mtxParentNodeWorld * apNode->mTransformation;
		std::map<tString, aiMatrix4x4>::const_iterator it = amapBind.find(sName);
		if(it != amapBind.end()) mtxWorld = it->second;

		aiMatrix4x4 mtxLocal = aiMatrix4x4(a_mtxParentBoneWorld).Inverse() * mtxWorld;
		cBone *pBone = apParent->CreateChildBone(sName, sName);
		pBone->SetTransform(ToMatrix(mtxLocal));

		for(unsigned int c=0; c<apNode->mNumChildren; ++c)
			CreateBones(apNode->mChildren[c], mtxWorld, mtxWorld, pBone, amapBind);
	}

	static void CollectBindPoses(const aiScene *apScene, const aiNode *apNode, const aiMatrix4x4 &a_mtxParent,
								 std::map<tString, aiMatrix4x4> &amapBind)
	{
		aiMatrix4x4 mtxWorld = a_mtxParent * apNode->mTransformation;
		for(unsigned int m=0; m<apNode->mNumMeshes; ++m)
		{
			const aiMesh *pSrc = apScene->mMeshes[apNode->mMeshes[m]];
			for(unsigned int b=0; b<pSrc->mNumBones; ++b)
				amapBind[pSrc->mBones[b]->mName.C_Str()] = mtxWorld * aiMatrix4x4(pSrc->mBones[b]->mOffsetMatrix).Inverse();
		}
		for(unsigned int c=0; c<apNode->mNumChildren; ++c)
			CollectBindPoses(apScene, apNode->mChildren[c], mtxWorld, amapBind);
	}

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
								cMaterialManager *apMaterialManager, const tString& asFallbackMaterial, cSkeleton *apSkeleton)
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

			if(apSkeleton)
			{
				// Unskinned parts ride on their own node
				if(pSrc->mNumBones == 0)
				{
					int lBone = apSkeleton->GetBoneIndexByName(apNode->mName.C_Str());
					for(unsigned int v=0; lBone >= 0 && v<pSrc->mNumVertices; ++v)
						pSubMesh->AddVertexBonePair(cVertexBonePair(v, lBone, 1.0f));
				}
				for(unsigned int b=0; b<pSrc->mNumBones; ++b)
				{
					const aiBone *pBone = pSrc->mBones[b];
					int lBone = apSkeleton->GetBoneIndexByName(pBone->mName.C_Str());
					if(lBone < 0) continue;
					for(unsigned int w=0; w<pBone->mNumWeights; ++w)
						pSubMesh->AddVertexBonePair(cVertexBonePair(pBone->mWeights[w].mVertexId, lBone, pBone->mWeights[w].mWeight));
				}
			}

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
			AddNodeMeshes(apScene, apNode->mChildren[c], mtxWorld, apMesh, apLowLevelGraphics, apMaterialManager, asFallbackMaterial, apSkeleton);
	}

	//-----------------------------------------------------------------------

	cMesh* cMeshLoaderAssimp::LoadMesh(const tWString& asFile, tMeshLoadFlag aFlags)
	{
		Assimp::Importer importer;
		// Keep FBX node names as-is (animation tracks target them)
		importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
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

		cSkeleton *pSkeleton = NULL;
		if(SceneHasBones(pScene))
		{
			std::map<tString, aiMatrix4x4> mapBind;
			CollectBindPoses(pScene, pScene->mRootNode, aiMatrix4x4(), mapBind);
			pSkeleton = hplNew( cSkeleton, () );
			for(unsigned int c=0; c<pScene->mRootNode->mNumChildren; ++c)
				CreateBones(pScene->mRootNode->mChildren[c], pScene->mRootNode->mTransformation, aiMatrix4x4(), pSkeleton->GetRootBone(), mapBind);
			pMesh->SetSkeleton(pSkeleton);
		}

		AddNodeMeshes(pScene, pScene->mRootNode, aiMatrix4x4(), pMesh, mpLowLevelGraphics, mpMaterialManager, sFallbackMaterial, pSkeleton);
		if(pSkeleton) pMesh->CompileBonesAndSubMeshes();

		if(pMesh->GetSubMeshNum() == 0)
		{
			Error("No triangle geometry in '%s'\n", cString::To8Char(asFile).c_str());
			hplDelete(pMesh);
			return NULL;
		}
		return pMesh;
	}

	//-----------------------------------------------------------------------
	//-----------------------------------------------------------------------

	// HPL3 bakes each FBX animation to a sibling .anm: bone tracks relative to the bind pose, FBX units
	cAnimation* cMeshLoaderAssimp::LoadAnimation(const tWString& asFile)
	{
		tString sAnm = cString::SetFileExt(cString::To8Char(asFile), "anm");
		FILE *pFile = fopen(sAnm.c_str(), "rb");
		if(pFile == NULL) return NULL;
		std::vector<unsigned char> vData;
		unsigned char vBuf[65536];
		size_t lRead;
		while((lRead = fread(vBuf, 1, sizeof(vBuf), pFile)) > 0) vData.insert(vData.end(), vBuf, vBuf + lRead);
		fclose(pFile);

		size_t lPos = 0;
		bool bOk = true;
		auto Read = [&](void *apDest, size_t alSize) {
			if(lPos + alSize > vData.size()) { bOk = false; memset(apDest, 0, alSize); return; }
			memcpy(apDest, &vData[lPos], alSize);
			lPos += alSize;
		};
		auto ReadString = [&]() {
			tString sStr;
			while(lPos < vData.size() && vData[lPos]) sStr += (char)vData[lPos++];
			if(lPos >= vData.size()) bOk = false;
			++lPos;
			return sStr;
		};

		char vMagic[4];
		unsigned int lVersion, lTrackNum, lExtraNum;
		float fLength;
		Read(vMagic, 4);
		Read(&lVersion, 4);
		if(bOk == false || memcmp(vMagic, "iE\x03v", 4) != 0) return NULL;
		ReadString();
		Read(&fLength, 4);
		Read(&lTrackNum, 4);
		Read(&lExtraNum, 4);
		if(bOk == false) return NULL;

		tString sFile = cString::To8Char(asFile);
		float fUnitScale = 1;
		{
			Assimp::Importer importer;
			const aiScene *pScene = importer.ReadFile(sFile, 0);
			float fUnit;
			double fUnitD;
			if(pScene && pScene->mMetaData)
			{
				if(pScene->mMetaData->Get("UnitScaleFactor", fUnit)) fUnitScale = fUnit / 100.0f;
				else if(pScene->mMetaData->Get("UnitScaleFactor", fUnitD)) fUnitScale = (float)fUnitD / 100.0f;
			}
		}
		cAnimation *pAnimation = hplNew( cAnimation, (cString::GetFileName(sFile), asFile, cString::GetFileName(sFile)) );
		pAnimation->SetAnimationName("Default");
		pAnimation->SetLength(fLength);
		pAnimation->ReserveTrackNum((int)lTrackNum);
		for(unsigned int t=0; t<lTrackNum && bOk; ++t)
		{
			tString sName = ReadString();
			unsigned short lFlags;
			unsigned int lKeyNum;
			Read(&lFlags, 2);
			Read(&lKeyNum, 4);
			cAnimationTrack *pTrack = pAnimation->CreateTrack(sName, eAnimTransformFlag_Translate | eAnimTransformFlag_Rotate);
			for(unsigned int k=0; k<lKeyNum && bOk; ++k)
			{
				float vKey[8];
				Read(vKey, sizeof(vKey));
				cKeyFrame *pKey = pTrack->CreateKeyFrame(vKey[0]);
				pKey->trans = cVector3f(vKey[1], vKey[2], vKey[3]) * fUnitScale;
				pKey->rotation = cQuaternion(vKey[7], vKey[4], vKey[5], vKey[6]);
			}
		}
		if(bOk == false)
		{
			Error("Corrupt animation '%s'\n", sAnm.c_str());
			hplDelete(pAnimation);
			return NULL;
		}
		return pAnimation;
	}

	//-----------------------------------------------------------------------
}
