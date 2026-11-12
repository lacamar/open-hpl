// cfg_probe ROOT LIST SCRATCH: config files in LIST (relative to ROOT). Per file: load, every key read as
// string/int/float/bool/vector, then save as plain and CRC containers and reload the CRC one.
#include "common.h"
#include "resources/ConfigFile.h"
#include "system/String.h"
#include "impl/tinyXML/tinyxml.h"
#include <fstream>
#include <iostream>

using namespace hpl;

static std::string ReadFile(const std::string &p)
{
	std::string out;
	FILE *f = fopen(p.c_str(), "rb");
	if(!f) return out;
	char buf[65536];
	size_t n;
	while((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
	fclose(f);
	return out;
}

static std::string Reads(cConfigFile &cfg, const std::vector<std::pair<std::string, std::string>> &keys)
{
	std::string s;
	for(auto &k : keys)
	{
		s += k.first + "/" + k.second + "=" + cfg.GetString(k.first, k.second, "<D>");
		s += "|" + std::to_string(cfg.GetInt(k.first, k.second, -7));
		char b[64];
		snprintf(b, sizeof b, "|%.9g|%d", cfg.GetFloat(k.first, k.second, -1.5f), (int)cfg.GetBool(k.first, k.second, true));
		s += b;
		cVector3f v = cfg.GetVector3f(k.first, k.second, cVector3f(1, 2, 3));
		snprintf(b, sizeof b, "|%.9g,%.9g,%.9g", v.x, v.y, v.z);
		s += b;
		cVector2l l = cfg.GetVector2l(k.first, k.second, cVector2l(4, 5));
		s += "|" + std::to_string(l.x) + "," + std::to_string(l.y) + "\n";
	}
	s += "miss=" + cfg.GetString("NoLevel", "NoName", "<D>") + "\n";
	return s;
}

int main(int argc, char **argv)
{
	if(argc < 4) return 2;
	std::string root = argv[1], scratch = argv[3];
	std::ifstream list(argv[2]);
	std::string rel;
	while(std::getline(list, rel))
	{
		std::string path = root + "/" + rel;
		std::vector<std::pair<std::string, std::string>> keys;
		TiXmlDocument ref;
		if(FILE *f = fopen(path.c_str(), "rb"))
		{
			ref.LoadFile(f);
			fclose(f);
			for(TiXmlElement *e = ref.FirstChildElement(); e; e = e->NextSiblingElement())
				for(TiXmlAttribute *a = e->FirstAttribute(); a; a = a->Next()) keys.push_back({e->Value(), a->Name()});
		}

		cConfigFile cfg(cString::To16Char(path));
		bool ok = cfg.Load();
		std::string line = rel + "\t" + (ok ? "1" : "0");
		if(ok)
		{
			line += "\t" + Hex(Fnv(Reads(cfg, keys)));
			cfg.SetString("Probe", "Str", "va<l>&\"ue");
			cfg.SetInt("Probe", "Int", -42);
			cfg.SetFloat("Probe", "Float", 3.25f);
			cfg.SetBool("Probe", "Bool", true);
			cfg.SetVector3f("Probe", "Vec", cVector3f(1.5f, -2.f, 0.125f));
			cfg.SetString("Probe", "Str", "replaced");
			keys.push_back({"Probe", "Str"});
			keys.push_back({"Probe", "Vec"});
			cfg.SetFileLocation(cString::To16Char(scratch + ".plain"));
			bool s1 = cfg.Save();
			cfg.SetUseCRC(true);
			cfg.SetFileLocation(cString::To16Char(scratch + ".crc"));
			bool s2 = cfg.Save();
			cConfigFile back(cString::To16Char(scratch + ".crc"));
			back.SetUseCRC(true);
			bool ok2 = back.Load();
			line += "\t" + std::to_string(s1) + std::to_string(s2) + std::to_string(ok2);
			line += "\t" + Hex(Fnv(ReadFile(scratch + ".plain"))) + "\t" + Hex(Fnv(ReadFile(scratch + ".crc")));
			line += "\t" + Hex(Fnv(Reads(back, keys)));
		}
		cConfigFile bad(cString::To16Char(path));
		bad.SetUseCRC(true);
		line += "\tcrc-garbage:" + std::to_string(bad.Load());
		std::cout << line << "\n";
	}
	return 0;
}
