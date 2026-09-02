/*
 * Regression tests for soma/src/game/HpslTranspiler.cpp - the best-effort
 * HPSL->GLSL 120 syntax transpiler for SOMA's HPL3-format shaders (see that
 * file's header comment for full scope notes).
 *
 * Plain, dependency-free checks (no GL/SDL/game-data needed - same rationale
 * as PhysicsNewtonTests.cpp: run fast and deterministically under CTest,
 * without depending on a live GPU context or a real SOMA install being
 * present on the machine running the test).
 *
 * IMPORTANT SCOPE NOTE: this only checks that TranspileHpslToGlsl() produces
 * the *expected GLSL syntax* (string-level assertions on the output) - it
 * does NOT run a real GLSL compiler, unlike soma/src/game/
 * HpslTranspilerSelfTest.cpp (which does, via a live GL context inside a
 * fully-booted SOMA process, but needs the real game installed and running
 * to do it). The five source strings below are verbatim copies of five real
 * files from a real SOMA install
 * (~/.local/share/Steam/steamapps/common/SOMA/core/shaders/hpsl/), already
 * run through the same @ifdef-stripping cPreprocessParser would apply (only
 * deferred_posteffect_quad_vtx.hpsl has any directives - the UseUvCoord1
 * block, stripped here to match what Parse() produces when that var is
 * undefined, exactly as it is for every material that doesn't request a
 * second UV channel).
 */

#include <cstdio>
#include <cstring>

#include "../../soma/src/game/HpslTranspiler.h"

using namespace hpl;

static int gFailures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
			++gFailures; \
		} \
	} while (0)

#define CHECK_CONTAINS(haystack, needle) \
	do { \
		if ((haystack).find(needle) == tString::npos) { \
			std::fprintf(stderr, "FAILED: '%s' not found in output (%s:%d)\n--- output ---\n%s\n--------------\n", \
				needle, __FILE__, __LINE__, (haystack).c_str()); \
			++gFailures; \
		} \
	} while (0)
#define CHECK_NOT_CONTAINS(haystack, needle) \
	CHECK((haystack).find(needle) == tString::npos)

//-----------------------------------------------------------------------

// Verbatim from clear_vtx.hpsl (already preprocessed - it has no @ifdef).
static const char* gpsClearVtx =
	"void main(in cVector4f vtx_vPosition,\n"
	"		  in cVector4f vtx_vColor, \n"
	"		  out cVector4f px_vColor,\n"
	"		  out cVector4f px_vPosition)\n"
	"{	\n"
	"	px_vPosition = vtx_vPosition;\n"
	"	px_vColor = vtx_vColor;\n"
	"}";

// Verbatim from clear_frag.hpsl.
static const char* gpsClearFrag =
	"void main(in cVector4f px_vPosition,\n"
	"		  in cVector4f px_vColor,\n"
	"		  out cVector4f out_vColor0 : 0,\n"
	"		  out cVector4f out_vColor1 : 1,\n"
	"		  out cVector4f out_vColor2 : 2,\n"
	"		  out cVector4f out_vColor3 : 3)\n"
	"{\n"
	"	out_vColor0 = out_vColor1 = out_vColor2 = out_vColor3 = px_vColor;\n"
	"}";

// Verbatim from null_vtx.hpsl - exercises mul() and a plain (non-texture)
// uniform declaration with no binding-index suffix.
static const char* gpsNullVtx =
	"uniform cMatrixf a_mtxModelViewProjection;\n"
	"\n"
	"void main(in cVector4f vtx_vPosition,\n"
	"		  in cVector4f vtx_vColor, \n"
	"		  in cVector4f vtx_vTexCoord0,\n"
	"		  out cVector4f px_vColor,\n"
	"		  out cVector4f px_vTexCoord0,\n"
	"		  out cVector4f px_vPosition)\n"
	"{	\n"
	"	px_vPosition = mul(a_mtxModelViewProjection, vtx_vPosition);\n"
	"	px_vColor = vtx_vColor;\n"
	"	px_vTexCoord0 = vtx_vTexCoord0;\n"
	"}";

// Verbatim from null_frag.hpsl - exercises sample() and a texture uniform
// with a D3D-style ": N" binding-index suffix that must be stripped.
static const char* gpsNullFrag =
	"uniform cTexture2D aColorMap : 0;\n"
	"\n"
	"void main(in cVector4f px_vPosition,\n"
	"		  in cVector4f px_vColor,\n"
	"		  in cVector4f px_vTexCoord0,\n"
	"		  out cVector4f out_vColor0 : 0,\n"
	"		  out cVector4f out_vColor1 : 1,\n"
	"		  out cVector4f out_vColor2 : 2,\n"
	"		  out cVector4f out_vColor3 : 3)\n"
	"{\n"
	"	cVector4f vColor = px_vColor;\n"
	"\n"
	"\n"
	"	vColor *= sample(aColorMap, px_vTexCoord0.xy);\n"
	"\n"
	"\n"
	"	out_vColor0 = out_vColor1 = out_vColor2 = out_vColor3 = vColor;\n"
	"}";

// Verbatim from deferred_depthonly_frag.hpsl.
static const char* gpsDepthonlyFrag =
	"void main(in cVector4f px_vPosition,\n"
	"		  out cVector4f px_vColor : 0)\n"
	"{\n"
	"	px_vColor = cVector4f(1.0);\n"
	"}";

// deferred_posteffect_quad_vtx.hpsl, preprocessed with UseUvCoord1
// undefined (the @ifdef block removed) - matches what cPreprocessParser
// produces for any material that doesn't request a second UV channel.
static const char* gpsPosteffectQuadVtx =
	"void main(in cVector4f vtx_vPosition,\n"
	"	      in cVector4f vtx_vTexCoord0,\n"
	"		  in cVector4f vtx_vTexCoord1,\n"
	"		  out cVector4f px_vTexCoord0,\n"
	"		  out cVector4f px_vPosition)\n"
	"{	\n"
	"	px_vPosition = vtx_vPosition * cVector4f(2.0,-2.0, 0.0, 0.0) + cVector4f(-1.0, 1.0, 0.0, 1.0);\n"
	"	px_vTexCoord0 = vtx_vTexCoord0;\n"
	"}";

// Verbatim from debug_overdraw_frag.hpsl.
static const char* gpsDebugOverdrawFrag =
	"void main(in cVector4f px_vPosition,\n"
	"		  out cVector4f out_vColor : 0)\n"
	"{\n"
	"	out_vColor = cVector4f(1.0f / 48.0f);\n"
	"}";

//-----------------------------------------------------------------------

static void TestClearPair()
{
	tString sGlsl, sErr;

	CHECK(TranspileHpslToGlsl(gpsClearVtx, eGpuShaderType_Vertex, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_Position = gl_Vertex");
	CHECK_CONTAINS(sGlsl, "varying vec4 px_vColor");
	CHECK_NOT_CONTAINS(sGlsl, "varying vec4 px_vPosition"); // must NOT be a varying

	CHECK(TranspileHpslToGlsl(gpsClearFrag, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_FragData[0]");
	CHECK_CONTAINS(sGlsl, "gl_FragData[3]");
	CHECK_CONTAINS(sGlsl, "#extension GL_ARB_draw_buffers");
}

static void TestNullPair()
{
	tString sGlsl, sErr;

	CHECK(TranspileHpslToGlsl(gpsNullVtx, eGpuShaderType_Vertex, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "uniform mat4 a_mtxModelViewProjection;");
	CHECK_CONTAINS(sGlsl, "gl_Position = (a_mtxModelViewProjection * gl_Vertex)");
	CHECK_NOT_CONTAINS(sGlsl, "mul(");

	CHECK(TranspileHpslToGlsl(gpsNullFrag, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "uniform sampler2D aColorMap;"); // ": 0" binding index stripped
	CHECK_NOT_CONTAINS(sGlsl, "aColorMap : 0");
	CHECK_CONTAINS(sGlsl, "texture2D(aColorMap, px_vTexCoord0.xy)");
	CHECK_NOT_CONTAINS(sGlsl, "sample(");
	// null_frag.hpsl declares "in cVector4f px_vPosition" but never
	// references it in the body - the substitution table maps it to
	// gl_FragCoord (see TestFragPositionUsed() below for a case that
	// actually uses it), but an unused parameter naturally leaves no
	// trace either way. The regression-worthy assertion here is just
	// that it's never emitted as a bogus varying (nothing on the vertex
	// side would ever write to one, since px_vPosition is special-cased
	// to gl_Position there instead).
	CHECK_NOT_CONTAINS(sGlsl, "varying vec4 px_vPosition");
}

static void TestFragPositionUsed()
{
	// A synthetic case (no real .hpsl file happens to actually reference
	// px_vPosition in its body) proving the gl_FragCoord substitution
	// itself actually fires when it's not a no-op.
	tString sGlsl, sErr;
	static const char* psSrc =
		"void main(in cVector4f px_vPosition,\n"
		"		  out cVector4f px_vColor : 0)\n"
		"{\n"
		"	px_vColor = px_vPosition;\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_FragData[0] = gl_FragCoord");
}

static void TestDepthonlyFrag()
{
	tString sGlsl, sErr;
	CHECK(TranspileHpslToGlsl(gpsDepthonlyFrag, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_FragData[0]");
	// px_vPosition is declared but unused in the body - just must not be
	// emitted as a bogus varying (nothing would ever write to a
	// vertex-side px_vPosition varying, since that identifier is special-
	// cased to gl_Position on the vertex side instead).
	CHECK_NOT_CONTAINS(sGlsl, "varying vec4 px_vPosition");
}

static void TestPosteffectQuadVtx()
{
	tString sGlsl, sErr;
	CHECK(TranspileHpslToGlsl(gpsPosteffectQuadVtx, eGpuShaderType_Vertex, sGlsl, sErr));
	// No mul() in this file (uses GLSL-native '*'/'+' operators directly),
	// so no extra parens get added - only the identifier substitution
	// (vtx_vPosition->gl_Vertex, px_vPosition->gl_Position) applies here.
	CHECK_CONTAINS(sGlsl, "gl_Position = gl_Vertex * vec4(2.0,-2.0, 0.0, 0.0) + vec4(-1.0, 1.0, 0.0, 1.0)");
	CHECK_CONTAINS(sGlsl, "varying vec4 px_vTexCoord0");
	// vtx_vTexCoord1 is declared but never referenced in this file's body
	// (its counterpart px_vTexCoord1 was stripped by preprocessing, since
	// this copy simulates UseUvCoord1 undefined) - the gl_MultiTexCoord1
	// mapping itself is exercised directly below instead.
}

static void TestVertexTexCoord1Builtin()
{
	// Directly exercises the vtx_vTexCoord1->gl_MultiTexCoord1 guess (see
	// HpslTranspiler.cpp's gmapVertexBuiltins) - unverified against any
	// real .hpsl file (none of the ones examined this pass reference it
	// in a shader body), but confirms the mapping table entry itself
	// fires correctly when it IS referenced.
	tString sGlsl, sErr;
	static const char* psSrc =
		"void main(in cVector4f vtx_vTexCoord1,\n"
		"		  out cVector4f px_vColor)\n"
		"{\n"
		"	px_vColor = vtx_vTexCoord1;\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Vertex, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "px_vColor = gl_MultiTexCoord1");
}

static void TestDebugOverdrawFrag()
{
	tString sGlsl, sErr;
	CHECK(TranspileHpslToGlsl(gpsDebugOverdrawFrag, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_FragData[0] = vec4(1.0f / 48.0f)");
}

static void TestMulRejectsWrongArgCount()
{
	tString sGlsl, sErr;
	static const char* psBadMul =
		"void main(out cVector4f px_vColor : 0)\n"
		"{\n"
		"	px_vColor = mul(a, b, c);\n"
		"}";
	CHECK(TranspileHpslToGlsl(psBadMul, eGpuShaderType_Fragment, sGlsl, sErr) == false);
	CHECK_CONTAINS(sErr, "mul()");
}

static void TestSampleRejectsUnknownTexture()
{
	tString sGlsl, sErr;
	static const char* psBadSample =
		"void main(out cVector4f px_vColor : 0)\n"
		"{\n"
		"	px_vColor = sample(aNotDeclared, uv);\n"
		"}";
	CHECK(TranspileHpslToGlsl(psBadSample, eGpuShaderType_Fragment, sGlsl, sErr) == false);
	CHECK_CONTAINS(sErr, "aNotDeclared");
}

//-----------------------------------------------------------------------

int main()
{
	TestClearPair();
	TestNullPair();
	TestFragPositionUsed();
	TestDepthonlyFrag();
	TestPosteffectQuadVtx();
	TestVertexTexCoord1Builtin();
	TestDebugOverdrawFrag();
	TestMulRejectsWrongArgCount();
	TestSampleRejectsUnknownTexture();

	if (gFailures == 0)
	{
		std::printf("All HpslTranspilerTests passed.\n");
		return 0;
	}
	std::fprintf(stderr, "%d HpslTranspilerTests check(s) failed.\n", gFailures);
	return 1;
}
