#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "shims.h"
#include "OALWrapper/OAL_Funcs.h"
#include "OALWrapper/OAL_Effect_Reverb.h"


static_assert(sizeof(wchar_t) == sizeof(uint32_t), "wide text crosses the ABI as u32");

extern "C" {

void* ohpl_oal_sample_load(const uint32_t* path, size_t len) { return OAL_Sample_Load(std::wstring((const wchar_t*)path, len)); }
void* ohpl_oal_stream_load(const uint32_t* path, size_t len) { return OAL_Stream_Load(std::wstring((const wchar_t*)path, len)); }
void ohpl_oal_sample_unload(void* s) { OAL_Sample_Unload((cOAL_Sample*)s); }
void ohpl_oal_stream_unload(void* s) { OAL_Stream_Unload((cOAL_Stream*)s); }
void ohpl_oal_sample_set_loop(void* s, bool l) { OAL_Sample_SetLoop((cOAL_Sample*)s, l); }
void ohpl_oal_stream_set_loop(void* s, bool l) { OAL_Stream_SetLoop((cOAL_Stream*)s, l); }
int ohpl_oal_sample_channels(void* s) { return OAL_Sample_GetChannels((cOAL_Sample*)s); }
int ohpl_oal_stream_channels(void* s) { return OAL_Stream_GetChannels((cOAL_Stream*)s); }
int ohpl_oal_sample_play(int src, void* s, float vol, bool paused, int prio) { return OAL_Sample_Play(src, (cOAL_Sample*)s, vol, paused, prio); }
int ohpl_oal_stream_play(int src, void* s, float vol, bool paused) { return OAL_Stream_Play(src, (cOAL_Stream*)s, vol, paused); }

void ohpl_oal_source_stop(int s) { OAL_Source_Stop(s); }
void ohpl_oal_source_set_paused(int s, bool p) { OAL_Source_SetPaused(s, p); }
void ohpl_oal_source_set_gain(int s, float g) { OAL_Source_SetGain(s, g); }
void ohpl_oal_source_set_pitch(int s, float p) { OAL_Source_SetPitch(s, p); }
void ohpl_oal_source_set_loop(int s, bool l) { OAL_Source_SetLoop(s, l); }
void ohpl_oal_source_set_attributes(int s, const float* pos, const float* vel) { OAL_Source_SetAttributes(s, pos, vel); }
void ohpl_oal_source_set_spatialize(int s, bool v) { OAL_Source_SetSpatialize(s, v); }
void ohpl_oal_source_set_priority(int s, unsigned p) { OAL_Source_SetPriority(s, p); }
unsigned ohpl_oal_source_get_priority(int s) { return OAL_Source_GetPriority(s); }
bool ohpl_oal_source_is_playing(int s) { return OAL_Source_IsPlaying(s); }
bool ohpl_oal_source_is_buffer_underrun(int s) { return OAL_Source_IsBufferUnderrun(s); }
double ohpl_oal_source_get_elapsed(int s) { return OAL_Source_GetElapsedTime(s); }
double ohpl_oal_source_get_total(int s) { return OAL_Source_GetTotalTime(s); }
void ohpl_oal_source_set_elapsed(int s, double t) { OAL_Source_SetElapsedTime(s, t); }
void ohpl_oal_source_set_filter_type_bandpass(int s) { OAL_Source_SetFilterType(s, eOALFilterType_BandPass); }
void ohpl_oal_source_set_aux_send_slot(int s, int send, int slot) { OAL_Source_SetAuxSendSlot(s, send, slot); }
void ohpl_oal_source_set_filtering(int s, bool on, int flags) { OAL_Source_SetFiltering(s, on, flags); }
void ohpl_oal_source_set_filter_gain(int s, float g) { OAL_Source_SetFilterGain(s, g); }
void ohpl_oal_source_set_filter_gain_hf(int s, float g) { OAL_Source_SetFilterGainHF(s, g); }
void ohpl_oal_source_set_filter_gain_lf(int s, float g) { OAL_Source_SetFilterGainLF(s, g); }
void ohpl_oal_source_set_aux_send_gain(int s, float g) { OAL_Source_SetAuxSendGain(s, g); }

void ohpl_oal_listener_set_attributes(const float* pos, const float* vel, const float* fwd, const float* up) { OAL_Listener_SetAttributes(pos, vel, fwd, up); }
void ohpl_oal_listener_set_master_volume(float v) { OAL_Listener_SetMasterVolume(v); }

bool ohpl_oal_init(const OhplOalInit* p)
{
	cOAL_Init_Params params;
	params.mbVoiceManagement = p->voice_management;
	params.mlNumSourcesHint = p->num_sources_hint;
	params.mlUpdateFreq = p->update_freq;
	params.mlStreamingBufferCount = p->stream_buffer_count;
	params.mlStreamingBufferSize = p->stream_buffer_size;
	params.mlMinMonoSourcesHint = p->min_mono_hint;
	params.mlMinStereoSourcesHint = p->min_stereo_hint;
	params.mbUseThread = p->use_thread;
	params.mlOutputFreq = p->output_freq;
	params.msDeviceName = p->device_name;
	params.mbUseEFX = p->use_efx;
	return OAL_Init(params);
}
void ohpl_oal_close() { OAL_Close(); }
void ohpl_oal_update() { OAL_Update(); }
void ohpl_oal_set_rolloff_factor(float f) { OAL_SetRollOffFactor(f); }
void ohpl_oal_set_distance_model_none() { OAL_SetDistanceModel(eOAL_DistanceModel_None); }
void ohpl_oal_setup_logging(bool on, const char* name) { OAL_SetupLogging(on, eOAL_LogOutput_File, eOAL_LogVerbose_High, name); }

const char* ohpl_oal_info_device_name() { return OAL_Info_GetDeviceName(); }
int ohpl_oal_info_num_sources() { return OAL_Info_GetNumSources(); }
bool ohpl_oal_info_efx_active() { return OAL_Info_IsEFXActive(); }
int ohpl_oal_info_stream_buffer_count() { return OAL_Info_GetStreamBufferCount(); }
int ohpl_oal_info_stream_buffer_size() { return OAL_Info_GetStreamBufferSize(); }
void ohpl_oal_info_default_device(void (*sink)(void*, const char*), void* ctx) { sink(ctx, OAL_Info_GetDefaultOutputDevice().c_str()); }
void ohpl_oal_info_output_devices(void (*sink)(void*, const char*), void* ctx)
{
	for (const std::string& n : OAL_Info_GetOutputDevices()) sink(ctx, n.c_str());
}

void* ohpl_oal_reverb_create() { return OAL_Effect_Reverb_Create(); }
bool ohpl_oal_slot_attach(int slot, void* effect) { return OAL_EffectSlot_AttachEffect(slot, (cOAL_Effect*)effect); }
void ohpl_oal_slot_set_gain(int slot, float g) { OAL_EffectSlot_SetGain(slot, g); }
void ohpl_oal_slot_set_auto_adjust(int slot, bool v) { OAL_EffectSlot_SetAutoAdjust(slot, v); }
void ohpl_oal_reverb_set_decay_hf_limit(void* e, int v) { OAL_Effect_Reverb_SetDecayHFLimit((cOAL_Effect_Reverb*)e, v); }

// ids follow OalReverbParam in rust/src/backend/media/oal.rs
void ohpl_oal_reverb_set(void* e, int id, float v)
{
	cOAL_Effect_Reverb* r = (cOAL_Effect_Reverb*)e;
	switch (id)
	{
	case 0: OAL_Effect_Reverb_SetDensity(r, v); break;
	case 1: OAL_Effect_Reverb_SetDiffusion(r, v); break;
	case 2: OAL_Effect_Reverb_SetEchoTime(r, v); break;
	case 3: OAL_Effect_Reverb_SetEchoDepth(r, v); break;
	case 4: OAL_Effect_Reverb_SetModulationTime(r, v); break;
	case 5: OAL_Effect_Reverb_SetModulationDepth(r, v); break;
	case 6: OAL_Effect_Reverb_SetGain(r, v); break;
	case 7: OAL_Effect_Reverb_SetGainHF(r, v); break;
	case 8: OAL_Effect_Reverb_SetGainLF(r, v); break;
	case 9: OAL_Effect_Reverb_SetReflectionsDelay(r, v); break;
	case 10: OAL_Effect_Reverb_SetReflectionsGain(r, v); break;
	case 11: OAL_Effect_Reverb_SetLateReverbGain(r, v); break;
	case 12: OAL_Effect_Reverb_SetLateReverbDelay(r, v); break;
	case 13: OAL_Effect_Reverb_SetHFReference(r, v); break;
	case 14: OAL_Effect_Reverb_SetLFReference(r, v); break;
	case 15: OAL_Effect_Reverb_SetAirAbsorptionGainHF(r, v); break;
	case 16: OAL_Effect_Reverb_SetDecayTime(r, v); break;
	case 17: OAL_Effect_Reverb_SetDecayHFRatio(r, v); break;
	case 18: OAL_Effect_Reverb_SetDecayLFRatio(r, v); break;
	case 19: OAL_Effect_Reverb_SetRoomRolloffFactor(r, v); break;
	}
}

}
