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

#include "gui/Gui.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"

#include "sound/Sound.h"

#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/GuiGfxElement.h"

#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/ImageManager.h"
#include "graphics/FrameSubImage.h"
#include "graphics/FrameBitmap.h"
#include "resources/FileSearcher.h"

#include "system/String.h"

#include "input/Keyboard.h"


#include "gui/GuiMaterialBasicTypes.h"
#include "gui/RustGuiSkin.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	cGuiGfxElement* cGui::mpGfxRect = NULL;

	//-----------------------------------------------------------------------

	cGui::cGui()  : iUpdateable("HPL_Gui")
	{
		mpSetInFocus = NULL;

		mlLastRenderTime = 0;
	}

	//-----------------------------------------------------------------------

	cGui::~cGui()
	{
		Log("Exiting Gui Module\n");
		Log("--------------------------------------------------------\n");
		
		Log(" Deleting all sets\n");
		STLMapDeleteAll(m_mapSets);
		Log(" Deleting all skins\n");
		STLMapDeleteAll(m_mapSkins);
		
		Log(" Deleting all gfx elements\n");
		//STLDeleteAll(mlstGfxElements);
		for(tGuiGfxElementListIt it = mlstGfxElements.begin(); it != mlstGfxElements.end(); ++it)
		{
			cGuiGfxElement *pGfx = *it;
			//Log("Removing %d image: %d\n", pGfx, pGfx->GetImage(0));
			hplDelete(pGfx);
		}
		STLDeleteAll(mlstToBeDestroyedGfxElements);

		Log(" Deleting all materials\n");
		for(int i=0; i< eGuiMaterial_LastEnum; ++i) 
		{
			if(mvMaterials[i]) hplDelete(mvMaterials[i]);
		}
		
		Log("--------------------------------------------------------\n\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cGui::Init(cResources *apResources, cGraphics* apGraphics,
						cSound *apSound, cScene *apScene, cInput* apInput)
	{
		mpResources = apResources;
		mpGraphics = apGraphics;
		mpSound = apSound;
		mpScene = apScene;
		mpInput = apInput;

		//////////////////////////////
		// Create materials
		for(int i=0; i< eGuiMaterial_LastEnum; ++i) mvMaterials[i] = NULL;

		mvMaterials[eGuiMaterial_Diffuse] = hplNew( cGuiMaterial_Diffuse,(mpGraphics->GetLowLevel()) );
		mvMaterials[eGuiMaterial_Alpha] = hplNew( cGuiMaterial_Alpha,(mpGraphics->GetLowLevel()) );
		mvMaterials[eGuiMaterial_FontNormal] = hplNew( cGuiMaterial_FontNormal,(mpGraphics->GetLowLevel()) );
		mvMaterials[eGuiMaterial_Additive] = hplNew( cGuiMaterial_Additive,(mpGraphics->GetLowLevel()) );
		mvMaterials[eGuiMaterial_Modulative] = hplNew( cGuiMaterial_Modulative,(mpGraphics->GetLowLevel()) );
		mvMaterials[eGuiMaterial_PremulAlpha] = hplNew( cGuiMaterial_PremulAlpha,(mpGraphics->GetLowLevel()) );


		//////////////////////////////
		// Set up global gfx
		if(mpGfxRect==NULL)
			mpGfxRect = CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Alpha);
	}

	//-----------------------------------------------------------------------

	void cGui::Update(float afTimeStep)
	{
		////////////////////////////////////
		// Update Sets
		tGuiSetMapIt it = m_mapSets.begin();
		for(; it != m_mapSets.end(); ++it)
		{
			cGuiSet *pSet = it->second;
			if(pSet->IsActive()) pSet->Update(afTimeStep);
		}

		/////////////////////////////
		// Update gfx elements
		tGuiGfxElementListIt gfxIt = mlstGfxElements.begin();
		for(; gfxIt != mlstGfxElements.end(); ++gfxIt)
		{
			cGuiGfxElement *pGfx = *gfxIt;
			pGfx->Update(afTimeStep);
		}
	}

	//-----------------------------------------------------------------------

	void cGui::OnDraw(float afFrameTime)
	{
		tGuiSetMapIt setIt = m_mapSets.begin();
		for(; setIt != m_mapSets.end(); ++setIt)
		{
			cGuiSet *pSet = setIt->second;
			pSet->DrawAll(afFrameTime);
		}
	}

	//-----------------------------------------------------------------------

	void cGui::OnPostBufferSwap()
	{
		//Log("Bug:Clearing render objects\n");

		//////////////////////////////////
		// Clear render sets in all sets
		tGuiSetMapIt setIt = m_mapSets.begin();
		for(; setIt != m_mapSets.end(); ++setIt)
		{
			cGuiSet *pSet = setIt->second;
			pSet->ClearRenderObjects();
		}

		//Delete all gfx elements to be deleted (this way, all pointers in set stay valid!)
		STLDeleteAll(mlstToBeDestroyedGfxElements);
	}
	
	//-----------------------------------------------------------------------

	iGuiMaterial* cGui::GetMaterial(eGuiMaterial aType)
	{
		return mvMaterials[ohpl_gui_material_slot(aType)];
	}

	//-----------------------------------------------------------------------

	cGuiSkin* cGui::CreateSkin(const tString& asFile)
	{
		tString sName = cString::SetFileExt(cString::GetFileName(asFile),"");
		
		//See if skin exists
		tGuiSkinMapIt it = m_mapSkins.find(sName);
		if(it != m_mapSkins.end())
		{
			return it->second;
		}
		
		//Create new skin.
		cGuiSkin *pSkin = hplNew( cGuiSkin, (sName, this) );
		tWString sPath = mpResources->GetFileSearcher()->GetFilePath(asFile);

		if(pSkin->LoadFromFile(sPath)==false)
		{
			hplDelete(pSkin);
			return NULL;
		}

		m_mapSkins.insert(tGuiSkinMap::value_type(sName,pSkin));
		return pSkin;
	}

	//-----------------------------------------------------------------------

	eGuiSkinGfx cGui::GetSkinGfxFromString(const tString& asType)
	{
		eGuiSkinGfx type = (eGuiSkinGfx)ohpl_gui_skin_from_name(kOhplSkinGfx, asType.data(), asType.size());
		if(type == eGuiSkinGfx_LastEnum) Warning("Skin gfx type '%s' does not exist!\n",asType.c_str());

		return type;
	}

	eGuiSkinFont cGui::GetSkinFontFromString(const tString& asType)
	{
		eGuiSkinFont type = (eGuiSkinFont)ohpl_gui_skin_from_name(kOhplSkinFont, asType.data(), asType.size());
		if(type == eGuiSkinFont_LastEnum) Warning("Skin Font type '%s' does not exist!\n",asType.c_str());

		return type;
	}

	eGuiSkinAttribute cGui::GetSkinAttributeFromString(const tString& asType)
	{
		eGuiSkinAttribute type = (eGuiSkinAttribute)ohpl_gui_skin_from_name(kOhplSkinAttribute, asType.data(), asType.size());
		if(type == eGuiSkinAttribute_LastEnum) Warning("Skin Attribute type '%s' does not exist!\n",asType.c_str());

		return type;
	}

	//-----------------------------------------------------------------------

	static tString SkinTypeString(int alKind, unsigned int alType)
	{
		tString sName;
		ohpl_gui_skin_name(alKind, alType, [](void* c, const char* p, size_t n){ static_cast<tString*>(c)->assign(p, n); }, &sName);
		return sName;
	}

	tString cGui::GetSkinGfxString(eGuiSkinGfx aType)
	{
		return SkinTypeString(kOhplSkinGfx, aType);
	}
	
	tString cGui::GetSkinFontString(eGuiSkinFont aType)
	{
		return SkinTypeString(kOhplSkinFont, aType);
	}
	
	tString cGui::GetSkinAttributeString(eGuiSkinAttribute aType)
	{
		return SkinTypeString(kOhplSkinAttribute, aType);
	}

	//-----------------------------------------------------------------------

	cGuiSet* cGui::CreateSet(const tString& asName, cGuiSkin *apSkin)
	{
		cGuiSet *pSet = hplNew( cGuiSet, (asName,this,apSkin,mpResources, mpGraphics, mpSound,mpScene) );

		m_mapSets.insert(tGuiSetMap::value_type(asName, pSet));
        
		return pSet;
	}
	
	cGuiSet* cGui::GetSetFromName(const tString& asName)
	{
		tGuiSetMapIt it = m_mapSets.find(asName);
		if(it == m_mapSets.end()) return NULL;

		return it->second;
	}
	
	void cGui::SetFocus(cGuiSet* apSet)
	{
		if(mpSetInFocus == apSet) return;

		//TODO: Call lost focus or stuff

		mpSetInFocus = apSet;
	}
	
	void cGui::SetFocusByName(const tString& asSetName)
	{
		cGuiSet *pSet = GetSetFromName(asSetName);
		if(pSet) SetFocus(pSet);
	}
	
	void cGui::DestroySet(cGuiSet *apSet)
	{
		if(apSet==NULL) return;

		tGuiSetMapIt it = m_mapSets.begin();
		for(; it != m_mapSets.end(); ++it)
		{
			if(apSet == it->second)
			{
				m_mapSets.erase(it);
				hplDelete( apSet);
				break;
			}
		}
	}

	//-----------------------------------------------------------------------

	cGuiGfxElement* cGui::CreateGfxFilledRect(const cColor& aColor, eGuiMaterial aMaterial, bool abAddToList)
	{
		cGuiGfxElement *pGfxElem = hplNew( cGuiGfxElement, (this) );

		pGfxElem->SetColor(aColor);
		pGfxElem->SetMaterial(GetMaterial(aMaterial));

		if(abAddToList) mlstGfxElements.push_back(pGfxElem);

		return pGfxElem;
	}

	//-----------------------------------------------------------------------

	cGuiGfxElement* cGui::CreateGfxImage(	const tString &asFile, eGuiMaterial aMaterial,
		const cColor& aColor, bool abAddToList)
	{
		////////////////////////////
		// Load image
		cFrameSubImage *pImage = mpResources->GetImageManager()->CreateImage(asFile);
		if(pImage==NULL){
			Error("Could not load image '%s'!\n",asFile.c_str());
			return NULL;
		}

		/////////////////////////////
		// Create element
		cGuiGfxElement *pGfxElem = hplNew( cGuiGfxElement, (this) );

		pGfxElem->SetColor(aColor);
		pGfxElem->SetMaterial(GetMaterial(aMaterial));
		pGfxElem->AddImage(pImage);

		if(abAddToList) mlstGfxElements.push_back(pGfxElem);

		//if(abAddToList) Log("Create gfx '%s': %d %d\n", asFile.c_str(), pGfxElem, pImage);

		return pGfxElem;
	}

	//-----------------------------------------------------------------------

	cGuiGfxElement* cGui::CreateGfxTexture(	const tString &asFile,
											eGuiMaterial aMaterial,
											eTextureType aTextureType,
											const cColor& aColor, bool abMipMaps, 
											bool abAddToList)
	{
		///////////////////
		// Error check
		if( aTextureType != eTextureType_2D && aTextureType != eTextureType_Rect )
		{
			Error("Texture %s could not be loaded because texture type param was not valid!\n",asFile.c_str());
			return NULL;
		}

		///////////////////
		// Load texture
		iTexture *pTexture = mpResources->GetTextureManager()->Create2D(asFile,abMipMaps,aTextureType);
		if(pTexture==NULL)
		{
			Error("Could not load texture '%s'!\n",asFile.c_str());
			return NULL;
		}
		
		/////////////////////////////
		// Create element
		cGuiGfxElement *pGfxElem = CreateGfxTexture(pTexture,true,aMaterial,aColor,abAddToList);

		return pGfxElem;
	}

	//-----------------------------------------------------------------------

	cGuiGfxElement* cGui::CreateGfxTexture(iTexture *apTexture, bool abAutoDestroyTexture, 
											eGuiMaterial aMaterial,
											const cColor& aColor,
											bool abAddToList,
											const cVector2f& avStartUV,
											const cVector2f& avEndUV)
	{
		cGuiGfxElement *pGfxElem = hplNew( cGuiGfxElement, (this) );

		if(apTexture->GetUsage() == eTextureUsage_RenderTarget) pGfxElem->SetFlipUvYAxis(true);

		pGfxElem->SetColor(aColor);
		pGfxElem->SetMaterial(GetMaterial(aMaterial));
		pGfxElem->AddTexture(apTexture, avStartUV, avEndUV);
		pGfxElem->SetDestroyTexture(abAutoDestroyTexture);

		if(abAddToList) mlstGfxElements.push_back(pGfxElem);

		return pGfxElem;
	}

	//-----------------------------------------------------------------------

	cGuiGfxElement* cGui::CreateGfxImageBuffer(	const tString &asFile,eGuiMaterial aMaterial,
		bool abCreateAnimation,
		const cColor& aColor, bool abAddToList)
	{
		////////////////////////////
		// Load images
		tString sName = cString::SetFileExt(asFile,"");
		tString sExt = cString::GetFileExt(asFile);

		std::vector<cFrameSubImage*> vImages;

		int lFileNum =0;
		while(true)
		{
			tString sNum = lFileNum<=9 ? "0"+cString::ToString(lFileNum) : cString::ToString(lFileNum);
			tString sFile = sName + sNum+"."+sExt;

			if(mpResources->GetFileSearcher()->GetFilePath(sFile)==_W("")) break;

			cFrameSubImage *pImage = mpResources->GetImageManager()->CreateImage(sFile);
			vImages.push_back(pImage);
			++lFileNum;
		}

		if(vImages.empty())
		{
			Error("Could not load any images with '%s' as base!\n",asFile.c_str());
			return NULL;
		}

		/////////////////////////////
		// Create element
		cGuiGfxElement *pGfxElem = hplNew( cGuiGfxElement, (this) );

		pGfxElem->SetColor(aColor);
		pGfxElem->SetMaterial(GetMaterial(aMaterial));

		for(size_t i=0; i< vImages.size(); ++i)
		{
			if(i==0) pGfxElem->AddImage(vImages[i]);
			pGfxElem->AddImageToBuffer(vImages[i]);
		}

		if(abAddToList) mlstGfxElements.push_back(pGfxElem);

		///////////////////////////////
		// Create animation
		if(abCreateAnimation)
		{
			cGuiGfxAnimation *pAnim = pGfxElem->CreateAnimtion("Default");
			for(size_t i=0; i<vImages.size(); ++i)
			{
				pAnim->AddFrame((int)i);
			}
		}

		return pGfxElem;
	}


	//-----------------------------------------------------------------------

	void cGui::DestroyGfx(cGuiGfxElement* apGfx)
	{
		//STLFindAndDelete(mlstGfxElements,apGfx);

		tGuiGfxElementListIt it = mlstGfxElements.begin();
		for(; it != mlstGfxElements.end(); ++it)
		{
			cGuiGfxElement*pGfx = *it;
			if(pGfx == apGfx)
			{
				mlstToBeDestroyedGfxElements.push_back(pGfx);
				mlstGfxElements.erase(it);
				break;
			}
		}
	}

	//-----------------------------------------------------------------------

	
	bool cGui::SendMousePos(const cVector2l &avPos, const cVector2l &avRel)
	{
		if(mpSetInFocus==NULL) return false;

		cVector2f vVirtualPos = cVector2f( (float)avPos.x, (float)avPos.y) / mpGraphics->GetLowLevel()->GetScreenSizeFloat();
		cVector2f vVirtualRel = cVector2f( (float)avRel.x, (float)avRel.y) / mpGraphics->GetLowLevel()->GetScreenSizeFloat();
		
        vVirtualPos *= mpSetInFocus->GetVirtualSize();
		vVirtualPos -= mpSetInFocus->GetVirtualSizeOffset();
		
		vVirtualRel *= mpSetInFocus->GetVirtualSize();

		return mpSetInFocus->SendMessage(eGuiMessage_MouseMove, cGuiMessageData(vVirtualPos, vVirtualRel));
	}
	
	bool cGui::SendMouseClickDown(eGuiMouseButton aButton, int alKeyModifiers)
	{
		if(mpSetInFocus==NULL)return false;

		cGuiMessageData data;
		data.mlVal = aButton;
		data.mKeyPress.mlModifier = alKeyModifiers;

		return mpSetInFocus->SendMessage(eGuiMessage_MouseDown,data);
	}
	
	bool cGui::SendMouseClickUp(eGuiMouseButton aButton, int alKeyModifiers)
	{
		if(mpSetInFocus==NULL)return false;

		cGuiMessageData data;
		data.mlVal = aButton;
		data.mKeyPress.mlModifier = alKeyModifiers;

		return mpSetInFocus->SendMessage(eGuiMessage_MouseUp, data);
	}

	bool cGui::SendMouseDoubleClick(eGuiMouseButton aButton, int alKeyModifiers)
	{
		if(mpSetInFocus==NULL)return false;

		cGuiMessageData data;
		data.mlVal = aButton;
		data.mKeyPress.mlModifier = alKeyModifiers;

		return mpSetInFocus->SendMessage(eGuiMessage_MouseDoubleClick,data);
	}

	bool cGui::SendKeyPress(const cKeyPress& keyPress)
	{
		if(mpSetInFocus==NULL)return false;

		return mpSetInFocus->SendMessage(eGuiMessage_KeyPress, cGuiMessageData(keyPress));
	}

	bool cGui::SendKeyRelease(const cKeyPress& aKeyPress)
	{
		if(mpSetInFocus==NULL)return false;

		return mpSetInFocus->SendMessage(eGuiMessage_KeyRelease, cGuiMessageData(aKeyPress));
	}

	//-----------------------------------------------------------------------

	bool cGui::SendGamepadInput(const cGamepadInputData& aInput)
	{
		if(mpSetInFocus==NULL)return false;

		return mpSetInFocus->SendMessage(eGuiMessage_GamepadInput, cGuiMessageData(aInput));
	}

	//-----------------------------------------------------------------------

	bool cGui::SendUIArrowPress(eUIArrow aX)
	{
		if(mpSetInFocus==NULL) return false;

		return mpSetInFocus->SendMessage(eGuiMessage_UIArrowPress, cGuiMessageData(aX));
	}

	bool cGui::SendUIArrowRelease(eUIArrow aX)
	{
		if(mpSetInFocus==NULL) return false;

		return mpSetInFocus->SendMessage(eGuiMessage_UIArrowRelease, cGuiMessageData(aX));
	}

	bool cGui::SendUIButtonPress(eUIButton aX)
	{
		if(mpSetInFocus==NULL) return false;

		return mpSetInFocus->SendMessage(eGuiMessage_UIButtonPress, cGuiMessageData(aX));
	}
	
	bool cGui::SendUIButtonRelease(eUIButton aX)
	{
		if(mpSetInFocus==NULL) return false;

		return mpSetInFocus->SendMessage(eGuiMessage_UIButtonRelease, cGuiMessageData(aX));
	}

	bool cGui::SendUIButtonDoublePress(eUIButton aX)
	{
		if(mpSetInFocus==NULL) return false;

		return mpSetInFocus->SendMessage(eGuiMessage_UIButtonDoublePress, cGuiMessageData(aX));
	}
	
	//-----------------------------------------------------------------------
	
	eMouseButton cGui::TranslateGuiMouseToMouse(eGuiMouseButton aButton)
	{
		return (eMouseButton)ohpl_gui_mouse_to_mouse(aButton);
	}

	//-----------------------------------------------------------------------

	eGuiMouseButton cGui::TranslateMouseToGuiMouse(eMouseButton aButton)
	{
		return (eGuiMouseButton)ohpl_gui_mouse_to_gui(aButton);
	}

	//-----------------------------------------------------------------------

}
