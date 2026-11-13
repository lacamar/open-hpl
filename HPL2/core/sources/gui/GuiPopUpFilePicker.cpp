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

#include "gui/GuiPopUpFilePicker.h"

#include "gui/GuiTypes.h"

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"

#include "math/Math.h"

#include "resources/Resources.h"

#include "graphics/FontData.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"

#include "gui/WidgetTextBox.h"
#include "gui/WidgetMultiPropertyListBox.h"
#include "gui/WidgetWindow.h"
#include "gui/WidgetButton.h"
#include "gui/WidgetComboBox.h"

#include "gui/RustWidget3.h"

namespace hpl {

	/////////////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

	cGuiPopUpFilePicker::cGuiPopUpFilePicker(cGuiSet* apSet, cGuiSkin* apSkin,
											 eFilePickerType aType, 
											 const tWString &asStartPath, bool abShowHiddenFiles,
											 void *apCallbackObject, tGuiCallbackFunc apCallback, 
											 const tWString& asStartFilename) : iGuiPopUp(apSet,false,cVector2f(600,500)), 
																				iFileBrowser(asStartPath, abShowHiddenFiles)
	{
		mpSkin = apSkin;

		mPickerType = aType;

		mpCallbackObject = apCallbackObject;
		mpCallback = apCallback;

		size_t lMaxLen = asStartFilename.size() > asStartPath.size() ? asStartFilename.size() : asStartPath.size();
		msStartFilename.assign(lMaxLen, _W('\0'));
		msStartFilename.resize(ohpl_w3_fp_start_filename(asStartFilename.data(), asStartFilename.size(),
														 asStartPath.data(), asStartPath.size(),
														 lMaxLen ? &msStartFilename[0] : NULL, lMaxLen));

		mpSaveFileDest = NULL;
		mpLoadFileListDest = NULL;
		for(int i=0;i<3;++i) mvButtons[i] = NULL;
		mpCurrentDirectory = NULL;
		mpFileList = NULL;
		mpCurrentFileName = NULL;
		mpFilterList = NULL;
		mpGfxUpButton = NULL;
		for(int i=0;i<6;++i) mvGfxFileTypeIcons[i] = NULL;

		if(OhplBackendRustRequested())
		{
			OhplH21Host host = H21Host();
			mR = ohpl_h21_fp_new(&host, (int32_t)aType, reinterpret_cast<const uint32_t*>(asStartPath.data()), asStartPath.size(),
								 reinterpret_cast<const uint32_t*>(asStartFilename.data()), asStartFilename.size(),
								 (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback));
			mpWindow = (cWidgetWindow*)ohpl_h21_window(mR);
		}
	}

    //-------------------------------------------------------------------------------

	cGuiPopUpFilePicker::~cGuiPopUpFilePicker()
	{
		if(mR)
		{
			OhplH21Host host = H21Host();
			ohpl_h21_destroy(&host, mR);
			mR = NULL;
			return;
		}
		for(int i=0;i<3;++i) if(mvButtons[i]) mpSet->DestroyWidget(mvButtons[i]);
		if(mpCurrentDirectory) mpSet->DestroyWidget(mpCurrentDirectory);
		if(mpCurrentFileName) mpSet->DestroyWidget(mpCurrentFileName);
		if(mpFileList) mpSet->DestroyWidget(mpFileList);
		if(mpFilterList) mpSet->DestroyWidget(mpFilterList);
	}

	//-------------------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

	void cGuiPopUpFilePicker::SetSaveFileDest( tWString &asFileName )
	{
		mpSaveFileDest = &asFileName;
		if(mR)
		{
			OhplH21Host host = H21Host();
			ohpl_h21_fp_set_save_dest(&host, mR, reinterpret_cast<const uint32_t*>(asFileName.data()), asFileName.size());
			return;
		}

		mpCurrentFileName->SetText(cString::GetFileNameW(asFileName));
	}

	//-------------------------------------------------------------------------------

	void cGuiPopUpFilePicker::SetLoadFileListDest( tWStringVec &asFileList )
	{
		mpLoadFileListDest = &asFileList;
	}

	//-------------------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

	bool cGuiPopUpFilePicker::Button_Pressed(iWidget* apWidget, const cGuiMessageData& aData)
	{
		/////////////////////////////
		// Go Up one folder button
		if(apWidget == mvButtons[0])
		{
			NavigateUp();			
		}
		/////////////////////////////
		// Save/Load button
		else if(apWidget == mvButtons[1] ||
				apWidget == mpCurrentFileName)
		{
			if(ohpl_w3_fp_accept(mPickerType, mpCurrentFileName->GetText() == _W(""), false)==kOhplW3FpErrorEmpty)
				mpSet->CreatePopUpMessageBox(	_W("Error"),
												_W("File name must not be empty"),
												_W("OK"),
												_W(""),
												NULL,NULL);
			else
			{
				tWString sFilenameFullPath = GetCurrentFullPath() + mpCurrentFileName->GetText();
				bool bFileExists = cPlatform::FileExists(sFilenameFullPath);

				if(mPickerType==eFilePickerType_Save)
				{
					bool bHasFilter = mvCategories.empty()==false && mvCategories[0]->mvFilters.empty()==false;
					tWString sFilter = bHasFilter ? mvCategories[0]->mvFilters[0] : tWString();
					bool bAppend;
					size_t lExtLen = ohpl_w3_fp_save_extension(sFilenameFullPath.data(), sFilenameFullPath.size(), bHasFilter,
															   sFilter.data(), sFilter.size(), NULL, 0, &bAppend);
					if(bAppend)
					{
						tWString sExt(lExtLen, _W('\0'));
						if(lExtLen) ohpl_w3_fp_save_extension(sFilenameFullPath.data(), sFilenameFullPath.size(), bHasFilter,
															  sFilter.data(), sFilter.size(), &sExt[0], lExtLen, &bAppend);
						mpCurrentFileName->SetText(mpCurrentFileName->GetText() + sExt);
						sFilenameFullPath += sExt;

						bFileExists = bFileExists || cPlatform::FileExists(sFilenameFullPath);
					}
				}

				switch(ohpl_w3_fp_accept(mPickerType, false, bFileExists))
				{
				case kOhplW3FpSaveConfirm:
					mpSet->CreatePopUpMessageBox(	_W("Warning"),
													_W("File already exists. Overwrite?"),
													_W("Yes"), _W("No"),
													this, kGuiCallback(MessageBoxReturnCallback));
					break;
				case kOhplW3FpSaveDone:
					BuildSaveFileName();
					ClosePopUp(true);
					break;
				case kOhplW3FpLoadMissing:
					mpSet->CreatePopUpMessageBox(	_W("Error"),
													_W("File does not exist. Please choose a valid file."),
													_W("OK"), _W(""),
													NULL, NULL);
					break;
				case kOhplW3FpLoadDone:
					BuildLoadFileList();
					ClosePopUp(true);
					break;
				default:
					break;
				}
			}
		}
		/////////////////////////////
		// Cancel button
		else if(apWidget == mvButtons[2])
		{
			ClosePopUp(false);
		}
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpFilePicker, Button_Pressed);

	//-------------------------------------------------------------------------

	bool cGuiPopUpFilePicker::MessageBoxReturnCallback( iWidget* apWidget, const cGuiMessageData& aData)
	{
		/////////////////////////
		// If button pressed is 0 (Yes)
		if(ohpl_w3_fp_overwrite_confirmed(mPickerType, aData.mlVal))
		{
			BuildSaveFileName();
			ClosePopUp(true);
		}
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpFilePicker, MessageBoxReturnCallback);

	//-------------------------------------------------------------------------

	bool cGuiPopUpFilePicker::Directory_OnSelectionChange(iWidget* apWidget, const cGuiMessageData& aData)
	{
		cWidgetComboBox* pBox = (cWidgetComboBox*)apWidget;
		int lNumItems = pBox->GetItemNum();
		int lSelection = pBox->GetSelectedItem();

		tWString sDir;
		
		////////////////////////////////////////
        // If the selection is a system item (e.g. begins with "<") handle it differently
		OhplW3DirSelect sel = ohpl_w3_fp_dir_select(lSelection, lNumItems, mvCurrentDirFullPath.size());
        if(sel.action==kOhplW3DirClear) {
            mvCurrentDirFullPath.clear();
            NavigateTo(pBox->GetItemText(lSelection));
        }
        // If selection is not last item, remove items from selection onwards, then navigate to selection
        else if(sel.action==kOhplW3DirPop)
		{
			sDir = pBox->GetItemText(lSelection);

			for(size_t i=0; i<sel.pops; ++i)
			{
				mvCurrentDirFullPath.pop_back();
			}

			NavigateTo(sDir);
		}
		
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpFilePicker, Directory_OnSelectionChange);

	//-------------------------------------------------------------------------

	bool cGuiPopUpFilePicker::FileList_OnSelectionDblClick(iWidget* apWidget, const cGuiMessageData& aData)
	{
		/////////////////////////////////////
		// Get path item which got the double click
		cWidgetItem* pItem = (cWidgetItem*)mpFileList->GetItem(mpFileList->GetSelectedItem());
		if(pItem==NULL || pItem->GetProperty(1)==NULL) return true;
		tWString sPath = pItem->GetProperty(1)->GetText();

		// Try to navigate to path (if folder)
		if(NavigateTo(sPath)==false)
		{
			// Simulate Load/Save Button press
			Button_Pressed(mvButtons[1], cGuiMessageData());
		}

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpFilePicker, FileList_OnSelectionDblClick);

	//-------------------------------------------------------------------------

	bool cGuiPopUpFilePicker::FileList_OnSelectionChange(iWidget* apWidget, const cGuiMessageData& aData)
	{
		int lFolder = mpCurrentDirectory->GetSelectedItem();

		///////////////////////////////////////
		// If checking special folders, do nothing
		if(lFolder == 0) return true;

		///////////////////////////////////////
		// Else, check if selected item is a folder, if not, add file name to current filename textbox
		int lSelection = mpFileList->GetSelectedItem();

		tWString sPath = _W("");

		if(lSelection != -1) 
		{
			cWidgetItem* pItem = (cWidgetItem*)mpFileList->GetItem(lSelection);
			
			if(pItem)
				// Property 1 : file name
				sPath = pItem->GetProperty(1)->GetText();
		}

		if(sPath != _W("") && cPlatform::FolderExists(GetCurrentFullPath() + sPath)==false)
			mpCurrentFileName->SetText( sPath );

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpFilePicker, FileList_OnSelectionChange);

	//-------------------------------------------------------------------------

	bool cGuiPopUpFilePicker::FilterList_OnSelectionChange(iWidget* apWidget, const cGuiMessageData& aData)
	{
		////////////////////////////
		// If filter has changed, repopulate
		PopulateFileList();
		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cGuiPopUpFilePicker, FilterList_OnSelectionChange);

	//-------------------------------------------------------------------------

	void cGuiPopUpFilePicker::BuildSaveFileName()
	{
        *mpSaveFileDest = GetCurrentFullPath() + mpCurrentFileName->GetText();
	}

	//-------------------------------------------------------------------------

	void cGuiPopUpFilePicker::BuildLoadFileList()
	{
		// TODO : Build load file list
		mpLoadFileListDest->clear();

		mpLoadFileListDest->push_back(GetCurrentFullPath() + mpCurrentFileName->GetText());
	}

	//-------------------------------------------------------------------------

    /**	This method clears the current filter combobox,
	 *	and then repopulates it with the new filter values:
	 *	it adds an item per category, and writes the filters on the item text
	 */
	void cGuiPopUpFilePicker::OnAddFilter()
	{
		if(mR)
		{
			OhplH21Host host = H21Host();
			ohpl_h21_fp_on_add_filter(&host, mR);
			return;
		}
		////////////////////////////////////////////////
		// This method clears the current filter combobox,
		// and then repopulates it with the new filter values:
		// it adds an item per category, and writes the filters on the item text
		mpFilterList->ClearItems();
		
		for(int i=0;i<(int)mvCategories.size();++i)
		{
			cFileBrowserCategory* pCat = mvCategories[i];

			std::vector<const wchar_t*> vFilters;
			std::vector<size_t> vLens;
			for(int j=0;j <(int)pCat->mvFilters.size();++j)
			{
				vFilters.push_back(pCat->mvFilters[j].data());
				vLens.push_back(pCat->mvFilters[j].size());
			}
			size_t lLen = ohpl_w3_fp_filter_text(pCat->msName.data(), pCat->msName.size(), vFilters.data(), vLens.data(),
												 vFilters.size(), NULL, 0);
			tWString sFilterText(lLen, _W('\0'));
			if(lLen) ohpl_w3_fp_filter_text(pCat->msName.data(), pCat->msName.size(), vFilters.data(), vLens.data(),
											vFilters.size(), &sFilterText[0], lLen);

			mpFilterList->AddItem(sFilterText);
		}

		mpFilterList->SetSelectedItem(0);
	}

	//-------------------------------------------------------------------------

	void cGuiPopUpFilePicker::OnNavigate()
	{
		if(mR)
		{
			OhplH21Host host = H21Host();
			ohpl_h21_fp_on_navigate(&host, mR);
			return;
		}
		mpCurrentDirectory->ClearItems();
		std::vector<int32_t> vEntries(ohpl_w3_fp_dir_entries(iFileBrowser::msPersonalDir.length()!=0, iFileBrowser::msGameDir.length()!=0,
															 mvCurrentDirFullPath.size(), NULL, 0));
		ohpl_w3_fp_dir_entries(iFileBrowser::msPersonalDir.length()!=0, iFileBrowser::msGameDir.length()!=0,
							   mvCurrentDirFullPath.size(), vEntries.data(), vEntries.size());
		for(size_t i=0; i<vEntries.size(); ++i)
		{
			switch(vEntries[i])
			{
			case kOhplW3DirRoot: mpCurrentDirectory->AddItem(_W("<System Root>")); break;
			case kOhplW3DirPersonal: mpCurrentDirectory->AddItem(_W("<Personal Folder>")); break;
			case kOhplW3DirGame: mpCurrentDirectory->AddItem(_W("<Game Folder>")); break;
			default: mpCurrentDirectory->AddItem(mvCurrentDirFullPath[vEntries[i]]); break;
			}
		}
		mpCurrentDirectory->SetText(GetCurrentFullPath());

		PopulateFileList();
	}

	//-------------------------------------------------------------------------

	/** This method populates the File list according to the current directory and
	 *	selected filters. If current directory is empty, populate the list with
	 *	special folders (Windows only)
	 */
	void cGuiPopUpFilePicker::PopulateFileList()
	{
		int lSelectedCategoryIndex = mpFilterList->GetSelectedItem();

		if(lSelectedCategoryIndex<0) return;

		//////////////////////////////////////
		// Reset Items
		mpFileList->ClearItems();

		tWString sCurrentFullPath = GetCurrentFullPath();
		///////////////////////////////////////
		// If we are at the root, set up special folders
		if(sCurrentFullPath==_W(""))
		{
			for(int i=0;i<(int)mvSystemRootFolders.size();++i)
			{
				cWidgetItem* pItem = mpFileList->AddItem();

				pItem->AddProperty(NULL);
				pItem->AddProperty(mvSystemRootFolders[i]);
				pItem->AddProperty(_W(""));
				pItem->AddProperty(_W(""));
			}
			return;
		}

		///////////////////////////////////////
		// Else, scan current directory
		tWStringList lstFilesAndFolders;
		GetFilesAndFoldersInCurrentPath(lSelectedCategoryIndex, lstFilesAndFolders);
		tWStringListIt it = lstFilesAndFolders.begin();

		////////////////////////////////////////
		// Add them as items to the file list
		for(;it!=lstFilesAndFolders.end(); ++it)
		{
			tWString sFilename = *it;

			cWidgetItem* pItem = mpFileList->AddItem();
            
			cGuiGfxElement* pGfx = mvGfxFileTypeIcons[GetFileTypeByName(sFilename)];
			tWString sFileDate = cString::To16Char(cPlatform::FileModifiedDate(sCurrentFullPath + sFilename).ToString());
			tWString sFileSize = GetHumanReadableSize(cPlatform::GetFileSize(sCurrentFullPath + sFilename));

			pItem->AddProperty(pGfx);
			pItem->AddProperty(sFilename);
			pItem->AddProperty(sFileSize);
			pItem->AddProperty(sFileDate);
		}
		
	}


	//-------------------------------------------------------------------------
	
	/** Builds the window and sets up stuff
	 *
	 */
	void cGuiPopUpFilePicker::Init()
	{
		if(mR)
		{
			OhplH21Host host = H21Host();
			ohpl_h21_fp_init(&host, mR);
			return;
		}
		tWString sCaption;
		sCaption.resize(ohpl_w3_fp_caption(mPickerType, NULL, 0));
		ohpl_w3_fp_caption(mPickerType, &sCaption[0], sCaption.size());

		OhplW3FpLayout layout;
		ohpl_w3_fp_layout(mpWindow->GetDefaultFontSize().y, &layout);

		////////////////////////////////
		// Set up graphics
		mpGfxUpButton = mpSkin->GetGfx(eGuiSkinGfx_FilePickerUpButton);

		for(int i=0; i<6; ++i)
		{
			mvGfxFileTypeIcons[i] = mpSkin->GetGfx((eGuiSkinGfx) (i + eGuiSkinGfx_FilePickerUpButton+1));
		}


		/////////////////////////////////
		// Init widgets
		
		// Main window
		mpWindow->SetText(sCaption);
		
		// Current directory TextBox
		mpCurrentDirectory = mpSet->CreateWidgetComboBox(layout.dir_pos,
														layout.dir_size,
														_W(""), 
														mpWindow);
		mpCurrentDirectory->SetCanEdit(false);
		mpCurrentDirectory->AddCallback(eGuiMessage_SelectionChange,this, kGuiCallback(Directory_OnSelectionChange));

		// Buttons
		for(int i=0; i<3; ++i)
		{
			mvButtons[i] = mpSet->CreateWidgetButton(	0,
														0,
														_W(""), 
														mpWindow);

			mvButtons[i]->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(Button_Pressed));
		}

        if(mpGfxUpButton) mvButtons[0]->SetImage( mpGfxUpButton, false);
		mvButtons[0]->SetPosition(layout.button_pos[0]);
		mvButtons[0]->SetSize(layout.button_size[0]);
		
		mvButtons[1]->SetText(sCaption);
		mvButtons[1]->SetPosition(layout.button_pos[1]);
		mvButtons[1]->SetSize(layout.button_size[1]);
		
		mvButtons[2]->SetText(_W("Cancel"));
		mvButtons[2]->SetPosition(layout.button_pos[2]);
		mvButtons[2]->SetSize(layout.button_size[2]);

		// Current file name TextBox
		mpCurrentFileName = mpSet->CreateWidgetTextBox(	layout.name_pos,
														layout.name_size,
														msStartFilename, 
														mpWindow);
		mpCurrentFileName->SetForceCallBackOnEnter(true);
		mpCurrentFileName->SetCallbackOnLostFocus(false);
		mpCurrentFileName->AddCallback(eGuiMessage_TextBoxEnter, this, kGuiCallback(Button_Pressed));

		mpSet->SetFocusedWidget(mpCurrentFileName);
		mpCurrentFileName->SetSelectedText();
		
		// Filters ComboBox
		mpFilterList = mpSet->CreateWidgetComboBox(	layout.filter_pos,
													layout.filter_size,
													_W(""), 
													mpWindow);
		mpFilterList->AddCallback(eGuiMessage_SelectionChange, this, kGuiCallback(FilterList_OnSelectionChange));

		// File ListBox        
		mpFileList = mpSet->CreateWidgetMultiPropertyListBox(layout.list_pos,
															 layout.list_size,
															 mpWindow);
		mpFileList->AddColumn("",0);
		mpFileList->SetColumnWidth(0,layout.column_width[0]);
		mpFileList->AddColumn("Name",1);
		mpFileList->SetColumnWidth(1,layout.column_width[1]);
		mpFileList->AddColumn("Size",2, eFontAlign_Right);
		mpFileList->SetColumnWidth(2,layout.column_width[2]);
		mpFileList->AddColumn("Date modified",3);
		mpFileList->SetColumnWidth(3,layout.column_width[3]);

		mpFileList->AddCallback(eGuiMessage_SelectionChange, this, kGuiCallback(FileList_OnSelectionChange));
		mpFileList->AddCallback(eGuiMessage_SelectionDoubleClick, this, kGuiCallback(FileList_OnSelectionDblClick));

		InitBrowser();
	}

	//-------------------------------------------------------------------------

	bool cGuiPopUpFilePicker::H21Ext(const OhplH21Cmd &c, OhplH21Ret &r)
	{
		tWString sPath;
		switch(c.op)
		{
		case kOhplH21FpLoadGfx:
			mpGfxUpButton = mpSkin->GetGfx(eGuiSkinGfx_FilePickerUpButton);
			for(int i=0; i<6; ++i)
				mvGfxFileTypeIcons[i] = mpSkin->GetGfx((eGuiSkinGfx) (i + eGuiSkinGfx_FilePickerUpButton+1));
			return true;
		case kOhplH21BtnUpImage:
			if(mpGfxUpButton) static_cast<cWidgetButton*>((iWidget*)c.w[0])->SetImage(mpGfxUpButton, false);
			return true;
		case kOhplH21MlAddRow:
		{
			cWidgetMultiPropertyListBox *pList = static_cast<cWidgetMultiPropertyListBox*>((iWidget*)c.w[0]);
			cWidgetItem *pItem = pList->AddItem();
			if(c.i[0] == 0)
			{
				pItem->AddProperty((const cGuiGfxElement*)NULL);
				pItem->AddProperty(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0]));
				pItem->AddProperty(_W(""));
				pItem->AddProperty(_W(""));
			}
			else
			{
				pItem->AddProperty(mvGfxFileTypeIcons[c.i[1]]);
				for(int i=0; i<3; ++i) pItem->AddProperty(tWString(reinterpret_cast<const wchar_t*>(c.s[i]), c.n[i]));
			}
			return true;
		}
		case kOhplH21FbNavUp: NavigateUp(); return true;
		case kOhplH21FbNavTo: r.i = NavigateTo(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0])); return true;
		case kOhplH21FbFullPath: H21Str(r, 0, GetCurrentFullPath()); return true;
		case kOhplH21FbPathCount: r.i = (int)mvCurrentDirFullPath.size(); return true;
		case kOhplH21FbPathPop:
			for(int i=0; i<c.i[0] && !mvCurrentDirFullPath.empty(); ++i) mvCurrentDirFullPath.pop_back();
			return true;
		case kOhplH21FbPathClear: mvCurrentDirFullPath.clear(); return true;
		case kOhplH21FbPathEntry: H21Str(r, 0, mvCurrentDirFullPath[c.i[0]]); return true;
		case kOhplH21FbDirs: r.i = (iFileBrowser::msPersonalDir.length()!=0 ? 1 : 0) | (iFileBrowser::msGameDir.length()!=0 ? 2 : 0); return true;
		case kOhplH21FbCatCount: r.i = (int)mvCategories.size(); return true;
		case kOhplH21FbCatName: H21Str(r, 0, mvCategories[c.i[0]]->msName); return true;
		case kOhplH21FbCatFilterCount: r.i = (int)mvCategories[c.i[0]]->mvFilters.size(); return true;
		case kOhplH21FbCatFilter: H21Str(r, 0, mvCategories[c.i[0]]->mvFilters[c.i[1]]); return true;
		case kOhplH21FbInit: InitBrowser(); return true;
		case kOhplH21FbRootCount: r.i = (int)mvSystemRootFolders.size(); return true;
		case kOhplH21FbRoot: H21Str(r, 0, mvSystemRootFolders[c.i[0]]); return true;
		case kOhplH21FbList:
		{
			tWStringList lst;
			GetFilesAndFoldersInCurrentPath(c.i[0], lst);
			mvH21Files.assign(lst.begin(), lst.end());
			r.i = (int)mvH21Files.size();
			return true;
		}
		case kOhplH21FbRow:
		{
			tWString sFilename = mvH21Files[c.i[0]];
			tWString sFull = GetCurrentFullPath();
			r.i = (int)GetFileTypeByName(sFilename);
			H21Str(r, 0, sFilename);
			H21Str(r, 1, GetHumanReadableSize(cPlatform::GetFileSize(sFull + sFilename)));
			H21Str(r, 2, cString::To16Char(cPlatform::FileModifiedDate(sFull + sFilename).ToString()));
			return true;
		}
		case kOhplH21FileExists: r.i = cPlatform::FileExists(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0])); return true;
		case kOhplH21FolderExists: r.i = cPlatform::FolderExists(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0])); return true;
		case kOhplH21SaveDest:
			if(mpSaveFileDest) *mpSaveFileDest = tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0]);
			return true;
		case kOhplH21LoadDest:
			if(mpLoadFileListDest)
			{
				mpLoadFileListDest->clear();
				mpLoadFileListDest->push_back(tWString(reinterpret_cast<const wchar_t*>(c.s[0]), c.n[0]));
			}
			return true;
		default:
			break;
		}
		return false;
	}

	/////////////////////////////////////////////////////////////////////////////////

	void cGuiPopUpFilePicker::ClosePopUp(bool abPickedFile)
	{
		if(abPickedFile)
			mpSet->GetResources()->AddResourceDir(GetCurrentFullPath(), false);

		int lIntMessageData = (int)abPickedFile;
		RunCallback(mpCallbackObject, mpCallback, NULL, cGuiMessageData(lIntMessageData), true);

		SelfDestruct();
	}
	//-------------------------------------------------------------------------

};




