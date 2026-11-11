#ifndef HPL_RUST_MATERIAL_SHIM_H
#define HPL_RUST_MATERIAL_SHIM_H

// Glue between the iMaterialType classes and the Rust material port: builds the views Rust reads,
// answers its lazy queries and applies the results. Included by the MaterialType_*.cpp files only.

#include <cstring>
#include <vector>

#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "resources/Resources.h"
#include "resources/GpuShaderManager.h"
#include "graphics/Graphics.h"
#include "graphics/GPUProgram.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/MaterialType_BasicTranslucent.h"
#include "graphics/Renderable.h"
#include "graphics/Renderer.h"
#include "graphics/RendererDeferred.h"
#include "graphics/RenderList.h"
#include "graphics/RustMaterial.h"
#include "graphics/Texture.h"
#include "math/Frustum.h"
#include "math/Math.h"
#include "scene/ForceField.h"
#include "scene/Light.h"
#include "scene/LightBox.h"
#include "scene/LightSpot.h"
#include "scene/World.h"

namespace hpl { namespace rustmat {

	inline void CopyColor(float* apDst, const cColor& aCol){ apDst[0] = aCol.r; apDst[1] = aCol.g; apDst[2] = aCol.b; apDst[3] = aCol.a; }
	inline void CopyVec3(float* apDst, const cVector3f& avVec){ apDst[0] = avVec.x; apDst[1] = avVec.y; apDst[2] = avVec.z; }

	/// apGraphics: set when the type's LoadData asks the GPU for caps.
	inline OhplGlobals MakeGlobals(cGraphics* apGraphics = NULL, float afVirtualPositionAddScale = 0.0f)
	{
		OhplGlobals g;
		memset(&g, 0, sizeof g);
		g.hdr = cRendererDeferred::GetHdr();
		g.parallaxEnabled = iRenderer::GetParallaxEnabled();
		g.parallaxLow = iRenderer::GetParallaxQuality() == eParallaxQuality_Low;
		if(apGraphics && g.parallaxLow == false) g.shaderModel3 = apGraphics->GetLowLevel()->GetCaps(eGraphicCaps_ShaderModel_3) != 0;
		g.gbuffer32bit = cRendererDeferred::GetGBufferType() == eDeferredGBuffer_32Bit;
		g.gbufferFourTargets = cRendererDeferred::GetNumOfGBufferTextures() == 4;
		g.hpsl = cGpuShaderManager::IsHpsl();
		g.refraction = iRenderer::GetRefractionEnabled();
		g.lightProbes = cMaterialType_Translucent::GetLightProbes();
		g.depthOfField = cMaterialType_Translucent::GetDepthOfField();
		g.texRect = cGraphics::GetTempFrameBufferTextureType() == eTextureType_Rect;
		g.worldFog = cMaterialType_Translucent::GetWorldFog();
		g.reflectionSizeDiv = iRenderer::GetReflectionSizeDiv();
		g.virtualPositionAddScale = afVirtualPositionAddScale;
		CopyColor(g.dofParams, cMaterialType_Translucent::GetDepthOfFieldParams());
		return g;
	}

	inline OhplMatView MakeView(cMaterial *apMaterial)
	{
		OhplMatView v;
		memset(&v, 0, sizeof v);
		for(int i=0; i<eMaterialTexture_LastEnum; ++i)
			if(apMaterial->GetTexture((eMaterialTexture)i)) v.texMask |= 1ull << i;
		memcpy(v.uvMatrix, apMaterial->GetUvMatrix().v, sizeof v.uvMatrix);
		if(iTexture *pLiquid = apMaterial->GetTexture(eMaterialTexture_Liquid))
		{
			v.liquidSize[0] = pLiquid->GetWidth();
			v.liquidSize[1] = pLiquid->GetHeight();
		}
		v.blendMode = apMaterial->GetBlendMode();
		v.hasUvAnim = apMaterial->HasUvAnimation();
		iTexture *pDiffuse = apMaterial->GetTexture(eMaterialTexture_Diffuse);
		v.diffuseSrgb = pDiffuse && pDiffuse->GetsRGB();
		return v;
	}

	inline OhplRenderView MakeRenderView(iRenderer *apRenderer)
	{
		OhplRenderView r;
		memset(&r, 0, sizeof r);
		if(cFrustum *pFrustum = apRenderer->GetCurrentFrustum())
		{
			memcpy(r.view, pFrustum->GetViewMatrix().v, sizeof r.view);
			CopyVec3(r.origin, pFrustum->GetOrigin());
			r.nearPlane = pFrustum->GetNearPlane();
			r.farPlane = pFrustum->GetFarPlane();
		}
		r.time = apRenderer->GetTimeCount();
		r.tempAlpha = apRenderer->GetTempAlpha();
		r.rtSize[0] = apRenderer->GetRenderTargetSize().x;
		r.rtSize[1] = apRenderer->GetRenderTargetSize().y;
		CopyColor(r.fogAreaColor, apRenderer->GetTempFogAreaColor());
		return r;
	}

	inline OhplObjView MakeObjView(iRenderable *apObject)
	{
		OhplObjView o;
		memset(&o, 0, sizeof o);
		if(cMatrixf *pMtx = apObject->GetModelMatrixPtr())
		{
			memcpy(o.model, pMtx->v, sizeof o.model);
			o.hasModel = true;
		}
		CopyColor(o.colorMul, apObject->GetColorMul());
		CopyColor(o.illumColor, apObject->GetIlluminationColor());
		o.illumAmount = apObject->GetIlluminationAmount();
		o.liquidAmount = apObject->GetLiquidAmount();
		o.isStatic = apObject->IsStatic();
		return o;
	}

	inline void Apply(iGpuProgram *apProgram, const OhplUniform *apUniforms, size_t alNum)
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

	/// Settings CompileMaterialSpecifics gives the material.
	inline void ApplyCompile(cMaterial *apMaterial, const OhplMatCompileOut &c)
	{
		for(int mode=0; mode<eMaterialRenderMode_LastEnum; ++mode)
		{
			if(c.specSet & (1u << mode))	apMaterial->SetHasSpecificSettings((eMaterialRenderMode)mode, (c.specVal >> mode) & 1);
			if(c.objSet & (1u << mode))		apMaterial->SetHasObjectSpecificsSettings((eMaterialRenderMode)mode, (c.objVal >> mode) & 1);
		}
		if(c.alphaMode >= 0)			apMaterial->SetAlphaMode((eMaterialAlphaMode)c.alphaMode);
		if(c.dissolveFilter >= 0)		apMaterial->SetUseAlphaDissolveFilter(c.dissolveFilter != 0);
		if(c.blendMode >= 0)			apMaterial->SetBlendMode((eMaterialBlendMode)c.blendMode);
		if(c.hasRefraction >= 0)		apMaterial->SetHasRefraction(c.hasRefraction != 0);
		if(c.refractionEdge >= 0)		apMaterial->SetUseRefractionEdgeCheck(c.refractionEdge != 0);
		if(c.worldReflection >= 0)		apMaterial->SetHasWorldReflection(c.worldReflection != 0);
		if(c.translucentIllum >= 0)		apMaterial->SetHasTranslucentIllumination(c.translucentIllum != 0);
	}

	//--------------------------------------------------------------------
	// Lazy queries
	//--------------------------------------------------------------------

	struct HostCtx
	{
		iRenderer *mpRenderer;
		iRenderable *mpObject;
		/// force fields cached by the type (undergrowth); object queries go to the world instead
		cForceField **mpFields;
		int *mpFieldNum;
	};

	inline float HostCoverage(void *apCtx)
	{
		HostCtx *h = (HostCtx*)apCtx;
		return h->mpRenderer->GetObjectCoverage(h->mpObject);
	}

	inline void FillField(OhplForceField &o, cForceField *apField)
	{
		cVector3f vPos = apField->GetWorldPosition();
		CopyVec3(o.pos, vPos);
		o.falloffStart = apField->GetFinalFalloffStartRadius();
		o.radius = apField->GetFinalRadius();
		o.force = apField->GetFinalForce();
		o.t = apField->GetT();
	}

	inline int HostForceFields(void *apCtx, OhplForceField *apOut)
	{
		HostCtx *h = (HostCtx*)apCtx;
		cForceField *vFields[4];
		int lNum;
		if(h->mpObject)
		{
			cBoundingVolume *pBV = h->mpObject->GetBoundingVolume();
			lNum = h->mpRenderer->GetCurrentWorld()->GetForceFields(pBV->GetMin(), pBV->GetMax(), vFields);
		}
		else
		{
			lNum = h->mpFieldNum ? *h->mpFieldNum : 0;
			for(int i=0; i<lNum && i<4; ++i) vFields[i] = h->mpFields[i];
		}
		for(int i=0; i<lNum && i<4; ++i) FillField(apOut[i], vFields[i]);
		return lNum;
	}

	inline int HostFieldsInBox(void *apCtx, const cVector3f *apMin, const cVector3f *apMax)
	{
		HostCtx *h = (HostCtx*)apCtx;
		int lNum = h->mpRenderer->GetCurrentWorld()->GetForceFields(*apMin, *apMax, h->mpFields);
		*h->mpFieldNum = lNum;
		return lNum;
	}

	inline bool HostWorld(void *apCtx, OhplWorld *apOut)
	{
		HostCtx *h = (HostCtx*)apCtx;
		cWorld *pWorld = h->mpRenderer->GetCurrentWorld();
		if(pWorld == NULL) return false;
		apOut->fogActive = h->mpRenderer->WorldFogActive();
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

	inline void HostObjectBv(void *apCtx, OhplBv *apOut)
	{
		cBoundingVolume *pBV = ((HostCtx*)apCtx)->mpObject->GetBoundingVolume();
		CopyVec3(apOut->min, pBV->GetMin());
		CopyVec3(apOut->max, pBV->GetMax());
		CopyVec3(apOut->worldCenter, pBV->GetWorldCenter());
	}

	inline int HostLightCount(void *apCtx)
	{
		return ((HostCtx*)apCtx)->mpRenderer->GetCurrentRenderList()->GetLightNum();
	}

	inline bool HostLight(void *apCtx, int alIdx, bool abDetail, OhplLight *apOut)
	{
		iLight *pLight = ((HostCtx*)apCtx)->mpRenderer->GetCurrentRenderList()->GetLight(alIdx);
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

	inline bool HostLightIntersects(void *apCtx, int alIdx)
	{
		HostCtx *h = (HostCtx*)apCtx;
		return h->mpRenderer->GetCurrentRenderList()->GetLight(alIdx)->CheckObjectIntersection(h->mpObject);
	}

	inline bool HostSpotClip(void *apCtx, int alIdx, cVector3f *apOut)
	{
		HostCtx *h = (HostCtx*)apCtx;
		cLightSpot *pSpot = static_cast<cLightSpot*>(h->mpRenderer->GetCurrentRenderList()->GetLight(alIdx));
		return cMath::GetNormalizedClipRectFromBV(apOut[0], apOut[1], *h->mpObject->GetBoundingVolume(), pSpot->GetFrustum(), pSpot->GetTanHalfFOV());
	}

	inline OhplHostVt MakeHost(HostCtx *apCtx)
	{
		OhplHostVt vt;
		vt.ctx = apCtx;
		vt.coverage = HostCoverage;
		vt.forceFields = HostForceFields;
		vt.fieldsInBox = HostFieldsInBox;
		vt.world = HostWorld;
		vt.objectBv = HostObjectBv;
		vt.lightCount = HostLightCount;
		vt.light = HostLight;
		vt.lightIntersects = HostLightIntersects;
		vt.spotClip = HostSpotClip;
		return vt;
	}

	//--------------------------------------------------------------------
	// Variables
	//--------------------------------------------------------------------

	inline tString Name(const char *apName, size_t alLen){ return tString(apName, alLen); }

	inline bool VarBool(void *c, const char *n, size_t l, bool d){ return ((cResourceVarsObject*)c)->GetVarBool(Name(n, l), d); }
	inline float VarFloat(void *c, const char *n, size_t l, float d){ return ((cResourceVarsObject*)c)->GetVarFloat(Name(n, l), d); }
	inline void VarVec2(void *c, const char *n, size_t l, const cVector2f *d, cVector2f *o){ *o = ((cResourceVarsObject*)c)->GetVarVector2f(Name(n, l), *d); }
	inline void VarVec3(void *c, const char *n, size_t l, const cVector3f *d, cVector3f *o){ *o = ((cResourceVarsObject*)c)->GetVarVector3f(Name(n, l), *d); }
	inline void VarColor(void *c, const char *n, size_t l, const cColor *d, cColor *o){ *o = ((cResourceVarsObject*)c)->GetVarColor(Name(n, l), *d); }
	inline size_t VarString(void *c, const char *n, size_t l, const char *d, size_t dl, char *o, size_t cap)
	{
		tString s = ((cResourceVarsObject*)c)->GetVarString(Name(n, l), tString(d, dl));
		memcpy(o, s.data(), s.size() < cap ? s.size() : cap);
		return s.size();
	}
	inline void AddVarBool(void *c, const char *n, size_t l, bool v){ ((cResourceVarsObject*)c)->AddVarBool(Name(n, l), v); }
	inline void AddVarFloat(void *c, const char *n, size_t l, float v){ ((cResourceVarsObject*)c)->AddVarFloat(Name(n, l), v); }

	inline OhplVarsVt MakeVarsVt(cResourceVarsObject *apVars)
	{
		OhplVarsVt vt;
		vt.ctx = apVars;
		vt.getBool = VarBool;
		vt.getFloat = VarFloat;
		vt.getVec2 = VarVec2;
		vt.getVec3 = VarVec3;
		vt.getColor = VarColor;
		vt.getString = VarString;
		vt.addBool = AddVarBool;
		vt.addFloat = AddVarFloat;
		return vt;
	}

	/// LoadVariables of every type: creates the vars when missing, loads, applies the material settings.
	inline void LoadVariables(int alKind, cMaterial *apMaterial, cResourceVarsObject *apVars, void *apPod)
	{
		OhplVarsVt vt = MakeVarsVt(apVars);
		OhplLoadOut out;
		memset(&out, 0, sizeof out);
		out.affectedByDof = out.worldReflectionOcclusionTest = out.largeTransparentSurface = -1;
		ohpl_mat_load_vars(alKind, apPod, &vt, &out);
		if(out.affectedByDof >= 0)						apMaterial->SetAffectedByDepthOfField(out.affectedByDof != 0);
		if(out.worldReflectionOcclusionTest >= 0)		apMaterial->SetWorldReflectionOcclusionTest(out.worldReflectionOcclusionTest != 0);
		if(out.hasMaxReflectionDistance)				apMaterial->SetMaxReflectionDistance(out.maxReflectionDistance);
		if(out.largeTransparentSurface >= 0)			apMaterial->SetLargeTransperantSurface(out.largeTransparentSurface != 0);
	}

	//--------------------------------------------------------------------
	// Program manager setup (LoadData)
	//--------------------------------------------------------------------

	struct SetupCtx
	{
		/// managers by Rust code: 0 = own, 2+i = blend[i]
		cProgramComboManager *mpOwn;
		cProgramComboManager **mpBlend;
		cParserVarContainer mVars;

		cProgramComboManager* Manager(int alCode){ return alCode >= kOhplMgrBlend0 ? mpBlend[alCode - kOhplMgrBlend0] : mpOwn; }
	};

	inline void SetupClear(void *c){ ((SetupCtx*)c)->mVars.Clear(); }
	inline void SetupDefine(void *c, const char *n, size_t l){ ((SetupCtx*)c)->mVars.Add(tString(n, l)); }
	inline void SetupDefineValue(void *c, const char *n, size_t l, float v){ ((SetupCtx*)c)->mVars.Add(tString(n, l), v); }
	inline void SetupProgram(void *c, int alManager, int alMode, const char *n, size_t nl, const char *v, size_t vl, const char *f, size_t fl,
							 const OhplFeature *apFeatures, size_t alNum)
	{
		SetupCtx *s = (SetupCtx*)c;
		std::vector<cProgramComboFeature> vFeatures;
		vFeatures.reserve(alNum);
		for(size_t i=0; i<alNum; ++i) vFeatures.push_back(cProgramComboFeature(apFeatures[i].name, apFeatures[i].stage, apFeatures[i].required));
		s->Manager(alManager)->SetupGenerateProgramData(alMode, tString(n, nl), tString(v, vl), tString(f, fl), vFeatures.data(), (int)alNum, s->mVars);
	}
	inline void SetupVarId(void *c, int alManager, const char *n, size_t nl, int alId, int alMode)
	{
		((SetupCtx*)c)->Manager(alManager)->AddGenerateProgramVariableId(tString(n, nl), alId, alMode);
	}

	inline void RunSetup(int alKind, bool abGlobal, const OhplGlobals &aGlobals, cProgramComboManager *apOwn, cProgramComboManager **apBlend)
	{
		SetupCtx ctx;
		ctx.mpOwn = apOwn;
		ctx.mpBlend = apBlend;
		OhplSetupVt vt;
		vt.ctx = &ctx;
		vt.clearDefines = SetupClear;
		vt.define = SetupDefine;
		vt.defineValue = SetupDefineValue;
		vt.setupProgram = SetupProgram;
		vt.varId = SetupVarId;
		ohpl_mat_setup_programs(alKind, abGlobal, &aGlobals, &vt);
	}


	//--------------------------------------------------------------------
	// Results
	//--------------------------------------------------------------------

	enum { kMaxUniforms = 64 };

	inline iTexture* PickTexture(const OhplTexSel &aSel, cMaterial *apMaterial, cGraphics *apGraphics, iTexture *apDissolve, iTexture *apWhite)
	{
		switch(aSel.kind)
		{
		case kOhplTexSlot:			return apMaterial->GetTexture((eMaterialTexture)aSel.slot);
		case kOhplTexDissolve:		return apDissolve;
		case kOhplTexRefraction:	return apGraphics->GetRenderer(eRenderer_Main)->GetRefractionTexture();
		case kOhplTexSceneDepth:	return apGraphics->GetRenderer(eRenderer_Main)->GetSceneDepthTexture();
		case kOhplTexReflection:	return apGraphics->GetRenderer(eRenderer_Main)->GetReflectionTexture();
		case kOhplTexWhite:			return apWhite;
		}
		return NULL;
	}

	/// apBlend: the five blend managers of the translucent type, else NULL.
	inline cProgramComboManager* PickManager(int alCode, cProgramComboManager *apOwn, cProgramComboManager *apGlobal, cProgramComboManager **apBlend)
	{
		if(alCode == kOhplMgrOwn)		return apOwn;
		if(alCode == kOhplMgrGlobal)	return apGlobal;
		if(alCode >= kOhplMgrBlend0 && apBlend) return apBlend[alCode - kOhplMgrBlend0];
		return NULL;
	}

	inline iGpuProgram* PickProgram(const OhplProgSel &aSel, cProgramComboManager *apOwn, cProgramComboManager *apGlobal, cProgramComboManager **apBlend)
	{
		cProgramComboManager *pManager = PickManager(aSel.manager, apOwn, apGlobal, apBlend);
		return pManager ? pManager->GenerateProgram(aSel.mode, aSel.flags) : NULL;
	}

	inline void DestroyProgram(int alKind, eMaterialRenderMode aRenderMode, int alBlendMode, iGpuProgram *apProgram,
							   cProgramComboManager *apOwn, cProgramComboManager *apGlobal, cProgramComboManager **apBlend)
	{
		OhplProgSel sel;
		ohpl_mat_destroy_target(alKind, aRenderMode, alBlendMode, &sel);
		if(cProgramComboManager *pManager = PickManager(sel.manager, apOwn, apGlobal, apBlend)) pManager->DestroyGeneratedProgram(sel.mode, apProgram);
	}

	//--------------------------------------------------------------------
	// One call per iMaterialType virtual
	//--------------------------------------------------------------------

	inline iTexture* TextureForUnit(int alKind, const void *apVars, cMaterial *apMaterial, cGraphics *apGraphics, eMaterialRenderMode aRenderMode, int alUnit,
									iTexture *apDissolve, iTexture *apWhite)
	{
		OhplMatView v = MakeView(apMaterial);
		OhplGlobals g = MakeGlobals();
		OhplTexSel sel;
		ohpl_mat_texture_unit(alKind, &v, apVars, &g, aRenderMode, alUnit, &sel);
		return PickTexture(sel, apMaterial, apGraphics, apDissolve, apWhite);
	}

	inline OhplProgSel SelectProgram(int alKind, const void *apVars, cMaterial *apMaterial, eMaterialRenderMode aRenderMode, int alSkeleton)
	{
		OhplMatView v = MakeView(apMaterial);
		OhplGlobals g = MakeGlobals();
		OhplProgSel sel;
		ohpl_mat_program(alKind, &v, apVars, &g, aRenderMode, alSkeleton, &sel);
		return sel;
	}

	inline void CompileMaterial(int alKind, const void *apVars, cMaterial *apMaterial)
	{
		OhplMatView v = MakeView(apMaterial);
		OhplGlobals g = MakeGlobals();
		OhplMatCompileOut out;
		ohpl_mat_compile(alKind, &v, apVars, &g, &out);
		ApplyCompile(apMaterial, out);
	}

	inline void SetupType(int alKind, eMaterialRenderMode aRenderMode, iGpuProgram *apProgram, iRenderer *apRenderer)
	{
		OhplRenderView r = MakeRenderView(apRenderer);
		OhplUniform vUniforms[kMaxUniforms];
		Apply(apProgram, vUniforms, ohpl_mat_setup_type(alKind, aRenderMode, &r, vUniforms, kMaxUniforms));
	}

	/// apFields/apFieldNum: the undergrowth type's cached force fields, else NULL.
	inline void SetupMaterial(int alKind, const void *apVars, eMaterialRenderMode aRenderMode, iGpuProgram *apProgram, cMaterial *apMaterial,
							  iRenderer *apRenderer, cForceField **apFields = NULL, int *apFieldNum = NULL)
	{
		OhplMatView v = MakeView(apMaterial);
		OhplGlobals g = MakeGlobals();
		OhplRenderView r = MakeRenderView(apRenderer);
		HostCtx ctx = { apRenderer, NULL, apFields, apFieldNum };
		OhplHostVt host = MakeHost(&ctx);
		OhplUniform vUniforms[kMaxUniforms];
		Apply(apProgram, vUniforms, ohpl_mat_setup_material(alKind, aRenderMode, &v, apVars, &g, &r, apFieldNum ? (size_t)*apFieldNum : 0, &host, vUniforms, kMaxUniforms));
	}

	/// The vars are those of the object's own material.
	inline void SetupObject(int alKind, const void *apVars, eMaterialRenderMode aRenderMode, iGpuProgram *apProgram, iRenderable *apObject, iRenderer *apRenderer)
	{
		OhplMatView v = MakeView(apObject->GetMaterial());
		OhplGlobals g = MakeGlobals();
		OhplRenderView r = MakeRenderView(apRenderer);
		OhplObjView o = MakeObjView(apObject);
		HostCtx ctx = { apRenderer, apObject, NULL, NULL };
		OhplHostVt host = MakeHost(&ctx);
		OhplUniform vUniforms[kMaxUniforms];
		Apply(apProgram, vUniforms, ohpl_mat_setup_object(alKind, aRenderMode, &v, apVars, &g, &r, &o, &host, vUniforms, kMaxUniforms));
	}

	inline void VarValues(int alKind, const void *apVars, cMaterial *apMaterial, cResourceVarsObject *apVarsObject)
	{
		OhplVarsVt vt = MakeVarsVt(apVarsObject);
		ohpl_mat_var_values(alKind, apVars, apMaterial->GetAffectedByDepthOfField(), apMaterial->GetWorldReflectionOcclusionTest(),
							apMaterial->GetLargeTransperantSurface(), &vt);
	}

}}

#endif
