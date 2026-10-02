#include "SomaScriptNatives.h"
#include "SomaScriptBind.h"
#include "SomaLuxScriptable.h"

#include "graphics/Color.h"
#include "math/Math.h"
#include "math/MathTypes.h"
#include "math/Quaternion.h"
#include "impl/scriptarray.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <map>
#include <set>
#include <set>
#include "SomaImGui.h"
#include <string>

using namespace hpl;

struct cSomaVector4f
{
	float x, y, z, w;
	cSomaVector4f operator+(const cSomaVector4f &o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
	cSomaVector4f operator-(const cSomaVector4f &o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
	cSomaVector4f operator*(const cSomaVector4f &o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
	cSomaVector4f operator/(const cSomaVector4f &o) const { return {x / o.x, y / o.y, z / o.z, w / o.w}; }
	cSomaVector4f operator+(float f) const { return {x + f, y + f, z + f, w + f}; }
	cSomaVector4f operator-(float f) const { return {x - f, y - f, z - f, w - f}; }
	cSomaVector4f operator*(float f) const { return {x * f, y * f, z * f, w * f}; }
	cSomaVector4f operator/(float f) const { return {x / f, y / f, z / f, w / f}; }
	bool operator==(const cSomaVector4f &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
	float SqrLength() const { return x * x + y * y + z * z + w * w; }
	float Length() const { return sqrtf(SqrLength()); }
	float Normalize()
	{
		float l = Length();
		if (l > 0)
			*this = *this / l;
		return l;
	}
};

static std::set<std::string> gsetNativeBehaviourTypes = {"cImGuiGfx", "cImGuiFont", "cVector2f", "cVector3f", "cVector4f", "cVector2l", "cVector3l",
														  "cColor", "cMatrixf", "cQuaternion", "cPidControllerVec3", "cPidControllerf",
														  "cRect2f", "cRect2l", "cPlanef", "cDate", "cScriptStringSet"};

bool SomaScriptHasNativeBehaviours(const char *apType)
{
	return gsetNativeBehaviourTypes.count(apType) != 0;
}

//---------------------------------------
// Vector operators, shared by all vector types

template <class V> static V &Elem(V &v, asQWORD i) { return v; }

template <class V> static V SomaCatmullRom(const V &p0, const V &p1, const V &p2, const V &p3, float t)
{
	float t2 = t * t, t3 = t2 * t;
	return ((p1 * 2) + (p2 - p0) * t + (p0 * 2 - p1 * 5 + p2 * 4 - p3) * t2 + (p1 * 3 - p0 - p2 * 3 + p3) * t3) * 0.5f;
}

template <class V, class S> static S GetElem(const V &v, asQWORD i) { return ((const S *)&v)[i]; }
template <class V, class S> static void SetElem(const V &v, asQWORD i, S f) { ((S *)&v)[i] = f; }

#define VEC_OPS(V, S, NAME, SNAME)                                                                                                     \
	SOMA_METHOD(e, NAME, NAME "&opAssign(const " NAME " &in)", +[](V &a, const V &b) -> V & { return a = b; });                          \
	SOMA_METHOD(e, NAME, NAME " &opAddAssign(const " NAME " &in)", +[](V &a, const V &b) -> V & { return a = a + b; });                   \
	SOMA_METHOD(e, NAME, NAME " &opSubAssign(const " NAME " &in)", +[](V &a, const V &b) -> V & { return a = a - b; });                   \
	SOMA_METHOD(e, NAME, NAME " &opMulAssign(const " NAME " &in)", +[](V &a, const V &b) -> V & { return a = a * b; });                   \
	SOMA_METHOD(e, NAME, NAME " &opDivAssign(const " NAME " &in)", +[](V &a, const V &b) -> V & { return a = a / b; });                   \
	SOMA_METHOD(e, NAME, "bool opEquals(const " NAME " &in) const", +[](const V &a, const V &b) { return a == b; });                      \
	SOMA_METHOD(e, NAME, NAME " opAdd(const " NAME " &in) const", +[](const V &a, const V &b) -> V { return a + b; });                    \
	SOMA_METHOD(e, NAME, NAME " opSub(const " NAME " &in) const", +[](const V &a, const V &b) -> V { return a - b; });                    \
	SOMA_METHOD(e, NAME, NAME " opMul(const " NAME " &in) const", +[](const V &a, const V &b) -> V { return a * b; });                    \
	SOMA_METHOD(e, NAME, NAME " opDiv(const " NAME " &in) const", +[](const V &a, const V &b) -> V { return a / b; });                    \
	SOMA_METHOD(e, NAME, NAME " &opAddAssign(" SNAME ")", +[](V &a, S f) -> V & { return a = a + f; });                                  \
	SOMA_METHOD(e, NAME, NAME " &opSubAssign(" SNAME ")", +[](V &a, S f) -> V & { return a = a - f; });                                  \
	SOMA_METHOD(e, NAME, NAME " &opMulAssign(" SNAME ")", +[](V &a, S f) -> V & { return a = a * f; });                                  \
	SOMA_METHOD(e, NAME, NAME " &opDivAssign(" SNAME ")", +[](V &a, S f) -> V & { return a = a / f; });                                  \
	SOMA_METHOD(e, NAME, NAME " opAdd(" SNAME ") const", +[](const V &a, S f) -> V { return a + f; });                                   \
	SOMA_METHOD(e, NAME, NAME " opSub(" SNAME ") const", +[](const V &a, S f) -> V { return a - f; });                                   \
	SOMA_METHOD(e, NAME, NAME " opMul(" SNAME ") const", +[](const V &a, S f) -> V { return a * f; });                                   \
	SOMA_METHOD(e, NAME, NAME " opDiv(" SNAME ") const", +[](const V &a, S f) -> V { return a / f; });                                   \
	SOMA_METHOD(e, NAME, NAME " opMul_r(" SNAME ") const", +[](const V &a, S f) -> V { return a * f; });                                 \
	SOMA_METHOD(e, NAME, NAME " opAdd_r(" SNAME ") const", +[](const V &a, S f) -> V { return a + f; });                                 \
	SOMA_METHOD(e, NAME, NAME " opNeg() const", +[](const V &a) -> V { return a * (S)-1; });                                             \
	SOMA_METHOD(e, NAME, SNAME " GetElement(uint64 alIdx) const", (GetElem<V, S>));                                                      \
	SOMA_METHOD(e, NAME, "void SetElement(uint64 alIdx," SNAME ") const", (SetElem<V, S>));                                              \
	SOMA_METHOD(e, NAME, NAME " &opAssign(const " SNAME " &in)", +[](V &a, const S &f) -> V & { return a = V(f); });                     \
	SOMA_METHOD(e, NAME, NAME " &opAddAssign(const " SNAME " &in)", +[](V &a, const S &f) -> V & { return a = a + f; });                 \
	SOMA_METHOD(e, NAME, NAME " opAdd(const " SNAME " &in) const", +[](const V &a, const S &f) -> V { return a + f; });                  \
	SOMA_CONSTRUCT(e, NAME, "void f()", +[](V *p) { new (p) V(0); });                                                                    \
	SOMA_CONSTRUCT(e, NAME, "void f(const " NAME " &in)", +[](V *p, const V &o) { new (p) V(o); });

static cSomaVector4f V4(float f) { return {f, f, f, f}; }

static void RegisterVectors(asIScriptEngine *e)
{
	VEC_OPS(cVector2f, float, "cVector2f", "float")
	SOMA_CONSTRUCT(e, "cRect2f", "void f()", +[](cRect2f *p) { new (p) cRect2f(0, 0, 0, 0); });
	SOMA_CONSTRUCT(e, "cRect2f", "void f(const cRect2f &in)", +[](cRect2f *p, const cRect2f &o) { new (p) cRect2f(o); });
	SOMA_CONSTRUCT(e, "cRect2f", "void f(float afX, float afY, float afW, float afH)", +[](cRect2f *p, float x, float y, float w, float h) { new (p) cRect2f(x, y, w, h); });
	SOMA_CONSTRUCT(e, "cRect2f", "void f(float afX, float afY)", +[](cRect2f *p, float x, float y) { new (p) cRect2f(x, y, 0, 0); });
	SOMA_CONSTRUCT(e, "cRect2l", "void f()", +[](cRect2l *p) { new (p) cRect2l(0, 0, 0, 0); });
	SOMA_CONSTRUCT(e, "cRect2l", "void f(const cRect2l &in)", +[](cRect2l *p, const cRect2l &o) { new (p) cRect2l(o); });
	SOMA_CONSTRUCT(e, "cRect2l", "void f(int afX, int afY, int afW, int afH)", +[](cRect2l *p, int x, int y, int w, int h) { new (p) cRect2l(x, y, w, h); });
	SOMA_CONSTRUCT(e, "cRect2l", "void f(int afX, int afY)", +[](cRect2l *p, int x, int y) { new (p) cRect2l(x, y, 0, 0); });
	SOMA_CONSTRUCT(e, "cPlanef", "void f()", +[](cPlanef *p) { new (p) cPlanef(0, 0, 0, 0); });
	SOMA_CONSTRUCT(e, "cPlanef", "void f(const cPlanef &in)", +[](cPlanef *p, const cPlanef &o) { new (p) cPlanef(o); });
	SOMA_CONSTRUCT(e, "cPlanef", "void f(float afA, float afB, float afC, float afD)", +[](cPlanef *p, float a, float b, float c, float d) { new (p) cPlanef(a, b, c, d); });
	SOMA_CONSTRUCT(e, "cPlanef", "void f(const cVector3f &in avNormal, const cVector3f &in avPoint)", +[](cPlanef *p, const cVector3f &n, const cVector3f &pt) { new (p) cPlanef(n, pt); });
	SOMA_CONSTRUCT(e, "cPlanef", "void f(const cVector3f &in avPoint0,const cVector3f &in avPoint1, const cVector3f &in avPoint2)",
				   +[](cPlanef *p, const cVector3f &a, const cVector3f &b, const cVector3f &c) { new (p) cPlanef(a, b, c); });
	SOMA_CONSTRUCT(e, "cDate", "void f()", +[](cDate *p) { memset((void *)p, 0, sizeof(cDate)); });
	SOMA_CONSTRUCT(e, "cDate", "void f(const cDate &in)", +[](cDate *p, const cDate &o) { memcpy((void *)p, &o, sizeof(cDate)); });
	SOMA_CONSTRUCT(e, "cVector2f", "void f(float afX)", +[](cVector2f *p, float x) { new (p) cVector2f(x); });
	SOMA_CONSTRUCT(e, "cVector2f", "void f(float afX, float afY)", +[](cVector2f *p, float x, float y) { new (p) cVector2f(x, y); });
	SOMA_CONSTRUCT(e, "cVector2f", "void f(const cVector3f& in avX)", +[](cVector2f *p, const cVector3f &v) { new (p) cVector2f(v.x, v.y); });
	SOMA_METHOD(e, "cVector2f", "float SqrLength() const", +[](const cVector2f &v) { return v.SqrLength(); });
	SOMA_METHOD(e, "cVector2f", "float Length() const", +[](const cVector2f &v) { return v.Length(); });
	SOMA_METHOD(e, "cVector2f", "float Normalize()", +[](cVector2f &v) { return v.Normalize(); });

	VEC_OPS(cVector3f, float, "cVector3f", "float")
	SOMA_CONSTRUCT(e, "cVector3f", "void f(float afX)", +[](cVector3f *p, float x) { new (p) cVector3f(x); });
	SOMA_CONSTRUCT(e, "cVector3f", "void f(float afX, float afY, float afZ)", +[](cVector3f *p, float x, float y, float z) { new (p) cVector3f(x, y, z); });
	SOMA_CONSTRUCT(e, "cVector3f", "void f(const cVector2f& in avX)", +[](cVector3f *p, const cVector2f &v) { new (p) cVector3f(v.x, v.y, 0); });
	SOMA_METHOD(e, "cVector3f", "float SqrLength() const", +[](const cVector3f &v) { return v.SqrLength(); });
	SOMA_METHOD(e, "cVector3f", "float Length() const", +[](const cVector3f &v) { return v.Length(); });
	SOMA_METHOD(e, "cVector3f", "float Normalize()", +[](cVector3f &v) { return v.Normalize(); });

	VEC_OPS(cSomaVector4f, float, "cVector4f", "float")
	SOMA_CONSTRUCT(e, "cVector4f", "void f(float afX)", +[](cSomaVector4f *p, float x) { *p = V4(x); });
	SOMA_CONSTRUCT(e, "cVector4f", "void f(float afX, float afY, float afZ, float afW)", +[](cSomaVector4f *p, float x, float y, float z, float w) { *p = {x, y, z, w}; });
	SOMA_METHOD(e, "cVector4f", "float SqrLength() const", +[](const cSomaVector4f &v) { return v.SqrLength(); });
	SOMA_METHOD(e, "cVector4f", "float Length() const", +[](const cSomaVector4f &v) { return v.Length(); });
	SOMA_METHOD(e, "cVector4f", "float Normalize()", +[](cSomaVector4f &v) { return v.Normalize(); });

	VEC_OPS(cVector2l, int, "cVector2l", "int")
	SOMA_CONSTRUCT(e, "cVector2l", "void f(int afX)", +[](cVector2l *p, int x) { new (p) cVector2l(x); });
	SOMA_CONSTRUCT(e, "cVector2l", "void f(int alX, int alY)", +[](cVector2l *p, int x, int y) { new (p) cVector2l(x, y); });
	SOMA_METHOD(e, "cVector2l", "int SqrLength() const", +[](const cVector2l &v) { return v.x * v.x + v.y * v.y; });

	VEC_OPS(cVector3l, int, "cVector3l", "int")
	SOMA_CONSTRUCT(e, "cVector3l", "void f(int afX)", +[](cVector3l *p, int x) { new (p) cVector3l(x); });
	SOMA_CONSTRUCT(e, "cVector3l", "void f(int alX, int alY, int alZ)", +[](cVector3l *p, int x, int y, int z) { new (p) cVector3l(x, y, z); });
	SOMA_METHOD(e, "cVector3l", "int SqrLength() const", +[](const cVector3l &v) { return v.x * v.x + v.y * v.y + v.z * v.z; });
}

static void RegisterColor(asIScriptEngine *e)
{
	SOMA_CONSTRUCT(e, "cColor", "void f()", +[](cColor *p) { new (p) cColor(0, 0); });
	SOMA_CONSTRUCT(e, "cColor", "void f(const cColor &in)", +[](cColor *p, const cColor &c) { new (p) cColor(c); });
	SOMA_CONSTRUCT(e, "cColor", "void f(float afR, float afG, float afB, float afA)", +[](cColor *p, float r, float g, float b, float a) { new (p) cColor(r, g, b, a); });
	SOMA_CONSTRUCT(e, "cColor", "void f(float afR, float afG, float afB)", +[](cColor *p, float r, float g, float b) { new (p) cColor(r, g, b, 1); });
	SOMA_CONSTRUCT(e, "cColor", "void f(float afVal, float afAlpha)", +[](cColor *p, float v, float a) { new (p) cColor(v, a); });
	SOMA_CONSTRUCT(e, "cColor", "void f(float afVal)", +[](cColor *p, float v) { new (p) cColor(v, 1); });
	SOMA_METHOD(e, "cColor", "cColor&opAssign(const cColor &in)", +[](cColor &a, const cColor &b) -> cColor & { return a = b; });
	SOMA_METHOD(e, "cColor", "bool opEquals(const cColor &in) const", +[](const cColor &a, const cColor &b) { return a == b; });
	SOMA_METHOD(e, "cColor", "cColor opAdd(const cColor &in) const", +[](const cColor &a, const cColor &b) { return a + b; });
	SOMA_METHOD(e, "cColor", "cColor opSub(const cColor &in) const", +[](const cColor &a, const cColor &b) { return a - b; });
	SOMA_METHOD(e, "cColor", "cColor opMul(const cColor &in) const", +[](const cColor &a, const cColor &b) { return a * b; });
	SOMA_METHOD(e, "cColor", "cColor opDiv(const cColor &in) const", +[](const cColor &a, const cColor &b) { return cColor(a.r / b.r, a.g / b.g, a.b / b.b, a.a / b.a); });
	SOMA_METHOD(e, "cColor", "cColor opMul(float) const", +[](const cColor &a, float f) { return a * f; });
	SOMA_METHOD(e, "cColor", "cColor opDiv(float) const", +[](const cColor &a, float f) { return a / f; });
	SOMA_METHOD(e, "cColor", "cColor ToLinearSpace(const float afPower, const bool abCorrectAlpha) const",
				+[](const cColor &c, float p, bool abAlpha) { return cColor(powf(c.r, p), powf(c.g, p), powf(c.b, p), abAlpha ? powf(c.a, p) : c.a); });
	SOMA_METHOD(e, "cColor", "cColor ToSRGB(const bool abCorrectAlpha) const",
				+[](const cColor &c, bool abAlpha) { float p = 1 / 2.2f; return cColor(powf(c.r, p), powf(c.g, p), powf(c.b, p), abAlpha ? powf(c.a, p) : c.a); });
}

static void RegisterMatrixQuat(asIScriptEngine *e)
{
	SOMA_CONSTRUCT(e, "cMatrixf", "void f()", +[](cMatrixf *p) { new (p) cMatrixf(cMatrixf::Identity); });
	SOMA_CONSTRUCT(e, "cMatrixf", "void f(const cMatrixf &in)", +[](cMatrixf *p, const cMatrixf &m) { new (p) cMatrixf(m); });
	SOMA_CONSTRUCT(e, "cMatrixf", "void f(const cVector4f &in, const cVector4f &in, const cVector4f &in,const cVector4f &in)",
				   +[](cMatrixf *p, const cSomaVector4f &a, const cSomaVector4f &b, const cSomaVector4f &c, const cSomaVector4f &d) {
					   new (p) cMatrixf(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, c.x, c.y, c.z, c.w, d.x, d.y, d.z, d.w);
				   });
	SOMA_CONSTRUCT(e, "cMatrixf", "void f(float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float)",
				   +[](cMatrixf *p, float a, float b, float c, float d, float e2, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float q) {
					   new (p) cMatrixf(a, b, c, d, e2, f, g, h, i, j, k, l, m, n, o, q);
				   });
	SOMA_METHOD(e, "cMatrixf", "cMatrixf&opAssign(const cMatrixf &in)", +[](cMatrixf &a, const cMatrixf &b) -> cMatrixf & { return a = b; });
	SOMA_METHOD(e, "cMatrixf", "bool opEquals(const cMatrixf &in) const", +[](const cMatrixf &a, const cMatrixf &b) { return memcmp(&a, &b, sizeof(cMatrixf)) == 0; });
	SOMA_METHOD(e, "cMatrixf", "float GetElement(uint64, uint64) const", +[](const cMatrixf &m, asQWORD c, asQWORD r) { return r < 4 && c < 4 ? m.m[r][c] : 0.0f; });
	SOMA_METHOD(e, "cMatrixf", "cVector3f GetRight() const", +[](const cMatrixf &m) { return m.GetRight(); });
	SOMA_METHOD(e, "cMatrixf", "void SetRight(const cVector3f&in avVec)", +[](cMatrixf &m, const cVector3f &v) { m.SetRight(v); });
	SOMA_METHOD(e, "cMatrixf", "cVector3f GetUp() const", +[](const cMatrixf &m) { return m.GetUp(); });
	SOMA_METHOD(e, "cMatrixf", "void SetUp(const cVector3f&in avVec)", +[](cMatrixf &m, const cVector3f &v) { m.SetUp(v); });
	SOMA_METHOD(e, "cMatrixf", "cVector3f GetForward() const", +[](const cMatrixf &m) { return m.GetForward(); });
	SOMA_METHOD(e, "cMatrixf", "void SetForward(const cVector3f&in avVec)", +[](cMatrixf &m, const cVector3f &v) { m.SetForward(v); });
	SOMA_METHOD(e, "cMatrixf", "cVector3f GetTranslation() const", +[](const cMatrixf &m) { return m.GetTranslation(); });
	SOMA_METHOD(e, "cMatrixf", "void SetTranslation(const cVector3f&in avTrans)", +[](cMatrixf &m, const cVector3f &v) { m.SetTranslation(v); });
	SOMA_METHOD(e, "cMatrixf", "void SetRotation(const cMatrixf&in a_mtxRot)", +[](cMatrixf &m, const cMatrixf &r) {
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				m.m[i][j] = r.m[i][j];
	});
	SOMA_METHOD(e, "cMatrixf", "cMatrixf GetRotation() const", +[](const cMatrixf &m) { return m.GetRotation(); });
	SOMA_METHOD(e, "cMatrixf", "cMatrixf GetTranspose() const", +[](const cMatrixf &m) { return m.GetTranspose(); });

	SOMA_CONSTRUCT(e, "cQuaternion", "void f()", +[](cQuaternion *p) { new (p) cQuaternion(cQuaternion::Identity); });
	SOMA_CONSTRUCT(e, "cQuaternion", "void f(const cQuaternion &in)", +[](cQuaternion *p, const cQuaternion &q) { new (p) cQuaternion(q); });
	SOMA_CONSTRUCT(e, "cQuaternion", "void f(float, const cVector3f &in)", +[](cQuaternion *p, float a, const cVector3f &v) { new (p) cQuaternion(a, v); });
	SOMA_CONSTRUCT(e, "cQuaternion", "void f(float, float, float, float)", +[](cQuaternion *p, float w, float x, float y, float z) { new (p) cQuaternion(w, x, y, z); });
	SOMA_CONSTRUCT(e, "cQuaternion", "void f(const cMatrixf &in)", +[](cQuaternion *p, const cMatrixf &m) { new (p) cQuaternion(); p->FromRotationMatrix(m); });
	SOMA_METHOD(e, "cQuaternion", "cQuaternion&opAssign(const cQuaternion &in)", +[](cQuaternion &a, const cQuaternion &b) -> cQuaternion & { return a = b; });
	SOMA_METHOD(e, "cQuaternion", "cQuaternion opAdd(const cQuaternion &in) const", +[](const cQuaternion &a, const cQuaternion &b) { return a + b; });
	SOMA_METHOD(e, "cQuaternion", "cQuaternion opSub(const cQuaternion &in) const", +[](const cQuaternion &a, const cQuaternion &b) { return a - b; });
	SOMA_METHOD(e, "cQuaternion", "cQuaternion opMul(const cQuaternion &in) const", +[](const cQuaternion &a, const cQuaternion &b) { return a * b; });
	SOMA_METHOD(e, "cQuaternion", "cQuaternion opMul(float) const", +[](const cQuaternion &a, float f) { return a * f; });
	SOMA_METHOD(e, "cQuaternion", "void Normalize()", +[](cQuaternion &q) { q.Normalize(); });
	SOMA_METHOD(e, "cQuaternion", "void FromRotationMatrix(const cMatrixf &in)", +[](cQuaternion &q, const cMatrixf &m) { q.FromRotationMatrix(m); });
}

//---------------------------------------
// Global constants

static void RegisterConstants(asIScriptEngine *e)
{
	static float vFloats[] = {kPif, kPi2f, kPi4f, k2Pif, kEpsilonf, 1.41421356f};
	static const char *vFloatNames[] = {"const float cMath_Pi", "const float cMath_PiDiv2", "const float cMath_PiDiv4",
										"const float cMath_PiMul2", "const float cMath_Epsilon", "const float cMath_Sqrt2"};
	for (int i = 0; i < 6; ++i)
		e->RegisterGlobalProperty(vFloatNames[i], &vFloats[i]);

	static cColor vColors[] = {cColor(1, 0, 0, 1), cColor(0, 1, 0, 1), cColor(0, 0, 1, 1), cColor(1, 1)};
	e->RegisterGlobalProperty("const cColor cColor_Red", &vColors[0]);
	e->RegisterGlobalProperty("const cColor cColor_Green", &vColors[1]);
	e->RegisterGlobalProperty("const cColor cColor_Blue", &vColors[2]);
	e->RegisterGlobalProperty("const cColor cColor_White", &vColors[3]);

	static cVector2l v2lMinusOne(-1);
	e->RegisterGlobalProperty("const cVector2l cVector2l_MinusOne", &v2lMinusOne);
	static cVector2f v2f[] = {cVector2f(0), cVector2f(1), cVector2f(1, 0), cVector2f(-1, 0), cVector2f(0, 1), cVector2f(0, -1), cVector2f(-1)};
	const char *v2fNames[] = {"Zero", "One", "Right", "Left", "Up", "Down", "MinusOne"};
	for (int i = 0; i < 7; ++i)
		e->RegisterGlobalProperty(("const cVector2f cVector2f_" + std::string(v2fNames[i])).c_str(), &v2f[i]);
	static cVector3f v3f[] = {cVector3f(0), cVector3f(1), cVector3f(1, 0, 0), cVector3f(-1, 0, 0), cVector3f(0, 1, 0),
							  cVector3f(0, -1, 0), cVector3f(0, 0, -1), cVector3f(0, 0, 1), cVector3f(-1)};
	const char *v3fNames[] = {"Zero", "One", "Right", "Left", "Up", "Down", "Forward", "Back", "MinusOne"};
	for (int i = 0; i < 9; ++i)
		e->RegisterGlobalProperty(("const cVector3f cVector3f_" + std::string(v3fNames[i])).c_str(), &v3f[i]);
	static cSomaVector4f v4f[] = {V4(0), V4(1), V4(-1)};
	e->RegisterGlobalProperty("const cVector4f cVector4f_Zero", &v4f[0]);
	e->RegisterGlobalProperty("const cVector4f cVector4f_One", &v4f[1]);
	e->RegisterGlobalProperty("const cVector4f cVector4f_MinusOne", &v4f[2]);
	static cMatrixf vMat[] = {cMatrixf::Identity, cMatrixf::Zero};
	e->RegisterGlobalProperty("const cMatrixf cMatrixf_Identity", &vMat[0]);
	e->RegisterGlobalProperty("const cMatrixf cMatrixf_Zero", &vMat[1]);
	static cQuaternion qIdentity = cQuaternion::Identity;
	e->RegisterGlobalProperty("const cQuaternion cQuaternion_Identity", &qIdentity);
}

//---------------------------------------
// cMath_*

static void RegisterMathFunctions(asIScriptEngine *e)
{
	SOMA_FUNC(e, "float cMath_Sin(float afX)", +[](float x) { return sinf(x); });
	SOMA_FUNC(e, "float cMath_Cos(float afX)", +[](float x) { return cosf(x); });
	SOMA_FUNC(e, "float cMath_Tan(float afX)", +[](float x) { return tanf(x); });
	SOMA_FUNC(e, "float cMath_SigmoidCurve(float afX)", +[](float x) { return x * x * (3 - 2 * x); });
	SOMA_FUNC(e, "float cMath_InterpolateSigmoid(float afA,float afB,float afT)", +[](float a, float b, float t) { return a + (b - a) * t * t * (3 - 2 * t); });
	SOMA_FUNC(e, "float cMath_ASin(float afX)", +[](float x) { return asinf(x); });
	SOMA_FUNC(e, "float cMath_ACos(float afX)", +[](float x) { return acosf(x); });
	SOMA_FUNC(e, "float cMath_ATan(float afX)", +[](float x) { return atanf(x); });
	SOMA_FUNC(e, "float cMath_ATan2(float afY, float afX)", +[](float y, float x) { return atan2f(y, x); });
	SOMA_FUNC(e, "float cMath_Log(float afX)", +[](float x) { return logf(x); });
	SOMA_FUNC(e, "float cMath_Pow(float afX, float afExp)", +[](float x, float p) { return powf(x, p); });
	SOMA_FUNC(e, "float cMath_Sqrt(float afX)", +[](float x) { return sqrtf(x); });
	SOMA_FUNC(e, "void cMath_Randomize(int alSeed)", +[](int s) { cMath::Randomize(s); });
	SOMA_FUNC(e, "float cMath_Easing(eEasing aType, float afT, float afMin = 0, float afMax = 1)",
			  +[](int t, float x, float a, float b) { return a + (b - a) * SomaEasing(t, cMath::Clamp(x, 0.0f, 1.0f)); });
	SOMA_FUNC(e, "float cMath_Vector3MaxElement(const cVector3f &in avVec)", +[](const cVector3f &v) { return std::max(v.x, std::max(v.y, v.z)); });
	SOMA_FUNC(e, "cQuaternion cMath_QuaternionEuler(const cVector3f&in avEuler, eEulerRotationOrder aOrder)", +[](const cVector3f &v, int o) {
		cQuaternion q;
		q.FromRotationMatrix(cMath::MatrixRotate(v, (eEulerRotationOrder)o));
		return q;
	});
	SOMA_FUNC(e, "cVector3f cMath_Vector3CatmullRom(const cVector3f&in avP0, const cVector3f&in avP1, const cVector3f&in avP2, const cVector3f&in avP3, float afFract)",
			  +[](const cVector3f &a, const cVector3f &b, const cVector3f &c, const cVector3f &d, float t) { return SomaCatmullRom(a, b, c, d, t); });
	SOMA_FUNC(e, "cVector2f cMath_Vector2CatmullRom(const cVector2f &in avP0, const cVector2f &in avP1, const cVector2f &in avP2, const cVector2f &in avP3, float afFract)",
			  +[](const cVector2f &a, const cVector2f &b, const cVector2f &c, const cVector2f &d, float t) { return SomaCatmullRom(a, b, c, d, t); });
	SOMA_FUNC(e, "void Math_CatmullRom(cVector3f &out avResult, const cVector3f &in avP0, const cVector3f &in avP1, const cVector3f &in avP2, const cVector3f &in avP3, float afFract)",
			  +[](cVector3f &r, const cVector3f &a, const cVector3f &b, const cVector3f &c, const cVector3f &d, float t) { r = SomaCatmullRom(a, b, c, d, t); });
	SOMA_METHOD(e, "cQuaternion", "void FromAngleAxis(float afAngle, const cVector3f &in)", +[](cQuaternion &q, float a, const cVector3f &v) { q.FromAngleAxis(a, v); });
	SOMA_FUNC(e, "cVector3f cMath_GetAngleDistanceVector3fRad(const cVector3f&in avAngles1, const cVector3f&in avAngles2)", +[](const cVector3f &a, const cVector3f &b) {
		return cVector3f(cMath::GetAngleDistanceRad(a.x, b.x), cMath::GetAngleDistanceRad(a.y, b.y), cMath::GetAngleDistanceRad(a.z, b.z));
	});
	SOMA_FUNC(e, "float cMath_Vector2Dot(const cVector2f &in avVecA, const cVector2f &in avVecB)", +[](const cVector2f &a, const cVector2f &b) { return a.x * b.x + a.y * b.y; });
	SOMA_FUNC(e, "cVector3f cMath_Vector3ProjectOnPlane(const cVector3f &in avPlaneNormal, const cVector3f &in avVec)",
			  +[](const cVector3f &n, const cVector3f &v) { return v - n * cMath::Vector3Dot(v, n); });
	SOMA_FUNC(e, "cMatrixf cMath_MatrixRotateXYZ(const cVector3f &in avRot)", +[](const cVector3f &r) { return cMath::MatrixRotate(r, eEulerRotationOrder_XYZ); });
	SOMA_FUNC(e, "cMatrixf cMath_MatrixRotateXZY(const cVector3f &in avRot)", +[](const cVector3f &r) { return cMath::MatrixRotate(r, eEulerRotationOrder_XZY); });
	SOMA_FUNC(e, "cMatrixf cMath_MatrixRotateYXZ(const cVector3f &in avRot)", +[](const cVector3f &r) { return cMath::MatrixRotate(r, eEulerRotationOrder_YXZ); });
	SOMA_FUNC(e, "cMatrixf cMath_MatrixRotateYZX(const cVector3f &in avRot)", +[](const cVector3f &r) { return cMath::MatrixRotate(r, eEulerRotationOrder_YZX); });
	SOMA_FUNC(e, "cMatrixf cMath_MatrixRotateZXY(const cVector3f &in avRot)", +[](const cVector3f &r) { return cMath::MatrixRotate(r, eEulerRotationOrder_ZXY); });
	SOMA_FUNC(e, "cMatrixf cMath_MatrixRotateZYX(const cVector3f &in avRot)", +[](const cVector3f &r) { return cMath::MatrixRotate(r, eEulerRotationOrder_ZYX); });
	SOMA_FUNC(e, "float cMath_InterpolateLinear(float afA, float afB, float afT)", +[](float a, float b, float t) { return a + (b - a) * t; });
	SOMA_FUNC(e, "float cMath_InterpolateCosine(float afA, float afB, float afT)",
			  +[](float a, float b, float t) { float f = (1 - cosf(t * kPif)) * 0.5f; return a * (1 - f) + b * f; });
	SOMA_FUNC(e, "float cMath_Round(float afX)", +[](float x) { return roundf(x); });
	SOMA_FUNC(e, "int cMath_GetBit(int alBitNum)", +[](int n) { return 1 << n; });
	SOMA_FUNC(e, "void cMath_SetBitFlag(int&out alFlagNum, int alBit, bool abSet)", +[](int &f, int b, bool s) { if (s) f |= b; else f &= ~b; });
	SOMA_FUNC(e, "bool cMath_GetBitFlag(int alFlagNum, int alBit)", +[](int f, int b) { return (f & b) != 0; });
	SOMA_FUNC(e, "cVector3f cMath_ExpandAABBMin(const cVector3f&in avBaseMin, const cVector3f&in avAddMin)", +[](const cVector3f &a, const cVector3f &b) { return cMath::Vector3Min(a, b); });
	SOMA_FUNC(e, "cVector3f cMath_ExpandAABBMax(const cVector3f&in avBaseMax, const cVector3f&in avAddMax)", +[](const cVector3f &a, const cVector3f &b) { return cMath::Vector3Max(a, b); });
}

//---------------------------------------
// cPidController<T> in the script struct block: p, i, d at 16/20/24 (recovered), then the history

template <class T> struct cSomaPid
{
	static const int kMaxErrors = 64;
	float p, i, d;
	int mlErrorNum, mlCur, mlLastPlusOne;
	T mLastError, mLastDerivative, mLastIntegral;
	T mvErrors[kMaxErrors];
	float mvTimeSteps[kMaxErrors];

	static cSomaPid *At(void *apObj) { return (cSomaPid *)((char *)apObj + 16); }

	void SetErrorNum(int alNum)
	{
		mlErrorNum = std::clamp(alNum, 1, kMaxErrors);
		Reset();
	}
	void Reset()
	{
		mlCur = 0;
		mlLastPlusOne = 0;
		for (int k = 0; k < kMaxErrors; ++k)
		{
			mvErrors[k] = T(0);
			mvTimeSteps[k] = 0;
		}
	}
	T Output(const T &aError, float afTimeStep)
	{
		if (mlErrorNum <= 0)
			SetErrorNum(10);
		mvErrors[mlCur] = aError;
		mvTimeSteps[mlCur] = afTimeStep;
		T integral = T(0);
		for (int k = 0; k < mlErrorNum; ++k)
			integral += mvErrors[k] * mvTimeSteps[k];
		T derivative = T(0);
		if (mlLastPlusOne > 0 && afTimeStep > 0)
			derivative = (mvErrors[mlCur] - mvErrors[mlLastPlusOne - 1]) / afTimeStep;
		mlLastPlusOne = mlCur + 1;
		mlCur = (mlCur + 1) % mlErrorNum;
		mLastError = aError;
		mLastDerivative = derivative;
		mLastIntegral = integral;
		return aError * p + integral * i + derivative * d;
	}
};
static_assert(sizeof(cSomaPid<cVector3f>) + 16 <= 4096, "pid state exceeds the struct block");

template <class T> static void RegisterPid(asIScriptEngine *e, const char *apType, const char *apT)
{
	typedef cSomaPid<T> P;
	std::string sType = apType, sT = apT;
	e->RegisterObjectBehaviour(apType, asBEHAVE_FACTORY, (sType + "@ f()").c_str(), asFUNCTION(+[](asIScriptGeneric *g) {
		void *pObj = SomaNewOwnedScriptStruct(g->GetEngine()->GetTypeInfoById(g->GetFunction()->GetReturnTypeId())->GetName());
		P::At(pObj)->SetErrorNum(10);
		*(void **)g->GetAddressOfReturnLocation() = pObj;
	}), asCALL_GENERIC);
	e->RegisterObjectBehaviour(apType, asBEHAVE_FACTORY, (sType + "@ f(float afP, float afI, float afD, int alErrorNum)").c_str(), asFUNCTION(+[](asIScriptGeneric *g) {
		void *pObj = SomaNewOwnedScriptStruct(g->GetEngine()->GetTypeInfoById(g->GetFunction()->GetReturnTypeId())->GetName());
		P *pPid = P::At(pObj);
		pPid->p = g->GetArgFloat(0);
		pPid->i = g->GetArgFloat(1);
		pPid->d = g->GetArgFloat(2);
		pPid->SetErrorNum((int)g->GetArgDWord(3));
		*(void **)g->GetAddressOfReturnLocation() = pObj;
	}), asCALL_GENERIC);
	e->RegisterObjectMethod(apType, (sT + " Output(const " + sT + "&in avError, float afTimeStep)").c_str(), asFUNCTION(+[](asIScriptGeneric *g) {
		T r = P::At(g->GetObject())->Output(*(const T *)g->GetArgAddress(0), g->GetArgFloat(1));
		if constexpr (std::is_same<T, float>::value)
			g->SetReturnFloat(r);
		else
			g->SetReturnObject(&r);
	}), asCALL_GENERIC);
	e->RegisterObjectMethod(apType, "void SetErrorNum(int alErrorNum)", asFUNCTION(+[](asIScriptGeneric *g) { P::At(g->GetObject())->SetErrorNum((int)g->GetArgDWord(0)); }), asCALL_GENERIC);
	e->RegisterObjectMethod(apType, "void Reset()", asFUNCTION(+[](asIScriptGeneric *g) { P::At(g->GetObject())->Reset(); }), asCALL_GENERIC);
	e->RegisterObjectMethod(apType, (sT + " GetLastError()").c_str(), asFUNCTION(+[](asIScriptGeneric *g) {
		T r = P::At(g->GetObject())->mLastError;
		if constexpr (std::is_same<T, float>::value) g->SetReturnFloat(r); else g->SetReturnObject(&r);
	}), asCALL_GENERIC);
	e->RegisterObjectMethod(apType, (sT + " GetLastDerivative()").c_str(), asFUNCTION(+[](asIScriptGeneric *g) {
		T r = P::At(g->GetObject())->mLastDerivative;
		if constexpr (std::is_same<T, float>::value) g->SetReturnFloat(r); else g->SetReturnObject(&r);
	}), asCALL_GENERIC);
	e->RegisterObjectMethod(apType, (sT + " GetLastIntegral()").c_str(), asFUNCTION(+[](asIScriptGeneric *g) {
		T r = P::At(g->GetObject())->mLastIntegral;
		if constexpr (std::is_same<T, float>::value) g->SetReturnFloat(r); else g->SetReturnObject(&r);
	}), asCALL_GENERIC);
}

// Keyed by struct block; a reused block address is cleared by the factory
static std::map<void *, std::multiset<tString>> gmapStringSets;

static void RegisterStringSet(asIScriptEngine *e)
{
	e->RegisterObjectBehaviour("cScriptStringSet", asBEHAVE_FACTORY, "cScriptStringSet@ f()", asFUNCTION(+[](asIScriptGeneric *g) {
		void *pObj = SomaNewOwnedScriptStruct("cScriptStringSet");
		gmapStringSets[pObj].clear();
		*(void **)g->GetAddressOfReturnLocation() = pObj;
	}), asCALL_GENERIC);
	typedef const tString &S;
	SOMA_METHOD(e, "cScriptStringSet", "cScriptStringSet& opAssign(const cScriptStringSet &in)", +[](void *p, void *o) -> void * {
		gmapStringSets[p] = gmapStringSets[o];
		return p;
	});
	SOMA_METHOD(e, "cScriptStringSet", "void Add(const tString &in asStr)", +[](void *p, S s) { gmapStringSets[p].insert(s); });
	SOMA_METHOD(e, "cScriptStringSet", "void Erase(const tString &in asStr)", +[](void *p, S s) { gmapStringSets[p].erase(s); });
	SOMA_METHOD(e, "cScriptStringSet", "bool Exists(const tString &in asStr)", +[](void *p, S s) { return gmapStringSets[p].count(s) != 0; });
	SOMA_METHOD(e, "cScriptStringSet", "int Count(const tString &in asStr)", +[](void *p, S s) { return (int)gmapStringSets[p].count(s); });
	SOMA_METHOD(e, "cScriptStringSet", "void Clear()", +[](void *p) { gmapStringSets[p].clear(); });
	SOMA_METHOD(e, "cScriptStringSet", "int Size()", +[](void *p) { return (int)gmapStringSets[p].size(); });
	SOMA_METHOD(e, "cScriptStringSet", "void ElementsToArray(array<tString> &out avOutElements)", +[](void *p, CScriptArray &a) {
		a.Resize(0);
		for (const tString &s : gmapStringSets[p])
			a.InsertLast((void *)&s);
	});
}

void RegisterSomaScriptMathNatives(asIScriptEngine *apEngine)
{
	RegisterStringSet(apEngine);
	RegisterPid<cVector3f>(apEngine, "cPidControllerVec3", "cVector3f");
	RegisterPid<float>(apEngine, "cPidControllerf", "float");
	RegisterVectors(apEngine);
	RegisterColor(apEngine);
	RegisterMatrixQuat(apEngine);
	RegisterConstants(apEngine);
	RegisterMathFunctions(apEngine);
}
