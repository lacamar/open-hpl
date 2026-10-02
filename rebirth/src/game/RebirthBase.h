
#ifndef REBIRTH_BASE_H
#define REBIRTH_BASE_H

#include "hpl.h"

#include "DebugFreeCamera.h"

using namespace hpl;

class cRebirthBase
{
public:
	cRebirthBase();
	~cRebirthBase();

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
	cRebirthDebugFreeCamera *mpDebugCameraController;
};

extern cRebirthBase *gpRebirthBase;

#endif // REBIRTH_BASE_H
