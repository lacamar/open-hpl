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
	};

	enum eOhplDrState { eOhplDrState_DepthTest, eOhplDrState_DepthWrite, eOhplDrState_DepthFunc, eOhplDrState_Blend,
						eOhplDrState_Alpha, eOhplDrState_Channel };
	enum { kOhplDrBlendNone = 0, kOhplDrBlendAlpha = 1, kOhplDrAlphaSolid = 0, kOhplDrChannelRgba = 0, kOhplDrChannelNone = 1,
		   kOhplDrFuncEqual = 0, kOhplDrFuncLessOrEqual = 1 };
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
		float f[4];
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
}

#endif
