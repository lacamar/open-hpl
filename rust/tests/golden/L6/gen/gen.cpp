// L6 harness. Links the engine (libHPL2.a); loads through the Rust loaders and prints the full output.
//   gen <msh|dae|fbx|anm|daeanim|fbxanim> <file>...
// prints "<file> <all> <vtx> <idx> <bone> <anim> <names> <xf> <log>" (FNV-1a 64) or "<file> FAIL <log>".
// Materials are not loaded (CreateMaterial is replaced); their names are hashed.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"
#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"
#include "graphics/VertexBuffer.h"
#include "impl/VertexBufferOGL_VBO.h"
#include "impl/MeshLoaderMSH.h"
#include "impl/MeshLoaderCollada.h"
#include "impl/MeshLoaderAssimp.h"
#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"
#include "resources/AnimationManager.h"
#include "scene/Node3D.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "resources/Resources.h"
#include "impl/LowLevelGraphicsSDL.h"

using namespace hpl;

static std::string gLog;
static std::vector<std::string> gMaterials;

static void LogCb(eLogOutputType t, const char* m)
{
	std::string line = m;
	size_t at = line.find(" in buffer ");
	if(at != std::string::npos) line = line.substr(0, at);
	gLog += std::to_string((int)t) + ":" + line + "\n";
}

namespace hpl {
	cMaterial* cMaterialManager::CreateMaterial(const tString& asName)
	{
		gMaterials.push_back(asName);
		return NULL;
	}
}

class HVb : public cVertexBufferOGL_VBO
{
public:
	HVb(iLowLevelGraphics* l, eVertexBufferDrawType d, eVertexBufferUsageType u, int v, int i)
		: cVertexBufferOGL_VBO(l, d, u, v, i) {}
	void UpdateData(tVertexElementFlag, bool) {}
	void Bind() {}
	void UnBind() {}
	void Draw(eVertexBufferDrawType) {}
	void DrawIndices(unsigned int*, int, eVertexBufferDrawType) {}
private:
	void CompileSpecific() {}
};

class HLow : public cLowLevelGraphicsSDL
{
public:
	iVertexBuffer* CreateVertexBuffer(eVertexBufferType, eVertexBufferDrawType d, eVertexBufferUsageType u, int v, int i)
	{
		return new HVb(this, d, u, v, i);
	}
};

struct Hash
{
	uint64_t h = 1469598103934665603ull;
	void Bytes(const void* p, size_t n)
	{
		const unsigned char* b = (const unsigned char*)p;
		for(size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
	}
	void I(int64_t v) { Bytes(&v, 8); }
	void F(float v) { Bytes(&v, 4); }
	void Fs(const float* p, size_t n) { for(size_t i = 0; i < n; ++i) F(p[i]); }
	void M(const cMatrixf& m) { Fs(m.v, 16); }
	void V3(const cVector3f& v) { F(v.x); F(v.y); F(v.z); }
	void S(const std::string& s) { I((int64_t)s.size()); Bytes(s.data(), s.size()); }
};

static std::string Hex(uint64_t v) { char b[20]; snprintf(b, sizeof b, "%016llx", (unsigned long long)v); return b; }

struct Sums { Hash all, vtx, idx, bone, anim, names, xf; };

static void HashAnim(Sums& s, cAnimation* a)
{
	Hash& h = s.anim;
	if(getenv("GEN_DUMP")) fprintf(stderr, "anim '%s' len=%a tracks=%d\n", a->GetAnimationName(), a->GetLength(), a->GetTrackNum());
	h.S(a->GetAnimationName());
	h.F(a->GetLength());
	h.I(a->GetTrackNum());
	for(int t = 0; t < a->GetTrackNum(); ++t)
	{
		cAnimationTrack* tr = a->GetTrack(t);
		if(getenv("GEN_DUMP")) fprintf(stderr, "track %s flags=%d unit=%a keys=%d\n", tr->GetName().c_str(), (int)tr->GetTransformFlags(), tr->GetUnitScale(), tr->GetKeyFrameNum());
		h.S(tr->GetName());
		h.I(tr->GetTransformFlags());
		h.F(tr->GetUnitScale());
		h.I(tr->GetKeyFrameNum());
		for(int k = 0; k < tr->GetKeyFrameNum(); ++k)
		{
			cKeyFrame* f = tr->GetKeyFrame(k);
			if(getenv("GEN_DUMP")) fprintf(stderr, "key %s %d t=%a tr=%a,%a,%a q=%a,%a,%a,%a\n", tr->GetName().c_str(), k, f->time, f->trans.x, f->trans.y, f->trans.z, f->rotation.v.x, f->rotation.v.y, f->rotation.v.z, f->rotation.w);
			h.F(f->time);
			h.V3(f->trans);
			h.F(f->rotation.v.x); h.F(f->rotation.v.y); h.F(f->rotation.v.z); h.F(f->rotation.w);
		}
	}
}

static void HashMesh(Sums& s, cMesh* m)
{
	s.names.S(m->GetName());
	s.names.S(cString::To8Char(m->GetFullPath()));
	cSkeleton* sk = m->GetSkeleton();
	s.bone.I(sk != NULL);
	if(sk)
	{
		s.bone.I(sk->mbFileUnits);
		s.bone.I(sk->GetBoneNum());
		for(int i = 0; i < sk->GetBoneNum(); ++i)
		{
			cBone* b = sk->GetBoneByIndex(i);
			s.bone.S(b->GetName());
			s.bone.S(b->GetSid());
			s.bone.M(b->GetLocalTransform());
			s.bone.M(b->GetInvBindTransform());
			s.bone.F(b->GetLocalUnitScale());
			s.bone.I(b->GetValue());
			int parent = -1;
			for(int j = 0; j < sk->GetBoneNum(); ++j) if(sk->GetBoneByIndex(j) == b->GetParent()) parent = j;
			s.bone.I(parent);
			s.bone.F(m->GetBoneBoundingRadius(i));
		}
	}
	s.xf.I(m->GetNodeNum());
	for(int i = 0; i < m->GetNodeNum(); ++i)
	{
		cNode3D* n = m->GetNode(i);
		s.names.S(n->GetName());
		s.xf.M(n->GetLocalMatrix());
		s.xf.I(n->GetCustomFlags());
	}
	s.xf.I(m->GetSubMeshNum());
	for(int i = 0; i < m->GetSubMeshNum(); ++i)
	{
		cSubMesh* sm = m->GetSubMesh(i);
		s.names.S(sm->GetName());
		s.names.S(sm->GetMaterialName());
		s.xf.M(sm->GetLocalTransform());
		s.xf.V3(sm->GetModelScale());
		s.xf.I(sm->IsCollideShape());
		s.xf.I(sm->GetMaterial() != NULL);
		s.xf.I(sm->GetDoubleSided());
		s.xf.I(sm->GetColliderNum());
		for(int c = 0; c < sm->GetColliderNum(); ++c)
		{
			cMeshCollider* mc = sm->GetCollider(c);
			s.xf.I(mc->mType);
			s.xf.V3(mc->mvSize);
			s.xf.M(mc->m_mtxOffset);
			s.xf.I(mc->mbCharCollider);
		}
		s.bone.I(sm->GetVertexBonePairNum());
		for(int p = 0; p < sm->GetVertexBonePairNum(); ++p)
		{
			cVertexBonePair& bp = sm->GetVertexBonePair(p);
			s.bone.I(bp.vtxIdx); s.bone.I(bp.boneIdx); s.bone.F(bp.weight);
		}
		iVertexBuffer* vb = sm->GetVertexBuffer();
		s.vtx.I(vb != NULL);
		if(!vb) continue;
		s.vtx.I(vb->GetVertexElementFlags());
		s.vtx.I(vb->GetVertexNum());
		for(int e = 0; e < eVertexBufferElement_LastEnum; ++e)
		{
			eVertexBufferElement el = (eVertexBufferElement)e;
			int n = vb->GetElementNum(el);
			if(n == 0) continue;
			eVertexBufferElementFormat f = vb->GetElementFormat(el);
			s.vtx.I(e); s.vtx.I(n); s.vtx.I(f); s.vtx.I(vb->GetElementProgramVarIndex(el));
			size_t cnt = (size_t)vb->GetVertexNum() * n;
			if(f == eVertexBufferElementFormat_Float) s.vtx.Bytes(vb->GetFloatArray(el), cnt * 4);
			else if(f == eVertexBufferElementFormat_Int) s.vtx.Bytes(vb->GetIntArray(el), cnt * 4);
			else s.vtx.Bytes(vb->GetByteArray(el), cnt);
		}
		s.idx.I(vb->GetIndexNum());
		s.idx.Bytes(vb->GetIndices(), (size_t)vb->GetIndexNum() * 4);
	}
	s.anim.I(m->GetAnimationNum());
	for(int i = 0; i < m->GetAnimationNum(); ++i) HashAnim(s, m->GetAnimation(i));
}

struct HMsh : cMeshLoaderMSH
{
	HMsh(iLowLevelGraphics* l) : cMeshLoaderMSH(l)
	{
		mpMaterialManager = (cMaterialManager*)calloc(1, 65536);
		mpMeshManager = (cMeshManager*)calloc(1, 65536);
		mpAnimationManager = (cAnimationManager*)calloc(1, 65536);
	}
};

struct HCol : cMeshLoaderCollada
{
	HCol(iLowLevelGraphics* l, cMeshLoaderMSH* m, bool cache) : cMeshLoaderCollada(l, m, cache)
	{
		mpMaterialManager = (cMaterialManager*)calloc(1, 65536);
		mpMeshManager = (cMeshManager*)calloc(1, 65536);
		mpAnimationManager = (cAnimationManager*)calloc(1, 65536);
	}
};

struct HAss : cMeshLoaderAssimp
{
	HAss(iLowLevelGraphics* l) : cMeshLoaderAssimp(l)
	{
		mpMaterialManager = (cMaterialManager*)calloc(1, 65536);
		mpMeshManager = (cMeshManager*)calloc(1, 65536);
		mpAnimationManager = (cAnimationManager*)calloc(1, 65536);
	}
};

int hplMain(const std::string&) { return 0; }

static std::string FileBytesHash(const std::string& path)
{
	FILE* f = fopen(path.c_str(), "rb");
	if(!f) return "none";
	Hash h;
	char buf[65536];
	size_t n;
	while((n = fread(buf, 1, sizeof buf, f)) > 0) h.Bytes(buf, n);
	fclose(f);
	return Hex(h.h);
}

int main(int argc, char** argv)
{
	if(argc < 3) { fprintf(stderr, "usage: gen <msh|dae|fbx|anm|daeanim|fbxanim> <file>... | -\n"); return 2; }
	std::string kind = argv[1];
	SetLogMessageCallback(LogCb);
	HLow low;
	HMsh msh(&low);
	HCol col(&low, &msh, false);
	HAss ass(&low);
	std::vector<std::string> files;
	for(int a = 2; a < argc; ++a)
	{
		if(strcmp(argv[a], "-") != 0) { files.push_back(argv[a]); continue; }
		char line[8192];
		while(fgets(line, sizeof line, stdin))
		{
			size_t n = strlen(line);
			while(n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
			if(n) files.push_back(line);
		}
	}
	for(size_t a = 0; a < files.size(); ++a)
	{
		gLog.clear();
		gMaterials.clear();
		tWString file = cString::To16Char(files[a]);
		Sums s;
		bool ok = false;
		if(kind == "daecache")
		{
			// cold load writes the cache, warm load reads it back; prints both hashes and the cache file hash
			std::string dir = getenv("GEN_CACHE_DIR");
			cResources::SetMeshCacheDir(cString::To16Char(dir));
			std::string flat = files[a];
			for(size_t i = 0; i < flat.size(); ++i) if(flat[i] == '/' || flat[i] == '\\' || flat[i] == ':') flat[i] = '_';
			std::string cache = dir + flat + ".v5.msh";
			remove(cache.c_str());
			HCol cc(&low, &msh, true);
			std::string line;
			for(int pass = 0; pass < 2; ++pass)
			{
				Sums t;
				gMaterials.clear();
				cMesh* m = cc.LoadMesh(file, 0);
				if(m) HashMesh(t, m);
				for(size_t i = 0; i < gMaterials.size(); ++i) t.names.S(gMaterials[i]);
				Hash all;
				for(Hash* h : { &t.vtx, &t.idx, &t.bone, &t.anim, &t.names, &t.xf }) all.I((int64_t)h->h);
				line += (m ? Hex(all.h) : "FAIL") + " ";
				if(pass == 0) line += FileBytesHash(cache) + " ";
			}
			Hash lg;
			lg.S(gLog);
			printf("%s %s%s\n", files[a].c_str(), line.c_str(), Hex(lg.h).c_str());
			continue;
		}
		if(kind == "msh" || kind == "dae" || kind == "fbx")
		{
			iMeshLoader* l = kind == "msh" ? (iMeshLoader*)&msh : kind == "dae" ? (iMeshLoader*)&col : (iMeshLoader*)&ass;
			cMesh* m = l->LoadMesh(file, 0);
			if(m) { HashMesh(s, m); ok = true; }
		}
		else
		{
			iMeshLoader* l = kind == "anm" ? (iMeshLoader*)&msh : kind == "daeanim" ? (iMeshLoader*)&col : (iMeshLoader*)&ass;
			cAnimation* an = l->LoadAnimation(file);
			if(an) { HashAnim(s, an); ok = true; }
		}
		if(getenv("GEN_LOG") && !gLog.empty()) fprintf(stderr, "%s: %s", files[a].c_str(), gLog.c_str());
		Hash lg;
		lg.S(gLog);
		for(size_t i = 0; i < gMaterials.size(); ++i) s.names.S(gMaterials[i]);
		if(!ok) { printf("%s FAIL %s\n", files[a].c_str(), Hex(lg.h).c_str()); continue; }
		Hash all;
		for(Hash* h : { &s.vtx, &s.idx, &s.bone, &s.anim, &s.names, &s.xf }) all.I((int64_t)h->h);
		printf("%s %s %s %s %s %s %s %s %s\n", files[a].c_str(), Hex(all.h).c_str(), Hex(s.vtx.h).c_str(), Hex(s.idx.h).c_str(),
			Hex(s.bone.h).c_str(), Hex(s.anim.h).c_str(), Hex(s.names.h).c_str(), Hex(s.xf.h).c_str(), Hex(lg.h).c_str());
	}
	return 0;
}
