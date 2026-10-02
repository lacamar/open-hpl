
#ifndef BUNKER_BASE_H
#define BUNKER_BASE_H

#include "hpl.h"

#include "BunkerAreaLoader.h"
#include "DebugFreeCamera.h"

using namespace hpl;

class cBunkerBase
{
public:
	cBunkerBase();
	~cBunkerBase();

	bool Init(const tString &asCommandline);
	void Exit();

	void Run();

private:
	bool ParseCommandLine(const tString &asCommandline);

	bool InitMainConfig();

	bool InitEngine();
	void ExitEngine();

	bool InitTestMap();
	void ExitTestMap();

public:
	cEngine *mpEngine;

	tString msGameName;
	tWString msErrorMessage;

	cCamera* GetDebugCamera(){ return mpDebugCamera; }

private:
	tWString msInitConfigFile;

	tString msResourceConfigPath;
	tString msMaterialConfigPath;

	tString msStartMapFile;
	tString msStartMapPos;

	cWorld *mpTestWorld;
	cCamera *mpDebugCamera;
	cViewport *mpDebugViewport;
	cBunkerDebugFreeCamera *mpDebugCameraController;
};

extern cBunkerBase *gpBunkerBase;

#endif // BUNKER_BASE_H
