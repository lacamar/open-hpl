#include "SomaScriptBuilder.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static std::set<std::string> gsetNoSave;

bool SomaScriptIsNoSave(asITypeInfo *apType, const char *asProp)
{
	for (; apType; apType = apType->GetBaseType())
		if (gsetNoSave.count(std::string(apType->GetName()) + "::" + asProp))
			return true;
	return false;
}

// Constructs AngelScript 2.28 accepted but 2.38 rejects, as {file suffix, from, to}.
static const char *gvCompatPatches[][3] = {
	{"custom_depth/agent_deepsea_suit.hps", "CheckShapeWorldCollision(cVector3f_Zero,", "CheckShapeWorldCollision(void,"},
	{"modules/emotionhandler.hps", "lBeat<0 ? null : mvHeartbeats[lBeat]", "lBeat<0 ? null : @mvHeartbeats[lBeat]"},
	{"helper_custom_depth_imgui.hps", "const cGuiDialogBoxSettings &in aSettings", "cGuiDialogBoxSettings &in aSettings"},
	{"03_02_omicron_inside.hps", ".length-", ".length()-"},
	{"03_02_omicron_inside.hps", ".length -", ".length() -"},
	{"modules/menuhandler.hps", "kOptionsGameplayBgSize = cVector2f(680, 255)", "kOptionsGameplayBgSize = cVector2f(680, 293)"},
	{"modules/menuhandler.hps", "OptionMenu_UpdateExtraWidth(\"CrosshairSimple\", true);", "OptionMenu_UpdateExtraWidth(\"CrosshairSimple\", true); OptionMenu_UpdateExtraWidth(\"OpenHplHud\", true);"},
	{"modules/menuhandler.hps", "OptionMenu_UpdateFocus(\"CrosshairSimple\", msSelectedGameplayButton);",
	 "OptionMenu_UpdateFocus(\"CrosshairSimple\", msSelectedGameplayButton); } {"
	 " bool bValue = mpConfig.GetBool(\"Gameplay\", \"OpenHplHud\", false);"
	 " bool bNewValue = OptionMenu_ButtonOptionsToggle(\"OpenHplHud\", kMainMenuButtonPos, lY++, msSelectedGameplayButton, bValue, mlActionHorizontal);"
	 " if(bValue != bNewValue) { mpConfig.SetBool(\"Gameplay\", \"OpenHplHud\", bNewValue); ApplySettings(); }"
	 " msSelectedGameplayButton = OptionMenu_UpdateFocus(\"OpenHplHud\", msSelectedGameplayButton);"},
};

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

static uint64_t Fnv(const void *apData, size_t alSize, uint64_t alHash = 14695981039346656037ull)
{
	for (size_t i = 0; i < alSize; ++i)
		alHash = (alHash ^ ((const unsigned char *)apData)[i]) * 1099511628211ull;
	return alHash;
}

static std::string Hex(uint64_t alValue)
{
	char vBuf[17];
	snprintf(vBuf, sizeof(vBuf), "%016llx", (unsigned long long)alValue);
	return vBuf;
}

static std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}

static void Index(const std::string &asRoot, const std::string &asRel, std::map<std::string, std::string> &aByRel,
				  std::map<std::string, std::string> &aByBase)
{
	DIR *pDir = opendir((asRoot + "/" + asRel).c_str());
	if (pDir == NULL)
		return;
	while (dirent *pEnt = readdir(pDir))
	{
		std::string sName = pEnt->d_name;
		if (sName == "." || sName == "..")
			continue;
		std::string sRel = asRel.empty() ? sName : asRel + "/" + sName;
		std::string sFull = asRoot + "/" + sRel;
		struct stat st;
		if (stat(sFull.c_str(), &st) != 0)
			continue;
		if (S_ISDIR(st.st_mode))
			Index(asRoot, sRel, aByRel, aByBase);
		else if (Lower(sName).size() > 4 && Lower(sName).compare(Lower(sName).size() - 4, 4, ".hps") == 0)
		{
			aByRel[Lower(sRel)] = sFull;
			aByBase.insert(std::make_pair(Lower(sName), sFull));
		}
	}
	closedir(pDir);
}

cSomaScriptBuilder::cSomaScriptBuilder(const std::string &asGameDir, const std::string &asCacheDir)
	: msGameDir(asGameDir), msCacheDir(asCacheDir)
{
	Index(asGameDir + "/script", "", mmapByRelPath, mmapByBaseName);
}

std::string cSomaScriptBuilder::Resolve(const std::string &asInclude, const std::string &asFromFile) const
{
	std::string sKey = Lower(asInclude);
	std::replace(sKey.begin(), sKey.end(), '\\', '/');
	std::map<std::string, std::string>::const_iterator it = mmapByRelPath.find(sKey);
	if (it != mmapByRelPath.end())
		return it->second;

	std::string sDir = asFromFile.substr(0, asFromFile.find_last_of('/') + 1);
	std::ifstream f((sDir + asInclude).c_str());
	if (f.is_open())
		return sDir + asInclude;

	size_t lSlash = sKey.find_last_of('/');
	it = mmapByBaseName.find(lSlash == std::string::npos ? sKey : sKey.substr(lSlash + 1));
	return it != mmapByBaseName.end() ? it->second : "";
}

bool cSomaScriptBuilder::AddFile(std::vector<std::pair<std::string, std::string>> &avSections, const std::string &asFile,
								 std::map<std::string, bool> &aIncluded, std::string *apMissingInclude)
{
	if (aIncluded[asFile])
		return true;
	aIncluded[asFile] = true;

	std::ifstream f(asFile.c_str(), std::ios::binary);
	if (f.is_open() == false)
		return false;
	std::stringstream ss;
	ss << f.rdbuf();
	std::string sCode = ss.str();

	std::string sLowerFile = Lower(asFile);
	for (auto &patch : gvCompatPatches)
	{
		size_t lSuffix = strlen(patch[0]);
		if (sLowerFile.size() < lSuffix || sLowerFile.compare(sLowerFile.size() - lSuffix, lSuffix, patch[0]) != 0)
			continue;
		for (size_t p = sCode.find(patch[1]); p != std::string::npos; p = sCode.find(patch[1], p + 1))
			sCode.replace(p, strlen(patch[1]), patch[2]);
	}

	// Blank out #include lines (keeping line numbers) and add each include as its own section.
	std::vector<std::string> vIncludes;
	size_t lPos = 0;
	std::string sClass;
	while (lPos < sCode.size())
	{
		size_t lEnd = sCode.find('\n', lPos);
		if (lEnd == std::string::npos)
			lEnd = sCode.size();
		size_t lFirst = sCode.find_first_not_of(" \t", lPos);
		static const std::regex reClass("^\\s*(?:shared\\s+|abstract\\s+|mixin\\s+)*class\\s+(\\w+)");
		std::smatch m;
		std::string sLine = sCode.substr(lPos, lEnd - lPos);
		if (sLine.find("class") != std::string::npos && std::regex_search(sLine, m, reClass))
			sClass = m[1];
		// Save-system metadata on declarations ([nosave], [volatile]): recorded, then blanked out.
		while (lFirst < lEnd && sCode[lFirst] == '[')
		{
			size_t lClose = sCode.find(']', lFirst);
			if (lClose == std::string::npos || lClose >= lEnd)
				break;
			std::string sValue = sCode.substr(lFirst + 1, lClose - lFirst - 1);
			size_t lDeclEnd = sCode.find_first_of(";=(", lClose);
			if ((sValue == "nosave" || sValue == "volatile") && lDeclEnd != std::string::npos)
			{
				size_t lNameEnd = sCode.find_last_not_of(" \t\r\n", lDeclEnd - 1);
				size_t lNameStart = sCode.find_last_of(" \t\r\n&@", lNameEnd);
				if (lNameEnd != std::string::npos && lNameStart != std::string::npos && lNameStart < lNameEnd)
					gsetNoSave.insert(sClass + "::" + sCode.substr(lNameStart + 1, lNameEnd - lNameStart));
			}
			for (size_t i = lFirst; i <= lClose; ++i)
				sCode[i] = ' ';
			lFirst = sCode.find_first_not_of(" \t", lClose + 1);
		}
		if (lFirst < lEnd && sCode.compare(lFirst, 8, "#include") == 0)
		{
			size_t q1 = sCode.find('"', lFirst);
			size_t q2 = q1 == std::string::npos ? q1 : sCode.find('"', q1 + 1);
			if (q2 != std::string::npos && q2 < lEnd)
				vIncludes.push_back(sCode.substr(q1 + 1, q2 - q1 - 1));
			for (size_t i = lPos; i < lEnd; ++i)
				if (sCode[i] != '\r')
					sCode[i] = ' ';
		}
		lPos = lEnd + 1;
	}

	avSections.emplace_back(asFile, sCode);

	for (size_t i = 0; i < vIncludes.size(); ++i)
	{
		std::string sPath = Resolve(vIncludes[i], asFile);
		if (sPath.empty())
		{
			if (apMissingInclude && apMissingInclude->empty())
				*apMissingInclude = vIncludes[i] + " (from " + asFile + ")";
			continue;
		}
		AddFile(avSections, sPath, aIncluded, apMissingInclude);
	}
	return true;
}

int cSomaScriptBuilder::Build(asIScriptEngine *apEngine, const std::string &asModuleName, const std::string &asEntryFile,
							  std::string *apMissingInclude)
{
	std::vector<std::pair<std::string, std::string>> vSections;
	std::map<std::string, bool> mapIncluded;
	if (AddFile(vSections, asEntryFile, mapIncluded, apMissingInclude) == false)
		return asERROR;
	asIScriptModule *pModule = apEngine->GetModule(asModuleName.c_str(), asGM_ALWAYS_CREATE);

	// Bytecode is only valid for the exact binary that registered the API
	std::string sKey, sCacheFile;
	struct stat st;
	if (msCacheDir.empty() == false && stat("/proc/self/exe", &st) == 0)
	{
		uint64_t lHash = Fnv(&st.st_size, sizeof(st.st_size), Fnv(&st.st_mtim, sizeof(st.st_mtim)));
		for (auto &sec : vSections)
			lHash = Fnv(sec.second.data(), sec.second.size(), Fnv(sec.first.c_str(), sec.first.size() + 1, lHash));
		sKey = Hex(lHash);
		sCacheFile = msCacheDir + "/" + Hex(Fnv(asEntryFile.data(), asEntryFile.size())) + ".asb";

		cByteStream in;
		std::ifstream f(sCacheFile.c_str(), std::ios::binary);
		in.msData.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
		if (in.msData.compare(0, sKey.size(), sKey) == 0)
		{
			in.mlPos = sKey.size();
			if (pModule->LoadByteCode(&in) >= 0)
				return 0;
			pModule = apEngine->GetModule(asModuleName.c_str(), asGM_ALWAYS_CREATE);
		}
	}

	for (auto &sec : vSections)
		pModule->AddScriptSection(sec.first.c_str(), sec.second.c_str(), sec.second.size());
	int r = pModule->Build();
	cByteStream out;
	out.msData = sKey;
	if (r >= 0 && sCacheFile.empty() == false && pModule->SaveByteCode(&out) >= 0)
	{
		std::string sTmp = sCacheFile + "." + std::to_string(getpid());
		if (std::ofstream(sTmp.c_str(), std::ios::binary).write(out.msData.data(), out.msData.size()))
			rename(sTmp.c_str(), sCacheFile.c_str());
	}
	return r;
}
