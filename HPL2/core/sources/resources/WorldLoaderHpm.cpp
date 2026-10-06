
#include "resources/WorldLoaderHpm.h"

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
#include "graphics/Texture.h"
#include "graphics/DecalCreator.h"
#include "scene/SubMeshEntity.h"

#include "physics/Physics.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "physics/CollideShape.h"

#include "math/Math.h"

#include <cmath>

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
		mlCombinedObjects = 0;

		unsigned long lLoadStartTime = cPlatform::GetApplicationTime();
		mmapTrackStats.clear();
		mlstLightBillboardConnections.clear();
		mvLightParticleConnections.clear();

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
		LoadTerrain(asFile);

		for (auto& [key, batch] : m_mapStaticBatches) FlushStaticBatch(std::get<0>(key), batch);
		m_mapStaticBatches.clear();
		for (auto& [key, batch] : m_mapStaticShapeBatches)
		{
			iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody(batch.msName, batch.mvShapes.size() == 1 ? batch.mvShapes[0] : mpCurrentPhysicsWorld->CreateCompundShape(batch.mvShapes));
			pBody->SetMass(0);
			pBody->SetCollideCharacter(std::get<1>(key));
			pBody->SetCollide(std::get<2>(key));
			pBody->SetBlocksSound(std::get<3>(key));
			if (iPhysicsMaterial* pMat = mpCurrentPhysicsWorld->GetMaterialFromName(std::get<0>(key))) pBody->SetMaterial(pMat);
		}
		m_mapStaticShapeBatches.clear();
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
					mpCurrentWorld->SetFogBrightness(pFog->GetAttributeFloat("Brightness", 1));
					mpCurrentWorld->SetFogUnderwater(pFog->GetAttributeBool("Underwater", false));
					mpCurrentWorld->SetFogUseSkybox(pFog->GetAttributeBool("UseSkybox", false));
					mpCurrentWorld->SetFogApplyAfterFogAreas(pFog->GetAttributeBool("ApplyAfterFogAreas", true));
					mpCurrentWorld->SetFogNoise(pFog->GetAttributeFloat("NoiseStrength", 0), pFog->GetAttributeFloat("NoiseSize", 1),
												pFog->GetAttributeVector3f("NoiseTurbulence", 0));
					mpCurrentWorld->SetSecondaryFogActive(pFog->GetAttributeBool("SecondaryActive", false));
					mpCurrentWorld->SetSecondaryFogColor(pFog->GetAttributeColor("SecondaryColor", cColor(0, 0)));
					mpCurrentWorld->SetSecondaryFogStart(pFog->GetAttributeFloat("SecondaryFadeStart", 0));
					mpCurrentWorld->SetSecondaryFogEnd(pFog->GetAttributeFloat("SecondaryFadeEnd", 0));
					mpCurrentWorld->SetSecondaryFogFalloffExp(pFog->GetAttributeFloat("SecondaryFalloffExp", 1));
				}

				cXmlElement* pDir = pGlobal->GetFirstElement("DirLight");
				if (pDir)
				{
					cLightDirectional* pLight = mpCurrentWorld->GetDirectionalLight();
					mpCurrentWorld->SetDirectionalLightActive(pDir->GetAttributeBool("Active", false));
					pLight->SetDiffuseColor(pDir->GetAttributeColor("DiffuseColor", cColor(1, 1)));
					pLight->SetBrightness(pDir->GetAttributeFloat("Brightness", 1));
					pLight->SetDirection(pDir->GetAttributeVector3f("Direction", cVector3f(0, -1, 0)));
					pLight->SetAmbientColorSky(pDir->GetAttributeColor("SkyCol", cColor(0, 0)));
					pLight->SetAmbientColorGround(pDir->GetAttributeColor("GroundCol", cColor(0, 0)));
					pLight->SetCastShadows(pDir->GetAttributeBool("CastShadows", false));
					pLight->SetShadowCasterDistance(pDir->GetAttributeFloat("ShadowCasterDist", 40));
					pLight->SetShadowMapBiasMul(pDir->GetAttributeFloat("ShadowMapBiasMul", 1));
					pLight->SetShadowMapSlopeScaleBiasMul(pDir->GetAttributeFloat("ShadowMapSlopeScaleBiasMul", 1));
					pLight->SetShadowMapBlurAmount(pDir->GetAttributeFloat("ShadowBlurAmount", 6));
					pLight->SetAutoShadowSliceSettings(pDir->GetAttributeBool("AutoShadowSliceSettings", true));
					pLight->SetAutoShadowSliceLogTerm(pDir->GetAttributeFloat("AutoShadowSliceLogTerm", 0.9f));
				}

				cXmlElement* pEnv = pGlobal->GetFirstElement("EnvParticles");
				if (pEnv)
				{
					tString sTex = pEnv->GetAttributeString("Texture", "");
					iTexture* pTex = sTex == "" ? NULL : mpResources->GetTextureManager()->Create2D(sTex, true);
					cVector2f vSubDiv = pEnv->GetAttributeVector2f("SubDivUV", 1);
					cEnvironmentParticles* pParticles = mpCurrentWorld->CreateEnvironmentParticles("WorldEnvParticles");
					pParticles->Setup(pEnv->GetAttributeFloat("BoxSize", 1), pEnv->GetAttributeInt("NumParticles", 100),
									  pEnv->GetAttributeVector2f("ParticleSize", 1), cVector2l((int)vSubDiv.x, (int)vSubDiv.y),
									  pEnv->GetAttributeBool("AffectedByLight", false), pTex);
					pParticles->SetColor(pEnv->GetAttributeColor("Color", cColor(1, 1)));
					pParticles->SetBrightness(pEnv->GetAttributeFloat("Brightness", 1));
					pParticles->SetBoxDistance(pEnv->GetAttributeFloat("BoxDistance", 2));
					pParticles->SetGravityVelocity(pEnv->GetAttributeVector3f("GravityVelocity", cVector3f(0, -0.1f, 0)));
					pParticles->SetGravitySpeedRandomAmount(pEnv->GetAttributeFloat("GravitySpeedRandomAmount", 0));
					pParticles->SetWindVelocity(pEnv->GetAttributeVector3f("WindVelocity", 0));
					pParticles->SetWindSpeedRandomAmount(pEnv->GetAttributeFloat("WindSpeedRandomAmount", 0));
					pParticles->SetWindDirectionRandomAmount(pEnv->GetAttributeFloat("WindDirRandomAmount", 0));
					pParticles->SetRotateVelocity(pEnv->GetAttributeVector3f("RotateVelocity", 0));
					pParticles->SetRotateSpeedRandomAmount(pEnv->GetAttributeFloat("RotateSpeedRandomAmount", 0));
					pParticles->SetRotateSpeedRandomBothDirs(pEnv->GetAttributeBool("RotateBothDirs", false));
					pParticles->SetIterationNum(pEnv->GetAttributeFloat("NumIterations", 1));
					pParticles->SetFadeInStart(pEnv->GetAttributeFloat("FadeInStart", 0.2f));
					pParticles->SetFadeInEnd(pEnv->GetAttributeFloat("FadeInEnd", 1));
					pParticles->SetFadeOutStart(pEnv->GetAttributeFloat("FadeOutStart", 10));
					pParticles->SetFadeOutEnd(pEnv->GetAttributeFloat("FadeOutEnd", 20));
					mpCurrentWorld->SetEnvironmentParticlesActive(pEnv->GetAttributeBool("Active", false));
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
					mpCurrentWorld->SetSkyBoxBrightness(pSky->GetAttributeFloat("Brightness", 1));

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
		if (asTrack == "LensFlare")
		{
			if (sTag != "LensFlare") return "unsupported_element:" + sTag;
			return cEngineFileLoading::LoadLensFlare(apElement, "", mpCurrentWorld, mpResources, false) ? "" : "load_failed";
		}
		if (asTrack == "ParticleSystem")
		{
			if (sTag != "ParticleSystem") return "unsupported_element:" + sTag;
			cParticleSystem *pPS = cEngineFileLoading::LoadParticleSystem(apElement, "", mpCurrentWorld);
			tString sLight = apElement->GetAttributeString("ConnectLight");
			if (pPS && sLight != "") mvLightParticleConnections.push_back(std::make_pair(pPS, sLight));
			return pPS ? "" : "load_failed";
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

		for (size_t i = 0; i < mvLightParticleConnections.size(); ++i)
		{
			iLight* pLight = mpCurrentWorld->GetLight(mvLightParticleConnections[i].second);
			if (pLight) pLight->AttachParticleSystem(mvLightParticleConnections[i].first);
			else ++mmapTrackStats["ParticleSystem"].mmapSkipped["connect_light_missing"];
		}
		mvLightParticleConnections.clear();
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

					tFloatVec vPositions, vRotations, vRadii;
					HpmFirstChildWithText(pMeshElem, "DetailMeshEntityPositions", vPositions);
					HpmFirstChildWithText(pMeshElem, "DetailMeshEntityRotations", vRotations);
					HpmFirstChildWithText(pMeshElem, "DetailMeshEntityRadii", vRadii);
					float fMeshRadius = 0;
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

						// Instance scale = stored radius / mesh radius (cDetailMesh)
						if (fMeshRadius == 0)
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
						float fScale = i < (int)vRadii.size() && fMeshRadius > 0 ? vRadii[i] / fMeshRadius : 1;

						cMeshEntity* pEntity = mpCurrentWorld->CreateMeshEntity("DetailMesh_" + cString::ToString(stats.mlCreated), pMesh, true);
						pEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);

						cQuaternion qRot(vRotations[i*4], vRotations[i*4+1], vRotations[i*4+2], vRotations[i*4+3]);
						cMatrixf mtxTransform = cMath::MatrixMul(cMath::MatrixQuaternion(qRot), cMath::MatrixScale(fScale));
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

	void cWorldLoaderHpm::LoadTerrain(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_Terrain"), false);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_Terrain");
		cXmlElement* pTerrain = pRoot ? pRoot->GetFirstElement("Terrain") : NULL;
		mbTerrainActive = pTerrain && pTerrain->GetAttributeBool("Active", false);
		if (mbTerrainActive) CreateTerrain(asBaseFile, pTerrain);

		hplDelete(pDoc);
	}

	void cWorldLoaderHpm::CreateTerrain(const tWString& asBaseFile, cXmlElement* apTerrain)
	{
		const int lSize = apTerrain->GetAttributeInt("HeightMapSize", 1024);
		const int lPatch = apTerrain->GetAttributeInt("GeometryPatchSize", 32);
		const float fUnit = apTerrain->GetAttributeFloat("UnitSize", 1);
		const float fMaxHeight = apTerrain->GetAttributeFloat("MaxHeight", 1);
		const float fTile = apTerrain->GetAttributeFloat("BaseMaterialTileAmount", 1);
		const tString sMaterial = apTerrain->GetAttributeString("BaseMaterialFile");

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
		std::vector<cMaterial*> vBlend = CreateTerrainBlendMaterials(asBaseFile, apTerrain, lSize * fUnit);

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
					std::isnan(Height(x0 + x, z0 + z + 1)) || std::isnan(Height(x0 + x + 1, z0 + z + 1))) continue;
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
		iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody("Terrain", mpCurrentPhysicsWorld->CreateHeightFieldShape(lSize + 1, vElev.data(), fUnit, fMaxHeight / 65535.0f));
		pBody->SetMass(0);
		pBody->SetMatrix(cMath::MatrixTranslate(cVector3f(-fOffset, 0, -fOffset)));
		for (cMaterial* pMat : vBlend) pMatMgr->Destroy(pMat);
		CreateTerrainDecals(apTerrain, vPatches, fMaxHeight);
		CreateTerrainUndergrowth(apTerrain, vHeight, lSize, fUnit);
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

	// Rebirth's cTerrainUndergrowth, baked once: each plant's seeds depend only on its cell and grid index.
	void cWorldLoaderHpm::CreateTerrainUndergrowth(cXmlElement* apTerrain, const std::vector<float>& avHeight, int alSize, float afUnit)
	{
		cXmlElement* pSections = apTerrain->GetFirstElement("Sections");
		if (pSections == NULL) return;
		const float fGrid = apTerrain->GetAttributeFloat("UndergrowthGridSize", 10);
		const float fFadeStart = apTerrain->GetAttributeFloat("UndergrowthFadeStart", 25);
		const float fFadeEnd = apTerrain->GetAttributeFloat("UndergrowthFadeEnd", 35);
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
			iTexture* pTex = mpResources->GetTextureManager()->Create2D(pDoc->GetAttributeString("DiffuseTex"), true);
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
		cXmlNodeListIterator sectionIt = pSections->GetChildIterator();
		while (sectionIt.HasNext())
		{
			cXmlElement* pSection = sectionIt.Next()->ToElement();
			cXmlElement* pUndergrowth = pSection->GetFirstElement("Undergrowth");
			if (pUndergrowth == NULL) continue;
			tStringVec vFiles;
			LoadLocalFileIndex(pSection, "FileIndex_UndergrowthMaterials", vFiles);
			cXmlNodeListIterator areaIt = pUndergrowth->GetChildIterator();
			while (areaIt.HasNext())
			{
				cXmlElement* pArea = areaIt.Next()->ToElement();
				int lFile = pArea->GetAttributeInt("FileIndex", -1);
				if (lFile < 0 || lFile >= (int)vFiles.size()) continue;
				cUndergrowthArea area;
				if ((area.mlMat = LoadMat(vFiles[lFile])) < 0) continue;
				area.mbSub = cString::ToLowerCase(pArea->GetAttributeString("BlendType")) == "sub";
				area.mfMaxInfluence = pArea->GetAttributeFloat("MaxInfluence", 1);
				area.mfFadeBorder = pArea->GetAttributeFloat("FadeBorder", 0);
				if (pArea->GetValue() == "Circle")
				{
					area.mvCenter = pArea->GetAttributeVector2f("Center", 0);
					area.mfRadius = pArea->GetAttributeFloat("Radius", 0.5f);
					area.mvMin = area.mvCenter - area.mfRadius;
					area.mvMax = area.mvCenter + area.mfRadius;
				}
				else
				{
					std::vector<cVector2f> vPts;
					cXmlNodeListIterator pointIt = pArea->GetChildIterator();
					while (pointIt.HasNext()) vPts.push_back(pointIt.Next()->ToElement()->GetAttributeVector2f("Coords", 0));
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

	void cWorldLoaderHpm::CreateTerrainDecals(cXmlElement* apTerrain, const std::vector<cSubMeshEntity*>& avPatches, float afMaxHeight)
	{
		cXmlElement* pSections = apTerrain->GetFirstElement("Sections");
		if (pSections == NULL) return;
		cDecalCreator creator(mpGraphics->GetLowLevel(), mpResources);
		creator.SetMaxTrianglesPerDecal(100000);
		creator.SetDecalOffset(0.02f);
		int lCount = 0;
		cXmlNodeListIterator sectionIt = pSections->GetChildIterator();
		while (sectionIt.HasNext())
		{
			cXmlElement* pSection = sectionIt.Next()->ToElement();
			tStringVec vFiles;
			if (cXmlElement* pIndex = pSection->GetFirstElement("FileIndex_TerrainDecals"))
			{
				cXmlNodeListIterator fileIt = pIndex->GetChildIterator();
				while (fileIt.HasNext()) vFiles.push_back(fileIt.Next()->ToElement()->GetAttributeString("Path"));
			}
			cXmlElement* pDecals = pSection->GetFirstElement("Decals");
			if (pDecals == NULL) continue;
			cXmlNodeListIterator decalIt = pDecals->GetChildIterator();
			while (decalIt.HasNext())
			{
				cXmlElement* pDecal = decalIt.Next()->ToElement();
				int lFile = pDecal->GetAttributeInt("FileIndex", -1);
				if (lFile < 0 || lFile >= (int)vFiles.size()) continue;
				cVector2f vPos = pDecal->GetAttributeVector2f("Position", 0);
				cVector2f vSize = pDecal->GetAttributeVector2f("Size", 1);
				cMatrixf mtxRot = cMath::MatrixRotateY(pDecal->GetAttributeFloat("Angle", 0));

				creator.ClearMeshes();
				creator.SetMaterial(vFiles[lFile]);
				if (creator.GetMaterial() == NULL) continue;
				creator.SetDecalPosition(cVector3f(vPos.x, afMaxHeight * 0.5f, vPos.y));
				creator.SetDecalRight(mtxRot.GetRight(), false);
				creator.SetDecalForward(mtxRot.GetForward(), false);
				creator.SetDecalSize(cVector3f(vSize.x, afMaxHeight + 2, vSize.y));
				creator.SetColor(pDecal->GetAttributeColor("Color", cColor(1, 1)));
				creator.SetUVSubDivisions(creator.GetMaterial()->GetUVSubDivisions());
				creator.SetCurrentSubDiv(pDecal->GetAttributeInt("CurrentUVSubDiv", 0));
				for (cSubMeshEntity* pPatch : avPatches) creator.AddSubMesh(pPatch);
				cMesh* pMesh = creator.CreateDecalMesh();
				if (pMesh == NULL) continue;
				cMeshEntity* pEntity = mpCurrentWorld->CreateMeshEntity("TerrainDecal_" + cString::ToString(lCount++), pMesh, true);
				pEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
			}
		}
		creator.ClearMeshes();
	}

	std::vector<cMaterial*> cWorldLoaderHpm::CreateTerrainBlendMaterials(const tWString& asBaseFile, cXmlElement* apTerrain, float afSize)
	{
		std::vector<cMaterial*> vMats;
		cMaterialManager* pMatMgr = mpResources->GetMaterialManager();
		cXmlElement* pLayers = apTerrain->GetFirstElement("BlendLayers");
		if (pLayers == NULL) return vMats;

		auto ShareTexture = [](cMaterial* apDest, int alSlot, cMaterial* apSrc, eMaterialTexture aType)
		{
			iTexture* pTex = apSrc ? apSrc->GetTexture(aType) : NULL;
			if (pTex == NULL) return;
			pTex->IncUserCount();
			apDest->SetTexture((eMaterialTexture)alSlot, pTex);
		};

		cXmlNodeListIterator layerIt = pLayers->GetChildIterator();
		while (layerIt.HasNext())
		{
			cXmlElement* pLayer = layerIt.Next()->ToElement();
			tString sId = pLayer->GetAttributeString("ID", "0");
			tString sMap = cString::To8Char(cString::GetFileNameW(asBaseFile)) + "_Terrain_blendlayer_" + sId + ".dds";
			iTexture* pMap = mpResources->GetTextureManager()->Create2D(sMap, true);
			if (pMap == NULL) continue;

			cMaterial* pMat = pMatMgr->CreateCustomMaterial(sMap, mpGraphics->GetMaterialType("terrainblend"));
			cMaterialType_TerrainBlend_Vars* pVars = static_cast<cMaterialType_TerrainBlend_Vars*>(pMat->GetVars());
			pMat->SetTexture((eMaterialTexture)0, pMap);
			if (vMats.empty())
			{
				cMaterial* pBase = pMatMgr->CreateMaterial(apTerrain->GetAttributeString("BaseMaterialFile"));
				ShareTexture(pMat, 5, pBase, eMaterialTexture_Diffuse);
				if (pBase) pMatMgr->Destroy(pBase);
			}
			pVars->mfBaseTextureCoordScale = apTerrain->GetAttributeFloat("BaseMaterialTileAmount", 1) * afSize;

			cXmlNodeListIterator matIt = pLayer->GetChildIterator();
			for (int i = 0; i < 4 && matIt.HasNext(); ++i)
			{
				cXmlElement* pLayerMat = matIt.Next()->ToElement();
				pVars->mvTextureCoordScale[i] = pLayerMat->GetAttributeFloat("TileAmount", 1) * afSize;
				pVars->mvOneMinusFadeStart[i] = 1 - pLayerMat->GetAttributeFloat("StartFadeValue", 0);
				tString sFile = pLayerMat->GetAttributeString("File");
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
		return vMats;
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
		{
			cMatrixf mtxTransform = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
			mtxTransform.SetTranslation(vPosition);
			if (CreateStaticBodiesFromEnt(sFileName, mtxTransform, vScale, sName) == false)
				CreateStaticBodyForMesh(pMeshEntity);
		}

		return "";
	}

	// HPL3 iHplMapLoader::CreateStaticMeshEntity: a sibling .ent's bodies replace the mesh collider
	bool cWorldLoaderHpm::CreateStaticBodiesFromEnt(const tString& asFile, const cMatrixf& a_mtxTransform, const cVector3f& avScale, const tString& asName)
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
				tStaticShapeBatchKey key(pBodyElem->GetAttributeString("Material"), pBodyElem->GetAttributeBool("CollideCharacter", true),
										 pBodyElem->GetAttributeBool("CollideNonCharacter", true), pBodyElem->GetAttributeBool("BlocksSound", false),
										 (int)std::floor(vPos.x / 8), (int)std::floor(vPos.y / 8), (int)std::floor(vPos.z / 24));
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
			CreateStaticBodyForMesh(pMeshEntity);

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

	// HPL3 iHplMapLoader::CombineObjectsAndCreatePhysics: one body per physics material and area, not per submesh
	void cWorldLoaderHpm::CreateStaticBodyForMesh(cMeshEntity* apMeshEntity)
	{
		for (int i = 0; i < apMeshEntity->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity* pSubEnt = apMeshEntity->GetSubMeshEntity(i);
			iVertexBuffer* pSrcVtx = pSubEnt->GetSubMesh()->GetVertexBuffer();
			if (pSrcVtx == NULL) continue;

			cVector3f vCell = pSubEnt->GetBoundingVolume()->GetWorldCenter() / 16.0f;
			tStaticBatchKey key(pSubEnt->GetMaterial() ? pSubEnt->GetMaterial()->GetPhysicsMaterial() : "",
								(int)std::floor(vCell.x), (int)std::floor(vCell.y), (int)std::floor(vCell.z));
			cStaticBatch& batch = m_mapStaticBatches[key];
			if (batch.mvIdx.size() + pSrcVtx->GetIndexNum() > 50000) FlushStaticBatch(std::get<0>(key), batch);

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

	void cWorldLoaderHpm::FlushStaticBatch(const tString& asPhysicsMaterial, cStaticBatch& aBatch)
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
			iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody("CombinedObjects" + cString::ToString(mlCombinedObjects++), pShape);
			pBody->SetMass(0);
			if (iPhysicsMaterial* pMat = mpCurrentPhysicsWorld->GetMaterialFromName(asPhysicsMaterial)) pBody->SetMaterial(pMat);
		}
		aBatch = cStaticBatch();
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
