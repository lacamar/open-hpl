#include "SomaScriptNatives.h"
#include "SomaImGui.h"
#include "SomaLuxGame.h"
#include "SomaLuxEntity.h"

void RegisterSomaScriptNatives(asIScriptEngine *apEngine)
{
	RegisterSomaScriptMathNatives(apEngine);
	RegisterSomaScriptStringNatives(apEngine);
	RegisterSomaScriptLuxNatives(apEngine);
	RegisterSomaScriptGlobalNatives(apEngine);
	cSomaImGui::RegisterNatives(apEngine);
	cSomaLuxEntity::RegisterNatives(apEngine);
	cSomaLuxGame::RegisterNatives(apEngine);
	RegisterSomaScriptGenBindings(apEngine);
}
