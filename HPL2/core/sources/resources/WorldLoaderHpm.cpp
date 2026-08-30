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

#include "graphics/Graphics.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/MeshCreator.h"
#include "graphics/LowLevelGraphics.h"

#include "physics/Physics.h"
#include "physics/PhysicsWorld.h"

#include "math/Math.h"

namespace hpl {

	//-----------------------------------------------------------------------

	cWorldLoaderHpm::cWorldLoaderHpm()
	{
		AddSupportedExtension("hpm");

		mpCurrentWorld = NULL;
		mpCurrentPhysicsWorld = NULL;

		mlStaticObjectsCreated = 0;
		mlPrimitivesCreated = 0;
		mlEntitiesCreated = 0;
		mlLightsCreated = 0;
		mlAreasCreated = 0;
		mlSoundsCreated = 0;
	}

	//-----------------------------------------------------------------------

	cWorldLoaderHpm::~cWorldLoaderHpm()
	{
	}

	//-----------------------------------------------------------------------

	cWorld* cWorldLoaderHpm::LoadWorld(const tWString& asFile, tWorldLoadFlag aFlags)
	{
		Log(" -------- Loading SOMA hpm map '%s' ---------\n", cString::To8Char(cString::GetFileNameW(asFile)).c_str());

		mlStaticObjectsCreated = 0;
		mlPrimitivesCreated = 0;
		mlEntitiesCreated = 0;
		mlLightsCreated = 0;
		mlAreasCreated = 0;
		mlSoundsCreated = 0;

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
		LoadStaticObjectsTrack(asFile);
		LoadPrimitivesTrack(asFile);
		LoadEntitiesTrack(asFile);
		LoadLightsTrack(asFile);
		LoadAreasTrack(asFile);
		LoadSoundsTrack(asFile);
		CheckTerrainTrackInactive(asFile);

		///////////////////////
		// Compile (sets up physics world size etc. from what was added)
		mpCurrentWorld->Compile(true);

		Log("  SOMA hpm: %d static objects, %d primitives, %d entities, %d lights, %d areas, %d sounds\n",
			mlStaticObjectsCreated, mlPrimitivesCreated, mlEntitiesCreated,
			mlLightsCreated, mlAreasCreated, mlSoundsCreated);
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

	void cWorldLoaderHpm::LoadStaticObjectsTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_StaticObject"), true);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_StaticObject");
		if (pRoot)
		{
			cXmlNodeListIterator sectionIt = pRoot->GetChildIterator();
			while (sectionIt.HasNext())
			{
				cXmlElement* pSection = sectionIt.Next()->ToElement();
				if (pSection->GetValue() != "Section") continue;

				// SOMA's multi-user map format gives each Section its OWN
				// local FileIndex_StaticObjects table (unlike Amnesia's
				// single map-wide FileIndex_StaticObjects) - the same
				// numeric FileIndex means a different mesh in a different
				// Section, so this must be reloaded per Section.
				tStringVec vFileIndex;
				LoadLocalFileIndex(pSection, "FileIndex_StaticObjects", vFileIndex);

				cXmlElement* pObjects = pSection->GetFirstElement("Objects");
				if (pObjects == NULL) continue;

				cXmlNodeListIterator objIt = pObjects->GetChildIterator();
				while (objIt.HasNext())
				{
					cXmlElement* pObjElem = objIt.Next()->ToElement();
					if (pObjElem->GetValue() != "StaticObject")
					{
						Warning("SOMA hpm: skipping unsupported static object track element '%s'\n", pObjElem->GetValue().c_str());
						continue;
					}
					CreateStaticObject(pObjElem, vFileIndex);
				}
			}
		}

		hplDelete(pDoc);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::LoadPrimitivesTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_Primitive"), true);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_Primitive");
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
					CreatePlanePrimitive(pObjElem);
				}
			}
		}

		hplDelete(pDoc);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::LoadEntitiesTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_Entity"), true);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_Entity");
		if (pRoot)
		{
			cXmlNodeListIterator sectionIt = pRoot->GetChildIterator();
			while (sectionIt.HasNext())
			{
				cXmlElement* pSection = sectionIt.Next()->ToElement();
				if (pSection->GetValue() != "Section") continue;

				tStringVec vFileIndex;
				LoadLocalFileIndex(pSection, "FileIndex_Entities", vFileIndex);

				cXmlElement* pObjects = pSection->GetFirstElement("Objects");
				if (pObjects == NULL) continue;

				cXmlNodeListIterator objIt = pObjects->GetChildIterator();
				while (objIt.HasNext())
				{
					cXmlElement* pObjElem = objIt.Next()->ToElement();
					if (pObjElem->GetValue() != "Entity")
					{
						Warning("SOMA hpm: skipping unsupported entity track element '%s'\n", pObjElem->GetValue().c_str());
						continue;
					}
					CreateMapEntity(pObjElem, vFileIndex);
				}
			}
		}

		hplDelete(pDoc);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::LoadLightsTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_Light"), true);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_Light");
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

					// BoxLight / SpotLight / PointLight - dispatched on tag
					// name internally by the same shared helper Amnesia's
					// loader uses.
					iLight* pLight = cEngineFileLoading::LoadLight(pObjElem, "", mpCurrentWorld, mpResources, true);
					if (pLight) ++mlLightsCreated;
				}
			}
		}

		hplDelete(pDoc);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::LoadAreasTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_Area"), true);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_Area");
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
					if (pObjElem->GetValue() != "Area")
					{
						Warning("SOMA hpm: skipping unsupported area track element '%s'\n", pObjElem->GetValue().c_str());
						continue;
					}
					CreateMapArea(pObjElem);
				}
			}
		}

		hplDelete(pDoc);
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::LoadSoundsTrack(const tWString& asBaseFile)
	{
		iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_Sound"), true);
		if (pDoc == NULL) return;

		cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_Sound");
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
					if (pObjElem->GetValue() != "Sound")
					{
						Warning("SOMA hpm: skipping unsupported sound track element '%s'\n", pObjElem->GetValue().c_str());
						continue;
					}

					cSoundEntity* pSound = cEngineFileLoading::LoadSound(pObjElem, "", mpCurrentWorld);
					if (pSound) ++mlSoundsCreated;
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

	void cWorldLoaderHpm::CreateStaticObject(cXmlElement* apElement, const tStringVec& avFileIndex)
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
			return;
		}

		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation", 0);
		bool bCastsShadows = apElement->GetAttributeBool("CastShadows", true);
		int lID = apElement->GetAttributeInt("ID", -1);

		// NOTE: "Collides" is intentionally ignored - this loader creates
		// no physics bodies (see file header).

		if (CheckTransformValidity(sName, vPosition, vRotation, vScale) == false) return;

		cMesh* pMesh = mpResources->GetMeshManager()->CreateMesh(sFileName);
		if (pMesh == NULL)
		{
			Warning("SOMA hpm: could not load mesh '%s' for static object '%s'\n", sFileName.c_str(), sName.c_str());
			return;
		}

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bCastsShadows);
		pMeshEntity->SetUniqueID(lID);

		pMeshEntity->SetWorldMatrix(cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ), cMath::MatrixScale(vScale)));
		pMeshEntity->SetPosition(vPosition);

		++mlStaticObjectsCreated;
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::CreatePlanePrimitive(cXmlElement* apElement)
	{
		tString sType = apElement->GetValue();
		if (sType != "Plane")
		{
			Warning("SOMA hpm: skipping unsupported primitive type '%s'\n", sType.c_str());
			return;
		}

		tString sName = apElement->GetAttributeString("Name");
		tString sMaterial = apElement->GetAttributeString("Material");
		bool bCastsShadows = apElement->GetAttributeBool("CastShadows", true);
		int lID = apElement->GetAttributeInt("ID", -1);

		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation", 0);

		if (CheckTransformValidity(sName, vPosition, vRotation, vScale) == false) return;

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
			return;
		}

		cMeshEntity* pMeshEntity = mpCurrentWorld->CreateMeshEntity(sName, pMesh, true);
		pMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, bCastsShadows);
		pMeshEntity->GetSubMeshEntity(0)->GetSubMesh()->SetMaterialName(sMaterial);
		pMeshEntity->SetUniqueID(lID);

		pMeshEntity->SetWorldMatrix(cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ), cMath::MatrixScale(vScale)));
		pMeshEntity->SetPosition(vPosition);

		++mlPrimitivesCreated;
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::CreateMapEntity(cXmlElement* apElement, const tStringVec& avFileIndex)
	{
		tString sName = apElement->GetAttributeString("Name");
		int lID = apElement->GetAttributeInt("ID");
		bool bActive = apElement->GetAttributeBool("Active", true);
		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation", 0);

		if (CheckTransformValidity(sName, vPosition, vRotation, vScale) == false) return;

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
			return;
		}

		cResourceVarsObject userVars;
		cXmlElement* pUserVarsElem = apElement->GetFirstElement("UserVariables");
		if (pUserVarsElem) userVars.LoadVariables(pUserVarsElem);

		cMatrixf mtxTransform = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
		mtxTransform.SetTranslation(vPosition);

		iEntity3D* pEntity = mpCurrentWorld->CreateEntity(sName, mtxTransform, sFilename, lID, bActive, vScale, &userVars, false);
		if (pEntity) ++mlEntitiesCreated;
	}

	//-----------------------------------------------------------------------

	void cWorldLoaderHpm::CreateMapArea(cXmlElement* apElement)
	{
		tString sName = apElement->GetAttributeString("Name");
		int lID = apElement->GetAttributeInt("ID");
		bool bActive = apElement->GetAttributeBool("Active", true);
		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation", 0);

		if (CheckTransformValidity(sName, vPosition, vRotation, vScale) == false) return;

		cMatrixf mtxTransform = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
		mtxTransform.SetTranslation(vPosition);

		tString sType = apElement->GetAttributeString("AreaType", "");

		iAreaLoader* pLoader = mpResources->GetAreaLoader(sType);
		if (pLoader == NULL)
		{
			Warning("SOMA hpm: no area loader registered for AreaType '%s' (area '%s')\n", sType.c_str(), sName.c_str());
			return;
		}

		cXmlElement* pVarRootElem = apElement->GetFirstElement("UserVariables");
		if (pVarRootElem) pLoader->LoadVariables(pVarRootElem);

		pLoader->Load(sName, lID, bActive, vScale, mtxTransform, mpCurrentWorld);

		++mlAreasCreated;
	}

	//-----------------------------------------------------------------------

};
