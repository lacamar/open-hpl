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

#include "gui/GuiPopUpMessageBox.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "graphics/FontData.h"
#include "gui/RustWidget3.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"

#include "gui/WidgetButton.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetWindow.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cGuiPopUpMessageBox::cGuiPopUpMessageBox(cGuiSet *apSet,
											const tWString& asLabel, const tWString& asText,
											const tWString& asButton1, const tWString& asButton2,
											void *apCallbackObject, tGuiCallbackFunc apCallback) 
						: iGuiPopUp(apSet, false, 0)
	{
		OhplH21Host host = H21Host();
		mR = ohpl_h21_msgbox_new(&host, reinterpret_cast<const uint32_t*>(asLabel.data()), asLabel.size(), reinterpret_cast<const uint32_t*>(asText.data()), asText.size(),
								 reinterpret_cast<const uint32_t*>(asButton1.data()), asButton1.size(), reinterpret_cast<const uint32_t*>(asButton2.data()), asButton2.size(),
								 (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback));
		mpWindow = (cWidgetWindow*)ohpl_h21_window(mR);
	}

	//-----------------------------------------------------------------------

	cGuiPopUpMessageBox::~cGuiPopUpMessageBox()
	{
		OhplH21Host host = H21Host();
		ohpl_h21_destroy(&host, mR);
		mR = NULL;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	

	//-----------------------------------------------------------------------

}
