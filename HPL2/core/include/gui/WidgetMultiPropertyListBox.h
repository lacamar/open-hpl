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

#ifndef HPL_WIDGET_MULTI_PROPERTY_LIST_BOX_H
#define HPL_WIDGET_MULTI_PROPERTY_LIST_BOX_H

#include "gui/WidgetListBoxBase.h"
#include "system/String.h"

namespace hpl {

	class cWidgetLabel;

	class cWidgetMultiPropertyListBox : public iWidgetListBoxBase
	{
	public:
		cWidgetMultiPropertyListBox(cGuiSet* apSet, cGuiSkin* apSkin) : iWidgetListBoxBase(apSet, apSkin, 1) {}

		void AddColumn(const tString& asName, const int alIndex, eFontAlign aAlign=eFontAlign_Left )
		{
			tWString sName = cString::To16Char(asName);
			ohpl_h20_lb_add_column(&H(), sName.data(), sName.size(), alIndex, aAlign);
		}
		void SetColumnWidth(const int alIdx, float afWidth){ ohpl_h20_lb_set_column_width(&H(), alIdx, afWidth); }
	};

};
#endif // HPL_WIDGET_MULTI_PROPERTY_LIST_BOX_H
