// Recording stand-in for OALWrapper: every call becomes one trace line. Pointers are small ids,
// floats and doubles are printed as hex bits so the Rust fake in backend/media/tests.rs can match.
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "OALWrapper/OAL_Funcs.h"
#include "OALWrapper/OAL_Effect_Reverb.h"

std::vector<std::string> gTrace;
std::vector<std::string> gFakeDevices;
std::string gFakeDefault;
int gFakeFailInits = 0;
bool gFakePlayFail = false;
static int gNextId = 1;
static int gNextHandle = 100;
static std::map<size_t, int> gChannels;

static std::string F(float v)
{
	unsigned b;
	memcpy(&b, &v, 4);
	char s[16];
	snprintf(s, sizeof(s), "%08x", b);
	return s;
}
static std::string D(double v)
{
	unsigned long long b;
	memcpy(&b, &v, 8);
	char s[24];
	snprintf(s, sizeof(s), "%016llx", b);
	return s;
}
static std::string P(const void* p)
{
	if (!p) return "null";
	return "#" + std::to_string((size_t)p);
}
static std::string W(const std::wstring& w)
{
	std::string s;
	for (wchar_t c : w) s += (char)c;
	return s;
}
static std::string V3(const float* v)
{
	return F(v[0]) + "," + F(v[1]) + "," + F(v[2]);
}
static void T(const std::string& s) { gTrace.push_back(s); }
static std::string B(bool b) { return b ? "1" : "0"; }

cOAL_Sample* OAL_Sample_Load(const std::wstring& f, eOAL_SampleFormat)
{
	T("Sample_Load " + W(f));
	if (W(f) == "missing.wav") return NULL;
	size_t id = gNextId++;
	gChannels[id] = W(f).find("stereo") != std::string::npos ? 2 : 1;
	return (cOAL_Sample*)id;
}
cOAL_Stream* OAL_Stream_Load(const std::wstring& f, eOAL_SampleFormat)
{
	T("Stream_Load " + W(f));
	if (W(f) == "missing.ogg") return NULL;
	size_t id = gNextId++;
	gChannels[id] = W(f).find("stereo") != std::string::npos ? 2 : 1;
	return (cOAL_Stream*)id;
}
void OAL_Sample_Unload(cOAL_Sample* s) { T("Sample_Unload " + P(s)); }
void OAL_Stream_Unload(cOAL_Stream* s) { T("Stream_Unload " + P(s)); }
void OAL_Sample_SetLoop(cOAL_Sample* s, bool l) { T("Sample_SetLoop " + P(s) + " " + B(l)); }
void OAL_Stream_SetLoop(cOAL_Stream* s, bool l) { T("Stream_SetLoop " + P(s) + " " + B(l)); }
int OAL_Sample_GetChannels(cOAL_Sample* s) { T("Sample_GetChannels " + P(s)); return gChannels[(size_t)s]; }
int OAL_Stream_GetChannels(cOAL_Stream* s) { T("Stream_GetChannels " + P(s)); return gChannels[(size_t)s]; }
int OAL_Sample_Play(int src, cOAL_Sample* s, float v, bool p, int prio)
{
	T("Sample_Play " + std::to_string(src) + " " + P(s) + " " + F(v) + " " + B(p) + " " + std::to_string(prio));
	return gFakePlayFail ? -1 : gNextHandle++;
}
int OAL_Stream_Play(int src, cOAL_Stream* s, float v, bool p)
{
	T("Stream_Play " + std::to_string(src) + " " + P(s) + " " + F(v) + " " + B(p));
	return gFakePlayFail ? -1 : gNextHandle++;
}

void OAL_Source_Stop(int s) { T("Source_Stop " + std::to_string(s)); }
void OAL_Source_SetPaused(int s, const bool p) { T("Source_SetPaused " + std::to_string(s) + " " + B(p)); }
void OAL_Source_SetGain(int s, float g) { T("Source_SetGain " + std::to_string(s) + " " + F(g)); }
void OAL_Source_SetPitch(int s, float g) { T("Source_SetPitch " + std::to_string(s) + " " + F(g)); }
void OAL_Source_SetLoop(int s, const bool l) { T("Source_SetLoop " + std::to_string(s) + " " + B(l)); }
void OAL_Source_SetAttributes(const int s, const float* p, const float* v) { T("Source_SetAttributes " + std::to_string(s) + " " + V3(p) + " " + V3(v)); }
void OAL_Source_SetSpatialize(const int s, const bool v) { T("Source_SetSpatialize " + std::to_string(s) + " " + B(v)); }
void OAL_Source_SetPriority(const int s, const unsigned int p) { T("Source_SetPriority " + std::to_string(s) + " " + std::to_string(p)); }
unsigned int OAL_Source_GetPriority(const int s) { T("Source_GetPriority " + std::to_string(s)); return 3; }
const bool OAL_Source_IsPlaying(const int s) { T("Source_IsPlaying " + std::to_string(s)); return true; }
const bool OAL_Source_IsBufferUnderrun(const int s) { T("Source_IsBufferUnderrun " + std::to_string(s)); return false; }
double OAL_Source_GetElapsedTime(const int s) { T("Source_GetElapsedTime " + std::to_string(s)); return 0.5; }
double OAL_Source_GetTotalTime(const int s) { T("Source_GetTotalTime " + std::to_string(s)); return 2.0; }
void OAL_Source_SetElapsedTime(const int s, double t) { T("Source_SetElapsedTime " + std::to_string(s) + " " + D(t)); }
void OAL_Source_SetFilterType(int s, eOALFilterType t) { T("Source_SetFilterType " + std::to_string(s) + " " + std::to_string((int)t)); }
void OAL_Source_SetAuxSendSlot(int s, int a, int slot) { T("Source_SetAuxSendSlot " + std::to_string(s) + " " + std::to_string(a) + " " + std::to_string(slot)); }
void OAL_Source_SetFiltering(int s, bool on, int f) { T("Source_SetFiltering " + std::to_string(s) + " " + B(on) + " " + std::to_string(f)); }
void OAL_Source_SetFilterGain(int s, float g) { T("Source_SetFilterGain " + std::to_string(s) + " " + F(g)); }
void OAL_Source_SetFilterGainHF(int s, float g) { T("Source_SetFilterGainHF " + std::to_string(s) + " " + F(g)); }
void OAL_Source_SetFilterGainLF(int s, float g) { T("Source_SetFilterGainLF " + std::to_string(s) + " " + F(g)); }
void OAL_Source_SetAuxSendGain(int s, float g) { T("Source_SetAuxSendGain " + std::to_string(s) + " " + F(g)); }

void OAL_Listener_SetAttributes(const float* p, const float* v, const float* f, const float* u)
{
	T("Listener_SetAttributes " + V3(p) + " " + V3(v) + " " + V3(f) + " " + V3(u));
}
void OAL_Listener_SetMasterVolume(const float v) { T("Listener_SetMasterVolume " + F(v)); }

bool OAL_Init(cOAL_Init_Params& p)
{
	bool ok = gFakeFailInits <= 0;
	if (!ok) --gFakeFailInits;
	T("Init '" + p.msDeviceName + "' " + std::to_string(p.mlOutputFreq) + " " + std::to_string(p.mlUpdateFreq) + " " +
	  std::to_string(p.mlNumSourcesHint) + " " + std::to_string(p.mlMinMonoSourcesHint) + " " + std::to_string(p.mlMinStereoSourcesHint) + " " +
	  std::to_string(p.mlStreamingBufferSize) + " " + std::to_string(p.mlStreamingBufferCount) + " " + B(p.mbUseThread) + B(p.mbVoiceManagement) + B(p.mbUseEFX) +
	  " -> " + B(ok));
	return ok;
}
void OAL_Close() { T("Close"); }
void OAL_Update() { T("Update"); }
void OAL_SetRollOffFactor(const float f) { T("SetRollOffFactor " + F(f)); }
void OAL_SetDistanceModel(eOAL_DistanceModel m) { T("SetDistanceModel " + std::to_string((int)m)); }
void OAL_SetupLogging(bool on, eOAL_LogOutput o, eOAL_LogVerbose v, std::string n)
{
	T("SetupLogging " + B(on) + " " + std::to_string((int)o) + " " + std::to_string((int)v) + " " + n);
}
const char* OAL_Info_GetDeviceName() { T("Info_GetDeviceName"); return gFakeDevices.size() > 1 ? gFakeDevices[1].c_str() : ""; }
const int OAL_Info_GetNumSources() { T("Info_GetNumSources"); return 32; }
const bool OAL_Info_IsEFXActive() { T("Info_IsEFXActive"); return false; }
int OAL_Info_GetStreamBufferCount() { T("Info_GetStreamBufferCount"); return 4; }
int OAL_Info_GetStreamBufferSize() { T("Info_GetStreamBufferSize"); return 8192; }
std::string OAL_Info_GetDefaultOutputDevice() { T("Info_GetDefaultOutputDevice"); return gFakeDefault; }
std::vector<std::string> OAL_Info_GetOutputDevices() { T("Info_GetOutputDevices"); return gFakeDevices; }

cOAL_Effect_Reverb* OAL_Effect_Reverb_Create() { T("Reverb_Create"); return (cOAL_Effect_Reverb*)(size_t)gNextId++; }
bool OAL_EffectSlot_AttachEffect(int slot, cOAL_Effect* e) { T("Slot_Attach " + std::to_string(slot) + " " + P(e)); return true; }
void OAL_EffectSlot_SetGain(int slot, float g) { T("Slot_SetGain " + std::to_string(slot) + " " + F(g)); }
void OAL_EffectSlot_SetAutoAdjust(int slot, bool v) { T("Slot_SetAutoAdjust " + std::to_string(slot) + " " + B(v)); }

#define REVERB(name) \
	void OAL_Effect_Reverb_Set##name(cOAL_Effect_Reverb* e, float v) { T("Reverb_Set" #name " " + P(e) + " " + F(v)); }
REVERB(Density)
REVERB(Diffusion)
REVERB(Gain)
REVERB(GainHF)
REVERB(GainLF)
REVERB(DecayTime)
REVERB(DecayHFRatio)
REVERB(DecayLFRatio)
REVERB(ReflectionsGain)
REVERB(ReflectionsDelay)
REVERB(LateReverbGain)
REVERB(LateReverbDelay)
REVERB(EchoTime)
REVERB(EchoDepth)
REVERB(ModulationTime)
REVERB(ModulationDepth)
REVERB(AirAbsorptionGainHF)
REVERB(HFReference)
REVERB(LFReference)
REVERB(RoomRolloffFactor)
void OAL_Effect_Reverb_SetDecayHFLimit(cOAL_Effect_Reverb* e, ALboolean v) { T("Reverb_SetDecayHFLimit " + P(e) + " " + std::to_string((int)v)); }
