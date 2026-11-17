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

#ifndef HPL_GUI_H
#define HPL_GUI_H

#include <map>
#include "engine/Updateable.h"
#include "gui/GuiTypes.h"
#include "gui/RustGuiG1.h"
#include "system/SystemTypes.h"

namespace hpl {

	class cResources;
	class cGraphics;
	class cSound;
	class cScene;
	class cInput;
	class cGuiSet;
	class cGuiSkin;
	class cGuiGfxElement;
	class iGuiMaterial;
	class iTexture;

	// Shell over the Rust-owned gui (rust/src/gui/g1_gui.rs): the engine needs an iUpdateable.
	class cGui : public iUpdateable
	{
	public:
		cGui() : iUpdateable("HPL_Gui"), mpRs(ohpl_g1_gui_new(this)) {}
		~cGui(){ ohpl_g1_gui_free(mpRs); }

		void Init(	cResources *apResources, cGraphics* apGraphics,
					cSound *apSound, cScene *apScene, cInput *apInput)
		{ ohpl_g1_gui_init(mpRs, apResources, apGraphics, apSound, apScene, apInput); }

		void Update(float afTimeStep){ ohpl_g1_gui_update(mpRs, afTimeStep); }
		void OnDraw(float afFrameTime){ ohpl_g1_gui_on_draw(mpRs, afFrameTime); }
		void OnPostBufferSwap(){ ohpl_g1_gui_on_post_buffer_swap(mpRs); }

		iGuiMaterial* GetMaterial(eGuiMaterial aType){ return (iGuiMaterial*)ohpl_g1_gui_material(mpRs, aType); }

		cGuiSkin* CreateSkin(const tString& asFile){ return ohpl_g1_gui_create_skin(mpRs, asFile.data(), asFile.size()); }

		eGuiSkinGfx GetSkinGfxFromString(const tString& asType){ return (eGuiSkinGfx)ohpl_g1_gui_skin_from_name(mpRs, kOhplSkinGfx, asType.data(), asType.size()); }
		eGuiSkinFont GetSkinFontFromString(const tString& asType){ return (eGuiSkinFont)ohpl_g1_gui_skin_from_name(mpRs, kOhplSkinFont, asType.data(), asType.size()); }
		eGuiSkinAttribute GetSkinAttributeFromString(const tString& asType){ return (eGuiSkinAttribute)ohpl_g1_gui_skin_from_name(mpRs, kOhplSkinAttribute, asType.data(), asType.size()); }
		tString GetSkinGfxString(eGuiSkinGfx aType){ return SkinTypeString(kOhplSkinGfx, aType); }
		tString GetSkinFontString(eGuiSkinFont aType){ return SkinTypeString(kOhplSkinFont, aType); }
		tString GetSkinAttributeString(eGuiSkinAttribute aType){ return SkinTypeString(kOhplSkinAttribute, aType); }

		cGuiSet* CreateSet(const tString& asName, cGuiSkin *apSkin){ return (cGuiSet*)ohpl_g1_gui_create_set(mpRs, asName.data(), asName.size(), apSkin); }
		cGuiSet* GetSetFromName(const tString& asName){ return (cGuiSet*)ohpl_g1_gui_set_from_name(mpRs, asName.data(), asName.size()); }
		void SetFocus(cGuiSet* apSet){ ohpl_g1_gui_set_focus(mpRs, (uintptr_t)apSet); }
		void SetFocusByName(const tString& asSetName){ ohpl_g1_gui_set_focus_by_name(mpRs, asSetName.data(), asSetName.size()); }
		cGuiSet* GetFocusedSet(){return (cGuiSet*)ohpl_g1_gui_focused_set(mpRs);}
		void DestroySet(cGuiSet *apSet){ ohpl_g1_gui_destroy_set(mpRs, (uintptr_t)apSet); }

		cGuiGfxElement* CreateGfxFilledRect(const cColor& aColor, eGuiMaterial aMaterial, bool abAddToList=true)
		{ return ohpl_g1_gui_create_gfx_filled_rect(mpRs, &aColor, aMaterial, abAddToList); }

		cGuiGfxElement* CreateGfxImage(	const tString &asFile,eGuiMaterial aMaterial,
										const cColor& aColor=cColor(1,1), bool abAddToList=true)
		{ return ohpl_g1_gui_create_gfx_image(mpRs, asFile.data(), asFile.size(), aMaterial, &aColor, abAddToList); }

		cGuiGfxElement* CreateGfxTexture(	const tString &asFile,eGuiMaterial aMaterial,
											eTextureType aTextureType = eTextureType_2D,
											const cColor& aColor=cColor(1,1),
											bool abMipMaps=false,
											bool abAddToList=true)
		{ return ohpl_g1_gui_create_gfx_texture_file(mpRs, asFile.data(), asFile.size(), aMaterial, aTextureType, &aColor, abMipMaps, abAddToList); }

		cGuiGfxElement* CreateGfxTexture(	iTexture *apTexture, bool abAutoDestroyTexture,
											eGuiMaterial aMaterial,
											const cColor& aColor=cColor(1,1),bool abAddToList=true,
											const cVector2f& avStartUV=0, const cVector2f& avEndUV=1)
		{ return ohpl_g1_gui_create_gfx_texture(mpRs, (uintptr_t)apTexture, abAutoDestroyTexture, aMaterial, &aColor, abAddToList, &avStartUV, &avEndUV); }

		/**
		 * Loads several images asFile+00, etc. Used for animations.Must have extension!
		 */
		cGuiGfxElement* CreateGfxImageBuffer(	const tString &asFile,eGuiMaterial aMaterial,
												bool abCreateAnimation=true,
												const cColor& aColor=cColor(1,1), bool abAddToList=true)
		{ return ohpl_g1_gui_create_gfx_image_buffer(mpRs, asFile.data(), asFile.size(), aMaterial, abCreateAnimation, &aColor, abAddToList); }

		void DestroyGfx(cGuiGfxElement* apGfx){ ohpl_g1_gui_destroy_gfx(mpRs, apGfx); }

		bool SendMousePos(const cVector2l &avPos, const cVector2l &avRel){ return ohpl_g1_gui_send_mouse_pos(mpRs, avPos.x, avPos.y, avRel.x, avRel.y); }
		bool SendMouseClickDown(eGuiMouseButton aButton, int alKeyModifiers=0){ return ohpl_g1_gui_send_mouse_button(mpRs, eGuiMessage_MouseDown, aButton, alKeyModifiers); }
		bool SendMouseClickUp(eGuiMouseButton aButton, int alKeyModifiers=0){ return ohpl_g1_gui_send_mouse_button(mpRs, eGuiMessage_MouseUp, aButton, alKeyModifiers); }
		bool SendMouseDoubleClick(eGuiMouseButton aButton, int alKeyModifiers=0){ return ohpl_g1_gui_send_mouse_button(mpRs, eGuiMessage_MouseDoubleClick, aButton, alKeyModifiers); }
		bool SendKeyPress(const cKeyPress& keyPress){ return ohpl_g1_gui_send_key(mpRs, eGuiMessage_KeyPress, &keyPress); }
		bool SendKeyRelease(const cKeyPress& keyPress){ return ohpl_g1_gui_send_key(mpRs, eGuiMessage_KeyRelease, &keyPress); }
		bool SendGamepadInput(const cGamepadInputData& aInput){ return ohpl_g1_gui_send_gamepad(mpRs, &aInput); }
		bool SendUIArrowPress(eUIArrow aX){ return ohpl_g1_gui_send_value(mpRs, eGuiMessage_UIArrowPress, aX); }
		bool SendUIArrowRelease(eUIArrow aX){ return ohpl_g1_gui_send_value(mpRs, eGuiMessage_UIArrowRelease, aX); }
		bool SendUIButtonPress(eUIButton aX){ return ohpl_g1_gui_send_value(mpRs, eGuiMessage_UIButtonPress, aX); }
		bool SendUIButtonRelease(eUIButton aX){ return ohpl_g1_gui_send_value(mpRs, eGuiMessage_UIButtonRelease, aX); }
		bool SendUIButtonDoublePress(eUIButton aX){ return ohpl_g1_gui_send_value(mpRs, eGuiMessage_UIButtonDoublePress, aX); }

		eMouseButton TranslateGuiMouseToMouse(eGuiMouseButton aButton){ return (eMouseButton)ohpl_gui_mouse_to_mouse(aButton); }
		eGuiMouseButton TranslateMouseToGuiMouse(eMouseButton aButton){ return (eGuiMouseButton)ohpl_gui_mouse_to_gui(aButton); }

		cResources* GetResources(){ return (cResources*)ohpl_g1_gui_resources(mpRs); }
		cInput*	GetInput() { return (cInput*)ohpl_g1_gui_input(mpRs); }

		static cGuiGfxElement* GetGfxRect(){ return ohpl_g1_gfx_rect(); }

	private:
		static tString SkinTypeString(int alKind, unsigned int alType)
		{
			tString sName;
			ohpl_gui_skin_name(alKind, alType, [](void* c, const char* p, size_t n){ static_cast<tString*>(c)->assign(p, n); }, &sName);
			return sName;
		}

		OhplG1Gui *mpRs;
	};

};
#endif // HPL_GUI_H
