#ifndef OHPL_LAUNCHER_RUST_H
#define OHPL_LAUNCHER_RUST_H

// C ABI of rust/src/launcher/ffi.rs: card database and rating, preset tables, init/config/language
// plans, game exe and arguments, resolution, language and sound device rules.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hpl.h"

struct OhplLnStr
{
	const unsigned char *p;
	size_t n;
};

inline OhplLnStr OhplLnS(const std::string &asS)
{
	OhplLnStr s = {(const unsigned char*)asS.data(), asS.size()};
	return s;
}

typedef void (*OhplLnSink)(void*, int, const unsigned char*, size_t);

struct OhplLnCfgOps
{
	void *ctx;
	int (*get_bool)(void*, const unsigned char*, size_t, const unsigned char*, size_t, int);
	int (*get_int)(void*, const unsigned char*, size_t, const unsigned char*, size_t, int);
	float (*get_float)(void*, const unsigned char*, size_t, const unsigned char*, size_t, float);
	void (*get_string)(void*, const unsigned char*, size_t, const unsigned char*, size_t, const unsigned char*, size_t, OhplLnSink, void*);
	void (*set_bool)(void*, const unsigned char*, size_t, const unsigned char*, size_t, int);
	void (*set_int)(void*, const unsigned char*, size_t, const unsigned char*, size_t, int);
	void (*set_float)(void*, const unsigned char*, size_t, const unsigned char*, size_t, float);
	void (*set_string)(void*, const unsigned char*, size_t, const unsigned char*, size_t, const unsigned char*, size_t);
};

struct OhplLnDetect
{
	int show_closest, show_unknown, show_unsupported, preset;
};

struct OhplLnInitial
{
	int fullscreen, shadow_quality, shadow_resolution, reflection, ssao, ssao_resolution, ssao_samples,
		texture_quality, texture_filter, anisotropy, edge_smooth, parallax;
};

struct OhplLnMode
{
	int display, width, height;
};

struct OhplLnDevice
{
	int is_default, is_current;
};

struct OhplLnChooser;

extern "C"
{
OhplLnChooser *ohpl_launcher_qc_load(const unsigned char *apPath, size_t alLen);
void ohpl_launcher_qc_free(OhplLnChooser *apChooser);
int ohpl_launcher_qc_rate(const OhplLnChooser *apChooser, const unsigned char *apCard, size_t alLen, OhplLnSink apSink, void *apCtx);

int ohpl_launcher_equals_preset(const OhplLnCfgOps *apFile, const OhplLnCfgOps *apPreset);
int ohpl_launcher_equal_preset_index(const OhplLnCfgOps *apFile, const OhplLnCfgOps *const *apPresets, size_t alNum);
void ohpl_launcher_apply_preset(const OhplLnCfgOps *apFile, const OhplLnCfgOps *apPreset);
void ohpl_launcher_detect(int alRating, int abClosestNonEmpty, OhplLnDetect *apOut);
void ohpl_launcher_initial(const OhplLnCfgOps *apCfg, OhplLnInitial *apOut);
int ohpl_launcher_menu_index(const int *apValues, size_t alNum, int alWanted);
int ohpl_launcher_texture_quality_row(int alMenuSize, int alQuality);

// sink tags: 0 = result
void ohpl_launcher_init_cfg(const unsigned char *apParams, size_t alLen, OhplLnSink apSink, void *apCtx); // 0: init file, 1: default init file
void ohpl_launcher_user_resource_dir(const unsigned char *apPersonal, size_t alLen, OhplLnSink apSink, void *apCtx);
void ohpl_launcher_init_cfg_path(const unsigned char *apUserDir, size_t alUserLen, const unsigned char *apInit, size_t alInitLen, int abUserExists, OhplLnSink apSink, void *apCtx);
void ohpl_launcher_message_init_load(const unsigned char *apInit, size_t alLen, OhplLnSink apSink, void *apCtx);
// tags 1..10: save folder, default main, low, medium, high, default base language, base language
// folder, game language folder, custom story path, game name; 100: message. Returns 1 to quit.
int ohpl_launcher_read_init(const OhplLnCfgOps *apInit, const unsigned char *apInitFile, size_t alInitLen,
							const unsigned char *apDefaultFile, size_t alDefaultLen,
							const OhplLnCfgOps *(*apLoadDefaults)(void*), void *apLoadCtx, OhplLnSink apSink, void *apCtx);
// executed as emitted (full paths): 10 create if missing, 11 mkdir, 12+13 move if missing (from, to);
// 20 base save, 21 base config, 22 crash flag, 23 log file
void ohpl_launcher_plan_paths(OhplLnStr aPersonal, OhplLnStr aConfigHome, OhplLnStr aStateHome, OhplLnStr aSaveFolder, OhplLnStr aCustom, OhplLnSink apSink, void *apCtx);
void ohpl_launcher_settings_path(const unsigned char *apBaseConfig, size_t alLen, OhplLnSink apSink, void *apCtx);
int ohpl_launcher_use_wanted_config(int abForceDefault, int abWantedExists);
int ohpl_launcher_config_load_message(int abLoadedWanted, int abDefaultEmpty, OhplLnSink apSink, void *apCtx);
int ohpl_launcher_sound_device(const OhplLnCfgOps *apCfg);
int ohpl_launcher_show_launcher(const OhplLnCfgOps *apCfg);
void ohpl_launcher_start_language(const OhplLnCfgOps *apCfg, OhplLnSink apSink, void *apCtx);
// 1: required file, 2: fallback file (user dir first)
void ohpl_launcher_language_plan(OhplLnStr aName, OhplLnStr aBaseFolder, OhplLnStr aGameFolder, OhplLnStr aDefaultBase, OhplLnStr aDefaultGame, OhplLnSink apSink, void *apCtx);
void ohpl_launcher_window_class(OhplLnSink apSink, void *apCtx);
void ohpl_launcher_game_exe(const unsigned char *apDir, size_t alLen, OhplLnSink apSink, void *apCtx);
int ohpl_launcher_run_plan(int alExitFlag, const unsigned char *apParams, size_t alLen, OhplLnSink apSink, void *apCtx);

int ohpl_launcher_fix_config_size(const OhplLnCfgOps *apCfg, const OhplLnMode *apModes, size_t alNum);
void ohpl_launcher_configured_mode(const OhplLnCfgOps *apCfg, OhplLnMode *apOut);
int ohpl_launcher_last_match(const OhplLnMode *apModes, size_t alNum, OhplLnMode aCur);
int ohpl_launcher_custom_resolution(const OhplLnMode *apModes, size_t alNum, int alW, int alH, int *apRow);
int ohpl_launcher_populate_selection(int alLastMatch, int alW, int alH);
int ohpl_launcher_multi_display(const OhplLnMode *apModes, size_t alNum);
void ohpl_launcher_aspect_ratio(int alW, int alH, int *apOut);
void ohpl_launcher_mode_label(int abFullScreenDesktop, OhplLnStr aFullScreenDesktopText, OhplLnStr aDisplayName, int alW, int alH,
							  int abCustom, OhplLnStr aCustomText, int abMultiDisplay, OhplLnSink apSink, void *apCtx);
void ohpl_launcher_language_name(const unsigned char *apFile, size_t alLen, OhplLnSink apSink, void *apCtx);
int ohpl_launcher_language_index(const OhplLnCfgOps *apCfg, const OhplLnStr *apFiles, size_t alNum, OhplLnStr aDefault);
void ohpl_launcher_languages_dir(OhplLnStr aFolder, OhplLnStr aUserDir, int abUserExists, OhplLnSink apSink, void *apCtx);
int ohpl_launcher_sound_selection(const OhplLnCfgOps *apCfg, const OhplLnDevice *apDevs, size_t alNum, int *apAppendUnsupported);
void ohpl_launcher_escape_device(const unsigned char *apName, size_t alLen, OhplLnSink apSink, void *apCtx);
}

inline void OhplLnStrSink(void *apCtx, int, const unsigned char *apData, size_t alLen)
{
	((std::string*)apCtx)->assign((const char*)apData, alLen);
}

struct OhplLnTagged
{
	std::vector<std::pair<int, std::string> > mvItems;
	static void Sink(void *apCtx, int alTag, const unsigned char *apData, size_t alLen)
	{
		((OhplLnTagged*)apCtx)->mvItems.push_back(std::make_pair(alTag, std::string((const char*)apData, alLen)));
	}
	std::string Get(int alTag) const
	{
		std::string s;
		for(size_t i = 0; i < mvItems.size(); ++i)
			if(mvItems[i].first == alTag) s = mvItems[i].second;
		return s;
	}
};

// cConfigFile seen through the Rust vtable
class OhplLnCfg
{
public:
	OhplLnCfg(hpl::cConfigFile *apFile)
	{
		mOps.ctx = apFile;
		mOps.get_bool = GetBool;
		mOps.get_int = GetInt;
		mOps.get_float = GetFloat;
		mOps.get_string = GetString;
		mOps.set_bool = SetBool;
		mOps.set_int = SetInt;
		mOps.set_float = SetFloat;
		mOps.set_string = SetString;
		mpFile = apFile;
	}
	// NULL file passes a NULL vtable (a missing preset)
	const OhplLnCfgOps *Ops() const { return mpFile ? &mOps : NULL; }

private:
	static hpl::tString S(const unsigned char *p, size_t n) { return hpl::tString((const char*)p, n); }
	static hpl::cConfigFile *F(void *c) { return (hpl::cConfigFile*)c; }
	static int GetBool(void *c, const unsigned char *l, size_t ln, const unsigned char *k, size_t kn, int d)
	{ return F(c)->GetBool(S(l, ln), S(k, kn), d != 0); }
	static int GetInt(void *c, const unsigned char *l, size_t ln, const unsigned char *k, size_t kn, int d)
	{ return F(c)->GetInt(S(l, ln), S(k, kn), d); }
	static float GetFloat(void *c, const unsigned char *l, size_t ln, const unsigned char *k, size_t kn, float d)
	{ return F(c)->GetFloat(S(l, ln), S(k, kn), d); }
	static void GetString(void *c, const unsigned char *l, size_t ln, const unsigned char *k, size_t kn,
						  const unsigned char *d, size_t dn, OhplLnSink sink, void *sc)
	{
		hpl::tString s = F(c)->GetString(S(l, ln), S(k, kn), S(d, dn));
		sink(sc, 0, (const unsigned char*)s.data(), s.size());
	}
	static void SetBool(void *c, const unsigned char *l, size_t ln, const unsigned char *k, size_t kn, int v)
	{ F(c)->SetBool(S(l, ln), S(k, kn), v != 0); }
	static void SetInt(void *c, const unsigned char *l, size_t ln, const unsigned char *k, size_t kn, int v)
	{ F(c)->SetInt(S(l, ln), S(k, kn), v); }
	static void SetFloat(void *c, const unsigned char *l, size_t ln, const unsigned char *k, size_t kn, float v)
	{ F(c)->SetFloat(S(l, ln), S(k, kn), v); }
	static void SetString(void *c, const unsigned char *l, size_t ln, const unsigned char *k, size_t kn,
						  const unsigned char *v, size_t vn)
	{ F(c)->SetString(S(l, ln), S(k, kn), S(v, vn)); }

	OhplLnCfgOps mOps;
	hpl::cConfigFile *mpFile;
};

class OhplLnQualityChooser
{
public:
	OhplLnQualityChooser(const hpl::tString &asDBFile);
	~OhplLnQualityChooser() { ohpl_launcher_qc_free(mpImpl); }
	// rating as eQRating; asMsg gets the closest series when the match was inexact
	int GetQualityRatingByCardString(const hpl::tString &asCardString, hpl::tString &asMsg);

private:
	OhplLnChooser *mpImpl;
};

#endif // OHPL_LAUNCHER_RUST_H
