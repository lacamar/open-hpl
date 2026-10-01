/*
 * The script-driven parts of SOMA's game layer: user modules (config/Modules.cfg), effects
 * (config/Effects.cfg) and the game.cfg handlers/player, created once, driven through the
 * iLuxUpdateable_ScriptInterface callbacks in the official order (cLuxMapHandler::SetCurrentMap).
 */

#ifndef SOMA_LUX_GAME_H
#define SOMA_LUX_GAME_H

#include "SomaLuxScriptable.h"

#include <map>

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
	bool mbActive = false;
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

	void Update(float afTimeStep, bool abPaused = false);
	void ResetScriptables();
	void UpdateGui(float afTimeStep);
	void Draw(float afFrameTime);
	void PreloadData(cSomaLuxMap *apMap);
	void EnterMap(cSomaLuxMap *apMap);
	void LeaveMap(cSomaLuxMap *apMap);

	// cLuxBase::BroadcastInputAction/BroadcastInputAnalog
	void BroadcastAction(int alAction, bool abPressed);
	void BroadcastAnalog(int alAnalogId, const cVector3f &avAmount);
	// Off while a menu or pause screen has the input
	bool mbGameInput = true;
	cVector2l mvMouseRel = 0;

	cSomaLuxModule *GetModule(int alId);
	cSomaLuxModule *GetModule(const tString &asName);
	cSomaLuxEffect *GetEffect(int alId);
	cSomaLuxHandler *GetHandler(const tString &asName);

	// config/EntityTypes.cfg: asGroup is the section (PropTypes, AreaTypes, AgentTypes...)
	struct cEntityScript
	{
		tString msFile;
		tString msClass;
	};
	const cEntityScript *GetEntityScript(const tString &asGroup, const tString &asType);

	static cSomaLuxGame *Get() { return mpInstance; }
	// game.cfg <Prop|Critter DefaultMaxInteractDistance>
	float GetDefaultInteractDistance(int alEntityType) { return alEntityType == 4 ? mfCritterInteractDistance : mfPropInteractDistance; }
	static void RegisterNatives(asIScriptEngine *apEngine);

private:
	template <class F> void ForEach(F aFunc, bool abActiveEffectsOnly = false);

	static cSomaLuxGame *mpInstance;

	cSomaScriptRuntime *mpRuntime;
	std::vector<cSomaLuxHandler *> mvHandlers;
	std::vector<cSomaLuxModule *> mvModules;
	std::vector<cSomaLuxEffect *> mvEffects;
	std::map<tString, cEntityScript> mmapEntityScripts;
	bool mbStarted = false;
	float mfPropInteractDistance = 2;
	float mfCritterInteractDistance = 2;
};

cConfigFile *SomaUserConfig();
cConfigFile *SomaKeyConfig();

#endif // SOMA_LUX_GAME_H
