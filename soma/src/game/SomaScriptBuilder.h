#ifndef SOMA_SCRIPT_BUILDER_H
#define SOMA_SCRIPT_BUILDER_H

#include <angelscript.h>

#include <map>
#include <set>
#include <string>
#include <vector>

// Members declared [nosave] or [volatile], skipped by save games
const std::set<std::string> &SomaScriptNoSaveNames();

class cSomaScriptBuilder
{
public:
	explicit cSomaScriptBuilder(const std::string &asGameDir);

	std::string Resolve(const std::string &asInclude, const std::string &asFromFile) const;

	int Build(asIScriptEngine *apEngine, const std::string &asModuleName, const std::string &asEntryFile,
			  std::string *apMissingInclude = NULL);

	const std::string &GetGameDir() const { return msGameDir; }

private:
	bool AddFile(asIScriptModule *apModule, const std::string &asFile, std::map<std::string, bool> &aIncluded,
				 std::string *apMissingInclude);

	std::string msGameDir;
	std::map<std::string, std::string> mmapByRelPath;  // lowercase path under script/
	std::map<std::string, std::string> mmapByBaseName; // lowercase basename
};

#endif // SOMA_SCRIPT_BUILDER_H
