#ifndef HPL_RUST_INPUT_SDL_H
#define HPL_RUST_INPUT_SDL_H

#include <list>
#include <vector>

#include "input/RustInput.h"
#include "impl/RustL1.h"
#include "SDL2/SDL_events.h"

static_assert(sizeof(((SDL_TextInputEvent*)0)->text) == 32, "OhplInputEvent::text");
static_assert(SDL_PRESSED == 1, "key state");

namespace hpl {
	inline void OhplConvertEvents(const std::list<SDL_Event>& alstEvents, std::vector<OhplInputEvent>& avOut)
	{
		std::vector<SDL_Event> vEvents(alstEvents.begin(), alstEvents.end());
		avOut.resize(vEvents.size());
		ohpl_l1_convert_events(vEvents.data(), vEvents.size(), avOut.data());
	}
}

#endif
