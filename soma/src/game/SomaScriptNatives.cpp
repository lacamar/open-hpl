#include "SomaScriptNatives.h"
#include "SomaLuxGame.h"

void RegisterSomaScriptNatives(asIScriptEngine *apEngine)
{
	RegisterSomaScriptMathNatives(apEngine);
	RegisterSomaScriptStringNatives(apEngine);
	RegisterSomaScriptLuxNatives(apEngine);
	cSomaLuxGame::RegisterNatives(apEngine);
}
