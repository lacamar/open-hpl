/*
 * SOMA Phase 1 data loading. See WorldLoaderHpm.h for the full design
 * rationale (why this is a new, additive loader rather than a modification
 * of cWorldLoaderHplMap).
 */

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

	tString cWorldLoaderHpm::msLastLoadReportJson = "";

	//-----------------------------------------------------------------------

	cWorldLoaderHpm::cWorldLoaderHpm()
	{
		AddSupportedExtension("hpm");

		mpCurrentWorld = NULL;
		mpCurrentPhysicsWorld = NULL;

		mbTerrainActive = false;
	}

	//-----------------------------------------------------------------------

	cWorldLoaderHpm::~cWorldLoaderHpm()
	{
	}

	//-----------------------------------------------------------------------

	cWorld* cWorldLoaderHpm::LoadWorld(const tWString& asFile, tWorldLoadFlag aFlags)
	{
		Log(" -------- Loading SOMA hpm map '%s' ---------\n", cString::To8Char(cString::GetFileNameW(asFile)).c_str());

		unsigned long lLoadStartTime = cPlatform::GetApplicationTime();
		mmapTrackStats.clear();
		mlstLightBillboardConnections.clear();

		mbTerrainActive = false;

		///////////////////////
		// Create world and set up physics world with default values.
		// No collision bodies are created by this loader (Phase 1 has no
		// player controller), but a physics world is still attached since
		// other engine systems (e.g. cWorld::Compile) expect one to exist.
		mpCurrentWorld = mpScene->CreateWorld(cString::To8Char(cString::GetFileNameW(asFile)));
		mpCurrentWorld->SetFilePath(asFile);

		mpCurrentPhysicsWorld = mpPhysics->CreateWorld(true);
		mpCurrentPhysicsWorld->SetAccuracyLevel(ePhysicsAccuracy_Medium);
		mpCurrentPhysicsWorld->SetWorldSize(-300, 300);
		mpCurrentPhysicsWorld->SetMaxTimeStep(1.0f / 60.0f);
		mpCurrentWorld->SetPhysicsWorld(mpCurrentPhysicsWorld);

		///////////////////////
		// Root .hpm: fog / skybox
		LoadGlobalSettings(asFile);

		///////////////////////
		// Sidecar track files
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

		// Not loaded yet - run through LoadTrack() so the report counts them.
		LoadTrack(asFile, "Compound", "");
		LoadTrack(asFile, "LightMask", "");
		LoadTrack(asFile, "LensFlare", "");
		LoadTrack(asFile, "StaticComboArea", "");
		CountUnsupportedFlatTracks(asFile);
		LoadDetailMeshesTrack(asFile);

		LoadExposureAreaTrack(asFile);
		CheckTerrainTrackInactive(asFile);

		///////////////////////
		// Compile (sets up physics world size etc. from what was added)
		mpCurrentWorld->Compile(true);

		BuildLoadReport(cString::To8Char(cString::GetFileNameW(asFile)), (int)(cPlatform::GetApplicationTime() - lLoadStartTime));
		Log(" -------- Loading complete ---------\n");

		return mpCurrentWorld;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// SIDECAR FILE HELPERS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

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

	//-----------------------------------------------------------------------

	cXmlElement* cWorldLoaderHpm::GetTrackRoot(iXmlDocument* apDoc, const tString& asExpectedRootValue)
	{
		// NOTE: cXmlDocumentTiny::LoadFromTinyXMLData() (see
		// impl/XmlDocumentTiny.cpp) sets the *document node's own*
		// value/attributes to those of the file's single top-level XML
		// element - the root element is not a child of the document, the
		// document IS the root element. Every ".hpm"/".hpm_*" file has
		// exactly one such top-level element (HPLMap, HPLMapTrack_Entity,
		// etc.), so apDoc itself (not a GetFirstElement() lookup on it) is
		// that element.
		if (apDoc->GetValue() != asExpectedRootValue)
		{
			Warning("SOMA hpm: sidecar file has unexpected root element '%s' (expected '%s')\n",
					apDoc->GetValue().c_str(), asExpectedRootValue.c_str());
			return NULL;
		}
		return static_cast<cXmlElement*>(apDoc);
	}

	//-----------------------------------------------------------------------

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

	//-----------------------------------------------------------------------

	bool cWorldLoaderHpm::CheckTransformValidity(const tString& asName, const cVector3f& avPos, const cVector3f& avRot, const cVector3f& avScale)
	{
		if (cMath::Abs(avPos.x) > 10000.0f || cMath::Abs(avPos.y) > 10000.0f || cMath::Abs(avPos.z) > 10000.0f)
		{
			Warning("SOMA hpm: object '%s' has an invalid position: (%s)!\n", asName.c_str(), avPos.ToString().c_str());
			return false;
		}
		return true;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// ROOT .hpm: GLOBAL SETTINGS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

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

				cXmlElement* pSky = pGlobal->GetFirstElement("SkyBox");
				if (pSky)
				{
					mpCurrentWorld->SetSkyBoxActive(pSky->GetAttributeBool("Active", false));
					mpCurrentWorld->SetSkyBoxColor(pSky->GetAttributeColor("Color", cColor(1, 1)));

					// NOTE: SOMA's map data often bakes in an absolute
					// developer-machine path for the skybox texture (e.g.
					// "D:/work/depth/redist/textures/..."). That will not
					// resolve here; CreateCubeMap() logs a warning and
					// returns NULL, which is handled gracefully below. Not
					// a parsing bug - a known Phase 1 data-path gap.
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

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// TRACK LOADERS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

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

				// Each Section has its OWN local file index - the same
				// numeric index means a different file in another Section.
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

	//-----------------------------------------------------------------------

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
			return CreateMapEntity(apElement, avFileIndex);
		}
		if (asTrack == "Light")
		{
			return cEngineFileLoading::LoadLight(apElement, "", mpCurrentWorld, mpResources, true) ? "" : "load_failed:" + sTag;
		}
		if (asTrack == "Area")
		{
			if (sTag != "Area") return "unsupported_element:" + sTag;
			return CreateMapArea(apElement);
		}
		if (asTrack == "Sound")
		{
			if (sTag != "Sound") return "unsupported_element:" + sTag;
			return cEngineFileLoading::LoadSound(apElement, "", mpCurrentWorld) ? "" : "load_failed";
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

		return "unsupported_track";
	}

	//-----------------------------------------------------------------------

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

	//-----------------------------------------------------------------------

	// Tracks whose file layout is not Section/Objects - counted only.
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

	//-----------------------------------------------------------------------

	static cXmlElement* HpmFirstChildWithText(cXmlElement* apParent, const tString& asName, tFloatVec& avOut)
	{
		cXmlElement* pElem = apParent->GetFirstElement(asName);
		if (pElem == NULL) return NULL;
		tString sSep = " ";
		cString::GetFloatVec(pElem->GetAttributeString("_Text", ""), avOut, &sSep);
		return pElem;
	}

	// One static, non-colliding mesh entity per instance; no batching.
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
						// A fresh CreateMesh() per instance only bumps the shared resource's user count.
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

	//-----------------------------------------------------------------------

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

	//-----------------------------------------------------------------------


	// SOMA/Rebirth/Bunker's real HPL3-authored maps can carry a
	// .hpm_ExposureArea sidecar (HPLMapTrack_ExposureArea) - real per-area
	// exposure/white-point/transition-time data for a camera-position-based
	// HDR exposure system this engine has never had a native concept of at
	// all (Dark Descent's own maps never ship one, and cWorldLoaderHplMap -
	// the separate, DD-only loader - has no matching track). Not every map
	// has one (unlike StaticObject/Light/etc, always expected), so
	// abWarnIfMissing=false below, unlike every other track load in this
	// file. Deliberately simplified: applies only the FIRST ExposureArea
	// entity found as one flat cWorld::SetGlobalExposure() value for the
	// whole world, not the real system's spatial blend/fade between
	// multiple overlapping areas as the camera moves through them
	// (WorldPos/Scale/TransitionTime are read from the file but unused here)
	// - a real, honest first step, not the full system. See
	// cWorld::SetGlobalExposure()'s own doc comment and
	// cRendererDeferred::CopyToFrameBuffer() for how the resulting scale is
	// actually applied.
	void cWorldLoaderHpm::LoadExposureAreaTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_ExposureArea"), false);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_ExposureArea");
		if (pRoot)
		{
			bool bApplied = false;
			cXmlNodeListIterator sectionIt = pRoot->GetChildIterator();
			while (bApplied == false && sectionIt.HasNext())
			{
				cXmlElement* pSection = sectionIt.Next()->ToElement();
				if (pSection->GetValue() != "Section") continue;

				cXmlElement* pObjects = pSection->GetFirstElement("Objects");
				if (pObjects == NULL) continue;

				cXmlNodeListIterator objIt = pObjects->GetChildIterator();
				while (bApplied == false && objIt.HasNext())
				{
					cXmlElement* pObjElem = objIt.Next()->ToElement();
					if (pObjElem->GetValue() != "ExposureArea") continue;
					++mmapTrackStats["ExposureArea"].mlInXml;
					++mmapTrackStats["ExposureArea"].mlCreated;

					// HPL3's Exposure attribute is a real photographic EV
					// (stops) compensation, same convention as
					// cCamera-less exposure systems elsewhere (2 == twice
					// as bright, -1 == half as bright) - convert to the
					// plain linear multiply cWorld::SetGlobalExposure()
					// stores.
					float fExposureEv = pObjElem->GetAttributeFloat("Exposure", 0);
					mpCurrentWorld->SetGlobalExposure(powf(2.0f, fExposureEv));
					bApplied = true;

					Log("SOMA hpm: applying global exposure %f EV (%f linear) from '%s' - real per-area "
						"exposure data exists (WorldPos %s, additional areas if any) but only this first "
						"one is used, see LoadExposureAreaTrack()'s comment\n",
						fExposureEv, powf(2.0f, fExposureEv),
						pObjElem->GetAttributeString("Name", "").c_str(),
						pObjElem->GetAttributeString("WorldPos", "").c_str());
				}
			}
		}

		hplDelete(pDoc);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::CheckTerrainTrackInactive(const tWString& asBaseFile)
	{
		// Explicitly out of scope: HPL2 has no terrain renderer at all, and
		// every SOMA map's Terrain track is Active="false" in practice
		// (confirmed for 00_01_apartment). Parse only far enough to log
		// a clear warning in the (currently never observed) case a map
		// actually has it active - never render or otherwise act on it.
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

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PER-OBJECT CREATION
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

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

		pMeshEntity->SetWorldMatrix(cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ), cMath::MatrixScale(vScale)));
		pMeshEntity->SetPosition(vPosition);

		// Real collision body from the real "Collides" attribute - see
		// CreateStaticBodyForMesh() and the class comment in the header for
		// why this now exists (it didn't in the original Phase 1 loader).
		if (bCollides)
			CreateStaticBodyForMesh(pMeshEntity, sName);

		return "";
	}

	//-----------------------------------------------------------------------

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

	//-----------------------------------------------------------------------

	tString cWorldLoaderHpm::CreateDecal(cXmlElement* apElement, const tStringVec& avFileIndex)
	{
		tString sName = apElement->GetAttributeString("Name");

		tString sMaterial;
		int lMaterialIdx = apElement->GetAttributeInt("MaterialIndex", -1);
		if (lMaterialIdx < 0) sMaterial = apElement->GetAttributeString("Material");
		else if (lMaterialIdx < (int)avFileIndex.size()) sMaterial = avFileIndex[lMaterialIdx];
		else return "file_index_out_of_bounds";

		// Decal geometry is baked in world space - no transform applied.
		cMesh* pMesh = cEngineFileLoading::LoadDecalMeshHelper(apElement->GetFirstElement("DecalMesh"), mpGraphics, mpResources,
																sName, sMaterial, apElement->GetAttributeColor("Color", cColor(1, 1)));
		if (pMesh == NULL) return "decal_mesh_failed";

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
		pMeshEntity->SetUniqueID(apElement->GetAttributeInt("ID", -1));

		return "";
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::CreateStaticBodyForMesh(cMeshEntity* apMeshEntity, const tString& asName)
	{
		cMesh* pMesh = apMeshEntity->GetMesh();
		if (pMesh == NULL) return;

		int lSubMeshNum = pMesh->GetSubMeshNum();
		if (lSubMeshNum <= 0) return;

		// One iCollideShape per submesh, each built from a Software copy of
		// that submesh's own vertex buffer transformed into world space by
		// the mesh entity's already-fully-set world matrix (position +
		// rotation + scale - see both call sites above, which call this only
		// after SetWorldMatrix()/SetPosition()) - same technique
		// cWorldLoaderHplMap::AddObjectsToStaticMeshBody() uses for Amnesia's
		// own batched static bodies, just one object (not a spatially-batched
		// group of many) per call here, matching this loader's existing
		// "one cMeshEntity per source object, not batched" simplification
		// for rendering (see the class comment in WorldLoaderHpm.h).
		tCollideShapeVec vShapes;
		for (int i = 0; i < lSubMeshNum; ++i)
		{
			cSubMesh* pSubMesh = pMesh->GetSubMesh(i);
			iVertexBuffer* pSrcVtxBuffer = pSubMesh->GetVertexBuffer();
			if (pSrcVtxBuffer == NULL) continue;

			iVertexBuffer* pVtxBuffer = pSrcVtxBuffer->CreateCopy(eVertexBufferType_Software, eVertexBufferUsageType_Static,
																   eVertexElementFlag_Position);
			pVtxBuffer->Transform(apMeshEntity->GetWorldMatrix());

			iCollideShape* pShape = mpCurrentPhysicsWorld->CreateMeshShape(pVtxBuffer);
			hplDelete(pVtxBuffer);

			if (pShape) vShapes.push_back(pShape);
		}

		if (vShapes.empty()) return;

		iCollideShape* pFinalShape = vShapes.size() > 1 ? mpCurrentPhysicsWorld->CreateCompundShape(vShapes) : vShapes[0];

		iPhysicsBody* pBody = mpCurrentPhysicsWorld->CreateBody(asName, pFinalShape);
		pBody->SetMass(0); // static - real objects are never pushed/simulated by this loader
	}

	//-----------------------------------------------------------------------

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

	//-----------------------------------------------------------------------

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

	//-----------------------------------------------------------------------

};
