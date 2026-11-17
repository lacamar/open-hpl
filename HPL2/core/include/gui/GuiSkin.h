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

#ifndef HPL_GUI_SKIN_H
#define HPL_GUI_SKIN_H

#include <cstddef>
#include "gui/GuiTypes.h"
#include "gui/RustGuiG1.h"

namespace hpl {

	class cGui;
	class cGuiGfxElement;
	class iFontData;
	class iGuiMaterial;

	// Created and owned by Rust (g1_skin.rs); layout checked in RustGuiG1.h.
	class cGuiSkinFont
	{
	public:
		iFontData* mpFont;
		cVector2f mvSize;
		cColor mColor;
		iGuiMaterial *mpMaterial;
		cGui *mpGui;
		cGuiSkinFont() = delete;
	};
	static_assert(sizeof(cGuiSkinFont) == 48 && offsetof(cGuiSkinFont, mvSize) == 8 && offsetof(cGuiSkinFont, mColor) == 16 &&
				  offsetof(cGuiSkinFont, mpMaterial) == 32, "cGuiSkinFont layout (rust/src/gui/g1_skin.rs SkinFont)");

	// Rust-owned, created by cGui::CreateSkin.
	class cGuiSkin
	{
	public:
		tString GetName(){ size_t n; const char* p = ohpl_g1_skin_name(this, &n); return tString(p, n); }

		bool LoadFromFile(const tWString &asFile){ return ohpl_g1_skin_load(this, (const uint32_t*)asFile.data(), asFile.size()); }

        cGuiGfxElement* GetGfx(eGuiSkinGfx aType){ return ohpl_g1_skin_gfx(this, aType); }
		cGuiSkinFont* GetFont(eGuiSkinFont aType){ return ohpl_g1_skin_font(this, aType); }
		const cVector3f& GetAttribute(eGuiSkinAttribute aType){ return *ohpl_g1_skin_attribute(this, aType); }
	private:
		cGuiSkin();
	};

};
#endif // HPL_GUI_SKIN_H
