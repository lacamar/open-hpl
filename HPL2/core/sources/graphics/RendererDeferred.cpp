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

#include "graphics/RendererDeferred.h"
#include "graphics/RustRendererDeferred.h"
#include "graphics/MaterialType_BasicTranslucent.h"

#include "math/Math.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/PreprocessParser.h"

#include "graphics/Graphics.h"
#include "graphics/Texture.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Renderable.h"
#include "graphics/RenderList.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/OcclusionQuery.h"
#include "graphics/TextureCreator.h"
#include "graphics/Bitmap.h"

#include "resources/Resources.h"
#include "resources/FileSearcher.h"
#include "resources/TextureManager.h"
#include "resources/GpuShaderManager.h"
#include "resources/MeshManager.h"
#include "graphics/GPUShader.h"
#include "graphics/GPUProgram.h"

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/RenderableContainer.h"
#include "scene/Light.h"
#include "scene/LightSpot.h"
#include "scene/LightBox.h"
#include "scene/LightDirectional.h"
#include "scene/FogArea.h"
#include "scene/EnvironmentParticles.h"
#include "scene/LensFlare.h"
#include "scene/MeshEntity.h"

#include <algorithm>
#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// STATIC VARIABLES
	//////////////////////////////////////////////////////////////////////////

	eDeferredGBuffer cRendererDeferred::mGBufferType = eDeferredGBuffer_32Bit;
	eTextureType cRendererDeferred::mGBufferTextureType = eTextureType_Rect;
	bool cRendererDeferred::mbDepthInNormalAlpha = false;
	int cRendererDeferred::mlNumOfGBufferTextures = 4;
	int cRendererDeferred::mlLinearDepthTextureIdx = 2;
	bool cRendererDeferred::mbDepthCullLights = true;

	bool cRendererDeferred::mbSSAOLoaded = false;
	bool cRendererDeferred::mbHpl3SSAO = false;
	int cRendererDeferred::mlSSAONumOfSamples = 8;
	int cRendererDeferred::mlSSAOBufferSizeDiv = 2;
	int cRendererDeferred::mlDepthOfFieldSampleNum = 16;
	float cRendererDeferred::mfSSAOScatterLengthMul = 0.2f;
	float cRendererDeferred::mfSSAOScatterLengthMin = 0.015f;
	float cRendererDeferred::mfSSAOScatterLengthMax = 0.13f;
	float cRendererDeferred::mfSSAODepthDiffMul = 1.5f;
	float cRendererDeferred::mfSSAOSkipEdgeLimit = 3.0f;
	eDeferredSSAO cRendererDeferred::mSSAOType = eDeferredSSAO_OnColorBuffer;
	bool cRendererDeferred::mbEdgeSmoothLoaded = false;
	
	//debug
	bool cRendererDeferred::mbOcclusionTestLargeLights = true;
	bool cRendererDeferred::mbDebugRenderFrameBuffers = false;
	int cRendererDeferred::mlDebugSkipPasses = 0;
	int cRendererDeferred::mlDebugSkipTranslucent = -1;
	bool cRendererDeferred::mbHdr = false;
	float cRendererDeferred::mfDefaultShadowDistanceNone = 40;
	float cRendererDeferred::mfToneMapKey = 0.5f;
	float cRendererDeferred::mfToneMapExposure = 1.0f;
	float cRendererDeferred::mfToneMapWhiteCut = 3.5f;
	float cRendererDeferred::mfToneMapGamma = 2.2f;
	float cRendererDeferred::mfImageTrailAlpha = 0;
	iTexture *cRendererDeferred::mpColorGradingTexture = NULL;
	iTexture *cRendererDeferred::mpColorGradingTarget = NULL;
	float cRendererDeferred::mfColorGradingBlend = 0;
	std::vector<std::pair<iTexture*,float> > cRendererDeferred::mvColorGradingLayers;
	bool cRendererDeferred::mbBloom = false;
	float cRendererDeferred::mfBloomBrightPass = 0.75f;
	float cRendererDeferred::mfBloomWidth = 128;
	cColor cRendererDeferred::mBloomTint = cColor(1, 1);
	iTexture *cRendererDeferred::mpFilmGrainNoise = NULL;
	float cRendererDeferred::mfFilmGrainIntensity = 1;
	bool cRendererDeferred::mbToneMapSRGB = false;


	//////////////////////////////////////////////////////////////////////////
	// DEBUG DEFINES
	//////////////////////////////////////////////////////////////////////////

	//#define kDebug_RenderLightData

	//////////////////////////////////////////////////////////////////////////
	// LIGHT PROGRAM COMBOS
	//////////////////////////////////////////////////////////////////////////

	enum eDefferredProgramMode
	{
		eDefferredProgramMode_Lights,
		eDefferredProgramMode_Misc,
		eDefferredProgramMode_LastEnum
	};

	#define eFeature_Light_LightShapes		eFlagBit_0
	#define eFeature_Light_Batching			eFlagBit_1
	#define eFeature_Light_Specular			eFlagBit_2
	#define eFeature_Light_SpotLight		eFlagBit_3
	#define eFeature_Light_Gobo				eFlagBit_4
	#define eFeature_Light_DivideInFrag		eFlagBit_5
	#define eFeature_Light_ShadowMap		eFlagBit_6
	#define eFeature_Light_BoxMask			eFlagBit_7
	#define eFeature_Light_GoboSpecular		eFlagBit_8
	#define eFeature_Light_GoboTypeSpecular	eFlagBit_9
	#define eFeature_Light_Translucency		eFlagBit_10
	#define eFeature_Light_UnderwaterFog	eFlagBit_11
	#define eFeature_Light_Directional		eFlagBit_12
	
	#define kLightFeatureNum 13

	cProgramComboFeature gvLightFeatureVec[] =
	{
		cProgramComboFeature("UseDeferredLightShapes", kPC_VertexBit),
		cProgramComboFeature("UseBatching", kPC_FragmentBit | kPC_VertexBit),
		cProgramComboFeature("UseSpecular", kPC_FragmentBit),
		cProgramComboFeature("LightType_Spot", kPC_FragmentBit | kPC_VertexBit),
		cProgramComboFeature("UseGobo", kPC_FragmentBit),
		cProgramComboFeature("DivideInFrag", kPC_FragmentBit | kPC_VertexBit),
		cProgramComboFeature("UseShadowMap", kPC_FragmentBit),
		cProgramComboFeature("BoxMask", kPC_FragmentBit),
		cProgramComboFeature("GoboSpecFlag", kPC_FragmentBit, eFeature_Light_Gobo),
		cProgramComboFeature("GoboType_Specular", kPC_FragmentBit, eFeature_Light_Gobo),
		cProgramComboFeature("UseTranslucency", kPC_FragmentBit),
		cProgramComboFeature("UseUnderwaterFog", kPC_FragmentBit),
		cProgramComboFeature("LightType_Directional", kPC_FragmentBit),
	};

	//////////////////////////////////////////////////////////////////////////
	// FOG PROGRAM COMBOS
	//////////////////////////////////////////////////////////////////////////
	
	#define eFeature_FogArea_OutsideBox		eFlagBit_0
	#define eFeature_FogArea_Backside		eFlagBit_1
	#define eFeature_FogArea_Underwater		eFlagBit_2
	#define eFeature_FogArea_Noise			eFlagBit_3
	#define eFeature_FogArea_Skybox			eFlagBit_4
	#define eFeature_FogArea_Secondary		eFlagBit_5
	#define eFeature_FogArea_FogArea		eFlagBit_6
	#define eFeature_FogArea_Lighten		eFlagBit_7
	#define eFeature_FogArea_SeparateSkybox	eFlagBit_8
	
	#define kFogAreaFeatureNum 9

	cProgramComboFeature gvFogAreaFeatureVec[] =
	{
		cProgramComboFeature("OutsideBox", kPC_FragmentBit | kPC_VertexBit),
		cProgramComboFeature("UseBackside", kPC_FragmentBit | kPC_VertexBit),
		cProgramComboFeature("UseUnderwaterFog", kPC_FragmentBit),
		cProgramComboFeature("UseNoise", kPC_FragmentBit),
		cProgramComboFeature("UseSkybox", kPC_FragmentBit),
		cProgramComboFeature("UseSecondaryFog", kPC_FragmentBit),
		cProgramComboFeature("FogArea", kPC_FragmentBit),
		cProgramComboFeature("Lighten", kPC_FragmentBit),
		cProgramComboFeature("UseSeparateFogSkybox", kPC_FragmentBit),
	};

	//////////////////////////////////////////////////////////////////////////
	// PROGRAM VARAIBLES
	//////////////////////////////////////////////////////////////////////////

	#define kVar_avLightPos							0
	#define kVar_avLightColor						1
	#define kVar_afInvLightRadius					2
	#define kVar_afNegFarPlane						3
	#define kVar_afOneMinusCosHalfSpotFOV			4
	#define kVar_avLightForward						5
	#define kVar_a_mtxSpotViewProj					6
	#define kVar_a_mtxInvViewRotation				7
	#define kVar_avShadowMapOffsetMul				8
	#define kVar_afNegInvFarPlane					9
	#define kVar_afFarPlane							10
	#define kVar_afScatterLengthMul					11
	#define kVar_avScreenSize						12
	#define kVar_avScatterLengthLimits				13
	#define kVar_avFogStartAndLength				14
	#define kVar_avFogColor							15
	#define kVar_avRayCastStart						16
	#define kVar_a_mtxBoxInvViewModelRotation		17
	#define kVar_avNegPlaneDistNeg					18
	#define kVar_avNegPlaneDistPos					19
	#define kVar_afFalloffExp						20
	#define kVar_afDepthDiffMul						21
	#define kVar_afSkipEdgeLimit					22
	#define kVar_avScreenToFarPlane					23
	#define kVar_avInvScreenSize					24
	#define kVar_a_mtxLightViewProj					25
	#define kVar_afFalloffPow						26
	#define kVar_afSpotFalloffPow					27
	#define kVar_afKey								28
	#define kVar_afExposure							29
	#define kVar_afWhiteCut							30
	#define kVar_afHdrHeadroom						119
	#define kVar_afInvGammaCorrection				31
	#define kVar_a_mtxInvView						32
	#define kVar_avMaskCenter						33
	#define kVar_avMaskExtent						34
	#define kVar_avAmbientColorSky					35
	#define kVar_avAmbientColorGround				36
	#define kVar_avBoxCenter						37
	#define kVar_avBoxExtent						38
	#define kVar_afFalloff							39
	#define kVar_afBevel							40
	#define kVar_afWeight							41
	#define kVar_afLightSourceRadius				42
	#define kVar_avLightUp							43
	#define kVar_avLightRight						44
	#define kVar_avViewSpaceUp						94
	#define kVar_afGradingBlendWeight				95
	#define kVar_afAlpha							96
	#define kVar_avBand0							43
	#define kVar_afSpotNearClip						52
	#define kVar_avFocusStartEnd					53
	#define kVar_avOffsetMul						54
	#define kVar_avUVToView0						55
	#define kVar_avUVToView1						56
	#define kVar_afT								57
	#define kVar_afStepSizeMax						58
	#define kVar_afRadius							59
	#define kVar_afScreenSizeDiv					60
	#define kVar_afLodScale							61
	#define kVar_afDepthDifference					62
	#define kVar_avDirection						63
	#define kVar_a_mtxTemporalView					64
	#define kVar_a_mtxTemporalProjection			65
	#define kVar_afTemporalBlurAmount				66
	#define kVar_afSizeDiv							67
	#define kVar_afPower							68
	#define kVar_afTranslucencyScale				69
	#define kVar_a_mtxNoise							70
	#define kVar_afInvNoiseScale					71
	#define kVar_afNoiseStrength					72
	#define kVar_avSkyboxColor						73
	#define kVar_avSecondFogColor					74
	#define kVar_avSecondFogStartAndLength			75
	#define kVar_afSecondFalloffExp					76
	#define kVar_afFogFalloffExp					77
	#define kVar_avLightDirection					78
	#define kVar_afBrightPass						79
	#define kVar_avWeights							80
	#define kVar_avOffsets							81
	#define kVar_afIntensity						82
	#define kVar_avTransform0						83
	#define kVar_avTransform1						84
	#define kVar_avBloomTint						85
	#define kVar_avSizeWeight						86
	#define kVar_a_mtxLightViewProj0				87
	#define kVar_avSplitsNear						91
	#define kVar_avSplitsFar						92
	#define kVar_avSplitOffsetMul					93
	#define kVar_avLightPosScale					94
	#define kVar_avFogSettings						95
	#define kVar_avSecondFogSettings				96
	#define kVar_avCameraPos						97
	#define kVar_alFogType							98
	#define kVar_a_mtxSkybox						99
	#define kVar_afSkyboxMipNum						100
	#define kVar_afMaxDistance						101
	#define kVar_afGradingMapAlpha0					102
	#define kVar_alGradingMapNum					103
	#define kVar_afGradingMapAlpha2					104 // ..109
	#define kVar_avShadowParams						110
	#define kVar_avShadowMapSize					111
	#define kVar_afShadowDistance					112
	#define kVar_alSliceNum							113
	#define kVar_afSliceDiv							114
	#define kVar_a_mtxLightViewProjSlice0			115 // ..118

	static_assert(kVar_afKey == kOhplDrVar_afKey && kVar_afExposure == kOhplDrVar_afExposure && kVar_afWhiteCut == kOhplDrVar_afWhiteCut &&
				  kVar_afInvGammaCorrection == kOhplDrVar_afInvGammaCorrection && kVar_avInvScreenSize == kOhplDrVar_avInvScreenSize &&
				  kVar_avScreenToFarPlane == kOhplDrVar_avScreenToFarPlane && kVar_afT == kOhplDrVar_afT &&
				  kVar_afBrightPass == kOhplDrVar_afBrightPass && kVar_avWeights == kOhplDrVar_avWeights &&
				  kVar_avOffsets == kOhplDrVar_avOffsets && kVar_afIntensity == kOhplDrVar_afIntensity &&
				  kVar_avTransform0 == kOhplDrVar_avTransform0 && kVar_avTransform1 == kOhplDrVar_avTransform1 &&
				  kVar_avBloomTint == kOhplDrVar_avBloomTint && kVar_avSizeWeight == kOhplDrVar_avSizeWeight &&
				  kVar_afGradingBlendWeight == kOhplDrVar_afGradingBlendWeight && kVar_afAlpha == kOhplDrVar_afAlpha &&
				  kVar_afGradingMapAlpha0 == kOhplDrVar_afGradingMapAlpha0 && kVar_alGradingMapNum == kOhplDrVar_alGradingMapNum &&
				  kVar_afGradingMapAlpha2 == kOhplDrVar_afGradingMapAlpha2 && kVar_afHdrHeadroom == kOhplDrVar_afHdrHeadroom,
				  "kVar ids out of step with rust/src/render/deferred.rs");


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cRendererDeferred::cRendererDeferred(cGraphics *apGraphics,cResources* apResources) 
		: iRenderer("Deferred",apGraphics, apResources,eDefferredProgramMode_LastEnum)
	{
		////////////////////////////////////
		// Set up render specific things
		mbSetFrameBufferAtBeginRendering = false;		//Not using the input frame buffer for any rendering. Only doing copy at the end!
		mbClearFrameBufferAtBeginRendering = false;
		mbSetupOcclusionPlaneForFog = true;

		////////////////////////////////////
		// Set up variables
		mfLastFrustumFOV = -1;
		mfLastFrustumFarPlane = -1;
		mpDirShadowData = NULL;
		
		mfMinLargeLightNormalizedArea = 0.2f*0.2f;
		mfMinRenderReflectionNormilzedLength = 0.15f;

		mfShadowDistanceMedium = 10;
		mfShadowDistanceNone = mfDefaultShadowDistanceNone;
		mfShadowDistanceLow = cMath::Min(20.0f, mfShadowDistanceNone);

		mlMaxBatchLights = 100;
		mpFxaaProgram = NULL;
		mpDofFocusProgram = NULL;
		for(int i=0; i<3; ++i) { mpH3SSAOTexture[i] = NULL; mpH3SSAOBuffer[i] = NULL; }
		mpH3SSAOMipTexture = NULL;
		mpH3SSAODownsampleProgram = NULL;
		mpH3SSAORenderProgram = NULL;
		mpNightVisionProgram = NULL;
		mpH3SSAOBlurProgram = NULL;
		mpH3SSAOTemporalProgram = NULL;
		mpH3SSAOUpsampleProgram = NULL;
		mfH3SSAOTime = 0;
		mbH3SSAOFirstFrame = true;
		mbH3SSAORendered = false;
		mpDofBlurProgram = NULL;
		mpDofGaussTexture = NULL;
		mpToneMapProgram = NULL;
		mpBoxResolveProgram = NULL;
		mpBoxWeightTexture = NULL;
		mpBoxWeightBuffer = NULL;
		for(int i=0; i<3; ++i) for(int j=0; j<2; ++j) mpBoxWeightedProgram[i][j] = NULL;
		mbBoxInstanceInputs = false;

		mbReflectionTextureCleared = false;

		OhplDrFilmGrain grain;
		ohpl_dr_film_grain_init(&grain);
		mfFilmGrainT = grain.t;
		memcpy(mvFilmGrainTransform, grain.transform, sizeof(mvFilmGrainTransform));
	}

	//-----------------------------------------------------------------------

	cRendererDeferred::~cRendererDeferred()
	{
		STLDeleteAll(mvTempDeferredLights);	
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cRendererDeferred::LoadData()
	{
		cVector2l vRelfectionSize = cVector2l(mvScreenSize.x/mlReflectionSizeDiv, mvScreenSize.y/mlReflectionSizeDiv);

		Log("Setting up G-Bugger: type: %d texturenum: %d\n", mGBufferType, mlNumOfGBufferTextures);
		////////////////////////////////////
		//Create G-Buffer textures
		for(int i=0; i<mlNumOfGBufferTextures; ++i)
		{
			ePixelFormat pixelFormat = mGBufferType == eDeferredGBuffer_32Bit ? ePixelFormat_RGBA : ePixelFormat_RGBA16;
			//ePixelFormat pixelFormat = ePixelFormat_RGBA16;

			tString sName = "G-BufferTexure"+cString::ToString(i);
			mpGBufferTexture[0][i] = CreateRenderTexture(sName, mvScreenSize,pixelFormat,eTextureFilter_Nearest, mGBufferTextureType);
			mpGBufferTexture[1][i] = CreateRenderTexture(sName+"_Reflection", vRelfectionSize, pixelFormat,eTextureFilter_Nearest, mGBufferTextureType);
		}

		////////////////////////////////////
		//Create Depth and stencil
		mpDepthStencil[0] = mpGraphics->CreateDepthStencilBuffer(mvScreenSize,24,8, false);
		mpDepthStencil[1] = mpGraphics->CreateDepthStencilBuffer(vRelfectionSize, 24,8, false);
		
		////////////////////////////////////
		//Create Frame buffers
		for(int type=0; type<2; ++type)
		{
			mpGBuffer[type][eGBufferComponents_Full] = mpGraphics->CreateFrameBuffer("Deferred_GBuffer_Main");
			for(int i=0;i<mlNumOfGBufferTextures; ++i) 
			{
				mpGBuffer[type][eGBufferComponents_Full]->SetTexture2D(i,mpGBufferTexture[type][i]);
			}
			mpGBuffer[type][eGBufferComponents_Full]->SetDepthStencilBuffer(mpDepthStencil[type]);
			if(mpGBuffer[type][eGBufferComponents_Full]->CompileAndValidate()==false)
			{
				Error("Could not create frame buffer for Deferred renderer type: %d!\n", type);
				return false;
			}

			/////////////////////////
			//Frame buffer with only color
			mpGBuffer[type][eGBufferComponents_Color] = mpGraphics->CreateFrameBuffer("Deferred_GBuffer_ColorDepth");
			mpGBuffer[type][eGBufferComponents_Color]->SetTexture2D(0,mpGBufferTexture[type][0]);
			if(mpGBuffer[type][eGBufferComponents_Color]->CompileAndValidate()==false)
			{
				Error("Could not create frame buffer with only color Deferred renderer type: %d!\n", type);
				return false;
			}

			/////////////////////////
			//Frame buffer with only depth
			mpGBuffer[type][eGBufferComponents_Depth] = mpGraphics->CreateFrameBuffer("Deferred_GBuffer_Depth");
			mpGBuffer[type][eGBufferComponents_Depth]->SetDepthStencilBuffer(mpDepthStencil[type]);
			if(mpGBuffer[type][eGBufferComponents_Depth]->CompileAndValidate()==false)
			{
				Error("Could not create frame buffer with only depth for Deferred renderer type: %d!\n", type);
				return false;
			}
			
			/////////////////////////
			//Frame buffer with only depth and color
			mpGBuffer[type][eGBufferComponents_ColorAndDepth] = mpGraphics->CreateFrameBuffer("Deferred_GBuffer_ColorDepth");
			mpGBuffer[type][eGBufferComponents_ColorAndDepth]->SetTexture2D(0,mpGBufferTexture[type][0]);
			mpGBuffer[type][eGBufferComponents_ColorAndDepth]->SetDepthStencilBuffer(mpDepthStencil[type]);
			if(mpGBuffer[type][eGBufferComponents_ColorAndDepth]->CompileAndValidate()==false)
			{
				Error("Could not create frame buffer with only color and depth for Deferred renderer type: %d!\n", type);
				return false;
			}

			/////////////////////////
			//Frame buffer with only normals
			mpGBuffer[type][eGBufferComponents_Normals] = mpGraphics->CreateFrameBuffer("Deferred_GBuffer_Normals");
			mpGBuffer[type][eGBufferComponents_Normals]->SetTexture2D(0,mpGBufferTexture[type][1]);
			if(mpGBuffer[type][eGBufferComponents_Normals]->CompileAndValidate()==false)
			{
				Error("Could not create frame buffer with only normals for Deferred renderer type: %d!\n", type);
				return false;
			}

			/////////////////////////
			//Frame buffer with only linear depth
			mpGBuffer[type][eGBufferComponents_LinearDepth] = mpGraphics->CreateFrameBuffer("Deferred_GBuffer_LinearDepth");
			mpGBuffer[type][eGBufferComponents_LinearDepth]->SetTexture2D(0,mpGBufferTexture[type][mlLinearDepthTextureIdx]);
			if(mpGBuffer[type][eGBufferComponents_LinearDepth]->CompileAndValidate()==false)
			{
				Error("Could not create frame buffer with only linear depth for Deferred renderer type: %d!\n", type);
				return false;
			}
		}
		
		////////////////////////////////////
		//Create Accumulation texture
		mpAccumBufferTexture = mpGraphics->CreateTexture("AccumBiffer",mGBufferTextureType,eTextureUsage_RenderTarget);
		mpAccumBufferTexture->CreateFromRawData(cVector3l(mvScreenSize.x, mvScreenSize.y,0),mbHdr ? ePixelFormat_RGBA16 : ePixelFormat_RGBA, NULL);
		mpAccumBufferTexture->SetWrapSTR(eTextureWrap_ClampToEdge);

		////////////////////////////////////
		//Create Accumulation buffer
		mpAccumBuffer = mpGraphics->CreateFrameBuffer("Deferred_Accumulation");
		mpAccumBuffer->SetTexture2D(0,mpAccumBufferTexture);
		mpAccumBuffer->SetDepthStencilBuffer(mpDepthStencil[0]);

		mpAccumBuffer->CompileAndValidate();

		if(mbHdr)
		{
			mpBoxWeightTexture = mpGraphics->CreateTexture("BoxWeightTexture",mGBufferTextureType,eTextureUsage_RenderTarget);
			mpBoxWeightTexture->CreateFromRawData(cVector3l(mvScreenSize.x, mvScreenSize.y,0),ePixelFormat_RGBA16, NULL);
			mpBoxWeightTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
			mpBoxWeightBuffer = mpGraphics->CreateFrameBuffer("Deferred_BoxWeight");
			mpBoxWeightBuffer->SetTexture2D(0,mpBoxWeightTexture);
			mpBoxWeightBuffer->SetDepthStencilBuffer(mpDepthStencil[0]);
			mpBoxWeightBuffer->CompileAndValidate();
		}

		////////////////////////////////////
		//Create Refraction texture
		mpRefractionTexture = mpGraphics->GetTempFrameBuffer(mvScreenSize,mbHdr ? ePixelFormat_RGBA16 : ePixelFormat_RGBA,0)->GetColorBuffer(0)->ToTexture();
		mpRefractionTexture->SetWrapSTR(eTextureWrap_ClampToEdge);

		////////////////////////////////////
		//Create Reflection texture
		mpReflectionTexture = CreateRenderTexture("ReflectionTexture",vRelfectionSize,ePixelFormat_RGBA,eTextureFilter_Bilinear,mGBufferTextureType);
		
		////////////////////////////////////
		//Create Reflection buffer
		mpReflectionBuffer = mpGraphics->CreateFrameBuffer("Deferred_Reflection");
		mpReflectionBuffer->SetTexture2D(0,mpReflectionTexture);
		mpReflectionBuffer->SetDepthStencilBuffer(mpDepthStencil[1]);
		mpReflectionBuffer->CompileAndValidate();
		
		
		////////////////////////////////////
		//Create Shadow Textures
		OhplDrShadowCfg shadowCfg;
		ohpl_dr_shadow_cfg((int)mShadowMapResolution, (int)mShadowMapQuality, mpLowLevelGraphics->GetCaps(eGraphicCaps_ShaderModel_4) != 0, &shadowCfg);

		// Depth16 shadow maps are never written on Mesa (asahi)
		CreateAndAddShadowMap(eShadowMapResolution_High, cVector3l(shadowCfg.high.x, shadowCfg.high.y, 1),ePixelFormat_Depth24);
		for(int i=0; i<4; ++i)
			CreateAndAddShadowMap(eShadowMapResolution_Medium, cVector3l(shadowCfg.medium.x, shadowCfg.medium.y, 1),ePixelFormat_Depth24);
		for(int i=0; i<6; ++i)
			CreateAndAddShadowMap(eShadowMapResolution_Low, cVector3l(shadowCfg.low.x, shadowCfg.low.y, 1),ePixelFormat_Depth24);

		mlShadowJitterSize = shadowCfg.jitterSize;
		mlShadowJitterSamples = shadowCfg.jitterSamples;

		if(mShadowMapQuality != eShadowMapQuality_Low)
		{
			mpShadowJitterTexture = mpGraphics->CreateTexture("ShadowOffset", eTextureType_2D, eTextureUsage_Normal);
			mpGraphics->GetTextureCreator()->GenerateScatterDiskMap2D(mpShadowJitterTexture,mlShadowJitterSize,mlShadowJitterSamples, true);
		}
		else
		{
			mpShadowJitterTexture = NULL;
		}

		if(mbHdr)
		{
			cParserVarContainer vars;
			vars.Add("UseUv");
			vars.Add("UseColor");
			mpLensFlareGlareProgram = mpGraphics->CreateGpuProgramFromShaders("FullscreenGlare", "posteffect_quad_vtx.glsl", "deferred_base_frag.glsl", &vars);
			vars.Add("UseDiffuse");
			mpLensFlareIrisProgram = mpGraphics->CreateGpuProgramFromShaders("LensFlareMultiIris", "posteffect_quad_vtx.glsl", "deferred_base_frag.glsl", &vars);

			mpGlareVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Dynamic, 400, 19*19*6);
			mpGlareVtxBuffer->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
			mpGlareVtxBuffer->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
			mpGlareVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
			std::vector<float> vGlarePos(kOhplDrGlareVerts*3);
			ohpl_dr_glare_positions(&vGlarePos[0], vGlarePos.size());
			for(int i=0; i<kOhplDrGlareVerts; ++i)
			{
				cVector3f vPos(vGlarePos[i*3], vGlarePos[i*3+1], vGlarePos[i*3+2]);
				mpGlareVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, vPos);
				mpGlareVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(0,1));
				mpGlareVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, vPos);
			}
			std::vector<uint32_t> vGlareIdx(kOhplDrGlareIndices);
			ohpl_dr_glare_indices(&vGlareIdx[0], vGlareIdx.size());
			for(uint32_t lIdx : vGlareIdx) mpGlareVtxBuffer->AddIndex(lIdx);
			mpGlareVtxBuffer->Compile(0);
		}

		////////////////////////////////////
		//Create Sky box program
		{
			cParserVarContainer vars;
			vars.Add("UseUv");
			vars.Add("UseColor");
			iGpuShader *pVtxShader = mpShaderManager->CreateShader("deferred_base_vtx.glsl",eGpuShaderType_Vertex,&vars);
			iGpuShader *pFragShader = mpShaderManager->CreateShader("deferred_gbuffer_skybox_frag.glsl", eGpuShaderType_Fragment,&vars);

			mpSkyBoxProgram = mpGraphics->CreateGpuProgram("DeferredSkyBox");
			mpSkyBoxProgram->SetShader(eGpuShaderType_Vertex, pVtxShader);
			mpSkyBoxProgram->SetShader(eGpuShaderType_Fragment, pFragShader);
			mpSkyBoxProgram->Link();

			std::vector<cBitmap> vFaces(6);
			std::vector<cBitmap*> vFacePtrs;
			for(cBitmap &bmp : vFaces)
			{
				bmp.CreateData(cVector3l(1,1,1), ePixelFormat_RGBA, 0, 0);
				bmp.Clear(cColor(1,1), 0, 0);
				vFacePtrs.push_back(&bmp);
			}
			mpWhiteCubeTexture = mpGraphics->CreateTexture("WhiteCube", eTextureType_CubeMap, eTextureUsage_Normal);
			mpWhiteCubeTexture->SetUseMipMaps(false);
			mpWhiteCubeTexture->CreateCubeFromBitmapVec(&vFacePtrs);
		}
		
		
		////////////////////////////////////
		//Create Fog program
		{
			cParserVarContainer vars;
			if(GetGBufferType() == eDeferredGBuffer_32Bit)	vars.Add("PackedDepth");
			
			mpFogProgramManager = hplNew(	cProgramComboManager, ("FogArea", mpGraphics, mpResources, 1));

			mpFogProgramManager->SetupGenerateProgramData(0,"Fog","deferred_fog_vtx.glsl","deferred_fog_frag.glsl",gvFogAreaFeatureVec,kFogAreaFeatureNum,vars);

			mpFogProgramManager->AddGenerateProgramVariableId("afNegFarPlane", kVar_afNegFarPlane,0);
			mpFogProgramManager->AddGenerateProgramVariableId("avFogSettings", kVar_avFogSettings,0);
			mpFogProgramManager->AddGenerateProgramVariableId("avSecondFogSettings", kVar_avSecondFogSettings,0);
			mpFogProgramManager->AddGenerateProgramVariableId("avCameraPos", kVar_avCameraPos,0);
			mpFogProgramManager->AddGenerateProgramVariableId("alFogType", kVar_alFogType,0);
			mpFogProgramManager->AddGenerateProgramVariableId("a_mtxSkybox", kVar_a_mtxSkybox,0);
			mpFogProgramManager->AddGenerateProgramVariableId("afSkyboxMipNum", kVar_afSkyboxMipNum,0);
			mpFogProgramManager->AddGenerateProgramVariableId("avFogStartAndLength", kVar_avFogStartAndLength,0);
			mpFogProgramManager->AddGenerateProgramVariableId("avFogColor", kVar_avFogColor,0);
			mpFogProgramManager->AddGenerateProgramVariableId("avRayCastStart", kVar_avRayCastStart,0);
			mpFogProgramManager->AddGenerateProgramVariableId("a_mtxBoxInvViewModelRotation", kVar_a_mtxBoxInvViewModelRotation,0);
			mpFogProgramManager->AddGenerateProgramVariableId("avNegPlaneDistNeg",kVar_avNegPlaneDistNeg,0);
			mpFogProgramManager->AddGenerateProgramVariableId("avNegPlaneDistPos",kVar_avNegPlaneDistPos,0);
			mpFogProgramManager->AddGenerateProgramVariableId("afFalloffExp",kVar_afFalloffExp,0);

			mpFogProgramManager->AddGenerateProgramVariableId("avScreenToFarPlane", kVar_avScreenToFarPlane, 0);
			mpFogProgramManager->AddGenerateProgramVariableId("avInvScreenSize", kVar_avInvScreenSize, 0);
			mpFogProgramManager->AddGenerateProgramVariableId("a_mtxNoise", kVar_a_mtxNoise, 0);
			mpFogProgramManager->AddGenerateProgramVariableId("afInvNoiseScale", kVar_afInvNoiseScale, 0);
			mpFogProgramManager->AddGenerateProgramVariableId("afNoiseStrength", kVar_afNoiseStrength, 0);
			mpFogProgramManager->AddGenerateProgramVariableId("avSkyboxColor", kVar_avSkyboxColor, 0);
			mpFogProgramManager->AddGenerateProgramVariableId("avSecondFogColor", kVar_avSecondFogColor, 0);
			mpFogProgramManager->AddGenerateProgramVariableId("avSecondFogStartAndLength", kVar_avSecondFogStartAndLength, 0);
			mpFogProgramManager->AddGenerateProgramVariableId("afSecondFalloffExp", kVar_afSecondFalloffExp, 0);
		}
		
		////////////////////////////////////
		//Create Light programs
		{
			/////////////////////////////
			//Misc
			{
				cParserVarContainer vars;

				//////////////
				//Light Stencil
				mpLightStencilProgram = mpProgramManager->CreateProgramFromShaders("LightStencil",
																					"deferred_base_vtx.glsl",
																					"deferred_base_frag.glsl",
																					&vars,true);

				//////////////
				//Light box
				for(int i=0;i<2; ++i)
				{
					if(i==1) vars.Add("UseSSAO");
					mpLightBoxProgram[i] = mpProgramManager->CreateProgramFromShaders("LightBoxNormal",
																					"deferred_base_vtx.glsl",
																					"deferred_light_box_frag.glsl",
																					&vars,true);

					vars.Clear();
					if(mpLightBoxProgram[i])
					{
						mpLightBoxProgram[i]->GetVariableAsId("avLightColor",kVar_avLightColor);
					}
				}

				if(mbHdr)
				{
					const char *vBlendVars[3] = {"Blend_Replace", "Blend_Add", "Blend_Blend"};
					for(int i=0; i<3; ++i) for(int j=0; j<2; ++j)
					{
						cParserVarContainer boxVars;
						boxVars.Add(vBlendVars[i]);
						if(j==1) boxVars.Add("UseIrradiance");
						iGpuProgram *pProg = mpGraphics->CreateGpuProgramFromShaders("LightBoxWeighted", "deferred_base_vtx.glsl",
																					"deferred_light_box_frag.glsl", &boxVars);
						mpBoxWeightedProgram[i][j] = pProg;
						if(pProg==NULL) continue;
						pProg->GetVariableAsId("afNegFarPlane",kVar_afNegFarPlane);
						pProg->GetVariableAsId("avScreenToFarPlane",kVar_avScreenToFarPlane);
						pProg->GetVariableAsId("avInvScreenSize",kVar_avInvScreenSize);
						pProg->GetVariableAsId("a_mtxInvView",kVar_a_mtxInvView);
						pProg->GetVariableAsId("avAmbientColorSky",kVar_avAmbientColorSky);
						pProg->GetVariableAsId("avAmbientColorGround",kVar_avAmbientColorGround);
						pProg->GetVariableAsId("avBoxCenter",kVar_avBoxCenter);
						pProg->GetVariableAsId("avBoxExtent",kVar_avBoxExtent);
						pProg->GetVariableAsId("afFalloff",kVar_afFalloff);
						pProg->GetVariableAsId("afBevel",kVar_afBevel);
						pProg->GetVariableAsId("afWeight",kVar_afWeight);
						pProg->GetVariableAsId("avViewSpaceUp",kVar_avViewSpaceUp);
						for(int k=0; k<9; ++k) pProg->GetVariableAsId("avBand["+cString::ToString(k)+"]",kVar_avBand0+k);
						if(pProg->GetVariableAsId("px_vBoxCenter",kVar_avBoxCenter))
						{
							mbBoxInstanceInputs = true;
							pProg->GetVariableAsId("afFarPlane",kVar_afFarPlane);
							pProg->GetVariableAsId("px_vSkyColor",kVar_avAmbientColorSky);
							pProg->GetVariableAsId("px_vGroundColor",kVar_avAmbientColorGround);
							pProg->GetVariableAsId("px_vBoxExtent",kVar_avBoxExtent);
							pProg->GetVariableAsId("px_fFalloff",kVar_afFalloff);
							pProg->GetVariableAsId("px_fBevel",kVar_afBevel);
							pProg->GetVariableAsId("px_fWeight",kVar_afWeight);
							for(int k=0; k<7; ++k) pProg->GetVariableAsId("px_vBands["+cString::ToString(k)+"]",kVar_avBand0+k);
						}
					}
					cParserVarContainer resolveVars;
					resolveVars.Add("UseUv");
					mpBoxResolveProgram = mpGraphics->CreateGpuProgramFromShaders("LightBoxResolve", "deferred_base_vtx.glsl",
																				  "deferred_light_box_resolve_frag.glsl", &resolveVars);
				}
			}
			
			/////////////////////////////
			//Lights
			{
				cParserVarContainer defaultVars;
				
				//Shadow variables
				defaultVars.Add("ShadowJitterLookupMul",1.0f / (float)mlShadowJitterSize);
				defaultVars.Add("ShadowJitterSamplesDiv2",mlShadowJitterSamples / 2);
				defaultVars.Add("ShadowJitterSamples", mlShadowJitterSamples);

				if(mShadowMapQuality == eShadowMapQuality_High)		defaultVars.Add("ShadowMapQuality_High");
				if(mShadowMapQuality == eShadowMapQuality_Medium)	defaultVars.Add("ShadowMapQuality_Medium");
				if(mShadowMapQuality == eShadowMapQuality_Low)		defaultVars.Add("ShadowMapQuality_Low");

				//Vertex shader will handles deferred lights
				defaultVars.Add("DeferredLight");

				//Deferred renderer type
				if(mGBufferType == eDeferredGBuffer_32Bit)	defaultVars.Add("Deferred_32bit","");
				else										defaultVars.Add("Deferred_64bit","");
				
				if(mlNumOfGBufferTextures == 4)				defaultVars.Add("RenderTargets_4","");
				else										defaultVars.Add("RenderTargets_3","");

				mpProgramManager->SetupGenerateProgramData(	eDefferredProgramMode_Lights,"Lights", "deferred_base_vtx.glsl", "deferred_light_frag.glsl",gvLightFeatureVec,
													kLightFeatureNum,defaultVars);
													//1, defaultVars);

				
				mpProgramManager->AddGenerateProgramVariableId("avLightPos",	kVar_avLightPos, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avLightColor",	kVar_avLightColor, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afInvLightRadius",	kVar_afInvLightRadius, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afNegFarPlane",	kVar_afNegFarPlane, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afOneMinusCosHalfSpotFOV",	kVar_afOneMinusCosHalfSpotFOV, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avLightForward", kVar_avLightForward, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("a_mtxSpotViewProj", kVar_a_mtxSpotViewProj, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("a_mtxInvViewRotation", kVar_a_mtxInvViewRotation, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avShadowMapOffsetMul", kVar_avShadowMapOffsetMul, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avShadowParams", kVar_avShadowParams, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avShadowMapSize", kVar_avShadowMapSize, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avScreenToFarPlane", kVar_avScreenToFarPlane, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avInvScreenSize", kVar_avInvScreenSize, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("a_mtxLightViewProj", kVar_a_mtxLightViewProj, eDefferredProgramMode_Lights);
				for(int i=0; i<4; ++i)
					mpProgramManager->AddGenerateProgramVariableId("a_mtxLightViewProj"+cString::ToString(i), kVar_a_mtxLightViewProj0+i, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afShadowDistance", kVar_afShadowDistance, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("alSliceNum", kVar_alSliceNum, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afSliceDiv", kVar_afSliceDiv, eDefferredProgramMode_Lights);
				for(int i=0; i<4; ++i)
					mpProgramManager->AddGenerateProgramVariableId("a_mtxLightViewProj["+cString::ToString(i)+"]", kVar_a_mtxLightViewProjSlice0+i, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avSplitsNear", kVar_avSplitsNear, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avSplitsFar", kVar_avSplitsFar, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avSplitOffsetMul", kVar_avSplitOffsetMul, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afSpotNearClip", kVar_afSpotNearClip, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afFalloffPow", kVar_afFalloffPow, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afTranslucencyScale", kVar_afTranslucencyScale, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afSpotFalloffPow", kVar_afSpotFalloffPow, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("a_mtxInvView", kVar_a_mtxInvView, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avMaskCenter", kVar_avMaskCenter, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avMaskExtent", kVar_avMaskExtent, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afLightSourceRadius", kVar_afLightSourceRadius, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avLightUp", kVar_avLightUp, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avLightRight", kVar_avLightRight, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avFogColor", kVar_avFogColor, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avFogStartAndLength", kVar_avFogStartAndLength, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("afFogFalloffExp", kVar_afFogFalloffExp, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avLightDirection", kVar_avLightDirection, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avAmbientColorSky", kVar_avAmbientColorSky, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avAmbientColorGround", kVar_avAmbientColorGround, eDefferredProgramMode_Lights);
				mpProgramManager->AddGenerateProgramVariableId("avViewSpaceUp", kVar_avViewSpaceUp, eDefferredProgramMode_Lights);

				// per-light inputs of the newer light shader; a program only has one name of each pair
				static const struct { const char *msName; int mlId; } vAliases[] = {
					{"px_vLightColor", kVar_avLightColor}, {"px_fFalloff", kVar_afFalloffPow}, {"px_fSpotFalloffPow", kVar_afSpotFalloffPow},
					{"px_fTranslucenyMul", kVar_afTranslucencyScale}, {"px_fLightSourceRadius", kVar_afLightSourceRadius},
					{"px_mtxLightViewProj", kVar_a_mtxLightViewProj}, {"px_vLightForward", kVar_avLightForward},
					{"px_vLightUp", kVar_avLightUp}, {"px_vLightRight", kVar_avLightRight},
					{"px_vMaskCenter", kVar_avMaskCenter}, {"px_vMaskExtent", kVar_avMaskExtent},
					{"px_vLightPosScale", kVar_avLightPosScale}, {"afFarPlane", kVar_afFarPlane},
				};
				for(size_t i=0; i<sizeof(vAliases)/sizeof(vAliases[0]); ++i)
					mpProgramManager->AddGenerateProgramVariableId(vAliases[i].msName, vAliases[i].mlId, eDefferredProgramMode_Lights);
			}

			//////////////////////////////
			// Generate some light programs
			//for(i=0; i<128; ++i)
			//	mpProgramManager->GenerateProgram(eDefferredProgramMode_Lights, i);
		}

		////////////////////////////////////
		//Create SSAO programs and textures
		if(mbSSAOLoaded && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat)==0)
		{
			mbSSAOLoaded = false;
			Warning("System does not support float textures! SSAO is disabled.\n");
		}
		if(mbSSAOLoaded)
		{
			cVector2l vSSAOSize = mvScreenSize / mlSSAOBufferSizeDiv;
			
			/////////////////////////////////////
			// Textures and frame buffers

			// Textures
			mpLinearDepthTexture = CreateRenderTexture("LinearDepth", vSSAOSize,ePixelFormat_RGB16);
			mpSSAOTexture = CreateRenderTexture("SSAO", vSSAOSize,ePixelFormat_RGB);
			mpSSAOBlurTexture = CreateRenderTexture("SSAOBlur", vSSAOSize,ePixelFormat_RGB);

			//Frame buffers
			mpLinearDepthBuffer = mpGraphics->CreateFrameBuffer("LinearDepth");
			mpLinearDepthBuffer->SetTexture2D(0,mpLinearDepthTexture);
			mpLinearDepthBuffer->CompileAndValidate();

			mpSSAOBuffer = mpGraphics->CreateFrameBuffer("SSAO");
			mpSSAOBuffer->SetTexture2D(0,mpSSAOTexture);
			mpSSAOBuffer->CompileAndValidate();

			mpSSAOBlurBuffer = mpGraphics->CreateFrameBuffer("SSAOBlur");
			mpSSAOBlurBuffer->SetTexture2D(0,mpSSAOBlurTexture);
			mpSSAOBlurBuffer->CompileAndValidate();

			//Scatter disk
			mpSSAOScatterDisk = mpGraphics->CreateTexture("SSAOScatterDisk", eTextureType_2D,eTextureUsage_Normal);
			mpGraphics->GetTextureCreator()->GenerateScatterDiskMap2D(mpSSAOScatterDisk,4, mlSSAONumOfSamples, false);
			
			
			/////////////////////////////////////
			// Programs
			cParserVarContainer programVars;

			//Program for unpacking depth to a lower resolution texture
			if(mGBufferType == eDeferredGBuffer_32Bit)	programVars.Add("Deferred_32bit");
			else										programVars.Add("Deferred_64bit");
			programVars.Add("UseUv");
			mpUnpackDepthProgram = mpGraphics->CreateGpuProgramFromShaders("UnpackDepth","deferred_base_vtx.glsl", "deferred_unpack_depth_frag.glsl",&programVars);
			if(mpUnpackDepthProgram)
			{
				mpUnpackDepthProgram->GetVariableAsId("afNegInvFarPlane",kVar_afNegInvFarPlane);
			}
			programVars.Clear();
			
			//SSAO Blur programs (vertical and horizontal)
			programVars.Add("BlurHorisontal");
			mpSSAOBlurProgram[0] = mpGraphics->CreateGpuProgramFromShaders("SSAOBlurHori","deferred_ssao_blur_vtx.glsl", "deferred_ssao_blur_frag.glsl",&programVars);
			programVars.Clear();
			mpSSAOBlurProgram[1] = mpGraphics->CreateGpuProgramFromShaders("SSAOBlurVert","deferred_ssao_blur_vtx.glsl", "deferred_ssao_blur_frag.glsl",&programVars);

			for(int i=0; i<2; ++i)
			{
				if(mpSSAOBlurProgram[i])
				{
					mpSSAOBlurProgram[i]->GetVariableAsId("afFarPlane",kVar_afFarPlane);
				}
			}
			
			//SSAO Rendering
			programVars.Add("SampleNumDiv2", mlSSAONumOfSamples / 2);
			mpSSAORenderProgram = mpGraphics->CreateGpuProgramFromShaders(	"SSAORender","deferred_ssao_render_vtx.glsl", 
																			"deferred_ssao_render_frag.glsl",&programVars);
			if(mpSSAORenderProgram)
			{
				mpSSAORenderProgram->GetVariableAsId("afFarPlane",kVar_afFarPlane);
				mpSSAORenderProgram->GetVariableAsId("afScatterLengthMul", kVar_afScatterLengthMul);
				mpSSAORenderProgram->GetVariableAsId("avScatterLengthLimits", kVar_avScatterLengthLimits);
				mpSSAORenderProgram->GetVariableAsId("avScreenSize", kVar_avScreenSize);
				mpSSAORenderProgram->GetVariableAsId("afDepthDiffMul", kVar_afDepthDiffMul);
				mpSSAORenderProgram->GetVariableAsId("afSkipEdgeLimit", kVar_afSkipEdgeLimit);
			}
		}

		////////////////////////////////////
		//Create Smooth Edge and textures
		if(mbEdgeSmoothLoaded && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat)==0)
		{
			mbEdgeSmoothLoaded = false;
			Warning("System does not support float textures! Edge smooth is disabled.\n");
		}
		if(mbEdgeSmoothLoaded)
		{
			/////////////////////////////////////
			// Textures and frame buffers

			// Textures
			mpEdgeSmooth_LinearDepthTexture = CreateRenderTexture("EdgeSmoothLinearDepth", mvScreenSize,ePixelFormat_RGB16);
			mpEdgeSmooth_TempAccum = mpGraphics->GetTempFrameBuffer(mvScreenSize,ePixelFormat_RGBA,0)->GetColorBuffer(0)->ToTexture();

			//Frame buffers
			mpEdgeSmooth_LinearDepthBuffer = mpGraphics->CreateFrameBuffer("EdgeSmoothLinearDepth");
			mpEdgeSmooth_LinearDepthBuffer->SetTexture2D(0,mpEdgeSmooth_LinearDepthTexture);
			mpEdgeSmooth_LinearDepthBuffer->CompileAndValidate();

			/////////////////////////////////////
			// Programs

			cParserVarContainer programVars;

			//Program for unpacking depth
			if(mGBufferType == eDeferredGBuffer_32Bit)	programVars.Add("Deferred_32bit");
			else										programVars.Add("Deferred_64bit");
			programVars.Add("UseUv");
			mpEdgeSmooth_UnpackDepthProgram = mpGraphics->CreateGpuProgramFromShaders("EdgeSmoothUnpackDepth","deferred_base_vtx.glsl", "deferred_unpack_depth_frag.glsl",&programVars);
			if(mpEdgeSmooth_UnpackDepthProgram)
			{
				mpEdgeSmooth_UnpackDepthProgram->GetVariableAsId("afNegInvFarPlane",kVar_afNegInvFarPlane);
			}
			programVars.Clear();

			//Program for edge smoothing
			programVars.Add("UseUv");
			mpEdgeSmooth_RenderProgram =  mpGraphics->CreateGpuProgramFromShaders("EdgeSmoothRender","deferred_base_vtx.glsl", "deferred_edge_smooth_frag.glsl",&programVars);
			if(mpEdgeSmooth_RenderProgram)
			{
				mpEdgeSmooth_RenderProgram->GetVariableAsId("afFarPlane",kVar_afFarPlane);
			}
		}

		mpFxaaProgram = NULL;
		if(mGBufferTextureType != eTextureType_Rect)
		{
			cParserVarContainer programVars;
			programVars.Add("UseUv");
			// HPL3.5 ships FXAA 3.11; official cPostEffectParams_FXAA::mQuality defaults to 1
			programVars.Add("Quality_Medium");
			bool bFxaa311 = mpResources->GetFileSearcher()->GetFilePath("posteffect_fxaa_3_11_frag.hpsl") != _W("");
			mpFxaaProgram = mpGraphics->CreateGpuProgramFromShaders("Fxaa","deferred_base_vtx.glsl", bFxaa311 ? "posteffect_fxaa_3_11_frag.glsl" : "posteffect_fxaa_frag.glsl",&programVars);
			if(mpFxaaProgram)
				mpFxaaProgram->GetVariableAsId("avInvScreenSize",kVar_avInvScreenSize);
		}

		if(mGBufferTextureType != eTextureType_Rect)
		{
			const int lDofSamples = mlDepthOfFieldSampleNum;
			cParserVarContainer programVars;
			programVars.Add("UseUv");
			mpDofFocusProgram = mpGraphics->CreateGpuProgramFromShaders("DOF - Focus","deferred_base_vtx.glsl", "deferred_dof_focus.glsl",&programVars);
			programVars.Add("kNumSamples", lDofSamples);
			mpDofBlurProgram = mpGraphics->CreateGpuProgramFromShaders("DOF - Blur","deferred_base_vtx.glsl", "deferred_dof_blur_fs.glsl",&programVars);
			if(mpDofFocusProgram)
			{
				mpDofFocusProgram->GetVariableAsId("avFocusStartEnd",kVar_avFocusStartEnd);
				mpDofFocusProgram->GetVariableAsId("afFarPlane",kVar_afFarPlane);
			}
			if(mpDofBlurProgram)
			{
				mpDofBlurProgram->GetVariableAsId("avOffsetMul",kVar_avOffsetMul);
				if(mpDofBlurProgram->GetVariableAsId("afStrength",kVar_afWeight)) mpDofBlurProgram->SetFloat(kVar_afWeight, 1.0f);
			}

			int lRows = ohpl_dr_dof_rows(lDofSamples);
			std::vector<float> vTable((size_t)64*lRows*4, 0.0f);
			ohpl_dr_dof_table(lDofSamples, mvScreenSizeFloat.x, &vTable[0], vTable.size());
			mpDofGaussTexture = mpGraphics->CreateTexture("Guassian Lookup", eTextureType_2D, eTextureUsage_Normal);
			mpDofGaussTexture->SetUseMipMaps(false);
			mpDofGaussTexture->CreateFromRawData(cVector3l(64,lRows,1), ePixelFormat_RGBA16, (unsigned char*)&vTable[0]);
			mpDofGaussTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
			mpDofGaussTexture->SetFilter(eTextureFilter_Bilinear);
		}

		if(mbHpl3SSAO && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat))
		{
			cVector2l vSize = mvScreenSize / mlSSAOBufferSizeDiv;
			const char* vNames[3] = {"SSAO", "SSAOBlur1", "SSAOTemporal"};
			for(int i=0; i<3; ++i)
			{
				mpH3SSAOTexture[i] = mpGraphics->CreateTexture(vNames[i], eTextureType_2D, eTextureUsage_RenderTarget);
				mpH3SSAOTexture[i]->SetUseMipMaps(false);
				mpH3SSAOTexture[i]->CreateFromRawData(cVector3l(vSize.x, vSize.y, 1), ePixelFormat_RGB16, NULL);
				mpH3SSAOTexture[i]->SetWrapSTR(eTextureWrap_ClampToEdge);
				mpH3SSAOTexture[i]->SetFilter(eTextureFilter_Nearest);
				mpH3SSAOBuffer[i] = mpGraphics->CreateFrameBuffer(vNames[i]);
				mpH3SSAOBuffer[i]->SetTexture2D(0, mpH3SSAOTexture[i]);
				mpH3SSAOBuffer[i]->CompileAndValidate();
			}

			OhplDrSsaoMips ssaoMips;
			ohpl_dr_ssao_mips(OhplVec2l{vSize.x, vSize.y}, &ssaoMips);
			cVector2l vMipSize(ssaoMips.mipSize.x, ssaoMips.mipSize.y);
			int lMips = ssaoMips.mips;
			mpH3SSAOMipTexture = mpGraphics->CreateTexture("SSAOMip", eTextureType_2D, eTextureUsage_RenderTarget);
			mpH3SSAOMipTexture->SetUseMipMaps(true);
			mpH3SSAOMipTexture->CreateFromRawData(cVector3l(vMipSize.x, vMipSize.y, 1), ePixelFormat_RGB16, NULL);
			mpH3SSAOMipTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
			mpH3SSAOMipTexture->SetFilter(eTextureFilter_Nearest);
			int lMipLevels = ssaoMips.levels;
			for(int i=0; i<lMipLevels; ++i)
			{
				iFrameBuffer *pFB = mpGraphics->CreateFrameBuffer("SSAOMip" + cString::ToString(i));
				pFB->SetTexture2D(0, mpH3SSAOMipTexture, i);
				pFB->CompileAndValidate();
				mvH3SSAOMipBuffers.push_back(pFB);
			}

			cParserVarContainer vars;
			vars.Add("UseUv");
			vars.Add("SSAO_VERSION_050");
			vars.Add("UseUnpackNormal");
			vars.Add("UseRotatedGrid");
			mpH3SSAODownsampleProgram = mpGraphics->CreateGpuProgramFromShaders("SSAO Downsample", "deferred_base_vtx.glsl", "deferred_ssao_depth_downsample_frag.glsl", &vars);
			vars.Clear();
			vars.Add("UseUv");
			vars.Add("SSAO_VERSION_050");
			mpH3SSAOBlurProgram = mpGraphics->CreateGpuProgramFromShaders("SSAO Blur", "deferred_base_vtx.glsl", "deferred_ssao_blur_frag.glsl", &vars);
			mpH3SSAOTemporalProgram = mpGraphics->CreateGpuProgramFromShaders("SSAO Temporal", "deferred_base_vtx.glsl", "deferred_ssao_temporal_frag.glsl", &vars);
			// the shader's full-size branch reads depth (.x) as AO; the upsample branch handles div 1
			vars.Add("UseUpsample");
			vars.Add("UseTemporal");
			mpH3SSAOUpsampleProgram = mpGraphics->CreateGpuProgramFromShaders("SSAO Upsample", "deferred_base_vtx.glsl", "deferred_ssao_upsample_frag.glsl", &vars);
			vars.Clear();
			vars.Add("UseUv");
			vars.Add("SSAO_VERSION_050");
			vars.Add("kNumSamples", 16);
			vars.Add("kMaxLod", lMips - 1);
			if(mlSSAOBufferSizeDiv != 1) vars.Add("UseDownsample");
			mpH3SSAORenderProgram = mpGraphics->CreateGpuProgramFromShaders("SSAO Render", "deferred_base_vtx.glsl", "deferred_ssao_render_frag.glsl", &vars);

			if(mpH3SSAODownsampleProgram)
				mpH3SSAODownsampleProgram->GetVariableAsId("avInvScreenSize", kVar_avInvScreenSize);
			if(mpH3SSAORenderProgram)
			{
				iGpuProgram *p = mpH3SSAORenderProgram;
				p->GetVariableAsId("avUVToView0", kVar_avUVToView0);
				p->GetVariableAsId("avUVToView1", kVar_avUVToView1);
				p->GetVariableAsId("afT", kVar_afT);
				p->GetVariableAsId("afStepSizeMax", kVar_afStepSizeMax);
				p->GetVariableAsId("afFarPlane", kVar_afFarPlane);
				p->GetVariableAsId("afRadius", kVar_afRadius);
				p->GetVariableAsId("afScreenSizeDiv", kVar_afScreenSizeDiv);
				p->GetVariableAsId("afLodScale", kVar_afLodScale);
			}
			if(mpH3SSAOBlurProgram)
			{
				mpH3SSAOBlurProgram->GetVariableAsId("afFarPlane", kVar_afFarPlane);
				mpH3SSAOBlurProgram->GetVariableAsId("afDepthDifference", kVar_afDepthDifference);
				mpH3SSAOBlurProgram->GetVariableAsId("avDirection", kVar_avDirection);
			}
			if(mpH3SSAOTemporalProgram)
			{
				iGpuProgram *p = mpH3SSAOTemporalProgram;
				p->GetVariableAsId("afFarPlane", kVar_afFarPlane);
				p->GetVariableAsId("avUVToView0", kVar_avUVToView0);
				p->GetVariableAsId("avUVToView1", kVar_avUVToView1);
				p->GetVariableAsId("a_mtxTemporalView", kVar_a_mtxTemporalView);
				p->GetVariableAsId("a_mtxTemporalProjection", kVar_a_mtxTemporalProjection);
				p->GetVariableAsId("avScreenSize", kVar_avScreenSize);
				p->GetVariableAsId("afTemporalBlurAmount", kVar_afTemporalBlurAmount);
			}
			if(mpH3SSAOUpsampleProgram)
			{
				mpH3SSAOUpsampleProgram->GetVariableAsId("afFarPlane", kVar_afFarPlane);
				mpH3SSAOUpsampleProgram->GetVariableAsId("afSizeDiv", kVar_afSizeDiv);
				mpH3SSAOUpsampleProgram->GetVariableAsId("afPower", kVar_afPower);
			}
		}

		if(mbHpl3SSAO && mbDepthInNormalAlpha==false)
		{
			cParserVarContainer vars;
			mpNightVisionProgram = mpGraphics->CreateGpuProgramFromShaders("NightVision", "deferred_base_vtx.glsl", "deferred_nightvision_frag.glsl", &vars);
			if(mpNightVisionProgram)
			{
				iGpuProgram *p = mpNightVisionProgram;
				p->GetVariableAsId("afFarPlane", kVar_afFarPlane);
				p->GetVariableAsId("avLightColor", kVar_avLightColor);
				p->GetVariableAsId("avScreenToFarPlane", kVar_avScreenToFarPlane);
				p->GetVariableAsId("avInvScreenSize", kVar_avInvScreenSize);
				p->GetVariableAsId("afFalloffPow", kVar_afFalloffPow);
				p->GetVariableAsId("afMaxDistance", kVar_afMaxDistance);
			}
		}

		mpToneMapProgram = NULL;
		for(int i=0; i<64; ++i) mpToneMapPrograms[i] = NULL;
		mpBloomBrightPassProgram = mpBloomBlurProgram[0] = mpBloomBlurProgram[1] = mpImageTrailProgram = NULL;
		if(mbHdr)
		{
			mpToneMapProgram = GetToneMapProgram(0);

			cParserVarContainer bloomVars;
			bloomVars.Add("UseUv");
			mpBloomBrightPassProgram = mpGraphics->CreateGpuProgramFromShaders("BloomBrightPass","deferred_base_vtx.glsl", "posteffect_bloomhdr_brightpass_frag.glsl",&bloomVars);
			if(mpBloomBrightPassProgram)
			{
				mpBloomBrightPassProgram->GetVariableAsId("afBrightPass",kVar_afBrightPass);
				mpBloomBrightPassProgram->GetVariableAsId("avInvScreenSize",kVar_avInvScreenSize);
			}
			mpImageTrailProgram = mpGraphics->CreateGpuProgramFromShaders("ImageTrailHdr","deferred_base_vtx.glsl", "posteffect_image_trail_frag.glsl",&bloomVars);
			if(mpImageTrailProgram)
				mpImageTrailProgram->GetVariableAsId("afAlpha",kVar_afAlpha);
			mlBloomBlurSamples = 10;
			bloomVars.Add("kBlurSamples", mlBloomBlurSamples);
			for(int i=0; i<2; ++i)
			{
				if(i==1) bloomVars.Add("BlurHorisontal");
				mpBloomBlurProgram[i] = mpGraphics->CreateGpuProgramFromShaders("BloomBlur"+cString::ToString(i),"deferred_base_vtx.glsl", "posteffect_bloomhdr_blur_frag.glsl",&bloomVars);
				if(mpBloomBlurProgram[i]==NULL) continue;
				mpBloomBlurProgram[i]->GetVariableAsId("avWeights",kVar_avWeights);
				mpBloomBlurProgram[i]->GetVariableAsId("avOffsets",kVar_avOffsets);
			}
		}

		////////////////////////////////////
		//Create light shapes
		tFlag lVtxFlag = eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0;
		mpShapeSphere[eDeferredShapeQuality_High] = LoadVertexBufferFromMesh("core_12_12_sphere.dae",lVtxFlag,1.0f);	
		mpShapeSphere[eDeferredShapeQuality_Medium] = LoadVertexBufferFromMesh("core_7_7_sphere.dae",lVtxFlag,1.0f);
		mpShapeSphere[eDeferredShapeQuality_Low] = LoadVertexBufferFromMesh("core_5_5_sphere.dae",lVtxFlag,1.0f);

		mpShapePyramid = LoadVertexBufferFromMesh("core_pyramid.dae",lVtxFlag,1.0f);
		
		////////////////////////////////////
		//Quad used when rendering light.
		mpFullscreenLightQuad = CreateQuadVertexBuffer(eVertexBufferType_Software,0,1,0,mvScreenSizeFloat, true);
		
		////////////////////////////////////
		//Batch vertex buffer
		ohpl_dr_batch_sizes(mpShapeSphere[eDeferredShapeQuality_Low]->GetVertexNum(), mpShapeSphere[eDeferredShapeQuality_Low]->GetIndexNum(),
							mlMaxBatchLights, &mlMaxBatchVertices, &mlMaxBatchIndices);
		mpBatchBuffer = mpLowLevelGraphics->CreateVertexBuffer(	eVertexBufferType_Software,
																eVertexBufferDrawType_Quad, eVertexBufferUsageType_Stream,
																mlMaxBatchVertices,mlMaxBatchIndices);
		mpBatchBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		mpBatchBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
		mpBatchBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Byte,4);
		mpBatchBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);
		mpBatchBuffer->CreateElementArray(eVertexBufferElement_Texture1,eVertexBufferElementFormat_Float,3);

		mpBatchBuffer->Compile(0);
        
		return true;
	}

	//-----------------------------------------------------------------------

	
	void cRendererDeferred::DestroyData()
	{
		/////////////////////////
		//Vertex buffers
		if(mpBatchBuffer) hplDelete(mpBatchBuffer);

		if(mpFullscreenLightQuad) hplDelete(mpFullscreenLightQuad);
		if(mpGlareVtxBuffer) hplDelete(mpGlareVtxBuffer);

		for(int i=0;i< eDeferredShapeQuality_LastEnum; ++i)
		{
			if(mpShapeSphere[i]) hplDelete(mpShapeSphere[i]);
		}
		if(mpShapePyramid) hplDelete(mpShapePyramid);
		
		
		/////////////////////////
		//G-Buffer and Accum buffer
		for(int type=0; type<2; ++type)
		{
			for(int j=0; j<eGBufferComponents_LastEnum; ++j)
			{
				mpGraphics->DestroyFrameBuffer(mpGBuffer[type][j]);	
			}
			
			for(int j=0; j<mlNumOfGBufferTextures; ++j)
			{
				mpGraphics->DestroyTexture(mpGBufferTexture[type][j]);	
			}

			mpGraphics->DestoroyDepthStencilBuffer(mpDepthStencil[type]);
		}
		
		mpGraphics->DestroyFrameBuffer(mpAccumBuffer);	
		if(mpBoxWeightBuffer) mpGraphics->DestroyFrameBuffer(mpBoxWeightBuffer);
		if(mpBoxWeightTexture) mpGraphics->DestroyTexture(mpBoxWeightTexture);
		mpGraphics->DestroyFrameBuffer(mpReflectionBuffer);
		
		mpGraphics->DestroyTexture(mpAccumBufferTexture);
		mpGraphics->DestroyTexture(mpReflectionTexture);
		
		
		/////////////////////////
		//Shadow textures
		DestroyShadowMaps();
		if(mpDirShadowData) DestroyShadowMap(mpDirShadowData);
		mpDirShadowData = NULL;
		
		if(mpShadowJitterTexture) mpGraphics->DestroyTexture(mpShadowJitterTexture);

		/////////////////////////
		//Fog stuff
		hplDelete(mpFogProgramManager);

		/////////////////////////
		//SSAO textures and programs
		if(mbSSAOLoaded)
		{
			mpGraphics->DestroyTexture(mpLinearDepthTexture);
			mpGraphics->DestroyTexture(mpSSAOTexture);
			mpGraphics->DestroyTexture(mpSSAOBlurTexture);
			mpGraphics->DestroyTexture(mpSSAOScatterDisk);

			mpGraphics->DestroyFrameBuffer(mpLinearDepthBuffer);
			mpGraphics->DestroyFrameBuffer(mpSSAOBuffer);
			mpGraphics->DestroyFrameBuffer(mpSSAOBlurBuffer);

			mpGraphics->DestroyGpuProgram(mpUnpackDepthProgram);
			for(int i=0;i<2; ++i)
				mpGraphics->DestroyGpuProgram(mpSSAOBlurProgram[i]);
			mpGraphics->DestroyGpuProgram(mpSSAORenderProgram);
		}

		/////////////////////////////
		// Edge smooth
		if(mbEdgeSmoothLoaded)
		{
			mpGraphics->DestroyTexture(mpEdgeSmooth_LinearDepthTexture);
			mpGraphics->DestroyFrameBuffer(mpEdgeSmooth_LinearDepthBuffer);
			
			mpGraphics->DestroyGpuProgram(mpEdgeSmooth_UnpackDepthProgram);
			mpGraphics->DestroyGpuProgram(mpEdgeSmooth_RenderProgram);
		}
		
		if(mpFxaaProgram) mpGraphics->DestroyGpuProgram(mpFxaaProgram);
		if(mpDofFocusProgram) mpGraphics->DestroyGpuProgram(mpDofFocusProgram);
		for(int i=0; i<3; ++i)
		{
			if(mpH3SSAOBuffer[i]) mpGraphics->DestroyFrameBuffer(mpH3SSAOBuffer[i]);
			if(mpH3SSAOTexture[i]) mpGraphics->DestroyTexture(mpH3SSAOTexture[i]);
		}
		for(size_t i=0; i<mvH3SSAOMipBuffers.size(); ++i) mpGraphics->DestroyFrameBuffer(mvH3SSAOMipBuffers[i]);
		mvH3SSAOMipBuffers.clear();
		if(mpH3SSAOMipTexture) mpGraphics->DestroyTexture(mpH3SSAOMipTexture);
		iGpuProgram *vH3SSAOPrograms[] = {mpH3SSAODownsampleProgram, mpH3SSAORenderProgram, mpH3SSAOBlurProgram, mpH3SSAOTemporalProgram, mpH3SSAOUpsampleProgram};
		for(int i=0; i<5; ++i) if(vH3SSAOPrograms[i]) mpGraphics->DestroyGpuProgram(vH3SSAOPrograms[i]);
		if(mpNightVisionProgram) mpGraphics->DestroyGpuProgram(mpNightVisionProgram);
		if(mpDofBlurProgram) mpGraphics->DestroyGpuProgram(mpDofBlurProgram);
		if(mpDofGaussTexture) mpGraphics->DestroyTexture(mpDofGaussTexture);
		for(int i=0; i<3; ++i) for(int j=0; j<2; ++j) if(mpBoxWeightedProgram[i][j]) mpGraphics->DestroyGpuProgram(mpBoxWeightedProgram[i][j]);
		if(mpBoxResolveProgram) mpGraphics->DestroyGpuProgram(mpBoxResolveProgram);
		for(int i=0; i<64; ++i) if(mpToneMapPrograms[i]) mpGraphics->DestroyGpuProgram(mpToneMapPrograms[i]);
		iGpuProgram *vBloomPrograms[] = {mpBloomBrightPassProgram, mpBloomBlurProgram[0], mpBloomBlurProgram[1], mpImageTrailProgram};
		for(int i=0; i<4; ++i) if(vBloomPrograms[i]) mpGraphics->DestroyGpuProgram(vBloomPrograms[i]);

		/////////////////////////
		//Gpu programs
		mpGraphics->DestroyGpuProgram(mpSkyBoxProgram);
		if(mpLensFlareGlareProgram) mpGraphics->DestroyGpuProgram(mpLensFlareGlareProgram);
		if(mpLensFlareIrisProgram) mpGraphics->DestroyGpuProgram(mpLensFlareIrisProgram);
		mpGraphics->DestroyTexture(mpWhiteCubeTexture);
		if(mpFogNoiseTexture) mpResources->GetTextureManager()->Destroy(mpFogNoiseTexture);
		mpFogNoiseTexture = NULL;

		mpProgramManager->DestroyShadersAndPrograms();
	}

	//-----------------------------------------------------------------------

	iTexture* cRendererDeferred::GetPostEffectTexture()
	{
		if(mpToneMapProgram) return mpGraphics->GetTempFrameBuffer(mvScreenSize,ePixelFormat_RGBA,6)->GetColorBuffer(0)->ToTexture();
		return mpAccumBufferTexture;

		//This should never be needed since if a post effect is used, 
		/*if(mpCurrentRenderTarget->mpFrameBuffer==NULL)
		{
			return mpAccumBuffer;
		}
		else
		{
			return mpCurrentRenderTarget->mpFrameBuffer;
		}*/
	}

	iTexture* cRendererDeferred::GetGbufferTexture(int alIdx)
	{ 
		int lType = mpCurrentSettings && mpCurrentSettings->mbIsReflection ? 1 : 0;
		return mpGBufferTexture[lType][alIdx];
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cRendererDeferred::DrawAccumulationQuad()
	{
		OhplVec2f vUvMin, vUvMax;
		ohpl_dr_accum_quad_uv(OhplVec2l{mpCurrentRenderTarget->mvPos.x, mpCurrentRenderTarget->mvPos.y}, OhplVec2l{mvRenderTargetSize.x, mvRenderTargetSize.y},
							  OhplVec2f{mvScreenSizeFloat.x, mvScreenSizeFloat.y}, mGBufferTextureType == eTextureType_Rect, &vUvMin, &vUvMax);
		DrawQuad(cVector2f(0,0),1, cVector2f(vUvMin.x, vUvMin.y), cVector2f(vUvMax.x, vUvMax.y), true);
	}

	// px_vPosition is gl_FragCoord in the whole G-buffer; a smaller target sits at its top left
	void cRendererDeferred::SetScreenPositionVars(iGpuProgram *apProgram)
	{
		iTexture *pGBuffer = GetGbufferTexture(0);
		OhplDrFarPlane far = {mfFarPlane, mfFarTop, mfFarBottom, mfFarRight, mfFarLeft};
		float vToFar[4], vInvSize[2];
		ohpl_dr_screen_position_vars(&far, OhplVec2l{mvRenderTargetSize.x, mvRenderTargetSize.y}, OhplVec2l{mpCurrentRenderTarget->mvPos.x, mpCurrentRenderTarget->mvPos.y},
									 OhplVec2l{pGBuffer->GetWidth(), pGBuffer->GetHeight()}, vToFar, vInvSize);
		apProgram->SetVec4f(kVar_avScreenToFarPlane, vToFar[0], vToFar[1], vToFar[2], vToFar[3]);
		apProgram->SetVec2f(kVar_avInvScreenSize, vInvSize[0], vInvSize[1]);
	}

	void cRendererDeferred::CopyToFrameBuffer()
	{
		if(mpCurrentSettings->mbIsReflection) return;
		CopyAccumTo(mpCurrentRenderTarget->mpFrameBuffer);
	}

	void cRendererDeferred::CopyAccumTo(iFrameBuffer *apTarget)
	{
		START_RENDER_PASS(CopyToFrameBuffer);

		iFrameBuffer *vFbs[kOhplDrFbCount] = {};
		iTexture *vTexs[kOhplDrTexCount] = {};
		iGpuProgram *vProgs[kOhplDrProgCount] = {};
		vFbs[kOhplDrFbTarget] = apTarget;
		vFbs[kOhplDrFbAccum] = mpAccumBuffer;
		vTexs[kOhplDrTexAccum] = mpAccumBufferTexture;
		vTexs[kOhplDrTexGrading] = mpColorGradingTexture;
		vTexs[kOhplDrTexGradingTarget] = mpColorGradingTarget;
		vTexs[kOhplDrTexNoise] = mpFilmGrainNoise;
		vProgs[kOhplDrProgFxaa] = mpFxaaProgram;
		vProgs[kOhplDrProgTrail] = mpImageTrailProgram;
		vProgs[kOhplDrProgBright] = mpBloomBrightPassProgram;
		vProgs[kOhplDrProgBlur0] = mpBloomBlurProgram[0];
		vProgs[kOhplDrProgBlur1] = mpBloomBlurProgram[1];

		std::vector<float> vLayerWeights;
		for(size_t i=0; i<mvColorGradingLayers.size(); ++i)
		{
			vLayerWeights.push_back(mvColorGradingLayers[i].second);
			if(i < 8) vTexs[kOhplDrTexLayer0 + i] = mvColorGradingLayers[i].first;
		}

		OhplDrCopyIn in = {};
		in.screen = OhplVec2l{mvScreenSize.x, mvScreenSize.y};
		in.screenF = OhplVec2f{mvScreenSizeFloat.x, mvScreenSizeFloat.y};
		in.rtPos = OhplVec2l{mpCurrentRenderTarget->mvPos.x, mpCurrentRenderTarget->mvPos.y};
		in.rtSize = OhplVec2l{mvRenderTargetSize.x, mvRenderTargetSize.y};
		in.rectType = mGBufferTextureType == eTextureType_Rect;
		in.useFxaa = mpCurrentSettings->mbUseFxaa;
		in.fxaaOk = mpFxaaProgram != NULL;
		in.tonemapOk = mpToneMapProgram != NULL;
		in.trailOk = mpImageTrailProgram != NULL;
		in.hpl35 = mbHpl3SSAO && mbDepthInNormalAlpha==false;
		in.grading = mpColorGradingTexture != NULL;
		in.gradingTarget = mpColorGradingTarget != NULL;
		in.noise = mpFilmGrainNoise != NULL;
		in.bloom = mbBloom;
		in.bloomProgramsOk = mpBloomBrightPassProgram && mpBloomBlurProgram[0] && mpBloomBlurProgram[1];
		in.srgb = mbToneMapSRGB;
		in.trailAlpha = mfImageTrailAlpha;
		in.skipPasses = mlDebugSkipPasses;
		in.noiseWidth = mpFilmGrainNoise ? mpFilmGrainNoise->GetWidth() : 0;
		in.key = mfToneMapKey;
		in.exposure = mfToneMapExposure;
		in.whiteCut = mfToneMapWhiteCut;
		in.gamma = mfToneMapGamma;
		in.bloomBright = mfBloomBrightPass;
		in.bloomWidth = mfBloomWidth;
		in.bloomTint[0] = mBloomTint.r; in.bloomTint[1] = mBloomTint.g; in.bloomTint[2] = mBloomTint.b; in.bloomTint[3] = mBloomTint.a;
		in.gradingBlend = mfColorGradingBlend;
		in.filmIntensity = mfFilmGrainIntensity;
		in.frameTime = mfCurrentFrameTime;
		in.blurSamples = mlBloomBlurSamples;
		in.layerWeights = vLayerWeights.empty() ? NULL : &vLayerWeights[0];
		in.layerCount = vLayerWeights.size();

		OhplDrFilmGrain grain;
		grain.t = mfFilmGrainT;
		memcpy(grain.transform, mvFilmGrainTransform, sizeof(grain.transform));
		OhplDrPlan *pPlan = ohpl_dr_plan_copy_accum(&in, &grain);

		RunPlan(pPlan, vFbs, vTexs, vProgs, false);

		ohpl_dr_plan_film_grain(pPlan, &grain);
		mfFilmGrainT = grain.t;
		memcpy(mvFilmGrainTransform, grain.transform, sizeof(mvFilmGrainTransform));
		if(ohpl_dr_plan_trail_consumed(pPlan)) mfImageTrailAlpha = 0;
		ohpl_dr_plan_free(pPlan);

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	iTexture* cRendererDeferred::GetDebugGBufferTexture(int alIdx)
	{
		int vShadowCounts[3];
		for(int r=0; r<3; ++r) vShadowCounts[r] = (int)mvShadowMapData[r].size();
		OhplDrDebugTex tex;
		ohpl_dr_debug_gbuffer_texture(alIdx, vShadowCounts, &tex);
		switch(tex.kind)
		{
		case kOhplDrDebugShadow:		return mvShadowMapData[tex.a][tex.b]->mpTexture;
		case kOhplDrDebugTemp:			return mpGraphics->GetTempFrameBuffer(mvScreenSize/(4<<tex.a),ePixelFormat_RGBA16,tex.b)->GetColorBuffer(0)->ToTexture();
		case kOhplDrDebugDirShadow:		return mpDirShadowData ? mpDirShadowData->mpTexture : NULL;
		case kOhplDrDebugRefraction:	return mpRefractionTexture;
		case kOhplDrDebugReflection:	return GetReflectionTexture();
		case kOhplDrDebugBoxWeight:		return mpBoxWeightTexture;
		case kOhplDrDebugH3Ssao:		return mpH3SSAOTexture[tex.a];
		case kOhplDrDebugH3Mip:			return mpH3SSAOMipTexture;
		case kOhplDrDebugAccum:			return mpAccumBufferTexture;
		case kOhplDrDebugBuffer:		return tex.a >= 0 && tex.a < 5 ? GetBufferTexture(tex.a) : NULL;
		}
		return NULL;
	}

	iGpuProgram* cRendererDeferred::GetToneMapProgram(int alCombo)
	{
		iGpuProgram *&pProg = mpToneMapPrograms[alCombo];
		if(pProg) return pProg;

		cParserVarContainer programVars;
		bool bHpl35 = mbHpl3SSAO && mbDepthInNormalAlpha==false;
		static const char *vDefineNames[] = {"UseUv", "UseColorGrading", "UseBloom", "UseNoise", "UseFilmGrain", "UseSRGB",
											 "LayeredColorGrading", "UseBlendGrading", "UseHdrOutput"};
		int vDefines[16];
		size_t lDefineNum = ohpl_dr_tonemap_defines(alCombo, bHpl35, vDefines, 16);
		for(size_t i=0; i<lDefineNum && i<16; ++i) programVars.Add(vDefineNames[vDefines[i]]);
		pProg = mpGraphics->CreateGpuProgramFromShaders("ToneMapping"+cString::ToString(alCombo),"deferred_base_vtx.glsl", "posteffect_tonemapping_frag.glsl",&programVars);
		if(pProg==NULL) return NULL;
		pProg->GetVariableAsId("afKey",kVar_afKey);
		pProg->GetVariableAsId("afExposure",kVar_afExposure);
		pProg->GetVariableAsId("afWhiteCut",kVar_afWhiteCut);
		pProg->GetVariableAsId("afHdrHeadroom",kVar_afHdrHeadroom);
		pProg->GetVariableAsId("afInvGammaCorrection",kVar_afInvGammaCorrection);
		pProg->GetVariableAsId("avInvScreenSize",kVar_avInvScreenSize);
		pProg->GetVariableAsId("avBloomTint",kVar_avBloomTint);
		pProg->GetVariableAsId("avSizeWeight",kVar_avSizeWeight);
		pProg->GetVariableAsId(bHpl35 ? "afTime" : "afT",kVar_afT);
		pProg->GetVariableAsId("afIntensity",kVar_afIntensity);
		pProg->GetVariableAsId("avTransform0",kVar_avTransform0);
		pProg->GetVariableAsId("avTransform1",kVar_avTransform1);
		pProg->GetVariableAsId(bHpl35 ? "afGradingMapAlpha[1]" : "afGradingBlendWeight",kVar_afGradingBlendWeight);
		pProg->GetVariableAsId("afGradingMapAlpha[0]",kVar_afGradingMapAlpha0);
		pProg->GetVariableAsId("alGradingMapNum",kVar_alGradingMapNum);
		if(bHpl35 && (alCombo & 16))
			for(int i=2; i<8; ++i) pProg->GetVariableAsId("afGradingMapAlpha["+cString::ToString(i)+"]",kVar_afGradingMapAlpha2+i-2);
		return pProg;
	}


	//-----------------------------------------------------------------------
	
	void cRendererDeferred::SetupRenderList()
	{
		mpCurrentRenderList->Setup(mfCurrentFrameTime,mpCurrentFrustum);
	}

		//-----------------------------------------------------------------------
	
	void cRendererDeferred::RenderObjects()
	{
		OhplDrObjectsIn in = {};
		in.useOcclusionCulling = mpCurrentSettings->mbUseOcclusionCulling;
		in.useDelayedOcclusion = mpCurrentSettings->mbUseDelayedOcclusionCulling;
		in.debugRenderBuffers = mbDebugRenderFrameBuffers;
		in.occlusionTestLargeLights = mbOcclusionTestLargeLights;
		in.sendToPostEffects = mbSendFrameBufferToPostEffects;
		in.tonemapOk = mpToneMapProgram != NULL;
		in.isReflection = mpCurrentSettings->mbIsReflection;
		in.skipPasses = mlDebugSkipPasses;
		RunPlan(ohpl_dr_plan_objects(in), NULL, NULL, NULL);
	}

	void cRendererDeferred::RunStep(int alStep, int alArg)
	{
		tRenderableFlag lVisibleFlags = mpCurrentSettings->mbIsReflection ? eRenderableFlag_VisibleInReflection : eRenderableFlag_VisibleInNonReflection;

		switch(alStep)
		{
		case eOhplDrStep_SetupRenderVariables:		SetupRenderVariables(); break;
		case eOhplDrStep_SetupGBuffer:				SetupGBuffer(); break;
		case eOhplDrStep_CheckVisibleZ:				CheckForVisibleObjectsAddToListAndRenderZ(mpCurrentSettings->mpVisibleNodeTracker,eObjectVariabilityFlag_All, lVisibleFlags, true, NULL); break;
		case eOhplDrStep_CheckVisibleStatic:		CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), lVisibleFlags); break;
		case eOhplDrStep_CheckVisibleDynamic:		CheckForVisibleAndAddToList(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), lVisibleFlags); break;
		case eOhplDrStep_FetchDelayedOcclusion:		FetchDelayedOcclusionResults(); break;
		case eOhplDrStep_AssignOcclusionQueries:	AssignAndRenderOcclusionQueryObjects(false, NULL, true); break;
		case eOhplDrStep_SetupLights:				SetupLightsAndRenderQueries(); break;
		case eOhplDrStep_CompileOcclusion:
			mpCurrentRenderList->Compile(	eRenderListCompileFlag_Diffuse |
											eRenderListCompileFlag_Translucent |
											eRenderListCompileFlag_Decal |
											eRenderListCompileFlag_Illumination);
			if(mbLog)mpCurrentRenderList->PrintAllObjects();
			break;
		case eOhplDrStep_CompileBruteForce:
			mpCurrentRenderList->Compile(	eRenderListCompileFlag_Z |
											eRenderListCompileFlag_Diffuse |
											eRenderListCompileFlag_Translucent |
											eRenderListCompileFlag_Decal |
											eRenderListCompileFlag_Illumination);
			if(mbLog)mpCurrentRenderList->PrintAllObjects();
			break;
		case eOhplDrStep_RenderZ:					RenderZ(); break;
		case eOhplDrStep_RenderDelayedOcclusionQueries:	RenderDelayedOcclusionQueries(); break;
		case eOhplDrStep_RenderGbuffer:				RenderGbuffer(); break;
		case eOhplDrStep_RenderGbufferContent:		RenderGbufferContent(); break;
		case eOhplDrStep_RenderDecals:				RenderDecals(); break;
		case eOhplDrStep_CallbackPostGBuffer:		RunCallback(eRendererMessage_PostGBuffer); break;
		case eOhplDrStep_RenderLights:				RenderLights(); break;
		case eOhplDrStep_RenderIllumination:		RenderIllumination(); break;
		case eOhplDrStep_RenderFog:					RenderFog(); break;
		case eOhplDrStep_RenderFullScreenFog:		RenderFullScreenFog(); break;
		case eOhplDrStep_RenderEdgeSmooth:			RenderEdgeSmooth(); break;
		case eOhplDrStep_RenderBasicSkyBox:			RenderBasicSkyBox(); break;
		case eOhplDrStep_CallbackPostSolid:			RunCallback(eRendererMessage_PostSolid); break;
		case eOhplDrStep_UpdateLensFlares:			UpdateLensFlares(); break;
		case eOhplDrStep_RenderEnvironmentParticles:	RenderEnvironmentParticles(alArg != 0); break;
		case eOhplDrStep_RenderTranslucent:			RenderTranslucent(alArg); break;
		case eOhplDrStep_RenderDepthOfField:		RenderDepthOfField(); break;
		case eOhplDrStep_RenderMultiIrisAndGlare:	RenderMultiIrisAndGlare(); break;
		case eOhplDrStep_CallbackPostTranslucent:	RunCallback(eRendererMessage_PostTranslucent); break;
		case eOhplDrStep_RetrieveLightOcclusion:	RetrieveAllLightOcclusionPair(false); break;
		case eOhplDrStep_CopyAccumToPostEffects:	CopyAccumTo(mpGraphics->GetTempFrameBuffer(mvScreenSize,ePixelFormat_RGBA,6)); break;
		}
	}

	void cRendererDeferred::RunPlan(OhplDrPlan *apPlan, iFrameBuffer **apFbs, iTexture **apTexs, iGpuProgram **apProgs, bool abFree, cDeferredLight *apLight)
	{
		auto Fb = [&](int i) { return i >= 0 ? apFbs[i] : (iFrameBuffer*)NULL; };
		auto Tex = [&](int i) { return i >= 0 ? apTexs[i] : (iTexture*)NULL; };
		auto Prog = [&](int i) { return i >= 0 ? apProgs[i] : (iGpuProgram*)NULL; };
		auto Light = [&](int i) { return i >= 0 && i < (int)mvTempDeferredLights.size() ? mvTempDeferredLights[i] : (cDeferredLight*)NULL; };
		cShadowMapData *pShadowData = NULL;
		OhplDrOp op;
		while(ohpl_dr_plan_next(apPlan, &op))
		{
			switch(op.kind)
			{
			case eOhplDrOp_State:
				switch(op.a)
				{
				case eOhplDrState_DepthTest:	SetDepthTest(op.b != 0); break;
				case eOhplDrState_DepthWrite:	SetDepthWrite(op.b != 0); break;
				case eOhplDrState_DepthFunc:
					SetDepthTestFunc(op.b == kOhplDrFuncEqual ? eDepthTestFunc_Equal : op.b == kOhplDrFuncGreaterOrEqual ? eDepthTestFunc_GreaterOrEqual : eDepthTestFunc_LessOrEqual);
					break;
				case eOhplDrState_Blend:
					SetBlendMode(op.b == kOhplDrBlendAlpha ? eMaterialBlendMode_Alpha : op.b == kOhplDrBlendAdd ? eMaterialBlendMode_Add :
								 op.b == kOhplDrBlendMul ? eMaterialBlendMode_Mul : eMaterialBlendMode_None);
					break;
				case eOhplDrState_Alpha:		SetAlphaMode(eMaterialAlphaMode_Solid); break;
				case eOhplDrState_Channel:
					SetChannelMode(op.b == kOhplDrChannelNone ? eMaterialChannelMode_None : op.b == kOhplDrChannelRgb ? eMaterialChannelMode_RGB : eMaterialChannelMode_RGBA);
					break;
				}
				break;
			case eOhplDrOp_FlatProjection:		SetFlatProjection(); break;
			case eOhplDrOp_SetFb:				SetFrameBuffer(Fb(op.a), op.b != 0); break;
			case eOhplDrOp_TexRangeNull:		SetTextureRange(NULL, op.a); break;
			case eOhplDrOp_Program:				SetProgram(Prog(op.a)); break;
			case eOhplDrOp_Tex:					SetTexture(op.a, Tex(op.b)); break;
			case eOhplDrOp_AcquireTemp:
			{
				iFrameBuffer *pFb = mpGraphics->GetTempFrameBuffer(cVector2l(op.c, op.d), (op.e & 0xff) == kOhplDrFormatRgba16 ? ePixelFormat_RGBA16 : ePixelFormat_RGBA, op.e >> 8);
				apFbs[op.a] = pFb;
				apTexs[op.b] = pFb ? pFb->GetColorBuffer(0)->ToTexture() : NULL;
				break;
			}
			// a null program is skipped: the original edge smooth pass dereferenced it
			case eOhplDrOp_Float:				if(Prog(op.a)) Prog(op.a)->SetFloat(op.b, op.f[0]); break;
			case eOhplDrOp_Int:					if(Prog(op.a)) Prog(op.a)->SetInt(op.b, op.c); break;
			case eOhplDrOp_Vec2:				if(Prog(op.a)) Prog(op.a)->SetVec2f(op.b, cVector2f(op.f[0], op.f[1])); break;
			case eOhplDrOp_Vec3:				if(Prog(op.a)) Prog(op.a)->SetVec3f(op.b, cVector3f(op.f[0], op.f[1], op.f[2])); break;
			case eOhplDrOp_Vec4:				if(Prog(op.a)) Prog(op.a)->SetVec4f(op.b, op.f[0], op.f[1], op.f[2], op.f[3]); break;
			case eOhplDrOp_FloatArr:			if(Prog(op.a)) Prog(op.a)->SetFloatArray(op.b, ohpl_dr_plan_array(apPlan, op.c), op.d); break;
			case eOhplDrOp_Quad:				DrawQuad(cVector2f(0,0),1, cVector2f(op.f[0], op.f[1]), cVector2f(op.f[2], op.f[3]), true); break;
			case eOhplDrOp_SetGBuffer:			SetGBuffer((eGBufferComponents)op.a); break;
			case eOhplDrOp_ClearDepthValue:		mpLowLevelGraphics->SetClearDepth(op.f[0]); break;
			case eOhplDrOp_ClearColorValue:		mpLowLevelGraphics->SetClearColor(cColor(op.f[0], op.f[1], op.f[2], op.f[3])); break;
			case eOhplDrOp_ClearColorSettings:	mpLowLevelGraphics->SetClearColor(mpCurrentSettings->mClearColor); break;
			case eOhplDrOp_ClearFb:
				ClearFrameBuffer(((op.a & kOhplDrClearDepth) ? eClearFrameBufferFlag_Depth : 0) | ((op.a & kOhplDrClearColor) ? eClearFrameBufferFlag_Color : 0), op.b != 0);
				break;
			case eOhplDrOp_Ask:
				switch(op.a)
				{
				case eOhplDrQuery_Headroom:		ohpl_dr_plan_feed_float(apPlan, mpLowLevelGraphics->GetHdrHeadroom()); break;
				case eOhplDrQuery_FogAfterAreas:	ohpl_dr_plan_feed_bool(apPlan, mpCurrentWorld->GetFogApplyAfterFogAreas()); break;
				case eOhplDrQuery_FogIsSkybox:
					ohpl_dr_plan_feed_bool(apPlan, WorldFogActive() && mpCurrentWorld->GetFogUseSkybox() &&
												   mpCurrentWorld->GetFogSkyboxTexture() == mpCurrentWorld->GetSkyBoxTexture());
					break;
				case eOhplDrQuery_DepthOfField:	ohpl_dr_plan_feed_bool(apPlan, DepthOfFieldIsActive()); break;
				}
				break;
			case eOhplDrOp_TryToneMap:
				apProgs[kOhplDrProgTonemap] = GetToneMapProgram(op.a);
				ohpl_dr_plan_feed_bool(apPlan, apProgs[kOhplDrProgTonemap] != NULL);
				break;
			case eOhplDrOp_Step:				RunStep(op.a, op.b); break;
			case eOhplDrOp_FlatProjectionMinMax:	SetFlatProjectionMinMax(cVector3f(op.f[0], op.f[1], op.f[2]), cVector3f(op.f[3], op.f[4], op.f[5])); break;
			case eOhplDrOp_NormalFrustumProjection:	SetNormalFrustumProjection(); break;
			case eOhplDrOp_QuadAt:
				DrawQuad(cVector3f(op.f[0], op.f[1], op.f[2]), cVector2f(op.f[3], op.f[4]), cVector2f(op.f[5], op.f[6]), cVector2f(op.f[7], op.f[8]), true);
				break;
			case eOhplDrOp_CopyFbToTex:			CopyFrameBufferToTexure(Tex(op.a), 0, cVector2l(op.b, op.c), 0, true); break;
			case eOhplDrOp_SetAccumBuffer:		SetAccumulationBuffer(); break;
			case eOhplDrOp_Matrix:
			{
				cMatrixf mtx;
				memcpy(&mtx, ohpl_dr_plan_array(apPlan, op.c), sizeof(OhplMat4));
				if(Prog(op.a)) Prog(op.a)->SetMatrixf(op.b, mtx);
				break;
			}
			case eOhplDrOp_InvViewMatrix:		if(Prog(op.a)) Prog(op.a)->SetMatrixf(op.b, m_mtxInvView); break;
			case eOhplDrOp_ScreenPositionVars:	if(Prog(op.a)) SetScreenPositionVars(Prog(op.a)); break;
			case eOhplDrOp_GenProgram:
				apProgs[op.a] = mpProgramManager->GenerateProgram(op.b, (tFlag)op.c);
				ohpl_dr_plan_feed_bool(apPlan, apProgs[op.a] != NULL);
				break;
			case eOhplDrOp_AskGeom:
			{
				iLight *pLight = op.b >= 0 && op.b < mpCurrentRenderList->GetLightNum() ? mpCurrentRenderList->GetLight(op.b) : NULL;
				switch(op.a)
				{
				case eOhplDrGeom_SphereNearPlane:
					ohpl_dr_plan_feed_bool(apPlan, mpCurrentFrustum->CheckSphereNearPlaneIntersection(cVector3f(op.f[0], op.f[1], op.f[2]), op.f[3]));
					break;
				case eOhplDrGeom_FrustumNearPlane:
					ohpl_dr_plan_feed_bool(apPlan, mpCurrentFrustum->CheckFrustumNearPlaneIntersection(static_cast<cLightSpot*>(pLight)->GetFrustum()));
					break;
				case eOhplDrGeom_LineIntersection:
				{
					cFrustum *pSpotFrustum = static_cast<cLightSpot*>(pLight)->GetFrustum();
					cVector3f vHit = pSpotFrustum->GetOrigin();
					pSpotFrustum->CheckLineIntersection(mpCurrentFrustum->GetOrigin(), pLight->GetBoundingVolume()->GetWorldCenter(), vHit);
					ohpl_dr_plan_feed_vec3(apPlan, vHit.x, vHit.y, vHit.z);
					break;
				}
				case eOhplDrGeom_BoxNearPlane:
					ohpl_dr_plan_feed_bool(apPlan, mpCurrentFrustum->CheckBVNearPlaneIntersection(pLight->GetBoundingVolume()));
					break;
				case eOhplDrGeom_QueryFetch:		ohpl_dr_plan_feed_bool(apPlan, Light(op.b)->mpQuery->FetchResults()); break;
				case eOhplDrGeom_QuerySamples:		ohpl_dr_plan_feed_int(apPlan, Light(op.b)->mpQuery->GetSampleCount()); break;
				case eOhplDrGeom_ShadowNeedsUpdate:	ohpl_dr_plan_feed_bool(apPlan, ShadowMapNeedsUpdate(apLight->mpLight, pShadowData)); break;
				}
				break;
			}
			case eOhplDrOp_Dl:
				switch(op.a)
				{
				case eOhplDrDl_ClearPairs:		mpCurrentSettings->mvLightOcclusionPairs.resize(0); break;
				case eOhplDrDl_ClearTempLights:
					STLDeleteAll_NoClear(mvTempDeferredLights);
					mvTempDeferredLights.resize(0);
					break;
				case eOhplDrDl_SetMinLargeArea:	mlMinLargeLightArea = op.b; break;
				case eOhplDrDl_Light:
				{
					cDeferredLight *pLightData = hplNew( cDeferredLight, () );
					OhplDlLightOut out;
					ohpl_dl_plan_light_out(apPlan, op.b, &out);
					pLightData->mpLight = mpCurrentRenderList->GetLight(op.b);
					pLightData->mpQuery = NULL;
					memcpy(&pLightData->m_mtxViewSpaceRender, &out.render, sizeof(OhplMat4));
					memcpy(&pLightData->m_mtxViewSpaceTransform, &out.transform, sizeof(OhplMat4));
					pLightData->mClipRect = cRect2l(out.clip.x, out.clip.y, out.clip.w, out.clip.h);
					pLightData->mlArea = out.area;
					pLightData->mbInsideNearPlane = out.insideNearPlane;
					pLightData->mbCastShadows = out.castShadows;
					pLightData->mShadowResolution = (eShadowMapResolution)out.shadowRes;
					mvTempDeferredLights.push_back(pLightData);
					break;
				}
				case eOhplDrDl_AcquireQuery:	Light(op.b)->mpQuery = GetOcclusionQuery(); break;
				case eOhplDrDl_SetModelView:	SetModelViewMatrix(Light(op.b)->m_mtxViewSpaceRender); break;
				case eOhplDrDl_SetShape:		SetVertexBuffer(GetLightShapeBuffer(op.b)); break;
				case eOhplDrDl_QueryBegin:		Light(op.b)->mpQuery->Begin(); break;
				case eOhplDrDl_QueryEnd:		Light(op.b)->mpQuery->End(); break;
				case eOhplDrDl_DrawCurrent:		DrawCurrent(); break;
				case eOhplDrDl_Flush:			mpLowLevelGraphics->FlushRendering(); break;
				case eOhplDrDl_InvView:			ohpl_dl_plan_inv_view(apPlan, (OhplMat4*)&m_mtxInvView); break;
				case eOhplDrDl_UpdateFullscreenQuad:
					UpdateqQuadVertexPostion(mpFullscreenLightQuad, cVector3f(op.f[0], op.f[1], op.f[2]), cVector2f(op.f[3], op.f[4]), true);
					break;
				case eOhplDrDl_LightMatrix:
				{
					OhplDlLightOut out;
					ohpl_dl_plan_light_out(apPlan, op.b, &out);
					memcpy(&Light(op.b)->m_mtxViewSpaceRender, &out.render, sizeof(OhplMat4));
					break;
				}
				case eOhplDrDl_QueryLog:
					if(mbLog)
					{
						iLight *pLight = Light(op.b)->mpLight;
						Log(" Fetching query for light '%s'/%d. Have %d samples. Visible: %d\n", pLight->GetName().c_str(), (int)(intptr_t)pLight, op.c, op.d);
					}
					break;
				case eOhplDrDl_ReleaseQuery:
					ReleaseOcclusionQuery(Light(op.b)->mpQuery);
					Light(op.b)->mpQuery = NULL;
					break;
				case eOhplDrDl_Lists:
				{
					mbStencilNeedClearing = false;
					for(int i=0; i<eDeferredLightList_LastEnum; ++i)
					{
						mvSortedLights[i].resize(0);
						const uint32_t *pIdx = NULL;
						size_t lNum = ohpl_dl_plan_list(apPlan, i, &pIdx);
						for(size_t j=0; j<lNum; ++j) mvSortedLights[i].push_back(mvTempDeferredLights[pIdx[j]]);
					}
					mpCurrentSettings->mlNumberOfLightsRendered = ohpl_dl_plan_rendered(apPlan);
					break;
				}
				case eOhplDrDl_ClearShadowTexture:	apLight->mpShadowTexture = NULL; break;
				case eOhplDrDl_ShadowData:
					pShadowData = GetShadowMapData(apLight->mShadowResolution, apLight->mpLight);
					apLight->mpShadowTexture = pShadowData->mpTexture;
					break;
				case eOhplDrDl_RenderShadowMap:		RenderShadowMap(apLight->mpLight, pShadowData->mpBuffer); break;
				}
				break;
			}
		}
		if(abFree) ohpl_dr_plan_free(apPlan);
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::SetupGBuffer()
	{
		START_RENDER_PASS(GBufferSetup);
		RunPlan(ohpl_dr_plan_setup_gbuffer(mGBufferType == eDeferredGBuffer_64Bit, mlLinearDepthTextureIdx), NULL, NULL, NULL);
		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::SetupRenderVariables()
	{
		OhplDrFarPlane far;
		ohpl_dr_far_plane(mpCurrentFrustum->GetFOV(), mpCurrentFrustum->GetAspect(), mpCurrentFrustum->GetFarPlane(), &far);
		mfFarPlane = far.plane;
		mfFarTop = far.top;
		mfFarBottom = far.bottom;
		mfFarRight = far.right;
		mfFarLeft = far.left;
	}
	
	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderZ()
	{
		START_RENDER_PASS(EarlyZ);

		RunPlan(ohpl_dr_plan_pass(eOhplDrPass_ZBegin), NULL, NULL, NULL);

		cRenderableVecIterator zIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Z);
		while(zIt.HasNext())
		{
			iRenderable *pObject = zIt.Next();
			RenderZObject(pObject, NULL);
		}

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderDynamicZTemp()
	{
		START_RENDER_PASS(EarlyZ);

		SetDepthTest(true);
		SetDepthWrite(true);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_None);

		SetTextureRange(NULL,0);

		for(int i=0; i<mpCurrentRenderList->GetSolidObjectNum(); ++i)
		{
			iRenderable *pObject = mpCurrentRenderList->GetSolidObject(i);
			if(pObject->IsStatic()) continue;

			cMaterial *pMaterial = pObject->GetMaterial();

			SetMaterialProgram(eMaterialRenderMode_Z,pMaterial);

			if(pMaterial->GetTexture(eMaterialTexture_Alpha))
			{
				SetTexture(0,pMaterial->GetTexture(eMaterialTexture_Alpha));
				SetAlphaMode(eMaterialAlphaMode_Trans);
			}
			else
			{
				SetTexture(0,NULL);
				SetAlphaMode(eMaterialAlphaMode_Solid);
			}

			SetMatrix(pObject->GetModelMatrixPtr());

			SetVertexBuffer(pObject->GetVertexBuffer());

			DrawCurrentMaterial(eMaterialRenderMode_Z, pObject);
		}
		END_RENDER_PASS();
	}

	
	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderGbuffer()
	{
		START_RENDER_PASS(GBuffer);

		RunPlan(ohpl_dr_plan_pass(eOhplDrPass_GbufferBegin), NULL, NULL, NULL);

		
		////////////////////////////////////
		//Iterate renderable objects and render to G-Buffer
		cRenderableVecIterator diffuseIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Diffuse);
		while(diffuseIt.HasNext())
		{
			iRenderable *pObject = diffuseIt.Next();
			cMaterial *pMaterial = pObject->GetMaterial();

			SetMaterialProgram(eMaterialRenderMode_Diffuse,pMaterial);

			SetMaterialTextures(eMaterialRenderMode_Diffuse, pMaterial);

			SetMatrix(pObject->GetModelMatrixPtr());

			SetVertexBuffer(pObject->GetVertexBuffer());

			DrawCurrentMaterial(eMaterialRenderMode_Diffuse, pObject);
		}
		
		RunPlan(ohpl_dr_plan_pass(eOhplDrPass_GbufferEnd), NULL, NULL, NULL);

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderSSAO()
	{
		OhplDlSsaoIn in = {};
		in.loaded = mbSSAOLoaded;
		in.active = mpCurrentSettings->mbSSAOActive;
		in.inBoxLight = mSSAOType == eDeferredSSAO_InBoxLight;
		in.boxListsEmpty = mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack].empty() &&
						   mvSortedLights[eDeferredLightList_Box_RenderBack].empty();
		if(in.loaded && in.active)
		{
			iTexture *pDepth = GetBufferTexture(mlLinearDepthTextureIdx);
			in.gbufferDepthSize = OhplVec2l{pDepth->GetWidth(), pDepth->GetHeight()};
			in.ssaoSize = OhplVec2l{mpSSAOTexture->GetWidth(), mpSSAOTexture->GetHeight()};
		}
		in.gbuffer64bit = mGBufferType == eDeferredGBuffer_64Bit;
		in.onColorBuffer = mSSAOType == eDeferredSSAO_OnColorBuffer;
		in.unpackOk = mpUnpackDepthProgram != NULL;
		in.renderOk = mpSSAORenderProgram != NULL;
		in.blurOk[0] = mpSSAOBlurProgram[0] != NULL;
		in.blurOk[1] = mpSSAOBlurProgram[1] != NULL;
		in.far = OhplDlFarIn{mfFarPlane, mfFarTop, mfFarBottom, mfFarRight, mfFarLeft};
		in.scatterMul = mfSSAOScatterLengthMul;
		in.scatterMin = mfSSAOScatterLengthMin;
		in.scatterMax = mfSSAOScatterLengthMax;
		in.depthDiffMul = mfSSAODepthDiffMul;
		in.skipEdge = mfSSAOSkipEdgeLimit;

		OhplDrPlan *pPlan = ohpl_dl_plan_ssao(&in);
		if(pPlan == NULL) return;

		START_RENDER_PASS(SSAO);

		iFrameBuffer *vFbs[kOhplDlFbCount] = {};
		iTexture *vTexs[kOhplDlTexCount] = {};
		iGpuProgram *vProgs[kOhplDlProgCount] = {};
		vFbs[kOhplDlFbLinearDepth] = mpLinearDepthBuffer;
		vFbs[kOhplDlFbSsao] = mpSSAOBuffer;
		vFbs[kOhplDlFbSsaoBlur] = mpSSAOBlurBuffer;
		vTexs[kOhplDlTexGbufDepth] = GetBufferTexture(mlLinearDepthTextureIdx);
		vTexs[kOhplDlTexLinearDepth] = mpLinearDepthTexture;
		vTexs[kOhplDlTexSsaoScatter] = mpSSAOScatterDisk;
		vTexs[kOhplDlTexSsao] = mpSSAOTexture;
		vTexs[kOhplDlTexSsaoBlur] = mpSSAOBlurTexture;
		vProgs[kOhplDlProgSsaoUnpack] = mpUnpackDepthProgram;
		vProgs[kOhplDlProgSsaoRender] = mpSSAORenderProgram;
		vProgs[kOhplDlProgSsaoBlur0] = mpSSAOBlurProgram[0];
		vProgs[kOhplDlProgSsaoBlur1] = mpSSAOBlurProgram[1];
		RunPlan(pPlan, vFbs, vTexs, vProgs);

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderEdgeSmooth()
	{
		OhplDlEdgeIn in = {};
		in.loaded = mbEdgeSmoothLoaded;
		in.useEdgeSmooth = mpCurrentSettings->mbUseEdgeSmooth;
		in.gbuffer64bit = mGBufferType == eDeferredGBuffer_64Bit;
		in.renderOk = mpEdgeSmooth_RenderProgram != NULL;
		in.far = OhplDlFarIn{mfFarPlane, mfFarTop, mfFarBottom, mfFarRight, mfFarLeft};
		in.screen = OhplVec2l{mvScreenSize.x, mvScreenSize.y};
		in.screenF[0] = mvScreenSizeFloat.x;
		in.screenF[1] = mvScreenSizeFloat.y;

		OhplDrPlan *pPlan = ohpl_dl_plan_edge_smooth(&in);
		if(pPlan == NULL) return;

		START_RENDER_PASS(EdgeSmooth);

		iFrameBuffer *vFbs[kOhplDlFbCount] = {};
		iTexture *vTexs[kOhplDlTexCount] = {};
		iGpuProgram *vProgs[kOhplDlProgCount] = {};
		vFbs[kOhplDlFbEdgeLinearDepth] = mpEdgeSmooth_LinearDepthBuffer;
		vTexs[kOhplDlTexGbufDepth] = GetBufferTexture(mlLinearDepthTextureIdx);
		vTexs[kOhplDlTexGbufNormal] = GetBufferTexture(1);
		vTexs[kOhplDlTexEdgeLinear] = mpEdgeSmooth_LinearDepthTexture;
		vTexs[kOhplDlTexEdgeTemp] = mpEdgeSmooth_TempAccum;
		vProgs[kOhplDlProgEdgeUnpack] = mpEdgeSmooth_UnpackDepthProgram;
		vProgs[kOhplDlProgEdgeRender] = mpEdgeSmooth_RenderProgram;
		RunPlan(pPlan, vFbs, vTexs, vProgs);

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	//Definitions used when rendering lights
	#define kLightRadiusMul_High		1.08f
	#define kLightRadiusMul_Medium		1.12f
	#define kLightRadiusMul_Low			1.2f
	#define kMaxStencilBitsUsed			8
	#define kStartStencilBit			0

	//-----------------------------------------------------------------------

	static cColor LinearLightColor(const cColor& aCol)
	{
		if(cRendererDeferred::GetHdr()==false) return aCol;
		return cColor(aCol.r * aCol.r, aCol.g * aCol.g, aCol.b * aCol.b, aCol.a);
	}

	// official cRendererDeferred::GetLightInstanceData 0xd7ee68: shader reads w<0 as no diffuse
	static cColor LightShaderColor(iLight *apLight)
	{
		cColor col = LinearLightColor(apLight->GetDiffuseColor());
		if(!apLight->GetCastSpecularLight()) col.a = 0;
		if(!apLight->GetCastDiffuseLight()) col.a = -col.a;
		return col;
	}

	static cColor UnderwaterFogColor(const cColor& aCol)
	{
		float fInvLen2 = 1.0f / (aCol.r*aCol.r + aCol.g*aCol.g + aCol.b*aCol.b + 1e-13f);
		return cColor(aCol.r*aCol.r*fInvLen2, aCol.g*aCol.g*fInvLen2, aCol.b*aCol.b*fInvLen2, aCol.a);
	}

	static bool WorldUnderwaterFog(iRenderer *apRenderer)
	{
		return apRenderer->WorldFogActive() && apRenderer->GetCurrentWorld()->GetFogUnderwater();
	}

	static void CopyMat(OhplMat4 &aDest, const cMatrixf &a_mtx)
	{
		static_assert(sizeof(cMatrixf) == sizeof(OhplMat4), "matrix layout");
		memcpy(&aDest, &a_mtx, sizeof(OhplMat4));
	}

	static OhplVec3f ToVec3(const cVector3f &avVec)
	{
		return OhplVec3f{avVec.x, avVec.y, avVec.z};
	}

	static void FillLightIn(OhplDlLightIn &aDest, iLight *apLight, bool abViewProj)
	{
		memset(&aDest, 0, sizeof(aDest));
		aDest.kind = apLight->GetLightType();
		aDest.radius = apLight->GetRadius();
		aDest.brightness = apLight->GetBrightness();
		aDest.falloffPow = apLight->GetFalloffPow();
		aDest.spotFalloffPow = apLight->GetSpotFalloffPow();
		aDest.translucency = apLight->GetTranslucency();
		cColor diffuse = apLight->GetDiffuseColor();
		aDest.diffuse[0] = diffuse.r; aDest.diffuse[1] = diffuse.g; aDest.diffuse[2] = diffuse.b; aDest.diffuse[3] = diffuse.a;
		aDest.castSpecular = apLight->GetCastSpecularLight();
		aDest.castDiffuse = apLight->GetCastDiffuseLight();
		aDest.castShadows = apLight->GetCastShadows();
		aDest.hasGobo = apLight->GetGoboTexture() != NULL;
		aDest.goboSpecular = apLight->GetGoboSpecular();
		aDest.hasMaskBox = apLight->HasMaskBox();
		aDest.shadowRes = apLight->GetShadowMapResolution();
		aDest.shadowBlur = apLight->GetShadowMapBlurAmount();
		if(aDest.hasMaskBox)
		{
			aDest.maskCenter = ToVec3(apLight->GetMaskCenter());
			aDest.maskSize = ToVec3(apLight->GetMaskSize());
		}
		CopyMat(aDest.world, apLight->GetWorldMatrix());
		aDest.worldPos = ToVec3(apLight->GetWorldPosition());
		cBoundingVolume *pBV = apLight->GetBoundingVolume();
		aDest.bvMin = ToVec3(pBV->GetMin());
		aDest.bvMax = ToVec3(pBV->GetMax());
		aDest.bvCenter = ToVec3(pBV->GetWorldCenter());
		aDest.falloffMap = (uint64_t)(uintptr_t)apLight->GetFalloffMap();
		aDest.gobo = (uint64_t)(uintptr_t)apLight->GetGoboTexture();
		aDest.id = (uint64_t)(uintptr_t)apLight;

		if(aDest.kind == eLightType_Spot)
		{
			cLightSpot *pSpot = static_cast<cLightSpot*>(apLight);
			aDest.tanHalfFov = pSpot->GetTanHalfFOV();
			aDest.aspect = pSpot->GetAspect();
			aDest.cosHalfFov = pSpot->GetCosHalfFOV();
			aDest.nearClip = pSpot->GetNearClipPlane();
			aDest.spotFalloff = (uint64_t)(uintptr_t)pSpot->GetSpotFalloffMap();
			if(abViewProj) CopyMat(aDest.viewProj, pSpot->GetViewProjMatrix());
		}
		else if(aDest.kind == eLightType_Box)
		{
			cLightBox *pBox = static_cast<cLightBox*>(apLight);
			aDest.boxSize = ToVec3(pBox->GetSize());
			aDest.boxPrio = pBox->GetBoxLightPrio();
		}
	}

	void cRendererDeferred::FillLightState(OhplDlLightState &aState, cDeferredLight *apLightData, bool abViewProj)
	{
		memset(&aState, 0, sizeof(aState));
		iLight *pLight = apLightData->mpLight;
		FillLightIn(aState.light, pLight, abViewProj);
		CopyMat(aState.viewRender, apLightData->m_mtxViewSpaceRender);
		CopyMat(aState.viewTransform, apLightData->m_mtxViewSpaceTransform);
		CopyMat(aState.invView, m_mtxInvView);
		aState.castShadows = apLightData->mbCastShadows;
		aState.shadowTex = apLightData->mpShadowTexture != NULL;
		if(aState.shadowTex) aState.shadowSize = OhplVec2l{apLightData->mpShadowTexture->GetWidth(), apLightData->mpShadowTexture->GetHeight()};
		aState.jitter = mpShadowJitterTexture != NULL;
		aState.hdr = GetHdr();
		aState.underwater = WorldUnderwaterFog(this);
		if(aState.underwater)
		{
			cColor fog = mpCurrentWorld->GetFogColor();
			aState.fogColor[0] = fog.r; aState.fogColor[1] = fog.g; aState.fogColor[2] = fog.b; aState.fogColor[3] = fog.a;
			aState.fogStart = mpCurrentWorld->GetFogStart();
			aState.fogEnd = mpCurrentWorld->GetFogEnd();
			aState.fogFalloffExp = mpCurrentWorld->GetFogFalloffExp();
		}
		aState.distanceFade = GetDistanceFadeAmount(pLight);
		aState.farPlane = mpCurrentFrustum->GetFarPlane();
		aState.depthTextureIdx = mlLinearDepthTextureIdx;
	}

	void cRendererDeferred::SetupLightProgramVariables(	iGpuProgram *apProgram,cDeferredLight* apLightData)
	{
		if(apProgram==NULL) return;

		OhplDlLightState state;
		FillLightState(state, apLightData, true);
		state.programOk = true;
		iGpuProgram *vProgs[kOhplDlProgCount] = {};
		vProgs[kOhplDlProgLight] = apProgram;
		RunPlan(ohpl_dl_plan_light_variables(&state), NULL, NULL, vProgs, true, apLightData);
	}

	//-----------------------------------------------------------------------

	#ifdef kDebug_RenderLightData

		#define RenderSingleLightDebug() \
			SetStencilActive(false); SetProgram(NULL);SetTextureRange(NULL,0); mpLowLevelGraphics->SetColor(cColor(0.25,0,0,0));\
			DrawCurrent();\
			mpLowLevelGraphics->SetColor(cColor(1,1)); for(int i=0; i<mlNumOfGBufferTextures; ++i) SetTexture(i, mpGBufferTexture[i]); SetStencilActive(true);
	#else
		#define RenderSingleLightDebug()
	#endif

	//-----------------------------------------------------------------------

	iGpuProgram* cRendererDeferred::SetupProgramAndTextures(cDeferredLight* apLightData, tFlag alExtraFlags)
	{
		OhplDlLightState state;
		FillLightState(state, apLightData, false);

		iLight *pLight = apLightData->mpLight;
		iTexture *vTexs[kOhplDlTexCount] = {};
		iGpuProgram *vProgs[kOhplDlProgCount] = {};
		vTexs[kOhplDlTexFalloff] = pLight->GetFalloffMap();
		vTexs[kOhplDlTexGobo] = pLight->GetGoboTexture();
		vTexs[kOhplDlTexShadow] = apLightData->mpShadowTexture;
		vTexs[kOhplDlTexJitter] = mpShadowJitterTexture;
		if(pLight->GetLightType() == eLightType_Spot) vTexs[kOhplDlTexSpotFalloff] = static_cast<cLightSpot*>(pLight)->GetSpotFalloffMap();

		RunPlan(ohpl_dl_plan_light_program(&state, alExtraFlags, eDefferredProgramMode_Lights), NULL, vTexs, vProgs, true, apLightData);
		return vProgs[kOhplDlProgLight];
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderLightShadowMap(cDeferredLight* apLightData)
	{
		iTexture *vTexs[kOhplDlTexCount] = {};
		for(int i=0; i<mlNumOfGBufferTextures && i<kOhplDlGbufferMax; ++i) vTexs[kOhplDlTexGbuffer0 + i] = GetBufferTexture(i);

		RunPlan(ohpl_dl_plan_shadow_map(mlNumOfGBufferTextures), NULL, vTexs, NULL, true, apLightData);
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cRendererDeferred::GetLightShapeBuffer(int alShape)
	{
		if(alShape == kOhplDlShapePyramid) return mpShapePyramid;
		if(alShape >= 0 && alShape < eDeferredShapeQuality_LastEnum) return mpShapeSphere[alShape];
		return NULL;
	}

	iVertexBuffer* cRendererDeferred::GetLightShape(iLight *apLight, eDeferredShapeQuality aQuality)
	{
		return GetLightShapeBuffer(ohpl_dl_light_shape(apLight->GetLightType(), aQuality));
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::SetupLightsAndRenderQueries()
	{
		OhplDlSetupIn in = {};
		in.testLargeLights = mbOcclusionTestLargeLights;
		in.useOcclusionCulling = mpCurrentSettings->mbUseOcclusionCulling;
		in.renderShadows = mpCurrentSettings->mbRenderShadows;
		in.rtSize = OhplVec2l{mvRenderTargetSize.x, mvRenderTargetSize.y};
		in.minLargeNormalizedArea = mfMinLargeLightNormalizedArea;
		in.maxShadowRes = mpCurrentSettings->mMaxShadowMapResolution;
		in.shadowDistNone = mfShadowDistanceNone;
		in.shadowDistLow = mfShadowDistanceLow;
		in.shadowDistMedium = mfShadowDistanceMedium;
		in.tanHalfFov = mfScissorLastTanHalfFov;
		in.sampleLimit = mpCurrentSettings->mlSampleVisiblilityLimit;
		CopyMat(in.frustum.view, mpCurrentFrustum->GetViewMatrix());
		CopyMat(in.frustum.proj, mpCurrentFrustum->GetProjectionMatrix());
		in.frustum.perspective = mpCurrentFrustum->GetProjectionType() == eProjectionType_Perspective;
		in.frustum.fov = mpCurrentFrustum->GetFOV();
		in.frustum.nearPlane = mpCurrentFrustum->GetNearPlane();
		in.frustum.aspect = mpCurrentFrustum->GetAspect();
		in.frustum.origin = ToVec3(mpCurrentFrustum->GetOrigin());

		std::vector<OhplDlLightIn> vLights(mpCurrentRenderList->GetLightNum());
		for(size_t i=0; i<vLights.size(); ++i) FillLightIn(vLights[i], mpCurrentRenderList->GetLight((int)i), false);

		std::vector<OhplDlPair> vPairs(mpCurrentSettings->mvLightOcclusionPairs.size());
		for(size_t i=0; i<vPairs.size(); ++i)
		{
			vPairs[i].light = (uint64_t)(uintptr_t)mpCurrentSettings->mvLightOcclusionPairs[i].mpLight;
			vPairs[i].samples = mpCurrentSettings->mvLightOcclusionPairs[i].mlSampleResults;
		}

		iGpuProgram *vProgs[kOhplDlProgCount] = {};
		vProgs[kOhplDlProgDepthOnly] = mpDepthOnlyProgram;
		RunPlan(ohpl_dl_plan_setup_lights(&in, vLights.empty() ? NULL : &vLights[0], vLights.size(), vPairs.empty() ? NULL : &vPairs[0], vPairs.size()),
				NULL, NULL, vProgs);
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::InitLightRendering()
	{
		OhplDlInitIn in = {};
		CopyMat(in.view, mpCurrentFrustum->GetViewMatrix());
		in.lastFov = mfLastFrustumFOV;
		in.lastFarPlane = mfLastFrustumFarPlane;
		in.fov = mpCurrentFrustum->GetFOV();
		in.farPlane = mfFarPlane;
		in.farLeft = mfFarLeft;
		in.farBottom = mfFarBottom;
		in.farRight = mfFarRight;
		in.farTop = mfFarTop;
		in.minLargeArea = mlMinLargeLightArea;
		in.sampleLimit = mpCurrentSettings->mlSampleVisiblilityLimit;

		std::vector<OhplDlInitLight> vLights(mvTempDeferredLights.size());
		for(size_t i=0; i<vLights.size(); ++i)
		{
			cDeferredLight *pLightData = mvTempDeferredLights[i];
			FillLightIn(vLights[i].light, pLightData->mpLight, false);
			vLights[i].insideNear = pLightData->mbInsideNearPlane;
			vLights[i].hasQuery = pLightData->mpQuery != NULL;
			vLights[i].area = pLightData->mlArea;
		}

		RunPlan(ohpl_dl_plan_init_lights(&in, vLights.empty() ? NULL : &vLights[0], vLights.size()), NULL, NULL, NULL);
	}


	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderLights_StencilBack_ScreenQuad()
	{
		if(mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad].empty()) return;
		if(mbLog) Log("---\nRendering Lights StencilBack_ScreenQuad Begin\n");
		
		//Check if stencil is dirty
		if(mbStencilNeedClearing)
		{
			ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			mbStencilNeedClearing = false;
		}
		
		if(mbDepthCullLights) SetStencilActive(true);
		
		///////////////////////
		// Render Inside Near Plane Lights
		int lNumOfNearPlaneLights = (int)mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad].size();
		int lStartLight = 0;
		while(lNumOfNearPlaneLights>0)
		{
			//cVector2l vMinClip(10000,10000);
			//cVector2l vMaxClip(-1000,-10000);

			int lIterations;
			if(lNumOfNearPlaneLights >kMaxStencilBitsUsed)	lIterations = kMaxStencilBitsUsed;
			else											lIterations = lNumOfNearPlaneLights;

			lNumOfNearPlaneLights -= lIterations;

			/////////////////////////////
			//Render stencil
			if(mbDepthCullLights)
				SetDepthTest(true);
			else
				SetDepthTest(false);
			SetChannelMode(eMaterialChannelMode_None);
			SetStencil(eStencilFunc_Always,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Keep,eStencilOp_Replace);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				if(mbLog)Log(" Stencil light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencilWriteMask(cMath::GetFlagBit(kStartStencilBit + i));

				//Set up textures
				SetTexture(5,NULL);

				//Set up matrix
				//SetupLightMatrix(m_mtxTempLight,pLight, mpCurrentFrustum);
				SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

				//Set vertex buffer
				SetVertexBuffer(GetLightShape(pLight, eDeferredShapeQuality_Medium));

				//Set program
				SetProgram(mpLightStencilProgram);

				DrawCurrent();
			}

			/////////////////////////////
			//Render lights as quads
			SetDepthTest(false);
			SetChannelMode(eMaterialChannelMode_RGBA);
			SetFlatProjectionMinMax(cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane*1.5f),cVector3f(mfFarRight,mfFarTop,mfFarPlane*1.5f));
			SetCullMode(eCullMode_CounterClockwise);
			
			SetStencilWriteMask(0xFF);
			
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_StencilBack_ScreenQuad][lStartLight + i];
				iLight *pLight = pLightData->mpLight;

				//////////////////
				// Render shadow (if light is caster)
				if(pLightData->mbCastShadows && SetupShadowMapRendering(pLight))
				{
					if(mbDepthCullLights) 
					{
						SetStencilActive(false);
						SetDepthTest(true);
					}
					else
					{
						SetDepthTest(false);
					}

					//Render shadow map
					RenderLightShadowMap(pLightData);
					
					if(mbDepthCullLights) 
					{
						SetDepthTest(false);
						SetStencilActive(true);
					}
					else
					{
						SetDepthTest(false);
					}

					SetFlatProjectionMinMax(cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane*1.5f),cVector3f(mfFarRight,mfFarTop,mfFarPlane*1.5f));
				}

				//////////////////
				// Render light
				if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencil(	eStencilFunc_Equal, 0xFF, cMath::GetFlagBit(kStartStencilBit + i),
												eStencilOp_Keep, eStencilOp_Keep,eStencilOp_Keep);

				//Set vertex buffer
				SetVertexBuffer(mpFullscreenLightQuad);

				// Set up clip
				SetScissorRect(pLightData->mClipRect, true);
								
				//Debug data
				RenderSingleLightDebug();

				//Set up program and textures
				iGpuProgram *pProgram = SetupProgramAndTextures(pLightData,0);

				//Set up Light specific variables
				SetupLightProgramVariables(pProgram, pLightData);

				//Draw the light
				DrawCurrent();
				
				/////////////////////////////
				// Update min max clip rect occupied by lights 
				/*if(vMinClip.x > lightData.mClipRect.x) vMinClip.x = lightData.mClipRect.x;
				if(vMinClip.y > lightData.mClipRect.y) vMinClip.y = lightData.mClipRect.y;
				if(vMaxClip.x < lightData.mClipRect.x+lightData.mClipRect.w) vMaxClip.x = lightData.mClipRect.x+lightData.mClipRect.w;
				if(vMaxClip.y < lightData.mClipRect.y+lightData.mClipRect.h) vMaxClip.y = lightData.mClipRect.y+lightData.mClipRect.h;*/
			}
			
			/////////////////////////////
			//Go back to normal frustum projection
			SetNormalFrustumProjection();
			SetCullMode(eCullMode_Clockwise);
			SetScissorActive(false);

			/////////////////////////////
			//Prepare for next iteration (if any)
			if(lNumOfNearPlaneLights >0)
			{
				//According to test there was a slow down using this. Keeping code anyways.
				//cRect2l dirtyRect(vMinClip, vMaxClip - vMinClip);
				//SetupScissorRect(dirtyRect);
				
				lStartLight += kMaxStencilBitsUsed;
				ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			}
		}
		
		////////////////////////////////////////
		// Reset things needed
		mbStencilNeedClearing = true;
		if(mbLog) Log("Rendering Lights StencilBack_ScreenQuad End\n---\n");
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderLights_StencilFront_RenderBack()
	{
		if(mvSortedLights[eDeferredLightList_StencilFront_RenderBack].empty()) return;
		if(mbLog) Log("---\nRendering Lights StencilFront_RenderBack Begin\n");

		//Check if stencil is dirty
		if(mbStencilNeedClearing)
		{
			ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			mbStencilNeedClearing = false;
		}

		if(mbDepthCullLights) 
		{
			SetStencilActive(true);
			SetDepthTest(true);
		}
		else
		{
			SetDepthTest(false);
		}
		
		///////////////////////
		// Render Large Lights
		int lNumOfNearPlaneLights = (int)mvSortedLights[eDeferredLightList_StencilFront_RenderBack].size();
		int lStartLight = 0;
		while(lNumOfNearPlaneLights>0)
		{

			
			int lIterations;
			if(lNumOfNearPlaneLights >kMaxStencilBitsUsed)	lIterations = kMaxStencilBitsUsed;
			else											lIterations = lNumOfNearPlaneLights;

			lNumOfNearPlaneLights -= lIterations;

			/////////////////////////////
			//Render stencil using the front (back?!), set bit at zfail (since test is inverted)
			SetChannelMode(eMaterialChannelMode_None);
			SetCullMode(eCullMode_CounterClockwise);
			SetStencil(eStencilFunc_Always,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Replace,eStencilOp_Keep);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_StencilFront_RenderBack][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				if(mbLog)Log(" Stencil light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencilWriteMask(cMath::GetFlagBit(kStartStencilBit + i));

				//Set up textures
				SetTexture(5,NULL);

				//Set up matrix
				SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

				//Set vertex buffer
				//SetVertexBuffer(mpShapeSphereMedRes);
				SetVertexBuffer(GetLightShape(pLight, eDeferredShapeQuality_Medium));

				//Set program
				SetProgram(mpLightStencilProgram);

				//Draw the light
				DrawCurrent();
			}

			/////////////////////////////
			//Render back (front?!) of light geometry, checking depth and stencil
			SetChannelMode(eMaterialChannelMode_RGBA);
			SetCullMode(eCullMode_Clockwise);
			SetStencilWriteMask(0xFF);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_StencilFront_RenderBack][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				
				//////////////////
				// Render shadow (if light is caster)
				if(pLightData->mbCastShadows && SetupShadowMapRendering(pLight))
				{
					//Setup render modes for shadow map rendering
					SetCullMode(eCullMode_CounterClockwise);
					SetStencilActive(false);

					//Render shadow map
					RenderLightShadowMap(pLightData);
					
					//Reset render settings to previous
					SetStencilActive(true);
					SetCullMode(eCullMode_Clockwise);
				}
				
				//////////////////
				// Render light
				if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencil(	eStencilFunc_Equal, 0xFF, cMath::GetFlagBit(kStartStencilBit + i),
												eStencilOp_Keep, eStencilOp_Keep,eStencilOp_Keep);

				//Set up matrix
				SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

				//Set vertex buffer
				//SetVertexBuffer(mpShapeSphereHighRes);
				SetVertexBuffer(GetLightShape(pLight, eDeferredShapeQuality_High));
				
				//Set up program and textures
				iGpuProgram *pProgram = SetupProgramAndTextures(pLightData,eFeature_Light_LightShapes | eFeature_Light_DivideInFrag);

				//Set up Light specific variables
				SetupLightProgramVariables(pProgram, pLightData);

				//Draw the light
				////////////////////////
				// If lights are occlusion tested, render a query with the front and push back into Light-Occlusion pair vector
				if(mbOcclusionTestLargeLights)
				{
					cLightOcclusionPair loPair;
					loPair.mpLight = pLight;
					loPair.mpQuery = GetOcclusionQuery();

					loPair.mpQuery->Begin();
					DrawCurrent();
					loPair.mpQuery->End();

					mpCurrentSettings->mvLightOcclusionPairs.push_back(loPair);
				}
				///////////////////////////////
				//Normal rendering of front
				else
				{
					DrawCurrent();
				}
			}
			
			/////////////////////////////
			//Prepare for next iteration (if any)
			if(lNumOfNearPlaneLights >0)
			{
				lStartLight += kMaxStencilBitsUsed;
				ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			}
		}

		////////////////////////////////////////
		// Reset things needed
		mbStencilNeedClearing = true;
		if(mbLog) Log("Rendering Lights StencilFront_RenderBack End\n---\n");
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderLights_RenderBack()
	{
		if(mvSortedLights[eDeferredLightList_RenderBack].empty()) return;
		if(mbLog) Log("---\nRendering Lights RenderBack Begin\n");
		
		if(mbDepthCullLights)
		{
			SetStencilActive(false);
			SetDepthTest(true);
		}
		else
		{
			SetDepthTest(false);
		}
		
		/////////////////////////////
		//Render back of light geometry, checking depth
		SetChannelMode(eMaterialChannelMode_RGBA);
		for(size_t i=0; i<mvSortedLights[eDeferredLightList_RenderBack].size(); ++i)
		{
			cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_RenderBack][i];
			iLight *pLight = pLightData->mpLight;

			//////////////////
			// Render shadow (if light is caster)
			if(pLightData->mbCastShadows && SetupShadowMapRendering(pLight))
			{
				//Setup render modes for shadow map rendering
				SetCullMode(eCullMode_CounterClockwise);

				//Render shadow map
				RenderLightShadowMap(pLightData);

				//Reset render settings to previous
				SetCullMode(eCullMode_Clockwise);
			}

			//////////////////
			// Render light
			if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

			//Set up matrix
			SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

			//Set vertex buffer
			//SetVertexBuffer(mpShapeSphereMedRes);
			SetVertexBuffer(GetLightShape(pLight, eDeferredShapeQuality_Medium));

			//Set up program and textures
			tFlag lExtraFlags = eFeature_Light_LightShapes; 
			if(pLightData->mbInsideNearPlane) lExtraFlags |= eFeature_Light_DivideInFrag;
			iGpuProgram *pProgram = SetupProgramAndTextures(pLightData,lExtraFlags);

			//Set up Light specific variables
			SetupLightProgramVariables(pProgram, pLightData);

			//Draw the light
			DrawCurrent();
		}
		if(mbLog) Log("Rendering Lights RenderBack End\n---\n");
	}

	//------------------------------------------------------------------------------

	void cRendererDeferred::RenderBoxLight(cDeferredLight* apLightData)
	{
		iLight *pLight = apLightData->mpLight;
		if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

		cLightBox *pLightBox = static_cast<cLightBox*>(pLight);

		//Get correct program, depending if ssao is used or not.
		int lProgramNum =	(mbSSAOLoaded && mpCurrentSettings->mbSSAOActive && mSSAOType == eDeferredSSAO_InBoxLight) ? 1 :0;

		//Set up texture
		if(lProgramNum == 1) SetTexture(5, mpSSAOTexture);

		//Set up program
		SetProgram(mpLightBoxProgram[lProgramNum]);

		//Set up Light specific variables
		if(mpLightBoxProgram[lProgramNum])
		{
			cColor boxColor = LinearLightColor(pLight->GetDiffuseColor());
			float fBrightness = pLight->GetBrightness() * GetDistanceFadeAmount(pLight);
			boxColor.r *= fBrightness; boxColor.g *= fBrightness; boxColor.b *= fBrightness;
			mpLightBoxProgram[lProgramNum]->SetColor4f(kVar_avLightColor,boxColor);
		}

		//Blend mode
		switch(pLightBox->GetBlendFunc())
		{
		case eLightBoxBlendFunc_Add:
			SetBlendMode(eMaterialBlendMode_Add); break;
		case eLightBoxBlendFunc_Replace:
			SetBlendMode(eMaterialBlendMode_None); break;
		}

		//Set up matrix
		SetModelViewMatrix(apLightData->m_mtxViewSpaceRender);

		//Draw the light
		DrawCurrent();
	}

	//------------------------------------------------------------------------------

	void cRendererDeferred::RenderLights_Box_StencilFront_RenderBack()
	{
		if(mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack].empty()) return;
		if(mbLog) Log("---\nRendering Lights Box_StencilFront_RenderBack Begin\n");

		//Check if stencil is dirty
		if(mbStencilNeedClearing)
		{
			ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			mbStencilNeedClearing = false;
		}

		if(mbDepthCullLights)
		{
			SetStencilActive(true);
			SetDepthTest(true);
		}
		else
		{
			SetDepthTest(false);
		}
		SetVertexBuffer(mpShapeBox);

		///////////////////////
		// Render Large Lights
		int lNumOfNearPlaneLights = (int)mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack].size();
		int lStartLight = 0;
		while(lNumOfNearPlaneLights>0)
		{
			int lIterations;
			if(lNumOfNearPlaneLights >kMaxStencilBitsUsed)	lIterations = kMaxStencilBitsUsed;
			else											lIterations = lNumOfNearPlaneLights;

			lNumOfNearPlaneLights -= lIterations;

			/////////////////////////////
			//Render stencil using the front, set bit at zfail (since test is inverted)
			SetChannelMode(eMaterialChannelMode_None);
			SetCullMode(eCullMode_CounterClockwise);
			SetStencil(eStencilFunc_Always,0xFF,0xFF,eStencilOp_Keep,eStencilOp_Replace,eStencilOp_Keep);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				if(mbLog)Log(" Stencil light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencilWriteMask(cMath::GetFlagBit(kStartStencilBit + i));

				//Set up matrix
				SetModelViewMatrix(pLightData->m_mtxViewSpaceRender);

				//Set program
				SetProgram(mpLightStencilProgram);

				DrawCurrent();
			}

			/////////////////////////////
			//Render back of light geometry, checking depth and stencil
			SetChannelMode(eMaterialChannelMode_RGBA);
			SetCullMode(eCullMode_Clockwise);
			SetStencilWriteMask(0xFF);
			for(int i=0; i<lIterations; ++i)
			{
				cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack][lStartLight + i];
				iLight *pLight = pLightData->mpLight;
				if(mbLog)Log(" Rendering light: '%s' / %d\n",pLight->GetName().c_str(), pLight);

				//Stencil
				SetStencil(	eStencilFunc_Equal, 0xFF, cMath::GetFlagBit(kStartStencilBit + i),
												eStencilOp_Keep, eStencilOp_Keep,eStencilOp_Keep);
				//SetStencil(	eStencilFunc_Always, 0xFF, cMath::GetFlagBit(kStartStencilBit + i),
				//								eStencilOp_Keep, eStencilOp_Keep,eStencilOp_Keep);
				
				RenderBoxLight(pLightData);
			}

			/////////////////////////////
			//Prepare for next iteration (if any)
			if(lNumOfNearPlaneLights >0)
			{
				lStartLight += kMaxStencilBitsUsed;
				ClearFrameBuffer(eClearFrameBufferFlag_Stencil,true);
			}
		}

		////////////////////////////////////////
		// Reset things needed
		mbStencilNeedClearing = true;
		SetBlendMode(eMaterialBlendMode_Add);
		
		if(mbLog) Log("Rendering Lights Box_StencilFront_RenderBack End\n---\n");
	}

	//------------------------------------------------------------------------------
	
	void cRendererDeferred::RenderLights_Box_RenderBack()
	{
		if(mvSortedLights[eDeferredLightList_Box_RenderBack].empty()) return;
		if(mbLog) Log("---\nRendering Lights Box_RenderBack Begin\n");

		if(mbDepthCullLights)
		{
			SetStencilActive(false);
			SetDepthTest(true);
		}
		else
		{
			SetDepthTest(false);
		}

		SetChannelMode(eMaterialChannelMode_RGBA);
		SetVertexBuffer(mpShapeBox);
		
		/////////////////////////////
		//Render back of light geometry, checking depth
		for(size_t i=0; i<mvSortedLights[eDeferredLightList_Box_RenderBack].size(); ++i)
		{
			cDeferredLight* pLightData = mvSortedLights[eDeferredLightList_Box_RenderBack][i];

			RenderBoxLight(pLightData);
		}

		//Reset what is needed.
		SetBlendMode(eMaterialBlendMode_Add);

		if(mbLog) Log("Rendering Lights Box_RenderBack End\n---\n");
	}
	
	//------------------------------------------------------------------------------

	static bool SortFunc_BoxWeighted(const cDeferredLight* apLightDataA, const cDeferredLight* apLightDataB)
	{
		cLightBox *pA = static_cast<cLightBox*>(apLightDataA->mpLight);
		cLightBox *pB = static_cast<cLightBox*>(apLightDataB->mpLight);
		static const int vRank[3] = {0, 2, 1};
		int lA = vRank[(int)cMath::Clamp((int)pA->GetBlendFunc(), 0, 2)], lB = vRank[(int)cMath::Clamp((int)pB->GetBlendFunc(), 0, 2)];
		if(lA != lB) return lA < lB;
		if(pA->GetBoxLightPrio() != pB->GetBoxLightPrio()) return pA->GetBoxLightPrio() < pB->GetBoxLightPrio();
		return pA < pB;
	}

	bool cRendererDeferred::RenderLights_BoxWeighted()
	{
		if(mpBoxWeightBuffer==NULL || mpBoxResolveProgram==NULL || mpCurrentSettings->mbIsReflection) return false;

		std::vector<cDeferredLight*> vLights = mvSortedLights[eDeferredLightList_Box_StencilFront_RenderBack];
		vLights.insert(vLights.end(), mvSortedLights[eDeferredLightList_Box_RenderBack].begin(), mvSortedLights[eDeferredLightList_Box_RenderBack].end());
		if(vLights.empty()) return true;
		std::sort(vLights.begin(), vLights.end(), SortFunc_BoxWeighted);

		SetFrameBuffer(mpBoxWeightBuffer, true);
		mpLowLevelGraphics->SetClearColor(cColor(0,0,0,1.5259022e-05f));
		ClearFrameBuffer(eClearFrameBufferFlag_Color, true);
		mpLowLevelGraphics->SetClearColor(mpCurrentSettings->mClearColor);

		SetDepthTest(false);
		SetStencilActive(false);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetCullMode(eCullMode_Clockwise);
		SetVertexBuffer(mpShapeBox);

		cVector3f vCamPos = mpCurrentFrustum->GetOrigin();
		cVector3f vViewUp = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix().GetRotation(), cVector3f(0,1,0));
		for(size_t i=0; i<vLights.size(); ++i)
		{
			cLightBox *pBox = static_cast<cLightBox*>(vLights[i]->mpLight);
			int lBlend = cMath::Clamp((int)pBox->GetBlendFunc(), 0, 2);
			iGpuProgram *pProg = mpBoxWeightedProgram[lBlend][pBox->GetUseSphericalHarmonics() ? 1 : 0];
			if(pProg==NULL) continue;
			if(mbLog) Log(" Rendering weighted box light: '%s'\n", pBox->GetName().c_str());

			SetBlendMode(lBlend == eLightBoxBlendFunc_Replace ? eMaterialBlendMode_PremulAlpha :
						 lBlend == eLightBoxBlendFunc_Add ? eMaterialBlendMode_ModulateSource : eMaterialBlendMode_Add);
			SetProgram(pProg);

			cColor diffuse = LinearLightColor(pBox->GetDiffuseColor()) * (pBox->GetBrightness() * GetDistanceFadeAmount(pBox));
			if(WorldUnderwaterFog(this) && !pBox->GetUseSphericalHarmonics())
			{
				cColor fogCol = UnderwaterFogColor(mpCurrentWorld->GetFogColor());
				diffuse.r *= fogCol.r; diffuse.g *= fogCol.g; diffuse.b *= fogCol.b;
			}
			const cColor &sky = pBox->GetAmbientColorSky();
			const cColor &ground = pBox->GetAmbientColorGround();
			pProg->SetFloat(kVar_afNegFarPlane, -mpCurrentFrustum->GetFarPlane());
			SetScreenPositionVars(pProg);
			pProg->SetMatrixf(kVar_a_mtxInvView, m_mtxInvView);
			pProg->SetVec3f(kVar_avBoxCenter, pBox->GetWorldPosition() + pBox->GetProbeOffset() - vCamPos);
			pProg->SetVec3f(kVar_avBoxExtent, pBox->GetSize() * 0.5f);
			pProg->SetFloat(kVar_afFalloff, pBox->GetFalloffPow());
			pProg->SetVec3f(kVar_avViewSpaceUp, vViewUp);
			const cVector3f *pBands = pBox->GetIrradianceBands();
			if(mbBoxInstanceInputs)
			{
				cColor skyCol = diffuse * LinearLightColor(sky), groundCol = diffuse * LinearLightColor(ground);
				pProg->SetFloat(kVar_afFarPlane, mpCurrentFrustum->GetFarPlane());
				pProg->SetFloat(kVar_afBevel, sqrtf(3.0f - 2.0f*pBox->GetBevel()));
				pProg->SetFloat(kVar_afWeight, pBox->GetWeight() * GetDistanceFadeAmount(pBox));
				pProg->SetColor4f(kVar_avAmbientColorSky, skyCol);
				pProg->SetColor4f(kVar_avAmbientColorGround, groundCol);
				cColor mul = (skyCol + groundCol) * 0.5f;
				const float vMul[3] = {mul.r, mul.g, mul.b};
				const cVector3f *b = pBands;
				// the official renderer writes the blue quadratic row past the light's data, so it stays zero
				for(int c=0; c<3; ++c)
				{
					pProg->SetVec4f(kVar_avBand0+c, b[3].v[c]*vMul[c], b[1].v[c]*vMul[c], b[2].v[c]*vMul[c], (b[0].v[c]-b[6].v[c])*vMul[c]);
					if(c<2) pProg->SetVec4f(kVar_avBand0+3+c, b[4].v[c]*vMul[c], b[5].v[c]*vMul[c], 3.0f*b[6].v[c]*vMul[c], b[7].v[c]*vMul[c]);
				}
				pProg->SetVec4f(kVar_avBand0+5, 0,0,0,0);
				pProg->SetVec4f(kVar_avBand0+6, b[8].x*mul.r, b[8].y*mul.g, b[8].z*mul.b, fabsf(mul.a));
			}
			else
			{
				pProg->SetVec3f(kVar_avAmbientColorSky, sky.r*diffuse.r, sky.g*diffuse.g, sky.b*diffuse.b);
				pProg->SetVec3f(kVar_avAmbientColorGround, ground.r*diffuse.r, ground.g*diffuse.g, ground.b*diffuse.b);
				pProg->SetFloat(kVar_afBevel, pBox->GetBevel());
				pProg->SetFloat(kVar_afWeight, pBox->GetWeight());
				for(int k=0; k<9; ++k) pProg->SetVec3f(kVar_avBand0+k, pBands[k]);
			}

			SetModelViewMatrix(vLights[i]->m_mtxViewSpaceRender);
			DrawCurrent();
		}

		SetAccumulationBuffer();
		SetBlendMode(eMaterialBlendMode_None);
		SetCullMode(eCullMode_CounterClockwise);
		SetFlatProjection();
		SetProgram(mpBoxResolveProgram);
		SetTexture(0, mpBoxWeightTexture);
		SetTextureRange(NULL, 1);
		DrawAccumulationQuad();
		SetNormalFrustumProjection();

		for(int i=0; i<mlNumOfGBufferTextures; ++i) SetTexture(i, GetBufferTexture(i));
		SetTextureRange(NULL, mlNumOfGBufferTextures);
		SetBlendMode(eMaterialBlendMode_Add);
		SetCullMode(eCullMode_Clockwise);
		SetDepthTest(true);
		return true;
	}

	void cRendererDeferred::RenderLights_Directional()
	{
		if(mpCurrentWorld->GetDirectionalLightActive()==false) return;
		cLightDirectional *pLight = mpCurrentWorld->GetDirectionalLight();
		if(pLight->GetBrightness() <= 0) return;

		tFlag lFlags = eFeature_Light_Directional;
		if(pLight->GetDiffuseColor().a > 0 && pLight->GetCastSpecularLight())	lFlags |= eFeature_Light_Specular;
		if(pLight->GetTranslucency() > 0)	lFlags |= eFeature_Light_Translucency;
		if(WorldUnderwaterFog(this))	lFlags |= eFeature_Light_UnderwaterFog;
		cMatrixf vShadowMtx[4];
		float vSplitsNear[4], vSplitsFar[4], vSplitOffsetMul[4];
		if(pLight->GetCastShadows() && mpCurrentSettings->mbRenderShadows)
		{
			lFlags |= eFeature_Light_ShadowMap;
			RenderDirectionalShadowMap(pLight, vShadowMtx, vSplitsNear, vSplitsFar, vSplitOffsetMul);
		}
		iGpuProgram *pProgram = mpProgramManager->GenerateProgram(eDefferredProgramMode_Lights, lFlags);
		if(pProgram==NULL) return;
		if(mbLog) Log(" Rendering directional light\n");

		SetProgram(pProgram);
		pProgram->SetFloat(kVar_afNegFarPlane, -mpCurrentFrustum->GetFarPlane());
		SetScreenPositionVars(pProgram);

		float fBrightness = pLight->GetBrightness();
		cColor lightColor = LightShaderColor(pLight);
		lightColor.r *= fBrightness; lightColor.g *= fBrightness; lightColor.b *= fBrightness;
		pProgram->SetColor4f(kVar_avLightColor, lightColor);
		cColor sky = LinearLightColor(pLight->GetAmbientColorSky()) * fBrightness;
		cColor ground = LinearLightColor(pLight->GetAmbientColorGround()) * fBrightness;
		pProgram->SetVec3f(kVar_avAmbientColorSky, sky.r, sky.g, sky.b);
		pProgram->SetVec3f(kVar_avAmbientColorGround, ground.r, ground.g, ground.b);
		const cMatrixf &mtxView = mpCurrentFrustum->GetViewMatrix();
		pProgram->SetVec3f(kVar_avLightDirection, cMath::MatrixMul3x3(mtxView, pLight->GetDirection() * -1.0f));
		pProgram->SetVec3f(kVar_avViewSpaceUp, cMath::MatrixMul3x3(mtxView, cVector3f(0,1,0)));
		pProgram->SetFloat(kVar_afTranslucencyScale, pLight->GetTranslucency() * pLight->GetTranslucency() * 0.5f);
		if(WorldUnderwaterFog(this)) pProgram->SetColor4f(kVar_avFogColor, UnderwaterFogColor(mpCurrentWorld->GetFogColor()));

		if(lFlags & eFeature_Light_ShadowMap)
		{
			for(int i=0; i<4; ++i) pProgram->SetMatrixf(kVar_a_mtxLightViewProj0+i, vShadowMtx[i]);
			if(pProgram->SetInt(kVar_alSliceNum, 4))
			{
				for(int i=0; i<4; ++i) pProgram->SetMatrixf(kVar_a_mtxLightViewProjSlice0+i, mvDirShadowMtxSlice[i]);
				pProgram->SetFloat(kVar_afSliceDiv, 0.25f);
				pProgram->SetFloat(kVar_afShadowDistance, mfDirShadowDistance);
				pProgram->SetVec4f(kVar_avShadowParams, 0, mfDirShadowDepth0, 24.0f * mfDirShadowDepth0, 0);
				pProgram->SetVec2f(kVar_avShadowMapSize, cVector2f((float)mpDirShadowData->mpTexture->GetWidth(), (float)mpDirShadowData->mpTexture->GetHeight()));
			}
			pProgram->SetVec4f(kVar_avSplitsNear, vSplitsNear[0], vSplitsNear[1], vSplitsNear[2], vSplitsNear[3]);
			pProgram->SetVec4f(kVar_avSplitsFar, vSplitsFar[0], vSplitsFar[1], vSplitsFar[2], vSplitsFar[3]);
			pProgram->SetVec4f(kVar_avSplitOffsetMul, vSplitOffsetMul[0], vSplitOffsetMul[1], vSplitOffsetMul[2], vSplitOffsetMul[3]);
			pProgram->SetVec2f(kVar_avShadowMapOffsetMul, pLight->GetShadowMapBlurAmount() / (float)mpDirShadowData->mpTexture->GetWidth());
			SetTexture(6, mpDirShadowData->mpTexture);
			if(mpShadowJitterTexture) SetTexture(7, mpShadowJitterTexture);
		}

		SetStencilActive(false);
		SetDepthTest(false);
		SetCullMode(eCullMode_CounterClockwise);
		SetFlatProjectionMinMax(cVector3f(mfFarLeft,mfFarBottom,-mfFarPlane*1.5f),cVector3f(mfFarRight,mfFarTop,mfFarPlane*1.5f));
		SetVertexBuffer(mpFullscreenLightQuad);
		DrawCurrent();
		SetNormalFrustumProjection();
		SetCullMode(eCullMode_Clockwise);
		SetDepthTest(true);
	}

	void cRendererDeferred::RenderDirectionalShadowMap(cLightDirectional *apLight, cMatrixf *apMtx, float *apNear, float *apFar, float *apOffsetMul)
	{
		const int lSlices = 4;
		if(mpDirShadowData==NULL)
		{
			int lSize = mShadowMapResolution == eShadowMapResolution_High ? 2048 : mShadowMapResolution == eShadowMapResolution_Medium ? 1024 : 512;
			mpDirShadowData = CreateShadowMap("DirShadowMap", cVector3l(lSize, lSize, 1), ePixelFormat_Depth24);
		}
		int lSliceRes = mpDirShadowData->mpTexture->GetWidth() / 2;

		////////////////////////
		// Practical split scheme
		float fNear = mpCurrentFrustum->GetNearPlane(), fFar = mpCurrentFrustum->GetFarPlane();
		if(mpCurrentWorld->GetFogActive() && mpCurrentWorld->GetFogColor().a >= 0.9) fFar = mpCurrentWorld->GetFogEnd();
		const float fLightShadowDistance = apLight->GetShadowDistance();
		if(fLightShadowDistance != -1.0f) fFar = cMath::Min(fFar, fLightShadowDistance);
		mfDirShadowDistance = fLightShadowDistance == -1.0f ? fFar : fLightShadowDistance;
		float fLogTerm = apLight->GetAutoShadowSliceLogTerm();
		float vSplit[lSlices+1];
		for(int i=0; i<=lSlices; ++i)
		{
			float fT = (float)i / (float)lSlices;
			vSplit[i] = cMath::Interpolate(fNear + (fFar-fNear)*fT, fNear * powf(fFar/fNear, fT), fLogTerm);
		}

		////////////////////////
		// Light rotation, rows: right, up, back
		cVector3f vBack = apLight->GetDirection() * -1.0f;
		vBack.Normalize();
		cVector3f vUp0 = fabsf(vBack.y) > 0.99f ? cVector3f(1,0,0) : cVector3f(0,1,0);
		cVector3f vRight = cMath::Vector3Normalize(cMath::Vector3Cross(vUp0, vBack));
		cVector3f vUp = cMath::Vector3Cross(vBack, vRight);

		float fTanHalfFov = tanf(mpCurrentFrustum->GetFOV()*0.5f);
		float fAspect = mpCurrentFrustum->GetAspect();
		float fCasterDist = apLight->GetShadowCasterDistance();

		////////////////////////
		// Render states
		SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
		SetDepthTest(true);
		SetDepthWrite(true);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetAlphaLimit(mfDefaultAlphaLimit);
		SetChannelMode(eMaterialChannelMode_None);
		SetTextureRange(NULL,0);
		SetOcclusionPlanesActive(false);
		mpLowLevelGraphics->SetPolygonOffsetActive(true);
		if(mbShadowDepthClamp) mpLowLevelGraphics->SetDepthClampActive(true);
		SetCullActive(false);
		mpLowLevelGraphics->SetPolygonOffset(mpCurrentSettings->mfShadowMapBias * apLight->GetShadowMapBiasMul(),
											 mpCurrentSettings->mfShadowMapSlopeScaleBias * apLight->GetShadowMapSlopeScaleBiasMul());
		SetFrameBuffer(mpDirShadowData->mpBuffer, false, false);
		mpLowLevelGraphics->SetClearDepth(1);
		ClearFrameBuffer(eClearFrameBufferFlag_Depth, false);

		cFrustum *pLastFrustum = mpCurrentFrustum;
		for(int i=0; i<lSlices; ++i)
		{
			////////////////////////
			// Bounding sphere of the slice, rotation stable
			cVector3f vCorners[8];
			cVector3f vCenter(0);
			for(int j=0; j<8; ++j)
			{
				float fZ = vSplit[i + j/4];
				float fH = fZ * fTanHalfFov;
				vCorners[j] = cMath::MatrixMul(m_mtxInvView, cVector3f((j&1 ? 1 : -1)*fH*fAspect, (j&2 ? 1 : -1)*fH, -fZ));
				vCenter += vCorners[j] / 8.0f;
			}
			float fRadius = 0;
			for(int j=0; j<8; ++j) fRadius = cMath::Max(fRadius, cMath::Vector3Dist(vCorners[j], vCenter));
			fRadius = ceilf(fRadius * 16.0f) / 16.0f;

			////////////////////////
			// Light view snapped to texels so the map does not swim
			cVector3f vC(cMath::Vector3Dot(vRight, vCenter), cMath::Vector3Dot(vUp, vCenter), cMath::Vector3Dot(vBack, vCenter));
			float fTexel = 2.0f * fRadius / (float)lSliceRes;
			vC.x = floorf(vC.x / fTexel) * fTexel;
			vC.y = floorf(vC.y / fTexel) * fTexel;
			vC.z += fRadius + fCasterDist;
			cMatrixf mtxView(	vRight.x, vRight.y, vRight.z, -vC.x,
								vUp.x,    vUp.y,    vUp.z,    -vC.y,
								vBack.x,  vBack.y,  vBack.z,  -vC.z,
								0, 0, 0, 1);
			float fDepth = 2.0f * fRadius + fCasterDist;
			cMatrixf mtxProj = cMath::MatrixOrthographicProjection(0, fDepth, cVector2f(2.0f * fRadius));
			cVector3f vEye = vRight * vC.x + vUp * vC.y + vBack * vC.z;
			cFrustum *pFrustum = &mDirShadowFrustum[i];
			pFrustum->SetupOrthoProj(mtxProj, mtxView, fDepth, 0, cVector2f(2.0f * fRadius), vEye);

			////////////////////////
			// Casters
			mvShadowCasters.resize(0);
			if(apLight->GetShadowCastersAffected() & eObjectVariabilityFlag_Dynamic)
				GetShadowCasters(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), mvShadowCasters, pFrustum, mbSunCasterCull ? 4.0f * fTexel : 0, mbSunCasterCull);
			if(apLight->GetShadowCastersAffected() & eObjectVariabilityFlag_Static)
				GetShadowCasters(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), mvShadowCasters, pFrustum, mbSunCasterCull ? 4.0f * fTexel : 0, mbSunCasterCull);

			mpLowLevelGraphics->SetCurrentFrameBuffer(mpDirShadowData->mpBuffer, cVector2l((i%2)*lSliceRes, (1-i/2)*lSliceRes), cVector2l(lSliceRes));
			mpCurrentFrustum = pFrustum;
			SetFrustumProjection(pFrustum);
			RenderShadowCastersNormal(pFrustum);
			mpCurrentFrustum = pLastFrustum;

			////////////////////////
			// Lookup: view space -> atlas quadrant
			cMatrixf mtxAtlas(	0.25f, 0, 0, 0.25f + 0.5f*(i%2),
								0, 0.25f, 0, 0.25f + 0.5f*(i/2),
								0, 0, 0.5f, 0.5f,
								0, 0, 0, 1);
			apMtx[i] = cMath::MatrixMul(cMath::MatrixMul(mtxAtlas, cMath::MatrixMul(mtxProj, mtxView)), m_mtxInvView);
			cMatrixf mtxBias(	0.5f, 0, 0, 0.5f,
								0, 0.5f, 0, 0.5f,
								0, 0, 0.5f, 0.5f,
								0, 0, 0, 1);
			mvDirShadowMtxSlice[i] = cMath::MatrixMul(cMath::MatrixMul(mtxBias, cMath::MatrixMul(mtxProj, mtxView)), m_mtxInvView);
			if(i == 0) mfDirShadowDepth0 = fDepth;
			apNear[i] = -vSplit[i];
			apFar[i] = i == lSlices-1 ? -1e6f : -vSplit[i+1];
			apOffsetMul[i] = (vSplit[1]-vSplit[0]) / (vSplit[i+1]-vSplit[i]);
		}

		SetTexture(0,NULL);
		SetOcclusionPlanesActive(true);
		mpLowLevelGraphics->SetPolygonOffsetActive(false);
		if(mbShadowDepthClamp) mpLowLevelGraphics->SetDepthClampActive(false);
		SetCullActive(true);
		SetNormalFrustumProjection();

		SetAccumulationBuffer();
		for(int i=0; i<mlNumOfGBufferTextures; ++i) SetTexture(i, GetBufferTexture(i));
		SetDepthTestFunc(eDepthTestFunc_GreaterOrEqual);
		SetDepthWrite(false);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetBlendMode(eMaterialBlendMode_Add);
		SetAlphaMode(eMaterialAlphaMode_Solid);
	}

	void cRendererDeferred::RenderLights()
	{
		START_RENDER_PASS(Lights);

		/////////////////////////////////////////
		// Sort the Lights and place them in the lists
		InitLightRendering();


		/////////////////////////////////////////
		// Render SSAO (used by box lights)
		RenderSSAO();
		RenderHpl3SSAO();
		
		/////////////////////////////////////////
		// Set up general render states.
		SetDepthTest(true);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_Add);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		for(int i=0; i<mlNumOfGBufferTextures; ++i) SetTexture(i, GetBufferTexture(i));
		SetTextureRange(NULL, mlNumOfGBufferTextures);

		////////////////////////////////
		//Set up frame buffer
		SetAccumulationBuffer();
		ClearFrameBuffer(eClearFrameBufferFlag_Color |eClearFrameBufferFlag_Stencil,true); //Perhaps there is some ambient light that can be added instead?

		///////////////////////
		// Set up culling and depth mode
		SetCullMode(eCullMode_Clockwise);
		SetDepthTestFunc(eDepthTestFunc_GreaterOrEqual);

		///////////////////////
		// Render box lights
		if(!RenderLights_BoxWeighted())
		{
			RenderLights_Box_StencilFront_RenderBack();
			RenderLights_Box_RenderBack();
		}
		RenderLights_Directional();
		
		///////////////////////
		// Render lights that are inside near plane
		RenderLights_StencilBack_ScreenQuad();

		///////////////////////
		// Render lights with large screen size or with shadows.
		RenderLights_StencilFront_RenderBack();

		///////////////////////
		// Simple rendering with no stencil
		RenderLights_RenderBack();
		
		///////////////////////
		// Batch lights and renderer several at a time.
		// Skip for now...
		//RenderLights_Batches();
				
		////////////////////////////
		//Reset settings
		SetStencilActive(false);
		SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
		SetCullMode(eCullMode_CounterClockwise);

		RenderNightVision();
		ApplyHpl3SSAO();

		////////////////////////////
		//Debug: Draw wire frame for all lights!
		#ifdef kDebug_RenderLightData
		{
			SetDepthTest(true);
			SetDepthWrite(false);
			SetBlendMode(eMaterialBlendMode_None);
			
			SetVertexBuffer(NULL);
			SetProgram(NULL);
			SetTextureRange(NULL,0);
			SetMatrix(NULL);

			for(int i=0; i<mpRenderList->GetLightNum(); ++i)
			{
				iLight* pLight = mpRenderList->GetLight(i);

				if(pLight->GetLightType() == eLightType_Point)
				{
					mpLowLevelGraphics->DrawSphere(pLight->GetWorldPosition(),pLight->GetRadius(), cColor(1,1));
					mpLowLevelGraphics->DrawSphere(pLight->GetWorldPosition(),0.2f, cColor(0.6f,1));
				}
				else if(pLight->GetLightType() == eLightType_Spot)
				{
					cLightSpot *pLightSpot = static_cast<cLightSpot*>(pLight);
					
					pLightSpot->GetFrustum()->Draw(mpLowLevelGraphics);
				}
			}
		}
		#endif

		
		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderIllumination()
	{
		if(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Illumination)==false) return;

		cRenderableVecIterator illumIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Illumination);
		if(illumIt.HasNext()==false) return;

		START_RENDER_PASS(Illumination);

		
		SetDepthTest(true);
		SetDepthWrite(false);
		SetDepthTestFunc(eDepthTestFunc_Equal);
		SetBlendMode(eMaterialBlendMode_Add);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		SetTextureRange(NULL,1);
				
		while(illumIt.HasNext())
		{
			iRenderable *pObject = illumIt.Next();
			cMaterial *pMaterial = pObject->GetMaterial();
			SetMaterialProgram(eMaterialRenderMode_Illumination,pMaterial);
			
			SetTexture(0,pMaterial->GetTextureInUnit(eMaterialRenderMode_Illumination,0));
			
			SetMatrix(pObject->GetModelMatrixPtr());

			SetVertexBuffer(pObject->GetVertexBuffer());

			DrawCurrentMaterial(eMaterialRenderMode_Illumination, pObject);
		}

		SetDepthTestFunc(eDepthTestFunc_LessOrEqual);

		
		END_RENDER_PASS();
	}
	
	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderDecals()
	{
		if(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Decal)==false) return;

		START_RENDER_PASS(Decals);

		SetGBuffer(eGBufferComponents_ColorAndDepth);

		SetDepthTest(true);
		SetDepthWrite(false);

		SetAlphaLimit(0.01f);
		SetAlphaMode(eMaterialAlphaMode_Trans);
		// HPL3 keeps diffuse alpha (translucency) out of decals
		SetChannelMode(mbDepthInNormalAlpha ? eMaterialChannelMode_RGB : eMaterialChannelMode_RGBA);
		SetDepthTestFunc(eDepthTestFunc_LessOrEqual);


		float fHalfFovTan=0;

		cRenderableVecIterator transIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Decal);
		while(transIt.HasNext())
		{
			iRenderable *pObject = transIt.Next();
			cMaterial *pMaterial = pObject->GetMaterial();

			SetBlendMode(pMaterial->GetBlendMode());

			SetMaterialProgram(eMaterialRenderMode_Diffuse,pMaterial);

			SetMaterialTextures(eMaterialRenderMode_Diffuse, pMaterial);

			SetMatrix(pObject->GetModelMatrixPtr());

			SetVertexBuffer(pObject->GetVertexBuffer());

			DrawCurrent();
		}

		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetAlphaLimit(mfDefaultAlphaLimit);
		SetChannelMode(eMaterialChannelMode_RGBA);


		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::SetFogDepthTexture(bool abBind, int alUnit)
	{
		if(mbDepthInNormalAlpha == false)
		{
			if(abBind) SetTexture(alUnit, GetGbufferTexture(mlLinearDepthTextureIdx));
			return;
		}
		GetGbufferTexture(1)->SetRedFromAlpha(abBind);
		if(abBind) SetTexture(alUnit, GetGbufferTexture(1));
	}

	void cRendererDeferred::RenderHpl3SSAO()
	{
		mbH3SSAORendered = false;
		if(mpH3SSAORenderProgram==NULL || mpH3SSAODownsampleProgram==NULL || mpH3SSAOBlurProgram==NULL ||
			mpH3SSAOTemporalProgram==NULL || mpH3SSAOUpsampleProgram==NULL) return;
		if(mpCurrentSettings->mbIsReflection || mpCurrentSettings->mbSSAOActive==false) return;

		START_RENDER_PASS(SSAO);

		const float fDiv = (float)mlSSAOBufferSizeDiv;
		float fTanHalfFov = tanf(mpCurrentFrustum->GetFOV() * 0.5f);
		float fT = fTanHalfFov * mfFarPlane;
		float fW = mpCurrentFrustum->GetAspect() * fT;
		cVector3f vUVToView0(2*fW, 2*fT, 0), vUVToView1(-fW, -fT, -mfFarPlane);
		cVector2f vSize((float)mpH3SSAOTexture[0]->GetWidth(), (float)mpH3SSAOTexture[0]->GetHeight());
		iTexture *pDepthN = GetBufferTexture(1);

		SetChannelMode(eMaterialChannelMode_RGBA);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetBlendMode(eMaterialBlendMode_None);
		SetDepthTest(false);
		SetDepthWrite(false);
		SetTextureRange(NULL, 0);
		SetFlatProjection();

		SetProgram(mpH3SSAODownsampleProgram);
		mpH3SSAODownsampleProgram->SetVec2f(kVar_avInvScreenSize, cVector2f(1.0f / mvScreenSizeFloat.x, 1.0f / mvScreenSizeFloat.y));
		SetFogDepthTexture(true, 0);
		for(size_t i=0; i<mvH3SSAOMipBuffers.size(); ++i)
		{
			SetFrameBuffer(mvH3SSAOMipBuffers[i], false, false);
			DrawQuad(cVector2f(0,0),1, cVector2f(0,0), cVector2f(1,1), true);
		}
		SetFogDepthTexture(false, 0);

		mfH3SSAOTime = fmodf(mfH3SSAOTime + mfCurrentFrameTime, 256.0f);
		cVector2f vMipSize((float)mpH3SSAOMipTexture->GetWidth(), (float)mpH3SSAOMipTexture->GetHeight());
		SetFrameBuffer(mpH3SSAOBuffer[0], false, false);
		SetProgram(mpH3SSAORenderProgram);
		mpH3SSAORenderProgram->SetVec3f(kVar_avUVToView0, vUVToView0);
		mpH3SSAORenderProgram->SetVec3f(kVar_avUVToView1, vUVToView1);
		mpH3SSAORenderProgram->SetFloat(kVar_afT, mfH3SSAOTime * 1.9416110515594482f);
		mpH3SSAORenderProgram->SetFloat(kVar_afStepSizeMax, 0.5f / fTanHalfFov);
		mpH3SSAORenderProgram->SetFloat(kVar_afFarPlane, mfFarPlane);
		mpH3SSAORenderProgram->SetFloat(kVar_afRadius, 1.5f);
		mpH3SSAORenderProgram->SetFloat(kVar_afScreenSizeDiv, fDiv);
		mpH3SSAORenderProgram->SetFloat(kVar_afLodScale, 0.4f * (int)sqrtf(vMipSize.x*vMipSize.x + vMipSize.y*vMipSize.y + 1));
		SetTexture(0, pDepthN);
		SetTexture(1, mpH3SSAOMipTexture);
		DrawQuad(cVector2f(0,0),1, cVector2f(0,0), cVector2f(1,1), true);
		SetTexture(1, NULL);

		SetProgram(mpH3SSAOBlurProgram);
		mpH3SSAOBlurProgram->SetFloat(kVar_afFarPlane, mfFarPlane);
		mpH3SSAOBlurProgram->SetFloat(kVar_afDepthDifference, 0.0625f);
		cVector2f vBlurDir[2] = {cVector2f(0, 1.0f / vSize.y), cVector2f(1.0f / vSize.x, 0)};
		int vBlurPasses[4][3] = {{0,1,0}, {1,0,1}, {1,0,0}, {0,2,1}};

		for(int i=0; i<2; ++i)
		{
			SetFrameBuffer(mpH3SSAOBuffer[vBlurPasses[i][1]], false, false);
			mpH3SSAOBlurProgram->SetVec2f(kVar_avDirection, vBlurDir[vBlurPasses[i][2]]);
			SetTexture(0, mpH3SSAOTexture[vBlurPasses[i][0]]);
			DrawQuad(cVector2f(0,0),1, cVector2f(0,0), cVector2f(1,1), true);
		}

		const cMatrixf& mtxView = mpCurrentFrustum->GetViewMatrix();
		cMatrixf mtxTemporalView = mbH3SSAOFirstFrame ? cMatrixf::Identity : cMath::MatrixMul(m_mtxH3SSAOPrevView, cMath::MatrixInverse(mtxView));
		float fAmount = cMath::Clamp(mfCurrentFrameTime, 1.0f/60.0f, 1.0f/15.0f);
		float fMoved = mtxTemporalView.GetTranslation().Length();
		if(fMoved > 0.1f) fAmount += (fMoved - 0.1f) / 10.0f;
		if(mbH3SSAOFirstFrame) fAmount += 2.0f;
		mbH3SSAOFirstFrame = false;
		m_mtxH3SSAOPrevView = mtxView;

		SetFrameBuffer(mpH3SSAOBuffer[1], false, false);
		SetProgram(mpH3SSAOTemporalProgram);
		mpH3SSAOTemporalProgram->SetFloat(kVar_afFarPlane, mfFarPlane);
		mpH3SSAOTemporalProgram->SetVec3f(kVar_avUVToView0, vUVToView0);
		mpH3SSAOTemporalProgram->SetVec3f(kVar_avUVToView1, vUVToView1);
		mpH3SSAOTemporalProgram->SetMatrixf(kVar_a_mtxTemporalView, mtxTemporalView);
		mpH3SSAOTemporalProgram->SetMatrixf(kVar_a_mtxTemporalProjection, mpCurrentFrustum->GetProjectionMatrix());
		mpH3SSAOTemporalProgram->SetVec2f(kVar_avScreenSize, vSize);
		mpH3SSAOTemporalProgram->SetFloat(kVar_afTemporalBlurAmount, cMath::Clamp(fAmount, 0.0f, 1.0f));
		SetTexture(0, mpH3SSAOTexture[0]);
		SetTexture(1, mpH3SSAOTexture[2]);
		DrawQuad(cVector2f(0,0),1, cVector2f(0,0), cVector2f(1,1), true);
		SetTexture(1, NULL);

		SetProgram(mpH3SSAOBlurProgram);
		for(int i=2; i<4; ++i)
		{
			SetFrameBuffer(mpH3SSAOBuffer[vBlurPasses[i][1]], false, false);
			mpH3SSAOBlurProgram->SetVec2f(kVar_avDirection, vBlurDir[vBlurPasses[i][2]]);
			SetTexture(0, mpH3SSAOTexture[vBlurPasses[i][0]]);
			DrawQuad(cVector2f(0,0),1, cVector2f(0,0), cVector2f(1,1), true);
		}

		SetTexture(0, NULL);
		SetProgram(NULL);
		SetNormalFrustumProjection();
		mbH3SSAORendered = true;

		END_RENDER_PASS();
	}

	void cRendererDeferred::RenderNightVision()
	{
		if(mpNightVisionProgram==NULL || mpCurrentWorld->GetNightVisionVisible()==false) return;

		cColor col = mpCurrentWorld->GetNightVisionColor();
		float fBrightness = mpCurrentWorld->GetNightVisionBrightness();

		SetDepthTest(false);
		SetDepthWrite(false);
		SetStencilActive(false);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetBlendMode(eMaterialBlendMode_Add);
		SetFlatProjection();

		SetAccumulationBuffer();
		SetProgram(mpNightVisionProgram);
		mpNightVisionProgram->SetFloat(kVar_afFarPlane, mfFarPlane);
		mpNightVisionProgram->SetColor4f(kVar_avLightColor, cColor(col.r*col.r*fBrightness, col.g*col.g*fBrightness, col.b*col.b*fBrightness, col.a));
		mpNightVisionProgram->SetFloat(kVar_afFalloffPow, mpCurrentWorld->GetNightVisionFalloff());
		mpNightVisionProgram->SetFloat(kVar_afMaxDistance, mpCurrentWorld->GetNightVisionRadiusMax());
		SetScreenPositionVars(mpNightVisionProgram);
		for(int i=0; i<3; ++i) SetTexture(i, GetGbufferTexture(i));
		SetFogDepthTexture(true, 3);
		DrawQuad(cVector2f(0,0),1, cVector2f(0,0), cVector2f(1,1), true);
		SetFogDepthTexture(false, 3);
		SetTextureRange(NULL, 0);
		SetProgram(NULL);

		SetNormalFrustumProjection();
		SetDepthTest(true);
	}

	void cRendererDeferred::ApplyHpl3SSAO()
	{
		if(mbH3SSAORendered==false) return;

		SetDepthTest(false);
		SetDepthWrite(false);
		SetStencilActive(false);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetBlendMode(eMaterialBlendMode_Mul);
		SetFlatProjection();

		SetAccumulationBuffer();
		SetProgram(mpH3SSAOUpsampleProgram);
		mpH3SSAOUpsampleProgram->SetFloat(kVar_afFarPlane, mfFarPlane);
		mpH3SSAOUpsampleProgram->SetFloat(kVar_afSizeDiv, 1.0f / mlSSAOBufferSizeDiv);
		mpH3SSAOUpsampleProgram->SetFloat(kVar_afPower, 8.0f);
		SetTexture(0, mpH3SSAOTexture[2]);
		SetFogDepthTexture(true, 1);
		cVector2f vHalfTexel(0.5f / mpH3SSAOTexture[2]->GetWidth(), 0.5f / mpH3SSAOTexture[2]->GetHeight());
		DrawQuad(cVector2f(0,0),1, vHalfTexel * -1.0f, cVector2f(1,1) - vHalfTexel, true);
		SetFogDepthTexture(false, 1);
		SetTexture(0, NULL);
		SetTexture(1, NULL);
		SetProgram(NULL);

		SetBlendMode(eMaterialBlendMode_Add);
		SetNormalFrustumProjection();
		SetDepthTest(true);
	}

	bool cRendererDeferred::DepthOfFieldIsActive()
	{
		return mpCurrentSettings->mbIsReflection==false && mpDofFocusProgram && mpDofBlurProgram &&
			mpCurrentWorld->IsDepthOfFieldActive() && mpCurrentWorld->GetDepthOfFieldFalloff() > 0;
	}

	void cRendererDeferred::RenderDepthOfField()
	{
		if(DepthOfFieldIsActive()==false) return;

		START_RENDER_PASS(DepthOfField);

		SetDepthTest(false);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetFlatProjection();

		float fStart = mpCurrentWorld->GetDepthOfFieldFocusStart();
		float fEnd = mpCurrentWorld->GetDepthOfFieldFocusEnd();
		float fFalloffLen = (fEnd - fStart) * 0.5f / cMath::Max(1e-15f, mpCurrentWorld->GetDepthOfFieldFalloff());
		float fNear = mpCurrentFrustum->GetNearPlane();

		iFrameBuffer *pBufferA = mpGraphics->GetTempFrameBuffer(mvScreenSize,ePixelFormat_RGBA16,6);
		iFrameBuffer *pBufferB = mpGraphics->GetTempFrameBuffer(mvScreenSize,ePixelFormat_RGBA16,7);
		iTexture *pTexA = pBufferA->GetColorBuffer(0)->ToTexture();
		iTexture *pTexB = pBufferB->GetColorBuffer(0)->ToTexture();

		SetFrameBuffer(pBufferA, false);
		SetProgram(mpDofFocusProgram);
		mpDofFocusProgram->SetVec4f(kVar_avFocusStartEnd, fNear + 0.5f*fStart, fNear + fStart, fEnd, fEnd + fFalloffLen);
		mpDofFocusProgram->SetFloat(kVar_afFarPlane, mfFarPlane);
		eTextureFilter accumFilter = mpAccumBufferTexture->GetFilter();
		mpAccumBufferTexture->SetFilter(eTextureFilter_Nearest);
		SetTexture(0, mpAccumBufferTexture);
		SetFogDepthTexture(true, 1);
		DrawQuad(cVector2f(0,0),1, cVector2f(0,0), cVector2f(1,1), true);
		SetFogDepthTexture(false, 1);
		mpAccumBufferTexture->SetFilter(accumFilter);

		SetFrameBuffer(pBufferB, false);
		SetProgram(mpDofBlurProgram);
		mpDofBlurProgram->SetVec2f(kVar_avOffsetMul, cVector2f(1.0f / mvScreenSizeFloat.x, 0));
		pTexA->SetFilter(eTextureFilter_Nearest);
		SetTexture(0, pTexA);
		SetTexture(1, mpDofGaussTexture);
		DrawQuad(cVector2f(0,0),1, cVector2f(0,0), cVector2f(1,1), true);

		SetAccumulationBuffer();
		mpDofBlurProgram->SetVec2f(kVar_avOffsetMul, cVector2f(0, -1.0f / mvScreenSizeFloat.y));
		pTexB->SetFilter(eTextureFilter_Nearest);
		SetTexture(0, pTexB);
		DrawAccumulationQuad();

		SetTexture(0, NULL);
		SetTexture(1, NULL);
		SetProgram(NULL);
		SetNormalFrustumProjection();

		END_RENDER_PASS();
	}

	static cVector3f FogNoiseTurbulence(const cVector3f& avTurb, float afSize, float afTime)
	{
		float t = afTime*0.0625f, a = 4*t, b = a+1;
		float d1 = 0.125f*sin(2.5f*b+13+1.13f*t) + 0.25f*sin(b+17+1.17f*t) + sin(0.5f*b+103+t);
		float b2 = b+d1;
		float d2 = 0.125f*sin(2.5f*b2+103+1.13f*t) + 0.25f*sin(b2+13+1.17f*t) + sin(0.5f*b2+113+t);
		return cVector3f(d1*avTurb.x, a*avTurb.y, d2*avTurb.z) * cMath::Max(1.0f, afSize*0.25f);
	}

	iGpuProgram* cRendererDeferred::SetupFogProgram(cFogArea *apFogArea, tFlag alFlags, bool abUnderwaterPass)
	{
		cWorld *pWorld = mpCurrentWorld;
		float fStart = apFogArea ? apFogArea->GetStart() : pWorld->GetFogStart();
		float fEnd = apFogArea ? apFogArea->GetEnd() : pWorld->GetFogEnd();
		float fFalloff = apFogArea ? apFogArea->GetFalloffExp() : pWorld->GetFogFalloffExp();
		const cColor& color = apFogArea ? apFogArea->GetColor() : pWorld->GetFogColor();
		float fBrightness = apFogArea ? apFogArea->GetBrightness() : pWorld->GetFogBrightness();
		float fNoiseStrength = apFogArea ? apFogArea->GetNoiseStrength() : pWorld->GetFogNoiseStrength();
		float fNoiseSize = apFogArea ? apFogArea->GetNoiseSize() : pWorld->GetFogNoiseSize();
		const cVector3f& vTurb = apFogArea ? apFogArea->GetNoiseTurbulence() : pWorld->GetFogNoiseTurbulence();
		bool bSecondary = apFogArea==NULL && pWorld->GetSecondaryFogActive();

		if(fNoiseStrength > 0)
		{
			if(mpFogNoiseTexture==NULL) mpFogNoiseTexture = mpResources->GetTextureManager()->Create3D("core_value_noise.dds", true);
			if(mpFogNoiseTexture) alFlags |= eFeature_FogArea_Noise;
		}
		bool bSkybox = false;
		if(abUnderwaterPass)
		{
			alFlags |= eFeature_FogArea_Underwater;
		}
		else
		{
			bSkybox = (apFogArea ? apFogArea->GetSkybox() : pWorld->GetFogUseSkybox()) && pWorld->GetFogSkyboxTexture() && pWorld->GetSkyBoxActive();
			if(bSkybox) alFlags |= eFeature_FogArea_Skybox;
			if(bSkybox && pWorld->GetFogSkyboxTexture() != pWorld->GetSkyBoxTexture()) alFlags |= eFeature_FogArea_SeparateSkybox;
			if(apFogArea) alFlags |= eFeature_FogArea_FogArea;
			if(apFogArea ? apFogArea->GetLighten() : pWorld->GetFogLighten()) alFlags |= eFeature_FogArea_Lighten;
		}
		if(bSecondary) alFlags |= eFeature_FogArea_Secondary;

		iGpuProgram *pProgram = mpFogProgramManager->GenerateProgram(0, alFlags);
		SetProgram(pProgram);
		if(pProgram==NULL) return NULL;

		cVector2f vStartAndLength(fStart, fEnd - fStart);
		cColor fogColor = abUnderwaterPass ? UnderwaterFogColor(color) : GetFogRenderColor(color, fBrightness);

		bool bExp = apFogArea ? apFogArea->GetExponential() : pWorld->GetFogExponential();
		bool bHeight = apFogArea ? apFogArea->GetHeightBased() : pWorld->GetFogHeightBased();
		cColor settings(fStart, fEnd, fFalloff, 0), secSettings(0, 0);
		cVector3f vCamHeight(mpCurrentFrustum->GetOrigin().y);
		if(bExp || bHeight)
		{
			settings.b = (apFogArea ? apFogArea->GetDensity() : pWorld->GetFogDensity()) * 0.03125f;
			settings.a = apFogArea ? apFogArea->GetHeightDensity() : pWorld->GetFogHeightDensity();
			vCamHeight.x -= apFogArea ? apFogArea->GetHeightHorizon() : pWorld->GetFogHeightHorizon();
		}
		if(apFogArea && bHeight) vCamHeight -= cVector3f(apFogArea->GetBoundingVolume()->GetMin().y);
		if(bSecondary && abUnderwaterPass==false)
		{
			vCamHeight.y -= pWorld->GetSecondaryFogHeightHorizon();
			secSettings = cColor(pWorld->GetSecondaryFogStart(), pWorld->GetSecondaryFogEnd(), pWorld->GetSecondaryFogFalloffExp(), 0);
			if(bExp || bHeight)
			{
				// x32 vs x1/32 for the primary: as in the official renderer
				secSettings.b = pWorld->GetSecondaryFogDensity() * 32.0f;
				secSettings.a = pWorld->GetSecondaryFogHeightDensity();
			}
			if(pWorld->GetSecondaryFogEnd() > fEnd)
			{
				std::swap(settings, secSettings);
				std::swap(vCamHeight.x, vCamHeight.y);
			}
		}
		pProgram->SetColor4f(kVar_avFogSettings, settings);
		pProgram->SetColor4f(kVar_avSecondFogSettings, secSettings);
		pProgram->SetVec3f(kVar_avCameraPos, vCamHeight);
		pProgram->SetInt(kVar_alFogType, (bExp ? 1 : 0) | (bHeight ? 2 : 0));

		if(bSecondary)
		{
			cVector2f vSecStartAndLength(pWorld->GetSecondaryFogStart(), pWorld->GetSecondaryFogEnd() - pWorld->GetSecondaryFogStart());
			cColor secColor = GetFogRenderColor(pWorld->GetSecondaryFogColor(), pWorld->GetSecondaryFogBrightness());
			float fSecFalloff = pWorld->GetSecondaryFogFalloffExp();
			if(abUnderwaterPass==false && pWorld->GetSecondaryFogEnd() > fEnd)
			{
				std::swap(vStartAndLength, vSecStartAndLength);
				std::swap(fogColor, secColor);
				std::swap(fFalloff, fSecFalloff);
			}
			pProgram->SetColor4f(kVar_avSecondFogColor, secColor);
			pProgram->SetVec2f(kVar_avSecondFogStartAndLength, vSecStartAndLength);
			pProgram->SetFloat(kVar_afSecondFalloffExp, fSecFalloff);
		}

		pProgram->SetFloat(kVar_afNegFarPlane, -mpCurrentFrustum->GetFarPlane());
		pProgram->SetVec2f(kVar_avFogStartAndLength, vStartAndLength);
		pProgram->SetColor4f(kVar_avFogColor, fogColor);
		pProgram->SetFloat(kVar_afFalloffExp, fFalloff);
		SetScreenPositionVars(pProgram);
		pProgram->SetMatrixf(kVar_a_mtxNoise, cMath::MatrixMul(cMath::MatrixTranslate(FogNoiseTurbulence(vTurb, fNoiseSize, GetTimeCount())), m_mtxInvView));
		if(alFlags & eFeature_FogArea_Noise)
		{
			pProgram->SetFloat(kVar_afInvNoiseScale, 1.0f / fNoiseSize);
			pProgram->SetFloat(kVar_afNoiseStrength, fNoiseStrength);
			SetTexture(1, mpFogNoiseTexture);
		}
		if(bSkybox)
		{
			pProgram->SetColor4f(kVar_avSkyboxColor, GetFogRenderColor(pWorld->GetSkyBoxColor(), pWorld->GetSkyBoxBrightness()));
			SetTexture(2, pWorld->GetFogSkyboxTexture());
			pProgram->SetMatrixf(kVar_a_mtxSkybox, cMath::MatrixRotate(pWorld->GetSkyBoxRotation(), eEulerRotationOrder_XYZ).GetTranspose());
			const cVector3l& vSkySize = pWorld->GetFogSkyboxTexture()->GetSize();
			pProgram->SetFloat(kVar_afSkyboxMipNum, log2f((float)cMath::Max(vSkySize.x, vSkySize.y)));
		}
		if(alFlags & eFeature_FogArea_Lighten)
		{
			CopyFrameBufferToTexure(mpRefractionTexture, 0, mvRenderTargetSize, 0, true);
			SetTexture(3, mpRefractionTexture);
			SetBlendMode(eMaterialBlendMode_None);
		}
		return pProgram;
	}

	void cRendererDeferred::RenderFullScreenFog()
	{
		if(WorldFogActive()==false) return;

		START_RENDER_PASS(FullScreenFog);

		//////////////////////////
		// Set up rendering
		SetDepthTest(false);
		SetDepthWrite(false);
		SetChannelMode(eMaterialChannelMode_RGBA);

		SetAlphaMode(eMaterialAlphaMode_Solid);

		SetFogDepthTexture(true);
		SetTextureRange(NULL, 1);

		SetMatrix(NULL);
		SetFlatProjection();

		for(int lPass = mpCurrentWorld->GetFogUnderwater() ? 0 : 1; lPass < 2; ++lPass)
		{
			SetBlendMode(lPass==0 ? eMaterialBlendMode_Mul : eMaterialBlendMode_Alpha);
			if(SetupFogProgram(NULL, 0, lPass==0)) DrawQuad(0, 1);
		}

		SetNormalFrustumProjection();
		SetDepthTest(true);
		SetFogDepthTexture(false);
		SetTextureRange(NULL, 1);

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------


	void cRendererDeferred::RenderFog()
	{
		if(mpCurrentRenderList->GetFogAreaNum()==0)
		{
			mpCurrentSettings->mvFogRenderData.resize(0); //Make sure render data array is empty!
			return;
		}

		START_RENDER_PASS(Fog);

		SetupFogRenderDataArray(true);

		SetDepthTest(true);
		SetDepthWrite(false);
		SetChannelMode(eMaterialChannelMode_RGBA);

		SetAlphaMode(eMaterialAlphaMode_Solid);
		
		SetFogDepthTexture(true);
		SetTextureRange(NULL, 1);

		
		for(size_t i=0; i<mpCurrentSettings->mvFogRenderData.size(); ++i)
		{
			cFogAreaRenderData& fogData = mpCurrentSettings->mvFogRenderData[i];
			cFogArea *pFogArea = fogData.mpFogArea;
			
			/////////////////////////////////////////////
			// Get program
			int lFlags =0;
			if(fogData.mbInsideNearFrustum)
			{
				if(pFogArea->GetShowBacksideWhenInside()) lFlags |= eFeature_FogArea_Backside;
			}
			else
			{
				if(pFogArea->GetShowBacksideWhenOutside()) lFlags |= eFeature_FogArea_Backside;
				lFlags |= eFeature_FogArea_OutsideBox;
			}
			
			for(int lPass = pFogArea->GetUnderwater() ? 0 : 1; lPass < 2; ++lPass)
			{
				SetBlendMode(lPass==0 ? eMaterialBlendMode_Mul : eMaterialBlendMode_Alpha);
				iGpuProgram *pProgram = SetupFogProgram(pFogArea, lFlags, lPass==0);
				if(pProgram==NULL) continue;

				/////////////////////////////////////////////
				//Outside of box setup
				if(fogData.mbInsideNearFrustum==false)
				{
					cMatrixf mtxInvModelView = cMath::MatrixInverse( cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), *pFogArea->GetModelMatrixPtr()) );
					cVector3f vRayCastStart = cMath::MatrixMul(mtxInvModelView, cVector3f(0));

				
					pProgram->SetVec3f(kVar_avRayCastStart, vRayCastStart);
					pProgram->SetMatrixf(kVar_a_mtxBoxInvViewModelRotation, mtxInvModelView.GetRotation());

					cVector3f vNegPlaneDistNeg( cMath::PlaneToPointDist(cPlanef(-1,0,0,0.5f),vRayCastStart), cMath::PlaneToPointDist(cPlanef(0,-1,0,0.5f),vRayCastStart),
												cMath::PlaneToPointDist(cPlanef(0,0,-1,0.5f),vRayCastStart));
					cVector3f vNegPlaneDistPos( cMath::PlaneToPointDist(cPlanef(1,0,0,0.5f),vRayCastStart), cMath::PlaneToPointDist(cPlanef(0,1,0,0.5f),vRayCastStart),
												cMath::PlaneToPointDist(cPlanef(0,0,1,0.5f),vRayCastStart));

					pProgram->SetVec3f(kVar_avNegPlaneDistNeg, vNegPlaneDistNeg*-1);
					pProgram->SetVec3f(kVar_avNegPlaneDistPos, vNegPlaneDistPos*-1);
				}
			
				/////////////////////////////////////////////
				// Render
				SetCullMode(fogData.mbInsideNearFrustum ? eCullMode_Clockwise : eCullMode_CounterClockwise);
				SetDepthTest(!fogData.mbInsideNearFrustum);

				SetMatrix(pFogArea->GetModelMatrixPtr());

				SetVertexBuffer(mpShapeBox);

				DrawCurrent();
			}
		}

		//////////////////////////////////
		// Reset settings
		SetCullMode(eCullMode_CounterClockwise);
		SetDepthTest(true);
		SetFogDepthTexture(false);
		SetTextureRange(NULL, 1);

		END_RENDER_PASS();
	}
	
	//-----------------------------------------------------------------------


	void cRendererDeferred::RenderEnvironmentParticles(bool abBehindFocus)
	{
		if(mpCurrentWorld->GetEnvironmentParticlesActive()==false || mpCurrentWorld->GetEnvironmentParticleNum()==0) return;
		bool bDof = DepthOfFieldIsActive();
		if(!bDof && !abBehindFocus) return;

		float fDofParams[4] = {0, 0, abBehindFocus ? 0.0f : 1.0f, abBehindFocus ? 1.0f : -1.0f};
		if(bDof)
		{
			const cMatrixf& mtxProj = mpCurrentFrustum->GetProjectionMatrix();
			float fEndZ = cMath::MatrixMul(mtxProj, cVector3f(0, 0, -mpCurrentWorld->GetDepthOfFieldFocusEnd())).z;
			float fStartZ = cMath::MatrixMul(mtxProj, cVector3f(0, 0, -mpCurrentWorld->GetDepthOfFieldFocusStart())).z;
			fDofParams[0] = fEndZ;
			fDofParams[1] = 1.0f / ((fStartZ - fEndZ) * 0.5f / cMath::Max(mpCurrentWorld->GetDepthOfFieldFalloff(), 1e-15f));
		}

		SetDepthTest(true);
		SetDepthWrite(false);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetMatrix(NULL);
		SetBlendMode(eMaterialBlendMode_Alpha);
		SetCullActive(false);
		SetNormalFrustumProjection();

		for(int i=0; i<mpCurrentWorld->GetEnvironmentParticleNum(); ++i)
		{
			cEnvironmentParticles *pEnv = mpCurrentWorld->GetEnvironmentParticles(i);
			if(pEnv->IsVisible(mpCurrentFrustum)==false) continue;
			iGpuProgram *pProg = pEnv->GetProgram(bDof);
			if(pProg==NULL) continue;
			SetProgram(pProg);
			pEnv->SetupProgramBase(pProg, mpCurrentFrustum, fDofParams);
			SetTexture(0, pEnv->mpTexture);
			SetVertexBuffer(pEnv->mpVtxBuffer);
			for(int j=0; j<pEnv->GetIterationNumInt(); ++j)
			{
				pEnv->SetupProgramIteration(pProg, j);
				DrawCurrent();
			}
		}

		SetCullActive(true);
		SetTexture(0, NULL);
		SetProgram(NULL);
		SetVertexBuffer(NULL);
	}

	void cRendererDeferred::UpdateLensFlares()
	{
		if(mpCurrentSettings->mbIsReflection) return;
		for(cLensFlare *pFlare : mpCurrentRenderList->GetLensFlares())
		{
			pFlare->RetrieveOcculsionQuery(this);
			pFlare->UpdateVisibility(mfCurrentFrameTime, GetRenderFrameCount());
		}
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderMultiIrisAndGlare()
	{
		if(mpLensFlareIrisProgram==NULL || mpLensFlareGlareProgram==NULL || mpCurrentRenderList->GetLensFlares().empty()) return;

		SetDepthTest(false);
		SetDepthWrite(false);
		SetCullActive(false);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetMatrix(NULL);

		cVector3f vCamPos = mpCurrentFrustum->GetOrigin();
		for(cLensFlare *pFlare : mpCurrentRenderList->GetMultiIrisFlares())
		{
			cMaterial *pMat = pFlare->GetTypeMaterial(eLensFlareType_MultiIris);
			iTexture *pTex = pMat->GetTexture(eMaterialTexture_Diffuse);
			if(pTex==NULL) continue;

			float fVis = pFlare->GetVisibility();
			if(WorldFogActive())
			{
				// ponytail: material type fog-end scale taken as 1
				float fDist = cMath::Vector3Dist(vCamPos, pFlare->GetWorldPosition());
				float fStart = mpCurrentWorld->GetFogStart(), fEnd = mpCurrentWorld->GetFogEnd();
				float fP = powf(cMath::Clamp((fDist - fStart)/(fEnd - fStart), 0, 1), mpCurrentWorld->GetFogFalloffExp());
				fVis *= (1 - fP) + (1 - mpCurrentWorld->GetFogColor().a)*fP;
			}
			if(fVis <= 0) continue;

			SetBlendMode(pMat->GetBlendMode());
			SetTexture(0, pTex);
			SetProgram(mpLensFlareIrisProgram);
			SetVertexBuffer(pFlare->PrepareMultiIrisVertexBuffer(mpCurrentFrustum, fVis));
			DrawCurrent();
		}

		float *pCol = mpGlareVtxBuffer->GetFloatArray(eVertexBufferElement_Color0);
		const float *pPos = mpGlareVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
		std::fill(pCol, pCol + 400*4, 0.0f);
		float fAspect = mvScreenSizeFloat.x / mvScreenSizeFloat.y;
		float fSum = 0;
		for(cLensFlare *pFlare : mpCurrentRenderList->GetLensFlares())
		{
			float fVis = pFlare->GetVisibility();
			if(fVis <= 0 || pFlare->IsAnyTypeActive()==false) continue;
			cVector3f vGlarePos;
			cColor cGlare = pFlare->GetGlare(mpCurrentFrustum, vGlarePos, fVis);
			if(cGlare.r + cGlare.g + cGlare.b <= 0) continue;
			fSum += cGlare.r + cGlare.g + cGlare.b;
			for(int i=0; i<400; ++i)
			{
				cVector3f vD((pPos[i*4] - vGlarePos.x)*fAspect, (1 - pPos[i*4+1]) - vGlarePos.y, pPos[i*4+2]);
				float fD2 = cMath::Vector3Dot(vD, vD);
				float fW = 1.4142135f / (1 + 128*fD2*fD2);
				for(int c=0; c<4; ++c) pCol[i*4+c] += cGlare.v[c]*fW;
			}
		}
		if(fSum > 1/255.0f)
		{
			mpGlareVtxBuffer->UpdateData(eVertexElementFlag_Color0, false);
			SetBlendMode(eMaterialBlendMode_Add);
			SetTexture(0, NULL);
			SetProgram(mpLensFlareGlareProgram);
			SetVertexBuffer(mpGlareVtxBuffer);
			DrawCurrent();
		}

		SetDepthTest(true);
		SetCullActive(true);
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderTranslucent(int alDofPass)
	{
		if(mpCurrentRenderList->ArrayHasObjects(eRenderListType_Translucent)==false) return;

		START_RENDER_PASS(Translucent);

		///////////////////////////////
		//Set up rendering
		SetDepthTest(true);
		SetDepthWrite(false);

		SetAlphaLimit(0.01f);
		SetAlphaMode(eMaterialAlphaMode_Trans);
		if(mbDepthInNormalAlpha) GetGbufferTexture(1)->SetRedFromAlpha(true);

		///////////////////////////////
		//Set up variables
		float fHalfFovTan=0;
		cMaterialType_Translucent::SetWorldFog(WorldUnderwaterFog(this), WorldFogActive() && mpCurrentWorld->GetSecondaryFogActive());
		tFlag lWorldFog = cMaterialType_Translucent::GetWorldFog();

		// each mid-pass copy flushes the whole tiled framebuffer, so one copy is reused until a draw overlaps;
		// refractive particles ignore each other, like particles within one emitter
		bool bRefractionValid = false;
		std::vector<cRect2l> vDrawnSinceCopy;

		///////////////////////////////
		//Iterate transparent objects
		cRenderableVecIterator transIt = mpCurrentRenderList->GetArrayIterator(eRenderListType_Translucent);
		int lTransIdx = -1;
		while(transIt.HasNext())
		{
			iRenderable *pObject = transIt.Next();
			cMaterial *pMaterial = pObject->GetMaterial();
			if(++lTransIdx == mlDebugSkipTranslucent) continue;
			if(pMaterial->mlCompiledWorldFog != lWorldFog) pMaterial->Compile();

			// objects straddling the focus end draw in both passes, crossfaded per pixel
			bool bDofBlend = false;
			cColor vDofParams(0,0);
			if(alDofPass && pMaterial->GetAffectedByDepthOfField()==false)
			{
				if(alDofPass==1) continue;
			}
			else if(alDofPass)
			{
				cBoundingVolume *pBV = pObject->GetBoundingVolume();
				float fDepth = -cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), pBV->GetWorldCenter()).z;
				float fStart = mpCurrentWorld->GetDepthOfFieldFocusStart();
				float fEnd = mpCurrentWorld->GetDepthOfFieldFocusEnd();
				bool bBehind = fDepth - pBV->GetRadius() > fEnd;
				bDofBlend = bBehind==false && fDepth + pBV->GetRadius() > fEnd && pMaterial->HasRefraction()==false;
				if(bDofBlend==false && bBehind != (alDofPass==1)) continue;
				float fFalloffLen = (fEnd - fStart) * 0.5f / cMath::Max(1e-15f, mpCurrentWorld->GetDepthOfFieldFalloff());
				vDofParams = cColor(fEnd, -1.0f / cMath::Max(1e-6f, fFalloffLen), alDofPass==1 ? 0.0f : 1.0f, alDofPass==1 ? 1.0f : -1.0f);
			}

			eMaterialRenderMode renderMode = WorldFogActive() ? eMaterialRenderMode_DiffuseFog : eMaterialRenderMode_Diffuse;
			if(pMaterial->GetAffectedByFog()==false) renderMode = eMaterialRenderMode_Diffuse;

			//No world reflections in a reflection!
			if(mpCurrentSettings->mbIsReflection && pMaterial->HasWorldReflection()) continue;

			////////////////////////////////////////
			// Check the fog area alpha
			mfTempAlpha = 1;
			mTempFogAreaColor = cColor(0,0);
			if(pMaterial->GetAffectedByFog())
			{
				for(size_t i=0; i<mpCurrentSettings->mvFogRenderData.size(); ++i)
				{
					float fVisibility = GetFogAreaVisibilityForObject(&mpCurrentSettings->mvFogRenderData[i], pObject);
					if(mbHdr==false) { mfTempAlpha *= fVisibility; continue; }

					// HPL3 fogs translucents toward the area colour instead of fading them
					cFogArea *pFogArea = mpCurrentSettings->mvFogRenderData[i].mpFogArea;
					cColor fogCol = GetFogRenderColor(pFogArea->GetColor(), pFogArea->GetBrightness());
					float fA = 1 - fVisibility;
					float fPrevA = mTempFogAreaColor.a * (1 - fA);
					float fNewA = fPrevA + fA;
					if(fNewA <= 0) continue;
					mTempFogAreaColor = (mTempFogAreaColor * fPrevA + fogCol * fA) * (1 / fNewA);
					mTempFogAreaColor.a = fNewA;
				}
				if(mTempFogAreaColor.a > 0) renderMode = eMaterialRenderMode_DiffuseFog;
			}
			
			
			////////////////////////////////////////
			// Update object, need to do this here since otherwise the reflection rendering might reset it!

			//Before viewport
			if(pObject->UpdateGraphicsForViewport(mpCurrentFrustum, mfCurrentFrameTime)==false)
			{
				continue;
			}
			
			if(pObject->RetrieveOcculsionQuery(this)==false)
			{
				continue;
			}
			
			cMatrixf *pMatrix = pObject->GetModelMatrix(mpCurrentFrustum);

			////////////////////////////////////////
			// World reflection
			if(pMaterial->HasWorldReflection() && pObject->GetRenderType() == eRenderableType_SubMesh)
			{
				if(CheckRenderablePlaneIsVisible(pObject, mpCurrentFrustum)==false) continue;

				///////////////////////////////////
				//Retrieve all occlusion queries before rendering new scene. 
				//  Otherwise it will lead to problems on some cards.
				WaitAndRetrieveAllOcclusionQueries();	//Queires for halos and such
				if(mbOcclusionTestLargeLights)
					RetrieveAllLightOcclusionPair(false);	//Queries for light visibility (false = no stop and wait!)

				///////////////////////////////////
				//Render the reflection
				cSubMeshEntity *pReflectSubMeshEnt = static_cast<cSubMeshEntity*>(pObject);
				if(mbDepthInNormalAlpha) GetGbufferTexture(1)->SetRedFromAlpha(false);
				RenderReflection(pReflectSubMeshEnt);
				if(mbDepthInNormalAlpha) GetGbufferTexture(1)->SetRedFromAlpha(true);
				bRefractionValid = false;
			}

			////////////////////////////////////////
			// Refraction set up
			if(pMaterial->HasRefraction())
			{
				if(CheckRenderablePlaneIsVisible(pObject, mpCurrentFrustum)==false) continue;
				
				////////////////////////////////////
				// Get the clip rect needed by the refraction
				cBoundingVolume *pBV = pObject->GetBoundingVolume();

				if(fHalfFovTan ==0)	
					fHalfFovTan = tan(mpCurrentFrustum->GetFOV()*0.5f);
				cRect2l clipRect = GetClipRectFromObject(pObject, 0.2f, mpCurrentFrustum, mvRenderTargetSize, fHalfFovTan);
				
				////////////////////////////////////
				// Add an extra check to make sure there is no bleeding. Draw outline of mesh to alpha!
				if(pMaterial->UseRefractionEdgeCheck())
				{
					////////////////////////////////////
					// Clear alpha
					SetFlatProjection(cVector2f((float)mvRenderTargetSize.x, (float)mvRenderTargetSize.y));					
					
					//Set up new settings
					SetDepthTest(false);
					SetProgram(NULL);
					SetBlendMode(eMaterialBlendMode_None);
					SetAlphaMode(eMaterialAlphaMode_Solid);
					SetChannelMode(eMaterialChannelMode_A);
					SetTextureRange(NULL,0);
					
					DrawQuad(	cVector2f((float)clipRect.x, (float)clipRect.y), 
								cVector2f((float)clipRect.w, (float)clipRect.h), 0, 1, false, cColor(1,0));
                    
					//Set Normal projection and depth test
					SetNormalFrustumProjection();
					SetDepthTest(true);

					////////////////////////////////////
					// Render alpha of object
					SetMatrix(pMatrix);
					SetVertexBuffer(pObject->GetVertexBuffer());

					DrawCurrent();
										
					//Set backs settings to normal.
					SetChannelMode(eMaterialChannelMode_RGBA);
					SetAlphaMode(eMaterialAlphaMode_Trans);
				}
				
				////////////////////////////////////
				// Copy frame buffer to texture (an empty rect would mean the whole screen)
				if(pMaterial->UseRefractionEdgeCheck())
				{
					if(clipRect.w > 0 && clipRect.h > 0)
					CopyFrameBufferToTexure(mpRefractionTexture, 
											cVector2l(clipRect.x, clipRect.y), 
											cVector2l(clipRect.w, clipRect.h), 
											cVector2l(clipRect.x, clipRect.y),
											true);
					bRefractionValid = false;
				}
				else if(bRefractionValid==false || std::any_of(vDrawnSinceCopy.begin(), vDrawnSinceCopy.end(),
							[&](const cRect2l& r){ return cMath::CheckRectIntersection(r, clipRect); }))
				{
					CopyFrameBufferToTexure(mpRefractionTexture, 0, mvRenderTargetSize, 0, true);
					bRefractionValid = true;
					vDrawnSinceCopy.clear();
				}
				
			}
			
			////////////////////////////////////////
			// Set up and render
			if(pMaterial->HasRefraction())	SetBlendMode(eMaterialBlendMode_None); //Blending shall take place in shader!
			else							SetBlendMode(pMaterial->GetBlendMode());
			SetDepthTest(pMaterial->GetDepthTest());
			
			cMaterialType_Translucent::SetDepthOfField(bDofBlend, vDofParams);
			SetMaterialProgram(renderMode,pMaterial);
			SetMaterialTextures(renderMode, pMaterial);
			
			SetMatrix(pMatrix);

			SetVertexBuffer(pObject->GetVertexBuffer());

			DrawCurrentMaterial(renderMode, pObject);
			cMaterialType_Translucent::SetDepthOfField(false);


			////////////////////////////////////////
			// Set up and render Illumination
			if(pMaterial->HasTranslucentIllumination() && (bDofBlend==false || alDofPass==2))
			{
				renderMode = renderMode == eMaterialRenderMode_Diffuse ? eMaterialRenderMode_Illumination : eMaterialRenderMode_IlluminationFog;

				SetBlendMode(eMaterialBlendMode_Add);
				SetDepthTest(pMaterial->GetDepthTest());
				
				SetMaterialProgram(renderMode,pMaterial);
				SetMaterialTextures(renderMode, pMaterial);
				
				SetMatrix(pMatrix);

				SetVertexBuffer(pObject->GetVertexBuffer());

				DrawCurrentMaterial(renderMode, pObject);
			}

			bool bRefractiveParticle = pMaterial->HasRefraction() && pObject->GetRenderType()==eRenderableType_ParticleEmitter;
			if(bRefractionValid && bRefractiveParticle==false)
			{
				cRect2l drawnRect;
				if(fHalfFovTan ==0) fHalfFovTan = tan(mpCurrentFrustum->GetFOV()*0.5f);
				cMath::GetClipRectFromBV(drawnRect, *pObject->GetBoundingVolume(), mpCurrentFrustum, mvRenderTargetSize, fHalfFovTan);
				vDrawnSinceCopy.push_back(drawnRect);
			}
		}

		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetAlphaLimit(mfDefaultAlphaLimit);
		if(mbDepthInNormalAlpha) GetGbufferTexture(1)->SetRedFromAlpha(false);

		

		END_RENDER_PASS();
	}

    //-----------------------------------------------------------------------

	void cRendererDeferred::RenderReflection(iRenderable *apObject)
	{
		////////////////////////////////////
		//Set up variables
		cSubMeshEntity *pReflectionObject = static_cast<cSubMeshEntity*>(apObject);
		cMaterial *pRelfMaterial = pReflectionObject->GetMaterial();


		///////////////////////////
		//Check if surface is close enough for reflection!
		bool bReflectionIsInRange=true;
		if(pRelfMaterial->GetMaxReflectionDistance() > 0)
		{
			cVector3f vPoint = mpCurrentFrustum->GetOrigin() + mpCurrentFrustum->GetForward()*-1*pRelfMaterial->GetMaxReflectionDistance();
			cVector3f vNormal = mpCurrentFrustum->GetForward();

			cPlanef maxRelfctionDistPlane;
			maxRelfctionDistPlane.FromNormalPoint(vNormal, vPoint);

			if(cMath::CheckPlaneBVCollision(maxRelfctionDistPlane, *pReflectionObject->GetBoundingVolume())==eCollision_Outside)
			{
				bReflectionIsInRange = false;
			}
		}

		//////////////////////////////////////////////
		// Render the reflection!
		if(mpCurrentSettings->mbRenderWorldReflection && bReflectionIsInRange && pReflectionObject->GetIsOneSided())
		{
			RenderSubMeshEntityReflection(pReflectionObject);
		}
		//////////////////////////////////////////////
		// No reflection, just clear!
		else
		{	
			if(mbReflectionTextureCleared == false)
			{
				if(mpCurrentSettings->mbLog) Log("- Clear reflection Begin!\n");

				cRenderTarget renderTarget;
				renderTarget.mpFrameBuffer = mpReflectionBuffer;

				SetFrameBuffer(mpReflectionBuffer, false, false);

				ClearFrameBuffer(eClearFrameBufferFlag_Color, false);

				SetAccumulationBuffer();

				mbReflectionTextureCleared = true;

				if(mpCurrentSettings->mbLog) Log("- Clear reflection End!\n");
			}
		}
	}

	//-----------------------------------------------------------------------
	
	void cRendererDeferred::RenderSubMeshEntityReflection(cSubMeshEntity *pReflectionObject)
	{
		cMaterial *pRelfMaterial = pReflectionObject->GetMaterial();

		if(mbLog) Log("------------- Setting up Reflection rendering -----------\n");

		////////////////////////
		//Reset settings from normal rendering
		cFrustum *pSaved_Frustum = mpCurrentFrustum;
		cRenderSettings *pSaved_Settings = mpCurrentSettings;
		cRenderTarget *pSaved_RenderTarget = mpCurrentRenderTarget;
		bool bSaved_SendFrameBufferToPostEffects = mbSendFrameBufferToPostEffects;

		EndRendering(false);

		SetAlphaLimit(mfDefaultAlphaLimit);//Need to have the normal alpha limit!

		////////////////////////
		//Setup settings
		mpCurrentSettings->SetupReflectionSettings();

		mpCurrentSettings->mpReflectionSettings->mbUseOcclusionCulling = pRelfMaterial->GetWorldReflectionOcclusionTest();

		///////////////////////////////////
		//Reflection texture is not cleared!
		mbReflectionTextureCleared = false;

		///////////////////////////////////
		//Make render target
		cRenderTarget renderTarget;
		cVector2l vCurrentFrameBufferSize = mpCurrentRenderTarget->mpFrameBuffer ? mpCurrentRenderTarget->mpFrameBuffer->GetSize() : mvScreenSize;
		renderTarget.mpFrameBuffer = mpReflectionBuffer;
		renderTarget.mvPos = mpCurrentRenderTarget->mvPos / mlReflectionSizeDiv;
		renderTarget.mvSize.x = mpCurrentRenderTarget->mvSize.x == -1 ? vCurrentFrameBufferSize.x/mlReflectionSizeDiv : mpCurrentRenderTarget->mvSize.x/mlReflectionSizeDiv;
		renderTarget.mvSize.y = mpCurrentRenderTarget->mvSize.y == -1 ? vCurrentFrameBufferSize.y/mlReflectionSizeDiv : mpCurrentRenderTarget->mvSize.y/mlReflectionSizeDiv;

		///////////////////////////
		//Make a frustum, mirrored along the plane
		cSubMesh *pSubMesh = pReflectionObject->GetSubMesh();
		cVector3f vSurfaceNormal = cMath::Vector3Normalize(cMath::MatrixMul3x3(pReflectionObject->GetWorldMatrix(), pSubMesh->GetOneSidedNormal()));
		cVector3f vSurfacePos = cMath::MatrixMul(pReflectionObject->GetWorldMatrix(), pSubMesh->GetOneSidedPoint());

		cPlanef reflectPlane;
		reflectPlane.FromNormalPoint(vSurfaceNormal, vSurfacePos);

		cMatrixf mtxReflection = cMath::MatrixPlaneMirror(reflectPlane);
		cMatrixf mtxReflView = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), mtxReflection);
		cVector3f vReflOrigin = cMath::MatrixMul(mtxReflection, mpCurrentFrustum->GetOrigin());


		cMatrixf mtxProj = mpCurrentFrustum->GetProjectionMatrix();

		cPlanef cameraSpaceReflPlane = cMath::TransformPlane(mtxReflView, reflectPlane);
		cMatrixf mtxReflProj = cMath::ProjectionMatrixObliqueNearClipPlane(mtxProj, cameraSpaceReflPlane);

		cFrustum reflectFrustum;
		reflectFrustum.SetupPerspectiveProj(mtxReflProj, mtxReflView, 
			mpCurrentFrustum->GetFarPlane(),mpCurrentFrustum->GetNearPlane(), 
			mpCurrentFrustum->GetFOV(), mpCurrentFrustum->GetAspect(),
			vReflOrigin,false, &mtxProj, true);
		reflectFrustum.SetInvertsCullMode(true);

		///////////////////////////
		//Reset clip planes
		mpCurrentSettings->mpReflectionSettings->ResetOcclusionPlanes();

		///////////////////////////
		//End of reflection clip plane
		if(pRelfMaterial->GetMaxReflectionDistance() > 0)
		{
			cVector3f vForward = mpCurrentFrustum->GetForward()*-1;
			float fMaxReflDist = pRelfMaterial->GetMaxReflectionDistance();
			cPlanef maxRelfctionDistPlane;

			///////////////////////////////
			//Forward and normal is aligned, the normal of plane becomes inverse forward
			float fFDotN = cMath::Vector3Dot(vForward, vSurfaceNormal);
			if(fFDotN <-0.99999f)
			{
				cVector3f vClipNormal, vClipPoint;
				vClipNormal = vForward*-1;
				vClipPoint = mpCurrentFrustum->GetOrigin() + vForward*pRelfMaterial->GetMaxReflectionDistance();

				maxRelfctionDistPlane.FromNormalPoint(vClipNormal, vClipPoint);
			}
			///////////////////////////////
			//Get the plane into camera space and then get a point where z=max reflection distance. 
			//Note: Because of test above a and b in plane cannot be 0!
			else
			{
				cPlanef cameraSpacePlane = cMath::TransformPlane(mpCurrentFrustum->GetViewMatrix(), reflectPlane);

				cVector3f vPoint1 = cVector3f(0,0, -fMaxReflDist);
				cVector3f vPoint2 = cVector3f(0,0, -fMaxReflDist);

				//Vertical row (x always same)
				if(fabs(cameraSpacePlane.b) < 0.0001f)
				{
					vPoint1.x = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.a;
					vPoint2 = vPoint1;
					vPoint2.y+=1;
				}
				//Horizontal row (y always same)
				else if(fabs(cameraSpacePlane.a) < 0.0001f)
				{
					vPoint1.y = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.b;
					vPoint2 = vPoint1;
					vPoint2.x+=1;
				}
				//Oblique row (x and y changes)
				else
				{
					vPoint1.x = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.a;
					vPoint2.y = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.b;
				}

				cMatrixf mtxInvCamera = cMath::MatrixInverse(mpCurrentFrustum->GetViewMatrix());
				vPoint1 = cMath::MatrixMul(mtxInvCamera, vPoint1);
				vPoint2 = cMath::MatrixMul(mtxInvCamera, vPoint2);

				cVector3f vNormal = cMath::Vector3Cross(vPoint1-vReflOrigin, vPoint2-vReflOrigin);
				vNormal.Normalize();
				//make sure normal has correct sign!
				if(cMath::Vector3Dot(vSurfaceNormal, vNormal)<0) vNormal = vNormal*-1;

				maxRelfctionDistPlane.FromNormalPoint(vNormal, vPoint1);
			}


			mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(maxRelfctionDistPlane);
		}

		//////////////////////////
		// Add screen rect Occlusion planes!
		if(mpCurrentSettings->mbClipReflectionScreenRect)
		{
			cVector3f vUp = reflectFrustum.GetViewMatrix().GetUp();
			cVector3f vRight = reflectFrustum.GetViewMatrix().GetRight();
			cVector3f vForward = reflectFrustum.GetViewMatrix().GetForward();
			cVector3f vOrigin = reflectFrustum.GetOrigin();

			float fNearPlane = reflectFrustum.GetNearPlane();
			float fHalfFovTan = tan(reflectFrustum.GetFOV()*0.5f);
			float fNearTop =  fHalfFovTan * fNearPlane;
			float fNearRight = reflectFrustum.GetAspect() * fNearTop;

			cVector3f vMin,vMax;
			bool bNeedsClipRect = false;
			bool bVisible = cMath::GetNormalizedClipRectFromBV(vMin, vMax, *pReflectionObject->GetBoundingVolume(), &reflectFrustum, fHalfFovTan);
			if(bVisible)
			{
				if(mbLog) Log("  Normalized Clip limits: (%s) -> (%s)\n", vMin.ToString().c_str(), vMax.ToString().c_str());
				////////////////////////////
				// Right
				if(vMax.x <1)
				{
					cVector3f vNearPlanePos = vOrigin + vRight*(vMax.x*fNearRight) + vForward*-fNearPlane;
					cPlanef rightPlane;
					rightPlane.FromPoints(vOrigin, vNearPlanePos, vNearPlanePos+vUp);
					mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(rightPlane);
					bNeedsClipRect =true;
				}

				////////////////////////////
				// Left
				if(vMin.x >-1)
				{
					cVector3f vNearPlanePos = vOrigin + vRight*(vMin.x*fNearRight) + vForward*-fNearPlane;
					cPlanef leftPlane;
					leftPlane.FromPoints(vOrigin, vNearPlanePos+vUp, vNearPlanePos);
					mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(leftPlane);
					bNeedsClipRect =true;
				}

				////////////////////////////
				// Top
				if(vMax.y <1)
				{
					cVector3f vNearPlanePos = vOrigin + vUp*(vMax.y*fNearTop) + vForward*-fNearPlane;
					cPlanef topPlane;
					topPlane.FromPoints(vOrigin, vNearPlanePos+vRight, vNearPlanePos);
					mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(topPlane);
					bNeedsClipRect = true;
				}

				////////////////////////////
				// Bottom
				if(vMin.y >-1)
				{
					cVector3f vNearPlanePos = vOrigin + vUp*(vMin.y*fNearTop) + vForward*-fNearPlane;
					cPlanef bottomPlane;
					bottomPlane.FromPoints(vOrigin, vNearPlanePos, vNearPlanePos+vRight);
					mpCurrentSettings->mpReflectionSettings->AddOcclusionPlane(bottomPlane);
					bNeedsClipRect =true;
				}

				////////////////////////////
				// Add a stencil rect!
				if(bNeedsClipRect)
				{
					cVector2l vFrameBufferSize = renderTarget.mpFrameBuffer->GetSize();
					cVector2l vRenderTargetSize;
					vRenderTargetSize.x = renderTarget.mvSize.x < 0 ? vFrameBufferSize.x : renderTarget.mvSize.x;
					vRenderTargetSize.y = renderTarget.mvSize.y < 0 ? vFrameBufferSize.y : renderTarget.mvSize.y;

					cRect2l clipRect;
					cMath::GetClipRectFromBV(clipRect, *pReflectionObject->GetBoundingVolume(), mpCurrentFrustum, vRenderTargetSize, fHalfFovTan);

					if(mbLog) Log("  Setting up scissor rect. pos: (%d, %d)  %d x %d\n", clipRect.x, clipRect.y,clipRect.w, clipRect.h);

					mpCurrentSettings->mpReflectionSettings->mbUseScissorRect = true;
					mpCurrentSettings->mpReflectionSettings->mvScissorRectPos = cVector2l(clipRect.x, clipRect.y);
					mpCurrentSettings->mpReflectionSettings->mvScissorRectSize = cVector2l(clipRect.w, clipRect.h);
				}
			}
		}

		///////////////////////////
		//Render
		if(mpCurrentSettings->mbLog)
			Log("\n==============================\n= BEGIN RENDER REFLECTION\n==============================\n\n");

		Render(mfCurrentFrameTime, &reflectFrustum, mpCurrentWorld, mpCurrentSettings->mpReflectionSettings, &renderTarget, false, mpCallbackList);				

		if(mpCurrentSettings->mbLog)
			Log("\n==============================\n= END RENDER REFLECTION\n==============================\n\n");


		///////////////////////////
		//Set back to order!
		BeginRendering(	mfCurrentFrameTime, pSaved_Frustum, mpCurrentWorld, pSaved_Settings, pSaved_RenderTarget, 
						bSaved_SendFrameBufferToPostEffects,mpCallbackList, false);


		SetAccumulationBuffer();

		//Set trans stuff
		SetDepthTest(true);
		SetDepthWrite(false);

		SetAlphaLimit(0.01f);
		SetAlphaMode(eMaterialAlphaMode_Trans);
	}
	
	//-----------------------------------------------------------------------

	void cRendererDeferred::SetAccumulationBuffer()
	{
		if(mpCurrentSettings->mbIsReflection)
			SetFrameBuffer(mpReflectionBuffer, true);
		else
			SetFrameBuffer(mpAccumBuffer, true);
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::SetGBuffer(eGBufferComponents aComponents)
	{
		int lType = mpCurrentSettings && mpCurrentSettings->mbIsReflection ? 1 : 0;
		SetFrameBuffer(mpGBuffer[lType][aComponents], true);
	}

	//-----------------------------------------------------------------------

	iFrameBuffer* cRendererDeferred::GetGBufferFrameBuffer(eGBufferComponents aComponents)
	{
		int lType = mpCurrentSettings && mpCurrentSettings->mbIsReflection ? 1 : 0;
		
		return mpGBuffer[lType][aComponents];
	}

	//-----------------------------------------------------------------------

	iTexture* cRendererDeferred::GetBufferTexture(int alIdx)
	{
		int lType = mpCurrentSettings && mpCurrentSettings->mbIsReflection ? 1 : 0;
		return mpGBufferTexture[lType][alIdx];
	}

	//-----------------------------------------------------------------------


	void cRendererDeferred::RenderDeferredSkyBox()
	{
		if(mpCurrentWorld==NULL || mpCurrentWorld->GetSkyBoxActive()==false) return;
		START_RENDER_PASS(DeferredSkyBox);

		SetGBuffer(eGBufferComponents_ColorAndDepth);
		

		//Debug, leave in for Luis or someone to test.
		//int lTargets1[] = {0,3};
		//mpLowLevelGraphics->SetFrameBufferDrawTargets(lTargets1,2);

		SetDepthTest(true);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		/////////////////////////
		//Calculate and set matrix
		float fFarClip = mpCurrentFrustum->GetFarPlane();
		float fSide = sqrt((fFarClip*fFarClip) / 3) *0.95f;

		m_mtxSkyBox = cMath::MatrixRotate(mpCurrentWorld->GetSkyBoxRotation(), eEulerRotationOrder_XYZ);
		for(int i=0; i<3; ++i) for(int j=0; j<3; ++j) m_mtxSkyBox.m[i][j] *= fSide;

		m_mtxSkyBox.SetTranslation(mpCurrentFrustum->GetOrigin());

		SetMatrix(&m_mtxSkyBox);

		/////////////////////////
		//Program
		SetProgram(NULL);//mpSkyBoxProgram);

		/////////////////////////
		//Texture and vertex buffer
		SetTexture(0,mpCurrentWorld->GetSkyBoxTexture());
		SetTextureRange(NULL,1);

		SetVertexBuffer(mpCurrentWorld->GetSkyBoxVertexBuffer());

		DrawCurrent();

		//Debug, leave in for Luis or someone to test.
		//int lTargets2[] = {0,1,2,3};
		//mpLowLevelGraphics->SetFrameBufferDrawTargets(lTargets2,4);

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------


	void cRendererDeferred::RenderGbufferContent()
	{
		START_RENDER_PASS(GBufferContent);

		//Pure testing below
		SetDepthTest(false);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		SetAccumulationBuffer();

		ClearFrameBuffer(eClearFrameBufferFlag_Depth | eClearFrameBufferFlag_Color, false);

		SetFlatProjection();
		SetProgram(NULL);

		SetTextureRange(NULL,1);

		SetTexture(0,GetBufferTexture(0));
		DrawQuad(cVector2f(0,0),cVector2f(0.5f,0.5f), 0,mvScreenSizeFloat, true);
		SetTexture(0,GetBufferTexture(1));
		DrawQuad(cVector2f(0.5f,0),cVector2f(0.5f,0.5f), 0,mvScreenSizeFloat, true);
		SetTexture(0,GetBufferTexture(2));
		DrawQuad(cVector2f(0,0.5f),cVector2f(0.5f,0.5f), 0,mvScreenSizeFloat, true);
		if(mlNumOfGBufferTextures > 3)
		{
			SetTexture(0,GetBufferTexture(3));
			DrawQuad(cVector2f(0.5f,0.5f),cVector2f(0.5f,0.5f), 0,mvScreenSizeFloat, true);
		}
		

		SetNormalFrustumProjection();
		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void cRendererDeferred::RenderReflectionContent()
	{
		START_RENDER_PASS(ReflectionContent);

		//Pure testing below
		SetDepthTest(false);
		SetDepthWrite(false);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);

		SetAccumulationBuffer();

		SetFlatProjection();
		SetProgram(NULL);

		SetTextureRange(NULL,1);

		SetTexture(0,mpReflectionTexture);
		DrawQuad(cVector2f(0,0),cVector2f(1,1), 0, mvScreenSizeFloat / (float)mlReflectionSizeDiv, true);
		

		SetNormalFrustumProjection();
		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

}
