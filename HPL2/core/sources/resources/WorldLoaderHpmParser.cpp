
#include "resources/HpmBuilder.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>

#include "system/String.h"
#include "math/Math.h"
#include "resources/XmlDocument.h"

namespace hpl {

	namespace
	{
		struct cTrackStats
		{
			cTrackStats() : mlInXml(0), mlCreated(0), mlTimeMs(0), mbFileMissing(false) {}
			int mlInXml;
			int mlCreated;
			int mlTimeMs;
			bool mbFileMissing;
			std::map<tString, int> mmapSkipped;
		};

		void Set(float* apDst, const cVector3f& avSrc)
		{
			apDst[0] = avSrc.x;
			apDst[1] = avSrc.y;
			apDst[2] = avSrc.z;
		}

		void Set(float* apDst, const cVector2f& avSrc)
		{
			apDst[0] = avSrc.x;
			apDst[1] = avSrc.y;
		}

		void Set(float* apDst, const cColor& aSrc)
		{
			apDst[0] = aSrc.r;
			apDst[1] = aSrc.g;
			apDst[2] = aSrc.b;
			apDst[3] = aSrc.a;
		}

		class cHpmParser
		{
		public:
			cHpmParser(iHpmBuilder& aBuilder) : mB(aBuilder) {}

			void Run(const tWString& asFile)
			{
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
				ConnectLights();
				LoadTrack(asFile, "LightMask", "");
				mB.ConnectLightMasks();

				LoadTrack(asFile, "Compound", "");
				LoadTrack(asFile, "LensFlare", "");
				LoadTrack(asFile, "StaticComboArea", "");
				CountUnsupportedFlatTracks(asFile);
				LoadDetailMeshesTrack(asFile);

				LoadExposureAreaTrack(asFile);
				LoadTerrain(asFile);

				for (std::map<tString, cTrackStats>::iterator it = mmapTrackStats.begin(); it != mmapTrackStats.end(); ++it)
				{
					std::vector<OhplHpmSkip> vSkipped;
					for (std::map<tString, int>::iterator rIt = it->second.mmapSkipped.begin(); rIt != it->second.mmapSkipped.end(); ++rIt)
					{
						OhplHpmSkip skip = {rIt->first.c_str(), rIt->second};
						vSkipped.push_back(skip);
					}
					mB.TrackReport(it->first.c_str(), it->second.mlInXml, it->second.mlCreated, it->second.mlTimeMs, it->second.mbFileMissing,
								   vSkipped.data(), vSkipped.size());
				}
			}

		private:
			void Warn(const char* asFormat, ...)
			{
				char sText[4096];
				va_list ap;
				va_start(ap, asFormat);
				vsnprintf(sText, sizeof(sText), asFormat, ap);
				va_end(ap);
				mB.Warn(sText);
			}

			iXmlDocument* OpenSidecar(const tWString& asBaseFile, const tWString& asSuffix, bool abWarnIfMissing)
			{
				tWString sPath = asBaseFile + asSuffix;
				iXmlDocument* pDoc = mB.OpenXml(sPath);
				if (pDoc == NULL && abWarnIfMissing)
					Warn("SOMA hpm: could not open/parse '%s'\n", cString::To8Char(sPath).c_str());
				return pDoc;
			}

			cXmlElement* GetTrackRoot(iXmlDocument* apDoc, const tString& asExpectedRootValue)
			{
				if (apDoc->GetValue() != asExpectedRootValue)
				{
					Warn("SOMA hpm: sidecar file has unexpected root element '%s' (expected '%s')\n",
						 apDoc->GetValue().c_str(), asExpectedRootValue.c_str());
					return NULL;
				}
				return static_cast<cXmlElement*>(apDoc);
			}

			void LoadLocalFileIndex(cXmlElement* apSection, const tString& asIndexElement, tStringVec& avIndexOut)
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

			bool CheckTransformValidity(const tString& asName, const cVector3f& avPos)
			{
				if (cMath::Abs(avPos.x) > 10000.0f || cMath::Abs(avPos.y) > 10000.0f || cMath::Abs(avPos.z) > 10000.0f)
				{
					Warn("SOMA hpm: object '%s' has an invalid position: (%s)!\n", asName.c_str(), avPos.ToString().c_str());
					return false;
				}
				return true;
			}

			void LoadGlobalSettings(const tWString& asBaseFile)
			{
				iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W(""), true);
				if (pDoc == NULL) return;

				cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMap");
				if (pRoot)
				{
					cXmlElement* pGlobal = pRoot->GetFirstElement("GlobalSettings");
					if (pGlobal)
					{
						if (cXmlElement* pFog = pGlobal->GetFirstElement("Fog"))
						{
							OhplHpmFog f;
							f.active = pFog->GetAttributeBool("Active", false);
							Set(f.color, pFog->GetAttributeColor("Color", cColor(1, 1)));
							f.falloffExp = pFog->GetAttributeFloat("FalloffExp", 1.0f);
							f.fadeStart = pFog->GetAttributeFloat("FadeStart", 0.0f);
							f.fadeEnd = pFog->GetAttributeFloat("FadeEnd", 0.0f);
							f.culling = pFog->GetAttributeBool("Culling", true);
							f.brightness = pFog->GetAttributeFloat("Brightness", 1);
							f.underwater = pFog->GetAttributeBool("Underwater", false);
							f.useSkybox = pFog->GetAttributeBool("UseSkybox", false);
							f.applyAfterFogAreas = pFog->GetAttributeBool("ApplyAfterFogAreas", true);
							f.noiseStrength = pFog->GetAttributeFloat("NoiseStrength", 0);
							f.noiseSize = pFog->GetAttributeFloat("NoiseSize", 1);
							Set(f.noiseTurbulence, pFog->GetAttributeVector3f("NoiseTurbulence", 0));
							f.secondaryActive = pFog->GetAttributeBool("SecondaryActive", false);
							Set(f.secondaryColor, pFog->GetAttributeColor("SecondaryColor", cColor(0, 0)));
							f.secondaryFadeStart = pFog->GetAttributeFloat("SecondaryFadeStart", 0);
							f.secondaryFadeEnd = pFog->GetAttributeFloat("SecondaryFadeEnd", 0);
							f.secondaryFalloffExp = pFog->GetAttributeFloat("SecondaryFalloffExp", 1);
							f.lighten = pFog->GetAttributeBool("Lighten", false);
							f.heightBased = pFog->GetAttributeBool("HeightBased", false);
							f.exponential = pFog->GetAttributeBool("Exponential", false);
							f.density = pFog->GetAttributeFloat("Density", 0.25f);
							f.heightDensity = pFog->GetAttributeFloat("HeightDensity", 0.25f);
							f.heightHorizon = pFog->GetAttributeFloat("HeightHorizon", 1);
							f.secondaryDensity = pFog->GetAttributeFloat("SecondaryDensity", 0.25f);
							f.secondaryHeightDensity = pFog->GetAttributeFloat("SecondaryHeightDensity", 0.25f);
							f.secondaryHeightHorizon = pFog->GetAttributeFloat("SecondaryHeightHorizon", 1);
							mB.GlobalFog(f);
						}

						if (cXmlElement* pCull = pGlobal->GetFirstElement("DistanceCulling"))
						{
							OhplHpmDistanceCull c;
							c.active = pCull->GetAttributeBool("Active", false);
							c.minRange = pCull->GetAttributeFloat("MinRange", 0);
							c.screenSize = pCull->GetAttributeFloat("ScreenSize", 0.05f);
							c.randomSize = pCull->GetAttributeFloat("RandomSize", 0.25f);
							mB.DistanceCull(c);
						}

						if (cXmlElement* pDir = pGlobal->GetFirstElement("DirLight"))
						{
							OhplHpmDirLight d;
							d.active = pDir->GetAttributeBool("Active", false);
							Set(d.diffuseColor, pDir->GetAttributeColor("DiffuseColor", cColor(1, 1)));
							d.brightness = pDir->GetAttributeFloat("Brightness", 1);
							Set(d.direction, pDir->GetAttributeVector3f("Direction", cVector3f(0, -1, 0)));
							Set(d.skyColor, pDir->GetAttributeColor("SkyCol", cColor(0, 0)));
							Set(d.groundColor, pDir->GetAttributeColor("GroundCol", cColor(0, 0)));
							d.castShadows = pDir->GetAttributeBool("CastShadows", false);
							d.shadowCasterDist = pDir->GetAttributeFloat("ShadowCasterDist", 40);
							d.shadowMapBiasMul = pDir->GetAttributeFloat("ShadowMapBiasMul", 1);
							d.shadowMapSlopeScaleBiasMul = pDir->GetAttributeFloat("ShadowMapSlopeScaleBiasMul", 1);
							d.shadowBlurAmount = pDir->GetAttributeFloat("ShadowBlurAmount", 6);
							d.autoShadowSliceSettings = pDir->GetAttributeBool("AutoShadowSliceSettings", true);
							d.autoShadowSliceLogTerm = pDir->GetAttributeFloat("AutoShadowSliceLogTerm", 0.9f);
							mB.DirLight(d);
						}

						if (cXmlElement* pEnv = pGlobal->GetFirstElement("EnvParticles"))
						{
							OhplHpmEnvParticles e;
							tString sTex = pEnv->GetAttributeString("Texture", "");
							e.texture = sTex.c_str();
							Set(e.subDivUv, pEnv->GetAttributeVector2f("SubDivUV", 1));
							e.boxSize = pEnv->GetAttributeFloat("BoxSize", 1);
							e.numParticles = pEnv->GetAttributeInt("NumParticles", 100);
							Set(e.particleSize, pEnv->GetAttributeVector2f("ParticleSize", 1));
							e.affectedByLight = pEnv->GetAttributeBool("AffectedByLight", false);
							Set(e.color, pEnv->GetAttributeColor("Color", cColor(1, 1)));
							e.brightness = pEnv->GetAttributeFloat("Brightness", 1);
							e.boxDistance = pEnv->GetAttributeFloat("BoxDistance", 2);
							Set(e.gravityVelocity, pEnv->GetAttributeVector3f("GravityVelocity", cVector3f(0, -0.1f, 0)));
							e.gravitySpeedRandomAmount = pEnv->GetAttributeFloat("GravitySpeedRandomAmount", 0);
							Set(e.windVelocity, pEnv->GetAttributeVector3f("WindVelocity", 0));
							e.windSpeedRandomAmount = pEnv->GetAttributeFloat("WindSpeedRandomAmount", 0);
							e.windDirRandomAmount = pEnv->GetAttributeFloat("WindDirRandomAmount", 0);
							Set(e.rotateVelocity, pEnv->GetAttributeVector3f("RotateVelocity", 0));
							e.rotateSpeedRandomAmount = pEnv->GetAttributeFloat("RotateSpeedRandomAmount", 0);
							e.rotateBothDirs = pEnv->GetAttributeBool("RotateBothDirs", false);
							e.numIterations = pEnv->GetAttributeFloat("NumIterations", 1);
							e.fadeInStart = pEnv->GetAttributeFloat("FadeInStart", 0.2f);
							e.fadeInEnd = pEnv->GetAttributeFloat("FadeInEnd", 1);
							e.fadeOutStart = pEnv->GetAttributeFloat("FadeOutStart", 10);
							e.fadeOutEnd = pEnv->GetAttributeFloat("FadeOutEnd", 20);
							e.active = pEnv->GetAttributeBool("Active", false);
							mB.EnvParticles(e);
						}

						if (cXmlElement* pPost = pGlobal->GetFirstElement("PostEffects"))
						{
							OhplHpmPostEffects p;
							p.toneMappingKey = pPost->GetAttributeFloat("ToneMappingKey", 0.5f);
							p.toneMappingExposure = pPost->GetAttributeFloat("ToneMappingExposure", 0);
							p.toneMappingWhiteCut = pPost->GetAttributeFloat("ToneMappingWhiteCut", 3.5f);
							tString sGrading = pPost->GetAttributeString("ColorGradingTexture", "");
							p.colorGradingTexture = sGrading.c_str();
							mB.PostEffects(p);
						}

						if (cXmlElement* pSky = pGlobal->GetFirstElement("SkyBox"))
						{
							OhplHpmSkyBox s;
							s.active = pSky->GetAttributeBool("Active", false);
							Set(s.color, pSky->GetAttributeColor("Color", cColor(1, 1)));
							s.brightness = pSky->GetAttributeFloat("Brightness", 1);
							tString sSkyTex = pSky->GetAttributeString("Texture", "");
							s.texture = sSkyTex.c_str();
							mB.SkyBox(s);
						}
					}
				}
				else
				{
					Warn("SOMA hpm: root file '%s' has no HPLMap element!\n", cString::To8Char(asBaseFile).c_str());
				}

				hplDelete(pDoc);
			}

			void LoadTrack(const tWString& asBaseFile, const tString& asTrack, const tString& asFileIndexElement)
			{
				cTrackStats& stats = mmapTrackStats[asTrack];
				unsigned long lStartTime = mB.NowMs();

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
							mB.ObjectDone();
						}
					}
				}

				hplDelete(pDoc);
				stats.mlTimeMs = (int)(mB.NowMs() - lStartTime);
			}

			tString CreateTrackObject(const tString& asTrack, cXmlElement* apElement, const tStringVec& avFileIndex)
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
					return mB.Light(apElement) ? "" : "load_failed:" + sTag;
				}
				if (asTrack == "Area")
				{
					if (sTag != "Area") return "unsupported_element:" + sTag;
					return CreateMapArea(apElement);
				}
				if (asTrack == "Sound")
				{
					if (sTag != "Sound") return "unsupported_element:" + sTag;
					if (mB.Sound(apElement, apElement->GetAttributeBool("Active", true))) return "";
					return apElement->GetAttributeString("SoundEntityFile", "").empty() ? "empty_file" : "load_failed";
				}
				if (asTrack == "Decal")
				{
					if (sTag != "Decal") return "unsupported_element:" + sTag;
					return CreateDecal(apElement, avFileIndex);
				}
				if (asTrack == "Billboard")
				{
					if (sTag != "Billboard") return "unsupported_element:" + sTag;
					return mB.Billboard(apElement) ? "" : "load_failed";
				}
				if (asTrack == "LensFlare")
				{
					if (sTag != "LensFlare") return "unsupported_element:" + sTag;
					return mB.LensFlare(apElement) ? "" : "load_failed";
				}
				if (asTrack == "ParticleSystem")
				{
					if (sTag != "ParticleSystem") return "unsupported_element:" + sTag;
					tString sLight = apElement->GetAttributeString("ConnectLight");
					return mB.ParticleSystem(apElement, sLight.c_str()) ? "" : "load_failed";
				}
				if (asTrack == "FogArea")
				{
					if (sTag != "FogArea") return "unsupported_element:" + sTag;
					return mB.FogArea(apElement) ? "" : "load_failed";
				}

				if (asTrack == "LightMask")
				{
					if (sTag != "LightMaskBox") return "unsupported_element:" + sTag;
					OhplHpmLightMask m;
					Set(m.rotation, apElement->GetAttributeVector3f("Rotation", 0));
					Set(m.size, apElement->GetAttributeVector3f("Size", 1));
					Set(m.scale, apElement->GetAttributeVector3f("Scale", 1));
					m.id = (unsigned int)strtoul(apElement->GetAttributeString("ID", "0").c_str(), NULL, 10);
					Set(m.worldPos, apElement->GetAttributeVector3f("WorldPos", 0));
					mB.LightMaskBox(m);
					return "";
				}

				return "unsupported_track";
			}

			void ConnectLights()
			{
				int lMissingBillboards, lMissingParticles;
				mB.ConnectLights(&lMissingBillboards, &lMissingParticles);
				if (lMissingBillboards > 0) mmapTrackStats["Billboard"].mmapSkipped["connect_light_missing"] += lMissingBillboards;
				if (lMissingParticles > 0) mmapTrackStats["ParticleSystem"].mmapSkipped["connect_light_missing"] += lMissingParticles;
			}

			void CountUnsupportedFlatTracks(const tWString& asBaseFile)
			{
				iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_StaticObjectBatches"), false);
				if (pDoc)
				{
					cTrackStats& stats = mmapTrackStats["StaticObjectBatches"];
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

			cXmlElement* FirstChildWithText(cXmlElement* apParent, const tString& asName, tFloatVec& avOut)
			{
				cXmlElement* pElem = apParent->GetFirstElement(asName);
				if (pElem == NULL) return NULL;
				tString sSep = " ";
				cString::GetFloatVec(pElem->GetAttributeString("_Text", ""), avOut, &sSep);
				return pElem;
			}

			void LoadDetailMeshesTrack(const tWString& asBaseFile)
			{
				iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_DetailMeshes"), false);
				if (pDoc == NULL) return;

				cTrackStats& stats = mmapTrackStats["DetailMeshes"];
				unsigned long lStartTime = mB.NowMs();

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
							FirstChildWithText(pMeshElem, "DetailMeshEntityPositions", vPositions);
							FirstChildWithText(pMeshElem, "DetailMeshEntityRotations", vRotations);
							FirstChildWithText(pMeshElem, "DetailMeshEntityRadii", vRadii);
							if ((int)vPositions.size() < lNum * 3 || (int)vRotations.size() < lNum * 4)
							{
								stats.mmapSkipped["instance_data_short:" + sFile] += lNum;
								continue;
							}

							if (lNum <= 0) continue;
							OhplHpmDetailMeshes d;
							d.file = sFile.c_str();
							d.num = lNum;
							d.firstIndex = stats.mlCreated;
							d.positions = vPositions.data();
							d.rotations = vRotations.data();
							d.radii = vRadii.data();
							d.numRadii = vRadii.size();
							int lMade = mB.DetailMeshes(d);
							stats.mlCreated += lMade;
							if (lMade < lNum) stats.mmapSkipped["mesh_missing:" + sFile] += lNum - lMade;
						}
					}
				}

				hplDelete(pDoc);
				stats.mlTimeMs = (int)(mB.NowMs() - lStartTime);
			}

			void LoadExposureAreaTrack(const tWString& asBaseFile)
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

							OhplHpmExposureArea a;
							tString sName = pObjElem->GetAttributeString("Name", "");
							a.name = sName.c_str();
							Set(a.rotation, pObjElem->GetAttributeVector3f("Rotation", 0));
							Set(a.worldPos, pObjElem->GetAttributeVector3f("WorldPos", 0));
							Set(a.scale, pObjElem->GetAttributeVector3f("Scale", 1));
							a.exposure = pObjElem->GetAttributeFloat("Exposure", 0);
							a.whiteCut = pObjElem->GetAttributeFloat("WhiteCut", 3.5f);
							a.transitionTime = pObjElem->GetAttributeFloat("TransitionTime", 1);
							mB.ExposureArea(a);
						}
					}
				}

				hplDelete(pDoc);
			}

			void LoadTerrain(const tWString& asBaseFile)
			{
				iXmlDocument* pDoc = OpenSidecar(asBaseFile, _W("_Terrain"), false);
				if (pDoc == NULL) return;

				cXmlElement* pRoot = GetTrackRoot(pDoc, "HPLMapTrack_Terrain");
				cXmlElement* pTerrain = pRoot ? pRoot->GetFirstElement("Terrain") : NULL;
				if (pTerrain && pTerrain->GetAttributeBool("Active", false)) CreateTerrain(pTerrain);

				hplDelete(pDoc);
			}

			void CreateTerrain(cXmlElement* apTerrain)
			{
				std::deque<tString> dqStrings;
				OhplHpmTerrain t;
				t.heightMapSize = apTerrain->GetAttributeInt("HeightMapSize", 1024);
				t.geometryPatchSize = apTerrain->GetAttributeInt("GeometryPatchSize", 32);
				t.unitSize = apTerrain->GetAttributeFloat("UnitSize", 1);
				t.maxHeight = apTerrain->GetAttributeFloat("MaxHeight", 1);
				t.baseMaterialTileAmount = apTerrain->GetAttributeFloat("BaseMaterialTileAmount", 1);
				dqStrings.push_back(apTerrain->GetAttributeString("BaseMaterialFile"));
				t.baseMaterialFile = dqStrings.back().c_str();
				Set(t.baseMaterialDetailTextureAmount, apTerrain->GetAttributeVector3f("BaseMaterialDetailTextureAmount", 0));

				std::vector<OhplHpmTerrainLayer> vLayers;
				cXmlElement* pLayers = apTerrain->GetFirstElement("BlendLayers");
				t.hasBlendLayers = pLayers != NULL;
				if (pLayers)
				{
					cXmlNodeListIterator layerIt = pLayers->GetChildIterator();
					while (layerIt.HasNext())
					{
						cXmlElement* pLayer = layerIt.Next()->ToElement();
						OhplHpmTerrainLayer layer;
						dqStrings.push_back(pLayer->GetAttributeString("ID", "0"));
						layer.id = dqStrings.back().c_str();
						layer.numMats = 0;
						cXmlNodeListIterator matIt = pLayer->GetChildIterator();
						for (int i = 0; i < 4 && matIt.HasNext(); ++i)
						{
							cXmlElement* pMat = matIt.Next()->ToElement();
							OhplHpmTerrainMat& mat = layer.mats[layer.numMats++];
							dqStrings.push_back(pMat->GetAttributeString("File"));
							mat.file = dqStrings.back().c_str();
							mat.tileAmount = pMat->GetAttributeFloat("TileAmount", 1);
							mat.startFadeValue = pMat->GetAttributeFloat("StartFadeValue", 0);
							Set(mat.detailAmount, pMat->GetAttributeVector3f("DetailTextureAmount", 0));
						}
						vLayers.push_back(layer);
					}
				}
				t.layers = vLayers.data();
				t.numLayers = vLayers.size();

				cXmlElement* pDetails = apTerrain->GetFirstElement("DetailTextures");
				t.hasDetails = pDetails != NULL;
				t.numDetails = 0;
				t.detailFadeStart = t.detailFadeEnd = 0;
				if (pDetails)
				{
					t.detailFadeStart = pDetails->GetAttributeFloat("DetailFadeStart", 10);
					t.detailFadeEnd = pDetails->GetAttributeFloat("DetailFadeEnd", 20);
					cXmlNodeListIterator detailIt = pDetails->GetChildIterator();
					for (int i = 0; i < 3 && detailIt.HasNext(); ++i)
					{
						cXmlElement* pDetail = detailIt.Next()->ToElement();
						dqStrings.push_back(pDetail->GetAttributeString("File"));
						t.details[t.numDetails].file = dqStrings.back().c_str();
						t.details[t.numDetails].scale = pDetail->GetAttributeFloat("Scale", 1);
						++t.numDetails;
					}
				}

				cXmlElement* pSections = apTerrain->GetFirstElement("Sections");
				t.hasSections = pSections != NULL;
				t.undergrowthGridSize = apTerrain->GetAttributeFloat("UndergrowthGridSize", 10);
				t.undergrowthFadeStart = apTerrain->GetAttributeFloat("UndergrowthFadeStart", 25);
				t.undergrowthFadeEnd = apTerrain->GetAttributeFloat("UndergrowthFadeEnd", 35);
				std::vector<OhplHpmTerrainDecal> vDecals;
				std::vector<OhplHpmUndergrowthArea> vAreas;
				std::deque<std::vector<float> > dqPoints;
				if (pSections)
				{
					cXmlNodeListIterator sectionIt = pSections->GetChildIterator();
					while (sectionIt.HasNext())
					{
						cXmlElement* pSection = sectionIt.Next()->ToElement();

						tStringVec vDecalFiles;
						if (cXmlElement* pIndex = pSection->GetFirstElement("FileIndex_TerrainDecals"))
						{
							cXmlNodeListIterator fileIt = pIndex->GetChildIterator();
							while (fileIt.HasNext()) vDecalFiles.push_back(fileIt.Next()->ToElement()->GetAttributeString("Path"));
						}
						if (cXmlElement* pDecals = pSection->GetFirstElement("Decals"))
						{
							cXmlNodeListIterator decalIt = pDecals->GetChildIterator();
							while (decalIt.HasNext())
							{
								cXmlElement* pDecal = decalIt.Next()->ToElement();
								int lFile = pDecal->GetAttributeInt("FileIndex", -1);
								if (lFile < 0 || lFile >= (int)vDecalFiles.size()) continue;
								OhplHpmTerrainDecal decal;
								dqStrings.push_back(vDecalFiles[lFile]);
								decal.file = dqStrings.back().c_str();
								Set(decal.position, pDecal->GetAttributeVector2f("Position", 0));
								Set(decal.size, pDecal->GetAttributeVector2f("Size", 1));
								decal.angle = pDecal->GetAttributeFloat("Angle", 0);
								Set(decal.color, pDecal->GetAttributeColor("Color", cColor(1, 1)));
								decal.currentUvSubDiv = pDecal->GetAttributeInt("CurrentUVSubDiv", 0);
								vDecals.push_back(decal);
							}
						}

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
							OhplHpmUndergrowthArea area;
							dqStrings.push_back(vFiles[lFile]);
							area.file = dqStrings.back().c_str();
							area.sub = cString::ToLowerCase(pArea->GetAttributeString("BlendType")) == "sub";
							area.maxInfluence = pArea->GetAttributeFloat("MaxInfluence", 1);
							area.fadeBorder = pArea->GetAttributeFloat("FadeBorder", 0);
							area.circle = pArea->GetValue() == "Circle";
							Set(area.center, pArea->GetAttributeVector2f("Center", 0));
							area.radius = pArea->GetAttributeFloat("Radius", 0.5f);
							dqPoints.push_back(std::vector<float>());
							if (area.circle == false)
							{
								cXmlNodeListIterator pointIt = pArea->GetChildIterator();
								while (pointIt.HasNext())
								{
									cVector2f vPt = pointIt.Next()->ToElement()->GetAttributeVector2f("Coords", 0);
									dqPoints.back().push_back(vPt.x);
									dqPoints.back().push_back(vPt.y);
								}
							}
							area.points = dqPoints.back().data();
							area.numPoints = dqPoints.back().size() / 2;
							vAreas.push_back(area);
						}
					}
				}
				t.decals = vDecals.data();
				t.numDecals = vDecals.size();
				t.areas = vAreas.data();
				t.numAreas = vAreas.size();

				mB.Terrain(t);
			}

			tString CreateStaticObject(cXmlElement* apElement, const tStringVec& avFileIndex)
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
					Warn("SOMA hpm: static object '%s' has out-of-bounds FileIndex %d\n", sName.c_str(), lFileNameIdx);
					return "file_index_out_of_bounds";
				}

				OhplHpmStaticObject o;
				cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
				Set(o.position, vPosition);
				Set(o.scale, apElement->GetAttributeVector3f("Scale", 1));
				Set(o.rotation, apElement->GetAttributeVector3f("Rotation", 0));
				o.castsShadows = apElement->GetAttributeBool("CastShadows", true);
				o.collides = apElement->GetAttributeBool("Collides", true);
				o.id = apElement->GetAttributeInt("ID", -1);

				if (CheckTransformValidity(sName, vPosition) == false) return "bad_transform";

				o.name = sName.c_str();
				o.filename = sFileName.c_str();
				o.culledByDistance = apElement->GetAttributeBool("CulledByDistance", true);
				o.culledByFog = apElement->GetAttributeBool("CulledByFog", true);
				Set(o.colorMul, apElement->GetAttributeColor("ColorMul", cColor(1, 1)));
				o.illumBrightness = apElement->GetAttributeFloat("IllumBrightness", 1);
				Set(o.illumColor, apElement->GetAttributeColor("IllumColor", cColor(1, 1)));
				cXmlElement* pPose = apElement->GetFirstElement("Pose");
				tFloatVec vPose;
				if (pPose) cString::GetFloatVec(pPose->GetAttributeString("_Text"), vPose, NULL);
				o.hasPose = pPose != NULL;
				o.pose = vPose.data();
				o.numPose = vPose.size();

				return mB.StaticObject(o) ? "" : "mesh_missing:" + sFileName;
			}

			tString CreatePlanePrimitive(cXmlElement* apElement)
			{
				tString sType = apElement->GetValue();
				if (sType != "Plane")
				{
					Warn("SOMA hpm: skipping unsupported primitive type '%s'\n", sType.c_str());
					return "unsupported_primitive:" + sType;
				}

				tString sName = apElement->GetAttributeString("Name");
				tString sMaterial = apElement->GetAttributeString("Material");
				OhplHpmPlane p;
				p.castsShadows = apElement->GetAttributeBool("CastShadows", true);
				p.collides = apElement->GetAttributeBool("Collides", true);
				p.id = apElement->GetAttributeInt("ID", -1);

				cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
				Set(p.position, vPosition);
				Set(p.scale, apElement->GetAttributeVector3f("Scale", 1));
				Set(p.rotation, apElement->GetAttributeVector3f("Rotation", 0));

				if (CheckTransformValidity(sName, vPosition) == false) return "bad_transform";

				p.name = sName.c_str();
				p.material = sMaterial.c_str();
				Set(p.startCorner, apElement->GetAttributeVector3f("StartCorner", 0));
				Set(p.endCorner, apElement->GetAttributeVector3f("EndCorner", 0));
				Set(p.uv[0], apElement->GetAttributeVector2f("Corner1UV"));
				Set(p.uv[1], apElement->GetAttributeVector2f("Corner2UV"));
				Set(p.uv[2], apElement->GetAttributeVector2f("Corner3UV"));
				Set(p.uv[3], apElement->GetAttributeVector2f("Corner4UV"));
				p.culledByDistance = apElement->GetAttributeBool("CulledByDistance", true);
				p.culledByFog = apElement->GetAttributeBool("CulledByFog", true);

				return mB.Plane(p) ? "" : "create_failed";
			}

			tString CreateDecal(cXmlElement* apElement, const tStringVec& avFileIndex)
			{
				tString sName = apElement->GetAttributeString("Name");

				tString sMaterial;
				int lMaterialIdx = apElement->GetAttributeInt("MaterialIndex", -1);
				if (lMaterialIdx < 0) sMaterial = apElement->GetAttributeString("Material");
				else if (lMaterialIdx < (int)avFileIndex.size()) sMaterial = avFileIndex[lMaterialIdx];
				else return "file_index_out_of_bounds";

				OhplHpmDecal d;
				d.name = sName.c_str();
				d.material = sMaterial.c_str();
				Set(d.color, apElement->GetAttributeColor("Color", cColor(1, 1)));
				d.id = apElement->GetAttributeInt("ID", -1);
				return mB.Decal(d, apElement->GetFirstElement("DecalMesh")) ? "" : "decal_mesh_failed";
			}

			tString CreateMapEntity(cXmlElement* apElement, const tStringVec& avFileIndex)
			{
				tString sName = apElement->GetAttributeString("Name");
				OhplHpmEntity e;
				e.id = apElement->GetAttributeInt("ID");
				e.active = apElement->GetAttributeBool("Active", true);
				cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
				Set(e.position, vPosition);
				Set(e.scale, apElement->GetAttributeVector3f("Scale", 1));
				Set(e.rotation, apElement->GetAttributeVector3f("Rotation", 0));

				if (CheckTransformValidity(sName, vPosition) == false) return "bad_transform";

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
					Warn("SOMA hpm: entity '%s' has out-of-bounds FileIndex %d\n", sName.c_str(), lFileNameIdx);
					return "file_index_out_of_bounds";
				}

				e.name = sName.c_str();
				e.filename = sFilename.c_str();
				e.isStatic = apElement->GetAttributeBool("Static", false);
				e.culledByDistance = apElement->GetAttributeBool("CulledByDistance", true);
				e.culledByFog = apElement->GetAttributeBool("CulledByFog", true);
				return mB.Entity(e, apElement) ? "" : "entity_failed:" + sFilename;
			}

			tString CreateMapArea(cXmlElement* apElement)
			{
				tString sName = apElement->GetAttributeString("Name");
				OhplHpmArea a;
				a.id = apElement->GetAttributeInt("ID");
				a.active = apElement->GetAttributeBool("Active", true);
				cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos", 0);
				Set(a.position, vPosition);
				Set(a.scale, apElement->GetAttributeVector3f("Scale", 1));
				Set(a.rotation, apElement->GetAttributeVector3f("Rotation", 0));

				if (CheckTransformValidity(sName, vPosition) == false) return "bad_transform";

				tString sType = apElement->GetAttributeString("AreaType", "");
				a.name = sName.c_str();
				a.areaType = sType.c_str();
				return mB.Area(a, apElement) ? "" : "no_area_loader:" + sType;
			}

			iHpmBuilder& mB;
			std::map<tString, cTrackStats> mmapTrackStats;
		};
	}

	void ParseHpmMap(iHpmBuilder& aBuilder, const tWString& asFile)
	{
		cHpmParser(aBuilder).Run(asFile);
	}

};
