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

#include "gui/GuiPopUpColorPicker.h"

#include "gui/GuiTypes.h"

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"

#include "math/Math.h"

#include "graphics/FontData.h"
#include "gui/RustWidget3.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"

#include "gui/WidgetTextBox.h"
#include "gui/WidgetWindow.h"
#include "gui/WidgetButton.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetLabel.h"

namespace hpl {

	/////////////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

	cGuiPopUpColorPicker::cGuiPopUpColorPicker(cGuiSet* apSet, 
											   cGuiSkin* apSkin,
											   const cVector3f& avPos, 
											   cColor* apDestColor, 
											   void *apCallbackObject, tGuiCallbackFunc apCallback) : iGuiPopUp(apSet, true, cVector2f(350,250))
	{
		OhplH21Host host = H21Host();
		mR = ohpl_h21_color_new(&host, (uintptr_t)apDestColor, (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback));
		mpWindow = (cWidgetWindow*)ohpl_h21_window(mR);
	}

	//-------------------------------------------------------------------------------

	cGuiPopUpColorPicker::~cGuiPopUpColorPicker()
	{
		OhplH21Host host = H21Host();
		ohpl_h21_destroy(&host, mR);
		mR = NULL;
	}

	//-------------------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	/////////////////////////////////////////////////////////////////////////////////


	//-------------------------------------------------------------------------------

};
