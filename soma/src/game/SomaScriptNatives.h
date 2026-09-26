#ifndef SOMA_SCRIPT_NATIVES_H
#define SOMA_SCRIPT_NATIVES_H

#include <angelscript.h>

// Native implementations of the SOMA script API; registered after the API's types and before
// its stubs, so every declaration registered here replaces a stub.
void RegisterSomaScriptNatives(asIScriptEngine *apEngine);

void RegisterSomaScriptMathNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptStringNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptLuxNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptGlobalNatives(asIScriptEngine *apEngine);
// scripts/soma-gen-bindings.py: recovered methods bound to same-named HPL2 methods
void RegisterSomaScriptGenBindings(asIScriptEngine *apEngine);

// Types whose constructors all come from natives (the API's recorded ones are skipped)
bool SomaScriptHasNativeBehaviours(const char *apType);

#endif // SOMA_SCRIPT_NATIVES_H
