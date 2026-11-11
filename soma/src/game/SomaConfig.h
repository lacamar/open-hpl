#ifndef SOMA_CONFIG_H
#define SOMA_CONFIG_H

#include "hpl.h"

using namespace hpl;

// "soma", "rebirth" or "bunker", from the game's main_init.cfg
const std::string &SomaGameId();
tWString SomaGameIdW();
// 0 soma, 1 rebirth, 2 bunker
int SomaGameIdValue();

class cSomaConfig
{
public:
	void Load();
	void Save();

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

private:
	tWString GetConfigFilePath();
};

#endif // SOMA_CONFIG_H
