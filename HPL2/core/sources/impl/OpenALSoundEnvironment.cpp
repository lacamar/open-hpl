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

#include "impl/OpenALSoundEnvironment.h"
#include "impl/tinyXML/tinyxml.h"

namespace hpl
{

	cOpenALSoundEnvironment::cOpenALSoundEnvironment ( ) /*:	
										mfDensity(1.0f), 
										mfDiffusion(1.0f),
										mfGain(0.32f),
										mfGainHF(0.89f),
										mfGainLF(0.0f),
										mfDecayTime(1.49f),
										mfDecayHFRatio(0.83f),
										mfDecayLFRatio(1.0f),
										mfReflectionsGain(0.05f),
										mfReflectionsDelay(0.007f),
										mfLateReverbGain(1.25f),
										mfLateReverbDelay(0.011f),
										mfEchoTime(0.25f),
										mfEchoDepth(0.0f),
										mfModulationTime(0.25f),
										mfModulationDepth(0.0f),
										mfAirAbsorptionGainHF(0.994f),
										mfHFReference(5000.0f),
										mfLFReference(250.0f),
										mfRoomRolloffFactor(0.0f),
										mbDecayHFLimit(true)*/
	{
	}

	cOpenALSoundEnvironment::~cOpenALSoundEnvironment()
	{
	}
		
	static void OhplL5EnvName(void* apCtx, const uint8_t* apName, size_t alLen)
	{
		((tString*)apCtx)->assign((const char*)apName, alLen);
	}

	void cOpenALSoundEnvironment::FromRust(const OhplL5EnvParams& p)
	{
		mfDensity = p.density; mfDiffusion = p.diffusion; mfGain = p.gain; mfGainHF = p.gain_hf; mfGainLF = p.gain_lf;
		mfDecayTime = p.decay_time; mfDecayHFRatio = p.decay_hf_ratio; mfDecayLFRatio = p.decay_lf_ratio;
		mfReflectionsGain = p.reflections_gain; mfReflectionsDelay = p.reflections_delay;
		for(int i=0;i<3;++i) { mfReflectionsPan[i] = p.reflections_pan[i]; mfLateReverbPan[i] = p.late_reverb_pan[i]; }
		mfLateReverbGain = p.late_reverb_gain; mfLateReverbDelay = p.late_reverb_delay;
		mfEchoTime = p.echo_time; mfEchoDepth = p.echo_depth; mfModulationTime = p.modulation_time; mfModulationDepth = p.modulation_depth;
		mfAirAbsorptionGainHF = p.air_absorption_gain_hf; mfHFReference = p.hf_reference; mfLFReference = p.lf_reference;
		mfRoomRolloffFactor = p.room_rolloff_factor; mbDecayHFLimit = p.decay_hf_limit;
	}

	void cOpenALSoundEnvironment::ToRust(OhplL5EnvParams& p)
	{
		p.density = mfDensity; p.diffusion = mfDiffusion; p.gain = mfGain; p.gain_hf = mfGainHF; p.gain_lf = mfGainLF;
		p.decay_time = mfDecayTime; p.decay_hf_ratio = mfDecayHFRatio; p.decay_lf_ratio = mfDecayLFRatio;
		p.reflections_gain = mfReflectionsGain; p.reflections_delay = mfReflectionsDelay;
		for(int i=0;i<3;++i) { p.reflections_pan[i] = mfReflectionsPan[i]; p.late_reverb_pan[i] = mfLateReverbPan[i]; }
		p.late_reverb_gain = mfLateReverbGain; p.late_reverb_delay = mfLateReverbDelay;
		p.echo_time = mfEchoTime; p.echo_depth = mfEchoDepth; p.modulation_time = mfModulationTime; p.modulation_depth = mfModulationDepth;
		p.air_absorption_gain_hf = mfAirAbsorptionGainHF; p.hf_reference = mfHFReference; p.lf_reference = mfLFReference;
		p.room_rolloff_factor = mfRoomRolloffFactor; p.decay_hf_limit = mbDecayHFLimit;
	}

	bool cOpenALSoundEnvironment::CreateFromFile(const tString &asFile)
	{
		if(OhplBackendRustRequested())
		{
			OhplL5EnvParams params;
			tString sName;
			if(!ohpl_l5_env_load(asFile.c_str(), &params, OhplL5EnvName, &sName)) return false;
			mstrName = sName;
			FromRust(params);
			return true;
		}
		tString strType;
		TiXmlDocument doc;
		if (!doc.LoadFile(asFile.c_str()))
			return false;

		TiXmlElement* pMain = doc.FirstChildElement("SoundEnvironment")->FirstChildElement("Main");
		if (pMain)
		{
			strType = pMain->Attribute("Type");
			mstrName = pMain->Attribute("Name");
		}


		float* pfTemp;

		TiXmlElement* pParams = doc.FirstChildElement("SoundEnvironment")->FirstChildElement("Parameters");

		if ( (pParams == NULL) || (strType.compare("OpenAL")!=0) )
		{
			doc.Clear();
			return false;
		}

        mfDensity = cString::ToFloat(pParams->Attribute("Density"),0);
		mfDiffusion = cString::ToFloat(pParams->Attribute("Diffusion"),0);
		mfGain = cString::ToFloat(pParams->Attribute("Gain"),0);
		mfGainHF = cString::ToFloat(pParams->Attribute("GainHF"),0);
		mfGainLF = cString::ToFloat(pParams->Attribute("GainLF"),0); 
		mfDecayTime = cString::ToFloat(pParams->Attribute("DecayTime"),0);
		mfDecayHFRatio = cString::ToFloat (pParams->Attribute("DecayHFRatio"),0);
		mfDecayLFRatio = cString::ToFloat (pParams->Attribute("DecayLFRatio"),0);
		mfReflectionsGain = cString::ToFloat(pParams->Attribute("ReflectionsGain"),0);
		mfReflectionsDelay = cString::ToFloat(pParams->Attribute("ReflectionsDelay"),0);
		pfTemp = cString::ToVector3f(pParams->Attribute("ReflectionsPan"),cVector3f(0)).v;
		mfReflectionsPan[0] = pfTemp[0];
		mfReflectionsPan[1] = pfTemp[1];
		mfReflectionsPan[2] = pfTemp[2];
		mfLateReverbGain = cString::ToFloat(pParams->Attribute("LateReverbGain"),0);
		mfLateReverbDelay = cString::ToFloat(pParams->Attribute("LateReverbDelay"),0);
		pfTemp = cString::ToVector3f(pParams->Attribute("LateReverbPan"),cVector3f(0)).v;
		mfLateReverbPan[0] = pfTemp[0];
		mfLateReverbPan[1] = pfTemp[1];
		mfLateReverbPan[2] = pfTemp[2];
		mfEchoTime = cString::ToFloat(pParams->Attribute("EchoTime"),0);
		mfEchoDepth = cString::ToFloat(pParams->Attribute("EchoDepth"),0);
		mfModulationTime = cString::ToFloat(pParams->Attribute("ModulationTime"),0);
		mfModulationDepth = cString::ToFloat(pParams->Attribute("ModulationDepth"),0);
		mfAirAbsorptionGainHF = cString::ToFloat(pParams->Attribute("AirAbsorptionGainHF"),0);
		mfHFReference = cString::ToFloat(pParams->Attribute("HFReference"),0);
		mfLFReference = cString::ToFloat(pParams->Attribute("LFReference"),0);
		mfRoomRolloffFactor =cString::ToFloat(pParams->Attribute("RoomRolloffFactor"),0);
		mbDecayHFLimit = cString::ToInt(pParams->Attribute("DecayHFLimit"),0);
		
		doc.Clear();
		pParams = NULL;

		return true;
	}

};
