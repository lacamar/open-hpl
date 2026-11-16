#ifndef SOMA_FSB_H
#define SOMA_FSB_H

#include "hpl.h"
#include "SomaConfig.h"
#include "RustSomaSound.h"

#include <map>
#include <vector>

using namespace hpl;

class cSomaFsb
{
public:
	// $XDG_CACHE_HOME/open-hpl/<game>/<asSubDir>/, created if missing
	static tWString GetCacheDir(const tWString &asSubDir)
	{
		tString sGame = SomaGameId(), sSub = cString::To8Char(asSubDir), sOut;
		ohpl_s7_cache_dir((const unsigned char *)sGame.data(), sGame.size(), (const unsigned char *)sSub.data(), sSub.size(),
						  [](void *c, const unsigned char *p, size_t n) { static_cast<tString *>(c)->assign((const char *)p, n); }, &sOut);
		return cString::To16Char(sOut);
	}

	static void ExtractSamples(const tWString &asBankFile, const tWString &asCacheDir, const tString &asPrefix,
							   const std::vector<tString> &avSamples, std::map<tString, tString> &amapOut)
	{
		tString sBank = cString::To8Char(asBankFile), sCache = cString::To8Char(asCacheDir);
		std::vector<OhplS7Str> vSamples;
		for (const tString &s : avSamples)
			vSamples.push_back({(const unsigned char *)s.data(), s.size()});
		ohpl_s7_extract_samples((const unsigned char *)sBank.data(), sBank.size(), (const unsigned char *)sCache.data(), sCache.size(),
								(const unsigned char *)asPrefix.data(), asPrefix.size(), vSamples.data(), vSamples.size(),
								[](void *c, const unsigned char *k, size_t kn, const unsigned char *f, size_t fn) {
									(*static_cast<std::map<tString, tString> *>(c))[tString((const char *)k, kn)] = tString((const char *)f, fn);
								}, &amapOut);
	}
};

#endif // SOMA_FSB_H
