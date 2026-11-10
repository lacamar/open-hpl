#ifndef HPL_RUST_INPUT_SDL_H
#define HPL_RUST_INPUT_SDL_H

#include <cstring>
#include <list>
#include <vector>

#include "input/RustInput.h"
#include "SDL2/SDL_events.h"

static_assert(sizeof(((SDL_TextInputEvent*)0)->text) == 32, "OhplInputEvent::text");
static_assert(SDL_PRESSED == 1, "key state");

namespace hpl {
	inline void OhplConvertEvents(const std::list<SDL_Event>& alstEvents, std::vector<OhplInputEvent>& avOut)
	{
		avOut.clear();
		avOut.reserve(alstEvents.size());
		for(const SDL_Event& e : alstEvents)
		{
			OhplInputEvent o;
			memset(&o, 0, sizeof(o));
			switch(e.type)
			{
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				o.kind = eOhplInputKind_Key; o.sym = e.key.keysym.sym; o.key_mod = e.key.keysym.mod; o.state = e.key.state;
				break;
			case SDL_TEXTINPUT:
				o.kind = eOhplInputKind_Text; memcpy(o.text, e.text.text, sizeof(o.text));
				break;
			case SDL_MOUSEMOTION:
				o.kind = eOhplInputKind_MouseMotion; o.x = e.motion.x; o.y = e.motion.y; o.xrel = e.motion.xrel; o.yrel = e.motion.yrel;
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				o.kind = eOhplInputKind_MouseButton; o.button = e.button.button; o.state = e.type == SDL_MOUSEBUTTONDOWN;
				break;
			case SDL_MOUSEWHEEL:
				o.kind = eOhplInputKind_Wheel; o.wheel_y = e.wheel.y;
				break;
			case SDL_CONTROLLERAXISMOTION:
				o.kind = eOhplInputKind_PadAxis; o.which = e.caxis.which; o.button = e.caxis.axis; o.axis_value = e.caxis.value;
				break;
			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				o.kind = eOhplInputKind_PadButton; o.which = e.cbutton.which; o.button = e.cbutton.button; o.state = e.cbutton.state;
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				o.kind = eOhplInputKind_PadRemoved; o.which = e.cdevice.which;
				break;
			}
			avOut.push_back(o);
		}
	}
}

#endif
