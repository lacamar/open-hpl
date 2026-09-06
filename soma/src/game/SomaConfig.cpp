/*
 * See SomaConfig.h for scope notes.
 */

#include "SomaConfig.h"

//---------------------------------------

cSomaConfig::cSomaConfig()
{
	mfMasterVolume = 1.0f;
	mfGamma = 1.0f;
	mbVSync = false;
	mbFullscreen = false;

	// Same defaults cSomaBase::InitEngine() hardcoded before this class had
	// fields for them.
	mlScreenWidth = 1280;
	mlScreenHeight = 720;

	// Real vAAValues default index (helper_imgui_options.hps'
	// mpConfig.GetString("Graphics","AntiAliasing","FXAA")) is FXAA on.
	mbAntiAliasing = true;

	mfFOV = 70.0f;
	mbShowSubtitles = true;
	mfMouseSensitivity = 1.0f;
	mbInvertMouseY = false;

	// Real iKeyboard::KeyToString() names (see Keyboard.cpp) - matches
	// exactly what cSomaBase::CreateInputActions() would fall back to
	// anyway if these were left empty, but spelling them out here means a
	// config file written before Load() ever runs (e.g. by a test) still
	// shows the real intended default.
	msKeyForward = "W";
	msKeyBackward = "S";
	msKeyLeft = "A";
	msKeyRight = "D";
	msKeyJump = "Space";
}

//-----------------------------------------------------------------------

tWString cSomaConfig::GetConfigFilePath()
{
	// Same "create each path segment if missing" pattern
	// cSomaBase::InitEngine() already uses for its XDG state directory,
	// just rooted at XDGConfigHome instead of XDGStateHome (settings, not
	// transient log/state data).
	tWString sConfigRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGConfigHome);
	tWString sDir = sConfigRoot + _W("open-hpl/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);
	sDir += _W("soma/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);

	return sDir + _W("main_settings.cfg");
}

//-----------------------------------------------------------------------

void cSomaConfig::Load()
{
	tWString sFile = GetConfigFilePath();

	cConfigFile *pCfg = hplNew(cConfigFile, (sFile));
	if (pCfg->Load() == false)
	{
		// Expected on a fresh install - no file yet, keep the constructor
		// defaults above rather than treating this as an error.
		Log("SOMA: no settings file yet at '%s', using defaults\n", cString::To8Char(sFile).c_str());
		hplDelete(pCfg);
		return;
	}

	mfMasterVolume = pCfg->GetFloat("Sound", "Volume", mfMasterVolume);
	mfGamma = pCfg->GetFloat("Graphics", "Gamma", mfGamma);
	mbVSync = pCfg->GetBool("Screen", "Vsync", mbVSync);
	mbFullscreen = pCfg->GetBool("Screen", "FullScreen", mbFullscreen);
	mlScreenWidth = pCfg->GetInt("Screen", "Width", mlScreenWidth);
	mlScreenHeight = pCfg->GetInt("Screen", "Height", mlScreenHeight);
	mbAntiAliasing = pCfg->GetBool("Graphics", "AntiAliasing", mbAntiAliasing);
	mfFOV = pCfg->GetFloat("Gameplay", "FOV", mfFOV);
	mbShowSubtitles = pCfg->GetBool("Sound", "ShowSubtitles", mbShowSubtitles);
	mfMouseSensitivity = pCfg->GetFloat("Input", "MouseSensitivity", mfMouseSensitivity);
	mbInvertMouseY = pCfg->GetBool("Input", "InvertMouse", mbInvertMouseY);
	msKeyForward = pCfg->GetString("Input", "KeyForward", msKeyForward);
	msKeyBackward = pCfg->GetString("Input", "KeyBackward", msKeyBackward);
	msKeyLeft = pCfg->GetString("Input", "KeyLeft", msKeyLeft);
	msKeyRight = pCfg->GetString("Input", "KeyRight", msKeyRight);
	msKeyJump = pCfg->GetString("Input", "KeyJump", msKeyJump);

	hplDelete(pCfg);
}

//-----------------------------------------------------------------------

void cSomaConfig::Save()
{
	tWString sFile = GetConfigFilePath();

	cConfigFile *pCfg = hplNew(cConfigFile, (sFile));

	pCfg->SetFloat("Sound", "Volume", mfMasterVolume);
	pCfg->SetFloat("Graphics", "Gamma", mfGamma);
	pCfg->SetBool("Screen", "Vsync", mbVSync);
	pCfg->SetBool("Screen", "FullScreen", mbFullscreen);
	pCfg->SetInt("Screen", "Width", mlScreenWidth);
	pCfg->SetInt("Screen", "Height", mlScreenHeight);
	pCfg->SetBool("Graphics", "AntiAliasing", mbAntiAliasing);
	pCfg->SetFloat("Gameplay", "FOV", mfFOV);
	pCfg->SetBool("Sound", "ShowSubtitles", mbShowSubtitles);
	pCfg->SetFloat("Input", "MouseSensitivity", mfMouseSensitivity);
	pCfg->SetBool("Input", "InvertMouse", mbInvertMouseY);
	pCfg->SetString("Input", "KeyForward", msKeyForward);
	pCfg->SetString("Input", "KeyBackward", msKeyBackward);
	pCfg->SetString("Input", "KeyLeft", msKeyLeft);
	pCfg->SetString("Input", "KeyRight", msKeyRight);
	pCfg->SetString("Input", "KeyJump", msKeyJump);

	if (pCfg->Save() == false)
		Log("SOMA: failed to save settings to '%s'\n", cString::To8Char(sFile).c_str());

	hplDelete(pCfg);
}
