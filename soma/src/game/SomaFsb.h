// Format per python-fsb5 (MIT)

#ifndef SOMA_FSB_H
#define SOMA_FSB_H

#include "hpl.h"

#include <map>
#include <vector>

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

	static void ExtractBank(cResources *apResources, const char *apBankPath, const tWString &asCacheDir,
							const cSomaFsbWanted *apWanted, size_t alCount);

	static void ExtractSamples(cResources *apResources, const tString &asBankPath, const tWString &asCacheDir, const tString &asPrefix,
							   const std::vector<tString> &avSamples, std::map<tString, tString> &amapOut);
};

#endif // SOMA_FSB_H
