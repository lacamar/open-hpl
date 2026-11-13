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

#include "gui/WidgetTextBox.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "graphics/FontData.h"

#include "system/String.h"
#include "system/Platform.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"

#include "gui/WidgetButton.h"
#include "impl/RustBackends.h"

#include <cstring>
#include <vector>

namespace hpl {

	int cWidgetTextBox::mlDefaultDecimals = 3;

	//////////////////////////////////////////////////////////////////////////
	// RUST GLUE
	//////////////////////////////////////////////////////////////////////////

	struct OhplW2TbEvent
	{
		int mlKind;
		tWString msText;
		tWString msNested;
		bool mbNested;
	};

	struct OhplW2TbCtx
	{
		cWidgetTextBox* mpWidget;
		iFontData* mpFont;
		cVector2f mvFontSize;
		cVector2f mvMouse;
		std::vector<OhplW2TbEvent> mvEvents;
		tWString msClipboard;
		tWString msNumber;
		OhplW2TextHost mHost;

		static float LocalX(void* apCtx)
		{
			OhplW2TbCtx* pCtx = (OhplW2TbCtx*)apCtx;
			return pCtx->mpWidget->WorldToLocalPosition(pCtx->mvMouse).x;
		}
	};

	static bool TbGlyph(void* apCtx, int alNum, float* apAdvance)
	{
		OhplW2TbCtx* pCtx = (OhplW2TbCtx*)apCtx;
		if(pCtx->mpFont==NULL) return false;
		cGlyph* pGlyph = pCtx->mpFont->GetGlyph(alNum);
		if(pGlyph==NULL) return false;
		*apAdvance = pGlyph->mfAdvance;
		return true;
	}

	static float TbLength(void* apCtx, const wchar_t* apText, size_t alLen)
	{
		OhplW2TbCtx* pCtx = (OhplW2TbCtx*)apCtx;
		if(pCtx->mpFont==NULL) return 0;
		tWString sText(apText, alLen);
		return pCtx->mpFont->GetLength(pCtx->mvFontSize, sText.c_str());
	}

	static const wchar_t* TbClipboard(void* apCtx, size_t* apLen)
	{
		OhplW2TbCtx* pCtx = (OhplW2TbCtx*)apCtx;
		pCtx->msClipboard = cPlatform::LoadTextFromClipboard();
		*apLen = pCtx->msClipboard.size();
		return pCtx->msClipboard.data();
	}

	static void TbCopy(void* apCtx, const wchar_t* apText, size_t alLen)
	{
		cPlatform::CopyTextToClipboard(tWString(apText, alLen));
	}

	static float TbToFloat(void* apCtx, const wchar_t* apText, size_t alLen, int alDecimals)
	{
		tWString sText(apText, alLen);
		float fValue = cString::ToFloat(cString::To8Char(sText).c_str(), 0);
		return cMath::RoundFloatToDecimals(fValue, alDecimals);
	}

	static const wchar_t* TbFloatText(void* apCtx, float afValue, int alDecimals, size_t* apLen)
	{
		OhplW2TbCtx* pCtx = (OhplW2TbCtx*)apCtx;
		pCtx->msNumber = cString::ToStringW(afValue, alDecimals, true);
		*apLen = pCtx->msNumber.size();
		return pCtx->msNumber.data();
	}

	static void TbEmit(void* apCtx, int alKind, const wchar_t* apA, size_t alALen, const wchar_t* apB, size_t alBLen, bool abHasB)
	{
		OhplW2TbCtx* pCtx = (OhplW2TbCtx*)apCtx;
		OhplW2TbEvent ev;
		ev.mlKind = alKind;
		if(apA) ev.msText.assign(apA, alALen);
		if(abHasB && apB) ev.msNested.assign(apB, alBLen);
		ev.mbNested = abHasB;
		pCtx->mvEvents.push_back(ev);
	}

	void cWidgetTextBox::SetupCall(OhplW2TbCtx& aCtx, OhplW2TextCall& aCall)
	{
		aCtx.mpWidget = this;
		aCtx.mpFont = mpDefaultFontType;
		aCtx.mvFontSize = mvDefaultFontSize;
		aCtx.mvMouse = cVector2f(0);
		aCtx.mHost.ctx = &aCtx;
		aCtx.mHost.glyph = TbGlyph;
		aCtx.mHost.local_x = OhplW2TbCtx::LocalX;
		aCtx.mHost.length = TbLength;
		aCtx.mHost.clipboard = TbClipboard;
		aCtx.mHost.copy = TbCopy;
		aCtx.mHost.to_float = TbToFloat;
		aCtx.mHost.float_text = TbFloatText;
		aCtx.mHost.emit = TbEmit;

		aCall.st = &mState;
		aCall.env.font_first = mpDefaultFontType ? mpDefaultFontType->GetFirstChar() : 0;
		aCall.env.font_last = mpDefaultFontType ? mpDefaultFontType->GetLastChar() : 0;
		aCall.env.font_size = mvDefaultFontSize;
		aCall.env.corner0 = mvGfxCorners[0]->GetActiveSize();
		aCall.env.default_decimals = mlDefaultDecimals;
		aCall.host = &aCtx.mHost;
		aCall.text = msText.data();
		aCall.text_len = msText.size();
		aCall.illegal = msIllegalChars.data();
		aCall.illegal_len = msIllegalChars.size();
	}

	// Replays the iWidget::SetText side effects Rust computed; true is the TextBoxEnter result.
	bool cWidgetTextBox::ApplyEvents(OhplW2TbCtx& aCtx, const cGuiMessageData& aData)
	{
		bool bResult = true;
		for(size_t i=0; i<aCtx.mvEvents.size(); ++i)
		{
			OhplW2TbEvent& ev = aCtx.mvEvents[i];
			if(ev.mlKind==kOhplW2TbEmitEnter)
			{
				bResult = ProcessMessage(eGuiMessage_TextBoxEnter, aData);
				continue;
			}

			mbReplaying = true;
			mpReplayEvent = &ev;
			SetText(ev.msText);
			mpReplayEvent = NULL;
			mbReplaying = false;
		}
		aCtx.mvEvents.clear();
		return bResult;
	}

	static bool TbReturn(int alRet, bool abEnterResult)
	{
		if(alRet==kOhplW2RetEnter) return abEnterResult;
		return alRet==kOhplW2RetTrue;
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetTextBox::cWidgetTextBox(cGuiSet *apSet, cGuiSkin *apSkin, eWidgetTextBoxInputType aType) : iWidget(eWidgetType_TextBox,apSet, apSkin),
		mpPrevAttention(mH.prevAttention), mState(mH.st), mbShowButtons(mH.showButtons)
	{
		ohpl_h20_tb_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, &mB, aType==eWidgetTextBoxInputType_Numeric);
		mvButtons[0] = NULL;
		mvButtons[1] = NULL;
		mInputType = aType;
		mbShowButtons = false;

		mbReplaying = false;
		mpReplayEvent = NULL;
		memset(&mState, 0, sizeof(mState));
		LoadGraphics();

		mpPointerGfx = mpSkin->GetGfx(eGuiSkinGfx_PointerText);
		mpPrevAttention = NULL;

		mState = ohpl_w2_tb_init(aType==eWidgetTextBoxInputType_Numeric);

		msIllegalChars = _W("");

		mbUIKeyboardOpen = false;
	}

	//-----------------------------------------------------------------------

	cWidgetTextBox::~cWidgetTextBox()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			if(OhplBackendRustRequested()) ohpl_h20_tb_destroy_buttons(&gH20Host, &mH);
			else
			{
				for(int i=0;i<2;++i)
					if(mvButtons[i]) mpSet->DestroyWidget(mvButtons[i]);
			}
		}
		ohpl_h20_tb_free(&mH);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetDefaultFontSize(const cVector2f& avSize)
	{
		iWidget::SetDefaultFontSize(avSize);
		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetMaxTextLength(int alLength)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_set_max_text_length(&gH20Host, &mH, alLength);
			return;
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ohpl_w2_tb_set_max_len(&call, alLength);
		ApplyEvents(ctx, cGuiMessageData());
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetSelectedText(int alStart, int alCount)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_set_selected_text(&gH20Host, &mH, alStart, alCount);
			return;
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ohpl_w2_tb_set_selected(&call, alStart, alCount);
		ApplyEvents(ctx, cGuiMessageData());
	}

	//-----------------------------------------------------------------------

	cVector2f cWidgetTextBox::GetBackgroundSize()
	{
		if(OhplBackendRustRequested())
		{
			cVector2f vSize;
			ohpl_h20_tb_background_size(&gH20Host, &mH, &vSize);
			return vSize;
		}

		return cVector2f(mvSize.x - mvGfxCorners[0]->GetActiveSize().x -
										mvGfxCorners[1]->GetActiveSize().x,
						mvSize.y - mvGfxCorners[0]->GetActiveSize().y -
									mvGfxCorners[2]->GetActiveSize().y);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetMaxTextSizeNeg(float afX)
	{
		mState.max_size_neg = afX;

		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetCanEdit(bool abX)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_set_can_edit(&gH20Host, &mH, abX);
			return;
		}

		mState.can_edit = abX;

		if(mState.can_edit)	mpPointerGfx = mpSkin->GetGfx(eGuiSkinGfx_PointerText);
		else				mpPointerGfx = mpSkin->GetGfx(eGuiSkinGfx_PointerNormal);

		for(int i=0;i<2;++i)
		{
			cWidgetButton* pButton = mvButtons[i];
			if(pButton)
				pButton->SetEnabled(mState.can_edit);
		}

	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetNumericValue(float afX)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_set_numeric_value(&gH20Host, &mH, afX);
			return;
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ohpl_w2_tb_set_numeric(&call, afX);
		ApplyEvents(ctx, cGuiMessageData());
	}

	//-----------------------------------------------------------------------

	float cWidgetTextBox::GetNumericValue()
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_numeric_value(&gH20Host, &mH);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		float fValue = ohpl_w2_tb_get_numeric(&call);
		ApplyEvents(ctx, cGuiMessageData());
		return fValue;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetLowerBound(bool abX, float afValue)
	{
		mState.has_lower = abX;
		mState.lower = afValue;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetUpperBound(bool abX, float afValue)
	{
		mState.has_upper = abX;
		mState.upper = afValue;
	}

	//-----------------------------------------------------------------------

	int cWidgetTextBox::GetDecimals()
	{
		if(mState.decimals<0)
			return mlDefaultDecimals;

		return mState.decimals;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetIllegalChars(const tWString &asIllegalChars)
	{
		if(OhplBackendRustRequested())
		{
			msIllegalChars = asIllegalChars;
			ohpl_h20_tb_set_illegal_chars(&gH20Host, &mH, asIllegalChars.data(), asIllegalChars.size());
			return;
		}

		msIllegalChars = asIllegalChars;
	}

	const tWString& cWidgetTextBox::GetIllegalChars()
	{
		return msIllegalChars;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::SetLegalCharCodeLimitEnabled(bool abX)
	{
		mState.limit_enabled = abX;
	}

	void cWidgetTextBox::SetLegalCharCodeMinLimit(wchar_t alX)
	{
		mState.limit_min = alX;
	}

	void cWidgetTextBox::SetLegalCharCodeMaxLimit(wchar_t alX)
	{
		mState.limit_max = alX;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::Widget_OnValueUp(iWidget* apWidget, const cGuiMessageData& aData)
	{
		ButtonPressed(mvButtons[0], cGuiMessageData());
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetTextBox, Widget_OnValueUp);

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::Widget_OnValueDown(iWidget* apWidget, const cGuiMessageData& aData)
	{
		ButtonPressed(mvButtons[1], cGuiMessageData());
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetTextBox, Widget_OnValueDown);

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::ButtonPressed(iWidget* apWidget, const cGuiMessageData& aData)
	{
		int lWhich = -1;
		if(apWidget==mvButtons[0]) lWhich = 0;
		else if(apWidget==mvButtons[1]) lWhich = 1;

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		int lRet = ohpl_w2_tb_button_pressed(&call, lWhich, IsEnabled());
		bool bEnter = ApplyEvents(ctx, aData);
		return TbReturn(lRet, bEnter);
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetTextBox, ButtonPressed);

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnChangeSize()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_on_change_size(&gH20Host, &mH);
			return;
		}

		cVector2f vBorders[3] = {mvGfxBorders[0]->GetActiveSize(), mvGfxBorders[1]->GetActiveSize(),
								 mvGfxBorders[2]->GetActiveSize()};

		float fButtonWidth = 0;
		if(mInputType==eWidgetTextBoxInputType_Numeric && mvButtons[0])
			fButtonWidth = mvButtons[0]->GetSize().x;

		float fHeight, fTextMaxSize;
		ohpl_w2_tb_layout(&mState, mvSize.x, vBorders, mvDefaultFontSize.y, fButtonWidth, &fHeight, &fTextMaxSize);
		mvSize.y = fHeight;
		mState.text_max_size = fTextMaxSize;

		OnChangeText();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnChangeText()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_on_change_text(&gH20Host, &mH);
			return;
		}

		if(mbReplaying)
		{
			// Rust already ran OnChangeText; only the nested truncating SetText is left to issue
			const OhplW2TbEvent* pEvent = mpReplayEvent;
			mpReplayEvent = NULL;
			if(pEvent && pEvent->mbNested)
				SetText(pEvent->msNested);
			mbReplaying = false;
			return;
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ohpl_w2_tb_change_text(&call, mbTextChanged);
		ApplyEvents(ctx, cGuiMessageData());
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnInit()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_on_init(&gH20Host, &mH);
			return;
		}

		if(mInputType!=eWidgetTextBoxInputType_Numeric)
			return;

		if(mbShowButtons==false)
			return;

		AddCallback(eGuiMessage_TextBoxValueUp, this, kGuiCallback(Widget_OnValueUp));
		AddCallback(eGuiMessage_TextBoxValueDown, this, kGuiCallback(Widget_OnValueDown));

        for(int i=0;i<2;++i)
		{
			mvButtons[i] = mpSet->CreateWidgetButton(cVector3f(mvSize.x-10,1 + i*10.0f,0.1f), cVector2f(10), _W(""), this);
			mvButtons[i]->SetDefaultFontSize(12);
			mvButtons[i]->SetRepeatActive(true);
			mvButtons[i]->SetRepeatFreq(3);
			mvButtons[i]->AddCallback(eGuiMessage_ButtonPressed,this,kGuiCallback(ButtonPressed));
		}

		mvButtons[0]->SetText(_W("+"));
		mvButtons[1]->SetText(_W("-"));

		// Init numeric value + text
		mState.numeric_value = -1;
		SetNumericValue(0);
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnLoadGraphics()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_on_load_graphics(&gH20Host, &mH);
			return;
		}

		mpGfxMarker = mpSkin->GetGfx(eGuiSkinGfx_TextBoxMarker);
		mpGfxSelectedTextBack = mpSkin->GetGfx(eGuiSkinGfx_TextBoxSelectedTextBack);

		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_TextBoxBackground);

		mvGfxBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderRight);
		mvGfxBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderLeft);
		mvGfxBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderUp);
		mvGfxBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderDown);

		mvGfxCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerLU);
		mvGfxCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerRU);
		mvGfxCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerRD);
		mvGfxCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerLD);

		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_tb_on_draw(&gH20Host, &mH);
			return;
		}

		cColor col = IsEnabled()? mDefaultFontColor : mpSkin->GetFont(eGuiSkinFont_Disabled)->mColor;

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		cVector3f vGlobal = GetGlobalPosition();
		OhplW2TextDraw d;
		ohpl_w2_tb_draw(&call, &vGlobal, &d);

		////////////////////////////////
		// Text
		DrawDefaultText(cString::SubW(msText,d.text_start,d.text_len),
						d.text_pos,
						eFontAlign_Left,col);

		//Marker
		if(d.has_marker)
		{
			mpSet->DrawGfx(	mpGfxMarker, d.marker_pos, d.marker_size);

			//Selected text
			if(d.has_selection)
			{
				mpSet->DrawGfx( mpGfxSelectedTextBack, d.sel_pos, d.sel_size);

				DrawDefaultTextHighlight(cString::SubW(msText,d.hl_start,d.hl_len),
								d.hl_pos,
								eFontAlign_Left);
			}
		}


		////////////////////////////////
		// Background and Borders
		DrawBordersAndCorners(	mpGfxBackground, mvGfxBorders, mvGfxCorners,
								GetGlobalPosition(), mvSize);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseMove(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_mouse_move(&gH20Host, &mH, &aData);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ctx.mvMouse = aData.mvPos;
		ohpl_w2_tb_mouse_move(&call);
		ApplyEvents(ctx, aData);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseDown(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_mouse_down(&gH20Host, &mH, &aData);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ctx.mvMouse = aData.mvPos;
		bool bAttention = false;
		int lRet = ohpl_w2_tb_mouse_down(&call, aData.mlVal, &bAttention);
		ApplyEvents(ctx, aData);
		if(lRet==kOhplW2RetFalse) return false;

		if(bAttention)
		{
			mpPrevAttention = mpSet->GetAttentionWidget();
			mpSet->SetAttentionWidget(this);
		}

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseUp(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_mouse_up(&gH20Host, &mH, &aData);
		}

		int lMarker, lSelectEnd;
		{
			OhplW2TbCtx ctx;
			OhplW2TextCall call;
			SetupCall(ctx, call);
			ctx.mvMouse = aData.mvPos;
			bool bRestore = ohpl_w2_tb_mouse_up(&call, aData.mlVal, &lMarker, &lSelectEnd);
			ApplyEvents(ctx, aData);
			if(bRestore==false) return true;
		}

		// SetAttention steals focus, so restore vars after it
		mpSet->SetAttentionWidget(mpPrevAttention, false);

		{
			OhplW2TbCtx ctx;
			OhplW2TextCall call;
			SetupCall(ctx, call);
			ohpl_w2_tb_mouse_up_restore(&call, lMarker, lSelectEnd);
			ApplyEvents(ctx, aData);
		}

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseDoubleClick(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_mouse_double_click(&gH20Host, &mH, &aData);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ctx.mvMouse = aData.mvPos;
		ohpl_w2_tb_double_click(&call, aData.mlVal);
		ApplyEvents(ctx, aData);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseEnter(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnMouseLeave(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnGotFocus(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_got_focus(&gH20Host, &mH);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ohpl_w2_tb_got_focus(&call);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnGotTabFocus(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_got_tab_focus(&gH20Host, &mH);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ohpl_w2_tb_got_tab_focus(&call);

		return true;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnLostFocus(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_lost_focus(&gH20Host, &mH, &aData);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ohpl_w2_tb_lost_focus(&call);
		ApplyEvents(ctx, aData);

		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnKeyPress(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_key_press(&gH20Host, &mH, &aData);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		bool bAnim = false;
		int lRet = ohpl_w2_tb_key(&call, aData.mKeyPress.mKey, aData.mKeyPress.mlModifier, aData.mKeyPress.mlUnicode, &bAnim);
		if(bAnim && mpGfxMarker) mpGfxMarker->SetAnimationTime(0);
		bool bEnter = ApplyEvents(ctx, aData);

		return TbReturn(lRet, bEnter);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnUIButtonPress(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_ui_button_press(&gH20Host, &mH, &aData);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		bool bKeyboard = ohpl_w2_tb_ui_button(&call, HasFocus(), aData.mlVal);
		ApplyEvents(ctx, aData);
		if(bKeyboard)
		{
			mpSet->CreatePopUpUIKeyboard(this);
			return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextBox::OnEnter(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_tb_on_enter(&gH20Host, &mH, &aData);
		}

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		bool bIssued = ohpl_w2_tb_enter(&call);
		bool bResult = ApplyEvents(ctx, aData);

		return bIssued ? bResult : true;
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnUIKeyboardOpen()
	{
		if(OhplBackendRustRequested())
		{
			mbUIKeyboardOpen = true;
			ohpl_h20_tb_ui_keyboard_open(&gH20Host, &mH);
			return;
		}

		mbUIKeyboardOpen = true;

		OhplW2TbCtx ctx;
		OhplW2TextCall call;
		SetupCall(ctx, call);
		ohpl_w2_tb_ui_keyboard_open(&call);
		ApplyEvents(ctx, cGuiMessageData());
	}

	//-----------------------------------------------------------------------

	void cWidgetTextBox::OnUIKeyboardClose()
	{
		mbUIKeyboardOpen = false;
		OnLostFocus(cGuiMessageData());
	}

	//-----------------------------------------------------------------------

}
