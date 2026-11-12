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

// TODO: there is some cleanup to do here

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxConfigHandler::cLuxConfigHandler()
{
	mRestart.needed = false;
	mRestart.dialog_shown = false;
}

//-----------------------------------------------------------------------

cLuxConfigHandler::~cLuxConfigHandler()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------

void cLuxConfigHandler::LoadMainConfig()
{
	////////////////////////////////////////////////////////////////////////////////
	// Purpose of this is loading variables that need to be set on engine creation,
	// or set to restart.
	cConfigFile *pCfg = gpBase->mpMainConfig;
	tString sDefaultLang = gpBase->msDefaultGameLanguage;
	OhplGa9MainCfg c;
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
		if(op == eOhplGa9Op_FirstStartFlag) r->i = gpBase->CheckFirstStartFlag();
		else OhplGa9CfgCall(pCfg, op, a, r);
	};
	auto emit = [&](uint32_t tag, const void *p, size_t n)
	{
		switch(tag)
		{
		case eOhplGa9Tag_MainLang: msLangFile = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_MainExt: msScreenShotExt = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_MainRenderer: msRenderer = OhplGa9EmitS(p, n); break;
		case eOhplGa9Tag_MainColor: msColorOutput = OhplGa9EmitS(p, n); break;
		}
	};
	ohpl_ga9_load_main_cfg(&call, OhplGa9CallOf<decltype(call)>::Fn(),
							(const unsigned char*)sDefaultLang.data(), sDefaultLang.size(), &c, OhplGa9EmitOf<decltype(emit)>::Fn(), &emit);

	mbLoadDebugMenu = c.load_debug_menu;
	mbFirstStart = c.first_start;
	mbForceCacheLoadingAndSkipSaving = c.force_cache;
	mvScreenSize.x = c.screen_width;
	mvScreenSize.y = c.screen_height;
	mlDisplay = c.display;
	mbFullscreen = c.fullscreen;
	mbVSync = c.vsync;
	mbAdaptiveVSync = c.adaptive_vsync;
	mbFastPhysicsLoad = c.fast_physics;
	mbFastStaticLoad = c.fast_static;
	mbFastEntityLoad = c.fast_entity;
	mbOcclusionTestLights = c.occlusion_test_lights;
	mbShadowsActive = c.shadows_active;
	mlShadowQuality = c.shadow_quality;
	mlShadowRes = c.shadow_res;
	mlGuiScale = c.gui_scale;
	mfPaperWhite = c.paper_white;
	mfPeakNits = c.peak_nits;
	mbWorldReflection = c.world_reflection;
	mbRefraction = c.refraction;
	mbEdgeSmooth = c.edge_smooth;
	mbSSAOActive = c.ssao_active;
	mlSSAOSamples = c.ssao_samples;
	mlSSAOResolution = c.ssao_resolution;
	mbParallaxEnabled = c.parallax_enabled;
	mlParallaxQuality = c.parallax_quality;
	mlTextureQuality = c.texture_quality;
	mlTextureFilter = c.texture_filter;
	mfTextureAnisotropy = c.texture_anisotropy;
	mbForceShaderModel3And4Off = c.force_sm34_off;
	mlSoundDevID = c.sound_dev_id;
	mlMaxSoundChannels = c.max_sound_channels;
	mlSoundStreamBuffers = c.sound_stream_buffers;
	mlSoundStreamBufferSize = c.sound_stream_buffer_size;
}

//-----------------------------------------------------------------------

void cLuxConfigHandler::SaveMainConfig()
{
	OhplGa9MainCfg c = {};
	c.load_debug_menu = mbLoadDebugMenu;
	c.first_start = mbFirstStart;
	c.force_cache = mbForceCacheLoadingAndSkipSaving;
	c.screen_width = mvScreenSize.x;
	c.screen_height = mvScreenSize.y;
	c.display = mlDisplay;
	c.fullscreen = mbFullscreen;
	c.vsync = mbVSync;
	c.adaptive_vsync = mbAdaptiveVSync;
	c.fast_physics = mbFastPhysicsLoad;
	c.fast_static = mbFastStaticLoad;
	c.fast_entity = mbFastEntityLoad;
	c.occlusion_test_lights = mbOcclusionTestLights;
	c.shadows_active = mbShadowsActive;
	c.shadow_quality = mlShadowQuality;
	c.shadow_res = mlShadowRes;
	c.gui_scale = mlGuiScale;
	c.paper_white = mfPaperWhite;
	c.peak_nits = mfPeakNits;
	c.world_reflection = mbWorldReflection;
	c.refraction = mbRefraction;
	c.edge_smooth = mbEdgeSmooth;
	c.ssao_active = mbSSAOActive;
	c.ssao_samples = mlSSAOSamples;
	c.ssao_resolution = mlSSAOResolution;
	c.parallax_enabled = mbParallaxEnabled;
	c.parallax_quality = mlParallaxQuality;
	c.texture_quality = mlTextureQuality;
	c.texture_filter = mlTextureFilter;
	c.texture_anisotropy = mfTextureAnisotropy;
	c.force_sm34_off = mbForceShaderModel3And4Off;
	c.sound_dev_id = mlSoundDevID;
	c.max_sound_channels = mlMaxSoundChannels;
	c.sound_stream_buffers = mlSoundStreamBuffers;
	c.sound_stream_buffer_size = mlSoundStreamBufferSize;
	c.lang_file = OhplGa9StrOf(msLangFile);
	c.screenshot_ext = OhplGa9StrOf(msScreenShotExt);
	c.renderer = OhplGa9StrOf(msRenderer);
	c.color_output = OhplGa9StrOf(msColorOutput);

	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r)
	{
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
	ohpl_ga9_save_main_cfg(&call, OhplGa9CallOf<decltype(call)>::Fn(), &c);
}

//-----------------------------------------------------------------------

bool cLuxConfigHandler::ShowRestartWarning(cGuiSet* apSet, void* apObject, tGuiCallbackFunc apCallback)
{
	///////////////////////////////////////////////////////////////////////////////////
	// This will show a popup if really needed, once per "restarting" setting changed.
	// Also, will return true if the popup was actually shown.
	if(ohpl_ga9_restart_due(&mRestart))
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

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
