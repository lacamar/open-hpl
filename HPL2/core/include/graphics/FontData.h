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

#ifndef HPL_FONTDATA_H
#define HPL_FONTDATA_H

#include <cstddef>
#include <vector>
#include "math/MathTypes.h"
#include "system/SystemTypes.h"
#include "system/SystemTypes.h"
#include "graphics/GraphicsTypes.h"
#include "resources/ResourceBase.h"
#include "resources/LowLevelResources.h"
#include "graphics/RustGraphicsH5b.h"
#include <cstdarg>
#include <cwchar>
#include <stdexcept>

namespace hpl {

	class cResources;
	class iLowLevelGraphics;
	class cBitmap;
	class cGuiGfxElement;
	class cGui;
	class cFrameSubImage;

	//------------------------------------------------
		
	class cGlyph
	{
	public:
		cGlyph(	cGuiGfxElement *apGuiGfx,const cVector2f &avOffset,
				const cVector2f &avSize, float afAdvance)
			: mpGuiGfx(apGuiGfx), mvOffset(avOffset), mvSize(avSize), mfAdvance(afAdvance) {}
		~cGlyph();

		cGuiGfxElement* GetGuiGfx() const { return mpGuiGfx; }
		const cVector2f& GetOffset() const { return mvOffset; }
		const cVector2f& GetSize() const { return mvSize; }
		float GetAdvance() const { return mfAdvance; }

		// Byte offsets read by the Rust draw path (RustGuiG1.cpp).
		static size_t GfxOffset() { return offsetof(cGlyph, mpGuiGfx); }
		static size_t OffsetOffset() { return offsetof(cGlyph, mvOffset); }
		static size_t SizeOffset() { return offsetof(cGlyph, mvSize); }
		static size_t AdvanceOffset() { return offsetof(cGlyph, mfAdvance); }

	private:
		cGuiGfxElement *mpGuiGfx;
		cVector2f mvOffset;
		cVector2f mvSize;
		float mfAdvance;
	};

	typedef std::vector<cGlyph*> tGlyphVec;
	typedef tGlyphVec::iterator tGlyphVecIt;

	class iFontData : public iResourceBase
	{
	public:
		iFontData(const tString &asName, iLowLevelGraphics* apLowLevelGraphics)
			: iResourceBase(asName,_W(""),0), mpLowLevelGraphics(apLowLevelGraphics), mpResources(NULL), mpGui(NULL),
			  mfHeight(0), mlFirstChar(0), mlLastChar(0), mlRs(ohpl_h5b_font_new()) {}
		~iFontData();

		virtual bool CreateFromFontFile(const tWString &asFileName, int alSize,unsigned short alFirstChar, 
									unsigned short alLastChar)=0;

		virtual bool CreateFromBitmapFile(const tWString &asFileName)=0;


		bool Reload(){ return false;}
		void Unload(){}
		void Destroy(){}

		/**
		 * Used internally
		 */
		void SetUp(cResources* apResources,	cGui *apGui)
		{
			mpResources = apResources;
			mpGui = apGui;
		}

		/**
		 * Used internally
		 * \param alNum 
		 * \return 
		 */
		inline cGlyph* GetGlyph(int alNum)const { if(alNum<0 || alNum>=(int)mvGlyphs.size()) return NULL; return mvGlyphs[alNum];}

		inline const tGlyphVec& GetGlyphs()const{ return mvGlyphs;}
		inline unsigned short GetFirstChar(){ return mlFirstChar;}
		inline unsigned short GetLastChar(){ return mlLastChar;}

		inline const cVector2f& GetSizeRatio()const{ return mvSizeRatio;}
		
		/**
		 * Draw a string.
		 * \param avPos Screen pos
		 * \param avSize size of the characters
		 * \param aCol color
		 * \param mAlign alignment
		 * \param fmt 
		 * \param ... 
		 */
		//void Draw(const cVector3f& avPos,const cVector2f& avSize, const cColor& aCol,eFontAlign mAlign,
		//			const wchar_t* fmt,...);
		/**
		 * Draw a string  with word wrap.
		 * \param avPos Screen pos
		 * \param afLength Max length of a line
		 * \param afFontHeight The distance from base of character above to base of character below
		 * \param avSize size of the characters
		 * \param aCol color
		 * \param aAlign alignment
		 * \param asString 
		 * \return Extra number of rows generated.
		 */
		//int DrawWordWrap(cVector3f avPos,float afLength,float afFontHeight,cVector2f avSize,const cColor& aCol,
		//					eFontAlign aAlign,	const tWString &asString);

		
		void GetWordWrapRows(float afLength,float afFontHeight,cVector2f avSize,const tWString& asString,
								tWStringVec *apRowVec)
		{
			auto push = [](void* apCtx, const uint32_t* apRow, size_t alLen) {
				static_cast<tWStringVec*>(apCtx)->emplace_back((const wchar_t*)apRow, alLen);
			};
			if(!ohpl_h5b_font_word_wrap(mlRs, afLength, avSize.x, (const uint32_t*)asString.data(), asString.size(), push, apRowVec))
				throw std::out_of_range("basic_string::substr");
		}

		/**
		 * Get height of the font.
		 * \return 
		 */
		inline float GetHeight()const{ return mfHeight; }

		/**
		 * Get the length in virtual screen size "pixels" of a formated string
		 * \param avSize size of the characters
		 * \param fmt 
		 * \param ... 
		 * \return 
		 */
		float GetLengthFmt(const cVector2f& avSize,const wchar_t* fmt,...)
		{
			if(!fmt) return 0;
			wchar_t vText[256];
			va_list vArgs;
			va_start(vArgs, fmt);
			vswprintf(vText, 255, fmt, vArgs);
			va_end(vArgs);
			return GetLength(avSize, vText);
		}
		float GetLength(const cVector2f& avSize,const wchar_t* sText)
		{
			return ohpl_h5b_font_length(mlRs, avSize.x, (const uint32_t*)sText, wcslen(sText));
		}

	protected:
		void SetMetrics(float afHeight, const cVector2f& avSizeRatio, unsigned short alFirst, unsigned short alLast, size_t alGlyphs)
		{
			mfHeight = afHeight;
			mvSizeRatio = avSizeRatio;
			mlFirstChar = alFirst;
			mlLastChar = alLast;
			mvGlyphs.resize(alGlyphs, NULL);
			OhplH5bFontHdr hdr = { alFirst, alLast, afHeight, { avSizeRatio.x, avSizeRatio.y } };
			ohpl_h5b_font_set_hdr(mlRs, &hdr, alGlyphs);
		}
		void SetGlyph(int alId, cGlyph *apGlyph)
		{
			mvGlyphs[alId] = apGlyph;
			const OhplH5bGlyph g = { apGlyph->GetGuiGfx(), { apGlyph->GetOffset().x, apGlyph->GetOffset().y },
									 { apGlyph->GetSize().x, apGlyph->GetSize().y }, apGlyph->GetAdvance() };
			ohpl_h5b_font_set_glyph(mlRs, alId, &g);
		}

		cGlyph* CreateGlyph(cFrameSubImage* apImage, const cVector2l &avOffset,const cVector2l &avSize,
							const cVector2l& avFontSize, int alAdvance);

		iLowLevelGraphics* mpLowLevelGraphics;
		cResources* mpResources;
		cGui *mpGui;

	private:
		tGlyphVec mvGlyphs;

		float mfHeight;
		unsigned short mlFirstChar;
		unsigned short mlLastChar;

		cVector2f mvSizeRatio;

		unsigned long long mlRs;
	};

};
#endif // HPL_FONTDATA_H
