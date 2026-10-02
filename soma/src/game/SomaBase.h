#ifndef SOMA_BASE_H
#define SOMA_BASE_H

#include "hpl.h"

#include "DebugFreeCamera.h"
#include "SomaSplash.h"
#include "SomaGammaScreen.h"
#include "SomaConfig.h"
#include "SomaLux.h"
#include "SomaLuxGame.h"
#include "SomaScriptRuntime.h"

using namespace hpl;

class cSomaBase
{
public:
	cSomaBase();
	~cSomaBase();

	bool Init(const tString &asCommandline);
	void Exit();

	void Run();

	double mfGameStartTime = -1;

	void OnSplashFinished();
	void OnGammaScreenFinished();

private:
	bool ParseCommandLine(const tString &asCommandline);

	void SetupLogFile();
	bool InitMainConfig();

	bool InitEngine();

	void ProceedPastBoot();

public:
	// asStartPosName: PlayerStart area, "*" = the first one; avStartPos when not found
	bool LoadMap(const tString &asMapFile, const cVector3f &avStartPos, tString &asErrorOut,
				 const tString &asStartPosName = "");
	std::set<tString> &GetVisitedMaps() { return msetVisitedMaps; }

	cEngine *mpEngine;

	tString msGameName;
	tWString msErrorMessage;

	cCamera* GetDebugCamera(){ return mpDebugCamera; }
	cWorld* GetCurrentWorld(){ return mpTestWorld; }
	cViewport* GetCurrentViewport(){ return mpDebugViewport; }

	cSomaConfig* GetConfig(){ return &mConfig; }

	bool mbScriptGamePaused = false;
	tString GetInitConfigString(const tString &asLevel, const tString &asName);
	void LoadScriptMainMenu();
	bool ScriptsHeld() { return mpSplash && mpSplash->ScriptsMayRun() == false; }
	bool UsesRealPlayer() { return mbUseRealPlayer; }

private:
	tWString msInitConfigFile;

	tString msResourceConfigPath;
	tString msMaterialConfigPath;

	cSomaConfig mConfig;

	cSomaSplash *mpSplash;
	cSomaGammaScreen *mpGammaScreen;

	cWorld *mpTestWorld;
	cCamera *mpDebugCamera;
	cViewport *mpDebugViewport;
	cSomaDebugFreeCamera *mpDebugCameraController;

	// false with OPENHPL_SOMA_FREECAM: the free camera drives the view instead of the player script
	bool mbUseRealPlayer;

	cSomaScriptRuntime *mpScriptRuntime;
	cSomaLuxMap *mpLuxMap;
	cSomaLuxUpdater *mpLuxUpdater;
	cSomaLuxGame *mpLuxGame;
	std::set<tString> msetVisitedMaps;
};

extern cSomaBase *gpSomaBase;

#endif // SOMA_BASE_H
