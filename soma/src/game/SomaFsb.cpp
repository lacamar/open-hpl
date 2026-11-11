#include "SomaFsb.h"
#include "RustFsb.h"
#include "RustSomaSound.h"
#include "SomaConfig.h"
#include "SomaVorbisSetups.h"
#include <vorbis/vorbisfile.h>

#include <set>

#include <cstring>
#include <fstream>
#include <vector>

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

static const unsigned char *U(const tString &s) { return (const unsigned char *)s.data(); }

static void SinkName(void *apCtx, const unsigned char *apData, size_t alLen) { static_cast<tString *>(apCtx)->assign((const char *)apData, alLen); }

static OhplFsbSample ToRust(const cFsbSample &aSample)
{
	return {NULL,
			0,
			aSample.mlFrequency,
			aSample.mlChannels,
			aSample.mlDataOffset,
			aSample.mlDataSize,
			aSample.mlNumPcmSamples,
			aSample.mbHasVorbisCrc,
			aSample.mlVorbisCrc};
}

static void SinkBytes(void *apCtx, const unsigned char *apData, size_t alSize)
{
	static_cast<std::vector<unsigned char> *>(apCtx)->assign(apData, apData + alSize);
}

static void SinkSample(void *apCtx, const OhplFsbSample *apSample)
{
	cFsbSample sample;
	sample.msName = tString((const char *)apSample->name, apSample->name_len);
	sample.mlFrequency = apSample->frequency;
	sample.mlChannels = apSample->channels;
	sample.mlDataOffset = apSample->data_offset;
	sample.mlDataSize = apSample->data_size;
	sample.mlNumPcmSamples = apSample->num_pcm_samples;
	sample.mbHasVorbisCrc = apSample->has_vorbis_crc;
	sample.mlVorbisCrc = apSample->vorbis_crc;
	static_cast<std::vector<cFsbSample> *>(apCtx)->push_back(sample);
}

static bool ParseFsb5(const std::vector<unsigned char> &aFile, unsigned int &alModeOut, std::vector<cFsbSample> &aSamplesOut)
{
	aSamplesOut.clear();
	if (ohpl_fsb_parse(aFile.data(), aFile.size(), &alModeOut, SinkSample, &aSamplesOut))
		return true;
	aSamplesOut.clear();
	return false;
}

static const cVorbisSetup *FindVorbisSetup(unsigned int alCrc)
{
	for (size_t i = 0; i < sizeof(kVorbisSetups) / sizeof(kVorbisSetups[0]); ++i)
		if (kVorbisSetups[i].mlCrc == alCrc)
			return &kVorbisSetups[i];
	return NULL;
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

static bool DownmixOggToWav(const cFsbSample &aSample, const std::vector<unsigned char> &aOgg, std::vector<unsigned char> &aOutWav)
{
	cMemReader reader = {&aOgg, 0};
	ov_callbacks cb = {MemRead, NULL, NULL, NULL};
	OggVorbis_File vf;
	if (ov_open_callbacks(&reader, &vf, NULL, 0, cb) != 0)
		return false;
	int lCh = aSample.mlChannels;
	std::vector<float> vL(lCh), vR(lCh);
	ohpl_fsb_downmix_matrix(lCh, vL.data(), vR.data());
	std::vector<short> vPcm;
	vPcm.reserve((size_t)aSample.mlNumPcmSamples * 2);
	int lSection = 0;
	for (;;)
	{
		float **pBuf;
		long n = ov_read_float(&vf, &pBuf, 4096, &lSection);
		if (n <= 0)
			break;
		size_t lOld = vPcm.size();
		vPcm.resize(lOld + (size_t)n * 2);
		ohpl_gs5_fsb_downmix_block(pBuf, lCh, (size_t)n, vL.data(), vR.data(), vPcm.data() + lOld);
	}
	ov_clear(&vf);
	cFsbSample stereo = aSample;
	stereo.mlChannels = 2;
	OhplFsbSample rs = ToRust(stereo);
	ohpl_fsb_wav_pcm16(&rs, (const unsigned char *)vPcm.data(), vPcm.size() * 2, SinkBytes, &aOutWav);
	return true;
}

static bool EncodeSample(unsigned int alMode, const cFsbSample &aSample, const std::vector<unsigned char> &aFile, std::vector<unsigned char> &aOut)
{
	OhplFsbSample rs = ToRust(aSample);
	if (alMode == kFsbMode_Pcm16 || alMode == kFsbMode_ImaAdpcm)
		return ohpl_fsb_encode_pcm(alMode, &rs, aFile.data(), aFile.size(), SinkBytes, &aOut);

	const cVorbisSetup *pSetup = aSample.mbHasVorbisCrc ? FindVorbisSetup(aSample.mlVorbisCrc) : NULL;
	if (pSetup == NULL)
		return false;
	if (ohpl_fsb_mux_vorbis(&rs, aFile.data(), aFile.size(), pSetup->mpData, (size_t)pSetup->mlSize, SinkBytes, &aOut) == false)
		return false;
	if (aSample.mlChannels > 2)
	{
		std::vector<unsigned char> vOgg;
		vOgg.swap(aOut);
		return DownmixOggToWav(aSample, vOgg, aOut);
	}
	return true;
}

static bool FsbModeSupported(unsigned int alMode)
{
	return ohpl_gs5_fsb_mode_supported(alMode);
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
	const tWString vParts[] = {_W("open-hpl/"), SomaGameIdW(), asSubDir + _W("/")};
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

	std::vector<tString> vWantedNames;
	for (size_t w = 0; w < alCount; ++w)
		vWantedNames.push_back(apWanted[w].pSampleName);
	std::vector<OhplGs5Str> vWanted;
	for (const tString &sName : vWantedNames)
		vWanted.push_back({U(sName), sName.size()});
	for (size_t i = 0; i < vSamples.size(); ++i)
	{
		int64_t w = ohpl_gs5_fsb_wanted_index(U(vSamples[i].msName), vSamples[i].msName.size(), vWanted.data(), vWanted.size());
		if (w < 0)
			continue;
		std::vector<unsigned char> vOut;
		if (EncodeSample(lMode, vSamples[i], vFile, vOut) == false)
		{
			Log("SOMA fsb: '%s' in '%s' cannot be encoded (Vorbis setup crc32 %u or data outside bank) - skipped\n",
				vSamples[i].msName.c_str(), apBankPath, vSamples[i].mlVorbisCrc);
			continue;
		}
		WriteWholeFile(asCacheDir + cString::To16Char(apWanted[w].pCacheFile), vOut);
	}
}

void cSomaFsb::ExtractSamples(const tWString &asBankFile, const tWString &asCacheDir, const tString &asPrefix,
							  const std::vector<tString> &avSamples, std::map<tString, tString> &amapOut)
{
	std::vector<tString> vMissing;
	for (const tString &sSample : avSamples)
	{
		tString sOgg = asPrefix + sSample + ".ogg", sWav = asPrefix + sSample + ".wav";
		bool bOgg = cPlatform::FileExists(asCacheDir + cString::To16Char(sOgg));
		switch (ohpl_gs5_fsb_cached_state(bOgg, bOgg == false && cPlatform::FileExists(asCacheDir + cString::To16Char(sWav))))
		{
		case 1: amapOut[sSample] = sOgg; break;
		case 2: amapOut[sSample] = sWav; break;
		default: vMissing.push_back(sSample);
		}
	}
	if (vMissing.empty())
		return;
	std::vector<unsigned char> vFile;
	unsigned int lMode = 0;
	std::vector<cFsbSample> vSamples;
	if (asBankFile == _W("") || ReadWholeFile(asBankFile, vFile) == false || ParseFsb5(vFile, lMode, vSamples) == false)
		return;
	if (FsbModeSupported(lMode) == false)
		return;
	std::set<tString> setWanted(vMissing.begin(), vMissing.end());
	for (const cFsbSample &sample : vSamples)
	{
		if (setWanted.count(sample.msName) == 0)
			continue;
		std::vector<unsigned char> vOut;
		tString sFile;
		ohpl_gs5_fsb_output_name(U(asPrefix), asPrefix.size(), U(sample.msName), sample.msName.size(), lMode, sample.mlChannels, SinkName, &sFile);
		if (EncodeSample(lMode, sample, vFile, vOut) == false)
			continue;
		WriteWholeFile(asCacheDir + cString::To16Char(sFile), vOut);
		amapOut[sample.msName] = sFile;
	}
}
