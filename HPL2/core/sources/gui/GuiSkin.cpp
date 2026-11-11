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

#include "gui/GuiSkin.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"

#include "gui/RustGuiSkin.h"

#include "gui/Gui.h"
#include "gui/GuiGfxElement.h"

#include "resources/Resources.h"
#include "resources/FontManager.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// FONT
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cGuiSkinFont::cGuiSkinFont(cGui *apGui)
	{
		mpGui = apGui;

		mpMaterial = NULL;
	}
	cGuiSkinFont::~cGuiSkinFont()
	{
		//Skip deleting font...
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cGuiSkin::cGuiSkin(const tString & asName,cGui *apGui)
	{
		mpGui = apGui;
		msName = asName;

		mvGfxElements.resize(eGuiSkinGfx_LastEnum, NULL);
		mvFonts.resize(eGuiSkinFont_LastEnum, NULL);
		mvAttributes.resize(eGuiSkinAttribute_LastEnum, cVector3f(0) );
	}

	//-----------------------------------------------------------------------

	cGuiSkin::~cGuiSkin()
	{
		for(size_t i=0; i< mvGfxElements.size(); ++i)
		{
			if(mvGfxElements[i]) mpGui->DestroyGfx(mvGfxElements[i]);
		}

		STLDeleteAll(mvFonts);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cGuiSkin::LoadFromFile(const tWString &asFile)
	{
		//////////////////////////////////
		//Open document
		FILE *pFile = cPlatform::OpenFile(asFile, _W("rb"));
		if (!pFile) {
			Error("Couldn't load skin file '%s'!\n",asFile.c_str());
			return false;
		}

		fseek(pFile, 0, SEEK_END);
		long lLength = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		std::vector<char> vData(lLength > 0 ? lLength : 0);
		bool bReadOk = lLength <= 0 || fread(vData.data(), lLength, 1, pFile) == 1;
		fclose(pFile);

		int lStatus = 0;
		void *pSkin = ohpl_gui_skin_parse(vData.data(), vData.size(), bReadOk, &lStatus);
		if(pSkin==NULL)
		{
			Error("Couldn't load skin file '%s'!\n",asFile.c_str());
			return false;
		}
		const OhplSkinView *pView = ohpl_gui_skin_view(pSkin);

		//////////////////////////////////////
		// ATTRIBUTES
		tBoolVec vAttribLoaded;
		vAttribLoaded.resize(eGuiSkinAttribute_LastEnum, false);

		for(size_t i=0; i<pView->attributeCount; ++i)
		{
			const OhplSkinAttribute &attr = pView->attributes[i];

			eGuiSkinAttribute type = mpGui->GetSkinAttributeFromString(attr.type);
			if(type == eGuiSkinAttribute_LastEnum) continue;

			mvAttributes[type] = cVector3f(attr.value[0], attr.value[1], attr.value[2]);
			vAttribLoaded[type] = true;
		}

		// Error checking, see so all elements are there
		for(int i=0; i<eGuiSkinAttribute_LastEnum; ++i)
		{
			if(vAttribLoaded[i]==false) Error("Could not find gui skin '%s' attribute %s!\n", msName.c_str(),mpGui->GetSkinAttributeString((eGuiSkinAttribute)i).c_str());
		}

		//////////////////////////////////////
		// FONTS
		for(size_t i=0; i<pView->fontCount; ++i)
		{
			const OhplSkinFont &font = pView->fonts[i];

			eGuiSkinFont type = mpGui->GetSkinFontFromString(font.type);
			if(type == eGuiSkinFont_LastEnum) continue;

			cGuiSkinFont *pFont = hplNew( cGuiSkinFont, (mpGui) );

			pFont->mpFont = mpGui->GetResources()->GetFontManager()->CreateFontData(font.file);
			pFont->mvSize = cVector2f(font.size[0], font.size[1]);
			pFont->mColor = cColor(font.color[0], font.color[1], font.color[2], font.color[3]);

			mvFonts[type] = pFont;
        }

		// Error checking, see so all elements are there
		for(int i=0; i<eGuiSkinFont_LastEnum; ++i)
		{
			if(mvFonts[i]==NULL) Error("Could not find gui skin '%s' font %s!\n", msName.c_str(),mpGui->GetSkinFontString((eGuiSkinFont)i).c_str());
		}

		//////////////////////////////////////
		// GFX ELEMENTS
		for(size_t i=0; i<pView->gfxCount; ++i)
		{
			const OhplSkinGfx &gfx = pView->gfx[i];

			eGuiSkinGfx type = mpGui->GetSkinGfxFromString(gfx.type);
			if(type == eGuiSkinGfx_LastEnum) continue;

			tString sFile = gfx.file;
			cColor color(gfx.color[0], gfx.color[1], gfx.color[2], gfx.color[3]);
			eGuiMaterial material = (eGuiMaterial)gfx.material;
			if(gfx.materialWarn) Warning("Material type '%s' does not exist!\n",gfx.materialWarn);
			if(gfx.animWarn) Warning("Animation type '%s' does not exist!\n",gfx.animWarn);
			bool bAnimated = gfx.animated != 0;

			cGuiGfxElement *pGfx = NULL;
			if(sFile != "")
			{
				if(bAnimated)
				{
					pGfx = mpGui->CreateGfxImageBuffer(sFile, material,true,color);
				}
				else
				{
					pGfx = mpGui->CreateGfxImage(sFile, material,color);
				}
			}
			else
			{
				pGfx = mpGui->CreateGfxFilledRect(color,material);
			}

			if(pGfx)
			{
				if(bAnimated && pGfx->GetAnimationNum() > 0)
				{
					cGuiGfxAnimation *pAnim = pGfx->GetAnimation(0);
					pAnim->SetFrameLength(gfx.frameTime);
					pAnim->SetType((eGuiGfxAnimationType)gfx.animMode);
				}

				if(gfx.hasActiveSize)
					pGfx->SetActiveSize(cVector2f(gfx.activeSize[0], gfx.activeSize[1]));
				pGfx->SetOffset(cVector3f(gfx.offset[0], gfx.offset[1],0));
				
				mvGfxElements[type] = pGfx;
			}
		}

		// Error checking, see so all elements are there
		for(int i=0; i<eGuiSkinGfx_LastEnum; ++i)
		{
			if(mvGfxElements[i]==NULL) Error("Could not find gui skin '%s' gfx %s!\n", msName.c_str(),mpGui->GetSkinGfxString((eGuiSkinGfx)i).c_str());
		}

		ohpl_gui_skin_free(pSkin);

		return true;
	}

	//-----------------------------------------------------------------------

	cGuiGfxElement* cGuiSkin::GetGfx(eGuiSkinGfx aType)
	{
		return mvGfxElements[aType];
	}

	cGuiSkinFont* cGuiSkin::GetFont(eGuiSkinFont aType)
	{
		return mvFonts[aType];
	}

	const cVector3f& cGuiSkin::GetAttribute(eGuiSkinAttribute aType)
	{
		return mvAttributes[aType];
	}
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	//-----------------------------------------------------------------------

}
