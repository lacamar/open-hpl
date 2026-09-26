#ifndef SOMA_SCRIPT_CHECK_H
#define SOMA_SCRIPT_CHECK_H

#include <string>

// Compiles every game script (config entry points + map scripts) against the recovered API
// without starting the engine; writes a JSON report. Returns 0 when everything compiles.
int RunSomaScriptCheck(const std::string &asGameDir, const std::string &asApiFile, const std::string &asReport);

#endif // SOMA_SCRIPT_CHECK_H
