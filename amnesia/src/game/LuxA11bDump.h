#ifndef LUX_A11B_DUMP_H
#define LUX_A11B_DUMP_H

#include "LuxBase.h"

bool OhplA11Enabled();
void OhplA11Log(const char *fmt, ...);
void OhplA11LogFont(const cVector3f &avPos, const cVector2f &avSize, const cColor &aCol, int alAlign, int alMat, const tWString &asText);
void OhplA11LogGfx(const cVector3f &avPos, const cVector2f &avSize, const cColor &aCol, int alMat);
void OhplA11Tick();

#endif
