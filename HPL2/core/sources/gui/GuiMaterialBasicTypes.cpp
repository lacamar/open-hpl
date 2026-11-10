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

#include "gui/GuiMaterialBasicTypes.h"

#include "graphics/LowLevelGraphics.h"
#include "gui/RustGuiSkin.h"

namespace hpl {

	static void ApplyBlend(iLowLevelGraphics *apLowLevelGraphics, eGuiMaterial aType)
	{
		int lSrc, lDst;
		if(ohpl_gui_material_blend(aType, &lSrc, &lDst)==false) return;

		apLowLevelGraphics->SetBlendActive(true);
		apLowLevelGraphics->SetBlendFunc((eBlendFunc)lSrc, (eBlendFunc)lDst);
	}

	void cGuiMaterial_Diffuse::BeforeRender()
	{
		ApplyBlend(mpLowLevelGraphics, eGuiMaterial_Diffuse);
	}

	void cGuiMaterial_Diffuse::AfterRender()
	{
	}

	void cGuiMaterial_Alpha::BeforeRender()
	{
		ApplyBlend(mpLowLevelGraphics, eGuiMaterial_Alpha);
	}

	void cGuiMaterial_Alpha::AfterRender()
	{
	}

	void cGuiMaterial_FontNormal::BeforeRender()
	{
		ApplyBlend(mpLowLevelGraphics, eGuiMaterial_FontNormal);
	}

	void cGuiMaterial_FontNormal::AfterRender()
	{
	}

	void cGuiMaterial_Additive::BeforeRender()
	{
		ApplyBlend(mpLowLevelGraphics, eGuiMaterial_Additive);
	}

	void cGuiMaterial_Additive::AfterRender()
	{
	}

	void cGuiMaterial_Modulative::BeforeRender()
	{
		ApplyBlend(mpLowLevelGraphics, eGuiMaterial_Modulative);
	}

	void cGuiMaterial_Modulative::AfterRender()
	{
	}

	void cGuiMaterial_PremulAlpha::BeforeRender()
	{
		ApplyBlend(mpLowLevelGraphics, eGuiMaterial_PremulAlpha);
	}

	void cGuiMaterial_PremulAlpha::AfterRender()
	{
	}

}
