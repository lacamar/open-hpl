#ifndef HPL_RUST_MATERIAL_H
#define HPL_RUST_MATERIAL_H

// C ABI of rust/src/material/ffi.rs. Layouts are asserted against the Rust side.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"

namespace hpl {

	enum eOhplMatKind
	{
		eOhplMatKind_SolidDiffuse,
		eOhplMatKind_ProjectedUv,
		eOhplMatKind_VertexBlend,
		eOhplMatKind_Translucent,
		eOhplMatKind_Water,
		eOhplMatKind_Decal,
		eOhplMatKind_TerrainBlend,
		eOhplMatKind_Undergrowth,
	};

	// Program manager codes of OhplProgSel::manager
	enum { kOhplMgrOwn = 0, kOhplMgrGlobal = 1, kOhplMgrBlend0 = 2, kOhplMgrNone = -1, kOhplMgrInvalid = -2 };
	enum { kOhplTexNone, kOhplTexSlot, kOhplTexDissolve, kOhplTexRefraction, kOhplTexSceneDepth, kOhplTexReflection, kOhplTexWhite };
	enum { kOhplUniFloat, kOhplUniInt, kOhplUniVec2, kOhplUniVec3, kOhplUniVec4, kOhplUniMat };

	//--------------------------------------------------------------------
	// Variables: bases of the cMaterialType_*_Vars classes (member names as the engine uses them)
	//--------------------------------------------------------------------

	struct OhplSolidDiffuseVars
	{
		float mfHeightMapScale, mfHeightMapBias, mfFrenselBias, mfFrenselPow;
		bool mbAlphaDissolveFilter, mbSwayActive, mbSwaySingleDir;
		cVector3f mvSwayProperties, mvSwayOctaveMuls, mvSwaySingleDir, mvSwaySingleSampleDir;
		float mfSwayYFreqMul;
		bool mbSwayForceFieldAffected;
		float mfSwayForceFieldMul, mfSwayForceFieldMax;
		float mvDetailProperties[4];
		cVector3f mvDetailWeights;
		float mfIlluminationBrightness;
		cColor mLiquidColor, mLiquidSpecular;
		bool mbLiquidLoopFade;
		cVector2f mvLiquidFadeSpeed;
		float mfLiquidEdgeSize, mfLiquidDryness;
		int mlLiquidBlendMode;
	};

	struct OhplProjectedUvVars
	{
		float mfBlendHardness;
		float mfNormalMapBlendImpact;
		cVector3f mvTextureScale;
		bool mbDynamicObjectSupport;
	};

	struct OhplVertexBlendVars
	{
		float mfNormalBlendHardness;
		float mfColorBlendHardness;
		cVector3f mvTextureScale;
		cVector3f mvHeightMapScale;
		cColor mvDirectionalWeight[3];
		bool mvAutoUv[3];
		bool mbAlphaMask;
		bool mbOverlayNormal;
	};

	struct OhplTranslucentVars
	{
		bool mbRefraction;
		bool mbRefractionEdgeCheck;
		bool mbRefractionNormals;
		bool mbRefractionAlphaBlend;
		float mfFogWeight;
		float mfRefractionScale;
		float mfFrenselBias;
		float mfFrenselPow;
		float mfRimLightMul;
		float mfRimLightPow;
		bool mbAffectedByLightLevel;
		bool mbSoftParticle;
		float mfSoftPartThickness;
		float mfSoftPartAlphaBasedThickness;
		float mfSoftPartDepthBias;
		float mfFadeInStart;
		float mfFadeInEnd;
		bool mbFadeColor;
		bool mbAngleFade;
		cColor mFadeColor;
	};

	struct OhplWaterVars
	{
		bool mbHasReflection;
		float mfRefractionScale;
		float mfFrenselBias;
		float mfFrenselPow;
		float mfReflectionFadeStart;
		float mfReflectionFadeEnd;
		float mfWaveSpeed;
		float mfWaveAmplitude;
		float mfWaveFreq;
		float mfFadeWhenShallowMul;
		float mfFadeWhenShallowPow;
		bool mbHasVertexWaves;
		float mfVertexWaveSpeed;
		float mfVertexWaveAmplitude;
		float mfVertexWaveFreq;
	};

	struct OhplTerrainBlendVars
	{
		float mvTextureCoordScale[4];
		float mvOneMinusFadeStart[4];
		float mvBlendMul[4];
		float mfBaseTextureCoordScale;
		cVector3f mvDetailMapScale, mvDetailFade, mvBaseDetailAmount, mvDetailAmount[4];
	};

	struct OhplUndergrowthVars
	{
		bool mbWind;
		cVector3f mvWind, mvWindOctaves;
		cVector2f mvDissolve;
		float mfForceFieldMul, mfMaxForceFieldForce;
	};

	static_assert(sizeof(OhplSolidDiffuseVars) == 172 && offsetof(OhplSolidDiffuseVars, mbSwayActive) == 17 &&
				  offsetof(OhplSolidDiffuseVars, mvSwayProperties) == 20 && offsetof(OhplSolidDiffuseVars, mvDetailProperties) == 84 &&
				  offsetof(OhplSolidDiffuseVars, mLiquidColor) == 116 && offsetof(OhplSolidDiffuseVars, mvLiquidFadeSpeed) == 152 &&
				  offsetof(OhplSolidDiffuseVars, mlLiquidBlendMode) == 168, "OhplSolidDiffuseVars layout");
	static_assert(sizeof(OhplProjectedUvVars) == 24 && offsetof(OhplProjectedUvVars, mvTextureScale) == 8, "OhplProjectedUvVars layout");
	static_assert(sizeof(OhplVertexBlendVars) == 88 && offsetof(OhplVertexBlendVars, mvDirectionalWeight) == 32 &&
				  offsetof(OhplVertexBlendVars, mvAutoUv) == 80, "OhplVertexBlendVars layout");
	static_assert(sizeof(OhplTranslucentVars) == 72 && offsetof(OhplTranslucentVars, mfFogWeight) == 4 &&
				  offsetof(OhplTranslucentVars, mfSoftPartThickness) == 32 && offsetof(OhplTranslucentVars, mbFadeColor) == 52 &&
				  offsetof(OhplTranslucentVars, mFadeColor) == 56, "OhplTranslucentVars layout");
	static_assert(sizeof(OhplWaterVars) == 60 && offsetof(OhplWaterVars, mbHasVertexWaves) == 44 &&
				  offsetof(OhplWaterVars, mfVertexWaveSpeed) == 48, "OhplWaterVars layout");
	static_assert(sizeof(OhplTerrainBlendVars) == 136 && offsetof(OhplTerrainBlendVars, mvDetailAmount) == 88, "OhplTerrainBlendVars layout");
	static_assert(sizeof(OhplUndergrowthVars) == 44 && offsetof(OhplUndergrowthVars, mvDissolve) == 28, "OhplUndergrowthVars layout");

	//--------------------------------------------------------------------
	// Views and results
	//--------------------------------------------------------------------

	struct OhplGlobals
	{
		bool hdr, parallaxEnabled, parallaxLow, shaderModel3, gbuffer32bit, gbufferFourTargets, hpsl, refraction;
		bool lightProbes, depthOfField, texRect, hasWaterVtx;
		uint32_t worldFog;
		int reflectionSizeDiv;
		float virtualPositionAddScale;
		float dofParams[4];
	};

	struct OhplMatView
	{
		uint64_t texMask;
		float uvMatrix[16];
		int liquidSize[2];
		int blendMode;
		bool hasUvAnim;
		bool diffuseSrgb;
	};

	struct OhplRenderView
	{
		float view[16];
		float origin[3];
		float time, nearPlane, farPlane, tempAlpha;
		int rtSize[2];
		float fogAreaColor[4];
	};

	struct OhplObjView
	{
		float model[16];
		float colorMul[4];
		float illumColor[4];
		float illumAmount, liquidAmount;
		bool hasModel, isStatic;
	};

	struct OhplForceField
	{
		float pos[3];
		float falloffStart, radius, force, t;
	};

	struct OhplWorld
	{
		float fogStart, fogEnd, fogFalloffExp, fogBrightness;
		float fogColor[4];
		float fogDensity, fogHeightDensity, fogHeightHorizon;
		float secFogStart, secFogEnd, secFogFalloffExp, secFogBrightness;
		float secFogColor[4];
		bool fogActive, fogExponential, fogHeightBased;
	};

	struct OhplBv
	{
		float min[3], max[3], worldCenter[3];
	};

	struct OhplLight
	{
		int kind;
		float min[3], max[3], pos[3];
		float radius, falloffPow, brightness;
		float diffuse[4];
		float nearClip;
		int blendFunc;
		float weight, bevel;
		float bands[4][3];
		bool useSh;
	};

	struct OhplUniform
	{
		uint32_t kind;
		int id;
		uint32_t check;
		int intValue;
		float v[16];
	};

	struct OhplProgSel { int manager; int mode; uint32_t flags; };
	struct OhplTexSel { int kind; int slot; };

	struct OhplMatCompileOut
	{
		uint32_t specSet, specVal, objSet, objVal;
		int alphaMode, dissolveFilter, blendMode, hasRefraction, refractionEdge, worldReflection, translucentIllum;
	};

	struct OhplLoadOut
	{
		int affectedByDof, worldReflectionOcclusionTest;
		float maxReflectionDistance;
		int largeTransparentSurface;
		bool hasMaxReflectionDistance;
	};

	struct OhplUvAnim { int type; float speed, amp; int axis; };

	struct OhplVarDef
	{
		const char* name; size_t nameLen;
		const char* desc; size_t descLen;
		bool isFloat, boolValue;
		float floatValue;
	};

	struct OhplFeature { const char* name; uint32_t stage; uint32_t required; };

	static_assert(sizeof(OhplGlobals) == 40 && offsetof(OhplGlobals, worldFog) == 12 && offsetof(OhplGlobals, dofParams) == 24, "OhplGlobals layout");
	static_assert(sizeof(OhplMatView) == 88 && offsetof(OhplMatView, uvMatrix) == 8 && offsetof(OhplMatView, blendMode) == 80 &&
				  offsetof(OhplMatView, hasUvAnim) == 84, "OhplMatView layout");
	static_assert(sizeof(OhplRenderView) == 116 && offsetof(OhplRenderView, rtSize) == 92, "OhplRenderView layout");
	static_assert(sizeof(OhplObjView) == 108 && offsetof(OhplObjView, hasModel) == 104, "OhplObjView layout");
	static_assert(sizeof(OhplForceField) == 28 && sizeof(OhplBv) == 36, "OhplForceField layout");
	static_assert(sizeof(OhplWorld) == 80 && offsetof(OhplWorld, fogActive) == 76, "OhplWorld layout");
	static_assert(sizeof(OhplLight) == 136 && offsetof(OhplLight, bands) == 84, "OhplLight layout");
	static_assert(sizeof(OhplUniform) == 80 && offsetof(OhplUniform, v) == 16, "OhplUniform layout");
	static_assert(sizeof(OhplProgSel) == 12 && sizeof(OhplTexSel) == 8 && sizeof(OhplMatCompileOut) == 44 && sizeof(OhplLoadOut) == 20, "result layouts");
	static_assert(sizeof(OhplUvAnim) == 16, "OhplUvAnim layout");

	//--------------------------------------------------------------------
	// Callback tables
	//--------------------------------------------------------------------

	struct OhplVarsVt
	{
		void* ctx;
		bool (*getBool)(void*, const char*, size_t, bool);
		float (*getFloat)(void*, const char*, size_t, float);
		void (*getVec2)(void*, const char*, size_t, const cVector2f*, cVector2f*);
		void (*getVec3)(void*, const char*, size_t, const cVector3f*, cVector3f*);
		void (*getColor)(void*, const char*, size_t, const cColor*, cColor*);
		size_t (*getString)(void*, const char*, size_t, const char*, size_t, char*, size_t);
		void (*addBool)(void*, const char*, size_t, bool);
		void (*addFloat)(void*, const char*, size_t, float);
	};

	struct OhplSetupVt
	{
		void* ctx;
		void (*clearDefines)(void*);
		void (*define)(void*, const char*, size_t);
		void (*defineValue)(void*, const char*, size_t, float);
		void (*setupProgram)(void*, int manager, int mode, const char* name, size_t nameLen, const char* vtx, size_t vtxLen,
							 const char* frag, size_t fragLen, const OhplFeature* features, size_t featureNum);
		void (*varId)(void*, int manager, const char* name, size_t nameLen, int id, int mode);
	};

	struct OhplHostVt
	{
		void* ctx;
		float (*coverage)(void*);
		int (*forceFields)(void*, OhplForceField*);
		int (*fieldsInBox)(void*, const cVector3f*, const cVector3f*);
		bool (*world)(void*, OhplWorld*);
		void (*objectBv)(void*, OhplBv*);
		int (*lightCount)(void*);
		bool (*light)(void*, int, bool, OhplLight*);
		bool (*lightIntersects)(void*, int);
		bool (*spotClip)(void*, int, cVector3f*);
	};

	extern "C" {

	uint32_t ohpl_mat_type_flags(int kind);
	size_t ohpl_mat_used_textures(int kind, int* out, size_t cap);
	bool ohpl_mat_user_var(int kind, size_t i, OhplVarDef* out);
	void ohpl_mat_default_vars(int kind, void* vars);
	void ohpl_mat_load_vars(int kind, void* vars, OhplVarsVt* api, OhplLoadOut* out);
	void ohpl_mat_var_values(int kind, const void* vars, bool affectedByDof, bool occlusionTest, bool largeSurface, OhplVarsVt* api);
	void ohpl_mat_setup_programs(int kind, bool global, const OhplGlobals* g, OhplSetupVt* sink);
	void ohpl_mat_texture_unit(int kind, const OhplMatView* view, const void* vars, const OhplGlobals* g, int mode, int unit, OhplTexSel* out);
	void ohpl_mat_program(int kind, const OhplMatView* view, const void* vars, const OhplGlobals* g, int mode, int skeleton, OhplProgSel* out);
	void ohpl_mat_destroy_target(int kind, int mode, int blendMode, OhplProgSel* out);
	int ohpl_mat_render_program_slot(int kind, int mode, const OhplGlobals* g);
	void ohpl_mat_undergrowth_render_program(const OhplUndergrowthVars* vars, int mode, const OhplRenderView* r, OhplHostVt* host, OhplProgSel* out, size_t* fields);
	void ohpl_mat_compile(int kind, const OhplMatView* view, const void* vars, const OhplGlobals* g, OhplMatCompileOut* out);
	size_t ohpl_mat_setup_type(int kind, int mode, const OhplRenderView* r, OhplUniform* out, size_t cap);
	size_t ohpl_mat_setup_material(int kind, int mode, const OhplMatView* view, const void* vars, const OhplGlobals* g, const OhplRenderView* r,
								   size_t fieldNum, OhplHostVt* host, OhplUniform* out, size_t cap);
	size_t ohpl_mat_setup_object(int kind, int mode, const OhplMatView* view, const void* vars, const OhplGlobals* g, const OhplRenderView* r,
								 const OhplObjView* obj, OhplHostVt* host, OhplUniform* out, size_t cap);
	void ohpl_mat_uv_matrix(const OhplUvAnim* anims, size_t n, float time, cMatrixf* out);
	uint32_t ohpl_mat_world_fog_flags(bool underwater, bool secondary);

	}
}

#endif
