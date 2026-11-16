#ifndef SOMA_CONFIG_H
#define SOMA_CONFIG_H

#include "hpl.h"
#include "RustSomaSave.h"

using namespace hpl;

// "soma", "rebirth" or "bunker", from the game's main_init.cfg
inline int SomaGameIdValue() { return ohpl_gs4_game_index(); }

inline const std::string &SomaGameId()
{
	static const std::string vIds[] = {"soma", "rebirth", "bunker"};
	return vIds[SomaGameIdValue()];
}

inline tWString SomaGameIdW() { return cString::To16Char(SomaGameId()) + _W("/"); }

class cSomaConfig
{
public:
	void Load()
	{
		OhplGs4Config c = {mfMasterVolume, mfGamma, mlScreenWidth, mlScreenHeight, mbVSync, mbFullscreen, mbAntiAliasing, mbShowSubtitles};
		if (ohpl_gs4_cfg_load(&c) == false)
		{
			Log("SOMA: no settings file yet, using defaults\n");
			return;
		}
		mfMasterVolume = c.master_volume;
		mfGamma = c.gamma;
		mbVSync = c.vsync;
		mbFullscreen = c.fullscreen;
		mlScreenWidth = c.screen_width;
		mlScreenHeight = c.screen_height;
		mbAntiAliasing = c.anti_aliasing;
		mbShowSubtitles = c.show_subtitles;
	}

	void Save()
	{
		OhplGs4Config c = {mfMasterVolume, mfGamma, mlScreenWidth, mlScreenHeight, mbVSync, mbFullscreen, mbAntiAliasing, mbShowSubtitles};
		if (ohpl_gs4_cfg_save(&c) == false)
			Log("SOMA: failed to save settings\n");
	}

	float mfMasterVolume = 1.0f;
	float mfGamma = 1.0f;
	bool mbVSync = false;
	bool mbFullscreen = false;
	int mlScreenWidth = 1280;
	int mlScreenHeight = 720;
	// helper_imgui_options.hps default: FXAA
	bool mbAntiAliasing = true;
	bool mbShowSubtitles = true;
	bool mbDevHud = false;
	tString msRenderer = "OpenGL";
	tString msColorOutput = "SDR";
	float mfHdrPaperWhite = 203;
	float mfHdrPeak = 1000;
};

#endif // SOMA_CONFIG_H
