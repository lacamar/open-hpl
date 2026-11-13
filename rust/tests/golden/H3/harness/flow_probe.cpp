// flow_probe GAME_ROOT OPS [LOG]: drives the manager flows (model, ent, script, font, sound, sound entity, particle,
// texture simple/anim/cube, image frames, material, gpu shader, video) through one stub manager per flavor.
// With OHPL_IMPL=cpp the flows are the pre-port C++ bodies (copied verbatim from the managers, object creation replaced
// by deterministic stubs); with OHPL_IMPL=rust they are the Rust flows driven through the host op table.
// Every op appends its result and the captured Log/Warning/Error lines to the transcript; prints "ops<TAB>fnv64".
#include "common.h"
#include "resources/FileSearcher.h"
#include "resources/ResourceBase.h"
#include "resources/ResourceManager.h"
#include "resources/RustResMgr.h"
#include "resources/RustShader.h"
#include "resources/XmlDocument.h"
#include "impl/XmlDocumentTiny.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include <algorithm>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <set>
#include <map>
#include <regex>

using namespace hpl;
namespace fs = std::filesystem;

static std::string gLog;
static long gTick = 0;
extern "C" time_t time(time_t *t) { time_t v = 1700000000 + gTick / 5; if(t) *t = v; return v; }

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

static bool Fails(const std::string &asKey, int alSalt) { return (Fnv(asKey) + alSalt) % 7 == 0; }
static tString Narrow(const tWString &asPath) { return cString::To8Char(asPath); }

class cRes : public iResourceBase
{
public:
	cRes(const tString &asName, const tWString &asPath, const char *asKind) : iResourceBase(asName, asPath, 0), msKind(asKind)
	{
		mlMem = (int)(Fnv(asName) % 1000);
		mbStream = false;
		mpFrame = NULL; mpBmpFrame = NULL;
	}
	~cRes() { gLog += std::string("D:") + msKind + " " + msName + "\n"; }
	bool Reload() override { return true; }
	void Unload() override {}
	void Destroy() override {}
	std::string msKind;
	int mlMem;
	bool mbStream;
	void *mpFrame, *mpBmpFrame;
};

struct cTF { int handle; int pics; bool custom; };
struct cBF { int handle; int cap; int imgs; int additions; bool locked; cTF *tf; };

enum eFlavor { fAnim, fMesh, fEnt, fScript, fFont, fSound, fSndEnt, fPs, fTex, fImage, fMat, fShader, fVideo, fCount };
static const char *gFlavorName[] = { "anim", "mesh", "ent", "script", "font", "sound", "sndent", "ps", "tex", "image", "mat", "shader", "video" };
static const char *gShutdown[] = { " Done with animations\n", " Done with meshes\n", " Done with ent files\n", " Done with scripts\n", " Done with fonts\n", " Done with sounds\n", " Done with sound entities\n", " Done with particles\n", " Destroyed all textures\n", " Done with images\n", " Done with materials\n", " Done with Gpu programs\n", "" };
static const int gShutdownCode[] = { OHPL_H3S_ANIMATION, OHPL_H3S_MESH, OHPL_H3S_ENT_FILE, OHPL_H3S_SCRIPT, OHPL_H3S_FONT, OHPL_H3S_SOUND, OHPL_H3S_SOUND_ENTITY, OHPL_H3S_PARTICLE, OHPL_H3S_TEXTURE, OHPL_H3S_IMAGE, OHPL_H3S_MATERIAL, OHPL_H3S_GPU_SHADER, -1 };

static tStringVec gBitmapExts = { "png", "jpg", "dds", "tga", "bmp" };
static tStringVec gMeshExts = { "dae", "msh", "fbx" };
static tStringList gSoundFormats = { "ogg", "wav" };

class cProbeMgr : public iResourceManager
{
public:
	cProbeMgr(cFileSearcher *apFs, eFlavor aFlavor) : iResourceManager(apFs, NULL, NULL), mFlavor(aFlavor), mlMem(0), mlFrameHandle(0), mvFrameSize(512, 512), mbHasResolver(false)
	{
		if(mpRs) ohpl_h3f_image_init(mpRs, &mOps, mvFrameSize.x, mvFrameSize.y);
		mlOp = 0;
	}

	void Shutdown()
	{
		if(mpRs) { if(gShutdownCode[mFlavor] >= 0) ohpl_h3f_shutdown(mpRs, H3Ops(), gShutdownCode[mFlavor]); return; }
		if(mFlavor == fPs)
		{
			for(tResourceBaseMapIt it = Map()->begin(); it != Map()->end(); ++it) while(it->second->HasUsers()) it->second->DecUserCount();
			DestroyUnused(0);
		}
		else if(mFlavor == fSound) SoundDestroyAll();
		else if(mFlavor != fVideo) DestroyAll();
		if(mFlavor != fVideo) Log("%s", gShutdown[mFlavor]);
	}

	cResourceMap *Map() { return &m_mapResources; }

	std::string Id(iResourceBase *p) { return p ? std::to_string(p->GetHandle()) + ":" + p->GetName() : "null"; }

	// ---------------------------------------------------------------- stubs
	cRes *Make(const char *asKind, const tString &asName, const tWString &asPath, int alSalt)
	{
		bool bFail = Fails(asName + Narrow(asPath), alSalt);
		gLog += std::string("M:") + asKind + " " + asName + " " + Narrow(asPath) + (bFail ? " fail\n" : "\n");
		if(bFail) return NULL;
		cRes *p = new cRes(asName, asPath, asKind);
		p->SetHandle(++mlSerial);
		return p;
	}
	static tString FileOf(const tWString &asPath) { return cString::GetFileName(Narrow(asPath)); }

	void *H3Create(OhplH3Create &c) override
	{
		tString sName = H3Name(c.name);
		tWString sPath = H3Wide(c.path);
		switch(c.kind)
		{
		case OHPL_H3_ANIMATION: return static_cast<iResourceBase*>(Make("anim", FileOf(sPath), sPath, 0));
		case OHPL_H3_MESH: return static_cast<iResourceBase*>(Make("mesh", FileOf(sPath), sPath, 0));
		case OHPL_H3_ENT_FILE: return static_cast<iResourceBase*>(Make("ent", sName, sPath, 0));
		case OHPL_H3_SCRIPT: return static_cast<iResourceBase*>(Make("script", sName, sPath, 0));
		case OHPL_H3_SOUND_DATA:
		{
			cRes *p = Make(c.i[0] ? "stream" : "sample", sName, sPath, 0);
			if(p) p->mbStream = c.i[0] != 0;
			return static_cast<iResourceBase*>(p);
		}
		case OHPL_H3_SOUND_ENTITY: return static_cast<iResourceBase*>(Make("sndent", sName, sPath, 0));
		case OHPL_H3_CUSTOM_RESOLVER: return static_cast<iResourceBase*>(Resolve(sName));
		case OHPL_H3_VIDEO:
		{
			cRes *p = Make("video", FileOf(sPath), sPath, 0);
			if(p) p->SetFullPath(sPath);
			return static_cast<iResourceBase*>(p);
		}
		case OHPL_H3_FONT_TTF: case OHPL_H3_FONT_BITMAP:
		{
			gLog += "M:fontobj " + sName + "\n";
			return static_cast<iResourceBase*>(Make(c.kind == OHPL_H3_FONT_TTF ? "ttf" : "bmpfont", sName, sPath, 1));
		}
		case OHPL_H3_FONT_UNKNOWN:
		{
			gLog += "M:fontobj " + sName + "\n";
			cRes *p = new cRes(sName, sPath, "fontobj");
			p->SetHandle(++mlSerial);
			return static_cast<iResourceBase*>(p);
		}
		case OHPL_H3_PS_DATA_FILE: case OHPL_H3_PS_DATA_ELEMENT:
			return static_cast<iResourceBase*>(Make("psdata", sName, sPath, 0));
		case OHPL_H3_PS_CREATE:
		{
			cRes *pData = H3Res<cRes>(c.ptr);
			gLog += "PS:" + pData->GetName() + " " + sName + " " + H3Name(c.name2) + " " + std::to_string(c.f[0]) + "\n";
			return &mPs;
		}
		case OHPL_H3_TEX_SIMPLE: return Tex(c, 1);
		case OHPL_H3_TEX_ANIM: case OHPL_H3_TEX_CUBE: return Tex(c, (int)c.npaths);
		case OHPL_H3_IMAGE: return Bitmap(sPath);
		case OHPL_H3_MAT_NEW: return static_cast<iResourceBase*>(MatNew(sName, sPath, c.i[0] != 0, c.ptr));
		case OHPL_H3_MAT_TYPE: return MatType(sName);
		case OHPL_H3_MAT_SET: MatSet(H3Res<cRes>(c.ptr), c.i[0], c.i[1], c.i[2], sName); return NULL;
		case OHPL_H3_MAT_TEXTURE: return static_cast<iResourceBase*>(MatTexture(sName, c.i[0], c.i[1], c.i[2], c.i[3]));
		case OHPL_H3_MAT_TEX_SETUP: MatTexSetup(H3Res<cRes>(c.ptr), H3Res<cRes>(c.ptr2), c.i[0], c.i[1], c.i[2], c.f[0]); return NULL;
		case OHPL_H3_MAT_VAR: MatVar(c.i[0] != 0, sName, H3Name(c.name2)); return NULL;
		case OHPL_H3_MAT_VARS_APPLY: MatVarsApply(c.ptr, H3Res<cRes>(c.ptr2)); return NULL;
		case OHPL_H3_MAT_ADD_UV: MatUv(H3Res<cRes>(c.ptr), c.i[0], c.i[1], c.f[0], c.f[1]); return NULL;
		case OHPL_H3_IMG_BITMAP_FRAME: return NewBitmapFrame(c.i[0], c.i[1], c.i[2]);
		case OHPL_H3_IMG_CUSTOM_FRAME: { cTF *p = new cTF{c.i[0], 0, true}; return p; }
		case OHPL_H3_IMG_ADD_BITMAP: return static_cast<iResourceBase*>(AddBitmap(static_cast<cBF*>(c.ptr), static_cast<int*>(c.ptr2), sPath));
		case OHPL_H3_SHADER_PARSE:
			gLog += "S:parse " + sName + " " + H3Name(c.name2) + " " + std::to_string(c.i[0]) + std::to_string(c.i[1]) + " " + Narrow(sPath) + "\n";
			msParsed = "parsed:" + sName + ":" + H3Name(c.name2);
			return NULL;
		case OHPL_H3_SHADER_TRANSPILE:
			gLog += "S:transpile " + sName + "\n";
			c.status = Fails(sName, 4) ? 1 : 0;
			if(c.status) msErr = "transpile failed " + sName;
			return NULL;
		case OHPL_H3_SHADER_FILE: return static_cast<iResourceBase*>(Make("shfile", sName, sPath, 5));
		case OHPL_H3_SHADER_STRING: return static_cast<iResourceBase*>(Make("shstr", sName, sPath, 6));
		}
		return NULL;
	}

	int64_t H3Call(int what, void *p, int64_t arg) override
	{
		switch(what)
		{
		case OHPL_H3C_IS_STREAM: return H3Res<cRes>(p)->mbStream;
		case OHPL_H3C_SOUND_SET_MGR: gLog += "set-sound-mgr " + H3Res<cRes>(p)->GetName() + "\n"; return 0;
		case OHPL_H3C_MEM_SIZE: return H3Res<cRes>(p)->mlMem;
		case OHPL_H3C_UPDATE: gLog += "update " + H3Res<cRes>(p)->GetName() + " " + std::to_string(H3Float(arg)) + "\n"; return 0;
		case OHPL_H3C_PRELOAD_SOUNDS: gLog += "preload " + H3Res<cRes>(p)->GetName() + "\n"; return 0;
		case OHPL_H3C_IMG_FRAME_TEX: return (int64_t)(intptr_t)H3Res<cRes>(p)->mpFrame;
		case OHPL_H3C_IMG_FRAME_BMP: return (int64_t)(intptr_t)H3Res<cRes>(p)->mpBmpFrame;
		case OHPL_H3C_FRAME_DEC_PIC: static_cast<cTF*>(p)->pics--; return 0;
		case OHPL_H3C_BMPFRAME_DEC_PIC: static_cast<cBF*>(p)->imgs--; return 0;
		case OHPL_H3C_FRAME_IS_EMPTY: return static_cast<cTF*>(p)->pics <= 0;
		case OHPL_H3C_FRAME_IS_CUSTOM: return static_cast<cTF*>(p)->custom;
		case OHPL_H3C_BMPFRAME_HANDLE: return static_cast<cBF*>(p)->handle;
		case OHPL_H3C_BMPFRAME_TEX: return (int64_t)(intptr_t)static_cast<cBF*>(p)->tf;
		case OHPL_H3C_BMPFRAME_IS_FULL: return static_cast<cBF*>(p)->imgs >= static_cast<cBF*>(p)->cap;
		case OHPL_H3C_BMPFRAME_IS_LOCKED: return static_cast<cBF*>(p)->locked;
		case OHPL_H3C_BMPFRAME_ADDITIONS: return static_cast<cBF*>(p)->additions;
		case OHPL_H3C_BMPFRAME_REORGANIZE: gLog += "reorganize " + std::to_string(static_cast<cBF*>(p)->handle) + "\n"; static_cast<cBF*>(p)->additions = 0; return 0;
		case OHPL_H3C_BMPFRAME_FLUSH: return static_cast<cBF*>(p)->handle % 2;
		case OHPL_H3C_BMPFRAME_SET_LOCKED: static_cast<cBF*>(p)->locked = arg != 0; return 0;
		case OHPL_H3C_BMPFRAME_DELETE: gLog += "del-bf " + std::to_string(static_cast<cBF*>(p)->handle) + "\n"; delete static_cast<cBF*>(p); return 0;
		case OHPL_H3C_FRAME_DELETE: gLog += "del-tf " + std::to_string(static_cast<cTF*>(p)->handle) + "\n"; delete static_cast<cTF*>(p); return 0;
		case OHPL_H3C_FRAME_HANDLE: return static_cast<cTF*>(p)->handle;
		case OHPL_H3C_BMP_WIDTH: return *static_cast<int*>(p) % 97 * 7 + 1;
		case OHPL_H3C_BMP_HEIGHT: return *static_cast<int*>(p) % 89 * 11 + 1;
		case OHPL_H3C_BMP_DELETE: delete static_cast<int*>(p); return 0;
		case OHPL_H3C_MAT_TYPE_Q:
		{
			cMT *pType = static_cast<cMT*>(p);
			if(arg == 0) return pType->trans;
			if(arg == 1) return (int64_t)pType->used.size();
			return pType->used[arg - 2];
		}
		case OHPL_H3C_MAT_COMPILE: gLog += "mat-compile " + H3Res<cRes>(p)->GetName() + "\n"; return 0;
		case OHPL_H3C_SHADER_POST: gLog += "S:post " + H3Res<cRes>(p)->GetName() + " " + std::to_string(arg) + "\n"; return 0;
		}
		return 0;
	}

	void H3Text(int what, void *p, const OhplH3WStr &path, void (*sink)(void*, const uint8_t*, size_t), void *ctx) override
	{
		std::string s;
		if(what == OHPL_H3C_SHADER_ERROR) s = msErr;
		else if(what == OHPL_H3C_FILE_BYTES)
		{
			FILE *pFile = fopen(Narrow(H3Wide(path)).c_str(), "rb");
			if(pFile == NULL) s = std::string(1, '\0');
			else
			{
				fseek(pFile, 0, SEEK_END);
				long lLength = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				std::vector<char> vData(lLength > 0 ? lLength : 0);
				bool bRead = lLength > 0 && fread(vData.data(), lLength, 1, pFile) == 1;
				fclose(pFile);
				s = std::string(1, (lLength <= 0 || bRead) ? '\2' : '\1') + std::string(vData.begin(), vData.end());
			}
		}
		else if(what == OHPL_H3C_PHYSICS_NAME_OF) s = p ? "phys-of-" + H3Res<cRes>(p)->GetName() : "phys-file-" + Narrow(H3Wide(path));
		sink(ctx, reinterpret_cast<const uint8_t*>(s.data()), s.size());
	}

	int *Bitmap(const tWString &asPath)
	{
		bool bFail = Fails(Narrow(asPath), 1);
		gLog += "B:" + Narrow(asPath) + (bFail ? " fail\n" : "\n");
		if(bFail) return NULL;
		return new int((int)(Fnv(Narrow(asPath)) % 1000));
	}

	void *Tex(OhplH3Create &c, int alBitmaps)
	{
		std::vector<int*> vBmps;
		for(int i = 0; i < alBitmaps; ++i)
		{
			tWString sPath = c.kind == OHPL_H3_TEX_SIMPLE ? H3Wide(c.path) : H3Wide(c.paths[i]);
			int *pBmp = Bitmap(sPath);
			if(pBmp == NULL)
			{
				for(int *b : vBmps) delete b;
				c.status = 1; c.idx = i;
				return NULL;
			}
			vBmps.push_back(pBmp);
		}
		for(int *b : vBmps) delete b;
		cRes *p = Make(c.kind == OHPL_H3_TEX_SIMPLE ? "tex" : c.kind == OHPL_H3_TEX_ANIM ? "texanim" : "texcube", H3Name(c.name), H3Wide(c.path), 2);
		if(p == NULL) { c.status = 2; return NULL; }
		return static_cast<iResourceBase*>(p);
	}

	cBF *NewBitmapFrame(int w, int h, int handle)
	{
		cTF *tf = new cTF{handle, 0, false};
		cBF *bf = new cBF{handle, 2 + (w + h) % 3, 0, 0, false, tf};
		gLog += "new-frame " + std::to_string(w) + "x" + std::to_string(h) + " h=" + std::to_string(handle) + " cap=" + std::to_string(bf->cap) + "\n";
		return bf;
	}

	cRes *AddBitmap(cBF *apFrame, int *apBmp, const tWString &asPath)
	{
		bool bFit = (*apBmp + apFrame->additions) % 3 != 0 && apFrame->imgs < apFrame->cap;
		apFrame->additions++;
		gLog += "add-bitmap f=" + std::to_string(apFrame->handle) + " " + std::to_string(*apBmp) + (bFit ? " ok\n" : " nofit\n");
		if(!bFit) return NULL;
		cRes *p = new cRes("img" + std::to_string(++mlImg), asPath, "image");
		p->SetHandle(++mlSerial);
		p->mpFrame = apFrame->tf; p->mpBmpFrame = apFrame;
		apFrame->imgs++; apFrame->tf->pics++;
		return p;
	}

	cRes *Resolve(const tString &asName)
	{
		gLog += "resolve " + asName + "\n";
		if(asName.size() % 2) return NULL;
		cRes *p = new cRes(asName, _W(""), "custom");
		p->SetHandle(++mlSerial);
		return p;
	}


	// ---------------------------------------------------------------- material stubs
	struct cMT { bool trans; std::vector<int> used; };

	void *MatType(const tString &asType)
	{
		uint64_t h = Fnv(asType);
		gLog += "mat-type " + asType + "\n";
		if(h % 9 == 0) return NULL;
		static std::map<tString, cMT*> mapTypes;
		cMT *&pType = mapTypes[asType];
		if(pType == NULL)
		{
			static const int vAll[] = { 0, 1, 2, 3, 5, 6, 12, 13, 14, 21, 22 };
			pType = new cMT{ h % 3 == 0, {} };
			for(int i = 0; i < 11; ++i) if((h >> i) & 1) pType->used.push_back(vAll[i]);
			if(pType->used.empty()) pType->used.push_back(0);
		}
		return pType;
	}

	cRes *MatNew(const tString &asName, const tWString &asPath, bool abBlank, void *apType)
	{
		gLog += "mat-new " + asName + " " + Narrow(asPath) + (abBlank ? " blank" : " typed") + "\n";
		cRes *p = new cRes(asName, asPath, "mat");
		p->SetHandle(++mlSerial);
		return p;
	}

	void MatSet(cRes *apMat, int alCode, int a, int b, const tString &asText)
	{
		gLog += "mat-set " + apMat->GetName() + " " + std::to_string(alCode) + " " + std::to_string(a) + " " + std::to_string(b) + " " + asText + "\n";
	}

	cRes *MatTexture(const tString &asFile, int abMip, int aType, int alKind, int alUsed)
	{
		gLog += "mat-tex " + asFile + " " + std::to_string(abMip) + " " + std::to_string(aType) + " " + std::to_string(alKind) + " " + std::to_string(alUsed) + "\n";
		if(alKind < 0 || Fails(asFile, 3)) return NULL;
		cRes *p = new cRes(asFile, _W(""), "mattex");
		p->SetHandle(++mlSerial);
		return p;
	}

	void MatTexSetup(cRes *apTex, cRes *apMat, int alUsed, int aAnim, int aWrap, float afFrame)
	{
		gLog += "mat-texsetup " + apTex->GetName() + " " + apMat->GetName() + " " + std::to_string(alUsed) + " " + std::to_string(aAnim) + " " + std::to_string(aWrap) + " " + std::to_string(afFrame) + "\n";
	}

	void MatVar(bool abClear, const tString &asName, const tString &asValue)
	{
		if(abClear) { gLog += "mat-vars-clear\n"; return; }
		gLog += "mat-var " + asName + "=" + asValue + "\n";
	}

	void MatVarsApply(void *apType, cRes *apMat) { gLog += "mat-vars-apply " + apMat->GetName() + "\n"; }

	void MatUv(cRes *apMat, int aType, int aAxis, float afSpeed, float afAmp)
	{
		gLog += "mat-uv " + apMat->GetName() + " " + std::to_string(aType) + " " + std::to_string(aAxis) + " " + std::to_string(afSpeed) + " " + std::to_string(afAmp) + "\n";
	}

	static tString TexString(int alType)
	{
		tString sOut;
		ohpl_rm_mat_texture_string(alType, [](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &sOut);
		return sOut;
	}

	// pre-port cMaterialManager::LoadFromFile
	cRes *LoadMat(const tString &asName, const tWString &asPath)
	{
		cXmlDocumentTiny *pDoc = new cXmlDocumentTiny("");
		struct cGuard { cXmlDocumentTiny *p; ~cGuard() { delete p; } } guard = { pDoc };
		if(pDoc->CreateFromFile(asPath) == false) return NULL;
		cXmlElement *pMain = pDoc->GetFirstElement("Main");
		if(pMain == NULL) { Error("Main child not found.\n"); return NULL; }
		tString sType = pMain->GetAttributeString("Type");
		if(sType == "") { Error("Type not found.\n"); return NULL; }
		bool bDepthTest = pMain->GetAttributeBool("DepthTest", true);
		tString sPhysicsMatName = pMain->GetAttributeString("PhysicsMaterial", "Default");
		tString sBlendMode = pMain->GetAttributeString("BlendMode", "Add");
		if(mbBlank)
		{
			cRes *pBlank = MatNew(asName, asPath, true, NULL);
			MatSet(pBlank, 2, 0, 0, sPhysicsMatName);
			return pBlank;
		}
		cMT *pMatType = static_cast<cMT*>(MatType(sType));
		if(pMatType == NULL) { Error("Invalid material type '%s'!\n", sType.c_str()); return NULL; }
		cRes *pMat = MatNew(asName, asPath, false, pMatType);
		MatSet(pMat, 0, bDepthTest, 0, "");
		MatSet(pMat, 1, pMain->GetAttributeBool("ForceFullScaleTextures", false), 0, "");
		MatSet(pMat, 2, 0, 0, sPhysicsMatName);
		cVector2f vSubDivs = pMain->GetAttributeVector2f("UVSubDivs", 1);
		MatSet(pMat, 3, (int)vSubDivs.x, (int)vSubDivs.y, "");
		if(pMatType->trans)
		{
			bool bWarn = false;
			int lMode = ohpl_rm_mat_blend_mode(RM_STR(sBlendMode), &bWarn);
			if(bWarn) Warning("Material BlendMode '%s' does not exist!\n", sBlendMode.c_str());
			MatSet(pMat, 4, lMode, 0, "");
		}
		cXmlElement *pTexRoot = pDoc->GetFirstElement("TextureUnits");
		if(pTexRoot == NULL) { Error("TextureUnits child not found.\n"); return NULL; }
		for(size_t i = 0; i < pMatType->used.size(); ++i)
		{
			int lUsed = pMatType->used[i];
			tString sTextureType = TexString(lUsed);
			cXmlElement *pTexChild = pTexRoot->GetFirstElement(sTextureType.c_str());
			if(pTexChild == NULL) pTexChild = pTexRoot->GetFirstElement((sTextureType + "Side").c_str());
			if(pTexChild == NULL) continue;
			int type = ohpl_rm_mat_texture_type(RM_STR(pTexChild->GetAttributeString("Type", "")));
			tString sFile = pTexChild->GetAttributeString("File", "");
			bool bMipMaps = pTexChild->GetAttributeBool("MipMaps", true);
			int wrap = ohpl_rm_mat_wrap(RM_STR(pTexChild->GetAttributeString("Wrap", "")));
			int animMode = ohpl_rm_mat_anim_mode(RM_STR(pTexChild->GetAttributeString("AnimMode", "None")));
			float fFrameTime = pTexChild->GetAttributeFloat("AnimFrameTime", 1.0f);
			if(sFile == "") continue;
			{
				tString sNewFile;
				ohpl_rm_mat_texture_file(RM_STR(sFile), reinterpret_cast<const uint32_t*>(asPath.data()), asPath.size(),
					[](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &sNewFile);
				sFile = sNewFile;
			}
			int lKind = ohpl_rm_mat_create_kind(animMode, type);
			if(lKind == 5) lKind = -1;
			cRes *pTex = MatTexture(sFile, bMipMaps, type, lKind, lUsed);
			if(pTex == NULL)
			{
				if(lUsed == 5) continue;
				delete pMat;
				return NULL;
			}
			MatTexSetup(pTex, pMat, lUsed, animMode, wrap, fFrameTime);
		}
		cXmlElement *pUvAnimRoot = pDoc->GetFirstElement("UvAnimations");
		if(pUvAnimRoot)
		{
			cXmlNodeListIterator it = pUvAnimRoot->GetChildIterator();
			while(it.HasNext())
			{
				cXmlElement *pAnimElem = it.Next()->ToElement();
				tString sT = pAnimElem->GetAttributeString("Type");
				bool bErr = false;
				int lType = ohpl_rm_mat_uv_anim_type(reinterpret_cast<const uint8_t*>(sT.c_str()), sT.size(), &bErr);
				if(bErr) Error("Invalid uv animation type %s\n", sT.c_str());
				tString sA = pAnimElem->GetAttributeString("Axis");
				bool bErr2 = false;
				int lAxis = ohpl_rm_mat_anim_axis(reinterpret_cast<const uint8_t*>(sA.c_str()), sA.size(), &bErr2);
				if(bErr2) Error("Invalid animation axis %s\n", sA.c_str());
				MatUv(pMat, lType, lAxis, pAnimElem->GetAttributeFloat("Speed", 0), pAnimElem->GetAttributeFloat("Amplitude", 0));
			}
		}
		cXmlElement *pUserVarsRoot = pDoc->GetFirstElement("SpecificVariables");
		MatVar(true, "", "");
		if(pUserVarsRoot)
		{
			std::map<tString, tString> mapVars;
			cXmlNodeListIterator it = pUserVarsRoot->GetChildIterator();
			while(it.HasNext())
			{
				cXmlElement *pVarElem = it.Next()->ToElement();
				mapVars.insert(std::make_pair(pVarElem->GetAttributeString("Name"), pVarElem->GetAttributeString("Value")));
			}
			for(auto &kv : mapVars) MatVar(false, kv.first, kv.second);
		}
		MatVarsApply(pMatType, pMat);
		gLog += "mat-compile " + pMat->GetName() + "\n";
		return pMat;
	}

	// ---------------------------------------------------------------- Destroy / Update
	void Unload(iResourceBase*) override {}
	void Destroy(iResourceBase *apRes) override
	{
		if(mpRs)
		{
			switch(mFlavor)
			{
			case fSound: ohpl_h3f_sound_destroy(mpRs, H3Ops(), apRes); break;
			case fPs: ohpl_h3f_particle_destroy(mpRs, H3Ops(), apRes); break;
			case fTex: ohpl_h3f_texture_destroy(mpRs, H3Ops(), apRes); break;
			case fImage: ohpl_h3f_image_destroy(mpRs, H3Ops(), apRes); break;
			case fShader: ohpl_h3f_gpu_destroy(mpRs, H3Ops(), apRes); break;
			case fVideo: ohpl_h3f_video_destroy(mpRs, H3Ops(), apRes); break;
			default: ohpl_h3f_destroy_standard(mpRs, H3Ops(), apRes);
			}
			return;
		}
		cRes *pRes = static_cast<cRes*>(apRes);
		switch(mFlavor)
		{
		case fSound:
			if(ohpl_rm_destroy_sound(apRes->GetMeta(), pRes->mbStream))
			{
				auto it = std::find(mlstStreams.begin(), mlstStreams.end(), pRes);
				if(it != mlstStreams.end()) { mlstStreams.erase(it); delete pRes; }
			}
			break;
		case fPs: ohpl_rm_destroy_particle(apRes->GetMeta()); break;
		case fTex:
			if(ohpl_rm_destroy_standard(apRes->GetMeta()))
			{
				mlMem -= pRes->mlMem;
				msetAnim.erase(pRes);
				RemoveResource(apRes);
				delete apRes;
			}
			break;
		case fImage: ImageDestroy(pRes); break;
		case fShader:
			apRes->DecUserCount();
			if(apRes->HasUsers() == false) { RemoveResource(apRes); delete apRes; }
			break;
		case fVideo: if(apRes) { RemoveResource(apRes); delete apRes; } break;
		default:
			if(ohpl_rm_destroy_standard(apRes->GetMeta())) { RemoveResource(apRes); delete apRes; }
		}
	}

	void Update(float afDt) override
	{
		if(mFlavor == fVideo)
		{
			if(mpRs) { ohpl_h3f_video_update(mpRs, H3Ops(), H3Bits(afDt)); return; }
			for(tResourceBaseMapIt it = Map()->begin(); it != Map()->end(); ++it) gLog += "update " + static_cast<cRes*>(it->second)->GetName() + " " + std::to_string(afDt) + "\n";
		}
		else if(mFlavor == fTex)
		{
			if(mpRs) { ohpl_h3f_texture_update(mpRs, H3Ops(), H3Bits(afDt)); return; }
			for(cRes *p : msetAnim) gLog += "update " + p->GetName() + " " + std::to_string(afDt) + "\n";
		}
	}

	void DestroyAllDispatch() { if(mFlavor == fSound) { if(mpRs) ohpl_h3f_sound_destroy_all(mpRs, H3Ops()); else SoundDestroyAll(); } else if(mFlavor != fPs) DestroyAll(); }

	void SoundDestroyAll()
	{
		for(cRes *pStream : mlstStreams)
		{
			Log("  destroying %p\n", (void*)pStream);
			Log("   file '%s'\n", cString::To8Char(pStream->GetFullPath()).c_str());
			delete pStream;
		}
		mlstStreams.clear();
		tResourceBaseMapIt it = m_mapResources.begin();
		while(it != m_mapResources.end())
		{
			iResourceBase *p = it->second;
			RemoveResource(p);
			delete p;
			it = m_mapResources.begin();
		}
	}

	// ---------------------------------------------------------------- flows (pre-port C++ bodies)
	cRes *Model(bool abMesh, const tString &asName, int alFlag)
	{
		if(mpRs)
		{
			cRmExts exts(gMeshExts);
			return H3Res<cRes>(ohpl_h3f_model(mpRs, H3Ops(), abMesh, RM_STR(asName), exts.h3(), exts.size(), alFlag));
		}
		BeginLoad(asName);
		cRmEnv env(mpFileSearcher, this);
		cRmExts exts(gMeshExts);
		cRmOut out;
		if(ohpl_rm_resolve_model(env.raw(), RM_STR(asName), exts.data(), exts.size(), out.raw()) == 0)
		{
			Error("Couldn't find %s file '%s' in any supported format!\n", abMesh ? "mesh" : "animation", asName.c_str());
			EndLoad();
			return NULL;
		}
		const tString &asNewName = out.msName;
		const tWString &sPath = out.msPath;
		cRes *pRes = static_cast<cRes*>(out.Res());
		if(pRes == NULL && sPath != _W(""))
		{
			pRes = Make(abMesh ? "mesh" : "anim", FileOf(sPath), sPath, 0);
			if(abMesh)
			{
				if(pRes == NULL) { EndLoad(); return NULL; }
				AddResource(pRes);
			}
			else if(pRes) AddResource(pRes);
		}
		if(pRes) pRes->IncUserCount();
		else Error(abMesh ? "Couldn't load mesh '%s'\n" : "Couldn't create animation '%s'\n", asNewName.c_str());
		EndLoad();
		return pRes;
	}

	cRes *Simple(int alKind, const tString &asName, int alA, int alB, int alC)
	{
		// alKind: 0 ent, 1 font, 2 script, 3 sound entity, 4 material
		if(mpRs)
		{
			switch(alKind)
			{
			case 0: return H3Res<cRes>(ohpl_h3f_ent_file(mpRs, H3Ops(), RM_STR(asName)));
			case 1: return H3Res<cRes>(ohpl_h3f_font(mpRs, H3Ops(), RM_STR(asName), alA, alB, alC));
			case 2: return H3Res<cRes>(ohpl_h3f_script(mpRs, H3Ops(), RM_STR(asName), NULL));
			case 3: return H3Res<cRes>(ohpl_h3f_sound_entity(mpRs, H3Ops(), RM_STR(asName), mbHasResolver));
			default: return H3Res<cRes>(ohpl_h3f_material(mpRs, H3Ops(), RM_STR(asName)));
			}
		}
		cRes *pRes;
		if(alKind == 3)
		{
			auto cit = mCustom.find(cString::ToLowerCase(asName));
			if(cit != mCustom.end()) { if(cit->second) cit->second->IncUserCount(); return cit->second; }
			cRmEnv env0(mpFileSearcher, this);
			if(ohpl_rm_snd_needs_resolver(env0.raw(), RM_STR(asName), mbHasResolver))
			{
				cRes *pCustom = Resolve(asName);
				mCustom[cString::ToLowerCase(asName)] = pCustom;
				if(pCustom) { pCustom->IncUserCount(); pCustom->IncUserCount(); }
				return pCustom;
			}
		}
		if(alKind == 4 && false) return NULL;
		cRmEnv env(mpFileSearcher, this);
		cRmOut out;
		if(alKind == 4)
		{
			if(ohpl_rm_resolve_simple(env.raw(), 4, RM_STR(asName), out.raw()) == 0) return NULL;
			BeginLoad(asName);
		}
		else
		{
			BeginLoad(asName);
			ohpl_rm_resolve_simple(env.raw(), alKind, RM_STR(asName), out.raw());
		}
		const tString &asNewName = out.msName;
		const tWString &sPath = out.msPath;
		pRes = static_cast<cRes*>(out.Res());
		if(pRes == NULL && sPath != _W(""))
		{
			switch(alKind)
			{
			case 0:
				pRes = Make("ent", asNewName, sPath, 0);
				if(pRes == NULL)
				{
					Error("Couldn't load entity file '%s'!\n", cString::To8Char(sPath).c_str());
					EndLoad();
					return NULL;
				}
				AddResource(pRes);
				break;
			case 1:
			{
				int lKind = ohpl_rm_font_kind(RM_STR(asName));
				gLog += "M:fontobj " + asNewName + "\n";
				if(lKind == 1 || lKind == 2)
				{
					pRes = Make(lKind == 1 ? "ttf" : "bmpfont", asNewName, sPath, 1);
					if(pRes == NULL) { EndLoad(); return NULL; }
				}
				else
				{
					Error("Font '%s' has an unkown extension!\n", asName.c_str());
					{ cRes *pF = new cRes(asNewName, sPath, "fontobj"); pF->SetHandle(++mlSerial); delete pF; }
					EndLoad();
					return NULL;
				}
				AddResource(pRes);
				break;
			}
			case 2:
				pRes = Make("script", asNewName, sPath, 0);
				if(pRes == NULL) { EndLoad(); return NULL; }
				AddResource(pRes);
				break;
			case 3:
				pRes = Make("sndent", asNewName, sPath, 0);
				if(pRes) AddResource(pRes);
				break;
			case 4:
				pRes = LoadMat(asNewName, sPath);
				if(pRes == NULL)
				{
					Error("Couldn't load material '%s'\n", asNewName.c_str());
					EndLoad();
					return NULL;
				}
				AddResource(pRes);
				break;
			}
		}
		static const char *sMsg[] = { "Couldn't create ent file '%s'\n", "Couldn't create font '%s'\n", "Couldn't create script '%s'\n", "Couldn't create SoundEntity '%s'\n", "Couldn't create material '%s'\n" };
		if(pRes) pRes->IncUserCount();
		else Error(sMsg[alKind], asNewName.c_str());
		EndLoad();
		return pRes;
	}

	void AddCustom(const tString &asName, cRes *apData)
	{
		if(mpRs) { ohpl_h3f_sound_entity_add_custom(mpRs, H3Ops(), RM_STR(asName), apData); return; }
		apData->IncUserCount();
		mCustom[cString::ToLowerCase(asName)] = apData;
	}

	void Preload(const tString &asName)
	{
		if(mpRs) { ohpl_h3f_sound_entity_preload(mpRs, H3Ops(), RM_STR(asName), mbHasResolver); return; }
		cRes *pData = Simple(3, asName, 0, 0, 0);
		if(pData == NULL) { Warning("Couldn't preload sound entity '%s'\n", asName.c_str()); return; }
		gLog += "preload " + pData->GetName() + "\n";
	}

	cRes *Video(const tString &asName)
	{
		if(mpRs) return H3Res<cRes>(ohpl_h3f_video(mpRs, H3Ops(), RM_STR(asName)));
		BeginLoad(asName);
		tWString sPath = mpFileSearcher->GetFilePath(asName);
		if(sPath == _W("")) { EndLoad(); Error("Video file '%s' could not be found!\n", asName.c_str()); return NULL; }
		cRes *pVideo = Make("video", FileOf(sPath), sPath, 0);
		if(pVideo == NULL) { EndLoad(); Error("Could not load video '%s'\n", asName.c_str()); return NULL; }
		pVideo->SetFullPath(sPath);
		AddResource(pVideo);
		EndLoad();
		return pVideo;
	}

	cRes *Sound(const tString &asName, bool abStream, bool abLoop)
	{
		if(mpRs)
		{
			cRmExts formats(gSoundFormats);
			return H3Res<cRes>(ohpl_h3f_sound(mpRs, H3Ops(), RM_STR(asName), abStream, abLoop, formats.h3(), formats.size()));
		}
		tWString sPath;
		cRes *pSound = NULL;
		BeginLoad(asName);
		cRmEnv env(mpFileSearcher, this);
		cRmExts formats(gSoundFormats);
		cRmOut out;
		if(abStream)
		{
			ohpl_rm_find_stream_path(env.raw(), RM_STR(asName), formats.data(), formats.size(), out.raw());
			sPath = out.msPath;
			if(sPath != _W(""))
			{
				pSound = Make("stream", asName, sPath, 0);
				if(pSound)
				{
					pSound->mbStream = true;
					AddResource(pSound, true, false);
					mlstStreams.push_back(pSound);
					gLog += "set-sound-mgr " + pSound->GetName() + "\n";
				}
			}
		}
		else
		{
			ohpl_rm_find_sample(env.raw(), RM_STR(asName), formats.data(), formats.size(), out.raw());
			sPath = out.msPath;
			pSound = static_cast<cRes*>(out.Res());
			if(pSound == NULL && sPath != _W(""))
			{
				pSound = Make("sample", asName, sPath, 0);
				if(pSound)
				{
					AddResource(pSound);
					gLog += "set-sound-mgr " + pSound->GetName() + "\n";
				}
			}
		}
		EndLoad();
		return pSound;
	}

	void *Particle(const tString &asName, const tString &asType, const tString &asDataName, bool abElement)
	{
		if(mpRs)
		{
			if(abElement) return ohpl_h3f_particle_by_element(mpRs, H3Ops(), RM_STR(asName), RM_STR(asDataName), NULL, 1.0f, 2.0f, 3.0f);
			return ohpl_h3f_particle_by_type(mpRs, H3Ops(), RM_STR(asName), RM_STR(asType), 1.0f, 2.0f, 3.0f);
		}
		cRes *pData = NULL;
		cRmEnv env(mpFileSearcher, this);
		if(abElement)
		{
			BeginLoad(asName);
			pData = static_cast<cRes*>(static_cast<iResourceBase*>(ohpl_rm_particle_by_data(env.raw(), RM_STR(asDataName))));
			if(pData == NULL)
			{
				pData = Make("psdata", asDataName, _W(""), 0);
				if(pData == NULL)
				{
					Error("Can't load particle system data '%s'\n", asDataName.c_str());
					EndLoad();
					return NULL;
				}
				AddResource(pData);
			}
		}
		else
		{
			cRmOut byFile;
			ohpl_rm_particle_by_file(env.raw(), RM_STR(asType), byFile.raw());
			const tString &sFile = byFile.msName;
			const tWString &sPath = byFile.msPath;
			BeginLoad(asName);
			pData = static_cast<cRes*>(byFile.Res());
			if(pData == NULL)
			{
				if(sPath == _W(""))
				{
					Error("Couldn't find particle system file '%s'\n", sFile.c_str());
					EndLoad();
					return NULL;
				}
				pData = Make("psdata", sFile, sPath, 0);
				if(pData == NULL)
				{
					Error("Can't load data from particle system file '%s'\n", cString::To8Char(sPath).c_str());
					EndLoad();
					return NULL;
				}
				AddResource(pData);
			}
		}
		pData->IncUserCount();
		gLog += "PS:" + pData->GetName() + " " + asName + " " + (abElement ? asDataName : asType) + " " + std::to_string(1.0f) + "\n";
		EndLoad();
		return &mPs;
	}

	void PsPreload(const tString &asFile)
	{
		if(mpRs) { ohpl_h3f_particle_preload(mpRs, H3Ops(), RM_STR(asFile)); return; }
		cRmEnv env(mpFileSearcher, this);
		cRmOut byFile;
		ohpl_rm_particle_by_file(env.raw(), RM_STR(asFile), byFile.raw());
		const tString &sFile = byFile.msName;
		const tWString &sPath = byFile.msPath;
		cRes *pData = static_cast<cRes*>(byFile.Res());
		if(pData == NULL)
		{
			if(sPath == _W("")) { Error("Couldn't find particle system file '%s'\n", sFile.c_str()); return; }
			cRes *pPSData = Make("psdata", sFile, sPath, 0);
			if(pPSData == NULL) { Error("Can't load data from particle system file '%s'\n", cString::To8Char(sPath).c_str()); return; }
			AddResource(pPSData);
		}
	}

	cRes *TexSimple(const tString &asName, bool abMip, int aType)
	{
		cRmExts exts(gBitmapExts);
		if(mpRs) return H3Res<cRes>(ohpl_h3f_texture_simple(mpRs, H3Ops(), RM_STR(asName), abMip, 0, aType, 0, exts.h3(), exts.size()));
		tWString sPath;
		BeginLoad(asName);
		cRmEnv env(mpFileSearcher, this);
		cRmOut out;
		ohpl_rm_find_with_formats(env.raw(), RM_STR(asName), exts.data(), exts.size(), out.raw());
		sPath = out.msPath;
		cRes *pTexture = static_cast<cRes*>(out.Res());
		if(pTexture == NULL && sPath != _W(""))
		{
			int *pBmp = Bitmap(sPath);
			if(pBmp == NULL)
			{
				Error("Texture manager Couldn't load bitmap '%s'\n", cString::To8Char(sPath).c_str());
				EndLoad();
				return NULL;
			}
			pTexture = Make("tex", asName, sPath, 2);
			if(pTexture == NULL) { delete pBmp; EndLoad(); return NULL; }
			delete pBmp;
			mlMem += pTexture->mlMem;
			AddResource(pTexture);
		}
		if(pTexture) pTexture->IncUserCount();
		else Error("Couldn't texture '%s'\n", asName.c_str());
		EndLoad();
		return pTexture;
	}

	cRes *TexAnim(const tString &asName, bool abMip)
	{
		cRmExts exts(gBitmapExts);
		if(mpRs) return H3Res<cRes>(ohpl_h3f_texture_anim(mpRs, H3Ops(), RM_STR(asName), abMip, 1, 0, 0, exts.h3(), exts.size()));
		cRmEnv env(mpFileSearcher, this);
		cRmOut firstFrame;
		ohpl_rm_anim_first_frame(env.raw(), RM_STR(asName), exts.data(), exts.size(), firstFrame.raw());
		const tString &asFirstFrameName = firstFrame.msName;
		BeginLoad(asFirstFrameName);
		cRmOut plan;
		int lPlan = ohpl_rm_anim_plan(env.raw(), RM_STR(asFirstFrameName), plan.raw());
		if(lPlan == -1) { Error("First frame of animation '%s' must contain '01'!\n", asFirstFrameName.c_str()); return NULL; }
		if(lPlan == -2) { Error("First frame of animation '%s' must contain '01' before extension!\n", asFirstFrameName.c_str()); return NULL; }
		if(lPlan == -3) { Error("First frame of animation '%s' could not be found!\n", asFirstFrameName.c_str()); return NULL; }
		if(lPlan == -4) { Error("No textures found for animation %s\n", plan.msName.c_str()); EndLoad(); return NULL; }
		cRes *pTexture = static_cast<cRes*>(plan.Res());
		if(lPlan == 1)
		{
			const tString &sBaseName = plan.msName;
			const tWString &sFakeFullPath = plan.msPath;
			const std::vector<tWString> &vPaths = plan.mvItems;
			std::vector<int*> vBitmaps;
			for(size_t i = 0; i < vPaths.size(); ++i)
			{
				int *pBmp = Bitmap(vPaths[i]);
				if(pBmp == NULL)
				{
					Error("Couldn't load bitmap '%s'!\n", cString::To8Char(vPaths[i]).c_str());
					for(int *b : vBitmaps) delete b;
					EndLoad();
					return NULL;
				}
				vBitmaps.push_back(pBmp);
			}
			pTexture = Make("texanim", sBaseName, sFakeFullPath, 2);
			if(pTexture == NULL)
			{
				Error("Couldn't create animated texture '%s'!\n", sBaseName.c_str());
				for(int *b : vBitmaps) delete b;
				EndLoad();
				return NULL;
			}
			for(int *b : vBitmaps) delete b;
			mlMem += pTexture->mlMem;
			AddResource(pTexture);
			msetAnim.insert(pTexture);
		}
		if(pTexture) pTexture->IncUserCount();
		else Error("Couldn't texture '%s'\n", asFirstFrameName.c_str());
		EndLoad();
		return pTexture;
	}

	cRes *TexCube(const tString &asPathName, bool abMip)
	{
		cRmExts exts(gBitmapExts);
		if(mpRs) return H3Res<cRes>(ohpl_h3f_texture_cube(mpRs, H3Ops(), RM_STR(asPathName), abMip, 0, 0, 3, exts.h3(), exts.size()));
		cRmEnv env(mpFileSearcher, this);
		cRmOut pre;
		if(ohpl_rm_cube_pre(env.raw(), RM_STR(asPathName), pre.raw()) == 0) return TexSimple(asPathName, abMip, 3);
		const tString &sName = pre.msName;
		const tWString &sFakeFullPath = pre.msPath;
		cRes *pTexture = static_cast<cRes*>(pre.Res());
		BeginLoad(asPathName);
		if(pTexture == NULL)
		{
			cRmOut faces;
			int lFace = ohpl_rm_cube_faces(env.raw(), RM_STR(sName), exts.data(), exts.size(), faces.raw());
			if(lFace != 0)
			{
				static const char *suf[] = { "_pos_x", "_neg_x", "_pos_y", "_neg_y", "_pos_z", "_neg_z" };
				int i = lFace - 1;
				tString sNewName = sName + suf[i];
				Error("Couldn't find %d-face '%s', for cubemap '%s' in path: '%s'\n", i, sNewName.c_str(), sName.c_str(), asPathName.c_str());
				return NULL;
			}
			const std::vector<tWString> &vPaths = faces.mvItems;
			std::vector<int*> vBitmaps;
			for(int i = 0; i < 6; i++)
			{
				int *pBmp = Bitmap(vPaths[i]);
				if(pBmp == NULL)
				{
					Error("Couldn't load bitmap '%s'!\n", cString::To8Char(vPaths[i]).c_str());
					for(int *b : vBitmaps) delete b;
					EndLoad();
					return NULL;
				}
				vBitmaps.push_back(pBmp);
			}
			pTexture = Make("texcube", sName, sFakeFullPath, 2);
			if(pTexture == NULL)
			{
				Error("Couldn't create cubemap '%s'!\n", sName.c_str());
				for(int *b : vBitmaps) delete b;
				EndLoad();
				return NULL;
			}
			for(int *b : vBitmaps) delete b;
			mlMem += pTexture->mlMem;
			AddResource(pTexture);
		}
		if(pTexture) pTexture->IncUserCount();
		else Error("Couldn't texture '%s'\n", sName.c_str());
		EndLoad();
		return pTexture;
	}

	void SetBlank(bool abX)
	{
		mbBlank = abX;
		if(mpRs) ohpl_h3f_material_blank(mpRs, &mOps, abX);
	}

	int MemUsage() { return mpRs ? ohpl_h3f_mem_usage(mpRs, &mOps) : mlMem; }

	// image
	cBF *CreateBitmapFrame(int w, int h)
	{
		cBF *pBFrame = NewBitmapFrame(w, h, mlFrameHandle);
		mlstBitmapFrames.push_back(pBFrame);
		auto ret = mmapTextureFrames.insert(std::make_pair(mlFrameHandle, pBFrame->tf));
		if(ret.second == false) Error("Could not add texture frame %d with handle %d! Handle already exist!\n", (int)(intptr_t)pBFrame->tf, mlFrameHandle);
		mlFrameHandle++;
		return pBFrame;
	}

	cRes *AddToFrame(int *apBmp, const tWString &asPath, int alFrameHandle)
	{
		bool bFound = false;
		cRes *pImage = NULL;
		if(mlstBitmapFrames.size() == 0) CreateBitmapFrame(mvFrameSize.x, mvFrameSize.y);
		if(alFrameHandle < 0)
		{
			for(auto it = mlstBitmapFrames.begin(); it != mlstBitmapFrames.end(); it++)
			{
				cBF *pFrame = *it;
				if((pFrame->imgs >= pFrame->cap) == false && pFrame->locked == false)
				{
					pImage = AddBitmap(pFrame, apBmp, asPath);
					if(pImage == NULL && pFrame->additions > 1)
					{
						gLog += "reorganize " + std::to_string(pFrame->handle) + "\n";
						pFrame->additions = 0;
						pImage = AddBitmap(pFrame, apBmp, asPath);
					}
					if(pImage != NULL) { bFound = true; break; }
				}
			}
			if(!bFound)
			{
				int vx = mvFrameSize.x, vy = mvFrameSize.y;
				int outx, outy;
				ohpl_rm_image_frame_size(vx, vy, *apBmp % 97 * 7 + 1, *apBmp % 89 * 11 + 1, &outx, &outy);
				cBF *pFrame = CreateBitmapFrame(outx, outy);
				if(pFrame)
				{
					pImage = AddBitmap(pFrame, apBmp, asPath);
					if(pImage == NULL) Log("No fit in new frame!\n");
				}
			}
		}
		else
		{
			for(auto it = mlstBitmapFrames.begin(); it != mlstBitmapFrames.end(); ++it)
				if((*it)->handle == alFrameHandle) { pImage = AddBitmap(*it, apBmp, asPath); break; }
			if(pImage == NULL) Error("Image didn't fit frame %d!\n", alFrameHandle);
		}
		return pImage;
	}

	cRes *ImageCreate(const tString &asName, int alFrameHandle)
	{
		cRmExts exts(gBitmapExts);
		if(mpRs) return H3Res<cRes>(ohpl_h3f_image_create_in_frame(mpRs, H3Ops(), RM_STR(asName), alFrameHandle, exts.h3(), exts.size()));
		cRes *pImage = NULL;
		tWString sPath;
		BeginLoad(asName);
		cRmEnv env(mpFileSearcher, this);
		cRmOut out;
		ohpl_rm_find_with_formats(env.raw(), RM_STR(asName), exts.data(), exts.size(), out.raw());
		sPath = out.msPath;
		pImage = static_cast<cRes*>(out.Res());
		if(!pImage)
		{
			if(sPath != _W(""))
			{
				int *pBmp = Bitmap(sPath);
				if(pBmp == NULL)
				{
					Error("Imagemanager Couldn't load bitmap '%s'\n", cString::To8Char(sPath).c_str());
					EndLoad();
					return NULL;
				}
				pImage = AddToFrame(pBmp, sPath, alFrameHandle);
				delete pBmp;
				if(pImage == NULL) Error("Imagemanager couldn't create image '%s'\n", asName.c_str());
				if(pImage) AddResource(pImage);
			}
		}
		if(pImage) pImage->IncUserCount();
		else Error("Couldn't load image '%s'\n", asName.c_str());
		EndLoad();
		return pImage;
	}

	cRes *ImageFromBitmap(int *apBmp, int alFrameHandle)
	{
		if(mpRs) return H3Res<cRes>(ohpl_h3f_image_from_bitmap(mpRs, H3Ops(), apBmp, alFrameHandle));
		if(apBmp == NULL) return NULL;
		cRes *pImage = AddToFrame(apBmp, _W(""), alFrameHandle);
		if(pImage) { AddResource(pImage, false); pImage->IncUserCount(); }
		return pImage;
	}

	void ImageDestroy(cRes *pImage)
	{
		cTF *pFrame = static_cast<cTF*>(pImage->mpFrame);
		cBF *pBmpFrame = static_cast<cBF*>(pImage->mpBmpFrame);
		bool bDelete = ohpl_rm_destroy_standard(pImage->GetMeta());
		if(bDelete)
		{
			pFrame->pics--;
			if(pBmpFrame) pBmpFrame->imgs--;
			RemoveResource(pImage);
			delete pImage;
		}
		uint32_t lAfter = ohpl_rm_destroy_image_frame(pFrame->pics <= 0, pFrame->custom, pBmpFrame != NULL);
		if(lAfter & 2)
		{
			if(lAfter & 1)
			{
				for(auto it = mlstBitmapFrames.begin(); it != mlstBitmapFrames.end(); ++it)
				{
					cBF *pTest = *it;
					if(pTest->tf == pFrame)
					{
						mlstBitmapFrames.erase(it);
						gLog += "del-bf " + std::to_string(pTest->handle) + "\n";
						delete pTest;
						break;
					}
				}
			}
			mmapTextureFrames.erase(pFrame->handle);
			gLog += "del-tf " + std::to_string(pFrame->handle) + "\n";
			delete pFrame;
		}
	}

	int FlushAll()
	{
		if(mpRs) return ohpl_h3f_image_flush_all(mpRs, H3Ops());
		int n = 0;
		for(cBF *p : mlstBitmapFrames) if(p->handle % 2) n++;
		return n;
	}

	void SetLocked(int alHandle, bool abLocked)
	{
		if(mpRs) { ohpl_h3f_image_set_frame_locked(mpRs, H3Ops(), alHandle, abLocked); return; }
		for(cBF *p : mlstBitmapFrames) if(p->handle == alHandle) { p->locked = abLocked; break; }
	}

	int CreateFrame(int w, int h)
	{
		if(mpRs) return ohpl_h3f_image_create_frame(mpRs, H3Ops(), w, h);
		return CreateBitmapFrame(w, h)->handle;
	}

	// shader
	cRes *Shader(const tString &asName, int aType, bool abVars, bool abTranspile)
	{
		cRes *pShader;
		if(mpRs) return H3Res<cRes>(ohpl_h3f_gpu_shader(mpRs, H3Ops(), RM_STR(asName), aType, abVars ? &mVars : NULL, abTranspile, aType == 1));
		BeginLoad(asName);
		if(abVars)
		{
			bool bIsHpslFallback = false;
			tString sHpslName;
			tWString sPath = mpFileSearcher->GetFilePath(asName);
			if(sPath == _W("") && abTranspile)
			{
				tString sOut;
				ohpl_shader_fallback_name(asName.data(), asName.size(), [](void *c, const char *p, size_t n){ static_cast<tString*>(c)->append(p, n); }, &sHpslName);
				tWString sHpslPath = mpFileSearcher->GetFilePath(sHpslName);
				if(sHpslPath != _W("")) { sPath = sHpslPath; bIsHpslFallback = true; }
			}
			if(sPath == _W("")) { Error("Couldn't find file '%s' in resources\n", asName.c_str()); EndLoad(); return NULL; }
			gLog += "S:parse " + asName + " " + sHpslName + " 0" + std::to_string(bIsHpslFallback ? 1 : 0) + " " + Narrow(sPath) + "\n";
			msParsed = "parsed:" + asName + ":" + sHpslName;
			if(bIsHpslFallback)
			{
				gLog += "S:transpile " + asName + "\n";
				if(Fails(asName, 4))
				{
					Error("Couldn't transpile HPSL shader '%s' (from '%s'): %s\n", asName.c_str(), sHpslName.c_str(), ("transpile failed " + asName).c_str());
					EndLoad();
					return NULL;
				}
			}
			pShader = Make("shstr", asName, sPath, 6);
			if(pShader == NULL) { Error("Couldn't create program '%s'\n", asName.c_str()); EndLoad(); return NULL; }
			int lBits = (bIsHpslFallback ? 1 : 0) | (aType == 1 ? 2 : 0);
			gLog += "S:post " + pShader->GetName() + " " + std::to_string(lBits) + "\n";
		}
		else
		{
			tWString sPath;
			pShader = static_cast<cRes*>(FindLoadedResource(asName, sPath));
			if(pShader == NULL && sPath != _W(""))
			{
				pShader = Make("shfile", asName, sPath, 5);
				if(pShader == NULL) { Error("Couldn't create program '%s'\n", asName.c_str()); EndLoad(); return NULL; }
				AddResource(pShader);
			}
			else if(pShader == NULL && sPath == _W("") && abTranspile)
			{
				tString sHpslName;
				ohpl_shader_fallback_name(asName.data(), asName.size(), [](void *c, const char *p, size_t n){ static_cast<tString*>(c)->append(p, n); }, &sHpslName);
				tWString sHpslPath = mpFileSearcher->GetFilePath(sHpslName);
				if(sHpslPath != _W(""))
				{
					gLog += "S:parse " + asName + " " + sHpslName + " 11 " + Narrow(sHpslPath) + "\n";
					gLog += "S:transpile " + asName + "\n";
					if(Fails(asName, 4) == false)
					{
						pShader = Make("shstr", asName, sHpslPath, 6);
						if(pShader == NULL) Error("Couldn't create program '%s' (from transpiled HPSL '%s')\n", asName.c_str(), sHpslName.c_str());
						else
						{
							gLog += "S:post " + pShader->GetName() + " 1\n";
							AddResource(pShader);
						}
					}
					else Error("Couldn't transpile HPSL shader '%s' (from '%s'): %s\n", asName.c_str(), sHpslName.c_str(), ("transpile failed " + asName).c_str());
				}
			}
			if(pShader) pShader->IncUserCount();
			else Error("Couldn't load program '%s'\n", asName.c_str());
		}
		EndLoad();
		return pShader;
	}

	std::vector<iResourceBase*> Live()
	{
		std::vector<iResourceBase*> v;
		cResourceBaseIterator it = GetResourceBaseIterator();
		while(it.HasNext()) v.push_back(it.Next());
		return v;
	}

	std::string Dump()
	{
		std::string s;
		for(iResourceBase *p : Live()) s += Narrow(p->GetFullPath()) + ":" + std::to_string(p->GetHandle()) + ":" + std::to_string(p->GetUserCount()) + ";";
		return std::to_string(Live().size()) + " " + Hex(Fnv(s));
	}

	eFlavor mFlavor;
	unsigned long mlSerial = 0, mlOp;
	int mlMem;
	int mlFrameHandle, mlImg = 0;
	cVector2l mvFrameSize;
	bool mbHasResolver;
	bool mbBlank = false;
	std::set<cRes*> msetAnim;
	std::list<cRes*> mlstStreams;
	std::map<tString, cRes*> mCustom;
	std::list<cBF*> mlstBitmapFrames;
	std::map<int, cTF*> mmapTextureFrames;
	int mPs = 0;
	int mVars = 0;
	tString msParsed, msErr;
};

int main(int argc, char **argv)
{
	if(argc < 3) return 2;
	std::string root = argv[1];
	long ops = atol(argv[2]);

	cFileSearcher searcher;
	searcher.AddDirectory(cString::To16Char(root), "*.*", true);
	iResourceBase::SetLogCreateAndDelete(true);
	const char *sExtra = getenv("FLOW_EXTRA_DIR");
	if(sExtra) searcher.AddDirectory(cString::To16Char(sExtra), "*.*", true);

	std::vector<std::string> names, snames, mnames;
	{
		std::vector<std::string> rels;
		std::error_code ec;
		for(fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
			if(!ec && it->is_regular_file(ec)) rels.push_back(it->path().lexically_relative(root).string());
		std::sort(rels.begin(), rels.end());
		static const char *kinds[] = { "png", "jpg", "dds", "tga", "bmp", "dae", "msh", "fbx", "ent", "hps", "chps", "ttf", "fnt", "snt", "mat", "ogg", "wav", "ps", "ogv", "glsl", "hpsl" };
		size_t stride = std::max<size_t>(1, rels.size() / 4000);
		for(size_t i = 0; i < rels.size(); i += stride)
		{
			fs::path p(rels[i]);
			std::string ext = p.extension().string();
			if(!ext.empty()) ext = ext.substr(1);
			bool bKnown = false;
			for(const char *k : kinds) if(ext == k) bKnown = true;
			if(!bKnown) continue;
			std::string base = p.filename().string(), parent = p.parent_path().filename().string();
			names.push_back(base);
			if(ext == "hpsl") { names.push_back(p.stem().string() + ".glsl"); names.push_back(parent + "/" + p.stem().string() + ".glsl"); snames.push_back(p.stem().string() + ".glsl"); snames.push_back(base); }
			if(ext == "glsl") snames.push_back(base);
			names.push_back(p.stem().string());
			if(!parent.empty()) names.push_back(parent + "/" + base);
			std::string up = base;
			for(char &c : up) c = toupper((unsigned char)c);
			names.push_back(up);
		}
		if(sExtra)
		{
			for(fs::directory_iterator it(sExtra), end; it != end; ++it) mnames.push_back(it->path().filename().string());
			std::sort(mnames.begin(), mnames.end());
			for(const std::string &sM : mnames) for(int r = 0; r < 6; ++r) names.push_back(sM);
		}
		names.push_back("does_not_exist.png");
		names.push_back("anim01.png");
	}
	if(names.empty()) return 3;

	uint64_t seed = 88172645463325252ull;
	auto Rnd = [&](uint64_t n) { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed % n; };
	auto Name = [&]() { return names[Rnd(names.size())]; };

	std::string transcript;
	uint64_t total = 1469598103934665603ull;
	{
		std::vector<cProbeMgr*> mgrs;
		for(int f = 0; f < fCount; ++f) mgrs.push_back(new cProbeMgr(&searcher, (eFlavor)f));
		mgrs[fSndEnt]->mbHasResolver = true;
		int lImgBmpSeed = 0;
		for(long op = 0; op < ops; ++op)
		{
			gLog.clear();
			eFlavor f = (eFlavor)Rnd(fCount);
			cProbeMgr &m = *mgrs[f];
			m.mlOp = op;
			gTick = op;
			std::string line = std::string(gFlavorName[f]) + " ";
			uint64_t k = Rnd(100);
			std::vector<iResourceBase*> live = m.Live();
			if(k < 50)
			{
				tString n = Name();
				switch(f)
				{
				case fAnim: line += "create " + n + " -> " + m.Id(m.Model(false, n, 0)); break;
				case fMesh: line += "create " + n + " -> " + m.Id(m.Model(true, n, (int)Rnd(4))); break;
				case fEnt: line += "create " + n + " -> " + m.Id(m.Simple(0, n, 0, 0, 0)); break;
				case fScript: line += "create " + n + " -> " + m.Id(m.Simple(2, n, 0, 0, 0)); break;
				case fFont: line += "create " + n + " -> " + m.Id(m.Simple(1, n, 12, 32, 255)); break;
				case fSndEnt:
					if(Rnd(4) == 0) { m.Preload(n); line += "preload " + n; }
					else line += "create " + n + " -> " + m.Id(m.Simple(3, n, 0, 0, 0));
					break;
				case fMat:
					if(Rnd(12) == 0) { bool bB = Rnd(2); m.SetBlank(bB); line += "blank " + std::to_string(bB) + " "; }
					if(!mnames.empty() && Rnd(2)) n = mnames[Rnd(mnames.size())];
					line += "create " + n + " -> " + m.Id(m.Simple(4, n, 0, 0, 0));
					break;
				case fSound: { bool bStream = Rnd(2); bool bLoop = Rnd(2); line += "create " + n + " -> " + m.Id(m.Sound(n, bStream, bLoop)); break; }
				case fVideo: line += "create " + n + " -> " + m.Id(m.Video(n)); break;
				case fPs:
				{
					uint64_t v = Rnd(3);
					if(v == 0) { m.PsPreload(n); line += "preload " + n; }
					else line += std::string(v == 1 ? "type " : "data ") + n + " -> " + std::to_string((long)(m.Particle("ps" + std::to_string(Rnd(5)), n, n, v == 2) != NULL));
					break;
				}
				case fTex:
				{
					uint64_t v = Rnd(4);
					if(v == 0) line += "anim " + n + " -> " + m.Id(m.TexAnim(n, Rnd(2)));
					else if(v == 1) line += "cube " + n + " -> " + m.Id(m.TexCube(n, Rnd(2)));
					else line += "simple " + n + " -> " + m.Id(m.TexSimple(n, Rnd(2), 1));
					line += " mem=" + std::to_string(m.MemUsage());
					break;
				}
				case fImage:
				{
					uint64_t v = Rnd(8);
					if(v == 0) { int *pBmp = new int(lImgBmpSeed += 37); line += "frombmp -> " + m.Id(m.ImageFromBitmap(pBmp, Rnd(3) ? -1 : (int)Rnd(4))); delete pBmp; }
					else if(v == 1) { int w = 64 + (int)Rnd(300); int h = 64 + (int)Rnd(300); line += "createframe " + std::to_string(m.CreateFrame(w, h)); }
					else if(v == 2) { int h = (int)Rnd(5); m.SetLocked(h, Rnd(2)); line += "lock " + std::to_string(h); }
					else if(v == 3) line += "flush " + std::to_string(m.FlushAll());
					else line += "create " + n + " -> " + m.Id(m.ImageCreate(n, Rnd(4) ? -1 : (int)Rnd(4)));
					break;
				}
				case fShader:
				{
					bool bVars = Rnd(2);
					if(!snames.empty() && Rnd(4)) n = snames[Rnd(snames.size())];
					int lType = (int)Rnd(2); bool bTranspile = Rnd(3) != 0;
					line += std::string(bVars ? "vars " : "plain ") + n + " -> " + m.Id(m.Shader(n, lType, bVars, bTranspile));
					break;
				}
				default: break;
				}
			}
			else if(k < 70 && !live.empty())
			{
				iResourceBase *p = live[Rnd(live.size())];
				if(p->HasUsers()) p->DecUserCount();
				line += "release " + m.Id(p) + " users=" + std::to_string(p->GetUserCount());
				if(Rnd(2)) { std::string id = m.Id(p); m.Destroy(p); line += " destroy " + id + " live=" + std::to_string(m.Live().size()); }
			}
			else if(k < 76) { int keep = (int)Rnd(12); if(f != fTex) m.DestroyUnused(keep); line += "unused " + std::to_string(keep) + " " + m.Dump(); }
			else if(k < 84) { m.Update(0.016f * (float)(op % 5)); line += "update"; }
			else if(k < 90 && !live.empty())
			{
				iResourceBase *p = live[Rnd(live.size())];
				while(p->HasUsers()) p->DecUserCount();
				std::string id = m.Id(p);
				m.Destroy(p);
				line += "force " + id + " live=" + std::to_string(m.Live().size());
			}
			else line += m.Dump();
			{
				std::vector<std::string> vUpd;
				std::string sRest;
				size_t pos = 0, firstUpd = std::string::npos;
				while(pos < gLog.size())
				{
					size_t e = gLog.find('\n', pos);
					std::string l = gLog.substr(pos, e - pos + 1);
					if(l.compare(0, 7, "update ") == 0) { vUpd.push_back(l); if(firstUpd == std::string::npos) firstUpd = sRest.size(); }
					else sRest += l;
					pos = e + 1;
				}
				std::sort(vUpd.begin(), vUpd.end());
				std::string sUpd;
				for(auto &l : vUpd) sUpd += l;
				gLog = firstUpd == std::string::npos ? sRest : sRest.substr(0, firstUpd) + sUpd + sRest.substr(firstUpd);
			}
			std::string out = std::regex_replace(gLog, std::regex(" in -?[0-9]+ ms"), " in N ms");
			out = std::regex_replace(out, std::regex("0x[0-9a-f]+"), "0xP");
			line += "\n" + out;
			total = Fnv(line, total);
			if(argc > 3) transcript += line;
		}
		for(int f = 0; f < fCount; ++f)
		{
			gLog.clear();
			cProbeMgr &m = *mgrs[f];
			m.DestroyAllDispatch();
			m.Shutdown();
			std::string line = std::string("final ") + gFlavorName[f] + " " + std::to_string(m.Live().size()) + "\n" + std::regex_replace(gLog, std::regex("0x[0-9a-f]+"), "0xP");
			total = Fnv(line, total);
			if(argc > 3) transcript += line;
		}
	}
	printf("ops=%ld digest=%s\n", ops, Hex(total).c_str());
	if(argc > 3) std::ofstream(argv[3]) << transcript;
	return 0;
}
