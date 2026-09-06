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

	// Screen - real keys "Screen"/"Width"+"Height" (real
	// GuiOptionsVideoDisplay()'s Resolution row - see MenuHandler.hps'
	// GetCurrentResolution()/SetCurrentResolution()). Same restart-required
	// contract as mbFullscreen above: cLowLevelGraphics has no live
	// window-resize/mode-switch call either, only cLowLevelGraphics::Init()'s
	// screen-size param - evaluated once at window-creation time in
	// cSomaBase::InitEngine(). The Options screen's Resolution row builds its
	// value list from cPlatform::GetAvailableVideoModes(), same real API
	// amnesia/src/game/LuxMainMenu_Options.cpp's own Resolution dropdown
	// uses.
	int mlScreenWidth;
	int mlScreenHeight;

	// Graphics - LIVE via cViewport::GetRenderSettings()->mbUseEdgeSmooth
	// (HPL2/core/include/graphics/Renderer.h's cRenderSettings, consumed
	// every frame by RendererDeferred.cpp's RenderEdgeSmooth() pass) - the
	// exact same FXAA-style edge-smoothing setting
	// amnesia/src/game/LuxConfigHandler.cpp's own "EdgeSmooth" field drives
	// via cLuxMapHandler::UpdateViewportRenderProperties(). Real SOMA only
	// ever offers "Off"/"FXAA" (helper_imgui_options.hps' vAAValues), so this
	// engine's single edge-smooth pass covers the full real option range -
	// stored as a bool rather than a string, same simplification already
	// applied to mbFullscreen/mbVSync above.
	bool mbAntiAliasing;

	// Gameplay - LIVE via cCamera::SetFOV() (radians) - real key
	// "Gameplay"/"FOV", real range 50-83 (see MenuHandler.hps'
	// GuiOptionsVideoDisplay()'s FOV row, which treats this as a
	// vertical-ish FOV and derives a horizontal display figure from it via
	// the real screen aspect ratio - see SomaMainMenu.cpp's own copy of that
	// formula). Applied every cSomaPlayer::Update() (cCamera::SetFOV() is a
	// cheap early-return-if-unchanged call, see Camera.cpp), so a change
	// here takes effect on the very next frame once a real player camera
	// exists.
	float mfFOV;

	// Sound - real key "Sound"/"ShowSubtitles" - gates
	// cSomaIntroSequence::DrawSubtitle() (see SomaIntroSequence.cpp), this
	// engine's only subtitle-rendering content so far. Real default true.
	bool mbShowSubtitles;

	// Input - real key "Input"/"MouseSensitivity" (real range ~0.01-4.01,
	// see MenuHandler.hps' GuiOptionsInputMouse()'s MouseSens slider) -
	// multiplies cSomaPlayer's own fixed base mouse-look constant live, every
	// frame (see SomaPlayer.cpp's mfMouseSensitivity comment).
	float mfMouseSensitivity;

	// Input - real key "Input"/"InvertMouse" - flips the pitch (vertical
	// look) delta's sign in cSomaPlayer::Update(). Real default false.
	bool mbInvertMouseY;

	// Input - keybindings for cSomaBase::eSomaPlayerAction's 5 movement/jump
	// actions (see SomaBase.h/.cpp's CreateInputActions()/RebindPlayerAction()) -
	// real HPL2 cAction/cInput system, same one
	// amnesia/src/game/LuxInputHandler.cpp's own action table uses, persisted
	// via the same real iKeyboard::KeyToString()/StringToKey() round trip
	// LuxInputHandler.cpp uses for its own keybind config. Real SOMA's own
	// key-config file format is far more elaborate (per-action primary AND
	// secondary binds, gamepad, many more actions than this scaffold's
	// player has) - this is a deliberately simplified "one key per action"
	// version covering just Forward/Backward/Left/Right/Jump.
	tString msKeyForward;
	tString msKeyBackward;
	tString msKeyLeft;
	tString msKeyRight;
	tString msKeyJump;

private:
	tWString GetConfigFilePath();
};

//----------------------------------------------

#endif // SOMA_CONFIG_H
