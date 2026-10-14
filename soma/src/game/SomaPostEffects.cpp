#include "SomaPostEffects.h"
#include "SomaBase.h"
#include "SomaScriptBind.h"

#include <angelscript.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <vector>

namespace
{
	struct cType
	{
		const char *mpName, *mpShader;
		int mlKey;
		float mfDefaults[8];
	};
	const cType gvTypes[] = {
		{"ChromaticAberration", "chromatic_aberration", 0, {0, kPif / 3}},
		{"RadialBlur", "radial_blur", 1, {0.06f}},
		{"ImageTrail", "image_trail", 0, {0.3f}},
		{"ImageFadeFX", "image_fade_fx", 0, {1}},
		{"VideoDistortion", "video_distortion", 0, {1, 0, 0.15f, 1}},
		{"FXAA", NULL, -1, {}},
		{"FearTentacles", NULL, -1, {}},
		{"Blur", NULL, 0, {0}},
	};
	enum { eChromatic, eRadialBlur, eImageTrail, eImageFade, eVideoDistortion };
	iGpuProgram *gvPrograms[std::size(gvTypes)] = {};
	bool gvProgramTried[std::size(gvTypes)] = {};

	std::set<cSomaPostEffect *> gsetEffects;
	cPostEffectComposite *gpViewportComposite = NULL;

	cSomaPostEffect *Effect(void *apObj)
	{
		cSomaPostEffect *pEffect = static_cast<cSomaPostEffect *>(apObj);
		return gsetEffects.count(pEffect) ? pEffect : NULL;
	}

	cColor Hue(float afDeg)
	{
		auto f = [&](float n) { float k = fmodf(n + afDeg / 60.0f, 6.0f); return 1 - std::max(0.0f, std::min({k, 4 - k, 1.0f})); };
		return cColor(f(5), f(3), f(1), 1);
	}
}

cSomaPostEffect::cSomaPostEffect(const tString &asType)
	: iPostEffect(gpSomaBase->mpEngine->GetGraphics(), gpSomaBase->mpEngine->GetResources(), NULL), msType(asType)
{
	mlType = std::find_if(std::begin(gvTypes), std::end(gvTypes), [&](const cType &t) { return asType == t.mpName; }) - gvTypes;
	std::copy(gvTypes[mlType].mfDefaults, gvTypes[mlType].mfDefaults + 8, mfParams);
	gsetEffects.insert(this);
}

cSomaPostEffect::~cSomaPostEffect() { gsetEffects.erase(this); }

void cSomaPostEffect::Set(std::initializer_list<float> alParams)
{
	std::copy(alParams.begin(), alParams.end(), mfParams);
	bool bActive = mfParams[gvTypes[mlType].mlKey] > 0;
	if (bActive && !IsActive())
		mbClear = true;
	SetActive(bActive);
}

iTexture *cSomaPostEffect::RenderEffect(iTexture *apIn, iFrameBuffer *apOut)
{
	if (mlType == eImageTrail)
	{
		// applied next frame by cRendererDeferred::RenderImageTrail, on the HDR buffer
		float fFrameTime = mpCurrentComposite->GetCurrentFrameTime();
		cRendererDeferred::SetImageTrailAlpha(mbClear ? 1 : std::max(0.05f, 1 - powf(1 - expf(mfParams[0] * -60 * 0.015f), fFrameTime * 60)));
		mbClear = false;
		return apIn;
	}
	const cType &type = gvTypes[mlType];
	if (type.mpShader && gvProgramTried[mlType] == false)
	{
		gvProgramTried[mlType] = true;
		cParserVarContainer vars;
		vars.Add("UseUv");
		gvPrograms[mlType] = mpGraphics->CreateGpuProgramFromShaders(tString("Soma") + type.mpName, "posteffect_quad_vtx.glsl",
																	 tString("posteffect_") + type.mpShader + "_frag.glsl", &vars);
	}
	iGpuProgram *pProg = gvPrograms[mlType];
	if (pProg == NULL)
		return apIn;

	cPostEffectComposite *c = mpCurrentComposite;
	auto U = [&](const char *n) { return pProg->GetVariableId(n); };
	const float *p = mfParams;
	cVector2f vTex = apIn->GetSizeFloat2D();
	float fFrameTime = c->GetCurrentFrameTime();

	c->SetFlatProjection();
	c->SetBlendMode(eMaterialBlendMode_None);
	c->SetChannelMode(eMaterialChannelMode_RGBA);
	c->SetTextureRange(NULL, 1);
	c->SetTexture(0, apIn);

	SetFinalFrameBuffer(apOut);
	c->SetProgram(pProg);
	if (mlType != eRadialBlur)
		apIn->SetFilter(eTextureFilter_Bilinear);

	if (mlType == eChromatic)
	{
		cVector2f vScale = cVector2f(mpLowLevelGraphics->GetScreenSizeFloat().y * p[0]) / vTex;
		cVector2f vDir(sinf(p[1]), -cosf(p[1])), vOffset(p[3], p[4]);
		pProg->SetVec2f(U("avOffsetA"), (vOffset + vDir) * vScale);
		pProg->SetVec2f(U("avOffsetB"), vOffset * vScale);
		pProg->SetVec2f(U("avOffsetC"), (vOffset - vDir) * vScale);
		cColor vCol[3] = {Hue(fmodf(p[2], 360)), Hue(fmodf(p[2] + 120, 360)), Hue(fmodf(p[2] + 240, 360))};
		cColor sum = vCol[0] + vCol[1] + vCol[2];
		const char *vNames[] = {"avColorA", "avColorB", "avColorC"};
		for (int i = 0; i < 3; ++i)
			pProg->SetVec4f(U(vNames[i]), vCol[i].r / sum.r, vCol[i].g / sum.g, vCol[i].b / sum.b, vCol[i].a / sum.a);
	}
	else if (mlType == eRadialBlur)
	{
		pProg->SetFloat(U("afSize"), p[0]);
		pProg->SetFloat(U("afBlurStartDist"), p[2]);
		pProg->SetFloat(U("afAlpha"), p[1]);
	}
	else if (mlType == eImageFade)
	{
		mfT += fFrameTime;
		pProg->SetVec2f(U("avScreenSize"), vTex);
		pProg->SetFloat(U("afAmount"), p[0]);
		pProg->SetFloat(U("afT"), mfT);
		for (int i = 0; i < 3; ++i)
			c->SetTexture(i + 1, mvTextures[i]);
	}
	else if (mlType == eVideoDistortion)
	{
		pProg->SetVec2f(U("avScreenSize"), vTex);
		pProg->SetFloat(U("afAmount"), p[0]);
		pProg->SetFloat(U("afSeed"), p[1]);
		pProg->SetFloat(U("afLineDensity"), p[2]);
		pProg->SetFloat(U("afOffsetMul"), p[3]);
		pProg->SetVec2f(U("avScreenOffset"), cVector2f(p[4], p[5]));
		pProg->SetVec2f(U("avScreenBendAmount"), cVector2f(p[6], p[7]));
	}

	DrawQuad(0, 1, apIn, true);
	c->SetProgram(NULL);
	c->SetBlendMode(eMaterialBlendMode_None);
	c->SetTextureRange(NULL, 0);
	return apOut->GetColorBuffer(0)->ToTexture();
}

namespace
{
	const char *gpTentacleVtx = R"(#version 120
uniform mat4 a_mtxModel;
uniform vec3 avPos;
uniform float afTentacleScale, afIntensity, afTime, afMaterialAlpha, afInstance;
uniform sampler2D aDispA, aDispB;
void main()
{
	vec2 uv = gl_MultiTexCoord0.xy;
	vec4 d = texture2DLod(aDispA, uv, 0.0);
	if (afMaterialAlpha > 0.00001) d = mix(d, texture2DLod(aDispB, uv, 0.0), afMaterialAlpha);
	vec3 p = gl_Vertex.xyz;
	p.xy += d.xy * afIntensity * 0.2 * sin(afTime + d.b * 2.0 + afInstance);
	gl_Position = vec4((a_mtxModel * vec4(p * afTentacleScale, 0.0)).xyz + avPos, 1.0);
	gl_TexCoord[0].xy = uv;
})";
	const char *gpTentacleFrag = R"(#version 120
uniform sampler2D aTexA, aTexB;
uniform float afIntensity, afAlpha, afMaterialAlpha;
void main()
{
	vec4 c = texture2D(aTexA, gl_TexCoord[0].xy);
	if (afMaterialAlpha > 0.00001) c = mix(c, texture2D(aTexB, gl_TexCoord[0].xy), afMaterialAlpha);
	gl_FragColor = mix(vec4(0.5, 0.5, 0.5, 0.0), c, c.a * afIntensity * afAlpha);
})";
	iGpuProgram *gpTentacleProg = NULL, *gpTentacleMergeProg = NULL;
	bool gbTentacleProgTried = false;

	class cSomaFearTentacles : public cSomaPostEffect
	{
	public:
		struct cAnim { tString msName; int mlIdx; float mfSpeed = 1, mfWeight = 1; };
		struct cFade { int mlMat; float mfT, mfSpeed; };
		struct cTentacle { cVector3f mvPos = 0, mvRot = 0; };

		cSomaFearTentacles() : cSomaPostEffect("FearTentacles") {}
		~cSomaFearTentacles()
		{
			ClearEntities();
			for (cMaterial *pMat : mvMaterials)
				if (pMat) mpResources->GetMaterialManager()->Destroy(pMat);
			DestroyTarget();
		}

		void SetupEntity(const tString &asFile, int alNum, int alGroups)
		{
			ClearEntities();
			iXmlDocument *pDoc = mpResources->LoadXmlDocument(asFile);
			cXmlElement *pModel = pDoc ? pDoc->GetFirstElement("ModelData") : NULL;
			cXmlElement *pMeshElem = pModel ? pModel->GetFirstElement("Mesh") : NULL;
			cXmlElement *pAnims = pModel ? pModel->GetFirstElement("Animations") : NULL;
			alGroups = std::max(1, std::min(alGroups, alNum));
			for (int i = 0; pMeshElem && i < alGroups; ++i)
			{
				cMesh *pMesh = mpResources->GetMeshManager()->CreateMesh(pMeshElem->GetAttributeString("Filename", ""));
				if (pMesh == NULL)
					break;
				cMeshEntity *pEnt = hplNew(cMeshEntity, ("FearTentacle", pMesh, mpResources->GetMaterialManager(), mpResources->GetMeshManager(),
														 mpResources->GetAnimationManager()));
				if (pAnims)
				{
					cXmlNodeListIterator it = pAnims->GetChildIterator();
					while (it.HasNext())
					{
						cXmlElement *pAnimElem = it.Next()->ToElement();
						if (cAnimation *pAnim = mpResources->GetAnimationManager()->CreateAnimation(pAnimElem->GetAttributeString("File", "")))
							pEnt->AddAnimation(pAnim, pAnimElem->GetAttributeString("Name", ""), pAnimElem->GetAttributeFloat("Speed", 1));
					}
				}
				pEnt->Play(0, true, true);
				mvGroups.push_back(pEnt);
			}
			if (pDoc)
				mpResources->DestroyXmlDocument(pDoc);
			if (mvGroups.empty() == false)
				mvTentacles.resize(std::max(0, alNum));
		}

		void SetupMaterial(int alIdx, const tString &asFile)
		{
			if (alIdx < 0 || alIdx > 64)
				return;
			if (alIdx >= (int)mvMaterials.size())
				mvMaterials.resize(alIdx + 1);
			if (mvMaterials[alIdx])
				mpResources->GetMaterialManager()->Destroy(mvMaterials[alIdx]);
			mvMaterials[alIdx] = mpResources->GetMaterialManager()->CreateMaterial(asFile);
		}

		void SetupAnimation(const tString &asName, float afMin, float afMax)
		{
			if (mvGroups.empty() || Anim(asName))
				return;
			int lIdx = mvGroups[0]->GetAnimationStateIndex(asName);
			if (lIdx < 0)
				return;
			mvAnims.push_back({asName, lIdx});
			float fStep = (afMax - afMin) / mvGroups.size(), fSpeed = afMin + cMath::RandRectf(0, fStep);
			for (cMeshEntity *pEnt : mvGroups)
			{
				pEnt->Play(lIdx, true, false);
				fSpeed += fStep;
				pEnt->GetAnimationState(lIdx)->SetBaseSpeed(fSpeed);
			}
		}

		void SetMaterial(int alIdx, float afTime)
		{
			if (mvTentacles.empty() || alIdx < 0 || alIdx >= (int)mvMaterials.size())
				return;
			if (afTime <= 0)
			{
				mlMat = alIdx;
				mvFades.clear();
			}
			else if (alIdx == mlMat)
				for (cFade &fade : mvFades) fade.mfSpeed = -1 / afTime;
			else if (mvFades.empty() == false && mvFades.back().mlMat == alIdx)
				mvFades.back().mfSpeed = 1 / afTime;
			else
				mvFades.push_back({alIdx, 0, 1 / afTime});
		}

		cAnim *Anim(const tString &asName)
		{
			for (cAnim &anim : mvAnims)
				if (anim.msName == asName) return &anim;
			return NULL;
		}

		std::vector<cMeshEntity *> mvGroups;
		std::vector<cTentacle> mvTentacles;

	private:
		void ClearEntities()
		{
			for (cMeshEntity *pEnt : mvGroups) hplDelete(pEnt);
			mvGroups.clear();
			mvTentacles.clear();
			mvAnims.clear();
			mvFades.clear();
		}

		void DestroyTarget()
		{
			if (mpBuffer) mpGraphics->DestroyFrameBuffer(mpBuffer);
			if (mpTarget) mpGraphics->DestroyTexture(mpTarget);
			mpBuffer = NULL;
			mpTarget = NULL;
		}

		bool LoadPrograms()
		{
			if (gbTentacleProgTried == false)
			{
				gbTentacleProgTried = true;
				iGpuShader *pVtx = mpLowLevelGraphics->CreateGpuShader("SomaFearTentacles_vtx", eGpuShaderType_Vertex);
				iGpuShader *pFrag = mpLowLevelGraphics->CreateGpuShader("SomaFearTentacles_frag", eGpuShaderType_Fragment);
				if (pVtx->CreateFromString(gpTentacleVtx) && pFrag->CreateFromString(gpTentacleFrag))
				{
					pFrag->AddSamplerUnit("aTexA", 0);
					pFrag->AddSamplerUnit("aTexB", 1);
					pVtx->AddSamplerUnit("aDispA", 2);
					pVtx->AddSamplerUnit("aDispB", 3);
					iGpuProgram *pProg = mpGraphics->CreateGpuProgram("SomaFearTentacles");
					pProg->SetShader(eGpuShaderType_Vertex, pVtx);
					pProg->SetShader(eGpuShaderType_Fragment, pFrag);
					if (pProg->Link())
						gpTentacleProg = pProg;
				}
				cParserVarContainer vars;
				vars.Add("UseUv");
				gpTentacleMergeProg = mpGraphics->CreateGpuProgramFromShaders("SomaFearTentaclesMerge", "posteffect_quad_vtx.glsl",
																			  "posteffect_fear_tentacles_merge_frag.glsl", &vars);
			}
			return gpTentacleProg && gpTentacleMergeProg;
		}

		iTexture *RenderEffect(iTexture *apIn, iFrameBuffer *apOut) override
		{
			cMaterial *pMatA = mlMat < (int)mvMaterials.size() ? mvMaterials[mlMat] : NULL;
			if (mvTentacles.empty() || mvAnims.empty() || pMatA == NULL || LoadPrograms() == false)
				return apIn;

			cPostEffectComposite *c = mpCurrentComposite;
			float fDt = c->GetCurrentFrameTime();
			mfTime += fDt * mvAnims[0].mfSpeed;
			if (mfTime > 10000)
				mfTime = 0;

			for (auto it = mvFades.begin(); it != mvFades.end();)
			{
				it->mfT += fDt * it->mfSpeed;
				if (it->mfSpeed > 0 && it->mfT >= 1)
				{
					mlMat = it->mlMat;
					it = mvFades.erase(it);
				}
				else if (it->mfSpeed < 0 && it->mfT <= 0)
					it = mvFades.erase(it);
				else
					++it;
			}
			pMatA = mvMaterials[mlMat];
			cMaterial *pMatB = mvFades.empty() ? NULL : mvMaterials[mvFades.front().mlMat];
			if (pMatA == NULL)
				return apIn;
			if (pMatB == NULL)
				pMatB = pMatA;
			float fMatAlpha = mvFades.empty() ? 0 : mvFades.front().mfT;

			for (cMeshEntity *pEnt : mvGroups)
			{
				for (const cAnim &anim : mvAnims)
				{
					cAnimationState *pState = pEnt->GetAnimationState(anim.mlIdx);
					pState->SetSpeed(anim.mfSpeed);
					pState->SetWeight(anim.mfWeight);
				}
				pEnt->UpdateLogic(fDt);
				pEnt->GetSubMeshEntity(0)->UpdateGraphicsForFrame(fDt);
			}

			cVector2l vSize(apIn->GetWidth(), apIn->GetHeight());
			if (vSize.x > 1920)
				vSize = cVector2l(1920, vSize.y * 1920 / vSize.x);
			if (mpTarget == NULL || mpTarget->GetWidth() != vSize.x || mpTarget->GetHeight() != vSize.y)
			{
				DestroyTarget();
				mpTarget = mpGraphics->CreateTexture("SomaFearTentacles", eTextureType_2D, eTextureUsage_RenderTarget);
				mpTarget->CreateFromRawData(cVector3l(vSize.x, vSize.y, 0), ePixelFormat_RGBA, NULL);
				mpTarget->SetWrapSTR(eTextureWrap_ClampToEdge);
				mpTarget->SetFilter(eTextureFilter_Bilinear);
				mpBuffer = mpGraphics->CreateFrameBuffer("SomaFearTentacles");
				mpBuffer->SetTexture2D(0, mpTarget);
				mpBuffer->CompileAndValidate();
			}

			const float *p = mfParams;
			iGpuProgram *pProg = gpTentacleProg;
			auto U = [&](const char *n) { return pProg->GetVariableId(n); };
			c->SetFrameBuffer(mpBuffer, false, false);
			mpLowLevelGraphics->SetClearColor(cColor(1, 1));
			c->ClearFrameBuffer(eClearFrameBufferFlag_Color, false);
			mpLowLevelGraphics->SetClearColor(cColor(0, 0));
			c->SetDepthTest(false);
			c->SetDepthWrite(false);
			c->SetCullActive(false);
			c->SetBlendMode(eMaterialBlendMode_MulX2);
			c->SetChannelMode(eMaterialChannelMode_RGBA);
			c->SetTexture(0, pMatA->GetTexture(eMaterialTexture_Diffuse));
			c->SetTexture(1, pMatB->GetTexture(eMaterialTexture_Diffuse));
			c->SetTexture(2, pMatA->GetTexture(eMaterialTexture_NMap));
			c->SetTexture(3, pMatB->GetTexture(eMaterialTexture_NMap));
			c->SetTextureRange(NULL, 4);
			c->SetMatrix(NULL);
			c->SetProgram(pProg);
			pProg->SetFloat(U("afIntensity"), p[0]);
			pProg->SetFloat(U("afTentacleScale"), p[1]);
			pProg->SetFloat(U("afAlpha"), p[5]);
			pProg->SetFloat(U("afTime"), mfTime);
			pProg->SetFloat(U("afMaterialAlpha"), fMatAlpha);
			int lModel = U("a_mtxModel"), lPos = U("avPos"), lInstance = U("afInstance");
			cMatrixf mtxAspect = cMath::MatrixScale(cVector3f((float)vSize.y / vSize.x, 1, 1));
			for (size_t i = 0; i < mvTentacles.size(); ++i)
			{
				// ponytail: displaced after CPU skinning, tentacles of a group share a pose; GPU dual-quat instancing if it shows
				pProg->SetMatrixf(lModel, cMath::MatrixMul(mtxAspect, cMath::MatrixRotate(mvTentacles[i].mvRot, eEulerRotationOrder_YZX)));
				pProg->SetVec3f(lPos, mvTentacles[i].mvPos * (1 - p[2]));
				pProg->SetFloat(lInstance, (float)i);
				c->SetVertexBuffer(mvGroups[i % mvGroups.size()]->GetSubMeshEntity(0)->GetVertexBuffer());
				c->DrawCurrent();
			}
			c->SetVertexBuffer(NULL);
			c->SetCullActive(true);

			pProg = gpTentacleMergeProg;
			c->SetFlatProjection();
			c->SetBlendMode(eMaterialBlendMode_None);
			c->SetTextureRange(NULL, 2);
			c->SetTexture(0, apIn);
			c->SetTexture(1, mpTarget);
			SetFinalFrameBuffer(apOut);
			c->SetProgram(pProg);
			pProg->SetFloat(U("afIntensity"), p[4]);
			pProg->SetFloat(U("afAlpha"), p[5]);
			pProg->SetVec2f(U("avTentacleUvMul"), cVector2f(1, 1));
			pProg->SetFloat(U("afAspect"), (float)vSize.x / vSize.y);
			pProg->SetFloat(U("afCircleRadius"), p[6]);
			pProg->SetFloat(U("afCircleAlpha"), p[7]);
			pProg->SetFloat(U("afCircleSoftness"), p[8]);
			DrawQuad(0, 1, apIn, true);
			c->SetProgram(NULL);
			c->SetTextureRange(NULL, 0);
			return apOut->GetColorBuffer(0)->ToTexture();
		}

		std::vector<cAnim> mvAnims;
		std::vector<cMaterial *> mvMaterials;
		std::vector<cFade> mvFades;
		int mlMat = 0;
		float mfTime = 0;
		iTexture *mpTarget = NULL;
		iFrameBuffer *mpBuffer = NULL;
	};

	iGpuProgram *gpBlurProgs[4] = {};
	bool gbBlurProgTried = false;
	enum { eBlurProg, eBlurPlain, eBlurCombineMip, eBlurBlurCombine };

	class cSomaBlur : public cSomaPostEffect
	{
	public:
		cSomaBlur() : cSomaPostEffect("Blur") {}
		~cSomaBlur() { DestroyLevels(); }

	private:
		struct cLevel
		{
			int w, h, lDiv;
			float fD, fT, vWeights[7];
			cVector2f vOffH[7], vOffV[7];
			iTexture *pTexA = NULL, *pTexB = NULL;
			iFrameBuffer *pFbA = NULL, *pFbB = NULL;
		};

		bool LoadPrograms()
		{
			if (gbBlurProgTried == false)
			{
				gbBlurProgTried = true;
				const char *vNames[4] = {"SomaBlur", "SomaBlurPlain", "SomaBlurCombineMip", "SomaBlurCombine"};
				for (int i = 0; i < 4; ++i)
				{
					cParserVarContainer vars;
					vars.Add("UseUv");
					if (i == eBlurProg || i == eBlurBlurCombine)
					{
						vars.Add("Blur");
						vars.Add("kTotalBlurSamples", 7);
					}
					if (i >= eBlurCombineMip)
						vars.Add("CombineMip");
					gpBlurProgs[i] = mpGraphics->CreateGpuProgramFromShaders(vNames[i], "posteffect_quad_vtx.glsl", "posteffect_blur_frag.glsl", &vars);
				}
			}
			return std::all_of(std::begin(gpBlurProgs), std::end(gpBlurProgs), [](iGpuProgram *p) { return p != NULL; });
		}

		void Target(iTexture *&apTex, iFrameBuffer *&apFb, int w, int h)
		{
			apTex = mpGraphics->CreateTexture("SomaBlur", eTextureType_2D, eTextureUsage_RenderTarget);
			apTex->CreateFromRawData(cVector3l(w, h, 0), ePixelFormat_RGBA, NULL);
			apTex->SetWrapSTR(eTextureWrap_ClampToEdge);
			apTex->SetFilter(eTextureFilter_Bilinear);
			apFb = mpGraphics->CreateFrameBuffer("SomaBlur");
			apFb->SetTexture2D(0, apTex);
			apFb->CompileAndValidate();
		}

		void DestroyLevels()
		{
			for (size_t i = 0; i < mvLevels.size(); ++i)
			{
				cLevel &l = mvLevels[i];
				for (iFrameBuffer *pFb : {l.pFbA, l.pFbB})
					if (pFb) mpGraphics->DestroyFrameBuffer(pFb);
				if (l.pTexB) mpGraphics->DestroyTexture(l.pTexB);
				if (l.pTexA && i > 0) mpGraphics->DestroyTexture(l.pTexA);
			}
			mvLevels.clear();
		}

		void CreateLevels(const cVector2l &avScreen)
		{
			DestroyLevels();
			cLevel l0 = {avScreen.x, avScreen.y, 1};
			Target(l0.pTexB, l0.pFbB, l0.w, l0.h);
			mvLevels.push_back(l0);
			for (cLevel l = l0; l.w > 8;)
			{
				l.lDiv <<= 1;
				l.w = (l.w + 1) / 2;
				l.h = (l.h + 1) / 2;
				Target(l.pTexA, l.pFbA, l.w, l.h);
				Target(l.pTexB, l.pFbB, l.w, l.h);
				mvLevels.push_back(l);
			}
		}

		static void Weights(cLevel &l)
		{
			float fSum = 0;
			l.vWeights[0] = 1;
			l.vOffH[0] = l.vOffV[0] = cVector2f(0);
			for (int i = 1; i < 7; ++i)
			{
				float x = 2.0f * i - 1, w0 = expf(-x * x / l.fD), w1 = expf(-(x + 1) * (x + 1) / l.fD), w = w0 + w1;
				float fOff = w > 0 ? x + w1 / w : x;
				l.vWeights[i] = w;
				fSum += w;
				l.vOffH[i] = cVector2f(fOff / l.w, 0);
				l.vOffV[i] = cVector2f(0, fOff / l.h);
			}
			for (float &w : l.vWeights) w /= 1 + 2 * fSum;
		}

		iGpuProgram *Program(int alProg, const cLevel *apUniforms, bool abVertical)
		{
			iGpuProgram *p = gpBlurProgs[alProg];
			mpCurrentComposite->SetProgram(p);
			if (apUniforms)
			{
				p->SetFloatArray(p->GetVariableId("avWeights"), apUniforms->vWeights, 7);
				for (int i = 0; i < 7; ++i)
					p->SetVec2f(p->GetVariableId("avOffsets[" + std::to_string(i) + "]"), abVertical ? apUniforms->vOffV[i] : apUniforms->vOffH[i]);
			}
			return p;
		}

		void Pass(iFrameBuffer *apFb, iTexture *apTex)
		{
			SetFrameBuffer(apFb);
			mpCurrentComposite->SetTexture(0, apTex);
			DrawQuad(0, 1, apTex, false);
		}

		void Blur(cLevel *apL, bool abVertical)
		{
			if (apL == NULL || (abVertical && apL->pFbA == NULL))
				return;
			Program(eBlurProg, apL, abVertical);
			Pass(abVertical ? apL->pFbA : apL->pFbB, abVertical ? apL->pTexB : apL->pTexA);
		}

		// official cPostEffect_Blur::RenderEffect 0x10c72a0
		iTexture *RenderEffect(iTexture *apIn, iFrameBuffer *apOut) override
		{
			if (mfParams[0] <= 0 || LoadPrograms() == false)
				return apIn;
			cVector2l vScreen = mpLowLevelGraphics->GetScreenSizeInt();
			if (mvLevels.empty() || mvLevels[0].w != vScreen.x || mvLevels[0].h != vScreen.y)
				CreateLevels(vScreen);

			cPostEffectComposite *c = mpCurrentComposite;
			mvLevels[0].pTexA = apIn;
			c->SetFlatProjection();
			c->SetBlendMode(eMaterialBlendMode_None);
			c->SetChannelMode(eMaterialChannelMode_RGBA);
			c->SetTextureRange(NULL, 1);

			float fSize = vScreen.y * mfParams[0] * 0.25f;
			cLevel *pCur = NULL, *pPrev = NULL;
			for (cLevel &l : mvLevels)
			{
				l.fD = fSize / l.lDiv;
				l.fT = cMath::Clamp(l.fD / 14, 0, 1) * 2 - 1;
				if (pPrev)
				{
					Program(eBlurPlain, NULL, false);
					Pass(l.pFbA, pPrev->pTexA);
				}
				if (l.fT < 1)
				{
					pCur = &l;
					break;
				}
				pPrev = &l;
			}
			iTexture *pResult = apIn;
			if (pCur)
			{
				const float fCutoff = 0.125f; // cPostEffectParams_Blur::mfWeightCutoffThreshold
				pCur->fT = (pCur->fT - fCutoff) / (1 - fCutoff);
				cLevel *pSecond = pPrev;
				if (pPrev && pCur->fT <= 0)
				{
					pCur = pPrev;
					pSecond = NULL;
				}
				if (pCur->fT == 1)
					pSecond = NULL;
				Weights(*pCur);
				if (pSecond)
					Weights(*pSecond);
				Blur(pCur, false);
				Blur(pSecond, false);
				Blur(pCur, true);
				Blur(pSecond, true);

				iTexture *pTex;
				if (pSecond)
				{
					// level 0 combine uses cur's offsets and weights, as official
					iGpuProgram *p = pSecond == &mvLevels[0] ? Program(eBlurBlurCombine, pCur, true) : Program(eBlurCombineMip, NULL, false);
					p->SetFloat(p->GetVariableId("afAlpha"), 1 - pCur->fT);
					pTex = pSecond == &mvLevels[0] ? pSecond->pTexB : pSecond->pTexA;
					c->SetTextureRange(NULL, 2);
					c->SetTexture(1, pCur->pTexA);
				}
				else if (pCur == &mvLevels[0])
				{
					Program(eBlurProg, pCur, true);
					pTex = pCur->pTexB;
				}
				else
				{
					Program(eBlurPlain, NULL, false);
					pTex = pCur->pTexA;
				}
				c->SetTexture(0, pTex);
				SetFinalFrameBuffer(apOut);
				DrawQuad(0, 1, pTex, true);
				pResult = apOut->GetColorBuffer(0)->ToTexture();
			}
			c->SetProgram(NULL);
			c->SetTextureRange(NULL, 0);
			mvLevels[0].pTexA = NULL;
			return pResult;
		}

		std::vector<cLevel> mvLevels;
	};
}

cPostEffectComposite *cSomaPostEffects::GetViewportComposite()
{
	if (gpViewportComposite == NULL)
		gpViewportComposite = gpSomaBase->mpEngine->GetGraphics()->CreatePostEffectComposite();
	return gpViewportComposite;
}

void cSomaPostEffects::RegisterNatives(asIScriptEngine *e)
{
	typedef cPostEffectComposite C;
	e->RegisterObjectMethod("cViewport", "cPostEffectComposite@ GetPostEffectComposite()",
							asFUNCTION(+[](asIScriptGeneric *g) { g->SetReturnAddress(GetViewportComposite()); }), asCALL_GENERIC);
	SOMA_METHOD(e, "cPostEffectComposite", "void AddPostEffect(iPostEffect@ apPostEffect, int alPrio)", +[](C *c, void *p, int l) {
		if (cSomaPostEffect *pEffect = Effect(p))
		{
			c->RemovePostEffect(pEffect);
			c->AddPostEffect(pEffect, -l);
		}
	});
	SOMA_METHOD(e, "cPostEffectComposite", "void RemovePostEffect(iPostEffect@ apPostEffect)",
				+[](C *c, void *p) { if (cSomaPostEffect *pEffect = Effect(p)) c->RemovePostEffect(pEffect); });
	SOMA_METHOD(e, "cPostEffectComposite", "iPostEffect@ GetPostEffect(int alIdx)", +[](C *c, int i) -> void * {
		return i >= 0 && i < c->GetPostEffectNum() ? c->GetPostEffect(i) : NULL;
	});
	SOMA_METHOD(e, "cPostEffectComposite", "iPostEffect@ GetPostEffectFromType(const tString&in asType)", +[](C *c, const tString &s) -> void * {
		for (int i = 0; i < c->GetPostEffectNum(); ++i)
			if (cSomaPostEffect *p = Effect(c->GetPostEffect(i)); p && p->msType == s)
				return p;
		return NULL;
	});
	SOMA_FUNC(e, "void cGraphics_DestroyPostEffect(iPostEffect@ apPostEffect)", +[](void *p) {
		if (cSomaPostEffect *pEffect = Effect(p))
		{
			GetViewportComposite()->RemovePostEffect(pEffect);
			delete pEffect;
		}
	});

	const char *vTypes[] = {"iPostEffect", "cPostEffect_ChromaticAberration", "cPostEffect_FXAA", "cPostEffect_ImageFadeFX",
							"cPostEffect_ImageTrail", "cPostEffect_RadialBlur", "cPostEffect_VideoDistortion", "cPostEffect_FearTentacles", "cPostEffect_Blur"};
	for (const char *pType : vTypes)
	{
		if (e->GetTypeInfoByName(pType) == NULL)
			continue;
		SOMA_METHOD(e, pType, "void SetDisabled(bool abX)", +[](void *p, bool b) { if (cSomaPostEffect *x = Effect(p)) x->SetDisabled(b); });
		SOMA_METHOD(e, pType, "bool IsDisabled()", +[](void *p) { cSomaPostEffect *x = Effect(p); return x && x->IsDisabled(); });
		SOMA_METHOD(e, pType, "void SetActive(bool abX)", +[](void *p, bool b) { if (cSomaPostEffect *x = Effect(p)) x->SetActive(b); });
		SOMA_METHOD(e, pType, "bool IsActive()", +[](void *p) { cSomaPostEffect *x = Effect(p); return x && x->IsActive(); });
		if (strcmp(pType, "iPostEffect") != 0)
			SOMA_METHOD(e, pType, "void Reset()", +[](void *p) { if (cSomaPostEffect *x = Effect(p)) x->Reset(); });
	}

	typedef cSomaPostEffect P;
	SOMA_FUNC(e, "cPostEffect_ChromaticAberration@ cGraphics_CreatePostEffect_ChromaticAberration()", +[]() { return new P("ChromaticAberration"); });
	SOMA_FUNC(e, "cPostEffect_FXAA@ cGraphics_CreatePostEffect_FXAA()", +[]() { return new P("FXAA"); });
	SOMA_FUNC(e, "cPostEffect_ImageFadeFX@ cGraphics_CreatePostEffect_ImageFadeFX()", +[]() { return new P("ImageFadeFX"); });
	SOMA_FUNC(e, "cPostEffect_ImageTrail@ cGraphics_CreatePostEffect_ImageTrail()", +[]() { return new P("ImageTrail"); });
	SOMA_FUNC(e, "cPostEffect_RadialBlur@ cGraphics_CreatePostEffect_RadialBlur()", +[]() { return new P("RadialBlur"); });

	SOMA_METHOD(e, "cPostEffect_ChromaticAberration", "void SetParams(float afAmount, float afRotation, float afHue, const cVector2f&in avOffset)",
				+[](P *p, float a, float r, float h, const cVector2f &o) { p->Set({a, r, h, o.x, o.y}); });
	SOMA_METHOD(e, "cPostEffect_ChromaticAberration", "void GetParams(float &out afAmount, float &out afRotation, float &out afHue, cVector2f &out avOffset)",
				+[](P *p, float &a, float &r, float &h, cVector2f &o) { a = p->mfParams[0]; r = p->mfParams[1]; h = p->mfParams[2]; o = cVector2f(p->mfParams[3], p->mfParams[4]); });
	SOMA_METHOD(e, "cPostEffect_ImageFadeFX", "void SetParams(float afAmount, iTexture@ apFadeTexture, iTexture@ apColorTexture, iTexture@ apOffsetTexture)",
				+[](P *p, float a, iTexture *f, iTexture *c, iTexture *o) { p->Set({a}); p->mvTextures = {f, c, o}; });
	SOMA_METHOD(e, "cPostEffect_ImageFadeFX", "void GetParams(float &out afAmount)", +[](P *p, float &a) { a = p->mfParams[0]; });
	SOMA_METHOD(e, "cPostEffect_ImageTrail", "void SetParams(float afAmount)", +[](P *p, float a) { p->Set({a}); });
	SOMA_METHOD(e, "cPostEffect_ImageTrail", "void GetParams(float &out afAmount)", +[](P *p, float &a) { a = p->mfParams[0]; });
	SOMA_METHOD(e, "cPostEffect_RadialBlur", "void SetParams(float afSize, float afAlpha, float afBlurStartDist)",
				+[](P *p, float s, float a, float d) { p->Set({s, a, d}); });
	SOMA_METHOD(e, "cPostEffect_RadialBlur", "void GetParams(float &out afSize, float &out afAlpha, float &out afBlurStartDist)",
				+[](P *p, float &s, float &a, float &d) { s = p->mfParams[0]; a = p->mfParams[1]; d = p->mfParams[2]; });
	if (e->GetTypeInfoByName("cPostEffect_FearTentacles"))
	{
		typedef cSomaFearTentacles T;
		const char *pT = "cPostEffect_FearTentacles";
		SOMA_FUNC(e, "cPostEffect_FearTentacles@ cGraphics_CreatePostEffect_FearTentacles()", +[]() -> P * { return new T(); });
		SOMA_METHOD(e, pT, "void SetParams(float afIntensity, float afTentacleScale, float afSpreadDist, float afRefraction, float afVingette, float afAlpha, float afCircleRadius, float afCircleAlpha, float afCircleSoftness)",
					+[](P *p, float a, float b, float c, float d, float f, float g, float h, float i, float j) {
						const float v[9] = {a, b, c, d, f, g, h, i, j};
						std::copy(v, v + 9, p->mfParams);
					});
		SOMA_METHOD(e, pT, "void GetParams(float&out afIntensity, float&out afTentacleScale, float&out afSpreadDist, float&out afRefraction, float&out afVingette, float&out afAlpha, float&out afCircleRadius, float&out afCircleAlpha, float&out afCircleSoftness)",
					+[](P *p, float &a, float &b, float &c, float &d, float &f, float &g, float &h, float &i, float &j) {
						float *v[9] = {&a, &b, &c, &d, &f, &g, &h, &i, &j};
						for (int n = 0; n < 9; ++n) *v[n] = p->mfParams[n];
					});
		SOMA_METHOD(e, pT, "void SetupTentacleEntity(const tString&in asEntFile, int alNum, int alAnimGroupNum)",
					+[](P *p, const tString &s, int n, int g) { static_cast<T *>(p)->SetupEntity(s, n, g); });
		SOMA_METHOD(e, pT, "void SetupTentacleMaterial(int alMatIndex, const tString&in asMat)",
					+[](P *p, int i, const tString &s) { static_cast<T *>(p)->SetupMaterial(i, s); });
		SOMA_METHOD(e, pT, "int GetTentacleEntityNum()", +[](P *p) { return (int)static_cast<T *>(p)->mvTentacles.size(); });
		SOMA_METHOD(e, pT, "void SetTentacleTransform(int alIndex, const cVector3f&in avPos, const cVector3f&in avRot)",
					+[](P *p, int i, const cVector3f &vPos, const cVector3f &vRot) {
						T *t = static_cast<T *>(p);
						if (i >= 0 && i < (int)t->mvTentacles.size()) t->mvTentacles[i] = {vPos, vRot};
					});
		SOMA_METHOD(e, pT, "void SetupTentacleAnimation(const tString&in asAnim, float afSpeedMin, float afSpeedMax)",
					+[](P *p, const tString &s, float a, float b) { static_cast<T *>(p)->SetupAnimation(s, a, b); });
		SOMA_METHOD(e, pT, "void SetTentacleAnimationSpeed(const tString&in asAnim, float afSpeed)",
					+[](P *p, const tString &s, float f) { if (T::cAnim *a = static_cast<T *>(p)->Anim(s)) a->mfSpeed = f; });
		SOMA_METHOD(e, pT, "void SetTentacleAnimationWeight(const tString&in asAnim, float afWeight)",
					+[](P *p, const tString &s, float f) { if (T::cAnim *a = static_cast<T *>(p)->Anim(s)) a->mfWeight = f; });
		SOMA_METHOD(e, pT, "void SetTentacleMaterial(int alMatIndex, float afFadeTime)",
					+[](P *p, int i, float f) { static_cast<T *>(p)->SetMaterial(i, f); });
		SOMA_METHOD(e, pT, "cMeshEntity@ GetTentacleEntity(int alIndex)", +[](P *p, int i) -> cMeshEntity * {
			T *t = static_cast<T *>(p);
			return i >= 0 && i < (int)t->mvTentacles.size() ? t->mvGroups[i % t->mvGroups.size()] : NULL;
		});
	}
	if (e->GetTypeInfoByName("cPostEffect_Blur"))
	{
		SOMA_FUNC(e, "cPostEffect_Blur@ cGraphics_CreatePostEffect_Blur()", +[]() -> P * { return new cSomaBlur(); });
		SOMA_METHOD(e, "cPostEffect_Blur", "void SetParams(float afAmount)", +[](P *p, float a) { p->Set({a}); });
		SOMA_METHOD(e, "cPostEffect_Blur", "void GetParams(float &out afAmount)", +[](P *p, float &a) { a = p->mfParams[0]; });
	}
	if (e->GetTypeInfoByName("cPostEffect_VideoDistortion") == NULL) // not in Bunker
		return;
	SOMA_FUNC(e, "cPostEffect_VideoDistortion@ cGraphics_CreatePostEffect_VideoDistortion()", +[]() { return new P("VideoDistortion"); });
	SOMA_METHOD(e, "cPostEffect_VideoDistortion",
				"void SetParams(float afAmount, float afRandomSeed, float afLineDensity, float afOffsetMul, const cVector2f&in avScreenOffset, const cVector2f&in avScreenBendAmount)",
				+[](P *p, float a, float s, float l, float m, const cVector2f &o, const cVector2f &b) { p->Set({a, s, l, m, o.x, o.y, b.x, b.y}); });
	SOMA_METHOD(e, "cPostEffect_VideoDistortion",
				"void GetParams(float &out afAmount, float&out afRandomSeed, float&out afLineDensity, float&out afOffsetMul, cVector2f&out avScreenOffset, cVector2f&out avScreenBendAmount)",
				+[](P *p, float &a, float &s, float &l, float &m, cVector2f &o, cVector2f &b) {
					a = p->mfParams[0]; s = p->mfParams[1]; l = p->mfParams[2]; m = p->mfParams[3];
					o = cVector2f(p->mfParams[4], p->mfParams[5]); b = cVector2f(p->mfParams[6], p->mfParams[7]);
				});
}
