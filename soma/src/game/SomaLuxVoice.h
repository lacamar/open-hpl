#ifndef SOMA_LUX_VOICE_H
#define SOMA_LUX_VOICE_H

#include "hpl.h"
#include "RustSomaLuxVoice.h"

#include <angelscript.h>
#include <functional>
#include <map>
#include <memory>
#include <set>

using namespace hpl;

class cSomaLuxVoiceHandler : public iUpdateable
{
public:
	explicit cSomaLuxVoiceHandler(cEngine *apEngine);
	~cSomaLuxVoiceHandler();

	static cSomaLuxVoiceHandler *Get() { return mpInstance; }
	cEngine *GetEngine() { return mpEngine; }

	void UpdateVoices(float afTimeStep);
	void OnDraw(float afFrameTime) override { ohpl_s5b_voice_draw(); }
	void LoadMapFile(const tString &asMapName) { ohpl_s5b_voice_load_map_file((const unsigned char *)asMapName.data(), asMapName.size()); }
	void ReapplyPauses() { ohpl_s5b_voice_reapply_pauses(); }
	void FadeTo(float afGoal, float afTime) { ohpl_s5b_voice_fade_to(afGoal, afTime); }

	std::set<tString> PlayedLines();
	void SetPlayedLines(const std::set<tString> &asetLines);
	void ClearPlayedLines() { ohpl_s5b_voice_clear_played(); }

	static void RegisterNatives(asIScriptEngine *apEngine) { ohpl_s5b_register_voice_natives(apEngine); }

	cGuiSet *mpGuiSet = NULL;
	iFontData *mpFont = NULL;
	cGuiGfxElement *mpFadeGfx = NULL;

private:
	static cSomaLuxVoiceHandler *mpInstance;
	cEngine *mpEngine;
};

class cSomaLuxDialogHandler
{
public:
	static cSomaLuxDialogHandler *Get()
	{
		static cSomaLuxDialogHandler handler;
		return &handler;
	}
	void Update(float afTimeStep) { ohpl_s7_dialog_update(afTimeStep); }
	void StopAll() { ohpl_s7_dialog_stop_all(); }
	static void RegisterNatives(asIScriptEngine *apEngine) { ohpl_s7_register_dialog(apEngine); }
};

void SomaMapScriptCall(const tString &asDecl, const std::function<void(asIScriptContext *)> &aSetArgs);

#endif // SOMA_LUX_VOICE_H
