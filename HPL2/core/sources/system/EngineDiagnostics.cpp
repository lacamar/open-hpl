#include "system/EngineDiagnostics.h"

#include "system/String.h"

#include <set>

#include "scene/World.h"
#include "scene/Viewport.h"
#include "scene/MeshEntity.h"
#include "scene/SubMeshEntity.h"
#include "scene/Light.h"
#include "scene/BillBoard.h"
#include "scene/FogArea.h"
#include "scene/ParticleSystem.h"
#include "scene/SoundEntity.h"

#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/RenderList.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"

#include "math/BoundingVolume.h"
#include "math/Math.h"

#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"

#include <GL/glew.h>

#include <cmath>
#include <map>
#include <vector>

namespace hpl {

	int cEngineDiagnostics::mlDrawCalls = 0;
	int cEngineDiagnostics::mlLastFrameDrawCalls = 0;
	int cEngineDiagnostics::mlShaderFailCount = 0;
	unsigned int cEngineDiagnostics::mlRenderedFrames = 0;

	struct cShaderReportEntry
	{
		tString msName;
		tString msStage;
		bool mbOk;
		tString msLog;
	};
	static std::vector<cShaderReportEntry> gvShaderReport;
	static std::map<int, int> gmapGLErrors;

	//-----------------------------------------------------------------------

	tString cEngineDiagnostics::JsonEscape(const tString &asIn)
	{
		tString sOut;
		for(size_t i=0; i<asIn.size(); ++i)
		{
			char c = asIn[i];
			if(c == '"' || c == '\\') { sOut += '\\'; sOut += c; }
			else if(c == '\n') sOut += "\\n";
			else if((unsigned char)c < 0x20) sOut += ' ';
			else sOut += c;
		}
		return sOut;
	}

	static tString Num(float afX)
	{
		if(std::isnan(afX) || std::isinf(afX)) return "null";
		return cString::ToString(afX, 6, true);
	}

	static tString Vec3(const cVector3f &avX)
	{
		return "[" + Num(avX.x) + "," + Num(avX.y) + "," + Num(avX.z) + "]";
	}

	//-----------------------------------------------------------------------

	void cEngineDiagnostics::ReportShader(const tString &asName, const char *apStage, bool abOk, const tString &asInfoLog)
	{
		cShaderReportEntry entry;
		entry.msName = asName;
		entry.msStage = apStage;
		entry.mbOk = abOk;
		entry.msLog = asInfoLog;
		gvShaderReport.push_back(entry);
		if(!abOk) ++mlShaderFailCount;
	}

	tString cEngineDiagnostics::GetShaderReportJson(bool abFailedOnly)
	{
		tString sOut = "[";
		bool bFirst = true;
		for(size_t i=0; i<gvShaderReport.size(); ++i)
		{
			cShaderReportEntry &entry = gvShaderReport[i];
			if(abFailedOnly && entry.mbOk) continue;
			if(!bFirst) sOut += ",";
			bFirst = false;
			sOut += "{\"name\":\"" + JsonEscape(entry.msName) + "\",\"stage\":\"" + entry.msStage +
					"\",\"ok\":" + (entry.mbOk ? "true" : "false") + ",\"log\":\"" + JsonEscape(entry.msLog) + "\"}";
		}
		return sOut + "]";
	}

	//-----------------------------------------------------------------------

	void cEngineDiagnostics::EndFrame()
	{
		mlLastFrameDrawCalls = mlDrawCalls;
		mlDrawCalls = 0;
		++mlRenderedFrames;
	}

	tString cEngineDiagnostics::PollGLErrorsJson(bool abReset)
	{
		for(int i=0; i<64; ++i)
		{
			GLenum lErr = glGetError();
			if(lErr == GL_NO_ERROR) break;
			++gmapGLErrors[(int)lErr];
		}

		tString sOut = "{";
		for(std::map<int,int>::iterator it = gmapGLErrors.begin(); it != gmapGLErrors.end(); ++it)
		{
			if(it != gmapGLErrors.begin()) sOut += ",";
			char sHex[16];
			snprintf(sHex, sizeof(sHex), "0x%04X", it->first);
			sOut += "\"" + tString(sHex) + "\":" + cString::ToString(it->second);
		}
		sOut += "}";

		if(abReset) gmapGLErrors.clear();
		return sOut;
	}

	//-----------------------------------------------------------------------

	tString cEngineDiagnostics::GetWorldStatsJson(cWorld *apWorld)
	{
		int lStaticMesh=0, lDynamicMesh=0, lSubMeshNoMaterial=0, lOversized=0, lNanBounds=0;
		std::map<tString,int> mapNoMaterial;
		cBoundingVolume totalBV;
		bool bHasBV = false;
		cVector3f vMin(0), vMax(0);

		tStringVec vOversized;
		for(int lPass=0; lPass<2; ++lPass)
		{
			cMeshEntityIterator it = lPass==0 ? apWorld->GetStaticMeshEntityIterator() : apWorld->GetDynamicMeshEntityIterator();
			while(it.HasNext())
			{
				cMeshEntity *pEnt = it.Next();
				if(lPass==0) ++lStaticMesh; else ++lDynamicMesh;

				for(int i=0; i<pEnt->GetSubMeshEntityNum(); ++i)
				{
					if(pEnt->GetSubMeshEntity(i)->GetMaterial() != NULL) continue;
					++lSubMeshNoMaterial;
					++mapNoMaterial[(pEnt->GetMesh() ? pEnt->GetMesh()->GetName() : tString("?")) + ":" + pEnt->GetSubMeshEntity(i)->GetSubMesh()->GetMaterialName()];
				}

				cBoundingVolume *pBV = pEnt->GetBoundingVolume();
				if(pBV == NULL) continue;
				{
					cVector3f vExt = pBV->GetMax() - pBV->GetMin();
					if(vExt.x != vExt.x || vExt.y != vExt.y || vExt.z != vExt.z) { ++lNanBounds; continue; }
					// Nothing authored in these games is a 100 m mesh except sky domes.
					if(vExt.x > 100 || vExt.y > 100 || vExt.z > 100)
					{
						if(lOversized < 12) vOversized.push_back(pEnt->GetName() + ":" + (pEnt->GetMesh() ? pEnt->GetMesh()->GetName() : tString("?")));
						++lOversized;
					}
				}
				if(!bHasBV) { vMin = pBV->GetMin(); vMax = pBV->GetMax(); bHasBV = true; }
				else
				{
					cVector3f vBMin = pBV->GetMin(), vBMax = pBV->GetMax();
					if(vBMin.x < vMin.x) vMin.x = vBMin.x; if(vBMin.y < vMin.y) vMin.y = vBMin.y; if(vBMin.z < vMin.z) vMin.z = vBMin.z;
					if(vBMax.x > vMax.x) vMax.x = vBMax.x; if(vBMax.y > vMax.y) vMax.y = vBMax.y; if(vBMax.z > vMax.z) vMax.z = vBMax.z;
				}
			}
		}

		int lLights[eLightType_LastEnum] = {0};
		int lLightsTotal = 0;
		cLightListIterator lightIt = apWorld->GetLightIterator();
		while(lightIt.HasNext())
		{
			iLight *pLight = lightIt.Next();
			++lLightsTotal;
			if(pLight->GetLightType() < eLightType_LastEnum) ++lLights[pLight->GetLightType()];
		}

		int lBillboards=0, lParticleSystems=0, lFogAreas=0, lSounds=0, lBodies=0, lBodiesDynamic=0, lBodiesAwake=0;
		{ cBillboardIterator it = apWorld->GetBillboardIterator(); while(it.HasNext()) { it.Next(); ++lBillboards; } }
		{ cParticleSystemIterator it = apWorld->GetParticleSystemIterator(); while(it.HasNext()) { it.Next(); ++lParticleSystems; } }
		{ cFogAreaIterator it = apWorld->GetFogAreaIterator(); while(it.HasNext()) { it.Next(); ++lFogAreas; } }
		{ cSoundEntityIterator it = apWorld->GetSoundEntityIterator(); while(it.HasNext()) { it.Next(); ++lSounds; } }
		if(apWorld->GetPhysicsWorld())
		{
			cPhysicsBodyIterator it = apWorld->GetPhysicsWorld()->GetBodyIterator();
			while(it.HasNext())
			{
				iPhysicsBody *pBody = it.Next();
				++lBodies;
				if(pBody->GetMass() <= 0) continue;
				++lBodiesDynamic;
				if(pBody->GetEnabled()) ++lBodiesAwake;
			}
		}

		tString sOut = "{";
		sOut += "\"name\":\"" + JsonEscape(apWorld->GetName()) + "\"";
		sOut += ",\"static_mesh_entities\":" + cString::ToString(lStaticMesh);
		sOut += ",\"dynamic_mesh_entities\":" + cString::ToString(lDynamicMesh);
		sOut += ",\"submeshes_without_material\":" + cString::ToString(lSubMeshNoMaterial);
		sOut += ",\"entities_oversized\":" + cString::ToString(lOversized);
		sOut += ",\"entities_nan_bounds\":" + cString::ToString(lNanBounds);
		sOut += ",\"oversized_top\":[";
		for(size_t i=0; i<vOversized.size(); ++i) sOut += (i ? ",\"" : "\"") + JsonEscape(vOversized[i]) + "\"";
		sOut += "]";
		{
			// Worst offenders as "mesh:material name"
			std::multimap<int,tString> mapSorted;
			for(std::map<tString,int>::iterator it = mapNoMaterial.begin(); it != mapNoMaterial.end(); ++it) mapSorted.insert(std::make_pair(-it->second, it->first));
			sOut += ",\"no_material_top\":{";
			int lCount = 0;
			for(std::multimap<int,tString>::iterator it = mapSorted.begin(); it != mapSorted.end() && lCount < 12; ++it, ++lCount)
				sOut += tString(lCount ? "," : "") + "\"" + JsonEscape(it->second) + "\":" + cString::ToString(-it->first);
			sOut += "}";
		}
		sOut += ",\"lights\":" + cString::ToString(lLightsTotal);
		sOut += ",\"lights_point\":" + cString::ToString(lLights[eLightType_Point]);
		sOut += ",\"lights_spot\":" + cString::ToString(lLights[eLightType_Spot]);
		sOut += ",\"lights_box\":" + cString::ToString(lLights[eLightType_Box]);
		sOut += ",\"billboards\":" + cString::ToString(lBillboards);
		sOut += ",\"particle_systems\":" + cString::ToString(lParticleSystems);
		sOut += ",\"fog_areas\":" + cString::ToString(lFogAreas);
		sOut += ",\"sound_entities\":" + cString::ToString(lSounds);
		sOut += ",\"physics_bodies\":" + cString::ToString(lBodies);
		sOut += ",\"physics_bodies_dynamic\":" + cString::ToString(lBodiesDynamic);
		sOut += ",\"physics_bodies_awake\":" + cString::ToString(lBodiesAwake);
		sOut += ",\"aabb_min\":" + Vec3(vMin) + ",\"aabb_max\":" + Vec3(vMax);
		return sOut + "}";
	}

	//-----------------------------------------------------------------------

	tString cEngineDiagnostics::GetRenderStatsJson(cViewport *apViewport, cGraphics *apGraphics)
	{
		cRenderSettings *pSettings = apViewport->GetRenderSettings();
		cRenderList *pList = pSettings->mpRenderList;

		tString sOut = "{";
		sOut += "\"draw_calls\":" + cString::ToString(mlLastFrameDrawCalls);
		sOut += ",\"solid_objects\":" + cString::ToString(pList->GetSolidObjectNum());
		sOut += ",\"trans_objects\":" + cString::ToString(pList->GetTransObjectNum());
		sOut += ",\"lights_in_list\":" + cString::ToString(pList->GetLightNum());
		sOut += ",\"fog_areas_in_list\":" + cString::ToString(pList->GetFogAreaNum());
		sOut += ",\"lights_rendered\":" + cString::ToString(pSettings->mlNumberOfLightsRendered);
		sOut += ",\"occlusion_queries\":" + cString::ToString(pSettings->mlNumberOfOcclusionQueries);
		sOut += ",\"shader_failures\":" + cString::ToString(mlShaderFailCount);
		sOut += ",\"gl_errors\":" + PollGLErrorsJson(false);
		return sOut + "}";
	}

	//-----------------------------------------------------------------------

	tString cEngineDiagnostics::GetLightsJson(cWorld *apWorld, const cVector3f &avPos, int alMax, cRenderList *apRenderList)
	{
		std::set<iLight*> setInList;
		if(apRenderList) for(int i=0; i<apRenderList->GetLightNum(); ++i) setInList.insert(apRenderList->GetLight(i));

		std::multimap<float, iLight*> mapByDist;
		cLightListIterator it = apWorld->GetLightIterator();
		while(it.HasNext())
		{
			iLight *pLight = it.Next();
			mapByDist.insert(std::make_pair(cMath::Vector3Dist(pLight->GetWorldPosition(), avPos), pLight));
		}

		static const char* vTypeNames[] = {"point","spot","box"};
		tString sOut = "[";
		int lCount = 0;
		for(std::multimap<float, iLight*>::iterator mIt = mapByDist.begin(); mIt != mapByDist.end() && lCount < alMax; ++mIt, ++lCount)
		{
			iLight *pLight = mIt->second;
			const cColor &col = pLight->GetDiffuseColor();
			if(lCount>0) sOut += ",";
			sOut += "{\"name\":\"" + JsonEscape(pLight->GetName()) + "\",\"type\":\"" + vTypeNames[pLight->GetLightType()] + "\"";
			sOut += ",\"dist\":" + Num(mIt->first) + ",\"radius\":" + Num(pLight->GetRadius());
			sOut += ",\"color\":[" + Num(col.r) + "," + Num(col.g) + "," + Num(col.b) + "," + Num(col.a) + "]";
			sOut += ",\"visible\":" + tString(pLight->IsVisible() ? "true" : "false");
			if(apRenderList) sOut += ",\"in_list\":" + tString(setInList.count(pLight) ? "true" : "false");
			sOut += ",\"shadows\":" + tString(pLight->GetCastShadows() ? "true" : "false");
			sOut += ",\"falloff_map\":" + tString(pLight->GetFalloffMap() ? "true" : "false");
			sOut += ",\"gobo\":" + tString(pLight->GetGoboTexture() ? "true" : "false") + "}";
		}
		return sOut + "]";
	}

	//-----------------------------------------------------------------------

	tString cEngineDiagnostics::GetEntityInfoJson(cWorld *apWorld, const tString &asName)
	{
		cMeshEntity *pEnt = NULL;
		bool bStatic = false;
		for(int lPass=0; lPass<2 && pEnt==NULL; ++lPass)
		{
			cMeshEntityIterator it = lPass==0 ? apWorld->GetStaticMeshEntityIterator() : apWorld->GetDynamicMeshEntityIterator();
			while(it.HasNext())
			{
				cMeshEntity *pTest = it.Next();
				if(pTest->GetName() == asName) { pEnt = pTest; bStatic = lPass==0; break; }
			}
		}
		if(pEnt == NULL) return "";

		tString sOut = "{\"name\":\"" + JsonEscape(pEnt->GetName()) + "\"";
		sOut += ",\"static\":" + tString(bStatic ? "true" : "false");
		sOut += ",\"visible\":" + tString(pEnt->IsVisible() ? "true" : "false");
		sOut += ",\"active\":" + tString(pEnt->IsActive() ? "true" : "false");
		sOut += ",\"position\":" + Vec3(pEnt->GetWorldPosition());
		cBoundingVolume *pBV = pEnt->GetBoundingVolume();
		if(pBV) sOut += ",\"aabb_min\":" + Vec3(pBV->GetMin()) + ",\"aabb_max\":" + Vec3(pBV->GetMax());
		if(pEnt->GetMesh()) sOut += ",\"mesh\":\"" + JsonEscape(pEnt->GetMesh()->GetName()) + "\"";
		sOut += ",\"has_body\":" + tString(pEnt->GetBody() ? "true" : "false");

		sOut += ",\"submeshes\":[";
		for(int i=0; i<pEnt->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity *pSub = pEnt->GetSubMeshEntity(i);
			cMaterial *pMat = pSub->GetMaterial();
			if(i>0) sOut += ",";
			cBoundingVolume *pSubBV = pSub->GetBoundingVolume();
			sOut += "{\"name\":\"" + JsonEscape(pSub->GetName()) + "\",\"material\":" +
					(pMat ? "\"" + JsonEscape(pMat->GetName()) + "\"" : tString("null")) +
					",\"visible\":" + (pSub->IsVisible() ? "true" : "false") +
					",\"pos\":" + Vec3(pSub->GetWorldPosition()) +
					(pSubBV ? ",\"aabb_min\":" + Vec3(pSubBV->GetMin()) + ",\"aabb_max\":" + Vec3(pSubBV->GetMax()) : tString("")) + "}";
		}
		return sOut + "]}";
	}

	//-----------------------------------------------------------------------

	tString cEngineDiagnostics::GetPixelStatsJson(const float *apPixels, int alWidth, int alHeight)
	{
		static const char* vChannelNames[4] = {"r","g","b","a"};
		size_t lNumPixels = (size_t)alWidth * (size_t)alHeight;

		tString sOut = "{\"width\":" + cString::ToString(alWidth) + ",\"height\":" + cString::ToString(alHeight);
		for(int c=0; c<4; ++c)
		{
			float fMin = 1e30f, fMax = -1e30f;
			double fSum = 0;
			int lNan = 0, lZero = 0;
			for(size_t i=0; i<lNumPixels; ++i)
			{
				float fVal = apPixels[i*4 + c];
				if(std::isnan(fVal)) { ++lNan; continue; }
				if(fVal == 0.0f) ++lZero;
				if(fVal < fMin) fMin = fVal;
				if(fVal > fMax) fMax = fVal;
				fSum += fVal;
			}
			size_t lValid = lNumPixels - lNan;
			sOut += tString(",\"") + vChannelNames[c] + "\":{\"min\":" + Num(lValid ? fMin : 0) + ",\"max\":" + Num(lValid ? fMax : 0) +
					",\"mean\":" + Num(lValid ? (float)(fSum / (double)lValid) : 0) +
					",\"nan\":" + cString::ToString(lNan) + ",\"zero\":" + cString::ToString(lZero) + "}";
		}
		return sOut + "}";
	}

	//-----------------------------------------------------------------------

	tString cEngineDiagnostics::GetFrameStatsJson(const unsigned char *apPixels, int alWidth, int alHeight, int alBytesPerPixel)
	{
		size_t lNumPixels = (size_t)alWidth * (size_t)alHeight;
		int vHist[16] = {0};
		size_t lBlack=0, lWhite=0, lMagenta=0;
		double fLumSum = 0;
		int lLumMin = 255, lLumMax = 0;

		for(size_t i=0; i<lNumPixels; ++i)
		{
			const unsigned char *pPix = apPixels + i*alBytesPerPixel;
			int r = pPix[0], g = pPix[1], b = pPix[2];
			int lLum = (r*54 + g*183 + b*19) >> 8;

			fLumSum += lLum;
			if(lLum < lLumMin) lLumMin = lLum;
			if(lLum > lLumMax) lLumMax = lLum;
			++vHist[lLum >> 4];

			if(r < 4 && g < 4 && b < 4) ++lBlack;
			if(r > 250 && g > 250 && b > 250) ++lWhite;
			if(r > 200 && b > 200 && g < 60) ++lMagenta;
		}

		double fInv = lNumPixels ? 1.0 / (double)lNumPixels : 0;
		tString sOut = "{\"width\":" + cString::ToString(alWidth) + ",\"height\":" + cString::ToString(alHeight);
		sOut += ",\"lum_mean\":" + Num((float)(fLumSum * fInv));
		sOut += ",\"lum_min\":" + cString::ToString(lLumMin) + ",\"lum_max\":" + cString::ToString(lLumMax);
		sOut += ",\"black_frac\":" + Num((float)(lBlack * fInv));
		sOut += ",\"white_frac\":" + Num((float)(lWhite * fInv));
		sOut += ",\"magenta_frac\":" + Num((float)(lMagenta * fInv));
		sOut += ",\"lum_hist16\":[";
		for(int i=0; i<16; ++i)
		{
			if(i>0) sOut += ",";
			sOut += Num((float)(vHist[i] * fInv));
		}
		return sOut + "]}";
	}

}
