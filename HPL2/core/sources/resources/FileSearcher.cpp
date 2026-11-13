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


#include "resources/LowLevelResources.h"

namespace hpl {

	//-----------------------------------------------------------------------

	cFileSearcher::cFileSearcher() : mpRs(ohpl_h2_fs_new())
	{
		msNull = _W("");
	}

	//-----------------------------------------------------------------------

	cFileSearcher::~cFileSearcher()
	{
		ohpl_h2_fs_free(mpRs);
	}

	//-----------------------------------------------------------------------

	void cFileSearcher::AddDirectory(const tWString& asSearchPath, const tString &asMask, bool abAddSubDirectories)
	{
		ohpl_h2_fs_add_dir(mpRs, reinterpret_cast<const uint32_t*>(asSearchPath.data()), asSearchPath.size(), asMask.data(), asMask.size(), abAddSubDirectories);
	}

	//-----------------------------------------------------------------------

	void cFileSearcher::ClearDirectories()
	{
		ohpl_h2_fs_clear(mpRs);
		mvPaths.clear();
	}

	//-----------------------------------------------------------------------

	const tWString& cFileSearcher::GetFilePath(const tString& asFileNameAndPath, int *apEqualCount)
	{
		int64_t lRsId = ohpl_h2_fs_get(mpRs, asFileNameAndPath.data(), asFileNameAndPath.size(), apEqualCount);
		if(lRsId < 0) return msNull;
		if(mvPaths.size() <= (size_t)lRsId) mvPaths.resize(lRsId + 1);
		if(mvPaths[lRsId].empty())
		{
			size_t lLen;
			const uint32_t *pPath = ohpl_h2_fs_path(mpRs, (uint32_t)lRsId, &lLen);
			mvPaths[lRsId].assign(reinterpret_cast<const wchar_t*>(pPath), lLen);
		}
		return mvPaths[lRsId];
	}

	//-----------------------------------------------------------------------
}
