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

#include "resources/FileSearcher.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"

#include "resources/LowLevelResources.h"

namespace hpl {

	//-----------------------------------------------------------------------

	cFileSearcher::cFileSearcher() : mpFiles(ohpl_fs_new())
	{
		msNull = _W("");
	}

	//-----------------------------------------------------------------------

	cFileSearcher::~cFileSearcher()
	{
		ohpl_fs_free(mpFiles);
	}

	//-----------------------------------------------------------------------

	void cFileSearcher::AddDirectory(const tWString& asSearchPath, const tString &asMask, bool abAddSubDirectories)
	{
		//Make the path with only "/" and lower case.
		tWString sPath = cString::ReplaceCharToW(asSearchPath,_W("\\"),_W("/"));

		///////////////////////////////
		//Add all files in directory
		tWStringList lstFileNames;

		cPlatform::FindFilesInDir(lstFileNames,sPath, cString::To16Char(asMask));
		tWString sFullPath = cString::ReplaceCharToW(cPlatform::GetFullFilePath(sPath), _W("\\"),_W("/"));

		for(tWStringListIt it = lstFileNames.begin();it!=lstFileNames.end();it++)
		{
			tWString& sFile = *it;
			tString sLowFile = cString::ToLowerCase(cString::To8Char(sFile));
			tWString sFilePath = cString::SetFilePathW(sFile,sFullPath);

			//Skipped when the first file of that name already has this path
			auto SamePath = [&](uint32_t alId){ return mvPaths[alId] == sFilePath; };
			int64_t lId = ohpl_fs_add(mpFiles, sLowFile.data(), sLowFile.size(),
				reinterpret_cast<const uint32_t*>(sFilePath.data()), sFilePath.size(),
				[](void *apCtx, uint32_t alId){ return (*static_cast<decltype(SamePath)*>(apCtx))(alId); }, &SamePath);
			if(lId >= 0) mvPaths.push_back(sFilePath);
		}

		//////////////////////////////////
		//Search sub directories if set.
		if(abAddSubDirectories)
		{
			tWStringList lstDirNames;
			cPlatform::FindFoldersInDir(lstDirNames,sPath,false);

			for(tWStringListIt it = lstDirNames.begin();it!=lstDirNames.end();it++)
			{
				tWString sNewPath = cString::SetFilePathW(*it, sPath);

				AddDirectory(sNewPath,asMask,true);
			}
		}
	}

	//-----------------------------------------------------------------------

	void cFileSearcher::ClearDirectories()
	{
		ohpl_fs_clear(mpFiles);
		mvPaths.clear();
	}

	//-----------------------------------------------------------------------

	const tWString& cFileSearcher::GetFilePath(const tString& asFileNameAndPath, int *apEqualCount)
	{
		int64_t lId = ohpl_fs_get(mpFiles, asFileNameAndPath.data(), asFileNameAndPath.size(), apEqualCount);
		if(lId < 0) return msNull;
		return mvPaths[lId];
	}

	//-----------------------------------------------------------------------
}
