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

#include "gui/WidgetGroup.h"

#include "gui/GuiTypes.h"

#include "gui/Gui.h"
#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/GuiGfxElement.h"
#include "gui/RustWidget.h"

#include "gui/WidgetLabel.h"

#include "graphics/FontData.h"



namespace hpl {
	
	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetGroup::cWidgetGroup(cGuiSet* apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_Group, apSet, apSkin)
	{
		mpHeader = NULL;

		LoadGraphics();
	}

	//-----------------------------------------------------------------------
	
	cWidgetGroup::~cWidgetGroup()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			mpSet->DestroyWidget(mpHeader);
		}
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetGroup::SetDefaultFontSize(const cVector2f& avSize)
	{
		iWidget::SetDefaultFontSize(avSize);
		mpHeader->SetDefaultFontSize(avSize);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetGroup::OnInit()
	{
		cVector3f vHeaderPos;
		ohpl_wd_group_header_pos(mfHeaderOffset, mpDefaultFont->mvSize.y, &vHeaderPos);
		mpHeader = mpSet->CreateWidgetLabel(vHeaderPos,0, msText, this);
		mpHeader->SetTextAlign(eFontAlign_Left);
		mpHeader->SetAutogenerateSize(true);

		OnChangeText();
	}

	//-----------------------------------------------------------------------

	void cWidgetGroup::OnLoadGraphics()
	{
		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_GroupBackground);

		mvGfxBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_GroupBorderRight);
		mvGfxBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_GroupBorderLeft);
		mvGfxBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_GroupBorderUp);
		mvGfxBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_GroupBorderDown);

		mvGfxCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_GroupCornerLU);
		mvGfxCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_GroupCornerRU);
		mvGfxCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_GroupCornerRD);
		mvGfxCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_GroupCornerLD);

		mfHeaderOffset = mpSkin->GetAttribute(eGuiSkinAttribute_GroupHeaderOffset).x;
		mbHeaderBreaksUpperBorder = (mpSkin->GetAttribute(eGuiSkinAttribute_GroupHeaderOverUpperBorder).x==0);
	}

	//-----------------------------------------------------------------------

	void cWidgetGroup::OnChangeText()
	{
		if(mpHeader) 
			mpHeader->SetText(msText);
	}

	//-----------------------------------------------------------------------

	void cWidgetGroup::OnDraw(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		OhplWdGroupDraw d;
		d.global = GetGlobalPosition();
		d.size = mvSize;
		d.has_background = mpGfxBackground != NULL;
		d.header_breaks_border = mbHeaderBreaksUpperBorder;
		if(mbHeaderBreaksUpperBorder)
		{
			d.header_pos = mpHeader->GetLocalPosition();
			d.header_size = mpHeader->GetSize();
		}

		OhplWdGfxTable table;
		table.SetFrame(mpGfxBackground, mvGfxBorders, mvGfxCorners);

		OhplWdOp ops[kOhplWdMaxOps];
		size_t lCount = ohpl_wd_group_draw(ops, kOhplWdMaxOps, &d, table.m);
		OhplWdRun(mpSet, ops, lCount, table, NULL, NULL);
	}

	//-----------------------------------------------------------------------

	void cWidgetGroup::OnDrawAfterClip(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
	}

	//-----------------------------------------------------------------------

};
