#ifndef SOMA_SOUNDSCAPE_H
#define SOMA_SOUNDSCAPE_H

#include "hpl.h"
#include "RustSomaSound.h"

class asIScriptEngine;
class cSomaLuxMap;

using namespace hpl;

class cSomaSoundscape
{
public:
	static cSomaSoundscape *Get()
	{
		static cSomaSoundscape gInstance;
		return &gInstance;
	}
	static void RegisterNatives(asIScriptEngine *apEngine) { ohpl_s7_register_soundscape(apEngine); }

	void Update(cSomaLuxMap *apMap, float afTimeStep) { ohpl_s7_scape_update((size_t)apMap, afTimeStep); }
	void Forget(cSomaLuxMap *apMap) { ohpl_s7_scape_forget((size_t)apMap); }
	void SetPaused(bool abX) { ohpl_s7_scape_set_paused(abX); }
	tString Describe()
	{
		tString s;
		ohpl_s7_scape_describe([](void *c, const unsigned char *p, size_t n) { static_cast<tString *>(c)->assign((const char *)p, n); }, &s);
		return s;
	}
};

#endif // SOMA_SOUNDSCAPE_H
