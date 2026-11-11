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

#include "gui/WidgetListBox.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"

#include "gui/WidgetSlider.h"

#include <algorithm>
#include <vector>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetListBox::cWidgetListBox(cGuiSet *apSet, cGuiSkin *apSkin) : iWidgetListBoxBase(apSet, apSkin)
	{
	}

	//-----------------------------------------------------------------------

	cWidgetListBox::~cWidgetListBox()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cWidgetListBox::DrawItems(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		cVector3f vPosition = GetItemStartPos()+cVector3f(3,0,mfBackgroundZ+0.1f);

		std::vector<uint8_t> vRows;
		for(int i=mList.first_item; i < (int)mvItems.size(); ++i)
		{
			if(i-mList.first_item > mList.max_items) break;
			cWidgetItem* pItem = mvItems[i];
			uint8_t lFlags = 0;
			if(pItem==NULL) lFlags = kOhplW2RowNull;
			else
			{
				if(pItem->IsSelectable()==false) lFlags |= kOhplW2RowUnselectable;
				if(pItem->IsSelected()) lFlags |= kOhplW2RowSelected;
			}
			vRows.push_back(lFlags);
		}

		std::vector<OhplW2RowOp> vOps(vRows.size()*3+1);
		size_t lCount = ohpl_w2_list_rows(mList.first_item, mList.max_items, (int)mvItems.size(), &vPosition, mvSize.x,
										  mvDefaultFontSize.y, vRows.data(), vRows.size(), vOps.data(), vOps.size());
		for(size_t i=0; i<lCount; ++i)
		{
			const OhplW2RowOp& op = vOps[i];
			switch(op.kind)
			{
			case kOhplW2OpBackground:
				mpSet->DrawGfx( mpGfxBackground, op.pos, op.size, cColor(0.82f,0.81f,0.79f,1));
				break;
			case kOhplW2OpSelection:
				mpSet->DrawGfx(	mpGfxSelection, op.pos, op.size);
				break;
			case kOhplW2OpTextHighlight:
				DrawDefaultTextHighlight(mvItems[op.item]->GetText(),op.pos,eFontAlign_Left);
				break;
			case kOhplW2OpText:
				DrawDefaultText(mvItems[op.item]->GetText(),op.pos,eFontAlign_Left);
				break;
			}
		}
	}
	
	//-----------------------------------------------------------------------

}
