/*
 * Regression tests for the XDG Base Directory / xdg-user-dirs support added
 * to cPlatform::GetSystemSpecialPath() (HPL2/core/sources/impl/PlatformUnix.cpp)
 * in the commit that stopped the engine from writing saves/config/cache/logs
 * directly into ~/.frictionalgames or the game's own install directory. That
 * logic has several easy-to-get-wrong edge cases (env var must be an
 * *absolute* path per spec, trailing-slash normalization, a hand-rolled
 * user-dirs.dirs parser) and had no test coverage - this exercises it
 * directly via getenv/temp files rather than a full engine boot.
 *
 * Plain, dependency-free checks (no GL/SDL/game-data needed), same pattern
 * as PhysicsNewtonTests.cpp. Linux-only, matching the feature itself.
 */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "system/Platform.h"
#include "system/String.h"

using namespace hpl;

static int gFailures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
			++gFailures; \
		} \
	} while (0)

#define CHECK_EQ_STR(actual, expected) \
	do { \
		tString sActual = (actual); \
		tString sExpected = (expected); \
		if (sActual != sExpected) { \
			std::fprintf(stderr, "FAILED: %s == %s, got \"%s\", expected \"%s\" (%s:%d)\n", \
				#actual, #expected, sActual.c_str(), sExpected.c_str(), __FILE__, __LINE__); \
			++gFailures; \
		} \
	} while (0)

//-----------------------------------------------------------------------

static tString MakeTempDir()
{
	char buf[] = "/tmp/hpl2_xdgtest_XXXXXX";
	char* pResult = mkdtemp(buf);
	CHECK(pResult != NULL);
	return pResult ? tString(pResult) : tString("/tmp/hpl2_xdgtest_fallback");
}

static tString GetSpecialPath8(eSystemPath aType)
{
	return cString::To8Char(cPlatform::GetSystemSpecialPath(aType));
}

//-----------------------------------------------------------------------

static void TestAbsoluteEnvVarIsUsedVerbatim()
{
	tString sHome = MakeTempDir();
	setenv("HOME", sHome.c_str(), 1);

	tString sData = MakeTempDir(); // deliberately distinct from sHome
	setenv("XDG_DATA_HOME", sData.c_str(), 1);

	// No trailing slash on the env var value -> the engine should add one.
	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGDataHome), sData + "/");

	unsetenv("XDG_DATA_HOME");
}

//-----------------------------------------------------------------------

static void TestUnsetEnvVarFallsBackUnderHome()
{
	tString sHome = MakeTempDir();
	setenv("HOME", sHome.c_str(), 1);
	unsetenv("XDG_CONFIG_HOME");

	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGConfigHome), sHome + "/.config/");
}

//-----------------------------------------------------------------------

static void TestRelativeEnvVarIsTreatedAsUnset()
{
	// XDG spec: a relative value for one of these vars "should be considered
	// as if it was not set" - not resolved relative to cwd.
	tString sHome = MakeTempDir();
	setenv("HOME", sHome.c_str(), 1);
	setenv("XDG_CACHE_HOME", "relative/not/absolute", 1);

	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGCacheHome), sHome + "/.cache/");

	unsetenv("XDG_CACHE_HOME");
}

//-----------------------------------------------------------------------

static void TestAllFourBaseDirDefaults()
{
	tString sHome = MakeTempDir();
	setenv("HOME", sHome.c_str(), 1);
	unsetenv("XDG_DATA_HOME");
	unsetenv("XDG_CONFIG_HOME");
	unsetenv("XDG_CACHE_HOME");
	unsetenv("XDG_STATE_HOME");

	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGDataHome), sHome + "/.local/share/");
	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGConfigHome), sHome + "/.config/");
	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGCacheHome), sHome + "/.cache/");
	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGStateHome), sHome + "/.local/state/");
}

//-----------------------------------------------------------------------

static void TestUserDirsFileIsParsed()
{
	tString sHome = MakeTempDir();
	setenv("HOME", sHome.c_str(), 1);
	unsetenv("XDG_CONFIG_HOME"); // force default $HOME/.config/ lookup location

	tString sConfigDir = sHome + "/.config";
	CHECK(mkdir(sConfigDir.c_str(), 0700) == 0);

	std::ofstream file((sConfigDir + "/user-dirs.dirs").c_str());
	CHECK(file.good());
	file << "# comment line, should be ignored\n";
	file << "XDG_DESKTOP_DIR=\"$HOME/Desktop\"\n";
	file << "XDG_PICTURES_DIR=\"$HOME/Photos\"\n";
	file.close();

	// Known quirk (see TASKS.md): GetXDGUserDir() builds its own $HOME with a
	// trailing slash already appended, then substitutes it verbatim into the
	// "$HOME/Photos"-shaped value from the file, producing a doubled slash.
	// Harmless in practice (POSIX collapses repeated slashes), but locked in
	// here so a future accidental behavior change doesn't go unnoticed.
	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGPictures), sHome + "//Photos/");
}

//-----------------------------------------------------------------------

static void TestUserDirsFallbackWhenFileMissing()
{
	tString sHome = MakeTempDir();
	setenv("HOME", sHome.c_str(), 1);
	unsetenv("XDG_CONFIG_HOME");

	// No user-dirs.dirs written in this fresh temp $HOME at all.
	CHECK_EQ_STR(GetSpecialPath8(eSystemPath_XDGPictures), sHome + "/Pictures/");
}

//-----------------------------------------------------------------------

int hplMain(const tString&)
{
	// Snapshot the real environment so a failure partway through doesn't
	// leave a mutated HOME/XDG_* behind for anything running after us.
	const char* pRealHome = getenv("HOME");
	tString sRealHome = pRealHome ? pRealHome : "";

	TestAbsoluteEnvVarIsUsedVerbatim();
	TestUnsetEnvVarFallsBackUnderHome();
	TestRelativeEnvVarIsTreatedAsUnset();
	TestAllFourBaseDirDefaults();
	TestUserDirsFileIsParsed();
	TestUserDirsFallbackWhenFileMissing();

	if (!sRealHome.empty()) setenv("HOME", sRealHome.c_str(), 1);

	if (gFailures > 0)
	{
		std::fprintf(stderr, "\n%d check(s) FAILED\n", gFailures);
		return 1;
	}

	std::printf("All XDG path-resolution checks passed.\n");
	return 0;
}
