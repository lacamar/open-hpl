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
	enum { kOhplDrBlendNone = 0, kOhplDrBlendAlpha = 1, kOhplDrBlendAdd = 2, kOhplDrBlendMul = 3, kOhplDrAlphaSolid = 0,
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
}

#endif
