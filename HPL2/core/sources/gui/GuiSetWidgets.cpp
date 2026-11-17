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

	cWidgetWindow* cGuiSet::CreateWidgetWindow(	tWidgetWindowButtonFlag alFlags,
												const cVector3f &avLocalPos,
												const cVector2f &avSize,
												const tWString &asText,
												iWidget *apParent,
												const tString& asName)
	{
		cWidgetWindow *pWindow = hplNew( cWidgetWindow, (this,GetSkin(), alFlags) );
		pWindow->SetPosition(avLocalPos);
		pWindow->SetSize(avSize);
		pWindow->SetText(asText);
		pWindow->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pWindow, (uintptr_t)apParent);

		ohpl_g1_set_add_window(this, (uintptr_t)pWindow);

		return pWindow;
	}

	cWidgetFrame* cGuiSet::CreateWidgetFrame(	const cVector3f &avLocalPos,
												const cVector2f &avSize,
												bool abDrawFrame,
												iWidget *apParent,
												bool abHScrollBar, bool abVScrollBar,
												const tString& asName)
	{
		cWidgetFrame *pFrame = hplNew( cWidgetFrame, (this,GetSkin(), abHScrollBar, abVScrollBar) );
		pFrame->SetPosition(avLocalPos);
		pFrame->SetSize(avSize);
		pFrame->SetDrawFrame(abDrawFrame);
		pFrame->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pFrame, (uintptr_t)apParent);
		return pFrame;
	}

	cWidgetButton* cGuiSet::CreateWidgetButton(	const cVector3f &avLocalPos,
												const cVector2f &avSize,
												const tWString &asText,
												iWidget *apParent,
												bool abToggleable,
												const tString& asName)
	{
		cWidgetButton *pButton = hplNew( cWidgetButton, (this,GetSkin()) );
		pButton->SetPosition(avLocalPos);
		pButton->SetSize(avSize);
		pButton->SetText(asText);
		pButton->SetName(asName);
		pButton->SetToggleable(abToggleable);
		ohpl_g1_set_add_widget(this, (uintptr_t)pButton, (uintptr_t)apParent);
		return pButton;
	}

	cWidgetLabel* cGuiSet::CreateWidgetLabel(	const cVector3f &avLocalPos,
												const cVector2f &avSize,
												const tWString &asText,
												iWidget *apParent,
												const tString& asName)
	{
		cWidgetLabel *pLabel = hplNew( cWidgetLabel, (this,GetSkin()) );
		pLabel->SetPosition(avLocalPos);
		pLabel->SetSize(avSize);
		pLabel->SetText(asText);
		pLabel->SetName(asName);

		if(avSize == -1)
		{
			pLabel->SetAutogenerateSize(true);
			pLabel->SetAutogenerateSize(false);
		}

		ohpl_g1_set_add_widget(this, (uintptr_t)pLabel, (uintptr_t)apParent);
		return pLabel;
	}

	cWidgetSlider* cGuiSet::CreateWidgetSlider(	eWidgetSliderOrientation aOrientation,
												const cVector3f &avLocalPos,
												const cVector2f &avSize,
												int alMaxValue,
                                                iWidget *apParent,
												const tString& asName)
	{
		cWidgetSlider *pSlider = hplNew( cWidgetSlider, (this,GetSkin(),aOrientation) );
		pSlider->SetPosition(avLocalPos);
		pSlider->SetSize(avSize);
		pSlider->SetMaxValue(alMaxValue);
		pSlider->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pSlider, (uintptr_t)apParent);
		return pSlider;
	}

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

	cWidgetCheckBox* cGuiSet::CreateWidgetCheckBox(	const cVector3f &avLocalPos,
													const cVector2f &avSize,
													const tWString &asText,
													iWidget *apParent,
													const tString& asName)
	{
		cWidgetCheckBox *pCheckBox = hplNew( cWidgetCheckBox, (this,GetSkin()) );
		pCheckBox->SetPosition(avLocalPos);
		pCheckBox->SetSize(avSize);
		pCheckBox->SetText(asText);
		pCheckBox->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pCheckBox, (uintptr_t)apParent);
		return pCheckBox;
	}

	cWidgetImage* cGuiSet::CreateWidgetImage(const tString &asFile,
											const cVector3f &avLocalPos,
											const cVector2f &avSize,
											eGuiMaterial aMaterial,
											bool abAnimate,
											iWidget *apParent,
											const tString& asName)
	{
		cWidgetImage *pImage = hplNew( cWidgetImage, (this,GetSkin()) );
		cGuiGfxElement *pGfx = NULL;
		if(asFile != "")
		{
			if(abAnimate)
			{
				pGfx = GetGui()->CreateGfxImageBuffer(asFile,aMaterial,true);
			}
			else
			{
				pGfx = GetGui()->CreateGfxImage(asFile,aMaterial);
			}
		}
		pImage->SetPosition(avLocalPos);

		if(pGfx && avSize.x <0)
		{
			pImage->SetSize(pGfx->GetImageSize());
		}
		else
		{
			pImage->SetSize(avSize);
		}

		pImage->SetImage(pGfx);

		pImage->SetName(asName);

		ohpl_g1_set_add_widget(this, (uintptr_t)pImage, (uintptr_t)apParent);
		return pImage;
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

	cWidgetGroup* cGuiSet::CreateWidgetGroup(const cVector3f &avLocalPos,
												const cVector2f &avSize,
												const tWString &asText,
												iWidget *apParent,
												const tString& asName)
	{
		cWidgetGroup* pGroup = hplNew( cWidgetGroup, (this,GetSkin()) );
		pGroup->SetPosition(avLocalPos);
		pGroup->SetSize(avSize);
		pGroup->SetText(asText);
		pGroup->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pGroup, (uintptr_t)apParent);
		return pGroup;
	}

	cWidgetDummy* cGuiSet::CreateWidgetDummy(const cVector3f &avLocalPos,
												iWidget* apParent,
												const tString &asName)
	{
		cWidgetDummy* pDummy = hplNew( cWidgetDummy, (this,GetSkin()) );
		pDummy->SetPosition(avLocalPos);
		pDummy->SetName(asName);
		ohpl_g1_set_add_widget(this, (uintptr_t)pDummy, (uintptr_t)apParent);
		return pDummy;
	}

	//-----------------------------------------------------------------------

	cGuiPopUpMessageBox* cGuiSet::CreatePopUpMessageBox(	const tWString& asLabel, const tWString& asText,
								const tWString& asButton1, const tWString& asButton2,
								void *apCallbackObject, tGuiCallbackFunc apCallback)
	{
		cGuiPopUpMessageBox* pMessageBox = hplNew( cGuiPopUpMessageBox, (this, asLabel,asText,
																asButton1,asButton2,
																apCallbackObject,apCallback) );

		return pMessageBox;
	}

	//-----------------------------------------------------------------------

	cGuiPopUpFilePicker* cGuiSet::CreatePopUpSaveFilePicker( tWString &asFileName, const tWString &asCategory,
															 const tWString &asFilter, const tWString &asStartPath, bool abShowHidden,
															void *apCallbackObject, tGuiCallbackFunc apCallback, const tWString& asStartFilename)
	{
		cGuiPopUpFilePicker* pPicker = hplNew( cGuiPopUpFilePicker, (this, GetSkin(), eFilePickerType_Save, asStartPath, abShowHidden, apCallbackObject, apCallback, asStartFilename) );
		pPicker->Init();
		pPicker->SetSaveFileDest( asFileName );

		pPicker->AddCategory( asCategory, asFilter );

		return pPicker;
	}

	//-----------------------------------------------------------------------

	cGuiPopUpFilePicker* cGuiSet::CreatePopUpLoadFilePicker( tWStringVec &avFileList, bool abAddAllFilesFilter,
															 const tWString &asStartPath, bool abShowHidden,
															 void *apCallbackObject, tGuiCallbackFunc apCallback)
	{
		cGuiPopUpFilePicker* pPicker = hplNew( cGuiPopUpFilePicker, (this, GetSkin(), eFilePickerType_Load, asStartPath, abShowHidden, apCallbackObject, apCallback) );
		pPicker->Init();
		pPicker->SetLoadFileListDest( avFileList );

		if(abAddAllFilesFilter)
			pPicker->AddCategory(_W("All files"), _W("*.*"));

		return pPicker;
	}

	//-----------------------------------------------------------------------

	cGuiPopUpColorPicker* cGuiSet::CreatePopUpColorPicker( cColor* apDestColor, const cVector3f& avPos,  void *apCallbackObject, tGuiCallbackFunc apCallback)
	{
		cGuiPopUpColorPicker* pPicker = hplNew( cGuiPopUpColorPicker,(this, GetSkin(), avPos, apDestColor, apCallbackObject, apCallback));

		return pPicker;
	}

	//-----------------------------------------------------------------------

	cGuiPopUpUIKeyboard* cGuiSet::CreatePopUpUIKeyboard(cWidgetTextBox* apTarget)
	{
		cGuiPopUpUIKeyboard* pKB = hplNew(cGuiPopUpUIKeyboard,(apTarget, NULL, NULL));

		return pKB;
	}

	//-----------------------------------------------------------------------



	void cGuiSet::ShowContextMenu( cWidgetContextMenu* apMenu, const cVector3f& avPosition )
	{
		if(apMenu==NULL) return;

		cVector3f vPos = avPosition;
		if(avPosition.x + apMenu->GetSize().x > GetVirtualSize().x)
			vPos.x = GetVirtualSize().x - apMenu->GetSize().x;
		if(avPosition.y + apMenu->GetSize().y > GetVirtualSize().y)
			vPos.y = GetVirtualSize().y - apMenu->GetSize().y;

		apMenu->SetVisible(true);
		apMenu->SetEnabled(true);
		apMenu->SetHighlightedItem(NULL);

		if(	apMenu->GetParentItem()==NULL ||
			apMenu->GetParentMenu()->GetType()==eWidgetType_MainMenu)
		{
			vPos.z = ohpl_g1_set_context_menu_z(this);
			IncContextMenuZ();
		}
		else
			vPos.z = apMenu->GetParentMenu()->GetGlobalPosition().z;

		SetAttentionWidget(apMenu->GetTopMostMenu());
		apMenu->SetGlobalPosition(vPos);
		SetFocusedWidget(apMenu);
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
