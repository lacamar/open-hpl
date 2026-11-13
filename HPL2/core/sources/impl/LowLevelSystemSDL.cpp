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
#include <string.h>
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
static void AppendSinkU8(void *apCtx, const unsigned char *apData, size_t alLen)
{
	((std::string*)apCtx)->append((const char*)apData, alLen);
}

int main(int argc, char *argv[])
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
		mpRust = NULL;
	}

	cLogWriter::~cLogWriter()
	{
		if(mpRust) ohpl_l1_log_close(mpRust);
	}

	void cLogWriter::Write(const tString& asMessage)
	{
		if(!mpRust) ReopenFile();

		if(mpRust) ohpl_l1_log_write(mpRust, asMessage.c_str());
	}

	void cLogWriter::Clear()
	{
		ReopenFile();
	}

	//-----------------------------------------------------------------------


	void cLogWriter::SetFileName(const tWString& asFile)
	{
		if(msFileName == asFile) return;

		msFileName = asFile;
		if(mpRust) ReopenFile();
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

		mpRust = ohpl_l1_log_open(cString::To8Char(msFileName).c_str());
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
