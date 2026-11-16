#ifndef LUX_A12D_DUMP_H
#define LUX_A12D_DUMP_H

#include "LuxBase.h"

bool OhplA12dEnabled();
void OhplA12dLog(const char *fmt, ...);
void OhplA12dTick();
void OhplA12dDrawLog(const cVector3f& avPos, const wchar_t *asText);

#endif
