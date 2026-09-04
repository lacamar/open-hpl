/*
 * Minimal persisted settings for the SOMA Phase 0/1 scaffold, backing the
 * real Options screen (see SomaMainMenu.{h,cpp}'s eSomaMenuScreen_Options*
 * states). Mirrors amnesia/src/game/LuxConfigHandler.{h,cpp}'s shape
 * (Load/Save against a cConfigFile) but trimmed to only the handful of
 * settings this engine actually has a live, working backend for - see
 * SomaMainMenu.cpp's Options screen comment for which real SOMA
 * script/modules/MenuHandler.hps + helper_imgui_options.hps settings were
 * deliberately left out because nothing in this engine implements them yet.
 *
 * Unlike Dark Descent (which keeps separate main_settings.cfg/
 * user_settings.cfg files, one per save profile), this scaffold has no
 * save-profile concept at all, so everything lives in one file. Stored
 * under $XDG_CONFIG_HOME (see PlatformXdgPathTests.cpp / SomaBase.cpp's own
 * XDG usage for hpl.log) - config, not state/cache, per the XDG Base
 * Directory spec, and specifically NOT under the real Steam install
 * directory this binary may be deployed into.
 */

#ifndef SOMA_CONFIG_H
#define SOMA_CONFIG_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cSomaConfig
{
public:
	cSomaConfig();

	// Reads main_settings.cfg if it exists, leaving the (already-sane)
	// default values below untouched for any field it doesn't find - so a
	// fresh install with no config file yet behaves exactly like the
	// hardcoded defaults SomaBase.cpp used before this class existed.
	void Load();

	// Writes main_settings.cfg, creating $XDG_CONFIG_HOME/open-hpl/soma/ if
	// needed. Called immediately whenever a value changes from the Options
	// screen (not batched behind a separate "Apply"/"Save" step) - cheap
	// enough for a handful of scalar fields, and means an alt-F4 out of the
	// menu never loses a change.
	void Save();

	// Sound - live via cSound's iLowLevelSound::SetVolume()/GetVolume(),
	// same API amnesia/src/game/LuxMainMenu_Options.cpp's master volume
	// slider uses (gpBase->mpEngine->GetSound()->GetLowLevel()->SetVolume).
	float mfMasterVolume;

	// Graphics - live via iLowLevelGraphics::SetGammaCorrection()/
	// GetGammaCorrection(), same range (0.3-2.0) and backend
	// cSomaGammaScreen's first-boot calibration slider already uses.
	float mfGamma;

	// Graphics - live via iLowLevelGraphics::SetVsyncActive(bool, false).
	// No GetVsyncActive() exists on the interface, so this class is the
	// only source of truth for the current value (not read back from the
	// engine).
	bool mbVSync;

	// Screen - NOT live: iLowLevelGraphics has no runtime "become
	// fullscreen"/"become windowed" call (only cLowLevelGraphics::Init()'s
	// abFullscreen parameter, evaluated once at window-creation time in
	// cSomaBase::InitEngine()). Persisted here and read back at the start
	// of the next InitEngine() call, same "changes apply after a restart"
	// contract amnesia/src/game/LuxMainMenu_Options.cpp's own Fullscreen
	// checkbox has (see its ApplyChanges()/ShowRestartWarning()) - a real,
	// working setting, just not an instant one.
	bool mbFullscreen;

private:
	tWString GetConfigFilePath();
};

//----------------------------------------------

#endif // SOMA_CONFIG_H
