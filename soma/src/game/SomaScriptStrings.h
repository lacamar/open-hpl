#ifndef SOMA_SCRIPT_STRINGS_H
#define SOMA_SCRIPT_STRINGS_H

#include <angelscript.h>

void RegisterSomaScriptStrings(asIScriptEngine *apEngine);

// After the API value types: tString + cVector3f, cColor, tID...
void RegisterSomaScriptStringConcats(asIScriptEngine *apEngine);

// After RegisterScriptArray(): push_back/push_front/pop_back/pop_front/size/insertBack
void RegisterSomaScriptArrayExtras(asIScriptEngine *apEngine);

#endif // SOMA_SCRIPT_STRINGS_H
