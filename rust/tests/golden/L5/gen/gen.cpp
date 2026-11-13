// L5 harness. Links the engine (libHPL2.a) with a recording OAL; checks the Rust backends against the golden vectors.
//   gen oal                    OAL call trace and log lines of a fixed sound scenario
//   gen bitmaps <file>...      "<file> <hash>" for each image
//   gen video <ogv> <frames>   frame hashes
//   gen audio <file>...        decoded sample hash (Rust path only; the C++ reference is gen_ref)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <vorbis/vorbisfile.h>

#include "graphics/Bitmap.h"
#include "impl/BitmapLoaderDevilDDS.h"
#include "impl/BitmapLoaderDevilMisc.h"
#include "impl/LowLevelSoundOpenAL.h"
#include "impl/OpenALSoundChannel.h"
#include "impl/OpenALSoundData.h"
#include "impl/OpenALSoundEnvironment.h"
#include "impl/VideoStreamTheora.h"
#include "graphics/Texture.h"
#include "system/String.h"
#include "system/Platform.h"
#include "impl/tinyXML/tinyxml.h"
#include "impl/RustL5.h"

using namespace hpl;

extern std::vector<std::string> gTrace;
extern std::vector<std::string> gFakeDevices;
extern std::string gFakeDefault;
extern int gFakeFailInits;
extern bool gFakePlayFail;

static void LogCb(eLogOutputType t, const char* m)
{
	std::string s = "LOG " + std::to_string((int)t) + " ";
	for (const char* c = m; *c; ++c)
	{
		if (*c == '\n') s += "\\n";
		else s += *c;
	}
	gTrace.push_back(s);
	if (t == eLogOutputType_FatalError)
		for (size_t i = 0; i < gTrace.size(); ++i) fprintf(stderr, "%s\n", gTrace[i].c_str());
}

static void Mark(const char* s) { gTrace.push_back(std::string("== ") + s); }

static void ChannelOps(iSoundChannel* ch)
{
	ch->Play();
	ch->SetSpeed(1.5f);
	ch->SetVolume(0.8f);
	ch->SetLooping(true);
	ch->SetPan(0.1f);
	ch->Set3D(true);
	ch->SetPositionIsRelative(true);
	ch->SetPosition(cVector3f(1, 2, 3));
	ch->SetVelocity(cVector3f(4, 5, 6));
	ch->SetPositionIsRelative(false);
	ch->SetMinDistance(1);
	ch->SetMaxDistance(9);
	ch->SetPaused(true);
	ch->IsPlaying();
	ch->IsBufferUnderrun();
	ch->GetElapsedTime();
	ch->GetTotalTime();
	ch->SetElapsedTime(-1);
	ch->SetElapsedTime(1.5);
	ch->SetElapsedTime(5);
	ch->SetPriority(7);
	ch->SetPriorityModifier(300);
	ch->GetPriority();
	ch->SetAffectedByEnv(true);
	ch->SetAffectedByEnv(false);
	cOpenALSoundChannel* oc = static_cast<cOpenALSoundChannel*>(ch);
	oc->SetFiltering(true, 3);
	oc->SetFilterGain(0.5f);
	oc->SetFilterGainHF(0.25f);
	oc->SetFilterGainLF(0.125f);
	oc->SetEnvSendGain(0.75f);
	ch->Stop();
	ch->SetVolume(0.1f);
}

static void InitVariant(const char* label, int dev, int failInits, const char* filter, const char* envDev)
{
	Mark(label);
	if (envDev) setenv("OPENHPL_SOUND_DEVICE", envDev, 1);
	else unsetenv("OPENHPL_SOUND_DEVICE");
	iLowLevelSound::SetSoundDeviceNameFilter(filter);
	gFakeFailInits = failInits;
	cLowLevelSoundOpenAL low;
	low.Init(dev, false, 32, 100, true, true, 16, 8, 8192, 4, false);
	low.GetCurrentSoundDevice();
}

static int RunOal()
{
	gFakeDevices = {"Alpha Speaker", "Beta Headset", "Gamma HDMI"};
	gFakeDefault = "Beta";
	SetLogMessageCallback(LogCb);

	Mark("main");
	iLowLevelSound::SetSoundDeviceNameFilter("");
	{
		cLowLevelSoundOpenAL low;
		low.Init(-1, false, 32, 100, true, true, 16, 8, 8192, 4, true);
		low.SetVolume(0.5f);
		low.SetListenerAttributes(cVector3f(1, 2, 3), cVector3f(0.5f, 0, 0), cVector3f(0, 0, 1), cVector3f(0, 1, 0));
		low.SetListenerPosition(cVector3f(4, 5, 6));
		low.SetSetRolloffFactor(0.7f);
		low.UpdateSound(0.016f);
		low.SetEnvVolume(0.5f);
		low.SetListenerAttenuation(false);
		low.LoadSoundEnvironment("x.env");
		low.SetSoundEnvironment(NULL);
		low.FadeSoundEnvironment(NULL, NULL, 0.5f);

		Mark("sample");
		iSoundData* d1 = low.LoadSoundData("a", _W("snd/a_stereo.wav"), "wav", false, false);
		d1->IsStereo();
		iSoundChannel* c1 = d1->CreateChannel(5);
		ChannelOps(c1);
		delete c1;
		iSoundChannel* c1b = d1->CreateChannel(9);
		delete c1b;
		delete d1;

		Mark("stream");
		iSoundData* d2 = low.LoadSoundData("m", _W("music/m.ogg"), "ogg", true, true);
		d2->IsStereo();
		iSoundChannel* c2 = d2->CreateChannel(1);
		ChannelOps(c2);
		delete c2;
		gFakePlayFail = true;
		iSoundChannel* c2f = d2->CreateChannel(1);
		gFakePlayFail = false;
		gTrace.push_back(c2f ? "channel" : "no channel");
		delete d2;

		Mark("missing");
		iSoundData* d3 = low.LoadSoundData("x", _W("missing.wav"), "wav", false, false);
		iSoundData* d4 = low.LoadSoundData("y", _W("missing.ogg"), "ogg", true, false);
		gTrace.push_back(d3 || d4 ? "loaded" : "both null");
	}

	InitVariant("fail1", -1, 1, "", NULL);
	InitVariant("fail2", 1, 2, "", NULL);
	InitVariant("fail3", -2, 3, "", NULL);
	InitVariant("fail4", 0, 4, "", NULL);
	InitVariant("fail5", 0, 5, "", NULL);
	InitVariant("filter", -1, 0, "gam", NULL);
	InitVariant("nofilter", -1, 0, "zzz", NULL);
	InitVariant("env", 2, 0, "", "Gamma HDMI");

	for (size_t i = 0; i < gTrace.size(); ++i) printf("%s\n", gTrace[i].c_str());
	return 0;
}

static unsigned long long Fnv(const unsigned char* p, size_t n, unsigned long long h = 0xcbf29ce484222325ULL)
{
	for (size_t i = 0; i < n; ++i)
	{
		h ^= p[i];
		h *= 0x100000001b3ULL;
	}
	return h;
}

static int RunBitmaps(int argc, char** argv)
{
	cBitmapLoaderDevilDDS dds;
	cBitmapLoaderDevilMisc misc;
	for (int i = 2; i < argc; ++i)
	{
		tWString w = cString::To16Char(argv[i]);
		std::string ext = cString::ToLowerCase(cString::GetFileExt(argv[i]));
		cBitmap* b = ext == "dds" ? dds.LoadBitmap(w, 0) : misc.LoadBitmap(w, 0);
		if (!b)
		{
			printf("%s none\n", argv[i]);
			continue;
		}
		unsigned long long h = 0xcbf29ce484222325ULL;
		int meta[8] = {b->GetWidth(), b->GetHeight(), b->GetDepth(), b->GetBytesPerPixel(), (int)b->GetPixelFormat(),
					   b->IsCompressed(), b->GetNumOfImages(), b->GetNumOfMipMaps()};
		h = Fnv((const unsigned char*)meta, sizeof(meta), h);
		for (int im = 0; im < b->GetNumOfImages(); ++im)
			for (int mp = 0; mp < b->GetNumOfMipMaps(); ++mp)
			{
				cBitmapData* d = b->GetData(im, mp);
				h = Fnv((const unsigned char*)&d->mlSize, sizeof(int), h);
				h = Fnv(d->mpData, d->mlSize, h);
			}
		printf("%s %016llx\n", argv[i], h);
		hplDelete(b);
	}
	return 0;
}

class cCaptureTexture : public iTexture
{
public:
	cCaptureTexture() : iTexture("", _W(""), eTextureType_2D, eTextureUsage_Normal, NULL), mHash(0), mFrames(0) {}
	unsigned long long mHash;
	int mFrames;
	void SetRawData(int, const cVector3l&, const cVector3l& s, ePixelFormat, void* d)
	{
		mHash = Fnv((const unsigned char*)d, (size_t)s.x * s.y * 3);
		++mFrames;
	}
	bool CreateFromBitmap(cBitmap*) { return false; }
	bool CreateAnimFromBitmapVec(std::vector<cBitmap*>*) { return false; }
	bool CreateCubeFromBitmapVec(std::vector<cBitmap*>*) { return false; }
	bool CreateFromRawData(const cVector3l&, ePixelFormat, unsigned char*) { return false; }
	void Update(float) {}
	void SetFilter(eTextureFilter) {}
	void SetAnisotropyDegree(float) {}
	void SetWrapS(eTextureWrap) {}
	void SetWrapT(eTextureWrap) {}
	void SetWrapR(eTextureWrap) {}
	void SetWrapSTR(eTextureWrap) {}
	void SetCompareMode(eTextureCompareMode) {}
	void SetCompareFunc(eTextureCompareFunc) {}
	void SetRedFromAlpha(bool) {}
	void AutoGenerateMipmaps() {}
	bool HasAnimation() { return false; }
	void NextFrame() {}
	void PrevFrame() {}
	float GetT() { return 0; }
	float GetTimeCount() { return 0; }
	void SetTimeCount(float) {}
	int GetCurrentLowlevelHandle() { return 0; }
	bool GetRawPixelsRGBAFloat(std::vector<float>&) { return false; }
};

static int RunVideo(int argc, char** argv)
{
	SetLogMessageCallback(LogCb);
	cVideoStreamTheora_Loader loader;
	for (int i = 2; i + 1 < argc; i += 2)
	{
		int steps = atoi(argv[i + 1]);
		iVideoStream* v = loader.LoadVideo(cString::To16Char(argv[i]));
		if (!v)
		{
			printf("%s load failed\n", argv[i]);
			continue;
		}
		printf("%s size %d %d\n", argv[i], v->GetSize().x, v->GetSize().y);
		cCaptureTexture tex;
		v->SetLoop(true);
		v->Play();
		for (int s = 0; s < steps; ++s)
		{
			v->Update(1.0f / 30.0f);
			int before = tex.mFrames;
			v->CopyToTexture(&tex);
			printf("step %d %s %016llx\n", s, tex.mFrames != before ? "frame" : "none", tex.mHash);
		}
		v->Pause(true);
		v->Stop();
		hplDelete(v);
	}
	for (size_t i = 0; i < gTrace.size(); ++i) printf("%s\n", gTrace[i].c_str());
	return 0;
}


static std::string H(float v)
{
	unsigned b;
	memcpy(&b, &v, 4);
	char s[16];
	snprintf(s, sizeof(s), "%08x", b);
	return s;
}

static int RunEnv(int argc, char** argv)
{
	for (int i = 2; i < argc; ++i)
	{
		cOpenALSoundEnvironment e;
		if (!e.CreateFromFile(argv[i]))
		{
			printf("%s none\n", argv[i]);
			continue;
		}
		OhplL5EnvParams p;
		e.ToRust(p);
		printf("%s name=%s", argv[i], e.GetName().c_str());
		const float* fl = (const float*)&p;
		for (int k = 0; k < 26; ++k) printf(" %s", H(fl[k]).c_str());
		printf(" %d\n", p.decay_hf_limit);
	}
	return 0;
}

// The original BMFont reading of cSDLFontData::CreateFromBitmapFile, parse only.
static void FontRef(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) { printf("%s open\n", path); return; }
	TiXmlDocument* doc = new TiXmlDocument();
	if (!doc->LoadFile(f)) { printf("%s parse\n", path); fclose(f); delete doc; return; }
	TiXmlElement* root = doc->RootElement();
	TiXmlElement* common = root ? root->FirstChildElement("common") : NULL;
	TiXmlElement* info = root ? root->FirstChildElement("info") : NULL;
	if (!common || !info || !root->FirstChildElement("chars") || !root->FirstChildElement("pages")) { printf("%s lacks\n", path); fclose(f); delete doc; return; }
	int outline = cString::ToInt(info->Attribute("outline"), 0);
	int lh = cString::ToInt(common->Attribute("lineHeight"), 0) + outline * 2;
	int base = cString::ToInt(common->Attribute("base"), 0) + outline * 2;
	float height = (float)lh;
	float ratio = (float)base / (float)lh;
	int largest = -1;
	TiXmlElement* chars = root->FirstChildElement("chars");
	for (TiXmlElement* c = chars->FirstChildElement("char"); c; c = c->NextSiblingElement("char"))
		largest = std::max(largest, cString::ToInt(c->Attribute("id"), 0));
	long long count = (long long)largest + 1;
	if (count < 0 || count > (1 << 24)) { printf("%s unreasonable %d\n", path, largest); fclose(f); delete doc; return; }
	printf("%s ok lh=%d base=%d h=%s r=%s largest=%d count=%lld\n", path, lh, base, H(height).c_str(), H(ratio).c_str(), largest, count);
	int pages = 0;
	for (TiXmlElement* pg = root->FirstChildElement("pages")->FirstChildElement("page"); pg; pg = pg->NextSiblingElement("page"), ++pages)
		printf(" page %s\n", pg->Attribute("file") ? pg->Attribute("file") : "");
	for (TiXmlElement* c = chars->FirstChildElement("char"); c; c = c->NextSiblingElement("char"))
	{
		int id = cString::ToInt(c->Attribute("id"), 0);
		int page = cString::ToInt(c->Attribute("page"), 0);
		int st = (id < 0 || id >= count) ? 1 : (page < 0 || page >= pages) ? 2 : 0;
		printf(" char %d %d %d %d %d %d %d %d %d st=%d\n", id, cString::ToInt(c->Attribute("x"), 0), cString::ToInt(c->Attribute("y"), 0),
			   cString::ToInt(c->Attribute("width"), 0), cString::ToInt(c->Attribute("height"), 0), cString::ToInt(c->Attribute("xoffset"), 0),
			   cString::ToInt(c->Attribute("yoffset"), 0), cString::ToInt(c->Attribute("xadvance"), 0) + outline, page, st);
	}
	fclose(f);
	delete doc;
}

static void PageSink(void* ctx, const uint8_t* n, size_t l) { ((std::string*)ctx)->assign((const char*)n, l); }

static void FontRust(const char* path)
{
	int32_t st = 0, largest = -1;
	OhplL5Font* f = ohpl_l5_font_load(path, &st, &largest);
	if (!f)
	{
		static const char* names[] = {"ok", "open", "parse", "lacks", "unreasonable"};
		printf("%s %s", path, names[st]);
		if (st == 4) printf(" %d", largest);
		printf("\n");
		return;
	}
	OhplL5FontInfo i;
	ohpl_l5_font_info(f, &i);
	printf("%s ok lh=%d base=%d h=%s r=%s largest=%d count=%lld\n", path, i.line_height, i.base, H(i.height).c_str(), H(i.size_ratio_x).c_str(),
		   i.largest_id, (long long)i.glyph_count);
	for (size_t k = 0; k < i.pages; ++k)
	{
		std::string n;
		ohpl_l5_font_page(f, k, PageSink, &n);
		printf(" page %s\n", n.c_str());
	}
	for (size_t k = 0; k < i.chars; ++k)
	{
		OhplL5FontChar c;
		ohpl_l5_font_char(f, k, &c);
		printf(" char %d %d %d %d %d %d %d %d %d st=%d\n", c.id, c.x, c.y, c.width, c.height, c.x_offset, c.y_offset, c.advance, c.page, c.status);
	}
	ohpl_l5_font_free(f);
}

static int RunFont(int argc, char** argv, bool rust)
{
	for (int i = 2; i < argc; ++i)
	{
		if (rust) FontRust(argv[i]);
		else FontRef(argv[i]);
	}
	return 0;
}

// OAL_WAVSample.cpp::CreateFromBuffer and OAL_OggSample.cpp::LoadOgg, verbatim apart from the output.
struct WavHdr { char id[4]; int size; char fmt[4]; };
struct WavFmt { char id[4]; int size; short tag, channels; int rate, byteRate; short align, bits; };
struct Chunk { char id[4]; int size; };

static const char* FindChunk(const char* start, const char* end, const char* id)
{
	Chunk chunk;
	const char* ptr = start;
	while (ptr < (end - sizeof(Chunk)))
	{
		memcpy(&chunk, ptr, sizeof(Chunk));
		if (!memcmp(chunk.id, id, 4)) return ptr;
		ptr += sizeof(Chunk) + chunk.size;
	}
	return 0;
}

static unsigned long long Fold(unsigned long long h, const void* p, size_t n) { return Fnv((const unsigned char*)p, n, h); }

static bool AudioRef(const char* path, uint64_t* out)
{
	std::string ext = cString::ToLowerCase(cString::GetFileExt(path));
	FILE* f = fopen(path, "rb");
	if (!f) return false;
	if (ext == "wav")
	{
		fseek(f, 0, SEEK_END);
		size_t n = ftell(f);
		fseek(f, 0, SEEK_SET);
		std::vector<char> buf(n);
		if (fread(buf.data(), n, 1, f) != 1 && n) { fclose(f); return false; }
		fclose(f);
		const char *start = buf.data(), *end = start + n, *ptr = start;
		if (n < sizeof(WavHdr)) return false;
		WavHdr rh;
		memcpy(&rh, ptr, sizeof(rh));
		ptr += sizeof(rh);
		if (memcmp(rh.id, "RIFF", 4) || memcmp(rh.fmt, "WAVE", 4)) return false;
		ptr = FindChunk(ptr, end, "fmt ");
		if (!ptr || ptr + sizeof(WavFmt) > end) return false;
		WavFmt wf;
		memcpy(&wf, ptr, sizeof(wf));
		ptr += sizeof(Chunk) + wf.size;
		if (wf.tag != 1) return false;
		ptr = FindChunk(ptr, end, "data");
		if (!ptr) return false;
		Chunk dc;
		memcpy(&dc, ptr, sizeof(dc));
		const char* base = ptr + sizeof(Chunk);
		size_t size = dc.size;
		if (size > (size_t)(end - base)) return false;
		long long samples;
		int bits;
		if (wf.channels == 2) { bits = wf.bits == 8 ? 8 : 16; samples = wf.bits == 8 ? size / 2 : size / 4; }
		else { bits = wf.bits == 8 ? 8 : 16; samples = wf.bits == 8 ? size : size / 2; }
		int ch = wf.channels, rate = wf.rate;
		unsigned long long h = 0xcbf29ce484222325ULL;
		h = Fold(h, &ch, 4); h = Fold(h, &rate, 4); h = Fold(h, &bits, 4); h = Fold(h, &samples, 8);
		unsigned long long sz = size;
		h = Fold(h, &sz, 8);
		*out = Fold(h, base, size);
		return true;
	}
	if (ext == "ogg")
	{
		OggVorbis_File vf;
		if (ov_open_callbacks(f, &vf, NULL, 0, OV_CALLBACKS_DEFAULT) < 0) { fclose(f); return false; }
		vorbis_info* vi = ov_info(&vf, -1);
		int ch = vi->channels, bits = 16;
		long rate = vi->rate;
		long long samples = (long long)ov_pcm_total(&vf, -1);
		std::vector<char> pcm;
		char chunk[16384];
		for (;;)
		{
			int sec;
			long n = ov_read(&vf, chunk, sizeof(chunk), 0, 2, 1, &sec);
			if (n == 0) break;
			if (n < 0) { ov_clear(&vf); return false; }
			pcm.insert(pcm.end(), chunk, chunk + n);
		}
		ov_clear(&vf);
		int rate32 = (int)rate;
		uint64_t h = 0xcbf29ce484222325ULL;
		h = Fold(h, &ch, 4); h = Fold(h, &rate32, 4); h = Fold(h, &bits, 4); h = Fold(h, &samples, 8);
		unsigned long long sz = pcm.size();
		h = Fold(h, &sz, 8);
		*out = Fold(h, pcm.data(), pcm.size());
		return true;
	}
	fclose(f);
	return false;
}

static int RunAudio(int argc, char** argv, bool rust)
{
	for (int i = 2; i < argc; ++i)
	{
		uint64_t h = 0;
		bool ok = rust ? ohpl_l5_audio_hash(argv[i], &h) : AudioRef(argv[i], &h);
		if (ok) printf("%s %016llx\n", argv[i], h);
		else printf("%s none\n", argv[i]);
	}
	return 0;
}

// the engine's own main() is linked in; it needs this symbol
int hplMain(const std::string&) { return 0; }

int main(int argc, char** argv)
{
	if (argc < 2) return 2;
	if (!strcmp(argv[1], "oal")) return RunOal();
	if (!strcmp(argv[1], "bitmaps")) return RunBitmaps(argc, argv);
	if (!strcmp(argv[1], "video")) return RunVideo(argc, argv);
	if (!strcmp(argv[1], "env")) return RunEnv(argc, argv);
	if (!strcmp(argv[1], "font")) return RunFont(argc, argv, getenv("OHPL_IMPL") && !strcmp(getenv("OHPL_IMPL"), "rust"));
	if (!strcmp(argv[1], "audio")) return RunAudio(argc, argv, getenv("OHPL_IMPL") && !strcmp(getenv("OHPL_IMPL"), "rust"));
	return 2;
}
