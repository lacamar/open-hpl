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

#ifndef HPL_WIDGET_COMBO_BOX_H
#define HPL_WIDGET_COMBO_BOX_H

#include "gui/Widget.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetBaseClasses.h"
#include "gui/RustWidget2.h"
#include "gui/RustGuiH20.h"


namespace hpl {

	class cGuiSkinFont;
	
	class cWidgetTextBox;
	class cWidgetButton;
	class cWidgetSlider;

	class cWidgetComboBox : public iWidget, public iWidgetItemContainer
	{
	public:
		cWidgetComboBox(cGuiSet *apSet, cGuiSkin *apSkin);
		virtual ~cWidgetComboBox();

		void SetSelectedItem(int alX,bool abMoveList=false, bool abGenCallback=true);
		int GetSelectedItem(){ return mCombo.selected;}

		void SetCanEdit(bool abX);
		bool GetCanEdit();

		void SetMaxShownItems(int alX);
		int GetMaxShownItems(){ return mCombo.max_items;}

		void SetDefaultFontSize(const cVector2f& avSize);
		
	protected:
		/////////////////////////
		// Own functions
		void UpdateProperties();

		/////////////////////////
		// Implemented functions
		void OnLoadGraphics();
		void OnChangeSize();
		void OnChangeText();
		void OnInit();
		
		void OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion);

		bool OnMouseMove(const cGuiMessageData& aData);
		bool OnMouseDown(const cGuiMessageData& aData);
		bool OnMouseUp(const cGuiMessageData& aData);
		bool OnMouseEnter(const cGuiMessageData& aData);
		bool OnMouseLeave(const cGuiMessageData& aData);

		bool OnUIArrowPress(const cGuiMessageData& aData);

		bool OnUIButtonPress(const cGuiMessageData& aData);
		bool OnUIButtonRelease(const cGuiMessageData& aData);
		
		bool OnLostFocus(const cGuiMessageData& aData);

		bool PointIsInside(const cVector2f& avPoint, bool abOnlyClipped);

		/////////////////////////
		// Data
		
		OhplH20Combo mH;
		OhplW2Combo& mCombo;

		float& mfButtonWidth;
		float& mfSliderWidth;

	};

};
#endif // HPL_WIDGET_COMBO_BOX_H
