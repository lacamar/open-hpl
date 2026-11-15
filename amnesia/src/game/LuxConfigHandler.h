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

#ifndef LUX_CONFIG_HANDLER_H
#define LUX_CONFIG_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"
#include "RustLuxFxA12b.h"

//----------------------------------------------

// The settings live in the Rust config handler; the members are views of its numbers.
class cLuxConfigHandler
{
public:	
	cLuxConfigHandler();
	~cLuxConfigHandler() { ohpl_a12b_cfg_free(mlH); }

	void LoadMainConfig();
	void SaveMainConfig();

	bool ShowRestartWarning(cGuiSet* apSet, void* apObject, tGuiCallbackFunc apCallback);

	void SetGameNeedsRestart() { ohpl_a12b_cfg_restart(mlH, 0); }

	tString LangFile(){ return Str(0); }
	void SetLangFile(const tString &asX){ SetStr(0, asX); }
	tString ScreenShotExt(){ return Str(1); }
	tString Renderer(){ return Str(2); }
	void SetRenderer(const tString &asX){ SetStr(2, asX); }
	tString ColorOutput(){ return Str(3); }
	void SetColorOutput(const tString &asX){ SetStr(3, asX); }

private:
	static void PutStr(void *apCtx, const unsigned char *apP, size_t alN) { ((tString*)apCtx)->assign((const char*)apP, alN); }
	tString Str(int alWhich) { tString s; ohpl_a12b_cfg_str(mlH, alWhich, PutStr, &s); return s; }
	void SetStr(int alWhich, const tString &asX) { ohpl_a12b_cfg_set_str(mlH, alWhich, asX.data(), asX.size()); }

	uint64_t mlH;
	OhplA12bMainNums *mpN;

public:
	bool &mbLoadDebugMenu;
	bool &mbFirstStart;
	bool &mbCreateAndLoadCompressedMaps;
	bool &mbForceCacheLoadingAndSkipSaving;

	cVector2l &mvScreenSize;
    int &mlDisplay;
	bool &mbFullscreen;
	bool &mbVSync;
	bool &mbAdaptiveVSync;
	int &mlTextureQuality;
	int &mlTextureFilter;
	float &mfTextureAnisotropy;
	int &mlShadowQuality;
	int &mlShadowRes;

	int &mlGuiScale;

	float &mfPaperWhite;
	float &mfPeakNits;

	bool &mbSSAOActive;
	int &mlSSAOSamples;
	int &mlSSAOResolution; //0= medium(div2), 1=high (same as screen resolution)
	
	int &mlParallaxQuality;
	bool &mbParallaxEnabled;

	bool &mbOcclusionTestLights;
	
	bool &mbEdgeSmooth;
		
	bool &mbWorldReflection;
	bool &mbRefraction;
	bool &mbShadowsActive;

	bool &mbForceShaderModel3And4Off;

	bool &mbFastPhysicsLoad;
	bool &mbFastStaticLoad;
	bool &mbFastEntityLoad;

	int &mlSoundDevID;
	int &mlMaxSoundChannels;
	int &mlSoundStreamBuffers;
	int &mlSoundStreamBufferSize;
};

//----------------------------------------------


#endif // LUX_DEBUG_HANDLER_H
