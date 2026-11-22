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

#ifndef HPL_GRAPHICS_H
#define HPL_GRAPHICS_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "engine/Updateable.h"
#include "graphics/RustGx.h"

namespace hpl {

	class cResources;
	class iRenderer;
	class iPostEffectType;
	class iPostEffectParams;
	class iPostEffect;
	class iLowLevelResources;
	class iLowLevelGraphics;
	class cMeshCreator;
	class cTextureCreator;
	class cDecalCreator;
	class iFrameBuffer;
	class iDepthStencilBuffer;
	class iTexture;
	class iMaterialType;
	class cPostEffectComposite;
	class iGpuProgram;
	class cParserVarContainer;

	//------------------------------------------------------

	// State and logic live in rust/src/graphics/gx_graphics.rs.
	class cGraphics : public iUpdateable
	{
	public:
		cGraphics(iLowLevelGraphics *apLowLevelGraphics,iLowLevelResources *apLowLevelResources);
		~cGraphics();

		bool Init(int alWidth, int alHeight, int alDisplay, int alBpp, int abFullscreen, int alMultisampling,
					eGpuProgramFormat aGpuProgramFormat,const tString &asWindowCaption, const cVector2l &avWindowPos,
					cResources* apResources, tFlag alHplSetupFlags)
		{
			return ohpl_gx_gfx_init(mpRs, alWidth, alHeight, alDisplay, alBpp, abFullscreen, alMultisampling, aGpuProgramFormat,
									asWindowCaption.data(), asWindowCaption.size(), &avWindowPos, apResources, alHplSetupFlags);
		}

		void Update(float afTimeStep);

		iLowLevelGraphics* GetLowLevel(){ return mpLowLevelGraphics;}

		iRenderer* GetRenderer(eRenderer aType){ return static_cast<iRenderer*>(ohpl_gx_gfx_renderer(mpRs, aType)); }
		void ReloadRendererData(){ ohpl_gx_gfx_reload_renderer_data(mpRs); }

		iFrameBuffer* CreateFrameBuffer(const tString& asName){ return static_cast<iFrameBuffer*>(ohpl_gx_gfx_create_frame_buffer(mpRs, asName.data(), asName.size())); }
		void DestroyFrameBuffer(iFrameBuffer* apFrameBuffer){ ohpl_gx_gfx_destroy_frame_buffer(mpRs, apFrameBuffer); }

		iFrameBuffer* GetTempFrameBuffer(const cVector2l& avSize, ePixelFormat aPixelFormat, int alIndex){ return static_cast<iFrameBuffer*>(ohpl_gx_gfx_temp_frame_buffer(mpRs, &avSize, aPixelFormat, alIndex)); }

		static void SetTempFrameBufferTextureType(eTextureType aType){ ohpl_gx_gfx_set_temp_texture_type(aType); }
		static eTextureType GetTempFrameBufferTextureType(){ return (eTextureType)ohpl_gx_gfx_temp_texture_type(); }

		iDepthStencilBuffer* CreateDepthStencilBuffer(	const cVector2l& avSize, int alDepthBits, int alStencilBits,
														bool abLookForMatchingFirst){ return static_cast<iDepthStencilBuffer*>(ohpl_gx_gfx_create_depth_stencil(mpRs, &avSize, alDepthBits, alStencilBits, abLookForMatchingFirst)); }
		iDepthStencilBuffer* FindDepthStencilBuffer(const cVector2l& avSize, int alMinDepthBits, int alMinStencilBits){ return static_cast<iDepthStencilBuffer*>(ohpl_gx_gfx_find_depth_stencil(mpRs, &avSize, alMinDepthBits, alMinStencilBits)); }
		void DestoroyDepthStencilBuffer(iDepthStencilBuffer* apBuffer){ ohpl_gx_gfx_destroy_depth_stencil(mpRs, apBuffer); }

		iTexture* CreateTexture(const tString &asName,eTextureType aType,   eTextureUsage aUsage){ return static_cast<iTexture*>(ohpl_gx_gfx_create_texture(mpRs, asName.data(), asName.size(), aType, aUsage)); }
		void DestroyTexture(iTexture *apTexture){ ohpl_gx_gfx_destroy_texture(mpRs, apTexture); }

		cPostEffectComposite* CreatePostEffectComposite(){ return static_cast<cPostEffectComposite*>(ohpl_gx_gfx_create_composite(mpRs)); }
		void DestroyPostEffectComposite(cPostEffectComposite* apComposite){ ohpl_gx_gfx_destroy_composite(mpRs, apComposite); }

        void AddPostEffectType(iPostEffectType *apPostEffectBase){ ohpl_gx_gfx_add_post_effect_type(mpRs, apPostEffectBase); }

		iPostEffect* CreatePostEffect(iPostEffectParams *apParams){ return static_cast<iPostEffect*>(ohpl_gx_gfx_create_post_effect(mpRs, apParams)); }
		void DestroyPostEffect(iPostEffect* apPostEffect){ ohpl_gx_gfx_destroy_post_effect(mpRs, apPostEffect); }

		iGpuProgram* CreateGpuProgram(const tString& asName){ return static_cast<iGpuProgram*>(ohpl_gx_gfx_create_gpu_program(mpRs, asName.data(), asName.size())); }
		iGpuProgram* CreateGpuProgramFromShaders(	const tString& asName, const tString& asVtxShader,const tString& asFragShader,
													cParserVarContainer *apVarContainer)
		{
			return static_cast<iGpuProgram*>(ohpl_gx_gfx_create_gpu_program_from_shaders(mpRs, asName.data(), asName.size(),
												asVtxShader.data(), asVtxShader.size(), asFragShader.data(), asFragShader.size(), apVarContainer));
		}
		void DestroyGpuProgram(iGpuProgram* apProgram){ ohpl_gx_gfx_destroy_gpu_program(mpRs, apProgram); }

		void AddMaterialType(iMaterialType *apType, const tString& asName){ ohpl_gx_gfx_add_material_type(mpRs, apType, asName.data(), asName.size()); }
		iMaterialType *GetMaterialType(const tString& asName){ return static_cast<iMaterialType*>(ohpl_gx_gfx_material_type(mpRs, asName.data(), asName.size())); }
		tStringVec GetMaterialTypeNames()
		{
			tStringVec vNames;
			size_t lLen;
			for(size_t i=0; const char *pName = ohpl_gx_gfx_material_type_name(mpRs, i, &lLen); ++i) vNames.push_back(tString(pName, lLen));
			return vNames;
		}
		void ReloadMaterials(){ ohpl_gx_gfx_reload_materials(mpRs); }

		cMeshCreator* GetMeshCreator(){return static_cast<cMeshCreator*>(ohpl_gx_gfx_mesh_creator(mpRs));}
		cTextureCreator* GetTextureCreator(){ return static_cast<cTextureCreator*>(ohpl_gx_gfx_texture_creator(mpRs));}
		cDecalCreator* GetDecalCreator() {return static_cast<cDecalCreator*>(ohpl_gx_gfx_decal_creator(mpRs));}

		bool GetScreenIsSetUp(){ return ohpl_gx_gfx_screen_is_setup(mpRs);}

	private:
		iLowLevelGraphics *mpLowLevelGraphics;
		OhplGxGraphics *mpRs;
	};

};
#endif // HPL_GRAPHICS_H
