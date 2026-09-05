/*
 * SOMA's real menu click/hover/glitch/sting sound effects - see the top
 * comment in SomaMenuSfx.cpp for the full background (this session's
 * bounded reverse-engineering attempt at problem 3 from the menu-polish
 * brief: "there are meant to be sounds when clicking or hovering over menu
 * options, but there are none").
 */

#ifndef SOMA_MENU_SFX_H
#define SOMA_MENU_SFX_H

#include "hpl.h"

using namespace hpl;

//----------------------------------------------

class cSomaMenuSfx
{
public:
	// Locates the real special_fx FMOD Studio/Designer banks
	// (sounds/special/special_fx.fsb + special_fx_stream.fsb) via
	// apResources' existing file searcher, converts whichever of the real
	// named events below it can find/decode into plain .ogg/.wav files
	// under $XDG_CACHE_HOME/open-hpl/soma/sfx/ (skipping any that were
	// already converted by a previous run), and registers that cache
	// directory as an extra resource dir so plain
	// cSoundHandler::PlayGui("name.ogg") calls resolve exactly like
	// Menu_Music.ogg already does. Idempotent and safe to call even if the
	// real SOMA install/bank files can't be found at all (logs a warning
	// and leaves every *Sound()/GlitchSound() accessor below returning ""
	// - callers must check before calling PlayGui, same contract as a
	// missing texture already has elsewhere in this engine).
	static void EnsureCached(cResources *apResources);

	// Plain filenames to hand cSoundHandler::PlayGui(), matching the real
	// Sound_PlayGui() event names in script/modules/MenuHandler.hps /
	// helper_imgui_options.hps 1:1 (see SomaMainMenu.cpp's call sites for
	// exactly which real UI action fires which of these). Empty ("") if
	// EnsureCached() hasn't run yet or that particular sample failed to
	// convert.
	static tString ChangeSound();	// frontend_menu_change - category/back navigation click
	static tString FocusSound();	// frontend_menu_focus - hover
	static tString SelectSound();	// frontend_menu_select - Options toggle/multi-select click
	static tString SliderSound();	// frontend_menu_slider - Options slider drag/step
	static tString NewGameSting();	// new_game_sting - New Game confirmed

	// Added for cSomaSplash (see SomaSplash.h/.cpp) - real sample names
	// confirmed via `strings` on sounds/special/special_fx_stream.fsb,
	// resolving this file's own "FG_Menu_Sting ... may resolve to
	// FG_Logo_Sting" doc comment above.
	static tString FGLogoSting();	// FG_Logo_Sting - real sample behind Sound_PlayGui("special_fx/frontend/FG_Menu_Sting", ...)
	static tString MenuBgNoise();	// menu_bg_noise - real sample behind Sound_CreateAtEntity(..., "special_fx/frontend/main_menu_bg", ...)

	// menu_glitch_001..012 - real script picks one via PlayGui("...menu_glitch")
	// which is itself an FMOD event with 12 layered/randomised wave
	// variants; this port doesn't have an FMOD event-random-container, so
	// the caller (DrawTitle()'s glitch-burst state machine) picks one
	// variant itself. alVariant is 1-12; returns "" if that variant didn't
	// convert.
	static tString GlitchSound(int alVariant);
	static int GlitchSoundCount();
};

#endif // SOMA_MENU_SFX_H
