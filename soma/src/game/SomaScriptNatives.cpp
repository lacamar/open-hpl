#include "SomaScriptNatives.h"
#include "RustSomaScriptS2.h"
#include "SomaImGui.h"
#include "SomaSound.h"
#include "SomaSoundscape.h"
#include "SomaSave.h"
#include "SomaLuxGame.h"
#include "SomaLuxEntity.h"
#include "SomaToneMapping.h"
#include "SomaPostEffects.h"
#include "SomaAgent.h"
#include "SomaCritter.h"

void RegisterSomaScriptNatives(asIScriptEngine *apEngine)
{
	if (OhplS2Rust())
	{
		OhplS2InstallHost();
		ohpl_s2_register_natives(apEngine);
		return;
	}
	RegisterSomaScriptMathNatives(apEngine);
	RegisterSomaScriptStringNatives(apEngine);
	RegisterSomaScriptLuxNatives(apEngine);
	RegisterSomaScriptGlobalNatives(apEngine);
	cSomaImGui::RegisterNatives(apEngine);
	cSomaLuxEntity::RegisterNatives(apEngine);
	cSomaLuxGame::RegisterNatives(apEngine);
	cSomaSoundEvents::RegisterNatives(apEngine);
	cSomaSoundscape::RegisterNatives(apEngine);
	cSomaSaveHandler::RegisterNatives(apEngine);
	cSomaToneMapping::RegisterNatives(apEngine);
	cSomaPostEffects::RegisterNatives(apEngine);
	SomaRegisterCritterNatives(apEngine);
	SomaRegisterAgentNatives(apEngine);
	RegisterSomaScriptGenBindings(apEngine);
}
