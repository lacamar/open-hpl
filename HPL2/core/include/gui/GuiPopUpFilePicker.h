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

#include "gui/GuiPopUp.h"

namespace hpl {

	// Rust-owned, see GuiPopUp.h; the browser state is an iFileBrowser object kept by the host (RustGuiH21Host.cpp).
	class cGuiPopUpFilePicker : public iGuiPopUp
	{
	public:
		void SetSaveFileDest( tWString &asFileName ){ ohpl_g2_popup_fp_save_dest(this, &asFileName, (const uint32_t*)asFileName.data(), asFileName.size()); }
		void SetLoadFileListDest( tWStringVec &asFileList ){ ohpl_g2_popup_fp_load_dest(this, &asFileList); }

		int AddCategory(const tWString& asCategory, const tWString& asFilter=_W(""))
		{ return ohpl_g2_popup_fp_add_category(this, (const uint32_t*)asCategory.data(), asCategory.size(), (const uint32_t*)asFilter.data(), asFilter.size()); }
		void AddFilter(int alCategoryId, const tWString& asFilter){ ohpl_g2_popup_fp_add_filter(this, alCategoryId, (const uint32_t*)asFilter.data(), asFilter.size()); }
	private:
		cGuiPopUpFilePicker();
	};

};
#endif // HPL_GUI_POP_UP_FILE_PICKER_H
