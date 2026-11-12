// mgr_probe GAME_ROOT OPS [FULL]: drives iResourceManager (create / release / destroy / DestroyUnused / DestroyAll /
// GetResource / iteration / load tabs) through a stub manager over a file searcher built from GAME_ROOT.
// Names come from the game's file names; the op sequence is a fixed LCG. Prints "ops<TAB>fnv64" and optionally the full log.
#include "common.h"
#include "resources/FileSearcher.h"
#include "resources/ResourceBase.h"
#include "resources/ResourceManager.h"
#include "system/String.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace hpl;
namespace fs = std::filesystem;

class cRes : public iResourceBase
{
public:
	cRes(const tString &asName, const tWString &asPath) : iResourceBase(asName, asPath, 0) {}
	bool Reload() override { return true; }
	void Unload() override {}
	void Destroy() override {}
};

class cMgr : public iResourceManager
{
public:
	cMgr(cFileSearcher *apFs) : iResourceManager(apFs, NULL, NULL), mlSerial(0), mlOp(0) {}
	~cMgr() { DestroyAll(); }

	void Unload(iResourceBase *) override {}
	void Destroy(iResourceBase *apRes) override
	{
		if(apRes->HasUsers() == false)
		{
			RemoveResource(apRes);
			delete apRes;
		}
	}

	std::string Id(iResourceBase *p) { return p ? std::to_string(p->GetHandle()) : "null"; }

	std::string Create(const tString &asName)
	{
		BeginLoad(asName);
		tWString sPath;
		int lEq = -1;
		iResourceBase *pRes = FindLoadedResource(asName, sPath, &lEq);
		std::string r = "create " + asName + " eq=" + std::to_string(lEq);
		if(pRes == NULL && sPath != _W(""))
		{
			pRes = new cRes(asName, sPath);
			pRes->SetHandle(++mlSerial);
			pRes->GetMeta()->time = mlOp;
			AddResource(pRes);
		}
		if(pRes) { pRes->GetMeta()->user_count++; pRes->GetMeta()->time = mlOp; }
		EndLoad();
		return r + " -> " + Id(pRes) + " " + cString::To8Char(sPath) + " tabs=" + std::to_string(GetTabs().size());
	}

	std::string Nest(int alDepth)
	{
		std::string r;
		for(int i = 0; i < alDepth; ++i) { BeginLoad("n" + std::to_string(i)); r += std::to_string(GetTabs().size()) + ","; }
		for(int i = 0; i < alDepth; ++i) EndLoad();
		return r + std::to_string(GetTabs().size());
	}

	std::string Dump()
	{
		std::string s;
		cResourceBaseIterator it = GetResourceBaseIterator();
		size_t n = 0;
		while(it.HasNext())
		{
			iResourceBase *p = it.Next();
			s += cString::To8Char(p->GetFullPath()) + ":" + std::to_string(p->GetHandle()) + ":" + std::to_string(p->GetUserCount()) + ";";
			++n;
		}
		return "dump " + std::to_string(n) + " " + Hex(Fnv(s));
	}

	std::vector<iResourceBase*> Live()
	{
		std::vector<iResourceBase*> v;
		cResourceBaseIterator it = GetResourceBaseIterator();
		while(it.HasNext()) v.push_back(it.Next());
		return v;
	}

	unsigned long mlSerial, mlOp;
};

int main(int argc, char **argv)
{
	if(argc < 3) return 2;
	std::string root = argv[1];
	long ops = atol(argv[2]);

	cFileSearcher searcher;
	searcher.AddDirectory(cString::To16Char(root), "*.*", true);

	std::vector<std::string> names, rels;
	{
		std::error_code ec;
		for(fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
			if(!ec && it->is_regular_file(ec)) rels.push_back(it->path().lexically_relative(root).string());
	}
	std::sort(rels.begin(), rels.end());
	for(size_t i = 0; i < rels.size(); i += std::max<size_t>(1, rels.size() / 3000))
	{
		fs::path p(rels[i]);
		std::string base = p.filename().string(), parent = p.parent_path().filename().string();
		names.push_back(base);
		if(!parent.empty()) names.push_back(parent + "/" + base);
		std::string up = base;
		for(char &c : up) c = toupper((unsigned char)c);
		names.push_back(up);
	}
	names.push_back("does_not_exist.png");
	if(getenv("MGR_DEBUG")) fprintf(stderr, "files=%zu names=%zu first=%s\n", rels.size(), names.size(), rels.empty() ? "" : rels[0].c_str());

	uint64_t seed = 88172645463325252ull;
	auto Rnd = [&](uint64_t n) { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed % n; };

	std::string log;
	uint64_t total = 1469598103934665603ull;
	{
		cMgr mgr(&searcher);
		for(long op = 0; op < ops; ++op)
		{
			mgr.mlOp = op;
			std::string line;
			uint64_t k = Rnd(100);
			std::vector<iResourceBase*> live;
			if(k >= 40) live = mgr.Live();
			if(k < 40) line = mgr.Create(names[Rnd(names.size())]);
			else if(k < 55 && !live.empty())
			{
				iResourceBase *p = live[Rnd(live.size())];
				line = "release " + mgr.Id(p);
				p->DecUserCount();
				line += " users=" + std::to_string(p->GetUserCount());
				if(Rnd(2)) { std::string id = mgr.Id(p); mgr.Destroy(p); line += " destroy " + id + " size=" + std::to_string(mgr.Live().size()); }
			}
			else if(k < 65 && !live.empty())
			{
				iResourceBase *p = live[Rnd(live.size())];
				p->GetMeta()->user_count++;
				p->GetMeta()->time = op;
				line = "addref " + mgr.Id(p) + " users=" + std::to_string(p->GetUserCount());
			}
			else if(k < 70) { int keep = (int)Rnd(20); mgr.DestroyUnused(keep); line = "unused " + std::to_string(keep) + " " + mgr.Dump(); }
			else if(k < 80 && !live.empty())
			{
				iResourceBase *p = live[Rnd(live.size())];
				tWString path = Rnd(5) ? p->GetFullPath() : p->GetFullPath() + _W("x");
				line = "get " + cString::To8Char(path) + " -> " + mgr.Id(mgr.GetResource(path));
			}
			else if(k < 85)
			{
				int depth = 1 + (int)Rnd(3);
				line = "nested " + mgr.Nest(depth) + " " + mgr.Create(names[Rnd(names.size())]);
			}
			else if(k < 95) line = mgr.Dump();
			else if(!live.empty())
			{
				iResourceBase *p = live[Rnd(live.size())];
				while(p->HasUsers()) p->DecUserCount();
				std::string id = mgr.Id(p);
				mgr.Destroy(p);
				line = "force " + id + " size=" + std::to_string(mgr.Live().size());
			}
			else line = "noop";
			line += "\n";
			total = Fnv(line, total);
			if(argc > 3) log += line;
		}
		mgr.DestroyAll();
		std::string fin = "final " + std::to_string(mgr.Live().size()) + "\n";
		total = Fnv(fin, total);
		if(argc > 3) log += fin;
	}
	printf("ops=%ld digest=%s\n", ops, Hex(total).c_str());
	if(argc > 3) std::ofstream(argv[3]) << log;
	return 0;
}
