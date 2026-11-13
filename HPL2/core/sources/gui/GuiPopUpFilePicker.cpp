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

		size_t lMaxLen = asStartFilename.size() > asStartPath.size() ? asStartFilename.size() : asStartPath.size();
		msStartFilename.assign(lMaxLen, _W('\0'));
		msStartFilename.resize(ohpl_w3_fp_start_filename(asStartFilename.data(), asStartFilename.size(),
														 asStartPath.data(), asStartPath.size(),
														 lMaxLen ? &msStartFilename[0] : NULL, lMaxLen));

		mpSaveFileDest = NULL;
		mpLoadFileListDest = NULL;
		mpGfxUpButton = NULL;
		for(int i=0;i<6;++i) mvGfxFileTypeIcons[i] = NULL;

		OhplH21Host host = H21Host();
		mR = ohpl_h21_fp_new(&host, (int32_t)aType, reinterpret_cast<const uint32_t*>(asStartPath.data()), asStartPath.size(),
							 reinterpret_cast<const uint32_t*>(asStartFilename.data()), asStartFilename.size(),
							 (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback));
		mpWindow = (cWidgetWindow*)ohpl_h21_window(mR);
	}

    //-------------------------------------------------------------------------------

	cGuiPopUpFilePicker::~cGuiPopUpFilePicker()
	{
		OhplH21Host host = H21Host();
		ohpl_h21_destroy(&host, mR);
		mR = NULL;
	}

	//-------------------------------------------------------------------------------

	/////////////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	/////////////////////////////////////////////////////////////////////////////////

	//-------------------------------------------------------------------------------

	void cGuiPopUpFilePicker::SetSaveFileDest( tWString &asFileName )
	{
		mpSaveFileDest = &asFileName;
		OhplH21Host host = H21Host();
		ohpl_h21_fp_set_save_dest(&host, mR, reinterpret_cast<const uint32_t*>(asFileName.data()), asFileName.size());
	}

	//-------------------------------------------------------------------------------

	void cGuiPopUpFilePicker::SetLoadFileListDest( tWStringVec &asFileList )
	{
		mpLoadFileListDest = &asFileList;
	}

	//-------------------------------------------------------------------------------

	void cGuiPopUpFilePicker::OnAddFilter()
	{
		OhplH21Host host = H21Host();
		ohpl_h21_fp_on_add_filter(&host, mR);
	}

	//-------------------------------------------------------------------------

	void cGuiPopUpFilePicker::OnNavigate()
	{
		OhplH21Host host = H21Host();
		ohpl_h21_fp_on_navigate(&host, mR);
	}

	//-------------------------------------------------------------------------

	void cGuiPopUpFilePicker::Init()
	{
		OhplH21Host host = H21Host();
		ohpl_h21_fp_init(&host, mR);
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

};
