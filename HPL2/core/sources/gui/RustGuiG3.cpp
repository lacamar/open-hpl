#include "gui/RustGuiGw.h"

#include "graphics/FontData.h"
#include "math/Math.h"
#include "system/Platform.h"
#include "system/String.h"
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
#include "gui/WidgetMenuItem.h"
#include "gui/WidgetContextMenu.h"
#include "gui/WidgetMainMenu.h"
#include "gui/WidgetTextBox.h"
#include "gui/WidgetListBox.h"
#include "gui/WidgetMultiPropertyListBox.h"
#include "gui/WidgetComboBox.h"
#include "gui/WidgetTabFrame.h"
#include "input/Input.h"
#include "input/Keyboard.h"
#include "resources/Resources.h"

namespace hpl {

	extern "C" {
	typedef void (*OhplGwRowSink)(void*, const uint32_t*, size_t);
	typedef void (*OhplGwSink)(void*, const char*, size_t);
	typedef void (*OhplGwWideSink)(void*, const uint32_t*, size_t);
	struct OhplGwHost {
		uintptr_t (*shellNew)(int, void*, void*, int, uintptr_t);
		void (*shellDelete)(uintptr_t);
		void (*drawFont)(void*, uintptr_t, const uint32_t*, size_t, const cVector2f*, const cVector3f*, const cColor*, int);
		float (*fontLength)(uintptr_t, const cVector2f*, const uint32_t*, size_t);
		void (*fontRows)(uintptr_t, float, float, const cVector2f*, const uint32_t*, size_t, OhplGwRowSink, void*);
		void (*fontRange)(uintptr_t, int*, int*);
		bool (*glyphAdvance)(uintptr_t, int, float*);
		void (*clipboardLoad)(void*, OhplGwWideSink);
		void (*clipboardSave)(const uint32_t*, size_t);
		float (*toFloat)(const uint32_t*, size_t, int);
		void (*floatText)(float, int, void*, OhplGwWideSink);
		void (*keyName)(void*, bool, int, void*, OhplGwSink);
	};
	void ohpl_gw_set_host(OhplGwHost*);
	}

	static uintptr_t ShellNew(int alKind, void *apSet, void *apSkin, int alA, uintptr_t alB)
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
		case 10: pWidget = hplNew( cWidgetMenuItem, ((iWidgetMenu*)alB) ); break;
		case 11: pWidget = hplNew( cWidgetContextMenu, (pSet, pSkin) ); break;
		case 12: pWidget = hplNew( cWidgetMainMenu, (pSet, pSkin) ); break;
		case 13: pWidget = hplNew( cWidgetTextBox, (pSet, pSkin, (eWidgetTextBoxInputType)alA) ); break;
		case 14: pWidget = hplNew( cWidgetListBox, (pSet, pSkin) ); break;
		case 15: pWidget = hplNew( cWidgetMultiPropertyListBox, (pSet, pSkin) ); break;
		case 16: pWidget = hplNew( cWidgetComboBox, (pSet, pSkin) ); break;
		case 17: pWidget = hplNew( cWidgetTabFrame, (pSet, pSkin) ); break;
		case 18: pWidget = hplNew( cWidgetTab, ((cWidgetTabFrame*)alB) ); break;
		case 19: pWidget = hplNew( cWidgetTabLabel, (pSet, pSkin) ); break;
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

	static void FontRange(uintptr_t aFont, int *apFirst, int *apLast)
	{
		iFontData *pFont = (iFontData*)aFont;
		*apFirst = pFont->GetFirstChar();
		*apLast = pFont->GetLastChar();
	}

	static bool GlyphAdvance(uintptr_t aFont, int alChar, float *apAdvance)
	{
		cGlyph *pGlyph = ((iFontData*)aFont)->GetGlyph(alChar);
		if(pGlyph==NULL) return false;
		*apAdvance = pGlyph->GetAdvance();
		return true;
	}

	static void ClipboardLoad(void *apCtx, OhplGwWideSink apSink)
	{
		tWString sText = cPlatform::LoadTextFromClipboard();
		apSink(apCtx, (const uint32_t*)sText.data(), sText.size());
	}

	static void ClipboardSave(const uint32_t *apText, size_t alLen) { cPlatform::CopyTextToClipboard(tWString((const wchar_t*)apText, alLen)); }

	static float ToFloat(const uint32_t *apText, size_t alLen, int alDecimals)
	{
		float fValue = cString::ToFloat(cString::To8Char(tWString((const wchar_t*)apText, alLen)).c_str(), 0);
		return cMath::RoundFloatToDecimals(fValue, alDecimals);
	}

	static void FloatText(float afValue, int alDecimals, void *apCtx, OhplGwWideSink apSink)
	{
		tWString sText = cString::ToStringW(afValue, alDecimals, true);
		apSink(apCtx, (const uint32_t*)sText.data(), sText.size());
	}

	static void KeyName(void *apInput, bool abModifier, int alKey, void *apCtx, OhplGwSink apSink)
	{
		iKeyboard *pKB = ((cInput*)apInput)->GetKeyboard();
		tString sName = abModifier ? pKB->ModifierKeyToString((eKeyModifier)alKey) : pKB->KeyToString((eKey)alKey);
		apSink(apCtx, sName.data(), sName.size());
	}

	static OhplGwHost gGwHost = { ShellNew, ShellDelete, DrawFontOp, FontLength, FontRows, FontRange, GlyphAdvance, ClipboardLoad, ClipboardSave, ToFloat, FloatText, KeyName };

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

	// Platform and resource calls of the Rust file browser and popups (rust/src/gui/h21_fb.rs).
	typedef void (*OhplFbSink)(void*, const uint32_t*, size_t);

	static tWString FbW(const uint32_t *apText, size_t alLen) { return tWString((const wchar_t*)apText, alLen); }
	static void FbSink(OhplFbSink apSink, void *apCtx, const tWString &asText)
	{
		apSink(apCtx, (const uint32_t*)asText.data(), asText.size());
	}

	struct OhplFbHost
	{
		bool (*exists)(int, const uint32_t*, size_t);
		void (*list)(int, const uint32_t*, size_t, const uint32_t*, size_t, bool, OhplFbSink, void*);
		void (*info)(const uint32_t*, size_t, uint64_t*, OhplFbSink, void*);
		void (*special)(int, OhplFbSink, void*);
		uint64_t (*drives)();
		void (*assign)(void*, int, const uint32_t*, size_t);
		void (*addResourceDir)(void*, const uint32_t*, size_t);
	};

	extern "C" void ohpl_g2fb_set_host(OhplFbHost*);

	static OhplFbHost gFbHost = {
		[](int alKind, const uint32_t *apText, size_t alLen) -> bool
		{
			return alKind ? cPlatform::FolderExists(FbW(apText, alLen)) : cPlatform::FileExists(FbW(apText, alLen));
		},
		[](int alKind, const uint32_t *apDir, size_t alDirLen, const uint32_t *apMask, size_t alMaskLen, bool abHidden, OhplFbSink apSink, void *apCtx)
		{
			tWStringList lstNames;
			if(alKind) cPlatform::FindFoldersInDir(lstNames, FbW(apDir, alDirLen), abHidden, true);
			else cPlatform::FindFilesInDir(lstNames, FbW(apDir, alDirLen), FbW(apMask, alMaskLen), abHidden);
			for(tWStringListIt it = lstNames.begin(); it != lstNames.end(); ++it) FbSink(apSink, apCtx, *it);
		},
		[](const uint32_t *apPath, size_t alLen, uint64_t *apSize, OhplFbSink apSink, void *apCtx)
		{
			tWString sPath = FbW(apPath, alLen);
			*apSize = cPlatform::GetFileSize(sPath);
			FbSink(apSink, apCtx, cString::To16Char(cPlatform::FileModifiedDate(sPath).ToString()));
		},
		[](int alKind, OhplFbSink apSink, void *apCtx)
		{
			FbSink(apSink, apCtx, alKind ? cPlatform::GetWorkingDir() : cPlatform::GetSystemSpecialPath(eSystemPath_Personal));
		},
		[]() -> uint64_t { return cPlatform::GetSystemAvailableDrives(); },
		[](void *apDest, int alKind, const uint32_t *apText, size_t alLen)
		{
			if(alKind == 0) { *(tWString*)apDest = FbW(apText, alLen); return; }
			tWStringVec *pVec = (tWStringVec*)apDest;
			pVec->clear();
			pVec->push_back(FbW(apText, alLen));
		},
		[](void *apResources, const uint32_t *apText, size_t alLen) { ((cResources*)apResources)->AddResourceDir(FbW(apText, alLen), false); },
	};

	static struct OhplFbRegister { OhplFbRegister() { ohpl_g2fb_set_host(&gFbHost); } } gFbRegister;
}
