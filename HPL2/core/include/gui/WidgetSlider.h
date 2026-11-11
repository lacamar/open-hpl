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

#ifndef HPL_WIDGET_SLIDER_H
#define HPL_WIDGET_SLIDER_H

#include "gui/Widget.h"
#include "gui/RustWidget2.h"

namespace hpl {

	class cGuiSkinFont;

	class cWidgetButton;

	//---------------------------------------------

	class cWidgetSlider : public iWidget
	{
	public:
		cWidgetSlider(cGuiSet *apSet, cGuiSkin *apSkin, eWidgetSliderOrientation aOrientation);
		virtual ~cWidgetSlider();

		int GetValue(){ return mSlider.value;}
		void SetValue(int alValue, bool abGenCallback=true);
		
		int GetMaxValue(){ return mSlider.max_value;}
		void SetMaxValue(int alMax);
		
		int GetButtonValueAdd(){ return mSlider.button_add; }
		void SetButtonValueAdd(int alAdd);

		int GetBarClickValueAdd() { return mSlider.click_add; }
		void SetBarClickValueAdd(int alAdd);

		int GetBarValueSize(){ return mSlider.bar_value_size;}
		void SetBarValueSize(int alSize);

		float GetButtonSize() const{return mSlider.button_size;}

	protected:
		/////////////////////////
		// Own functions
		void UpdateBarProperties();

		bool ArrowButtonDown(iWidget* apWidget, const cGuiMessageData& aData);
		kGuiCallbackDeclarationEnd(ArrowButtonDown);		

		/////////////////////////
		// Implemented functions
		void OnInit();
		void OnLoadGraphics();
		void OnChangeSize();
		void OnChangePosition();

		
		void OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion);

		bool OnMouseMove(const cGuiMessageData& aData);
		bool OnMouseDown(const cGuiMessageData& aData);
		bool OnMouseUp(const cGuiMessageData& aData);
		bool OnMouseEnter(const cGuiMessageData& aData);
		bool OnMouseLeave(const cGuiMessageData& aData);

		bool OnMouseDoubleClick(const cGuiMessageData& aData);

		bool OnLostFocus(const cGuiMessageData& aData);

		bool OnUIArrowPress(const cGuiMessageData& aData);
		bool OnUIArrowRelease(const cGuiMessageData& aData);

		bool OnUIButtonPress(const cGuiMessageData& aData);
		bool OnUIButtonRelease(const cGuiMessageData& aData);

		/////////////////////////
		// Data
		OhplW2Slider mSlider;

		cWidgetButton* mvButtons[2];

		cGuiGfxElement *mpGfxButtonBackground;
		cGuiGfxElement *mvGfxBorders[4];
		cGuiGfxElement *mvGfxCorners[4];

		cGuiGfxElement *mvGfxArrow[2];
		cGuiGfxElement *mpGfxBackground;
	};

};
#endif // HPL_WIDGET_SLIDER_H
