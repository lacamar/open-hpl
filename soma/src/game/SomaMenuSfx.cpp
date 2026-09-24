// Menu click/hover/glitch/sting SFX. MenuHandler.hps plays them as FMOD events
// (special_fx/frontend/...); the samples live in special_fx.fsb / special_fx_stream.fsb.

#include "SomaMenuSfx.h"
#include "SomaFsb.h"

//---------------------------------------

static bool gbSomaMenuSfxReady = false;
static bool gbSomaMenuSfxChangeOk = false;
static bool gbSomaMenuSfxFocusOk = false;
static bool gbSomaMenuSfxSelectOk = false;
static bool gbSomaMenuSfxSliderOk = false;
static bool gbSomaMenuSfxNewGameStingOk = false;
static bool gabSomaMenuSfxGlitchOk[12] = {false};
// FG_Menu_Sting / main_menu_bg events resolve to these sample names in special_fx_stream.fsb
static bool gbSomaMenuSfxFGLogoStingOk = false;
static bool gbSomaMenuSfxMenuBgNoiseOk = false;

static const cSomaFsbWanted kSpecialFx[] = {
	{"frontend_menu_change_01", "frontend_menu_change.ogg"},
	{"frontend_menu_focus_01", "frontend_menu_focus.ogg"},
	{"frontend_menu_select_01", "frontend_menu_select.ogg"},
	{"frontend_menu_slider_01", "frontend_menu_slider.ogg"},
	{"menu_glitch_001", "menu_glitch_01.ogg"},
	{"menu_glitch_002", "menu_glitch_02.ogg"},
	{"menu_glitch_003", "menu_glitch_03.ogg"},
	{"menu_glitch_004", "menu_glitch_04.ogg"},
	{"menu_glitch_005", "menu_glitch_05.ogg"},
	{"menu_glitch_006", "menu_glitch_06.ogg"},
	{"menu_glitch_007", "menu_glitch_07.ogg"},
	{"menu_glitch_008", "menu_glitch_08.ogg"},
	{"menu_glitch_009", "menu_glitch_09.ogg"},
	{"menu_glitch_010", "menu_glitch_10.ogg"},
	{"menu_glitch_011", "menu_glitch_11.ogg"},
	{"menu_glitch_012", "menu_glitch_12.ogg"},
};

static const cSomaFsbWanted kSpecialFxStream[] = {
	{"new_game_sting", "new_game_sting.wav"},
	{"FG_Logo_Sting", "fg_logo_sting.wav"},
	{"menu_bg_noise", "menu_bg_noise.wav"},
};

//---------------------------------------

void cSomaMenuSfx::EnsureCached(cResources *apResources)
{
	if (gbSomaMenuSfxReady)
		return;
	gbSomaMenuSfxReady = true;

	if (apResources == NULL)
		return;

	tWString sCacheDir = cSomaFsb::GetCacheDir(_W("sfx"));

	bool bAllPresent = cPlatform::FileExists(sCacheDir + _W("frontend_menu_change.ogg")) &&
						cPlatform::FileExists(sCacheDir + _W("frontend_menu_focus.ogg")) &&
						cPlatform::FileExists(sCacheDir + _W("frontend_menu_select.ogg")) &&
						cPlatform::FileExists(sCacheDir + _W("frontend_menu_slider.ogg")) &&
						cPlatform::FileExists(sCacheDir + _W("new_game_sting.wav")) &&
						cPlatform::FileExists(sCacheDir + _W("fg_logo_sting.wav")) &&
						cPlatform::FileExists(sCacheDir + _W("menu_bg_noise.wav")) &&
						cPlatform::FileExists(sCacheDir + _W("menu_glitch_01.ogg"));

	if (bAllPresent == false)
	{
		cSomaFsb::ExtractBank(apResources, "special/special_fx.fsb", sCacheDir, kSpecialFx, sizeof(kSpecialFx) / sizeof(kSpecialFx[0]));
		cSomaFsb::ExtractBank(apResources, "special/special_fx_stream.fsb", sCacheDir, kSpecialFxStream, sizeof(kSpecialFxStream) / sizeof(kSpecialFxStream[0]));
	}

	gbSomaMenuSfxChangeOk = cPlatform::FileExists(sCacheDir + _W("frontend_menu_change.ogg"));
	gbSomaMenuSfxFocusOk = cPlatform::FileExists(sCacheDir + _W("frontend_menu_focus.ogg"));
	gbSomaMenuSfxSelectOk = cPlatform::FileExists(sCacheDir + _W("frontend_menu_select.ogg"));
	gbSomaMenuSfxSliderOk = cPlatform::FileExists(sCacheDir + _W("frontend_menu_slider.ogg"));
	gbSomaMenuSfxNewGameStingOk = cPlatform::FileExists(sCacheDir + _W("new_game_sting.wav"));
	gbSomaMenuSfxFGLogoStingOk = cPlatform::FileExists(sCacheDir + _W("fg_logo_sting.wav"));
	gbSomaMenuSfxMenuBgNoiseOk = cPlatform::FileExists(sCacheDir + _W("menu_bg_noise.wav"));
	for (int i = 0; i < 12; ++i)
	{
		tString sName = "menu_glitch_" + (i + 1 < 10 ? tString("0") + cString::ToString(i + 1) : cString::ToString(i + 1)) + ".ogg";
		gabSomaMenuSfxGlitchOk[i] = cPlatform::FileExists(sCacheDir + cString::To16Char(sName));
	}

	// Real resources.cfg has no entry for a per-user cache dir, and this
	// engine never writes into the real SOMA install itself (see this
	// repo's own established rule) - register the cache dir directly so
	// cSoundHandler::PlayGui("frontend_menu_focus.ogg") resolves exactly
	// like Menu_Music.ogg already does via the real "/music" resources.cfg
	// entry.
	apResources->AddResourceDir(sCacheDir, false);
}

//---------------------------------------

tString cSomaMenuSfx::ChangeSound() { return gbSomaMenuSfxChangeOk ? "frontend_menu_change.ogg" : ""; }
tString cSomaMenuSfx::FocusSound() { return gbSomaMenuSfxFocusOk ? "frontend_menu_focus.ogg" : ""; }
tString cSomaMenuSfx::SelectSound() { return gbSomaMenuSfxSelectOk ? "frontend_menu_select.ogg" : ""; }
tString cSomaMenuSfx::SliderSound() { return gbSomaMenuSfxSliderOk ? "frontend_menu_slider.ogg" : ""; }
tString cSomaMenuSfx::NewGameSting() { return gbSomaMenuSfxNewGameStingOk ? "new_game_sting.wav" : ""; }
tString cSomaMenuSfx::FGLogoSting() { return gbSomaMenuSfxFGLogoStingOk ? "fg_logo_sting.wav" : ""; }
tString cSomaMenuSfx::MenuBgNoise() { return gbSomaMenuSfxMenuBgNoiseOk ? "menu_bg_noise.wav" : ""; }

int cSomaMenuSfx::GlitchSoundCount() { return 12; }

tString cSomaMenuSfx::GlitchSound(int alVariant)
{
	if (alVariant < 1 || alVariant > 12 || gabSomaMenuSfxGlitchOk[alVariant - 1] == false)
		return "";

	tString sNum = (alVariant < 10) ? (tString("0") + cString::ToString(alVariant)) : cString::ToString(alVariant);
	return "menu_glitch_" + sNum + ".ogg";
}
