/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HPL_RENDERER_DEFERRED_H
#define HPL_RENDERER_DEFERRED_H

#include "graphics/Renderer.h"

namespace hpl {

	//---------------------------------------------

	//////////////////////////////////////////////////////////////////////////////
	// GENERAL RENDERER INFO - DEFERRRED RENDERER
	//
	// Important to note is that this renderer does not require depth or stencil buffer
	// for the render target, but always use internal stuff for rendering.
	//
	//
	//////////////////////////////////////////////////////////////////////////////

    //---------------------------------------------
	
	class iFrameBuffer;
	class iDepthStencilBuffer;
	class iTexture;
	class iLight;
	class cSubMeshEntity;
	
	//---------------------------------------------

	enum eDeferredLightList
	{
		eDeferredLightList_StencilBack_ScreenQuad,		//First draw back to stencil, then draw light as full screen quad
		eDeferredLightList_StencilFront_RenderBack,		//First draw front to stencil, then draw back facing as light.
		eDeferredLightList_RenderBack,					//Draw back facing as light.
		eDeferredLightList_Batches,						//Draw many lights as batch. Spotlights not allowed!

		eDeferredLightList_Box_StencilFront_RenderBack,
		eDeferredLightList_Box_RenderBack,

		eDeferredLightList_LastEnum
	};

	//---------------------------------------------

	enum eDeferredShapeQuality
	{
		eDeferredShapeQuality_Low,
		eDeferredShapeQuality_Medium,
		eDeferredShapeQuality_High,
		eDeferredShapeQuality_LastEnum,
	};

	//---------------------------------------------

	enum eDeferredGBuffer
	{
		eDeferredGBuffer_32Bit,
		eDeferredGBuffer_64Bit,
		eDeferredGBuffer_LastEnum,
	};

	//---------------------------------------------

	enum eDeferredSSAO
	{
		eDeferredSSAO_InBoxLight,
		eDeferredSSAO_OnColorBuffer,
		eDeferredSSAO_LastEnum,
	};

	//---------------------------------------------

	enum eGBufferComponents
	{
		eGBufferComponents_Full,
		eGBufferComponents_ColorAndDepth,
		eGBufferComponents_Color,
		eGBufferComponents_Depth,
		eGBufferComponents_Normals,
		eGBufferComponents_LinearDepth,
		eGBufferComponents_LastEnum,
	};
	
	//---------------------------------------------
	
	class cDeferredLight
	{
	public:
		cDeferredLight() : mpShadowTexture(NULL), mbCastShadows(false){}

		iLight *mpLight;
		cRect2l mClipRect;
		int mlArea;
		cMatrixf m_mtxViewSpaceRender;
		cMatrixf m_mtxViewSpaceTransform;
		bool mbInsideNearPlane;
		iOcclusionQuery *mpQuery;

		iTexture *mpShadowTexture;
		bool mbCastShadows;
		eShadowMapResolution mShadowResolution;
	};

	//---------------------------------------------
	
	class cRendererDeferred : public  iRenderer
	{
	public:
		cRendererDeferred(cGraphics *apGraphics,cResources* apResources);
		~cRendererDeferred();
		
		bool LoadData();
		void DestroyData();

		iTexture* GetPostEffectTexture();

		iTexture* GetGbufferTexture(int alIdx);
		iFrameBuffer* GetGBufferFrameBuffer(eGBufferComponents aComponents);
		//iTexture *GetShadowTexture(eShadowMapResolution aQuality){ return mpShadowTexture[aQuality]; }

		iDepthStencilBuffer* GetDepthStencilBuffer(){ return mpDepthStencil[0];}

		iFrameBuffer *GetAccumBuffer(){ return mpAccumBuffer;}

		iTexture* GetRefractionTexture(){ return mpRefractionTexture;}
		iTexture* GetSceneDepthTexture(){ return GetGbufferTexture(mbDepthInNormalAlpha ? 1 : 2);}
		iTexture* GetReflectionTexture(){ return mpReflectionTexture;}

		//Static properties. Must be set before renderer data load.
		static void SetGBufferType(eDeferredGBuffer aType){ mGBufferType = aType; }
		static void SetGBufferTextureType(eTextureType aType){ mGBufferTextureType = aType; }
		static void SetDepthInNormalAlpha(bool abX){ mbDepthInNormalAlpha = abX; }
		static eDeferredGBuffer GetGBufferType(){ return mGBufferType; }

		static void SetNumOfGBufferTextures(int alNum){ mlNumOfGBufferTextures = alNum;}
		static int GetNumOfGBufferTextures(){ return mlNumOfGBufferTextures;}

		static void SetDepthCullLights(bool abX){ mbDepthCullLights = abX;}
		static int GetDepthCullLights(){ return mbDepthCullLights;}

		static void SetSSAOLoaded(bool abX){ mbSSAOLoaded = abX;}
		static void SetHpl3SSAO(bool abX){ mbHpl3SSAO = abX;}
		static void SetSSAONumOfSamples(int alX){ mlSSAONumOfSamples = alX;}
		static void SetSSAOBufferSizeDiv(int alX){ mlSSAOBufferSizeDiv = alX;}
		static void SetSSAOScatterLengthMul(float afX){ mfSSAOScatterLengthMul = afX;}
		static void SetSSAOScatterLengthMin(float afX){ mfSSAOScatterLengthMin = afX;}
		static void SetSSAOScatterLengthMax(float afX){ mfSSAOScatterLengthMax = afX;}
		static void SetSSAOType(eDeferredSSAO aType) {mSSAOType = aType;}
		static void SetSSAODepthDiffMul(float afX){ mfSSAODepthDiffMul = afX;}
		static void SetSSAOSkipEdgeLimit(float afX){mfSSAOSkipEdgeLimit = afX;}
		
		static bool GetSSAOLoaded(){ return mbSSAOLoaded;}
		static int GetSSAONumOfSamples(){ return mlSSAONumOfSamples;}
		static int GetSSAOBufferSizeDiv(){ return mlSSAOBufferSizeDiv;}
		static float GetSSAOScatterLengthMul(){ return mfSSAOScatterLengthMul;}
		static float GetSSAOScatterLengthMin(){ return mfSSAOScatterLengthMin;}
		static float GetSSAOScatterLengthMax(){ return mfSSAOScatterLengthMax;}
		static eDeferredSSAO GetSSAOType() {return mSSAOType;}
		static float GetSSAODepthDiffMul(){ return mfSSAODepthDiffMul;}
		static float GetSSAOSkipEdgeLimit(){ return mfSSAOSkipEdgeLimit;}
		
		static void SetEdgeSmoothLoaded(bool abX){ mbEdgeSmoothLoaded = abX;}
		static bool GetEdgeSmoothLoaded(){ return mbEdgeSmoothLoaded;}

		static void SetOcclusionTestLargeLights(bool abX){ mbOcclusionTestLargeLights = abX;}
		static bool GetOcclusionTestLargeLights(){ return mbOcclusionTestLargeLights;}

		static int mlDebugSkipPasses;
		static int mlDebugSkipTranslucent;
		static void SetHdr(bool abX){ mbHdr = abX;}
		static void SetShadowDistanceNone(float afX){ mfDefaultShadowDistanceNone = afX;}
		static bool GetHdr(){ return mbHdr;}
		static cColor GetFogRenderColor(const cColor& aCol, float afBrightness){ return mbHdr ? cColor(aCol.r*aCol.r*afBrightness, aCol.g*aCol.g*afBrightness, aCol.b*aCol.b*afBrightness, aCol.a) : aCol; }
		static void SetColorGradingTexture(iTexture *apTex){ mpColorGradingTexture = apTex;}
		static void SetToneMapping(float afKey, float afExposure, float afWhiteCut, float afGamma){ mfToneMapKey = afKey; mfToneMapExposure = afExposure; mfToneMapWhiteCut = afWhiteCut; mfToneMapGamma = afGamma;}
		static void SetBloom(bool abActive, float afBrightPass, float afWidth, const cColor& aTint){ mbBloom = abActive; mfBloomBrightPass = afBrightPass; mfBloomWidth = afWidth; mBloomTint = aTint;}
		static void SetFilmGrain(iTexture *apNoise, float afIntensity){ mpFilmGrainNoise = apNoise; mfFilmGrainIntensity = afIntensity;}
		static void SetToneMapSRGB(bool abX){ mbToneMapSRGB = abX;}

		static void SetDebugRenderFrameBuffers(bool abX){ mbDebugRenderFrameBuffers = abX;}
		static bool GetDebugRenderFrameBuffers(){ return mbDebugRenderFrameBuffers;}

		iGpuProgram* GetSkyBoxProgram(){ return mpSkyBoxProgram; }

		iTexture* GetDebugShadowTexture(iLight *apLight){ for(int r=0; r<eShadowMapResolution_LastEnum; ++r) for(size_t i=0; i<mvShadowMapData[r].size(); ++i) if(mvShadowMapData[r][i]->mCache.mpLight == apLight) return mvShadowMapData[r][i]->mpTexture; return NULL; }
		iTexture* GetDebugGBufferTexture(int alIdx);

	private:
		void DrawAccumulationQuad();
		void CopyToFrameBuffer();
		void CopyAccumTo(iFrameBuffer *apTarget);
		void SetupRenderList();
		void RenderObjects();

		void SetupGBuffer();

		void SetupRenderVariables();

		void RenderZ();
		void RenderDynamicZTemp();
		void RenderGbuffer();
		void RenderSSAO();
		void RenderHpl3SSAO();
		void ApplyHpl3SSAO();
		void RenderEdgeSmooth();
		void RenderDeferredSkyBox();
		
		void SetupLightsAndRenderQueries();
		void InitLightRendering();
		void RenderLights();
		void RenderLights_StencilBack_ScreenQuad();
		void RenderLights_StencilFront_RenderBack();
		void RenderLights_RenderBack();
		void RenderLights_Batches();
		void RenderLights_Box_StencilFront_RenderBack();
		void RenderLights_Box_RenderBack();
		bool RenderLights_BoxWeighted();
		void RenderLights_Directional();
        
		void RenderIllumination();

		void RenderReflection(iRenderable *apObject);
		void RenderSubMeshEntityReflection(cSubMeshEntity *pReflectionObject);

		void RenderDecals();
		void SetFogDepthTexture(bool abBind, int alUnit=0);
		bool DepthOfFieldIsActive();
		bool IsBehindDepthOfFieldFocus(iRenderable *apObject);
		void RenderDepthOfField();
		void RenderFullScreenFog();
		void RenderFog();
		iGpuProgram* SetupFogProgram(cFogArea *apFogArea, tFlag alFlags, bool abUnderwaterPass);
		void RenderTranslucent(int alDofPass=0);
		void RenderEnvironmentParticles(bool abBehindFocus);
		
		void SetAccumulationBuffer();
		void SetGBuffer(eGBufferComponents aComponents);
		iTexture* GetBufferTexture(int alIdx);
		
		
		void RenderGbufferContent();
		void RenderReflectionContent();

		////////////////
		//Draw helpers
		void RenderBoxLight(cDeferredLight* apLightData);
		
		////////////////
		//Misc Helpers
		void RenderLightShadowMap(cDeferredLight* apLightData);
		void SetupLightProgramVariables(iGpuProgram *apProgram, cDeferredLight* apLightData);
		iGpuProgram* SetupProgramAndTextures(cDeferredLight* apLightData, tFlag alExtraFlags);
		iVertexBuffer* GetLightShape(iLight *apLight, eDeferredShapeQuality aQuality);
		
		
		iVertexBuffer *mpShapeSphere[eDeferredShapeQuality_LastEnum];
		iVertexBuffer *mpShapePyramid;
		
		iVertexBuffer *mpBatchBuffer;
		int mlMaxBatchLights;
		int mlMaxBatchVertices;
		int mlMaxBatchIndices;
		
		iVertexBuffer *mpFullscreenLightQuad;
		float mfLastFrustumFOV;
		float mfLastFrustumFarPlane;
		
		float mfFarPlane;
		float mfFarBottom;
		float mfFarTop;
		float mfFarLeft;
		float mfFarRight;

		cMatrixf m_mtxInvView;

		float mfMinLargeLightNormalizedArea;
		int mlMinLargeLightArea;

		float mfMinRenderReflectionNormilzedLength;
		
		float mfShadowDistanceMedium;
		float mfShadowDistanceLow;
		float mfShadowDistanceNone;

		bool mbStencilNeedClearing;
		cRect2l mStencilDirtyRect;
		
		iFrameBuffer *mpGBuffer[2][eGBufferComponents_LastEnum]; //[2] = reflection or not
		
		iFrameBuffer *mpAccumBuffer;
		iFrameBuffer *mpReflectionBuffer;
		
		iTexture *mpGBufferTexture[2][4];	//[2] = reflection or not
		iTexture *mpAccumBufferTexture;
		iTexture *mpRefractionTexture;
		iTexture *mpReflectionTexture;
		iDepthStencilBuffer* mpDepthStencil[2];	//[2] = reflection or not
		

		bool mbReflectionTextureCleared;

		/*iTexture *mpShadowTempDiffTexture[eShadowMapResolution_LastEnum];
		iTexture *mpShadowTexture[eShadowMapResolution_LastEnum];
		iFrameBuffer *mpShadowBuffer[eShadowMapResolution_LastEnum];
		cShadowMapLightCache mShadowMapCacheData[eShadowMapResolution_LastEnum];*/
		iTexture *mpShadowJitterTexture;
		int mlShadowJitterSize;
		int mlShadowJitterSamples;

		iTexture *mpLinearDepthTexture;
		iTexture *mpSSAOTexture;
		iTexture *mpSSAOBlurTexture;
		iTexture *mpSSAOScatterDisk;
		iTexture *mpEdgeSmooth_LinearDepthTexture;
		iTexture *mpEdgeSmooth_TempAccum;

		iFrameBuffer *mpLinearDepthBuffer;
		iFrameBuffer *mpSSAOBuffer;
		iFrameBuffer *mpSSAOBlurBuffer;
		iFrameBuffer *mpEdgeSmooth_LinearDepthBuffer;

		iGpuProgram *mpUnpackDepthProgram;
		iGpuProgram *mpSSAOBlurProgram[2];
		iGpuProgram *mpSSAORenderProgram;
		iGpuProgram *mpEdgeSmooth_UnpackDepthProgram;
		iGpuProgram *mpEdgeSmooth_RenderProgram;

		iGpuProgram *mpFxaaProgram;
		iTexture *mpH3SSAOTexture[3];
		iTexture *mpH3SSAOMipTexture;
		iFrameBuffer *mpH3SSAOBuffer[3];
		std::vector<iFrameBuffer*> mvH3SSAOMipBuffers;
		iGpuProgram *mpH3SSAODownsampleProgram;
		iGpuProgram *mpH3SSAORenderProgram;
		iGpuProgram *mpH3SSAOBlurProgram;
		iGpuProgram *mpH3SSAOTemporalProgram;
		iGpuProgram *mpH3SSAOUpsampleProgram;
		float mfH3SSAOTime;
		bool mbH3SSAOFirstFrame;
		bool mbH3SSAORendered;
		cMatrixf m_mtxH3SSAOPrevView;
		iGpuProgram *mpDofFocusProgram;
		iGpuProgram *mpDofBlurProgram;
		iTexture *mpDofGaussTexture;
		void RenderBloom();
		void RandomizeFilmGrain();
		iGpuProgram* GetToneMapProgram(int alCombo);

		iGpuProgram *mpToneMapProgram;
		iGpuProgram *mpToneMapPrograms[16];//1=grading, 2=bloom, 4=film grain, 8=sRGB
		iGpuProgram *mpBloomBrightPassProgram;
		iGpuProgram *mpBloomBlurProgram[2];//0=vertical, 1=horizontal
		int mlBloomBlurSamples;
		float mfFilmGrainT = -1;
		float mvFilmGrainTransform[2][4];

		std::vector<cDeferredLight*> mvTempDeferredLights;
		std::vector<cDeferredLight*> mvSortedLights[eDeferredLightList_LastEnum];

		iGpuProgram *mpSkyBoxProgram; 
		iGpuProgram *mpLightStencilProgram;
		iGpuProgram *mpLightBoxProgram[2];//1=SSAO used, 0=no SSAO
		iGpuProgram *mpBoxWeightedProgram[3][2];
		iGpuProgram *mpBoxResolveProgram;
		iTexture *mpBoxWeightTexture;
		iFrameBuffer *mpBoxWeightBuffer;

		cProgramComboManager* mpFogProgramManager;
		iTexture *mpFogNoiseTexture = NULL;
		
		cMatrixf m_mtxTempLight;
		
		//Static setting variables
		static bool mbHdr;
		static float mfDefaultShadowDistanceNone;
		static float mfToneMapKey;
		static float mfToneMapExposure;
		static float mfToneMapWhiteCut;
		static float mfToneMapGamma;
		static iTexture *mpColorGradingTexture;
		static bool mbBloom;
		static float mfBloomBrightPass;
		static float mfBloomWidth;
		static cColor mBloomTint;
		static iTexture *mpFilmGrainNoise;
		static float mfFilmGrainIntensity;
		static bool mbToneMapSRGB;
		static eDeferredGBuffer mGBufferType;
		static eTextureType mGBufferTextureType;
		static bool mbDepthInNormalAlpha;
		static int mlNumOfGBufferTextures;
		static bool mbDepthCullLights;

		static bool mbSSAOLoaded;
		static bool mbHpl3SSAO;
		static int mlSSAONumOfSamples;
		static float mfSSAOScatterLengthMul;
		static float mfSSAOScatterLengthMin;
		static float mfSSAOScatterLengthMax;
		static float mfSSAODepthDiffMul;
		static float mfSSAOSkipEdgeLimit;
		static eDeferredSSAO mSSAOType;
		static int mlSSAOBufferSizeDiv;

		static bool mbEdgeSmoothLoaded;
		static bool mbEnableParallax;

		static bool mbDebugRenderFrameBuffers;
		static bool mbOcclusionTestLargeLights;

	};

	//---------------------------------------------

};
#endif // HPL_RENDERER_DEFERRED_H
