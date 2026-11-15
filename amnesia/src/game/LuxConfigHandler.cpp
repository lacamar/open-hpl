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

#include "LuxConfigHandler.h"

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxInputHandler.h"
#include "LuxHintHandler.h"
#include "LuxHelpFuncs.h"
#include "RustLuxBase.h"
#include "LuxA12bDump.h"

static_assert(sizeof(cVector2l) == 2 * sizeof(int32_t), "screen size layout");

cLuxConfigHandler::cLuxConfigHandler() : mlH(ohpl_a12b_cfg_new()), mpN(ohpl_a12b_cfg_nums(mlH)),
	mbLoadDebugMenu(mpN->loadDebugMenu), mbFirstStart(mpN->firstStart), mbCreateAndLoadCompressedMaps(mpN->createCompressed),
	mbForceCacheLoadingAndSkipSaving(mpN->forceCache), mvScreenSize(*(cVector2l*)&mpN->screenWidth), mlDisplay(mpN->display),
	mbFullscreen(mpN->fullscreen), mbVSync(mpN->vsync), mbAdaptiveVSync(mpN->adaptiveVsync), mlTextureQuality(mpN->textureQuality),
	mlTextureFilter(mpN->textureFilter), mfTextureAnisotropy(mpN->textureAnisotropy), mlShadowQuality(mpN->shadowQuality),
	mlShadowRes(mpN->shadowRes), mlGuiScale(mpN->guiScale), mfPaperWhite(mpN->paperWhite), mfPeakNits(mpN->peakNits),
	mbSSAOActive(mpN->ssaoActive), mlSSAOSamples(mpN->ssaoSamples), mlSSAOResolution(mpN->ssaoResolution),
	mlParallaxQuality(mpN->parallaxQuality), mbParallaxEnabled(mpN->parallaxEnabled), mbOcclusionTestLights(mpN->occlusionTestLights),
	mbEdgeSmooth(mpN->edgeSmooth), mbWorldReflection(mpN->worldReflection), mbRefraction(mpN->refraction),
	mbShadowsActive(mpN->shadowsActive), mbForceShaderModel3And4Off(mpN->forceSm34Off), mbFastPhysicsLoad(mpN->fastPhysics),
	mbFastStaticLoad(mpN->fastStatic), mbFastEntityLoad(mpN->fastEntity), mlSoundDevID(mpN->soundDevId),
	mlMaxSoundChannels(mpN->maxSoundChannels), mlSoundStreamBuffers(mpN->soundStreamBuffers), mlSoundStreamBufferSize(mpN->soundStreamBufferSize)
{
}

void cLuxConfigHandler::LoadMainConfig()
{
	////////////////////////////////////////////////////////////////////////////////
	// Purpose of this is loading variables that need to be set on engine creation,
	// or set to restart.
	cConfigFile *pCfg = gpBase->mpMainConfig;
	tString sDefaultLang = gpBase->msDefaultGameLanguage;
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t n, OhplGa9Ret *r)
	{
		OhplA12bLogCall("cfg", op, a, n);
		if(op == eOhplGa9Op_FirstStartFlag) r->i = gpBase->CheckFirstStartFlag();
		else OhplGa9CfgCall(pCfg, op, a, r);
	};
	ohpl_a12b_cfg_load(mlH, &call, OhplGa9CallOf<decltype(call)>::Fn(), (const unsigned char*)sDefaultLang.data(), sDefaultLang.size());
}

void cLuxConfigHandler::SaveMainConfig()
{
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t n, OhplGa9Ret *r)
	{
		OhplA12bLogCall("cfg", op, a, n);
		switch(op)
		{
		case eOhplGa9Op_RaiseFirstStart: gpBase->RaiseFirstStartFlag(); break;
		case eOhplGa9Op_Gamma: r->f = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection(); break;
		case eOhplGa9Op_GBufferType: r->i = cRendererDeferred::GetGBufferType(); break;
		case eOhplGa9Op_NumGBuffer: r->i = cRendererDeferred::GetNumOfGBufferTextures(); break;
		case eOhplGa9Op_Volume: r->f = gpBase->mpEngine->GetSound()->GetLowLevel()->GetVolume(); break;
		case eOhplGa9Op_LimitFps: r->i = gpBase->mpEngine->GetLimitFPS(); break;
		case eOhplGa9Op_WaitFocus: r->i = gpBase->mpEngine->GetWaitIfAppOutOfFocus(); break;
		default: OhplGa9CfgCall(gpBase->mpMainConfig, op, a, r); break;
		}
	};
	ohpl_a12b_cfg_save(mlH, &call, OhplGa9CallOf<decltype(call)>::Fn());
}

bool cLuxConfigHandler::ShowRestartWarning(cGuiSet* apSet, void* apObject, tGuiCallbackFunc apCallback)
{
	///////////////////////////////////////////////////////////////////////////////////
	// This will show a popup if really needed, once per "restarting" setting changed.
	// Also, will return true if the popup was actually shown.
	if(ohpl_a12b_cfg_restart(mlH, 1))
	{
		cGuiPopUpMessageBox* pPopUp = apSet->CreatePopUpMessageBox(kTranslate("OptionsMenu", "ReqRestartLabel"),
									 kTranslate("OptionsMenu", "ReqRestartMessage"), 
									 kTranslate("MainMenu","OK"), _W(""),
									 apObject, apCallback);
		pPopUp->GetGuiSet()->SetDrawFocus(true);
		return true;
	}

	return false;
}
