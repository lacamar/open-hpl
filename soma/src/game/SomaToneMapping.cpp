#include "SomaToneMapping.h"
#include "RustSomaS8b.h"
#include "SomaBase.h"
#include "SomaLuxGame.h"
#include "SomaS8Host.h"
#include "SomaScriptApi.h"

#include <cstring>

cSomaToneMapping *cSomaToneMapping::mpInstance = NULL;

namespace
{
	template <class T> T *Ptr(uint64_t a) { return (T *)a; }

	// Fields of the OHPL_S8B_DUMP line, completed by the last op of an update
	struct cDump
	{
		tString msLayers, msGrading, msTarget, msBloom;
		float mfKey, mfScale, mfCut, mfGamma, mfBlend, mfGrain;
		bool mbGrain;
	} gDump;

	tString gsOut;

	iTexture *Create(const tString &asName)
	{
		iTexture *pTex = gpSomaBase->mpEngine->GetResources()->GetTextureManager()->Create3D(asName, false);
		OhplS8bLog("tex+ %s %d", asName.c_str(), pTex != NULL);
		if (pTex)
		{
			pTex->SetWrapSTR(eTextureWrap_ClampToEdge);
			pTex->SetFilter(eTextureFilter_Bilinear);
		}
		return pTex;
	}

	tString NameOf(uint64_t a) { return a ? Ptr<iTexture>(a)->GetName() : tString("-"); }

	void Text(OhplS8bIo *io, const tString &asText)
	{
		gsOut = asText;
		io->s = (const uint8_t *)gsOut.data();
		io->n = gsOut.size();
	}

	void Call(void *, uint32_t aOp, OhplS8bIo *io)
	{
		cTextureManager *pTexMgr = gpSomaBase->mpEngine->GetResources()->GetTextureManager();
		switch (aOp)
		{
		case kS8b_GradingTex: io->r = (uint64_t)Create(tString((const char *)io->s, io->n)); break;
		case kS8b_NoiseTex:
		{
			iTexture *pNoise = pTexMgr->Create2D("core_noise2D.dds", false);
			if (pNoise) pNoise->SetWrapSTR(eTextureWrap_Repeat);
			io->r = (uint64_t)pNoise;
			break;
		}
		case kS8b_TexDestroy: pTexMgr->Destroy(Ptr<iTexture>(io->p[0])); break;
		case kS8b_TexName:
			Text(io, Ptr<iTexture>(io->p[0])->GetName());
			io->r = 1;
			break;
		case kS8b_WorldGradingName: Text(io, Ptr<cWorld>(io->p[0])->GetColorGradingTexture()); break;
		case kS8b_WorldTone:
		{
			cWorld *pWorld = Ptr<cWorld>(io->p[0]);
			io->f[0] = pWorld->GetToneMappingKey();
			io->f[1] = pWorld->GetToneMappingExposure();
			io->f[2] = pWorld->GetToneMappingWhiteCut();
			break;
		}
		case kS8b_AreaCount: io->r = Ptr<cWorld>(io->p[0])->GetExposureAreas().size(); break;
		case kS8b_AreaGet:
		{
			const cWorldExposureArea &area = Ptr<cWorld>(io->p[0])->GetExposureAreas()[io->i[0]];
			static_assert(sizeof(float) * 16 == sizeof(area.m_mtxInvTransform.m), "matrix layout");
			memcpy(io->f, area.m_mtxInvTransform.m, sizeof(float) * 16);
			memcpy(io->f + 16, area.mvHalfSize.v, sizeof(float) * 3);
			io->f[19] = area.mfExposure;
			io->f[20] = area.mfWhiteCut;
			io->f[21] = area.mfTransitionTime;
			break;
		}
		case kS8b_CurrentWorld: io->r = (uint64_t)gpSomaBase->GetCurrentWorld(); break;
		case kS8b_CurrentViewport: io->r = (uint64_t)gpSomaBase->GetCurrentViewport(); break;
		case kS8b_ToneObject: io->r = (uint64_t)cSomaToneMapping::Get(); break;
		case kS8b_CameraPos:
		{
			cCamera *pCam = gpSomaBase->GetDebugCamera();
			io->r = pCam != NULL;
			if (pCam) memcpy(io->f, pCam->GetPosition().v, sizeof(float) * 3);
			break;
		}
		case kS8b_SetTone:
			gDump.mfKey = io->f[0]; gDump.mfScale = io->f[1]; gDump.mfCut = io->f[2]; gDump.mfGamma = io->f[3];
			cRendererDeferred::SetToneMapping(io->f[0], io->f[1], io->f[2], io->f[3]);
			break;
		case kS8b_SetLayers:
		{
			std::vector<std::pair<iTexture *, float>> vLayers;
			gDump.msLayers.clear();
			for (size_t i = 0; i < io->n; ++i)
			{
				uint64_t lTex;
				float fAlpha;
				memcpy(&lTex, io->s + i * 16, 8);
				memcpy(&fAlpha, io->s + i * 16 + 8, 4);
				vLayers.push_back({Ptr<iTexture>(lTex), fAlpha});
				if (OhplS8bDump()) gDump.msLayers += Ptr<iTexture>(lTex)->GetName() + ":" + OhplS8bHex(fAlpha) + ",";
			}
			cRendererDeferred::SetColorGradingLayers(vLayers);
			break;
		}
		case kS8b_SetGrading:
			if (OhplS8bDump()) { gDump.msGrading = NameOf(io->p[0]); gDump.msTarget = NameOf(io->p[1]); }
			gDump.mfBlend = io->f[0];
			cRendererDeferred::SetColorGradingTexture(Ptr<iTexture>(io->p[0]), Ptr<iTexture>(io->p[1]), io->f[0]);
			break;
		case kS8b_UserBloom: io->r = SomaUserConfig()->GetBool("Graphics", "BloomActive", true); break;
		case kS8b_SetBloom:
			if (OhplS8bDump())
				gDump.msBloom = tString(io->i[0] ? "1 " : "0 ") + OhplS8bHex(io->f[0]) + " " + OhplS8bHex(io->f[1]) + " " + OhplS8bHex(io->f[2]) +
								"," + OhplS8bHex(io->f[3]) + "," + OhplS8bHex(io->f[4]) + "," + OhplS8bHex(io->f[5]);
			cRendererDeferred::SetBloom(io->i[0] != 0, io->f[0], io->f[1], cColor(io->f[2], io->f[3], io->f[4], io->f[5]));
			break;
		case kS8b_SetGrain:
			gDump.mbGrain = io->p[0] != 0;
			gDump.mfGrain = io->f[0];
			cRendererDeferred::SetFilmGrain(Ptr<iTexture>(io->p[0]), io->f[0]);
			break;
		case kS8b_SetSrgb:
			cRendererDeferred::SetToneMapSRGB(io->i[0] != 0);
			if (OhplS8bDump())
				OhplS8bLog("upd key=%s scale=%s cut=%s gamma=%s gt=%s gtg=%s blend=%s layers=%s bloom=%s grain=%d %s srgb=%d",
						   OhplS8bHex(gDump.mfKey).c_str(), OhplS8bHex(gDump.mfScale).c_str(), OhplS8bHex(gDump.mfCut).c_str(),
						   OhplS8bHex(gDump.mfGamma).c_str(), gDump.msGrading.c_str(), gDump.msTarget.c_str(), OhplS8bHex(gDump.mfBlend).c_str(),
						   gDump.msLayers.c_str(), gDump.msBloom.c_str(), gDump.mbGrain, OhplS8bHex(gDump.mfGrain).c_str(), io->i[0]);
			break;
		default: OhplS8cCall(aOp, io);
		}
	}

	const OhplS8bHost gHost = {NULL, &Call};

	void Install()
	{
		static bool sbDone = false;
		if (sbDone == false)
		{
			sbDone = true;
			ohpl_s8b_tone_install(&gHost);
		}
	}
}

cSomaToneMapping::cSomaToneMapping() : iUpdateable("SomaToneMapping")
{
	mpInstance = this;
	Install();
	ohpl_s8b_tone_new();
}

void cSomaToneMapping::OnMapLoaded(cWorld *apWorld) { ohpl_s8b_tone_map_loaded((size_t)apWorld); }

void cSomaToneMapping::Update(float afTimeStep)
{
	if (OhplS8bDump() && afTimeStep > 0)
		afTimeStep = 1.0f / 60.0f;
	ohpl_s8b_tone_update(afTimeStep);
}

bool cSomaToneMapping::WorldExposure(float *apExposure, float *apWhiteCut) { return ohpl_s8b_tone_world(apExposure, apWhiteCut); }

void cSomaToneMapping::FadeExposure(float afExposure, float afWhiteCut, float afTime) { ohpl_s8b_tone_fade_exposure(afExposure, afWhiteCut, afTime); }

void cSomaToneMapping::InstallHost() { Install(); }

void cSomaToneMapping::RegisterNatives(asIScriptEngine *e)
{
	Install();
	ohpl_s8b_tone_register(e);
}
