#include "SomaScriptNatives.h"
#include "SomaImGui.h"
#include "SomaSound.h"
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
	cSomaSoundEvents::RegisterNatives(apEngine);
	RegisterSomaScriptGenBindings(apEngine);
}
