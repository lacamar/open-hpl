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
#include "gui/RustGuiGw.h"
#include "gui/RustWidget2.h"
#include "gui/RustGuiH20.h"

namespace hpl {

	extern "C" {
	int ohpl_h20_tb_default_decimals();
	void ohpl_gw_tb_kb_open(uintptr_t);
	void ohpl_gw_tb_kb_close(uintptr_t);
	}

	// Rust kind (rust/src/gui/gw_h20.rs, logic in h20_text.rs)
	class cWidgetTextBox : public iWidget
	{
	public:
		cWidgetTextBox(cGuiSet *apSet, cGuiSkin *apSkin, eWidgetTextBoxInputType aType)
			: iWidget(eWidgetType_TextBox, apSet, apSkin) { ohpl_gw_set_kind((uintptr_t)this, kOhplGwTextBox, aType==eWidgetTextBoxInputType_Numeric, 0); }


		void SetCallbackOnLostFocus(bool abX) { St().enter_on_lost_focus = abX; }
		bool GetCallbackOnLostFocus() { return St().enter_on_lost_focus; }

		void SetForceCallBackOnEnter(bool abX) { St().force_enter=abX; }
		bool GetForceCallBackOnEnter() { return St().force_enter; }

		void SetMaxTextLength(int alLength){ ohpl_h20_tb_set_max_text_length(&gH20Host, &H(), alLength); }
		int GetMaxTextLength(){return St().max_chars;}

		void SetSelectedText(int alStart=0, int alCount=-1){ ohpl_h20_tb_set_selected_text(&gH20Host, &H(), alStart, alCount); }

		cVector2f GetBackgroundSize(){ cVector2f vSize; ohpl_h20_tb_background_size(&gH20Host, &H(), &vSize); return vSize; }

		void SetMaxTextSizeNeg(float afX){ St().max_size_neg = afX; ohpl_h20_tb_on_change_size(&gH20Host, &H()); }

		void SetCanEdit(bool abX){ ohpl_h20_tb_set_can_edit(&gH20Host, &H(), abX); }
		bool GetCanEdit(){return St().can_edit;}

		void SetShowButtons(bool abX) { H().showButtons = abX; }

		void SetNumericAdd(float afX) { St().numeric_add = afX; }
		float GetNumericAdd() { return St().numeric_add; }

		void SetNumericValue(float afX){ ohpl_h20_tb_set_numeric_value(&gH20Host, &H(), afX); }
		float GetNumericValue(){ return ohpl_h20_tb_numeric_value(&gH20Host, &H()); }

		void SetLowerBound(bool abX, float afValue=0){ St().has_lower = abX; St().lower = afValue; }
		float GetLowerBound() { return St().lower; }
		void SetUpperBound(bool abX, float afValue=0){ St().has_upper = abX; St().upper = afValue; }
		float GetUpperBound() { return St().upper; }

		void SetDecimals(int alX) { St().decimals = alX; }
		int GetDecimals(){ return St().decimals<0 ? ohpl_h20_tb_default_decimals() : St().decimals; }

		static void SetDefaultDecimals(int alX) { ohpl_h20_tb_set_default_decimals(alX); }

		void SetIllegalChars(const tWString &asIllegalChars){ ohpl_h20_tb_set_illegal_chars(&gH20Host, &H(), asIllegalChars.data(), asIllegalChars.size()); }

		void SetLegalCharCodeLimitEnabled(bool abX){ St().limit_enabled = abX; }

		/**
		* A character that is lower (NOT equal) to this is considered illegal
		*/
		void SetLegalCharCodeMinLimit(wchar_t alX){ St().limit_min = alX; }
		/**
		* A character that is higher (NOT equal) to this is considered illegal
		*/
		void SetLegalCharCodeMaxLimit(wchar_t alX){ St().limit_max = alX; }

		bool GetLegalCharCodeLimitEnabled(){ return St().limit_enabled;}
		wchar_t GetLegalCharCodeMinLimit(){ return St().limit_min;}
		wchar_t GetLegalCharCodeMaxLimit(){ return St().limit_max;}

	private:
		OhplH20TextBox& H(){ return *(OhplH20TextBox*)ohpl_gw_state((uintptr_t)this); }
		OhplW2Text& St(){ return H().st; }
	};

};
#endif // HPL_WIDGET_TEXT_BOX_H
