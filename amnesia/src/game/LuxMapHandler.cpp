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

#include "LuxMapHandler.h"
#include "LuxMap.h"

#include "LuxPlayer.h"
#include "LuxEffectRenderer.h"
#include "LuxDebugHandler.h"
#include "LuxSavedGame.h"
#include "LuxHelpFuncs.h"
#include "LuxConfigHandler.h"

cMapHandlerSoundCallback::cMapHandlerSoundCallback(OhplA13H *apHandler) : mpHandler(apHandler)
{
	tString sFile = "sounds/EnemySounds.dat";
	iXmlDocument* pXmlDoc = gpBase->mpEngine->GetResources()->LoadXmlDocument(sFile);
	if(pXmlDoc ==NULL)
	{
		Error("Couldn't load XML file '%s'!\n",sFile.c_str());
		return;
	}

	cXmlNodeListIterator it = pXmlDoc->GetChildIterator();
	while(it.HasNext())
	{
		cXmlElement *pChildElem = it.Next()->ToElement();
		tString sName = pChildElem->GetAttributeString("name");
		ohpl_a13_handler_add_hearable(mpHandler, sName.data(), sName.size());
	}

	gpBase->mpEngine->GetResources()->DestroyXmlDocument( pXmlDoc );
}

void cMapHandlerSoundCallback::OnStart(cSoundEntity *apSoundEntity)
{
	tString sTypeName = apSoundEntity->GetData()->GetName();
	cVector3f vPos = apSoundEntity->GetWorldPosition();
	ohpl_a13_handler_sound_started(mpHandler, sTypeName.data(), sTypeName.size(), apSoundEntity->GetMaxDistance(), apSoundEntity->GetVolume(), &vPos.x);
}

cLuxDebugRenderCallback::cLuxDebugRenderCallback()
{
}

void cLuxDebugRenderCallback::OnPostSolidDraw(cRendererCallbackFunctions* apFunctions)
{
	if(mpPhysicsWorld)
	{
		apFunctions->SetMatrix(NULL);
		apFunctions->SetBlendMode(eMaterialBlendMode_Alpha);
		apFunctions->SetTextureRange(NULL,0);
		apFunctions->SetProgram(NULL);

		apFunctions->SetDepthTest(true);
		apFunctions->SetDepthWrite(false);
	}

	gpBase->mpDebugHandler->RenderSolid(apFunctions);
	gpBase->mpMapHandler->RenderSolid(apFunctions);
	gpBase->mpPlayer->RenderSolid(apFunctions);
	gpBase->mpEffectRenderer->RenderSolid(apFunctions);
}

void cLuxDebugRenderCallback::OnPostTranslucentDraw(cRendererCallbackFunctions* apFunctions)
{
	gpBase->mpPlayer->RenderTrans(apFunctions);
	gpBase->mpEffectRenderer->RenderTrans(apFunctions);
}

static void A7DropMap(void *p){ hplDelete(static_cast<cLuxMap*>(p)); }

cLuxMapHandler::cLuxMapHandler() : iLuxUpdateable("LuxMapHandler")
{
	mpCurrentMap = NULL;
	mpR = ohpl_a13_handler_new((uintptr_t)this);
	ohpl_a7_set_drop(ohpl_a13_handler_maps(mpR), kOhplA7Entity, A7DropMap);

	//////////////////////////
	//Create and setup view port
	mpViewport = gpBase->mpEngine->GetScene()->CreateViewport();
	gpBase->mpEngine->GetScene()->SetCurrentListener(mpViewport);

	//////////////////////////
	//Set up post effects
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cPostEffectComposite *pPostEffectComp = pGraphics->CreatePostEffectComposite();
	mpViewport->SetPostEffectComposite(pPostEffectComp);

	//Bloom
	cPostEffectParams_Bloom bloomParams;
	bloomParams.mfBlurSize = 1.0f;
	bloomParams.mvRgbToIntensity = bloomParams.mvRgbToIntensity * 1.0f;
	mpPostEffect_Bloom = pGraphics->CreatePostEffect(&bloomParams);
	pPostEffectComp->AddPostEffect(mpPostEffect_Bloom, 100);

	//Image trail
	cPostEffectParams_ImageTrail imageTrailParams;
	mpPostEffect_ImageTrail = pGraphics->CreatePostEffect(&imageTrailParams);
	pPostEffectComp->AddPostEffect(mpPostEffect_ImageTrail, 10);
	mpPostEffect_ImageTrail->SetActive(false);

	//Radial
	cPostEffectParams_RadialBlur radialBlurParams;
	radialBlurParams.mfSize = 0.0f;
	mpPostEffect_RadialBlur = pGraphics->CreatePostEffect(&radialBlurParams);
	pPostEffectComp->AddPostEffect(mpPostEffect_RadialBlur, 9);
	mpPostEffect_RadialBlur->SetActive(false);

	//Sepia
	cPostEffectParams_ColorConvTex sepiaParams;
	sepiaParams.msTextureFile = "colorconv_sepia.tga";
	sepiaParams.mfFadeAlpha = 0.0f;
	mpPostEffect_Sepia = pGraphics->CreatePostEffect(&sepiaParams);
	pPostEffectComp->AddPostEffect(mpPostEffect_Sepia, 4);
	mpPostEffect_Sepia->SetActive(false);

	//////////////////////////
	//Saving
	mpSavedGame = hplNew( cLuxSavedGameMapCollection, () );

	//////////////////////////
	//Callbacks
	mpSoundCallback = hplNew( cMapHandlerSoundCallback, (mpR) );
	cSoundEntity::AddGlobalCallback(mpSoundCallback);

	//////////////////////////
	//Threading
	mpSavedGameMutex = cPlatform::CreateMutEx();

	//////////////////////////
	//Variables
	mpDataCache =NULL;

	Reset();
}

cLuxMapHandler::~cLuxMapHandler()
{
	hplDelete(mpSavedGame);
	hplDelete(mpSavedGameMutex);
	hplDelete(mpSoundCallback);

	ohpl_a13_handler_teardown(mpR);
	ohpl_a13_handler_free(mpR);
}

void cLuxMapHandler::OnStart()
{
	//////////////////////
	//Set up viewport
	mpViewport->SetCamera(gpBase->mpPlayer->GetCamera());
	mpViewport->AddGuiSet(gpBase->mpGameDebugSet);
	mpViewport->AddGuiSet(gpBase->mpGameHudSet);
	mpViewport->AddRendererCallback(&mRenderCallback);

	UpdateViewportRenderProperties();
}

void cLuxMapHandler::UpdateViewportRenderProperties()
{
	cRenderSettings *pRenderSettings = mpViewport->GetRenderSettings();

	pRenderSettings->mbRenderWorldReflection = gpBase->mpConfigHandler->mbWorldReflection;
	pRenderSettings->mbRenderShadows = gpBase->mpConfigHandler->mbShadowsActive;
}

void cLuxMapHandler::LoadUserConfig()
{
	ohpl_a13_handler_set_show_commentary(mpR, gpBase->mpUserConfig->GetBool("Game","ShowCommentary", false), false);
}

void cLuxMapHandler::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game","ShowCommentary", GetShowCommentary());
}

void cLuxMapHandler::CreateDataCache()
{
	if(mpDataCache) DestroyDataCache();

	mpDataCache = hplNew(cLuxModelCache, () );
	mpDataCache->Create();
}

void cLuxMapHandler::DestroyDataCache()
{
	if(mpDataCache==NULL) return;

	hplDelete(mpDataCache);
	mpDataCache = NULL;
}

void cLuxMapHandler::ClearSaveMapCollection()
{
	mpSavedGameMutex->Lock();
		mpSavedGame->Reset();
	mpSavedGameMutex->Unlock();
}

void cLuxMapHandler::SetSavedMapCollection(cLuxSavedGameMapCollection *apMaps)
{
	mpSavedGameMutex->Lock();
	{
		hplDelete(mpSavedGame);
		mpSavedGame = apMaps;
	}
	mpSavedGameMutex->Unlock();
}

void cLuxMapHandler::LoadMainConfig()
{
	mpPostEffect_Bloom->SetDisabled(gpBase->mpMainConfig->GetBool("Graphics", "PostEffectBloom", true)==false);
	mpPostEffect_ImageTrail->SetDisabled(gpBase->mpMainConfig->GetBool("Graphics", "PostEffectImageTrail", true)==false);
	mpPostEffect_Sepia->SetDisabled(gpBase->mpMainConfig->GetBool("Graphics", "PostEffectSepia", true)==false);
	mpPostEffect_RadialBlur->SetDisabled(gpBase->mpMainConfig->GetBool("Graphics", "PostEffectRadialBlur", true)==false);

	cRenderSettings *pRenderSettings = mpViewport->GetRenderSettings();
	pRenderSettings->mbUseEdgeSmooth = gpBase->mpConfigHandler->mbEdgeSmooth; //This is saved in config handler!
}

void cLuxMapHandler::SaveMainConfig()
{
	gpBase->mpMainConfig->SetBool("Graphics", "PostEffectBloom", mpPostEffect_Bloom->IsDisabled()==false);
	gpBase->mpMainConfig->SetBool("Graphics", "PostEffectImageTrail", mpPostEffect_ImageTrail->IsDisabled()==false);
	gpBase->mpMainConfig->SetBool("Graphics", "PostEffectSepia", mpPostEffect_Sepia->IsDisabled()==false);
	gpBase->mpMainConfig->SetBool("Graphics", "PostEffectRadialBlur", mpPostEffect_RadialBlur->IsDisabled()==false);
}

void cLuxMapHandler::HostSetView(cWorld *apWorld, iPhysicsWorld *apPhysicsWorld)
{
	mpViewport->SetWorld(apWorld);
	if(apWorld)
	{
		mRenderCallback.mpPhysicsWorld = apPhysicsWorld;
		mRenderCallback.mpLowLevelGfx = gpBase->mpEngine->GetGraphics()->GetLowLevel();
	}
}
