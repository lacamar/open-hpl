#include "SomaFsb.h"
#include "SomaVorbisSetup_6d39bf3e.h"
#include "SomaVorbisSetup_b62ad8df.h"

#include <cstring>
#include <fstream>
#include <vector>

//---------------------------------------

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

struct cFsbSample
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

static const unsigned int kFsbMode_Pcm16 = 2;
static const unsigned int kFsbMode_Vorbis = 15;
static const unsigned int kFsbChunkType_Frequency = 2;
static const unsigned int kFsbChunkType_VorbisData = 11;

static unsigned int FsbFrequencyEnum(unsigned int alIndex)
{
	static const unsigned int kTable[10] = {0, 8000, 11000, 11025, 16000, 22050, 24000, 32000, 44100, 48000};
	return alIndex < 10 ? kTable[alIndex] : 0;
}

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
	alModeOut = ReadU32LE(pData + 24);

	// SOMA ships only version 1 banks (60-byte header)
	if (lVersion == 0)
		return false;

	size_t lPos = 60;
	if (lPos + lSampleHeadersSize + lNameTableSize > aFile.size())
		return false;

	for (unsigned int i = 0; i < lNumSamples; ++i)
	{
		if (lPos + 8 > aFile.size())
			return false;

		unsigned long long raw = ReadU64LE(pData + lPos);
		lPos += 8;

		unsigned int lNextChunk = (unsigned int)(raw & 0x1);

		cFsbSample sample;
		sample.mlFrequency = FsbFrequencyEnum((unsigned int)((raw >> 1) & 0xF));
		sample.mlChannels = (int)((raw >> 5) & 0x1) + 1;
		sample.mlDataOffset = (size_t)((raw >> 6) & 0xFFFFFFFull) * 16;
		sample.mlNumPcmSamples = (unsigned int)((raw >> 34) & 0x3FFFFFFFull);
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
			else if (lChunkType == kFsbChunkType_Frequency && lChunkSize >= 4)
			{
				sample.mlFrequency = ReadU32LE(pData + lPos);
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
			size_t lStrPos = lNameTableStart + ReadU32LE(pData + lNameTableStart + i * 4);
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

// libogg's CRC: MSB-first, poly 0x04c11db7, no reflection - not zlib's
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

	// libvorbisfile needs the id header alone on the first page, the other headers before audio
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

		std::vector<unsigned char> page = {'O', 'g', 'g', 'S', 0};

		unsigned char lHeaderType = 0;
		if (mbContinuedIntoCurrentPage) lHeaderType |= 1;
		if (mbWroteAnyPage == false) lHeaderType |= 2;
		if (abEos) lHeaderType |= 4;
		page.push_back(lHeaderType);

		AppendU64LE(page, (unsigned long long)alGranulePos);
		AppendU32LE(page, mlSerial);
		AppendU32LE(page, mlPageSeq++);

		size_t lCrcPos = page.size();
		AppendU32LE(page, 0);

		page.push_back((unsigned char)mSegTable.size());
		page.insert(page.end(), mSegTable.begin(), mSegTable.end());
		page.insert(page.end(), mBody.begin(), mBody.end());

		unsigned int lCrc = OggCrc32(page.data(), page.size());
		for (int i = 0; i < 4; ++i)
			page[lCrcPos + i] = (unsigned char)((lCrc >> (8 * i)) & 0xFF);

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

static std::vector<unsigned char> BuildVorbisCommentHeader()
{
	std::vector<unsigned char> out = {0x03, 'v', 'o', 'r', 'b', 'i', 's'};
	AppendU32LE(out, 0);
	AppendU32LE(out, 0);
	out.push_back(0x01);
	return out;
}

struct cVorbisSetup
{
	unsigned int mlCrc;
	const unsigned char *mpData;
	int mlSize;
};

static const cVorbisSetup kVorbisSetups[] = {
	{kVorbisSetupCrc32_6d39bf3e, kVorbisSetupData_6d39bf3e, (int)sizeof(kVorbisSetupData_6d39bf3e)},
	{kVorbisSetupCrc32_b62ad8df, kVorbisSetupData_b62ad8df, (int)sizeof(kVorbisSetupData_b62ad8df)},
};

static const cVorbisSetup *FindVorbisSetup(unsigned int alCrc)
{
	for (size_t i = 0; i < sizeof(kVorbisSetups) / sizeof(kVorbisSetups[0]); ++i)
		if (kVorbisSetups[i].mlCrc == alCrc)
			return &kVorbisSetups[i];
	return NULL;
}

static void OggMuxVorbisSample(const cFsbSample &aSample, const unsigned char *apFileData, const cVorbisSetup &aSetup,
							   std::vector<unsigned char> &aOutOgg)
{
	std::vector<unsigned char> idHeader = BuildVorbisIdHeader(aSample.mlChannels, aSample.mlFrequency);
	std::vector<unsigned char> commentHeader = BuildVorbisCommentHeader();

	cOggMuxer muxer(1);
	muxer.AddPacket(idHeader.data(), idHeader.size());
	muxer.ForcePageBoundary();
	muxer.AddPacket(commentHeader.data(), commentHeader.size());
	muxer.AddPacket(aSetup.mpData, (size_t)aSetup.mlSize);
	muxer.ForcePageBoundary();

	const unsigned char *pSampleData = apFileData + aSample.mlDataOffset;
	size_t lSampleSize = aSample.mlDataSize;
	size_t lOff = 0;
	while (lOff + 2 <= lSampleSize)
	{
		unsigned int lPacketLen = (unsigned int)pSampleData[lOff] | ((unsigned int)pSampleData[lOff + 1] << 8);
		lOff += 2;
		if (lPacketLen == 0 || lOff + lPacketLen > lSampleSize)
			break;

		muxer.AddPacket(pSampleData + lOff, lPacketLen);
		lOff += lPacketLen;
	}

	muxer.Finish((long long)aSample.mlNumPcmSamples);
	aOutOgg = muxer.Bytes();
}

static void WritePcm16Wav(const cFsbSample &aSample, const unsigned char *apFileData, std::vector<unsigned char> &aOutWav)
{
	unsigned int lBlockAlign = aSample.mlChannels * 2;
	unsigned int lDataBytes = (unsigned int)(aSample.mlNumPcmSamples * lBlockAlign);
	if (lDataBytes > aSample.mlDataSize)
		lDataBytes = (unsigned int)aSample.mlDataSize;

	aOutWav.clear();
	aOutWav.reserve(44 + lDataBytes);

	aOutWav.insert(aOutWav.end(), {'R', 'I', 'F', 'F'});
	AppendU32LE(aOutWav, 36 + lDataBytes);
	aOutWav.insert(aOutWav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
	AppendU32LE(aOutWav, 16);
	aOutWav.insert(aOutWav.end(), {1, 0, (unsigned char)aSample.mlChannels, 0});
	AppendU32LE(aOutWav, aSample.mlFrequency);
	AppendU32LE(aOutWav, aSample.mlFrequency * lBlockAlign);
	aOutWav.insert(aOutWav.end(), {(unsigned char)lBlockAlign, 0, 16, 0, 'd', 'a', 't', 'a'});
	AppendU32LE(aOutWav, lDataBytes);

	const unsigned char *pData = apFileData + aSample.mlDataOffset;
	aOutWav.insert(aOutWav.end(), pData, pData + lDataBytes);
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

//---------------------------------------

tWString cSomaFsb::GetCacheDir(const tWString &asSubDir)
{
	tWString sDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGCacheHome);
	const tWString vParts[] = {_W("open-hpl/"), _W("soma/"), asSubDir + _W("/")};
	for (size_t i = 0; i < sizeof(vParts) / sizeof(vParts[0]); ++i)
	{
		sDir += vParts[i];
		if (cPlatform::FolderExists(sDir) == false)
			cPlatform::CreateFolder(sDir);
	}
	return sDir;
}

bool cSomaFsb::WriteTextFile(const tWString &asPath, const tString &asText)
{
	return WriteWholeFile(asPath, std::vector<unsigned char>(asText.begin(), asText.end()));
}

void cSomaFsb::ExtractBank(cResources *apResources, const char *apBankPath, const tWString &asCacheDir,
						   const cSomaFsbWanted *apWanted, size_t alCount)
{
	const tWString &sPath = apResources->GetFileSearcher()->GetFilePath(apBankPath);
	if (sPath == _W(""))
	{
		Log("SOMA fsb: bank '%s' not found - its sounds will be silent\n", apBankPath);
		return;
	}

	std::vector<unsigned char> vFile;
	unsigned int lMode = 0;
	std::vector<cFsbSample> vSamples;
	if (ReadWholeFile(sPath, vFile) == false || ParseFsb5(vFile, lMode, vSamples) == false)
	{
		Log("SOMA fsb: failed to read '%s' as an FSB5 bank\n", apBankPath);
		return;
	}
	if (lMode != kFsbMode_Pcm16 && lMode != kFsbMode_Vorbis)
	{
		Log("SOMA fsb: '%s' uses unsupported mode %u\n", apBankPath, lMode);
		return;
	}

	for (size_t i = 0; i < vSamples.size(); ++i)
	{
		for (size_t w = 0; w < alCount; ++w)
		{
			if (vSamples[i].msName != apWanted[w].pSampleName)
				continue;

			std::vector<unsigned char> vOut;
			if (lMode == kFsbMode_Pcm16)
			{
				WritePcm16Wav(vSamples[i], vFile.data(), vOut);
			}
			else
			{
				const cVorbisSetup *pSetup = vSamples[i].mbHasVorbisCrc ? FindVorbisSetup(vSamples[i].mlVorbisCrc) : NULL;
				if (pSetup == NULL)
				{
					Log("SOMA fsb: '%s' in '%s' uses unknown Vorbis setup crc32 %u - skipped\n",
						vSamples[i].msName.c_str(), apBankPath, vSamples[i].mlVorbisCrc);
					break;
				}
				OggMuxVorbisSample(vSamples[i], vFile.data(), *pSetup, vOut);
			}
			WriteWholeFile(asCacheDir + cString::To16Char(apWanted[w].pCacheFile), vOut);
			break;
		}
	}
}
