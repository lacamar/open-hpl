#ifndef SOMA_AGENT_DUMP_H
#define SOMA_AGENT_DUMP_H

class cSomaLuxEntity;

bool OhplS4bOn();
void OhplS4bBegin(cSomaLuxEntity *apEnt);
void OhplS4bEnd(cSomaLuxEntity *apEnt);
void OhplS4bLog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
