// Generates the L1 golden vectors from the original C++ (verbatim copies of the code the Rust path replaces).
// Build and run: rust/tests/golden/L1/gen/gen.sh <outdir>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <list>
#include <set>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <wchar.h>
#include <wctype.h>

#include "SDL2/SDL_events.h"
#include "impl/RustInputSDL.h" // original: OhplConvertEvents without the Rust branch

static unsigned gSeed = 12345;
static unsigned Rnd() { gSeed = gSeed * 1664525u + 1013904223u; return gSeed >> 8; }

static void Hex(FILE *f, const void *p, size_t n)
{
	for(size_t i = 0; i < n; ++i) fprintf(f, "%02x", ((const unsigned char*)p)[i]);
}

static void GenEvents(const char *dir)
{
	std::string path = std::string(dir) + "/events.txt";
	FILE *f = fopen(path.c_str(), "w");
	static const unsigned types[] = {SDL_KEYDOWN, SDL_KEYUP, SDL_TEXTINPUT, SDL_MOUSEMOTION, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP,
		SDL_MOUSEWHEEL, SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP, SDL_CONTROLLERDEVICEADDED,
		SDL_CONTROLLERDEVICEREMOVED, SDL_QUIT, SDL_WINDOWEVENT, SDL_JOYAXISMOTION};
	for(int n = 0; n < 600; ++n)
	{
		SDL_Event e;
		for(size_t i = 0; i < sizeof(e); ++i) ((unsigned char*)&e)[i] = (unsigned char)Rnd();
		e.type = types[n % (sizeof(types) / sizeof(types[0]))];
		std::list<SDL_Event> l(1, e);
		std::vector<OhplInputEvent> out;
		hpl::OhplConvertEvents(l, out);
		fprintf(f, "E ");
		Hex(f, &e, sizeof(SDL_Event));
		fprintf(f, " ");
		Hex(f, &out[0], sizeof(OhplInputEvent));
		fprintf(f, "\n");
	}
	fclose(f);
}

static inline int patiMatch(const wchar_t *pattern, const wchar_t *string)
{
	switch (pattern[0])
	{
		case L'\0': return !string[0];
		case L'*': return patiMatch(pattern+1, string) || (string[0] && patiMatch(pattern, string+1));
		case L'?': return string[0] && patiMatch(pattern+1, string+1);
		default: return (towupper(pattern[0]) == towupper(string[0])) && patiMatch(pattern+1, string+1);
	}
}

static void GenPati(const char *dir)
{
	std::string path = std::string(dir) + "/pati.txt";
	FILE *f = fopen(path.c_str(), "w");
	const char *pats[] = {"*", "*.*", "*.txt", "?.txt", "a*", "A*b", "a?c", "", "*a*b*c*", "abc", "ABC", "*?", "??*", "x*y*z"};
	const char *strs[] = {"", "a", "abc", "ABC", "a.txt", "A.TXT", "ab.txt", "file.txt", "file.TxT", "axbxc", "aXb", "abcabc", "xyz", "x1y2z", ".hidden", "a.b.c.txt"};
	for(const char *p : pats)
		for(const char *s : strs)
		{
			std::wstring wp, ws;
			for(const char *c = p; *c; ++c) wp.push_back(*c);
			for(const char *c = s; *c; ++c) ws.push_back(*c);
			fprintf(f, "%s\t%s\t%d\n", p, s, patiMatch(wp.c_str(), ws.c_str()) ? 1 : 0);
		}
	fclose(f);
}

static void GenTimer(const char *dir)
{
	std::string path = std::string(dir) + "/timer.txt";
	FILE *f = fopen(path.c_str(), "w");
	for(int n = 0; n < 200; ++n)
	{
		timeval a, b;
		a.tv_sec = 1700000000 + (long)(Rnd() % 100000000); a.tv_usec = Rnd() % 1000000;
		b.tv_sec = a.tv_sec + (long)(Rnd() % 100); b.tv_usec = Rnd() % 1000000;
		double s = ((double)a.tv_sec * 1000000.0) + a.tv_usec;
		double e = ((double)b.tv_sec * 1000000.0) + b.tv_usec;
		double d = e - s;
		unsigned long long bits;
		memcpy(&bits, &d, 8);
		fprintf(f, "%ld %ld %ld %ld %016llx\n", (long)a.tv_sec, (long)a.tv_usec, (long)b.tv_sec, (long)b.tv_usec, bits);
	}
	fclose(f);
}

struct VM
{
	int display, w, h, bpp, refresh;
};
struct VideoComp
{
	bool operator()(const VM &a, const VM &b) const
	{
		if(a.display != b.display) return a.display < b.display;
		if(a.w != b.w) return a.w < b.w;
		if(a.h != b.h) return a.h < b.h;
		return false;
	}
};

static void GenModes(const char *dir)
{
	std::string path = std::string(dir) + "/modes.txt";
	FILE *f = fopen(path.c_str(), "w");
	for(int n = 0; n < 60; ++n)
	{
		int nd = 1 + Rnd() % 3;
		std::set<VM, VideoComp> uniq;
		fprintf(f, "CASE %d\n", nd);
		for(int d = 0; d < nd; ++d)
		{
			int desktopBpp = (Rnd() % 2) ? 32 : 24;
			int nm = Rnd() % 12;
			fprintf(f, "DISPLAY %d %d\n", desktopBpp, nm);
			for(int m = 0; m < nm; ++m)
			{
				int bpp = (Rnd() % 3) ? 32 : 24;
				int w = 320 * (1 + Rnd() % 6), h = 200 * (1 + Rnd() % 5);
				fprintf(f, "MODE %d %d %d\n", w, h, bpp);
				if(desktopBpp != bpp) continue;
				VM v = {d, w, h, bpp, 1};
				uniq.insert(v);
			}
			VM v = {d, 0, 0, desktopBpp, 1};
			uniq.insert(v);
		}
		for(const VM &v : uniq) fprintf(f, "OUT %d %d %d %d\n", v.display, v.w, v.h, v.bpp);
	}
	fclose(f);
}

static void Append(std::string &cmd, const char *a)
{
	if(cmd.length() > 0) cmd.append(" ").append(a); else cmd.append(a);
}

static void GenArgs(const char *dir)
{
	std::string path = std::string(dir) + "/args.txt";
	FILE *f = fopen(path.c_str(), "w");
	// an arg named dir* is a directory (chdir ok), baddir* is a directory whose chdir fails
	const char *cases[][6] = {
		{"dir1", 0}, {"-cwd", "dir1", 0}, {"dir1", "-cwd", 0}, {"nodir", "dir1", 0}, {"-psn_0_1", "dir1", "x", 0},
		{"dir1", "dir2", "-foo", 0}, {"baddir", "-cwd", 0}, {"baddir", "dir1", 0}, {"-a", "-b", "c", 0}, {0},
		{"a", "b", "dir1", "c", 0}, {"-cwd", "-cwd", 0}, {"dir1", "-cwd", "baddir", 0}, {"x", "-psn", "y", 0},
	};
	for(auto &c : cases)
	{
		bool cwd = false, bGotGameDirArg = false;
		std::string cmdline;
		std::string joined;
		for(int i = 0; c[i]; ++i) { if(i) joined += "|"; joined += c[i]; }
		for(int i = 0; c[i]; ++i)
		{
			const char *arg = c[i];
			enum { Cwd, Skip, GameDir, Command } kind;
			if(!strcmp(arg, "-cwd")) kind = Cwd;
			else if(!strncmp(arg, "-psn", 4)) kind = Skip;
			else if(!bGotGameDirArg && arg[0] != '-') kind = GameDir;
			else kind = Command;
			if(kind == Cwd) cwd = true;
			else if(kind == Skip) {}
			else if(kind == GameDir)
			{
				bool isDir = !strncmp(arg, "dir", 3) || !strncmp(arg, "baddir", 6);
				if(isDir)
				{
					bGotGameDirArg = true;
					cwd = !strncmp(arg, "dir", 3);
					continue;
				}
				Append(cmdline, arg);
			}
			else Append(cmdline, arg);
		}
		fprintf(f, "ARGS [%s]\nCMD [%s]\nCWD %d\n", joined.c_str(), cmdline.c_str(), cwd ? 1 : 0);
	}
	fclose(f);
}

static std::wstring To16(const std::string &s)
{
	std::wstring w;
	for(unsigned char c : s) w.push_back(c);
	return w;
}
static std::string To8(const std::wstring &w)
{
	std::string s;
	for(wchar_t c : w) s.push_back((char)c);
	return s;
}

static void FindFiles(std::list<std::wstring> &out, const std::wstring &asDir, const std::wstring &asMask)
{
	wchar_t sSpec[256];
	wchar_t end = asDir[asDir.size()-1];
	DIR *dirhandle;
	dirent *_entry;
	struct stat statbuff;
	std::wstring fileentry;
	if((dirhandle = opendir(To8(asDir).c_str())) == NULL) return;
	while((_entry = readdir(dirhandle)) != NULL)
	{
		if(end == L'/') swprintf(sSpec, 256, L"%ls%s", asDir.c_str(), _entry->d_name);
		else swprintf(sSpec, 256, L"%ls/%s", asDir.c_str(), _entry->d_name);
		if(stat(To8(sSpec).c_str(), &statbuff) == -1) continue;
		if(S_ISDIR(statbuff.st_mode)) continue;
		fileentry.assign(To16(_entry->d_name));
		if(!patiMatch(asMask.c_str(), fileentry.c_str())) continue;
		out.push_back(fileentry);
	}
	closedir(dirhandle);
	out.sort();
}

static void FindFolders(std::list<std::wstring> &out, const std::wstring &asDir, bool abAddHidden, bool abAddUpFolder)
{
	char sSpec[256];
	std::string sDir8 = To8(asDir);
	char end = sDir8[sDir8.size()-1];
	if(end != '/') sDir8 += "/";
	DIR *dirhandle;
	dirent *_entry;
	struct stat statbuff;
	std::wstring fileentry;
	if((dirhandle = opendir(To8(asDir).c_str())) == NULL) return;
	while((_entry = readdir(dirhandle)) != NULL)
	{
		snprintf(sSpec, 256, "%s%s", sDir8.c_str(), _entry->d_name);
		if(lstat(sSpec, &statbuff) == -1) continue;
		if(!S_ISDIR(statbuff.st_mode)) continue;
		if(!abAddUpFolder && _entry->d_name[0] == '.' && _entry->d_name[1] == '.' && _entry->d_name[2] == '\0') continue;
		if(!abAddHidden && _entry->d_name[0] == '.') continue;
		if(_entry->d_name[0] == '.' && _entry->d_name[1] == '\0') continue;
		fileentry.assign(To16(_entry->d_name));
		out.push_back(fileentry);
	}
	closedir(dirhandle);
	out.sort();
}

static void Touch(const std::string &p) { FILE *f = fopen(p.c_str(), "w"); if(f) fclose(f); }

// The fixture is rebuilt by rust/src/backend/sdl/tests.rs; keep both lists identical.
static void GenFind(const char *dir)
{
	std::string base = std::string(dir) + "/fixture";
	mkdir(base.c_str(), 0755);
	const char *files[] = {"a.TXT", "b.txt", "c.dat", ".hidden.txt", "Zed.Txt", "x y.txt", "ab", "10.txt", "9.txt"};
	for(const char *n : files) Touch(base + "/" + n);
	const char *dirs[] = {"Sub", "subb", ".dotdir", "dir.txt"};
	for(const char *n : dirs) mkdir((base + "/" + n).c_str(), 0755);
	if(symlink("Sub", (base + "/linkdir").c_str())) {}
	if(symlink("missing", (base + "/dangling").c_str())) {}
	std::string path = std::string(dir) + "/find.txt";
	FILE *f = fopen(path.c_str(), "w");
	const char *masks[] = {"*", "*.txt", "?.txt", "A*", "*.dat", "x*", "??.txt", "nomatch"};
	for(const char *m : masks)
	{
		std::list<std::wstring> l;
		FindFiles(l, To16(base), To16(m));
		fprintf(f, "FILES %s", m);
		for(auto &w : l) fprintf(f, "\t%s", To8(w).c_str());
		fprintf(f, "\n");
		std::list<std::wstring> l2;
		FindFiles(l2, To16(base + "/"), To16(m));
		fprintf(f, "FILESSLASH %s", m);
		for(auto &w : l2) fprintf(f, "\t%s", To8(w).c_str());
		fprintf(f, "\n");
	}
	for(int h = 0; h < 2; ++h)
		for(int u = 0; u < 2; ++u)
		{
			std::list<std::wstring> l;
			FindFolders(l, To16(base), h, u);
			fprintf(f, "FOLDERS %d %d", h, u);
			for(auto &w : l) fprintf(f, "\t%s", To8(w).c_str());
			fprintf(f, "\n");
		}
	fclose(f);
	if(system(("rm -rf '" + base + "'").c_str())) {}
}

int main(int argc, char **argv)
{
	if(argc < 2) return 1;
	GenEvents(argv[1]);
	GenPati(argv[1]);
	GenTimer(argv[1]);
	GenModes(argv[1]);
	GenArgs(argv[1]);
	GenFind(argv[1]);
	return 0;
}
