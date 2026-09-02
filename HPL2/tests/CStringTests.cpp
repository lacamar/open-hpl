/*
 * Regression tests for hpl::cString (HPL2/core/sources/system/String.cpp).
 *
 * This is one of the most heavily used utility classes in the engine - every
 * resource loader, config file, and save-game field goes through its file
 * path helpers and string<->value conversions - yet it had zero test
 * coverage before this file. These are plain, dependency-free checks (no
 * GL/SDL/game-data needed), same pattern as PhysicsNewtonTests.cpp.
 */

#include <cstdio>
#include <cstdlib>

#include "math/MathTypes.h"
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

static void TestFileExtensions()
{
	CHECK_EQ_STR(cString::GetFileExt("archive.tar.gz"), "gz");
	CHECK_EQ_STR(cString::GetFileExt("noext"), "");
	CHECK_EQ_STR(cString::GetFileExt("/path/to/file.DAE"), "DAE");

	CHECK_EQ_STR(cString::SetFileExt("test.exe", "txt"), "test.txt");
	CHECK_EQ_STR(cString::SetFileExt("test.exe", ".txt"), "test.txt"); // leading dot accepted too
	CHECK_EQ_STR(cString::SetFileExt("test", "txt"), "test.txt");
	CHECK_EQ_STR(cString::SetFileExt("test.exe", ""), "test"); // empty ext strips it
}

//-----------------------------------------------------------------------

static void TestFileNameAndPath()
{
	CHECK_EQ_STR(cString::GetFileName("/a/b/c.txt"), "c.txt");
	CHECK_EQ_STR(cString::GetFileName("c.txt"), "c.txt");
	CHECK_EQ_STR(cString::GetFileName("C:\\a\\b\\c.txt"), "c.txt"); // backslash also handled

	CHECK_EQ_STR(cString::GetFilePath("/a/b/c.txt"), "/a/b/");
	CHECK_EQ_STR(cString::GetFilePath("c.txt"), ""); // no slash but has a dot -> empty path
	// Known quirk: GetFilePath only looks for a slash *after* first confirming
	// the string contains a '.' anywhere - a path with no extension at all
	// (even one with slashes) falls through to returning the whole input
	// unchanged rather than the directory part. Locking in current behavior
	// so a future accidental "fix" here doesn't silently change callers.
	CHECK_EQ_STR(cString::GetFilePath("a/b/noext"), "a/b/noext");
}

//-----------------------------------------------------------------------

static void TestSlashHelpers()
{
	CHECK_EQ_STR(cString::AddSlashAtEnd("foo"), "foo/");
	CHECK_EQ_STR(cString::AddSlashAtEnd("foo/"), "foo/");
	CHECK_EQ_STR(cString::AddSlashAtEnd(""), "");

	CHECK_EQ_STR(cString::RemoveSlashAtEnd("foo/"), "foo");
	CHECK_EQ_STR(cString::RemoveSlashAtEnd("foo"), "foo");
	CHECK_EQ_STR(cString::RemoveSlashAtEnd(""), "");
}

//-----------------------------------------------------------------------

static void TestRelativePath()
{
	// Diverges at "c" vs "x": back up two (to shared "a/b/"), then descend
	// into the target's remaining "c/file.txt/".
	CHECK_EQ_STR(cString::GetRelativePath("/a/b/c/file.txt", "/a/b/x/y"), "../../c/file.txt/");

	// No common prefix at all ("a" vs "z" differ immediately) -> just the
	// original path back, per the "no different partitions" early-out.
	CHECK_EQ_STR(cString::GetRelativePath("a/b", "z/y"), "a/b");
}

//-----------------------------------------------------------------------

static void TestNumericConversions()
{
	CHECK(cString::ToInt(NULL, 42) == 42);
	CHECK(cString::ToInt("7", 0) == 7);
	CHECK(cString::ToInt("-3", 0) == -3);

	CHECK(cString::ToFloat(NULL, 1.5f) == 1.5f);
	CHECK(cString::ToFloat("2.5", 0.0f) == 2.5f);

	CHECK(cString::ToBool(NULL, true) == true);
	CHECK(cString::ToBool("true", false) == true);
	CHECK(cString::ToBool("True", false) == true); // case-insensitive
	CHECK(cString::ToBool("false", true) == false);
	CHECK(cString::ToBool("garbage", true) == false); // anything but "true" -> false, not the default

	CHECK_EQ_STR(cString::ToString(5, 3), "005");
	CHECK_EQ_STR(cString::ToString(1.5f, 2, true), "1.5");
	CHECK_EQ_STR(cString::ToString(1.0f, 2, true), "1");
	CHECK_EQ_STR(cString::ToString(1.0f, 2, false), "1.00");
}

//-----------------------------------------------------------------------

static void TestVecParsing()
{
	tIntVec vInts;
	cString::GetIntVec("1, 2,3\n4", vInts, NULL);
	CHECK(vInts.size() == 4);
	if (vInts.size() == 4)
	{
		CHECK(vInts[0] == 1);
		CHECK(vInts[1] == 2);
		CHECK(vInts[2] == 3);
		CHECK(vInts[3] == 4);
	}

	const cVector3f vDefault(-1, -1, -1);
	cVector3f vResult = cString::ToVector3f("1.5 2.5 3.5", vDefault);
	CHECK(vResult == cVector3f(1.5f, 2.5f, 3.5f));

	// Wrong element count -> falls back to the caller's default rather than
	// guessing/truncating.
	cVector3f vBad = cString::ToVector3f("1.5 2.5", vDefault);
	CHECK(vBad == vDefault);
}

//-----------------------------------------------------------------------

int hplMain(const tString&)
{
	TestFileExtensions();
	TestFileNameAndPath();
	TestSlashHelpers();
	TestRelativePath();
	TestNumericConversions();
	TestVecParsing();

	if (gFailures > 0)
	{
		std::fprintf(stderr, "\n%d check(s) FAILED\n", gFailures);
		return 1;
	}

	std::printf("All cString checks passed.\n");
	return 0;
}
