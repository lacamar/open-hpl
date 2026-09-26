/*
 * The script-driven parts of SOMA's game layer: user modules (config/Modules.cfg), effects
 * (config/Effects.cfg) and the game.cfg handlers/player, created once, driven through the
 * iLuxUpdateable_ScriptInterface callbacks in the official order (cLuxMapHandler::SetCurrentMap).
 */

#ifndef SOMA_LUX_GAME_H
#define SOMA_LUX_GAME_H

#include "SomaLuxScriptable.h"

class cSomaLuxMap;

class cSomaLuxModule : public cSomaLuxScriptable
{
public:
	tString msName;
	tString msContainer;
	int mlId = -1;
	bool mbGlobal = false;
};

class cSomaLuxEffect : public cSomaLuxScriptable
{
public:
	tString msName;
	int mlId = -1;
	bool mbActive = true;
};

class cSomaLuxHandler : public cSomaLuxScriptable
{
public:
	tString msName;
	tString msBaseType;
};

class cSomaLuxGame
{
public:
	explicit cSomaLuxGame(cSomaScriptRuntime *apRuntime);
	~cSomaLuxGame();

	void Load();

	void Update(float afTimeStep);
	void PreloadData(cSomaLuxMap *apMap);
	void EnterMap(cSomaLuxMap *apMap);
	void LeaveMap(cSomaLuxMap *apMap);

	cSomaLuxModule *GetModule(int alId);
	cSomaLuxModule *GetModule(const tString &asName);
	cSomaLuxEffect *GetEffect(int alId);
	cSomaLuxHandler *GetHandler(const tString &asName);

	static cSomaLuxGame *Get() { return mpInstance; }
	static void RegisterNatives(asIScriptEngine *apEngine);

private:
	template <class F> void ForEach(F aFunc);

	static cSomaLuxGame *mpInstance;

	cSomaScriptRuntime *mpRuntime;
	std::vector<cSomaLuxHandler *> mvHandlers;
	std::vector<cSomaLuxModule *> mvModules;
	std::vector<cSomaLuxEffect *> mvEffects;
	bool mbStarted = false;
};

#endif // SOMA_LUX_GAME_H
