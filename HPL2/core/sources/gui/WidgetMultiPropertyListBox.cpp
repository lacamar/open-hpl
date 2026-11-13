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

#include "gui/WidgetMultiPropertyListBox.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "gui/WidgetLabel.h"

#include "graphics/FontData.h"


#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"


namespace hpl {

	/////////////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

	cWidgetMultiPropertyListBox::cWidgetMultiPropertyListBox(cGuiSet* apSet, cGuiSkin* apSkin) : iWidgetListBoxBase(apSet,apSkin,1)
	{
		mbClipsGraphics = true;
		mList.num_blank_rows = 1;
	}

	cWidgetMultiPropertyListBox::~cWidgetMultiPropertyListBox()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			ohpl_h20_lb_destroy_columns(&gH20Host, &mH);
		}
	}

	//-------------------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

	/**	Adds a new column
	 *
	 * \param asName 
	 * \param alIndex 
	 */
	void cWidgetMultiPropertyListBox::AddColumn(const tString& asName, const int alIndex, eFontAlign aAlign)
	{
		tWString sName = cString::To16Char(asName);
		ohpl_h20_lb_add_column(&gH20Host, &mH, sName.data(), sName.size(), alIndex, aAlign);
	}

	//-------------------------------------------------------------------------------

	/** Sets column width, and updates the others positions
	 *
	 * \param alIdx 
	 * \param afWidth 
	 */
	void cWidgetMultiPropertyListBox::SetColumnWidth(const int alIdx, float afWidth)
	{
		ohpl_h20_lb_set_column_width(&gH20Host, &mH, alIdx, afWidth);
	}

	cVector3f cWidgetMultiPropertyListBox::GetItemStartPos()
	{
		return GetGlobalPosition() + cVector3f(0, mList.num_blank_rows * (mvDefaultFontSize.y+2), mfBackgroundZ+0.02f);
	}

	//-------------------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

};


