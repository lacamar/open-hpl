#include "gui/RustGuiH20.h"

#include "gui/Gui.h"
#include "gui/GuiGfxElement.h"
#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/Widget.h"
#include "gui/WidgetBaseClasses.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetListBoxBase.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetTextBox.h"
#include "gui/WidgetTabFrame.h"
#include "gui/WidgetMenu.h"
#include "gui/WidgetMenuItem.h"
#include "gui/WidgetContextMenu.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetButton.h"
#include "graphics/FontData.h"
#include "math/Math.h"
#include "system/Platform.h"
#include "system/String.h"

namespace hpl {

	// Reaches iWidget's protected members through a derived type's member pointers.
	struct OhplH20Access : public iWidget
	{
		typedef void (iWidget::*tBorders)(cGuiGfxElement*, cGuiGfxElement**, cGuiGfxElement**, const cVector3f&, const cVector2f&, bool);
		typedef void (iWidget::*tText)(const tWString&, const cVector3f&, eFontAlign);
		static tBorders Borders() { return &OhplH20Access::DrawBordersAndCorners; }
		static tText Text() { return (tText)&OhplH20Access::DrawDefaultText; }
		typedef void (iWidget::*tTextCol)(const tWString&, const cVector3f&, eFontAlign, const cColor&);
		static tTextCol TextCol() { return (tTextCol)&OhplH20Access::DrawDefaultText; }
		static cColor FontColor(iWidget* apW) { return apW->*(&OhplH20Access::mDefaultFontColor); }
		static tText Highlight() { return &OhplH20Access::DrawDefaultTextHighlight; }
		static cGuiSet* Set(iWidget* apW) { return apW->*(&OhplH20Access::mpSet); }
		static cGuiSkin* Skin(iWidget* apW) { return apW->*(&OhplH20Access::mpSkin); }
		static cVector2f FontSize(iWidget* apW) { return apW->*(&OhplH20Access::mvDefaultFontSize); }
		static iFontData* Font(iWidget* apW) { return apW->*(&OhplH20Access::mpDefaultFontType); }
		static void SetPointer(iWidget* apW, cGuiGfxElement* apGfx) { apW->*(&OhplH20Access::mpPointerGfx) = apGfx; }
		static cColor HighlightColor(iWidget* apW) { return apW->*(&OhplH20Access::mDefaultFontHighlightColor); }
		static cGuiSkinFont* SkinFont(iWidget* apW) { return apW->*(&OhplH20Access::mpDefaultFont); }
		static void* State(iWidget* apW, int alKind)
		{
			if(alKind==0) return &((cWidgetTabLabel*)apW)->mH;
			if(alKind==1) return &((cWidgetTab*)apW)->mH;
			if(alKind==3) return &((cWidgetMenuItem*)apW)->mH;
			return &((cWidgetTabFrame*)apW)->mH;
		}
		static bool CheckClip(iWidget* apW, const cVector2f& avPoint) { return (apW->*(&OhplH20Access::CheckPointInsideClippingParent))(avPoint); }
		static uintptr_t MenuCall(OhplH20Args* a)
		{
			iWidgetMenu* pMenu = (iWidgetMenu*)(iWidget*)a->w;
			switch(a->i[0])
			{
			case 0: return (uintptr_t)(iWidget*)pMenu->GetHightlightedItem();
			case 1: pMenu->SetHighlightedItem((cWidgetMenuItem*)(iWidget*)a->p[0]); return 0;
			case 2: pMenu->Hide(); return 0;
			case 3: { cVector3f v = pMenu->GetArrowOffset(); a->rf[0] = v.x; a->rf[1] = v.y; a->rf[2] = v.z; return 0; }
			case 4: { cVector3f v = pMenu->GetCheckOffset(); a->rf[0] = v.x; a->rf[1] = v.y; a->rf[2] = v.z; return 0; }
			case 5: { cVector3f v = pMenu->GetTextOffset(); a->rf[0] = v.x; a->rf[1] = v.y; a->rf[2] = v.z; return 0; }
			case 6: a->rf[0] = pMenu->GetMenuItemWidth(); return 0;
			case 8: return pMenu->IsSubmenuOpen();
			case 9: return (uintptr_t)(iWidget*)pMenu->GetTopMostMenu();
			case 10: return (uintptr_t)(iWidget*)pMenu->GetParentMenu();
			case 11: pMenu->OnChildMenuHide(); return 0;
			case 12: pMenu->SetMustHide(a->i[1]!=0); return 0;
			case 13: return pMenu->mbMustHide;
			case 14: return (uintptr_t)(iWidget*)pMenu->GetParentItem();
			case 15: return (uintptr_t)pMenu->mpPrevAttention;
			case 16: return pMenu->mvMenuItems.size();
			case 17: return (uintptr_t)(iWidget*)pMenu->mvMenuItems[a->i[1]];
			case 18:
				if(a->i[1] & 1) pMenu->SetArrowPos(cVector3f(a->f[0], a->f[1], a->f[2]));
				if(a->i[1] & 2) pMenu->SetTextPos(cVector3f(a->f[3], a->f[4], a->f[5]));
				if(a->i[1] & 4) pMenu->SetCheckPos(cVector3f(a->f[6], a->f[7], a->f[8]));
				return 0;
			case 19: return (uintptr_t)(iWidget*)((cWidgetMenuItem*)(iWidget*)a->w)->GetParentMenu();
			}
			return 0;
		}
		static cVector3f ToLocal(iWidget* apW, const cVector3f& avPos) { return (apW->*(&OhplH20Access::WorldToLocalPosition))(avPos); }
	};

	static iWidget* W(uintptr_t a) { return (iWidget*)a; }
	static cWidgetItem* Item(uintptr_t a) { return (cWidgetItem*)a; }
	static cWidgetSlider* Slider(uintptr_t a) { return (cWidgetSlider*)a; }
	static iWidgetItemContainer* Cont(uintptr_t a) { return (iWidgetItemContainer*)a; }

	// Argument use per op: w target widget; p/i/f inputs; text/textLen text; rp/ri/rf results.
	static uintptr_t H20Op(void*, int aOp, OhplH20Args* a)
	{
		switch(aOp)
		{
		case kOhplH20Op_SLIDER_SET_VALUE:
			if(a->i[1]) Slider(a->w)->SetValue(a->i[0], a->i[2]!=0);
			else		Slider(a->w)->SetValue(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_GET_VALUE:
			a->ri[0] = Slider(a->w)->GetValue();
			return 0;
		case kOhplH20Op_SLIDER_SET_BAR_VALUE_SIZE:
			Slider(a->w)->SetBarValueSize(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_SET_MAX_VALUE:
			Slider(a->w)->SetMaxValue(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_SET_BUTTON_VALUE_ADD:
			Slider(a->w)->SetButtonValueAdd(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_SET_BAR_CLICK_VALUE_ADD:
			Slider(a->w)->SetBarClickValueAdd(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_GET_BUTTON_VALUE_ADD:
			a->ri[0] = Slider(a->w)->GetButtonValueAdd();
			return 0;
		case kOhplH20Op_WIDGET_SET_SIZE:
			W(a->w)->SetSize(cVector2f(a->f[0], a->f[1]));
			return 0;
		case kOhplH20Op_WIDGET_SET_POSITION:
			W(a->w)->SetPosition(cVector3f(a->f[0], a->f[1], a->f[2]));
			return 0;
		case kOhplH20Op_WIDGET_GET_SIZE:
		{
			cVector2f vSize = W(a->w)->GetSize();
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_ITEM_NUM:
			a->ri[0] = Cont(a->p[0])->GetItemNum();
			return 0;
		case kOhplH20Op_ITEM_GET:
			return (uintptr_t)Cont(a->p[0])->GetItem(a->i[0]);
		case kOhplH20Op_ITEM_IS_SELECTED:
			return Item(a->p[0])->IsSelected();
		case kOhplH20Op_ITEM_SET_SELECTED:
			Item(a->p[0])->SetSelected(a->i[0]!=0);
			return 0;
		case kOhplH20Op_ITEM_IS_SELECTABLE:
			return Item(a->p[0])->IsSelectable();
		case kOhplH20Op_ITEM_INDEX:
			a->ri[0] = Item(a->p[0])->GetIndex();
			return 0;
		case kOhplH20Op_ITEM_TEXT:
		{
			const tWString& sText = Item(a->p[0])->GetText();
			a->ri[0] = (int32_t)sText.size();
			return (uintptr_t)sText.data();
		}
		case kOhplH20Op_PROCESS_MESSAGE:
			if(a->p[0])
				return W(a->w)->ProcessMessage((eGuiMessage)a->i[0], *(const cGuiMessageData*)a->p[0], a->i[2]!=0, a->i[3]!=0);
			return W(a->w)->ProcessMessage((eGuiMessage)a->i[0], cGuiMessageData(a->i[1]));
		case kOhplH20Op_WORLD_TO_LOCAL:
		{
			cVector3f vLocal = OhplH20Access::ToLocal(W(a->w), cVector3f(cVector2f(a->f[0], a->f[1])));
			a->rf[0] = vLocal.x;
			a->rf[1] = vLocal.y;
			a->rf[2] = vLocal.z;
			return 0;
		}
		case kOhplH20Op_GET_GLOBAL:
		{
			cVector3f vGlobal = W(a->w)->GetGlobalPosition();
			a->rf[0] = vGlobal.x;
			a->rf[1] = vGlobal.y;
			a->rf[2] = vGlobal.z;
			return 0;
		}
		case kOhplH20Op_GET_FONT_SIZE:
		{
			cVector2f vSize = OhplH20Access::FontSize(W(a->w));
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_HAS_FOCUS:
			return W(a->w)->HasFocus();
		case kOhplH20Op_CREATE_SLIDER:
			return (uintptr_t)(iWidget*)OhplH20Access::Set(W(a->w))->CreateWidgetSlider(eWidgetSliderOrientation_Vertical, 0, 0, 0, W(a->w));
		case kOhplH20Op_ADD_CALLBACK:
			W(a->w)->AddCallback((eGuiMessage)a->i[0], (void*)a->p[0], (tGuiCallbackFunc)a->p[1]);
			return 0;
		case kOhplH20Op_DRAW_GFX:
		{
			cGuiSet* pSet = OhplH20Access::Set(W(a->w));
			cVector3f vPos(a->f[0], a->f[1], a->f[2]);
			cVector2f vSize(a->f[3], a->f[4]);
			if(a->i[1]) pSet->DrawGfx((cGuiGfxElement*)a->p[0], vPos);
			else if(a->i[0]) pSet->DrawGfx((cGuiGfxElement*)a->p[0], vPos, vSize, cColor(a->f[5], a->f[6], a->f[7], a->f[8]));
			else		pSet->DrawGfx((cGuiGfxElement*)a->p[0], vPos, vSize);
			return 0;
		}
		case kOhplH20Op_DRAW_BORDERS:
		{
			iWidget* pWidget = W(a->w);
			cVector3f vPos(a->f[0], a->f[1], a->f[2]);
			cVector2f vSize(a->f[3], a->f[4]);
			(pWidget->*OhplH20Access::Borders())((cGuiGfxElement*)a->p[0], (cGuiGfxElement**)a->p[1], (cGuiGfxElement**)a->p[2], vPos, vSize,
												 a->i[1]!=0);
			return 0;
		}
		case kOhplH20Op_SKIN_GFX:
			return (uintptr_t)OhplH20Access::Skin(W(a->w))->GetGfx((eGuiSkinGfx)a->i[0]);
		case kOhplH20Op_SKIN_ATTRIBUTE:
		{
			cVector3f vAttr = OhplH20Access::Skin(W(a->w))->GetAttribute((eGuiSkinAttribute)a->i[0]);
			a->rf[0] = vAttr.x;
			a->rf[1] = vAttr.y;
			a->rf[2] = vAttr.z;
			return 0;
		}
		case kOhplH20Op_GFX_ACTIVE_SIZE:
		{
			cVector2f vSize = ((cGuiGfxElement*)a->p[0])->GetActiveSize();
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_GFX_IMAGE_SIZE:
		{
			cVector2f vSize = ((cGuiGfxElement*)a->p[0])->GetImageSize();
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_CLIP_CREATE_CHILD:
			return (uintptr_t)((cGuiClipRegion*)a->p[0])->CreateChild(cVector3f(a->f[0], a->f[1], a->f[2]), cVector2f(a->f[3], a->f[4]));
		case kOhplH20Op_SET_CLIP:
			OhplH20Access::Set(W(a->w))->SetCurrentClipRegion((cGuiClipRegion*)a->p[0]);
			return 0;
		case kOhplH20Op_DRAW_TEXT:
		{
			tWString sText = a->text ? tWString(a->text, a->textLen) : tWString();
			cVector3f vPos(a->f[0], a->f[1], a->f[2]);
			iWidget* pWidget = W(a->w);
			if(a->i[2]==1)
			{
				cColor col = pWidget->IsEnabled() ? OhplH20Access::FontColor(pWidget) : OhplH20Access::Skin(pWidget)->GetFont(eGuiSkinFont_Disabled)->mColor;
				(pWidget->*OhplH20Access::TextCol())(sText, vPos, (eFontAlign)a->i[0], col);
			}
			else if(a->i[2]==2)
			{
				cColor col = a->i[3]==0 ? OhplH20Access::Skin(pWidget)->GetFont(eGuiSkinFont_Disabled)->mColor :
							 (a->i[3]==1 ? OhplH20Access::HighlightColor(pWidget) : OhplH20Access::FontColor(pWidget));
				(pWidget->*OhplH20Access::TextCol())(sText, vPos, (eFontAlign)a->i[0], col);
			}
			else if(a->i[1]) (pWidget->*OhplH20Access::Highlight())(sText, vPos, (eFontAlign)a->i[0]);
			else		(pWidget->*OhplH20Access::Text())(sText, vPos, (eFontAlign)a->i[0]);
			return 0;
		}
		case kOhplH20Op_GFX_RECT:
			return (uintptr_t)cGui::mpGfxRect;
		case kOhplH20Op_CREATE_LABEL:
		{
			iWidget* pParent = W(a->w);
			cWidgetLabel* pLabel = OhplH20Access::Set(pParent)->CreateWidgetLabel(cVector3f(a->f[0], a->f[1], a->f[2]), -1, tWString(a->text, a->textLen), pParent);
			pLabel->SetDefaultFontSize(OhplH20Access::FontSize(pParent));
			pLabel->SetBackGroundColor(cColor(0.82f,0.81f,0.79f,1));
			return (uintptr_t)(iWidget*)pLabel;
		}
		case kOhplH20Op_TEXT_LENGTH:
			a->rf[0] = OhplH20Access::Font(W(a->w))->GetLength(OhplH20Access::FontSize(W(a->w)), W(a->p[0])->GetText().c_str());
			return 0;
		case kOhplH20Op_ITEM_PROP_GET:
			return (uintptr_t)Item(a->p[0])->GetProperty(a->i[0]);
		case kOhplH20Op_PROP_TYPE:
			return ((cWidgetItemProperty*)a->p[0])->GetType();
		case kOhplH20Op_PROP_TEXT:
		{
			const tWString& sText = ((cWidgetItemProperty*)a->p[0])->GetText();
			a->ri[0] = (int32_t)sText.size();
			return (uintptr_t)sText.data();
		}
		case kOhplH20Op_PROP_GFX:
			return (uintptr_t)((cWidgetItemProperty*)a->p[0])->GetGfx();
		case kOhplH20Op_DESTROY_WIDGET:
			OhplH20Access::Set(W(a->w))->DestroyWidget(W(a->p[0]));
			return 0;
		case kOhplH20Op_WIDGET_SET_VISIBLE:
			W(a->w)->SetVisible(a->i[0]!=0);
			return 0;
		case kOhplH20Op_WIDGET_SET_ENABLED:
			W(a->w)->SetEnabled(a->i[0]!=0);
			return 0;
		case kOhplH20Op_WIDGET_IS_ENABLED:
			return W(a->w)->IsEnabled();
		case kOhplH20Op_WIDGET_IS_VISIBLE:
			return W(a->w)->IsVisible();
		case kOhplH20Op_WIDGET_GET_TEXT:
		{
			const tWString& sText = W(a->w)->GetText();
			a->ri[0] = (int32_t)sText.size();
			return (uintptr_t)sText.data();
		}
		case kOhplH20Op_WIDGET_SET_TEXT:
			W(a->w)->SetText(a->text ? tWString(a->text, a->textLen) : tWString());
			return 0;
		case kOhplH20Op_WIDGET_SET_FONT_SIZE:
			W(a->w)->SetDefaultFontSize(cVector2f(a->f[0], a->f[1]));
			return 0;
		case kOhplH20Op_WIDGET_GET_POSITION:
		{
			cVector3f vPos = W(a->w)->GetLocalPosition();
			a->rf[0] = vPos.x;
			a->rf[1] = vPos.y;
			a->rf[2] = vPos.z;
			return 0;
		}
		case kOhplH20Op_TEXTBOX_SET_CAN_EDIT:
			((cWidgetTextBox*)W(a->w))->SetCanEdit(a->i[0]!=0);
			return 0;
		case kOhplH20Op_TEXTBOX_GET_CAN_EDIT:
			return ((cWidgetTextBox*)W(a->w))->GetCanEdit();
		case kOhplH20Op_TEXTBOX_BG_SIZE:
		{
			cVector2f vSize = ((cWidgetTextBox*)W(a->w))->GetBackgroundSize();
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_CREATE_TEXTBOX:
			return (uintptr_t)(iWidget*)OhplH20Access::Set(W(a->w))->CreateWidgetTextBox(0, cVector2f(a->f[0], a->f[1]), _W(""), W(a->w));
		case kOhplH20Op_CREATE_BUTTON:
			return (uintptr_t)(iWidget*)OhplH20Access::Set(W(a->w))->CreateWidgetButton(0, 0, _W(""), W(a->w));
		case kOhplH20Op_BUTTON_SET_IMAGE:
			((cWidgetButton*)W(a->w))->SetImage((cGuiGfxElement*)a->p[0], false);
			return 0;
		case kOhplH20Op_CHECK_POINT_CLIP:
			return OhplH20Access::CheckClip(W(a->w), cVector2f(a->f[0], a->f[1]));
		case kOhplH20Op_SET_CALL:
		{
			cGuiSet* pSet = OhplH20Access::Set(W(a->w));
			switch(a->i[0])
			{
			case 0: pSet->PushAttentionWidget(); return 0;
			case 1: pSet->PushFocusedWidget(); return 0;
			case 2: pSet->PushDefaultFocusNavWidget(); return 0;
			case 3: pSet->SetAttentionWidget(W(a->p[0])); return 0;
			case 4: pSet->SetFocusedWidget(W(a->p[0])); return 0;
			case 5: pSet->PopAttentionWidget(); return 0;
			case 6: pSet->PopFocusedWidget(); return 0;
			case 7: pSet->PopDefaultFocusNavWidget(); return 0;
			case 8: a->rf[0] = pSet->GetPopUpZ(); return 0;
			case 9: a->rf[0] = pSet->GetVirtualSize().x; a->rf[1] = pSet->GetVirtualSize().y; return 0;
			case 10: pSet->DecContextMenuZ(); return 0;
			case 11: return (uintptr_t)pSet->GetAttentionWidget();
			case 12: return (uintptr_t)pSet->GetFocusedWidget();
			case 13: return (uintptr_t)pSet->GetBaseClipRegion();
			case 14: return (uintptr_t)pSet->GetCurrentClipRegion();
			case 15: pSet->SetAttentionWidget(W(a->p[0]), false); return 0;
			case 16: pSet->ShowContextMenu((cWidgetContextMenu*)W(a->p[0]), cVector3f(a->f[0], a->f[1], a->f[2])); return 0;
			case 17: return pSet->GetDrawMouse();
			}
			return 0;
		}
		case kOhplH20Op_STATE_PTR:
			return (uintptr_t)OhplH20Access::State(W(a->w), a->i[0]);
		case kOhplH20Op_CREATE_TAB:
			return (uintptr_t)(iWidget*)OhplH20Access::Set(W(a->w))->CreateWidgetTab(cVector3f(a->f[0], a->f[1], a->f[2]), cVector2f(a->f[3], a->f[4]), _W(""), W(a->w));
		case kOhplH20Op_CREATE_TAB_LABEL:
			return (uintptr_t)(iWidget*)OhplH20Access::Set(W(a->w))->CreateWidgetTabLabel(0, cVector2f(a->f[0], a->f[1]), W(a->w)->GetText(), NULL);
		case kOhplH20Op_CREATE_FRAME:
			return (uintptr_t)(iWidget*)OhplH20Access::Set(W(a->w))->CreateWidgetFrame(0, cVector2f(a->f[0], a->f[1]), false, NULL, a->i[0]!=0, a->i[1]!=0);
		case kOhplH20Op_WIDGET_SET_CLIP_ACTIVE:
			W(a->w)->SetClipActive(a->i[0]!=0);
			return 0;
		case kOhplH20Op_ATTACH_CHILD:
			if(a->i[0]) W(a->w)->AttachChild(W(a->p[0]));
			else		W(a->w)->iWidget::AttachChild(W(a->p[0]));
			return 0;
		case kOhplH20Op_REMOVE_CHILD:
			if(a->i[0]) W(a->w)->RemoveChild(W(a->p[0]));
			else		W(a->w)->iWidget::RemoveChild(W(a->p[0]));
			return 0;
		case kOhplH20Op_SKIN_FONT_SIZE:
		{
			cVector2f vSize = OhplH20Access::SkinFont(W(a->w))->mvSize;
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_TEXT_LENGTH_SKIN:
			a->rf[0] = OhplH20Access::Font(W(a->w))->GetLength(OhplH20Access::SkinFont(W(a->w))->mvSize, W(a->w)->GetText().c_str());
			return 0;
		case kOhplH20Op_CREATE_BUTTON_AT:
			return (uintptr_t)(iWidget*)OhplH20Access::Set(W(a->w))->CreateWidgetButton(cVector3f(a->f[0], a->f[1], a->f[2]), cVector2f(a->f[3], a->f[4]), _W(""), W(a->w));
		case kOhplH20Op_BUTTON_SETUP:
		{
			cWidgetButton* pButton = (cWidgetButton*)W(a->w);
			pButton->SetDefaultFontSize(cVector2f(a->f[0]));
			pButton->SetRepeatActive(true);
			pButton->SetRepeatFreq(a->f[1]);
			return 0;
		}
		case kOhplH20Op_FONT_RANGE:
		{
			iFontData* pFont = OhplH20Access::Font(W(a->w));
			a->ri[0] = pFont ? pFont->GetFirstChar() : 0;
			a->ri[1] = pFont ? pFont->GetLastChar() : 0;
			return 0;
		}
		case kOhplH20Op_GLYPH:
		{
			iFontData* pFont = OhplH20Access::Font(W(a->w));
			if(pFont==NULL) return 0;
			cGlyph* pGlyph = pFont->GetGlyph(a->i[0]);
			if(pGlyph==NULL) return 0;
			a->rf[0] = pGlyph->mfAdvance;
			return 1;
		}
		case kOhplH20Op_FONT_LENGTH:
		{
			iFontData* pFont = OhplH20Access::Font(W(a->w));
			if(pFont==NULL) { a->rf[0] = 0; return 0; }
			tWString sText = a->text ? tWString(a->text, a->textLen) : tWString();
			a->rf[0] = pFont->GetLength(OhplH20Access::FontSize(W(a->w)), sText.c_str());
			return 0;
		}
		case kOhplH20Op_CLIPBOARD:
		{
			static tWString sClipboard;
			sClipboard = cPlatform::LoadTextFromClipboard();
			a->ri[0] = (int32_t)sClipboard.size();
			return (uintptr_t)sClipboard.data();
		}
		case kOhplH20Op_COPY:
			cPlatform::CopyTextToClipboard(a->text ? tWString(a->text, a->textLen) : tWString());
			return 0;
		case kOhplH20Op_TO_FLOAT:
		{
			tWString sText = a->text ? tWString(a->text, a->textLen) : tWString();
			float fValue = cString::ToFloat(cString::To8Char(sText).c_str(), 0);
			a->rf[0] = cMath::RoundFloatToDecimals(fValue, a->i[0]);
			return 0;
		}
		case kOhplH20Op_FLOAT_TEXT:
		{
			static tWString sNumber;
			sNumber = cString::ToStringW(a->f[0], a->i[0], true);
			a->ri[0] = (int32_t)sNumber.size();
			return (uintptr_t)sNumber.data();
		}
		case kOhplH20Op_SET_POINTER_GFX:
			OhplH20Access::SetPointer(W(a->w), OhplH20Access::Skin(W(a->w))->GetGfx(a->i[0] ? eGuiSkinGfx_PointerText : eGuiSkinGfx_PointerNormal));
			return 0;
		case kOhplH20Op_GFX_ANIM_RESET:
			((cGuiGfxElement*)a->p[0])->SetAnimationTime(0);
			return 0;
		case kOhplH20Op_CREATE_POPUP_KEYBOARD:
			OhplH20Access::Set(W(a->w))->CreatePopUpUIKeyboard((cWidgetTextBox*)W(a->w));
			return 0;
		case kOhplH20Op_WIDGET_TYPE:
			return a->w ? (uintptr_t)W(a->w)->GetType() : (uintptr_t)-1;
		case kOhplH20Op_WIDGET_SET_CHILDREN_OFFSET:
			W(a->w)->SetChildrenOffset(cVector3f(a->f[0], a->f[1], a->f[2]));
			return 0;
		case kOhplH20Op_IS_CONNECTED:
			return a->i[0] ? W(a->w)->IsConnectedTo(W(a->p[0]), false) : W(a->w)->IsConnectedTo(W(a->p[0]));
		case kOhplH20Op_ITEM_TEXT_LENGTH:
			a->rf[0] = ((cWidgetMenuItem*)W(a->w))->GetTextLength();
			return 0;
		case kOhplH20Op_ITEM_IS_SEPARATOR:
			return ((cWidgetMenuItem*)W(a->w))->IsSeparator();
		case kOhplH20Op_WIDGET_SHORTCUT_TEXT:
		{
			const tWString& sText = ((cWidgetMenuItem*)W(a->w))->GetShortcutText();
			a->ri[0] = (int32_t)sText.size();
			return (uintptr_t)sText.data();
		}
		case kOhplH20Op_MENU_CALL:
			return OhplH20Access::MenuCall(a);
		default:
			return 0;
		}
	}

	const OhplH20Host gH20Host = { NULL, H20Op };

}
