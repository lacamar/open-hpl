
#ifndef AMFP_BASE_H
#define AMFP_BASE_H

#include "hpl.h"


using namespace hpl;

class cAmfpBase
{
public:
	cAmfpBase();
	~cAmfpBase();

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

	cWorld *mpTestWorld;
	cCamera *mpDebugCamera;
	cViewport *mpDebugViewport;
	cDebugFreeCamera *mpDebugCameraController;
};

extern cAmfpBase *gpAmfpBase;

#endif // AMFP_BASE_H
