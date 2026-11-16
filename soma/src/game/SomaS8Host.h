#ifndef SOMA_S8_HOST_H
#define SOMA_S8_HOST_H

#include "hpl.h"
#include "RustSomaS8.h"

using namespace hpl;

class cSomaBase;

bool OhplS8bDump();
void OhplS8bLog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
std::string OhplS8bHex(float afX);
bool OhplS8cDump();
void OhplS8cAdd(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void OhplS8cEnd();

// Engine side of the splash and gamma screen shells: one `call` entry for every host op.
class cOhplS8Shell
{
public:
	cOhplS8Shell(cEngine *apEngine, cSomaBase *apBase);

	OhplS8Host mHost;
	uint64_t mH = 0;
	cEngine *mpEngine;
	cSomaBase *mpBase;

	struct Cb
	{
		cOhplS8Shell *mpShell;
		int mlTag;
	};
	Cb mCb[2];
};

#endif
