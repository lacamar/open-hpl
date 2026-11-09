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

#include "resources/BinaryBuffer.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"

#include <cstdio>
#include <vector>

namespace hpl {

	bool cBinaryBuffer::Load()
	{
		if(msFile == _W(""))
		{
			Error("Could not save binary file. File not set!\n");
			return false;
		}

		return Load(msFile);
	}

	bool cBinaryBuffer::Load(const tWString& asFile)
	{
		FILE *pFile = cPlatform::OpenFile(asFile,_W("rb"));
		if(pFile==NULL)
		{
			Error("Could not open binary file '%s'\n", cString::To8Char(asFile).c_str());
			return false;
		}

		fseek(pFile,0,SEEK_END);
		size_t lFileSize = ftell(pFile);
		rewind(pFile);

		std::vector<uint8_t> vData(lFileSize);
		size_t lOut = fread(vData.data(), lFileSize, 1, pFile);
		fclose(pFile);

		ohpl_bb_assign(mpImpl, vData.data(), lFileSize);

		if(lOut == 0)
		{
			Error("Could not read from binary file '%s'\n", cString::To8Char(asFile).c_str());
			return false;
		}

		return true;
	}

	bool cBinaryBuffer::Save()
	{
		if(msFile == _W(""))
		{
			Error("Could not save binary file. File not set!\n");
			return false;
		}

		return Save(msFile);
	}

	bool cBinaryBuffer::Save(const tWString& asFile)
	{
		FILE *pFile = cPlatform::OpenFile(asFile,_W("wb"));
		if(pFile==NULL)
		{
			Error("Could not open binary file '%s'\n", cString::To8Char(asFile).c_str());
			return false;
		}

		size_t lOut = fwrite(GetDataPointer(), GetSize(), 1, pFile);
		fclose(pFile);

		if(lOut == 0)
		{
			Error("Could not write to binary file '%s'\n", cString::To8Char(asFile).c_str());
			return false;
		}

		return true;
	}

	void cBinaryBuffer::LoadFromCharEncode(const tString& asInputData, size_t alSize)
	{
		std::vector<uint8_t> vData(alSize);
		cString::DecodeDataFromTextString(asInputData, vData.data(), alSize);
		ohpl_bb_assign(mpImpl, vData.data(), alSize);
	}

	void cBinaryBuffer::SaveToCharEncode(tString& asOutputData)
	{
		cString::EncodeDataToTextString(GetDataPointer(), GetSize(), asOutputData);
	}
}
