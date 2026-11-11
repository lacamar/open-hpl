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
	}
	
	//-----------------------------------------------------------------------

	iFontData::~iFontData()
	{
		for(int i=0;i<(int)mvGlyphs.size();i++)
		{
			if(mvGlyphs[i]) hplDelete(mvGlyphs[i]);
		}
	}

	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	/*void iFontData::Draw(const cVector3f& avPos,const cVector2f& avSize, const cColor& aCol,
						eFontAlign aAlign,	const wchar_t* fmt,...)
	{
		wchar_t sText[256];
		va_list ap;	
		if (fmt == NULL) return;	
		va_start(ap, fmt);
		vswprintf(sText, 255, fmt, ap);
		va_end(ap);


		int lCount=0;
		float lXAdd =0;
		cVector3f vPos = avPos;

		if(aAlign == eFontAlign_Center){
			vPos.x -= GetLength(avSize, sText)/2;
		}
		else if(aAlign == eFontAlign_Right)
		{
			vPos.x -= GetLength(avSize, sText);
		}

		while(sText[lCount] != 0)
		{
			wchar_t lGlyphNum = ((wchar_t)sText[lCount]);
			if(lGlyphNum<mlFirstChar || lGlyphNum>mlLastChar){
				lCount++;
				continue;
			}
			lGlyphNum -= mlFirstChar;


			cGlyph *pGlyph = mvGlyphs[lGlyphNum];
			if(pGlyph)
			{
				cVector2f vOffset(pGlyph->mvOffset * avSize);
				cVector2f vSize(pGlyph->mvSize * avSize);

				mpGraphicsDrawer->DrawGfxObject(pGlyph->mpGfxObject,vPos + vOffset,vSize,aCol);

				vPos.x += pGlyph->mfAdvance*avSize.x; 
			}
			lCount++;
		}
	}*/
	
	//-----------------------------------------------------------------------

	/*int iFontData::DrawWordWrap(cVector3f avPos,float afLength,float afFontHeight,cVector2f avSize,const cColor& aCol,
								eFontAlign aAlign,	const tWString &asString)
	{
		int rows = 0;

		unsigned int pos;
		unsigned int first_letter=0;
		unsigned int last_space=0;

		tUIntList RowLengthList;
		
		float fTextLength;

		for(pos = 0; pos < asString.size();pos++)
		{
			if(asString[pos] == _W(' ') || asString[pos] == _W('\n'))
			{
				tWString temp = asString.substr(first_letter, pos-first_letter);
				fTextLength =  GetLength(avSize,temp.c_str());

				bool nothing = true;
				if(fTextLength > afLength)
				{
					rows++;
					RowLengthList.push_back(last_space);
					first_letter=last_space+1;
					last_space = pos;
					nothing = false;
				}
				if(asString[pos] == _W('\n'))
				{
					last_space = pos;
					first_letter=last_space+1;
					RowLengthList.push_back(last_space-1);
					rows++;
					nothing = false;
				}
				if(nothing)
				{
					last_space = pos;
				}
			}
		}
		tWString temp =  asString.substr(first_letter, pos-first_letter);
		fTextLength = GetLength(avSize,temp.c_str());
		if(fTextLength > afLength)
		{
			rows++;
			RowLengthList.push_back(last_space);
		}

		if(rows==0)
		{
			Draw(avPos,avSize,aCol,aAlign,_W("%ls"),asString.c_str());
		}
		else
		{
			first_letter=0;
			unsigned int i=0;
			
			for(tUIntListIt it = RowLengthList.begin();it != RowLengthList.end();++it)
			{
				Draw(cVector3f(avPos.x,avPos.y + i*afFontHeight,avPos.z),avSize,aCol,aAlign,
								_W("%ls"),asString.substr(first_letter,*it-first_letter).c_str());
				i++;
				first_letter = *it+1;
			}
			Draw(cVector3f(avPos.x,avPos.y + i*afFontHeight,avPos.z),avSize,aCol,aAlign,
				_W("%ls"),asString.substr(first_letter).c_str());

		}

		return rows;
	}*/

	//-----------------------------------------------------------------------

	static OhplGlyphs GlyphView(const tGlyphVec& avGlyphs, unsigned short alFirst, unsigned short alLast)
	{
		OhplGlyphs g = { (const void* const*)avGlyphs.data(), avGlyphs.size(), alFirst, alLast, offsetof(cGlyph, mfAdvance) };
		return g;
	}

	static void PushRow(void* apCtx, const wchar_t* apRow, size_t alLen)
	{
		((tWStringVec*)apCtx)->push_back(tWString(apRow, alLen));
	}

	void iFontData::GetWordWrapRows(float afLength,float afFontHeight,cVector2f avSize,
							const tWString& asString,tWStringVec *apRowVec)
	{
		OhplGlyphs g = GlyphView(mvGlyphs, mlFirstChar, mlLastChar);
		if(ohpl_font_word_wrap(&g, afLength, avSize.x, asString.data(), asString.size(), PushRow, apRowVec)==false)
			throw std::out_of_range("basic_string::substr");
	}
	
	//-----------------------------------------------------------------------
	
	float iFontData::GetLength(const cVector2f& avSize,const wchar_t* sText)
	{
		int lCount=0;
		float fLength =0;
		while(sText[lCount] != 0)
		{
			unsigned short lGlyphNum = ((wchar_t)sText[lCount]);
			if(lGlyphNum<mlFirstChar || lGlyphNum>mlLastChar){
				lCount++;
				continue;
			}
			lGlyphNum -= mlFirstChar;

			cGlyph *pGlyph = GetGlyph(lGlyphNum);
			if(pGlyph)
			{
				fLength += pGlyph->mfAdvance*avSize.x; 
			}
			lCount++;
		}

		return fLength;
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

		return pGlyph;
	}
	
	//-----------------------------------------------------------------------
	
	void iFontData::AddGlyph(cGlyph *apGlyph)
	{
		mvGlyphs.push_back(apGlyph);
	}

	

	//-----------------------------------------------------------------------

}
