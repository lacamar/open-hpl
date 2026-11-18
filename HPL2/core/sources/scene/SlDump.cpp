// OHPL_SL_DUMP: state of lights, billboards, beams, lens flares and fog areas after a world loads, plus a
// scripted fade/flicker drive of each light. Public accessors only, so it builds against both implementations.

#include "scene/Light.h"
#include "scene/LightSpot.h"
#include "scene/LightBox.h"
#include "scene/BillBoard.h"
#include "scene/Beam.h"
#include "scene/LensFlare.h"
#include "scene/FogArea.h"
#include "scene/World.h"
#include "graphics/Texture.h"
#include "graphics/Material.h"
#include "graphics/VertexBuffer.h"
#include "math/Math.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <set>

namespace hpl {

	static uint32_t B(float afX)
	{
		uint32_t l;
		memcpy(&l, &afX, 4);
		return l;
	}

	static void Col(FILE *f, const cColor &aCol)
	{
		fprintf(f, " %08x %08x %08x %08x", B(aCol.r), B(aCol.g), B(aCol.b), B(aCol.a));
	}

	static void V3(FILE *f, const cVector3f &avVec)
	{
		fprintf(f, " %08x %08x %08x", B(avVec.x), B(avVec.y), B(avVec.z));
	}

	static void Mtx(FILE *f, const cMatrixf &aMtx)
	{
		for(int i=0; i<16; ++i) fprintf(f, " %08x", B(aMtx.v[i]));
	}

	static void LightLive(FILE *f, const char *asTag, int alStep, iLight *apLight)
	{
		fprintf(f, "ll %s %s %d", apLight->GetName().c_str(), asTag, alStep);
		Col(f, apLight->GetDiffuseColor());
		fprintf(f, " %08x %d %d %d %d", B(apLight->GetRadius()), apLight->IsFading(), apLight->GetFlickerActive(), apLight->IsVisible(), 0);
		fprintf(f, " %08x", B(apLight->GetBrightness()));
		fprintf(f, "\n");
	}

	static void DumpLight(FILE *f, iLight *apLight)
	{
		const bool bDyn = apLight->IsFading() || apLight->GetFlickerActive();
		fprintf(f, "light %s %d type %d vis %d dyn %d", apLight->GetName().c_str(), apLight->GetUniqueID(), (int)apLight->GetLightType(), bDyn ? -1 : apLight->IsVisible(), bDyn);
		if(bDyn) fprintf(f, " -"); else Col(f, apLight->GetDiffuseColor());
		Col(f, apLight->GetDefaultDiffuseColor());
		Col(f, apLight->GetSpecularColor());
		fprintf(f, " cs %d aff %u res %d", apLight->GetCastShadows(), (unsigned)apLight->GetShadowCastersAffected(), (int)apLight->GetShadowMapResolution());
		fprintf(f, " %08x %d %08x %08x %08x %08x %08x %08x %08x %08x %08x",
			B(apLight->GetShadowMapBlurAmount()), apLight->GetOcclusionCullShadowCasters(), B(apLight->GetShadowMapBiasMul()),
			B(apLight->GetShadowMapSlopeScaleBiasMul()), B(apLight->GetShadowDistance()), bDyn ? 0 : B(apLight->GetRadius()),
			B(apLight->GetFalloffPow()), B(apLight->GetSpotFalloffPow()), B(apLight->GetTranslucency()),
			bDyn ? 0 : B(apLight->GetBrightness()), B(apLight->GetDefaultBrightness()));
		fprintf(f, " mask %u %d %d %d %d", apLight->GetMaskID(), apLight->GetGoboSpecular(), apLight->GetCastDiffuseLight(),
			apLight->GetCastSpecularLight(), apLight->HasMaskBox());
		V3(f, apLight->GetMaskCenter());
		V3(f, apLight->GetMaskSize());
		fprintf(f, " %08x", B(apLight->GetSourceRadius()));
		fprintf(f, " flick %d '%s' '%s' '%s' '%s'", apLight->GetFlickerActive(), apLight->GetFlickerOnSound().c_str(),
			apLight->GetFlickerOffSound().c_str(), apLight->GetFlickerOnPS().c_str(), apLight->GetFlickerOffPS().c_str());
		fprintf(f, " %08x %08x %08x %08x %08x %d %08x %08x %08x %08x", B(apLight->GetFlickerOnMinLength()), B(apLight->GetFlickerOffMinLength()),
			B(apLight->GetFlickerOnMaxLength()), B(apLight->GetFlickerOffMaxLength()), B(apLight->GetFlickerOffRadius()), apLight->GetFlickerFade(),
			B(apLight->GetFlickerOnFadeMinLength()), B(apLight->GetFlickerOnFadeMaxLength()), B(apLight->GetFlickerOffFadeMinLength()),
			B(apLight->GetFlickerOffFadeMaxLength()));
		Col(f, apLight->GetFlickerOffColor());
		Col(f, apLight->GetFlickerOnColor());
		fprintf(f, " %08x", B(apLight->GetFlickerOnRadius()));
		Col(f, apLight->GetDestColor());
		fprintf(f, " %08x '%s' '%s'", B(apLight->GetDestRadius()),
			apLight->GetFalloffMap() ? apLight->GetFalloffMap()->GetName().c_str() : "",
			apLight->GetGoboTexture() ? apLight->GetGoboTexture()->GetName().c_str() : "");
		fprintf(f, " tracker %d", apLight->GetVisibleNodeTracker() != NULL);
		if(!bDyn)
		{
			cBoundingVolume *pBV = apLight->GetBoundingVolume();
			V3(f, pBV->GetMin());
			V3(f, pBV->GetMax());
		}
		if(apLight->GetLightType() == eLightType_Spot)
		{
			cLightSpot *pSpot = static_cast<cLightSpot*>(apLight);
			fprintf(f, " spot %08x %08x %08x %08x %08x '%s'", B(pSpot->GetFOV()), B(pSpot->GetAspect()), B(pSpot->GetNearClipPlane()),
				B(pSpot->GetTanHalfFOV()), B(pSpot->GetCosHalfFOV()),
				pSpot->GetSpotFalloffMap() ? pSpot->GetSpotFalloffMap()->GetName().c_str() : "");
			if(!bDyn) Mtx(f, pSpot->GetViewProjMatrix());
		}
		else if(apLight->GetLightType() == eLightType_Box)
		{
			cLightBox *pBox = static_cast<cLightBox*>(apLight);
			fprintf(f, " box %d %d %08x %08x %08x %d", (int)pBox->GetBlendFunc(), pBox->GetBoxLightPrio(), B(pBox->GetWeight()), B(pBox->GetBevel()),
				B(pBox->GetFalloffPow()), pBox->GetUseSphericalHarmonics());
			V3(f, pBox->GetSize());
			V3(f, pBox->GetProbeOffset());
			Col(f, pBox->GetAmbientColorSky());
			Col(f, pBox->GetAmbientColorGround());
			for(int i=0; i<9; ++i) V3(f, pBox->GetIrradianceBands()[i]);
		}
		fprintf(f, "\n");
	}

	// Fade, brightness fade and flicker on a light that is idle at load; restored afterwards.
	static void DriveLight(FILE *f, iLight *apLight)
	{
		if(apLight->IsFading() || apLight->GetFlickerActive()) return;

		const cColor vDiffuse = apLight->GetDiffuseColor();
		const float fRadius = apLight->GetRadius();
		const float fBrightness = apLight->GetBrightness();

		srand(12345);
		LightLive(f, "start", 0, apLight);
		apLight->FadeTo(cColor(0.2f, 0.4f, 0.6f, 0.1f), fRadius*0.5f + 1.0f, 1.0f);
		for(int i=1; i<=12; ++i){ apLight->UpdateLight(0.1f); LightLive(f, "fade", i, apLight); }
		apLight->FadeBrightnessTo(0.25f, 0.5f);
		for(int i=1; i<=6; ++i){ apLight->UpdateLight(0.1f); LightLive(f, "bright", i, apLight); }
		apLight->SetFlicker(cColor(0,0), 0.5f, 0.2f, 0.5f, "", "", 0.1f, 0.3f, "", "", true, 0.05f, 0.1f, 0.05f, 0.1f);
		apLight->SetFlickerActive(true);
		for(int i=1; i<=40; ++i){ apLight->UpdateLight(0.05f); LightLive(f, "flicker", i, apLight); }
		apLight->SetFlicker(cColor(0,0), 0.5f, 0.2f, 0.5f, "", "", 0.1f, 0.3f, "", "", false, 0, 0, 0, 0);
		for(int i=1; i<=20; ++i){ apLight->UpdateLight(0.05f); LightLive(f, "flicker2", i, apLight); }

		apLight->SetFlickerActive(false);
		apLight->StopFading();
		apLight->SetDiffuseColor(vDiffuse);
		apLight->SetRadius(fRadius);
		apLight->SetBrightness(fBrightness);

		apLight->ClearShadowCasterCache();
		apLight->AddShadowCaster(apLight);
		fprintf(f, "cache %s %d", apLight->GetName().c_str(), apLight->ShadowCasterIsValid(apLight));
		tRenderableVec vCasters(1, apLight);
		fprintf(f, " %d", apLight->ShadowCastersAreUnchanged(vCasters));
		apLight->SetShadowCasterCacheFromVec(vCasters);
		fprintf(f, " %d\n", apLight->ShadowCastersAreUnchanged(vCasters));
		apLight->ClearShadowCasterCache();
	}

	static void DumpWorld(FILE *f, cWorld *apWorld)
	{
		fprintf(f, "world %s\n", apWorld->GetName().c_str());

		cLightListIterator lightIt = apWorld->GetLightIterator();
		while(lightIt.HasNext()) DumpLight(f, lightIt.Next());

		cBillboardIterator bbIt = apWorld->GetBillboardIterator();
		while(bbIt.HasNext())
		{
			cBillboard *p = bbIt.Next();
			fprintf(f, "billboard %s %d type %d vis %d", p->GetName().c_str(), p->GetUniqueID(), (int)p->GetType(), p->IsVisible());
			fprintf(f, " %08x %08x", B(p->GetSize().x), B(p->GetSize().y));
			V3(f, p->GetAxis());
			Col(f, p->GetColor());
			fprintf(f, " %08x %08x %08x halo %d", B(p->GetForwardOffset()), B(p->GetBrightness()), B(p->GetHaloAlpha()), p->IsHalo());
			V3(f, p->GetHaloSourceSize());
			fprintf(f, " mat '%s' oq %d", p->GetMaterial() ? p->GetMaterial()->GetName().c_str() : "", p->UsesOcclusionQuery());
			cBoundingVolume *pBV = p->GetBoundingVolume();
			V3(f, pBV->GetMin());
			V3(f, pBV->GetMax());
			fprintf(f, "\n");
		}

		cBeamIterator beamIt = apWorld->GetBeamIterator();
		while(beamIt.HasNext())
		{
			cBeam *p = beamIt.Next();
			p->UpdateGraphicsForFrame(0);
			fprintf(f, "beam %s %d file '%s' vis %d %08x %08x", p->GetName().c_str(), p->GetUniqueID(), p->GetFileName().c_str(), p->IsVisible(),
				B(p->GetSize().x), B(p->GetSize().y));
			Col(f, p->GetColor());
			Col(f, p->GetEnd()->GetColor());
			fprintf(f, " %d %d", p->GetTileHeight(), p->GetMultiplyAlphaWithColor());
			V3(f, p->GetAxis());
			V3(f, p->GetMidPosition());
			fprintf(f, " mat '%s'", p->GetMaterial() ? p->GetMaterial()->GetName().c_str() : "");
			cBoundingVolume *pBV = p->GetBoundingVolume();
			V3(f, pBV->GetMin());
			V3(f, pBV->GetMax());
			float *pPos = p->GetVertexBuffer()->GetFloatArray(eVertexBufferElement_Position);
			for(int i=0; i<16; ++i) fprintf(f, " %08x", B(pPos[i]));
			float *pCol = p->GetVertexBuffer()->GetFloatArray(eVertexBufferElement_Color0);
			for(int i=0; i<16; ++i) fprintf(f, " %08x", B(pCol[i]));
			fprintf(f, "\n");
		}

		cFogAreaIterator fogIt = apWorld->GetFogAreaIterator();
		while(fogIt.HasNext())
		{
			cFogArea *p = fogIt.Next();
			fprintf(f, "fogarea %s %d", p->GetName().c_str(), p->GetUniqueID());
			Col(f, p->GetColor());
			V3(f, p->GetSize());
			fprintf(f, " %08x %08x %08x %08x %d %d %d %d %08x %08x", B(p->GetBrightness()), B(p->GetStart()), B(p->GetEnd()), B(p->GetFalloffExp()),
				p->GetShowBacksideWhenOutside(), p->GetShowBacksideWhenInside(), p->GetUnderwater(), p->GetSkybox(),
				B(p->GetNoiseStrength()), B(p->GetNoiseSize()));
			V3(f, p->GetNoiseTurbulence());
			fprintf(f, " %d %d %d %08x %08x %08x", p->GetLighten(), p->GetHeightBased(), p->GetExponential(), B(p->GetDensity()),
				B(p->GetHeightDensity()), B(p->GetHeightHorizon()));
			Mtx(f, *p->GetModelMatrix(NULL));
			fprintf(f, "\n");
		}

		cLensFlareIterator flareIt = apWorld->GetLensFlareIterator();
		while(flareIt.HasNext())
		{
			cLensFlare *p = flareIt.Next();
			fprintf(f, "lensflare %s %d vis %d", p->GetName().c_str(), p->GetUniqueID(), p->IsVisible());
			V3(f, p->GetFlareSourceSize());
			fprintf(f, " %08x %08x %d %d %d %d", B(p->GetOuterFieldOfView()), B(p->GetInnerFieldOfView()), p->GetMultiIrisTextureAtlasGrid().x,
				p->GetMultiIrisTextureAtlasGrid().y, p->GetMultiIrisSeed(), p->GetMultiIrisCount());
			fprintf(f, " %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x", B(p->GetRangeMaxStart()), B(p->GetRangeMaxEnd()), B(p->GetRangeMinStart()),
				B(p->GetRangeMinEnd()), B(p->GetGlareBrightness()), B(p->GetGlareFieldOfView()), B(p->GetGlareRangeMaxStart()),
				B(p->GetGlareRangeMaxEnd()), B(p->GetSizeChangeBasedOnDistance()), B(p->GetBrightness()));
			fprintf(f, " %d %d %d", p->GetUseParentMeshForOcclusion(), p->GetShrinkWhenOccluded(), p->GetMultiplyGlareWithMultiIris());
			for(int t=0; t<eLensFlareType_LastEnum; ++t)
			{
				eLensFlareType type = (eLensFlareType)t;
				fprintf(f, " t%d %d %d %08x %08x", t, p->GetFlareActive(type), p->IsFlareActive(type), B(p->GetFlareSize(type).x), B(p->GetFlareSize(type).y));
				Col(f, p->GetFlareColor(type));
			}
			fprintf(f, " first %d any %d\n", (int)p->GetFirstActiveType(), p->IsAnyTypeActive());
		}
	}

	void OhplSLDumpLight(iLight *apLight, cWorld *apWorld)
	{
		static const char *sPath = getenv("OHPL_SL_DUMP");
		static std::set<cWorld*> setSeen;
		static std::set<iLight*> setDriven;
		if(sPath == NULL || apWorld == NULL) return;

		FILE *f = fopen(sPath, "a");
		if(f == NULL) return;
		if(setSeen.insert(apWorld).second) DumpWorld(f, apWorld);
		if(setDriven.insert(apLight).second) DriveLight(f, apLight);
		fclose(f);
	}
}
