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

// Verbatim from deferred_base_vtx.hpsl's "TEMP BACKWARD COMPATIBILITY"
// @else-UseTextureBuffer branch (lines 517-569 of a real SOMA install's
// copy) - the single flat "cBuffer cVertexArguments" block real material
// shaders declare when UseTextureBuffer is left undefined (see
// PORTING_NOTES.md and HpslTranspiler.h for why that's the combo this port
// targets). Trimmed to just the cBuffer block plus a trivial main() that
// references one flattened member unqualified, exactly as the real file's
// body does (e.g. "mul(a_mtxModelViewProjection, ...)", never
// "cVertexArguments.a_mtx...").
static const char* gpsVertexArgumentsCBuffer =
	"cBuffer cVertexArguments //make sure the struct in c++ has the same layout!\n"
	"{\n"
	"	//////////////\n"
	"	// Default input\n"
	"	cMatrixf a_mtxProjection;\n"
	"	cMatrixf a_mtxModelViewProjection;\n"
	"	cMatrixf a_mtxModelView;\n"
	"	cMatrixf a_mtxUV;\n"
	"	cMatrixf a_mtxModel;\n"
	"	cMatrixf a_mtxNormal;\n"
	"\n"
	"	float afInvFarPlane;\n"
	"	cVector4f avColorMul;\n"
	"	int alInstanceOffset;\n"
	"};\n"
	"\n"
	"void main(in cVector4f vtx_vPosition,\n"
	"		  out cVector4f px_vPosition)\n"
	"{	\n"
	"	px_vPosition = mul(a_mtxModelViewProjection, vtx_vPosition);\n"
	"}";

static void TestConstantBufferFlattening()
{
	tString sGlsl, sErr;
	CHECK(TranspileHpslToGlsl(gpsVertexArgumentsCBuffer, eGpuShaderType_Vertex, sGlsl, sErr));

	// The "cBuffer NAME { ... };" wrapper itself must be gone - GLSL 120
	// has no such syntax.
	CHECK_NOT_CONTAINS(sGlsl, "cBuffer");
	CHECK_NOT_CONTAINS(sGlsl, "cVertexArguments");

	// Every member becomes its own top-level "uniform TYPE NAME;",
	// unqualified, using the same type mapping as any other uniform.
	CHECK_CONTAINS(sGlsl, "uniform mat4 a_mtxModelViewProjection;");
	CHECK_CONTAINS(sGlsl, "uniform mat4 a_mtxNormal;");
	CHECK_CONTAINS(sGlsl, "uniform float afInvFarPlane;");
	CHECK_CONTAINS(sGlsl, "uniform vec4 avColorMul;");
	CHECK_CONTAINS(sGlsl, "uniform int alInstanceOffset;");

	// The body's unqualified reference to a flattened member still
	// resolves (same name, now a plain global uniform instead of a
	// cbuffer member) - proves the flattening didn't just declare the
	// uniforms but leave the body unable to use them.
	CHECK_CONTAINS(sGlsl, "gl_Position = (a_mtxModelViewProjection * gl_Vertex)");
}

// Exercises a #define *inside* a cBuffer body (real case: helper_type_
// arguments.hpsl-adjacent deferred_base_vtx.hpsl's "cBuffer cSkinningData"
// block, gated behind UseSkeleton, defines "kMaxBones" this way right
// before using it in an array size) - the #define must survive verbatim
// (it's real C-preprocessor syntax GLSL itself understands, not something
// FlattenConstantBuffers should try to interpret), and an array member
// must still get "uniform " prepended like any other declaration.
static void TestConstantBufferPreservesDefine()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"cBuffer cSkinningData\n"
		"{\n"
		"	#define kMaxBones 96\n"
		"	cVector4f avDualQuatBones[kMaxBones*2];\n"
		"};\n"
		"\n"
		"void main(out cVector4f px_vColor : 0)\n"
		"{\n"
		"	px_vColor = avDualQuatBones[0];\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "#define kMaxBones 96");
	CHECK_CONTAINS(sGlsl, "uniform vec4 avDualQuatBones[kMaxBones*2];");
}

// A "cBuffer NAME : N { ... };" with the D3D-register-style binding index
// (helper_type_arguments.hpsl's actual spelling, e.g. "cBuffer
// cInstanceArguments : 2") - the ": N" must be discarded along with the
// rest of the header, same as a texture uniform's own ": N" suffix.
static void TestConstantBufferWithBindingIndex()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"cBuffer cInstanceArguments : 2\n"
		"{\n"
		"	int alInstanceOffset;\n"
		"	int alInstanceStride;\n"
		"};\n"
		"\n"
		"void main(out cVector4f px_vColor : 0)\n"
		"{\n"
		"	px_vColor = cVector4f(float(alInstanceOffset + alInstanceStride));\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_NOT_CONTAINS(sGlsl, "cBuffer");
	CHECK_NOT_CONTAINS(sGlsl, ": 2");
	CHECK_CONTAINS(sGlsl, "uniform int alInstanceOffset;");
	CHECK_CONTAINS(sGlsl, "uniform int alInstanceStride;");
}

// Exercises the custom-"attribute"-fallback path for vertex inputs with no
// known GLSL 120 fixed-function built-in (e.g. deferred_base_vtx.hpsl's
// vtx_vTangent/vtx_vBoneIndices/vtx_vBoneWeight - real, unconditional main()
// parameters in that file even when skinning/normal-mapping combo vars are
// off). Must compile (not the old hard error) and must NOT silently alias
// onto a gl_MultiTexCoordN slot.
static void TestUnknownVertexInputBecomesAttribute()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"void main(in cVector4f vtx_vPosition,\n"
		"		  in cVector4f vtx_vTangent,\n"
		"		  in cVector4f vtx_vBoneIndices,\n"
		"		  in cVector4f vtx_vBoneWeight,\n"
		"		  out cVector4f px_vPosition)\n"
		"{	\n"
		"	px_vPosition = vtx_vPosition + vtx_vTangent + vtx_vBoneIndices + vtx_vBoneWeight;\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Vertex, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "attribute vec4 vtx_vTangent;");
	CHECK_CONTAINS(sGlsl, "attribute vec4 vtx_vBoneIndices;");
	CHECK_CONTAINS(sGlsl, "attribute vec4 vtx_vBoneWeight;");
	// vtx_vTangent must keep its own name in the body, not get rewritten
	// onto gl_MultiTexCoord1 (that would collide with a real
	// vtx_vTexCoord1 input elsewhere - see HpslTranspiler.cpp's comment at
	// this fallback for why).
	CHECK_CONTAINS(sGlsl, "gl_Vertex + vtx_vTangent + vtx_vBoneIndices + vtx_vBoneWeight");
}

// cTexture3D (real use: deferred_base_frag.hpsl's dissolve map,
// "uniform cTexture3D aDissolveMap : 14;") must map to sampler3D/texture3D,
// same pattern as the other texture types.
static void TestTexture3D()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"uniform cTexture3D aDissolveMap : 14;\n"
		"\n"
		"void main(in cVector4f px_vPosition, out cVector4f out_vColor : 0)\n"
		"{\n"
		"	out_vColor = sample(aDissolveMap, cVector3f(0.0, 0.0, 0.0));\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "uniform sampler3D aDissolveMap;");
	CHECK_CONTAINS(sGlsl, "texture3D(aDissolveMap, vec3(0.0, 0.0, 0.0))");
}

// cMatrix3f (real use: deferred_base_vtx.hpsl's normal matrix,
// "cMatrix3f mtxNormal = cMatrix3f(a_mtxNormal);") must map to mat3 - a real
// bug this pass's live glCompileShader() self-test caught (see
// PORTING_NOTES.md): the syntax-level checks in this file didn't know to
// look for it until the live compile failed with "syntax error, unexpected
// NEW_IDENTIFIER" at exactly this line, because an unmapped type name is
// silently passed through unchanged rather than erroring - GLSL's own
// parser was the only thing that caught it. Locked in here so a regression
// doesn't need a live GPU to catch again.
static void TestMatrix3f()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"uniform cMatrixf a_mtxNormal;\n"
		"\n"
		"void main(out cVector4f px_vColor : 0)\n"
		"{\n"
		"	cMatrix3f mtxNormal = cMatrix3f(a_mtxNormal);\n"
		"	px_vColor = cVector4f(mtxNormal[0], 1.0);\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "mat3 mtxNormal = mat3(a_mtxNormal);");
	CHECK_NOT_CONTAINS(sGlsl, "cMatrix3f");
}

// Verbatim (parameter list only) from deferred_gbuffer_solid_frag.hpsl's
// main() - a real, live-found bug (see PORTING_NOTES.md): a trailing
// "//comment" after a parameter's ": N" semantic, where the comment text
// itself contains a comma ("//diffuse rgb, translucency a"), used to
// corrupt SplitParams()'s naive comma-split into bogus pieces that failed
// ParseParam()'s regex. Found by a different concurrent session's real
// GpuShaderManager wiring work, fixed here (StripLineComments()).
static void TestParameterListTrailingCommentWithComma()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"void main(in cVector4f px_vPosition,\n"
		"		  out cVector4f out_vDiffuse : 0,		//diffuse rgb, translucency a\n"
		"		  out cVector4f out_vNormal : 1,			//normal xyz, depth w\n"
		"		  out cVector4f out_vSpecular : 2)		//spec color rgb, spec power a\n"
		"{\n"
		"	out_vDiffuse = px_vPosition;\n"
		"	out_vNormal = px_vPosition;\n"
		"	out_vSpecular = px_vPosition;\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_FragData[0] = gl_FragCoord");
	CHECK_CONTAINS(sGlsl, "gl_FragData[1] = gl_FragCoord");
	CHECK_CONTAINS(sGlsl, "gl_FragData[2] = gl_FragCoord");
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
	TestConstantBufferFlattening();
	TestConstantBufferPreservesDefine();
	TestConstantBufferWithBindingIndex();
	TestUnknownVertexInputBecomesAttribute();
	TestTexture3D();
	TestMatrix3f();
	TestParameterListTrailingCommentWithComma();

	if (gFailures == 0)
	{
		std::printf("All HpslTranspilerTests passed.\n");
		return 0;
	}
	std::fprintf(stderr, "%d HpslTranspilerTests check(s) failed.\n", gFailures);
	return 1;
}
