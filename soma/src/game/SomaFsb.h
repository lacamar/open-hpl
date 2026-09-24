/*
 * FMOD FSB5 bank extraction: named samples -> plain .ogg (Vorbis banks) or .wav (PCM16 banks)
 * in a per-user cache dir. Format per python-fsb5 (MIT), https://github.com/HearthSim/python-fsb5.
 */

#ifndef SOMA_FSB_H
#define SOMA_FSB_H

#include "hpl.h"

using namespace hpl;

struct cSomaFsbWanted
{
	const char *pSampleName;
	const char *pCacheFile;
};

class cSomaFsb
{
public:
	// $XDG_CACHE_HOME/open-hpl/soma/<asSubDir>/, created if missing
	static tWString GetCacheDir(const tWString &asSubDir);

	// apBankPath is resolved through the resource file searcher
	static void ExtractBank(cResources *apResources, const char *apBankPath, const tWString &asCacheDir,
							const cSomaFsbWanted *apWanted, size_t alCount);

	static bool WriteTextFile(const tWString &asPath, const tString &asText);
};

#endif // SOMA_FSB_H
