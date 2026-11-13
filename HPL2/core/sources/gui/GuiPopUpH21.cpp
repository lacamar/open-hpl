// Host side of the Rust popups (OHPL_IMPL=rust): one command per C++ widget or cGuiSet call.

#include "gui/GuiPopUp.h"

#include <cstring>
#include "system/String.h"
#include "resources/Resources.h"
#include "graphics/FontData.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/WidgetBaseClasses.h"
#include "gui/WidgetButton.h"
#include "gui/WidgetComboBox.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetMultiPropertyListBox.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetTextBox.h"
#include "gui/WidgetWindow.h"
#include "gui/GuiPopUpMessageBox.h"

namespace hpl {

	static_assert(eGuiMessage_ButtonPressed == 11 && eGuiMessage_UIButtonPress == 19 && eGuiMessage_UIButtonRelease == 20 &&
				  eGuiMessage_SliderMove == 24 && eGuiMessage_SelectionChange == 26 && eGuiMessage_SelectionDoubleClick == 27 &&
				  eGuiMessage_TextBoxEnter == 28 && eGuiMessage_WindowClose == 31, "popup message numbers");
	static_assert(eUIArrow_Up == 1 && eUIArrow_Right == 2 && eUIArrow_Down == 4 && eUIArrow_Left == 8, "popup arrow numbers");
	static_assert(eKey_BackSpace == 0 && eKey_Return == 3 && eKey_Space == 6 && eKey_0 == 21 && eKey_A == 44 && eKey_None == 133, "popup key numbers");
	static_assert(eWidgetSliderOrientation_Horizontal == 0 && eWidgetTextBoxInputType_Normal == 0 && eWidgetTextBoxInputType_Numeric == 1, "popup widget enums");
	static_assert(eWidgetWindowButtonFlag_ButtonClose == 1, "popup window flag");

	static tWString H21Ws(const uint32_t *apStr, size_t alLen)
	{
		return apStr ? tWString(reinterpret_cast<const wchar_t*>(apStr), alLen) : tWString();
	}

	OhplH21Host iGuiPopUp::H21Host()
	{
		OhplH21Host host = { this, &iGuiPopUp::H21Exec };
		return host;
	}

	void iGuiPopUp::H21Bind(iWidget *apWidget, int alMsg, int alTag)
	{
		OhplH21Binding b = { this, alTag };
		mBindings.push_back(b);
		apWidget->AddCallback((eGuiMessage)alMsg, &mBindings.back(), &iGuiPopUp::H21Dispatch);
	}

	bool iGuiPopUp::H21Dispatch(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
	{
		OhplH21Binding *b = static_cast<OhplH21Binding*>(apObj);
		OhplH21Host host = b->popup->H21Host();
		return ohpl_h21_event(&host, b->popup->mR, b->tag, (uintptr_t)apWidget, &aData);
	}

	void iGuiPopUp::H21Str(OhplH21Ret &aRet, int alIdx, const tWString &asStr)
	{
		mH21Str[alIdx] = asStr;
		aRet.s[alIdx] = reinterpret_cast<const uint32_t*>(mH21Str[alIdx].data());
		aRet.n[alIdx] = mH21Str[alIdx].size();
	}

	void iGuiPopUp::H21Exec(void *apCtx, const OhplH21Cmd *apCmd, OhplH21Ret *apRet)
	{
		iGuiPopUp *self = static_cast<iGuiPopUp*>(apCtx);
		const OhplH21Cmd &c = *apCmd;
		OhplH21Ret &r = *apRet;
		cGuiSet *set = self->mpSet;
		iWidget *w = (iWidget*)c.w[0];
		iWidget *w2 = (iWidget*)c.w[1];
		cVector3f v3(c.f[0], c.f[1], c.f[2]);
		cVector2f v2(c.f[0], c.f[1]);
		cVector2f sz(c.f[3], c.f[4]);
		memset(&r, 0, sizeof(r));
		const bool bTrue = c.i[0] != 0;

		switch(c.op)
		{
		case kOhplH21VirtualSize: { cVector2f s = set->GetVirtualSize(); r.f[0] = s.x; r.f[1] = s.y; break; }
		case kOhplH21PopupZ: r.f[0] = set->GetPopUpZ(); break;
		case kOhplH21IncPopupZ: set->IncPopUpZ(); break;
		case kOhplH21PopupCount: set->mlPopupCount += c.i[0]; break;
		case kOhplH21GetAttention: r.w = (uintptr_t)set->GetAttentionWidget(); break;
		case kOhplH21GetFocused: r.w = (uintptr_t)set->GetFocusedWidget(); break;
		case kOhplH21GetDefaultFocus: r.w = (uintptr_t)set->GetDefaultFocusNavWidget(); break;
		case kOhplH21SetAttention: set->SetAttentionWidget(w); break;
		case kOhplH21SetFocused: set->SetFocusedWidget(w); break;
		case kOhplH21SetDefaultFocus: set->SetDefaultFocusNavWidget(w); break;
		case kOhplH21PushAttention: set->PushAttentionWidget(); break;
		case kOhplH21PushFocused: set->PushFocusedWidget(); break;
		case kOhplH21PushDefaultFocus: set->PushDefaultFocusNavWidget(); break;
		case kOhplH21PopAttention: set->PopAttentionWidget(); break;
		case kOhplH21PopFocused: set->PopFocusedWidget(); break;
		case kOhplH21PopDefaultFocus: set->PopDefaultFocusNavWidget(); break;
		case kOhplH21DestroyWidget: set->DestroyWidget(w, bTrue); break;
		case kOhplH21CreateWindow:
			r.w = (uintptr_t)set->CreateWidgetWindow(c.i[0], v3, sz, _W(""), NULL);
			break;
		case kOhplH21CreateButton:
			r.w = (uintptr_t)set->CreateWidgetButton(v3, sz, H21Ws(c.s[0], c.n[0]), w);
			break;
		case kOhplH21CreateLabel:
			r.w = (uintptr_t)set->CreateWidgetLabel(v3, sz, H21Ws(c.s[0], c.n[0]), w);
			break;
		case kOhplH21CreateSlider:
			r.w = (uintptr_t)set->CreateWidgetSlider((eWidgetSliderOrientation)c.i[0], v3, sz, c.i[1], w);
			break;
		case kOhplH21CreateTextbox:
			r.w = (uintptr_t)set->CreateWidgetTextBox(v3, sz, H21Ws(c.s[0], c.n[0]), w, (eWidgetTextBoxInputType)c.i[0], c.f[5]);
			break;
		case kOhplH21CreateFrame:
			r.w = (uintptr_t)set->CreateWidgetFrame(v3, sz, c.i[0] != 0, w);
			break;
		case kOhplH21CreateCombo:
			r.w = (uintptr_t)set->CreateWidgetComboBox(v3, sz, H21Ws(c.s[0], c.n[0]), w);
			break;
		case kOhplH21CreateList:
			r.w = (uintptr_t)set->CreateWidgetMultiPropertyListBox(v3, sz, w);
			break;
		case kOhplH21MessageBox:
		{
			void *obj = c.i[0] >= 0 ? (self->mBindings.push_back(OhplH21Binding{self, c.i[0]}), (void*)&self->mBindings.back()) : NULL;
			set->CreatePopUpMessageBox(H21Ws(c.s[0], c.n[0]), H21Ws(c.s[1], c.n[1]), H21Ws(c.s[2], c.n[2]), H21Ws(c.s[3], c.n[3]),
									   obj, c.i[0] >= 0 ? &iGuiPopUp::H21Dispatch : NULL);
			break;
		}
		case kOhplH21DestroyPopup: self->SelfDestruct(); break;
		case kOhplH21AddResourceDir: set->GetResources()->AddResourceDir(H21Ws(c.s[0], c.n[0]), false); break;
		case kOhplH21RunCallback:
		{
			tGuiCallbackFunc f = reinterpret_cast<tGuiCallbackFunc>(c.p[1]);
			void *obj = (void*)c.p[0];
			if(c.i[0] == 0) r.i = f(obj, w, cGuiMessageData());
			else r.i = f(obj, w, cGuiMessageData(c.i[1]));
			break;
		}
		case kOhplH21CallDestroyCb:
		{
			tGuiCallbackFunc f = reinterpret_cast<tGuiCallbackFunc>(c.p[1]);
			f((void*)c.p[0], NULL, cGuiMessageData());
			break;
		}
		case kOhplH21AddCallback: self->H21Bind(w, c.i[0], c.i[1]); break;
		case kOhplH21SetText: w->SetText(H21Ws(c.s[0], c.n[0])); break;
		case kOhplH21GetText: self->H21Str(r, 0, w->GetText()); break;
		case kOhplH21SetPosition: w->SetPosition(v3); break;
		case kOhplH21SetSize: w->SetSize(v2); break;
		case kOhplH21SetGlobalPosition: w->SetGlobalPosition(v3); break;
		case kOhplH21GetGlobalPosition: { cVector3f g = w->GetGlobalPosition(); r.f[0] = g.x; r.f[1] = g.y; r.f[2] = g.z; break; }
		case kOhplH21GetSize: { cVector2f s = w->GetSize(); r.f[0] = s.x; r.f[1] = s.y; break; }
		case kOhplH21SetEnabled: w->SetEnabled(bTrue); break;
		case kOhplH21SetVisible: w->SetVisible(bTrue); break;
		case kOhplH21SetFocusNav: w->SetFocusNavigation((eUIArrow)c.i[0], w2); break;
		case kOhplH21SetGlobalUiListener: w->SetGlobalUIInputListener(bTrue); break;
		case kOhplH21WinCloseDisables: static_cast<cWidgetWindow*>(w)->SetCloseButtonDisablesWindow(bTrue); break;
		case kOhplH21WinEscCloses: static_cast<cWidgetWindow*>(w)->SetEscapeKeyClosesWindow(bTrue); break;
		case kOhplH21WinGetEsc: r.i = static_cast<cWidgetWindow*>(w)->GetEscapeKeyClosesWindow(); break;
		case kOhplH21WinStatic: static_cast<cWidgetWindow*>(w)->SetStatic(bTrue); break;
		case kOhplH21WinDrawLabel: static_cast<cWidgetWindow*>(w)->SetDrawLabel(bTrue); break;
		case kOhplH21WinFontSize: { cVector2f s = static_cast<cWidgetWindow*>(w)->GetDefaultFontSize(); r.f[0] = s.x; r.f[1] = s.y; break; }
		case kOhplH21InsideBounds: set->PositionWidgetInsideBounds(w); break;
		case kOhplH21SliderGet: r.i = static_cast<cWidgetSlider*>(w)->GetValue(); break;
		case kOhplH21SliderSet: static_cast<cWidgetSlider*>(w)->SetValue(c.i[0], c.i[1] != 0); break;
		case kOhplH21TbGetNum: r.f[0] = static_cast<cWidgetTextBox*>(w)->GetNumericValue(); break;
		case kOhplH21TbSetNum: static_cast<cWidgetTextBox*>(w)->SetNumericValue(c.f[0]); break;
		case kOhplH21TbLower: static_cast<cWidgetTextBox*>(w)->SetLowerBound(bTrue, c.f[0]); break;
		case kOhplH21TbUpper: static_cast<cWidgetTextBox*>(w)->SetUpperBound(bTrue, c.f[0]); break;
		case kOhplH21TbDecimals: static_cast<cWidgetTextBox*>(w)->SetDecimals(c.i[0]); break;
		case kOhplH21TbForceEnter: static_cast<cWidgetTextBox*>(w)->SetForceCallBackOnEnter(bTrue); break;
		case kOhplH21TbLostFocus: static_cast<cWidgetTextBox*>(w)->SetCallbackOnLostFocus(bTrue); break;
		case kOhplH21TbSelectAll: static_cast<cWidgetTextBox*>(w)->SetSelectedText(); break;
		case kOhplH21TbUserData: r.w = (uintptr_t)static_cast<iWidget*>(w->GetUserData()); break;
		case kOhplH21TbKeySkip:
			r.i = static_cast<cWidgetTextBox*>(w)->ProcessMessage(eGuiMessage_KeyPress, cGuiMessageData(cKeyPress((eKey)c.i[0], c.i[1], c.i[2])), true, true);
			break;
		case kOhplH21TbKey:
			r.i = static_cast<cWidgetTextBox*>(w)->ProcessMessage(eGuiMessage_KeyPress, cGuiMessageData(cKeyPress((eKey)c.i[0], c.i[1], c.i[2])));
			break;
		case kOhplH21FrameColor: static_cast<cWidgetFrame*>(w)->SetBackGroundColor(cColor(c.f[0], c.f[1], c.f[2], c.f[3])); break;
		case kOhplH21FrameDrawBg: static_cast<cWidgetFrame*>(w)->SetDrawBackground(bTrue); break;
		case kOhplH21FrameBgZ: static_cast<cWidgetFrame*>(w)->SetBackgroundZ(c.f[0]); break;
		case kOhplH21CbCanEdit: static_cast<cWidgetComboBox*>(w)->SetCanEdit(bTrue); break;
		case kOhplH21CbItemNum: r.i = static_cast<cWidgetComboBox*>(w)->GetItemNum(); break;
		case kOhplH21CbSelected: r.i = static_cast<cWidgetComboBox*>(w)->GetSelectedItem(); break;
		case kOhplH21CbItemText: self->H21Str(r, 0, static_cast<cWidgetComboBox*>(w)->GetItemText(c.i[0])); break;
		case kOhplH21CbAddItem: static_cast<cWidgetComboBox*>(w)->AddItem(H21Ws(c.s[0], c.n[0])); break;
		case kOhplH21CbClear: static_cast<cWidgetComboBox*>(w)->ClearItems(); break;
		case kOhplH21CbSetSelected: static_cast<cWidgetComboBox*>(w)->SetSelectedItem(c.i[0]); break;
		case kOhplH21MlAddColumn:
		{
			tString sName;
			for(size_t i=0; i<c.n[0]; ++i) sName.push_back((char)c.s[0][i]);
			cWidgetMultiPropertyListBox *pList = static_cast<cWidgetMultiPropertyListBox*>(w);
			if(c.i[1] == 1) pList->AddColumn(sName, c.i[0], eFontAlign_Right);
			else pList->AddColumn(sName, c.i[0]);
			break;
		}
		case kOhplH21MlColWidth: static_cast<cWidgetMultiPropertyListBox*>(w)->SetColumnWidth(c.i[0], c.f[0]); break;
		case kOhplH21MlSelected: r.i = static_cast<cWidgetMultiPropertyListBox*>(w)->GetSelectedItem(); break;
		case kOhplH21MlProp1:
		{
			cWidgetItem *pItem = (cWidgetItem*)static_cast<cWidgetMultiPropertyListBox*>(w)->GetItem(c.i[0]);
			if(pItem == NULL) r.i = 0;
			else if(pItem->GetProperty(1) == NULL) r.i = 1;
			else
			{
				r.i = 2;
				self->H21Str(r, 0, pItem->GetProperty(1)->GetText());
			}
			break;
		}
		case kOhplH21MlClear: static_cast<cWidgetMultiPropertyListBox*>(w)->ClearItems(); break;
		case kOhplH21TextLen:
		{
			cGuiSkinFont *pFont = self->mpSkin->GetFont(eGuiSkinFont_Default);
			tWString sText = H21Ws(c.s[0], c.n[0]);
			r.f[0] = pFont->mpFont->GetLength(pFont->mvSize, sText.c_str());
			break;
		}
		case kOhplH21FontSize:
		{
			cGuiSkinFont *pFont = self->mpSkin->GetFont(eGuiSkinFont_Default);
			r.f[0] = pFont->mvSize.x;
			r.f[1] = pFont->mvSize.y;
			break;
		}
		case kOhplH21ColorRead:
		{
			cColor *pColor = (cColor*)c.p[0];
			r.f[0] = pColor->r; r.f[1] = pColor->g; r.f[2] = pColor->b; r.f[3] = pColor->a;
			break;
		}
		case kOhplH21ColorWrite: *(cColor*)c.p[0] = cColor(c.f[0], c.f[1], c.f[2], c.f[3]); break;
		default:
			self->H21Ext(c, r);
			break;
		}
	}

}
