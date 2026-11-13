// H5b harness: LowLevelGraphicsImmediate.inl (the C++ draw helpers and CPU batch) compiled against a
// recording class whose IM_* primitives write one text line each; the seeded Rust draw scenario drives it
// through the facade call entry. usage: H5B_SEED=<n> H5B_CALLS=<n> H5B_OUT=<file> h5b_imm
#include <assert.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "graphics/GraphicsTypes.h"
#include "graphics/RustGraphicsH5b.h"
#include "math/Math.h"
#include "resources/GpuShaderManager.h"
#include "system/PreprocessParser.h"

using namespace hpl;

namespace hpl { class iTexture; }

class cImmRec
{
public:
	cImmRec() : mlBatchArraySize(20000), mlBatchStride(13), mlVertexCount(0), mlIndexCount(0)
	{
		mVertex.assign(mlBatchStride * mlBatchArraySize, 0.0f);
		mIndex.assign(mlBatchArraySize, 0);
		mpVertexArray = mVertex.data();
		mpIndexArray = mIndex.data();
		for(int i = 0; i < kMaxTextureUnits; ++i)
		{
			mTex[i].assign(3 * mlBatchArraySize, 0.0f);
			mpTexCoordArray[i] = mTex[i].data();
			mlTexCoordArrayCount[i] = 0;
		}
	}

	void DrawTriangle(tVertexVec &avVtx);
	void DrawQuad(const cVector3f &avPos, const cVector2f &avSize, const cColor &aColor);
	void DrawQuad(const cVector3f &avPos, const cVector2f &avSize, const cVector2f &avMinTexCoord, const cVector2f &avMaxTexCoord, const cColor &aColor);
	void DrawQuad(const cVector3f &avPos, const cVector2f &avSize, const cVector2f &avMinTexCoord0, const cVector2f &avMaxTexCoord0,
				  const cVector2f &avMinTexCoord1, const cVector2f &avMaxTexCoord1, const cColor &aColor);
	void DrawQuad(const tVertexVec &avVtx);
	void DrawQuad(const tVertexVec &avVtx, const cColor aCol);
	void DrawQuad(const tVertexVec &avVtx, const float afZ);
	void DrawQuad(const tVertexVec &avVtx, const float afZ, const cColor &aCol);
	void DrawQuadMultiTex(const tVertexVec &avVtx, const tVector3fVec &avExtraUvs);
	void DrawLine(const cVector3f &avBegin, const cVector3f &avEnd, cColor aCol);
	void DrawLine(const cVector3f &avBegin, const cColor &aBeginCol, const cVector3f &avEnd, const cColor &aEndCol);
	void DrawBoxMinMax(const cVector3f &avMin, const cVector3f &avMax, cColor aCol);
	void DrawSphere(const cVector3f &avPos, float afRadius, cColor aCol);
	void DrawSphere(const cVector3f &avPos, float afRadius, cColor aColX, cColor aColY, cColor aColZ);
	void DrawLineQuad(const cRect2f &aRect, float afZ, cColor aCol);
	void DrawLineQuad(const cVector3f &avPos, const cVector2f &avSize, cColor aCol);
	void AddVertexToBatch(const cVertex *apVtx);
	void AddVertexToBatch(const cVertex *apVtx, const cVector3f *avTransform);
	void AddVertexToBatch(const cVertex *apVtx, const cMatrixf *aMtx);
	void AddVertexToBatch_Size2D(const cVertex *apVtx, const cVector3f *avTransform, const cColor *apCol, const float &mfW, const float &mfH);
	void AddVertexToBatch_Raw(const cVector3f &avPos, const cColor &aColor, const cVector3f &avTex);
	void AddIndexToBatch(int alIndex);
	void AddTexCoordToBatch(unsigned int alUnit, const cVector3f *apCoord);

	void SetTexture(unsigned int, iTexture*) { Line("settex0"); }
	void SetBlendActive(bool) { Line("blend0"); }
	void SetBatchTextureUnitActive(unsigned int alUnit, bool abActive) { char b[64]; snprintf(b, sizeof(b), "unit %u %d", alUnit, abActive ? 1 : 0); Line(b); }
	void Flush(const char *asKind, unsigned alFlags, bool abAuto);
	void ClearBatch() { mlIndexCount = 0; mlVertexCount = 0; }

	void Line(const std::string &asS) { mOut += asS; mOut += '\n'; }
	static std::string F(float afX) { uint32_t u; memcpy(&u, &afX, 4); char b[16]; snprintf(b, sizeof(b), "f:%08x", u); return b; }
	template<class... A> void Pr(const char *asFmt, A... a)
	{
		char b[256]; snprintf(b, sizeof(b), asFmt, a...); Line(b);
	}

	std::string mOut;
	int mlBatchArraySize, mlBatchStride, mlVertexCount, mlIndexCount;
	float *mpVertexArray;
	unsigned int *mpIndexArray;
	float *mpTexCoordArray[kMaxTextureUnits];
	int mlTexCoordArrayCount[kMaxTextureUnits];
	std::vector<float> mVertex;
	std::vector<unsigned int> mIndex;
	std::vector<float> mTex[kMaxTextureUnits];
};

#define IM_CLASS cImmRec
#define IM_BEGIN(m) Pr("begin %u", (unsigned)(m))
#define IM_END() Line("end")
#define IM_VERTEX3(x, y, z) Line("v3 " + F(x) + " " + F(y) + " " + F(z))
#define IM_COLOR4(r, g, b, a) Line("c4 " + F(r) + " " + F(g) + " " + F(b) + " " + F(a))
#define IM_TEXCOORD2(s, t) Line("t2 " + F(s) + " " + F(t))
#define IM_TEXCOORD3(s, t, r) Line("t3 " + F(s) + " " + F(t) + " " + F(r))
#define IM_MULTITEXCOORD2(u, s, t) Line("mt2 " + std::to_string((unsigned)(u)) + " " + F(s) + " " + F(t))
#define IM_MULTITEXCOORD3(u, s, t, r) Line("mt3 " + std::to_string((unsigned)(u)) + " " + F(s) + " " + F(t) + " " + F(r))
#define IM_QUADS 7
#define IM_LINES 1
#define IM_LINE_STRIP 3
#define IM_TRIANGLES 4
#define IM_TEXTURE0 0x84C0
#define IM_TEXTURE1 0x84C1
#include "impl/LowLevelGraphicsImmediate.inl"

static uint64_t Fnv(const void *apData, size_t alSize)
{
	const unsigned char *p = static_cast<const unsigned char*>(apData);
	uint64_t h = 1469598103934665603ull;
	for(size_t i = 0; i < alSize; ++i) { h ^= p[i]; h *= 1099511628211ull; }
	return h;
}

void cImmRec::Flush(const char *asKind, unsigned alFlags, bool abAuto)
{
	char b[256];
	snprintf(b, sizeof(b), "flush %s %u %d v=%d:%016llx i=%d:%016llx", asKind, alFlags, abAuto ? 1 : 0, mlVertexCount,
			 (unsigned long long)Fnv(mpVertexArray, sizeof(float) * mlVertexCount), mlIndexCount,
			 (unsigned long long)Fnv(mpIndexArray, sizeof(unsigned) * mlIndexCount));
	std::string s = b;
	for(int u = 0; u < kMaxTextureUnits; ++u)
	{
		if(mlTexCoordArrayCount[u] == 0) continue;
		snprintf(b, sizeof(b), " tc%d=%d:%016llx", u, mlTexCoordArrayCount[u], (unsigned long long)Fnv(mpTexCoordArray[u], sizeof(float) * mlTexCoordArrayCount[u]));
		s += b;
	}
	Line(s);
	if(abAuto)
	{
		mlIndexCount = 0;
		mlVertexCount = 0;
		for(int i = 0; i < kMaxTextureUnits; ++i) mlTexCoordArrayCount[i] = 0;
	}
}

namespace {
	class cRead
	{
	public:
		cRead(const uint64_t *apA, size_t alN) : mpA(apA), mlN(alN), mlI(0) {}
		uint64_t U() { return mlI < mlN ? mpA[mlI++] : 0; }
		int I() { return (int)(int64_t)U(); }
		unsigned Un() { return (unsigned)U(); }
		float F() { uint32_t u = (uint32_t)U(); float f; memcpy(&f, &u, 4); return f; }
		bool B() { return (U() & 1) != 0; }
		template<class T> T* P() { return reinterpret_cast<T*>((uintptr_t)U()); }
		cVector2f V2() { float x = F(); float y = F(); return cVector2f(x, y); }
		cVector3f V3() { float x = F(); float y = F(); float z = F(); return cVector3f(x, y, z); }
		cColor C() { float r = F(); float g = F(); float b = F(); float a = F(); return cColor(r, g, b, a); }
		cMatrixf M() { cMatrixf m; memcpy(m.v, P<const float>(), sizeof(float) * 16); return m; }
		static cVertex Vtx(const float *p)
		{
			cVertex v;
			v.pos = cVector3f(p[0], p[1], p[2]);
			v.tex = cVector3f(p[3], p[4], p[5]);
			v.tan = cVector3f(p[6], p[7], p[8]);
			v.norm = cVector3f(p[9], p[10], p[11]);
			v.col = cColor(p[12], p[13], p[14], p[15]);
			return v;
		}
		cVertex V1() { return Vtx(P<const float>()); }
		tVertexVec Vs()
		{
			const float *p = P<const float>(); size_t n = (size_t)U();
			tVertexVec v;
			for(size_t i = 0; i < n; ++i) v.push_back(Vtx(p + i * 16));
			return v;
		}
	private:
		const uint64_t *mpA; size_t mlN, mlI;
	};

	cImmRec gRec;

	extern "C" uint64_t ImmCall(void*, uint32_t op, const uint64_t *a, size_t n, uint64_t*)
	{
		cRead r(a, n);
		cImmRec *g = &gRec;
		switch(op)
		{
		case kOhplH5b_DRAW_TRIANGLE: { tVertexVec v = r.Vs(); g->DrawTriangle(v); break; }
		case kOhplH5b_DRAW_QUAD_POS: { cVector3f p = r.V3(); cVector2f s = r.V2(); g->DrawQuad(p, s, r.C()); break; }
		case kOhplH5b_DRAW_QUAD_UV: { cVector3f p = r.V3(); cVector2f s = r.V2(); cVector2f x = r.V2(); cVector2f y = r.V2(); g->DrawQuad(p, s, x, y, r.C()); break; }
		case kOhplH5b_DRAW_QUAD_UV2:
		{
			cVector3f p = r.V3(); cVector2f s = r.V2(); cVector2f a0 = r.V2(); cVector2f b0 = r.V2(); cVector2f a1 = r.V2(); cVector2f b1 = r.V2();
			g->DrawQuad(p, s, a0, b0, a1, b1, r.C());
			break;
		}
		case kOhplH5b_DRAW_QUAD_VTX: { tVertexVec v = r.Vs(); g->DrawQuad(v); break; }
		case kOhplH5b_DRAW_QUAD_VTX_COL: { tVertexVec v = r.Vs(); g->DrawQuad(v, r.C()); break; }
		case kOhplH5b_DRAW_QUAD_VTX_Z: { tVertexVec v = r.Vs(); g->DrawQuad(v, r.F()); break; }
		case kOhplH5b_DRAW_QUAD_VTX_Z_COL: { tVertexVec v = r.Vs(); float z = r.F(); cColor c = r.C(); g->DrawQuad(v, z, c); break; }
		case kOhplH5b_DRAW_QUAD_MULTITEX:
		{
			tVertexVec v = r.Vs(); const float *p = r.P<const float>(); size_t cnt = (size_t)r.U();
			tVector3fVec e;
			for(size_t i = 0; i < cnt; ++i) e.push_back(cVector3f(p[i * 3], p[i * 3 + 1], p[i * 3 + 2]));
			g->DrawQuadMultiTex(v, e);
			break;
		}
		case kOhplH5b_DRAW_LINE: { cVector3f x = r.V3(); cVector3f y = r.V3(); g->DrawLine(x, y, r.C()); break; }
		case kOhplH5b_DRAW_LINE_COLS: { cVector3f x = r.V3(); cColor xc = r.C(); cVector3f y = r.V3(); cColor yc = r.C(); g->DrawLine(x, xc, y, yc); break; }
		case kOhplH5b_DRAW_BOX_MIN_MAX: { cVector3f x = r.V3(); cVector3f y = r.V3(); g->DrawBoxMinMax(x, y, r.C()); break; }
		case kOhplH5b_DRAW_SPHERE: { cVector3f p = r.V3(); float rad = r.F(); g->DrawSphere(p, rad, r.C()); break; }
		case kOhplH5b_DRAW_SPHERE_AXES: { cVector3f p = r.V3(); float rad = r.F(); cColor x = r.C(); cColor y = r.C(); cColor z = r.C(); g->DrawSphere(p, rad, x, y, z); break; }
		case kOhplH5b_DRAW_LINE_QUAD_RECT: { cColor q = r.C(); float z = r.F(); cColor c = r.C(); g->DrawLineQuad(cRect2f(q.r, q.g, q.b, q.a), z, c); break; }
		case kOhplH5b_DRAW_LINE_QUAD_POS: { cVector3f p = r.V3(); cVector2f s = r.V2(); g->DrawLineQuad(p, s, r.C()); break; }
		case kOhplH5b_ADD_VERTEX_TO_BATCH: { cVertex v = r.V1(); g->AddVertexToBatch(&v); break; }
		case kOhplH5b_ADD_VERTEX_TO_BATCH_TRANSLATED: { cVertex v = r.V1(); cVector3f t = r.V3(); g->AddVertexToBatch(&v, &t); break; }
		case kOhplH5b_ADD_VERTEX_TO_BATCH_MATRIX: { cVertex v = r.V1(); cMatrixf m = r.M(); g->AddVertexToBatch(&v, &m); break; }
		case kOhplH5b_ADD_VERTEX_TO_BATCH_SIZE2D:
		{
			cVertex v = r.V1(); cVector3f t = r.V3(); bool has = r.B(); cColor c = r.C(); float w = r.F(), h = r.F();
			static const cColor kBlank(0, 0);
			g->AddVertexToBatch_Size2D(&v, &t, has ? &c : &kBlank, w, h);
			break;
		}
		case kOhplH5b_ADD_VERTEX_TO_BATCH_RAW: { cVector3f p = r.V3(); cColor c = r.C(); cVector3f t = r.V3(); g->AddVertexToBatch_Raw(p, c, t); break; }
		case kOhplH5b_ADD_INDEX_TO_BATCH: g->AddIndexToBatch(r.I()); break;
		case kOhplH5b_ADD_TEXCOORD_TO_BATCH: { unsigned u = r.Un(); cVector3f c = r.V3(); g->AddTexCoordToBatch(u, &c); break; }
		case kOhplH5b_SET_BATCH_TEXTURE_UNIT_ACTIVE: { unsigned u = r.Un(); g->SetBatchTextureUnitActive(u, r.B()); break; }
		case kOhplH5b_FLUSH_TRI_BATCH: { unsigned f = r.Un(); g->Flush("tri", f, r.B()); break; }
		case kOhplH5b_FLUSH_QUAD_BATCH: { unsigned f = r.Un(); g->Flush("quad", f, r.B()); break; }
		case kOhplH5b_CLEAR_BATCH: g->ClearBatch(); break;
		}
		return 0;
	}
}

// Link-time hook of the shared build wrapper; unused here.
extern "C" iGpuShader* __wrap__ZN3hpl17cGpuShaderManager12CreateShaderERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_14eGpuShaderTypeEPNS_19cParserVarContainerE(
	void*, const tString&, eGpuShaderType, cParserVarContainer*)
{
	return NULL;
}

int hplMain(const tString&)
{
	const char *sSeed = getenv("H5B_SEED"), *sCalls = getenv("H5B_CALLS"), *sOut = getenv("H5B_OUT");
	if(!sSeed || !sCalls || !sOut) { fprintf(stderr, "set H5B_SEED, H5B_CALLS, H5B_OUT\n"); return 2; }
	ohpl_h5b_run_draws(NULL, &ImmCall, strtoull(sSeed, NULL, 10), (uint32_t)atoi(sCalls));
	FILE *f = fopen(sOut, "wb");
	if(f == NULL) return 1;
	fwrite(gRec.mOut.data(), 1, gRec.mOut.size(), f);
	fclose(f);
	return 0;
}
