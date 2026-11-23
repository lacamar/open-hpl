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

#ifndef HPL_THREAD_SDL_H
#define HPL_THREAD_SDL_H

#include "impl/RustL1.h"
#include "SDL2/SDL.h"
#include <typeinfo>

#include "system/Thread.h"

struct SDL_Thread;
struct OhplL1Thread;

namespace hpl {

	class cThreadSDL : public iThread
	{
	public:
		
		cThreadSDL();
		~cThreadSDL();
		
        void Start();
		void Stop();
		void Sleep(unsigned int alSleepTime);
		void SetPriority(eThreadPrio aPrio);

	protected:
		int TranslateEnginePrio(eThreadPrio aPrio);

	private:
		OhplL1Thread* mpRust;

	};


inline cThreadSDL::cThreadSDL() { mpRust = ohpl_l1_thread_new(); }
inline cThreadSDL::~cThreadSDL() { ohpl_l1_thread_free(mpRust); }
inline void cThreadSDL::Start() { mbThreadActive = true; ohpl_l1_thread_start(mpRust, iThread::MainThreadFunc, typeid(this).name(), this); }
inline void cThreadSDL::Stop() { mbThreadActive = false; ohpl_l1_thread_stop(mpRust); }
inline void cThreadSDL::Sleep(unsigned int alSleepTime) { ohpl_l1_delay(alSleepTime); }
inline void cThreadSDL::SetPriority(eThreadPrio aPrio) { ohpl_l1_thread_set_priority((int)aPrio); iThread::SetPriority(aPrio); }
inline int cThreadSDL::TranslateEnginePrio(eThreadPrio aPrio) { switch (aPrio) { case eThreadPrio_Low: return SDL_THREAD_PRIORITY_LOW; case eThreadPrio_High: return SDL_THREAD_PRIORITY_HIGH; case eThreadPrio_Normal: default: return SDL_THREAD_PRIORITY_NORMAL; } }

};
#endif // HPL_THREAD_SDL_H
