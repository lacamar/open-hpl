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

#include "resources/LanguageFile.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "resources/FileSearcher.h"
#include "system/RustXml.h"

#include "resources/Resources.h"
#include "resources/FileSearcher.h"
#include "system/Platform.h"
#include "impl/RustBackends.h"
#include "resources/RustResourcesH3.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	static void LangDecodeSink(void *apCtx, const int *apText, size_t alLen)
	{
		tWString *pOut = static_cast<tWString*>(apCtx);
		pOut->clear();
		for(size_t i=0; i<alLen; ++i) pOut->push_back((wchar_t)apText[i]);
	}

	tWString GetDecodedString(const tString &asString)
	{
		tWString wsText;
		ohpl_xml_lang_decode(asString.data(), asString.size(), LangDecodeSink, &wsText);
		return wsText;
	}

	//-----------------------------------------------------------------------

	cLanguageFile::cLanguageFile(cResources *apResources) : mpRs(NULL)
	{
		mpResources = apResources;
		if(OhplBackendRustRequested() == false) return;

		OhplH2LangOps ops = {};
		ops.ctx = this;
		ops.log = [](void*, int32_t alLevel, const uint8_t *apMsg, size_t alLen)
		{
			tString sMsg(reinterpret_cast<const char*>(apMsg), alLen);
			if(alLevel == 2) Error("%s", sMsg.c_str());
			else Warning("%s", sMsg.c_str());
		};
		ops.add_dir = [](void *c, const uint32_t *p, size_t n, bool abSubDirs)
		{
			static_cast<cLanguageFile*>(c)->mpResources->AddResourceDir(tWString(reinterpret_cast<const wchar_t*>(p), n), abSubDirs);
		};
		ops.file_exists = [](void*, const uint32_t *p, size_t n){ return cPlatform::FileExists(tWString(reinterpret_cast<const wchar_t*>(p), n)); };
		ops.read_file = [](void*, const uint32_t *p, size_t n, void (*apSink)(void*, const uint8_t*, size_t), void *apCtx, bool *apReadOk) -> bool
		{
			FILE *pFile = cPlatform::OpenFile(tWString(reinterpret_cast<const wchar_t*>(p), n), _W("rb"));
			if(pFile == NULL) return false;
			fseek(pFile, 0, SEEK_END);
			long lLength = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);
			std::vector<char> vData(lLength > 0 ? lLength : 0);
			bool bRead = lLength > 0 && fread(vData.data(), lLength, 1, pFile) == 1;
			fclose(pFile);
			*apReadOk = lLength <= 0 || bRead;
			apSink(apCtx, reinterpret_cast<const uint8_t*>(vData.data()), vData.size());
			return true;
		};
		ops.text_new = [](void*, const int32_t *p, size_t n) -> void*
		{
			tWString *pText = new tWString();
			for(size_t i=0; i<n; ++i) pText->push_back((wchar_t)p[i]);
			return pText;
		};
		ops.text_free = [](void*, void *apText){ delete static_cast<tWString*>(apText); };
		mpRs = ohpl_h2_lang_new(&ops);
	}

	//-----------------------------------------------------------------------

	cLanguageFile::~cLanguageFile()
	{
		if(mpRs) { ohpl_h2_lang_free(mpRs); return; }
		STLMapDeleteAll(m_mapCategories);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	struct cLangCtx
	{
		cResources *mpResources;
		tLanguageCategoryMap *mpCategories;
		cLanguageCategory *mpCategory;
		tWString msAltPath;
		const tWString *mpFile;
	};

	static void LangDirectory(void *apCtx, const char *asPath, size_t alLen, bool abAddSubDirs)
	{
		cLangCtx *pCtx = static_cast<cLangCtx*>(apCtx);
		tString sPath(asPath, alLen);
#ifndef HPL_MINIMAL
		if (pCtx->msAltPath.length()) {
			pCtx->mpResources->AddResourceDir(pCtx->msAltPath + cString::To16Char(sPath),abAddSubDirs);
		}
		pCtx->mpResources->AddResourceDir(cString::To16Char(sPath),abAddSubDirs);
#endif
	}

	static void LangNoResources(void *apCtx)
	{
		Warning("No resources element found in '%ls'\n",static_cast<cLangCtx*>(apCtx)->mpFile->c_str());
	}

	static void LangCategory(void *apCtx, const char *asName, size_t alLen)
	{
		cLangCtx *pCtx = static_cast<cLangCtx*>(apCtx);
		tString sCatName(asName, alLen);
		tLanguageCategoryMapIt CatIt = pCtx->mpCategories->find(sCatName);
		if(CatIt == pCtx->mpCategories->end())
		{
			pCtx->mpCategory = hplNew( cLanguageCategory, () );
			pCtx->mpCategories->insert(tLanguageCategoryMap::value_type(sCatName, pCtx->mpCategory));
		}
		else
		{
			pCtx->mpCategory = CatIt->second;
		}
	}

	static void LangEntry(void *apCtx, const char *asName, size_t alLen, const int *apText, size_t alTextLen)
	{
		cLangCtx *pCtx = static_cast<cLangCtx*>(apCtx);
		tString sEntryName(asName, alLen);
		if(pCtx->mpCategory->m_mapEntries.find(sEntryName) != pCtx->mpCategory->m_mapEntries.end())
		{
			return;
		}
		cLanguageEntry *pEntry = hplNew( cLanguageEntry, () );
		for(size_t i=0; i<alTextLen; ++i) pEntry->mwsText.push_back((wchar_t)apText[i]);
		pCtx->mpCategory->m_mapEntries.insert(tLanguageEntryMap::value_type(sEntryName,pEntry));
	}

	bool cLanguageFile::AddFromFile(const tWString& asFile, bool abAddResourceDirs, const tWString& asAltPath)
	{
		if(mpRs) return ohpl_h2_lang_add_file(mpRs, reinterpret_cast<const uint32_t*>(asFile.data()), asFile.size(), abAddResourceDirs,
											  reinterpret_cast<const uint32_t*>(asAltPath.data()), asAltPath.size());
		tWString sPath = asFile;
		if (asAltPath.length() > 0 && cPlatform::FileExists(asAltPath + asFile)) {
			sPath = asAltPath + asFile;
		}

		FILE *pFile = cPlatform::OpenFile(sPath, _W("rb"));
		bool bRet = false;
		if (pFile!=NULL) {
			long lLength = 0;
			fseek(pFile, 0, SEEK_END);
			lLength = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);
			std::vector<char> vData(lLength > 0 ? lLength : 0);
			bool bRead = lLength > 0 && fread(vData.data(), lLength, 1, pFile) == 1;
			fclose(pFile);

			cLangCtx ctx = { mpResources, &m_mapCategories, NULL, asAltPath, &asFile };
			OhplLangSink sink = { &ctx, LangDirectory, LangNoResources, LangCategory, LangEntry };
			bRet = ohpl_xml_lang_read(vData.data(), vData.size(), lLength <= 0 || bRead, abAddResourceDirs, &sink);
		}
		if (!bRet)
		{
			Error("Couldn't load language file '%ls'\n",asFile.c_str());
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------

	const tWString& cLanguageFile::Translate(const tString& asCat, const tString& asName)
	{
		if(mpRs)
		{
			void *pText = ohpl_h2_lang_translate(mpRs, RM_STR(asCat), RM_STR(asName));
			return pText ? *static_cast<tWString*>(pText) : mwsEmpty;
		}
		tLanguageCategoryMapIt CatIt = m_mapCategories.find(asCat);
		if(CatIt == m_mapCategories.end())
		{
			Warning("Could not find language file category '%s'\n",asCat.c_str());
			return mwsEmpty;
		}
		
        cLanguageCategory *pCategory = CatIt->second;
		tLanguageEntryMapIt EntryIt = pCategory->m_mapEntries.find(asName);
		if(EntryIt == pCategory->m_mapEntries.end())
		{
			Warning("Could not find language file entry '%s'\n",asName.c_str());
			return mwsEmpty;
		}

		cLanguageEntry *pEntry = EntryIt->second;

		return pEntry->mwsText;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	
	//-----------------------------------------------------------------------

	void cLanguageFile::SetEntry(const tString& asCat, const tString& asName, const tWString& asText)
	{
		if(mpRs)
		{
			std::vector<int32_t> vText(asText.begin(), asText.end());
			ohpl_h2_lang_set(mpRs, RM_STR(asCat), RM_STR(asName), vText.data(), vText.size());
			return;
		}
		cLanguageCategory *&pCategory = m_mapCategories[asCat];
		if(pCategory == NULL) pCategory = hplNew( cLanguageCategory, () );
		pCategory->m_mapEntries[asName] = hplNew( cLanguageEntry, () );
		pCategory->m_mapEntries[asName]->mwsText = asText;
	}

	bool cLanguageFile::HasEntry(const tString& asCat, const tString& asName)
	{
		if(mpRs) return ohpl_h2_lang_has(mpRs, RM_STR(asCat), RM_STR(asName));
		tLanguageCategoryMapIt it = m_mapCategories.find(asCat);
		return it != m_mapCategories.end() && it->second->m_mapEntries.count(asName) > 0;
	}

	void cLanguageFile::ForEach(void (*apFunc)(void*, const tString&, const tString&, const tWString*), void *apCtx)
	{
		if(mpRs)
		{
			struct cEach { void (*mpFunc)(void*, const tString&, const tString&, const tWString*); void *mpCtx; } each = { apFunc, apCtx };
			ohpl_h2_lang_each(mpRs, [](void *c, const uint8_t *pCat, size_t lCat, const uint8_t *pName, size_t lName, void *pText)
			{
				cEach *pEach = static_cast<cEach*>(c);
				pEach->mpFunc(pEach->mpCtx, tString(reinterpret_cast<const char*>(pCat), lCat), tString(reinterpret_cast<const char*>(pName), lName), static_cast<tWString*>(pText));
			}, &each);
			return;
		}
		for(tLanguageCategoryMapIt cit = m_mapCategories.begin(); cit != m_mapCategories.end(); ++cit)
		{
			if(cit->second->m_mapEntries.empty()) apFunc(apCtx, cit->first, "", NULL);
			for(tLanguageEntryMapIt eit = cit->second->m_mapEntries.begin(); eit != cit->second->m_mapEntries.end(); ++eit)
				apFunc(apCtx, cit->first, eit->first, &eit->second->mwsText);
		}
	}
}
