#include "SomaScriptCheck.h"
#include "SomaScriptApi.h"
#include "SomaScriptBuilder.h"
#include "SomaScriptStrings.h"

#include "impl/scriptarray.h"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <regex>
#include <sstream>

//---------------------------------------

struct cCheckMessages
{
	std::vector<std::string> mvErrors;
	int mlWarnings = 0;
	std::string msContext;
	void Callback(const asSMessageInfo *apMsg)
	{
		if (apMsg->type == asMSGTYPE_INFORMATION)
			msContext = apMsg->message;
		if (apMsg->type == asMSGTYPE_ERROR)
		{
			char sBuf[64];
			snprintf(sBuf, sizeof(sBuf), ":%d:%d: ", apMsg->row, apMsg->col);
			mvErrors.push_back(std::string(apMsg->section ? apMsg->section : "") + sBuf + apMsg->message +
							   (apMsg->row <= 1 ? " [" + msContext + "]" : ""));
		}
		else if (apMsg->type == asMSGTYPE_WARNING)
			++mlWarnings;
	}
};

static std::string JsonEscape(const std::string &s)
{
	std::string o;
	for (char c : s)
	{
		if (c == '"' || c == '\\')
			o += '\\';
		if (c == '\n')
		{
			o += "\\n";
			continue;
		}
		if ((unsigned char)c < 0x20)
			continue;
		o += c;
	}
	return o;
}

static void FindMapScripts(const std::string &asDir, std::vector<std::string> &aOut, int alDepth)
{
	DIR *pDir = opendir(asDir.c_str());
	if (pDir == NULL || alDepth > 3)
	{
		if (pDir)
			closedir(pDir);
		return;
	}
	while (dirent *pEnt = readdir(pDir))
	{
		std::string sName = pEnt->d_name;
		if (sName == "." || sName == ".." || sName == "maps")
			continue;
		std::string sFull = asDir + "/" + sName;
		if (sName.size() > 4 && sName.compare(sName.size() - 4, 4, ".hps") == 0)
			aOut.push_back(sFull);
		else if (pEnt->d_type == DT_DIR || pEnt->d_type == DT_LNK)
			FindMapScripts(sFull, aOut, alDepth + 1);
	}
	closedir(pDir);
}

// Script entry points named by config/*.cfg (ScriptFile="..." and the game.cfg handlers).
static void FindConfigScripts(const std::string &asGameDir, std::vector<std::string> &aOut)
{
	const char *vCfgs[] = {"EntityTypes.cfg", "Modules.cfg", "PlayerStates.cfg", "game.cfg", "Effects.cfg"};
	std::regex re("\"([^\"]+\\.hps)\"", std::regex::icase);
	for (const char *pCfg : vCfgs)
	{
		std::ifstream f((asGameDir + "/config/" + pCfg).c_str());
		std::stringstream ss;
		ss << f.rdbuf();
		std::string s = ss.str();
		for (std::sregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it)
			aOut.push_back((*it)[1]);
	}
	std::sort(aOut.begin(), aOut.end());
	aOut.erase(std::unique(aOut.begin(), aOut.end()), aOut.end());
}

int RunSomaScriptCheck(const std::string &asGameDir, const std::string &asApiFile, const std::string &asReport)
{
	asIScriptEngine *pEngine = asCreateScriptEngine();
	cCheckMessages msgs;
	pEngine->SetMessageCallback(asMETHOD(cCheckMessages, Callback), &msgs, asCALL_THISCALL);
	ConfigureSomaScriptEngine(pEngine);

	RegisterSomaScriptStrings(pEngine);
	RegisterScriptArray(pEngine, true);
	RegisterSomaScriptArrayExtras(pEngine);

	cSomaScriptApi api;
	if (api.Load(asApiFile) == false)
	{
		fprintf(stderr, "cannot read script API '%s'\n", asApiFile.c_str());
		return 1;
	}
	api.Register(pEngine);
	std::vector<std::string> vApiErrors = api.GetErrors();
	vApiErrors.insert(vApiErrors.end(), msgs.mvErrors.begin(), msgs.mvErrors.end());
	msgs.mvErrors.clear();

	cSomaScriptBuilder builder(asGameDir);
	std::vector<std::string> vEntries;
	FindConfigScripts(asGameDir, vEntries);
	for (std::string &s : vEntries)
	{
		std::string sPath = builder.Resolve(s, asGameDir + "/script/");
		s = sPath.empty() ? asGameDir + "/script/" + s : sPath;
	}
	FindMapScripts(asGameDir + "/maps", vEntries, 0);

	std::ofstream out(asReport.c_str());
	out << "{\"api_errors\":[";
	for (size_t i = 0; i < vApiErrors.size(); ++i)
		out << (i ? "," : "") << "\"" << JsonEscape(vApiErrors[i]) << "\"";
	out << "],\"modules\":{";

	int lOk = 0, lMissing = 0;
	for (size_t i = 0; i < vEntries.size(); ++i)
	{
		msgs.mvErrors.clear();
		std::string sRelEntry = vEntries[i].substr(asGameDir.size() + 1);
		if (std::ifstream(vEntries[i].c_str()).is_open() == false)
		{
			// Named by config but not shipped
			++lMissing;
			out << (i ? "," : "") << "\"" << JsonEscape(sRelEntry) << "\":{\"ok\":true,\"missing\":true}";
			continue;
		}
		std::string sMissing;
		int r = builder.Build(pEngine, "check_" + std::to_string(i), vEntries[i], &sMissing);
		if (r >= 0)
			++lOk;
		std::string sRel = vEntries[i].substr(asGameDir.size() + 1);
		out << (i ? "," : "") << "\"" << JsonEscape(sRel) << "\":{\"ok\":" << (r >= 0 ? "true" : "false")
			<< ",\"missing_include\":\"" << JsonEscape(sMissing) << "\",\"error_count\":" << msgs.mvErrors.size() << ",\"errors\":[";
		for (size_t j = 0; j < msgs.mvErrors.size() && j < 20; ++j)
			out << (j ? "," : "") << "\"" << JsonEscape(msgs.mvErrors[j].substr(msgs.mvErrors[j].find("/script/") != std::string::npos ? msgs.mvErrors[j].find("/script/") + 1 : 0)) << "\"";
		out << "]}";
		pEngine->DiscardModule(("check_" + std::to_string(i)).c_str());
	}
	int lTotal = (int)vEntries.size() - lMissing;
	out << "},\"summary\":{\"modules\":" << lTotal << ",\"compiled\":" << lOk << ",\"not_shipped\":" << lMissing
		<< ",\"api_errors\":" << vApiErrors.size() << "}}\n";
	printf("script check: %d/%d modules compile (%d named in config but not shipped), %d API registration errors -> %s\n",
		   lOk, lTotal, lMissing, (int)vApiErrors.size(), asReport.c_str());

	pEngine->ShutDownAndRelease();
	return lOk == lTotal && vApiErrors.empty() ? 0 : 1;
}
