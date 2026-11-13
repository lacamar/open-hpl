
#include "resources/WorldLoaderHpmBuilder.h"

#include <algorithm>

#include "system/String.h"
#include "system/LowLevelSystem.h"

#include "resources/Resources.h"
#include "resources/MeshManager.h"
#include "resources/MaterialManager.h"
#include "resources/TextureManager.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"
#include "resources/EngineFileLoading.h"
#include "resources/EntityLoader_Object.h"
#include "resources/RustWorldH4.h"
#include "resources/FileSearcher.h"
#include "physics/PhysicsMaterial.h"

#include "scene/Scene.h"
#include "scene/World.h"
#include "scene/MeshEntity.h"
#include "scene/SubMeshEntity.h"
#include "scene/LightDirectional.h"
#include "scene/BillBoard.h"
#include "scene/FogArea.h"
#include "scene/EnvironmentParticles.h"
#include "scene/ParticleSystem.h"
#include "scene/SoundEntity.h"

#include "system/Platform.h"

#include "graphics/Graphics.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/MeshCreator.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/MaterialType_Decal.h"
#include "graphics/MaterialType_BasicSolid.h"
#include "graphics/Texture.h"
#include "graphics/DecalCreator.h"
#include "scene/SubMeshEntity.h"

#include "physics/Physics.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "physics/CollideShape.h"

#include "math/Math.h"

#include "scene/Terrain.h"
#include <array>
#include <memory>
#include <cmath>
#include <cstring>

namespace hpl {

	namespace
	{
		const float* F3(const cVector3f& v) { return &v.x; }

		cMatrixf H4RotTrans(const cVector3f& avRot, const cVector3f& avPos)
		{
			if (OhplH4UseRust())
			{
				cMatrixf m;
				ohpl_h4_rot_trans(F3(avRot), F3(avPos), &m);
				return m;
			}
			cMatrixf m = cMath::MatrixRotate(avRot, eEulerRotationOrder_XYZ);
			m.SetTranslation(avPos);
			return m;
		}

		// world = rotate * scale, body = rotate + translation
		void H4StaticXform(const cVector3f& avRot, const cVector3f& avScale, const cVector3f& avPos, cMatrixf& a_mtxWorld, cMatrixf& a_mtxBody)
		{
			if (OhplH4UseRust())
			{
				OhplH4Xform x;
				ohpl_h4_static_xform(F3(avRot), F3(avScale), F3(avPos), &x);
				a_mtxWorld = x.world;
				a_mtxBody = x.body;
				return;
			}
			a_mtxWorld = cMath::MatrixMul(cMath::MatrixRotate(avRot, eEulerRotationOrder_XYZ), cMath::MatrixScale(avScale));
			a_mtxBody = cMath::MatrixRotate(avRot, eEulerRotationOrder_XYZ);
			a_mtxBody.SetTranslation(avPos);
		}

		// Object creation sequence for A/B runs: OHPL_H4_SEQDUMP=<file> appends one line per object.
		void H4Seq(const char* asType, const tString& asName, const tString& asFile, const cMatrixf* apMtx, const char* asFlags)
		{
			static FILE* pOut = [] { const char* s = getenv("OHPL_H4_SEQDUMP"); return s ? fopen(s, "a") : (FILE*)NULL; }();
			if (pOut == NULL) return;
			fprintf(pOut, "%s\t%s\t%s\t", asType, asName.c_str(), asFile.c_str());
			if (apMtx)
				for (int r = 0; r < 4; ++r)
					for (int c = 0; c < 4; ++c) fprintf(pOut, "%a%c", apMtx->m[r][c], r == 3 && c == 3 ? '\t' : ',');
			else fputs("-\t", pOut);
			fprintf(pOut, "%s\n", asFlags);
			fflush(pOut);
		}
	}

	// HPL3 ships prebaked static batches; build them here like cWorldLoaderHplMap to cut draw calls
	void cHpmWorldBuilder::CombineStaticMeshes()
	{
		typedef std::tuple<int, int, int, cMaterial*, bool, bool, std::vector<int>, std::array<float, 9>, bool, bool> tKey;
		typedef std::vector<std::pair<cMeshEntity*, cSubMeshEntity*> > tSubList;
		std::vector<std::pair<tKey, std::pair<cMeshEntity*, cSubMeshEntity*> > > vRecs;
		for (cMeshEntity* pEnt : mvCombineCandidates)
		{
			for (int i = 0; i < pEnt->GetSubMeshEntityNum(); ++i)
			{
				cSubMeshEntity* pSub = pEnt->GetSubMeshEntity(i);
				cMaterial* pMat = pSub->GetMaterial();
				iVertexBuffer* pVtx = pSub->GetVertexBuffer();
				if (pMat == NULL || pVtx == NULL || pSub->IsVisible() == false || pMat->HasWorldReflection()) continue;
				if (dynamic_cast<cMaterialType_SolidDiffuse*>(pMat->GetType()) == NULL) continue;
				if (pMat->GetVars() && static_cast<cMaterialType_SolidDiffuse_Vars*>(pMat->GetVars())->mbSwayActive) continue;
				if (pVtx->GetVertexNum() == 0 || pVtx->GetIndexNum() == 0) continue;
				const cMatrixf& m = pSub->GetWorldMatrix();
				if (cMath::Vector3Dot(cVector3f(m.m[0][0], m.m[1][0], m.m[2][0]),
									  cMath::Vector3Cross(cVector3f(m.m[0][1], m.m[1][1], m.m[2][1]), cVector3f(m.m[0][2], m.m[1][2], m.m[2][2]))) < 0)
					continue;

				std::vector<int> vLayout;
				bool bFloat = true;
				tVertexElementFlag lSeen = 0;
				for (int e = 0; e < eVertexBufferElement_LastEnum; ++e)
				{
					eVertexBufferElement elem = (eVertexBufferElement)e;
					tVertexElementFlag lFlag = GetVertexElementFlagFromEnum(elem);
					// Texture1Tangent and Texture1 share a flag
					if ((pVtx->GetVertexElementFlags() & lFlag) == 0 || (lSeen & lFlag)) continue;
					lSeen |= lFlag;
					eVertexBufferElementFormat fmt = pVtx->GetElementFormat(elem);
					if (fmt != eVertexBufferElementFormat_Float && (elem == eVertexBufferElement_Position || elem == eVertexBufferElement_Normal ||
																	 elem == eVertexBufferElement_Texture1Tangent))
						bFloat = false;
					vLayout.insert(vLayout.end(), {e, pVtx->GetElementNum(elem), fmt});
				}
				if (bFloat == false) continue;

				cVector3f vC = pSub->GetBoundingVolume()->GetWorldCenter() / 16.0f;
				const cColor& cm = pSub->GetColorMul();
				const cColor& ic = pSub->GetIlluminationColor();
				tKey key((int)floorf(vC.x), (int)floorf(vC.y), (int)floorf(vC.z), pMat, pSub->GetRenderFlagBit(eRenderableFlag_ShadowCaster),
						 pSub->GetIsOneSided(), vLayout, {cm.r, cm.g, cm.b, cm.a, ic.r, ic.g, ic.b, ic.a, pSub->GetIlluminationAmount()}, pSub->IsCulledByDistance(), pSub->IsCulledByFog());
				vRecs.push_back({key, {pEnt, pSub}});
			}
		}

		std::vector<std::pair<tKey, tSubList> > vGroups;
		std::vector<cMeshEntity*> vPlanEnts;
		OhplH4CombineOut plan = {};
		std::vector<uint32_t> vPlanOrder, vPlanSizes;
		std::unique_ptr<bool[]> pPlanDestroy;
		if (OhplH4UseRust())
		{
			std::map<cMeshEntity*, uint32_t> mapEntIdx;
			std::vector<uint32_t> vEntSubs;
			std::vector<int32_t> vPool;
			std::vector<OhplH4Cand> vCands;
			for (auto& [key, rec] : vRecs)
			{
				auto itEnt = mapEntIdx.insert({rec.first, (uint32_t)vPlanEnts.size()});
				if (itEnt.second)
				{
					vPlanEnts.push_back(rec.first);
					vEntSubs.push_back(rec.first->GetSubMeshEntityNum());
				}
				OhplH4Cand c = {};
				c.ent = itEnt.first->second;
				c.cell[0] = std::get<0>(key); c.cell[1] = std::get<1>(key); c.cell[2] = std::get<2>(key);
				c.mat = (uint64_t)(uintptr_t)std::get<3>(key);
				c.shadow = std::get<4>(key);
				c.oneSided = std::get<5>(key);
				c.layoutOff = (uint32_t)vPool.size();
				c.layoutLen = (uint32_t)std::get<6>(key).size();
				vPool.insert(vPool.end(), std::get<6>(key).begin(), std::get<6>(key).end());
				std::copy(std::get<7>(key).begin(), std::get<7>(key).end(), c.v);
				c.culledDist = std::get<8>(key);
				c.culledFog = std::get<9>(key);
				vCands.push_back(c);
			}
			vPlanOrder.resize(vCands.size());
			vPlanSizes.resize(vCands.size());
			pPlanDestroy.reset(new bool[vPlanEnts.size() + 1]());
			plan.order = vPlanOrder.data();
			plan.groupSizes = vPlanSizes.data();
			plan.destroy = pPlanDestroy.get();
			ohpl_h4_combine_plan(vCands.data(), vCands.size(), vPool.data(), vPool.size(), vEntSubs.data(), vEntSubs.size(), &plan);
			size_t lAt = 0;
			for (uint32_t g = 0; g < plan.numGroups; ++g)
			{
				tSubList vSubs;
				const size_t lFirst = lAt;
				for (uint32_t i = 0; i < vPlanSizes[g]; ++i) vSubs.push_back(vRecs[vPlanOrder[lAt++]].second);
				vGroups.push_back({vRecs[vPlanOrder[lFirst]].first, vSubs});
			}
		}
		else
		{
			std::map<tKey, tSubList> mapGroups;
			for (auto& [key, rec] : vRecs) mapGroups[key].push_back(rec);
			for (auto& [key, vSubs] : mapGroups)
				if (vSubs.size() >= 2) vGroups.push_back({key, vSubs});
		}

		std::map<cMeshEntity*, int> mapCombinedSubs;
		for (auto& [key, vSubs] : vGroups)
		{
			const std::vector<int>& vLayout = std::get<6>(key);
			int lVtxNum = 0, lIdxNum = 0;
			for (auto [pEnt, pSub] : vSubs)
			{
				lVtxNum += pSub->GetVertexBuffer()->GetVertexNum();
				lIdxNum += pSub->GetVertexBuffer()->GetIndexNum();
			}

			iVertexBuffer* pVtx = mpGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																				 eVertexBufferUsageType_Static, lVtxNum, lIdxNum);
			tVertexElementFlag lFlags = 0;
			for (size_t e = 0; e < vLayout.size(); e += 3)
			{
				eVertexBufferElement elem = (eVertexBufferElement)vLayout[e];
				lFlags |= GetVertexElementFlagFromEnum(elem);
				pVtx->CreateElementArray(elem, (eVertexBufferElementFormat)vLayout[e + 2], vLayout[e + 1],
										 vSubs[0].second->GetVertexBuffer()->GetElementProgramVarIndex(elem));
				pVtx->ResizeArray(elem, lVtxNum * vLayout[e + 1]);
			}
			pVtx->ResizeIndices(lIdxNum);

			int lVtxOffset = 0, lIdxOffset = 0;
			unsigned int* pIdx = pVtx->GetIndices();
			for (auto [pEnt, pSub] : vSubs)
			{
				iVertexBuffer* pSrc = pSub->GetVertexBuffer()->CreateCopy(eVertexBufferType_Software, eVertexBufferUsageType_Static, lFlags);
				pSrc->Transform(pSub->GetWorldMatrix());
				for (size_t e = 0; e < vLayout.size(); e += 3)
				{
					eVertexBufferElement elem = (eVertexBufferElement)vLayout[e];
					size_t lStride = vLayout[e + 1] * (vLayout[e + 2] == eVertexBufferElementFormat_Byte ? 1 : 4);
					memcpy(pVtx->GetByteArray(elem) + lVtxOffset * lStride, pSrc->GetByteArray(elem), pSrc->GetVertexNum() * lStride);
				}
				unsigned int* pSrcIdx = pSrc->GetIndices();
				for (int i = 0; i < pSrc->GetIndexNum(); ++i) pIdx[lIdxOffset + i] = pSrcIdx[i] + lVtxOffset;
				lVtxOffset += pSrc->GetVertexNum();
				lIdxOffset += pSrc->GetIndexNum();
				hplDelete(pSrc);
				pSub->SetVisible(false);
				++mapCombinedSubs[pEnt];
			}
			pVtx->Compile(0);

			tString sName = "CombinedMesh" + cString::ToString(mlCombinedMeshes++);
			cMesh* pMesh = hplNew(cMesh, (sName, _W(""), mpResources->GetMaterialManager(), mpResources->GetAnimationManager()));
			cSubMesh* pSubMesh = pMesh->CreateSubMesh("SubMesh");
			pSubMesh->SetVertexBuffer(pVtx);
			cMaterial* pMat = std::get<3>(key);
			pMat->IncUserCount();
			pSubMesh->SetMaterial(pMat);
			pSubMesh->SetMaterialName(vSubs[0].second->GetSubMesh()->GetMaterialName());
			pSubMesh->Compile();

			cMeshEntity* pEnt = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
			H4Seq("combined", sName, pMat->GetName(), NULL, (cString::ToString((int)vSubs.size()) + (std::get<4>(key) ? ",shadow" : "")).c_str());
			const std::array<float, 9>& v = std::get<7>(key);
			pEnt->SetRenderFlagBit(eRenderableFlag_ShadowCaster, std::get<4>(key));
			pEnt->SetCulledByDistance(std::get<8>(key));
			pEnt->SetCulledByFog(std::get<9>(key));
			pEnt->SetColorMul(cColor(v[0], v[1], v[2], v[3]));
			pEnt->SetIlluminationColor(cColor(v[4], v[5], v[6], v[7]));
			pEnt->SetIlluminationAmount(v[8]);
			if (mpObjectDoneCallback) mpObjectDoneCallback(mpCurrentWorld);
		}

		std::set<cMeshEntity*> setDestroy;
		int lCombinedSubs = 0;
		if (OhplH4UseRust())
		{
			for (size_t i = 0; i < vPlanEnts.size(); ++i)
				if (pPlanDestroy[i]) setDestroy.insert(vPlanEnts[i]);
			lCombinedSubs = (int)plan.combinedSubs;
		}
		else
		{
			for (auto& [pEnt, lNum] : mapCombinedSubs)
				if (lNum == pEnt->GetSubMeshEntityNum()) setDestroy.insert(pEnt);
			for (auto& p : mapCombinedSubs) lCombinedSubs += p.second;
		}
		mpCurrentWorld->DestroyStaticMeshEntities(setDestroy);
		::hpl::Log("  SOMA hpm combined %d static submeshes into %d meshes\n", lCombinedSubs, mlCombinedMeshes);
	}

	namespace
	{
		struct cUndergrowthMat
		{
			cMaterial* mpMat;
			std::vector<cVector3f> mvPos, mvUv;
			std::vector<unsigned int> mvIdx;
			cVector2f mvSubDiv;
			cColor mMinColor, mMaxColor;
			float mfDensity, mfAlign;
			cVector3f mvBaseSize, mvMinSizeMul, mvMaxSizeMul;
		};

		struct cUndergrowthArea
		{
			int mlMat;
			bool mbSub;
			float mfMaxInfluence, mfFadeBorder, mfRadius = 0;
			cVector2f mvCenter, mvMin, mvMax;
			std::vector<cVector3f> mvPlanes;

			float Influence(const cVector2f& avP) const
			{
				float e = 1e8f;
				if (mvPlanes.empty())
				{
					float d2 = (avP - mvCenter).SqrLength();
					if (d2 > mfRadius * mfRadius) return 0;
					e = mfRadius - sqrtf(d2);
				}
				else for (const cVector3f& vPlane : mvPlanes)
				{
					float d = vPlane.x * avP.x + vPlane.y * avP.y + vPlane.z;
					if (d < 0) return 0;
					e = std::min(e, d);
				}
				return e > mfFadeBorder ? mfMaxInfluence : e / mfFadeBorder * mfMaxInfluence;
			}
		};
	}

	// HPL3 iHplMapLoader::CreateStaticMeshEntity: a sibling .ent's bodies replace the mesh collider
	bool cHpmWorldBuilder::CreateStaticBodiesFromEnt(const tString& asFile, const cMatrixf& a_mtxTransform, const cVector3f& avScale, const tString& asName)
	{
		tWString sPath = mpResources->GetFileSearcher()->GetFilePath(asFile);
		if (sPath == _W("")) return false;
		sPath = cString::SetFileExtW(sPath, _W("ent"));
		if (cPlatform::FileExists(sPath) == false) return false;

		iXmlDocument* pDoc = mpResources->GetLowLevel()->CreateXmlDocument();
		cXmlElement* pModel = pDoc->CreateFromFile(sPath) ? pDoc->GetFirstElement("ModelData") : NULL;
		cXmlElement* pShapes = pModel ? pModel->GetFirstElement("Shapes") : NULL;
		cXmlElement* pBodies = pModel ? pModel->GetFirstElement("Bodies") : NULL;
		bool bCreated = false;
		if (pShapes && pBodies)
		{
			std::map<int, cXmlElement*> mapShapes;
			cXmlNodeListIterator it = pShapes->GetChildIterator();
			while (it.HasNext())
			{
				cXmlElement* pElem = it.Next()->ToElement();
				mapShapes[pElem->GetAttributeInt("ID")] = pElem;
			}

			cXmlNodeListIterator bodyIt = pBodies->GetChildIterator();
			while (bodyIt.HasNext())
			{
				cXmlElement* pBodyElem = bodyIt.Next()->ToElement();
				cMatrixf mtxBody = cMath::MatrixRotate(pBodyElem->GetAttributeVector3f("Rotation"), eEulerRotationOrder_XYZ);
				mtxBody.SetTranslation(pBodyElem->GetAttributeVector3f("WorldPos") * avScale);
				mtxBody = cMath::MatrixMul(a_mtxTransform, mtxBody);

				// HPL3 iHplMapLoader::CombineStaticBodies: merged by flags, material and an 8x8x24 cell
				cVector3f vPos = mtxBody.GetTranslation();
				int32_t vCellI[3] = {(int)std::floor(vPos.x / 8), (int)std::floor(vPos.y / 8), (int)std::floor(vPos.z / 24)};
				if (OhplH4UseRust()) ohpl_h4_body_cell(F3(vPos), vCellI);
				tStaticShapeBatchKey key(pBodyElem->GetAttributeString("Material"), pBodyElem->GetAttributeBool("CollideCharacter", true),
										 pBodyElem->GetAttributeBool("CollideNonCharacter", true), pBodyElem->GetAttributeBool("BlocksSound", false),
										 vCellI[0], vCellI[1], vCellI[2]);
				cStaticShapeBatch& batch = m_mapStaticShapeBatches[key];
				cXmlNodeListIterator shapeIt = pBodyElem->GetChildIterator();
				while (shapeIt.HasNext())
				{
					cXmlElement* pElem = shapeIt.Next()->ToElement();
					if (pElem->GetValue() != "Shape") continue;
					std::map<int, cXmlElement*>::iterator itShape = mapShapes.find(pElem->GetAttributeInt("ID"));
					iCollideShape* pShape = itShape != mapShapes.end() ? CreateCollideShape(itShape->second, mpCurrentPhysicsWorld, avScale, mtxBody) : NULL;
					if (pShape == NULL) continue;
					if (batch.mvShapes.empty()) batch.msName = asName;
					batch.mvShapes.push_back(pShape);
					bCreated = true;
				}
			}
		}
		hplDelete(pDoc);
		return bCreated;
	}

	// HPL3 iHplMapLoader::CombineObjectsAndCreatePhysics: one body per physics material and area, not per submesh
	// HPL3 iHplMapLoader::CreateSubMeshShapeBodies: submesh colliders and _collider/_charcollider meshes
	void cHpmWorldBuilder::CreateStaticBodyForMesh(cMeshEntity* apMeshEntity, const cMatrixf& a_mtxTransform, const cVector3f& avScale)
	{
		const bool bRust = OhplH4UseRust();
		bool bHasColliderMesh = false;
		for (int i = 0; i < apMeshEntity->GetSubMeshEntityNum(); ++i)
		{
			const tString& sSubName = apMeshEntity->GetSubMeshEntity(i)->GetSubMesh()->GetName();
			bHasColliderMesh |= bRust ? ohpl_h4_is_collider_name(sSubName.c_str()) : cString::Sub(sSubName, 0, 9) == "_collider";
		}

		for (int i = 0; i < apMeshEntity->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity* pSubEnt = apMeshEntity->GetSubMeshEntity(i);
			cSubMesh* pSubMesh = pSubEnt->GetSubMesh();
			tString sPhysMat = pSubEnt->GetMaterial() ? pSubEnt->GetMaterial()->GetPhysicsMaterial() : "";
			OhplH4SubMesh subPlan = {};
			if (bRust) ohpl_h4_sub_mesh_plan(pSubMesh->GetName().c_str(), &subPlan);
			bool bCharMesh = bRust ? subPlan.isCharColliderMesh : cString::Sub(pSubMesh->GetName(), 0, 13) == "_charcollider";
			bool bColliderMesh = bRust ? subPlan.isColliderMesh : (bCharMesh || cString::Sub(pSubMesh->GetName(), 0, 9) == "_collider");
			bool bTriangles = !bHasColliderMesh;
			if (bRust ? subPlan.hide : (bColliderMesh || cString::Sub(pSubMesh->GetName(), 0, 16) == "_noncharcollider")) pSubEnt->SetVisible(false);

			std::vector<iCollideShape*> vShapes[2];
			std::unique_ptr<bool[]> vIsChar(new bool[pSubMesh->GetColliderNum() + 1]());
			for (int j = 0; j < pSubMesh->GetColliderNum(); ++j)
			{
				cMeshCollider* pCollider = pSubMesh->GetCollider(j);
				vIsChar[j] = pCollider->mbCharCollider;
				if (pCollider->mbCharCollider == false) bTriangles = false;
				cMatrixf mtxOffset = pCollider->m_mtxOffset;
				mtxOffset.SetTranslation(mtxOffset.GetTranslation() * avScale);
				if (iCollideShape* pShape = cSubMesh::CreateCollideShapeFromCollider(pCollider, mpCurrentPhysicsWorld, avScale, &mtxOffset))
					vShapes[pCollider->mbCharCollider].push_back(pShape);
			}
			for (int j = 0; j < 2; ++j)
			{
				if (vShapes[j].empty()) continue;
				iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody("ShapeBody", vShapes[j].size() == 1 ? vShapes[j][0] : mpCurrentPhysicsWorld->CreateCompundShape(vShapes[j]));
				pBody->SetMass(0);
				pBody->SetCollide(j == 0);
				pBody->SetBlocksLight(pSubEnt->GetRenderFlagBit(eRenderableFlag_ShadowCaster));
				if (iPhysicsMaterial* pMat = mpCurrentPhysicsWorld->GetMaterialFromName(sPhysMat)) pBody->SetMaterial(pMat);
				pBody->SetMatrix(a_mtxTransform);
			}

			iVertexBuffer* pSrcVtx = pSubMesh->GetVertexBuffer();
			if (pSrcVtx == NULL) continue;
			if (bRust ? ohpl_h4_sub_mesh_batches(bHasColliderMesh, bColliderMesh, vIsChar.get(), pSubMesh->GetColliderNum()) == false
					  : (bTriangles || bColliderMesh) == false) continue;

			int32_t vCellI[3];
			if (bRust)
			{
				cVector3f vCenter = pSubEnt->GetBoundingVolume()->GetWorldCenter();
				ohpl_h4_mesh_cell(F3(vCenter), vCellI);
			}
			else
			{
				cVector3f vCell = pSubEnt->GetBoundingVolume()->GetWorldCenter() / 16.0f;
				vCellI[0] = (int)std::floor(vCell.x); vCellI[1] = (int)std::floor(vCell.y); vCellI[2] = (int)std::floor(vCell.z);
			}
			tStaticBatchKey key(sPhysMat, bCharMesh, vCellI[0], vCellI[1], vCellI[2]);
			cStaticBatch& batch = m_mapStaticBatches[key];
			if (bRust ? ohpl_h4_batch_must_flush(batch.mvIdx.size(), pSrcVtx->GetIndexNum()) : batch.mvIdx.size() + pSrcVtx->GetIndexNum() > 50000)
				FlushStaticBatch(key, batch);

			iVertexBuffer* pVtx = pSrcVtx->CreateCopy(eVertexBufferType_Software, eVertexBufferUsageType_Static, eVertexElementFlag_Position);
			pVtx->Transform(pSubEnt->GetWorldMatrix());
			unsigned int lBase = (unsigned int)batch.mvPos.size() / 3;
			int lStride = pVtx->GetElementNum(eVertexBufferElement_Position);
			const float* pPos = pVtx->GetFloatArray(eVertexBufferElement_Position);
			for (int v = 0; v < pVtx->GetVertexNum(); ++v) batch.mvPos.insert(batch.mvPos.end(), pPos + v * lStride, pPos + v * lStride + 3);
			const unsigned int* pIdx = pVtx->GetIndices();
			for (int j = 0; j < pVtx->GetIndexNum(); ++j) batch.mvIdx.push_back(pIdx[j] + lBase);
			hplDelete(pVtx);
		}
	}

	void cHpmWorldBuilder::FlushStaticBatch(const tStaticBatchKey& aKey, cStaticBatch& aBatch)
	{
		if (aBatch.mvIdx.empty()) return;
		iVertexBuffer* pVtx = mpGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Software, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static,
																			  (int)aBatch.mvPos.size() / 3, (int)aBatch.mvIdx.size());
		pVtx->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 3);
		pVtx->ResizeArray(eVertexBufferElement_Position, (int)aBatch.mvPos.size());
		std::copy(aBatch.mvPos.begin(), aBatch.mvPos.end(), pVtx->GetFloatArray(eVertexBufferElement_Position));
		pVtx->ResizeIndices((int)aBatch.mvIdx.size());
		std::copy(aBatch.mvIdx.begin(), aBatch.mvIdx.end(), pVtx->GetIndices());
		pVtx->Compile(0);
		iCollideShape* pShape = mpCurrentPhysicsWorld->CreateMeshShape(pVtx);
		hplDelete(pVtx);
		if (pShape)
		{
			H4Seq("batch", "CombinedObjects" + cString::ToString(mlCombinedObjects), std::get<0>(aKey), NULL, std::get<1>(aKey) ? "char" : "-");
			iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody("CombinedObjects" + cString::ToString(mlCombinedObjects++), pShape);
			pBody->SetMass(0);
			pBody->SetCollide(std::get<1>(aKey) == false);
			if (iPhysicsMaterial* pMat = mpCurrentPhysicsWorld->GetMaterialFromName(std::get<0>(aKey))) pBody->SetMaterial(pMat);
		}
		aBatch = cStaticBatch();
	}

	cHpmWorldBuilder::cHpmWorldBuilder(cWorld* apWorld, iPhysicsWorld* apPhysicsWorld, cResources* apResources, cGraphics* apGraphics,
									   void (*apObjectDone)(cWorld*), cXmlElement** appCurrentElement)
	{
		mpCurrentWorld = apWorld;
		mpCurrentPhysicsWorld = apPhysicsWorld;
		mpResources = apResources;
		mpGraphics = apGraphics;
		mpObjectDoneCallback = apObjectDone;
		mppCurrentElement = appCurrentElement;
		mlCombinedObjects = 0;
		mlCombinedMeshes = 0;
		mbTerrainActive = false;
	}

	void cHpmWorldBuilder::Finish()
	{
		const bool bNoCombine = getenv("OPENHPL_NO_MESH_COMBINE") != NULL;
		if (OhplH4UseRust())
		{
			OhplH4FinishStep vSteps[8];
			size_t lSteps = ohpl_h4_finish_steps(bNoCombine, vSteps);
			for (size_t i = 0; i < lSteps; ++i)
			{
				switch (vSteps[i])
				{
				case OhplH4Finish_FlushStaticBatches:
					for (auto& [key, batch] : m_mapStaticBatches)
					{
						FlushStaticBatch(key, batch);
						if (mpObjectDoneCallback) mpObjectDoneCallback(mpCurrentWorld);
					}
					m_mapStaticBatches.clear();
					break;
				case OhplH4Finish_BuildShapeBodies: BuildShapeBodies(); break;
				case OhplH4Finish_CombineStaticMeshes: CombineStaticMeshes(); mvCombineCandidates.clear(); break;
				case OhplH4Finish_ObjectDone:
					if (mpObjectDoneCallback) mpObjectDoneCallback(mpCurrentWorld);
					break;
				case OhplH4Finish_CompileWorld: mpCurrentWorld->Compile(true); break;
				}
			}
			mvCombineCandidates.clear();
			return;
		}
		for (auto& [key, batch] : m_mapStaticBatches)
		{
			FlushStaticBatch(key, batch);
			if (mpObjectDoneCallback) mpObjectDoneCallback(mpCurrentWorld);
		}
		m_mapStaticBatches.clear();
		BuildShapeBodies();
		if (bNoCombine == false) CombineStaticMeshes();
		mvCombineCandidates.clear();
		if (mpObjectDoneCallback) mpObjectDoneCallback(mpCurrentWorld);
		mpCurrentWorld->Compile(true);
	}

	void cHpmWorldBuilder::BuildShapeBodies()
	{
		for (auto& [key, batch] : m_mapStaticShapeBatches)
		{
			H4Seq("shapebody", batch.msName, std::get<0>(key), NULL, cString::ToString((int)batch.mvShapes.size()).c_str());
			iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody(batch.msName, batch.mvShapes.size() == 1 ? batch.mvShapes[0] : mpCurrentPhysicsWorld->CreateCompundShape(batch.mvShapes));
			pBody->SetMass(0);
			pBody->SetCollideCharacter(std::get<1>(key));
			pBody->SetCollide(std::get<2>(key));
			pBody->SetBlocksSound(std::get<3>(key));
			if (iPhysicsMaterial* pMat = mpCurrentPhysicsWorld->GetMaterialFromName(std::get<0>(key))) pBody->SetMaterial(pMat);
		}
		m_mapStaticShapeBatches.clear();
	}

	uint64_t cHpmWorldBuilder::NowMs()
	{
		return cPlatform::GetApplicationTime();
	}

	void cHpmWorldBuilder::ObjectDone()
	{
		if (mpObjectDoneCallback) mpObjectDoneCallback(mpCurrentWorld);
	}

	void cHpmWorldBuilder::Warn(const char* asMessage)
	{
		Warning("%s", asMessage);
	}

	void cHpmWorldBuilder::Log(const char* asMessage)
	{
		::hpl::Log("%s", asMessage);
	}

	int cHpmWorldBuilder::ReadFile(const tWString& asPath, std::vector<char>& avData)
	{
		FILE* pFile = cPlatform::OpenFile(asPath, _W("rb"));
		if (pFile == NULL) return 0;
		fseek(pFile, 0, SEEK_END);
		long lLength = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		bool bRead = true;
		if (lLength > 0)
		{
			avData.resize(lLength);
			bRead = fread(avData.data(), lLength, 1, pFile) == 1;
		}
		fclose(pFile);
		return bRead ? 1 : 2;
	}

	void cHpmWorldBuilder::GlobalFog(const OhplHpmFog& f)
	{
		mpCurrentWorld->SetFogActive(f.active);
		mpCurrentWorld->SetFogColor(cColor(f.color[0], f.color[1], f.color[2], f.color[3]));
		mpCurrentWorld->SetFogFalloffExp(f.falloffExp);
		mpCurrentWorld->SetFogStart(f.fadeStart);
		mpCurrentWorld->SetFogEnd(f.fadeEnd);
		mpCurrentWorld->SetFogCulling(f.culling);
		mpCurrentWorld->SetFogBrightness(f.brightness);
		mpCurrentWorld->SetFogUnderwater(f.underwater);
		mpCurrentWorld->SetFogUseSkybox(f.useSkybox);
		mpCurrentWorld->SetFogApplyAfterFogAreas(f.applyAfterFogAreas);
		mpCurrentWorld->SetFogNoise(f.noiseStrength, f.noiseSize, cVector3f(f.noiseTurbulence[0], f.noiseTurbulence[1], f.noiseTurbulence[2]));
		mpCurrentWorld->SetSecondaryFogActive(f.secondaryActive);
		mpCurrentWorld->SetSecondaryFogColor(cColor(f.secondaryColor[0], f.secondaryColor[1], f.secondaryColor[2], f.secondaryColor[3]));
		mpCurrentWorld->SetSecondaryFogStart(f.secondaryFadeStart);
		mpCurrentWorld->SetSecondaryFogEnd(f.secondaryFadeEnd);
		mpCurrentWorld->SetSecondaryFogFalloffExp(f.secondaryFalloffExp);
		mpCurrentWorld->SetFogLighten(f.lighten);
		mpCurrentWorld->SetFogHeightBased(f.heightBased);
		mpCurrentWorld->SetFogExponential(f.exponential);
		mpCurrentWorld->SetFogDensity(f.density);
		mpCurrentWorld->SetFogHeightDensity(f.heightDensity);
		mpCurrentWorld->SetFogHeightHorizon(f.heightHorizon);
		mpCurrentWorld->SetSecondaryFogDensity(f.secondaryDensity);
		mpCurrentWorld->SetSecondaryFogHeightDensity(f.secondaryHeightDensity);
		mpCurrentWorld->SetSecondaryFogHeightHorizon(f.secondaryHeightHorizon);
	}

	void cHpmWorldBuilder::DistanceCull(const OhplHpmDistanceCull& c)
	{
		mpCurrentWorld->SetDistanceCullActive(c.active);
		mpCurrentWorld->SetDistanceCullMinRange(c.minRange);
		mpCurrentWorld->SetDistanceCullScreenSize(c.screenSize);
		mpCurrentWorld->SetDistanceCullRandomSize(c.randomSize);
	}

	void cHpmWorldBuilder::DirLight(const OhplHpmDirLight& d)
	{
		cLightDirectional* pLight = mpCurrentWorld->GetDirectionalLight();
		mpCurrentWorld->SetDirectionalLightActive(d.active);
		pLight->SetDiffuseColor(cColor(d.diffuseColor[0], d.diffuseColor[1], d.diffuseColor[2], d.diffuseColor[3]));
		pLight->SetBrightness(d.brightness);
		pLight->SetDefaultBrightness(pLight->GetBrightness());
		pLight->SetDirection(cVector3f(d.direction[0], d.direction[1], d.direction[2]));
		pLight->SetAmbientColorSky(cColor(d.skyColor[0], d.skyColor[1], d.skyColor[2], d.skyColor[3]));
		pLight->SetAmbientColorGround(cColor(d.groundColor[0], d.groundColor[1], d.groundColor[2], d.groundColor[3]));
		pLight->SetCastShadows(d.castShadows);
		pLight->SetShadowCasterDistance(d.shadowCasterDist);
		pLight->SetShadowMapBiasMul(d.shadowMapBiasMul);
		pLight->SetShadowMapSlopeScaleBiasMul(d.shadowMapSlopeScaleBiasMul);
		pLight->SetShadowMapBlurAmount(d.shadowBlurAmount);
		pLight->SetAutoShadowSliceSettings(d.autoShadowSliceSettings);
		pLight->SetAutoShadowSliceLogTerm(d.autoShadowSliceLogTerm);
	}

	void cHpmWorldBuilder::EnvParticles(const OhplHpmEnvParticles& e)
	{
		tString sTex = e.texture;
		mpResources->GetTextureManager()->SetSRGB(true);
		iTexture* pTex = sTex == "" ? NULL : mpResources->GetTextureManager()->Create2D(sTex, true);
		mpResources->GetTextureManager()->SetSRGB(false);
		cVector2f vSubDiv(e.subDivUv[0], e.subDivUv[1]);
		cEnvironmentParticles* pParticles = mpCurrentWorld->CreateEnvironmentParticles("WorldEnvParticles");
		pParticles->Setup(e.boxSize, e.numParticles, cVector2f(e.particleSize[0], e.particleSize[1]), cVector2l((int)vSubDiv.x, (int)vSubDiv.y),
						  e.affectedByLight, pTex);
		pParticles->SetColor(cColor(e.color[0], e.color[1], e.color[2], e.color[3]));
		pParticles->SetBrightness(e.brightness);
		pParticles->SetBoxDistance(e.boxDistance);
		pParticles->SetGravityVelocity(cVector3f(e.gravityVelocity[0], e.gravityVelocity[1], e.gravityVelocity[2]));
		pParticles->SetGravitySpeedRandomAmount(e.gravitySpeedRandomAmount);
		pParticles->SetWindVelocity(cVector3f(e.windVelocity[0], e.windVelocity[1], e.windVelocity[2]));
		pParticles->SetWindSpeedRandomAmount(e.windSpeedRandomAmount);
		pParticles->SetWindDirectionRandomAmount(e.windDirRandomAmount);
		pParticles->SetRotateVelocity(cVector3f(e.rotateVelocity[0], e.rotateVelocity[1], e.rotateVelocity[2]));
		pParticles->SetRotateSpeedRandomAmount(e.rotateSpeedRandomAmount);
		pParticles->SetRotateSpeedRandomBothDirs(e.rotateBothDirs);
		pParticles->SetIterationNum(e.numIterations);
		pParticles->SetFadeInStart(e.fadeInStart);
		pParticles->SetFadeInEnd(e.fadeInEnd);
		pParticles->SetFadeOutStart(e.fadeOutStart);
		pParticles->SetFadeOutEnd(e.fadeOutEnd);
		mpCurrentWorld->SetEnvironmentParticlesActive(e.active);
	}

	void cHpmWorldBuilder::PostEffects(const OhplHpmPostEffects& p)
	{
		mpCurrentWorld->SetToneMapping(p.toneMappingKey, p.toneMappingExposure, p.toneMappingWhiteCut);
		mpCurrentWorld->SetColorGradingTexture(p.colorGradingTexture);
	}

	void cHpmWorldBuilder::SkyBox(const OhplHpmSkyBox& s)
	{
		mpCurrentWorld->SetSkyBoxActive(s.active);
		mpCurrentWorld->SetSkyBoxColor(cColor(s.color[0], s.color[1], s.color[2], s.color[3]));
		mpCurrentWorld->SetSkyBoxBrightness(s.brightness);

		tString sSkyTex = s.texture;
		if (sSkyTex != "")
		{
			mpResources->GetTextureManager()->SetSRGB(true);
			iTexture* pSkyTexture = mpResources->GetTextureManager()->CreateCubeMap(sSkyTex, false);
			mpResources->GetTextureManager()->SetSRGB(false);
			if (pSkyTexture) mpCurrentWorld->SetSkyBox(pSkyTexture, true);
			else Warning("SOMA hpm: could not load skybox texture '%s'\n", sSkyTex.c_str());
		}
	}

	bool cHpmWorldBuilder::StaticObject(const OhplHpmStaticObject& o)
	{
		tString sName = o.name;
		tString sFileName = o.filename;
		cVector3f vPosition(o.position[0], o.position[1], o.position[2]);
		cVector3f vScale(o.scale[0], o.scale[1], o.scale[2]);
		cVector3f vRotation(o.rotation[0], o.rotation[1], o.rotation[2]);

		cMesh* pMesh = mpResources->GetMeshManager()->CreateMesh(sFileName);
		if (pMesh == NULL)
		{
			Warning("SOMA hpm: could not load mesh '%s' for static object '%s'\n", sFileName.c_str(), sName.c_str());
			return false;
		}

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, o.castsShadows);
		pMeshEntity->SetCulledByDistance(o.culledByDistance);
		pMeshEntity->SetCulledByFog(o.culledByFog);
		pMeshEntity->SetUniqueID(o.id);
		pMeshEntity->SetColorMul(cColor(o.colorMul[0], o.colorMul[1], o.colorMul[2], o.colorMul[3]));
		pMeshEntity->SetIlluminationAmount(o.illumBrightness);
		pMeshEntity->SetIlluminationColor(cColor(o.illumColor[0], o.illumColor[1], o.illumColor[2], o.illumColor[3]));

		cMatrixf mtxWorld, mtxTransform;
		H4StaticXform(vRotation, vScale, vPosition, mtxWorld, mtxTransform);
		pMeshEntity->SetWorldMatrix(mtxWorld);
		pMeshEntity->SetPosition(vPosition);
		tFloatVec vPose(o.pose, o.pose + o.numPose);
		const bool bPoseApplied = o.hasPose && pMeshEntity->SetBoneRestPoseFromMapValues(vPose);
		const bool bCandidate = OhplH4UseRust() ? ohpl_h4_is_combine_candidate(o.hasPose, bPoseApplied) : (o.hasPose == false || bPoseApplied == false);
		if (bCandidate) mvCombineCandidates.push_back(pMeshEntity);
		H4Seq("static", sName, sFileName, &mtxTransform, o.collides ? "collides" : "-");

		if (o.collides)
		{
			if (CreateStaticBodiesFromEnt(sFileName, mtxTransform, vScale, sName) == false)
				CreateStaticBodyForMesh(pMeshEntity, mtxTransform, vScale);
		}

		return true;
	}

	bool cHpmWorldBuilder::Plane(const OhplHpmPlane& p)
	{
		tString sName = p.name;
		tString sMaterial = p.material;
		cVector3f vPosition(p.position[0], p.position[1], p.position[2]);
		cVector3f vScale(p.scale[0], p.scale[1], p.scale[2]);
		cVector3f vRotation(p.rotation[0], p.rotation[1], p.rotation[2]);

		cMesh* pMesh = mpGraphics->GetMeshCreator()->CreatePlane(sName, cVector3f(p.startCorner[0], p.startCorner[1], p.startCorner[2]),
																   cVector3f(p.endCorner[0], p.endCorner[1], p.endCorner[2]),
																   cVector2f(p.uv[0][0], p.uv[0][1]), cVector2f(p.uv[1][0], p.uv[1][1]),
																   cVector2f(p.uv[2][0], p.uv[2][1]), cVector2f(p.uv[3][0], p.uv[3][1]), sMaterial);
		if (pMesh == NULL)
		{
			Warning("SOMA hpm: could not create plane primitive '%s'\n", sName.c_str());
			return false;
		}

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, p.castsShadows);
		pMeshEntity->SetCulledByDistance(p.culledByDistance);
		pMeshEntity->SetCulledByFog(p.culledByFog);
		pMeshEntity->GetSubMeshEntity(0)->GetSubMesh()->SetMaterialName(sMaterial);
		pMeshEntity->SetUniqueID(p.id);

		cMatrixf mtxWorld, mtxBody;
		H4StaticXform(vRotation, vScale, vPosition, mtxWorld, mtxBody);
		pMeshEntity->SetWorldMatrix(mtxWorld);
		pMeshEntity->SetPosition(vPosition);
		H4Seq("plane", sName, sMaterial, &mtxBody, p.collides ? "collides" : "-");

		if (p.collides)
			CreateStaticBodyForMesh(pMeshEntity, cMatrixf::Identity, 1);

		return true;
	}

	bool cHpmWorldBuilder::Entity(const OhplHpmEntity& e, cXmlElement* apElement)
	{
		tString sName = e.name;
		tString sFilename = e.filename;
		cVector3f vPosition(e.position[0], e.position[1], e.position[2]);
		cVector3f vScale(e.scale[0], e.scale[1], e.scale[2]);
		cVector3f vRotation(e.rotation[0], e.rotation[1], e.rotation[2]);

		*mppCurrentElement = apElement;

		cResourceVarsObject userVars;
		cXmlElement* pUserVarsElem = apElement->GetFirstElement("UserVariables");
		if (pUserVarsElem) userVars.LoadVariables(pUserVarsElem);
		userVars.AddVarBool("Static", e.isStatic);

		cMatrixf mtxTransform = H4RotTrans(vRotation, vPosition);
		H4Seq("entity", sName, sFilename, &mtxTransform, e.active ? (e.isStatic ? "active,static" : "active") : (e.isStatic ? "static" : "-"));

		iEntity3D* pEntity = mpCurrentWorld->CreateEntity(sName, mtxTransform, sFilename, e.id, e.active, vScale, &userVars, false);
		if (pEntity && pEntity->GetEntityType() == "MeshEntity")
		{
			static_cast<cMeshEntity*>(pEntity)->SetCulledByDistance(e.culledByDistance);
			static_cast<cMeshEntity*>(pEntity)->SetCulledByFog(e.culledByFog);
		}
		*mppCurrentElement = NULL;
		return pEntity != NULL;
	}

	bool cHpmWorldBuilder::Light(cXmlElement* apElement)
	{
		return cEngineFileLoading::LoadLight(apElement, "", mpCurrentWorld, mpResources, true);
	}

	bool cHpmWorldBuilder::Area(const OhplHpmArea& a, cXmlElement* apElement)
	{
		tString sName = a.name;
		cVector3f vScale(a.scale[0], a.scale[1], a.scale[2]);
		cVector3f vRotation(a.rotation[0], a.rotation[1], a.rotation[2]);

		*mppCurrentElement = apElement;

		cMatrixf mtxTransform = H4RotTrans(vRotation, cVector3f(a.position[0], a.position[1], a.position[2]));

		tString sType = a.areaType;
		H4Seq("area", sName, sType, &mtxTransform, a.active ? "active" : "-");

		iAreaLoader* pLoader = mpResources->GetAreaLoader(sType);
		if (pLoader == NULL)
		{
			Warning("SOMA hpm: no area loader registered for AreaType '%s' (area '%s')\n", sType.c_str(), sName.c_str());
			*mppCurrentElement = NULL;
			return false;
		}

		cXmlElement* pVarRootElem = apElement->GetFirstElement("UserVariables");
		if (pVarRootElem) pLoader->LoadVariables(pVarRootElem);

		pLoader->Load(sName, a.id, a.active, vScale, mtxTransform, mpCurrentWorld);

		*mppCurrentElement = NULL;
		return true;
	}

	bool cHpmWorldBuilder::Sound(cXmlElement* apElement, bool abActive)
	{
		cSoundEntity* pSound = cEngineFileLoading::LoadSound(apElement, "", mpCurrentWorld);
		if (pSound && !abActive) pSound->SetActive(false);
		return pSound != NULL;
	}

	bool cHpmWorldBuilder::Decal(const OhplHpmDecal& d, cXmlElement* apMeshElement)
	{
		cMesh* pMesh = cEngineFileLoading::LoadDecalMeshHelper(apMeshElement, mpGraphics, mpResources, d.name, d.material,
															   cColor(d.color[0], d.color[1], d.color[2], d.color[3]));
		if (pMesh == NULL) return false;

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(d.name, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
		pMeshEntity->SetUniqueID(d.id);
		return true;
	}

	bool cHpmWorldBuilder::Billboard(cXmlElement* apElement)
	{
		return cEngineFileLoading::LoadBillboard(apElement, "", mpCurrentWorld, mpResources, true, &mlstLightBillboardConnections);
	}

	bool cHpmWorldBuilder::ParticleSystem(cXmlElement* apElement, const char* asConnectLight)
	{
		cParticleSystem* pPS = cEngineFileLoading::LoadParticleSystem(apElement, "", mpCurrentWorld);
		tString sLight = asConnectLight;
		if (pPS && sLight != "") mvLightParticleConnections.push_back(std::make_pair(pPS, sLight));
		return pPS != NULL;
	}

	bool cHpmWorldBuilder::FogArea(cXmlElement* apElement)
	{
		return cEngineFileLoading::LoadFogArea(apElement, "", mpCurrentWorld, true);
	}

	bool cHpmWorldBuilder::LensFlare(cXmlElement* apElement)
	{
		return cEngineFileLoading::LoadLensFlare(apElement, "", mpCurrentWorld, mpResources, false);
	}

	void cHpmWorldBuilder::LightMaskBox(const OhplHpmLightMask& m)
	{
		if (OhplH4UseRust())
		{
			OhplH4LightMask p;
			ohpl_h4_light_mask(m.rotation, m.size, m.scale, m.worldPos, &p);
			mmapLightMasks[m.id] = std::make_pair(cVector3f(p.center[0], p.center[1], p.center[2]), cVector3f(p.extent[0], p.extent[1], p.extent[2]));
			return;
		}
		cMatrixf mtxRot = cMath::MatrixRotate(cVector3f(m.rotation[0], m.rotation[1], m.rotation[2]), eEulerRotationOrder_XYZ);
		cVector3f vHalf = cVector3f(m.size[0], m.size[1], m.size[2]) * cVector3f(m.scale[0], m.scale[1], m.scale[2]) * 0.5f;
		cVector3f vExtent;
		for (int i = 0; i < 3; ++i)
			vExtent.v[i] = 2.0f * (std::fabs(mtxRot.m[i][0]) * vHalf.x + std::fabs(mtxRot.m[i][1]) * vHalf.y + std::fabs(mtxRot.m[i][2]) * vHalf.z);
		mmapLightMasks[m.id] = std::make_pair(cVector3f(m.worldPos[0], m.worldPos[1], m.worldPos[2]), vExtent);
	}

	void cHpmWorldBuilder::ConnectLightMasks()
	{
		cLightListIterator it = mpCurrentWorld->GetLightIterator();
		while (it.HasNext())
		{
			iLight* pLight = it.Next();
			std::map<unsigned int, std::pair<cVector3f, cVector3f> >::iterator maskIt = mmapLightMasks.find(pLight->GetMaskID());
			if (maskIt != mmapLightMasks.end())
				pLight->SetMaskBox(true, maskIt->second.first, maskIt->second.second);
		}
		mmapLightMasks.clear();
	}

	void cHpmWorldBuilder::ConnectLights(int* apMissingBillboards, int* apMissingParticles)
	{
		*apMissingBillboards = 0;
		*apMissingParticles = 0;
		tEFL_LightBillboardConnectionListIt it = mlstLightBillboardConnections.begin();
		for (; it != mlstLightBillboardConnections.end(); ++it)
		{
			cBillboard* pBB = mpCurrentWorld->GetBillboardFromUniqueID(it->msBillboardID);
			iLight* pLight = mpCurrentWorld->GetLight(it->msLightName);
			if (pBB == NULL || pLight == NULL)
			{
				++*apMissingBillboards;
				continue;
			}
			pLight->AttachBillboard(pBB, pBB->GetColor());
		}
		mlstLightBillboardConnections.clear();

		for (size_t i = 0; i < mvLightParticleConnections.size(); ++i)
		{
			iLight* pLight = mpCurrentWorld->GetLight(mvLightParticleConnections[i].second);
			if (pLight) pLight->AttachParticleSystem(mvLightParticleConnections[i].first);
			else ++*apMissingParticles;
		}
		mvLightParticleConnections.clear();
	}

	int cHpmWorldBuilder::DetailMeshes(const OhplHpmDetailMeshes& d)
	{
		float fMeshRadius = 0;
		for (int i = 0; i < d.num; ++i)
		{
			cMesh* pMesh = mpResources->GetMeshManager()->CreateMesh(d.file);
			if (pMesh == NULL) return i;

			// Instance scale = stored radius / mesh radius (cDetailMesh)
			if (fMeshRadius == 0 && OhplH4UseRust())
			{
				std::vector<float> vMins, vMaxs;
				for (int j = 0; j < pMesh->GetSubMeshNum(); ++j)
				{
					cBoundingVolume bv = pMesh->GetSubMesh(j)->GetVertexBuffer()->CreateBoundingVolume();
					for (int k = 0; k < 3; ++k) { vMins.push_back(bv.GetLocalMin().v[k]); vMaxs.push_back(bv.GetLocalMax().v[k]); }
				}
				fMeshRadius = ohpl_h4_detail_mesh_radius(vMins.data(), vMaxs.data(), vMins.size() / 3);
			}
			else if (fMeshRadius == 0)
			{
				cVector3f vMin(1e9f), vMax(-1e9f);
				for (int j = 0; j < pMesh->GetSubMeshNum(); ++j)
				{
					cBoundingVolume bv = pMesh->GetSubMesh(j)->GetVertexBuffer()->CreateBoundingVolume();
					vMin = cMath::Vector3Min(vMin, bv.GetLocalMin());
					vMax = cMath::Vector3Max(vMax, bv.GetLocalMax());
				}
				fMeshRadius = cMath::Max(vMin.Length(), vMax.Length());
			}
			float fScale = OhplH4UseRust() ? ohpl_h4_detail_scale(i < (int)d.numRadii, i < (int)d.numRadii ? d.radii[i] : 0, fMeshRadius)
										   : (i < (int)d.numRadii && fMeshRadius > 0 ? d.radii[i] / fMeshRadius : 1);

			cMeshEntity* pEntity = mpCurrentWorld->CreateMeshEntity("DetailMesh_" + cString::ToString(d.firstIndex + i), pMesh, true);
			pEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);

			cMatrixf mtxTransform;
			if (OhplH4UseRust())
				ohpl_h4_detail_matrix(&d.rotations[i*4], &d.positions[i*3], fScale, &mtxTransform);
			else
			{
				cQuaternion qRot(d.rotations[i*4], d.rotations[i*4+1], d.rotations[i*4+2], d.rotations[i*4+3]);
				mtxTransform = cMath::MatrixMul(cMath::MatrixQuaternion(qRot), cMath::MatrixScale(fScale));
				mtxTransform.SetTranslation(cVector3f(d.positions[i*3], d.positions[i*3+1], d.positions[i*3+2]));
			}
			H4Seq("detail", "DetailMesh_" + cString::ToString(d.firstIndex + i), d.file, &mtxTransform, "-");
			pEntity->SetMatrix(mtxTransform);
			mvCombineCandidates.push_back(pEntity);
		}
		return d.num;
	}

	void cHpmWorldBuilder::ExposureArea(const OhplHpmExposureArea& a)
	{
		cWorldExposureArea area;
		area.msName = a.name;
		if (OhplH4UseRust())
		{
			OhplH4Exposure p;
			ohpl_h4_exposure_area(a.rotation, a.worldPos, a.scale, &p);
			area.m_mtxInvTransform = p.invTransform;
			area.mvHalfSize = cVector3f(p.halfSize[0], p.halfSize[1], p.halfSize[2]);
		}
		else
		{
			cMatrixf mtxTransform = cMath::MatrixRotate(cVector3f(a.rotation[0], a.rotation[1], a.rotation[2]), eEulerRotationOrder_XYZ);
			mtxTransform.SetTranslation(cVector3f(a.worldPos[0], a.worldPos[1], a.worldPos[2]));
			area.m_mtxInvTransform = cMath::MatrixInverse(mtxTransform);
			area.mvHalfSize = cVector3f(a.scale[0], a.scale[1], a.scale[2]) * 0.5f;
		}
		H4Seq("exposure", area.msName, "", &area.m_mtxInvTransform, "-");
		area.mfExposure = a.exposure;
		area.mfWhiteCut = a.whiteCut;
		area.mfTransitionTime = a.transitionTime;
		mpCurrentWorld->AddExposureArea(area);
	}

	void cHpmWorldBuilder::TrackReport(const char* asTrack, int alInXml, int alCreated, int alTimeMs, bool abFileMissing,
									   const OhplHpmSkip* apSkipped, size_t alNumSkipped)
	{
		cHpmTrackStats& stats = mmapTrackStats[asTrack];
		stats.mlInXml = alInXml;
		stats.mlCreated = alCreated;
		stats.mlTimeMs = alTimeMs;
		stats.mbFileMissing = abFileMissing;
		for (size_t i = 0; i < alNumSkipped; ++i) stats.mmapSkipped[apSkipped[i].reason] = apSkipped[i].count;
	}

	void cHpmWorldBuilder::Terrain(const OhplHpmTerrain& aTerrain)
	{
		mbTerrainActive = true;
		const tWString asBaseFile = mpCurrentWorld->GetFilePath();
		const int lSize = aTerrain.heightMapSize;
		const int lPatch = aTerrain.geometryPatchSize;
		const float fUnit = aTerrain.unitSize;
		const float fMaxHeight = aTerrain.maxHeight;
		const float fTile = aTerrain.baseMaterialTileAmount;
		const tString sMaterial = aTerrain.baseMaterialFile;

		tWString sPath = asBaseFile + _W("_Terrain_heightmap.dds");
		std::vector<unsigned char> vData(cPlatform::GetFileSize(sPath));
		if (vData.size() < 128 + (size_t)lSize * lSize * 3 || !cPlatform::CopyFileToBuffer(sPath, vData.data(), vData.size()))
		{
			Warning("SOMA hpm: bad terrain heightmap '%s'\n", cString::To8Char(sPath).c_str());
			return;
		}

		std::vector<float> vHeight((size_t)lSize * lSize);
		std::vector<unsigned short> vElev((size_t)(lSize + 1) * (lSize + 1));
		for (size_t i = 0; i < vHeight.size(); ++i)
		{
			const unsigned char* p = &vData[128 + i * 3];
			unsigned int lVal = p[0] | (p[1] << 8) | (p[2] << 16);
			vHeight[i] = lVal ? lVal * (fMaxHeight / 16777215.0f) : NAN;
			vElev[i / lSize * (lSize + 1) + i % lSize] = (unsigned short)lroundf(lVal / 16777215.0f * 65535.0f);
		}
		for (int i = 0; i < lSize; ++i) vElev[(size_t)i * (lSize + 1) + lSize] = vElev[(size_t)i * (lSize + 1) + lSize - 1];
		std::copy_n(&vElev[(size_t)(lSize - 1) * (lSize + 1)], lSize + 1, &vElev[(size_t)lSize * (lSize + 1)]);
		auto Height = [&](int x, int z) { return vHeight[(size_t)cMath::Clamp(z, 0, lSize - 1) * lSize + cMath::Clamp(x, 0, lSize - 1)]; };
		auto Solid = [&](int x, int z, float fFallback) { float h = Height(x, z); return std::isnan(h) ? fFallback : h; };

		cMaterialManager* pMatMgr = mpResources->GetMaterialManager();
		std::vector<cMaterial*> vBlend = CreateTerrainBlendMaterials(aTerrain, asBaseFile, lSize * fUnit);

		// Rebirth: the blend channel painted with terrain_hole.mat is a hole mask
		std::vector<unsigned char> vHole;
		int lHoleScale = 0, lHoleChannel = 0;
		for (size_t lLayer = 0; lLayer < aTerrain.numLayers && vHole.empty(); ++lLayer)
		{
			const OhplHpmTerrainLayer& layer = aTerrain.layers[lLayer];
			for (int i = 0; i < layer.numMats; ++i)
			{
				if (cString::ToLowerCase(cString::GetFileName(layer.mats[i].file)) != "terrain_hole.mat") continue;
				tWString sHole = asBaseFile + _W("_Terrain_blendlayer_") + cString::To16Char(layer.id) + _W(".dds");
				std::vector<unsigned char> vFile(cPlatform::GetFileSize(sHole));
				if (vFile.size() < 128 || !cPlatform::CopyFileToBuffer(sHole, vFile.data(), vFile.size())) break;
				const int lW = vFile[16] | (vFile[17] << 8) | (vFile[18] << 16);
				const bool bRaw32 = vFile[84] == 0 && vFile[88] == 32 && vFile[92] == 0xFF;
				if (!bRaw32 || lW < lSize || vFile.size() < 128 + (size_t)lW * lW * 4) break;
				lHoleScale = lW / lSize;
				lHoleChannel = i;
				vHole = std::move(vFile);
				break;
			}
		}
		// ponytail: holes cut per cell (official discards per pixel); move to the shader if edges show
		auto Hole = [&](int x, int z)
		{
			if (vHole.empty()) return false;
			const int lW = lHoleScale * lSize;
			int lSum = 0;
			for (int tz = z * lHoleScale; tz < (z + 1) * lHoleScale; ++tz)
			for (int tx = x * lHoleScale; tx < (x + 1) * lHoleScale; ++tx)
				lSum += vHole[128 + ((size_t)tz * lW + tx) * 4 + lHoleChannel];
			return lSum * 2 > 255 * lHoleScale * lHoleScale;
		};

		std::vector<cSubMeshEntity*> vPatches;
		const float fOffset = lSize * fUnit * 0.5f;
		for (int z0 = 0; z0 < lSize - 1; z0 += lPatch)
		for (int x0 = 0; x0 < lSize - 1; x0 += lPatch)
		{
			const int lW = std::min(lPatch, lSize - 1 - x0) + 1;
			const int lH = std::min(lPatch, lSize - 1 - z0) + 1;

			iVertexBuffer* pVtx = mpGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																				 eVertexBufferUsageType_Static, lW * lH, (lW - 1) * (lH - 1) * 6);
			pVtx->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
			pVtx->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
			pVtx->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
			std::vector<iVertexBuffer*> vBlendVtx;
			for (size_t i = 0; i < vBlend.size(); ++i)
			{
				vBlendVtx.push_back(mpGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri,
																				   eVertexBufferUsageType_Static, lW * lH, (lW - 1) * (lH - 1) * 6));
				vBlendVtx.back()->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
				vBlendVtx.back()->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
			}

			for (int z = z0; z < z0 + lH; ++z)
			for (int x = x0; x < x0 + lW; ++x)
			{
				float h = Solid(x, z, 0);
				cVector3f vPos(x * fUnit - fOffset, h, z * fUnit - fOffset);
				cVector3f vNormal(Solid(x - 1, z, h) - Solid(x + 1, z, h), 2 * fUnit, Solid(x, z - 1, h) - Solid(x, z + 1, h));
				pVtx->AddVertexVec3f(eVertexBufferElement_Position, vPos);
				pVtx->AddVertexVec3f(eVertexBufferElement_Normal, cMath::Vector3Normalize(vNormal));
				pVtx->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(vPos.x, vPos.z, 0) * fTile);
				for (iVertexBuffer* pBlendVtx : vBlendVtx)
				{
					pBlendVtx->AddVertexVec3f(eVertexBufferElement_Position, vPos);
					pBlendVtx->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f((float)x, (float)z, 0) / (float)lSize);
				}
			}

			for (int z = 0; z < lH - 1; ++z)
			for (int x = 0; x < lW - 1; ++x)
			{
				if (std::isnan(Height(x0 + x, z0 + z)) || std::isnan(Height(x0 + x + 1, z0 + z)) ||
					std::isnan(Height(x0 + x, z0 + z + 1)) || std::isnan(Height(x0 + x + 1, z0 + z + 1)) || Hole(x0 + x, z0 + z)) continue;
				int i = z * lW + x;
				for (int lIdx : {i, i + 1, i + lW, i + 1, i + lW + 1, i + lW})
				{
					pVtx->AddIndex(lIdx);
					for (iVertexBuffer* pBlendVtx : vBlendVtx) pBlendVtx->AddIndex(lIdx);
				}
			}
			if (pVtx->GetIndexNum() == 0)
			{
				hplDelete(pVtx);
				for (iVertexBuffer* pBlendVtx : vBlendVtx) hplDelete(pBlendVtx);
				continue;
			}
			pVtx->Compile(eVertexCompileFlag_CreateTangents);

			tString sName = "Terrain_" + cString::ToString(x0 / lPatch) + "_" + cString::ToString(z0 / lPatch);
			cMesh* pMesh = hplNew(cMesh, (sName, _W(""), mpResources->GetMaterialManager(), mpResources->GetAnimationManager()));
			cSubMesh* pSubMesh = pMesh->CreateSubMesh("Main");
			pSubMesh->SetVertexBuffer(pVtx);
			pSubMesh->SetMaterial(pMatMgr->CreateMaterial(sMaterial));
			for (size_t i = 0; i < vBlend.size(); ++i)
			{
				vBlendVtx[i]->Compile(0);
				cSubMesh* pBlendSub = pMesh->CreateSubMesh("Blend" + cString::ToString((int)i));
				pBlendSub->SetVertexBuffer(vBlendVtx[i]);
				vBlend[i]->IncUserCount();
				pBlendSub->SetMaterial(vBlend[i]);
			}

			cMeshEntity* pEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
			pEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, true);
			vPatches.push_back(pEntity->GetSubMeshEntity(0));
		}
		// ponytail: Newton heightfields have no holes, so sink vertices inside one; holes under 2 cells wide stay solid
		if (!vHole.empty())
			for (int z = 1; z < lSize; ++z)
			for (int x = 1; x < lSize; ++x)
				if (Hole(x - 1, z - 1) && Hole(x, z - 1) && Hole(x - 1, z) && Hole(x, z)) vElev[(size_t)z * (lSize + 1) + x] = 0;
		iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody("Terrain", mpCurrentPhysicsWorld->CreateHeightFieldShape(lSize + 1, vElev.data(), fUnit, fMaxHeight / 65535.0f));
		pBody->SetMass(0);
		pBody->SetMatrix(cMath::MatrixTranslate(cVector3f(-fOffset, 0, -fOffset)));
		for (cMaterial* pMat : vBlend) pMatMgr->Destroy(pMat);
		CreateTerrainDecals(aTerrain, vPatches);
		CreateTerrainUndergrowth(aTerrain, vHeight, lSize, fUnit);
		for (float& h : vHeight) if (std::isnan(h)) h = 0;
		mpCurrentWorld->SetTerrain(hplNew(cTerrain, (lSize, fUnit, std::move(vHeight))));
	}

	// Rebirth's cTerrainUndergrowth, baked once: each plant's seeds depend only on its cell and grid index.
	void cHpmWorldBuilder::CreateTerrainUndergrowth(const OhplHpmTerrain& aTerrain, const std::vector<float>& avHeight, int alSize, float afUnit)
	{
		if (aTerrain.hasSections == false) return;
		const float fGrid = aTerrain.undergrowthGridSize;
		const float fFadeStart = aTerrain.undergrowthFadeStart;
		const float fFadeEnd = aTerrain.undergrowthFadeEnd;
		const float fW = alSize * afUnit;
		const int lCells = (int)(fW / fGrid + 1);
		cMaterialManager* pMatMgr = mpResources->GetMaterialManager();

		std::map<tString, int> mapMatIdx;
		std::vector<cUndergrowthMat> vMats;
		auto LoadMat = [&](const tString& asFile) -> int
		{
			auto it = mapMatIdx.find(asFile);
			if (it != mapMatIdx.end()) return it->second;
			int& lIdx = mapMatIdx[asFile] = -1;
			tWString sPath = mpResources->GetFileSearcher()->GetFilePath(asFile);
			iXmlDocument* pDoc = mpResources->GetLowLevel()->CreateXmlDocument();
			if (sPath == _W("") || !pDoc->CreateFromFile(sPath)) { hplDelete(pDoc); return -1; }
			cMesh* pMesh = mpResources->GetMeshManager()->CreateMesh(pDoc->GetAttributeString("BaseMesh"));
			mpResources->GetTextureManager()->SetSRGB(true);
			iTexture* pTex = mpResources->GetTextureManager()->Create2D(pDoc->GetAttributeString("DiffuseTex"), true);
			mpResources->GetTextureManager()->SetSRGB(false);
			if (pMesh == NULL || pTex == NULL)
			{
				if (pMesh) mpResources->GetMeshManager()->Destroy(pMesh);
				if (pTex) mpResources->GetTextureManager()->Destroy(pTex);
				hplDelete(pDoc);
				return -1;
			}
			cUndergrowthMat mat;
			for (int i = 0; i < pMesh->GetSubMeshNum(); ++i)
			{
				iVertexBuffer* pVtx = pMesh->GetSubMesh(i)->GetVertexBuffer();
				const float* pPos = pVtx->GetFloatArray(eVertexBufferElement_Position);
				const float* pUv = pVtx->GetFloatArray(eVertexBufferElement_Texture0);
				const int lPosStride = pVtx->GetElementNum(eVertexBufferElement_Position), lUvStride = pVtx->GetElementNum(eVertexBufferElement_Texture0);
				const unsigned int lBase = (unsigned int)mat.mvPos.size();
				for (int v = 0; v < pVtx->GetVertexNum(); ++v)
				{
					mat.mvPos.push_back(cVector3f(pPos[v * lPosStride], pPos[v * lPosStride + 1], pPos[v * lPosStride + 2]));
					mat.mvUv.push_back(pUv ? cVector3f(pUv[v * lUvStride], pUv[v * lUvStride + 1], 0) : cVector3f(0));
				}
				for (int k = 0; k < pVtx->GetIndexNum(); ++k) mat.mvIdx.push_back(lBase + pVtx->GetIndices()[k]);
			}
			mpResources->GetMeshManager()->Destroy(pMesh);

			mat.mpMat = pMatMgr->CreateCustomMaterial(asFile, mpGraphics->GetMaterialType("undergrowth"));
			mat.mpMat->SetTexture(eMaterialTexture_Diffuse, pTex);
			cMaterialType_Undergrowth_Vars* pVars = static_cast<cMaterialType_Undergrowth_Vars*>(mat.mpMat->GetVars());
			pVars->mbWind = pDoc->GetAttributeBool("WindActive", false);
			if (pVars->mbWind)
			{
				pVars->mvWind = cVector3f(pDoc->GetAttributeFloat("WindFreq", 1), pDoc->GetAttributeFloat("WindAmplitude", 1), pDoc->GetAttributeFloat("WindSpeed", 1));
				pVars->mvWindOctaves = pDoc->GetAttributeVector3f("WindOctaveMuls", 1);
			}
			pVars->mvDissolve = cVector2f(fFadeStart, fFadeEnd - fFadeStart);
			pVars->mfForceFieldMul = pDoc->GetAttributeFloat("ForceFieldForceMul", 0);
			pVars->mfMaxForceFieldForce = pDoc->GetAttributeFloat("MaxForceFieldForce", 0);
			mat.mpMat->Compile();
			mat.mvSubDiv = pDoc->GetAttributeVector2f("TextureSubDivisions", 1);
			mat.mMinColor = pDoc->GetAttributeColor("MinColor", cColor(1, 1));
			mat.mMaxColor = pDoc->GetAttributeColor("MaxColor", cColor(1, 1));
			mat.mfDensity = pDoc->GetAttributeFloat("MaxDensity", 1);
			mat.mfAlign = pDoc->GetAttributeFloat("AlignToSlopeAmount", 0);
			mat.mvBaseSize = pDoc->GetAttributeVector3f("BaseSize", 1);
			mat.mvMinSizeMul = pDoc->GetAttributeVector3f("MinSizeMul", 1);
			mat.mvMaxSizeMul = pDoc->GetAttributeVector3f("MaxSizeMul", 1);
			hplDelete(pDoc);
			vMats.push_back(mat);
			return lIdx = (int)vMats.size() - 1;
		};

		std::vector<cUndergrowthArea> vAreas;
		for (size_t lArea = 0; lArea < aTerrain.numAreas; ++lArea)
		{
			const OhplHpmUndergrowthArea& src = aTerrain.areas[lArea];
			cUndergrowthArea area;
			if ((area.mlMat = LoadMat(src.file)) < 0) continue;
			area.mbSub = src.sub;
			area.mfMaxInfluence = src.maxInfluence;
			area.mfFadeBorder = src.fadeBorder;
			if (src.circle)
			{
				area.mvCenter = cVector2f(src.center[0], src.center[1]);
				area.mfRadius = src.radius;
				area.mvMin = area.mvCenter - area.mfRadius;
				area.mvMax = area.mvCenter + area.mfRadius;
			}
			else
			{
				std::vector<cVector2f> vPts;
				for (size_t i = 0; i < src.numPoints; ++i) vPts.push_back(cVector2f(src.points[i * 2], src.points[i * 2 + 1]));
				if (vPts.size() < 3) continue;
				cVector2f vCentroid = 0;
				area.mvMin = area.mvMax = vPts[0];
				for (const cVector2f& v : vPts)
				{
					vCentroid += v / (float)vPts.size();
					area.mvMin = cVector2f(std::min(area.mvMin.x, v.x), std::min(area.mvMin.y, v.y));
					area.mvMax = cVector2f(std::max(area.mvMax.x, v.x), std::max(area.mvMax.y, v.y));
				}
				for (size_t i = 0; i < vPts.size(); ++i)
				{
					const cVector2f &a = vPts[i], &b = vPts[(i + 1) % vPts.size()];
					cVector2f n(b.y - a.y, a.x - b.x);
					n.Normalize();
					if (n.x * (vCentroid.x - a.x) + n.y * (vCentroid.y - a.y) < 0) n = n * -1;
					area.mvPlanes.push_back(cVector3f(n.x, n.y, -(n.x * a.x + n.y * a.y)));
				}
			}
			vAreas.push_back(area);
		}
		if (vAreas.empty()) return;

		std::map<int, std::vector<int>> mapCellAreas;
		for (size_t a = 0; a < vAreas.size(); ++a)
		{
			auto Cell = [&](float f) { return cMath::Clamp((int)((f + fW * 0.5f) / fGrid), 0, lCells - 1); };
			for (int cy = Cell(vAreas[a].mvMin.y); cy <= Cell(vAreas[a].mvMax.y); ++cy)
			for (int cx = Cell(vAreas[a].mvMin.x); cx <= Cell(vAreas[a].mvMax.x); ++cx)
				mapCellAreas[(cy * lCells + cx) * (int)vMats.size() + vAreas[a].mlMat].push_back((int)a);
		}

		auto Height = [&](int x, int z) { return avHeight[(size_t)cMath::Clamp(z, 0, alSize - 1) * alSize + cMath::Clamp(x, 0, alSize - 1)]; };
		auto Solid = [&](int x, int z, float fFallback) { float h = Height(x, z); return std::isnan(h) ? fFallback : h; };
		std::map<int, cMesh*> mapCellMesh;
		for (auto& cellAreas : mapCellAreas)
		{
			const int lMat = cellAreas.first % (int)vMats.size(), lCell = cellAreas.first / (int)vMats.size();
			const int cx = lCell % lCells, cy = lCell / lCells;
			const cUndergrowthMat& mat = vMats[lMat];
			const int lRes = std::max(1, (int)(fGrid * mat.mfDensity + 0.5f));
			const cVector2f vCacheMin(cx * fGrid - fW * 0.5f, cy * fGrid - fW * 0.5f);

			std::vector<float> vGrid((size_t)lRes * lRes, 0.0f);
			for (int a : cellAreas.second)
			{
				const cUndergrowthArea& area = vAreas[a];
				auto Index = [&](float f) { return (int)(cMath::Clamp(f, 0.0f, fGrid) / fGrid * (lRes - 1) + 0.5f); };
				for (int y = Index(area.mvMin.y - vCacheMin.y); y <= Index(area.mvMax.y - vCacheMin.y); ++y)
				for (int x = Index(area.mvMin.x - vCacheMin.x); x <= Index(area.mvMax.x - vCacheMin.x); ++x)
				{
					float fInfl = area.Influence(vCacheMin + cVector2f((float)x, (float)y) / (float)lRes * fGrid);
					vGrid[y * lRes + x] += area.mbSub ? -fInfl : fInfl;
				}
			}

			const float fStep = fGrid / lRes;
			const cVector2f vSubMul = cVector2f(1) / mat.mvSubDiv;
			const int lSubX = std::max(1, (int)mat.mvSubDiv.x), lSubNum = lSubX * std::max(1, (int)mat.mvSubDiv.y);
			std::vector<float> vVtxData;
			iVertexBuffer* pVtx = NULL;
			for (int i = 0; i < lRes * lRes; ++i)
			{
				float v = cMath::Clamp(vGrid[i], 0.0f, 1.0f);
				if (v < 1 && !(v > cMath::Abs(cMath::FastRandomFloat(cx + cy + i)))) continue;
				float fJitter = cMath::FastRandomFloat(cx * 13 + cy + i) * fStep * 0.5f;
				cVector3f vPos(vCacheMin.x + (i % lRes) * fStep + fJitter, 0, vCacheMin.y + (i / lRes) * fStep + fJitter);
				float u = (vPos.x + fW * 0.5f) / afUnit, w = (vPos.z + fW * 0.5f) / afUnit;
				if (u < 0 || w < 0 || u > alSize - 1 || w > alSize - 1) continue;
				int x0 = std::min((int)u, alSize - 2), z0 = std::min((int)w, alSize - 2);
				float fu = u - x0, fw = w - z0;
				vPos.y = (Height(x0, z0) * (1 - fu) + Height(x0 + 1, z0) * fu) * (1 - fw) + (Height(x0, z0 + 1) * (1 - fu) + Height(x0 + 1, z0 + 1) * fu) * fw;
				if (std::isnan(vPos.y)) continue;

				int nx = (int)(u + 0.5f), nz = (int)(w + 0.5f);
				float h = Solid(nx, nz, vPos.y);
				cVector3f vNormal = cMath::Vector3Normalize(cVector3f(Solid(nx - 1, nz, h) - Solid(nx + 1, nz, h), 2 * afUnit, Solid(nx, nz - 1, h) - Solid(nx, nz + 1, h)));
				cMatrixf mtxRot = cMath::MatrixRotateY(cMath::FastRandomFloat(cx * 17 + cy + i) * kPif);
				if (mat.mfAlign > 0)
				{
					cVector3f n = mat.mfAlign >= 1 ? vNormal : cMath::Vector3Normalize(cVector3f(0, 1, 0) * (1 - mat.mfAlign) + vNormal * mat.mfAlign);
					cVector3f vTan = cMath::Vector3Normalize(cMath::Vector3Cross(n, cVector3f(0, 0, 1)));
					mtxRot = cMath::MatrixMul(cMath::MatrixUnitVectors(vTan * -1, n, cMath::Vector3Cross(n, vTan), 0), mtxRot);
				}
				float fSizeT = cMath::Abs(cMath::FastRandomFloat(cx * 23 + cy + i));
				cVector3f vSize = mat.mvBaseSize * (mat.mvMinSizeMul * (1 - fSizeT) + mat.mvMaxSizeMul * fSizeT);
				float fColorT = cMath::Abs(cMath::FastRandomFloat(cx * 31 + cy + i));
				cColor col = mat.mMinColor * (1 - fColorT) + mat.mMaxColor * fColorT;
				int lTex = (i * 13) % lSubNum;
				cVector3f vUvAdd = cVector3f((float)(lTex % lSubX), (float)(lTex / lSubX), 0) * cVector3f(vSubMul.x, vSubMul.y, 1);

				if (pVtx == NULL)
				{
					pVtx = mpGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static, 0, 0);
					pVtx->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
					pVtx->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
					pVtx->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
					pVtx->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
					pVtx->CreateElementArray(eVertexBufferElement_Texture1, eVertexBufferElementFormat_Float, 3);
				}
				const unsigned int lBase = (unsigned int)pVtx->GetVertexNum();
				const float fRand = cMath::RandRectf(0, 1);
				for (size_t k = 0; k < mat.mvPos.size(); ++k)
				{
					pVtx->AddVertexVec3f(eVertexBufferElement_Position, cMath::MatrixMul3x3(mtxRot, mat.mvPos[k]) * vSize + vPos);
					pVtx->AddVertexVec3f(eVertexBufferElement_Normal, vNormal);
					pVtx->AddVertexColor(eVertexBufferElement_Color0, col);
					pVtx->AddVertexVec3f(eVertexBufferElement_Texture0, mat.mvUv[k] * cVector3f(vSubMul.x, vSubMul.y, 1) + vUvAdd);
					pVtx->AddVertexVec3f(eVertexBufferElement_Texture1, cVector3f(mat.mvPos[k].y, fRand, 0));
				}
				// Rebirth draws undergrowth with culling off; ours is global, so add the back faces.
				for (size_t k = 0; k < mat.mvIdx.size(); ++k) pVtx->AddIndex(lBase + mat.mvIdx[k]);
				for (size_t k = 0; k + 2 < mat.mvIdx.size(); k += 3)
					for (int j : {0, 2, 1}) pVtx->AddIndex(lBase + mat.mvIdx[k + j]);
			}
			if (pVtx == NULL) continue;
			pVtx->Compile(0);

			cMesh*& pMesh = mapCellMesh[lCell];
			if (pMesh == NULL) pMesh = hplNew(cMesh, ("Undergrowth_" + cString::ToString(lCell), _W(""), pMatMgr, mpResources->GetAnimationManager()));
			cSubMesh* pSubMesh = pMesh->CreateSubMesh("Mat" + cString::ToString(lMat));
			pSubMesh->SetVertexBuffer(pVtx);
			mat.mpMat->IncUserCount();
			pSubMesh->SetMaterial(mat.mpMat);
		}
		// ponytail: every cell is drawn and only the shader dissolve hides far plants; cull cells by distance if it costs fps.
		for (auto& cellMesh : mapCellMesh)
			mpCurrentWorld->CreateMeshEntity(cellMesh.second->GetName(), cellMesh.second, true)->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
		for (cUndergrowthMat& mat : vMats) pMatMgr->Destroy(mat.mpMat);
	}

	void cHpmWorldBuilder::CreateTerrainDecals(const OhplHpmTerrain& aTerrain, const std::vector<cSubMeshEntity*>& avPatches)
	{
		if (aTerrain.hasSections == false) return;
		const float afMaxHeight = aTerrain.maxHeight;
		cDecalCreator creator(mpGraphics->GetLowLevel(), mpResources);
		creator.SetMaxTrianglesPerDecal(100000);
		creator.SetDecalOffset(0.02f);
		int lCount = 0;
		for (size_t lDecal = 0; lDecal < aTerrain.numDecals; ++lDecal)
		{
			const OhplHpmTerrainDecal& decal = aTerrain.decals[lDecal];
			cVector2f vPos(decal.position[0], decal.position[1]);
			cVector2f vSize(decal.size[0], decal.size[1]);
			cMatrixf mtxRot = cMath::MatrixRotateY(decal.angle);

			creator.ClearMeshes();
			creator.SetMaterial(decal.file);
			if (creator.GetMaterial() == NULL) continue;
			creator.SetDecalPosition(cVector3f(vPos.x, afMaxHeight * 0.5f, vPos.y));
			creator.SetDecalRight(mtxRot.GetRight(), false);
			creator.SetDecalForward(mtxRot.GetForward(), false);
			creator.SetDecalSize(cVector3f(vSize.x, afMaxHeight + 2, vSize.y));
			creator.SetColor(cColor(decal.color[0], decal.color[1], decal.color[2], decal.color[3]));
			creator.SetUVSubDivisions(creator.GetMaterial()->GetUVSubDivisions());
			creator.SetCurrentSubDiv(decal.currentUvSubDiv);
			for (cSubMeshEntity* pPatch : avPatches) creator.AddSubMesh(pPatch);
			cMesh* pMesh = creator.CreateDecalMesh();
			if (pMesh == NULL) continue;
			cMeshEntity* pEntity = mpCurrentWorld->CreateMeshEntity("TerrainDecal_" + cString::ToString(lCount++), pMesh, true);
			pEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
		}
		creator.ClearMeshes();
	}

	std::vector<cMaterial*> cHpmWorldBuilder::CreateTerrainBlendMaterials(const OhplHpmTerrain& aTerrain, const tWString& asBaseFile, float afSize)
	{
		std::vector<cMaterial*> vMats;
		cMaterialManager* pMatMgr = mpResources->GetMaterialManager();
		if (aTerrain.hasBlendLayers == false) return vMats;

		auto ShareTexture = [](cMaterial* apDest, int alSlot, cMaterial* apSrc, eMaterialTexture aType)
		{
			iTexture* pTex = apSrc ? apSrc->GetTexture(aType) : NULL;
			if (pTex == NULL) return;
			pTex->IncUserCount();
			apDest->SetTexture((eMaterialTexture)alSlot, pTex);
		};

		std::vector<iTexture*> vDetailTex;
		cVector3f vDetailScale(0), vDetailFade(0, 1, afSize);
		if (aTerrain.hasDetails)
		{
			vDetailFade.x = aTerrain.detailFadeStart;
			vDetailFade.y = std::max(aTerrain.detailFadeEnd - vDetailFade.x, 0.001f);
			for (int i = 0; i < aTerrain.numDetails; ++i)
			{
				tString sFile = aTerrain.details[i].file;
				vDetailTex.push_back(sFile == "" ? NULL : mpResources->GetTextureManager()->Create2D(sFile, true));
				vDetailScale.v[i] = aTerrain.details[i].scale;
			}
		}
		auto SetDetail = [&](cMaterial* apMat, cMaterialType_TerrainBlend_Vars* apVars)
		{
			if (vDetailTex.empty() || vDetailTex[0] == NULL) return;
			for (size_t i = 0; i < vDetailTex.size(); ++i)
			{
				if (vDetailTex[i] == NULL) continue;
				vDetailTex[i]->IncUserCount();
				apMat->SetTexture((eMaterialTexture)(10 + i), vDetailTex[i]);
			}
			apVars->mvDetailMapScale = vDetailScale;
			apVars->mvDetailFade = vDetailFade;
		};

		for (size_t lLayer = 0; lLayer < aTerrain.numLayers; ++lLayer)
		{
			const OhplHpmTerrainLayer& layer = aTerrain.layers[lLayer];
			tString sId = layer.id;
			tString sMap = cString::To8Char(cString::GetFileNameW(asBaseFile)) + "_Terrain_blendlayer_" + sId + ".dds";
			iTexture* pMap = mpResources->GetTextureManager()->Create2D(sMap, true);
			if (pMap == NULL) continue;

			cMaterial* pMat = pMatMgr->CreateCustomMaterial(sMap, mpGraphics->GetMaterialType("terrainblend"));
			cMaterialType_TerrainBlend_Vars* pVars = static_cast<cMaterialType_TerrainBlend_Vars*>(pMat->GetVars());
			pMat->SetTexture((eMaterialTexture)0, pMap);
			if (vMats.empty())
			{
				cMaterial* pBase = pMatMgr->CreateMaterial(aTerrain.baseMaterialFile);
				ShareTexture(pMat, 5, pBase, eMaterialTexture_Diffuse);
				if (pBase) pMatMgr->Destroy(pBase);
				pVars->mvBaseDetailAmount = cVector3f(aTerrain.baseMaterialDetailTextureAmount[0], aTerrain.baseMaterialDetailTextureAmount[1], aTerrain.baseMaterialDetailTextureAmount[2]);
			}
			SetDetail(pMat, pVars);
			pVars->mfBaseTextureCoordScale = aTerrain.baseMaterialTileAmount * afSize;

			for (int i = 0; i < layer.numMats; ++i)
			{
				const OhplHpmTerrainMat& layerMat = layer.mats[i];
				pVars->mvTextureCoordScale[i] = layerMat.tileAmount * afSize;
				pVars->mvOneMinusFadeStart[i] = 1 - layerMat.startFadeValue;
				pVars->mvDetailAmount[i] = cVector3f(layerMat.detailAmount[0], layerMat.detailAmount[1], layerMat.detailAmount[2]);
				tString sFile = layerMat.file;
				// hole mask channel, not a material
				if (cString::ToLowerCase(cString::GetFileName(sFile)) == "terrain_hole.mat") pVars->mvBlendMul[i] = 0;
				cMaterial* pSrc = sFile != "" ? pMatMgr->CreateMaterial(sFile) : NULL;
				ShareTexture(pMat, 1 + i, pSrc, eMaterialTexture_Diffuse);
				ShareTexture(pMat, 6 + i, pSrc, eMaterialTexture_Alpha);
				if (pSrc) pMatMgr->Destroy(pSrc);
			}

			pMat->SetBlendMode(vMats.empty() ? eMaterialBlendMode_None : eMaterialBlendMode_Alpha);
			pMat->SetDecalSortOrder((int)vMats.size() - 100);
			pMat->Compile();
			vMats.push_back(pMat);
		}
		for (iTexture* pTex : vDetailTex)
			if (pTex) mpResources->GetTextureManager()->Destroy(pTex);
		return vMats;
	}

};
