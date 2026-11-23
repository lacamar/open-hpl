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


#define _UNICODE

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>

#include "impl/LowLevelSystemSDL.h"
#include "system/Platform.h"
#include "SDL2/SDL.h"
#include "system/String.h"
#include "impl/RustImpl.h"
#include "impl/RustBackends.h"
#include "impl/RustL1.h"

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
		char vCwd[4096];
		if (sPath.empty() == false && sPath[0] != '/' && getcwd(vCwd, sizeof(vCwd)) != NULL) pCwd = vCwd;
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

}
