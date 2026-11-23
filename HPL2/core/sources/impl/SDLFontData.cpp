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

#include "impl/SDLFontData.h"

#include "resources/Resources.h"
#include "graphics/LowLevelGraphics.h"
#include "system/LowLevelSystem.h"
#include "resources/BitmapLoaderHandler.h"
#include "graphics/Bitmap.h"
#include "resources/ImageManager.h"
#include "graphics/FrameTexture.h"
#include "graphics/FrameSubImage.h"
#include "graphics/Texture.h"


#include "system/String.h"
#include "system/Platform.h"
#include "impl/RustImpl.h"
#include "impl/RustL5.h"

namespace hpl {


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSDLFontData::cSDLFontData(const tString &asName,iLowLevelGraphics* apLowLevelGraphics)
		: iFontData(asName,apLowLevelGraphics)
	{
	
	
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cSDLFontData::CreateFromBitmapFile(const tWString &asFileName)
	{
		SetFullPath(asFileName);

		tWString sPath = cString::GetFilePathW(asFileName);

		return CreateFromBitmapFileRust(asFileName, sPath);
	}

	static void OhplL5PageName(void* apCtx, const uint8_t* apName, size_t alLen)
	{
		((tString*)apCtx)->assign((const char*)apName, alLen);
	}

	bool cSDLFontData::CreateFromBitmapFileRust(const tWString &asFileName, const tWString &sPath)
	{
		OhplL5InstallLog();

		int32_t lStatus = 0, lLargestId = -1;
		OhplL5Font* pFont = ohpl_l5_font_load(cString::To8Char(asFileName).c_str(), &lStatus, &lLargestId);
		if(pFont==NULL)
		{
			switch(lStatus)
			{
			case 2: Error("Couldn't load angle code font file '%s'\n",asFileName.c_str()); break;
			case 3: Error("Font file '%s' lacks info, common, chars or pages\n", cString::To8Char(asFileName).c_str()); break;
			case 4: Error("Font file '%s' has an unreasonable glyph id %d\n", cString::To8Char(asFileName).c_str(), lLargestId); break;
			}
			return false;
		}

		OhplL5FontInfo info;
		ohpl_l5_font_info(pFont, &info);
		int lLineHeight = info.line_height;
		int lBase = info.base;

		mfHeight = info.height;
		mvSizeRatio.x = info.size_ratio_x;
		mvSizeRatio.y = 1;

		mlFirstChar =0;
		mlLastChar = info.largest_id;
		mvGlyphs.resize(info.glyph_count, NULL);

		std::vector<cFrameTexture*> vFrameTextures;
		for(size_t lPage=0; lPage<info.pages; ++lPage)
		{
			tString sPageFile;
			ohpl_l5_font_page(pFont, lPage, OhplL5PageName, &sPageFile);
			tWString sFileName = cString::To16Char(sPageFile);
			tWString sFilePath = cString::SetFilePathW(sFileName,sPath);

			cBitmap *pBitmap = mpResources->GetBitmapLoaderHandler()->LoadBitmap(sFilePath, 0);
			if(pBitmap==NULL)
			{
				Error("Couldn't load bitmap %s for FNT file '%s'\n",cString::To8Char(sFilePath).c_str(),cString::To8Char(asFileName).c_str());
				ohpl_l5_font_free(pFont);
				return false;
			}

			if(pBitmap->GetPixelFormat() == ePixelFormat_Luminance || pBitmap->GetPixelFormat() == ePixelFormat_Alpha)
			{
				cBitmap *pTempBitmap = hplNew( cBitmap, ());
				pTempBitmap->CreateData(pBitmap->GetSize(),ePixelFormat_LuminanceAlpha,0,0);

				pBitmap->SetPixelFormat(ePixelFormat_Alpha);
				pTempBitmap->Blit(pBitmap,0,pBitmap->GetSize(),0);

				hplDelete( pBitmap );

				pBitmap = pTempBitmap;
			}

			iTexture *pTexture = mpLowLevelGraphics->CreateTexture("",eTextureType_2D,eTextureUsage_Normal);
			pTexture->CreateFromBitmap(pBitmap);
			hplDelete( pBitmap );

			vFrameTextures.push_back(mpResources->GetImageManager()->CreateCustomFrame(pTexture));
		}

		for(size_t i=0; i<info.chars; ++i)
		{
			OhplL5FontChar c;
			ohpl_l5_font_char(pFont, i, &c);

			if(c.status==1)
			{
				Warning("Font '%s' contain glyph with invalid id: %d. Skipping loading of it!\n", cString::To8Char(asFileName).c_str(), c.id);
				continue;
			}
			if(c.status==2)
			{
				Warning("Font '%s' glyph %d uses invalid page %d. Skipping loading of it!\n", cString::To8Char(asFileName).c_str(), c.id, c.page);
				continue;
			}

			cFrameTexture* pFrameTexture = vFrameTextures[c.page];
			cFrameSubImage *pImage = pFrameTexture->CreateCustomImage(cVector2l(c.x, c.y),cVector2l(c.width,c.height));
			cGlyph *pGlyph = CreateGlyph(pImage,cVector2l(c.x_offset,c.y_offset),cVector2l(c.width,c.height),
										cVector2l(lBase,lLineHeight),c.advance);
			mvGlyphs[c.id] = pGlyph;
		}

		ohpl_l5_font_free(pFont);
		return true;
	}

	//-----------------------------------------------------------------------

	bool cSDLFontData::CreateFromFontFile(const tWString &asFileName, int alSize, unsigned short alFirstChar, 
											unsigned short alLastChar)
	{
			return false;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	

	//-----------------------------------------------------------------------

}
