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

#ifndef HPL_TIMER_SDL_H
#define HPL_TIMER_SDL_H

#include "impl/RustL1.h"

#include "system/Timer.h"

struct OhplL1Timer;

#ifdef WIN32   // Windows system specific
	#include <windows.h>
#else          // Unix based system specific
	#include <sys/time.h>
#endif

namespace hpl {

	class cTimerSDL : public iTimer
	{
	public:
		
		cTimerSDL();
		~cTimerSDL();
		
        void Start();
		void Stop();

		double GetTimeInMicroSec(); 
	private:
		OhplL1Timer *mpRust;
	};


inline cTimerSDL::cTimerSDL() { mpRust = ohpl_l1_timer_new(); }
inline cTimerSDL::~cTimerSDL() { ohpl_l1_timer_free(mpRust); }
inline void cTimerSDL::Start() { ohpl_l1_timer_start(mpRust); }
inline void cTimerSDL::Stop() { ohpl_l1_timer_stop(mpRust); }
inline double cTimerSDL::GetTimeInMicroSec() { return ohpl_l1_timer_micros(mpRust); }

};
#endif // HPL_TIMER_SDL_H
