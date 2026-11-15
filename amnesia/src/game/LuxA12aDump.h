#ifndef LUX_A12A_DUMP_H
#define LUX_A12A_DUMP_H

#include "LuxBase.h"
#include "RustLuxFx.h"

struct OhplA12aState
{
	OhplGa9bFade fade;
	OhplGa9bFlash flash, sanity;
	OhplGa9bAmount trail, sepia, radial;
	float radialStart;
	OhplGa9bEmotion emotion;
	uint32_t shakeActive, shakeCount;
	uint32_t voiceActive, voicePaused, voiceCount, voiceRows, voiceQueue, voiceEntries;
	float voiceVol;
	uint32_t commentaryActive;
	int32_t commentaryIcon;
	uint32_t playerPaused;
	float insT, insAnim, insWave, insZoom, insSpeed;
	uint32_t insActive;
};

bool OhplA12Enabled();
void OhplA12Log(const char *fmt, ...);
void OhplA12Tick();
float OhplA12Dt();
bool OhplA12InTick();
int OhplA12Frame();
void OhplA12FillState(OhplA12aState *apState);
std::string OhplA12F(float afX);
std::string OhplA12D(double afX);
std::string OhplA12S(const tString &asText);
std::string OhplA12S(const tWString &asText);

#endif
