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

#include "system/Platform.h"

#include "system/String.h"

#include "system/LowLevelSystem.h"

#if USE_SDL2
#include "SDL2/SDL.h"
#else
#include "SDL/SDL.h"
#endif

#include "impl/TimerSDL.h"
#include "impl/ThreadSDL.h"
#include "impl/MutexSDL.h"
#include "impl/RustL1.h"
#include <stdexcept>

#include <set>
#include <algorithm>

namespace hpl {

	static void L1Utf8Sink(void *apCtx, const unsigned char *apData, size_t alLen)
	{
		static_cast<tString*>(apCtx)->assign(reinterpret_cast<const char*>(apData), alLen);
	}

	static void L1VideoModeSink(void *apCtx, int alDisplay, int alW, int alH, int alBpp)
	{
		static_cast<tVideoModeVec*>(apCtx)->push_back(cVideoMode(alDisplay, cVector2l(alW, alH), alBpp, 1));
	}
	//////////////////////////////////////////////////////////////////////////
	// APPLICATION
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	unsigned long cPlatform::GetApplicationTime()
	{
		return ohpl_l1_ticks();
	}

	//-----------------------------------------------------------------------

	void cPlatform::Sleep ( const unsigned int alMillisecs )
	{
		ohpl_l1_delay(alMillisecs);
	}

	void cPlatform::CopyTextToClipboard(const tWString &asText)
	{
		tString tstr = cString::S16BitToUTF8(asText);
		ohpl_l1_clipboard_set(tstr.c_str());
	}

	//-----------------------------------------------------------------------

	tWString cPlatform::LoadTextFromClipboard()
	{
		tWString tstr;
		tString sClip;
		if(ohpl_l1_clipboard_get(L1Utf8Sink, &sClip)) tstr = cString::UTF8ToWChar(sClip.c_str());
		return tstr;
	}

	void cPlatform::GetDisplayResolution(int alDisplay, int& alWidth, int& alHeight)
	{
		ohpl_l1_display_resolution(alDisplay, &alWidth, &alHeight);
	}

	//-----------------------------------------------------------------------
	void cPlatform::GetAvailableVideoModes(tVideoModeVec& avDestVidModes, int alMinBpp, int alMinRefreshRate)
	{
		avDestVidModes.clear();
		ohpl_l1_video_modes(L1VideoModeSink, &avDestVidModes);
	}

	tWString cPlatform::GetDisplayName(int alDisplay)
	{
		tString sName;
		if(!ohpl_l1_display_name(alDisplay, L1Utf8Sink, &sName)) throw std::logic_error("GetDisplayName: no display name");
		return cString::To16Char(sName);
	}

#ifndef HPL_MINIMAL
	//-----------------------------------------------------------------------

	iTimer * cPlatform::CreateTimer()
	{
		return hplNew(cTimerSDL, () );
	}


	//////////////////////////////////////////////////////////////////////////
	// THREADING
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	iThread* cPlatform::CreateThread(iThreadClass* apThreadClass)
	{
		iThread* pThread = hplNew(cThreadSDL, ());
		pThread->SetThreadClass(apThreadClass);

		return pThread;
	}

	//-----------------------------------------------------------------------

	iMutex* cPlatform::CreateMutEx()
	{
		return hplNew(cMutexSDL, ());
	}
#endif
}
