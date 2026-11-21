#include <cstring>
#include <vector>

#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "resources/Resources.h"
#include "resources/FileSearcher.h"
#include "resources/GpuShaderManager.h"
#include "resources/TextureManager.h"
#include "graphics/Bitmap.h"
#include "graphics/Graphics.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "graphics/RendererDeferred.h"
#include "graphics/RenderList.h"
#include "graphics/Texture.h"
#include "math/Frustum.h"
#include "math/Math.h"
#include "scene/ForceField.h"
#include "scene/Light.h"
#include "scene/LightBox.h"
#include "scene/LightSpot.h"
#include "scene/World.h"

// Engine calls of rust/src/material/{mt,pcm}.rs over the classes still in C++ (renderer, renderable,
// GPU program and shader objects, textures, variable objects). Each goes away with the class it wraps.

namespace hpl {

	namespace {

	void CopyColor(float *apDst, const cColor &aCol){ apDst[0] = aCol.r; apDst[1] = aCol.g; apDst[2] = aCol.b; apDst[3] = aCol.a; }
	void CopyVec3(float *apDst, const cVector3f &avVec){ apDst[0] = avVec.x; apDst[1] = avVec.y; apDst[2] = avVec.z; }

	iGpuProgram *Prog(void *p){ return static_cast<iGpuProgram*>(p); }
	iGpuShader *Shader(void *p){ return static_cast<iGpuShader*>(p); }
	cResources *Res(void *p){ return static_cast<cResources*>(p); }
	cGraphics *Gfx(void *p){ return static_cast<cGraphics*>(p); }

	void Name(const tString &asName, const unsigned char **apOut, size_t *apLen){ *apOut = (const unsigned char*)asName.data(); *apLen = asName.size(); }

	void Apply(iGpuProgram *apProgram, const OhplUniform *apUniforms, size_t alNum)
	{
		for(size_t i=0; i<alNum; ++i)
		{
			const OhplUniform &u = apUniforms[i];
			switch(u.kind)
			{
			case kOhplUniFloat:
				if(apProgram->SetFloat(u.id, u.v[0]) == false && u.check) Error("Could not set variable!\n");
				break;
			case kOhplUniInt:	apProgram->SetInt(u.id, u.intValue); break;
			case kOhplUniVec2:	apProgram->SetVec2f(u.id, u.v[0], u.v[1]); break;
			case kOhplUniVec3:	apProgram->SetVec3f(u.id, u.v[0], u.v[1], u.v[2]); break;
			case kOhplUniVec4:	apProgram->SetVec4f(u.id, u.v[0], u.v[1], u.v[2], u.v[3]); break;
			case kOhplUniMat:
				{
					cMatrixf mtx;
					memcpy(mtx.v, u.v, sizeof mtx.v);
					apProgram->SetMatrixf(u.id, mtx);
				}
				break;
			}
		}
	}

	// Lazy queries of the setup passes

	float HostCoverage(void *apCtx)
	{
		OhplMtHostCtx *h = (OhplMtHostCtx*)apCtx;
		return static_cast<iRenderer*>(h->renderer)->GetObjectCoverage(static_cast<iRenderable*>(h->object));
	}

	void FillField(OhplForceField &o, cForceField *apField)
	{
		cVector3f vPos = apField->GetWorldPosition();
		CopyVec3(o.pos, vPos);
		o.falloffStart = apField->GetFinalFalloffStartRadius();
		o.radius = apField->GetFinalRadius();
		o.force = apField->GetFinalForce();
		o.t = apField->GetT();
	}

	int HostForceFields(void *apCtx, OhplForceField *apOut)
	{
		OhplMtHostCtx *h = (OhplMtHostCtx*)apCtx;
		iRenderer *pRenderer = static_cast<iRenderer*>(h->renderer);
		iRenderable *pObject = static_cast<iRenderable*>(h->object);
		cForceField *vFields[4];
		int lNum;
		if(pObject)
		{
			cBoundingVolume *pBV = pObject->GetBoundingVolume();
			lNum = pRenderer->GetCurrentWorld()->GetForceFields(pBV->GetMin(), pBV->GetMax(), vFields);
		}
		else
		{
			lNum = h->fieldNum ? *h->fieldNum : 0;
			for(int i=0; i<lNum && i<4; ++i) vFields[i] = static_cast<cForceField*>(h->fields[i]);
		}
		for(int i=0; i<lNum && i<4; ++i) FillField(apOut[i], vFields[i]);
		return lNum;
	}

	int HostFieldsInBox(void *apCtx, const cVector3f *apMin, const cVector3f *apMax)
	{
		OhplMtHostCtx *h = (OhplMtHostCtx*)apCtx;
		int lNum = static_cast<iRenderer*>(h->renderer)->GetCurrentWorld()->GetForceFields(*apMin, *apMax, reinterpret_cast<cForceField**>(h->fields));
		*h->fieldNum = lNum;
		return lNum;
	}

	bool HostWorld(void *apCtx, OhplWorld *apOut)
	{
		OhplMtHostCtx *h = (OhplMtHostCtx*)apCtx;
		iRenderer *pRenderer = static_cast<iRenderer*>(h->renderer);
		cWorld *pWorld = pRenderer->GetCurrentWorld();
		if(pWorld == NULL) return false;
		apOut->fogActive = pRenderer->WorldFogActive();
		apOut->fogStart = pWorld->GetFogStart();
		apOut->fogEnd = pWorld->GetFogEnd();
		apOut->fogFalloffExp = pWorld->GetFogFalloffExp();
		apOut->fogBrightness = pWorld->GetFogBrightness();
		CopyColor(apOut->fogColor, pWorld->GetFogColor());
		apOut->fogExponential = pWorld->GetFogExponential();
		apOut->fogHeightBased = pWorld->GetFogHeightBased();
		apOut->fogDensity = pWorld->GetFogDensity();
		apOut->fogHeightDensity = pWorld->GetFogHeightDensity();
		apOut->fogHeightHorizon = pWorld->GetFogHeightHorizon();
		apOut->secFogStart = pWorld->GetSecondaryFogStart();
		apOut->secFogEnd = pWorld->GetSecondaryFogEnd();
		apOut->secFogFalloffExp = pWorld->GetSecondaryFogFalloffExp();
		apOut->secFogBrightness = pWorld->GetSecondaryFogBrightness();
		CopyColor(apOut->secFogColor, pWorld->GetSecondaryFogColor());
		return true;
	}

	void HostObjectBv(void *apCtx, OhplBv *apOut)
	{
		cBoundingVolume *pBV = static_cast<iRenderable*>(((OhplMtHostCtx*)apCtx)->object)->GetBoundingVolume();
		CopyVec3(apOut->min, pBV->GetMin());
		CopyVec3(apOut->max, pBV->GetMax());
		CopyVec3(apOut->worldCenter, pBV->GetWorldCenter());
	}

	cRenderList *List(void *apCtx){ return static_cast<iRenderer*>(((OhplMtHostCtx*)apCtx)->renderer)->GetCurrentRenderList(); }

	int HostLightCount(void *apCtx){ return List(apCtx)->GetLightNum(); }

	bool HostLight(void *apCtx, int alIdx, bool abDetail, OhplLight *apOut)
	{
		iLight *pLight = List(apCtx)->GetLight(alIdx);
		apOut->kind = pLight->GetLightType();
		CopyVec3(apOut->pos, pLight->GetWorldPosition());
		apOut->radius = pLight->GetRadius();
		apOut->falloffPow = pLight->GetFalloffPow();
		apOut->brightness = pLight->GetBrightness();
		CopyColor(apOut->diffuse, pLight->GetDiffuseColor());
		if(abDetail)
		{
			cBoundingVolume *pBV = pLight->GetBoundingVolume();
			CopyVec3(apOut->min, pBV->GetMin());
			CopyVec3(apOut->max, pBV->GetMax());
			if(pLight->GetLightType() == eLightType_Box)
			{
				cLightBox *pBox = static_cast<cLightBox*>(pLight);
				apOut->falloffPow = pBox->GetFalloffPow();
				apOut->blendFunc = pBox->GetBlendFunc();
				apOut->weight = pBox->GetWeight();
				apOut->bevel = pBox->GetBevel();
				apOut->useSh = pBox->GetUseSphericalHarmonics();
				const cVector3f *pBands = pBox->GetIrradianceBands();
				for(int k=0; k<4; ++k) CopyVec3(apOut->bands[k], pBands[k]);
			}
			else if(pLight->GetLightType() == eLightType_Spot)
			{
				apOut->nearClip = static_cast<cLightSpot*>(pLight)->GetNearClipPlane();
			}
		}
		return true;
	}

	bool HostLightIntersects(void *apCtx, int alIdx)
	{
		return List(apCtx)->GetLight(alIdx)->CheckObjectIntersection(static_cast<iRenderable*>(((OhplMtHostCtx*)apCtx)->object));
	}

	bool HostSpotClip(void *apCtx, int alIdx, cVector3f *apOut)
	{
		cLightSpot *pSpot = static_cast<cLightSpot*>(List(apCtx)->GetLight(alIdx));
		return cMath::GetNormalizedClipRectFromBV(apOut[0], apOut[1], *static_cast<iRenderable*>(((OhplMtHostCtx*)apCtx)->object)->GetBoundingVolume(), pSpot->GetFrustum(), pSpot->GetTanHalfFOV());
	}

	// Variable objects

	cResourceVarsObject *Vars(void *c){ return static_cast<cResourceVarsObject*>(c); }
	tString Str(const char *n, size_t l){ return tString(n, l); }

	bool VarBool(void *c, const char *n, size_t l, bool d){ return Vars(c)->GetVarBool(Str(n, l), d); }
	float VarFloat(void *c, const char *n, size_t l, float d){ return Vars(c)->GetVarFloat(Str(n, l), d); }
	void VarVec2(void *c, const char *n, size_t l, const cVector2f *d, cVector2f *o){ *o = Vars(c)->GetVarVector2f(Str(n, l), *d); }
	void VarVec3(void *c, const char *n, size_t l, const cVector3f *d, cVector3f *o){ *o = Vars(c)->GetVarVector3f(Str(n, l), *d); }
	void VarColor(void *c, const char *n, size_t l, const cColor *d, cColor *o){ *o = Vars(c)->GetVarColor(Str(n, l), *d); }
	size_t VarString(void *c, const char *n, size_t l, const char *d, size_t dl, char *o, size_t cap)
	{
		tString s = Vars(c)->GetVarString(Str(n, l), tString(d, dl));
		memcpy(o, s.data(), s.size() < cap ? s.size() : cap);
		return s.size();
	}
	void AddVarBool(void *c, const char *n, size_t l, bool v){ Vars(c)->AddVarBool(Str(n, l), v); }
	void AddVarFloat(void *c, const char *n, size_t l, float v){ Vars(c)->AddVarFloat(Str(n, l), v); }

	void *VarsNew(int alCode)
	{
		switch(alCode)
		{
		case eOhplMatKind_SolidDiffuse:		return static_cast<iMaterialVars*>(hplNew(cMaterialType_SolidDiffuse_Vars,()));
		case eOhplMatKind_ProjectedUv:		return static_cast<iMaterialVars*>(hplNew(cMaterialType_ProjectedUV_Vars,()));
		case eOhplMatKind_VertexBlend:		return static_cast<iMaterialVars*>(hplNew(cMaterialType_VertexBlend_Vars,()));
		case eOhplMatKind_Translucent:		return static_cast<iMaterialVars*>(hplNew(cMaterialType_Translucent_Vars,()));
		case eOhplMatKind_Water:			return static_cast<iMaterialVars*>(hplNew(cMaterialType_Water_Vars,()));
		case eOhplMatKind_TerrainBlend:		return static_cast<iMaterialVars*>(hplNew(cMaterialType_TerrainBlend_Vars,()));
		case eOhplMatKind_Undergrowth:		return static_cast<iMaterialVars*>(hplNew(cMaterialType_Undergrowth_Vars,()));
		}
		return hplNew(iMaterialVars,());
	}

	void *VarsPod(int alCode, void *apVars)
	{
		iMaterialVars *v = static_cast<iMaterialVars*>(apVars);
		switch(alCode)
		{
		case eOhplMatKind_SolidDiffuse:		return static_cast<OhplSolidDiffuseVars*>(static_cast<cMaterialType_SolidDiffuse_Vars*>(v));
		case eOhplMatKind_ProjectedUv:		return static_cast<OhplProjectedUvVars*>(static_cast<cMaterialType_ProjectedUV_Vars*>(v));
		case eOhplMatKind_VertexBlend:		return static_cast<OhplVertexBlendVars*>(static_cast<cMaterialType_VertexBlend_Vars*>(v));
		case eOhplMatKind_Translucent:		return static_cast<OhplTranslucentVars*>(static_cast<cMaterialType_Translucent_Vars*>(v));
		case eOhplMatKind_Water:			return static_cast<OhplWaterVars*>(static_cast<cMaterialType_Water_Vars*>(v));
		case eOhplMatKind_TerrainBlend:		return static_cast<OhplTerrainBlendVars*>(static_cast<cMaterialType_TerrainBlend_Vars*>(v));
		case eOhplMatKind_Undergrowth:		return static_cast<OhplUndergrowthVars*>(static_cast<cMaterialType_Undergrowth_Vars*>(v));
		}
		return NULL;
	}

	const OhplMtHost gHost = {
		[](void *g, const unsigned char *n, size_t l) -> void*
		{
			iGpuProgram *pProgram = Gfx(g)->GetLowLevel()->CreateGpuProgram(tString((const char*)n, l));
			pProgram->SetAutoDestroyShaders(false);
			return pProgram;
		},
		[](void *p){ return Prog(p)->Link(); },
		[](void *p){ hplDelete(Prog(p)); },
		[](void *p, void *v, void *f){ Prog(p)->SetShader(eGpuShaderType_Vertex, Shader(v)); Prog(p)->SetShader(eGpuShaderType_Fragment, Shader(f)); },
		[](void *p, void *r){ Prog(p)->SetAutoDestroyShaders(true); Prog(p)->SetResources(Res(r)); },
		[](void *p, int t) -> void* { return p ? Prog(p)->GetShader((eGpuShaderType)t) : NULL; },
		[](void *p, uint32_t id){ Prog(p)->SetUserId(id); },
		[](void *p) -> uint32_t { return Prog(p)->GetUserId(); },
		[](void *p, const unsigned char **o, size_t *n){ Name(Prog(p)->GetName(), o, n); },
		[](void *p, const unsigned char *n, size_t l, int id){ Prog(p)->GetVariableAsId(tString((const char*)n, l), id); },
		[](void *p, const OhplUniform *u, size_t n){ Apply(Prog(p), u, n); },
		[](void *r, const unsigned char *n, size_t l, int t, const OhplMtKV *kv, size_t kn, bool abVars) -> void*
		{
			cParserVarContainer vars;
			for(size_t i=0; i<kn; ++i) vars.Add(tString((const char*)kv[i].name, kv[i].nameLen), tString((const char*)kv[i].value, kv[i].valueLen));
			return Res(r)->GetGpuShaderManager()->CreateShader(tString((const char*)n, l), (eGpuShaderType)t, abVars ? &vars : NULL);
		},
		[](void *r, void *s){ Res(r)->GetGpuShaderManager()->Destroy(Shader(s)); },
		[](void *s, uint32_t id){ Shader(s)->SetUserId(id); },
		[](void *s) -> uint32_t { return Shader(s)->GetUserId(); },
		[](void *s, const unsigned char **o, size_t *n){ Name(Shader(s)->GetName(), o, n); },
		[](const unsigned char *m, size_t n){ Error("%.*s", (int)n, (const char*)m); },
		[](void *g, float afVps, OhplGlobals *o)
		{
			memset(o, 0, sizeof *o);
			o->hdr = cRendererDeferred::GetHdr();
			o->parallaxEnabled = iRenderer::GetParallaxEnabled();
			o->parallaxLow = iRenderer::GetParallaxQuality() == eParallaxQuality_Low;
			if(g && o->parallaxLow == false) o->shaderModel3 = Gfx(g)->GetLowLevel()->GetCaps(eGraphicCaps_ShaderModel_3) != 0;
			o->gbuffer32bit = cRendererDeferred::GetGBufferType() == eDeferredGBuffer_32Bit;
			o->gbufferFourTargets = cRendererDeferred::GetNumOfGBufferTextures() == 4;
			o->hpsl = cGpuShaderManager::IsHpsl();
			o->refraction = iRenderer::GetRefractionEnabled();
			o->texRect = cGraphics::GetTempFrameBufferTextureType() == eTextureType_Rect;
			o->reflectionSizeDiv = iRenderer::GetReflectionSizeDiv();
			o->virtualPositionAddScale = afVps;
		},
		[](void *rp, OhplRenderView *r)
		{
			iRenderer *apRenderer = static_cast<iRenderer*>(rp);
			memset(r, 0, sizeof *r);
			if(cFrustum *pFrustum = apRenderer->GetCurrentFrustum())
			{
				memcpy(r->view, pFrustum->GetViewMatrix().v, sizeof r->view);
				CopyVec3(r->origin, pFrustum->GetOrigin());
				r->nearPlane = pFrustum->GetNearPlane();
				r->farPlane = pFrustum->GetFarPlane();
			}
			r->time = apRenderer->GetTimeCount();
			r->tempAlpha = apRenderer->GetTempAlpha();
			r->rtSize[0] = apRenderer->GetRenderTargetSize().x;
			r->rtSize[1] = apRenderer->GetRenderTargetSize().y;
			CopyColor(r->fogAreaColor, apRenderer->GetTempFogAreaColor());
		},
		[](void *op, OhplObjView *o) -> void*
		{
			iRenderable *apObject = static_cast<iRenderable*>(op);
			memset(o, 0, sizeof *o);
			if(cMatrixf *pMtx = apObject->GetModelMatrixPtr())
			{
				memcpy(o->model, pMtx->v, sizeof o->model);
				o->hasModel = true;
			}
			CopyColor(o->colorMul, apObject->GetColorMul());
			CopyColor(o->illumColor, apObject->GetIlluminationColor());
			o->illumAmount = apObject->GetIlluminationAmount();
			o->liquidAmount = apObject->GetLiquidAmount();
			o->isStatic = apObject->IsStatic();
			cMaterial *pMat = apObject->GetMaterial();
			return pMat ? iMaterialType::State(pMat) : NULL;
		},
		[](OhplMtHostCtx *c, OhplHostVt *vt)
		{
			vt->ctx = c;
			vt->coverage = HostCoverage;
			vt->forceFields = HostForceFields;
			vt->fieldsInBox = HostFieldsInBox;
			vt->world = HostWorld;
			vt->objectBv = HostObjectBv;
			vt->lightCount = HostLightCount;
			vt->light = HostLight;
			vt->lightIntersects = HostLightIntersects;
			vt->spotClip = HostSpotClip;
		},
		[](void *obj, OhplVarsVt *vt)
		{
			vt->ctx = obj;
			vt->getBool = VarBool;
			vt->getFloat = VarFloat;
			vt->getVec2 = VarVec2;
			vt->getVec3 = VarVec3;
			vt->getColor = VarColor;
			vt->getString = VarString;
			vt->addBool = AddVarBool;
			vt->addFloat = AddVarFloat;
		},
		VarsNew,
		VarsPod,
		[]() -> void* { return hplNew(cResourceVarsObject,()); },
		[](void *r, const unsigned char *n, size_t l, bool abMips) -> void* { return Res(r)->GetTextureManager()->Create2D(tString((const char*)n, l), abMips); },
		[](void *r, void *t){ Res(r)->GetTextureManager()->Destroy(static_cast<iTexture*>(t)); },
		[](void *g) -> void*
		{
			cBitmap bmp;
			bmp.CreateData(cVector3l(1,1,1), ePixelFormat_RGBA, 0, 0);
			bmp.Clear(cColor(1,1), 0, 0);
			iTexture *pTex = Gfx(g)->CreateTexture("TerrainBlendWhite", eTextureType_2D, eTextureUsage_Normal);
			pTex->SetUseMipMaps(false);
			pTex->CreateFromBitmap(&bmp);
			return pTex;
		},
		[](void *g, void *t){ Gfx(g)->DestroyTexture(static_cast<iTexture*>(t)); },
		[](void *t, int *w, int *h, bool *srgb){ iTexture *p = static_cast<iTexture*>(t); *w = p->GetWidth(); *h = p->GetHeight(); *srgb = p->GetsRGB(); },
		[](void *g, int alKind) -> void*
		{
			iRenderer *pRenderer = Gfx(g)->GetRenderer(eRenderer_Main);
			switch(alKind)
			{
			case kOhplTexRefraction:	return pRenderer->GetRefractionTexture();
			case kOhplTexSceneDepth:	return pRenderer->GetSceneDepthTexture();
			case kOhplTexReflection:	return pRenderer->GetReflectionTexture();
			}
			return NULL;
		},
		[](void *r, const unsigned char *n, size_t l) -> bool { return Res(r)->GetFileSearcher()->GetFilePath(tString((const char*)n, l)) != _W(""); },
	};

	}

	const OhplMtHost *OhplMtHostGet(){ return &gHost; }

	void OhplDestroyGpuShader(cResources *apResources, iGpuShader *apShader){ if(apShader) apResources->GetGpuShaderManager()->Destroy(apShader); }

	static void Pairs(cParserVarContainer *apVars, std::vector<OhplMtKV> &avOut)
	{
		if(apVars == NULL) return;
		tParseVarMap *pMap = apVars->GetMapPtr();
		for(tParseVarMapIt it = pMap->begin(); it != pMap->end(); ++it)
		{
			OhplMtKV kv = { (const unsigned char*)it->first.data(), it->first.size(), (const unsigned char*)it->second.data(), it->second.size() };
			avOut.push_back(kv);
		}
	}

	void cProgramComboManager::SetupGenerateProgramData(int alMainMode, const tString &asModeName,
														const tString &asVtxShaderName, const tString &asFragShaderName,
														cProgramComboFeature *apFeatures, int alFeatureNum,
														cParserVarContainer &avDefaultVars)
	{
		std::vector<OhplMtKV> vFeatures, vDefaults;
		std::vector<uint32_t> vStages;
		for(int i=0; i<alFeatureNum; ++i)
		{
			OhplMtKV kv = { (const unsigned char*)apFeatures[i].msVariable.data(), apFeatures[i].msVariable.size(), NULL, 0 };
			vFeatures.push_back(kv);
			vStages.push_back(apFeatures[i].mlShaders);
		}
		Pairs(&avDefaultVars, vDefaults);
		ohpl_pcm_setup_mode(mpRs, alMainMode, (const unsigned char*)asModeName.data(), asModeName.size(),
							(const unsigned char*)asVtxShaderName.data(), asVtxShaderName.size(),
							(const unsigned char*)asFragShaderName.data(), asFragShaderName.size(),
							vFeatures.data(), vStages.data(), vFeatures.size(), vDefaults.data(), vDefaults.size());
	}

	iGpuShader* cProgramComboManager::CreateShader(const tString& asName, eGpuShaderType aType, cParserVarContainer *apVars, bool abAddtoList)
	{
		std::vector<OhplMtKV> vVars;
		Pairs(apVars, vVars);
		return static_cast<iGpuShader*>(ohpl_pcm_create_shader(mpRs, (const unsigned char*)asName.data(), asName.size(), aType, vVars.data(), vVars.size(), apVars != NULL, abAddtoList));
	}

	iGpuProgram* cProgramComboManager::CreateProgramFromShaders(const tString &asProgramName, const tString &asVtxShaderName, const tString &asFragShaderName,
																cParserVarContainer *apVars, bool abAddtoList)
	{
		std::vector<OhplMtKV> vVars;
		Pairs(apVars, vVars);
		return static_cast<iGpuProgram*>(ohpl_pcm_program_from_names(mpRs, (const unsigned char*)asProgramName.data(), asProgramName.size(),
																	 (const unsigned char*)asVtxShaderName.data(), asVtxShaderName.size(),
																	 (const unsigned char*)asFragShaderName.data(), asFragShaderName.size(),
																	 vVars.data(), vVars.size(), apVars != NULL, abAddtoList));
	}

}
