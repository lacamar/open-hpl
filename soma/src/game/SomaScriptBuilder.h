#ifndef SOMA_SCRIPT_BUILDER_H
#define SOMA_SCRIPT_BUILDER_H

#include <angelscript.h>

#include <map>
#include <set>
#include <string>
#include <vector>

struct OhplGs6Builder;

// Members declared [nosave] or [volatile], skipped by save games
bool SomaScriptIsNoSave(asITypeInfo *apType, const char *asProp);

class cSomaScriptBuilder
{
public:
	explicit cSomaScriptBuilder(const std::string &asGameDir, const std::string &asCacheDir = "");
	~cSomaScriptBuilder();
	cSomaScriptBuilder(const cSomaScriptBuilder &) = delete;
	cSomaScriptBuilder &operator=(const cSomaScriptBuilder &) = delete;

	std::string Resolve(const std::string &asInclude, const std::string &asFromFile) const;

	int Build(asIScriptEngine *apEngine, const std::string &asModuleName, const std::string &asEntryFile,
			  std::string *apMissingInclude = NULL);

	const std::string &GetGameDir() const { return msGameDir; }

private:
	bool Collect(std::vector<std::pair<std::string, std::string>> &avSections, const std::string &asFile, std::string *apMissingInclude);

	std::string msGameDir;
	std::string msCacheDir;
	OhplGs6Builder *mpRust;
};

#endif // SOMA_SCRIPT_BUILDER_H
