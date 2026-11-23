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

#include "graphics/FontData.h"
#include "graphics/RustGraphics.h"
#include "graphics/RustGraphicsH5b.h"
#include <stddef.h>
#include <stdexcept>
#include <stdarg.h>
#include <stdlib.h>

#include "system/LowLevelSystem.h"

#include "resources/Resources.h"
#include "graphics/FrameSubImage.h"
#include "resources/ImageManager.h"

#include "gui/GuiGfxElement.h"
#include "gui/Gui.h"

namespace hpl {


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	
	//-----------------------------------------------------------------------

	cGlyph::cGlyph(	cGuiGfxElement *apGuiGfx,const cVector2f &avOffset, const cVector2f &avSize, float afAdvance)
	{
		mpGuiGfx = apGuiGfx;
		mvOffset = avOffset;
		mvSize = avSize;
		mfAdvance = afAdvance;
	}
		


	cGlyph::~cGlyph()
	{ 
		if(mpGuiGfx) hplDelete(mpGuiGfx);
	
	}

	//-----------------------------------------------------------------------
	
	iFontData::iFontData(const tString &asName,iLowLevelGraphics* apLowLevelGraphics) : iResourceBase(asName,_W(""),0)
	{
		mpLowLevelGraphics = apLowLevelGraphics;
		mpResources = NULL;
		mlRs = ohpl_h5b_font_new();
		mbRsDirty = true;
	}
	
	//-----------------------------------------------------------------------

	iFontData::~iFontData()
	{
		for(int i=0;i<(int)mvGlyphs.size();i++)
		{
			if(mvGlyphs[i]) hplDelete(mvGlyphs[i]);
		}
		ohpl_h5b_font_free(mlRs);
	}

	void iFontData::SyncRs()
	{
		std::vector<OhplH5bGlyph> vGlyphs(mvGlyphs.size());
		std::vector<unsigned char> vPresent(mvGlyphs.size(), 0);
		for(size_t i = 0; i < mvGlyphs.size(); ++i)
		{
			cGlyph *pGlyph = mvGlyphs[i];
			if(pGlyph == NULL) continue;
			OhplH5bGlyph g = { pGlyph->mpGuiGfx, { pGlyph->mvOffset.x, pGlyph->mvOffset.y }, { pGlyph->mvSize.x, pGlyph->mvSize.y }, pGlyph->mfAdvance };
			vGlyphs[i] = g;
			vPresent[i] = 1;
		}
		OhplH5bFontHdr hdr = { mlFirstChar, mlLastChar, mfHeight, { mvSizeRatio.x, mvSizeRatio.y } };
		ohpl_h5b_font_set(mlRs, &hdr, vGlyphs.data(), vPresent.data(), vGlyphs.size());
		mbRsDirty = false;
	}

	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	
	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	static void PushRowRs(void* apCtx, const uint32_t* apRow, size_t alLen)
	{
		((tWStringVec*)apCtx)->push_back(tWString((const wchar_t*)apRow, alLen));
	}

	void iFontData::GetWordWrapRows(float afLength,float afFontHeight,cVector2f avSize,
							const tWString& asString,tWStringVec *apRowVec)
	{
		if(mbRsDirty) SyncRs();
		if(ohpl_h5b_font_word_wrap(mlRs, afLength, avSize.x, (const uint32_t*)asString.data(), asString.size(), PushRowRs, apRowVec)==false)
			throw std::out_of_range("basic_string::substr");
	}
	
	//-----------------------------------------------------------------------
	
	float iFontData::GetLength(const cVector2f& avSize,const wchar_t* sText)
	{
		if(mbRsDirty) SyncRs();
		size_t lLen = 0;
		while(sText[lLen] != 0) ++lLen;
		return ohpl_h5b_font_length(mlRs, avSize.x, (const uint32_t*)sText, lLen);
	}
	
	//-----------------------------------------------------------------------

	
	float iFontData::GetLengthFmt(const cVector2f& avSize,const wchar_t* fmt,...)
	{
		wchar_t sText[256];
		va_list ap;	
		if (fmt == NULL) return 0;	
		va_start(ap, fmt);
		vswprintf(sText, 255, fmt, ap);
		va_end(ap);

		return GetLength(avSize, sText);
	}
	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------
	
	cGlyph* iFontData::CreateGlyph(	cFrameSubImage* apImage, const cVector2l &avOffset,const cVector2l &avSize,
									const cVector2l& avFontSize, int alAdvance)
	{
		//////////////////////////
		//Gui gfx
		cGuiGfxElement* pGuiGfx = mpGui->CreateGfxFilledRect(cColor(1,1),eGuiMaterial_FontNormal,false);
		pGuiGfx->AddImage(apImage);
		
		//////////////////////////
		//Sizes
		float vM[5];
		ohpl_font_glyph_metrics(&avOffset, &avSize, &avFontSize, alAdvance, &mvSizeRatio, vM);
		cVector2f vOffset(vM[0], vM[1]);
		cVector2f vSize(vM[2], vM[3]);
		float fAdvance = vM[4];

		cGlyph* pGlyph = hplNew( cGlyph,(pGuiGfx,vOffset,vSize,fAdvance));
		mbRsDirty = true;

		return pGlyph;
	}
	
	//-----------------------------------------------------------------------
	
	void iFontData::AddGlyph(cGlyph *apGlyph)
	{
		mvGlyphs.push_back(apGlyph);
		mbRsDirty = true;
	}

	

	//-----------------------------------------------------------------------

}
