// res_probe ROOT OPS [LOG]: drives cResources (resource dirs, dir files, XML documents, binary buffers, entity/area loader
// tables, language files, destruction log), cResourceVarsObject and the loader handlers (mesh, bitmap, video, world with
// stub loaders) with a seeded op walk over the game data. Prints "ops<TAB>fnv64"; the full transcript (results plus every
// Log/Warning/Error line) goes to LOG. OHPL_IMPL=cpp and OHPL_IMPL=rust must produce the same bytes.
#include "common.h"
#include "resources/Resources.h"
#include "resources/ResourceBase.h"
#include "resources/FileSearcher.h"
#include "resources/LanguageFile.h"
#include "resources/LowLevelResources.h"
#include "resources/MeshLoaderHandler.h"
#include "resources/MeshLoader.h"
#include "resources/BitmapLoaderHandler.h"
#include "resources/BitmapLoader.h"
#include "resources/VideoLoaderHandler.h"
#include "resources/VideoLoader.h"
#include "resources/WorldLoaderHandler.h"
#include "resources/WorldLoader.h"
#include "resources/XmlDocument.h"
#include "resources/BinaryBuffer.h"
#include "impl/XmlDocumentTiny.h"
#include "graphics/Mesh.h"
#include "graphics/Bitmap.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include <algorithm>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>

using namespace hpl;
namespace fs = std::filesystem;

static std::string gLog;

namespace hpl {
	void FatalError(const char*, ...) { abort(); }
	static void Emit(const char *asPrefix, const char *fmt, va_list ap)
	{
		char text[2048];
		vsnprintf(text, sizeof(text), fmt, ap);
		gLog += asPrefix;
		gLog += text;
	}
	void Error(const char *fmt, ...) { va_list ap; va_start(ap, fmt); Emit("E:", fmt, ap); va_end(ap); }
	void Warning(const char *fmt, ...) { va_list ap; va_start(ap, fmt); Emit("W:", fmt, ap); va_end(ap); }
	void Log(const char *fmt, ...) { va_list ap; va_start(ap, fmt); Emit("L:", fmt, ap); va_end(ap); }
	void LogUpdate(const char*, ...) {}
}

class cStubLowLevel : public iLowLevelResources
{
public:
	void AddBitmapLoaders(cBitmapLoaderHandler*) override {}
	void AddMeshLoaders(cMeshLoaderHandler*) override {}
	void AddVideoLoaders(cVideoLoaderHandler*) override {}
	iXmlDocument *CreateXmlDocument(const tString &asName = "") override { return new cXmlDocumentTiny(asName); }
};

class cStubEntityLoader : public iEntityLoader
{
public:
	cStubEntityLoader(const tString &asName) : iEntityLoader(asName) {}
	~cStubEntityLoader() { gLog += "D:entity " + msName + "\n"; }
	iEntity3D *Load(const tString&, int, bool, cXmlElement*, const cMatrixf&, const cVector3f&, cWorld*, const tString&, const tWString&, cResourceVarsObject*) override { return NULL; }
};

class cStubAreaLoader : public iAreaLoader
{
public:
	cStubAreaLoader(const tString &asName) : iAreaLoader(asName) {}
	~cStubAreaLoader() { gLog += "D:area " + msName + "\n"; }
	void Load(const tString&, int, bool, const cVector3f&, const cMatrixf&, cWorld*) override {}
};

class cStubMeshLoader : public iMeshLoader
{
public:
	cStubMeshLoader() : iMeshLoader(NULL) { AddSupportedExtension("dae"); AddSupportedExtension("msh"); AddSupportedExtension("FBX"); }
	cMesh *LoadMesh(const tWString &asFile, tMeshLoadFlag aFlags) override
	{
		gLog += "mesh-load " + cString::To8Char(asFile) + " " + std::to_string((int)aFlags) + "\n";
		if(Fnv(cString::To8Char(asFile)) % 5 == 0) return NULL;
		return new cMesh("m", asFile, NULL, NULL);
	}
	bool SaveMesh(cMesh*, const tWString &asFile) override { gLog += "mesh-save " + cString::To8Char(asFile) + "\n"; return asFile.size() % 2; }
	cAnimation *LoadAnimation(const tWString &asFile) override { gLog += "anim-load " + cString::To8Char(asFile) + "\n"; return reinterpret_cast<cAnimation*>(asFile.size() % 3 ? 0x10 : 0); }
	bool SaveAnimation(cAnimation*, const tWString&) override { return false; }
};

class cStubBitmapLoader : public iBitmapLoader
{
public:
	cStubBitmapLoader() { AddSupportedExtension("png"); AddSupportedExtension("jpg"); AddSupportedExtension("dds"); AddSupportedExtension("tga"); }
	cBitmap *LoadBitmap(const tWString &asFile, tBitmapLoadFlag aFlags) override
	{
		gLog += "bmp-load " + cString::To8Char(asFile) + " " + std::to_string((int)aFlags) + "\n";
		if(Fnv(cString::To8Char(asFile)) % 4 == 0) return NULL;
		return new cBitmap();
	}
	bool SaveBitmap(cBitmap*, const tWString &asFile, tBitmapSaveFlag aFlags) override { gLog += "bmp-save " + cString::To8Char(asFile) + " " + std::to_string((int)aFlags) + "\n"; return true; }
};

class cStubVideoLoader : public iVideoLoader
{
public:
	cStubVideoLoader() { AddSupportedExtension("ogv"); AddSupportedExtension("bik"); }
	iVideoStream *LoadVideo(const tWString &asFile) override { gLog += "video-load " + cString::To8Char(asFile) + "\n"; return reinterpret_cast<iVideoStream*>(0x20); }
};

class cStubWorldLoader : public iWorldLoader
{
public:
	cStubWorldLoader() { AddSupportedExtension("hpm"); AddSupportedExtension("map"); }
	cWorld *LoadWorld(const tWString &asFile, tWorldLoadFlag aFlags) override { gLog += "world-load " + cString::To8Char(asFile) + " " + std::to_string((int)aFlags) + "\n"; return reinterpret_cast<cWorld*>(0x30); }
};

int main(int argc, char **argv)
{
	if(argc < 3) return 2;
	std::string root = argv[1];
	long ops = atol(argv[2]);
	iResourceBase::SetLogCreateAndDelete(true);

	std::vector<std::string> files, xmls, langs, dirs;
	{
		std::error_code ec;
		for(fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
		{
			if(ec || !it->is_regular_file(ec)) { if(!ec && it->is_directory(ec)) dirs.push_back(it->path().lexically_relative(root).string()); continue; }
			std::string rel = it->path().lexically_relative(root).string();
			std::string ext = it->path().extension().string();
			files.push_back(rel);
			if(ext == ".xml" || ext == ".cfg" || ext == ".mat" || ext == ".ps") xmls.push_back(rel);
			if(ext == ".lang") langs.push_back(rel);
		}
		std::sort(files.begin(), files.end());
		std::sort(xmls.begin(), xmls.end());
		std::sort(langs.begin(), langs.end());
		std::sort(dirs.begin(), dirs.end());
	}
	auto Thin = [](std::vector<std::string> &v, size_t keep) { if(v.size() > keep) { std::vector<std::string> o; for(size_t i = 0; i < v.size(); i += v.size() / keep) o.push_back(v[i]); v = o; } };
	Thin(files, 2000); Thin(xmls, 400); Thin(dirs, 60);
	if(files.empty()) return 3;

	uint64_t seed = 88172645463325252ull;
	auto Rnd = [&](uint64_t n) { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed % n; };

	cStubLowLevel low;
	void *pMem = calloc(1, sizeof(cResources));
	cResources *pRes = new(pMem) cResources(&low, NULL);

	pRes->AddResourceDir(cString::To16Char(root), true);
	void *pGfx = calloc(1, 4096);
	cMeshLoaderHandler meshes(pRes, NULL);
	cBitmapLoaderHandler bitmaps(pRes, reinterpret_cast<cGraphics*>(pGfx));
	cVideoLoaderHandler videos(pRes, reinterpret_cast<cGraphics*>(pGfx));
	cWorldLoaderHandler worlds(pRes, NULL, NULL, NULL);
	meshes.AddLoader(new cStubMeshLoader());
	bitmaps.AddLoader(new cStubBitmapLoader());
	videos.AddLoader(new cStubVideoLoader());
	worlds.AddLoader(new cStubWorldLoader());

	cResourceVarsObject vars[2];
	std::vector<iXmlDocument*> docs;
	std::vector<cBinaryBuffer*> bufs;
	static const char *vnames[] = { "A", "b", "Width", "Pos", "Col", "S", "x y", "" };
	static const char *vvals[] = { "true", "false", "1", "-7", "3.5", "1 2 3", "1 2", "1 2 3 4", "abc", "", "0.25", " 4  5 " };

	std::string transcript;
	uint64_t total = 1469598103934665603ull;
	std::vector<std::string> opened;
	for(long op = 0; op < ops; ++op)
	{
		gLog.clear();
		std::string line;
		uint64_t k = Rnd(100);
		cResources::SetForceCacheLoadingAndSkipSaving(Rnd(2));
		auto Pick = [&](std::vector<std::string> &v) { return v.empty() ? std::string("none") : v[Rnd(v.size())]; };
		if(k < 15)
		{
			cResourceVarsObject &v = vars[Rnd(2)];
			const char *n = vnames[Rnd(8)];
			const char *val = vvals[Rnd(12)];
			switch(Rnd(14))
			{
			case 0: v.SetUserVariable(n, val); line = "set"; break;
			case 1: v.AddVarBool(n, Rnd(2)); line = "addb"; break;
			case 2: v.AddVarInt(n, (int)Rnd(100) - 50); line = "addi"; break;
			case 3: v.AddVarFloat(n, (float)Rnd(1000) / 8.0f); line = "addf"; break;
			case 4: v.AddVarVector2f(n, cVector2f((float)Rnd(9), -1.5f)); line = "addv2"; break;
			case 5: v.AddVarVector3f(n, cVector3f(1.0f, (float)Rnd(7) / 3.0f, 3.0f)); line = "addv3"; break;
			case 6: v.AddVarColor(n, cColor(0.5f, (float)Rnd(5), 0.25f, 1.0f)); line = "addc"; break;
			case 7: v.AddVarString(n, val); line = "adds"; break;
			case 8: { tString *p = v.GetUserVariable(n); line = std::string("get ") + (p ? "'" + *p + "'" : "null"); break; }
			case 9: line = "b=" + std::to_string(v.GetVarBool(n, Rnd(2))) + " i=" + std::to_string(v.GetVarInt(n, 99)) + " f=" + std::to_string(v.GetVarFloat(n, 1.25f)); break;
			case 10: { cVector2f a = v.GetVarVector2f(n, cVector2f(9, 8)); cVector3f b = v.GetVarVector3f(n, cVector3f(7, 6, 5)); cColor c = v.GetVarColor(n); line = "v " + a.ToString() + b.ToString() + c.ToString(); break; }
			case 11: line = "s=" + v.GetVarString(n, "dflt"); break;
			case 12: vars[0] = vars[1]; line = "assign"; break;
			default:
			{
				cResourceVarsObject copy(v);
				line = "copy " + copy.GetVarString(n, "-");
				break;
			}
			}
			line = "vars " + line;
		}
		else if(k < 25)
		{
			std::string n = "T" + std::to_string(Rnd(6));
			bool bDefault = Rnd(3) == 0;
			if(Rnd(2)) pRes->AddEntityLoader(new cStubEntityLoader(n), bDefault);
			else pRes->AddAreaLoader(new cStubAreaLoader(n), bDefault);
			line = "addloader " + n + (bDefault ? " default" : "");
		}
		else if(k < 35)
		{
			std::string n = "T" + std::to_string(Rnd(9));
			iEntityLoader *e = pRes->GetEntityLoader(n);
			iAreaLoader *a = pRes->GetAreaLoader(n);
			line = "getloader " + n + " " + (e ? e->GetName() : "null") + " " + (a ? a->GetName() : "null");
		}
		else if(k < 42)
		{
			std::string d = Pick(dirs);
			bool bSub = Rnd(2);
			line = "adddir " + d + std::to_string(pRes->AddResourceDir(cString::To16Char(root + "/" + d), bSub));
			if(Rnd(40) == 0) { pRes->ClearResourceDirs(); pRes->AddResourceDir(cString::To16Char(root), true); line += " cleared"; }
		}
		else if(k < 52)
		{
			std::string n = Pick(files);
			cFileSearcher *pFs = pRes->GetFileSearcher();
			line = "path " + cString::To8Char(pFs->GetFilePath(cString::GetFileName(n)));
		}
		else if(k < 60)
		{
			std::string n = Rnd(5) ? Pick(xmls) : "missing.xml";
			bool bAlt = Rnd(2);
			line = "dirsfile " + n + " " + std::to_string(pRes->LoadResourceDirsFile(root + "/" + n, bAlt ? cString::To16Char(root + "/") : _W("")));
		}
		else if(k < 70)
		{
			if(!docs.empty() && Rnd(3) == 0)
			{
				size_t i = Rnd(docs.size());
				pRes->DestroyXmlDocument(docs[i]);
				docs.erase(docs.begin() + i);
				line = "xmldel";
			}
			else
			{
				std::string n = Rnd(6) ? Pick(xmls) : "missing.xml";
				iXmlDocument *d = pRes->LoadXmlDocument(cString::GetFileName(n));
				line = "xmlload " + n + " " + (d ? "ok" : "null");
				if(d) { docs.push_back(d); cXmlElement *e = d->GetFirstElement(""); line += e ? " " + e->GetValue() : ""; }
			}
		}
		else if(k < 76)
		{
			if(!bufs.empty() && Rnd(3) == 0)
			{
				size_t i = Rnd(bufs.size());
				pRes->DestroyBinaryBuffer(bufs[i]);
				bufs.erase(bufs.begin() + i);
				line = "bindel";
			}
			else
			{
				std::string n = Rnd(6) ? Pick(files) : "missing.bin";
				cBinaryBuffer *b = pRes->LoadBinaryBuffer(cString::GetFileName(n));
				line = "binload " + n + " " + (b ? "ok" : "null");
				if(b) bufs.push_back(b);
			}
		}
		else if(k < 84)
		{
			if(Rnd(6) == 0) { pRes->ClearTranslations(); line = "cleartr"; }
			else if(!langs.empty() && Rnd(2))
			{
				std::string n = Pick(langs);
				line = "lang " + n + " " + std::to_string(pRes->AddLanguageFile(root + "/" + n, Rnd(2), Rnd(2) ? cString::To16Char(root + "/") : _W("")));
			}
			else
			{
				static const char *cats[] = { "Menu", "menu", "Hints", "Nope" };
				static const char *names[] = { "Start", "start", "Exit", "Options", "x" };
				std::string a = cats[Rnd(4)], b = names[Rnd(5)];
				line = "tr " + a + "/" + b + " " + cString::To8Char(pRes->Translate(a, b));
				cLanguageFile *pL = pRes->GetLanguageFile();
				if(pL && Rnd(4) == 0) { pL->SetEntry(a, b, _W("set")); line += " has=" + std::to_string(pL->HasEntry(a, b)); }
			}
		}
		else if(k < 95)
		{
			std::string n = Rnd(8) ? root + "/" + Pick(files) : "/x/none.unknown";
			tWString w = cString::To16Char(n);
			switch(Rnd(9))
			{
			case 0: { cMesh *m = meshes.LoadMesh(w, (tMeshLoadFlag)Rnd(4)); line = "mesh " + std::string(m ? cString::To8Char(m->GetFullPath()) : "null"); break; }
			case 1: line = "meshsave " + std::to_string(meshes.SaveMesh(NULL, w)); break;
			case 2: line = "anim " + std::to_string((long)(meshes.LoadAnimation(w) != NULL)); break;
			case 3: { cBitmap *b = bitmaps.LoadBitmap(w, (tBitmapLoadFlag)Rnd(3)); line = "bmp " + (b ? cString::To8Char(b->GetFileName()) : std::string("null")); break; }
			case 4: line = "bmpsave " + std::to_string(bitmaps.SaveBitmap(NULL, w, (tBitmapSaveFlag)Rnd(3))); break;
			case 5: line = "video " + std::to_string((long)(videos.LoadVideo(w) != NULL)); break;
			case 6: line = "world " + std::to_string((long)(worlds.LoadWorld(cString::To16Char(cString::SetFileExt(n, Rnd(2) ? "hpm" : "map")), (tWorldLoadFlag)Rnd(3)) != NULL)); break;
			case 7: line = "types " + std::to_string(meshes.GetSupportedTypes()->size()) + std::to_string(bitmaps.GetSupportedTypes()->size()); break;
			default: line = "cache " + cString::To8Char(cResources::GetCacheFile(w, _W("msh")));
			}
		}
		else line = "noop";
		line += "\n";
		for(size_t i = 0; i < gLog.size(); ) { size_t e = gLog.find('\n', i); line += "  " + gLog.substr(i, e - i) + "\n"; i = e + 1; }
		total = Fnv(line, total);
		if(argc > 3) transcript += line;
	}
	gLog.clear();
	pRes->~cResources();
	std::string fin = "final\n" + gLog;
	total = Fnv(fin, total);
	if(argc > 3) transcript += fin;
	printf("ops=%ld digest=%s\n", ops, Hex(total).c_str());
	if(argc > 3) std::ofstream(argv[3]) << transcript;
	return 0;
}
