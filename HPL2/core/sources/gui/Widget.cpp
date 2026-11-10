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

#include "gui/Widget.h"

#include "system/LowLevelSystem.h"

#include "graphics/FontData.h"

#include "system/String.h"

#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/GuiGfxElement.h"
#include "gui/RustWidget.h"

#include "math/Math.h"

namespace hpl {

	static OhplWdParentPos ParentPosOf(iWidget* apParent)
	{
		OhplWdParentPos pos;
		pos.global = apParent->GetGlobalPosition();
		pos.children_offset = apParent->GetChildrenOffset();
		pos.scroll = apParent->GetScrollAmount();
		return pos;
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iWidget::iWidget(eWidgetType aType,cGuiSet *apSet, cGuiSkin *apSkin)
	{
		mpSet = apSet;
		mpSkin = apSkin;
		mpGui = mpSet->GetGui();

		mType = aType;

		mvCallbackLists.resize(eGuiMessage_LastEnum);

		mpParent = NULL;

		mvPosition =0;
		mvSize = 0;
		mvChildrenOffset = 0;
		mvScrollAmount = 0;
		mbAffectedByScroll = true;

		mbEnabled = true;
		mbVisible = true;

		mbClipsGraphics = false;

		mbMouseIsOver = false;

		msText =_W("");

		mbPositionIsUpdated = true;

		mlPositionCount =0;

		mbConnectedToChildren = true;

		mColorMul = cColor(1,1);

		if(mpSkin)  mpPointerGfx = mpSkin->GetGfx(eGuiSkinGfx_PointerNormal);
		else		mpPointerGfx = NULL;

		mpUserData = NULL;
		mlUserValue = 0;

		mbToolTipEnabled = false;

		mbTextChanged = false;

		mbCallbacksDisabled = false;

		mvFocusNavWidgets.resize(4, NULL);

		mbGlobalKeyPressListener = false;
		mbGlobalUIInputListener = false;
	}

	//-----------------------------------------------------------------------

	iWidget::~iWidget()
	{
		if(mpSet->GetToolTipWidget()==this)
			mpSet->SetToolTipWidget(NULL);

		for(size_t i=0; i<mvShortcuts.size(); ++i)
		{
			mpSet->RemoveGlobalShortcut(mvShortcuts[i]);
		}

		////////////////////////
		//Remove all children
		tWidgetListIt it = mlstChildren.begin();
		while(it != mlstChildren.end())
		{
			iWidget* pChild = *it;

			RemoveChild(pChild);
			it = mlstChildren.begin();
		}
		
		////////////////////////////
		//Remove from parent
		if(mpParent) mpParent->RemoveChild(this);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iWidget::Update(float afTimeStep)
	{
		////////////////////////////
		//Update callback
		cGuiMessageData data;
		data.mfVal = afTimeStep;
		ProcessMessage(eGuiMessage_OnUpdate, data);

		/////////////////////////////
		//Implemented update
		OnUpdate(afTimeStep);
	}

	//-----------------------------------------------------------------------

	void iWidget::Draw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		if(mbVisible==false) return;
		
		OnDraw(afTimeStep, apClipRegion);

		cGuiClipRegion *pChildRegion = apClipRegion;
		if(mbClipsGraphics)
		{
			pChildRegion = apClipRegion->CreateChild(GetGlobalPosition(), mvSize);
			mpSet->SetCurrentClipRegion(pChildRegion);
		}

		OnDrawAfterClip(afTimeStep,apClipRegion);

		/////////////////////////////////
		//Draw callbacks
		cGuiMessageData data;
		data.mfVal = afTimeStep;
		data.mpData = apClipRegion;
		ProcessMessage(eGuiMessage_OnDraw, data);
		
		/////////////////////////////////
		//Draw children
		tWidgetListIt it = mlstChildren.begin();
		for(; it != mlstChildren.end(); ++it)
		{
			iWidget *pChild = *it;

			pChild->Draw(afTimeStep, pChildRegion);
		}

		if(mbClipsGraphics) mpSet->SetCurrentClipRegion(apClipRegion);
	}
	
	//-----------------------------------------------------------------------
	
	void iWidget::Init()
	{
		OnInit();
		LoadGraphics();
	}

	//-----------------------------------------------------------------------

	cGuiGlobalShortcut* iWidget::AddShortcut(int alKeyModifiers, eKey aKey, eGuiMessage aMsg, bool abBypassVisibility, bool abBypassEnabled)
	{
		cGuiGlobalShortcut* pShortcut = mpSet->AddGlobalShortcut(alKeyModifiers, aKey, this, aMsg, abBypassVisibility, abBypassEnabled);
		mvShortcuts.push_back(pShortcut);

		return pShortcut;
	}

	//-----------------------------------------------------------------------

	bool iWidget::ProcessMessage(eGuiMessage aMessage, const cGuiMessageData& aData, bool abSkipVisCheck, bool abSkipEnabledCheck)
	{
		if(mbCallbacksDisabled ||
			!abSkipVisCheck && IsVisible()==false ||
			!abSkipEnabledCheck && IsEnabled()==false) return false;
		
		cGuiMessageData tData = aData;
		tData.mMessage = aMessage;

		bool bRet = false;
		bRet = OnMessage(aMessage,tData); //This can override any message.

		/////////////////////////////////////////
		//Call the correct virtual function
		if(bRet==false)
		{
			switch(aMessage)
			{
				case eGuiMessage_MouseMove:				bRet = OnMouseMove(tData); break;
				case eGuiMessage_MouseDown:				bRet = OnMouseDown(tData); break;
				case eGuiMessage_MouseUp:				bRet = OnMouseUp(tData); break;
				case eGuiMessage_MouseDoubleClick:		bRet = OnMouseDoubleClick(tData); break;
				case eGuiMessage_MouseEnter:			bRet = OnMouseEnter(tData); break;
				case eGuiMessage_MouseLeave:			bRet = OnMouseLeave(tData); break;
				case eGuiMessage_KeyPress:				bRet = OnKeyPress(tData); break;
				case eGuiMessage_GotFocus:				bRet = OnGotFocus(tData); break;
				case eGuiMessage_GotTabFocus:			bRet = OnGotTabFocus(tData); break;
				case eGuiMessage_LostFocus:				bRet = OnLostFocus(tData); break;
				case eGuiMessage_UIArrowPress:			bRet = OnUIArrowPress(tData); break;
				case eGuiMessage_UIArrowRelease:		bRet = OnUIArrowRelease(tData); break;
				case eGuiMessage_UIButtonPress:			bRet = OnUIButtonPress(tData); break;
				case eGuiMessage_UIButtonDoublePress:	bRet = OnUIButtonDoublePress(tData); break;
				case eGuiMessage_UIButtonRelease:		bRet = OnUIButtonRelease(tData); break;
				case eGuiMessage_GetUINavFocus:			bRet = OnGetUINavFocus(tData); break;
				case eGuiMessage_LoseUINavFocus:		bRet = OnLoseUINavFocus(tData); break;
			}
		}

		/////////////////////////////////////////
		//Process user callbacks for the event.
		if(ProcessCallbacks(aMessage,tData)) bRet = true;
		
		return bRet;
	}

	//-----------------------------------------------------------------------

	void iWidget::AddCallback(eGuiMessage aMessage,void *apObject,tGuiCallbackFunc apFunc)
	{
		mvCallbackLists[aMessage].push_back(cWidgetCallback(apObject, apFunc));
	}

	//-----------------------------------------------------------------------

	bool iWidget::PointIsInside(const cVector2f& avPoint, bool abOnlyClipped)
	{
		int lStage = ohpl_wd_point_stage(CheckPointInsideClippingParent(avPoint), abOnlyClipped, mbClipsGraphics);
		if(lStage != 2) return lStage == 1;

		cVector3f vGlobalPos = GetGlobalPosition();
		return ohpl_wd_point_in_box(&avPoint, &vGlobalPos, &mvSize);
	}

	//-----------------------------------------------------------------------

	void iWidget::AttachChild(iWidget *apChild)
	{
		if(apChild->mpParent)
		{
			iWidget *pParent = apChild->mpParent;
			pParent->RemoveChild(apChild);
			cVector3f vGlobal = GetGlobalPosition();
			cVector3f vLocal;
			ohpl_wd_rebase(&apChild->mvPosition, &vGlobal, false, &vLocal);
			apChild->SetPosition(vLocal);
		}
		apChild->mpParent = this;
		apChild->SetPositionUpdated();
		mlstChildren.push_back(apChild);

		OnChildUpdate(apChild);
		OnAttachChild(apChild);
	}
	
	void iWidget::RemoveChild(iWidget *apChild)
	{
		tWidgetListIt it = mlstChildren.begin();
		for(; it != mlstChildren.end(); ++it)
		{
			iWidget *pChild = *it;

			if(pChild == apChild)
			{
				mlstChildren.erase(it);
				
				pChild->mpParent = NULL;
				pChild->SetPositionUpdated();
				cVector3f vGlobal = GetGlobalPosition();
				cVector3f vLocal;
				ohpl_wd_rebase(&pChild->mvPosition, &vGlobal, true, &vLocal);
				pChild->SetPosition(vLocal);

				OnRemoveChild(pChild);

				break;
			}
		}
	}

	//-----------------------------------------------------------------------

	void iWidget::SetEnabled(bool abX)
	{
		if(mbEnabled == abX) return;

		mbEnabled = abX;

	}

	bool iWidget::IsEnabled()
	{
		if(mpParent)
		{
			if(mpParent->IsEnabled()) return mbEnabled;
			else					return false;
		}
		
		return mbEnabled;
	}
	
	//-----------------------------------------------------------------------

	void iWidget::SetVisible(bool abX)
	{
		if(mbVisible == abX) return;

		mbVisible = abX;

		if(mpParent)
			mpParent->OnChildUpdate(this);
	}

	bool iWidget::IsVisible()
	{
		if(mpParent)
		{
			if(mpParent->IsVisible())	return mbVisible;
			else						return false;
		}

		return mbVisible;
	}

	//-----------------------------------------------------------------------
	
	bool iWidget::HasFocus()
	{
		return mpSet->GetFocusedWidget() == this;
	}

	//-----------------------------------------------------------------------

	void iWidget::SetText(const tWString& asText)
	{
		if(asText == msText) return;

		mbTextChanged = true;

		msText = asText;

		OnChangeText();
		ProcessMessage(eGuiMessage_TextChange, cGuiMessageData());

		if(mpParent)
			mpParent->OnChildUpdate(this);

		mbTextChanged = false;
	}

	//-----------------------------------------------------------------------

	void iWidget::SetPosition(const cVector3f &avPos)
	{
		if(mvPosition.z!=avPos.z)
			mpSet->SetWidgetsNeedSorting();

		mvPosition = avPos;

		if(mpParent)
			mpParent->OnChildUpdate(this);

		SetPositionUpdated();
	}

	void iWidget::SetGlobalPosition(const cVector3f &avPos)
	{
		OhplWdParentPos parent;
		if(mpParent) parent = ParentPosOf(mpParent);
		cVector3f vLocal;
		ohpl_wd_global_to_local(&avPos, mpParent ? &parent : NULL, &vLocal);
		SetPosition(vLocal);
	}
	
	const cVector3f& iWidget::GetLocalPosition()
	{
		return mvPosition;
	}
	
	const cVector3f& iWidget::GetGlobalPosition()
	{
		if(mpParent)
		{
			if(mbPositionIsUpdated)
			{
				mbPositionIsUpdated = false;
				OhplWdParentPos parent = ParentPosOf(mpParent);
				ohpl_wd_global_position(&parent, mbAffectedByScroll, &mvPosition, &mvGlobalPosition);
			}
			return mvGlobalPosition;
		}
		else
		{
			return mvPosition;
		}
	}

	//-----------------------------------------------------------------------

	void iWidget::SetChildrenOffset(const cVector3f& avX)
	{
		if(mvChildrenOffset==avX)
			return;

		mvChildrenOffset = avX;
		SetPositionUpdated();
	}

	//-----------------------------------------------------------------------

	void iWidget::SetAffectedByScroll(bool abX)
	{
		if(mbAffectedByScroll==abX)
			return;

		mbAffectedByScroll = abX;
		SetPositionUpdated();
	}

	void iWidget::SetScrollAmount(const cVector3f& avX)
	{
		if(mvScrollAmount==avX)
			return;

		mvScrollAmount = avX;
		SetPositionUpdated();
	}

	//-----------------------------------------------------------------------

	void iWidget::CenterGlobalPositionInSet()
	{
		cVector2f vSetSize = mpSet->GetVirtualSize();
		float fGlobalZ = GetGlobalPosition().z;

		cVector3f vCentered;
		ohpl_wd_center_in_set(&vSetSize, &mvSize, fGlobalZ, &vCentered);
		SetGlobalPosition(vCentered);
	}
	
	//-----------------------------------------------------------------------

	void iWidget::SetSize(const cVector2f &avSize)
	{
		mvSize = avSize;
		
		OnChangeSize();

		if(mpParent)
			mpParent->OnChildUpdate(this);
	}
	
	//-----------------------------------------------------------------------

	bool iWidget::ClipsGraphics()
	{
		if(mpParent && mpParent->ClipsGraphics()) return true;
		
		return mbClipsGraphics;
	}

	//-----------------------------------------------------------------------

	bool iWidget::IsConnectedTo(iWidget *apWidget, bool abIsStartWidget)
	{
		if(abIsStartWidget == false && mbConnectedToChildren==false) return false; 

		if(apWidget == NULL) return false;
		if(apWidget == this) return true;

		if(mpParent) return mpParent->IsConnectedTo(apWidget,false);

		return false;
	}


	//-----------------------------------------------------------------------

	cGuiGfxElement* iWidget::GetPointerGfx()
	{
		return mpPointerGfx;
	}

	//-----------------------------------------------------------------------

	void iWidget::SetFocusNavigation(eUIArrow aDir, iWidget* apWidget)
	{
		int lSlot = UIArrowToArrayPos(aDir);
		if(lSlot < 0) return;
		mvFocusNavWidgets[lSlot] = apWidget;
	}

	iWidget* iWidget::GetFocusNavigation(eUIArrow aDir)
	{
		int lSlot = UIArrowToArrayPos(aDir);
		if(lSlot < 0) return NULL;
		return mvFocusNavWidgets[lSlot];
	}

	//-----------------------------------------------------------------------

	bool iWidget::HasFocusNavigation()
	{
		for(size_t i=0; i<mvFocusNavWidgets.size(); ++i)
			if(mvFocusNavWidgets[i]) return true;
		
		return false;
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	bool iWidget::GetFocus(const cGuiMessageData& aData)
	{
		bool bRet = ProcessMessage(eGuiMessage_GotFocus, aData);
		if(bRet)
			OnChildGotFocus(this, aData);

		return bRet;
	}

	bool iWidget::OnGotFocus(const cGuiMessageData& aData)
	{
		return mbEnabled;
	}

	void iWidget::OnChildGotFocus(iWidget* apChild, const cGuiMessageData& aData)
	{
		if(mpParent)
			mpParent->OnChildGotFocus(apChild, aData);
	}

	//-----------------------------------------------------------------------

	bool iWidget::GetTabFocus(const cGuiMessageData& aData)
	{
		bool bRet = ProcessMessage(eGuiMessage_GotTabFocus, aData);
		if(bRet)
			OnChildGotTabFocus(this, aData);

		return bRet;
	}

	bool iWidget::OnGotTabFocus(const cGuiMessageData& aData)
	{
		return mbEnabled;
	}

	void iWidget::OnChildGotTabFocus(iWidget* apChild, const cGuiMessageData& aData)
	{
		if(mpParent)
			mpParent->OnChildGotTabFocus(apChild, aData);
	}

	//-----------------------------------------------------------------------

	bool iWidget::LoseFocus(const cGuiMessageData& aData)
	{
		bool bRet = ProcessMessage(eGuiMessage_LostFocus, aData);
		if(bRet)
			OnChildLostFocus(this, aData);

		return bRet;
	}

	void iWidget::OnChildLostFocus(iWidget* apChild, const cGuiMessageData& aData)
	{
		if(mpParent)
			mpParent->OnChildLostFocus(apChild, aData);
	}

	//-----------------------------------------------------------------------

	bool iWidget::CheckPointInsideClippingParent(const cVector2f& avPoint)
	{
		if(mpParent && mpParent->ClipsGraphics())
		{
			if(mpParent->PointIsInside(avPoint, true)==false)
			{
				return false;
			}
		}

		return true;
	}

	cVector3f iWidget::WorldToLocalPosition(const cVector3f &avPos)
	{
		cVector3f vGlobal = GetGlobalPosition();
		cVector3f vLocal;
		ohpl_wd_rebase(&avPos, &vGlobal, false, &vLocal);
		return vLocal;
	}

	//-----------------------------------------------------------------------

	cVector2f iWidget::GetPosRelativeToMouse(const cGuiMessageData& aData)
	{
		cVector3f vGlobal = GetGlobalPosition();
		cVector2f vRel;
		ohpl_wd_rel_to_mouse(&vGlobal, &aData.mvPos, &vRel);
		return vRel;
	}

	//-----------------------------------------------------------------------

	void iWidget::DrawBordersAndCorners(cGuiGfxElement *apBackground,
										cGuiGfxElement **apBorderVec,cGuiGfxElement **apCornerVec,
										const cVector3f &avPosition, const cVector2f &avSize, bool abBordersOutside)
	{
		OhplWdGfxTable table;
		table.SetFrame(apBackground, apBorderVec, apCornerVec);

		OhplWdOp ops[kOhplWdMaxOps];
		size_t lCount = ohpl_wd_borders(ops, kOhplWdMaxOps, apBackground!=NULL, abBordersOutside, &avPosition, &avSize, table.m);
		OhplWdRun(mpSet, ops, lCount, table, NULL, NULL);
	}

	//-----------------------------------------------------------------------

	void iWidget::DrawSkinText(	const tWString& asText,eGuiSkinFont aFont,
								const cVector3f& avPosition,eFontAlign aAlign)
	{
		cGuiSkinFont *pFont = mpSkin->GetFont(aFont);
		mpSet->DrawFont(asText,pFont->mpFont,avPosition,pFont->mvSize,pFont->mColor*mColorMul,
						aAlign);
	}

	//-----------------------------------------------------------------------

	void iWidget::DrawDefaultText(	const tWString& asText,
									const cVector3f& avPosition,eFontAlign aAlign)
	{
		if(mpDefaultFontType==NULL) return;

		mpSet->DrawFont(asText,mpDefaultFontType,avPosition,mvDefaultFontSize,
						mDefaultFontColor*mColorMul, aAlign);
	}

	//-----------------------------------------------------------------------

	void iWidget::DrawDefaultText(	const tWString& asText,
									const cVector3f& avPosition,eFontAlign aAlign, const cColor& aCol)
	{
		if(mpDefaultFontType==NULL) return;

		mpSet->DrawFont(asText,mpDefaultFontType,avPosition,mvDefaultFontSize,
						aCol*mColorMul, aAlign);
	}

	//-----------------------------------------------------------------------


	void iWidget::DrawDefaultTextHighlight(	const tWString& asText,
											const cVector3f& avPosition,eFontAlign aAlign) //, int alFirstVisibleChar, int alHighlightStart, int alHighlightSize )
	{
		if(mpDefaultFontType==NULL) return;
/*
		if(alHighlightSize!=asText.size())
			mpSet->DrawFont(asText,mpDefaultFontType,avPosition,mvDefaultFontSize,
						mDefaultFontColor, aAlign);
						*/
		mpSet->DrawFont(asText,mpDefaultFontType,avPosition,mvDefaultFontSize,
						mDefaultFontHighlightColor*mColorMul,aAlign);

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	bool iWidget::ProcessCallbacks(eGuiMessage aMessage, const cGuiMessageData& aData)
	{
		tWidgetCallbackList &lstCallbacks = mvCallbackLists[aMessage];

		if(lstCallbacks.empty()) return false;

		bool bRet = false;
		tWidgetCallbackListIt it = lstCallbacks.begin();
		for(; it != lstCallbacks.end(); ++it)
		{
			cWidgetCallback &callback = *it;
            
			bool bX = (callback.mpFunc)(callback.mpObject,this, aData);
			if(bX) bRet = true;
		}

		return bRet;
	}

	//-----------------------------------------------------------------------

	int iWidget::UIArrowToArrayPos(eUIArrow aDir)
	{
		return ohpl_wd_ui_arrow_slot(aDir);
	}

	//-----------------------------------------------------------------------

	void iWidget::LoadGraphics()
	{
		if(mpSkin)
		{
			mpDefaultFont = mpSkin->GetFont(eGuiSkinFont_Default);

			mpDefaultFontType = mpDefaultFont->mpFont;
			mDefaultFontColor = mpDefaultFont->mColor;
			mvDefaultFontSize = mpDefaultFont->mvSize;

			cVector3f vHighlightCol = mpSkin->GetAttribute(eGuiSkinAttribute_DefaultFontHighlightColor);
            mDefaultFontHighlightColor = cColor(vHighlightCol.x, vHighlightCol.y, vHighlightCol.z, 1);
		}
		else
		{
			mpDefaultFont = NULL;
		}

		OnLoadGraphics();
	}

	//-----------------------------------------------------------------------

	void iWidget::SetPositionUpdated()
	{
        mbPositionIsUpdated = true;
		mlPositionCount++;

		OnChangePosition();

		tWidgetListIt it = mlstChildren.begin();
		for(; it != mlstChildren.end(); ++it) 
		{
			iWidget *pChild = *it;
			pChild->SetPositionUpdated();
		}
	}


	//-----------------------------------------------------------------------

	void iWidget::SetToolTip(const tWString& asToolTip)
	{
		msToolTip = asToolTip;
		mbToolTipEnabled = (msToolTip!=_W("")); 

		if(GetSize()==0)
		{
			float fWidth = mpDefaultFontType->GetLength(mvDefaultFontSize, msText.c_str());
			float fHeight = mvDefaultFontSize.y;

			SetSize(cVector2f(fWidth, fHeight));
		}
	}


}
