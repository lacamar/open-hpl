#ifndef LUX_A11C_DUMP_H
#define LUX_A11C_DUMP_H

#include "LuxBase.h"

bool OhplA11cOn();
void OhplA11cTick();
inline float OhplA11cDt() { return 1.0f / 60.0f; }

#endif
