// lang_probe ROOT LIST: language files; dumps resource dirs, categories and decoded entries after one load,
// then a second load of the same file (merge keeps the first entries).
#include "common.h"
#include "resources/LanguageFile.h"
#include "resources/Resources.h"
#include "system/String.h"
#include <fstream>
#include <iostream>

using namespace hpl;

static std::string gDirs;

namespace hpl {
	bool cResources::AddResourceDir(const tWString &asDir, bool abAddSubDirectories, const tString &asMask)
	{
		gDirs += cString::To8Char(asDir) + (abAddSubDirectories ? "+" : "-") + asMask + "\n";
		return true;
	}
}

static std::string Dump(cLanguageFile &lf)
{
#ifdef OHPL_LANG_FOREACH
	struct cDump { std::string msOut, msCat; bool mbAny = false; } d;
	lf.ForEach([](void *c, const tString &asCat, const tString &asName, const tWString *apText)
	{
		cDump *p = static_cast<cDump*>(c);
		if(!p->mbAny || asCat != p->msCat) { p->msOut += "[" + asCat + "]\n"; p->msCat = asCat; p->mbAny = true; }
		if(apText == NULL) return;
		p->msOut += asName + "=";
		for(wchar_t w : *apText) p->msOut += std::to_string((int)w) + ",";
		p->msOut += "\n";
	}, &d);
	return d.msOut;
#else
	std::string s;
	for(auto &c : *lf.GetCategoryMap())
	{
		s += "[" + c.first + "]\n";
		for(auto &e : c.second->m_mapEntries)
		{
			s += e.first + "=";
			for(wchar_t w : e.second->mwsText) s += std::to_string((int)w) + ",";
			s += "\n";
		}
	}
	return s;
#endif
}

int main(int argc, char **argv)
{
	if(argc < 3) return 2;
	std::string root = argv[1];
	std::ifstream list(argv[2]);
	std::string rel;
	int dummy = 0;
	while(std::getline(list, rel))
	{
		std::string path = root + "/" + rel;
		gDirs.clear();
		cLanguageFile lf(reinterpret_cast<cResources*>(&dummy));
		bool ok = lf.AddFromFile(cString::To16Char(path), true, _W(""));
		std::string first = ok ? Dump(lf) : "";
		bool ok2 = lf.AddFromFile(cString::To16Char(path), false, _W("ALT/"));
		std::string line = rel + "\t" + (ok ? "1" : "0") + (ok2 ? "1" : "0") + "\t" + Hex(Fnv(gDirs)) + "\t" + Hex(Fnv(first)) + "\t" + Hex(Fnv(Dump(lf)));
		if(ok)
		{
			std::string tr = cString::To8Char(lf.Translate("Menu", "Start"));
			line += "\t" + Hex(Fnv(tr));
		}
		std::cout << line << "\n";
	}
	return 0;
}
