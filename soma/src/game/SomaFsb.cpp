#include "SomaFsb.h"
#include "SomaVorbisSetups.h"
#include <vorbis/vorbisfile.h>

#include <set>

#include <cstring>
#include <fstream>
#include <vector>

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
static const unsigned int kFsbMode_ImaAdpcm = 7;
static const unsigned int kFsbMode_Vorbis = 15;
static const unsigned int kFsbChunkType_Channels = 1;
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

	static const int kChannels[4] = {1, 2, 6, 8};
	for (unsigned int i = 0; i < lNumSamples; ++i)
	{
		if (lPos + 8 > aFile.size())
			return false;

		unsigned long long raw = ReadU64LE(pData + lPos);
		lPos += 8;

		unsigned int lNextChunk = (unsigned int)(raw & 0x1);

		cFsbSample sample;
		sample.mlFrequency = FsbFrequencyEnum((unsigned int)((raw >> 1) & 0xF));
		sample.mlChannels = kChannels[(raw >> 5) & 0x3];
		sample.mlDataOffset = (size_t)((raw >> 7) & 0x7FFFFFFull) * 32;
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
			else if (lChunkType == kFsbChunkType_Channels && lChunkSize >= 1)
			{
				sample.mlChannels = pData[lPos];
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

static void WritePcm16Wav(const cFsbSample &aSample, const unsigned char *apPcm, size_t alBytes, std::vector<unsigned char> &aOutWav)
{
	unsigned int lBlockAlign = aSample.mlChannels * 2;
	unsigned int lDataBytes = (unsigned int)std::min<size_t>(aSample.mlNumPcmSamples * lBlockAlign, alBytes);

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
	aOutWav.insert(aOutWav.end(), apPcm, apPcm + lDataBytes);
}

// FMOD stores mono/stereo IMA ADPCM in the Xbox layout: 36-byte blocks per channel,
// 4-byte channel headers, then 4-byte words interleaved per channel, 64 samples each
static std::vector<short> DecodeXboxImaAdpcm(const cFsbSample &aSample, const unsigned char *apData)
{
	static const int kStep[89] = {7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80,
								  88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
								  598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749,
								  3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635,
								  13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
	static const int kIndex[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
	int lCh = aSample.mlChannels;
	size_t lBlock = 36 * lCh;
	std::vector<short> vOut;
	vOut.reserve((size_t)aSample.mlNumPcmSamples * lCh);
	for (size_t lOff = 0; lOff + lBlock <= aSample.mlDataSize && vOut.size() < (size_t)aSample.mlNumPcmSamples * lCh; lOff += lBlock)
	{
		const unsigned char *pBlock = apData + lOff;
		std::vector<short> vBlock(64 * lCh);
		for (int c = 0; c < lCh; ++c)
		{
			int lPred = (short)(pBlock[c * 4] | (pBlock[c * 4 + 1] << 8));
			int lIdx = std::min(88, (int)pBlock[c * 4 + 2]);
			for (int w = 0; w < 8; ++w)
				for (int b = 0; b < 8; ++b)
				{
					unsigned char lByte = pBlock[4 * lCh + (w * lCh + c) * 4 + b / 2];
					int n = (b & 1) ? lByte >> 4 : lByte & 0xF;
					int lStep = kStep[lIdx], lDiff = lStep >> 3;
					if (n & 1) lDiff += lStep >> 2;
					if (n & 2) lDiff += lStep >> 1;
					if (n & 4) lDiff += lStep;
					lPred = std::max(-32768, std::min(32767, (n & 8) ? lPred - lDiff : lPred + lDiff));
					lIdx = std::max(0, std::min(88, lIdx + kIndex[n]));
					vBlock[(w * 8 + b) * lCh + c] = (short)lPred;
				}
		}
		vOut.insert(vOut.end(), vBlock.begin(), vBlock.end());
	}
	return vOut;
}

struct cMemReader
{
	const std::vector<unsigned char> *mpData;
	size_t mlPos;
};

static size_t MemRead(void *apDest, size_t alSize, size_t alCount, void *apSrc)
{
	cMemReader *r = (cMemReader *)apSrc;
	size_t lBytes = std::min(alSize * alCount, r->mpData->size() - r->mlPos);
	std::memcpy(apDest, r->mpData->data() + r->mlPos, lBytes);
	r->mlPos += lBytes;
	return alSize ? lBytes / alSize : 0;
}

// OpenAL only plays mono and stereo: fold 5.1/7.1 (Vorbis channel order) down like FMOD's stereo mix, LFE dropped
// FSB multichannel is Vorbis order (FL C FR SL SR [BL BR] LFE); gains fitted to the official game's output
static void DownmixMatrix(int alCh, std::vector<float> &aL, std::vector<float> &aR)
{
	aL.assign(alCh, 0.5f);
	aR.assign(alCh, 0.5f);
	aL[0] = aR[2] = 1;
	aL[2] = aR[0] = 0;
	for (int c = 3; c + 1 < alCh - 1; c += 2)
	{
		aR[c] = 0;
		aL[c + 1] = 0;
	}
}

static bool DownmixOggToWav(const cFsbSample &aSample, const std::vector<unsigned char> &aOgg, std::vector<unsigned char> &aOutWav)
{
	cMemReader reader = {&aOgg, 0};
	ov_callbacks cb = {MemRead, NULL, NULL, NULL};
	OggVorbis_File vf;
	if (ov_open_callbacks(&reader, &vf, NULL, 0, cb) != 0)
		return false;
	int lCh = aSample.mlChannels;
	std::vector<float> vL, vR;
	DownmixMatrix(lCh, vL, vR);
	std::vector<short> vPcm;
	vPcm.reserve((size_t)aSample.mlNumPcmSamples * 2);
	int lSection = 0;
	for (;;)
	{
		float **pBuf;
		long n = ov_read_float(&vf, &pBuf, 4096, &lSection);
		if (n <= 0)
			break;
		for (long i = 0; i < n; ++i)
		{
			float l = 0, r = 0;
			for (int c = 0; c < lCh; ++c)
			{
				l += pBuf[c][i] * vL[c];
				r += pBuf[c][i] * vR[c];
			}
			vPcm.push_back((short)cMath::Clamp(l * 32767.0f, -32768.0f, 32767.0f));
			vPcm.push_back((short)cMath::Clamp(r * 32767.0f, -32768.0f, 32767.0f));
		}
	}
	ov_clear(&vf);
	cFsbSample stereo = aSample;
	stereo.mlChannels = 2;
	WritePcm16Wav(stereo, (const unsigned char *)vPcm.data(), vPcm.size() * 2, aOutWav);
	return true;
}

static void WritePcm16WavStereo(const cFsbSample &aSample, const short *apPcm, size_t alCount, std::vector<unsigned char> &aOutWav)
{
	int lCh = aSample.mlChannels;
	if (lCh <= 2)
	{
		WritePcm16Wav(aSample, (const unsigned char *)apPcm, alCount * 2, aOutWav);
		return;
	}
	std::vector<float> vL, vR;
	DownmixMatrix(lCh, vL, vR);
	std::vector<short> vOut;
	vOut.reserve(alCount / lCh * 2);
	for (size_t i = 0; i + lCh <= alCount; i += lCh)
	{
		float l = 0, r = 0;
		for (int c = 0; c < lCh; ++c)
		{
			l += apPcm[i + c] * vL[c];
			r += apPcm[i + c] * vR[c];
		}
		vOut.push_back((short)cMath::Clamp(l, -32768.0f, 32767.0f));
		vOut.push_back((short)cMath::Clamp(r, -32768.0f, 32767.0f));
	}
	cFsbSample stereo = aSample;
	stereo.mlChannels = 2;
	WritePcm16Wav(stereo, (const unsigned char *)vOut.data(), vOut.size() * 2, aOutWav);
}

static bool EncodeSample(unsigned int alMode, const cFsbSample &aSample, const unsigned char *apFileData, std::vector<unsigned char> &aOut)
{
	if (alMode == kFsbMode_Pcm16)
		WritePcm16WavStereo(aSample, (const short *)(apFileData + aSample.mlDataOffset), aSample.mlDataSize / 2, aOut);
	else if (alMode == kFsbMode_ImaAdpcm)
	{
		std::vector<short> vPcm = DecodeXboxImaAdpcm(aSample, apFileData + aSample.mlDataOffset);
		WritePcm16WavStereo(aSample, vPcm.data(), vPcm.size(), aOut);
	}
	else
	{
		const cVorbisSetup *pSetup = aSample.mbHasVorbisCrc ? FindVorbisSetup(aSample.mlVorbisCrc) : NULL;
		if (pSetup == NULL)
			return false;
		OggMuxVorbisSample(aSample, apFileData, *pSetup, aOut);
		if (aSample.mlChannels > 2)
		{
			std::vector<unsigned char> vOgg;
			vOgg.swap(aOut);
			return DownmixOggToWav(aSample, vOgg, aOut);
		}
	}
	return true;
}

static bool FsbModeSupported(unsigned int alMode)
{
	return alMode == kFsbMode_Pcm16 || alMode == kFsbMode_ImaAdpcm || alMode == kFsbMode_Vorbis;
}

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
	if (FsbModeSupported(lMode) == false)
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
			if (EncodeSample(lMode, vSamples[i], vFile.data(), vOut) == false)
			{
				Log("SOMA fsb: '%s' in '%s' uses unknown Vorbis setup crc32 %u - skipped\n",
					vSamples[i].msName.c_str(), apBankPath, vSamples[i].mlVorbisCrc);
				break;
			}
			WriteWholeFile(asCacheDir + cString::To16Char(apWanted[w].pCacheFile), vOut);
			break;
		}
	}
}

void cSomaFsb::ExtractSamples(cResources *apResources, const tString &asBankPath, const tWString &asCacheDir, const tString &asPrefix,
							  const std::vector<tString> &avSamples, std::map<tString, tString> &amapOut)
{
	std::vector<tString> vMissing;
	for (const tString &sSample : avSamples)
	{
		tString sOgg = asPrefix + sSample + ".ogg", sWav = asPrefix + sSample + ".wav";
		if (cPlatform::FileExists(asCacheDir + cString::To16Char(sOgg)))
			amapOut[sSample] = sOgg;
		else if (cPlatform::FileExists(asCacheDir + cString::To16Char(sWav)))
			amapOut[sSample] = sWav;
		else
			vMissing.push_back(sSample);
	}
	if (vMissing.empty())
		return;
	const tWString &sPath = apResources->GetFileSearcher()->GetFilePath(asBankPath);
	std::vector<unsigned char> vFile;
	unsigned int lMode = 0;
	std::vector<cFsbSample> vSamples;
	if (sPath == _W("") || ReadWholeFile(sPath, vFile) == false || ParseFsb5(vFile, lMode, vSamples) == false)
		return;
	if (FsbModeSupported(lMode) == false)
		return;
	std::set<tString> setWanted(vMissing.begin(), vMissing.end());
	for (const cFsbSample &sample : vSamples)
	{
		if (setWanted.count(sample.msName) == 0)
			continue;
		std::vector<unsigned char> vOut;
		tString sFile = asPrefix + sample.msName + (lMode == kFsbMode_Vorbis && sample.mlChannels <= 2 ? ".ogg" : ".wav");
		if (EncodeSample(lMode, sample, vFile.data(), vOut) == false)
			continue;
		WriteWholeFile(asCacheDir + cString::To16Char(sFile), vOut);
		amapOut[sample.msName] = sFile;
	}
}
