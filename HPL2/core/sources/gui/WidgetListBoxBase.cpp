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

#include "gui/WidgetListBoxBase.h"

#include "system/LowLevelSystem.h"

#include "math/Math.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"

#include "gui/WidgetSlider.h"
#include "impl/RustBackends.h"

#include <vector>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iWidgetListBoxBase::iWidgetListBoxBase(cGuiSet *apSet, cGuiSkin *apSkin, int alKind) : iWidget(eWidgetType_ListBox,apSet, apSkin),
		mfBackgroundZ(mH.bgZ), mfSliderWidth(mH.sliderW), mList(mH.list)
	{
		ohpl_h20_lb_init(&gH20Host, &mH, (uintptr_t)(iWidget*)this, (uintptr_t)(iWidgetItemContainer*)this, &mB, alKind);

		mbClipsGraphics = true;

		mfBackgroundZ = -0.5;

		mfSliderWidth = mpSkin->GetAttribute(eGuiSkinAttribute_ListBoxSliderWidth).x;

		mpSlider = NULL;

		mbClearingSelection = false;
	}

	//-----------------------------------------------------------------------

	iWidgetListBoxBase::~iWidgetListBoxBase()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			if(OhplBackendRustRequested()) ohpl_h20_lb_destroy_slider(&gH20Host, &mH);
			else mpSet->DestroyWidget(mpSlider);
		}
		ohpl_h20_lb_free(&mH);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::SetSelectedItem(int alX, bool abMoveList, bool abGenerateCallback, eListBoxSelectType aType,
												bool abClearPrevious)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_set_selected_item(&gH20Host, &mH, alX, abMoveList, abGenerateCallback, aType, abClearPrevious);
			return;
		}

		if(abClearPrevious || mList.allow_multi==false)
			ClearSelection();

		if(abMoveList)
			SetCursorPos(alX, true, true);

		cWidgetItem* pItem = GetItem(alX);
		OhplW2SelAction action = ohpl_w2_list_select_action(&mList, aType, alX, pItem!=NULL, pItem ? pItem->IsSelected() : false);
		if(action.kind==1)
			pItem->SetSelected(action.flag);
		else if(action.kind==2)
			SelectRange(action.start, action.end);

		if(abGenerateCallback)
			ProcessMessage(eGuiMessage_SelectionChange, cGuiMessageData(alX));
	}

	//-----------------------------------------------------------------------

	int iWidgetListBoxBase::GetSelectedItem()
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_selected_item(&gH20Host, &mH);
		}

		if(mlstSelectedItems.empty())
			return -1;

		return mlstSelectedItems.back();
	}

	//-----------------------------------------------------------------------

	int iWidgetListBoxBase::GetMultiSelection(int alIdx)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_multi_selection(&gH20Host, &mH, alIdx);
		}

		std::vector<int> vSelected(mlstSelectedItems.begin(), mlstSelectedItems.end());
		return ohpl_w2_list_multi_selection(vSelected.data(), vSelected.size(), alIdx);
	}

	//-----------------------------------------------------------------------

	int iWidgetListBoxBase::GetMultiSelectionNum()
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_multi_selection_num(&gH20Host, &mH);
		}

		return (int)mlstSelectedItems.size();
	}
	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::ClearSelection()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_clear_selection(&gH20Host, &mH);
			return;
		}

		mbClearingSelection = true;

		tIntListIt it = mlstSelectedItems.begin();
		for(;it!=mlstSelectedItems.end();++it)
		{
			cWidgetItem* pItem = GetItem(*it);
			if(pItem) pItem->SetSelected(false);
		}
		mlstSelectedItems.clear();

		mbClearingSelection = false;
	}

	//-----------------------------------------------------------------------

	cVector3f iWidgetListBoxBase::GetItemStartPos()
	{
		if(OhplBackendRustRequested())
		{
			cVector3f vPos;
			ohpl_h20_lb_item_start_pos(&gH20Host, &mH, &vPos);
			return vPos;
		}

		return GetGlobalPosition() + cVector3f(0,2,0.01f);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::SelectRange(int alStart, int alEnd)
	{
		OhplW2Range range = ohpl_w2_list_range(alStart, alEnd, GetItemNum());

		for(int i=0; i<range.count; ++i)
		{
			cWidgetItem* pItem = GetItem(range.first + i*range.step);
			if(pItem)
				pItem->SetSelected(true);
		}
		if(range.end_valid)
		{
			cWidgetItem* pItem = GetItem(alEnd);
			if(pItem)
				pItem->SetSelected(true);
		}
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::UpdateProperties()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_update_properties(&gH20Host, &mH);
			return;
		}

		OhplW2ListUpdate update;
		ohpl_w2_list_update(&mList, mvSize.y, mvDefaultFontSize.y, (int)mvItems.size(), &update);

        if(mpSlider == NULL)
			return;

		if(update.big)
		{
			mpSlider->SetBarValueSize(update.bar_value_size);
			mpSlider->SetMaxValue(update.max_value);
		}
		else
		{
			mpSlider->SetMaxValue(update.max_value);
			mpSlider->SetBarValueSize(update.bar_value_size);
		}

		mpSlider->SetButtonValueAdd(1);
		mpSlider->SetBarClickValueAdd(mList.max_items-1);

		std::vector<int> vSelected(mlstSelectedItems.begin(), mlstSelectedItems.end());
		vSelected.resize(ohpl_w2_list_prune(vSelected.data(), vSelected.size(), GetItemNum()));
		mlstSelectedItems.assign(vSelected.begin(), vSelected.end());

		mpSlider->SetValue(GetSelectedItem(), true);

		if(mList.num_items==0)
			SetCursorPos(0,true, true);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnChangeSize()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_on_change_size(&gH20Host, &mH);
			return;
		}

		if(mpSlider)
		{
			mpSlider->SetSize(cVector2f(mfSliderWidth,mvSize.y));
			mpSlider->SetPosition(cVector3f(mvSize.x - mfSliderWidth, 0, 0.2f));
		}

		UpdateProperties();
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnInit()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_on_init(&gH20Host, &mH);
			return;
		}

		mpSlider = mpSet->CreateWidgetSlider(eWidgetSliderOrientation_Vertical,0,0,0,this);
		mpSlider->AddCallback(eGuiMessage_SliderMove,this,kGuiCallback(MoveSlider));

		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::AddItemToSelection(cWidgetItem* apItem)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_add_item_to_selection(&gH20Host, &mH, (uintptr_t)apItem);
			return;
		}

		mlstSelectedItems.push_back(apItem->GetIndex());
	}

	void iWidgetListBoxBase::RemoveItemFromSelection(cWidgetItem* apItem)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_remove_item_from_selection(&gH20Host, &mH, (uintptr_t)apItem);
			return;
		}

		if(mbClearingSelection==false)
			mlstSelectedItems.remove(apItem->GetIndex());
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::MoveSlider(iWidget* apWidget, const cGuiMessageData& aData)
	{
		mList.first_item = aData.mlVal;

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(iWidgetListBoxBase,MoveSlider)

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnLoadGraphics()
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_on_load_graphics(&gH20Host, &mH);
			return;
		}

		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_ListBoxBackground);

		mpGfxSelection = mpSkin->GetGfx(eGuiSkinGfx_TextBoxSelectedTextBack);

		mvGfxBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderRight);
		mvGfxBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderLeft);
		mvGfxBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderUp);
		mvGfxBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderDown);

		mvGfxCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerLU);
		mvGfxCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerRU);
		mvGfxCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerRD);
		mvGfxCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerLD);
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_on_draw(&gH20Host, &mH);
			return;
		}

		////////////////////////////////
		// Background
		mpSet->DrawGfx(	mpGfxBackground,GetGlobalPosition() +cVector3f(0,0,mfBackgroundZ),
						mvSize);


		////////////////////////////////
		// Borders
		DrawBordersAndCorners(	NULL, mvGfxBorders, mvGfxCorners,
								GetGlobalPosition() -
									cVector3f(	mvGfxCorners[0]->GetActiveSize().x,
												mvGfxCorners[0]->GetActiveSize().y,0),
									mvSize +	mvGfxCorners[0]->GetActiveSize() +
												mvGfxCorners[2]->GetActiveSize());
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::OnDrawAfterClip(float afTimeStep, cGuiClipRegion* apClipRegion)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_on_draw_after_clip(&gH20Host, &mH, (uintptr_t)apClipRegion);
			return;
		}

		cGuiClipRegion* pRegion = apClipRegion->CreateChild(GetGlobalPosition(), mvSize-cVector2f(mfSliderWidth,0));
		mpSet->SetCurrentClipRegion(pRegion);

		DrawItems(afTimeStep, pRegion);
		// Draw cursor
		if(HasFocus())
		{
			if(mList.cursor_pos-mList.first_item!=-1)
			{
				cVector3f vPosition = GetItemStartPos();
				OhplW2PosSize vLines[4];
				if(ohpl_w2_list_cursor_lines(&mList, &vPosition, mvSize.x, mfSliderWidth, mvDefaultFontSize.y, vLines))
				{
					for(int i=0; i<4; ++i)
						mpSet->DrawGfx(cGui::mpGfxRect, vLines[i].pos, vLines[i].size, cColor(0.3f, 0.4));
				}
			}
		}

		mpSet->SetCurrentClipRegion(apClipRegion);
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseMove(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseDown(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_on_mouse_down(&gH20Host, &mH, &aData);
		}

		int lSliderValue = mpSlider->GetValue();
		int lWheel = ohpl_w2_list_wheel(aData.mlVal);
		if(lWheel==0)
		{
			mpSlider->SetValue(lSliderValue-5);
			return true;
		}

		if(lWheel==1)
		{
			mpSlider->SetValue(lSliderValue+5);
			return true;
		}

		cVector3f vLocalPos = WorldToLocalPosition(aData.mvPos);

		int lModifier = aData.mKeyPress.mlModifier;
		OhplW2ListMouse mouse = ohpl_w2_list_mouse_down(&mList, vLocalPos.y, mvDefaultFontSize.y, lModifier);
		if(mouse.proceed==false) return false;

		ApplyCursor(mouse.cursor, true);

		bool bUsingShift = (lModifier & eKeyModifier_Shift)!=0;
		int lCursor = ohpl_w2_list_mouse_down_finish(&mList, bUsingShift, mouse.index);

		cWidgetItem* pItem = GetItem(lCursor);
		if(GetMultiSelectionNum()==1 && pItem && pItem->IsSelected())
			;
		else
			SetSelectedItem(lCursor, false, true, (eListBoxSelectType)mouse.ty, mouse.clear);

		return true;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseUp(const cGuiMessageData& aData)
	{
		return true;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseDoubleClick(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_on_mouse_double_click(&gH20Host, &mH, &aData);
		}

		cVector3f vLocalPos = WorldToLocalPosition(aData.mvPos);

		bool bFire = false;
		bool bRet = ohpl_w2_list_double_click(&mList, vLocalPos.x, mvSize.x, mpSlider->GetSize().x, &bFire);
		if(bFire)
			ProcessMessage(eGuiMessage_SelectionDoubleClick, aData);

		return bRet;
	}


	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseEnter(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnMouseLeave(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnKeyPress(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_on_key_press(&gH20Host, &mH, &aData);
		}

		int lMod = aData.mKeyPress.mlModifier;
		OhplW2ListKey key = ohpl_w2_list_key(&mList, aData.mKeyPress.mKey, lMod);

		if(key.arrow)
		{
			ApplyCursor(key.cursor, true);

			OhplW2ListKey finish = ohpl_w2_list_key_finish(&mList, lMod);
			if(finish.select)
				SetSelectedItem(mList.cursor_pos, false, true, (eListBoxSelectType)finish.ty, finish.clear);

			return true;
		}
		if(key.select)
		{
			SetSelectedItem(mList.cursor_pos, false, true, (eListBoxSelectType)key.ty, key.clear);

			return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnUIArrowPress(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_on_ui_arrow_press(&gH20Host, &mH, &aData);
		}

		OhplW2ListArrow arrow = ohpl_w2_list_ui_arrow(&mList, aData.mlVal);
		if(arrow.handled==false)
			return false;

		ApplyCursor(arrow.cursor, true);

		SetSelectedItem(mList.cursor_pos, true, true, eListBoxSelectType_Select);

		if(arrow.unlock)
		{
			ProcessMessage(eGuiMessage_CheckChange, aData);
			mList.locked = false;
		}

		return true;
	}

	bool iWidgetListBoxBase::OnUIArrowRelease(const cGuiMessageData& aData)
	{
		return false;
	}

	//-----------------------------------------------------------------------

	bool iWidgetListBoxBase::OnUIButtonPress(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_on_ui_button_press(&gH20Host, &mH, &aData);
		}

		OhplW2ListButton button = ohpl_w2_list_ui_button(&mList, HasFocus(), aData.mlVal, GetSelectedItem());
		if(button.ret==0) return false;
		if(button.ret==1) return true;

		if(button.init_select)
		{
			SetCursorPos(0, true,true);
			SetSelectedItem(mList.cursor_pos, true, true, eListBoxSelectType_Select);
		}
		if(button.check_locked)
		{
		    if(ProcessMessage(eGuiMessage_CheckChange, aData))
			{
				mList.locked = false;
				return true;
			}
		}

		mList.locked = true;

		return true;
	}

	bool iWidgetListBoxBase::OnUIButtonRelease(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_on_ui_button_release(&gH20Host, &mH);
		}

		return HasFocus();
	}

	bool iWidgetListBoxBase::OnUIButtonDoublePress(const cGuiMessageData& aData)
	{
		if(OhplBackendRustRequested())
		{
			return ohpl_h20_lb_on_ui_button_double_press(&gH20Host, &mH, &aData);
		}

		if(HasFocus())
		{
			if(aData.mlVal==eUIButton_Primary)
			{
				mList.locked = false;
				return ProcessMessage(eGuiMessage_SelectionDoubleClick, aData);
			}

			return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------

	void iWidgetListBoxBase::ApplyCursor(const OhplW2SliderSet& aFirst, bool abMoveList)
	{
		if(aFirst.set) mpSlider->SetValue(aFirst.value);

		OhplW2SliderSet second = ohpl_w2_list_cursor2(&mList, abMoveList);
		if(second.set) mpSlider->SetValue(second.value);
	}

	void iWidgetListBoxBase::SetCursorPos(int alPos, bool abMoveList, bool abClamp)
	{
		if(OhplBackendRustRequested())
		{
			ohpl_h20_lb_set_cursor_pos(&gH20Host, &mH, alPos, abMoveList, abClamp);
			return;
		}

		ApplyCursor(ohpl_w2_list_cursor1(&mList, alPos, abMoveList, abClamp), abMoveList);
	}

	//-----------------------------------------------------------------------


}
