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

	if (pCfg->Save() == false)
		Log("SOMA: failed to save settings to '%s'\n", cString::To8Char(sFile).c_str());

	hplDelete(pCfg);
}
