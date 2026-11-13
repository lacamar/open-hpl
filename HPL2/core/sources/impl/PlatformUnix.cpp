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

#include "system/Platform.h"
#include "system/RustPlatform.h"
#include "system/RustH1.h"
#include "impl/RustL1.h"

#include <stdexcept>

#include "system/String.h"

#include "system/LowLevelSystem.h"

#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>
#include <fstream>

#if USE_SDL2
#include "SDL2/SDL.h"
#else
#include "SDL/SDL.h"
#endif

#ifdef __linux__
#include <FL/fl_ask.H>
#include "binreloc.h"

#include <sys/types.h>
#endif
#include <unistd.h>
#include <sys/time.h>

#include <algorithm>

namespace hpl {

	static void L1Utf8Sink(void *apCtx, const unsigned char *apData, size_t alLen)
	{
		static_cast<tString*>(apCtx)->assign(reinterpret_cast<const char*>(apData), alLen);
	}

	static void L1NameSink(void *apCtx, const uint32_t *apData, size_t alLen)
	{
		static_cast<tWStringList*>(apCtx)->push_back(tWString(apData, apData + alLen));
	}

	static cDate L1ToDate(const OhplL1Date &aDate)
	{
		cDate date;
		date.seconds = aDate.seconds;
		date.minutes = aDate.minutes;
		date.hours = aDate.hours;
		date.month_day = aDate.month_day;
		date.month = aDate.month;
		date.year = aDate.year;
		date.week_day = aDate.week_day;
		date.year_day = aDate.year_day;
		return date;
	}

	//////////////////////////////////////////////////////////////////////////
	// FILE HANDLING
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	unsigned long cPlatform::GetFileSize(const tWString& asFileName)
	{
		return ohpl_l1_file_size(cString::To8Char(asFileName).c_str());
	}
	
	//-----------------------------------------------------------------------

	bool cPlatform::CopyFileToBuffer(const tWString& asFileName, void *apBuffer, unsigned long alSize)
	{
		FILE *pFile = OpenFile(asFileName, _W("r"));
		if (pFile==NULL) return false;
		fread(apBuffer, sizeof(char), alSize, pFile);
		
		fclose(pFile);
		return true;
	}

	//-----------------------------------------------------------------------

	bool cPlatform::FileExists(const tWString& asFileName)
	{
		return ohpl_l1_file_exists(cString::To8Char(asFileName).c_str());
	}

	//-----------------------------------------------------------------------

	void cPlatform::RemoveFile(const tWString& asFilePath)
	{
		ohpl_l1_remove_file(cString::To8Char(asFilePath).c_str());
	}

	//-----------------------------------------------------------------------
#define COPY_BUFFSIZE 8192

	bool cPlatform::CloneFile(	const tWString& asSrcFileName,const tWString& asDestFileName,
		bool abFailIfExists)
	{
		ohpl_h1_clone_file(cString::To8Char(asSrcFileName).c_str(), cString::To8Char(asDestFileName).c_str());
		return true;
	}

	//-----------------------------------------------------------------------

	bool cPlatform::CreateFolder(const tWString& asPath)
	{
		return ohpl_l1_create_folder(cString::To8Char(asPath).c_str());
	}
	
	//-----------------------------------------------------------------------

	bool cPlatform::RemoveFolder(const tWString& asPath, bool abDeleteAllFiles, bool abDeleteAllSubFolders)
	{
		////////////////////
		// Remove any files in the directory
		if(abDeleteAllFiles)
		{
			tWStringList lstFiles;
            FindFilesInDir(lstFiles,asPath,_W("*"), true);
			for(tWStringListIt it = lstFiles.begin(); it != lstFiles.end(); ++it)
			{
				tWString sFilePath = cString::SetFilePathW(*it, asPath);
				RemoveFile(sFilePath);
			}
		}
		
		////////////////////
		// Remove any sub folders in the directory
		if(abDeleteAllSubFolders)
		{
			tWStringList lstFolders;
			FindFoldersInDir(lstFolders, asPath,true, false);
			for(tWStringListIt it = lstFolders.begin(); it != lstFolders.end(); ++it)
			{
				tWString sFolderPath = cString::SetFilePathW(*it, asPath);
				RemoveFolder(sFolderPath, abDeleteAllFiles,abDeleteAllSubFolders);
			}
		}
		
		if(rmdir(cString::To8Char(asPath).c_str())!=0)
		{
//			wchar_t sTempString[2048];
//			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,0,GetLastError(),0,sTempString,2048,NULL);
//			Error("Could not remove folder: '%s': %s",cString::To8Char(sPath).c_str(), cString::To8Char(sTempString).c_str());
			return false;
		}
		return true;
	}

	//-----------------------------------------------------------------------

	bool cPlatform::FolderExists(const tWString& asPath)
	{
		return ohpl_l1_folder_exists(cString::To8Char(asPath).c_str());
	}

	//-----------------------------------------------------------------------

	tWString cPlatform::GetFullFilePath(const tWString& asFilePath)
	{
		tString sReal;
		ohpl_l1_full_path(cString::To8Char(asFilePath).c_str(), L1Utf8Sink, &sReal);
		return cString::To16Char(sReal);
	}
	
	//-----------------------------------------------------------------------
	
	FILE *cPlatform::OpenFile(const tWString& asFileName, const tWString asMode)
	{
		return fopen(cString::To8Char(asFileName).c_str(), cString::To8Char(asMode).c_str());
	}

	//-----------------------------------------------------------------------

	cDate cPlatform::FileModifiedDate(const tWString& asFilePath)
	{
		return L1ToDate(ohpl_l1_file_date(cString::To8Char(asFilePath).c_str(), false));
	}

	//-----------------------------------------------------------------------

	cDate cPlatform::FileCreationDate(const tWString& asFilePath)
	{
		return L1ToDate(ohpl_l1_file_date(cString::To8Char(asFilePath).c_str(), true));
	}

	//-----------------------------------------------------------------------
	void cPlatform::FindFilesInDir(tWStringList &alstStrings,const tWString& asDir, const tWString& asMask, bool abAddHidden)
	{
		ohpl_l1_find_files(cString::To8Char(asDir).c_str(), (const uint32_t*)asMask.data(), asMask.size(), L1NameSink, &alstStrings);
	}

	//-----------------------------------------------------------------------
	void cPlatform::FindFoldersInDir(tWStringList &alstStrings,const tWString& asDir, bool abAddHidden, bool abAddUpFolder)
	{
		ohpl_l1_find_folders(cString::To8Char(asDir).c_str(), abAddHidden, abAddUpFolder, L1NameSink, &alstStrings);
	}

	//-----------------------------------------------------------------------
#ifdef __linux__
	tString cPlatform::GetDataDir()
	{
		tString temp;
		ohpl_h1_data_dir(L1Utf8Sink, &temp);
		return temp;
	}
#endif

	//-----------------------------------------------------------------------

	tWString cPlatform::GetWorkingDir()
	{
		tString sDir;
		ohpl_l1_working_dir(L1Utf8Sink, &sDir);
		return cString::To16Char(sDir);
	}

	//-----------------------------------------------------------------------

	void cPlatform::CreateMessageBoxBase(eMsgBoxType eType, const wchar_t* asCaption, const wchar_t* fmt, va_list ap)
	{
		wchar_t text[2048];

		if (fmt == NULL)
			return;
		vswprintf(text, 2047, fmt, ap);

		tWString sMess = _W("");

		sMess += text;

		tString sCaption8 = cString::To8Char(asCaption);
		tString sMessage8 = cString::To8Char(sMess);
		ohpl_l1_message_box((unsigned)eType, sCaption8.c_str(), sMessage8.c_str());
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// SYSTEM STATS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	ePlatform cPlatform::GetPlatform()
	{
#if defined(__linux__)
		return ePlatform_Linux;
#elif defined(__APPLE__)
        return ePlatform_Mac;
#else
#error Platfom Unsupported
#endif
	}

	//-----------------------------------------------------------------------
#if defined(__linux__) && defined(__LP64__)
	tString cPlatform::msName = "Linux x86_64";
#elif defined(__linux__)
    tString cPlatform::msName = "Linux x86";
#elif defined(__APPLE__) && (defined(__PPC__) || defined(__ppc__))
    tString cPlatform::msName = "Mac OS X PowerPC";
#elif defined(__APPLE__) && defined(__LP64__)
	tString cPlatform::msName = "Mac OS X x86_64";
#elif defined(__APPLE__)
	tString cPlatform::msName = "Mac OS X x86";
#else
#error Platform Unsupported
#endif

	//-----------------------------------------------------------------------

	cDate cPlatform::GetDate()
	{
		return L1ToDate(ohpl_l1_date_now());
	}

	//-----------------------------------------------------------------------

	static void PathSink(void *apCtx, const unsigned char *apData, size_t alLen)
	{
		static_cast<tString*>(apCtx)->assign(reinterpret_cast<const char*>(apData), alLen);
	}

	tWString cPlatform::GetSystemSpecialPath(eSystemPath aPathType)
	{
		tString sPath;
		if(!ohpl_platform_special_path(aPathType, PathSink, &sPath))
		{
			// Known kinds fail only with HOME unset, where the C++ original threw from tString(nullptr).
			if(aPathType <= eSystemPath_XDGPictures) throw std::logic_error("GetSystemSpecialPath: HOME is not set");
			return _W("");
		}
		return cString::To16Char(sPath);
	}

	//-----------------------------------------------------------------------

	unsigned long cPlatform::GetSystemAvailableDrives()
	{
		/* @THOMAS Mac and Linux do not have an equivilent of Drives everything is underone tree */
		return 0x0;
	}

	//////////////////////////////////////////////////////////////////////////
	// SYSTEM COMMANDS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	void cPlatform::OpenBrowserWindow ( const tWString& asURL )
	{
		tString sURL = cString::To8Char(asURL);
		const char *vArgs[] = { "xdg-open", sURL.c_str() };
		pid_t pID = ohpl_h1_spawn("xdg-open", vArgs, 2);
		if (pID < 0) {
			Error("Could not Open URL %ls\n", asURL.c_str());
		} else {
			Log("Opened URL %ls\n", asURL.c_str());
		}
	}

	bool cPlatform::OpenFileOnShell( const tWString& asPath )
	{
		tString sPath = cString::To8Char(asPath);
		const char *vArgs[] = { "xdg-open", "xdg-open", sPath.c_str() };
		pid_t pID = ohpl_h1_spawn("/bin/bash", vArgs, 3);
		if (pID < 0) {
			Error("Could not Open File %ls\n", asPath.c_str());
			return false;
		}
		Log("Opened File %ls\n", asPath.c_str());
		return true;
	}

	bool cPlatform::RunProgram( const tWString& asPath, const tWString& asParams )
	{
		tStringVec tvArgs;
		tString sSepp = " ";
		cString::GetStringVec( cString::To8Char(asParams), tvArgs, &sSepp);
		tString sTemp = cString::To8Char(asPath);
		std::vector<const char*> vArgs(1, sTemp.c_str());
		for (size_t i = 0; i < tvArgs.size(); ++i) vArgs.push_back(tvArgs[i].c_str());
		pid_t pID = ohpl_h1_spawn(sTemp.c_str(), vArgs.data(), vArgs.size());
		if (pID < 0) {
			Error("Could not Launch program %ls\n", asPath.c_str());
			return false;
		}
		Log("Launched Program %ls\n", asPath.c_str());
		return true;
	}
}
