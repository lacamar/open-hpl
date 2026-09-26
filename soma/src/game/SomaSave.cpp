#include "SomaSave.h"
#include "SomaBase.h"
#include "SomaScriptBind.h"

#include <fstream>
#include <sstream>

void SomaRequestMapChange(const tString &asMap, const tString &asStart);

namespace
{
	tString gsMapFile, gsStartPos, gsEntryVars;

	std::string Escape(const tString &s)
	{
		std::string r;
		for (char c : s)
			r += c == '\n' ? "\\n" : c == '\t' ? "\\t" : c == '\\' ? "\\\\" : std::string(1, c);
		return r;
	}

	tString Unescape(const std::string &s)
	{
		tString r;
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] != '\\' || i + 1 == s.size())
			{
				r += s[i];
				continue;
			}
			char c = s[++i];
			r += c == 'n' ? '\n' : c == 't' ? '\t' : c;
		}
		return r;
	}
}

tWString cSomaSaveHandler::GetSaveDir()
{
	tWString sDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGDataHome);
	for (const wchar_t *pPart : {L"open-hpl/", L"soma/", L"saves/"})
	{
		sDir += pPart;
		if (cPlatform::FolderExists(sDir) == false)
			cPlatform::CreateFolder(sDir);
	}
	return sDir;
}

tWString cSomaSaveHandler::GetLatestSave()
{
	tWStringList lstFiles;
	cPlatform::FindFilesInDir(lstFiles, GetSaveDir(), _W("*.sav"));
	tWString sBest;
	cDate bestDate;
	for (const tWString &sFile : lstFiles)
	{
		cDate date = cPlatform::FileModifiedDate(GetSaveDir() + sFile);
		if (sBest.empty() || date > bestDate)
		{
			sBest = sFile;
			bestDate = date;
		}
	}
	return sBest;
}

void cSomaSaveHandler::OnMapEnter(const tString &asMapFile, const tString &asStartPos)
{
	gsMapFile = asMapFile;
	gsStartPos = asStartPos;
	gsEntryVars = SomaSerializeGlobalVars();
}

bool cSomaSaveHandler::Save(const tWString &asFile, bool abCurrentVars)
{
	if (gsMapFile.empty())
		return false;
	tWString sPath = GetSaveDir() + cString::GetFileNameW(asFile);
	std::ofstream file(cString::To8Char(sPath).c_str(), std::ios::binary | std::ios::trunc);
	if (file.is_open() == false)
	{
		Error("SOMA save: could not write '%s'\n", cString::To8Char(sPath).c_str());
		return false;
	}
	file << "map\t" << Escape(gsMapFile) << "\n";
	file << "pos\t" << Escape(gsStartPos) << "\n";
	for (const tString &sMap : gpSomaBase->GetVisitedMaps())
		file << "visited\t" << Escape(sMap) << "\n";
	file << (abCurrentVars ? SomaSerializeGlobalVars() : gsEntryVars);
	Log("SOMA save: %s (%s)\n", cString::To8Char(sPath).c_str(), gsMapFile.c_str());
	return true;
}

bool cSomaSaveHandler::AutoSave(bool abCheckpoint, bool abCurrentVars)
{
	bool bOk = Save(_W("auto.sav"), abCurrentVars);
	if (abCheckpoint)
		bOk = Save(_W("CheckPoint.sav"), abCurrentVars) && bOk;
	return bOk;
}

bool cSomaSaveHandler::Load(const tWString &asFile, bool abImmediate)
{
	tWString sPath = GetSaveDir() + cString::GetFileNameW(asFile);
	std::ifstream file(cString::To8Char(sPath).c_str(), std::ios::binary);
	if (file.is_open() == false)
	{
		Error("SOMA save: could not read '%s'\n", cString::To8Char(sPath).c_str());
		return false;
	}
	tString sMap, sPos, sVars;
	std::set<tString> setVisited;
	std::string sLine;
	while (std::getline(file, sLine))
	{
		size_t lTab = sLine.find('\t');
		std::string sKey = sLine.substr(0, lTab);
		std::string sValue = lTab == std::string::npos ? "" : sLine.substr(lTab + 1);
		if (sKey == "map")
			sMap = Unescape(sValue);
		else if (sKey == "pos")
			sPos = Unescape(sValue);
		else if (sKey == "visited")
			setVisited.insert(Unescape(sValue));
		else
			sVars += sLine + "\n";
	}
	if (sMap.empty())
		return false;
	SomaDeserializeGlobalVars(sVars);
	// Entered again below, so not yet visited
	setVisited.erase(sMap);
	gpSomaBase->GetVisitedMaps() = setVisited;
	Log("SOMA save: loading %s (%s at %s)\n", cString::To8Char(sPath).c_str(), sMap.c_str(), sPos.c_str());
	if (abImmediate == false)
	{
		SomaRequestMapChange(sMap, sPos);
		return true;
	}
	tString sError;
	if (gpSomaBase->LoadMap(sMap, cVector3f(0), sError, sPos.empty() ? "*" : sPos))
		return true;
	Error("SOMA save: %s\n", sError.c_str());
	return false;
}

void cSomaSaveHandler::RegisterNatives(asIScriptEngine *e)
{
	const char *T = "cLuxSaveHandler";
	static char gHandler;
	typedef const tWString &W;
	SOMA_FUNC(e, "cLuxSaveHandler@ cLux_GetSaveHandler()", +[]() { return (void *)&gHandler; });
	SOMA_METHOD(e, T, "void SaveGameToFile(const tWString&in asSaveFile)", +[](void *, W f) { Save(f, true); });
	SOMA_METHOD(e, T, "void LoadGameFromFile(const tWString&in asSaveFile)", +[](void *, W f) { Load(f); });
	SOMA_METHOD(e, T, "bool AutoSave(bool abSaveCheckpoint, bool abDelayed=true)", +[](void *, bool c, bool) { return AutoSave(c, true); });
	SOMA_METHOD(e, T, "bool GetSaveThreadActive()", +[](void *) { return false; });
	SOMA_METHOD(e, T, "bool HasLoadError(tString&out asError)", +[](void *, tString &) { return false; });
	SOMA_METHOD(e, T, "void DelayedLoadGameFromFile(const tWString&in asSaveFile, const tString&in asCallbackObject, const tString&in asCallbackFunction, bool abWaitAfterHeader, bool abWaitAfterLoad)",
				+[](void *, W f, const tString &, const tString &, bool, bool) { Load(f); });
	SOMA_METHOD(e, T, "void DelayedSaveGameToFile(const tWString&in asSaveFile, bool abSaveAsCheckpoint)", +[](void *, W f, bool) { Save(f, true); });
	SOMA_METHOD(e, T, "void DeleteSaveFile(const tWString&in asSaveFile)", +[](void *, W f) { cPlatform::RemoveFile(GetSaveDir() + cString::GetFileNameW(f)); });
	SOMA_METHOD(e, T, "bool IsDoneLoadingHeader()", +[](void *) { return true; });
	SOMA_METHOD(e, T, "void ContinueLoading(bool abDisableWaits)", +[](void *, bool) {});
	SOMA_METHOD(e, T, "bool IsDoneLoadingSavedGame()", +[](void *) { return true; });
	SOMA_METHOD(e, T, "void StartLoadedGame()", +[](void *) {});
	SOMA_FUNC(e, "bool cLux_HasConfigLoadError(tString&out asError)", +[](tString &) { return false; });
}
