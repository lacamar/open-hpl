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

#include "graphics/Renderer.h"

#include "math/Math.h"
#include "math/BoundingVolume.h"

#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "system/String.h"

#include "graphics/Graphics.h"
#include "graphics/RustRenderH8.h"
#include "impl/RustBackends.h"
#include "graphics/Texture.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/MeshCreator.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Renderable.h"
#include "graphics/RenderList.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/ProgramComboManager.h"
#include "graphics/GPUProgram.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/OcclusionQuery.h"

#include "resources/Resources.h"
#include "resources/LowLevelResources.h"
#include "resources/TextureManager.h"
#include "resources/GpuShaderManager.h"
#include "resources/MeshManager.h"

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/RenderableContainer.h"
#include "scene/LightSpot.h"
#include "scene/LightPoint.h"
#include "scene/LightBox.h"
#include "scene/SubMeshEntity.h"
#include "scene/FogArea.h"

#include <algorithm>
#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// STATIC VARAIBLES
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	eShadowMapQuality iRenderer::mShadowMapQuality	=		eShadowMapQuality_Medium;
	eShadowMapResolution iRenderer::mShadowMapResolution	=	eShadowMapResolution_High;
	eParallaxQuality iRenderer::mParallaxQuality = eParallaxQuality_Low;
	bool iRenderer::mbParallaxEnabled=true;
	int iRenderer::mlReflectionSizeDiv = 2;
	bool iRenderer::mbRefractionEnabled=true;
	bool iRenderer::mbShadowDepthClamp=false;
	bool iRenderer::mbShadowCull=true;
	bool iRenderer::mbSunCasterCull=true;
	bool iRenderer::mbShadowMapCompare=true;

	//-----------------------------------------------------------------------

	int iRenderer::mlRenderFrameCount =0;

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// RUST HOST
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	struct cRendererRustHost
	{
		struct Ctx
		{
			iRenderer *mpRenderer;
			tRenderCHCObjectCallbackFunc mpCallback;
			tRenderableVec *mpCasters;
		};

		static iRenderableContainerNode* N(uintptr_t alNode){ return (iRenderableContainerNode*)alNode;}
		static iRenderable* O(uintptr_t alObj){ return (iRenderable*)alObj;}
		static cFrustum* F(uintptr_t alFrustum){ return (cFrustum*)alFrustum;}
		static iOcclusionQuery* Q(uintptr_t alQuery){ return (iOcclusionQuery*)alQuery;}
		static Ctx* C(void *apCtx){ return (Ctx*)apCtx;}

		static void UpdateNode(void*, uintptr_t alNode){ N(alNode)->UpdateBeforeUse();}
		static int32_t CollideNode(void*, uintptr_t alFrustum, uintptr_t alNode){ return F(alFrustum)->CollideNode(N(alNode));}
		static int32_t CollideObject(void*, uintptr_t alFrustum, uintptr_t alObj){ return F(alFrustum)->CollideBoundingVolume(O(alObj)->GetBoundingVolume());}
		static bool ObjectCollidesWithFrustum(void*, uintptr_t alFrustum, uintptr_t alObj){ return O(alObj)->CollidesWithFrustum(F(alFrustum));}
		static bool NearPlaneAABB(void*, uintptr_t alFrustum, const cVector3f *apMin, const cVector3f *apMax){ return F(alFrustum)->CheckAABBNearPlaneIntersection(*apMin, *apMax);}

		static void ObjectCheck(void*, uintptr_t alObj, OhplRnObjCheck *apOut)
		{
			iRenderable *pObject = O(alObj);
			apOut->mbVisible = pObject->IsVisible();
			apOut->mlRenderFlags = pObject->GetRenderFlags();
			apOut->mbCulledByFog = pObject->IsCulledByFog();
			apOut->mbCulledByDistance = pObject->IsCulledByDistance();
			apOut->mfFade = pObject->DistanceCullFade();
			cBoundingVolume *pBV = pObject->GetBoundingVolume();
			apOut->mbHasBV = pBV != NULL;
			if(pBV)
			{
				apOut->mvMin = pBV->GetMin();
				apOut->mvMax = pBV->GetMax();
			}
		}

		static void SetObjectFade(void*, uintptr_t alObj, float afFade){ O(alObj)->DistanceCullFade() = afFade;}

		static void ObjectMaterial(void*, uintptr_t alObj, OhplRnMatView *apOut)
		{
			cMaterial *pMaterial = O(alObj)->GetMaterial();
			apOut->mbPresent = pMaterial != NULL;
			if(pMaterial==NULL) return;
			apOut->mbTranslucent = pMaterial->GetType()->IsTranslucent();
			apOut->mlAlphaMode = (int32_t)pMaterial->GetAlphaMode();
			apOut->mpZProgram = (uintptr_t)pMaterial->GetProgram(0,eMaterialRenderMode_Z);
			apOut->mpDiffuseTexture = (uintptr_t)pMaterial->GetTexture(eMaterialTexture_Diffuse);
		}

		static void SetViewSpaceZ(void*, uintptr_t alObj, float afZ){ O(alObj)->SetViewSpaceZ(afZ);}
		static float GetViewSpaceZ(void*, uintptr_t alObj){ return O(alObj)->GetViewSpaceZ();}

		static void AddToRenderList(void *apCtx, uintptr_t alObj){ C(apCtx)->mpRenderer->mpCurrentRenderList->AddObject(O(alObj));}
		static bool RenderObject(void *apCtx, uintptr_t alObj){ return C(apCtx)->mpCallback(C(apCtx)->mpRenderer, O(alObj));}
		static void RenderNodeBox(void *apCtx, uintptr_t alNode, uintptr_t alQuery){ C(apCtx)->mpRenderer->RenderNodeBoundingBox(N(alNode), Q(alQuery));}
		static uintptr_t GetQuery(void *apCtx){ return (uintptr_t)C(apCtx)->mpRenderer->GetOcclusionQuery();}
		static void ReleaseQuery(void *apCtx, uintptr_t alQuery){ C(apCtx)->mpRenderer->ReleaseOcclusionQuery(Q(alQuery));}
		static bool QueryFetch(void*, uintptr_t alQuery){ return Q(alQuery)->FetchResults();}
		static uint32_t QuerySamples(void*, uintptr_t alQuery){ return Q(alQuery)->GetSampleCount();}
		static void Flush(void *apCtx){ C(apCtx)->mpRenderer->mpLowLevelGraphics->FlushRendering();}
		static void Caster(void *apCtx, uintptr_t alObj){ C(apCtx)->mpCasters->push_back(O(alObj));}
	};

	const OhplRnHostVt* iRenderer::RustHost()
	{
		static const OhplRnHostVt vt = []{
			OhplRnHostVt v;
			memset(&v, 0, sizeof(v));
			OhplFillContainerHost(v);
			v.update_node = cRendererRustHost::UpdateNode;
			v.collide_node = cRendererRustHost::CollideNode;
			v.collide_object = cRendererRustHost::CollideObject;
			v.object_collides_frustum = cRendererRustHost::ObjectCollidesWithFrustum;
			v.near_plane_aabb = cRendererRustHost::NearPlaneAABB;
			v.object_check = cRendererRustHost::ObjectCheck;
			v.set_object_fade = cRendererRustHost::SetObjectFade;
			v.object_material = cRendererRustHost::ObjectMaterial;
			v.set_view_space_z = cRendererRustHost::SetViewSpaceZ;
			v.view_space_z = cRendererRustHost::GetViewSpaceZ;
			v.add_to_render_list = cRendererRustHost::AddToRenderList;
			v.render_object = cRendererRustHost::RenderObject;
			v.render_node_bbox = cRendererRustHost::RenderNodeBox;
			v.get_query = cRendererRustHost::GetQuery;
			v.release_query = cRendererRustHost::ReleaseQuery;
			v.query_fetch = cRendererRustHost::QueryFetch;
			v.query_samples = cRendererRustHost::QuerySamples;
			v.flush = cRendererRustHost::Flush;
			v.caster = cRendererRustHost::Caster;
			return v;
		}();
		return &vt;
	}

	//-----------------------------------------------------------------------

	void iRenderer::FillChecks(OhplRnChecks &avChecks)
	{
		avChecks.mbOcclusionPlanesActive = mbOcclusionPlanesActive;
		avChecks.mpPlanes = mvCurrentOcclusionPlanes.empty() ? NULL : &mvCurrentOcclusionPlanes[0];
		avChecks.mlPlaneCount = mvCurrentOcclusionPlanes.size();
		avChecks.mbFogCullPlaneActive = mbFogCullPlaneActive;
		avChecks.mFogCullPlane = mFogCullPlane;
		avChecks.mbDistanceCullActive = mbDistanceCullActive;
		avChecks.mvOrigin = mvDistanceCullOrigin;
		avChecks.mfMinRangeSqr = mfDistanceCullMinRangeSqr;
		avChecks.mfScreenSizeSqr = mfDistanceCullScreenSizeSqr;
		avChecks.mfRandomSize = mfDistanceCullRandomSize;
		avChecks.mfFadeTime = mfDistanceCullFadeTime;
		avChecks.mfFadeTimeInv = mfDistanceCullFadeTimeInv;
		avChecks.mfMaxRangeSqr = mfDistanceCullMaxRangeSqr;
		avChecks.mfTime = mfDistanceCullTime;
	}

	void iRenderer::RunSimpleRenderOps(bool abWireFrame, bool abUseShaders, iGpuProgram *apFlatProgram, iGpuProgram *apDiffuseProgram)
	{
		static const eRenderListType vTypes[4] = { eRenderListType_Z, eRenderListType_Diffuse, eRenderListType_Decal, eRenderListType_Translucent };
		std::vector<iRenderable*> vLists[4];
		for(int i=0; i<4; ++i)
		{
			if(abWireFrame && i==0) continue;
			cRenderableVecIterator it = mpCurrentRenderList->GetArrayIterator(vTypes[i]);
			while(it.HasNext()) vLists[i].push_back(it.Next());
		}

		std::vector<char> vZHasAlpha(vLists[0].size());
		for(size_t i=0; i<vLists[0].size(); ++i) vZHasAlpha[i] = vLists[0][i]->GetMaterial()->GetTexture(eMaterialTexture_Alpha) != NULL;

		bool *pAlpha = (bool*)(vZHasAlpha.empty() ? NULL : &vZHasAlpha[0]);
		size_t lOpNum = ohpl_rn_simple_ops(pAlpha, vLists[0].size(), vLists[1].size(), vLists[2].size(), vLists[3].size(), abUseShaders, abWireFrame, NULL, 0);
		std::vector<OhplRnOp> vOps(lOpNum);
		ohpl_rn_simple_ops(pAlpha, vLists[0].size(), vLists[1].size(), vLists[2].size(), vLists[3].size(), abUseShaders, abWireFrame, lOpNum ? &vOps[0] : NULL, lOpNum);

		for(size_t i=0; i<vOps.size(); ++i)
		{
			const OhplRnOp &op = vOps[i];
			iRenderable *pObject = NULL;
			if(op.mlKind >= eOhplRnOp_ObjTexture && op.mlKind <= eOhplRnOp_ObjMatrixFrustum) pObject = vLists[op.mlList][op.mlIndex];

			switch(op.mlKind)
			{
			case eOhplRnOp_DepthFunc:		SetDepthTestFunc(op.mlArg==0 ? eDepthTestFunc_LessOrEqual : eDepthTestFunc_Equal); break;
			case eOhplRnOp_DepthTest:		SetDepthTest(op.mlArg!=0); break;
			case eOhplRnOp_DepthWrite:		SetDepthWrite(op.mlArg!=0); break;
			case eOhplRnOp_BlendNone:		SetBlendMode(eMaterialBlendMode_None); break;
			case eOhplRnOp_AlphaMode:		SetAlphaMode(op.mlArg==0 ? eMaterialAlphaMode_Solid : eMaterialAlphaMode_Trans); break;
			case eOhplRnOp_ChannelMode:		SetChannelMode(op.mlArg==0 ? eMaterialChannelMode_None : eMaterialChannelMode_RGBA); break;
			case eOhplRnOp_TextureRangeNone:SetTextureRange(NULL,0); break;
			case eOhplRnOp_Program:			SetProgram(op.mlArg==0 ? apFlatProgram : apDiffuseProgram); break;
			case eOhplRnOp_ObjTexture:		SetTexture(0, pObject->GetMaterial()->GetTexture(op.mlArg==0 ? eMaterialTexture_Alpha : eMaterialTexture_Diffuse)); break;
			case eOhplRnOp_ObjMatrix:		SetMatrix(pObject->GetModelMatrixPtr()); break;
			case eOhplRnOp_ObjVertexBuffer:	SetVertexBuffer(pObject->GetVertexBuffer()); break;
			case eOhplRnOp_ObjBlend:		SetBlendMode(pObject->GetMaterial()->GetBlendMode()); break;
			case eOhplRnOp_ObjUpdateGraphics:pObject->UpdateGraphicsForViewport(mpCurrentFrustum, mfCurrentFrameTime); break;
			case eOhplRnOp_ObjMatrixFrustum:SetMatrix(pObject->GetModelMatrix(mpCurrentFrustum)); break;
			case eOhplRnOp_Draw:			if(op.mlArg==0) DrawCurrent(); else DrawCurrent(eVertexBufferDrawType_LineStrip); break;
			case eOhplRnOp_Callback:		RunCallback(op.mlArg==0 ? eRendererMessage_PostSolid : eRendererMessage_PostTranslucent); break;
			}
		}
	}

	//-----------------------------------------------------------------------

	void iRenderer::FillFrustumData(cFrustum *apFrustum, OhplRnFrustumData &avData)
	{
		avData.mvOrigin = apFrustum->GetOrigin();
		avData.mvForward = apFrustum->GetForward();
		for(int i=0; i<eFrustumPlane_LastEnum; ++i) avData.mvPlanes[i] = apFrustum->GetPlane((eFrustumPlane)i);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// RENDERER SORTING
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------
		
	//////////////////////////////////////////////////////////////////////////
	// RENDER SETTINGS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cRenderSettings::cRenderSettings(bool abIsReflection)
	{
		////////////////////////
		// Create data
		mpRenderList = hplNew( cRenderList, () );
	
		mpVisibleNodeTracker = hplNew( cVisibleRCNodeTracker, () );

		////////////////////////
		// Setup data
		mlCurrentOcclusionObject = 0;
		
		////////////////////////
		// Set up General Variables
		mbIsReflection = abIsReflection;
		mbLog = false;

		mClearColor = cColor(0,0);

		////////////////////////
		// Set up Render Variables
		//Change this later I assume:
		mlMinimumObjectsBeforeOcclusionTesting = 0;//8;//8 should be good default, giving a good amount of colliders, or? Clarifiction: Minium num of object rendered until node visibility tests start!
		mlSampleVisiblilityLimit = 3;

		mbUseCallbacks = true;

		mbUseEdgeSmooth = false;
		mbUseFxaa = false;

		mbUseOcclusionCulling = true;
		mbUseDelayedOcclusionCulling = false;

		mMaxShadowMapResolution = eShadowMapResolution_High;
		if(mbIsReflection)
			mMaxShadowMapResolution = eShadowMapResolution_Medium;

		mbClipReflectionScreenRect = true;

		mbUseScissorRect = false;

		mbRenderWorldReflection = true;
		mbRenderWorldFog = true;

		////////////////////////
		// Set up Shadow Variables
		mbRenderShadows = true;
		mfShadowMapBias = 4;			//The constant bias
		mfShadowMapSlopeScaleBias = 2;	//The bias based on sloping of depth.

		////////////////////////////
		//Light settings
		mbSSAOActive = mbIsReflection ? false : true;
		
		////////////////////////
		// Set up Output Variables
		mlNumberOfLightsRendered =0;
		mlNumberOfOcclusionQueries =0;

		////////////////////////
		// Set up Private Variables


		////////////////////////
		// Create Reflection settings
		mpReflectionSettings = NULL;
		if(mbIsReflection == false)
		{
			mpReflectionSettings = hplNew(cRenderSettings, (true) );
		}

	}

	cRenderSettings::~cRenderSettings()
	{
		hplDelete(mpRenderList);
		hplDelete(mpVisibleNodeTracker);
		
		STLDeleteAll(mvOcclusionObjectPool);

		for(size_t i=0; i<mvLightOcclusionPairs.size(); ++i)
		{
			if(mvLightOcclusionPairs[i].mpQuery)
				hplDelete(mvLightOcclusionPairs[i].mpQuery);
		}

		if(mpReflectionSettings) hplDelete(mpReflectionSettings);
	}

	//-----------------------------------------------------------------------

	void cRenderSettings::ResetVariables()
	{
		//return;
		mpVisibleNodeTracker->Reset();

        if(mpReflectionSettings) mpReflectionSettings->ResetVariables();		
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////
	//The render settings will use the default setup, except for the variables below
	// This means SSAO, edgesmooth, etc are always off for reflections.
	#define RenderSettingsCopy(aVar) mpReflectionSettings->aVar = aVar
	void cRenderSettings::SetupReflectionSettings()
	{
		if(mpReflectionSettings==NULL) return;
		RenderSettingsCopy(mbLog);

		RenderSettingsCopy(mClearColor);

		////////////////////////////
		//Render settings
		RenderSettingsCopy(mlMinimumObjectsBeforeOcclusionTesting);
		RenderSettingsCopy(mlSampleVisiblilityLimit);

		mpReflectionSettings->mbUseScissorRect = false;
		
		////////////////////////////
		//Shadow settings
		RenderSettingsCopy(mbRenderShadows);
		RenderSettingsCopy(mbRenderWorldFog);
		RenderSettingsCopy(mfShadowMapBias);
		RenderSettingsCopy(mfShadowMapSlopeScaleBias);

		////////////////////////////
		//Light settings
		//RenderSettingsCopy(mbSSAOActive);

		////////////////////////////
		//Output
		RenderSettingsCopy(mlNumberOfLightsRendered);
		RenderSettingsCopy(mlNumberOfOcclusionQueries);
	}

	//-----------------------------------------------------------------------

	void cRenderSettings::AddOcclusionPlane(const cPlanef &aPlane)
	{
		mvOcclusionPlanes.push_back(aPlane);
	}
	
	//-----------------------------------------------------------------------

	void cRenderSettings::ResetOcclusionPlanes()
	{
		mvOcclusionPlanes.clear();
	}


	//-----------------------------------------------------------------------

	void cRenderSettings::AssignOcclusionObject(iRenderer *apRenderer, void *apSource, int alCustomIndex, iVertexBuffer *apVtxBuffer, cMatrixf *apMatrix, bool abDepthTest)
	{
		if(mlCurrentOcclusionObject == mvOcclusionObjectPool.size())
		{
			mvOcclusionObjectPool.push_back(hplNew(cOcclusionQueryObject, () ));
		}

		cOcclusionQueryObject *pObject = mvOcclusionObjectPool[mlCurrentOcclusionObject];

		pObject->mlCustomID = alCustomIndex;
		pObject->mpQuery = apRenderer->GetOcclusionQuery();
		pObject->mpVtxBuffer = apVtxBuffer;
		pObject->mpMatrix = apMatrix;
		pObject->mbDepthTest = abDepthTest;

		m_setOcclusionObjects.insert(tOcclusionQueryObjectMap::value_type(apSource, pObject));

		++mlCurrentOcclusionObject;
	}
	
	//-----------------------------------------------------------------------


	int cRenderSettings::RetrieveOcclusionObjectSamples(iRenderer *apRenderer, void *apSource, int alCustomIndex)
	{
		tOcclusionQueryObjectMapIt it = m_setOcclusionObjects.find(apSource);
		if(it == m_setOcclusionObjects.end()){
			if(mbLog) Log(" Could not find source %d custom index %d in occlusion objects set!\n", apSource, alCustomIndex);
			return 0;
		}

		//////////////////////////////////////
		//Get the number of objects with key and get the one with right custom ID
		size_t lCount = m_setOcclusionObjects.count(apSource);
		cOcclusionQueryObject* pObject = NULL;
		for(size_t i=0; i < lCount; ++i)
		{
			cOcclusionQueryObject* pTestObject = it->second;
			if(pTestObject->mlCustomID == alCustomIndex)
			{
				pObject = pTestObject;
				break;
			}
			it++;
		}
		if(pObject==NULL)
		{
			if(mbLog) Log(" Found source %d but could NOT find custom index %d in occlusion objects set!\n", apSource, alCustomIndex);
			return 0;
		}

		//////////////////////////////////////
		// Get the query and wait (if needed) for result to come in.
		iOcclusionQuery *pQuery = pObject->mpQuery;
		
		//If query is null, then samples have already been retrieved.
		if(pQuery==NULL) return pObject->mlSampleResults;
		
		while(pQuery->FetchResults()==false);

		pObject->mlSampleResults = pQuery->GetSampleCount();
		pObject->mpQuery = NULL;
		apRenderer->ReleaseOcclusionQuery(pQuery);

		return pObject->mlSampleResults;
	}

	//-----------------------------------------------------------------------

	void cRenderSettings::WaitAndRetrieveAllOcclusionQueries(iRenderer *apRenderer)
	{
		for(int i=0; i<mlCurrentOcclusionObject; ++i)
		{
			iOcclusionQuery *pQuery = mvOcclusionObjectPool[i]->mpQuery;
			if(pQuery==NULL) continue;
			
            while(pQuery->FetchResults()==false);

			mvOcclusionObjectPool[i]->mlSampleResults = pQuery->GetSampleCount();
			mvOcclusionObjectPool[i]->mpQuery = NULL;
			apRenderer->ReleaseOcclusionQuery(pQuery);
		}
	}

	//-----------------------------------------------------------------------

	void cRenderSettings::ClearOcclusionObjects(iRenderer *apRenderer)
	{
		if(mbLog) Log(" Clearing occlusion queries i settings!\n");
		m_setOcclusionObjects.clear();
		for(int i=0; i<mlCurrentOcclusionObject; ++i)
		{
			iOcclusionQuery *pQuery = mvOcclusionObjectPool[i]->mpQuery;
			if(pQuery==NULL) continue;

			apRenderer->ReleaseOcclusionQuery(pQuery);
			mvOcclusionObjectPool[i]->mpQuery = NULL;
		}

		mlCurrentOcclusionObject = 0;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// SHADOWMAP CACHE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cShadowMapLightCache::SetFromLight(iLight* apLight)
	{
		mpLight = apLight;
		mlTransformCount = apLight->GetTransformUpdateCount();
		mfRadius = apLight->GetRadius();
		
		if(apLight->GetLightType() == eLightType_Spot)
		{
			cLightSpot *pSpotLight = static_cast<cLightSpot*>(apLight);
			mfAspect = pSpotLight->GetAspect();
			mfFOV = pSpotLight->GetFOV();
			mfNearClip = pSpotLight->GetNearClipPlane();
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iRenderer::iRenderer(const tString& asName, cGraphics *apGraphics,cResources* apResources, int alNumOfProgramComboModes)
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		mpCurrentSettings = NULL;

		//////////////
		//Set variables from arguments
		msName = asName;

		mpShaderManager = mpResources->GetGpuShaderManager();

		/////////////////////////////////
		//Set up the render functions
		SetupRenderFunctions(mpGraphics->GetLowLevel());

		////////////////////////
		// Debug Variables
		mbOnlyRenderPrevVisibleOcclusionObjects = false;
		mlOnlyRenderPrevVisibleOcclusionObjectsFrameCount =0;

		//////////////
		//Create data classes
		mpProgramManager  = hplNew(	cProgramComboManager, (msName, mpGraphics, mpResources, alNumOfProgramComboModes));

		//////////////
		//Init variables
		mfDefaultAlphaLimit = 0.5f;

		mbSetFrameBufferAtBeginRendering = true;
		mbClearFrameBufferAtBeginRendering = true;
		mbSetupOcclusionPlaneForFog = false;

		mfScissorLastFov =0;
		mfScissorLastTanHalfFov =0;

		mpCallbackFunctions = hplNew( cRendererCallbackFunctions, (this) );

		mfTimeCount =0;

		mlActiveOcclusionQueryNum =0;

		//////////////
		// Create programs
		cParserVarContainer vars;
		
		mpDepthOnlyProgram = mpProgramManager->CreateProgramFromShaders("DepthOnly",
											"deferred_base_vtx.glsl",
											"deferred_depthonly_frag.glsl",
											&vars,false);

		
		////////////
		// Create shapes
		//  Color and Texture because Geforce cards fail without it, no idea why...
		mpShapeBox = LoadVertexBufferFromMesh("core_box.dae", eVertexElementFlag_Position | eVertexElementFlag_Texture0| eVertexElementFlag_Color0, 0.5f);
	}

	//-----------------------------------------------------------------------

	iRenderer::~iRenderer()
	{
		DestroyShadowMaps();

		STLDeleteAll(mvOcclusionQueryPool);

		if(mpShapeBox) hplDelete(mpShapeBox);

		hplDelete(mpCallbackFunctions);
		hplDelete(mpProgramManager);
		
		hplDelete(mpDepthOnlyProgram);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void iRenderer::Render(float afFrameTime,cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings, cRenderTarget *apRenderTarget,
							bool abSendFrameBufferToPostEffects,tRendererCallbackList *apCallbackList)
	{
		BeginRendering(afFrameTime,apFrustum, apWorld, apSettings,apRenderTarget,abSendFrameBufferToPostEffects,apCallbackList);

		SetupRenderList();
        RenderObjects();
		
        EndRendering();
	}

	//-----------------------------------------------------------------------

	void iRenderer::Update(float afTimeStep)
	{
		if(mbWorldPaused==false) mfTimeCount += afTimeStep;
	}

	//-----------------------------------------------------------------------

	iTexture* iRenderer::GetPostEffectTexture()
	{
		if(mpCurrentRenderTarget->mpFrameBuffer)
		{
			iFrameBufferAttachment *pAttachement = mpCurrentRenderTarget->mpFrameBuffer->GetColorBuffer(0);
            iTexture *pTexture = static_cast<iTexture*>(pAttachement);
			
			return pTexture;
		}
		else
		{
			//TODO: Copy from frambuffer to texture. But what texture?
			return NULL;
		}
	}

	//-----------------------------------------------------------------------

	void iRenderer::AssignOcclusionObject(void *apSource, int alCustomIndex, iVertexBuffer *apVtxBuffer, cMatrixf *apMatrix, bool abDepthTest)
	{
		if(mbLog) Log("  Creating occlusion object from source %d with custom ID: %d . VtxBuffer: %d Matrix: %d Depthtest: %d\n", apSource, alCustomIndex, apVtxBuffer, apMatrix, abDepthTest);

		mpCurrentSettings->AssignOcclusionObject(this, apSource, alCustomIndex, apVtxBuffer, apMatrix, abDepthTest);
	}
	
	int iRenderer::RetrieveOcclusionObjectSamples(void *apSource, int alCustomIndex)
	{
		int lSamples = mpCurrentSettings->RetrieveOcclusionObjectSamples(this, apSource, alCustomIndex);	

		if(mbLog) Log("  Retrieved %d samples from occlusion object with source %d and custom ID: %d.\n",lSamples,apSource, alCustomIndex);

		return lSamples;
	}

	void iRenderer::WaitAndRetrieveAllOcclusionQueries()
	{
		if(mbLog) Log("  Retrieving sample count for all active occlusion queries.\n");

		mpCurrentSettings->WaitAndRetrieveAllOcclusionQueries(this);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool iRenderer::WorldFogActive()
	{
		return mpCurrentWorld->GetFogActive() && mpCurrentSettings->mbRenderWorldFog;
	}

	//-----------------------------------------------------------------------

	void iRenderer::BeginRendering(	float afFrameTime,cFrustum *apFrustum, cWorld *apWorld, cRenderSettings *apSettings, cRenderTarget *apRenderTarget,
									bool abSendFrameBufferToPostEffects,tRendererCallbackList *apCallbackList, bool abAtStartOfRendering)
	{
		if(apSettings->mbLog)
		{
			Log("Start Rendering Frustum: %d, World: '%s' TargetBuffer %d\n", apFrustum, apWorld->GetName().c_str(), apRenderTarget->mpFrameBuffer);
			Log("-----------------  START -------------------------\n");
		}

		//////////////////////////////////////////
		//Set up variables
		mbWorldPaused = apWorld->IsActive()==false;
		if(mbWorldPaused) afFrameTime = 0;
		mfCurrentFrameTime = afFrameTime;
		mpCurrentWorld = apWorld;
		mpCurrentSettings = apSettings;
		mpCurrentRenderList = apSettings->mpRenderList;

		mbSendFrameBufferToPostEffects = abSendFrameBufferToPostEffects;
		mpCallbackList = apCallbackList;

		mbOcclusionPlanesActive = true;

		mbDistanceCullActive = apWorld->GetDistanceCullActive() && apSettings->mbUseDistanceCulling;
		mvDistanceCullOrigin = apFrustum->GetOrigin();
		mfDistanceCullMinRangeSqr = apWorld->GetDistanceCullMinRange() * apWorld->GetDistanceCullMinRange();
		float fCullScreenSize = apWorld->GetDistanceCullScreenSize() / tan(apFrustum->GetFOV()*0.5f);
		mfDistanceCullScreenSizeSqr = fCullScreenSize * fCullScreenSize;
		mfDistanceCullRandomSize = apWorld->GetDistanceCullRandomSize();
		mfDistanceCullFadeTime = cMath::Max(1e-5f, apWorld->GetDistanceCullFadeSpeed());
		mfDistanceCullFadeTimeInv = 1.0f / mfDistanceCullFadeTime;
		mfDistanceCullMaxRangeSqr = apWorld->GetDistanceCullMaxRange() * apWorld->GetDistanceCullMaxRange();
		mfDistanceCullTime += afFrameTime;

		////////////////////////////////
		//Initialize render functions
		InitAndResetRenderFunctions(apFrustum, apRenderTarget, apSettings->mbLog, 
									apSettings->mbUseScissorRect, apSettings->mvScissorRectPos, apSettings->mvScissorRectSize);


		////////////////////////////////
		//Set up near plane variables

		//Calculate radius for near plane so that it is always inside it.
		float fTanHalfFOV = tan(mpCurrentFrustum->GetFOV()*0.5f);

		float fNearPlane = mpCurrentFrustum->GetNearPlane();
		mfCurrentNearPlaneTop =  fTanHalfFOV * fNearPlane;
		mfCurrentNearPlaneRight = mpCurrentFrustum->GetAspect() * mfCurrentNearPlaneTop;


		/////////////////////////////////////////////
		// Setup occlusion planes
		mvCurrentOcclusionPlanes.resize(0);

		//User clip planes
		for(size_t i=0; i<apSettings->mvOcclusionPlanes.size(); ++i)
			mvCurrentOcclusionPlanes.push_back(apSettings->mvOcclusionPlanes[i]);

        //Fog
		mbFogCullPlaneActive = false;
		float fFogCullEnd = apWorld->GetFogEnd();
		float fFogCullAlpha = apWorld->GetFogColor().a;
		if(apWorld->GetSecondaryFogActive() && apWorld->GetSecondaryFogEnd() > fFogCullEnd)
		{
			fFogCullEnd = apWorld->GetSecondaryFogEnd();
			fFogCullAlpha = apWorld->GetSecondaryFogColor().a;
		}
		if(mbSetupOcclusionPlaneForFog && WorldFogActive() && fFogCullAlpha >= 1.0f && apWorld->GetFogCulling())
		{
			mFogCullPlane.FromNormalPoint(apFrustum->GetForward(), apFrustum->GetOrigin() + apFrustum->GetForward()*-fFogCullEnd);
			mbFogCullPlaneActive = true;
		}

		/////////////////////////////////////////////
		// Init rendering

			//////////////////////////////
			//Render Target
			if(mbSetFrameBufferAtBeginRendering && abAtStartOfRendering)
			{
				SetFrameBuffer(mpCurrentRenderTarget->mpFrameBuffer, true, true);
			}

			/////////////////////////
			// General states
			mpLowLevelGraphics->SetClearColor(mpCurrentSettings->mClearColor);
			mpLowLevelGraphics->SetClearDepth(1);
			
			/////////////////////////
			// Render states
			mpLowLevelGraphics->SetColorWriteActive(true, true, true, true);
			
			mpLowLevelGraphics->SetCullActive(true);
			mpLowLevelGraphics->SetCullMode(eCullMode_CounterClockwise);
			
			mpLowLevelGraphics->SetDepthTestActive(true);
			mpLowLevelGraphics->SetDepthWriteActive(true);
			mpLowLevelGraphics->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
			
			mpLowLevelGraphics->SetColor(cColor(1,1,1,1));

			for(int i=0; i<kMaxTextureUnits; ++i)
				mpLowLevelGraphics->SetTexture(i, NULL);
		
			//////////////////////////////
			//Clear screen
			if(mbClearFrameBufferAtBeginRendering && abAtStartOfRendering)
			{
				ClearFrameBuffer(eClearFrameBufferFlag_Depth | eClearFrameBufferFlag_Color, true);
			}

			
			//////////////////////////////////////////////////
			// Projection matrix
			SetNormalFrustumProjection();

			/////////////////////////////////////////////
			// Clear Render list
			if(abAtStartOfRendering)
				mpCurrentRenderList->Clear();
	}

	//-----------------------------------------------------------------------
	
	void iRenderer::EndRendering(bool abAtEndOfRendering)
	{
		///////////////////////////////
		//Clear all occlusion objects
		if(abAtEndOfRendering)
			mpCurrentSettings->ClearOcclusionObjects(this);

		/////////////////////////////////////////////
		// If no post effects, make sure rendering is copied to frame buffer.
		if(abAtEndOfRendering && mbSendFrameBufferToPostEffects==false)
		{
			CopyToFrameBuffer();
		}

		/////////////////////////////////////////////
		// Reset all rendering states
		SetBlendMode(eMaterialBlendMode_None);
		SetChannelMode(eMaterialChannelMode_RGBA);
		SetAlphaMode(eMaterialAlphaMode_Solid);

		/////////////////////////////////////////////
		// Reset the cull mode setup
		mbInvertCullMode = false;
		mpLowLevelGraphics->SetCullMode(eCullMode_CounterClockwise);
		
		/////////////////////////////////////////////
		// Unbind all rendering data
		for(int i=0; i<kMaxTextureUnits; ++i)
		{
			if(mvCurrentTexture[i]) mpLowLevelGraphics->SetTexture(i, NULL);
		}
		
		if(mpCurrentProgram) mpCurrentProgram->UnBind();
		if(mpCurrentVtxBuffer) mpCurrentVtxBuffer->UnBind();

		/////////////////////////////////////////////
		// Clean up render functions
		ExitAndCleanUpRenderFunctions();

		if(abAtEndOfRendering && mbLog)
		{
			Log("----------\n");
			Log("Frame stats:\n");
			Log("----------\n");
			Log(" Number of Solid Objects: %d\n", mpCurrentRenderList->GetSolidObjectNum());
			Log(" Number of Trans Objects: %d\n", mpCurrentRenderList->GetTransObjectNum());
			Log(" Number of Lights: %d\n", mpCurrentRenderList->GetLightNum());
			Log("-----------------  END -------------------------\n");
		}

	}

	//-----------------------------------------------------------------------

	void iRenderer::CreateAndAddShadowMap(eShadowMapResolution aResolution, const cVector3l &avSize, ePixelFormat aFormat)
	{
		tString sName = "ShadowMap"+cString::ToString(avSize.x)+"x"+cString::ToString(avSize.y)+"_"+
						cString::ToString((int)mvShadowMapData[aResolution].size());
		mvShadowMapData[aResolution].push_back(CreateShadowMap(sName, avSize, aFormat));
	}

	cShadowMapData* iRenderer::CreateShadowMap(const tString& sName, const cVector3l &avSize, ePixelFormat aFormat)
	{
		cShadowMapData *pData = hplNew(cShadowMapData, ());
		pData->mlFrameCount = -1;

		pData->mpTexture = mpGraphics->CreateTexture(sName+"_Texture",eTextureType_2D, eTextureUsage_RenderTarget);
		pData->mpTexture->CreateFromRawData(avSize, aFormat, NULL);
		if(mbShadowMapCompare)
		{
			pData->mpTexture->SetCompareMode(eTextureCompareMode_RToTexture);
			pData->mpTexture->SetCompareFunc(eTextureCompareFunc_LessOrEqual);
		}
		pData->mpTexture->SetFilter(eTextureFilter_Nearest);	
		pData->mpTexture->SetWrapSTR(eTextureWrap_ClampToEdge);

		pData->mpTempDiffTexture = NULL;

		pData->mpBuffer = mpGraphics->CreateFrameBuffer(sName+"_Buffer");
		if(pData->mpTempDiffTexture) pData->mpBuffer->SetTexture2D(0, pData->mpTempDiffTexture);
		pData->mpBuffer->SetDepthTexture2D(pData->mpTexture);

		pData->mpBuffer->CompileAndValidate();

		return pData;
	}

	//-----------------------------------------------------------------------


    
	cShadowMapData* iRenderer::GetShadowMapData(eShadowMapResolution aResolution, iLight *apLight)
	{
		std::vector<cShadowMapData*> &vData = mvShadowMapData[aResolution];

		std::vector<OhplRnShadowSlot> vSlots(vData.size());
		for(size_t i=0; i<vData.size(); ++i)
		{
			vSlots[i].mpLight = (uintptr_t)vData[i]->mCache.mpLight;
			vSlots[i].mlFrameCount = vData[i]->mlFrameCount;
		}

		bool bUpdateFrameCount = false;
		int64_t lBest = ohpl_rn_pick_shadow_map(vSlots.empty() ? NULL : &vSlots[0], vSlots.size(), (uintptr_t)apLight, mlRenderFrameCount, &bUpdateFrameCount);
		if(lBest < 0) return NULL;

		cShadowMapData *pBestData = vData[(size_t)lBest];
		if(bUpdateFrameCount) pBestData->mlFrameCount = mlRenderFrameCount;

		return pBestData;
	}

	//-----------------------------------------------------------------------


	bool iRenderer::ShadowMapNeedsUpdate(iLight *apLight, cShadowMapData *apShadowData)
	{
		//Occlusion culling must always be updated!
		if(apLight->GetOcclusionCullShadowCasters()) return true;

		cShadowMapLightCache& cacheData = apShadowData->mCache;

		OhplRnShadowCache cache;
		cache.mpLight = (uintptr_t)cacheData.mpLight;
		cache.mlTransformCount = cacheData.mlTransformCount;
		cache.mfRadius = cacheData.mfRadius;
		cache.mfFOV = cacheData.mfFOV;
		cache.mfAspect = cacheData.mfAspect;
		cache.mfNearClip = cacheData.mfNearClip;

		OhplRnLightKey key;
		key.mpLight = (uintptr_t)apLight;
		key.mlTransformCount = apLight->GetTransformUpdateCount();
		key.mfRadius = apLight->GetRadius();
		key.mbIsSpot = apLight->GetLightType() == eLightType_Spot;
		key.mfFOV = key.mfAspect = key.mfNearClip = 0;
		if(key.mbIsSpot)
		{
			cLightSpot *pSpotLight = static_cast<cLightSpot*>(apLight);
			key.mfAspect = pSpotLight->GetAspect();
			key.mfFOV = pSpotLight->GetFOV();
			key.mfNearClip = pSpotLight->GetNearClipPlane();
		}

		bool bValid = ohpl_rn_shadow_cache_valid(&cache, &key);

		/////////////////////////////
		// Shadow casters
		if(bValid)
		{
			bValid = apLight->ShadowCastersAreUnchanged(mvShadowCasters);
		}

		/////////////////////////////
		// If not valid, update data
		if(bValid == false)
		{
			cacheData.SetFromLight(apLight);
			apLight->SetShadowCasterCacheFromVec(mvShadowCasters);
		}

		return bValid ? false : true;
	}
	
	//-----------------------------------------------------------------------

	void iRenderer::DestroyShadowMaps()
	{
		for(int res=0; res < eShadowMapResolution_LastEnum; ++res)
		{
			for(size_t i=0; i<mvShadowMapData[res].size(); ++i) DestroyShadowMap(mvShadowMapData[res][i]);
			mvShadowMapData[res].clear();
		}
	}

	void iRenderer::DestroyShadowMap(cShadowMapData *apData)
	{
		mpGraphics->DestroyFrameBuffer(apData->mpBuffer);
		mpGraphics->DestroyTexture(apData->mpTexture);
		if(apData->mpTempDiffTexture) mpGraphics->DestroyTexture(apData->mpTempDiffTexture);
		hplDelete(apData);
	}

	//-----------------------------------------------------------------------

	void iRenderer::RenderZObject(iRenderable *apObject, cFrustum *apCustomFrustum)
	{
		cMaterial *pMaterial = apObject->GetMaterial();

		eMaterialRenderMode renderMode = GetObjectCoverage(apObject)>=1 ? eMaterialRenderMode_Z : eMaterialRenderMode_Z_Dissolve;

		////////////////////////
		//Set up render modes
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaLimit(mfDefaultAlphaLimit);
		SetAlphaMode( pMaterial->GetTexture(eMaterialTexture_Alpha) || renderMode==eMaterialRenderMode_Z_Dissolve ? 
						eMaterialAlphaMode_Trans : eMaterialAlphaMode_Solid);

		////////////////////////
		//Set up textures
        SetMaterialTextures(renderMode, pMaterial);
		
        ////////////////////////
		//Set up program
		SetMaterialProgram(renderMode,pMaterial);
			

		////////////////////////
		//Matrix
		if(apCustomFrustum)
			SetMatrix(apObject->GetModelMatrix(apCustomFrustum));
		else
			SetMatrix(apObject->GetModelMatrixPtr());

		////////////////////////
		//Vertex buffer
		SetVertexBuffer(apObject->GetVertexBuffer());

		////////////////////////
		//Draw
		DrawCurrentMaterial(renderMode, apObject);
	}
	
	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void iRenderer::FetchDelayedOcclusionResults()
	{
		mpCurrentSettings->mlNumberOfOcclusionQueries =0;

		cRendererRustHost::Ctx ctx = { this, NULL, NULL };
		ohpl_rn_fetch_delayed(RustHost(), &ctx, mpCurrentSettings->mpVisibleNodeTracker->GetRustTracker(), mpCurrentSettings->mlSampleVisiblilityLimit);
	}

	// Occluded only while queries keep confirming it; unknown counts as visible.

	void iRenderer::RenderDelayedOcclusionQueries()
	{
		OhplRnTracker *pTracker = mpCurrentSettings->mpVisibleNodeTracker->GetRustTracker();
		if(ohpl_rn_has_delayed_queries(pTracker)==false) return;

		SetDepthTest(true);
		SetBlendMode(eMaterialBlendMode_None);
		SetChannelMode(eMaterialChannelMode_None);
		SetTextureRange(NULL,0);

		cRendererRustHost::Ctx ctx = { this, NULL, NULL };
		ohpl_rn_render_delayed(RustHost(), &ctx, pTracker);
	}

	//-----------------------------------------------------------------------

	void iRenderer::CheckForVisibleAndAddToList(iRenderableContainer *apContainer, tRenderableFlag alNeededFlags)
	{
		apContainer->UpdateBeforeRendering();

		OhplRnChecks checks;
		FillChecks(checks);
		cRendererRustHost::Ctx ctx = { this, NULL, NULL };
		ohpl_rn_add_visible_to_list(	RustHost(), &ctx, &checks, mpCurrentSettings->mpVisibleNodeTracker->GetRustTracker(),
										(uintptr_t)mpCurrentFrustum, mpCurrentSettings->mbUseDelayedOcclusionCulling, alNeededFlags,
										(uintptr_t)apContainer->GetRoot());
	}

	//-----------------------------------------------------------------------

	/**
	* Inserts the child nodes in apNode in a_setNodeStack.
	*/

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void iRenderer::RenderNodeBoundingBox(iRenderableContainerNode *apNode, iOcclusionQuery *apQuery)
	{
		////////////////////////
		//States
		SetDepthWrite(false);
		SetAlphaMode(eMaterialAlphaMode_Solid);

		/////////////////////////
		//Program
		SetProgram(mpDepthOnlyProgram);

		/////////////////////////
		//Texture
		SetTexture(0,NULL);

		/////////////////////////
		//Matrix
		cVector3f vSize = apNode->GetMax() - apNode->GetMin();
		cMatrixf mtxBox = cMath::MatrixScale(vSize);
		mtxBox.SetTranslation(apNode->GetCenter());
		SetModelViewMatrix( cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), mtxBox) );

		/////////////////////////
		//Vertex buffer
		SetVertexBuffer(mpShapeBox);
		

		/////////////////////////
		//Draw
		if(apQuery) apQuery->Begin();
		DrawCurrent();
		if(apQuery) apQuery->End();

		mpCurrentSettings->mlNumberOfOcclusionQueries++;
		
		//Debug:
		//mpCurrentSettings->mlstRenderedNodes.push_back(apNode);
	}

	//-----------------------------------------------------------------------

	bool iRenderer::RenderObjectZAndAddToRenderListStaticCallback(iRenderer *apRenderer, iRenderable *apObject)
	{
		return apRenderer->RenderObjectZAndAddToRenderList(apObject);
	}
	
	bool iRenderer::RenderObjectZAndAddToRenderList(iRenderable *apObject)
	{
		cMaterial *pMaterial = apObject->GetMaterial();

		/////////////////////////////
		// Add to render list
		mpCurrentRenderList->AddObject(apObject);
		
		////////////////////////////
		//Check if object is translucent or has no material
		// If so, do not render it.
		if( pMaterial==NULL || pMaterial->GetType()->IsTranslucent())
		{
			return false;
		}

		////////////////////////
		//Set up states
		SetDepthWrite(true);

		RenderZObject(apObject, NULL);

		return true;
	}

	//-----------------------------------------------------------------------

	void iRenderer::CheckForVisibleObjectsAddToListAndRenderZ(	cVisibleRCNodeTracker *apVisibleNodeTracker,
																tObjectVariabilityFlag alObjectTypes, tRenderableFlag alNeededFlags,
																bool abSetupRenderStates,
																tRenderCHCObjectCallbackFunc apRenderObjectCallback)
	{
		START_RENDER_PASS(CHC_Culling);

		////////////////////////////
		//Set up rendering
		if(abSetupRenderStates)
		{
			SetDepthTest(true);
			SetDepthWrite(false);
			SetBlendMode(eMaterialBlendMode_None);
			SetAlphaMode(eMaterialAlphaMode_Solid);
			SetChannelMode(eMaterialChannelMode_None);

			SetTextureRange(NULL,0);
		}

		////////////////////////////
		//Set up the containers and the root nodes
		const tFlag vContainerFlags[2] = { eObjectVariabilityFlag_Static, eObjectVariabilityFlag_Dynamic};

		iRenderableContainer *pContainers[2];
		pContainers[0] = mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static);
		pContainers[1] = mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic);

		// Set up output variables
		mpCurrentSettings->mlNumberOfOcclusionQueries =0;
		
		// Switch sets, so that the last frames set is not current.
		apVisibleNodeTracker->SwitchAndClearVisibleNodeSet();

		cRendererRustHost::Ctx ctx = { this, NULL, NULL };
		ctx.mpCallback = apRenderObjectCallback ? apRenderObjectCallback : RenderObjectZAndAddToRenderListStaticCallback;

		// Setup the container before rendering
		for(int i=0;i<2; ++i)
		{
			if(vContainerFlags[i] & alObjectTypes)
			{
				pContainers[i]->UpdateBeforeRendering();
			}
		}

		uintptr_t vRoots[2];
		size_t lRootNum =0;
		for(int i=0;i<2; ++i)
		{
			if(vContainerFlags[i] & alObjectTypes)
			{
				vRoots[lRootNum++] = (uintptr_t)pContainers[i]->GetRoot();
			}
		}

		OhplRnChecks checks;
		FillChecks(checks);

		OhplRnChc chc;
		chc.mpFrustum = (uintptr_t)mpCurrentFrustum;
		chc.m_mtxView = mpCurrentFrustum->GetViewMatrix();
		chc.mvOrigin = mpCurrentFrustum->GetOrigin();
		chc.mlNeededFlags = alNeededFlags;
		chc.mlMinObjectsBeforeOcclusion = mpCurrentSettings->mlMinimumObjectsBeforeOcclusionTesting;
		chc.mlSampleLimit = mpCurrentSettings->mlSampleVisiblilityLimit;
		chc.mbOnlyRenderPrevVisible = mbOnlyRenderPrevVisibleOcclusionObjects;
		chc.mlOnlyPrevVisibleFrameCount = mlOnlyRenderPrevVisibleOcclusionObjectsFrameCount;

		mlOnlyRenderPrevVisibleOcclusionObjectsFrameCount =
			ohpl_rn_chc_traverse(RustHost(), &ctx, &checks, apVisibleNodeTracker->GetRustTracker(), &chc, vRoots, lRootNum);

		END_RENDER_PASS();
	}
	
	//-----------------------------------------------------------------------

	void iRenderer::GetShadowCasters(iRenderableContainer *apContainer, tRenderableVec& avObjectVec, cFrustum *apLightFrustum, float afMinRadius, bool abViewCheck)
	{
		apContainer->UpdateBeforeRendering();

		OhplRnChecks checks;
		FillChecks(checks);

		OhplRnShadowQuery query;
		query.mpLight = (uintptr_t)apLightFrustum;
		query.mvLightOrigin = apLightFrustum->GetOrigin();
		query.mfMinRadius = afMinRadius;
		query.mlCasterFlag = eRenderableFlag_ShadowCaster;
		query.mbHasViewCheck = abViewCheck;
		if(abViewCheck)
		{
			OhplRnFrustumData viewData, lightData;
			FillFrustumData(mpCurrentFrustum, viewData);
			FillFrustumData(apLightFrustum, lightData);
			ohpl_rn_view_check(&viewData, &lightData, &query.mViewCheck);
		}

		cRendererRustHost::Ctx ctx = { this, NULL, &avObjectVec };
		ohpl_rn_shadow_casters(RustHost(), &ctx, &checks, &query, (uintptr_t)apContainer->GetRoot());
	}

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	bool iRenderer::SetupShadowMapRendering(iLight *apLight)
	{
		/////////////////////////
		// Get light data
		if(apLight->GetLightType() != eLightType_Spot) return false; //Only support spot lights for now...

		cLightSpot *pSpotLight = static_cast<cLightSpot*>(apLight);
		cFrustum *pLightFrustum = pSpotLight->GetFrustum();

		/////////////////////////
		// If culling by occlusion, skip rest of function
		if(apLight->GetOcclusionCullShadowCasters())
		{
			//mvShadowCasters.resize(0); //Debug reasons only, remove later!
			return true;
		}


		/////////////////////////
		// Get objects to render

		//Clear list
		mvShadowCasters.resize(0); //No clear, so we keep all in memory.

		//Get the objects
		if(apLight->GetShadowCastersAffected() & eObjectVariabilityFlag_Dynamic)
			GetShadowCasters(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic),mvShadowCasters, pSpotLight->GetFrustum());

		if(apLight->GetShadowCastersAffected() & eObjectVariabilityFlag_Static)
			GetShadowCasters(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static),mvShadowCasters, pSpotLight->GetFrustum());

		//See if any objects where added.
		if(mvShadowCasters.empty()) return false;

		//Sort the list
		ohpl_rn_sort_shadow_casters(RustHost(), NULL, (uintptr_t*)&mvShadowCasters[0], mvShadowCasters.size());
		
		return true;
	}

	//-----------------------------------------------------------------------

	static cFrustum * gpTempLightFrustum=NULL;
	bool iRenderer::RenderShadowCasterCHCStaticCallback(iRenderer *apRenderer, iRenderable *apObject)
	{
		return apRenderer->RenderShadowCasterCHC(apObject);
	}
	
	bool iRenderer::RenderShadowCasterCHC(iRenderable *apObject)
	{
		cMaterial *pMaterial = apObject->GetMaterial();

		//Check so it is a solid object
		if( pMaterial==NULL || pMaterial->GetType()->IsTranslucent())
		{
			return false;
		}

		//mvShadowCasters.push_back(apObject); //Debug. Only to see what object are rendered.
		
		//Render the object
		RenderShadowCaster(apObject,gpTempLightFrustum);

		return true;
	}

	//-----------------------------------------------------------------------

	void iRenderer::RenderShadowCaster(iRenderable *apObject, cFrustum *apLightFrustum)
	{
		if(mbLog) Log("  Shadow caster '%s'\n", apObject->GetName().c_str());
		RenderZObject(apObject, apLightFrustum);
	}

	//-----------------------------------------------------------------------


	void iRenderer::RenderShadowCastersNormal(cFrustum *apLightFrustum)
	{
		////////////////////////////////
		// Iterate the objects to be rendered 
		for(size_t i=0; i<mvShadowCasters.size(); ++i)
		{
			RenderShadowCaster(mvShadowCasters[i], apLightFrustum);
		}
	}
	//-----------------------------------------------------------------------

	void iRenderer::RenderShadowMap(iLight *apLight, iFrameBuffer *apShadowBuffer)
	{
		if(mbLog){ 
			Log("---\nBegin Rendering Shadow Map for light '%s' / %d to buffer %d\n",apLight->GetName().c_str(), apLight, apShadowBuffer);
		}
		/////////////////////////
		// Get light data
		if(apLight->GetLightType() != eLightType_Spot) return; //Only support spot lights for now...

        cLightSpot *pSpotLight = static_cast<cLightSpot*>(apLight);
		cFrustum *pLightFrustum = pSpotLight->GetFrustum();
	
       		
		/////////////////////////
		// Setup render states
		SetDepthTest(true);
		SetDepthWrite(true);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetAlphaLimit(mfDefaultAlphaLimit);
		SetChannelMode(eMaterialChannelMode_None);

		SetTextureRange(NULL,0);

		//Do not use any custom occlusion for shadows!
		SetOcclusionPlanesActive(false);

		mpLowLevelGraphics->SetPolygonOffsetActive(true);
		if(mbShadowDepthClamp) mpLowLevelGraphics->SetDepthClampActive(true);
		if(!mbShadowCull) SetCullActive(false);
		mpLowLevelGraphics->SetPolygonOffset(mpCurrentSettings->mfShadowMapBias * apLight->GetShadowMapBiasMul(), 
											 mpCurrentSettings->mfShadowMapSlopeScaleBias * apLight->GetShadowMapSlopeScaleBiasMul());
		
		/////////////////////////
		// Setup render target
		SetFrameBuffer(apShadowBuffer,false, false);

		mpLowLevelGraphics->SetClearDepth(1);
		ClearFrameBuffer(eClearFrameBufferFlag_Depth, false);
		
		/////////////////////////
		// Setup projection
		cFrustum *pLastFrustum = mpCurrentFrustum;
		mpCurrentFrustum = pLightFrustum;	//Is this a little too hackish? Not sure...
		SetFrustumProjection(pLightFrustum);
		
		/////////////////////////
		// Render the shadow casters
		if(apLight->GetOcclusionCullShadowCasters())
		{
			gpTempLightFrustum = pLightFrustum;
			CheckForVisibleObjectsAddToListAndRenderZ(	apLight->GetVisibleNodeTracker(),
														apLight->GetShadowCastersAffected(),
														eRenderableFlag_ShadowCaster, 
														false, 
														RenderShadowCasterCHCStaticCallback);
		}
		else
		{
			RenderShadowCastersNormal(pLightFrustum);
		}
		
        
		/////////////////////////
		// Reset states
		SetTexture(0,NULL);

		SetOcclusionPlanesActive(true);

		mpLowLevelGraphics->SetPolygonOffsetActive(false);
		if(mbShadowDepthClamp) mpLowLevelGraphics->SetDepthClampActive(false);
		if(!mbShadowCull) SetCullActive(true);

		/////////////////////////
		// Reset projection
		mpCurrentFrustum = pLastFrustum;
		SetNormalFrustumProjection();

        
		if(mbLog) Log("End Rendering Shadow Map\n---\n");
	}

	//-----------------------------------------------------------------------

	static bool SortFunc_OcclusionObject(cOcclusionQueryObject* apObjectA, cOcclusionQueryObject *apObjectB)
	{
		//////////////////////////
		//Vertex buffer
		if(apObjectA->mpVtxBuffer != apObjectB->mpVtxBuffer)
		{
			return apObjectA->mpVtxBuffer < apObjectB->mpVtxBuffer;
		}

		//////////////////////////
		//Depth
		if(apObjectA->mbDepthTest != apObjectB->mbDepthTest)
		{
			return apObjectA->mbDepthTest;
		}

		//////////////////////////
		//Matrix
		return apObjectA->mpMatrix < apObjectB->mpMatrix;
	}

	void iRenderer::AssignAndRenderOcclusionQueryObjects(bool abSetFrameBuffer, iFrameBuffer *apFrameBuffer, bool abUsePosAndSize)
	{
		cRenderList *pRenderList = mpCurrentSettings->mpRenderList;

		///////////////////////////////////
		// Get and use any previous occlusion queries
		cRenderableVecIterator objIt = pRenderList->GetOcclusionQueryObjectIterator();
		while(objIt.HasNext())
		{
			iRenderable *pObject = objIt.Next();
			pObject->AssignOcclusionQuery(this);
		}

		/////////////////////////////////////
		//If no queries added, then skip any rendering
		if(mpCurrentSettings->mlCurrentOcclusionObject <=0) return;
		
		START_RENDER_PASS(OcclusionObjects);

		////////////////////////////////////
		// Copying queries to new array
		mvSortedOcclusionObjects.resize(mpCurrentSettings->mlCurrentOcclusionObject);
		for(int i=0; i<mpCurrentSettings->mlCurrentOcclusionObject; ++i)
			mvSortedOcclusionObjects[i] = mpCurrentSettings->mvOcclusionObjectPool[i];

		////////////////////////////////////
		// Sort the queries
		std::sort(mvSortedOcclusionObjects.begin(), mvSortedOcclusionObjects.end(), SortFunc_OcclusionObject);

		///////////////////////////////////
		// Set up frame buffer
		if(abSetFrameBuffer)
		{
			SetFrameBuffer(apFrameBuffer, abUsePosAndSize);
		}

		///////////////////////////////////
		// Set up rendering
		SetDepthTest(true);
		SetDepthWrite(false);
		SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_None);

		SetTextureRange(NULL,0);
		SetProgram(NULL);

		///////////////////////////////////
		// Render the queries
		for(size_t i=0; i<mvSortedOcclusionObjects.size(); ++i)
		{
			cOcclusionQueryObject *pObject = mvSortedOcclusionObjects[i];

			if(pObject->mbDepthTest)
				SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
			else
				SetDepthTestFunc(eDepthTestFunc_Always);

			SetMatrix(pObject->mpMatrix);


			SetVertexBuffer(pObject->mpVtxBuffer);

			pObject->mpQuery->Begin();
			DrawCurrent();
			pObject->mpQuery->End();
		}


		//Make sure rendering is on its way!
		mpLowLevelGraphics->FlushRendering();

		///////////////////////////////////
		// Reset some settings
		SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
		SetChannelMode(eMaterialChannelMode_RGBA);


		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	void iRenderer::RetrieveAllLightOcclusionPair(bool abWaitForResult)
	{
		for(size_t i=0; i<mpCurrentSettings->mvLightOcclusionPairs.size(); ++i)
		{
			cLightOcclusionPair &loPair = mpCurrentSettings->mvLightOcclusionPairs[i];
			iOcclusionQuery *pQuery = loPair.mpQuery;

			if(pQuery == NULL) continue;

			//Wait for results if specfied.
			if(abWaitForResult)
			{
				while(pQuery->FetchResults());
			}

			//Get result and set resulting samples
			if(pQuery->FetchResults())
			{
				loPair.mlSampleResults = pQuery->GetSampleCount();
			}
			else
			{
				loPair.mlSampleResults =0;
			}

			loPair.mpQuery = NULL;
			ReleaseOcclusionQuery(pQuery);
		}
	}
	
	//-----------------------------------------------------------------------

	void iRenderer::RenderBasicSkyBox()
	{
		if(mpCurrentWorld==NULL || mpCurrentWorld->GetSkyBoxActive()==false) return;
		START_RENDER_PASS(BasicSkyBox);

		SetDepthTest(true);
		SetDepthWrite(false);

		SetBlendMode(eMaterialBlendMode_None);
		SetAlphaMode(eMaterialAlphaMode_Solid);
		SetChannelMode(eMaterialChannelMode_RGBA);
				
		/////////////////////////
		//Calculate and set matrix
		m_mtxSkyBox = cMatrixf::Identity;

		float fFarClip = mpCurrentFrustum->GetFarPlane();

		float fSide = sqrt((fFarClip*fFarClip) / 3) *0.95f;
		m_mtxSkyBox.m[0][0] = fSide;
		m_mtxSkyBox.m[1][1] = fSide;
		m_mtxSkyBox.m[2][2] = fSide;

		m_mtxSkyBox.SetTranslation(mpCurrentFrustum->GetOrigin());
		
		SetMatrix(&m_mtxSkyBox);

		/////////////////////////
		//Program
		SetProgram(GetSkyBoxProgram());

		/////////////////////////
		//Texture and vertex buffer
		iTexture *pSkyTex = mpCurrentWorld->GetSkyBoxTexture();
		SetTexture(0, pSkyTex ? pSkyTex : mpWhiteCubeTexture);
		SetTextureRange(NULL,1);
        
		SetVertexBuffer(mpCurrentWorld->GetSkyBoxVertexBuffer());

		DrawCurrent();

		END_RENDER_PASS();
	}

	//-----------------------------------------------------------------------

	bool iRenderer::SetupLightScissorRect(iLight *apLight, cMatrixf *apViewSpaceMatrix)
	{
		if(mfScissorLastFov != mpCurrentFrustum->GetFOV())
		{
			mfScissorLastTanHalfFov = tan(mpCurrentFrustum->GetFOV()*0.5f);
		}
	
		/*cMath::GetClipRectFromBV(mTempClipRect,*apLight->GetBoundingVolume(),mpCurrentFrustum,
								mvScreenSize, mfScissorLastTanHalfFov);*/
		

		mTempClipRect = cMath::GetClipRectFromSphere(apViewSpaceMatrix->GetTranslation(),
													apLight->GetRadius(), mpCurrentFrustum,
													mvRenderTargetSize,true,mfScissorLastTanHalfFov);
		
		return SetScissorRect(mTempClipRect, true);
	}

	//-----------------------------------------------------------------------

	
	void iRenderer::SetMaterialProgram(eMaterialRenderMode aRenderMode, cMaterial *apMaterial)
	{
		iMaterialType *pMatType = apMaterial->GetType();
		iGpuProgram *pProgram = pMatType->GetRenderProgram(apMaterial, aRenderMode, this);
		
		///////////////////////////////////////
		// Check if program is set
		bool bNewProgramSet = false;
		if(mpCurrentProgram != pProgram)
		{
			bNewProgramSet = true;
			if(pProgram)
			{
				if(mbLog) Log("  Setting gpu program %d : '%s'\n", pProgram, pProgram->GetName().c_str());
				pProgram->Bind();	
			}
			else
			{
				if(mbLog) Log("  Setting gpu program NULL\n");
				mpCurrentProgram->UnBind();
			}
			
			mpCurrentProgram = pProgram;
		}

		///////////////////////////////////////
		// A program is currently set, check if variables need updating
		if(mpCurrentProgram)
		{
			//////////////////////
			//Check if Type specific changes are needed
			if(pMatType->HasTypeSpecifics(aRenderMode) && (bNewProgramSet || mpCurrentMaterialType != pMatType) )
			{
				if(mbLog) Log("  Setting up type specific program vars for material type %d/'%s'\n",pMatType,pMatType->GetName().c_str());
				pMatType->SetupTypeSpecificData(aRenderMode,pProgram,this);
				mpCurrentMaterialType = pMatType;
			}
			//////////////////////
			//Check if Material specific changes are needed
			if(apMaterial->GetHasSpecificSettings(aRenderMode) && (bNewProgramSet || mpCurrentMaterial != apMaterial) )
			{
				if(mbLog) Log("  Setting up material specific program vars for material %d/'%s'\n",apMaterial,apMaterial->GetName().c_str());
				pMatType->SetupMaterialSpecificData(aRenderMode,pProgram,apMaterial,this);
				mpCurrentMaterial = apMaterial;
			}
		}

		
		
		//TODO: If Cg is to be used, the worldviewproj matrix need to be setup!
	}

	//-----------------------------------------------------------------------

	void iRenderer::SetMaterialTextures(eMaterialRenderMode aRenderMode, cMaterial *apMaterial)
	{
		iMaterialType *pType = apMaterial->GetType();
		
		for(int i=0; i<kMaxTextureUnits; ++i)
		{
			//Set texture, if special textures are used, check for those too!
			iTexture *pTexture = apMaterial->GetTextureInUnit(aRenderMode,i);
			
			if(mvCurrentTexture[i] != pTexture)
			{
				if(mbLog) {
					if(pTexture)
						Log("  Setting texture unit: %d, %d/'%s'\n",i,pTexture,pTexture->GetName().c_str());
					else
						Log("  Setting texture unit: %d, 'NULL\n",i);
				}

				mpLowLevelGraphics->SetTexture(i, pTexture);
				mvCurrentTexture[i] = pTexture;
			}
		}
	}

	//-----------------------------------------------------------------------

	void iRenderer::DrawCurrentMaterial(eMaterialRenderMode aRenderMode, iRenderable *apObject)
	{
		if(apObject)
		{
			cMaterial *pMaterial = apObject->GetMaterial();
			iMaterialType *pMatType = pMaterial->GetType();

			if(pMaterial->HasObjectSpecificsSettings(aRenderMode) && mpCurrentProgram)
			{
				pMatType->SetupObjectSpecificData(aRenderMode,mpCurrentProgram,apObject,this);
			}
		}

		DrawCurrent();
	}
	
	//-----------------------------------------------------------------------

	bool iRenderer::CheckRenderablePlaneIsVisible(iRenderable *apObject, cFrustum *apFrustum)
	{
		if(apObject->GetRenderType() != eRenderableType_SubMesh) return true;
		
		cSubMeshEntity *pSubMeshEnt = static_cast<cSubMeshEntity*>(apObject);
		cSubMesh *pSubMesh = pSubMeshEnt->GetSubMesh();
		
		if(pSubMesh->GetIsOneSided()==false) return true;

		cVector3f vNormal = cMath::MatrixMul3x3(apObject->GetWorldMatrix(), pSubMesh->GetOneSidedNormal());
		cVector3f vPos = cMath::MatrixMul(apObject->GetWorldMatrix(), pSubMesh->GetOneSidedPoint());

		float fDot = cMath::Vector3Dot(vPos - apFrustum->GetOrigin(), vNormal);

		return fDot < 0;		
	}

	//-----------------------------------------------------------------------

	cRect2l iRenderer::GetClipRectFromObject(iRenderable *apObject, float afPaddingPercent, cFrustum *apFrustum, const cVector2l &avScreenSize, float afHalfFovTan)
	{
		cBoundingVolume *pBV = apObject->GetBoundingVolume();

		cRect2l clipRect;
		if(afHalfFovTan ==0)	
			afHalfFovTan = tan(apFrustum->GetFOV()*0.5f);
		cMath::GetClipRectFromBV(clipRect, *pBV, apFrustum, avScreenSize, afHalfFovTan);

		if(OhplBackendRustRequested())
		{
			OhplH8Rect rect = { clipRect.x, clipRect.y, clipRect.w, clipRect.h };
			ohpl_h8_clip_pad(&rect, avScreenSize.x, avScreenSize.y, afHalfFovTan);
			clipRect.x = rect.x; clipRect.y = rect.y; clipRect.w = rect.w; clipRect.h = rect.h;
			return clipRect;
		}

		//Add 20% padding on clip rect
		int lXInc = (int)((float)clipRect.w*afHalfFovTan);
		int lYInc = (int)((float)clipRect.h*afHalfFovTan);

		clipRect.x = cMath::Max(clipRect.x-lXInc, 0);
		clipRect.y = cMath::Max(clipRect.y-lYInc, 0);
		clipRect.w = cMath::Min(clipRect.w+lXInc*2, avScreenSize.x-clipRect.x);
		clipRect.h = cMath::Min(clipRect.h+lYInc*2, avScreenSize.y-clipRect.y);

		return clipRect;
	}

	
	//-----------------------------------------------------------------------

	bool iRenderer::CheckObjectIsVisible(iRenderable *apObject, tRenderableFlag alNeededFlags)
	{
		OhplRnChecks checks;
		FillChecks(checks);
		return ohpl_rn_object_visible(RustHost(), NULL, &checks, (uintptr_t)apObject, alNeededFlags);
	}


	float iRenderer::GetObjectCoverage(iRenderable *apObject)
	{
		return apObject->GetCoverageAmount() * GetDistanceFadeAmount(apObject);
	}

	float iRenderer::GetDistanceFadeAmount(iRenderable *apObject)
	{
		OhplRnChecks checks;
		FillChecks(checks);
		return ohpl_rn_distance_fade_amount(&checks, apObject->DistanceCullFade());
	}

	//-----------------------------------------------------------------------

	bool iRenderer::CheckNodeIsVisible(iRenderableContainerNode *apNode)
	{
		OhplRnChecks checks;
		FillChecks(checks);
		return ohpl_rn_node_visible(RustHost(), NULL, &checks, (uintptr_t)apNode);
	}

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------
	
	static const cVector3f gvFogBoxPlaneNormals[6] = {
			cVector3f(-1,0,0), //Left
			cVector3f(1,0,0), //Right

			cVector3f(0,-1,0), //Bottom	
			cVector3f(0,1,0), //Top

			cVector3f(0,0,-1), //Back
			cVector3f(0,0,1), //Front	
	};
	static const cVector3f gvFogBoxCompareSize = cVector3f(0.5001f);


	//-----------------------------------------------------------------------



	void iRenderer::SetupFogRenderDataArray(bool abSort)
	{
		mpCurrentSettings->mvFogRenderData.resize(0);
		for(int i=0; i<mpCurrentRenderList->GetFogAreaNum(); ++i)
		{
			cFogArea *pFogArea = mpCurrentRenderList->GetFogArea(i);
			cFogAreaRenderData fogData;

			fogData.mpFogArea = pFogArea;
			fogData.m_mtxInvBoxSpace = cMath::MatrixInverse( *pFogArea->GetModelMatrixPtr() );

			cVector3f vNearPlaneVtx[4];
			for(int j=0; j<4; ++j) vNearPlaneVtx[j] = mpCurrentFrustum->GetVertex(j);
			fogData.mbInsideNearFrustum = ohpl_rn_fog_inside_near_plane(&fogData.m_mtxInvBoxSpace, vNearPlaneVtx);
			fogData.mvBoxSpaceFrustumOrigin = cMath::MatrixMul(fogData.m_mtxInvBoxSpace, mpCurrentFrustum->GetOrigin());

			mpCurrentSettings->mvFogRenderData.push_back(fogData);
		}

		if(abSort && mpCurrentSettings->mvFogRenderData.empty()==false)
		{
			std::vector<cFogAreaRenderData> &vData = mpCurrentSettings->mvFogRenderData;
			std::vector<float> vViewZ(vData.size());
			std::vector<uint32_t> vOrder(vData.size());
			for(size_t i=0; i<vData.size(); ++i) vViewZ[i] = vData[i].mpFogArea->GetViewSpaceZ();
			ohpl_rn_fog_sort_order(&vViewZ[0], vViewZ.size(), &vOrder[0]);

			std::vector<cFogAreaRenderData> vSorted;
			vSorted.reserve(vData.size());
			for(size_t i=0; i<vOrder.size(); ++i) vSorted.push_back(vData[vOrder[i]]);
			vData.swap(vSorted);
		}
	}

	//-----------------------------------------------------------------------

	float iRenderer::GetFogAreaVisibilityForObject(cFogAreaRenderData *apFogData, iRenderable *apObject)
	{
		cFogArea *pFogArea = apFogData->mpFogArea;

		OhplRnFogParams params;
		params.mbShowBacksideInside = pFogArea->GetShowBacksideWhenInside();
		params.mbShowBacksideOutside = pFogArea->GetShowBacksideWhenOutside();
		params.mbInsideNearPlane = apFogData->mbInsideNearFrustum;
		params.mfStart = pFogArea->GetStart();
		params.mfEnd = pFogArea->GetEnd();
		params.mfColorA = pFogArea->GetColor().a;
		params.mfFalloffExp = pFogArea->GetFalloffExp();

		cVector3f vObjectPos = apObject->GetBoundingVolume()->GetWorldCenter();
		cVector3f vFrustumOrigin = mpCurrentFrustum->GetOrigin();
		return ohpl_rn_fog_visibility(&params, &apFogData->m_mtxInvBoxSpace, &apFogData->mvBoxSpaceFrustumOrigin, &vFrustumOrigin, &vObjectPos);
	}


	//-----------------------------------------------------------------------

	void iRenderer::SetOcclusionPlanesActive(bool abX)
	{
		mbOcclusionPlanesActive = abX;
	}
	
	//-----------------------------------------------------------------------

	iVertexBuffer* iRenderer::CreateQuadVertexBuffer(	eVertexBufferType aType,
		const cVector3f& avPos, const cVector2f& avSize, 
		const cVector2f& avMinUV, const cVector2f& avMaxUV,
		bool abInvertY)
	{
		iVertexBuffer *pVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(	aType,
			eVertexBufferDrawType_Quad,
			eVertexBufferUsageType_Dynamic);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);

		for(int i=0; i<4; ++i) pVtxBuffer->AddIndex(i);

		float fMinUV_Y = abInvertY ? avMaxUV.y : avMinUV.y;
		float fMaxUV_Y = abInvertY ? avMinUV.y : avMaxUV.y;

		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(avMinUV.x, fMinUV_Y,0)); 
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, cVector3f(avPos.x, avPos.y,avPos.z));

		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(avMaxUV.x, fMinUV_Y,0)); 
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position,  cVector3f(avPos.x+avSize.x, avPos.y,avPos.z));

		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(avMaxUV.x, fMaxUV_Y,0)); 
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position,  cVector3f(avPos.x+avSize.x, avPos.y+avSize.y,avPos.z));

		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(avMinUV.x, fMaxUV_Y,0)); 
		pVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position,  cVector3f(avPos.x, avPos.y+avSize.y,avPos.z));

		pVtxBuffer->Compile(0);

		return pVtxBuffer;
	}

	//-----------------------------------------------------------------------

	void iRenderer::UpdateqQuadVertexPostion(iVertexBuffer *apVtxBuffer,const cVector3f& avPos, const cVector2f& avSize, bool abCallUpdate)
	{
		int lVtxStride = apVtxBuffer->GetElementNum(eVertexBufferElement_Position);
		float *pPos = apVtxBuffer->GetFloatArray(eVertexBufferElement_Position);

		//0
		pPos[0*lVtxStride +0] = avPos.x;
		pPos[0*lVtxStride +1] = avPos.y;
		pPos[0*lVtxStride +2] = avPos.z;

		//1
		pPos[1*lVtxStride +0] = avPos.x+avSize.x;
		pPos[1*lVtxStride +1] = avPos.y;
		pPos[1*lVtxStride +2] = avPos.z;

		//2
		pPos[2*lVtxStride +0] = avPos.x+avSize.x;
		pPos[2*lVtxStride +1] = avPos.y+avSize.y;
		pPos[2*lVtxStride +2] = avPos.z;

		//3
		pPos[3*lVtxStride +0] = avPos.x;
		pPos[3*lVtxStride +1] = avPos.y+avSize.y;
		pPos[3*lVtxStride +2] = avPos.z;

		if(abCallUpdate) apVtxBuffer->UpdateData(eVertexElementFlag_Position,false);
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* iRenderer::LoadVertexBufferFromMesh(const tString& asMeshName, tVertexElementFlag alVtxToCopy, float afMaxExtent)
	{
		iVertexBuffer *pVtxBuffer = mpResources->GetMeshManager()-> CreateVertexBufferFromMesh(asMeshName, alVtxToCopy);
		if(pVtxBuffer==NULL) FatalError("Could not load vertex buffer from mesh '%s'\n",asMeshName.c_str());

		if(afMaxExtent > 0)
		{
			float *pPos = pVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
			int lStride = pVtxBuffer->GetElementNum(eVertexBufferElement_Position);
			int lNum = pVtxBuffer->GetVertexNum();
			float fMax = 0;
			for(int i=0; i<lNum; ++i)
				for(int j=0; j<3; ++j) fMax = cMath::Max(fMax, std::fabs(pPos[i*lStride+j]));
			if(fMax > 0 && std::fabs(fMax - afMaxExtent) > afMaxExtent*0.01f)
			{
				float fScale = afMaxExtent / fMax;
				for(int i=0; i<lNum; ++i)
					for(int j=0; j<3; ++j) pPos[i*lStride+j] *= fScale;
				pVtxBuffer->UpdateData(eVertexElementFlag_Position, false);
			}
		}

		return pVtxBuffer;

		/*cMesh *pMesh = mpResources->GetMeshManager()->CreateMesh(asMeshName);
		if(pMesh==NULL)FatalError("Could not load mesh '%s'\n",asMeshName.c_str());

		iVertexBuffer *pVtxBuffer = pMesh->GetSubMesh(0)->GetVertexBuffer()->CreateCopy(eVertexBufferType_Hardware,
																						eVertexBufferUsageType_Static,
																						alVtxToCopy);
		mpResources->GetMeshManager()->Destroy(pMesh);
		return pVtxBuffer;*/
	}

	//-----------------------------------------------------------------------

	void iRenderer::RunCallback(eRendererMessage aMessage)
	{
		if(mpCallbackList == NULL || mpCurrentSettings->mbUseCallbacks==false) return;

		tRendererCallbackListIt it = mpCallbackList->begin();
        for(; it != mpCallbackList->end(); ++it)
		{
			iRendererCallback *pCallback = *it;

            pCallback->RunMessage(aMessage, mpCallbackFunctions);
		}
	}

	//-----------------------------------------------------------------------

	eShadowMapResolution iRenderer::GetShadowMapResolution(eShadowMapResolution aWanted, eShadowMapResolution aMax)
	{
		return (eShadowMapResolution)ohpl_rn_shadow_map_resolution((int32_t)aWanted, (int32_t)aMax);
	}

	//-----------------------------------------------------------------------
	
	iOcclusionQuery *iRenderer::GetOcclusionQuery()
	{
		iOcclusionQuery *pOcclusionQuery = NULL;

		if(mvOcclusionQueryPool.empty())
		{
			pOcclusionQuery = mpLowLevelGraphics->CreateOcclusionQuery();
		}
		else
		{
			pOcclusionQuery = mvOcclusionQueryPool.back();
			mvOcclusionQueryPool.resize(mvOcclusionQueryPool.size()-1);
		}

		mlActiveOcclusionQueryNum++;

		return pOcclusionQuery;
	}
	
	void iRenderer::ReleaseOcclusionQuery(iOcclusionQuery * apQuery)
	{
		mlActiveOcclusionQueryNum--;

		mvOcclusionQueryPool.push_back(apQuery);
	}
	
	//-----------------------------------------------------------------------


}
