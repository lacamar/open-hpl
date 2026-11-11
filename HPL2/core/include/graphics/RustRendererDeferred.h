#ifndef HPL_RUST_RENDERERDEFERRED_H
#define HPL_RUST_RENDERERDEFERRED_H

// C ABI of rust/src/render/deferred_ffi.rs. Slot and id constants mirror deferred.rs.

#include <cstddef>
#include <cstdint>

#include "graphics/RustRender.h"

namespace hpl {

	struct OhplDrPlan;

	enum eOhplDrOp
	{
		eOhplDrOp_State,
		eOhplDrOp_FlatProjection,
		eOhplDrOp_SetFb,
		eOhplDrOp_TexRangeNull,
		eOhplDrOp_Program,
		eOhplDrOp_Tex,
		eOhplDrOp_AcquireTemp,
		eOhplDrOp_Float,
		eOhplDrOp_Int,
		eOhplDrOp_Vec2,
		eOhplDrOp_Vec3,
		eOhplDrOp_Vec4,
		eOhplDrOp_FloatArr,
		eOhplDrOp_Quad,
		eOhplDrOp_SetGBuffer,
		eOhplDrOp_ClearDepthValue,
		eOhplDrOp_ClearColorValue,
		eOhplDrOp_ClearColorSettings,
		eOhplDrOp_ClearFb,
		eOhplDrOp_Ask,
		eOhplDrOp_TryToneMap,
		eOhplDrOp_Step,
		eOhplDrOp_FlatProjectionMinMax,
		eOhplDrOp_NormalFrustumProjection,
		eOhplDrOp_QuadAt,
		eOhplDrOp_CopyFbToTex,
		eOhplDrOp_SetAccumBuffer,
		eOhplDrOp_Matrix,
		eOhplDrOp_InvViewMatrix,
		eOhplDrOp_ScreenPositionVars,
		eOhplDrOp_GenProgram,
		eOhplDrOp_AskGeom,
		eOhplDrOp_Dl,
		eOhplDrOp_P3,
		eOhplDrOp_P4,
	};

	enum eOhplDrGeom
	{
		eOhplDrGeom_SphereNearPlane,
		eOhplDrGeom_FrustumNearPlane,
		eOhplDrGeom_LineIntersection,
		eOhplDrGeom_BoxNearPlane,
		eOhplDrGeom_QueryFetch,
		eOhplDrGeom_QuerySamples,
		eOhplDrGeom_ShadowNeedsUpdate,
	};

	enum eOhplDrDl
	{
		eOhplDrDl_ClearPairs,
		eOhplDrDl_ClearTempLights,
		eOhplDrDl_SetMinLargeArea,
		eOhplDrDl_Light,
		eOhplDrDl_AcquireQuery,
		eOhplDrDl_SetModelView,
		eOhplDrDl_SetShape,
		eOhplDrDl_QueryBegin,
		eOhplDrDl_QueryEnd,
		eOhplDrDl_DrawCurrent,
		eOhplDrDl_Flush,
		eOhplDrDl_InvView,
		eOhplDrDl_UpdateFullscreenQuad,
		eOhplDrDl_LightMatrix,
		eOhplDrDl_QueryLog,
		eOhplDrDl_ReleaseQuery,
		eOhplDrDl_Lists,
		eOhplDrDl_ClearShadowTexture,
		eOhplDrDl_ShadowData,
		eOhplDrDl_RenderShadowMap,
	};

	enum eOhplDrState { eOhplDrState_DepthTest, eOhplDrState_DepthWrite, eOhplDrState_DepthFunc, eOhplDrState_Blend,
						eOhplDrState_Alpha, eOhplDrState_Channel };
	enum { kOhplDrBlendNone = 0, kOhplDrBlendAlpha = 1, kOhplDrBlendAdd = 2, kOhplDrBlendMul = 3, kOhplDrBlendPremulAlpha = 4, kOhplDrBlendModulateSource = 5, kOhplDrAlphaSolid = 0,
		   kOhplDrChannelRgba = 0, kOhplDrChannelNone = 1, kOhplDrChannelRgb = 2,
		   kOhplDrFuncEqual = 0, kOhplDrFuncLessOrEqual = 1, kOhplDrFuncGreaterOrEqual = 2 };
	enum { kOhplDrFormatRgba = 0, kOhplDrFormatRgba16 = 1 };
	enum { kOhplDrCompFull = 0, kOhplDrCompLinearDepth = 5 };
	enum { kOhplDrClearDepth = 1, kOhplDrClearColor = 2 };
	enum eOhplDrQuery { eOhplDrQuery_Headroom, eOhplDrQuery_FogAfterAreas, eOhplDrQuery_FogIsSkybox, eOhplDrQuery_DepthOfField };

	enum eOhplDrStep
	{
		eOhplDrStep_SetupRenderVariables,
		eOhplDrStep_SetupGBuffer,
		eOhplDrStep_CheckVisibleZ,
		eOhplDrStep_CheckVisibleStatic,
		eOhplDrStep_CheckVisibleDynamic,
		eOhplDrStep_FetchDelayedOcclusion,
		eOhplDrStep_AssignOcclusionQueries,
		eOhplDrStep_SetupLights,
		eOhplDrStep_CompileOcclusion,
		eOhplDrStep_CompileBruteForce,
		eOhplDrStep_RenderZ,
		eOhplDrStep_RenderDelayedOcclusionQueries,
		eOhplDrStep_RenderGbuffer,
		eOhplDrStep_RenderGbufferContent,
		eOhplDrStep_RenderDecals,
		eOhplDrStep_CallbackPostGBuffer,
		eOhplDrStep_RenderLights,
		eOhplDrStep_RenderIllumination,
		eOhplDrStep_RenderFog,
		eOhplDrStep_RenderFullScreenFog,
		eOhplDrStep_RenderEdgeSmooth,
		eOhplDrStep_RenderBasicSkyBox,
		eOhplDrStep_CallbackPostSolid,
		eOhplDrStep_UpdateLensFlares,
		eOhplDrStep_RenderEnvironmentParticles,
		eOhplDrStep_RenderTranslucent,
		eOhplDrStep_RenderDepthOfField,
		eOhplDrStep_RenderMultiIrisAndGlare,
		eOhplDrStep_CallbackPostTranslucent,
		eOhplDrStep_RetrieveLightOcclusion,
		eOhplDrStep_CopyAccumToPostEffects,
		eOhplDrStep_InitLightRendering,
		eOhplDrStep_RenderSsao,
		eOhplDrStep_RenderHpl3Ssao,
		eOhplDrStep_RenderBoxWeighted,
		eOhplDrStep_RenderDirectional,
		eOhplDrStep_RenderNightVision,
		eOhplDrStep_ApplyHpl3Ssao,
		eOhplDrStep_LightPass,
	};

	enum eOhplDrPass { eOhplDrPass_ZBegin, eOhplDrPass_GbufferBegin, eOhplDrPass_GbufferEnd };

	enum
	{
		kOhplDrFbTarget = 0, kOhplDrFbAccum = 1, kOhplDrFbTonemap = 2, kOhplDrFbTrail = 3, kOhplDrFbBloom0 = 4, kOhplDrFbCount = 10,
		kOhplDrTexAccum = 0, kOhplDrTexTonemapOut = 1, kOhplDrTexTrail = 2, kOhplDrTexBloom0 = 3, kOhplDrTexGrading = 9,
		kOhplDrTexGradingTarget = 10, kOhplDrTexNoise = 11, kOhplDrTexLayer0 = 12, kOhplDrTexCount = 20,
		kOhplDrProgTonemap = 0, kOhplDrProgFxaa = 1, kOhplDrProgTrail = 2, kOhplDrProgBright = 3, kOhplDrProgBlur0 = 4,
		kOhplDrProgBlur1 = 5, kOhplDrProgCount = 6,
	};

	enum
	{
		kOhplDrVar_afKey = 28, kOhplDrVar_afExposure = 29, kOhplDrVar_afWhiteCut = 30, kOhplDrVar_afInvGammaCorrection = 31,
		kOhplDrVar_avInvScreenSize = 24, kOhplDrVar_avScreenToFarPlane = 23, kOhplDrVar_afT = 57, kOhplDrVar_afBrightPass = 79,
		kOhplDrVar_avWeights = 80, kOhplDrVar_avOffsets = 81, kOhplDrVar_afIntensity = 82, kOhplDrVar_avTransform0 = 83,
		kOhplDrVar_avTransform1 = 84, kOhplDrVar_avBloomTint = 85, kOhplDrVar_avSizeWeight = 86,
		kOhplDrVar_afGradingBlendWeight = 95, kOhplDrVar_afAlpha = 96, kOhplDrVar_afGradingMapAlpha0 = 102,
		kOhplDrVar_alGradingMapNum = 103, kOhplDrVar_afGradingMapAlpha2 = 104, kOhplDrVar_afHdrHeadroom = 119,
	};

	enum { kOhplDrDefine_UseUv, kOhplDrDefine_ColorGrading, kOhplDrDefine_Bloom, kOhplDrDefine_Noise, kOhplDrDefine_FilmGrain,
		   kOhplDrDefine_Srgb, kOhplDrDefine_LayeredGrading, kOhplDrDefine_BlendGrading, kOhplDrDefine_HdrOutput };

	enum { kOhplDrDebugNull, kOhplDrDebugShadow, kOhplDrDebugTemp, kOhplDrDebugDirShadow, kOhplDrDebugRefraction,
		   kOhplDrDebugReflection, kOhplDrDebugBoxWeight, kOhplDrDebugH3Ssao, kOhplDrDebugH3Mip, kOhplDrDebugAccum,
		   kOhplDrDebugBuffer };

	struct OhplDrOp
	{
		uint32_t kind;
		int32_t a, b, c, d, e;
		float f[12];
	};

	struct OhplDrFilmGrain
	{
		float t;
		float transform[2][4];
	};

	struct OhplDrCopyIn
	{
		OhplVec2l screen;
		OhplVec2f screenF;
		OhplVec2l rtPos, rtSize;
		bool rectType, useFxaa, fxaaOk, tonemapOk, trailOk, hpl35, grading, gradingTarget, noise, bloom, bloomProgramsOk, srgb;
		float trailAlpha;
		int skipPasses, noiseWidth;
		float key, exposure, whiteCut, gamma, bloomBright, bloomWidth, bloomTint[4], gradingBlend, filmIntensity, frameTime;
		int blurSamples;
		const float* layerWeights;
		size_t layerCount;
	};

	struct OhplDrObjectsIn
	{
		bool useOcclusionCulling, useDelayedOcclusion, debugRenderBuffers, occlusionTestLargeLights, sendToPostEffects,
			tonemapOk, isReflection;
		int skipPasses;
	};

	struct OhplDrFarPlane { float plane, top, bottom, right, left; };
	struct OhplDrShadowCfg { OhplVec2l high, medium, low; int jitterSize, jitterSamples; };
	struct OhplDrSsaoMips { OhplVec2l mipSize; int mips, levels; };
	struct OhplDrDebugTex { int kind, a, b; };

	enum { kOhplDrGlareVerts = 400, kOhplDrGlareIndices = 19 * 19 * 6 };

	extern "C"
	{
		OhplDrPlan* ohpl_dr_plan_copy_accum(const OhplDrCopyIn* in, const OhplDrFilmGrain* grain);
		OhplDrPlan* ohpl_dr_plan_setup_gbuffer(bool gbuffer64bit, int linearDepthIdx);
		OhplDrPlan* ohpl_dr_plan_pass(uint32_t kind);
		OhplDrPlan* ohpl_dr_plan_objects(OhplDrObjectsIn in);
		bool ohpl_dr_plan_next(OhplDrPlan*, OhplDrOp* out);
		void ohpl_dr_plan_feed_bool(OhplDrPlan*, bool v);
		void ohpl_dr_plan_feed_float(OhplDrPlan*, float v);
		void ohpl_dr_plan_feed_int(OhplDrPlan*, int32_t v);
		void ohpl_dr_plan_feed_vec3(OhplDrPlan*, float x, float y, float z);
		const float* ohpl_dr_plan_array(const OhplDrPlan*, int idx);
		void ohpl_dr_plan_film_grain(const OhplDrPlan*, OhplDrFilmGrain* out);
		bool ohpl_dr_plan_trail_consumed(const OhplDrPlan*);
		void ohpl_dr_plan_free(OhplDrPlan*);

		void ohpl_dr_film_grain_init(OhplDrFilmGrain* out);
		int ohpl_dr_cdiv(int a, int b);
		void ohpl_dr_far_plane(float fov, float aspect, float far, OhplDrFarPlane* out);
		void ohpl_dr_accum_quad_uv(OhplVec2l rtPos, OhplVec2l rtSize, OhplVec2f screen, bool rect, OhplVec2f* outMin,
								   OhplVec2f* outMax);
		void ohpl_dr_screen_position_vars(const OhplDrFarPlane* far, OhplVec2l rtSize, OhplVec2l rtPos, OhplVec2l gbufferSize,
										  float outToFar[4], float outInvSize[2]);
		void ohpl_dr_shadow_cfg(int resolution, int quality, bool shaderModel4, OhplDrShadowCfg* out);
		int ohpl_dr_dof_rows(int samples);
		bool ohpl_dr_dof_table(int samples, float screenX, float* out, size_t len);
		void ohpl_dr_ssao_mips(OhplVec2l size, OhplDrSsaoMips* out);
		bool ohpl_dr_glare_positions(float* out, size_t len);
		bool ohpl_dr_glare_indices(uint32_t* out, size_t len);
		void ohpl_dr_batch_sizes(int vertices, int indices, int lights, int* outVertices, int* outIndices);
		void ohpl_dr_debug_gbuffer_texture(int idx, const int shadowCounts[3], OhplDrDebugTex* out);
		size_t ohpl_dr_tonemap_defines(int combo, bool hpl35, int* out, size_t cap);
	}

	// G6b: SSAO, edge smoothing and light setup. Fields mirror rust/src/render/deferred_light*.rs.
	enum
	{
		kOhplDlFbLinearDepth = 0, kOhplDlFbSsao = 1, kOhplDlFbSsaoBlur = 2, kOhplDlFbEdgeLinearDepth = 3, kOhplDlFbCount = 4,
		kOhplDlTexGbufDepth = 0, kOhplDlTexGbufNormal = 1, kOhplDlTexLinearDepth = 2, kOhplDlTexSsaoScatter = 3, kOhplDlTexSsao = 4,
		kOhplDlTexSsaoBlur = 5, kOhplDlTexEdgeLinear = 6, kOhplDlTexEdgeTemp = 7, kOhplDlTexFalloff = 8, kOhplDlTexGobo = 9,
		kOhplDlTexShadow = 10, kOhplDlTexJitter = 11, kOhplDlTexSpotFalloff = 12, kOhplDlTexGbuffer0 = 13, kOhplDlGbufferMax = 5,
		kOhplDlTexCount = 18,
		kOhplDlProgSsaoUnpack = 0, kOhplDlProgSsaoRender = 1, kOhplDlProgSsaoBlur0 = 2, kOhplDlProgSsaoBlur1 = 3,
		kOhplDlProgEdgeUnpack = 4, kOhplDlProgEdgeRender = 5, kOhplDlProgDepthOnly = 6, kOhplDlProgLight = 7, kOhplDlProgCount = 8,
		kOhplDlShapePyramid = 3,
	};

	struct OhplDlFarIn { float plane, top, bottom, right, left; };
	struct OhplDlRect { int32_t x, y, w, h; };

	struct OhplDlSsaoIn
	{
		bool loaded, active, inBoxLight, boxListsEmpty, gbuffer64bit, onColorBuffer, unpackOk, renderOk, blurOk[2];
		OhplDlFarIn far;
		OhplVec2l gbufferDepthSize, ssaoSize;
		float scatterMul, scatterMin, scatterMax, depthDiffMul, skipEdge;
	};

	struct OhplDlEdgeIn
	{
		bool loaded, useEdgeSmooth, gbuffer64bit, renderOk;
		OhplDlFarIn far;
		OhplVec2l screen;
		float screenF[2];
	};

	struct OhplDlLightIn
	{
		int32_t kind;
		float radius, brightness, falloffPow, spotFalloffPow, translucency;
		float diffuse[4];
		bool castSpecular, castDiffuse, castShadows, hasGobo, goboSpecular, hasMaskBox;
		int32_t shadowRes;
		float shadowBlur;
		OhplVec3f maskCenter, maskSize;
		OhplMat4 world;
		OhplVec3f worldPos, bvMin, bvMax, bvCenter;
		float tanHalfFov, aspect, cosHalfFov, nearClip;
		OhplMat4 viewProj;
		OhplVec3f boxSize;
		int32_t boxPrio;
		uint64_t falloffMap, gobo, spotFalloff, id;
	};

	struct OhplDlLightOut
	{
		OhplMat4 render, transform;
		OhplDlRect clip;
		int32_t area;
		bool insideNearPlane, castShadows;
		int32_t shadowRes;
	};

	struct OhplDlLightState
	{
		OhplDlLightIn light;
		OhplMat4 viewRender, viewTransform, invView;
		bool castShadows, shadowTex;
		OhplVec2l shadowSize;
		bool jitter, hdr, underwater;
		float fogColor[4], fogStart, fogEnd, fogFalloffExp, distanceFade, farPlane;
		int32_t depthTextureIdx;
		bool programOk;
	};

	struct OhplDlFrustum
	{
		OhplMat4 view, proj;
		bool perspective;
		float fov, nearPlane, aspect;
		OhplVec3f origin;
	};

	struct OhplDlSetupIn
	{
		bool testLargeLights, useOcclusionCulling, renderShadows;
		OhplVec2l rtSize;
		float minLargeNormalizedArea;
		int32_t maxShadowRes;
		float shadowDistNone, shadowDistLow, shadowDistMedium, tanHalfFov;
		int32_t sampleLimit;
		OhplDlFrustum frustum;
	};

	struct OhplDlPair { uint64_t light; int32_t samples; };

	struct OhplDlInitLight
	{
		OhplDlLightIn light;
		bool insideNear, hasQuery;
		int32_t area;
	};

	struct OhplDlInitIn
	{
		OhplMat4 view;
		float lastFov, lastFarPlane, fov, farPlane, farLeft, farBottom, farRight, farTop;
		int32_t minLargeArea, sampleLimit;
	};

	static_assert(sizeof(OhplDrOp) == 72 && sizeof(OhplDlLightIn) == 320 && sizeof(OhplDlLightOut) == 156 && sizeof(OhplDlLightState) == 576 &&
				  sizeof(OhplDlSsaoIn) == 68 && sizeof(OhplDlEdgeIn) == 40 && sizeof(OhplDlSetupIn) == 196 && sizeof(OhplDlPair) == 16 &&
				  sizeof(OhplDlInitLight) == 328 && sizeof(OhplDlInitIn) == 104 && sizeof(OhplDlFrustum) == 156, "Rust struct layout");

	// G6c: light passes. Fields mirror rust/src/render/deferred_pass.rs.
	enum eOhplDr3P3
	{
		eOhplDr3P3_StencilActive, eOhplDr3P3_ClearStencil, eOhplDr3P3_ClearColorStencil, eOhplDr3P3_Stencil, eOhplDr3P3_StencilWriteMask,
		eOhplDr3P3_Cull, eOhplDr3P3_Scissor, eOhplDr3P3_ScissorOff, eOhplDr3P3_LightProgram, eOhplDr3P3_ShadowSetup, eOhplDr3P3_ShadowMap,
		eOhplDr3P3_PairBegin, eOhplDr3P3_PairEnd, eOhplDr3P3_StencilDirty, eOhplDr3P3_Log,
	};
	enum { kOhplDr3StencilFuncEqual = 0, kOhplDr3StencilFuncAlways = 1, kOhplDr3StencilOpKeep = 0, kOhplDr3StencilOpReplace = 1 };
	enum { kOhplDr3LogBegin = 0, kOhplDr3LogStencil = 1, kOhplDr3LogRender = 2, kOhplDr3LogEnd = 3 };
	enum { kOhplDr3ProgStencil = 8, kOhplDr3ProgBox0 = 9, kOhplDr3ProgBox1 = 10, kOhplDr3ProgCount = 11, kOhplDr3ShapeBox = 4, kOhplDr3ShapeFullscreenQuad = 5 };
	enum eOhplDr3Pass
	{
		eOhplDr3Pass_StencilBackScreenQuad, eOhplDr3Pass_StencilFrontRenderBack, eOhplDr3Pass_RenderBack, eOhplDr3Pass_BoxStencilFrontRenderBack,
		eOhplDr3Pass_BoxRenderBack,
	};

	struct OhplDr3PassLight
	{
		int32_t idx, kind;
		bool castShadows, insideNear;
		int32_t blendFunc;
		float diffuse[4];
		float brightness, distanceFade;
	};

	struct OhplDr3PassIn
	{
		int32_t pass;
		bool stencilDirty, depthCull, occlusionTest, boxSsao, hdr;
		float farLeft, farBottom, farRight, farTop, farPlane;
	};

	static_assert(sizeof(OhplDr3PassLight) == 40 && sizeof(OhplDr3PassIn) == 32, "Rust struct layout");

	extern "C"
	{
		OhplDrPlan* ohpl_dr3_plan_light_pass(const OhplDr3PassIn*, const OhplDr3PassLight* lights, size_t n);
		OhplDrPlan* ohpl_dr3_plan_render_lights(int32_t gbufferTextures);
	}

	extern "C"
	{
		OhplDrPlan* ohpl_dl_plan_ssao(const OhplDlSsaoIn*);
		OhplDrPlan* ohpl_dl_plan_edge_smooth(const OhplDlEdgeIn*);
		OhplDrPlan* ohpl_dl_plan_light_variables(const OhplDlLightState*);
		OhplDrPlan* ohpl_dl_plan_light_program(const OhplDlLightState*, uint32_t extraFlags, int32_t mode);
		OhplDrPlan* ohpl_dl_plan_shadow_map(int32_t gbufferTextures);
		OhplDrPlan* ohpl_dl_plan_setup_lights(const OhplDlSetupIn*, const OhplDlLightIn* lights, size_t n, const OhplDlPair* pairs, size_t np);
		OhplDrPlan* ohpl_dl_plan_init_lights(const OhplDlInitIn*, const OhplDlInitLight* lights, size_t n);
		bool ohpl_dl_plan_light_out(const OhplDrPlan*, int32_t idx, OhplDlLightOut* out);
		void ohpl_dl_plan_inv_view(const OhplDrPlan*, OhplMat4* out);
		size_t ohpl_dl_plan_list(const OhplDrPlan*, int32_t which, const uint32_t** out);
		int32_t ohpl_dl_plan_rendered(const OhplDrPlan*);
		int32_t ohpl_dl_light_shape(int32_t kind, int32_t quality);
	}

	// G6d: fog, depth of field, night vision, HPL3 SSAO upsample, directional and weighted box lights.
	// Fields mirror rust/src/render/deferred_{fx,fog,dir,box}.rs; every field is 4 bytes wide.
	enum eOhplDr4X
	{
		eOhplDr4X_TexGbuffer, eOhplDr4X_RedFromAlpha, eOhplDr4X_FilterSave, eOhplDr4X_FilterNearest, eOhplDr4X_FilterRestore,
		eOhplDr4X_DrawAccumQuad, eOhplDr4X_PassBegin, eOhplDr4X_PassEnd, eOhplDr4X_QuadPlain, eOhplDr4X_MatrixNull,
		eOhplDr4X_CullActive, eOhplDr4X_GenFogProgram, eOhplDr4X_EnsureFogNoise, eOhplDr4X_IntAsk, eOhplDr4X_FogAreaMatrix,
		eOhplDr4X_SetupFogData, eOhplDr4X_ClearFogData, eOhplDr4X_AskFogs, eOhplDr4X_Log, eOhplDr4X_DirShadowCreate,
		eOhplDr4X_AskDirShadowSize, eOhplDr4X_PolygonOffsetActive, eOhplDr4X_DepthClampActive, eOhplDr4X_PolygonOffset,
		eOhplDr4X_OcclusionPlanes, eOhplDr4X_AlphaLimitDefault, eOhplDr4X_DirShadowFb, eOhplDr4X_DirFrustum, eOhplDr4X_DirCasters,
		eOhplDr4X_DirSetFb, eOhplDr4X_DirRender, eOhplDr4X_TexBuffer, eOhplDr4X_SetVertexBufferNone, eOhplDr4X_EnvVisible,
		eOhplDr4X_EnvSetupBase, eOhplDr4X_EnvVertexBuffer, eOhplDr4X_EnvIteration, eOhplDr4X_FlareRetrieve, eOhplDr4X_FlareUpdate,
		eOhplDr4X_BlendRaw, eOhplDr4X_FlareTexture, eOhplDr4X_FlareIrisBuffer, eOhplDr4X_AskGlare, eOhplDr4X_GlareWrite,
		eOhplDr4X_GlareUpdateData, eOhplDr4X_SetGlareBuffer, eOhplDr4X_SetFb3,
	};
	enum { kOhplDr4PassDof, kOhplDr4PassFullScreenFog, kOhplDr4PassFog, kOhplDr4PassSsao };
	enum { kOhplDr4LogDirectional, kOhplDr4LogBoxWeighted };
	enum
	{
		kOhplDr4FogProg = 0, kOhplDr4FogTexNoise = 0, kOhplDr4FogTexSky = 1, kOhplDr4FogTexRefraction = 2,
		kOhplDr4NvProg = 0, kOhplDr4H3UpsampleProg = 0, kOhplDr4H3TexSsao = 0,
		kOhplDr4DofProgFocus = 0, kOhplDr4DofProgBlur = 1, kOhplDr4DofTexAccum = 0, kOhplDr4DofTexGauss = 1, kOhplDr4DofTexA = 2,
		kOhplDr4DofTexB = 3, kOhplDr4DofFbA = 0, kOhplDr4DofFbB = 1,
		kOhplDr4DirProg = 0, kOhplDr4DirTexShadow = 0, kOhplDr4DirTexJitter = 1,
		kOhplDr4BoxProgResolve = 6, kOhplDr4BoxFb = 0, kOhplDr4BoxTexWeight = 0, kOhplDr4BoxProgCount = 7,
	};

	struct OhplDr4NightIn
	{
		int32_t programOk, visible;
		float color[4];
		float brightness, falloff, radiusMax, farPlane;
		int32_t depthInNormalAlpha, linearDepthIdx;
	};

	struct OhplDr4Hpl3In
	{
		int32_t rendered;
		float farPlane;
		int32_t sizeDiv, texW, texH, depthInNormalAlpha, linearDepthIdx;
	};

	struct OhplDr4DofIn
	{
		int32_t screenW, screenH;
		float screenFx, screenFy, focusStart, focusEnd, falloff, nearPlane, farPlane;
		int32_t depthInNormalAlpha, linearDepthIdx;
	};

	struct OhplDr4FogWorld
	{
		int32_t hdr, depthInNormalAlpha, linearDepthIdx, fogActive, fogUnderwater, areaNum;
		float frustumFar, camY, time;
		int32_t rtW, rtH, secActive;
		float secStart, secEnd, secFalloff, secHorizon, secDensity, secHeightDensity;
		float secColor[4], secBrightness;
		int32_t skyTex, skyActive, skySeparate;
		float skyColor[4], skyBrightness, skyRot[3];
		int32_t skyW, skyH;
		OhplMat4 view, invView;
	};

	struct OhplDr4FogParams
	{
		int32_t isArea;
		float start, end, falloff, color[4], brightness, noiseStrength, noiseSize, turb[3];
		int32_t skyboxWanted, lighten, exp, height;
		float density, heightDensity, heightHorizon, bvMinY;
	};

	struct OhplDr4FogArea
	{
		int32_t insideNear, backsideInside, backsideOutside, underwater;
		OhplDr4FogParams params;
		OhplMat4 model;
	};

	struct OhplDr4DirIn
	{
		int32_t active;
		float brightness, diffuse[4];
		int32_t castSpecular, castDiffuse;
		float translucency;
		int32_t underwater, castShadows, renderShadows, hdr;
		float ambientSky[4], ambientGround[4], direction[3], shadowBlur, frustumFar, frustumNear, fov, aspect;
		OhplMat4 view, invView;
		int32_t fogActive;
		float fogColor[4], fogEnd, shadowDistance, sliceLogTerm, casterDistance, biasMul, slopeBiasMul;
		int32_t castersAffected;
		float mapBias, mapSlopeBias;
		int32_t shadowExists, shadowRes, depthClamp, sunCasterCull, jitterTex;
		float farLeft, farBottom, farRight, farTop, farPlane;
		int32_t gbufferTextures;
	};

	struct OhplDr4BoxLight
	{
		uint64_t id;
		int32_t idx, blendFunc, prio, spherical;
		float diffuse[4], brightness, fade, ambientSky[4], ambientGround[4], pos[3], probeOffset[3], size[3], falloff, bevel, weight,
			bands[9][3];
	};

	struct OhplDr4BoxIn
	{
		int32_t bufferOk, resolveOk, isReflection, progOk, instanceInputs, underwater, hdr;
		float frustumFar, origin[3];
		OhplMat4 view;
		float fogColor[4];
		int32_t gbufferTextures;
	};

	struct OhplDr4EnvIn
	{
		int32_t active, num, behindFocus;
		float focusStart, focusEnd, falloff;
		OhplMat4 proj;
	};

	struct OhplDr4FlareUpdateIn
	{
		int32_t isReflection, count;
		float frameTime;
		int32_t frame;
	};

	struct OhplDr4GlareIn
	{
		int32_t irisOk, glareOk, flareCount, multiCount, fogActive;
		float fogStart, fogEnd, fogFalloff, fogAlpha, cam[3], aspect;
	};

	struct OhplDr4GlareFlare { float vis; int32_t anyActive; };
	struct OhplDr4IrisFlare { int32_t texOk, blend; float vis, pos[3]; };

	struct OhplDr4Hpl3RenderIn
	{
		int32_t programsOk, isReflection, ssaoActive, sizeDiv;
		float fov, aspect, farPlane;
		int32_t texW, texH, mipW, mipH;
		float screenFx, screenFy;
		int32_t depthInNormalAlpha, linearDepthIdx, mipBuffers;
		float time;
		int32_t firstFrame;
		float frameTime;
		OhplMat4 prevView, view, proj;
	};

	struct OhplDr4Hpl3State
	{
		float time;
		int32_t firstFrame, rendered;
		OhplMat4 prevView;
	};

	static_assert(sizeof(OhplDr4Hpl3RenderIn) == 268 && sizeof(OhplDr4Hpl3State) == 76, "Rust struct layout");

	static_assert(sizeof(OhplDr4EnvIn) == 88 && sizeof(OhplDr4FlareUpdateIn) == 16 && sizeof(OhplDr4GlareIn) == 52 &&
				  sizeof(OhplDr4GlareFlare) == 8 && sizeof(OhplDr4IrisFlare) == 24, "Rust struct layout");

	static_assert(sizeof(OhplDr4NightIn) == 48 && sizeof(OhplDr4Hpl3In) == 28 && sizeof(OhplDr4DofIn) == 44 && sizeof(OhplDr4FogWorld) == 272 &&
				  sizeof(OhplDr4FogParams) == 88 && sizeof(OhplDr4FogArea) == 168 && sizeof(OhplDr4DirIn) == 344 &&
				  sizeof(OhplDr4BoxLight) == 240 && sizeof(OhplDr4BoxIn) == 128, "Rust struct layout");

	extern "C"
	{
		OhplDrPlan* ohpl_dr4_plan_fog_depth(bool depthInNormalAlpha, int32_t linearIdx, bool bind, int32_t unit);
		OhplDrPlan* ohpl_dr4_plan_night_vision(const OhplDr4NightIn*);
		OhplDrPlan* ohpl_dr4_plan_hpl3_ssao(const OhplDr4Hpl3In*);
		OhplDrPlan* ohpl_dr4_plan_dof(const OhplDr4DofIn*);
		OhplDrPlan* ohpl_dr4_plan_full_screen_fog(const OhplDr4FogWorld*, const OhplDr4FogParams*);
		OhplDrPlan* ohpl_dr4_plan_fog(const OhplDr4FogWorld*);
		void ohpl_dr4_feed_fogs(OhplDrPlan*, const OhplDr4FogArea* areas, size_t n);
		OhplDrPlan* ohpl_dr4_plan_directional(const OhplDr4DirIn*);
		OhplDrPlan* ohpl_dr4_plan_box_weighted(const OhplDr4BoxIn*, const OhplDr4BoxLight* lights, size_t n);
		bool ohpl_dr4_plan_handled(const OhplDrPlan*);
		OhplDrPlan* ohpl_dr4_plan_env(const OhplDr4EnvIn*);
		OhplDrPlan* ohpl_dr4_plan_update_flares(const OhplDr4FlareUpdateIn*);
		OhplDrPlan* ohpl_dr4_plan_glare(const OhplDr4GlareIn*, const OhplDr4GlareFlare* flares, size_t nflares,
										const OhplDr4IrisFlare* iris, size_t niris, const float* pos, size_t npos);
		void ohpl_dr4_feed_floats(OhplDrPlan*, const float* v, size_t n);
		OhplDrPlan* ohpl_dr4_plan_hpl3_render(const OhplDr4Hpl3RenderIn*);
		void ohpl_dr4_plan_hpl3_state(const OhplDrPlan*, OhplDr4Hpl3State* out);
	}

	// G6e: object passes (Z, G-buffer, decals, illumination, translucent, reflection, sky box).
	// Ids and layouts mirror rust/src/render/deferred_obj.rs. Object ops: op.b = mode/argument, op.c = list, op.d = index,
	// op.e = texture unit; ask ops that name only an object carry the index in op.b.
	enum eOhplDr5X
	{
		eOhplDr5X_ObjProgram = 100, eOhplDr5X_ObjTextures, eOhplDr5X_ObjMatrixPtr, eOhplDr5X_ObjVbuf, eOhplDr5X_ObjDrawMat,
		eOhplDr5X_ObjZ, eOhplDr5X_ObjTexUnit, eOhplDr5X_AskObjBlend, eOhplDr5X_SetAlphaLimit, eOhplDr5X_SkyMatrix,
		eOhplDr5X_SkyTexture, eOhplDr5X_SkyVbuf, eOhplDr5X_Quad2, eOhplDr5X_TexReflection, eOhplDr5X_SetWorldFog,
		eOhplDr5X_AskWorldFog, eOhplDr5X_AskCompiledFog, eOhplDr5X_ObjCompile, eOhplDr5X_AskTransInfo, eOhplDr5X_AskFogVis,
		eOhplDr5X_SetTempFog, eOhplDr5X_AskUpdateGraphics, eOhplDr5X_AskRetrieveQuery, eOhplDr5X_ObjModelMatrix,
		eOhplDr5X_SetMatrixCtx, eOhplDr5X_AskPlaneVisible, eOhplDr5X_WaitRetrieveQueries, eOhplDr5X_AskClipRectObj,
		eOhplDr5X_AskBvMinMax, eOhplDr5X_FlatProjectionSize, eOhplDr5X_QuadClip, eOhplDr5X_CopyRefractionRect,
		eOhplDr5X_CopyRefractionFull, eOhplDr5X_SetDof, eOhplDr5X_AskReflInfo, eOhplDr5X_AskPlaneBv, eOhplDr5X_AskReflCleared,
		eOhplDr5X_ReflSetFb, eOhplDr5X_ReflSetCleared, eOhplDr5X_ReflLog, eOhplDr5X_ReflBegin, eOhplDr5X_AskReflTarget,
		eOhplDr5X_AskReflSurface, eOhplDr5X_ReflFrustum, eOhplDr5X_ReflResetPlanes, eOhplDr5X_ReflAddPlane, eOhplDr5X_ReflScissor,
		eOhplDr5X_ReflTarget, eOhplDr5X_ReflRender, eOhplDr5X_ReflEnd,
	};
	enum
	{
		kOhplDr5PassEarlyZ = 16, kOhplDr5PassGBuffer, kOhplDr5PassGBufferContent, kOhplDr5PassDecals, kOhplDr5PassIllumination,
		kOhplDr5PassSkyBox, kOhplDr5PassReflectionContent, kOhplDr5PassTranslucent,
	};
	enum { kOhplDr5LogSetup, kOhplDr5LogClearBegin, kOhplDr5LogClearEnd, kOhplDr5LogClipLimits, kOhplDr5LogScissor };
	enum { kOhplDr5AlphaTrans = 1, kOhplDr5ChannelA = 3, kOhplDr5CompColorAndDepth = 1 };
	// words of the AskTransInfo answer
	enum
	{
		kOhplDr5InfoAffectedByDof, kOhplDr5InfoHasRefraction, kOhplDr5InfoHasWorldReflection, kOhplDr5InfoAffectedByFog,
		kOhplDr5InfoHasTranslucentIllumination, kOhplDr5InfoUseRefractionEdgeCheck, kOhplDr5InfoDepthTest, kOhplDr5InfoBlend,
		kOhplDr5InfoRenderType, kOhplDr5InfoBvCenter, kOhplDr5InfoBvRadius = 12, kOhplDr5InfoWords,
	};

	struct OhplDr5DecalIn { int32_t count; bool depthInNormalAlpha; };
	struct OhplDr5SkyIn { bool worldOk, active; float farPlane; OhplVec3f rotation, origin; };
	struct OhplDr5Camera { OhplDlFrustum f; float farPlane; OhplVec3f forward; };
	struct OhplDr5TransIn
	{
		int32_t count, debugSkip, dofPass;
		bool depthInNormalAlpha, hdr, isReflection, fogActive, underwater, secondary, renderWorldReflection, clipReflectionScreenRect,
			settingsLog, occlusionTestLargeLights;
		int32_t fogAreaNum;
		float dofStart, dofEnd, dofFalloff;
		OhplVec2l rtSize;
		OhplDr5Camera camera;
	};
	static_assert(sizeof(OhplDr5DecalIn) == 8 && sizeof(OhplDr5SkyIn) == 32 && sizeof(OhplDr5Camera) == 172 && sizeof(OhplDr5TransIn) == 220,
				  "Rust struct layout");

	extern "C"
	{
		OhplDrPlan* ohpl_dr5_plan_z(int32_t n);
		OhplDrPlan* ohpl_dr5_plan_gbuffer(int32_t n);
		OhplDrPlan* ohpl_dr5_plan_illumination(int32_t n);
		OhplDrPlan* ohpl_dr5_plan_gbuffer_content(float w, float h, int32_t numTextures);
		OhplDrPlan* ohpl_dr5_plan_reflection_content(float w, float h, int32_t div);
		OhplDrPlan* ohpl_dr5_plan_decals(const OhplDr5DecalIn*);
		OhplDrPlan* ohpl_dr5_plan_skybox(const OhplDr5SkyIn*);
		OhplDrPlan* ohpl_dr5_plan_translucent(const OhplDr5TransIn*);
		void ohpl_dr5_feed_words(OhplDrPlan*, const uint32_t* v, size_t n);
	}
}

#endif
