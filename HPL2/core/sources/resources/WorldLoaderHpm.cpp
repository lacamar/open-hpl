
#include "resources/WorldLoaderHpm.h"

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

		unsigned long lLoadStartTime = cPlatform::GetApplicationTime();
		mmapTrackStats.clear();
		mlstLightBillboardConnections.clear();
		mvLightParticleConnections.clear();

		mbTerrainActive = false;

		mpCurrentWorld = mpScene->CreateWorld(cString::To8Char(cString::GetFileNameW(asFile)));
		mpCurrentWorld->SetFilePath(asFile);

		mpCurrentPhysicsWorld = mpPhysics->CreateWorld(true);
		m_mapStaticShapes.clear();
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
		for (size_t i = 0; i < vHeight.size(); ++i)
		{
			const unsigned char* p = &vData[128 + i * 3];
			unsigned int lVal = p[0] | (p[1] << 8) | (p[2] << 16);
			vHeight[i] = lVal ? lVal * (fMaxHeight / 16777215.0f) : NAN;
		}
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
			std::vector<float> vPatch(lW * lH);
			for (int z = 0; z < lH; ++z)
			for (int x = 0; x < lW; ++x) vPatch[z * lW + x] = Height(x0 + x, z0 + z);
			iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody(sName, mpCurrentPhysicsWorld->CreateHeightFieldShape(lW, lH, vPatch.data(), fUnit));
			pBody->SetMass(0);
			pBody->SetMatrix(cMath::MatrixTranslate(cVector3f(x0 * fUnit - fOffset, 0, z0 * fUnit - fOffset)));
		}
		for (cMaterial* pMat : vBlend) pMatMgr->Destroy(pMat);
		CreateTerrainDecals(apTerrain, vPatches, fMaxHeight);
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
				CreateStaticBodyForMesh(pMeshEntity, sName);
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
				tCollideShapeVec vShapes;
				cXmlNodeListIterator shapeIt = pBodyElem->GetChildIterator();
				while (shapeIt.HasNext())
				{
					cXmlElement* pElem = shapeIt.Next()->ToElement();
					if (pElem->GetValue() != "Shape") continue;
					std::map<int, cXmlElement*>::iterator itShape = mapShapes.find(pElem->GetAttributeInt("ID"));
					iCollideShape* pShape = itShape != mapShapes.end() ? CreateCollideShape(itShape->second, mpCurrentPhysicsWorld, avScale) : NULL;
					if (pShape) vShapes.push_back(pShape);
				}
				if (vShapes.empty()) continue;

				iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody(asName, vShapes.size() == 1 ? vShapes[0] : mpCurrentPhysicsWorld->CreateCompundShape(vShapes));
				pBody->SetMass(0);
				cMatrixf mtxBody = cMath::MatrixRotate(pBodyElem->GetAttributeVector3f("Rotation"), eEulerRotationOrder_XYZ);
				mtxBody.SetTranslation(pBodyElem->GetAttributeVector3f("WorldPos") * avScale);
				pBody->SetMatrix(cMath::MatrixMul(a_mtxTransform, mtxBody));
				pBody->SetCollideCharacter(pBodyElem->GetAttributeBool("CollideCharacter", true));
				pBody->SetCollide(pBodyElem->GetAttributeBool("CollideNonCharacter", true));
				pBody->SetBlocksSound(pBodyElem->GetAttributeBool("BlocksSound", false));
				iPhysicsMaterial* pMat = mpCurrentPhysicsWorld->GetMaterialFromName(pBodyElem->GetAttributeString("Material"));
				if (pMat) pBody->SetMaterial(pMat);
				bCreated = true;
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

		for (int i = 0; i < lSubMeshNum; ++i)
		{
			cSubMesh* pSubMesh = pMesh->GetSubMesh(i);
			iVertexBuffer* pSrcVtxBuffer = pSubMesh->GetVertexBuffer();
			if (pSrcVtxBuffer == NULL) continue;

			// shared local-space shape per (submesh, scale); rotation/translation go in the body
			const cMatrixf& mtxWorld = apMeshEntity->GetSubMeshEntity(i)->GetWorldMatrix();
			cVector3f vAxis[3], vScale;
			for (int c = 0; c < 3; ++c)
			{
				vAxis[c] = cVector3f(mtxWorld.m[0][c], mtxWorld.m[1][c], mtxWorld.m[2][c]);
				vScale.v[c] = vAxis[c].Length();
			}
			bool bRigid = vScale.x > 0 && vScale.y > 0 && vScale.z > 0 &&
						  cMath::Vector3Dot(cMath::Vector3Cross(vAxis[0], vAxis[1]), vAxis[2]) > 0;
			for (int c = 0; c < 3 && bRigid; ++c)
				bRigid = std::fabs(cMath::Vector3Dot(vAxis[c], vAxis[(c + 1) % 3])) < 1e-3f * vScale.v[c] * vScale.v[(c + 1) % 3];

			cMatrixf mtxBody = cMatrixf::Identity;
			cMatrixf mtxShape = mtxWorld;
			if (bRigid)
			{
				for (int c = 0; c < 3; ++c)
					for (int r = 0; r < 3; ++r) mtxBody.m[r][c] = mtxWorld.m[r][c] / vScale.v[c];
				mtxBody.SetTranslation(mtxWorld.GetTranslation());
				mtxShape = cMath::MatrixScale(vScale);
			}

			char sKey[96];
			snprintf(sKey, sizeof(sKey), "%p %g %g %g", (void*)pSubMesh, vScale.x, vScale.y, vScale.z);
			iCollideShape* pShape = bRigid ? m_mapStaticShapes[sKey] : NULL;
			if (pShape == NULL)
			{
				iVertexBuffer* pVtxBuffer = pSrcVtxBuffer->CreateCopy(eVertexBufferType_Software, eVertexBufferUsageType_Static,
																	   eVertexElementFlag_Position);
				pVtxBuffer->Transform(mtxShape);
				pShape = mpCurrentPhysicsWorld->CreateMeshShape(pVtxBuffer);
				hplDelete(pVtxBuffer);
				if (pShape == NULL) continue;
				if (bRigid) m_mapStaticShapes[sKey] = pShape;
			}

			iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody(asName, pShape);
			pBody->SetMass(0);
			pBody->SetMatrix(mtxBody);
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
