#ifndef SOMA_SCRIPT_BUILDER_H
#define SOMA_SCRIPT_BUILDER_H

#include <angelscript.h>

#include <map>
#include <set>
#include <string>
#include <vector>

struct cSomaScriptMetadata
{
	std::string msFile;
	int mlLine;
	std::string msValue;
	std::string msName; // declared member
};

// Members declared [nosave] or [volatile], skipped by save games
const std::set<std::string> &SomaScriptNoSaveNames();

// Compiles SOMA .hps files: resolves #include against the game's script/ tree (paths and bare
// basenames, case-insensitively), each file included once per module.
class cSomaScriptBuilder
{
public:
	explicit cSomaScriptBuilder(const std::string &asGameDir);

	std::string Resolve(const std::string &asInclude, const std::string &asFromFile) const;

	// Returns the asIScriptModule::Build() result; sections are added for every included file.
	int Build(asIScriptEngine *apEngine, const std::string &asModuleName, const std::string &asEntryFile,
			  std::string *apMissingInclude = NULL);

	const std::string &GetGameDir() const { return msGameDir; }
	const std::vector<cSomaScriptMetadata> &GetMetadata() const { return mvMetadata; }

private:
	bool AddFile(asIScriptModule *apModule, const std::string &asFile, std::map<std::string, bool> &aIncluded,
				 std::string *apMissingInclude);

	std::string msGameDir;
	std::vector<cSomaScriptMetadata> mvMetadata;
	std::map<std::string, std::string> mmapByRelPath;  // lowercase path under script/
	std::map<std::string, std::string> mmapByBaseName; // lowercase basename
};

#endif // SOMA_SCRIPT_BUILDER_H
