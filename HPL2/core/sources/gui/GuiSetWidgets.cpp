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

#include "gui/GuiSet.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiGfxElement.h"
#include "gui/Widget.h"
#include "gui/GuiPopUp.h"
#include "gui/GuiPopUpMessageBox.h"
#include "gui/GuiPopUpFilePicker.h"
#include "gui/GuiPopUpColorPicker.h"
#include "gui/GuiPopUpUIKeyboard.h"

#include "gui/WidgetWindow.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetButton.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetSlider.h"
#include "gui/WidgetTextBox.h"
#include "gui/WidgetCheckBox.h"
#include "gui/WidgetImage.h"
#include "gui/WidgetListBox.h"
#include "gui/WidgetMultiPropertyListBox.h"
#include "gui/WidgetComboBox.h"
#include "gui/WidgetMenuItem.h"
#include "gui/WidgetContextMenu.h"
#include "gui/WidgetMainMenu.h"
#include "gui/WidgetTabFrame.h"
#include "gui/WidgetGroup.h"
#include "gui/WidgetDummy.h"

namespace hpl {

	// Construction of C++ widgets and popups; the set itself is Rust-owned (rust/src/gui/g1_set.rs).

	cWidgetTextBox* cGuiSet::CreateWidgetTextBox( const cVector3f &avLocalPos,
												  const cVector2f &avSize,
												  const tWString &asText,
												  iWidget *apParent,
												  eWidgetTextBoxInputType aType,
												  float afNumericAdd,
												  bool abShowButtons,
												  const tString& asName)
	{
		cWidgetTextBox *pTextBox = hplNew( cWidgetTextBox, (this,GetSkin(),aType) );
		pTextBox->SetPosition(avLocalPos);
		pTextBox->SetSize(avSize);
		pTextBox->SetShowButtons(abShowButtons);
		pTextBox->SetNumericAdd(afNumericAdd);
		pTextBox->SetText(asText);
		pTextBox->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pTextBox, (uintptr_t)apParent);
		return pTextBox;
	}

	cWidgetImage* cGuiSet::CreateWidgetImage(const tString &asFile,
											const cVector3f &avLocalPos,
											const cVector2f &avSize,
											eGuiMaterial aMaterial,
											bool abAnimate,
											iWidget *apParent,
											const tString& asName)
	{
		cGuiGfxElement *pGfx = NULL;
		if(asFile != "")
		{
			if(abAnimate)
				pGfx = GetGui()->CreateGfxImageBuffer(asFile,aMaterial,true);
			else
				pGfx = GetGui()->CreateGfxImage(asFile,aMaterial);
		}

		cVector2f vSize = avSize;
		if(pGfx && avSize.x <0)
			vSize = pGfx->GetImageSize();

		return (cWidgetImage*)ohpl_gw_create(this, kOhplGwImage, &avLocalPos, &vSize, NULL, 0, (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0, pGfx);
	}

	cWidgetListBox* cGuiSet::CreateWidgetListBox(const cVector3f &avLocalPos,
												const cVector2f &avSize,
												iWidget *apParent,
												const tString& asName)
	{
		cWidgetListBox *pListBox = hplNew( cWidgetListBox,(this,GetSkin()) );
		pListBox->SetPosition(avLocalPos);
		pListBox->SetSize(avSize);
		pListBox->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pListBox, (uintptr_t)apParent);
		return pListBox;
	}

	cWidgetMultiPropertyListBox* cGuiSet::CreateWidgetMultiPropertyListBox(	const cVector3f& avLocalPos,
																		const cVector2f& avSize,
																		iWidget* apParent,
																		const tString& asName)
	{
		cWidgetMultiPropertyListBox* pListBox = hplNew( cWidgetMultiPropertyListBox, (this,GetSkin()));
		pListBox->SetPosition(avLocalPos);
		pListBox->SetSize(avSize);
		pListBox->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pListBox, (uintptr_t)apParent);
		return pListBox;
	}

	cWidgetComboBox* cGuiSet::CreateWidgetComboBox(	const cVector3f &avLocalPos,
													const cVector2f &avSize,
													const tWString &asText,
													iWidget *apParent,
													const tString& asName)
	{
		cWidgetComboBox *pComboBox = hplNew( cWidgetComboBox, (this,GetSkin()) );
		pComboBox->SetPosition(avLocalPos);
		pComboBox->SetSize(avSize);
		pComboBox->SetText(asText);
		pComboBox->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pComboBox, (uintptr_t)apParent);
		return pComboBox;
	}

	cWidgetMenuItem* cGuiSet::CreateWidgetMenuItem(	const cVector3f &avLocalPos,
													const cVector2f &avSize,
													const tWString &asText,
													iWidget *apParent,
													const tString& asName)
	{
		cWidgetMenuItem *pItem = hplNew( cWidgetMenuItem, ((iWidgetMenu*)apParent) );
		pItem->SetPosition(avLocalPos);
		pItem->SetSize(avSize);
		pItem->SetText(asText);
		pItem->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pItem, (uintptr_t)apParent);
		return pItem;
	}

	cWidgetContextMenu* cGuiSet::CreateWidgetContextMenu(	const cVector3f &avLocalPos,
													const cVector2f &avSize,
													const tWString &asText,
													iWidget *apParent,
													const tString& asName)
	{
		cWidgetContextMenu *pMenu = hplNew( cWidgetContextMenu, (this,GetSkin()) );
		pMenu->SetPosition(avLocalPos);
		pMenu->SetSize(avSize);
		pMenu->SetText(asText);
		pMenu->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pMenu, (uintptr_t)apParent);
		return pMenu;
	}

	cWidgetMainMenu* cGuiSet::CreateWidgetMainMenu(iWidget *apParent,
													const tString& asName)
	{
		cWidgetMainMenu *pMenu = hplNew( cWidgetMainMenu, (this,GetSkin()) );
		pMenu->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pMenu, (uintptr_t)apParent);
		return pMenu;
	}

	cWidgetTabLabel* cGuiSet::CreateWidgetTabLabel(	const cVector3f &avLocalPos,
													const cVector2f &avSize,
													const tWString &asText,
													iWidget *apParent,
													const tString& asName)
	{
		cWidgetTabLabel *pLabel = hplNew( cWidgetTabLabel, (this,GetSkin()) );
		pLabel->SetPosition(avLocalPos);
		pLabel->SetSize(avSize);
		pLabel->SetText(asText);
		pLabel->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pLabel, (uintptr_t)apParent);
		return pLabel;
	}

	cWidgetTab* cGuiSet::CreateWidgetTab(	const cVector3f &avLocalPos,
													const cVector2f &avSize,
													const tWString &asText,
													iWidget *apParent,
													const tString& asName)
	{
		cWidgetTab *pTab = hplNew( cWidgetTab, ((cWidgetTabFrame*)apParent) );
        pTab->SetPosition(avLocalPos);
		pTab->SetSize(avSize);
		pTab->SetText(asText);
		pTab->SetName(asName);

		ohpl_g1_set_add_widget(this, (uintptr_t)pTab, (uintptr_t)apParent);
		return pTab;
	}

	cWidgetTabFrame* cGuiSet::CreateWidgetTabFrame(	const cVector3f &avLocalPos,
													const cVector2f &avSize,
													const tWString &asText,
													iWidget *apParent,
													bool abAllowHScroll,
													bool abAllowVScroll,
													const tString& asName)
	{
		cWidgetTabFrame *pFrame = hplNew( cWidgetTabFrame, (this,GetSkin()) );
        pFrame->SetPosition(avLocalPos);
		pFrame->SetSize(avSize);
		pFrame->SetText(asText);
		pFrame->SetName(asName);
		pFrame->SetHorizontalScrollEnabled(abAllowHScroll);
		pFrame->SetVerticalScrollEnabled(abAllowVScroll);
		ohpl_g1_set_add_widget(this, (uintptr_t)pFrame, (uintptr_t)apParent);
		return pFrame;
	}

	//-----------------------------------------------------------------------

	cGuiPopUpUIKeyboard* cGuiSet::CreatePopUpUIKeyboard(cWidgetTextBox* apTarget)
	{
		cGuiSet *pSet = apTarget->GetSet();
		return static_cast<cGuiPopUpUIKeyboard*>(ohpl_g2_popup_kb(pSet, pSet->GetSkin(), (uintptr_t)apTarget));
	}

	//-----------------------------------------------------------------------



	void cGuiSet::ShowContextMenu( cWidgetContextMenu* apMenu, const cVector3f& avPosition )
	{
		ohpl_gw_show_context_menu(this, (uintptr_t)apMenu, avPosition.x, avPosition.y, avPosition.z);
	}

	void cGuiSet::SetRootWidgetClips(bool abX)
	{
		iWidget *pRoot = (iWidget*)ohpl_g1_set_root(this);
		pRoot->SetClipActive(abX);
		if(abX)
			pRoot->SetSize(GetVirtualSize());
		else
			pRoot->SetSize(0);
	}

	bool cGuiSet::GetRootWidgetClips()
	{
		return ((iWidget*)ohpl_g1_set_root(this))->GetClipActive();
	}

	bool cGuiSet::HasFocus()
	{
		return GetGui()->GetFocusedSet() == this;
	}

	void cGuiSet::PositionWidgetInsideBounds(iWidget* apWidget)
	{
		cVector3f vPos = apWidget->GetGlobalPosition();
		const cVector2f& vSize = apWidget->GetSize();
		const cVector2f& vSetSize = GetVirtualSize();
		ohpl_gui_position_inside_bounds(&vPos, &vSize, &vSetSize, &vPos);
		apWidget->SetGlobalPosition(vPos);
	}
}
