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

#include "impl/tinyXML/tinyxml.h"

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

		if(OhplBackendRustRequested()) return CreateFromBitmapFileRust(asFileName, sPath);

		////////////////////////////////////////////
		// Load xml file
		FILE *pFile = cPlatform::OpenFile(asFileName, _W("rb"));
		if(pFile==NULL) return false;
		
		TiXmlDocument *pXmlDoc = hplNew( TiXmlDocument,() );
		if(pXmlDoc->LoadFile(pFile)==false)
		{
			Error("Couldn't load angle code font file '%s'\n",asFileName.c_str());
			fclose(pFile);
			hplDelete(pXmlDoc);
			return false;
		}

		TiXmlElement *pRootElem = pXmlDoc->RootElement();

		////////////////////////////////////////////
		// Load Common info
		TiXmlElement *pCommonElem = pRootElem ? pRootElem->FirstChildElement("common") : NULL;
		TiXmlElement *pInfoElem = pRootElem ? pRootElem->FirstChildElement("info") : NULL;
		if(pCommonElem==NULL || pInfoElem==NULL || pRootElem->FirstChildElement("chars")==NULL || pRootElem->FirstChildElement("pages")==NULL)
		{
			Error("Font file '%s' lacks info, common, chars or pages\n", cString::To8Char(asFileName).c_str());
			fclose(pFile);
			hplDelete(pXmlDoc);
			return false;
		}

		int lOutline = cString::ToInt(pInfoElem->Attribute("outline"),0);
		OhplFontHeader header = ohpl_font_header(lOutline, cString::ToInt(pCommonElem->Attribute("lineHeight"),0),
												cString::ToInt(pCommonElem->Attribute("base"),0));
		int lLineHeight = header.line_height;
		int lBase = header.base;

		mfHeight = header.height;

		mvSizeRatio.x = header.size_ratio_x;//I think this is a not correct. Not sure what is done here :S
		mvSizeRatio.y = 1;

		int lLargestGlyphId=-1;

		////////////////////////////////////////////
		// Check for largest glyph number and resize array.
		TiXmlElement *pCharsRootElem = pRootElem->FirstChildElement("chars");
		TiXmlElement *pCharElem = pCharsRootElem->FirstChildElement("char");
		for(; pCharElem != NULL; pCharElem = pCharElem->NextSiblingElement("char"))
		{
			int lId = cString::ToInt(pCharElem->Attribute("id"),0);
			if(lId >lLargestGlyphId) lLargestGlyphId = lId;
		}

		long long llGlyphCount = ohpl_font_glyph_count(lLargestGlyphId);
		if(llGlyphCount < 0)
		{
			Error("Font file '%s' has an unreasonable glyph id %d\n", cString::To8Char(asFileName).c_str(), lLargestGlyphId);
			fclose(pFile);
			hplDelete(pXmlDoc);
			return false;
		}
		mlFirstChar =0;
		mlLastChar = lLargestGlyphId;
		mvGlyphs.resize((size_t)llGlyphCount, NULL);

		////////////////////////////////////////////
		// Load bitmaps
		std::vector<cFrameTexture*> vFrameTextures;
		
		TiXmlElement *pPagesRootElem = pRootElem->FirstChildElement("pages");

		int lCount=0;
		TiXmlElement *pPageElem = pPagesRootElem->FirstChildElement("page");
		for(; pPageElem != NULL; pPageElem = pPageElem->NextSiblingElement("page"), ++lCount)
		{
			tWString sFileName = cString::To16Char(pPageElem->Attribute("file"));
			tWString sFilePath = cString::SetFilePathW(sFileName,sPath);

			//////////////////////////////
			//Load image to bitmap
			cBitmap *pBitmap = mpResources->GetBitmapLoaderHandler()->LoadBitmap(sFilePath, 0);
			if(pBitmap==NULL)
			{
				Error("Couldn't load bitmap %s for FNT file '%s'\n",cString::To8Char(sFilePath).c_str(),cString::To8Char(asFileName).c_str());
				hplDelete(pXmlDoc);
				return false;
			}
			
			//Make a fix here so we get a white luminence and alpha filled with data.
			if(pBitmap->GetPixelFormat() == ePixelFormat_Luminance || pBitmap->GetPixelFormat() == ePixelFormat_Alpha)
			{
				cBitmap *pTempBitmap = hplNew( cBitmap, ());
				pTempBitmap->CreateData(pBitmap->GetSize(),ePixelFormat_LuminanceAlpha,0,0);

				pBitmap->SetPixelFormat(ePixelFormat_Alpha);
				pTempBitmap->Blit(pBitmap,0,pBitmap->GetSize(),0);

				hplDelete( pBitmap );

                pBitmap = pTempBitmap;
			}
			//Log("Loaded bitmap %s, bpp: %d pixelformat: %d\n",sFileName.c_str(),pBitmap->GetBytesPerPixel(), pBitmap->GetPixelFormat());

			///////////////////////
			//Create a texture from bitmap (do not want to load it from texture manager since that would delete the texture on its own).
			tString sName = cString::SetFileExt(cString::To8Char(asFileName),"")+"_"+cString::ToString(lCount);
			iTexture *pTexture = mpLowLevelGraphics->CreateTexture("",eTextureType_2D,eTextureUsage_Normal);

			pTexture->CreateFromBitmap(pBitmap);

			hplDelete( pBitmap ); //Bitmap no longer needed

			///////////////////////
			//Create Custom Frame for images
			cFrameTexture *pFrameTexture = mpResources->GetImageManager()->CreateCustomFrame(pTexture);

			vFrameTextures.push_back(pFrameTexture);
		}

		////////////////////////////////////////////
		// Load glyphs
		pCharsRootElem = pRootElem->FirstChildElement("chars");
		pCharElem = pCharsRootElem->FirstChildElement("char");
		for(; pCharElem != NULL; pCharElem = pCharElem->NextSiblingElement("char"))
		{
			//Get the info on the character
			int lId = cString::ToInt(pCharElem->Attribute("id"),0);
			int lX = cString::ToInt(pCharElem->Attribute("x"),0);
			int lY = cString::ToInt(pCharElem->Attribute("y"),0);

			int lW = cString::ToInt(pCharElem->Attribute("width"),0);
			int lH = cString::ToInt(pCharElem->Attribute("height"),0);

			int lXOffset = cString::ToInt(pCharElem->Attribute("xoffset"),0);
			int lYOffset = cString::ToInt(pCharElem->Attribute("yoffset"),0);

			int lAdvance = ohpl_font_glyph_advance(cString::ToInt(pCharElem->Attribute("xadvance"),0), lOutline);

			int lPage = cString::ToInt(pCharElem->Attribute("page"),0);

			if(!ohpl_font_glyph_id_valid(lId, mvGlyphs.size()))
			{
				Warning("Font '%s' contain glyph with invalid id: %d. Skipping loading of it!\n", cString::To8Char(asFileName).c_str(), lId);
				continue;
			}

			if(!ohpl_font_page_valid(lPage, vFrameTextures.size()))
			{
				Warning("Font '%s' glyph %d uses invalid page %d. Skipping loading of it!\n", cString::To8Char(asFileName).c_str(), lId, lPage);
				continue;
			}

			//Get the bitmap where the character graphics is
			cFrameTexture* pFrameTexture = vFrameTextures[lPage];
			
			cFrameSubImage *pImage = pFrameTexture->CreateCustomImage(cVector2l(lX, lY),cVector2l(lW,lH));
            
			//Create glyph and place it correctly.
            cGlyph *pGlyph = CreateGlyph(pImage,cVector2l(lXOffset,lYOffset),cVector2l(lW,lH),
										cVector2l(lBase,lLineHeight),lAdvance);
			
			mvGlyphs[lId] = pGlyph;

		}

		//Destroy XML
		fclose(pFile);
		hplDelete(pXmlDoc);
		return true;
	}
	
	//-----------------------------------------------------------------------
	
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
		/*SetFullPath(asFileName);

		cGlyph* pGlyph=NULL;

		mlFirstChar = alFirstChar;
		mlLastChar = alLastChar;
		
		TTF_Font* pFont = TTF_OpenFont(cString::To8Char(asFileName).c_str(), alSize);
		if(pFont==NULL){
			Error("Error when opening '%s': %s\n",asFileName.c_str(),TTF_GetError());
			return false;
		}
		
		//Create bitmaps from all of the characters and create
		//Images from those. 
		for(int i=alFirstChar; i<=alLastChar;i++)
		{
			unsigned short lUniCode = i;
			//char c = (char)i;
			
			//if(c == 'ˆ')lUniCode = 'o';
			//else if(c == '÷')lUniCode = 'O';

            pGlyph = RenderGlyph(pFont, lUniCode, alSize);
			AddGlyph(pGlyph);
		}
		
		//Get the properties
		mfHeight = (float)TTF_FontHeight(pFont);

		mvSizeRatio = 1;
		
		//Cleanup
		TTF_CloseFont(pFont);

		return true;*/
		return false;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	/*cGlyph* cSDLFontData::RenderGlyph(TTF_Font* apFont,unsigned short aChar, int alFontSize)
	{
		//If the font is saved to disk, then load the bitmap from disk instead.
		
		cVector2l vMin;
		cVector2l vMax;
		int lAdvance=0;
		
		TTF_GlyphMetrics(apFont, aChar, &vMin.x, &vMax.x, &vMin.y, &vMax.y, &lAdvance);

		//Create the bitmap we want to draw upon
		cVector2l vSize = vMax - vMin;
		cBitmap *pBmp = hplNew(cBitmap, () );
		//TODO: Create and fill bitmap		
		//pBmp->CreateData(cVector3l(vSize.x, vSize.y,1),ePixelFormat_RGBA,0,0);
		//pBmp->Clear(cColor(0,1),0,0);
		
		//create a surface with the glyph
		SDL_Color Col;Col.r=255;Col.g=255;Col.b=255;
		SDL_Surface* pGlyphSurface = TTF_RenderGlyph_Blended(apFont,aChar,Col);

		SDL_Surface* pTempSurface = SDL_CreateRGBSurface(	SDL_SWSURFACE, vSize.x, vSize.y,32, 
															0x000000ff,0x0000ff00,0x00ff0000,0xff000000);
        
		//Blit the surface using blending. This way it should create a nice
		//b&w image where the font is white.
		//TODO: Get Surface data and do a raw data blit.
		SDL_SetAlpha(pTempSurface,0,0);
		SDL_SetAlpha(pGlyphSurface,SDL_SRCALPHA,0);
		SDL_BlitSurface(pGlyphSurface, NULL, pTempSurface,NULL);
		SDL_SetAlpha(pTempSurface,0,0);

        //Set the alpha of the bitmap to the average color.
		//So we get some alpha bledning.
		int lBmpSize = 4;
		unsigned char* PixBuffer = (unsigned char*)pTempSurface->pixels;
		
		//Should not be needed:
		for(int y=0;y<vSize.y;y++)
			for(int x=0;x<vSize.x;x++)
			{
				unsigned char* Pix = &PixBuffer[y*pBmp->GetWidth()*lBmpSize + 
					x*lBmpSize];

				Pix[3] = Pix[0];	
			}
		pBmp->SetSize(cVector3l(vSize.x, vSize.y,1));
		pBmp->SetPixelFormat(ePixelFormat_RGBA);
		pBmp->SetBytesPerPixel(4);
		pBmp->GetData(0,0)->SetData(PixBuffer,vSize.x*vSize.y * 4);

		SDL_FreeSurface(pTempSurface);

		//////////////////////////
		//Create Image
		cFrameSubImage* pImage = mpResources->GetImageManager()->CreateFromBitmap("",pBmp);
		if(pImage==NULL){
			FatalError("Couldn't create image for bitmap\n");
			return NULL;
		}
		
		////////////////////////////////////
		//Create the Glyph
		int lHeight = TTF_FontHeight(apFont);
		cVector2l vOffset = cVector2l(vMin.x, alFontSize - vMax.y);//(lHeight - vSize.y) - vMin.y);

		cGlyph* pGlyph = CreateGlyph(pImage,vOffset,vSize,alFontSize,lAdvance);

		hplDelete(pBmp);
		SDL_FreeSurface(pGlyphSurface);

		return pGlyph;
	}*/

	//-----------------------------------------------------------------------

}
