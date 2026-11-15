#ifndef LUX_A12B_DUMP_H
#define LUX_A12B_DUMP_H

#include "LuxBase.h"
#include "RustLuxBase.h"

bool OhplA12bEnabled();
void OhplA12bLog(const char *fmt, ...);
void OhplA12bTick();
void OhplA12bLogCall(const char *asTag, uint32_t alOp, const OhplGa9Arg *apArgs, size_t alN);
inline float OhplA12bDt() { return 1.0f / 60.0f; }

#endif
