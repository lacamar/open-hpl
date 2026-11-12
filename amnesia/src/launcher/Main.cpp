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

#include "launcher.h"
#include "hpl.h"
#include "QualityChooser.h"
#include "RustLauncher.h"

#include "FL/fl_ask.H"

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <stdio.h>

using namespace hpl;

//--------------------------------------------------------------------------------

cEngine* gpEngine = NULL;

tWString gsUserResourceDir;

tString gsDefaultBaseLanguage;
tString gsDefaultGameLanguage;

tString gsBaseLanguageFolder;
tString gsGameLanguageFolder;

tWString gsDefaultMainConfigPath;
tWString gsDefaultMainConfigPathLow;
tWString gsDefaultMainConfigPathMedium;
tWString gsDefaultMainConfigPathHigh;
tWString gsBaseConfigPath;
tWString gsCrashFlagPath;

std::vector<cConfigFile*> gvPresets;

//--------------------------------------------------------------------------------

static tString Rust1(void (*apFn)(const unsigned char*, size_t, OhplLnSink, void*), const tString &asIn)
{
	tString sOut;
	apFn((const unsigned char*)asIn.data(), asIn.size(), OhplLnStrSink, &sOut);
	return sOut;
}

struct cPathPlanCtx
{
	tString msMoveFrom;
	tString msBaseConfig;
	tString msCrashFlag;
	tString msLogFile;
};

static void PathPlanSink(void *apCtx, int alTag, const unsigned char *apData, size_t alLen)
{
	cPathPlanCtx *pCtx = (cPathPlanCtx*)apCtx;
	tString sPath((const char*)apData, alLen);
	switch(alTag)
	{
	case 10:
	{
		tWString sDir = cString::To16Char(sPath);
		if(cPlatform::FolderExists(sDir) == false) cPlatform::CreateFolder(sDir);
		break;
	}
	case 11: cPlatform::CreateFolder(cString::To16Char(sPath)); break;
	case 12: pCtx->msMoveFrom = sPath; break;
	case 13:
		if(cPlatform::FileExists(cString::To16Char(pCtx->msMoveFrom)) && !cPlatform::FileExists(cString::To16Char(sPath)))
			rename(pCtx->msMoveFrom.c_str(), sPath.c_str());
		break;
	case 21: pCtx->msBaseConfig = sPath; break;
	case 22: pCtx->msCrashFlag = sPath; break;
	case 23: pCtx->msLogFile = sPath; break;
	}
}

struct cDefaultsLoader
{
	cDefaultsLoader(const tString &asFile) : msFile(asFile), mpFile(NULL), mpCfg(NULL) {}
	~cDefaultsLoader()
	{
		delete mpCfg;
		if(mpFile) hplDelete(mpFile);
	}

	static const OhplLnCfgOps* Load(void *apCtx)
	{
		cDefaultsLoader *pThis = (cDefaultsLoader*)apCtx;
		pThis->mpFile = hplNew(cConfigFile, (cString::To16Char(pThis->msFile)));
		if(pThis->mpFile->Load() == false) return NULL;
		pThis->mpCfg = new OhplLnCfg(pThis->mpFile);
		return pThis->mpCfg->Ops();
	}

	tString msFile;
	cConfigFile *mpFile;
	OhplLnCfg *mpCfg;
};

bool InitPaths(const tString& asInitConfigFile, const tString &asDefaultInitConfigFile)
{
	tWString sPersonalDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGDataHome);
	tString sPersonal = cString::To8Char(sPersonalDir);

	tString sUserDir = Rust1(ohpl_launcher_user_resource_dir, sPersonal);
	gsUserResourceDir = cString::To16Char(sUserDir);

	bool bUserInit = cPlatform::FileExists(gsUserResourceDir + cString::To16Char(asInitConfigFile));
	tString sInitConfigFile;
	ohpl_launcher_init_cfg_path((const unsigned char*)sUserDir.data(), sUserDir.size(),
								(const unsigned char*)asInitConfigFile.data(), asInitConfigFile.size(), bUserInit,
								OhplLnStrSink, &sInitConfigFile);

	cConfigFile *pInitCfg = hplNew(cConfigFile, (cString::To16Char(sInitConfigFile)));
	if(pInitCfg->Load()==false){
		fl_message("%s", Rust1(ohpl_launcher_message_init_load, asInitConfigFile).c_str());
		return false;
	}

	OhplLnTagged init;
	cDefaultsLoader defaults(asDefaultInitConfigFile);
	OhplLnCfg initOps(pInitCfg);
	int lQuit = ohpl_launcher_read_init(initOps.Ops(), (const unsigned char*)asInitConfigFile.data(), asInitConfigFile.size(),
										(const unsigned char*)asDefaultInitConfigFile.data(), asDefaultInitConfigFile.size(),
										cDefaultsLoader::Load, &defaults, OhplLnTagged::Sink, &init);
	hplDelete(pInitCfg);

	for(size_t i = 0; i < init.mvItems.size(); ++i)
		if(init.mvItems[i].first == 100) fl_message("%s", init.mvItems[i].second.c_str());
	if(lQuit) return false;

	tString sSaveFolder = init.Get(1);
	gsDefaultMainConfigPath = cString::To16Char(init.Get(2));
	gsDefaultMainConfigPathLow = cString::To16Char(init.Get(3));
	gsDefaultMainConfigPathMedium = cString::To16Char(init.Get(4));
	gsDefaultMainConfigPathHigh = cString::To16Char(init.Get(5));
	gsDefaultBaseLanguage = init.Get(6);
	gsBaseLanguageFolder = init.Get(7);
	gsGameLanguageFolder = init.Get(8);
	tString sCustomStories = init.Get(9);

	cPathPlanCtx plan;
	tString sConfigHome = cString::To8Char(cPlatform::GetSystemSpecialPath(eSystemPath_XDGConfigHome));
	tString sStateHome = cString::To8Char(cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome));
	ohpl_launcher_plan_paths(OhplLnS(sPersonal), OhplLnS(sConfigHome), OhplLnS(sStateHome), OhplLnS(sSaveFolder),
							 OhplLnS(sCustomStories), PathPlanSink, &plan);

	gsBaseConfigPath = cString::To16Char(plan.msBaseConfig);
	gsCrashFlagPath = cString::To16Char(plan.msCrashFlag);

	SetLogFile(cString::To16Char(plan.msLogFile));
	return true;
}

//--------------------------------------------------------------------------------

cConfigFile* LoadConfigFile(const tWString& asDefaultPath, const tWString& asWantedPath, bool abForceLoadDefault)
{
	cConfigFile *pConfig;
	bool bLoadedWantedPath = ohpl_launcher_use_wanted_config(abForceLoadDefault, cPlatform::FileExists(asWantedPath)) != 0;

	if(bLoadedWantedPath)
	{
		pConfig = hplNew( cConfigFile, (asWantedPath) );
	}
	else
	{
		pConfig = hplNew( cConfigFile, (asDefaultPath, gsUserResourceDir) );
	}

	if(pConfig->Load()==false)
	{
		tString sMsg;
		if(ohpl_launcher_config_load_message(bLoadedWantedPath, asDefaultPath.empty(), OhplLnStrSink, &sMsg))
			fl_message("%s", sMsg.c_str());
		return NULL;
	}

	if(bLoadedWantedPath==false)
	{
		pConfig->SetFileLocation(asWantedPath);
	}

	return pConfig;
}

//--------------------------------------------------------------------------------

bool LoadLanguage(cEngine* apEngine, const tString& asName, bool abForceReload)
{
	cResources *pResources = apEngine->GetResources();
	pResources->ClearTranslations();

	OhplLnTagged plan;
	ohpl_launcher_language_plan(OhplLnS(asName), OhplLnS(gsBaseLanguageFolder), OhplLnS(gsGameLanguageFolder),
								OhplLnS(gsDefaultBaseLanguage), OhplLnS(gsDefaultGameLanguage), OhplLnTagged::Sink, &plan);

	for(size_t i = 0; i < plan.mvItems.size(); ++i)
	{
		if(plan.mvItems[i].first == 1)
			pResources->AddLanguageFile(plan.mvItems[i].second, true);
		else
			pResources->AddLanguageFile(plan.mvItems[i].second, false, gsUserResourceDir);
	}

	return true;
}

tString SDL2GetRenderer() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    SDL_Window* sdlwin = SDL_CreateWindow("GL Info",0, 0, 50, 50, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GLContext sdlctx = SDL_GL_CreateContext(sdlwin);
    tString ret;
    const char* pCardString = (const char*)glGetString(GL_RENDERER);
    if (pCardString)
    {
        ret = pCardString;
    }
    SDL_GL_DeleteContext(sdlctx);
    SDL_DestroyWindow(sdlwin);
    return ret;
}

//--------------------------------------------------------------------------------

int hplMain(const tString &asCommandLine)
{
	// the command line is ignored (it broke Epic Games launches)
	tString sParams;

	OhplLnTagged names;
	ohpl_launcher_init_cfg((const unsigned char*)sParams.data(), sParams.size(), OhplLnTagged::Sink, &names);

	if(InitPaths(names.Get(0), names.Get(1))==false)
		return -1;

	cQualityChooser* pChooser = hplNew(cQualityChooser,("launcher/launcher_card_database.cfg"));

    // FLTK loop never polls SDL_QUIT, so SDL's handler would swallow SIGTERM
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    tString sCardString = SDL2GetRenderer();

	tString sConfigFilePath8;
	tString sBaseConfig8 = cString::To8Char(gsBaseConfigPath);
	ohpl_launcher_settings_path((const unsigned char*)sBaseConfig8.data(), sBaseConfig8.size(), OhplLnStrSink, &sConfigFilePath8);
	tWString sConfigFilePath = cString::To16Char(sConfigFilePath8);

	bool bConfigFileExists = cPlatform::FileExists(sConfigFilePath);
	bool bLastInitCrashed = cPlatform::FileExists(gsCrashFlagPath);

	cConfigFile* pMainConfig = LoadConfigFile(gsDefaultMainConfigPath, sConfigFilePath, bLastInitCrashed);
	if(pMainConfig==NULL) return -1;

	OhplLnCfg mainOps(pMainConfig);

	cEngineInitVars vars;
	vars.mSound.mlSoundDeviceID = ohpl_launcher_sound_device(mainOps.Ops());

	gpEngine = CreateHPLEngine(eHplAPI_OpenGL, eHplSetup_Video, &vars);

	gvPresets.push_back(hplNew(cConfigFile, (gsDefaultMainConfigPathLow)));
	gvPresets.push_back(hplNew(cConfigFile, (gsDefaultMainConfigPathMedium)));
	gvPresets.push_back(hplNew(cConfigFile, (gsDefaultMainConfigPathHigh)));
	gvPresets.push_back(NULL);

	for(int i=0;i<(int)gvPresets.size();++i)
	{
		if(gvPresets[i]) gvPresets[i]->Load();
	}

	int exitflag = -1;
	bool bShowLauncher = ohpl_launcher_show_launcher(mainOps.Ops()) != 0;

	static tString sWindowClass;
	ohpl_launcher_window_class(OhplLnStrSink, &sWindowClass);
	Fl_Window::default_xclass(sWindowClass.c_str());

	if(bShowLauncher)
	{
		while(exitflag==-1)
		{
			tString sLanguage;
			ohpl_launcher_start_language(mainOps.Ops(), OhplLnStrSink, &sLanguage);
			LoadLanguage(gpEngine, sLanguage, true);
			cUserInterface* win = hplNew(cUserInterface,(exitflag, pMainConfig, bConfigFileExists, bLastInitCrashed, sCardString, pChooser, gvPresets, gpEngine ));

			win->Run();

			hplDelete(win);
		}
	}

	tString sDataDir = cPlatform::GetDataDir();
	tString sGameExe8;
	ohpl_launcher_game_exe((const unsigned char*)sDataDir.data(), sDataDir.size(), OhplLnStrSink, &sGameExe8);

	tString sArgs;
	if(ohpl_launcher_run_plan(exitflag, (const unsigned char*)sParams.data(), sParams.size(), OhplLnStrSink, &sArgs))
		cPlatform::RunProgram(cString::To16Char(sGameExe8), cString::To16Char(sArgs));

	DestroyHPLEngine(gpEngine);

	hplDelete(pMainConfig);
	hplDelete(pChooser);

	return 0;
}
