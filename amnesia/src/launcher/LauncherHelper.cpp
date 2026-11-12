/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "LauncherHelper.h"
#include "hpl.h"
#include "launcher.h"
#include "RustLauncher.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>


#ifdef USERDIR_RESOURCES
extern tWString gsUserResourceDir;
#endif

using namespace hpl;

static std::vector<OhplLnMode> ToLnModes(const tVideoModeVec& avModes)
{
	std::vector<OhplLnMode> vModes;
	for(size_t i = 0; i < avModes.size(); ++i)
	{
		OhplLnMode mode = {avModes[i].mlDisplay, avModes[i].mvScreenSize.x, avModes[i].mvScreenSize.y};
		vModes.push_back(mode);
	}
	return vModes;
}

static void SelectByUserData(Fl_Choice* apChoice, int alWanted)
{
	std::vector<int> vValues;
	for(int i = 0; i < apChoice->size(); ++i)
		vValues.push_back((int)(long int)apChoice->menu()[i].user_data());
	apChoice->value(ohpl_launcher_menu_index(vValues.data(), vValues.size(), alWanted));
}

static void SelectByUserDataIndex(Fl_Choice* apRes, int alIndex)
{
	const Fl_Menu_Item *item = apRes->menu();
	for (int i=1; i<(int)apRes->size(); ++i,++item) {
		if ((long int)item->user_data()==alIndex) {
			apRes->value(item);
			break;
		}
	}
}

//------------------------------------------------------------------------------

void cLauncherHelper::SetWidgetLabel(Fl_Widget* apWidget, const tWString& asLabel)
{
	tString sUTF8Label = cString::S16BitToUTF8(asLabel);
	
	apWidget->copy_label(sUTF8Label.c_str());
}

//------------------------------------------------------------------------------

void cLauncherHelper::SetMenuItemLabel(Fl_Menu_Item* apItem, const tWString &asLabel, tStringList& alstLabels)
{
	tString sUTF8Label = cString::S16BitToUTF8(asLabel);
	
	alstLabels.push_back(sUTF8Label);
	apItem->label(alstLabels.back().c_str());
}

//------------------------------------------------------------------------------

tVideoModeVec cLauncherHelper::GetAvailableVideoModes(cConfigFile* apFile)
{
	tVideoModeVec vVidModes;
	cPlatform::GetAvailableVideoModes(vVidModes, 32);

	OhplLnCfg cfg(apFile);
	std::vector<OhplLnMode> vModes = ToLnModes(vVidModes);
	if(ohpl_launcher_fix_config_size(cfg.Ops(), vModes.data(), vModes.size()))
	{
		OhplLnMode cur;
		ohpl_launcher_configured_mode(cfg.Ops(), &cur);
		vVidModes.push_back(cVideoMode(cVector2l(cur.width, cur.height)));
	}

	return vVidModes;
}
//------------------------------------------------------------------------------

tStringVec cLauncherHelper::GetAvailableLanguages(const tString& asGameLangFolder)
{
	tStringVec vLangFiles;
	tWStringList lstLangs;
	tWString sPath = cString::To16Char(asGameLangFolder);
	tString sDir;
#ifdef USERDIR_RESOURCES
	tString sUserDir = cString::To8Char(gsUserResourceDir);
	bool bUserExists = cPlatform::FileExists(gsUserResourceDir + sPath);
	ohpl_launcher_languages_dir(OhplLnS(asGameLangFolder), OhplLnS(sUserDir), bUserExists, OhplLnStrSink, &sDir);
	sPath = cString::To16Char(sDir);
#endif
	cPlatform::FindFilesInDir(lstLangs, sPath, _W("*.lang"));

	tWStringListIt it = lstLangs.begin();
	for(;it!=lstLangs.end();++it)
	{
		tWString sLang = *it;

		vLangFiles.push_back(cString::To8Char(sLang));
	}

	return vLangFiles;
}
//------------------------------------------------------------------------------

tSoundDeviceVec cLauncherHelper::GetAvailableSoundDevices()
{
	////////////////////////////////////
	// Retrieve sound devices
	#if defined(WIN32)
		iLowLevelSound::SetSoundDeviceNameFilter("software");
	#endif

	return iLowLevelSound::GetFilteredSoundDevices();
}

//------------------------------------------------------------------------------

int cLauncherHelper::GetEqualPresetIndex(cConfigFile* apConfig, const std::vector<cConfigFile*>& avPresets)
{
	std::vector<OhplLnCfg> vPresets;
	for(size_t i = 0; i < avPresets.size(); ++i)
		vPresets.push_back(OhplLnCfg(avPresets[i]));
	std::vector<const OhplLnCfgOps*> vOps;
	for(size_t i = 0; i < vPresets.size(); ++i)
		vOps.push_back(vPresets[i].Ops());
	OhplLnCfg cfg(apConfig);
	return ohpl_launcher_equal_preset_index(cfg.Ops(), vOps.data(), vOps.size());
}
//------------------------------------------------------------------------------

bool cLauncherHelper::EqualsPreset(cConfigFile* apFile, cConfigFile* apPresetFile)
{
	OhplLnCfg file(apFile), preset(apPresetFile);
	return ohpl_launcher_equals_preset(file.Ops(), preset.Ops()) != 0;
}
//------------------------------------------------------------------------------

void cLauncherHelper::UpdateConfigFile(cUserInterface* apUI, cConfigFile* apFile,
									   const tString& asLevel, const tString& asEntry, const tString& asValue, bool abCheckEqualsPreset)
{
	apFile->SetString(asLevel, asEntry, asValue);
	if(abCheckEqualsPreset) apUI->CheckEqualsPreset();
}

//------------------------------------------------------------------------------

void cLauncherHelper::UpdateConfigFile(cUserInterface* apUI, cConfigFile* apFile,
									   const tString& asLevel, const tString& asEntry, bool abValue, bool abCheckEqualsPreset)
{
	apFile->SetBool(asLevel, asEntry, abValue);
	if(abCheckEqualsPreset) apUI->CheckEqualsPreset();
}

//------------------------------------------------------------------------------

void cLauncherHelper::UpdateConfigFile(cUserInterface* apUI, cConfigFile* apFile, 
									   const tString& asLevel, const tString& asEntry, int alValue, bool abCheckEqualsPreset)
{
	apFile->SetInt(asLevel, asEntry, alValue);
	if(abCheckEqualsPreset) apUI->CheckEqualsPreset();
}

//------------------------------------------------------------------------------

void cLauncherHelper::UpdateConfigFile(cUserInterface* apUI, cConfigFile* apFile, 
									   const tString& asLevel, const tString& asEntry, float afValue, bool abCheckEqualsPreset)
{
	apFile->SetFloat(asLevel, asEntry, afValue);
	if(abCheckEqualsPreset) apUI->CheckEqualsPreset();
}

//------------------------------------------------------------------------------

void cLauncherHelper::UpdateConfigFileWithPreset(cConfigFile* apFile, cConfigFile* apPreset)
{
	OhplLnCfg file(apFile), preset(apPreset);
	ohpl_launcher_apply_preset(file.Ops(), preset.Ops());
}
//------------------------------------------------------------------------------

void cLauncherHelper::PopulateLanguages(const tStringVec& avLangFiles, const tString& asDefaultLang, cConfigFile* apConfig, Fl_Choice* apLangs)
{
	std::vector<OhplLnStr> vFiles;
	for(size_t i=0;i<avLangFiles.size();++i)
	{
		const tString& sLangFile = avLangFiles[i];
		vFiles.push_back(OhplLnS(sLangFile));

		tString sLangName;
		ohpl_launcher_language_name((const unsigned char*)sLangFile.data(), sLangFile.size(), OhplLnStrSink, &sLangName);
		tString sLangEntry = cString::S16BitToUTF8(kTranslate("Languages", sLangName));
		apLangs->add(sLangEntry.c_str());
	}

	OhplLnCfg cfg(apConfig);
	apLangs->value(ohpl_launcher_language_index(cfg.Ops(), vFiles.data(), vFiles.size(), OhplLnS(asDefaultLang)));
}
//------------------------------------------------------------------------------

void cLauncherHelper::cb_AddResolution(Fl_Widget* o, void* v) 
{
	cUserInterface* ui = (cUserInterface*)v;
	ui->AddResolutionWindow->show();
}

//------------------------------------------------------------------------------

bool cLauncherHelper::AddCustomResolution(tVideoModeVec& avVidModes, cConfigFile* apConfig, Fl_Choice* apRes, cUserInterface* apUI, int apWidth, int apHeight)
{
	std::vector<OhplLnMode> vModes = ToLnModes(avVidModes);
	int lRow = -1;
	int lAction = ohpl_launcher_custom_resolution(vModes.data(), vModes.size(), apWidth, apHeight, &lRow);
	if (lAction == 0) {
		return false;
	}

	UpdateConfigFile(apUI, apConfig, "Screen", "Width", apWidth);
	UpdateConfigFile(apUI, apConfig, "Screen", "Height", apHeight);
	UpdateConfigFile(apUI, apConfig, "Screen", "Display", 0);

	if (lAction == 1) {
		const Fl_Menu_Item *item = apRes->menu();
		for (int i=1; i<(int)apRes->size(); ++i,++item) {
			if ((long int)item->user_data()==lRow) {
				apRes->value(item);
				return true;
			}
		}
	}

	cVideoMode vMode(cVector2l(apWidth, apHeight));
	avVidModes.push_back(vMode);
	PopulateResolutions(avVidModes, apConfig, apRes, apUI);

	return true;
}
//------------------------------------------------------------------------------

void cLauncherHelper::ReselectResolution(const tVideoModeVec& avVidModes, cConfigFile* apConfig, Fl_Choice* apRes)
{
	OhplLnCfg cfg(apConfig);
	OhplLnMode cur;
	ohpl_launcher_configured_mode(cfg.Ops(), &cur);
	std::vector<OhplLnMode> vModes = ToLnModes(avVidModes);
	SelectByUserDataIndex(apRes, ohpl_launcher_last_match(vModes.data(), vModes.size(), cur));
}
//------------------------------------------------------------------------------

void cLauncherHelper::PopulateResolutions(const tVideoModeVec& avVidModes, cConfigFile* apConfig, Fl_Choice* apRes, cUserInterface* apUI)
{
	OhplLnCfg cfg(apConfig);
	OhplLnMode cur;
	ohpl_launcher_configured_mode(cfg.Ops(), &cur);
	std::vector<OhplLnMode> vModes = ToLnModes(avVidModes);

	apRes->menu(NULL);

	bool bMultiDisplay = ohpl_launcher_multi_display(vModes.data(), vModes.size()) != 0;
	int lItemPos = -1;

	for(int i=0; i<(int)avVidModes.size(); ++i)
	{
		const cVideoMode& vMode = avVidModes[i];

		tString sFullScreenDesktop = cString::S16BitToUTF8(kTranslate("OptionsMenu", "FullScreenDesktop"));
		tString sDisplayName = cString::S16BitToUTF8(cPlatform::GetDisplayName(vMode.mlDisplay));
		tString sCustom = cString::S16BitToUTF8(tWString(kTranslate("Launcher", "Custom")));
		tString sModeStr;
		ohpl_launcher_mode_label(vMode.isFullScreenDesktop(), OhplLnS(sFullScreenDesktop), OhplLnS(sDisplayName),
								 vMode.mvScreenSize.x, vMode.mvScreenSize.y, vMode.mbCustom, OhplLnS(sCustom),
								 bMultiDisplay, OhplLnStrSink, &sModeStr);

		lItemPos = apRes->add(sModeStr.c_str(), 0, NULL, (void*)(long)i);
	}

	if(lItemPos>-1) {
		apRes->mode(lItemPos,FL_MENU_DIVIDER);
	}
	apRes->add("(Add Custom...)", 0, (Fl_Callback*)cb_AddResolution, (void*)apUI);

	apRes->value(ohpl_launcher_populate_selection(ohpl_launcher_last_match(vModes.data(), vModes.size(), cur), cur.width, cur.height));
}
//------------------------------------------------------------------------------

void cLauncherHelper::PopulateSoundDevices(const tSoundDeviceVec& avSoundDevices, iSoundDeviceIdentifier* apCurrentDev, cConfigFile* apConfig, Fl_Choice* apSndDevs)
{
	std::vector<OhplLnDevice> vDevs;
	for(int i=0;i<(int)avSoundDevices.size();++i)
	{
		iSoundDeviceIdentifier* pDev = avSoundDevices[i];

		tString sDev;
		ohpl_launcher_escape_device((const unsigned char*)pDev->GetName().data(), pDev->GetName().size(), OhplLnStrSink, &sDev);
		apSndDevs->add(sDev.c_str());

		OhplLnDevice dev = {pDev->IsDefault(), apCurrentDev==pDev};
		vDevs.push_back(dev);
	}

	OhplLnCfg cfg(apConfig);
	int lAppend = 0;
	int lIndex = ohpl_launcher_sound_selection(cfg.Ops(), vDevs.data(), vDevs.size(), &lAppend);
	if(lAppend)
	{
		tString sDev = "(" + cString::S16BitToUTF8(tWString(kTranslate("Launcher", "Unsupported"))) +") " + apCurrentDev->GetName();
		apSndDevs->add(sDev.c_str());
	}

	apSndDevs->value(lIndex);
}
//------------------------------------------------------------------------------

void cLauncherHelper::SetInitialValues(cConfigFile* apConfig, cUserInterface* apUI)
{
	OhplLnCfg cfg(apConfig);
	OhplLnInitial v;
	ohpl_launcher_initial(cfg.Ops(), &v);

	apUI->ChBFullScreen->value(v.fullscreen);
	apUI->CBShadowQ->value(v.shadow_quality);
	apUI->CBShadowRes->value(v.shadow_resolution);
	apUI->ChBWaterReflect->value(v.reflection);
	apUI->ChBSSAO->value(v.ssao);
	apUI->CBSSAORes->value(v.ssao_resolution);
	SelectByUserData(apUI->CBSSAOSamples, v.ssao_samples);
	apUI->CBTexQ->value(ohpl_launcher_texture_quality_row(apUI->CBTexQ->size(), v.texture_quality));
	apUI->CBTexFilter->value(v.texture_filter);
	SelectByUserData(apUI->CBAnisotropy, v.anisotropy);
	apUI->ChBEdgeSmooth->value(v.edge_smooth);
	apUI->CBParallax->value(v.parallax);
}
//------------------------------------------------------------------------------

void cLauncherHelper::SetUpWidgetLabels(cUserInterface* apUI, tStringList& alstLabels)
{
	alstLabels.clear();

	cLauncherHelper::SetWidgetLabel(apUI->TabGeneral, kTranslate("Launcher", "TabGeneral"));

	cLauncherHelper::SetWidgetLabel(apUI->CBQPreset, kTranslate("Launcher", "Quality"));
	cLauncherHelper::SetMenuItemLabel(apUI->MIQPLow, kTranslate("OptionsMenu", "Low"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MIQPMed, kTranslate("OptionsMenu", "Medium"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MIQPHigh, kTranslate("OptionsMenu", "High"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MIQPCustom, kTranslate("Launcher", "Custom"), alstLabels);

	cLauncherHelper::SetWidgetLabel(apUI->BDetectSettings, kTranslate("Launcher", "DetectSettings"));

	cLauncherHelper::SetWidgetLabel(apUI->CBLanguage, kTranslate("OptionsMenu", "Language"));

	cLauncherHelper::SetWidgetLabel(apUI->BShowAdvanced, kTranslate("Launcher", "ShowAdvanced"));
	cLauncherHelper::SetWidgetLabel(apUI->BShowBasic, kTranslate("Launcher", "ShowBasic"));

	cLauncherHelper::SetWidgetLabel(apUI->CBResolution, kTranslate("OptionsMenu", "Resolution"));
	cLauncherHelper::SetWidgetLabel(apUI->ChBFullScreen, kTranslate("OptionsMenu", "FullScreen"));

	cLauncherHelper::SetWidgetLabel(apUI->CBShadowQ, kTranslate("Launcher", "ShadowQuality"));
	cLauncherHelper::SetMenuItemLabel(apUI->MISQLow, kTranslate("OptionsMenu", "Low"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MISQMed, kTranslate("OptionsMenu", "Medium"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MISQHigh, kTranslate("OptionsMenu", "High"), alstLabels);

	cLauncherHelper::SetWidgetLabel(apUI->CBShadowRes, kTranslate("Launcher", "ShadowResolution"));
	cLauncherHelper::SetMenuItemLabel(apUI->MISRLow, kTranslate("OptionsMenu", "Low"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MISRMed, kTranslate("OptionsMenu", "Medium"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MISRHigh, kTranslate("OptionsMenu", "High"), alstLabels);



	cLauncherHelper::SetWidgetLabel(apUI->CBTexQ, kTranslate("OptionsMenu", "TexQuality"));
	cLauncherHelper::SetMenuItemLabel(apUI->MITQLow, kTranslate("OptionsMenu", "Low"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MITQMed, kTranslate("OptionsMenu", "Medium"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MITQHigh, kTranslate("OptionsMenu", "High"), alstLabels);

	cLauncherHelper::SetWidgetLabel(apUI->CBAnisotropy, kTranslate("OptionsMenu", "Anisotropy"));
	cLauncherHelper::SetMenuItemLabel(apUI->MIAnisotropyOff, kTranslate("Launcher", "Off"), alstLabels);

	cLauncherHelper::SetWidgetLabel(apUI->CBSSAORes, kTranslate("Launcher", "SSAORes"));
	cLauncherHelper::SetMenuItemLabel(apUI->MISSAOResMed, kTranslate("OptionsMenu", "Medium"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MISSAOResHigh, kTranslate("OptionsMenu", "High"), alstLabels);

	tWString sSSAOSamplesLabel = _W("SSAO ") + tWString(kTranslate("OptionsMenu", "SSAOSamples"));
	cLauncherHelper::SetWidgetLabel(apUI->CBSSAOSamples, sSSAOSamplesLabel);

	cLauncherHelper::SetWidgetLabel(apUI->ChBEdgeSmooth, kTranslate("Launcher", "EdgeSmooth"));

	cLauncherHelper::SetWidgetLabel(apUI->CBTexFilter, kTranslate("OptionsMenu", "TexFilter"));
	cLauncherHelper::SetMenuItemLabel(apUI->MITFilterNearest, kTranslate("OptionsMenu", "Nearest"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MITFilterBilinear, kTranslate("OptionsMenu", "Bilinear"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MITFilterTrilinear, kTranslate("OptionsMenu", "Trilinear"), alstLabels);

	cLauncherHelper::SetWidgetLabel(apUI->ChBWaterReflect, kTranslate("OptionsMenu", "WorldReflection"));
	cLauncherHelper::SetWidgetLabel(apUI->CBParallax, kTranslate("Launcher", "Parallax"));
	cLauncherHelper::SetMenuItemLabel(apUI->MIParallaxOff, kTranslate("Launcher", "Off"), alstLabels);
	cLauncherHelper::SetMenuItemLabel(apUI->MIParallaxOn, kTranslate("Launcher", "On"), alstLabels);


	cLauncherHelper::SetWidgetLabel(apUI->CBSoundDevices, kTranslate("Launcher", "SoundDevice"));

	cLauncherHelper::SetWidgetLabel(apUI->BLaunchGame, kTranslate("Launcher", "LaunchGame"));
	cLauncherHelper::SetWidgetLabel(apUI->BCancel, kTranslate("Global", "Cancel"));

	cLauncherHelper::SetWidgetLabel(apUI->TabGraphics,kTranslate("Launcher", "TabGraphics"));
	cLauncherHelper::SetWidgetLabel(apUI->LabelVideoDevice, kTranslate("Launcher", "VideoDevice"));
	cLauncherHelper::SetWidgetLabel(apUI->TabSound,kTranslate("Launcher", "TabSound"));
}

//------------------------------------------------------------------------------

void cLauncherHelper::DetectSettings(cUserInterface* apUI, cConfigFile* apConfig, const std::vector<cConfigFile*>& avPresets,
									 cQualityChooser* apChooser, const tString& asCardString)
{
	tString sClosestMatch;
	int lRating = apChooser->GetQualityRatingByCardString(asCardString, sClosestMatch);

	OhplLnDetect plan;
	ohpl_launcher_detect(lRating, sClosestMatch.empty() == false, &plan);

	if(plan.show_closest)
	{
		sClosestMatch = cString::S16BitToUTF8(kTranslate("Launcher", "UnlistedVCardClosestMatch")) + " " + sClosestMatch;
		fl_message("%s", sClosestMatch.c_str());
	}

	if(plan.show_unknown)
	{
		tString sMsg = cString::S16BitToUTF8(kTranslate("Launcher", "UnlistedVCard"));
		fl_message("%s", sMsg.c_str());
	}

	if(plan.show_unsupported)
	{
		tString sMsg = cString::S16BitToUTF8(kTranslate("Launcher", "UnsupportedVCard"));
		fl_message("%s", sMsg.c_str());
	}
	else if(plan.preset >= 0)
	{
		cLauncherHelper::UpdateConfigFileWithPreset(apConfig, avPresets[plan.preset]);

		tVideoModeVec vResolutions = cLauncherHelper::GetAvailableVideoModes(apConfig);

		int lWidth, lHeight;
		cPlatform::GetDisplayResolution(0, lWidth, lHeight);
		UpdateConfigFile(apUI, apConfig, "Screen", "Width", lWidth);
		UpdateConfigFile(apUI, apConfig, "Screen", "Height", lHeight);
		UpdateConfigFile(apUI, apConfig, "Screen", "Display", vResolutions.back().mlDisplay);

		ReselectResolution(vResolutions, apConfig, apUI->CBResolution);

		apUI->CheckEqualsPreset();
		SetInitialValues(apConfig, apUI);
	}
}
//------------------------------------------------------------------------------

AspectRatio cLauncherHelper::AspectRatioForResolution(int w, int h) {
	int vRatio[2];
	ohpl_launcher_aspect_ratio(w, h, vRatio);
	return AspectRatio(vRatio[0], vRatio[1]);
}
