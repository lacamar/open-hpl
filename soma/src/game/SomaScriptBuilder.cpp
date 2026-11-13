#include "SomaScriptBuilder.h"
#include "RustSomaScript.h"
#include "RustSomaScriptS2.h"
#include "SomaConfig.h"
#include "system/LowLevelSystem.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>

using namespace OhplGs6;

bool SomaScriptIsNoSave(asITypeInfo *apType, const char *asProp)
{
	for (; apType; apType = apType->GetBaseType())
	{
		std::string sKey = std::string(apType->GetName()) + "::" + asProp;
		if (ohpl_gs6_is_nosave(U(sKey), sKey.size()))
			return true;
	}
	return false;
}

struct cByteStream : asIBinaryStream
{
	std::string msData;
	size_t mlPos = 0;
	int Write(const void *apData, asUINT alSize) override
	{
		msData.append((const char *)apData, alSize);
		return 0;
	}
	int Read(void *apData, asUINT alSize) override
	{
		if (mlPos + alSize > msData.size())
			return -1;
		memcpy(apData, msData.data() + mlPos, alSize);
		mlPos += alSize;
		return 0;
	}
};

cSomaScriptBuilder::cSomaScriptBuilder(const std::string &asGameDir, const std::string &asCacheDir)
	: msGameDir(asGameDir), msCacheDir(asCacheDir)
{
	const std::string &sGame = SomaGameId();
	mpRust = ohpl_gs6_builder_new(U(asGameDir), asGameDir.size(), U(sGame), sGame.size());
}

cSomaScriptBuilder::~cSomaScriptBuilder()
{
	ohpl_gs6_builder_free(mpRust);
}

std::string cSomaScriptBuilder::Resolve(const std::string &asInclude, const std::string &asFromFile) const
{
	return Call8([&](OhplGs6Sink8 s, void *c) { ohpl_gs6_builder_resolve(mpRust, U(asInclude), asInclude.size(), U(asFromFile), asFromFile.size(), s, c); });
}

namespace
{
	struct cCollected
	{
		std::vector<std::pair<std::string, std::string>> *mpSections;
		std::string msMissing;
		std::vector<std::string> mvWarnings;
	};
}

bool cSomaScriptBuilder::Collect(std::vector<std::pair<std::string, std::string>> &avSections, const std::string &asFile, std::string *apMissingInclude)
{
	cCollected out{&avSections, "", {}};
	bool bOk = ohpl_gs6_builder_collect(
		mpRust, U(asFile), asFile.size(),
		+[](void *c, const unsigned char *n, size_t ln, const unsigned char *code, size_t lc) {
			((cCollected *)c)->mpSections->emplace_back(std::string((const char *)n, ln), std::string((const char *)code, lc));
		},
		+[](void *c, const unsigned char *p, size_t n) { ((cCollected *)c)->msMissing.assign((const char *)p, n); },
		+[](void *c, const unsigned char *p, size_t n) { ((cCollected *)c)->mvWarnings.emplace_back((const char *)p, n); }, &out);
	if (bOk == false)
		return false;
	for (const std::string &sWarning : out.mvWarnings)
		Warning("%s", sWarning.c_str());
	if (apMissingInclude && apMissingInclude->empty())
		*apMissingInclude = out.msMissing;
	return true;
}

int cSomaScriptBuilder::Build(asIScriptEngine *apEngine, const std::string &asModuleName, const std::string &asEntryFile,
							  std::string *apMissingInclude)
{
	std::string sMissing;
	OhplS2InstallHost();
	int r = ohpl_s2_builder_build(mpRust, U(msCacheDir), msCacheDir.size(), apEngine, U(asModuleName), asModuleName.size(), U(asEntryFile), asEntryFile.size(),
								  +[](void *c, const unsigned char *p, size_t n) { ((std::string *)c)->assign((const char *)p, n); }, &sMissing);
	if (apMissingInclude && apMissingInclude->empty())
		*apMissingInclude = sMissing;
	return r;
}
