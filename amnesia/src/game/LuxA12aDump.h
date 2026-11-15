#ifndef LUX_A12A_DUMP_H
#define LUX_A12A_DUMP_H

#include "LuxBase.h"
#include "RustLuxFxA12a.h"

struct OhplA12aState : OhplA12aFxState
{
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
