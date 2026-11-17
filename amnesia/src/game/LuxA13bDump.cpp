#include "LuxA13bDump.h"
#include "resources/ConfigFile.h"
#include "system/String.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#if defined(__linux__)
#include <unistd.h>
#endif

// OHPL_A13B_DUMP: init/exit sequence of cLuxBase (steps, config reads, statics, modules, loaders, paths).
// Paths are masked so runs from different scratch dirs compare equal.

static FILE *gpDump = NULL;
static bool gbInit = false;

static std::string Mask(std::string s)
{
	static const char *vars[] = {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME", "XDG_RUNTIME_DIR", "OPENHPL_SCRATCH_ROOT", "HOME", "PWD"};
	std::vector<std::pair<std::string, std::string> > v;
	for(const char *n : vars)
	{
		const char *e = getenv(n);
		if(e && strlen(e) > 3) v.push_back(std::make_pair(std::string(e), std::string("$") + n));
	}
	std::sort(v.begin(), v.end(), [](const std::pair<std::string, std::string> &a, const std::pair<std::string, std::string> &b) { return a.first.size() > b.first.size(); });
	for(auto &p : v)
		for(size_t i = s.find(p.first); i != std::string::npos; i = s.find(p.first, i + p.second.size()))
			s.replace(i, p.first.size(), p.second);
#if defined(__linux__)
	std::string sPid = "." + std::to_string((int)getpid());
	for(size_t i = s.find(sPid); i != std::string::npos; i = s.find(sPid, i + 5))
		s.replace(i, sPid.size(), ".$PID");
#endif
	return s;
}

static void CfgHook(const hpl::tWString& asFile, const hpl::tString& asLevel, const hpl::tString& asName, const char *asVal)
{
	std::string sFile = hpl::cString::To8Char(asFile);
	size_t lSlash = sFile.rfind('/');
	if(lSlash != std::string::npos) sFile = sFile.substr(lSlash + 1);
	OhplA13bLog("cfg %s %s.%s = %s", sFile.c_str(), asLevel.c_str(), asName.c_str(), asVal ? asVal : "<none>");
}

static FILE *Dump()
{
	if(gbInit == false)
	{
		gbInit = true;
		const char *sPath = getenv("OHPL_A13B_DUMP");
		if(sPath && *sPath)
		{
			gpDump = fopen(sPath, "w");
			if(gpDump) hpl::gpConfigReadHook = CfgHook;
		}
	}
	return gpDump;
}

bool OhplA13bEnabled() { return Dump() != NULL; }

void OhplA13bLog(const char *fmt, ...)
{
	FILE *f = Dump();
	if(f == NULL) return;
	char buf[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	fputs(Mask(buf).c_str(), f);
	fputc('\n', f);
	fflush(f);
}
