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

#ifndef HPL_GUI_POP_UP_FILE_PICKER_H
#define HPL_GUI_POP_UP_FILE_PICKER_H

#include "gui/WidgetBaseClasses.h"
#include "gui/GuiPopUp.h"

namespace hpl {

	class cGuiSet;
	class cGuiSkin;

	class cWidgetWindow;
	class cWidgetMultiPropertyListBox;
	class cWidgetButton;
	class cWidgetTextBox;
	class cWidgetComboBox;

	//---------------------------------------------------------------

	//---------------------------------------------------------------

	class cGuiPopUpFilePicker : public iGuiPopUp, public iFileBrowser
	{
	public:
		cGuiPopUpFilePicker(cGuiSet* apSet, cGuiSkin* apSkin, eFilePickerType aeType, const tWString &asStartPath, bool abShowHiddenFiles,
							void *apCallbackObject, tGuiCallbackFunc apCallback, const tWString& asStartFilename=_W(""));
		virtual ~cGuiPopUpFilePicker();

		void Init();

		void SetSaveFileDest( tWString &asFileName );
		void SetLoadFileListDest( tWStringVec &asFileList );

	protected:
		void OnAddFilter();
		void OnNavigate();
		bool H21Ext(const OhplH21Cmd &aCmd, OhplH21Ret &aRet);




		//////////////////////////////
		// Data
		tWString msStartFilename;

		tWString* mpSaveFileDest;
		tWStringVec* mpLoadFileListDest;

		eFilePickerType mPickerType;

		cGuiGfxElement* mvGfxFileTypeIcons[6];
		cGuiGfxElement* mpGfxUpButton;

		std::vector<tWString> mvH21Files;
	};
};

#endif // HPL_GUI_POP_UP_FILE_PICKER_H
