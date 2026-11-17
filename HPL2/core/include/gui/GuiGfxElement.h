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

#ifndef HPL_GUI_GFX_ELEMENT_H
#define HPL_GUI_GFX_ELEMENT_H

#include "gui/GuiTypes.h"
#include "gui/RustGuiG1.h"

namespace hpl {

	class iGuiMaterial;
	class iTexture;
	class cFrameSubImage;
	class cGuiGfxElement;

	#define kMaxGuiTextures (4)

	enum eGuiGfxAnimationType
	{
		eGuiGfxAnimationType_Loop,
		eGuiGfxAnimationType_StopAtEnd,
		eGuiGfxAnimationType_Oscillate,
		eGuiGfxAnimationType_Random,
		eGuiGfxAnimationType_LastEnum
	};

	// Rust-owned: `this` is the Rust allocation (rust/src/gui/g1_gfx.rs), never constructed here.
	class cGuiGfxAnimation
	{
	public:
		void AddFrame(int alNum){ ohpl_g1_anim_add_frame(this, alNum); }
		void SetType(eGuiGfxAnimationType aType){ ohpl_g1_anim_set_type(this, (int)aType); }
		void SetFrameLength(float afLength){ ohpl_g1_anim_set_frame_length(this, afLength); }
	private:
		cGuiGfxAnimation();
	};

	// Rust-owned, created by cGui::CreateGfx* and freed by cGui::DestroyGfx.
	class cGuiGfxElement
	{
	public:
		void Update(float afTimeStep){ ohpl_g1_gfx_update(this, afTimeStep); }

		void AddImage(cFrameSubImage* apImage){ ohpl_g1_gfx_add_image(this, (uintptr_t)apImage); }
		void AddTexture(iTexture* apTexture, const cVector2f& avStartUV=0, const cVector2f& avEndUV=1)
		{ ohpl_g1_gfx_add_texture(this, (uintptr_t)apTexture, &avStartUV, &avEndUV); }
		void AddImageToBuffer(cFrameSubImage* apImage){ ohpl_g1_gfx_add_image_to_buffer(this, (uintptr_t)apImage); }

		int GetTextureNum(){ return ohpl_g1_gfx_texture_num(this); }
		void SetDestroyTexture(bool abX){ ohpl_g1_gfx_set_destroy_texture(this, abX); }
		bool GetDestroyTexture(){ return ohpl_g1_gfx_destroy_texture(this); }

		iTexture * GetTexture(int alIdx){ return (iTexture*)ohpl_g1_gfx_texture(this, alIdx); }
		cFrameSubImage * GetImage(int alIdx){ return (cFrameSubImage*)ohpl_g1_gfx_image(this, alIdx); }

		void SetOffset(const cVector3f& avOffset){ ohpl_g1_gfx_set_offset(this, &avOffset); }
		const cVector3f& GetOffset()const { return *ohpl_g1_gfx_offset((cGuiGfxElement*)this); }

		void SetActiveSize(const cVector2f& avSize){ ohpl_g1_gfx_set_active_size(this, &avSize); }
		const cVector2f& GetActiveSize(){ return *ohpl_g1_gfx_active_size(this); }

		cGuiGfxAnimation* CreateAnimtion(const tString& asName){ return ohpl_g1_gfx_create_animation(this); }
		void PlayAnimation(int alNum){ ohpl_g1_gfx_play_animation(this, alNum); }
		cGuiGfxAnimation* GetAnimation(int alIdx){ return ohpl_g1_gfx_animation(this, alIdx); }
		int GetAnimationNum(){ return ohpl_g1_gfx_animation_num(this); }
		void SetAnimationTime(float afTime){ ohpl_g1_gfx_set_animation_time(this, afTime); }
		void SetAnimationPaused(bool abX){ ohpl_g1_gfx_set_paused(this, abX); }
		bool GSetAnimationPaused(){ return ohpl_g1_gfx_paused(this); }

		void SetMaterial(iGuiMaterial *apMat){ ohpl_g1_gfx_set_material(this, apMat); }
		void SetColor(const cColor &aColor){ ohpl_g1_gfx_set_color(this, &aColor); }

		void SetFlipUvYAxis(bool abX){ ohpl_g1_gfx_set_flip(this, abX); }
		bool GetFlipUvYAxis() { return ohpl_g1_gfx_flip(this); }

		cVector2f GetImageSize(){ cVector2f v; ohpl_g1_gfx_image_size(this, &v); return v; }

		void Flush(){ ohpl_g1_gfx_flush(this); }
	private:
		cGuiGfxElement();
	};

};
#endif // HPL_GUI_GFX_ELEMENT_H
