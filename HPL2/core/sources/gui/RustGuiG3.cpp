#include "gui/RustGuiGw.h"

#include "graphics/FontData.h"
#include "gui/Gui.h"
#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/Widget.h"
#include "gui/WidgetButton.h"
#include "gui/WidgetCheckBox.h"
#include "gui/WidgetDummy.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetGroup.h"
#include "gui/WidgetImage.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetWindow.h"
#include "input/Input.h"
#include "input/Keyboard.h"

namespace hpl {

	extern "C" {
	typedef void (*OhplGwRowSink)(void*, const uint32_t*, size_t);
	typedef void (*OhplGwSink)(void*, const char*, size_t);
	struct OhplGwHost {
		uintptr_t (*shellNew)(int, void*, void*, int, int);
		void (*shellDelete)(uintptr_t);
		bool (*foreignCall)(uintptr_t, int, uintptr_t, const cGuiMessageData*, float);
		void (*drawFont)(void*, uintptr_t, const uint32_t*, size_t, const cVector2f*, const cVector3f*, const cColor*, int);
		float (*fontLength)(uintptr_t, const cVector2f*, const uint32_t*, size_t);
		void (*fontRows)(uintptr_t, float, float, const cVector2f*, const uint32_t*, size_t, OhplGwRowSink, void*);
		void (*keyName)(void*, bool, int, void*, OhplGwSink);
	};
	void ohpl_gw_set_host(OhplGwHost*);
	}

	// Virtuals of the widgets still written in C++, called by the Rust base (h19_widget::Op numbering).
	struct OhplGwForeign
	{
		static iWidget* Wd(uintptr_t w) { return (iWidget*)w; }

		static bool Call(uintptr_t w, int op, uintptr_t other, const cGuiMessageData* d, float f)
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
			case 16:						pW->AttachChild(Wd(other)); return false;
			case 18:						pW->OnInit(); return false;
			case 19:						pW->OnLoadGraphics(); return false;
			}
			return false;
		}
	};

	static uintptr_t ShellNew(int alKind, void *apSet, void *apSkin, int alA, int alB)
	{
		cGuiSet *pSet = (cGuiSet*)apSet;
		cGuiSkin *pSkin = (cGuiSkin*)apSkin;
		iWidget *pWidget = NULL;
		switch(alKind)
		{
		case 0: pWidget = hplNew( iWidget, (eWidgetType_Root, pSet, (cGuiSkin*)NULL) ); break;
		case 1: pWidget = hplNew( cWidgetFrame, (pSet, pSkin, alA != 0, alB != 0) ); break;
		case 2: pWidget = hplNew( cWidgetLabel, (pSet, pSkin) ); break;
		case 3: pWidget = hplNew( cWidgetButton, (pSet, pSkin) ); break;
		case 4: pWidget = hplNew( cWidgetSlider, (pSet, pSkin, (eWidgetSliderOrientation)alA) ); break;
		case 5: pWidget = hplNew( cWidgetWindow, (pSet, pSkin, (tWidgetWindowButtonFlag)alA) ); break;
		case 6: pWidget = hplNew( cWidgetCheckBox, (pSet, pSkin) ); break;
		case 7: pWidget = hplNew( cWidgetGroup, (pSet, pSkin) ); break;
		case 8: pWidget = hplNew( cWidgetImage, (pSet, pSkin) ); break;
		case 9: pWidget = hplNew( cWidgetDummy, (pSet, pSkin) ); break;
		}
		return (uintptr_t)pWidget;
	}

	static void ShellDelete(uintptr_t aW) { iWidget *p = (iWidget*)aW; hplDelete(p); }

	static void DrawFontOp(void *apSet, uintptr_t aFont, const uint32_t *apText, size_t alLen, const cVector2f *apSize,
						   const cVector3f *apPos, const cColor *apColor, int alAlign)
	{
		((cGuiSet*)apSet)->DrawFont(tWString((const wchar_t*)apText, alLen), (iFontData*)aFont, *apPos, *apSize, *apColor, (eFontAlign)alAlign);
	}

	static float FontLength(uintptr_t aFont, const cVector2f *apSize, const uint32_t *apText, size_t alLen)
	{
		return ((iFontData*)aFont)->GetLength(*apSize, tWString((const wchar_t*)apText, alLen).c_str());
	}

	static void FontRows(uintptr_t aFont, float afWidth, float afHeight, const cVector2f *apSize, const uint32_t *apText, size_t alLen,
						 OhplGwRowSink apSink, void *apCtx)
	{
		tWStringVec vRows;
		((iFontData*)aFont)->GetWordWrapRows(afWidth, afHeight, *apSize, tWString((const wchar_t*)apText, alLen), &vRows);
		for(size_t i=0; i<vRows.size(); ++i) apSink(apCtx, (const uint32_t*)vRows[i].data(), vRows[i].size());
	}

	static void KeyName(void *apInput, bool abModifier, int alKey, void *apCtx, OhplGwSink apSink)
	{
		iKeyboard *pKB = ((cInput*)apInput)->GetKeyboard();
		tString sName = abModifier ? pKB->ModifierKeyToString((eKeyModifier)alKey) : pKB->KeyToString((eKey)alKey);
		apSink(apCtx, sName.data(), sName.size());
	}

	static OhplGwHost gGwHost = { ShellNew, ShellDelete, &OhplGwForeign::Call, DrawFontOp, FontLength, FontRows, KeyName };

	void OhplGwRegisterHost() { ohpl_gw_set_host(&gGwHost); }

	//////////////////////////////////////////////////////////////////////////
	// iWidget text helpers for the widgets still written in C++
	//////////////////////////////////////////////////////////////////////////

	void iWidget::DrawSkinText(	const tWString& asText,eGuiSkinFont aFont,
								const cVector3f& avPosition,eFontAlign aAlign)
	{
		cGuiSkinFont *pFont = mpSkin->GetFont(aFont);
		mpSet->DrawFont(asText,pFont->mpFont,avPosition,pFont->mvSize,pFont->mColor*GetColorMul(),
						aAlign);
	}

	void iWidget::DrawDefaultText(	const tWString& asText,
									const cVector3f& avPosition,eFontAlign aAlign)
	{
		if(GetDefaultFontType()==NULL) return;
		mpSet->DrawFont(asText,GetDefaultFontType(),avPosition,GetDefaultFontSize(),
						GetDefaultFontColor()*GetColorMul(), aAlign);
	}

	void iWidget::DrawDefaultText(	const tWString& asText,
									const cVector3f& avPosition,eFontAlign aAlign, const cColor& aCol)
	{
		if(GetDefaultFontType()==NULL) return;
		mpSet->DrawFont(asText,GetDefaultFontType(),avPosition,GetDefaultFontSize(),
						aCol*GetColorMul(), aAlign);
	}

	void iWidget::DrawDefaultTextHighlight(	const tWString& asText,
											const cVector3f& avPosition,eFontAlign aAlign)
	{
		if(GetDefaultFontType()==NULL) return;
		mpSet->DrawFont(asText,GetDefaultFontType(),avPosition,GetDefaultFontSize(),
						GetDefaultFontHighlightColor()*GetColorMul(),aAlign);
	}
}
