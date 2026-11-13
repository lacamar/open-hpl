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

#ifndef HPL_WIDGET_TEXT_BOX_H
#define HPL_WIDGET_TEXT_BOX_H

#include "gui/Widget.h"
#include "gui/RustWidget2.h"
#include "gui/RustGuiH20.h"

namespace hpl {

	class cGuiSkinFont;
	
	class cWidgetButton;
	class cGuiPopUpUIKeyboard;

	class cWidgetTextBox : public iWidget
	{
		friend class cGuiPopUpUIKeyboard;
		friend struct OhplH20Access;
	public:
		cWidgetTextBox(cGuiSet *apSet, cGuiSkin *apSkin, eWidgetTextBoxInputType aType);
		virtual ~cWidgetTextBox();

		void SetDefaultFontSize(const cVector2f& avSize);

		void SetCallbackOnLostFocus(bool abX) { mState.enter_on_lost_focus = abX; }
		bool GetCallbackOnLostFocus() { return mState.enter_on_lost_focus; }

		void SetForceCallBackOnEnter(bool abX) { mState.force_enter=abX; }
		bool GetForceCallBackOnEnter() { return mState.force_enter; }
		
		void SetMaxTextLength(int alLength);
		int GetMaxTextLength(){return mState.max_chars;}

		void SetSelectedText(int alStart=0, int alCount=-1);

		cVector2f GetBackgroundSize();

		void SetMaxTextSizeNeg(float afX);

		void SetCanEdit(bool abX);
		bool GetCanEdit(){return mState.can_edit;}

		void SetShowButtons(bool abX) { mbShowButtons = abX; }

		void SetNumericAdd(float afX) { mState.numeric_add = afX; }
		float GetNumericAdd() { return mState.numeric_add; }

		void SetNumericValue(float afX);
		float GetNumericValue();

		void SetLowerBound(bool abX, float afValue=0);
		float GetLowerBound() { return mState.lower; }
		void SetUpperBound(bool abX, float afValue=0);
		float GetUpperBound() { return mState.upper; }

		void SetDecimals(int alX) { mState.decimals = alX; }
		int GetDecimals();

		static void SetDefaultDecimals(int alX) { mlDefaultDecimals = alX; ohpl_h20_tb_set_default_decimals(alX); }

		void SetIllegalChars(const tWString &asIllegalChars);
		const tWString& GetIllegalChars();

		void SetLegalCharCodeLimitEnabled(bool abX);
		
		/**
		* A character that is lower (NOT equal) to this is considered illegal
		*/
		void SetLegalCharCodeMinLimit(wchar_t alX);
		/**
		* A character that is higher (NOT equal) to this is considered illegal
		*/
		void SetLegalCharCodeMaxLimit(wchar_t alX);

		bool GetLegalCharCodeLimitEnabled(){ return mState.limit_enabled;}
		wchar_t GetLegalCharCodeMinLimit(){ return mState.limit_min;}
		wchar_t GetLegalCharCodeMaxLimit(){ return mState.limit_max;}
		
	protected:
		/////////////////////////
		// Implemented functions
		void OnInit();
		void OnLoadGraphics();
		void OnChangeSize();
		void OnChangeText();
		
		void OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion);

		bool OnMouseMove(const cGuiMessageData& aData);
		bool OnMouseDown(const cGuiMessageData& aData);
		bool OnMouseDoubleClick(const cGuiMessageData& aData);
		bool OnMouseUp(const cGuiMessageData& aData);
		bool OnMouseEnter(const cGuiMessageData& aData);
		bool OnMouseLeave(const cGuiMessageData& aData);

		bool OnGotFocus(const cGuiMessageData& aData);
		bool OnGotTabFocus(const cGuiMessageData& aData);
		bool OnLostFocus(const cGuiMessageData& aData);

		bool OnKeyPress(const cGuiMessageData& aData);

		bool OnUIButtonPress(const cGuiMessageData& aData);

		bool OnEnter(const cGuiMessageData& aData);


		////////////////////////////
		// Interface for UI keyboard
		void OnUIKeyboardOpen();
		void OnUIKeyboardClose();

		/////////////////////////
		// Data
		OhplH20TextBox mH;
		iWidget*& mpPrevAttention;
		OhplW2Text& mState;

		eWidgetTextBoxInputType mInputType;
		bool& mbShowButtons;

		static int mlDefaultDecimals;

		tWString msIllegalChars;

		bool mbUIKeyboardOpen;
	};

};
#endif // HPL_WIDGET_TEXT_BOX_H
