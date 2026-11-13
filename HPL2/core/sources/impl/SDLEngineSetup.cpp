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

#include "impl/SDLEngineSetup.h"

#include <cstdlib>

#include "system/System.h"
#include "input/Input.h"
#include "graphics/Graphics.h"
#include "resources/Resources.h"
#include "scene/Scene.h"
#include "sound/Sound.h"
#include "physics/Physics.h"
#include "ai/AI.h"
#include "system/HeadlessControl.h"
#include "impl/RustImpl.h"
#include "graphics/RustGraphicsH5b.h"
#include "impl/RustL1.h"

#include "impl/KeyboardSDL.h"
#include "impl/MouseSDL.h"
#include "impl/LowLevelGraphicsSDL.h"
#ifdef HPL_VULKAN
#include "../../../../cshim/vk_backend.h"
#endif
#include "impl/LowLevelResourcesSDL.h"
#include "impl/LowLevelSystemSDL.h"
#include "impl/LowLevelInputSDL.h"
#include "impl/LowLevelSoundOpenAL.h"
#include "impl/LowLevelPhysicsNewton.h"

#if USE_SDL2
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"
#else
#include "SDL/SDL.h"
#include "SDL/SDL_syswm.h"
#endif
#ifdef WIN32
#include "Windows.h"
#include "Dbt.h"
#endif

namespace hpl {

	static void L1ErrorSink(void *apCtx, const unsigned char *apData, size_t alLen)
	{
		static_cast<tString*>(apCtx)->assign(reinterpret_cast<const char*>(apData), alLen);
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSDLEngineSetup::cSDLEngineSetup(tFlag alHplSetupFlags, const cEngineInitVars *apVars)
	{
		ohpl_l1_sdl_hints();
		AcquireHeadlessSingleInstanceLock();
		if(!ohpl_l1_sdl_init((alHplSetupFlags & (eHplSetup_Screen | eHplSetup_Video)) != 0))
		{
			tString sError;
			ohpl_l1_sdl_error(L1ErrorSink, &sError);
			FatalError("Error Initializing Display: %s", sError.c_str());
			exit(1);
		}

		//////////////////////////
		// System
		mpLowLevelSystem = hplNew( cLowLevelSystemSDL, () );
		
		//////////////////////////
		// Graphics
		const char *pRenderer = getenv("OPENHPL_RENDERER");
		if(pRenderer == NULL && apVars) pRenderer = apVars->mGraphics.msRenderer.c_str();
#ifdef HPL_VULKAN
		if(pRenderer && ohpl_sys_renderer_is_vulkan(pRenderer))
		{
			const tString sMode = apVars ? apVars->mGraphics.msColorOutput : tString();
			mpLowLevelGraphics = reinterpret_cast<iLowLevelGraphics*>(ohpl_vk_create(apVars ? sMode.c_str() : NULL,
				apVars ? apVars->mGraphics.mfPaperWhite : 0, apVars ? apVars->mGraphics.mfPeakNits : 0));
		}
		else
#endif
			mpLowLevelGraphics = hplNew( cLowLevelGraphicsSDL,() );
		if(pRenderer && ohpl_sys_renderer_is_vulkan(pRenderer) && dynamic_cast<cLowLevelGraphicsSDL*>(mpLowLevelGraphics))
			Warning("OPENHPL_RENDERER=vulkan: built without Vulkan, using OpenGL\n");
		{
			cLowLevelGraphicsSDL *pGl = dynamic_cast<cLowLevelGraphicsSDL*>(mpLowLevelGraphics);
			OhplH5bAttachGfx(mpLowLevelGraphics, pGl ? pGl->GetRsCtx() : NULL);
		}
		
		//////////////////////////
		// Input
		mpLowLevelInput = hplNew( cLowLevelInputSDL,(mpLowLevelGraphics) );
		
		//////////////////////////
		// Resources
		mpLowLevelResources = hplNew( cLowLevelResourcesSDL,(mpLowLevelGraphics) );
		
		//////////////////////////
		// Sound
		mpLowLevelSound	= hplNew( cLowLevelSoundOpenAL,() );
		
		//////////////////////////
		// Physics
		mpLowLevelPhysics = hplNew( cLowLevelPhysicsNewton,() );
		
	}

	//-----------------------------------------------------------------------

	cSDLEngineSetup::~cSDLEngineSetup()
	{
		Log("- Deleting lowlevel stuff.\n");
		
		Log("  Physics\n");
		hplDelete(mpLowLevelPhysics);
		Log("  Sound\n");
		hplDelete(mpLowLevelSound);
		Log("  Input\n");
		hplDelete(mpLowLevelInput);
		Log("  Resources\n");
		hplDelete(mpLowLevelResources);
		Log("  System\n");
		hplDelete(mpLowLevelSystem);
		Log("  Graphics\n");
		OhplH5bDetachGfx();
		hplDelete(mpLowLevelGraphics);
		ohpl_l1_sdl_shutdown();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cScene* cSDLEngineSetup::CreateScene(cGraphics* apGraphics, cResources *apResources, cSound* apSound,
										cPhysics *apPhysics, cSystem *apSystem,cAI *apAI,cGui *apGui)
	{
		cScene *pScene = hplNew( cScene, (apGraphics,apResources, apSound,apPhysics, apSystem,apAI,apGui) );
		return pScene;
	}

	//-----------------------------------------------------------------------

	
	/**
	 * \todo Lowlevelresource and resource both use lowlevel graphics. Can this be fixed??
	 * \param apGraphics 
	 * \return 
	 */
	cResources* cSDLEngineSetup::CreateResources(cGraphics* apGraphics)
	{
		cResources *pResources = hplNew( cResources, (mpLowLevelResources,mpLowLevelGraphics) );
		return pResources;
	}
	
	//-----------------------------------------------------------------------

	cInput* cSDLEngineSetup::CreateInput(cGraphics* apGraphics)
	{
		cInput *pInput = hplNew( cInput, (mpLowLevelInput) );
		return pInput;
	}
	
	//-----------------------------------------------------------------------

	cSystem* cSDLEngineSetup::CreateSystem()
	{
		cSystem *pSystem = hplNew( cSystem, (mpLowLevelSystem) );
		return pSystem;
	}
	
	//-----------------------------------------------------------------------

	cGraphics* cSDLEngineSetup::CreateGraphics()
	{
		cGraphics *pGraphics = hplNew( cGraphics, (mpLowLevelGraphics,mpLowLevelResources) );
		return pGraphics;
	}
	//-----------------------------------------------------------------------
	
	cSound* cSDLEngineSetup::CreateSound()
	{
		cSound *pSound = hplNew( cSound, (mpLowLevelSound) );
		return pSound;
	}
	
	//-----------------------------------------------------------------------
	
	cPhysics* cSDLEngineSetup::CreatePhysics()
	{
		cPhysics *pPhysics = hplNew( cPhysics, (mpLowLevelPhysics) );
		return pPhysics;
	}

	//-----------------------------------------------------------------------

	cAI* cSDLEngineSetup::CreateAI()
	{
		cAI *pAI = hplNew( cAI,() );
		return pAI;
	}

	//-----------------------------------------------------------------------

}
