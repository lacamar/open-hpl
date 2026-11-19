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
#include "gui/GuiPopUpFilePicker.h"
#include "system/Platform.h"

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

	// Reaches cWidgetTextBox's protected keyboard hooks.
	struct OhplG2PopupHost
	{
		static void KbOpen(iWidget *w) { ohpl_gw_tb_kb_open((uintptr_t)w); }
		static void KbClose(iWidget *w) { ohpl_gw_tb_kb_close((uintptr_t)w); }
	};

	// The file picker's iFileBrowser, owned by the Rust popup; its virtuals report back to Rust.
	struct OhplG2Browser : public iFileBrowser
	{
		OhplG2Browser(iGuiPopUp *apPopup, const tWString &asStartPath, bool abHidden)
			: iFileBrowser(asStartPath, abHidden), mpPopup(apPopup), mpSaveFileDest(NULL), mpLoadFileListDest(NULL), mpGfxUpButton(NULL)
		{
			for(int i=0; i<6; ++i) mvGfxFileTypeIcons[i] = NULL;
		}

		void OnAddFilter() { ohpl_g2_popup_on_add_filter(mpPopup); }
		void OnNavigate() { ohpl_g2_popup_on_navigate(mpPopup); }

		iGuiPopUp *mpPopup;
		tWString* mpSaveFileDest;
		tWStringVec* mpLoadFileListDest;
		cGuiGfxElement* mvGfxFileTypeIcons[6];
		cGuiGfxElement* mpGfxUpButton;
		std::vector<tWString> mvH21Files;
	};

	static OhplG2Browser* Fb(iGuiPopUp *apPopup) { return (OhplG2Browser*)ohpl_g2_popup_fb(apPopup); }

	static void H21Str(iGuiPopUp *self, OhplH21Ret &aRet, int alIdx, const tWString &asStr)
	{
		ohpl_g2_popup_str(self, alIdx, reinterpret_cast<const uint32_t*>(asStr.data()), asStr.size(), &aRet);
	}

	static void H21Exec(void *apCtx, const OhplH21Cmd *apCmd, OhplH21Ret *apRet)
	{
		iGuiPopUp *self = static_cast<iGuiPopUp*>(apCtx);
		OhplG2Browser *fb = Fb(self);
		const OhplH21Cmd &c = *apCmd;
		OhplH21Ret &r = *apRet;
		cGuiSet *set = ohpl_g2_popup_set(self);
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
		case kOhplH21PopupCount: set->AddPopUpCount(c.i[0]); break;
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
			void *obj = c.i[0] >= 0 ? ohpl_g2_popup_binding(self, c.i[0]) : NULL;
			set->CreatePopUpMessageBox(H21Ws(c.s[0], c.n[0]), H21Ws(c.s[1], c.n[1]), H21Ws(c.s[2], c.n[2]), H21Ws(c.s[3], c.n[3]),
									   obj, c.i[0] >= 0 ? reinterpret_cast<tGuiCallbackFunc>(&ohpl_g2_popup_dispatch) : NULL);
			break;
		}
		case kOhplH21DestroyPopup: ohpl_g2_popup_self_destruct(self); break;
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
		case kOhplH21AddCallback: w->AddCallback((eGuiMessage)c.i[0], ohpl_g2_popup_binding(self, c.i[1]), reinterpret_cast<tGuiCallbackFunc>(&ohpl_g2_popup_dispatch)); break;
		case kOhplH21SetText: w->SetText(H21Ws(c.s[0], c.n[0])); break;
		case kOhplH21GetText: H21Str(self, r, 0, w->GetText()); break;
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
		case kOhplH21CbItemText: H21Str(self, r, 0, static_cast<cWidgetComboBox*>(w)->GetItemText(c.i[0])); break;
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
				H21Str(self, r, 0, pItem->GetProperty(1)->GetText());
			}
			break;
		}
		case kOhplH21MlClear: static_cast<cWidgetMultiPropertyListBox*>(w)->ClearItems(); break;
		case kOhplH21TextLen:
		{
			cGuiSkinFont *pFont = ohpl_g2_popup_skin(self)->GetFont(eGuiSkinFont_Default);
			tWString sText = H21Ws(c.s[0], c.n[0]);
			r.f[0] = pFont->mpFont->GetLength(pFont->mvSize, sText.c_str());
			break;
		}
		case kOhplH21FontSize:
		{
			cGuiSkinFont *pFont = ohpl_g2_popup_skin(self)->GetFont(eGuiSkinFont_Default);
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
		case kOhplH21TbKbOpen: OhplG2PopupHost::KbOpen(w); break;
		case kOhplH21TbKbClose: OhplG2PopupHost::KbClose(w); break;
		case kOhplH21FpLoadGfx:
		{
			cGuiSkin *pSkin = ohpl_g2_popup_skin(self);
			fb->mpGfxUpButton = pSkin->GetGfx(eGuiSkinGfx_FilePickerUpButton);
			for(int i=0; i<6; ++i)
				fb->mvGfxFileTypeIcons[i] = pSkin->GetGfx((eGuiSkinGfx) (i + eGuiSkinGfx_FilePickerUpButton+1));
			break;
		}
		case kOhplH21BtnUpImage:
			if(fb->mpGfxUpButton) static_cast<cWidgetButton*>(w)->SetImage(fb->mpGfxUpButton, false);
			break;
		case kOhplH21MlAddRow:
		{
			cWidgetMultiPropertyListBox *pList = static_cast<cWidgetMultiPropertyListBox*>(w);
			cWidgetItem *pItem = pList->AddItem();
			if(c.i[0] == 0)
			{
				pItem->AddProperty((const cGuiGfxElement*)NULL);
				pItem->AddProperty(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0]));
				pItem->AddProperty(_W(""));
				pItem->AddProperty(_W(""));
			}
			else
			{
				pItem->AddProperty(fb->mvGfxFileTypeIcons[c.i[1]]);
				for(int i=0; i<3; ++i) pItem->AddProperty(tWString(reinterpret_cast<const wchar_t*>(c.s[i]), c.n[i]));
			}
			break;
		}
		case kOhplH21FbNavUp: fb->NavigateUp(); break;
		case kOhplH21FbNavTo: r.i = fb->NavigateTo(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0])); break;
		case kOhplH21FbFullPath: H21Str(self, r, 0, fb->GetCurrentFullPath()); break;
		case kOhplH21FbPathCount: r.i = (int)fb->mvCurrentDirFullPath.size(); break;
		case kOhplH21FbPathPop:
			for(int i=0; i<c.i[0] && !fb->mvCurrentDirFullPath.empty(); ++i) fb->mvCurrentDirFullPath.pop_back();
			break;
		case kOhplH21FbPathClear: fb->mvCurrentDirFullPath.clear(); break;
		case kOhplH21FbPathEntry: H21Str(self, r, 0, fb->mvCurrentDirFullPath[c.i[0]]); break;
		case kOhplH21FbDirs: r.i = (iFileBrowser::msPersonalDir.length()!=0 ? 1 : 0) | (iFileBrowser::msGameDir.length()!=0 ? 2 : 0); break;
		case kOhplH21FbCatCount: r.i = (int)fb->mvCategories.size(); break;
		case kOhplH21FbCatName: H21Str(self, r, 0, fb->mvCategories[c.i[0]]->msName); break;
		case kOhplH21FbCatFilterCount: r.i = (int)fb->mvCategories[c.i[0]]->mvFilters.size(); break;
		case kOhplH21FbCatFilter: H21Str(self, r, 0, fb->mvCategories[c.i[0]]->mvFilters[c.i[1]]); break;
		case kOhplH21FbInit: fb->InitBrowser(); break;
		case kOhplH21FbRootCount: r.i = (int)fb->mvSystemRootFolders.size(); break;
		case kOhplH21FbRoot: H21Str(self, r, 0, fb->mvSystemRootFolders[c.i[0]]); break;
		case kOhplH21FbList:
		{
			tWStringList lst;
			fb->GetFilesAndFoldersInCurrentPath(c.i[0], lst);
			fb->mvH21Files.assign(lst.begin(), lst.end());
			r.i = (int)fb->mvH21Files.size();
			break;
		}
		case kOhplH21FbRow:
		{
			tWString sFilename = fb->mvH21Files[c.i[0]];
			tWString sFull = fb->GetCurrentFullPath();
			r.i = (int)fb->GetFileTypeByName(sFilename);
			H21Str(self, r, 0, sFilename);
			H21Str(self, r, 1, fb->GetHumanReadableSize(cPlatform::GetFileSize(sFull + sFilename)));
			H21Str(self, r, 2, cString::To16Char(cPlatform::FileModifiedDate(sFull + sFilename).ToString()));
			break;
		}
		case kOhplH21FileExists: r.i = cPlatform::FileExists(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0])); break;
		case kOhplH21FolderExists: r.i = cPlatform::FolderExists(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0])); break;
		case kOhplH21SaveDest:
			if(fb && fb->mpSaveFileDest) *fb->mpSaveFileDest = tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0]);
			break;
		case kOhplH21LoadDest:
			if(fb && fb->mpLoadFileListDest)
			{
				fb->mpLoadFileListDest->clear();
				fb->mpLoadFileListDest->push_back(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0]));
			}
			break;
		default:
			break;
		}
	}

	static OhplG2PopupOps gG2PopupOps = {
		H21Exec,
		[](iGuiPopUp *apPopup, const uint32_t *apPath, size_t alLen, bool abHidden) -> uintptr_t
		{
			return (uintptr_t)hplNew( OhplG2Browser, (apPopup, tWString(reinterpret_cast<const wchar_t*>(apPath), alLen), abHidden) );
		},
		[](uintptr_t aFb) { OhplG2Browser *pFb = (OhplG2Browser*)aFb; hplDelete(pFb); },
		[](uintptr_t aFb, void *apDest) { ((OhplG2Browser*)aFb)->mpSaveFileDest = (tWString*)apDest; },
		[](uintptr_t aFb, void *apDest) { ((OhplG2Browser*)aFb)->mpLoadFileListDest = (tWStringVec*)apDest; },
		[](uintptr_t aFb, const uint32_t *apCat, size_t alCat, const uint32_t *apFilter, size_t alFilter) -> int
		{
			return ((OhplG2Browser*)aFb)->AddCategory(tWString(reinterpret_cast<const wchar_t*>(apCat), alCat), tWString(reinterpret_cast<const wchar_t*>(apFilter), alFilter));
		},
		[](uintptr_t aFb, int alCat, const uint32_t *apFilter, size_t alFilter)
		{
			((OhplG2Browser*)aFb)->AddFilter(alCat, tWString(reinterpret_cast<const wchar_t*>(apFilter), alFilter));
		},
	};

	void OhplG2RegisterPopupOps() { ohpl_g2_popup_ops(&gG2PopupOps); }

}
