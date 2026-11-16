#ifndef SOMA_LUX_GAME_H
#define SOMA_LUX_GAME_H

#include "SomaLuxScriptable.h"

class cSomaLuxMap;

class cSomaLuxModule : public cSomaLuxScriptable
{
public:
	tString msName;
	int mlId = -1;
};

class cSomaLuxEffect : public cSomaLuxScriptable
{
public:
	tString msName;
	int mlId = -1;
};

class cSomaLuxHandler : public cSomaLuxScriptable
{
public:
	tString msName;
};

// Rust owns the handler, module and effect lists (rust/src/somalux/s5c_game.rs); this is the handle
class cSomaLuxGame
{
public:
	explicit cSomaLuxGame(cSomaScriptRuntime *apRuntime);
	~cSomaLuxGame();

	void Load() { ohpl_s5c_game_load(); }

	void Update(float afTimeStep, bool abPaused = false) { ohpl_s5c_game_update(afTimeStep, abPaused); }
	void PostUpdate(float afTimeStep) { ohpl_s5c_game_post_update(afTimeStep); }
	void ResetScriptables() { ohpl_s5c_game_reset_scriptables(); }
	void ReloadUserConfig() { ohpl_s5c_game_reload_user_config(); }
	void Draw(float afFrameTime) { ohpl_s5c_game_draw(afFrameTime); }
	void PreloadData(cSomaLuxMap *apMap) { ohpl_s5c_game_preload(apMap); }
	void EnterMap(cSomaLuxMap *apMap) { ohpl_s5c_game_enter_map(apMap); }
	void ModulesMapEnter(cSomaLuxMap *apMap) { ohpl_s5c_game_modules_map_enter(apMap); }
	void LeaveMap(cSomaLuxMap *apMap) { ohpl_s5c_game_leave_map(apMap); }

	// cLuxBase::BroadcastInputAction/BroadcastInputAnalog
	void BroadcastAction(int alAction, bool abPressed) { ohpl_s5c_game_broadcast_action(alAction, abPressed); }
	void BroadcastAnalog(int alAnalogId, const cVector3f &avAmount) { ohpl_s5c_game_broadcast_analog(alAnalogId, avAmount.x, avAmount.y, avAmount.z); }
	// Off while a menu or pause screen has the input
	void SetGameInput(bool abX) { ohpl_s5c_game_set_input(abX); }
	cVector2l MouseRel()
	{
		cVector2l v;
		ohpl_s5c_game_mouse_rel(&v.x, &v.y);
		return v;
	}

	cSomaLuxModule *GetModule(int alId) { return static_cast<cSomaLuxModule *>((cSomaLuxScriptable *)ohpl_s5c_game_module_by_id(alId)); }
	cSomaLuxModule *GetModule(const tString &asName)
	{
		return static_cast<cSomaLuxModule *>((cSomaLuxScriptable *)ohpl_s5c_game_module_by_name((const unsigned char *)asName.data(), asName.size()));
	}
	cSomaLuxEffect *GetEffect(int alId) { return static_cast<cSomaLuxEffect *>((cSomaLuxScriptable *)ohpl_s5c_game_effect_by_id(alId)); }
	cSomaLuxHandler *GetHandler(const tString &asName)
	{
		return static_cast<cSomaLuxHandler *>((cSomaLuxScriptable *)ohpl_s5c_game_handler((const unsigned char *)asName.data(), asName.size()));
	}

	// config/EntityTypes.cfg: asGroup is the section (PropTypes, AreaTypes, AgentTypes...)
	struct cEntityScript
	{
		tString msFile;
		tString msClass;
	};
	const cEntityScript *GetEntityScript(const tString &asGroup, const tString &asType);

	static cSomaLuxGame *Get() { return mpInstance; }
	// game.cfg <Prop|Critter DefaultMaxInteractDistance>
	float GetDefaultInteractDistance(int alEntityType) { return ohpl_s5c_game_interact_distance(alEntityType); }
	static void RegisterNatives(asIScriptEngine *apEngine);

private:
	static cSomaLuxGame *mpInstance;
};

cConfigFile *SomaUserConfig();
cConfigFile *SomaGameConfig();
float SomaStringDuration(const tWString &asText);
cConfigFile *SomaKeyConfig();

#endif // SOMA_LUX_GAME_H
