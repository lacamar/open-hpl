#include "SomaConfig.h"

tWString cSomaConfig::GetConfigFilePath()
{
	tWString sDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGConfigHome) + _W("open-hpl/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);
	sDir += _W("soma/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);

	return sDir + _W("main_settings.cfg");
}

void cSomaConfig::Load()
{
	tWString sFile = GetConfigFilePath();

	cConfigFile *pCfg = hplNew(cConfigFile, (sFile));
	if (pCfg->Load() == false)
	{
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
	mbShowSubtitles = pCfg->GetBool("Sound", "ShowSubtitles", mbShowSubtitles);

	hplDelete(pCfg);
}

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
	pCfg->SetBool("Sound", "ShowSubtitles", mbShowSubtitles);

	if (pCfg->Save() == false)
		Log("SOMA: failed to save settings to '%s'\n", cString::To8Char(sFile).c_str());

	hplDelete(pCfg);
}
