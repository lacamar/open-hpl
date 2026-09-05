/*
 * SOMA's real menu click/hover/glitch/sting sound effects.
 *
 * script/modules/MenuHandler.hps and helper_imgui_options.hps call
 * Sound_PlayGui("special_fx/frontend/frontend_menu_change", ...) (and
 * .../frontend_menu_focus, .../frontend_menu_select, .../frontend_menu_slider,
 * .../menu_glitch, .../new_game_sting) throughout the real menu code - real,
 * confirmed events, not a guess. But unlike Menu_Music.ogg (a plain OGG file
 * this engine's existing OpenAL music backend already plays with zero extra
 * work), every one of these is FMOD Studio/Designer-banked: the real names
 * only exist as *events* inside sounds/special/special_fx.fev, backed by
 * actual sample data inside sounds/special/special_fx.fsb (Vorbis-mode) and
 * sounds/special/special_fx_stream.fsb (new_game_sting, plain PCM16) - a
 * proprietary FMOD container format this engine has no reader for at all.
 *
 * This file IS that reader - a real, bounded FSB5 parser plus just enough of
 * an Ogg-Vorbis container writer to turn the extracted samples back into
 * plain files this engine's existing sound backend can already load, so the
 * real sounds actually play rather than being faked or left silent. What was
 * found and how, precisely:
 *
 *  - sounds/special/special_fx.fsb is a real FSB5 bank (magic "FSB5",
 *    confirmed via a hex dump of a real install), mode=15=VORBIS, containing
 *    17 named samples including all 5 real events above except
 *    new_game_sting (frontend_menu_change_01/focus_01/select_01/slider_01,
 *    menu_glitch_001..012, plus one unrelated "abyss_look_at" sample this
 *    session didn't need). Its 60-byte header + variable-length bit-packed
 *    per-sample header format (next-chunk flag / frequency enum / channels /
 *    dataOffset*16 / numPcmSamples, then optional metadata chunks) is a
 *    real, previously reverse-engineered public format - ParseFsb5() below
 *    follows the exact same field layout as the MIT-licensed python-fsb5
 *    project's fsb5/__init__.py (https://github.com/HearthSim/python-fsb5),
 *    cross-checked field-by-field against this real file's actual bytes
 *    (not merely reused fromtrust) before being ported to C++ here.
 *
 *  - FSB5's Vorbis mode never stores the standard Ogg Vorbis "identification/
 *    comment/setup" header packets a decoder needs - only a crc32 per
 *    sample (an unassuming per-sample VORBISDATA metadata chunk) identifying
 *    which of a small, fixed, generic set of setup-header presets FMOD's own
 *    authoring tool used. This crc32 (0x6d39bf3e) turned out to be IDENTICAL
 *    across every single one of the 17 samples in this real bank, which is
 *    exactly why this was a bounded/tractable task rather than an open-ended
 *    one - see SomaMenuSfxVorbisSetup.h for exactly where that one preset's
 *    raw bytes came from (the public, MIT-licensed python-fsb5 table, not
 *    extracted from SOMA - nothing in this file or that header contains any
 *    of SOMA's own copyrighted audio). OggMuxVorbisSample() below
 *    reconstructs the identification/comment headers itself (byte-for-byte
 *    per the public Vorbis I spec) and pairs them with that one embedded
 *    setup packet plus the sample's own real (proprietary, read live from
 *    the user's own install, never persisted anywhere but a per-user cache)
 *    audio packets, then muxes all of it into a real, valid Ogg container
 *    (OggMuxer below - real page framing/lacing/CRC32, the same algorithm
 *    libogg itself uses) - verified against a real install this session:
 *    ffprobe/ffmpeg both parse and fully decode the reconstructed
 *    frontend_menu_focus_01.ogg as valid 48kHz stereo Vorbis.
 *
 *  - sounds/special/special_fx_stream.fsb is mode=2=PCM16 (new_game_sting,
 *    44.1kHz mono) - no Vorbis reconstruction needed at all, just a plain
 *    44-byte WAV header in front of the real PCM bytes (WritePcm16Wav()).
 *
 * Everything here only ever reads the user's own real, legitimately-owned
 * SOMA install (via apResources' existing file searcher - sounds/ is
 * already a registered resource dir) and writes its OUTPUT (plain .ogg/.wav
 * files, playable by any standard decoder, containing the same audio any
 * real SOMA install already ships) to a per-user cache directory
 * ($XDG_CACHE_HOME/open-hpl/soma/sfx/) - nothing proprietary is embedded in
 * this repo; only the one generic, reusable FMOD preset table entry is (see
 * SomaMenuSfxVorbisSetup.h's own comment for why that's fine to redistribute).
 *
 * Known limitation, left as a documented gap rather than faked: "FG_Menu_Sting"
 * (script/modules/MenuHandler.hps line ~6807) is a real Sound_PlayGui() call
 * too, but it only fires from the real premenu cinematic sequence
 * (mlPreMenuState state machine) this scaffold doesn't have at all (no
 * premenu exists here - SomaMainMenu goes straight to the main menu) - not
 * reachable from anything this port's menu actually does, so it was not
 * chased further. Its target sample doesn't obviously exist in either bank
 * this file parses either (a plain per-event wav name search for
 * "FG_Menu_Sting" turned up nothing in special_fx.fev's own referenced wave
 * list - it may resolve to "FG_Logo_Sting" via FMOD event-internal logic
 * this file doesn't parse the .fev project format for).
 */

#include "SomaMenuSfx.h"
#include "SomaMenuSfxVorbisSetup.h"

#include <cstring>
#include <fstream>
#include <vector>

//---------------------------------------

static bool gbSomaMenuSfxReady = false;
static bool gbSomaMenuSfxChangeOk = false;
static bool gbSomaMenuSfxFocusOk = false;
static bool gbSomaMenuSfxSelectOk = false;
static bool gbSomaMenuSfxSliderOk = false;
static bool gbSomaMenuSfxNewGameStingOk = false;
static bool gabSomaMenuSfxGlitchOk[12] = {false};
// Added for cSomaSplash's own SFX needs (see SomaSplash.h/.cpp) - the real
// sample names behind script/modules/MenuHandler.hps's
// Sound_PlayGui("special_fx/frontend/FG_Menu_Sting", ...) and
// Sound_CreateAtEntity("MenuBGNoise", "special_fx/frontend/main_menu_bg", ...)
// calls, resolving this file's own documented "FG_Menu_Sting ... may resolve
// to FG_Logo_Sting via FMOD event-internal logic this file doesn't parse the
// .fev format for" gap above: confirmed via `strings` on the real
// sounds/special/special_fx_stream.fsb (same bank/PCM16 path already used
// for new_game_sting below) - it contains real named samples "FG_Logo_Sting"
// and "menu_bg_noise", not "FG_Menu_Sting"/"main_menu_bg" literally.
static bool gbSomaMenuSfxFGLogoStingOk = false;
static bool gbSomaMenuSfxMenuBgNoiseOk = false;

//---------------------------------------
// Tiny byte-buffer helpers - FSB5's own fields are packed with no alignment
// guarantee, so everything is read via explicit little-endian byte assembly
// rather than pointer-cast + dereference.

static unsigned int ReadU32LE(const unsigned char *apData)
{
	return (unsigned int)apData[0] | ((unsigned int)apData[1] << 8) | ((unsigned int)apData[2] << 16) | ((unsigned int)apData[3] << 24);
}

static unsigned long long ReadU64LE(const unsigned char *apData)
{
	unsigned long long lo = ReadU32LE(apData);
	unsigned long long hi = ReadU32LE(apData + 4);
	return lo | (hi << 32);
}

static void AppendU32LE(std::vector<unsigned char> &aOut, unsigned int aVal)
{
	aOut.push_back((unsigned char)(aVal & 0xFF));
	aOut.push_back((unsigned char)((aVal >> 8) & 0xFF));
	aOut.push_back((unsigned char)((aVal >> 16) & 0xFF));
	aOut.push_back((unsigned char)((aVal >> 24) & 0xFF));
}

static void AppendU64LE(std::vector<unsigned char> &aOut, unsigned long long aVal)
{
	AppendU32LE(aOut, (unsigned int)(aVal & 0xFFFFFFFFull));
	AppendU32LE(aOut, (unsigned int)((aVal >> 32) & 0xFFFFFFFFull));
}

//---------------------------------------
// A real FSB5 sample header, parsed field-for-field per fsb5/__init__.py
// (see this file's top comment) - not every field FSB5 supports is kept,
// only what's needed to reconstruct a Vorbis or PCM16 sample.

struct cFsbSample
{
	tString msName;
	unsigned int mlFrequency;
	int mlChannels;
	size_t mlDataOffset; // relative to the start of the FSB's data section
	size_t mlDataSize;	  // computed from the next sample's offset (or dataSize for the last one)
	unsigned int mlNumPcmSamples;
	bool mbHasVorbisCrc;
	unsigned int mlVorbisCrc;
};

// Real FSB5 frequency enum (fsb5/__init__.py's frequency_values) - index 0
// is deliberately absent/invalid, matching the real table.
static unsigned int FsbFrequencyEnum(unsigned int alIndex)
{
	static const unsigned int kTable[10] = {0, 8000, 11000, 11025, 16000, 22050, 24000, 32000, 44100, 48000};
	if (alIndex < 10)
		return kTable[alIndex];
	return 0;
}

// Real FSB5 metadata chunk type enum value for the crc32-carrying chunk
// (fsb5/__init__.py's MetadataChunkType.VORBISDATA).
static const unsigned int kFsbChunkType_VorbisData = 11;

static bool ParseFsb5(const std::vector<unsigned char> &aFile, unsigned int &alModeOut, std::vector<cFsbSample> &aSamplesOut)
{
	aSamplesOut.clear();

	if (aFile.size() < 60 || std::memcmp(aFile.data(), "FSB5", 4) != 0)
		return false;

	const unsigned char *pData = aFile.data();

	unsigned int lVersion = ReadU32LE(pData + 4);
	unsigned int lNumSamples = ReadU32LE(pData + 8);
	unsigned int lSampleHeadersSize = ReadU32LE(pData + 12);
	unsigned int lNameTableSize = ReadU32LE(pData + 16);
	unsigned int lDataSize = ReadU32LE(pData + 20);
	unsigned int lMode = ReadU32LE(pData + 24);
	alModeOut = lMode;

	// Real header total size is 60 bytes for version>=1's layout (this
	// engine only ever needs to read real installed SOMA banks, which are
	// all version 1 - confirmed via a hex dump of a real install; version 0
	// bank support was not implemented since none exist in SOMA's data).
	size_t lHeaderSize = 60;
	if (lVersion == 0)
	{
		Log("SOMA menu sfx: FSB5 version 0 header not supported (real SOMA banks are all version 1)\n");
		return false;
	}

	size_t lPos = lHeaderSize;
	if (lPos + lSampleHeadersSize + lNameTableSize > aFile.size())
		return false;

	// Sample headers - variable length (a base 8-byte qword, then zero or
	// more 4-byte metadata chunks if its "next chunk" bit is set).
	std::vector<unsigned int> vDataOffsets;
	for (unsigned int i = 0; i < lNumSamples; ++i)
	{
		if (lPos + 8 > aFile.size())
			return false;

		unsigned long long raw = ReadU64LE(pData + lPos);
		lPos += 8;

		unsigned int lNextChunk = (unsigned int)(raw & 0x1);
		unsigned int lFreqIndex = (unsigned int)((raw >> 1) & 0xF);
		int lChannels = (int)((raw >> 5) & 0x1) + 1;
		unsigned int lDataOffset = (unsigned int)((raw >> 6) & 0xFFFFFFFull) * 16;
		unsigned int lNumSamplesInSound = (unsigned int)((raw >> 34) & 0x3FFFFFFFull);

		cFsbSample sample;
		sample.mlFrequency = FsbFrequencyEnum(lFreqIndex);
		sample.mlChannels = lChannels;
		sample.mlDataOffset = lDataOffset;
		sample.mlNumPcmSamples = lNumSamplesInSound;
		sample.mbHasVorbisCrc = false;
		sample.mlVorbisCrc = 0;

		while (lNextChunk)
		{
			if (lPos + 4 > aFile.size())
				return false;

			unsigned int craw = ReadU32LE(pData + lPos);
			lPos += 4;

			lNextChunk = craw & 0x1;
			unsigned int lChunkSize = (craw >> 1) & 0xFFFFFF;
			unsigned int lChunkType = (craw >> 25) & 0x7F;

			if (lPos + lChunkSize > aFile.size())
				return false;

			if (lChunkType == kFsbChunkType_VorbisData && lChunkSize >= 4)
			{
				sample.mbHasVorbisCrc = true;
				sample.mlVorbisCrc = ReadU32LE(pData + lPos);
			}
			else if (lChunkType == 2 /* FREQUENCY override */ && lChunkSize >= 4)
			{
				sample.mlFrequency = ReadU32LE(pData + lPos);
			}

			lPos += lChunkSize;
		}

		vDataOffsets.push_back(lDataOffset);
		aSamplesOut.push_back(sample);
	}

	// Name table: numSamples * uint32 offsets (relative to the table's own
	// start), then null-terminated names at those offsets.
	size_t lNameTableStart = lPos;
	if (lNameTableSize > 0)
	{
		if (lNameTableStart + lNumSamples * 4 > aFile.size())
			return false;

		for (unsigned int i = 0; i < lNumSamples; ++i)
		{
			unsigned int lOff = ReadU32LE(pData + lNameTableStart + i * 4);
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

	// Per-sample byte length within the data section = next sample's
	// dataOffset minus this one's (samples are stored in the same order as
	// their headers, confirmed via a real bank's actual offsets - the last
	// sample runs to dataSize).
	for (unsigned int i = 0; i < lNumSamples; ++i)
	{
		size_t lEnd = (i + 1 < lNumSamples) ? aSamplesOut[i + 1].mlDataOffset : lDataSize;
		aSamplesOut[i].mlDataSize = (lEnd > aSamplesOut[i].mlDataOffset) ? (lEnd - aSamplesOut[i].mlDataOffset) : 0;
	}

	// Stash the data section's absolute file offset by shifting every
	// sample's mlDataOffset to be file-absolute, so callers don't need to
	// track lPos themselves.
	for (unsigned int i = 0; i < lNumSamples; ++i)
		aSamplesOut[i].mlDataOffset += lPos;

	return true;
}

//---------------------------------------
// Real Ogg CRC32 (libogg's framing.c algorithm - MSB-first, polynomial
// 0x04c11db7, zero init, no reflection/final-xor - deliberately NOT the same
// table/algorithm as zlib/PNG's crc32).

static unsigned int OggCrc32(const unsigned char *apData, size_t alSize)
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
// Minimal real Ogg page muxer - takes an ordered list of raw packets and
// produces a byte-correct Ogg bitstream (page framing/lacing/CRC32), same
// algorithm libogg itself implements. Only what OggMuxVorbisSample() below
// needs: one logical stream, packets added in order, a single forced final
// flush with the real end-of-stream granulepos.

class cOggMuxer
{
public:
	explicit cOggMuxer(unsigned int alSerial) : mlSerial(alSerial), mlPageSeq(0), mbWroteAnyPage(false), mbContinuedIntoCurrentPage(false)
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

	const std::vector<unsigned char> &Bytes() const { return mOut; }

private:
	void FlushPage(bool abEos, long long alGranulePos)
	{
		if (mSegTable.empty())
			return;

		bool bBos = (mbWroteAnyPage == false);

		std::vector<unsigned char> page;
		page.push_back('O');
		page.push_back('g');
		page.push_back('g');
		page.push_back('S');
		page.push_back(0); // stream structure version

		unsigned char lHeaderType = 0;
		if (mbContinuedIntoCurrentPage)
			lHeaderType |= 1;
		if (bBos)
			lHeaderType |= 2;
		if (abEos)
			lHeaderType |= 4;
		page.push_back(lHeaderType);

		AppendU64LE(page, (unsigned long long)alGranulePos);
		AppendU32LE(page, mlSerial);
		AppendU32LE(page, mlPageSeq++);

		size_t lCrcPos = page.size();
		AppendU32LE(page, 0); // checksum placeholder, filled below

		page.push_back((unsigned char)mSegTable.size());
		page.insert(page.end(), mSegTable.begin(), mSegTable.end());
		page.insert(page.end(), mBody.begin(), mBody.end());

		unsigned int lCrc = OggCrc32(page.data(), page.size());
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
// Real Vorbis I identification header packet, built byte-for-byte per the
// public spec (https://xiph.org/vorbis/doc/Vorbis_I_spec.html section 4.2.2)
// using a small LSB-first bit packer matching libogg's oggpack_write()
// semantics - not read from any file, this is a from-scratch reconstruction
// (FSB5 never stores it at all, see this file's top comment).

class cBitPackerLSB
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

static std::vector<unsigned char> BuildVorbisIdHeader(int alChannels, unsigned int alFrequency)
{
	cBitPackerLSB bp;
	bp.Write(0x01, 8);
	const char *pTag = "vorbis";
	for (int i = 0; i < 6; ++i)
		bp.Write((unsigned long)(unsigned char)pTag[i], 8);
	bp.Write(0, 32);				// vorbis_version
	bp.Write((unsigned long)alChannels, 8);
	bp.Write(alFrequency, 32);
	bp.Write(0, 32); // bitrate_maximum
	bp.Write(0, 32); // bitrate_nominal
	bp.Write(0, 32); // bitrate_minimum
	bp.Write(8, 4);	 // blocksize_0 = log2(256)
	bp.Write(11, 4); // blocksize_1 = log2(2048)
	bp.Write(1, 1);	 // framing bit
	return bp.Bytes();
}

static std::vector<unsigned char> BuildVorbisCommentHeader()
{
	std::vector<unsigned char> out;
	out.push_back(0x03);
	const char *pTag = "vorbis";
	for (int i = 0; i < 6; ++i)
		out.push_back((unsigned char)pTag[i]);
	AppendU32LE(out, 0); // vendor string length
	AppendU32LE(out, 0); // user comment count
	out.push_back(0x01); // framing bit, byte-aligned already at this point
	return out;
}

// Reconstructs one Vorbis-mode FSB5 sample into a real, playable Ogg Vorbis
// file - see this file's top comment for the crc32-keyed setup header
// lookup this depends on.
static bool OggMuxVorbisSample(const cFsbSample &aSample, const unsigned char *apFileData, std::vector<unsigned char> &aOutOgg)
{
	if (aSample.mbHasVorbisCrc == false || aSample.mlVorbisCrc != kSomaVorbisSetupCrc32)
	{
		Log("SOMA menu sfx: sample '%s' uses an unrecognised Vorbis setup crc32 (%u) - skipped\n",
			aSample.msName.c_str(), aSample.mlVorbisCrc);
		return false;
	}

	std::vector<unsigned char> idHeader = BuildVorbisIdHeader(aSample.mlChannels, aSample.mlFrequency);
	std::vector<unsigned char> commentHeader = BuildVorbisCommentHeader();

	cOggMuxer muxer(1);
	muxer.AddPacket(idHeader.data(), idHeader.size());
	muxer.AddPacket(commentHeader.data(), commentHeader.size());
	muxer.AddPacket(kSomaVorbisSetupHeaderData, (size_t)kSomaVorbisSetupHeaderSize);

	// Real FSB5 Vorbis sample data is a sequence of (uint16 LE length,
	// payload) audio packets, zero-length-terminated.
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

static bool WritePcm16Wav(const cFsbSample &aSample, const unsigned char *apFileData, std::vector<unsigned char> &aOutWav)
{
	unsigned int lByteRate = aSample.mlFrequency * aSample.mlChannels * 2;
	unsigned int lBlockAlign = aSample.mlChannels * 2;
	unsigned int lDataBytes = (unsigned int)(aSample.mlNumPcmSamples * lBlockAlign);
	if (lDataBytes > aSample.mlDataSize)
		lDataBytes = (unsigned int)aSample.mlDataSize;

	aOutWav.clear();
	aOutWav.reserve(44 + lDataBytes);

	aOutWav.insert(aOutWav.end(), {'R', 'I', 'F', 'F'});
	AppendU32LE(aOutWav, 36 + lDataBytes);
	aOutWav.insert(aOutWav.end(), {'W', 'A', 'V', 'E'});
	aOutWav.insert(aOutWav.end(), {'f', 'm', 't', ' '});
	AppendU32LE(aOutWav, 16);
	aOutWav.push_back(1); aOutWav.push_back(0); // PCM
	aOutWav.push_back((unsigned char)aSample.mlChannels); aOutWav.push_back(0);
	AppendU32LE(aOutWav, aSample.mlFrequency);
	AppendU32LE(aOutWav, lByteRate);
	aOutWav.push_back((unsigned char)lBlockAlign); aOutWav.push_back(0);
	aOutWav.push_back(16); aOutWav.push_back(0); // bits per sample
	aOutWav.insert(aOutWav.end(), {'d', 'a', 't', 'a'});
	AppendU32LE(aOutWav, lDataBytes);

	const unsigned char *pData = apFileData + aSample.mlDataOffset;
	aOutWav.insert(aOutWav.end(), pData, pData + lDataBytes);
	return true;
}

//---------------------------------------

static bool ReadWholeFile(const tWString &asPath, std::vector<unsigned char> &aOut)
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

static bool WriteWholeFile(const tWString &asPath, const std::vector<unsigned char> &aData)
{
	std::ofstream f(cString::To8Char(asPath).c_str(), std::ios::binary | std::ios::trunc);
	if (f.is_open() == false)
		return false;
	f.write(reinterpret_cast<const char *>(aData.data()), (std::streamsize)aData.size());
	return f.good();
}

static tWString GetCacheDir()
{
	tWString sRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGCacheHome);
	tWString sDir = sRoot + _W("open-hpl/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);
	sDir += _W("soma/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);
	sDir += _W("sfx/");
	if (cPlatform::FolderExists(sDir) == false)
		cPlatform::CreateFolder(sDir);
	return sDir;
}

//---------------------------------------

void cSomaMenuSfx::EnsureCached(cResources *apResources)
{
	if (gbSomaMenuSfxReady)
		return;
	gbSomaMenuSfxReady = true; // only ever attempt this once per process, success or not

	if (apResources == NULL)
		return;

	tWString sCacheDir = GetCacheDir();

	// Skip the whole real-bank read/parse/convert pipeline if every real
	// output this session cares about is already sitting in the cache from
	// a previous run.
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
		const tWString &sFsbPath = apResources->GetFileSearcher()->GetFilePath("special/special_fx.fsb");
		if (sFsbPath != _W(""))
		{
			std::vector<unsigned char> vFile;
			if (ReadWholeFile(sFsbPath, vFile))
			{
				unsigned int lMode = 0;
				std::vector<cFsbSample> vSamples;
				if (ParseFsb5(vFile, lMode, vSamples))
				{
					struct cWantedSample
					{
						const char *pRealName;
						const char *pCacheFile;
					};
					const cWantedSample kWanted[] = {
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

					for (size_t i = 0; i < vSamples.size(); ++i)
					{
						for (size_t w = 0; w < sizeof(kWanted) / sizeof(kWanted[0]); ++w)
						{
							if (vSamples[i].msName != kWanted[w].pRealName)
								continue;

							std::vector<unsigned char> vOgg;
							if (OggMuxVorbisSample(vSamples[i], vFile.data(), vOgg))
								WriteWholeFile(sCacheDir + cString::To16Char(kWanted[w].pCacheFile), vOgg);
							break;
						}
					}
				}
				else
				{
					Log("SOMA menu sfx: failed to parse '%s' as an FSB5 bank\n", cString::To8Char(sFsbPath).c_str());
				}
			}
		}
		else
		{
			Log("SOMA menu sfx: could not find sounds/special/special_fx.fsb in the real SOMA install - menu click/hover/glitch sounds will be silent\n");
		}

		const tWString &sStreamFsbPath = apResources->GetFileSearcher()->GetFilePath("special/special_fx_stream.fsb");
		if (sStreamFsbPath != _W(""))
		{
			std::vector<unsigned char> vFile;
			if (ReadWholeFile(sStreamFsbPath, vFile))
			{
				unsigned int lMode = 0;
				std::vector<cFsbSample> vSamples;
				if (ParseFsb5(vFile, lMode, vSamples))
				{
					struct cPcmWanted { const char *pRealName; const wchar_t *pCacheFile; };
					static const cPcmWanted kPcmWanted[] = {
						{"new_game_sting", L"new_game_sting.wav"},
						// cSomaSplash's FG logo sting / menu ambience - see
						// the static bool declarations above for how these
						// two real sample names were confirmed.
						{"FG_Logo_Sting", L"fg_logo_sting.wav"},
						{"menu_bg_noise", L"menu_bg_noise.wav"},
					};

					for (size_t i = 0; i < vSamples.size(); ++i)
					{
						for (size_t w = 0; w < sizeof(kPcmWanted) / sizeof(kPcmWanted[0]); ++w)
						{
							if (vSamples[i].msName != kPcmWanted[w].pRealName)
								continue;

							std::vector<unsigned char> vWav;
							if (WritePcm16Wav(vSamples[i], vFile.data(), vWav))
								WriteWholeFile(sCacheDir + kPcmWanted[w].pCacheFile, vWav);
							break;
						}
					}
				}
			}
		}
		else
		{
			Log("SOMA menu sfx: could not find sounds/special/special_fx_stream.fsb in the real SOMA install - New Game sting will be silent\n");
		}
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
