/*
 * Real SOMA map-authored ambient/environmental sound entities - car honking,
 * distant dogs barking, seagulls, plus a few interior loops (a fridge hum, a
 * DVD player idle, a small ventilation cluster) - user-reported as silent
 * ("the ringing phone sound doesn't play or any of the other sounds besides
 * the voice dialog lines... there are meant to be car honking sounds and
 * things like that in the background", 00_01_apartment.hpm's intro).
 *
 * Root cause, confirmed against real data and a real live boot BEFORE
 * writing this fix (see PORTING_NOTES.md for the full citation trail):
 *
 *  - Real SOMA maps DO author ambient sound entities, via a real
 *    `.hpm_Sound` sidecar (HPLMapTrack_Sound) - e.g.
 *    maps/chapter00/00_01_apartment/00_01_apartment.hpm_Sound really
 *    contains 7 <Sound> elements (fridge_hum, dvd_idle, vent_sound_1, dogs,
 *    car_drive, seagulls, dogs_1), each with a SoundEntityFile="..." real
 *    per-instance MinDistance/MaxDistance/Volume.
 *
 *  - This engine ALREADY has a working, generic loader for that track -
 *    `cWorldLoaderHpm::LoadSoundsTrack()` (HPL2/core/sources/resources/
 *    WorldLoaderHpm.cpp) calls the shared `cEngineFileLoading::LoadSound()`
 *    for every one, which calls `cWorld::CreateSoundEntity()` ->
 *    `cSoundEntityManager::CreateSoundEntity()`. This is NOT a missing-
 *    loader gap like ExposureArea's was - the loader exists and runs, unlike
 *    that earlier fix's precedent.
 *
 *  - The real gap: `SoundEntityFile="Entities_Urban/kitchen/fridge/hum_loop"`
 *    etc. are real FMOD Ex/Studio *event* paths, not real HPL `.snt` sound-
 *    entity resource files - no `.snt` file exists anywhere in a real SOMA
 *    install at any of these paths (confirmed: `find .../SOMA -iname
 *    '*.snt'` turns up exactly ONE real `.snt` in the whole install,
 *    unrelated - a player vocalization file). So
 *    `cSoundEntityManager::CreateSoundEntity()`'s own file-searcher lookup
 *    (`SetFileExt(asName,"snt")` then `GetFilePath()`) always misses, and
 *    `cWorld::CreateSoundEntity()` returns NULL for every single one -
 *    confirmed live via a real headless boot of 00_01_apartment.hpm BEFORE
 *    this fix (hpl.log): "ERROR: Couldn't create SoundEntity
 *    'Entities_Urban/kitchen/fridge/hum_loop.snt'" / "ERROR: Cannot find
 *    sound entity 'Entities_Urban/kitchen/fridge/hum_loop'" for all 7, and
 *    the loader's own summary line ending "...0 sounds" for the whole map.
 *    Exactly the same FMOD-event-not-a-real-resource pattern already root-
 *    caused for the main menu's click/hover SFX (see SomaMenuSfx.cpp) and,
 *    per this session's task briefing, for the apartment phone ring too.
 *
 * The fix, mirroring SomaMenuSfx.cpp's established pattern for this exact
 * problem shape (own FSB5 parser -> real extracted PCM/Vorbis samples ->
 * real playable files this engine's existing backend already loads) but
 * carried one step further: rather than exposing a getter this game code
 * calls at some Sound_PlayGui() call site (there's no equivalent AngelScript
 * layer driving map ambiences here), this synthesizes real, valid `.snt`
 * SOUNDENTITY XML sidecars - the exact schema a genuine SOMA `.snt` uses
 * (verified against the one real one in the install,
 * lang/eng/voices/vocalizations/player/player_burned.snt) - referencing the
 * extracted audio, and registers them in a cache resource dir. This means
 * ZERO changes to any shared core loader: `cWorldLoaderHpm::
 * LoadSoundsTrack()`/`cEngineFileLoading::LoadSound()`/
 * `cSoundEntityManager::CreateSoundEntity()` all run completely unmodified
 * and now simply find a real resource where they used to find nothing - the
 * narrowest possible fix for a loader-exists-but-target-resource-doesn't
 * gap. `.snt`'s own real Interval/Random/Loop fields (see
 * cSoundEntityData::CreateFromFile()) are also a real, exact fit for FMOD's
 * "spot" scattering behaviour (car honks/dog barks/gull calls repeating
 * every so often, at random) - not a fabricated mechanism, just this
 * engine's own pre-existing generic one.
 *
 * Where the real audio came from (all three real banks read live from the
 * user's own install; parsed with the SAME FSB5 reader SomaMenuSfx.cpp
 * already implements and validated - deliberately duplicated here rather
 * than shared, to keep this fix's blast radius to new files only, touching
 * zero already-shipped/verified menu-SFX code):
 *
 *  - sounds/level/00_06_lab.fsb - real FSB5, mode=2 (plain PCM16, no Vorbis
 *    reconstruction needed at all), containing car_drive_01..10,
 *    distant_dog_bark_11..19, urban_seagull_01..06 - real named samples
 *    matching "00_06_lab/amb/spot/{car_drive,distant_dog,seagull}"'s leaf
 *    event names (this per-level bank is reused by other levels/areas that
 *    reference the same generic urban ambience, which is exactly why the
 *    apartment's own .hpm_Sound points at a "00_06_lab" bank rather than
 *    one named for itself - confirmed by this session's own `strings`
 *    dump, not a guess).
 *
 *  - sounds/entities/entities_urban.fsb - real FSB5, mode=15 (Vorbis),
 *    containing hum_loop and dvd_player_idle_sweet_01..04, matching
 *    "Entities_Urban/kitchen/fridge/hum_loop" and ".../dvd_player/idle".
 *    Its samples' Vorbis setup-header crc32 (0xb62ad8df) is a DIFFERENT
 *    real FMOD codebook preset than the one SomaMenuSfxVorbisSetup.h
 *    already embeds for special_fx.fsb (0x6d39bf3e) - confirmed via this
 *    session's own FSB5 header parse. That second preset's raw bytes were
 *    pulled from the same public, MIT-licensed python-fsb5 project table
 *    (fsb5/vorbis_headers.py) the first one came from - see
 *    SomaAmbientSfxVorbisSetup.h's own header comment.
 *
 *  - sounds/entities/Entities_Station.fsb - real FSB5, mode=15 (Vorbis),
 *    containing small_ventilation_cluster_001..004, matching
 *    "Entities_Station/object/small_ventilation_cluser/loop" (real,
 *    misspelled-in-the-original-data folder name - kept verbatim since
 *    that's the actual SoundEntityFile string real maps reference, though
 *    this file only needs the LEAF "loop" for its synthesized .snt's own
 *    basename - see cFileSearcher::GetFilePath()'s basename-only matching).
 *    Its samples' crc32 (0x6d39bf3e) IS the preset SomaMenuSfxVorbisSetup.h
 *    already embeds - reused directly, no new preset needed.
 *
 * Two deliberate, documented simplifications (same spirit as
 * LoadExposureAreaTrack()'s "real, honest first step, not the full system"):
 *
 *  1. Real per-instance Interval/Random timing for FMOD's "spot" scatter
 *     sounds (car_drive/distant_dog/seagull) lives inside the .fev event
 *     project this codebase has no parser for (same documented gap as
 *     SomaMenuSfx.cpp's FG_Menu_Sting) - the values below are reasonable
 *     fixed guesses (15-27s, staggered per sound so they don't sync), not
 *     extracted real timing data. Real per-instance MinDistance/MaxDistance/
 *     Volume, by contrast, ARE the real authored .hpm_Sound values - those
 *     already flow through unmodified (cEngineFileLoading::LoadSound()
 *     calls SetMinDistance/SetMaxDistance/SetVolume() straight from the
 *     real XML, overriding whatever default this file's own .snt carries).
 *
 *  2. "dogs"/"distant_dog" and "dogs_1"/"distant_dog_type2" are two
 *     separate real .hpm_Sound entities in 00_01_apartment alone, but only
 *     ONE real sample family (distant_dog_bark_11..19) was found in
 *     00_06_lab.fsb - no separate "type2" pool exists in this bank. Both
 *     synthesized .snt's <Main> list draws from the same real sample pool
 *     (still real dog-bark audio, just not a verified-distinct second
 *     variant set) rather than fabricating new content.
 */

#include "SomaAmbientSfx.h"
#include "SomaMenuSfxVorbisSetup.h"
#include "SomaAmbientSfxVorbisSetup.h"

#include <cstring>
#include <fstream>
#include <vector>

//---------------------------------------
// Tiny byte-buffer helpers - same as SomaMenuSfx.cpp (FSB5 fields are packed
// with no alignment guarantee).

static unsigned int AmbSfx_ReadU32LE(const unsigned char *apData)
{
	return (unsigned int)apData[0] | ((unsigned int)apData[1] << 8) | ((unsigned int)apData[2] << 16) | ((unsigned int)apData[3] << 24);
}

static unsigned long long AmbSfx_ReadU64LE(const unsigned char *apData)
{
	unsigned long long lo = AmbSfx_ReadU32LE(apData);
	unsigned long long hi = AmbSfx_ReadU32LE(apData + 4);
	return lo | (hi << 32);
}

static void AmbSfx_AppendU32LE(std::vector<unsigned char> &aOut, unsigned int aVal)
{
	aOut.push_back((unsigned char)(aVal & 0xFF));
	aOut.push_back((unsigned char)((aVal >> 8) & 0xFF));
	aOut.push_back((unsigned char)((aVal >> 16) & 0xFF));
	aOut.push_back((unsigned char)((aVal >> 24) & 0xFF));
}

static void AmbSfx_AppendU64LE(std::vector<unsigned char> &aOut, unsigned long long aVal)
{
	AmbSfx_AppendU32LE(aOut, (unsigned int)(aVal & 0xFFFFFFFFull));
	AmbSfx_AppendU32LE(aOut, (unsigned int)((aVal >> 32) & 0xFFFFFFFFull));
}

//---------------------------------------
// Real FSB5 sample header - see SomaMenuSfx.cpp's top comment for the full
// field-layout citation (python-fsb5's fsb5/__init__.py). Duplicated here
// deliberately (see this file's top comment for why).

struct cAmbFsbSample
{
	tString msName;
	unsigned int mlFrequency;
	int mlChannels;
	size_t mlDataOffset;
	size_t mlDataSize;
	unsigned int mlNumPcmSamples;
	bool mbHasVorbisCrc;
	unsigned int mlVorbisCrc;
};

static unsigned int AmbSfx_FsbFrequencyEnum(unsigned int alIndex)
{
	static const unsigned int kTable[10] = {0, 8000, 11000, 11025, 16000, 22050, 24000, 32000, 44100, 48000};
	if (alIndex < 10)
		return kTable[alIndex];
	return 0;
}

static const unsigned int kAmbSfxFsbChunkType_VorbisData = 11;

static bool AmbSfx_ParseFsb5(const std::vector<unsigned char> &aFile, unsigned int &alModeOut, std::vector<cAmbFsbSample> &aSamplesOut)
{
	aSamplesOut.clear();

	if (aFile.size() < 60 || std::memcmp(aFile.data(), "FSB5", 4) != 0)
		return false;

	const unsigned char *pData = aFile.data();

	unsigned int lVersion = AmbSfx_ReadU32LE(pData + 4);
	unsigned int lNumSamples = AmbSfx_ReadU32LE(pData + 8);
	unsigned int lSampleHeadersSize = AmbSfx_ReadU32LE(pData + 12);
	unsigned int lNameTableSize = AmbSfx_ReadU32LE(pData + 16);
	unsigned int lDataSize = AmbSfx_ReadU32LE(pData + 20);
	unsigned int lMode = AmbSfx_ReadU32LE(pData + 24);
	alModeOut = lMode;

	if (lVersion == 0)
	{
		Log("SOMA ambient sfx: FSB5 version 0 header not supported (real SOMA banks are all version 1)\n");
		return false;
	}

	size_t lPos = 60;
	if (lPos + lSampleHeadersSize + lNameTableSize > aFile.size())
		return false;

	for (unsigned int i = 0; i < lNumSamples; ++i)
	{
		if (lPos + 8 > aFile.size())
			return false;

		unsigned long long raw = AmbSfx_ReadU64LE(pData + lPos);
		lPos += 8;

		unsigned int lNextChunk = (unsigned int)(raw & 0x1);
		unsigned int lFreqIndex = (unsigned int)((raw >> 1) & 0xF);
		int lChannels = (int)((raw >> 5) & 0x1) + 1;
		unsigned int lDataOffset = (unsigned int)((raw >> 6) & 0xFFFFFFFull) * 16;
		unsigned int lNumSamplesInSound = (unsigned int)((raw >> 34) & 0x3FFFFFFFull);

		cAmbFsbSample sample;
		sample.mlFrequency = AmbSfx_FsbFrequencyEnum(lFreqIndex);
		sample.mlChannels = lChannels;
		sample.mlDataOffset = lDataOffset;
		sample.mlNumPcmSamples = lNumSamplesInSound;
		sample.mbHasVorbisCrc = false;
		sample.mlVorbisCrc = 0;

		while (lNextChunk)
		{
			if (lPos + 4 > aFile.size())
				return false;

			unsigned int craw = AmbSfx_ReadU32LE(pData + lPos);
			lPos += 4;

			lNextChunk = craw & 0x1;
			unsigned int lChunkSize = (craw >> 1) & 0xFFFFFF;
			unsigned int lChunkType = (craw >> 25) & 0x7F;

			if (lPos + lChunkSize > aFile.size())
				return false;

			if (lChunkType == kAmbSfxFsbChunkType_VorbisData && lChunkSize >= 4)
			{
				sample.mbHasVorbisCrc = true;
				sample.mlVorbisCrc = AmbSfx_ReadU32LE(pData + lPos);
			}
			else if (lChunkType == 2 && lChunkSize >= 4)
			{
				sample.mlFrequency = AmbSfx_ReadU32LE(pData + lPos);
			}

			lPos += lChunkSize;
		}

		aSamplesOut.push_back(sample);
	}

	size_t lNameTableStart = lPos;
	if (lNameTableSize > 0)
	{
		if (lNameTableStart + lNumSamples * 4 > aFile.size())
			return false;

		for (unsigned int i = 0; i < lNumSamples; ++i)
		{
			unsigned int lOff = AmbSfx_ReadU32LE(pData + lNameTableStart + i * 4);
			size_t lStrPos = lNameTableStart + lOff;
			if (lStrPos >= aFile.size())
				continue;

			size_t lEnd = lStrPos;
			while (lEnd < aFile.size() && lEnd < lNameTableStart + lNameTableSize && pData[lEnd] != 0)
				++lEnd;

			aSamplesOut[i].msName = tString((const char *)pData + lStrPos, lEnd - lStrPos);
		}
	}
	lPos = lNameTableStart + lNameTableSize;

	for (unsigned int i = 0; i < lNumSamples; ++i)
	{
		size_t lEnd = (i + 1 < lNumSamples) ? aSamplesOut[i + 1].mlDataOffset : lDataSize;
		aSamplesOut[i].mlDataSize = (lEnd > aSamplesOut[i].mlDataOffset) ? (lEnd - aSamplesOut[i].mlDataOffset) : 0;
	}

	for (unsigned int i = 0; i < lNumSamples; ++i)
		aSamplesOut[i].mlDataOffset += lPos;

	return true;
}

//---------------------------------------
// Real Ogg CRC32 (libogg's framing.c algorithm) - same as SomaMenuSfx.cpp.

static unsigned int AmbSfx_OggCrc32(const unsigned char *apData, size_t alSize)
{
	static unsigned int table[256];
	static bool bInit = false;
	if (bInit == false)
	{
		for (unsigned int i = 0; i < 256; ++i)
		{
			unsigned int r = i << 24;
			for (int j = 0; j < 8; ++j)
				r = (r & 0x80000000u) ? ((r << 1) ^ 0x04c11db7u) : (r << 1);
			table[i] = r;
		}
		bInit = true;
	}

	unsigned int crc = 0;
	for (size_t i = 0; i < alSize; ++i)
		crc = (crc << 8) ^ table[((crc >> 24) & 0xFF) ^ apData[i]];
	return crc;
}

//---------------------------------------
// Minimal real Ogg page muxer - identical algorithm/shape to SomaMenuSfx.cpp's
// cOggMuxer (see that file's own comments for the two real libvorbisfile
// page-boundary requirements this depends on).

class cAmbSfxOggMuxer
{
public:
	explicit cAmbSfxOggMuxer(unsigned int alSerial) : mlSerial(alSerial), mlPageSeq(0), mbWroteAnyPage(false), mbContinuedIntoCurrentPage(false)
	{
	}

	void AddPacket(const unsigned char *apData, size_t alSize)
	{
		size_t lRemaining = alSize;
		size_t lOff = 0;
		do
		{
			unsigned char lLacing = (lRemaining >= 255) ? (unsigned char)255 : (unsigned char)lRemaining;

			if (mSegTable.size() == 255)
			{
				FlushPage(false, -1);
				mbContinuedIntoCurrentPage = true;
			}

			mSegTable.push_back(lLacing);
			mBody.insert(mBody.end(), apData + lOff, apData + lOff + lLacing);
			lOff += lLacing;
			lRemaining -= lLacing;
		} while (lRemaining > 0);
	}

	void Finish(long long alFinalGranulePos)
	{
		if (mSegTable.empty() == false)
			FlushPage(true, alFinalGranulePos);
	}

	void ForcePageBoundary()
	{
		if (mSegTable.empty() == false)
			FlushPage(false, -1);
	}

	const std::vector<unsigned char> &Bytes() const { return mOut; }

private:
	void FlushPage(bool abEos, long long alGranulePos)
	{
		if (mSegTable.empty())
			return;

		bool bBos = (mbWroteAnyPage == false);

		std::vector<unsigned char> page;
		page.push_back('O'); page.push_back('g'); page.push_back('g'); page.push_back('S');
		page.push_back(0);

		unsigned char lHeaderType = 0;
		if (mbContinuedIntoCurrentPage) lHeaderType |= 1;
		if (bBos) lHeaderType |= 2;
		if (abEos) lHeaderType |= 4;
		page.push_back(lHeaderType);

		AmbSfx_AppendU64LE(page, (unsigned long long)alGranulePos);
		AmbSfx_AppendU32LE(page, mlSerial);
		AmbSfx_AppendU32LE(page, mlPageSeq++);

		size_t lCrcPos = page.size();
		AmbSfx_AppendU32LE(page, 0);

		page.push_back((unsigned char)mSegTable.size());
		page.insert(page.end(), mSegTable.begin(), mSegTable.end());
		page.insert(page.end(), mBody.begin(), mBody.end());

		unsigned int lCrc = AmbSfx_OggCrc32(page.data(), page.size());
		page[lCrcPos + 0] = (unsigned char)(lCrc & 0xFF);
		page[lCrcPos + 1] = (unsigned char)((lCrc >> 8) & 0xFF);
		page[lCrcPos + 2] = (unsigned char)((lCrc >> 16) & 0xFF);
		page[lCrcPos + 3] = (unsigned char)((lCrc >> 24) & 0xFF);

		mOut.insert(mOut.end(), page.begin(), page.end());

		mbWroteAnyPage = true;
		mSegTable.clear();
		mBody.clear();
		mbContinuedIntoCurrentPage = false;
	}

	unsigned int mlSerial;
	unsigned int mlPageSeq;
	bool mbWroteAnyPage;
	bool mbContinuedIntoCurrentPage;
	std::vector<unsigned char> mSegTable;
	std::vector<unsigned char> mBody;
	std::vector<unsigned char> mOut;
};

//---------------------------------------

class cAmbSfxBitPackerLSB
{
public:
	void Write(unsigned long aValue, int alBits)
	{
		while (alBits > 0)
		{
			if (mlBitPos == 0)
				mBuf.push_back(0);

			int lFree = 8 - mlBitPos;
			int lTake = (alBits < lFree) ? alBits : lFree;
			unsigned char lChunk = (unsigned char)(aValue & ((1u << lTake) - 1));
			mBuf.back() = (unsigned char)(mBuf.back() | (lChunk << mlBitPos));

			mlBitPos = (mlBitPos + lTake) % 8;
			aValue >>= lTake;
			alBits -= lTake;
		}
	}

	const std::vector<unsigned char> &Bytes() const { return mBuf; }

private:
	std::vector<unsigned char> mBuf;
	int mlBitPos = 0;
};

static std::vector<unsigned char> AmbSfx_BuildVorbisIdHeader(int alChannels, unsigned int alFrequency)
{
	cAmbSfxBitPackerLSB bp;
	bp.Write(0x01, 8);
	const char *pTag = "vorbis";
	for (int i = 0; i < 6; ++i)
		bp.Write((unsigned long)(unsigned char)pTag[i], 8);
	bp.Write(0, 32);
	bp.Write((unsigned long)alChannels, 8);
	bp.Write(alFrequency, 32);
	bp.Write(0, 32);
	bp.Write(0, 32);
	bp.Write(0, 32);
	bp.Write(8, 4);
	bp.Write(11, 4);
	bp.Write(1, 1);
	return bp.Bytes();
}

static std::vector<unsigned char> AmbSfx_BuildVorbisCommentHeader()
{
	std::vector<unsigned char> out;
	out.push_back(0x03);
	const char *pTag = "vorbis";
	for (int i = 0; i < 6; ++i)
		out.push_back((unsigned char)pTag[i]);
	AmbSfx_AppendU32LE(out, 0);
	AmbSfx_AppendU32LE(out, 0);
	out.push_back(0x01);
	return out;
}

// Reconstructs one Vorbis-mode FSB5 sample into a real, playable Ogg Vorbis
// file, given whichever real FMOD setup-header preset (bytes+size) matches
// the sample's own crc32 - see this file's top comment for why there are
// two possible presets here (special_fx.fsb's vs entities_urban.fsb's).
static bool AmbSfx_OggMuxVorbisSample(const cAmbFsbSample &aSample, const unsigned char *apFileData,
									   const unsigned char *apPresetData, int alPresetSize,
									   std::vector<unsigned char> &aOutOgg)
{
	std::vector<unsigned char> idHeader = AmbSfx_BuildVorbisIdHeader(aSample.mlChannels, aSample.mlFrequency);
	std::vector<unsigned char> commentHeader = AmbSfx_BuildVorbisCommentHeader();

	cAmbSfxOggMuxer muxer(1);
	muxer.AddPacket(idHeader.data(), idHeader.size());
	muxer.ForcePageBoundary();
	muxer.AddPacket(commentHeader.data(), commentHeader.size());
	muxer.AddPacket(apPresetData, (size_t)alPresetSize);
	muxer.ForcePageBoundary();

	const unsigned char *pSampleData = apFileData + aSample.mlDataOffset;
	size_t lSampleSize = aSample.mlDataSize;
	size_t lOff = 0;
	while (lOff + 2 <= lSampleSize)
	{
		unsigned int lPacketLen = (unsigned int)pSampleData[lOff] | ((unsigned int)pSampleData[lOff + 1] << 8);
		lOff += 2;
		if (lPacketLen == 0)
			break;
		if (lOff + lPacketLen > lSampleSize)
			break;

		muxer.AddPacket(pSampleData + lOff, lPacketLen);
		lOff += lPacketLen;
	}

	muxer.Finish((long long)aSample.mlNumPcmSamples);

	aOutOgg = muxer.Bytes();
	return true;
}

static bool AmbSfx_WritePcm16Wav(const cAmbFsbSample &aSample, const unsigned char *apFileData, std::vector<unsigned char> &aOutWav)
{
	unsigned int lByteRate = aSample.mlFrequency * aSample.mlChannels * 2;
	unsigned int lBlockAlign = aSample.mlChannels * 2;
	unsigned int lDataBytes = (unsigned int)(aSample.mlNumPcmSamples * lBlockAlign);
	if (lDataBytes > aSample.mlDataSize)
		lDataBytes = (unsigned int)aSample.mlDataSize;

	aOutWav.clear();
	aOutWav.reserve(44 + lDataBytes);

	aOutWav.insert(aOutWav.end(), {'R', 'I', 'F', 'F'});
	AmbSfx_AppendU32LE(aOutWav, 36 + lDataBytes);
	aOutWav.insert(aOutWav.end(), {'W', 'A', 'V', 'E'});
	aOutWav.insert(aOutWav.end(), {'f', 'm', 't', ' '});
	AmbSfx_AppendU32LE(aOutWav, 16);
	aOutWav.push_back(1); aOutWav.push_back(0);
	aOutWav.push_back((unsigned char)aSample.mlChannels); aOutWav.push_back(0);
	AmbSfx_AppendU32LE(aOutWav, aSample.mlFrequency);
	AmbSfx_AppendU32LE(aOutWav, lByteRate);
	aOutWav.push_back((unsigned char)lBlockAlign); aOutWav.push_back(0);
	aOutWav.push_back(16); aOutWav.push_back(0);
	aOutWav.insert(aOutWav.end(), {'d', 'a', 't', 'a'});
	AmbSfx_AppendU32LE(aOutWav, lDataBytes);

	const unsigned char *pData = apFileData + aSample.mlDataOffset;
	aOutWav.insert(aOutWav.end(), pData, pData + lDataBytes);
	return true;
}

//---------------------------------------

static bool AmbSfx_ReadWholeFile(const tWString &asPath, std::vector<unsigned char> &aOut)
{
	std::ifstream f(cString::To8Char(asPath).c_str(), std::ios::binary | std::ios::ate);
	if (f.is_open() == false)
		return false;

	std::streamsize lSize = f.tellg();
	if (lSize <= 0)
		return false;
	f.seekg(0, std::ios::beg);

	aOut.resize((size_t)lSize);
	f.read(reinterpret_cast<char *>(aOut.data()), lSize);
	return f.good() || f.eof();
}

static bool AmbSfx_WriteWholeFile(const tWString &asPath, const std::vector<unsigned char> &aData)
{
	std::ofstream f(cString::To8Char(asPath).c_str(), std::ios::binary | std::ios::trunc);
	if (f.is_open() == false)
		return false;
	f.write(reinterpret_cast<const char *>(aData.data()), (std::streamsize)aData.size());
	return f.good();
}

static bool AmbSfx_WriteTextFile(const tWString &asPath, const tString &aText)
{
	std::ofstream f(cString::To8Char(asPath).c_str(), std::ios::binary | std::ios::trunc);
	if (f.is_open() == false)
		return false;
	f.write(aText.c_str(), (std::streamsize)aText.size());
	return f.good();
}

static tWString AmbSfx_GetCacheDir()
{
	tWString sRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGCacheHome);
	tWString sDir = sRoot + _W("open-hpl/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);
	sDir += _W("soma/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);
	sDir += _W("ambient/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);
	return sDir;
}

//---------------------------------------
// Extraction recipes - one row per real sample this fix needs, grouped by
// which real bank (and which encoding) it comes from.

struct cAmbSfxWantedSample
{
	const char *pRealName;
	const char *pCacheFile; // basename only, written into the cache dir
};

// 00_00_intro.hps' OnStart plays Sound_PlayGui("00_05_apartment2/SFX/game_intro_seq"),
// the intro slideshow's whole ambience/score bed. One mode=2 (PCM16) sample, ~27 MB.
static const cAmbSfxWantedSample kPcmBank_00_05_apartment2_streamvip[] = {
	{"game_intro_seq", "game_intro_seq.wav"},
};

static const cAmbSfxWantedSample kPcmBank_00_06_lab[] = {
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

static const cAmbSfxWantedSample kVorbisBank_entities_urban[] = {
	{"hum_loop", "hum_loop.ogg"},
	{"dvd_player_idle_sweet_01", "dvd_player_idle_sweet_01.ogg"},
	{"dvd_player_idle_sweet_02", "dvd_player_idle_sweet_02.ogg"},
	{"dvd_player_idle_sweet_03", "dvd_player_idle_sweet_03.ogg"},
	{"dvd_player_idle_sweet_04", "dvd_player_idle_sweet_04.ogg"},
};

static const cAmbSfxWantedSample kVorbisBank_Entities_Station[] = {
	{"small_ventilation_cluster_001", "small_ventilation_cluster_001.ogg"},
	{"small_ventilation_cluster_002", "small_ventilation_cluster_002.ogg"},
	{"small_ventilation_cluster_003", "small_ventilation_cluster_003.ogg"},
	{"small_ventilation_cluster_004", "small_ventilation_cluster_004.ogg"},
};

static void AmbSfx_ExtractPcmBank(cResources *apResources, const char *apBankPath, const tWString &aCacheDir,
								   const cAmbSfxWantedSample *apWanted, size_t alCount)
{
	const tWString &sPath = apResources->GetFileSearcher()->GetFilePath(apBankPath);
	if (sPath == _W(""))
	{
		Log("SOMA ambient sfx: could not find '%s' in the real SOMA install - some ambient sounds will be silent\n", apBankPath);
		return;
	}

	std::vector<unsigned char> vFile;
	if (AmbSfx_ReadWholeFile(sPath, vFile) == false)
		return;

	unsigned int lMode = 0;
	std::vector<cAmbFsbSample> vSamples;
	if (AmbSfx_ParseFsb5(vFile, lMode, vSamples) == false)
	{
		Log("SOMA ambient sfx: failed to parse '%s' as an FSB5 bank\n", apBankPath);
		return;
	}

	for (size_t i = 0; i < vSamples.size(); ++i)
	{
		for (size_t w = 0; w < alCount; ++w)
		{
			if (vSamples[i].msName != apWanted[w].pRealName)
				continue;

			std::vector<unsigned char> vWav;
			if (AmbSfx_WritePcm16Wav(vSamples[i], vFile.data(), vWav))
				AmbSfx_WriteWholeFile(aCacheDir + cString::To16Char(apWanted[w].pCacheFile), vWav);
			break;
		}
	}
}

static void AmbSfx_ExtractVorbisBank(cResources *apResources, const char *apBankPath, const tWString &aCacheDir,
									  const cAmbSfxWantedSample *apWanted, size_t alCount,
									  unsigned int alExpectedCrc, const unsigned char *apPresetData, int alPresetSize)
{
	const tWString &sPath = apResources->GetFileSearcher()->GetFilePath(apBankPath);
	if (sPath == _W(""))
	{
		Log("SOMA ambient sfx: could not find '%s' in the real SOMA install - some ambient sounds will be silent\n", apBankPath);
		return;
	}

	std::vector<unsigned char> vFile;
	if (AmbSfx_ReadWholeFile(sPath, vFile) == false)
		return;

	unsigned int lMode = 0;
	std::vector<cAmbFsbSample> vSamples;
	if (AmbSfx_ParseFsb5(vFile, lMode, vSamples) == false)
	{
		Log("SOMA ambient sfx: failed to parse '%s' as an FSB5 bank\n", apBankPath);
		return;
	}

	for (size_t i = 0; i < vSamples.size(); ++i)
	{
		for (size_t w = 0; w < alCount; ++w)
		{
			if (vSamples[i].msName != apWanted[w].pRealName)
				continue;

			if (vSamples[i].mbHasVorbisCrc == false || vSamples[i].mlVorbisCrc != alExpectedCrc)
			{
				Log("SOMA ambient sfx: sample '%s' in '%s' uses an unrecognised Vorbis setup crc32 (%u) - skipped\n",
					vSamples[i].msName.c_str(), apBankPath, vSamples[i].mlVorbisCrc);
				break;
			}

			std::vector<unsigned char> vOgg;
			if (AmbSfx_OggMuxVorbisSample(vSamples[i], vFile.data(), apPresetData, alPresetSize, vOgg))
				AmbSfx_WriteWholeFile(aCacheDir + cString::To16Char(apWanted[w].pCacheFile), vOgg);
			break;
		}
	}
}

//---------------------------------------
// Synthesizes a real SOUNDENTITY .snt sidecar (see this file's top comment
// for the exact real schema this mirrors) referencing already-extracted
// cache audio files, at cacheDir/<asBaseName>.snt - the basename SOMA's own
// real SoundEntityFile leaf name resolves to via cFileSearcher::
// GetFilePath()'s basename-only matching.

static void AmbSfx_WriteSnt(const tWString &aCacheDir, const char *apBaseName,
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
	AmbSfx_WriteTextFile(aCacheDir + cString::To16Char(sBaseName + ".snt"), sXml);
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

	tWString sCacheDir = AmbSfx_GetCacheDir();

	bool bAllPresent = cPlatform::FileExists(sCacheDir + _W("game_intro_seq.wav")) &&
						cPlatform::FileExists(sCacheDir + _W("hum_loop.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("idle.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("loop.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("car_drive.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("distant_dog.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("distant_dog_type2.snt")) &&
						cPlatform::FileExists(sCacheDir + _W("seagull.snt"));

	if (bAllPresent == false)
	{
		AmbSfx_ExtractPcmBank(apResources, "00_05_apartment2_streamvip.fsb", sCacheDir,
							   kPcmBank_00_05_apartment2_streamvip,
							   sizeof(kPcmBank_00_05_apartment2_streamvip) / sizeof(kPcmBank_00_05_apartment2_streamvip[0]));
		AmbSfx_ExtractPcmBank(apResources, "00_06_lab.fsb", sCacheDir, kPcmBank_00_06_lab,
							   sizeof(kPcmBank_00_06_lab) / sizeof(kPcmBank_00_06_lab[0]));

		AmbSfx_ExtractVorbisBank(apResources, "entities_urban.fsb", sCacheDir, kVorbisBank_entities_urban,
								  sizeof(kVorbisBank_entities_urban) / sizeof(kVorbisBank_entities_urban[0]),
								  kSomaAmbientVorbisSetupCrc32, kSomaAmbientVorbisSetupHeaderData, kSomaAmbientVorbisSetupHeaderSize);

		// Directory-qualified: a second, unrelated "Entities_Station.fsb" also
		// exists under sounds/level/ - see this file's top comment.
		AmbSfx_ExtractVorbisBank(apResources, "entities/Entities_Station.fsb", sCacheDir, kVorbisBank_Entities_Station,
								  sizeof(kVorbisBank_Entities_Station) / sizeof(kVorbisBank_Entities_Station[0]),
								  kSomaVorbisSetupCrc32, kSomaVorbisSetupHeaderData, kSomaVorbisSetupHeaderSize);

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
		AmbSfx_WriteSnt(sCacheDir, "hum_loop", {"hum_loop.ogg"}, true, 0, 1, 0.25f, 1.5f);
		AmbSfx_WriteSnt(sCacheDir, "idle", vDvdIdle, true, 0, 1, 0.5f, 5.0f);
		AmbSfx_WriteSnt(sCacheDir, "loop", vVent, true, 0, 1, 0.5f, 7.5f);

		// Periodic outdoor "spot" scatter sounds (see this file's top comment,
		// simplification #1, for why Interval/Random are fixed guesses rather
		// than real extracted FMOD event timing).
		AmbSfx_WriteSnt(sCacheDir, "car_drive", vCarDrive, true, 18, 1, 10.0f, 25.0f);
		AmbSfx_WriteSnt(sCacheDir, "distant_dog", vDog, true, 22, 1, 10.0f, 25.0f);
		AmbSfx_WriteSnt(sCacheDir, "distant_dog_type2", vDog, true, 27, 1, 10.0f, 25.0f);
		AmbSfx_WriteSnt(sCacheDir, "seagull", vSeagull, true, 14, 1, 3.0f, 15.0f);
	}

	// Real resources.cfg has no entry for a per-user cache dir - register it
	// directly, same as cSomaMenuSfx::EnsureCached() already does for its
	// own cache dir, so cSoundEntityManager::CreateSoundEntity()'s file-
	// searcher lookup resolves these synthesized .snt files (and their
	// referenced audio) by basename exactly like any other real resource.
	apResources->AddResourceDir(sCacheDir, false);
}
