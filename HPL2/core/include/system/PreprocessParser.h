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

#ifndef HPL_PREPROCESS_PARSER_H
#define HPL_PREPROCESS_PARSER_H

#include "system/SystemTypes.h"

namespace hpl {

	//---------------------------------------

	typedef std::map<tString, tString> tParseVarMap;
	typedef tParseVarMap::iterator tParseVarMapIt;

	
	class cParserVarContainer
	{
	public:
		void Add(const tString& asName, const tString& asVal="");
		void Add(const tString& asName, int alVal);
		void Add(const tString& asName, float afVal);
		bool Remove(const tString& asName);
		tString* Get(const tString& asName);
		
		tParseVarMap* GetMapPtr(){ return &m_mapVars;}

		void Clear();

	private:
		tParseVarMap m_mapVars;
	};

	//---------------------------------------

	class cPreprocessParser
	{
	public:
		cPreprocessParser();
		~cPreprocessParser();

        bool Parse(	const tString* apInput, tString *apOutput, cParserVarContainer *apVarContainer,
					const tWString& asDir=_W(""));

		cParserVarContainer* GetEnvVarContainer(){ return &mEnvironmentVars;}
		cParserVarContainer* GetParsingVarContainer(){ return &mParsingVars;}
		
	private:
		cParserVarContainer mEnvironmentVars;
		cParserVarContainer mParsingVars;
	};

};
#endif // HPL_PREPROCESS_PARSER_H
