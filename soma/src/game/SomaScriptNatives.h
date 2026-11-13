#ifndef SOMA_SCRIPT_NATIVES_H
#define SOMA_SCRIPT_NATIVES_H

#include <angelscript.h>
#include <set>
#include <string>

void RegisterSomaScriptNatives(asIScriptEngine *apEngine);

void RegisterSomaScriptMathNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptStringNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptLuxNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptGlobalNatives(asIScriptEngine *apEngine);
void RegisterSomaScriptGenBindings(asIScriptEngine *apEngine);
size_t SomaScriptGenBindingCount();
void (*SomaScriptGenBindingFunc(size_t alIdx))(asIScriptGeneric *);

// Types whose constructors all come from natives (the API's recorded ones are skipped)
bool SomaScriptHasNativeBehaviours(const char *apType);
std::multiset<std::string> &SomaStringSetOf(void *apSet);

#endif // SOMA_SCRIPT_NATIVES_H
