/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

//#include <vld.h>
//Use this to check for memory leaks!

#ifdef WIN32
#pragma comment(lib, "angelscript.lib")
#define UNICODE
#include <windows.h>
#include <shlobj.h>
#endif

#define _UNICODE

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <vector>

#ifndef WIN32
#include <unistd.h>
#endif

#include "impl/LowLevelSystemSDL.h"
#include "impl/SqScript.h"
#include "system/Platform.h"

#if USE_SDL2
#include "SDL2/SDL.h"
#else
#include "SDL/SDL.h"
#endif


#include "system/String.h"
#include "impl/RustImpl.h"
#include "impl/RustBackends.h"
#include "impl/RustL1.h"
#include "impl/RustL7.h"

static void AppendSink(void *apCtx, const char *apData, size_t alLen)
{
	((std::string*)apCtx)->append(apData, alLen);
}

#ifndef WIN32
#include <clocale>
#include <langinfo.h>
#include <unistd.h>
#endif

#ifdef __linux__
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <set>
#endif


// Include for using the versioning header
//#include "BuildID_HPL2_0.h"

// @todo redo this prototype to take a tStringVec instead

#ifndef IGNORE_HPL_MAIN
extern int hplMain(const hpl::tString &asCommandLine);

#ifdef WIN32
#include <windows.h>
int WINAPI WinMain(	HINSTANCE hInstance,  HINSTANCE hPrevInstance,LPSTR	lpCmdLine, int nCmdShow)
{
	return hplMain(lpCmdLine);
}
#else
#ifdef __linux__
static bool WriteProcFile(const char *apPath, const std::string &asData)
{
	FILE *pFile = fopen(apPath, "w");
	if(pFile == NULL) return false;
	bool bOk = fputs(asData.c_str(), pFile) >= 0;
	return fclose(pFile) == 0 && bOk;
}

static void AddSteamRoot(std::set<std::string> &aSet, const std::string &asPath)
{
	char vReal[PATH_MAX];
	if(realpath(asPath.c_str(), vReal) == NULL) return;
	std::string sRoot;
	if(ohpl_sys_steam_root(vReal, AppendSink, &sRoot)) aSet.insert(sRoot);
}

static void AddSteamLibraryFolders(std::set<std::string> &aSet, const std::string &asVdf)
{
	std::ifstream file(asVdf);
	for(std::string sLine; std::getline(file, sLine);)
	{
		std::string sPath;
		if(ohpl_sys_vdf_path(sLine.data(), sLine.size(), AppendSink, &sPath)) AddSteamRoot(aSet, sPath + "/steamapps/common");
	}
}

// Game data is read-only to the engine: every Steam library and every steamapps/common reachable from the game dir
// gets a private read-only bind mount. Fails closed: a root that stays writable stops the engine.
static bool ProtectSteamLibraries()
{
	std::set<std::string> setRoots;
	DIR *pDir = opendir(".");
	for(dirent *pEnt = pDir ? readdir(pDir) : NULL; pEnt; pEnt = readdir(pDir))
		AddSteamRoot(setRoots, pEnt->d_name);
	if(pDir) closedir(pDir);
	const char *pHome = getenv("HOME");
	const char *pData = getenv("XDG_DATA_HOME");
	if(pHome)
	{
		AddSteamLibraryFolders(setRoots, std::string(pHome) + "/.steam/steam/steamapps/libraryfolders.vdf");
		AddSteamLibraryFolders(setRoots, std::string(pHome) + "/.local/share/Steam/steamapps/libraryfolders.vdf");
	}
	if(pData && *pData) AddSteamLibraryFolders(setRoots, std::string(pData) + "/Steam/steamapps/libraryfolders.vdf");
	if(setRoots.empty()) return true;

	char vCwd[PATH_MAX];
	uid_t lUid = getuid();
	gid_t lGid = getgid();
	if(getcwd(vCwd, sizeof(vCwd)) == NULL || unshare(CLONE_NEWUSER | CLONE_NEWNS) != 0 ||
	   !WriteProcFile("/proc/self/setgroups", "deny") ||
	   !WriteProcFile("/proc/self/uid_map", std::to_string(lUid) + " " + std::to_string(lUid) + " 1") ||
	   !WriteProcFile("/proc/self/gid_map", std::to_string(lGid) + " " + std::to_string(lGid) + " 1") ||
	   mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0)
	{
		fprintf(stderr, "open-hpl: could not make Steam libraries read-only: %s\n", strerror(errno));
	}
	else
	{
		static_assert(ST_NOSUID == 2 && ST_NODEV == 4 && ST_NOEXEC == 8 && ST_NOATIME == 1024 && ST_NODIRATIME == 2048 && ST_RELATIME == 4096 &&
			MS_RDONLY == 1 && MS_NOSUID == 2 && MS_NODEV == 4 && MS_NOEXEC == 8 && MS_REMOUNT == 32 && MS_NOATIME == 1024 &&
			MS_NODIRATIME == 2048 && MS_BIND == 4096 && MS_RELATIME == (1 << 21), "mount flags cross the ABI");
		for(const std::string &sRoot : setRoots)
		{
			struct statvfs st;
			unsigned long lFlags = 0;
			bool bOk = mount(sRoot.c_str(), sRoot.c_str(), NULL, MS_BIND | MS_REC, NULL) == 0 && statvfs(sRoot.c_str(), &st) == 0;
			if(bOk) lFlags = (unsigned long)ohpl_sys_remount_flags(st.f_flag);
			if(!bOk || mount(NULL, sRoot.c_str(), NULL, lFlags, NULL) != 0)
				fprintf(stderr, "open-hpl: could not make '%s' read-only: %s\n", sRoot.c_str(), strerror(errno));
		}
		// the old cwd still points into the writable mount
		if(chdir(vCwd) != 0) fprintf(stderr, "open-hpl: chdir '%s': %s\n", vCwd, strerror(errno));
	}

	// an already read-only root (bwrap, flatpak) passes even when unshare failed
	bool bAllReadOnly = true;
	for(const std::string &sRoot : setRoots)
		if(access(sRoot.c_str(), W_OK) == 0)
		{
			fprintf(stderr, "open-hpl: refusing to run: '%s' is writable\n", sRoot.c_str());
			bAllReadOnly = false;
		}
	return bAllReadOnly;
}
#endif

static void AppendSinkU8(void *apCtx, const unsigned char *apData, size_t alLen)
{
	((std::string*)apCtx)->append((const char*)apData, alLen);
}

int main(int argc, char *argv[])
{
#ifdef __linux__
	if(hpl::OhplBackendRustRequested())
	{
		bool bCwd = false;
		hpl::tString sCmdLine;
		if(ohpl_l1_main_args(argc, argv, AppendSinkU8, &sCmdLine, &bCwd) != 0) return 1;
		if(!bCwd)
		{
			hpl::tString sDataDir = hpl::cPlatform::GetDataDir();
			if(chdir(sDataDir.c_str())) {}
		}
		if(!ohpl_l1_protect_steam()) return 1;
		return hplMain(sCmdLine);
	}
#endif
#ifdef __linux__
	if(!std::setlocale(LC_CTYPE, "")) {
		fprintf(stderr, "Can't set the specified locale! Check LANG, LC_CTYPE, LC_ALL.\n");
		return 1;
	}
	char *charset = nl_langinfo(CODESET);
	bool utf8_mode = (strcasecmp(charset, "UTF-8") == 0);
	if (!utf8_mode) {
		fprintf(stderr, "UTF-8 Charset %s available.\nCurrent LANG is %s\nCharset: %s\n",
			utf8_mode ? "is" : "not",
			getenv("LANG"), charset);
	}
#endif

	bool cwd = false;
	bool bGotGameDirArg = false;
	hpl::tString cmdline = "";
	for (int i=1; i < argc; i++) {
		unsigned lKind = ohpl_sys_classify_arg(argv[i], bGotGameDirArg);
        if (lKind == kOhplArgCwd) {
            cwd = true;
        } else if (lKind == kOhplArgSkip) {
            // skip "finder" process number
		} else if (lKind == kOhplArgGameDir) {
			struct stat dirStat;
			if (stat(argv[i], &dirStat) == 0 && S_ISDIR(dirStat.st_mode))
			{
				bGotGameDirArg = true;
				cwd = (chdir(argv[i]) == 0);
				continue;
			}

			if (cmdline.length()>0) {
				cmdline.append(" ").append(argv[i]);
			} else {
				cmdline.append(argv[i]);
			}
		} else {
			if (cmdline.length()>0) {
				cmdline.append(" ").append(argv[i]);
			} else {
				cmdline.append(argv[i]);
			}
		}
	}

	if (!cwd) {
        hpl::tString dataDir = hpl::cPlatform::GetDataDir();

        chdir(dataDir.c_str());
	}
#ifdef __linux__
	if(!ProtectSteamLibraries()) return 1;
#endif

	return hplMain(cmdline);
}
#endif
#endif





namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// LOG WRITER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static cLogWriter gLogWriter(_W("hpl.log"));
	static cLogWriter gUpdateLogWriter(_W("hpl_update.log"));

	//-----------------------------------------------------------------------
	
	cLogWriter::cLogWriter(const tWString& asFileName)
	{
		msFileName = asFileName;
		mpFile = NULL;
		mpRust = NULL;
	}

	cLogWriter::~cLogWriter()
	{
		if(mpFile) fclose(mpFile);
		if(mpRust) ohpl_l1_log_close(mpRust);
	}

	void cLogWriter::Write(const tString& asMessage)
	{
		if(!mpFile && !mpRust) ReopenFile();
		
		if(mpRust)
		{
			ohpl_l1_log_write(mpRust, asMessage.c_str());
		}
		else if(mpFile)
		{
			fprintf(mpFile, "%s", asMessage.c_str());
			fflush(mpFile);
		}
	}

	void cLogWriter::Clear()
	{
		ReopenFile();
		if(mpFile) fflush(mpFile);
	}

	//-----------------------------------------------------------------------


	void cLogWriter::SetFileName(const tWString& asFile)
	{
		if(msFileName == asFile) return;

		msFileName = asFile;
		if(mpFile || mpRust) ReopenFile();
	}

	//-----------------------------------------------------------------------


	static bool PathLooksLikeSteamInstall(const tWString &asFile)
	{
		tString sPath = cString::To8Char(asFile);
		const char *pCwd = NULL;
#ifndef WIN32
		char vCwd[4096];
		if (sPath.empty() == false && sPath[0] != '/' && getcwd(vCwd, sizeof(vCwd)) != NULL) pCwd = vCwd;
#endif
		return ohpl_sys_looks_like_steam_install(sPath.data(), sPath.size(), pCwd);
	}

	void cLogWriter::ReopenFile()
	{
		if(mpFile) fclose(mpFile);
		mpFile = NULL;
		if(mpRust) ohpl_l1_log_close(mpRust);
		mpRust = NULL;

		if(PathLooksLikeSteamInstall(msFileName))
		{
			fprintf(stderr, "open-hpl: refusing to open log file '%s' - it resolves inside "
				"what looks like a real Steam install directory (steamapps/common). Game "
				"modules in this codebase must route logs through an XDG state path, never "
				"a path resolved against a real game install's own directory. Log output for "
				"this run will go to stderr only.\n", cString::To8Char(msFileName).c_str());
			return;
		}

		if(OhplBackendRustRequested())
		{
			mpRust = ohpl_l1_log_open(cString::To8Char(msFileName).c_str());
			return;
		}
		#ifdef WIN32
			mpFile = _wfopen(msFileName.c_str(),_W("w"));
		#else
			mpFile = fopen(cString::To8Char(msFileName).c_str(),"w");
		#endif
	}


	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// LOG FUNCTIONS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	static tLogMessageCallbackFunc gpLogMessageCallbackFunc=NULL;

	//-----------------------------------------------------------------------
	
	void SetLogFile(const tWString &asFile)
	{
		gLogWriter.SetFileName(asFile);
	}

	tWString GetLogFile()
	{
		return gLogWriter.GetFileName();
	}

	//-----------------------------------------------------------------------

	void FatalError(const char* fmt,... )
	{
		char text[4096];
		va_list ap;	
		if (fmt == NULL)
			return;	
		va_start(ap, fmt);
		vsnprintf(text, sizeof(text), fmt, ap);
		va_end(ap);

		tString sMess = "FATAL ERROR: ";
		sMess += text;
		gLogWriter.Write(sMess);

		if(gpLogMessageCallbackFunc) gpLogMessageCallbackFunc(eLogOutputType_FatalError, sMess.c_str());

#if defined(__APPLE__) || defined(__linux__)
#if !SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
		SDL_Quit();
#endif
		if(getenv("OPENHPL_HEADLESS_SOCKET") == NULL)
		{
			cPlatform::CreateMessageBox(eMsgBoxType_Error, _W("FATAL ERROR"), _W("%ls"), cString::To16Char(sMess).c_str());
		}

		exit(1);
	}

	//-----------------------------------------------------------------------

	void Error(const char* fmt, ...)
	{
		char text[2048];
		va_list ap;	
		if (fmt == NULL)
			return;	
		va_start(ap, fmt);
		vsnprintf(text, sizeof(text), fmt, ap);
		va_end(ap);

		tString sMess = "ERROR: ";
		sMess += text;
		gLogWriter.Write(sMess);

		if(gpLogMessageCallbackFunc) gpLogMessageCallbackFunc(eLogOutputType_Error, sMess.c_str());
	}

	//-----------------------------------------------------------------------


	void Warning(const char* fmt, ...)
	{
		char text[2048];
		va_list ap;	
		if (fmt == NULL)
			return;	
		va_start(ap, fmt);
		vsnprintf(text, sizeof(text), fmt, ap);
		va_end(ap);

		tString sMess = "WARNING: ";
		sMess += text;
		gLogWriter.Write(sMess);

		if(gpLogMessageCallbackFunc) gpLogMessageCallbackFunc(eLogOutputType_Warning, sMess.c_str());
	}

	//-----------------------------------------------------------------------


	void Log(const char* fmt, ...)
	{
		char text[4096];
		va_list ap;	
		if (fmt == NULL)
			return;	
		va_start(ap, fmt);
		vsnprintf(text, sizeof(text), fmt, ap);
		va_end(ap);

		tString sMess = "";
		sMess += text;
		gLogWriter.Write(sMess);

		if(gpLogMessageCallbackFunc) gpLogMessageCallbackFunc(eLogOutputType_Normal, sMess.c_str());
	}

	//-----------------------------------------------------------------------

	static bool gbUpdateLogIsActive;
	void SetUpdateLogFile(const tWString &asFile)
	{
		gUpdateLogWriter.SetFileName(asFile);
	}

	void ClearUpdateLogFile()
	{
		if(!gbUpdateLogIsActive) return;

		gUpdateLogWriter.Clear();
	}

	void SetUpdateLogActive(bool abX)
	{
		gbUpdateLogIsActive =abX;
	}

	bool GetUpdateLogActive()
	{
		return gbUpdateLogIsActive;
	}

	//-----------------------------------------------------------------------

	void LogUpdate(const char* fmt, ...)
	{
		if(!gbUpdateLogIsActive) return;

		char text[2048];
		va_list ap;	
		if (fmt == NULL)
			return;	
		va_start(ap, fmt);
		vsnprintf(text, sizeof(text), fmt, ap);
		va_end(ap);

		tString sMess = "";
		sMess += text;
		gUpdateLogWriter.Write(sMess);
	}

	//-----------------------------------------------------------------------

	extern void SetLogMessageCallback(tLogMessageCallbackFunc apFunc)
	{
		gpLogMessageCallbackFunc = apFunc;
	}

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelSystemSDL::cLowLevelSystemSDL()
	{
		OhplL7InstallAddons();
		mpRustEngine = ohpl_l7_engine_new();
		if(ohpl_l7_engine_raw(mpRustEngine)==NULL)
		{
			Error("Failed to start angel script!\n");
		}

		mlHandleCount = 0;

		Log("-------- THE HPL ENGINE LOG ------------\n");
		//Log("Engine build ID %s\n\n", 
		//	GetBuildID_HPL2_0());

		//const char* pASLibOptions = asGetLibraryOptions();
		//Log(" AngelScript options: '%s'\n", pASLibOptions);
	}

	//-----------------------------------------------------------------------

	cLowLevelSystemSDL::~cLowLevelSystemSDL()
	{
		/*Release all runnings contexts */
		
		ohpl_l7_engine_free(mpRustEngine);

		//perhaps not the best thing to skip :)
		//if(gpLogWriter)	hplDelete(gpLogWriter);
		//gpLogWriter = NULL;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	
	iScript* cLowLevelSystemSDL::CreateScript(const tString& asName)
	{
		return hplNew( cSqScript, (asName,mlHandleCount++,mpRustEngine) );
	}

	//-----------------------------------------------------------------------

	bool cLowLevelSystemSDL::AddScriptFunc(const tString& asFuncDecl, void* pFunc)
	{
		if(!ohpl_l7_add_func(mpRustEngine, asFuncDecl.c_str(), pFunc))
		{
			Error("Couldn't add func '%s'\n",asFuncDecl.c_str());
			return false;
		}

		return true;
	}
	
	//-----------------------------------------------------------------------

	bool cLowLevelSystemSDL::AddScriptVar(const tString& asVarDecl, void *pVar)
	{
		if(!ohpl_l7_add_var(mpRustEngine, asVarDecl.c_str(), pVar))
		{
			Error("Couldn't add var '%s'\n",asVarDecl.c_str());
			return false;
		}

		return true;
	}
	
	//-----------------------------------------------------------------------
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}
