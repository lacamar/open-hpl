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

#include "gui/WidgetLabel.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "graphics/FontData.h"

#include "math/Math.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"
#include "gui/RustWidget.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetLabel::cWidgetLabel(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_Label,apSet, apSkin)
	{
		mfScrollTimer = 0.0f;
		mfWaitToScrollTime = 1.0f;
		mfScrollSpeedMul = 1.0f;

		mbWordWrap = false;
		mfWordWrapOffset = 0.0f;
		mfWordWrapRowsHeight = 0.0f;
		mbScrollingDown = true;

		mTextAlign = eFontAlign_Left;

		mlMaxCharacters = -1;

		mbAutogenerateSize = false;

		mbDrawBackGround = false;
		mBackGroundColor = cColor(1,1);

		LoadGraphics();
	}

	//-----------------------------------------------------------------------

	cWidgetLabel::~cWidgetLabel()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cWidgetLabel::SetMaxTextLength(int alLength)
	{
		if(mlMaxCharacters == alLength) return;

		mlMaxCharacters = alLength;
	}

	//-----------------------------------------------------------------------

	void cWidgetLabel::SetAutogenerateSize(bool abX)
	{
		if(mbAutogenerateSize == abX) return;

		mbAutogenerateSize = abX;
		
		OnChangeText();
	}

	//-----------------------------------------------------------------------

	void cWidgetLabel::SetDefaultFontSize(const cVector2f& avSize)
	{
		iWidget::SetDefaultFontSize(avSize);

		OnChangeText();
	}

	//-----------------------------------------------------------------------



	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetLabel::DrawText(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		OhplWdLabelDraw d;
		d.global = GetGlobalPosition();
		d.size = mvSize;
		d.align = mTextAlign;
		d.word_wrap = mbWordWrap;
		d.max_chars = mlMaxCharacters;
		d.enabled = IsEnabled();
		d.has_default_font = mpDefaultFontType != NULL;
		d.font_size = mvDefaultFontSize;
		d.font_color = mDefaultFontColor;
		d.color_mul = mColorMul;
		d.wrap_offset = mfWordWrapOffset;
		d.text_len = (int)msText.length();

		tWStringVec vRows;
		std::vector<int> vRowLengths;
		if(mbWordWrap)
		{
			float fHeight = mvDefaultFontSize.y+2;
			mpDefaultFontType->GetWordWrapRows(mvSize.x,fHeight,
												mvDefaultFontSize, msText,
												&vRows);
			for(size_t i=0; i<vRows.size(); ++i)
				vRowLengths.push_back((int)vRows[i].length());
		}

		std::vector<OhplWdOp> vOps(vRows.size() + 1);
		float fRowsHeight = 0;
		bool bRowsHeightSet = false;
		size_t lCount = ohpl_wd_label_draw_text(&vOps[0], vOps.size(), &d, vRowLengths.empty() ? NULL : &vRowLengths[0], vRowLengths.size(),
												&fRowsHeight, &bRowsHeightSet);
		if(bRowsHeightSet)
			mfWordWrapRowsHeight = fRowsHeight;

		std::vector<const tWString*> vTexts;
		vTexts.push_back(&msText);
		for(size_t i=0; i<vRows.size(); ++i)
			vTexts.push_back(&vRows[i]);
		iFontData* fonts[1] = { mpDefaultFontType };
		OhplWdRun(mpSet, &vOps[0], lCount, OhplWdGfxTable(), &vTexts[0], fonts);
	}

	//-----------------------------------------------------------------------

	void cWidgetLabel::OnLoadGraphics()
	{
		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_FrameBackground);
	}

	//-----------------------------------------------------------------------

	void cWidgetLabel::OnUpdate(float afTimeStep)
	{
		if(mbWordWrap==false)
			return;

		OhplWdLabelScroll scroll;
		scroll.timer = mfScrollTimer;
		scroll.offset = mfWordWrapOffset;
		scroll.down = mbScrollingDown;
		OhplWdLabelScroll next;
		ohpl_wd_label_update(&scroll, afTimeStep, mfScrollSpeedMul, mvSize.y, mfWordWrapRowsHeight, &next);
		mfScrollTimer = next.timer;
		mfWordWrapOffset = next.offset;
		mbScrollingDown = next.down;
	}

	//-----------------------------------------------------------------------

	void cWidgetLabel::OnChangeText()
	{
		mfScrollTimer = mfWaitToScrollTime;
		mfWordWrapOffset = 0;
		mbScrollingDown = true;

		if(mbAutogenerateSize==false) return;

		float fLength = mpDefaultFontType->GetLength(mvDefaultFontSize, msText.c_str());

        SetSize(cVector2f(fLength, mvDefaultFontSize.y));
	}

	//-----------------------------------------------------------------------

	void cWidgetLabel::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		////////////////////////////////
		// Background
		if(mbDrawBackGround)
		{
			cVector3f vGlobal = GetGlobalPosition();
			cVector3f vBackPos;
			cVector2f vBackSize;
			ohpl_wd_label_background(&vGlobal, &mvSize, &vBackPos, &vBackSize);
			mpSet->DrawGfx(	mpGfxBackground,vBackPos,
				vBackSize,mBackGroundColor);
		}

		if(mbClipsGraphics)
			return;

		DrawText(afTimeStep, apClipRegion);
	}

	//-----------------------------------------------------------------------

	void cWidgetLabel::OnDrawAfterClip(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		DrawText(afTimeStep, apClipRegion);
	}

	//-----------------------------------------------------------------------
	
	bool cWidgetLabel::OnMouseMove(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetLabel::OnMouseDown(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetLabel::OnMouseUp(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool cWidgetLabel::OnMouseEnter(const cGuiMessageData& aData)
	{
		return false;
	}
	
	//-----------------------------------------------------------------------
	
	bool cWidgetLabel::OnMouseLeave(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------


}
