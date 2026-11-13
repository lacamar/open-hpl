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

#ifndef HPL_LANGUAGE_FILE_H
#define HPL_LANGUAGE_FILE_H

#include "system/SystemTypes.h"
#include "resources/RustResourcesH2.h"

#define OHPL_LANG_FOREACH 1

namespace hpl {

	class cResources;

	tWString GetDecodedString(const tString &asString);

	//--------------------------------

	class cLanguageFile
	{
	public:
		cLanguageFile(cResources *apResources);
		~cLanguageFile();
		
		bool AddFromFile(const tWString& asFile, bool abAddResourceDirs, const tWString& asAltPath = _W(""));
		
		const tWString& Translate(const tString& asCat, const tString& asName);

		void SetEntry(const tString& asCat, const tString& asName, const tWString& asText);
		bool HasEntry(const tString& asCat, const tString& asName);
		void ForEach(void (*apFunc)(void*, const tString&, const tString&, const tWString*), void *apCtx);
	private:
		OhplH2Lang *mpRs;
		tWString mwsEmpty;

		cResources *mpResources;
	};

};
#endif // HPL_LANGUAGE_FILE_H
