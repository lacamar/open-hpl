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

/*
 *  LusBasePersonal.h
 *  Lux
 *
 *  Created by Edward Rudd on 8/27/10.
 *  Copyright 2010 Frictional Games All rights reserved.
 *
 */

#if defined(__linux__)
#include <sys/stat.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#endif

/////////////////////////
// Multi platform personal directory specifics.
#if defined(WIN32)
#define PERSONAL_RELATIVEROOT _W("")
#define PERSONAL_RELATIVEPIECES
#define PERSONAL_RELATIVEPIECES_COUNT 0
#define PERSONAL_SYSTEMPATH_TYPE eSystemPath_Personal
#elif defined(__linux__)
// XDG Base Directory Specification: saves/config/resources live under
// $XDG_DATA_HOME/open-hpl/ (see cPlatform::GetSystemSpecialPath()'s
// eSystemPath_XDGDataHome case), not a bare ~/.frictionalgames dotfile -
// PERSONAL_RELATIVEROOT plays the same "vendor folder" role either way, just
// resolved from a different, already-hidden root. See
// MigrateLegacyPersonalDir() below for the one-time move of any real data an
// older build already wrote to the old location.
#define PERSONAL_RELATIVEROOT _W("open-hpl/")
#define PERSONAL_RELATIVEPIECES _W("open-hpl"),
#define PERSONAL_RELATIVEPIECES_COUNT 1
#define PERSONAL_SYSTEMPATH_TYPE eSystemPath_XDGDataHome
#else
#define PERSONAL_RELATIVEROOT _W("Library/Application Support/Frictional Games/")
#define PERSONAL_RELATIVEPIECES _W("Library"), _W("Library/Application Support"), _W("Library/Application Support/Frictional Games"),
#define PERSONAL_RELATIVEPIECES_COUNT 3
#define PERSONAL_SYSTEMPATH_TYPE eSystemPath_Personal
#endif
#define PERSONAL_RELATIVEGAME_PARENT _W("Amnesia/")
#define PERSONAL_RESOURCES _W("local_resources/")
namespace hpl {

#if defined(__linux__)
// One-time best-effort migration of a real pre-XDG install
// (~/.frictionalgames/Amnesia/) to the new $XDG_DATA_HOME/open-hpl/Amnesia/
// location, so upgrading this package doesn't strand existing save games.
// Whole-subtree rename (not per-file), and only when the new location
// doesn't already exist - never overwrites anything. Failure (e.g. old
// location never existed, or new one's parent is on a different filesystem)
// is silently non-fatal: CreateBaseDirs() below creates a fresh tree either way.
inline void MigrateLegacyPersonalDir(const tWString &asNewGameParentDir)
{
	const char *pHome = getenv("HOME");
	if(pHome == NULL) return;

	// rename() requires the destination's parent to already exist - this runs before
	// CreateBaseDirs() below has had a chance to create it, so make sure of it here too
	// (just the one "open-hpl/" piece; asNewGameParentDir's own root, e.g. $XDG_DATA_HOME,
	// is assumed to already exist, same as everywhere else this session's XDG work does).
	tWString sParent = asNewGameParentDir;
	if(!sParent.empty() && cString::GetLastCharW(sParent) == _W("/")) sParent.resize(sParent.size()-1);
	size_t lSlashPos = sParent.find_last_of(_W('/'));
	if(lSlashPos != tWString::npos) sParent = sParent.substr(0, lSlashPos+1);
	if(cPlatform::FolderExists(sParent) == false) cPlatform::CreateFolder(sParent);

	tString sOldDir = tString(pHome) + "/.frictionalgames/Amnesia";
	tString sNewDir = cString::To8Char(asNewGameParentDir);
	// Strip a trailing slash - rename() on some filesystems is picky about it
	// on the destination when the source has none.
	if(!sNewDir.empty() && sNewDir[sNewDir.size()-1] == '/') sNewDir.resize(sNewDir.size()-1);

	struct stat oldStat, newStat;
	if(stat(sOldDir.c_str(), &oldStat) != 0) return;   // nothing to migrate
	if(stat(sNewDir.c_str(), &newStat) == 0) return;   // already migrated (or fresh install already has data)

	if(rename(sOldDir.c_str(), sNewDir.c_str()) == 0)
	{
		Log("Migrated legacy save/config directory '%s' -> '%s'\n", sOldDir.c_str(), sNewDir.c_str());
	}
	else
	{
		Log("Could not migrate legacy save/config directory '%s' -> '%s': %s (starting fresh there instead)\n",
			sOldDir.c_str(), sNewDir.c_str(), strerror(errno));
	}
}
#endif
inline void SetupBaseDirs(tWStringVec& vDirs, const tWString& asRelativeParent = _W(""), const tWString& asMainFolder = _W(""),
                                        bool userDir = false, const tWString& asCustomStoryPath = _W(""))
{
    vDirs.clear();
#if PERSONAL_RELATIVEPIECES_COUNT > 0
    tWString aDirs[] = { PERSONAL_RELATIVEPIECES };
    for (int i = 0; i < PERSONAL_RELATIVEPIECES_COUNT; ++i) {
        vDirs.push_back(aDirs[i]);
    }
#endif
    if (asRelativeParent.length()) {
        vDirs.push_back(PERSONAL_RELATIVEROOT + asRelativeParent);
        if (asMainFolder.length()) {
            vDirs.push_back(PERSONAL_RELATIVEROOT + asRelativeParent + asMainFolder + _W("/"));
        }
    }
    vDirs.push_back(PERSONAL_RELATIVEROOT PERSONAL_RELATIVEGAME_PARENT);
#ifndef HPL_MINIMAL
    iFileBrowser::msGameDir = cPlatform::GetWorkingDir();
    iFileBrowser::msPersonalDir = PERSONAL_RELATIVEROOT PERSONAL_RELATIVEGAME_PARENT;
#endif
    if (userDir) {
        vDirs.push_back(PERSONAL_RELATIVEROOT PERSONAL_RELATIVEGAME_PARENT PERSONAL_RESOURCES);
        if (asCustomStoryPath.length()) {
            vDirs.push_back(PERSONAL_RELATIVEROOT PERSONAL_RELATIVEGAME_PARENT PERSONAL_RESOURCES
                            + asCustomStoryPath + _W("/"));
        }
    }
}

inline void CreateBaseDirs(const tWStringVec& vDirs, const tWString& asRoot)
{
	//Check if directories exist and if not create
    for(tWStringVec::const_iterator it = vDirs.begin(); it != vDirs.end(); ++it)
	{
		tWString sDir = asRoot + (*it);
		if(cPlatform::FolderExists(sDir)) continue;

		cPlatform::CreateFolder(sDir);
	}
}
}
