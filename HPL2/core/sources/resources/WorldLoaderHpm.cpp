
#include "resources/WorldLoaderHpm.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"

#include "resources/Resources.h"
#include "resources/MeshManager.h"
#include "resources/TextureManager.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"
#include "resources/EngineFileLoading.h"

#include "scene/Scene.h"
#include "scene/World.h"
#include "scene/MeshEntity.h"
#include "scene/SubMeshEntity.h"
#include "scene/Light.h"
#include "scene/BillBoard.h"
#include "scene/FogArea.h"
#include "scene/ParticleSystem.h"
#include "scene/SoundEntity.h"

#include "system/Platform.h"

#include "graphics/Graphics.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/MeshCreator.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"

#include "physics/Physics.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "physics/CollideShape.h"

#include "math/Math.h"

namespace hpl {

	cXmlElement* cWorldLoaderHpm::mpCurrentElement = NULL;
	tString cWorldLoaderHpm::msLastLoadReportJson = "";

	cWorldLoaderHpm::cWorldLoaderHpm()
	{
		AddSupportedExtension("hpm");

		mpCurrentWorld = NULL;
		mpCurrentPhysicsWorld = NULL;

		mbTerrainActive = false;
	}

	cWorldLoaderHpm::~cWorldLoaderHpm()
	{
	}

	cWorld* cWorldLoaderHpm::LoadWorld(const tWString& asFile, tWorldLoadFlag aFlags)
	{
		Log(" -------- Loading SOMA hpm map '%s' ---------\n", cString::To8Char(cString::GetFileNameW(asFile)).c_str());

		unsigned long lLoadStartTime = cPlatform::GetApplicationTime();
		mmapTrackStats.clear();
		mlstLightBillboardConnections.clear();

		mbTerrainActive = false;

		mpCurrentWorld = mpScene->CreateWorld(cString::To8Char(cString::GetFileNameW(asFile)));
		mpCurrentWorld->SetFilePath(asFile);

		mpCurrentPhysicsWorld = mpPhysics->CreateWorld(true);
		mpCurrentPhysicsWorld->SetAccuracyLevel(ePhysicsAccuracy_Medium);
		mpCurrentPhysicsWorld->SetWorldSize(-300, 300);
		mpCurrentPhysicsWorld->SetMaxTimeStep(1.0f / 60.0f);
		mpCurrentWorld->SetPhysicsWorld(mpCurrentPhysicsWorld);

		LoadGlobalSettings(asFile);

		LoadTrack(asFile, "StaticObject", "FileIndex_StaticObjects");
		LoadTrack(asFile, "Primitive", "");
		LoadTrack(asFile, "Entity", "FileIndex_Entities");
		LoadTrack(asFile, "Light", "");
		LoadTrack(asFile, "Area", "");
		LoadTrack(asFile, "Sound", "");
		LoadTrack(asFile, "Decal", "FileIndex_Decals");
		LoadTrack(asFile, "Billboard", "");
		LoadTrack(asFile, "ParticleSystem", "");
		LoadTrack(asFile, "FogArea", "");
		ConnectLightBillboards();
		LoadTrack(asFile, "LightMask", "");
		ConnectLightMasks();

		LoadTrack(asFile, "Compound", "");
		LoadTrack(asFile, "LensFlare", "");
		LoadTrack(asFile, "StaticComboArea", "");
		CountUnsupportedFlatTracks(asFile);
		LoadDetailMeshesTrack(asFile);

		LoadExposureAreaTrack(asFile);
		CheckTerrainTrackInactive(asFile);

		mpCurrentWorld->Compile(true);

		BuildLoadReport(cString::To8Char(cString::GetFileNameW(asFile)), (int)(cPlatform::GetApplicationTime() - lLoadStartTime));
		Log(" -------- Loading complete ---------\n");

		return mpCurrentWorld;
	}

	iXmlDocument* cWorldLoaderHpm::OpenSidecar(const tWString& asBaseFile, const tWString& asSuffix, bool abWarnIfMissing)
	{
		tWString sPath = asBaseFile + asSuffix;

		iXmlDocument* pDoc = mpResources->GetLowLevel()->CreateXmlDocument();
		if (pDoc->CreateFromFile(sPath) == false)
		{
			if (abWarnIfMissing)
				Warning("SOMA hpm: could not open/parse '%s'\n", cString::To8Char(sPath).c_str());
			hplDelete(pDoc);
			return NULL;
		}

		return pDoc;
	}

	cXmlElement* cWorldLoaderHpm::GetTrackRoot(iXmlDocument* apDoc, const tString& asExpectedRootValue)
	{
		if (apDoc->GetValue() != asExpectedRootValue)
		{
			Warning("SOMA hpm: sidecar file has unexpected root element '%s' (expected '%s')\n",
					apDoc->GetValue().c_str(), asExpectedRootValue.c_str());
			return NULL;
		}
		return static_cast<cXmlElement*>(apDoc);
	}

	void cWorldLoaderHpm::LoadLocalFileIndex(cXmlElement* apSection, const tString& asIndexElement, tStringVec& avIndexOut)
	{
		cXmlElement* pIndex = apSection->GetFirstElement(asIndexElement);
		if (pIndex == NULL) return;

		avIndexOut.resize(pIndex->GetAttributeInt("NumOfFiles", 0));

		cXmlNodeListIterator it = pIndex->GetChildIterator();
		while (it.HasNext())
		{
			cXmlElement* pFile = it.Next()->ToElement();

			int lIdx = pFile->GetAttributeInt("Id", 0);
			if (lIdx >= 0 && lIdx < (int)avIndexOut.size())
				avIndexOut[lIdx] = pFile->GetAttributeString("Path", "");
		}
	}

	bool cWorldLoaderHpm::CheckTransformValidity(const tString& asName, const cVector3f& avPos, const cVector3f& avRot, const cVector3f& avScale)
	{
		if (cMath::Abs(avPos.x) > 10000.0f || cMath::Abs(avPos.y) > 10000.0f || cMath::Abs(avPos.z) > 10000.0f)
		{
			Warning("SOMA hpm: object '%s' has an invalid position: (%s)!\n", asName.c_str(), avPos.ToString().c_str());
			return false;
		}
		return true;
	}

	void cWorldLoaderHpm::LoadGlobalSettings(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W(""), true);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMap");
		if (pRoot)
		{
			cXmlElement* pGlobal = pRoot->GetFirstElement("GlobalSettings");
			if (pGlobal)
			{
				cXmlElement* pFog = pGlobal->GetFirstElement("Fog");
				if (pFog)
				{
					mpCurrentWorld->SetFogActive(pFog->GetAttributeBool("Active", false));
					mpCurrentWorld->SetFogColor(pFog->GetAttributeColor("Color", cColor(1, 1)));
					mpCurrentWorld->SetFogFalloffExp(pFog->GetAttributeFloat("FalloffExp", 1.0f));
					mpCurrentWorld->SetFogStart(pFog->GetAttributeFloat("FadeStart", 0.0f));
					mpCurrentWorld->SetFogEnd(pFog->GetAttributeFloat("FadeEnd", 0.0f));
					mpCurrentWorld->SetFogCulling(pFog->GetAttributeBool("Culling", true));
				}

				cXmlElement* pPost = pGlobal->GetFirstElement("PostEffects");
				if (pPost)
				{
					mpCurrentWorld->SetToneMapping(pPost->GetAttributeFloat("ToneMappingKey", 0.5f),
												   pPost->GetAttributeFloat("ToneMappingExposure", 0),
												   pPost->GetAttributeFloat("ToneMappingWhiteCut", 3.5f));
					mpCurrentWorld->SetColorGradingTexture(pPost->GetAttributeString("ColorGradingTexture", ""));
				}

				cXmlElement* pSky = pGlobal->GetFirstElement("SkyBox");
				if (pSky)
				{
					mpCurrentWorld->SetSkyBoxActive(pSky->GetAttributeBool("Active", false));
					mpCurrentWorld->SetSkyBoxColor(pSky->GetAttributeColor("Color", cColor(1, 1)));

					tString sSkyTex = pSky->GetAttributeString("Texture", "");
					if (sSkyTex != "")
					{
						iTexture* pSkyTexture = mpResources->GetTextureManager()->CreateCubeMap(sSkyTex, false);
						if (pSkyTexture) mpCurrentWorld->SetSkyBox(pSkyTexture, true);
						else Warning("SOMA hpm: could not load skybox texture '%s'\n", sSkyTex.c_str());
					}
				}
			}
		}
		else
		{
			Warning("SOMA hpm: root file '%s' has no HPLMap element!\n", cString::To8Char(asBaseFile).c_str());
		}

		hplDelete(pDoc);
	}

	void cWorldLoaderHpm::LoadTrack(const tWString& asBaseFile, const tString& asTrack, const tString& asFileIndexElement)
	{
		cHpmTrackStats& stats = mmapTrackStats[asTrack];
		unsigned long lStartTime = cPlatform::GetApplicationTime();

		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_") + cString::To16Char(asTrack), true);
		if (pDoc == NULL)
		{
			stats.mbFileMissing = true;
			return;
		}

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_" + asTrack);
		if (pRoot)
		{
			cXmlNodeListIterator sectionIt = pRoot->GetChildIterator();
			while (sectionIt.HasNext())
			{
				cXmlElement* pSection = sectionIt.Next()->ToElement();
				if (pSection->GetValue() != "Section") continue;

				tStringVec vFileIndex;
				if (asFileIndexElement != "") LoadLocalFileIndex(pSection, asFileIndexElement, vFileIndex);

				cXmlElement* pObjects = pSection->GetFirstElement("Objects");
				if (pObjects == NULL) continue;

				cXmlNodeListIterator objIt = pObjects->GetChildIterator();
				while (objIt.HasNext())
				{
					cXmlElement* pObjElem = objIt.Next()->ToElement();
					++stats.mlInXml;

					tString sReason = CreateTrackObject(asTrack, pObjElem, vFileIndex);
					if (sReason == "") ++stats.mlCreated;
					else ++stats.mmapSkipped[sReason];
				}
			}
		}

		hplDelete(pDoc);
		stats.mlTimeMs = (int)(cPlatform::GetApplicationTime() - lStartTime);
	}

	tString cWorldLoaderHpm::CreateTrackObject(const tString& asTrack, cXmlElement* apElement, const tStringVec& avFileIndex)
	{
		const tString& sTag = apElement->GetValue();

		if (asTrack == "StaticObject")
		{
			if (sTag != "StaticObject") return "unsupported_element:" + sTag;
			return CreateStaticObject(apElement, avFileIndex);
		}
		if (asTrack == "Primitive") return CreatePlanePrimitive(apElement);
		if (asTrack == "Entity")
		{
			if (sTag != "Entity") return "unsupported_element:" + sTag;
		{
			mpCurrentElement = apElement;
			tString sResult = CreateMapEntity(apElement, avFileIndex);
			mpCurrentElement = NULL;
			return sResult;
		}
		}
		if (asTrack == "Light")
		{
			return cEngineFileLoading::LoadLight(apElement, "", mpCurrentWorld, mpResources, true) ? "" : "load_failed:" + sTag;
		}
		if (asTrack == "Area")
		{
			if (sTag != "Area") return "unsupported_element:" + sTag;
		{
			mpCurrentElement = apElement;
			tString sResult = CreateMapArea(apElement);
			mpCurrentElement = NULL;
			return sResult;
		}
		}
		if (asTrack == "Sound")
		{
			if (sTag != "Sound") return "unsupported_element:" + sTag;
			cSoundEntity *pSound = cEngineFileLoading::LoadSound(apElement, "", mpCurrentWorld);
			if (pSound && !apElement->GetAttributeBool("Active", true)) pSound->SetActive(false);
			return pSound ? "" : "load_failed";
		}
		if (asTrack == "Decal")
		{
			if (sTag != "Decal") return "unsupported_element:" + sTag;
			return CreateDecal(apElement, avFileIndex);
		}
		if (asTrack == "Billboard")
		{
			if (sTag != "Billboard") return "unsupported_element:" + sTag;
			return cEngineFileLoading::LoadBillboard(apElement, "", mpCurrentWorld, mpResources, true, &mlstLightBillboardConnections) ? "" : "load_failed";
		}
		if (asTrack == "ParticleSystem")
		{
			if (sTag != "ParticleSystem") return "unsupported_element:" + sTag;
			return cEngineFileLoading::LoadParticleSystem(apElement, "", mpCurrentWorld) ? "" : "load_failed";
		}
		if (asTrack == "FogArea")
		{
			if (sTag != "FogArea") return "unsupported_element:" + sTag;
			return cEngineFileLoading::LoadFogArea(apElement, "", mpCurrentWorld, true) ? "" : "load_failed";
		}

		if (asTrack == "LightMask")
		{
			if (sTag != "LightMaskBox") return "unsupported_element:" + sTag;
			cMatrixf mtxRot = cMath::MatrixRotate(apElement->GetAttributeVector3f("Rotation", 0), eEulerRotationOrder_XYZ);
			cVector3f vHalf = apElement->GetAttributeVector3f("Size", 1) * apElement->GetAttributeVector3f("Scale", 1) * 0.5f;
			cVector3f vExtent;
			for (int i = 0; i < 3; ++i)
				vExtent.v[i] = 2.0f * (std::fabs(mtxRot.m[i][0]) * vHalf.x + std::fabs(mtxRot.m[i][1]) * vHalf.y + std::fabs(mtxRot.m[i][2]) * vHalf.z);
			unsigned int lID = (unsigned int)strtoul(apElement->GetAttributeString("ID", "0").c_str(), NULL, 10);
			mmapLightMasks[lID] = std::make_pair(apElement->GetAttributeVector3f("WorldPos", 0), vExtent);
			return "";
		}

		return "unsupported_track";
	}

	void cWorldLoaderHpm::ConnectLightMasks()
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

	void cWorldLoaderHpm::ConnectLightBillboards()
	{
		tEFL_LightBillboardConnectionListIt it = mlstLightBillboardConnections.begin();
		for (; it != mlstLightBillboardConnections.end(); ++it)
		{
			cBillboard* pBB = mpCurrentWorld->GetBillboardFromUniqueID(it->msBillboardID);
			iLight* pLight = mpCurrentWorld->GetLight(it->msLightName);
			if (pBB == NULL || pLight == NULL)
			{
				++mmapTrackStats["Billboard"].mmapSkipped["connect_light_missing"];
				continue;
			}
			pLight->AttachBillboard(pBB, pBB->GetColor());
		}
		mlstLightBillboardConnections.clear();
	}

	void cWorldLoaderHpm::CountUnsupportedFlatTracks(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_StaticObjectBatches"), false);
		if (pDoc)
		{
			cHpmTrackStats& stats = mmapTrackStats["StaticObjectBatches"];
			cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_StaticObjectBatches");
			cXmlElement* pBatches = pRoot ? pRoot->GetFirstElement("StaticObjectBatches") : NULL;
			if (pBatches)
			{
				cXmlNodeListIterator it = pBatches->GetChildIterator();
				while (it.HasNext()) { it.Next(); ++stats.mlInXml; }
			}
			if (stats.mlInXml > 0) stats.mmapSkipped["unsupported_track"] = stats.mlInXml;
			hplDelete(pDoc);
		}

	}

	static cXmlElement* HpmFirstChildWithText(cXmlElement* apParent, const tString& asName, tFloatVec& avOut)
	{
		cXmlElement* pElem = apParent->GetFirstElement(asName);
		if (pElem == NULL) return NULL;
		tString sSep = " ";
		cString::GetFloatVec(pElem->GetAttributeString("_Text", ""), avOut, &sSep);
		return pElem;
	}

	void cWorldLoaderHpm::LoadDetailMeshesTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_DetailMeshes"), false);
		if (pDoc == NULL) return;

		cHpmTrackStats& stats = mmapTrackStats["DetailMeshes"];
		unsigned long lStartTime = cPlatform::GetApplicationTime();

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_DetailMeshes");
		cXmlElement* pDetail = pRoot ? pRoot->GetFirstElement("DetailMeshes") : NULL;
		cXmlElement* pSections = pDetail ? pDetail->GetFirstElement("Sections") : NULL;
		if (pSections)
		{
			cXmlNodeListIterator secIt = pSections->GetChildIterator();
			while (secIt.HasNext())
			{
				cXmlNodeListIterator meshIt = secIt.Next()->ToElement()->GetChildIterator();
				while (meshIt.HasNext())
				{
					cXmlElement* pMeshElem = meshIt.Next()->ToElement();
					int lNum = pMeshElem->GetAttributeInt("NumOfInstances", 0);
					tString sFile = pMeshElem->GetAttributeString("File", "");
					stats.mlInXml += lNum;

					tFloatVec vPositions, vRotations;
					HpmFirstChildWithText(pMeshElem, "DetailMeshEntityPositions", vPositions);
					HpmFirstChildWithText(pMeshElem, "DetailMeshEntityRotations", vRotations);
					if ((int)vPositions.size() < lNum * 3 || (int)vRotations.size() < lNum * 4)
					{
						stats.mmapSkipped["instance_data_short:" + sFile] += lNum;
						continue;
					}

					for (int i = 0; i < lNum; ++i)
					{
						cMesh* pMesh = mpResources->GetMeshManager()->CreateMesh(sFile);
						if (pMesh == NULL)
						{
							stats.mmapSkipped["mesh_missing:" + sFile] += lNum - i;
							break;
						}

						cMeshEntity* pEntity = mpCurrentWorld->CreateMeshEntity("DetailMesh_" + cString::ToString(stats.mlCreated), pMesh, true);
						pEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);

						cQuaternion qRot(vRotations[i*4], vRotations[i*4+1], vRotations[i*4+2], vRotations[i*4+3]);
						cMatrixf mtxTransform = cMath::MatrixQuaternion(qRot);
						mtxTransform.SetTranslation(cVector3f(vPositions[i*3], vPositions[i*3+1], vPositions[i*3+2]));
						pEntity->SetMatrix(mtxTransform);

						++stats.mlCreated;
					}
				}
			}
		}

		hplDelete(pDoc);
		stats.mlTimeMs = (int)(cPlatform::GetApplicationTime() - lStartTime);
	}

	static tString HpmJsonEscape(const tString& asIn)
	{
		tString sOut;
		for (size_t i = 0; i < asIn.size(); ++i)
		{
			char c = asIn[i];
			if (c == '"' || c == '\\') { sOut += '\\'; sOut += c; }
			else if ((unsigned char)c < 0x20) sOut += ' ';
			else sOut += c;
		}
		return sOut;
	}

	void cWorldLoaderHpm::BuildLoadReport(const tString& asMap, int alTotalTimeMs)
	{
		tString sJson = "{\"map\":\"" + HpmJsonEscape(asMap) + "\",\"total_ms\":" + cString::ToString(alTotalTimeMs) +
						",\"terrain_active\":" + (mbTerrainActive ? "true" : "false") + ",\"tracks\":{";

		bool bFirstTrack = true;
		for (std::map<tString, cHpmTrackStats>::iterator it = mmapTrackStats.begin(); it != mmapTrackStats.end(); ++it)
		{
			cHpmTrackStats& stats = it->second;
			if (!bFirstTrack) sJson += ",";
			bFirstTrack = false;

			sJson += "\"" + it->first + "\":{\"xml\":" + cString::ToString(stats.mlInXml) +
					 ",\"created\":" + cString::ToString(stats.mlCreated) +
					 ",\"ms\":" + cString::ToString(stats.mlTimeMs) +
					 ",\"file_missing\":" + (stats.mbFileMissing ? "true" : "false") + ",\"skipped\":{";

			bool bFirstReason = true;
			for (std::map<tString, int>::iterator rIt = stats.mmapSkipped.begin(); rIt != stats.mmapSkipped.end(); ++rIt)
			{
				if (!bFirstReason) sJson += ",";
				bFirstReason = false;
				sJson += "\"" + HpmJsonEscape(rIt->first) + "\":" + cString::ToString(rIt->second);
			}
			sJson += "}}";

			int lSkipped = stats.mlInXml - stats.mlCreated;
			Log("  SOMA hpm track %-20s xml=%d created=%d skipped=%d (%d ms)\n", it->first.c_str(),
				stats.mlInXml, stats.mlCreated, lSkipped, stats.mlTimeMs);
		}
		sJson += "}}";

		msLastLoadReportJson = sJson;
	}


	void cWorldLoaderHpm::LoadExposureAreaTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_ExposureArea"), false);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_ExposureArea");
		if (pRoot)
		{
			cXmlNodeListIterator sectionIt = pRoot->GetChildIterator();
			while (sectionIt.HasNext())
			{
				cXmlElement* pSection = sectionIt.Next()->ToElement();
				if (pSection->GetValue() != "Section") continue;

				cXmlElement* pObjects = pSection->GetFirstElement("Objects");
				if (pObjects == NULL) continue;

				cXmlNodeListIterator objIt = pObjects->GetChildIterator();
				while (objIt.HasNext())
				{
					cXmlElement* pObjElem = objIt.Next()->ToElement();
					if (pObjElem->GetValue() != "ExposureArea") continue;
					++mmapTrackStats["ExposureArea"].mlInXml;
					++mmapTrackStats["ExposureArea"].mlCreated;

					cWorldExposureArea area;
					area.msName = pObjElem->GetAttributeString("Name", "");
					cMatrixf mtxTransform = cMath::MatrixRotate(pObjElem->GetAttributeVector3f("Rotation", 0), eEulerRotationOrder_XYZ);
					mtxTransform.SetTranslation(pObjElem->GetAttributeVector3f("WorldPos", 0));
					area.m_mtxInvTransform = cMath::MatrixInverse(mtxTransform);
					area.mvHalfSize = pObjElem->GetAttributeVector3f("Scale", 1) * 0.5f;
					area.mfExposure = pObjElem->GetAttributeFloat("Exposure", 0);
					area.mfWhiteCut = pObjElem->GetAttributeFloat("WhiteCut", 3.5f);
					area.mfTransitionTime = pObjElem->GetAttributeFloat("TransitionTime", 1);
					mpCurrentWorld->AddExposureArea(area);
				}
			}
		}

		hplDelete(pDoc);
	}

	void cWorldLoaderHpm::CheckTerrainTrackInactive(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_Terrain"), false);
		if (pDoc == NULL)
		{
			Log("  SOMA hpm: no Terrain track file - skipping (expected)\n");
			return;
		}

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_Terrain");
		bool bActive = false;
		if (pRoot)
		{
			cXmlElement* pTerrain = pRoot->GetFirstElement("Terrain");
			if (pTerrain) bActive = pTerrain->GetAttributeBool("Active", false);
		}
		mbTerrainActive = bActive;

		if (bActive)
			Warning("SOMA hpm: map has an ACTIVE terrain track - HPL2 has no terrain renderer, terrain will NOT be visible (out of scope for Phase 1)\n");
		else
			Log("  SOMA hpm: Terrain track present but inactive - skipping (as expected)\n");

		hplDelete(pDoc);
	}

	tString cWorldLoaderHpm::CreateStaticObject(cXmlElement* apElement, const tStringVec& avFileIndex)
	{
		tString sName = apElement->GetAttributeString("Name");
		tString sFileName;

		int lFileNameIdx = apElement->GetAttributeInt("FileIndex", -1);
		if (lFileNameIdx < 0)
		{
			sFileName = apElement->GetAttributeString("Filename");
		}
		else if (lFileNameIdx < (int)avFileIndex.size())
		{
			sFileName = avFileIndex[lFileNameIdx];
		}
		else
		{
			Warning("SOMA hpm: static object '%s' has out-of-bounds FileIndex %d\n", sName.c_str(), lFileNameIdx);
			return "file_index_out_of_bounds";
		}

		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation", 0);
		bool bCastsShadows = apElement->GetAttributeBool("CastShadows", true);
		bool bCollides = apElement->GetAttributeBool("Collides", true);
		int lID = apElement->GetAttributeInt("ID", -1);

		if (CheckTransformValidity(sName, vPosition, vRotation, vScale) == false) return "bad_transform";

		cMesh* pMesh = mpResources->GetMeshManager()->CreateMesh(sFileName);
		if (pMesh == NULL)
		{
			Warning("SOMA hpm: could not load mesh '%s' for static object '%s'\n", sFileName.c_str(), sName.c_str());
			return "mesh_missing:" + sFileName;
		}

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bCastsShadows);
		pMeshEntity->SetUniqueID(lID);
		pMeshEntity->SetColorMul(apElement->GetAttributeColor("ColorMul", cColor(1, 1)));
		pMeshEntity->SetIlluminationAmount(apElement->GetAttributeFloat("IllumBrightness", 1));
		pMeshEntity->SetIlluminationColor(apElement->GetAttributeColor("IllumColor", cColor(1, 1)));

		pMeshEntity->SetWorldMatrix(cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ), cMath::MatrixScale(vScale)));
		pMeshEntity->SetPosition(vPosition);

		if (bCollides)
			CreateStaticBodyForMesh(pMeshEntity, sName);

		return "";
	}

	tString cWorldLoaderHpm::CreatePlanePrimitive(cXmlElement* apElement)
	{
		tString sType = apElement->GetValue();
		if (sType != "Plane")
		{
			Warning("SOMA hpm: skipping unsupported primitive type '%s'\n", sType.c_str());
			return "unsupported_primitive:" + sType;
		}

		tString sName = apElement->GetAttributeString("Name");
		tString sMaterial = apElement->GetAttributeString("Material");
		bool bCastsShadows = apElement->GetAttributeBool("CastShadows", true);
		bool bCollides = apElement->GetAttributeBool("Collides", true);
		int lID = apElement->GetAttributeInt("ID", -1);

		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation", 0);

		if (CheckTransformValidity(sName, vPosition, vRotation, vScale) == false) return "bad_transform";

		cVector3f vStartCorner = apElement->GetAttributeVector3f("StartCorner", 0);
		cVector3f vEndCorner = apElement->GetAttributeVector3f("EndCorner", 0);

		cVector2f vUV1 = apElement->GetAttributeVector2f("Corner1UV");
		cVector2f vUV2 = apElement->GetAttributeVector2f("Corner2UV");
		cVector2f vUV3 = apElement->GetAttributeVector2f("Corner3UV");
		cVector2f vUV4 = apElement->GetAttributeVector2f("Corner4UV");

		cMesh* pMesh = mpGraphics->GetMeshCreator()->CreatePlane(sName, vStartCorner, vEndCorner, vUV1, vUV2, vUV3, vUV4, sMaterial);
		if (pMesh == NULL)
		{
			Warning("SOMA hpm: could not create plane primitive '%s'\n", sName.c_str());
			return "create_failed";
		}

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bCastsShadows);
		pMeshEntity->GetSubMeshEntity(0)->GetSubMesh()->SetMaterialName(sMaterial);
		pMeshEntity->SetUniqueID(lID);

		pMeshEntity->SetWorldMatrix(cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ), cMath::MatrixScale(vScale)));
		pMeshEntity->SetPosition(vPosition);

		if (bCollides)
			CreateStaticBodyForMesh(pMeshEntity, sName);

		return "";
	}

	tString cWorldLoaderHpm::CreateDecal(cXmlElement* apElement, const tStringVec& avFileIndex)
	{
		tString sName = apElement->GetAttributeString("Name");

		tString sMaterial;
		int lMaterialIdx = apElement->GetAttributeInt("MaterialIndex", -1);
		if (lMaterialIdx < 0) sMaterial = apElement->GetAttributeString("Material");
		else if (lMaterialIdx < (int)avFileIndex.size()) sMaterial = avFileIndex[lMaterialIdx];
		else return "file_index_out_of_bounds";

		cMesh* pMesh = cEngineFileLoading::LoadDecalMeshHelper(apElement->GetFirstElement("DecalMesh"), mpGraphics, mpResources,
																sName, sMaterial, apElement->GetAttributeColor("Color", cColor(1, 1)));
		if (pMesh == NULL) return "decal_mesh_failed";

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
		pMeshEntity->SetUniqueID(apElement->GetAttributeInt("ID", -1));

		return "";
	}

	void cWorldLoaderHpm::CreateStaticBodyForMesh(cMeshEntity* apMeshEntity, const tString& asName)
	{
		cMesh* pMesh = apMeshEntity->GetMesh();
		if (pMesh == NULL) return;

		int lSubMeshNum = pMesh->GetSubMeshNum();
		if (lSubMeshNum <= 0) return;

		tCollideShapeVec vShapes;
		for (int i = 0; i < lSubMeshNum; ++i)
		{
			cSubMesh* pSubMesh = pMesh->GetSubMesh(i);
			iVertexBuffer* pSrcVtxBuffer = pSubMesh->GetVertexBuffer();
			if (pSrcVtxBuffer == NULL) continue;

			iVertexBuffer* pVtxBuffer = pSrcVtxBuffer->CreateCopy(eVertexBufferType_Software, eVertexBufferUsageType_Static,
																   eVertexElementFlag_Position);
			pVtxBuffer->Transform(apMeshEntity->GetSubMeshEntity(i)->GetWorldMatrix());

			iCollideShape* pShape = mpCurrentPhysicsWorld->CreateMeshShape(pVtxBuffer);
			hplDelete(pVtxBuffer);

			if (pShape) vShapes.push_back(pShape);
		}

		for (size_t i = 0; i < vShapes.size(); ++i)
		{
			iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody(asName, vShapes[i]);
			pBody->SetMass(0);
		}
	}

	tString cWorldLoaderHpm::CreateMapEntity(cXmlElement* apElement, const tStringVec& avFileIndex)
	{
		tString sName = apElement->GetAttributeString("Name");
		int lID = apElement->GetAttributeInt("ID");
		bool bActive = apElement->GetAttributeBool("Active", true);
		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation", 0);

		if (CheckTransformValidity(sName, vPosition, vRotation, vScale) == false) return "bad_transform";

		tString sFilename;
		int lFileNameIdx = apElement->GetAttributeInt("FileIndex", -1);
		if (lFileNameIdx < 0)
		{
			sFilename = apElement->GetAttributeString("Filename");
		}
		else if (lFileNameIdx < (int)avFileIndex.size())
		{
			sFilename = avFileIndex[lFileNameIdx];
		}
		else
		{
			Warning("SOMA hpm: entity '%s' has out-of-bounds FileIndex %d\n", sName.c_str(), lFileNameIdx);
			return "file_index_out_of_bounds";
		}

		cResourceVarsObject userVars;
		cXmlElement* pUserVarsElem = apElement->GetFirstElement("UserVariables");
		if (pUserVarsElem) userVars.LoadVariables(pUserVarsElem);

		cMatrixf mtxTransform = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
		mtxTransform.SetTranslation(vPosition);

		iEntity3D* pEntity = mpCurrentWorld->CreateEntity(sName, mtxTransform, sFilename, lID, bActive, vScale, &userVars, false);
		return pEntity ? "" : "entity_failed:" + sFilename;
	}

	tString cWorldLoaderHpm::CreateMapArea(cXmlElement* apElement)
	{
		tString sName = apElement->GetAttributeString("Name");
		int lID = apElement->GetAttributeInt("ID");
		bool bActive = apElement->GetAttributeBool("Active", true);
		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation", 0);

		if (CheckTransformValidity(sName, vPosition, vRotation, vScale) == false) return "bad_transform";

		cMatrixf mtxTransform = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
		mtxTransform.SetTranslation(vPosition);

		tString sType = apElement->GetAttributeString("AreaType", "");

		iAreaLoader* pLoader = mpResources->GetAreaLoader(sType);
		if (pLoader == NULL)
		{
			Warning("SOMA hpm: no area loader registered for AreaType '%s' (area '%s')\n", sType.c_str(), sName.c_str());
			return "no_area_loader:" + sType;
		}

		cXmlElement* pVarRootElem = apElement->GetFirstElement("UserVariables");
		if (pVarRootElem) pLoader->LoadVariables(pVarRootElem);

		pLoader->Load(sName, lID, bActive, vScale, mtxTransform, mpCurrentWorld);

		return "";
	}

};
