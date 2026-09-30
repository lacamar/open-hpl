#include "SomaScriptNatives.h"
#include "SomaImGui.h"
#include "SomaSound.h"
#include "SomaSave.h"
#include "SomaLuxGame.h"
#include "SomaLuxEntity.h"
#include "SomaToneMapping.h"
#include "SomaPostEffects.h"
#include "SomaCritter.h"

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
	cSomaSaveHandler::RegisterNatives(apEngine);
	cSomaToneMapping::RegisterNatives(apEngine);
	cSomaPostEffects::RegisterNatives(apEngine);
	SomaRegisterCritterNatives(apEngine);
	RegisterSomaScriptGenBindings(apEngine);
}
