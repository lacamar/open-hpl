#ifndef SOMA_S9_DUMP_H
#define SOMA_S9_DUMP_H

#include "SomaLux.h"

void OhplS9Log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void OhplS9DumpEnter(cSomaLuxMap *apMap);

#endif
