#ifndef LUX_A12B_DUMP_H
#define LUX_A12B_DUMP_H

#include "LuxBase.h"
#include "RustLuxBase.h"

bool OhplA12Enabled();
void OhplA12Log(const char *fmt, ...);
void OhplA12Tick();
void OhplA12LogCall(const char *asTag, uint32_t alOp, const OhplGa9Arg *apArgs, size_t alN);
inline float OhplA12Dt() { return 1.0f / 60.0f; }

#endif
