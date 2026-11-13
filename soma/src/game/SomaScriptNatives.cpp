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
	OhplS2InstallHost();
	ohpl_s2_register_natives(apEngine);
}
