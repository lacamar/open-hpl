// Records the pre-port decisions of cHpmWorldBuilder (the original C++ expressions, copied verbatim)
// over every .hpm map, plus a synthetic corpus for the combine/collider/batch decisions.
//   h4_probe maps ROOT MAPLIST   -> "map<TAB>lines<TAB>fnv64" per map
//   h4_probe synth               -> "synth<TAB>cases<TAB>fnv64"
#include "resources/HpmBuilder.h"
#include "resources/XmlDocument.h"
#include "math/Math.h"
#include "system/String.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace hpl;

static uint64_t gHash;
static size_t gLines;
static void Hash(const std::string& s)
{
	if (getenv("H4_DUMP")) fprintf(stderr, "%s\n", s.c_str());
	for (unsigned char c : s) gHash = (gHash ^ c) * 1099511628211ull;
	gHash = (gHash ^ '\n') * 1099511628211ull;
	++gLines;
}
static std::string X(float f)
{
	uint32_t u;
	memcpy(&u, &f, 4);
	char b[16];
	snprintf(b, sizeof b, "%08x", u);
	return b;
}
static std::string M(const cMatrixf& m)
{
	std::string s;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c) s += X(m.m[r][c]) + (r == 3 && c == 3 ? "" : ",");
	return s;
}
static std::string V(float a, float b, float c) { return X(a) + "," + X(b) + "," + X(c); }
static std::string I3(int a, int b, int c) { return std::to_string(a) + "," + std::to_string(b) + "," + std::to_string(c); }

class cProbe : public iHpmBuilder
{
public:
	uint64_t NowMs() { return 0; }
	void ObjectDone() {}
	void Warn(const char*) {}
	void Log(const char*) {}
	int ReadFile(const tWString& asPath, std::vector<char>& avData)
	{
		FILE* f = fopen(cString::To8Char(asPath).c_str(), "rb");
		if (!f) return 0;
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		bool ok = true;
		if (n > 0)
		{
			avData.resize(n);
			ok = fread(avData.data(), n, 1, f) == 1;
		}
		fclose(f);
		return ok ? 1 : 2;
	}
	void GlobalFog(const OhplHpmFog&) {}
	void DistanceCull(const OhplHpmDistanceCull&) {}
	void DirLight(const OhplHpmDirLight&) {}
	void EnvParticles(const OhplHpmEnvParticles&) {}
	void PostEffects(const OhplHpmPostEffects&) {}
	void SkyBox(const OhplHpmSkyBox&) {}

	bool StaticObject(const OhplHpmStaticObject& o)
	{
		cVector3f vPosition(o.position[0], o.position[1], o.position[2]);
		cVector3f vScale(o.scale[0], o.scale[1], o.scale[2]);
		cVector3f vRotation(o.rotation[0], o.rotation[1], o.rotation[2]);
		cMatrixf mtxWorld = cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ), cMath::MatrixScale(vScale));
		cMatrixf mtxTransform = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
		mtxTransform.SetTranslation(vPosition);
		bool bPoseApplied = o.hasPose && (o.id & 1) == 0;
		bool bCand = (o.hasPose == false || bPoseApplied == false);
		cVector3f vCell = vPosition / 16.0f;
		int lMesh[3] = {(int)floorf(vCell.x), (int)floorf(vCell.y), (int)floorf(vCell.z)};
		int lBody[3] = {(int)std::floor(vPosition.x / 8), (int)std::floor(vPosition.y / 8), (int)std::floor(vPosition.z / 24)};
		Hash("static|" + std::string(o.name) + "|" + o.filename + "|" + M(mtxWorld) + "|" + M(mtxTransform) + "|" + (bCand ? "1" : "0") + "|" +
			 I3(lMesh[0], lMesh[1], lMesh[2]) + "|" + I3(lBody[0], lBody[1], lBody[2]));
		return true;
	}
	bool Plane(const OhplHpmPlane& p)
	{
		cVector3f vPosition(p.position[0], p.position[1], p.position[2]);
		cVector3f vScale(p.scale[0], p.scale[1], p.scale[2]);
		cVector3f vRotation(p.rotation[0], p.rotation[1], p.rotation[2]);
		cMatrixf mtxWorld = cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ), cMath::MatrixScale(vScale));
		cMatrixf mtxBody = cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ);
		mtxBody.SetTranslation(vPosition);
		Hash("plane|" + std::string(p.name) + "|" + p.material + "|" + M(mtxWorld) + "|" + M(mtxBody));
		return true;
	}
	bool Entity(const OhplHpmEntity& e, cXmlElement*)
	{
		cMatrixf m = cMath::MatrixRotate(cVector3f(e.rotation[0], e.rotation[1], e.rotation[2]), eEulerRotationOrder_XYZ);
		m.SetTranslation(cVector3f(e.position[0], e.position[1], e.position[2]));
		Hash("entity|" + std::string(e.name) + "|" + e.filename + "|" + M(m));
		return true;
	}
	bool Light(cXmlElement*) { Hash("light"); return true; }
	bool Area(const OhplHpmArea& a, cXmlElement*)
	{
		cMatrixf m = cMath::MatrixRotate(cVector3f(a.rotation[0], a.rotation[1], a.rotation[2]), eEulerRotationOrder_XYZ);
		m.SetTranslation(cVector3f(a.position[0], a.position[1], a.position[2]));
		Hash("area|" + std::string(a.name) + "|" + a.areaType + "|" + M(m));
		return true;
	}
	bool Sound(cXmlElement*, bool) { Hash("sound"); return true; }
	bool Decal(const OhplHpmDecal&, cXmlElement*) { Hash("decal"); return true; }
	bool Billboard(cXmlElement*) { Hash("billboard"); return true; }
	bool ParticleSystem(cXmlElement*, const char*) { Hash("ps"); return true; }
	bool FogArea(cXmlElement*) { Hash("fogarea"); return true; }
	bool LensFlare(cXmlElement*) { Hash("lensflare"); return true; }
	void LightMaskBox(const OhplHpmLightMask& m)
	{
		cMatrixf mtxRot = cMath::MatrixRotate(cVector3f(m.rotation[0], m.rotation[1], m.rotation[2]), eEulerRotationOrder_XYZ);
		cVector3f vHalf = cVector3f(m.size[0], m.size[1], m.size[2]) * cVector3f(m.scale[0], m.scale[1], m.scale[2]) * 0.5f;
		cVector3f vExtent;
		for (int i = 0; i < 3; ++i)
			vExtent.v[i] = 2.0f * (std::fabs(mtxRot.m[i][0]) * vHalf.x + std::fabs(mtxRot.m[i][1]) * vHalf.y + std::fabs(mtxRot.m[i][2]) * vHalf.z);
		Hash("mask|" + std::to_string(m.id) + "|" + V(m.worldPos[0], m.worldPos[1], m.worldPos[2]) + "|" + V(vExtent.x, vExtent.y, vExtent.z));
	}
	void ConnectLights(int* a, int* b) { *a = 0; *b = 0; }
	void ConnectLightMasks() {}
	int DetailMeshes(const OhplHpmDetailMeshes& d)
	{
		float fMeshRadius = 0;
		for (int i = 0; i < d.num; ++i)
		{
			if (fMeshRadius == 0)
			{
				cVector3f vMin(1e9f), vMax(-1e9f);
				for (int j = 0; j < 4 && j < d.num; ++j)
				{
					float r = j < (int)d.numRadii ? d.radii[j] : 1.0f;
					vMin = cMath::Vector3Min(vMin, cVector3f(-r * (j + 1), -r, -r * 0.5f));
					vMax = cMath::Vector3Max(vMax, cVector3f(r, r * 2, r));
				}
				fMeshRadius = cMath::Max(vMin.Length(), vMax.Length());
			}
			float fScale = i < (int)d.numRadii && fMeshRadius > 0 ? d.radii[i] / fMeshRadius : 1;
			cQuaternion qRot(d.rotations[i * 4], d.rotations[i * 4 + 1], d.rotations[i * 4 + 2], d.rotations[i * 4 + 3]);
			cMatrixf mtxTransform = cMath::MatrixMul(cMath::MatrixQuaternion(qRot), cMath::MatrixScale(fScale));
			mtxTransform.SetTranslation(cVector3f(d.positions[i * 3], d.positions[i * 3 + 1], d.positions[i * 3 + 2]));
			Hash("detail|" + std::string(d.file) + "|" + std::to_string(d.firstIndex + i) + "|" + X(fMeshRadius) + "|" + X(fScale) + "|" + M(mtxTransform));
		}
		return d.num;
	}
	void ExposureArea(const OhplHpmExposureArea& a)
	{
		cMatrixf mtxTransform = cMath::MatrixRotate(cVector3f(a.rotation[0], a.rotation[1], a.rotation[2]), eEulerRotationOrder_XYZ);
		mtxTransform.SetTranslation(cVector3f(a.worldPos[0], a.worldPos[1], a.worldPos[2]));
		cMatrixf inv = cMath::MatrixInverse(mtxTransform);
		cVector3f half = cVector3f(a.scale[0], a.scale[1], a.scale[2]) * 0.5f;
		Hash("expo|" + std::string(a.name) + "|" + M(inv) + "|" + V(half.x, half.y, half.z));
	}
	void Terrain(const OhplHpmTerrain&) { Hash("terrain"); }
	void TrackReport(const char*, int, int, int, bool, const OhplHpmSkip*, size_t) {}
};

struct cRng
{
	uint64_t s = 0x9e3779b97f4a7c15ull;
	uint32_t Next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return (uint32_t)(s >> 33); }
};

// The original grouping of cHpmWorldBuilder::CombineStaticMeshes, over synthetic candidates.
static void Synth()
{
	cRng r;
	typedef std::tuple<int, int, int, const void*, bool, bool, std::vector<int>, std::array<float, 9>, bool, bool> tKey;
	const float kVals[3] = {0.0f, 0.5f, 1.0f};
	for (int c = 0; c < 400; ++c)
	{
		int n = 1 + r.Next() % 40, m = 1 + r.Next() % 8;
		std::map<tKey, std::vector<std::pair<int, int> > > mapGroups;
		std::vector<int> subCount(m, 0);
		for (int i = 0; i < n; ++i)
		{
			int ent = r.Next() % m;
			++subCount[ent];
			int lCell[3] = {(int)(r.Next() % 3) - 1, (int)(r.Next() % 2), (int)(r.Next() % 2)};
			const void* mat = (const void*)(uintptr_t)((r.Next() % 3 + 1) * 0x1000);
			bool sh = r.Next() % 2, os = r.Next() % 2;
			std::vector<int> lay;
			for (int k = r.Next() % 4; k > 0; --k) lay.push_back(r.Next() % 3);
			std::array<float, 9> v;
			for (auto& x : v) x = kVals[r.Next() % 3];
			bool d = r.Next() % 2, f = r.Next() % 2;
			tKey key(lCell[0], lCell[1], lCell[2], mat, sh, os, lay, v, d, f);
			mapGroups[key].push_back({ent, i});
		}
		std::vector<int> subNum(m);
		for (int e = 0; e < m; ++e) subNum[e] = subCount[e] + (r.Next() % 4 == 0);
		std::string line = "combine|" + std::to_string(n) + "|";
		std::map<int, int> mapCombined;
		int lCombined = 0;
		for (auto& [key, vSubs] : mapGroups)
		{
			if (vSubs.size() < 2) continue;
			line += "g" + std::to_string(vSubs.size()) + ":";
			for (auto& [e, i] : vSubs)
			{
				line += std::to_string(i) + ",";
				++mapCombined[e];
				++lCombined;
			}
		}
		line += "|d:";
		for (int e = 0; e < m; ++e)
		{
			auto it = mapCombined.find(e);
			line += (it != mapCombined.end() && it->second == subNum[e]) ? "1" : "0";
		}
		line += "|" + std::to_string(lCombined);
		Hash(line);
	}
	const char* names[] = {"", "body", "_collider", "_collider_1", "_charcollider", "_charcollider2", "_noncharcollider", "_noncharcollider_a", "_col",
						   "_charcol", "_Collider", "x_collider", "_noncharcollide"};
	for (const char* n : names)
	{
		tString s = n;
		bool bHas = cString::Sub(s, 0, 9) == "_collider";
		bool bChar = cString::Sub(s, 0, 13) == "_charcollider";
		bool bCol = bChar || cString::Sub(s, 0, 9) == "_collider";
		bool bHide = bCol || cString::Sub(s, 0, 16) == "_noncharcollider";
		Hash(std::string("name|") + n + "|" + (bHas ? "1" : "0") + (bChar ? "1" : "0") + (bCol ? "1" : "0") + (bHide ? "1" : "0"));
		for (int hasCol = 0; hasCol < 2; ++hasCol)
			for (int mask = 0; mask < 8; ++mask)
				for (int nc = 0; nc <= 3; ++nc)
				{
					bool bTriangles = !hasCol;
					for (int j = 0; j < nc; ++j)
						if (((mask >> j) & 1) == 0) bTriangles = false;
					Hash("batches|" + std::to_string(hasCol) + std::to_string(mask) + std::to_string(nc) + std::to_string(bCol) + "|" + ((bTriangles || bCol) ? "1" : "0"));
				}
	}
	for (int i = 0; i < 4000; ++i)
	{
		float x = ((int)(r.Next() % 4000) - 2000) / 7.0f, y = ((int)(r.Next() % 4000) - 2000) / 3.0f, z = ((int)(r.Next() % 4000) - 2000) / 11.0f;
		cVector3f vc = cVector3f(x, y, z) / 16.0f;
		Hash("cell|" + I3((int)floorf(vc.x), (int)floorf(vc.y), (int)floorf(vc.z)) + "|" + I3((int)std::floor(x / 8), (int)std::floor(y / 8), (int)std::floor(z / 24)));
		size_t cur = r.Next() % 60000, add = r.Next() % 20000;
		Hash(std::string("flush|") + (cur + add > 50000 ? "1" : "0"));
	}
}

int main(int argc, char** argv)
{
	if (argc >= 2 && strcmp(argv[1], "synth") == 0)
	{
		gHash = 14695981039346656037ull;
		gLines = 0;
		Synth();
		printf("synth\t%zu\t%016llx\n", gLines, (unsigned long long)gHash);
		return 0;
	}
	if (argc < 4) return 1;
	std::string root = argv[2];
	FILE* l = fopen(argv[3], "r");
	char buf[4096];
	while (l && fgets(buf, sizeof buf, l))
	{
		std::string rel = buf;
		while (!rel.empty() && (rel.back() == '\n' || rel.back() == '\r')) rel.pop_back();
		gHash = 14695981039346656037ull;
		gLines = 0;
		cProbe p;
		ParseHpmMap(p, cString::To16Char(root + "/" + rel));
		printf("%s\t%zu\t%016llx\n", rel.c_str(), gLines, (unsigned long long)gHash);
	}
	return 0;
}
