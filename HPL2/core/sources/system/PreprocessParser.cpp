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

#include "system/PreprocessParser.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"
#include "system/RustPreprocess.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// VAR CONTAINER
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cParserVarContainer::Add(const tString& asName, const tString& asVal)
	{
		//If it already exists, just change value
		tParseVarMapIt it = m_mapVars.find(asName);
		if(it != m_mapVars.end())
		{
			it->second = asVal;
			return;
		}
		
		m_mapVars.insert(tParseVarMap::value_type(asName,asVal));
	}

	void cParserVarContainer::Add(const tString& asName, int alVal)
	{
		Add(asName, cString::ToString(alVal));
	}
	
	void cParserVarContainer::Add(const tString& asName, float afVal)
	{	
		char sTemp[1024];
		sprintf(sTemp, "%f", afVal);

		Add(asName, sTemp);
	}

	//-----------------------------------------------------------------------

	bool cParserVarContainer::Remove(const tString& asName)
	{
		tParseVarMapIt it = m_mapVars.find(asName);
		if(it == m_mapVars.end()) return false;

		m_mapVars.erase(it);
		return true;
	}

	//-----------------------------------------------------------------------

	tString* cParserVarContainer::Get(const tString& asName)
	{
		tParseVarMapIt it = m_mapVars.find(asName);
		if(it == m_mapVars.end()) return NULL;
        
		return &it->second;
	}

	
	//-----------------------------------------------------------------------
	
	void cParserVarContainer::Clear()
	{
		m_mapVars.clear();
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	namespace {

		struct cIncludeCtx
		{
			const tWString* mpDir;
		};

		void AppendSink(void* apCtx, const char* apData, size_t alLen) noexcept
		{
			static_cast<tString*>(apCtx)->append(apData, alLen);
		}

		void DefineSink(void* apCtx, const char* apName, size_t alNameLen, const char* apVal, size_t alValLen) noexcept
		{
			static_cast<cParserVarContainer*>(apCtx)->Add(tString(apName, alNameLen), tString(apVal, alValLen));
		}

		bool IncludeCallback(void* apCtx, const char* apName, size_t alLen, void (*apSink)(void*, const char*, size_t), void* apSinkCtx) noexcept
		{
			const tWString& sDir = *static_cast<cIncludeCtx*>(apCtx)->mpDir;
			tString sFile(apName, alLen);
			tWString sPath;
			if(sDir!=_W(""))
				sPath = cString::SetFilePathW(cString::To16Char(sFile), sDir);
			else
				sPath = cString::To16Char(sFile);

			if(cPlatform::FileExists(sPath))
			{
				unsigned int lFileSize = cPlatform::GetFileSize(sPath);
				tString sFileData;
				sFileData.resize(lFileSize);
				cPlatform::CopyFileToBuffer(sPath,&sFileData[0],lFileSize);
				apSink(apSinkCtx, sFileData.data(), sFileData.size());
				return true;
			}

			tString sPath8 = cString::To8Char(sPath);
			apSink(apSinkCtx, sPath8.data(), sPath8.size());
			return false;
		}

		void LogCallback(void*, bool abError, const char* apMsg, size_t alLen) noexcept
		{
			tString sMsg(apMsg, alLen);
			if(abError) Error("%s", sMsg.c_str());
			else Warning("%s", sMsg.c_str());
		}

		void FlattenVars(cParserVarContainer& aVars, std::vector<ohpl_pp_kv>& avOut)
		{
			for(tParseVarMapIt it = aVars.GetMapPtr()->begin(); it != aVars.GetMapPtr()->end(); ++it)
			{
				ohpl_pp_kv kv = { it->first.data(), it->first.size(), it->second.data(), it->second.size() };
				avOut.push_back(kv);
			}
		}
	}

	//-----------------------------------------------------------------------

	cPreprocessParser::cPreprocessParser()
	{
	}

	cPreprocessParser::~cPreprocessParser()
	{
	}

	//-----------------------------------------------------------------------

	bool cPreprocessParser::Parse(const tString* apInput, tString *apOutput, cParserVarContainer *apVarContainer,const tWString& asDir)
	{
		mParsingVars.Clear();

		std::vector<ohpl_pp_kv> vEnv, vCur;
		FlattenVars(mEnvironmentVars, vEnv);
		if(apVarContainer) FlattenVars(*apVarContainer, vCur);

		cIncludeCtx includeCtx = { &asDir };
		return ohpl_pp_parse(apInput->data(), apInput->size(),
							 vEnv.data(), vEnv.size(), vCur.data(), vCur.size(),
							 IncludeCallback, LogCallback, &includeCtx,
							 AppendSink, apOutput, DefineSink, &mParsingVars);
	}

	//-----------------------------------------------------------------------

}
