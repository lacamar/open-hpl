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
#include "haptic/Haptic.h"

#include "impl/KeyboardSDL.h"
#include "impl/MouseSDL.h"
#include "impl/LowLevelGraphicsSDL.h"
#include "impl/LowLevelResourcesSDL.h"
#include "impl/LowLevelSystemSDL.h"
#include "impl/LowLevelInputSDL.h"
#include "impl/LowLevelSoundFmod.h"
#include "impl/LowLevelSoundOpenAL.h"
#include "impl/LowLevelPhysicsNewton.h"

#ifdef INCLUDE_HAPTIC 
	#include "impl/LowLevelHapticHaptX.h"
#endif

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

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSDLEngineSetup::cSDLEngineSetup(tFlag alHplSetupFlags)
	{
#if SDL_VERSION_ATLEAST(2,0,0)
		SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
#endif
		// Headless test runs must never make real sound come out of the
		// physical speakers. OpenAL-soft (the only sound backend this engine
		// builds) reads ALSOFT_DRIVERS at alcOpenDevice() time to restrict
		// which backends it considers - "null" gives a real, silently-
		// succeeding device with no hardware/server access at all. Set this
		// as early as possible (before any subsystem, sound included, is
		// initialized below) and only if a caller hasn't already chosen a
		// driver list, same guard shape as the SDL_VIDEODRIVER one further
		// down. Inert for normal play, where OPENHPL_HEADLESS_SOCKET is unset.
		if(getenv("OPENHPL_HEADLESS_SOCKET") != NULL && getenv("ALSOFT_DRIVERS") == NULL)
		{
			setenv("ALSOFT_DRIVERS", "null", 1);
		}
		if(alHplSetupFlags & (eHplSetup_Screen | eHplSetup_Video))
		{
			// Under the opt-in headless automation server (OPENHPL_HEADLESS_SOCKET -
			// see HeadlessControl.h), prefer X11/XWayland over this system's default
			// Wayland driver, if a caller hasn't already forced one and an X server
			// looks available (DISPLAY set): a hidden (SDL_WINDOW_HIDDEN, see
			// LowLevelGraphicsSDL::Init()) window never gets mapped, and on Wayland
			// that means its wl_egl_window surface never receives the compositor's
			// initial configure event, so rendering into it silently produces
			// nothing (CopyFrameBufferToBitmap() reads back solid black) - verified
			// live, X11's hidden-window model has no such requirement. Left alone
			// (and normal, on-screen play unaffected) when DISPLAY isn't set, since
			// forcing x11 with no X server available would just fail SDL_Init below.
			if(getenv("OPENHPL_HEADLESS_SOCKET") != NULL &&
			   getenv("SDL_VIDEODRIVER") == NULL &&
			   getenv("DISPLAY") != NULL)
			{
				setenv("SDL_VIDEODRIVER", "x11", 1);
			}

			if(SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0) {
				FatalError("Error Initializing Display: %s",SDL_GetError());
				exit(1);
			}
#if SDL_VERSION_ATLEAST(2,0,0)
            // Don't suspend the real desktop's screensaver/DPMS on behalf of a
            // headless test run - X11_SuspendScreenSaver() resets the X server's
            // idle timer, which under this repo's headless testing (the hidden
            // window still lives on the real DISPLAY, see the driver-selection
            // comment above) was observed waking the physical monitor even
            // though the window itself is never shown.
            if(getenv("OPENHPL_HEADLESS_SOCKET") == NULL)
            {
                SDL_DisableScreenSaver();
            }
#elif defined WIN32 // only on SDL1.2
			// Set up device notifications!
			// This is bad, cos it is actually Windows specific code, should not be here. TODO: move it, obviously
			SDL_SysWMinfo info;
			SDL_VERSION(&info.version);
			if(SDL_GetWMInfo(&info))
			{
				DEV_BROADCAST_DEVICEINTERFACE notificationFilter;
				ZeroMemory(&notificationFilter, sizeof(notificationFilter));
 
				// set up filtering, so we only get notified of input device changes
				notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
				static const GUID GuidDevInterfaceHID = {0x745a17a0, 0x74d3, 0x11d0,
															{ 0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda }};
				notificationFilter.dbcc_classguid = GuidDevInterfaceHID;

				notificationFilter.dbcc_size = sizeof(notificationFilter);
 
				HDEVNOTIFY hDevNotify;
				hDevNotify = RegisterDeviceNotification(info.window, &notificationFilter,
					DEVICE_NOTIFY_WINDOW_HANDLE |
					DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
 
				if(hDevNotify == NULL) {
				}
			}
#endif // WIN32
		}
		else
		{
			SDL_Init( SDL_INIT_TIMER );
		}
		
		//////////////////////////
		// System
		mpLowLevelSystem = hplNew( cLowLevelSystemSDL, () );
		
		//////////////////////////
		// Graphics
		mpLowLevelGraphics = hplNew( cLowLevelGraphicsSDL,() );
		
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
		
		//////////////////////////
		// Haptic
#ifdef INCLUDE_HAPTIC 
		mpLowLevelHaptic = hplNew( cLowLevelHapticHaptX,() );
#else 
		mpLowLevelHaptic = NULL;
#endif
		
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
		hplDelete(mpLowLevelGraphics);
		Log("  Haptic\n");
#ifdef INCLUDE_HAPTIC 	
		hplDelete(mpLowLevelHaptic);
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
        SDL_EnableScreenSaver();
#endif
		SDL_Quit();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cScene* cSDLEngineSetup::CreateScene(cGraphics* apGraphics, cResources *apResources, cSound* apSound,
										cPhysics *apPhysics, cSystem *apSystem,cAI *apAI,cGui *apGui,
										cHaptic *apHaptic)
	{
		cScene *pScene = hplNew( cScene, (apGraphics,apResources, apSound,apPhysics, apSystem,apAI,apGui,apHaptic) );
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

	cHaptic* cSDLEngineSetup::CreateHaptic()
	{
		cHaptic *pHaptic = hplNew( cHaptic, (mpLowLevelHaptic) );
		return pHaptic;
	}

	//-----------------------------------------------------------------------

}
