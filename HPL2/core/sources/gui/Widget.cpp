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
#include <string.h>

#include "math/Math.h"

namespace hpl {


	struct OhplH19HostImpl
	{
		static iWidget* Wd(OhplH19W w) { return (iWidget*)w; }
		static OhplH19Base* Base(void*, OhplH19W w) { return &Wd(w)->mB; }

		static bool Call(void*, OhplH19W w, int op, OhplH19W other, const cGuiMessageData* d, float f)
		{
			iWidget* pW = Wd(w);
			switch(op)
			{
			case kOhplH19OnMessage:			return pW->OnMessage((eGuiMessage)other, *d);
			case kOhplH19Virtual:
				switch((eGuiMessage)other)
				{
				case eGuiMessage_MouseMove:				return pW->OnMouseMove(*d);
				case eGuiMessage_MouseDown:				return pW->OnMouseDown(*d);
				case eGuiMessage_MouseUp:				return pW->OnMouseUp(*d);
				case eGuiMessage_MouseDoubleClick:		return pW->OnMouseDoubleClick(*d);
				case eGuiMessage_MouseEnter:			return pW->OnMouseEnter(*d);
				case eGuiMessage_MouseLeave:			return pW->OnMouseLeave(*d);
				case eGuiMessage_KeyPress:				return pW->OnKeyPress(*d);
				case eGuiMessage_GotFocus:				return pW->OnGotFocus(*d);
				case eGuiMessage_GotTabFocus:			return pW->OnGotTabFocus(*d);
				case eGuiMessage_LostFocus:				return pW->OnLostFocus(*d);
				case eGuiMessage_UIArrowPress:			return pW->OnUIArrowPress(*d);
				case eGuiMessage_UIArrowRelease:		return pW->OnUIArrowRelease(*d);
				case eGuiMessage_UIButtonPress:			return pW->OnUIButtonPress(*d);
				case eGuiMessage_UIButtonDoublePress:	return pW->OnUIButtonDoublePress(*d);
				case eGuiMessage_UIButtonRelease:		return pW->OnUIButtonRelease(*d);
				case eGuiMessage_GetUINavFocus:			return pW->OnGetUINavFocus(*d);
				case eGuiMessage_LoseUINavFocus:		return pW->OnLoseUINavFocus(*d);
				default: return false;
				}
			case kOhplH19ChangePosition:	pW->OnChangePosition(); return false;
			case kOhplH19ChangeSize:		pW->OnChangeSize(); return false;
			case kOhplH19ChangeText:		pW->OnChangeText(); return false;
			case kOhplH19ChildUpdate:		pW->OnChildUpdate(Wd(other)); return false;
			case kOhplH19OnAttachChild:		pW->OnAttachChild(Wd(other)); return false;
			case kOhplH19OnRemoveChild:		pW->OnRemoveChild(Wd(other)); return false;
			case kOhplH19RemoveChild:		pW->RemoveChild(Wd(other)); return false;
			case kOhplH19ChildGotFocus:		pW->OnChildGotFocus(Wd(other), *d); return false;
			case kOhplH19ChildGotTabFocus:	pW->OnChildGotTabFocus(Wd(other), *d); return false;
			case kOhplH19ChildLostFocus:	pW->OnChildLostFocus(Wd(other), *d); return false;
			case kOhplH19Update:			pW->OnUpdate(f); return false;
			case kOhplH19Draw:				pW->OnDraw(f, (cGuiClipRegion*)other); return false;
			case kOhplH19DrawAfterClip:		pW->OnDrawAfterClip(f, (cGuiClipRegion*)other); return false;
			case kOhplH19PointIsInside:		return pW->PointIsInside(d->mvPos, f != 0);
			}
			return false;
		}

		static tWidgetListIt Cursor(uintptr_t c) { tWidgetListIt it; memcpy(&it, &c, sizeof(it)); return it; }
		static uintptr_t Pack(tWidgetListIt it) { uintptr_t c; memcpy(&c, &it, sizeof(c)); return c; }
		static uintptr_t Begin(void*, OhplH19W w) { return Pack(Wd(w)->mlstChildren.begin()); }
		static bool End(void*, OhplH19W w, uintptr_t c) { return Cursor(c) == Wd(w)->mlstChildren.end(); }
		static OhplH19W Get(void*, OhplH19W, uintptr_t c) { return (OhplH19W)*Cursor(c); }
		static uintptr_t Next(void*, OhplH19W, uintptr_t c) { tWidgetListIt it = Cursor(c); ++it; return Pack(it); }
		static void Erase(void*, OhplH19W w, uintptr_t c) { Wd(w)->mlstChildren.erase(Cursor(c)); }
		static void Push(void*, OhplH19W w, OhplH19W child) { Wd(w)->mlstChildren.push_back(Wd(child)); }
		static void NeedsSorting(void*, OhplH19W w) { Wd(w)->mpSet->SetWidgetsNeedSorting(); }
		static void VirtualSize(void*, OhplH19W w, float* out)
		{
			cVector2f v = Wd(w)->mpSet->GetVirtualSize();
			out[0] = v.x; out[1] = v.y;
		}
		static uintptr_t ClipChild(void*, OhplH19W, uintptr_t clip, const float* pos, const float* size)
		{
			return (uintptr_t)((cGuiClipRegion*)clip)->CreateChild(cVector3f(pos[0], pos[1], pos[2]), cVector2f(size[0], size[1]));
		}
		static void SetClip(void*, OhplH19W w, uintptr_t clip) { Wd(w)->mpSet->SetCurrentClipRegion((cGuiClipRegion*)clip); }
	};

	static_assert(sizeof(tWidgetListIt) == sizeof(uintptr_t), "list iterator is one word");

	static const OhplH19Host gH19Host = {
		NULL, &OhplH19HostImpl::Base, &OhplH19HostImpl::Call, &OhplH19HostImpl::Begin, &OhplH19HostImpl::End,
		&OhplH19HostImpl::Get, &OhplH19HostImpl::Next, &OhplH19HostImpl::Erase, &OhplH19HostImpl::Push,
		&OhplH19HostImpl::NeedsSorting, &OhplH19HostImpl::VirtualSize, &OhplH19HostImpl::ClipChild, &OhplH19HostImpl::SetClip
	};

	#define H19W ((OhplH19W)this)

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
		ohpl_h19_base_init(&mB, true);

		mpSet = apSet;
		mpSkin = apSkin;
		mpGui = mpSet->GetGui();

		mType = aType;

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

		ohpl_h19_base_free(&mB);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iWidget::Update(float afTimeStep)
	{
		ohpl_h19_update(&gH19Host, H19W, afTimeStep);
	}

	//-----------------------------------------------------------------------

	void iWidget::Draw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		ohpl_h19_draw(&gH19Host, H19W, afTimeStep, (uintptr_t)apClipRegion);
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
		return ohpl_h19_process_message(&gH19Host, H19W, aMessage, &aData, abSkipVisCheck, abSkipEnabledCheck);
	}

	//-----------------------------------------------------------------------

	void iWidget::AddCallback(eGuiMessage aMessage,void *apObject,tGuiCallbackFunc apFunc)
	{
		ohpl_h19_add_callback(&gH19Host, H19W, aMessage, apObject, reinterpret_cast<OhplH19Callback>(apFunc));
	}

	//-----------------------------------------------------------------------

	bool iWidget::PointIsInside(const cVector2f& avPoint, bool abOnlyClipped)
	{
		return ohpl_h19_point_is_inside(&gH19Host, H19W, avPoint.x, avPoint.y, abOnlyClipped);
	}

	//-----------------------------------------------------------------------

	void iWidget::AttachChild(iWidget *apChild)
	{
		ohpl_h19_attach_child(&gH19Host, H19W, (OhplH19W)apChild);
	}
	
	void iWidget::RemoveChild(iWidget *apChild)
	{
		ohpl_h19_remove_child(&gH19Host, H19W, (OhplH19W)apChild);
	}

	//-----------------------------------------------------------------------

	void iWidget::SetEnabled(bool abX)
	{
		ohpl_h19_set_enabled(&gH19Host, H19W, abX);
	}

	bool iWidget::IsEnabled()
	{
		return ohpl_h19_is_enabled(&gH19Host, H19W);
	}
	
	//-----------------------------------------------------------------------

	void iWidget::SetVisible(bool abX)
	{
		ohpl_h19_set_visible(&gH19Host, H19W, abX);
	}

	bool iWidget::IsVisible()
	{
		return ohpl_h19_is_visible(&gH19Host, H19W);
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

		msText = asText;
		ohpl_h19_text_changed(&gH19Host, H19W);
	}

	//-----------------------------------------------------------------------

	void iWidget::SetPosition(const cVector3f &avPos)
	{
		ohpl_h19_set_position(&gH19Host, H19W, &avPos);
	}

	void iWidget::SetGlobalPosition(const cVector3f &avPos)
	{
		ohpl_h19_set_global_position(&gH19Host, H19W, &avPos);
	}
	
	const cVector3f& iWidget::GetLocalPosition()
	{
		return mvPosition;
	}
	
	const cVector3f& iWidget::GetGlobalPosition()
	{
		ohpl_h19_refresh_global(&gH19Host, H19W);
		return mpParent ? mvGlobalPosition : mvPosition;
	}

	//-----------------------------------------------------------------------

	void iWidget::SetChildrenOffset(const cVector3f& avX)
	{
		ohpl_h19_set_children_offset(&gH19Host, H19W, &avX);
	}

	//-----------------------------------------------------------------------

	void iWidget::SetAffectedByScroll(bool abX)
	{
		ohpl_h19_set_affected_by_scroll(&gH19Host, H19W, abX);
	}

	void iWidget::SetScrollAmount(const cVector3f& avX)
	{
		ohpl_h19_set_scroll_amount(&gH19Host, H19W, &avX);
	}

	//-----------------------------------------------------------------------

	void iWidget::CenterGlobalPositionInSet()
	{
		ohpl_h19_center_in_set(&gH19Host, H19W);
	}
	
	//-----------------------------------------------------------------------

	void iWidget::SetSize(const cVector2f &avSize)
	{
		ohpl_h19_set_size(&gH19Host, H19W, &avSize);
	}
	
	//-----------------------------------------------------------------------

	bool iWidget::ClipsGraphics()
	{
		return ohpl_h19_clips_graphics(&gH19Host, H19W);
	}

	//-----------------------------------------------------------------------

	bool iWidget::IsConnectedTo(iWidget *apWidget, bool abIsStartWidget)
	{
		return ohpl_h19_is_connected_to(&gH19Host, H19W, (OhplH19W)apWidget, abIsStartWidget);
	}


	//-----------------------------------------------------------------------

	cGuiGfxElement* iWidget::GetPointerGfx()
	{
		return mpPointerGfx;
	}

	//-----------------------------------------------------------------------

	void iWidget::SetFocusNavigation(eUIArrow aDir, iWidget* apWidget)
	{
		ohpl_h19_set_focus_navigation(&gH19Host, H19W, aDir, (OhplH19W)apWidget);
	}

	iWidget* iWidget::GetFocusNavigation(eUIArrow aDir)
	{
		return (iWidget*)ohpl_h19_focus_navigation(&gH19Host, H19W, aDir);
	}

	//-----------------------------------------------------------------------

	bool iWidget::HasFocusNavigation()
	{
		return ohpl_h19_has_focus_navigation(&gH19Host, H19W);
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	bool iWidget::GetFocus(const cGuiMessageData& aData)
	{
		return ohpl_h19_focus_message(&gH19Host, H19W, eGuiMessage_GotFocus, kOhplH19ChildGotFocus, &aData);
	}

	bool iWidget::OnGotFocus(const cGuiMessageData& aData)
	{
		return mbEnabled;
	}

	void iWidget::OnChildGotFocus(iWidget* apChild, const cGuiMessageData& aData)
	{
		ohpl_h19_forward_to_parent(&gH19Host, H19W, kOhplH19ChildGotFocus, (OhplH19W)apChild, &aData);
	}

	//-----------------------------------------------------------------------

	bool iWidget::GetTabFocus(const cGuiMessageData& aData)
	{
		return ohpl_h19_focus_message(&gH19Host, H19W, eGuiMessage_GotTabFocus, kOhplH19ChildGotTabFocus, &aData);
	}

	bool iWidget::OnGotTabFocus(const cGuiMessageData& aData)
	{
		return mbEnabled;
	}

	void iWidget::OnChildGotTabFocus(iWidget* apChild, const cGuiMessageData& aData)
	{
		ohpl_h19_forward_to_parent(&gH19Host, H19W, kOhplH19ChildGotTabFocus, (OhplH19W)apChild, &aData);
	}

	//-----------------------------------------------------------------------

	bool iWidget::LoseFocus(const cGuiMessageData& aData)
	{
		return ohpl_h19_focus_message(&gH19Host, H19W, eGuiMessage_LostFocus, kOhplH19ChildLostFocus, &aData);
	}

	void iWidget::OnChildLostFocus(iWidget* apChild, const cGuiMessageData& aData)
	{
		ohpl_h19_forward_to_parent(&gH19Host, H19W, kOhplH19ChildLostFocus, (OhplH19W)apChild, &aData);
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
