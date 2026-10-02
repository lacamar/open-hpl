#ifndef SOMA_SCRIPT_NATIVES_H
#define SOMA_SCRIPT_NATIVES_H

#include <angelscript.h>

void RegisterSomaScriptNatives(asIScriptEngine *apEngine);

void RegisterSomaScriptMathNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptStringNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptLuxNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptGlobalNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptGenBindings(asIScriptEngine *apEngine);

// Types whose constructors all come from natives (the API's recorded ones are skipped)
bool SomaScriptHasNativeBehaviours(const char *apType);

#endif // SOMA_SCRIPT_NATIVES_H
