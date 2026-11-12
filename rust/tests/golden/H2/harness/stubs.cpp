// Silent logging stubs so the probes do not pull LowLevelSystemSDL.cpp (and its main) out of libHPL2.a.
#include "system/LowLevelSystem.h"
#include <cstdlib>

namespace hpl {
	void FatalError(const char*, ...) { abort(); }
	void Error(const char*, ...) {}
	void Warning(const char*, ...) {}
	void Log(const char*, ...) {}
	void LogUpdate(const char*, ...) {}
}
