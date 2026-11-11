#include "SomaConfig.h"
#include "RustSomaSave.h"

#include <fstream>
#include <iterator>

static int SomaGameIdIndex()
{
	static int lIdx = -1;
	if (lIdx < 0)
	{
		const char *pDir = getenv("OPENHPL_SOMA_GAME_DIR");
		std::ifstream f((pDir ? std::string(pDir) + "/" : std::string()) + "config/main_init.cfg");
		std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		lIdx = ohpl_gs4_game_id((const unsigned char *)s.data(), s.size());
	}
	return lIdx;
}

const std::string &SomaGameId()
{
	static const std::string vIds[] = {"soma", "rebirth", "bunker"};
	return vIds[SomaGameIdIndex()];
}

tWString SomaGameIdW()
{
	return cString::To16Char(SomaGameId()) + _W("/");
}

int SomaGameIdValue()
{
	return SomaGameIdIndex();
}

tWString cSomaConfig::GetConfigFilePath()
{
	struct cPaths
	{
		tWString msFile;
	} paths;
	tWString sRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGConfigHome);
	const std::string &sId = SomaGameId();
	ohpl_gs4_paths(1, (const uint32_t *)sRoot.data(), sRoot.size(), (const unsigned char *)sId.data(), sId.size(), false, 0,
				   [](void *c, const uint32_t *apPath, size_t alPath, bool abFile) {
					   tWString sPath((const wchar_t *)apPath, alPath);
					   if (abFile)
						   ((cPaths *)c)->msFile = sPath;
					   else if (cPlatform::FolderExists(sPath) == false)
						   cPlatform::CreateFolder(sPath);
				   },
				   &paths);
	return paths.msFile;
}

static std::string S(const char *p, size_t n)
{
	return std::string(p, n);
}

static OhplGs4CfgHost MakeCfgHost(cConfigFile *apCfg)
{
	return OhplGs4CfgHost{
		apCfg,
		[](void *c, const char *l, size_t ln, const char *k, size_t kn, float d) { return ((cConfigFile *)c)->GetFloat(S(l, ln), S(k, kn), d); },
		[](void *c, const char *l, size_t ln, const char *k, size_t kn, bool d) { return ((cConfigFile *)c)->GetBool(S(l, ln), S(k, kn), d); },
		[](void *c, const char *l, size_t ln, const char *k, size_t kn, int32_t d) { return ((cConfigFile *)c)->GetInt(S(l, ln), S(k, kn), d); },
		[](void *c, const char *l, size_t ln, const char *k, size_t kn, float v) { ((cConfigFile *)c)->SetFloat(S(l, ln), S(k, kn), v); },
		[](void *c, const char *l, size_t ln, const char *k, size_t kn, bool v) { ((cConfigFile *)c)->SetBool(S(l, ln), S(k, kn), v); },
		[](void *c, const char *l, size_t ln, const char *k, size_t kn, int32_t v) { ((cConfigFile *)c)->SetInt(S(l, ln), S(k, kn), v); }};
}

void cSomaConfig::Load()
{
	tWString sFile = GetConfigFilePath();

	cConfigFile *pCfg = hplNew(cConfigFile, (sFile));
	if (pCfg->Load() == false)
	{
		Log("SOMA: no settings file yet at '%s', using defaults\n", cString::To8Char(sFile).c_str());
		hplDelete(pCfg);
		return;
	}

	OhplGs4Config c = {mfMasterVolume, mfGamma, mlScreenWidth, mlScreenHeight, mbVSync, mbFullscreen, mbAntiAliasing, mbShowSubtitles};
	OhplGs4CfgHost host = MakeCfgHost(pCfg);
	ohpl_gs4_cfg_load(&host, &c);
	mfMasterVolume = c.master_volume;
	mfGamma = c.gamma;
	mbVSync = c.vsync;
	mbFullscreen = c.fullscreen;
	mlScreenWidth = c.screen_width;
	mlScreenHeight = c.screen_height;
	mbAntiAliasing = c.anti_aliasing;
	mbShowSubtitles = c.show_subtitles;

	hplDelete(pCfg);
}

void cSomaConfig::Save()
{
	tWString sFile = GetConfigFilePath();

	cConfigFile *pCfg = hplNew(cConfigFile, (sFile));

	OhplGs4Config c = {mfMasterVolume, mfGamma, mlScreenWidth, mlScreenHeight, mbVSync, mbFullscreen, mbAntiAliasing, mbShowSubtitles};
	OhplGs4CfgHost host = MakeCfgHost(pCfg);
	ohpl_gs4_cfg_save(&host, &c);

	if (pCfg->Save() == false)
		Log("SOMA: failed to save settings to '%s'\n", cString::To8Char(sFile).c_str());

	hplDelete(pCfg);
}
