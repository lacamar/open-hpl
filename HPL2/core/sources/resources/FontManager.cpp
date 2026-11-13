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

#include "resources/FontManager.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "resources/Resources.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "resources/ImageManager.h"

#include "graphics/FontData.h"
#include "resources/RustResMgr.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cFontManager::cFontManager(cGraphics* apGraphics,cGui *apGui,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		mpGui = apGui;
	}

	cFontManager::~cFontManager()
	{
		if(mpRs) { ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_FONT); return; }
		DestroyAll();
		Log(" Done with fonts\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iFontData* cFontManager::CreateFontData(const tString& asName, int alSize,unsigned short alFirstChar,
											unsigned short alLastChar)
	{
		if(mpRs) return H3Res<iFontData>(ohpl_h3f_font(mpRs, H3Ops(), RM_STR(asName), alSize, alFirstChar, alLastChar));
		iFontData* pFont;

		BeginLoad(asName);

		cRmEnv env(mpFileSearcher, this);
		cRmOut out;
		ohpl_rm_resolve_simple(env.raw(), 1, RM_STR(asName), out.raw());
		const tString& asNewName = out.msName;
		const tWString& sPath = out.msPath;

		pFont = static_cast<iFontData*>(out.Res());

		if(pFont==NULL && sPath!=_W(""))
		{
			pFont = mpGraphics->GetLowLevel()->CreateFontData(asNewName);
			pFont->SetUp(mpResources,mpGui);

			int lKind = ohpl_rm_font_kind(RM_STR(asName));

			//True Type Font
			if(lKind == 1)
			{
				if(pFont->CreateFromFontFile(sPath,alSize,alFirstChar,alLastChar)==false){
					hplDelete(pFont);
					EndLoad();
					return NULL;
				}
			}
			//Angel code font type
			else if(lKind == 2)
			{
				if(pFont->CreateFromBitmapFile(sPath)==false){
					hplDelete(pFont);
					EndLoad();
					return NULL;
				}
			}
			else
			{
				Error("Font '%s' has an unkown extension!\n",asName.c_str());
				hplDelete(pFont);
				EndLoad();
				return NULL;
			}

			AddResource(pFont);
		}

		if(pFont)pFont->IncUserCount();
		else Error("Couldn't create font '%s'\n",asNewName.c_str());

		EndLoad();
		return pFont;
	}

	//-----------------------------------------------------------------------

	void cFontManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cFontManager::Destroy(iResourceBase* apResource)
	{
		if(mpRs) { ohpl_h3f_destroy_standard(mpRs, H3Ops(), apResource); return; }
		if(ohpl_rm_destroy_standard(apResource->GetMeta())){
			RemoveResource(apResource);
			hplDelete(apResource);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void* cFontManager::H3Create(OhplH3Create &aCreate)
	{
		iFontData *pFont = mpGraphics->GetLowLevel()->CreateFontData(H3Name(aCreate.name));
		pFont->SetUp(mpResources,mpGui);
		bool bOk = false;
		if(aCreate.kind == OHPL_H3_FONT_UNKNOWN) return static_cast<iResourceBase*>(pFont);
		if(aCreate.kind == OHPL_H3_FONT_TTF) bOk = pFont->CreateFromFontFile(H3Wide(aCreate.path),aCreate.i[0],(unsigned short)aCreate.i[1],(unsigned short)aCreate.i[2]);
		else if(aCreate.kind == OHPL_H3_FONT_BITMAP) bOk = pFont->CreateFromBitmapFile(H3Wide(aCreate.path));
		if(bOk==false)
		{
			hplDelete(pFont);
			return NULL;
		}
		return static_cast<iResourceBase*>(pFont);
	}
}
