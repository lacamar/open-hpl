// Map-authored ambient sound entities (.hpm_Sound) name FMOD events, and SOMA ships no .snt
// for them. The samples are extracted from their FSB5 banks and matching .snt sidecars are
// synthesized into a cache resource dir, so the stock sound-entity loader finds them.
// Spot-sound Interval/Random values are guesses (the .fev timing is not parsed); both dog
// entities share one sample pool, the only one 00_06_lab.fsb has.

#include "SomaAmbientSfx.h"
#include "SomaFsb.h"

//---------------------------------------
// Extraction recipes - one row per real sample this fix needs, grouped by
// which real bank (and which encoding) it comes from.

// 00_00_intro.hps' OnStart plays Sound_PlayGui("00_05_apartment2/SFX/game_intro_seq"),
// the intro slideshow's whole ambience/score bed. One mode=2 (PCM16) sample, ~27 MB.
static const cSomaFsbWanted kPcmBank_00_05_apartment2_streamvip[] = {
	{"game_intro_seq", "game_intro_seq.wav"},
};

static const cSomaFsbWanted kPcmBank_00_06_lab[] = {
	{"car_drive_01", "car_drive_01.wav"}, {"car_drive_02", "car_drive_02.wav"},
	{"car_drive_03", "car_drive_03.wav"}, {"car_drive_04", "car_drive_04.wav"},
	{"car_drive_05", "car_drive_05.wav"}, {"car_drive_06", "car_drive_06.wav"},
	{"car_drive_07", "car_drive_07.wav"}, {"car_drive_08", "car_drive_08.wav"},
	{"car_drive_09", "car_drive_09.wav"}, {"car_drive_10", "car_drive_10.wav"},
	{"distant_dog_bark_11", "distant_dog_bark_11.wav"}, {"distant_dog_bark_12", "distant_dog_bark_12.wav"},
	{"distant_dog_bark_13", "distant_dog_bark_13.wav"}, {"distant_dog_bark_14", "distant_dog_bark_14.wav"},
	{"distant_dog_bark_15", "distant_dog_bark_15.wav"}, {"distant_dog_bark_16", "distant_dog_bark_16.wav"},
	{"distant_dog_bark_17", "distant_dog_bark_17.wav"}, {"distant_dog_bark_18", "distant_dog_bark_18.wav"},
	{"distant_dog_bark_19", "distant_dog_bark_19.wav"},
	{"urban_seagull_01", "urban_seagull_01.wav"}, {"urban_seagull_02", "urban_seagull_02.wav"},
	{"urban_seagull_03", "urban_seagull_03.wav"}, {"urban_seagull_04", "urban_seagull_04.wav"},
	{"urban_seagull_05", "urban_seagull_05.wav"}, {"urban_seagull_06", "urban_seagull_06.wav"},
};

static const cSomaFsbWanted kVorbisBank_entities_urban[] = {
	{"hum_loop", "hum_loop.ogg"},
	{"dvd_player_idle_sweet_01", "dvd_player_idle_sweet_01.ogg"},
	{"dvd_player_idle_sweet_02", "dvd_player_idle_sweet_02.ogg"},
	{"dvd_player_idle_sweet_03", "dvd_player_idle_sweet_03.ogg"},
	{"dvd_player_idle_sweet_04", "dvd_player_idle_sweet_04.ogg"},
	{"phone_vibrate_wooden_surface_001", "phone_vibrate_wooden_surface_001.ogg"},
	{"phone_vibrate_wooden_surface_002", "phone_vibrate_wooden_surface_002.ogg"},
	{"phone_vibrate_wooden_surface_003", "phone_vibrate_wooden_surface_003.ogg"},
	{"phone_vibrate_wooden_surface_004", "phone_vibrate_wooden_surface_004.ogg"},
	{"phone_vibrate_wooden_surface_005", "phone_vibrate_wooden_surface_005.ogg"},
	{"phone_vibrate_wooden_surface_006", "phone_vibrate_wooden_surface_006.ogg"},
	{"phone_vibrate_wooden_surface_007", "phone_vibrate_wooden_surface_007.ogg"},
	{"phone_vibrate_wooden_surface_008", "phone_vibrate_wooden_surface_008.ogg"},
};

// 00_01_apartment.hps AnswerPhone(): "00_05_apartment2/SFX/phone/pickup_counter"
static const cSomaFsbWanted kVorbisBank_00_05_apartment2_sfx[] = {
	{"pickup_phone_counter_01", "pickup_phone_counter_01.ogg"},
};

static const cSomaFsbWanted kVorbisBank_Entities_Station[] = {
	{"small_ventilation_cluster_001", "small_ventilation_cluster_001.ogg"},
	{"small_ventilation_cluster_002", "small_ventilation_cluster_002.ogg"},
	{"small_ventilation_cluster_003", "small_ventilation_cluster_003.ogg"},
	{"small_ventilation_cluster_004", "small_ventilation_cluster_004.ogg"},
};

//---------------------------------------
// Synthesizes a real SOUNDENTITY .snt sidecar (see this file's top comment
// for the exact real schema this mirrors) referencing already-extracted
// cache audio files, at cacheDir/<asBaseName>.snt - the basename SOMA's own
// real SoundEntityFile leaf name resolves to via cFileSearcher::
// GetFilePath()'s basename-only matching.

static void WriteSnt(const tWString &aCacheDir, const char *apBaseName,
							 const std::vector<tString> &aAudioFiles,
							 bool abLoop, float afInterval, float afRandom,
							 float afMinDistance, float afMaxDistance)
{
	if (aAudioFiles.empty())
		return;

	tString sXml = "<SOUNDENTITY>\n  <SOUNDS>\n  <Main>\n";
	for (size_t i = 0; i < aAudioFiles.size(); ++i)
		sXml += "     <Sound File=\"" + aAudioFiles[i] + "\" />\n";
	sXml += "  </Main>\n  </SOUNDS>\n";
	sXml += "  <PROPERTIES Use3D=\"true\" Loop=\"" + tString(abLoop ? "true" : "false") + "\" Stream=\"false\" "
			"Volume=\"1\" MinDistance=\"" + cString::ToString(afMinDistance, 4, true) + "\" MaxDistance=\"" + cString::ToString(afMaxDistance, 4, true) + "\" "
			"FadeStart=\"true\" FadeStop=\"true\" Random=\"" + cString::ToString(afRandom, 4, true) + "\" Interval=\"" + cString::ToString(afInterval, 4, true) + "\" "
			"Priority=\"0\" />\n";
	sXml += "</SOUNDENTITY>\n";

	tString sBaseName(apBaseName);
	cSomaFsb::WriteTextFile(aCacheDir + cString::To16Char(sBaseName + ".snt"), sXml);
}

//---------------------------------------

static bool gbSomaAmbientSfxReady = false;

void cSomaAmbientSfx::EnsureCached(cResources *apResources)
{
	if (gbSomaAmbientSfxReady)
		return;
	gbSomaAmbientSfxReady = true;

	if (apResources == NULL)
		return;

	tWString sCacheDir = cSomaFsb::GetCacheDir(_W("ambient"));

	bool bAllPresent = cPlatform::FileExists(sCacheDir + _W("game_intro_seq.wav")) &&
						cPlatform::FileExists(sCacheDir + _W("hum_loop.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("idle.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("loop.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("car_drive.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("distant_dog.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("distant_dog_type2.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("seagull.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("vibrating_wood.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("pickup_phone_counter_01.ogg"));

	if (bAllPresent == false)
	{
#define SOMA_FSB_EXTRACT(bank, table) cSomaFsb::ExtractBank(apResources, bank, sCacheDir, table, sizeof(table) / sizeof(table[0]))
		SOMA_FSB_EXTRACT("00_05_apartment2_streamvip.fsb", kPcmBank_00_05_apartment2_streamvip);
		SOMA_FSB_EXTRACT("00_06_lab.fsb", kPcmBank_00_06_lab);
		SOMA_FSB_EXTRACT("entities_urban.fsb", kVorbisBank_entities_urban);
		SOMA_FSB_EXTRACT("00_05_apartment2_sfx.fsb", kVorbisBank_00_05_apartment2_sfx);
		// sounds/level/ has an unrelated Entities_Station.fsb
		SOMA_FSB_EXTRACT("entities/Entities_Station.fsb", kVorbisBank_Entities_Station);
#undef SOMA_FSB_EXTRACT

		std::vector<tString> vCarDrive, vDog, vSeagull, vVent, vDvdIdle;
		for (int i = 1; i <= 10; ++i)
			vCarDrive.push_back(tString("car_drive_") + (i < 10 ? "0" : "") + cString::ToString(i) + ".wav");
		for (int i = 11; i <= 19; ++i)
			vDog.push_back(tString("distant_dog_bark_") + cString::ToString(i) + ".wav");
		for (int i = 1; i <= 6; ++i)
			vSeagull.push_back(tString("urban_seagull_0") + cString::ToString(i) + ".wav");
		for (int i = 1; i <= 4; ++i)
			vVent.push_back(tString("small_ventilation_cluster_00") + cString::ToString(i) + ".ogg");
		for (int i = 1; i <= 4; ++i)
			vDvdIdle.push_back(tString("dvd_player_idle_sweet_0") + cString::ToString(i) + ".ogg");

		// Continuous interior loops (Interval=0 -> plays once looped and stays
		// on, exactly like a real ambient room-tone loop).
		WriteSnt(sCacheDir, "hum_loop", {"hum_loop.ogg"}, true, 0, 1, 0.25f, 1.5f);
		WriteSnt(sCacheDir, "idle", vDvdIdle, true, 0, 1, 0.5f, 5.0f);
		WriteSnt(sCacheDir, "loop", vVent, true, 0, 1, 0.5f, 7.5f);

		// Periodic outdoor "spot" scatter sounds (see this file's top comment,
		// simplification #1, for why Interval/Random are fixed guesses rather
		// than real extracted FMOD event timing).
		WriteSnt(sCacheDir, "car_drive", vCarDrive, true, 18, 1, 10.0f, 25.0f);
		WriteSnt(sCacheDir, "distant_dog", vDog, true, 22, 1, 10.0f, 25.0f);
		WriteSnt(sCacheDir, "distant_dog_type2", vDog, true, 27, 1, 10.0f, 25.0f);
		WriteSnt(sCacheDir, "seagull", vSeagull, true, 14, 1, 3.0f, 15.0f);

		// FMOD: random variant each 1 s spawn tick, one instance at a time; variants are ~2 s
		std::vector<tString> vPhoneVibrate;
		for (int i = 1; i <= 8; ++i)
			vPhoneVibrate.push_back(tString("phone_vibrate_wooden_surface_00") + cString::ToString(i) + ".ogg");
		WriteSnt(sCacheDir, "vibrating_wood", vPhoneVibrate, true, 0.01f, 1, 1.0f, 10.0f);
	}

	apResources->AddResourceDir(sCacheDir, false);
}
