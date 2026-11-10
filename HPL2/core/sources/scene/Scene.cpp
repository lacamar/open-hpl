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

#include "scene/Scene.h"

#include "scene/Viewport.h"
#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/RustWorld.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Script.h"
#include "system/Platform.h"

#include "resources/Resources.h"
#include "resources/ScriptManager.h"
#include "resources/FileSearcher.h"
#include "resources/WorldLoaderHandler.h"

#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/LowLevelGraphics.h"

#include "sound/Sound.h"
#include "sound/LowLevelSound.h"
#include "sound/SoundHandler.h"

#include "gui/Gui.h"
#include "gui/GuiSet.h"

#include "physics/Physics.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cScene::cScene(cGraphics *apGraphics,cResources *apResources, cSound* apSound,cPhysics *apPhysics,
					cSystem *apSystem, cAI *apAI,cGui *apGui, cHaptic *apHaptic)
		: iUpdateable("HPL_Scene")
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		mpSound = apSound;
		mpPhysics = apPhysics;
		mpSystem = apSystem;
		mpAI = apAI;
		mpGui = apGui;
		mpHaptic = apHaptic;

		mpCurrentListener = NULL;
	}

	//-----------------------------------------------------------------------

	cScene::~cScene()
	{
		Log("Exiting Scene Module\n");
		Log("--------------------------------------------------------\n");

		STLDeleteAll(mlstViewports);
		STLDeleteAll(mlstWorlds);
		STLDeleteAll(mlstCameras);

		Log("--------------------------------------------------------\n\n");

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	cViewport* cScene::CreateViewport(cCamera *apCamera, cWorld *apWorld, bool abPushFront)
	{
		cViewport *pViewport = hplNew ( cViewport, (this) );	

		pViewport->SetCamera(apCamera);
		pViewport->SetWorld(apWorld);
		pViewport->SetSize(-1);
		pViewport->SetRenderer(mpGraphics->GetRenderer(eRenderer_Main));

		if (abPushFront) {
			mlstViewports.push_front(pViewport);
		} else {
			mlstViewports.push_back(pViewport);
		}

		return pViewport;
	}
	
	//-----------------------------------------------------------------------

	void cScene::DestroyViewport(cViewport* apViewPort)
	{
		STLFindAndDelete(mlstViewports, apViewPort);
	}

	//-----------------------------------------------------------------------

	bool cScene::ViewportExists(cViewport* apViewPort)
	{
		return OhplPtrIndex(mlstViewports, apViewPort) >= 0;
	}

	//-----------------------------------------------------------------------

	void cScene::SetCurrentListener(cViewport* apViewPort)
	{
		//If there was a previous listener make sure that world is not a listener.
		if(mpCurrentListener != NULL && ViewportExists(mpCurrentListener))
		{
			mpCurrentListener->SetIsListener(false);
			cWorld *pWorld = mpCurrentListener->GetWorld();
			if(pWorld && WorldExists(pWorld)) pWorld->SetIsSoundEmitter(false);
		}
		
		mpCurrentListener = apViewPort;
		if(mpCurrentListener)
		{
			mpCurrentListener->SetIsListener(true);
			cWorld *pWorld = mpCurrentListener->GetWorld();
			if(pWorld) pWorld->SetIsSoundEmitter(true);
		}
	}

	//-----------------------------------------------------------------------

	cCamera* cScene::CreateCamera(eCameraMoveMode aMoveMode)
	{
		cCamera *pCamera = hplNew( cCamera, () );
		pCamera->SetAspect(mpGraphics->GetLowLevel()->GetScreenSizeFloat().x /
							mpGraphics->GetLowLevel()->GetScreenSizeFloat().y);

		//Add Camera to list
		mlstCameras.push_back(pCamera);

		return pCamera;
	}


	//-----------------------------------------------------------------------

	void cScene::DestroyCamera(cCamera* apCam)
	{
		STLFindAndDelete(mlstCameras, apCam);
	}

	//-----------------------------------------------------------------------

	void cScene::Render(float afFrameTime, tFlag alFlags)
	{
		//Increase the frame count (do this at top, so render count is valid until this Render is called again!)
		iRenderer::IncRenderFrameCount();


		///////////////////////////////////////////
		// Iterate all viewports and render
		tViewportListIt viewIt = mlstViewports.begin();
		for(; viewIt != mlstViewports.end(); ++viewIt)
		{
			cViewport *pViewPort = *viewIt;

			//////////////////////////////////////////////
			//Init vars
			cPostEffectComposite *pPostEffectComposite = pViewPort->GetPostEffectComposite();
			iRenderer *pRenderer = pViewPort->GetRenderer();
			cCamera *pCamera = pViewPort->GetCamera();
			cFrustum *pFrustum = pCamera ? pCamera->GetFrustum() : NULL;

			OhplViewportIn view = {};
			view.visible = pViewPort->IsVisible();
			view.has_composite = pPostEffectComposite != NULL;
			view.has_renderer = pRenderer != NULL;
			view.has_frustum = pFrustum != NULL;

			uint32_t lPlan = ohpl_scene_plan_viewport(alFlags, &view);
			if(lPlan & kOhplStepSkip) continue;

			//////////////////////////////////////////////
			//Render world and call callbacks
			if(lPlan & kOhplStepPreCallback)
				pViewPort->RunViewportCallbackMessage(eViewportMessage_OnPreWorldDraw);

			//The callback may change the world and the effects, so ask again after it
			view.has_world = pViewPort->GetWorld() != NULL;
			if(ohpl_scene_wants_post_effects(alFlags, view.has_composite))
				view.post_effects_active = pPostEffectComposite->HasActiveEffects();
			lPlan = ohpl_scene_plan_viewport(alFlags, &view);
			const bool bPostEffects = (lPlan & kOhplStepPostFlag) != 0;

			if(lPlan & kOhplStepPostCallback)
			{
				if(lPlan & kOhplStepRenderWorld)
				{
					START_TIMING(RenderWorld)
					pRenderer->Render(	afFrameTime,pFrustum,
										pViewPort->GetWorld(),pViewPort->GetRenderSettings(), 
										pViewPort->GetRenderTarget(),
										bPostEffects,
										pViewPort->GetRendererCallbackList());
					STOP_TIMING(RenderWorld)
				}
				else
				{
					//If no renderer sets up viewport do that by our selves.
					cRenderTarget* pRenderTarget = pViewPort->GetRenderTarget();
					mpGraphics->GetLowLevel()->SetCurrentFrameBuffer(	pRenderTarget->mpFrameBuffer,
																		pRenderTarget->mvPos,
																		pRenderTarget->mvSize);
				}
				pViewPort->RunViewportCallbackMessage(eViewportMessage_OnPostWorldDraw);

				//////////////////////////////////////////////
				//Render 3D GuiSets
				// Should this really be here? Or perhaps send in a frame buffer depending on the renderer.
				START_TIMING(Render3DGui)
				Render3DGui(pViewPort,pFrustum, afFrameTime);
				STOP_TIMING(Render3DGui)
			}

			//////////////////////////////////////////////
			//Render Post effects
			if(lPlan & kOhplStepPostEffects)
			{
				//TODO: If renderer is null get texture from frame buffer and if frame buffer is NULL, then copy to a texture.
				//		Or this is solved?
				iTexture *pInputTexture = pRenderer->GetPostEffectTexture();

				START_TIMING(RenderPostEffects)
				pPostEffectComposite->Render(afFrameTime, pFrustum, pInputTexture,pViewPort->GetRenderTarget());
				STOP_TIMING(RenderPostEffects)
			}
			
			//////////////////////////////////////////////
			//Render Screen GUI
			if(lPlan & kOhplStepGuiScreen)
			{
				START_TIMING(RenderGUI)
				RenderScreenGui(pViewPort, afFrameTime);
				STOP_TIMING(RenderGUI)
			}
		}
	}

	//-----------------------------------------------------------------------

	void cScene::PostUpdate(float afTimeStep)
	{
		//////////////////////////////////////
		//Update worlds
		tWorldListIt it = mlstWorlds.begin();
		for(; it != mlstWorlds.end(); ++it)
		{
			cWorld *pWorld = *it;
            if(pWorld->IsActive()) pWorld->Update(afTimeStep);
		}


		//////////////////////////////////////
		//Update listener position with current listener, if there is one.
		if(mpCurrentListener && mpCurrentListener->GetCamera())
		{
			cCamera* pCamera3D = mpCurrentListener->GetCamera();
			mpSound->GetLowLevel()->SetListenerAttributes(	pCamera3D->GetPosition(), cVector3f(0,0,0),
															pCamera3D->GetForward()*-1.0f, pCamera3D->GetUp());
		}
	}

	//-----------------------------------------------------------------------

	void cScene::Reset()
	{
	}

	//-----------------------------------------------------------------------

	cWorld* cScene::LoadWorld(const tString& asFile, tWorldLoadFlag aFlags)
	{
		///////////////////////////////////
		// Load the map file
		tWString asPath = mpResources->GetFileSearcher()->GetFilePath(asFile);
		if(asPath == _W(""))
		{
			if(cResources::GetCreateAndLoadCompressedMaps())
				asPath = mpResources->GetFileSearcher()->GetFilePath(cString::SetFileExt(asFile,"cmap"));
			
			if(asPath == _W(""))
			{
				Error("World '%s' doesn't exist\n",asFile.c_str());
				return NULL;
			}
		}

		cWorld* pWorld = mpResources->GetWorldLoaderHandler()->LoadWorld(asPath, aFlags);
		if(pWorld==NULL){
			Error("Couldn't load world from '%s'\n",cString::To8Char(asPath).c_str());
			return NULL;
		}

		return pWorld;
	}

	//-----------------------------------------------------------------------

	cWorld* cScene::CreateWorld(const tString& asName)
	{
		cWorld* pWorld = hplNew( cWorld, (asName,mpGraphics,mpResources,mpSound,mpPhysics,this,
										mpSystem,mpAI,mpHaptic) );

		mlstWorlds.push_back(pWorld);

		return pWorld;
	}

	//-----------------------------------------------------------------------

	void cScene::DestroyWorld(cWorld* apWorld)
	{
		STLFindAndDelete(mlstWorlds,apWorld);
	}

	//-----------------------------------------------------------------------

	bool cScene::WorldExists(cWorld* apWorld)
	{
		return OhplPtrIndex(mlstWorlds, apWorld) >= 0;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cScene::Render3DGui(cViewport *apViewPort,cFrustum *apFrustum,float afTimeStep)
	{
		if(apViewPort->GetCamera()==NULL) return;

		cGuiSetListIterator it = apViewPort->GetGuiSetIterator();	
		while(it.HasNext())
		{
			cGuiSet *pSet = it.Next();
			if(pSet->Is3D())
			{
				pSet->Render(apFrustum);
			}
		}
	}
	
	void cScene::RenderScreenGui(cViewport *apViewPort,float afTimeStep)
	{
		///////////////////////////////////////
		//Put all of the non 3D sets in draw priority order (stable)
		std::vector<cGuiSet*> vSets;
		std::vector<int32_t> vPrios;
		cGuiSetListIterator it = apViewPort->GetGuiSetIterator();
		while(it.HasNext())
		{
			cGuiSet *pSet = it.Next();
			
			if(pSet->Is3D()==false)
			{
				vSets.push_back(pSet);
				vPrios.push_back(pSet->GetDrawPriority());
			}
		}

		///////////////////////////////////////
		//Iterate and render all sets
		if(vSets.empty()) return;
		std::vector<uint32_t> vOrder(vSets.size());
		const int lNum = ohpl_scene_gui_order(vPrios.data(), (int32_t)vSets.size(), vOrder.data());
		for(int i=0; i<lNum; ++i)
		{
			cGuiSet *pSet = vSets[vOrder[i]];

			//Log("Rendering gui '%s'\n", pSet->GetName().c_str());

			pSet->Render(NULL);
		}
	}

	//-----------------------------------------------------------------------
}
