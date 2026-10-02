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

static const char* gpsClearVtx =
	"void main(in cVector4f vtx_vPosition,\n"
	"		  in cVector4f vtx_vColor, \n"
	"		  out cVector4f px_vColor,\n"
	"		  out cVector4f px_vPosition)\n"
	"{	\n"
	"	px_vPosition = vtx_vPosition;\n"
	"	px_vColor = vtx_vColor;\n"
	"}";

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

static const char* gpsDepthonlyFrag =
	"void main(in cVector4f px_vPosition,\n"
	"		  out cVector4f px_vColor : 0)\n"
	"{\n"
	"	px_vColor = cVector4f(1.0);\n"
	"}";

// UseUvCoord1 undefined
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

static const char* gpsDebugOverdrawFrag =
	"void main(in cVector4f px_vPosition,\n"
	"		  out cVector4f out_vColor : 0)\n"
	"{\n"
	"	out_vColor = cVector4f(1.0f / 48.0f);\n"
	"}";

static void TestClearPair()
{
	tString sGlsl, sErr;

	CHECK(TranspileHpslToGlsl(gpsClearVtx, eGpuShaderType_Vertex, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_Position = gl_Vertex");
	CHECK_CONTAINS(sGlsl, "varying vec4 px_vColor");
	CHECK_NOT_CONTAINS(sGlsl, "varying vec4 px_vPosition");

	CHECK(TranspileHpslToGlsl(gpsClearFrag, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_FragData[0]");
	CHECK_CONTAINS(sGlsl, "gl_FragData[3]");
	CHECK_CONTAINS(sGlsl, "#extension GL_ARB_draw_buffers");
}

static void TestNullPair()
{
	tString sGlsl, sErr;

	CHECK(TranspileHpslToGlsl(gpsNullVtx, eGpuShaderType_Vertex, sGlsl, sErr));
	CHECK_NOT_CONTAINS(sGlsl, "a_mtxModelViewProjection");
	CHECK_CONTAINS(sGlsl, "gl_Position = (gl_ModelViewProjectionMatrix * gl_Vertex)");
	CHECK_NOT_CONTAINS(sGlsl, "mul(");

	CHECK(TranspileHpslToGlsl(gpsNullFrag, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "uniform sampler2D aColorMap;"); // ": 0" binding index stripped
	CHECK_NOT_CONTAINS(sGlsl, "aColorMap : 0");
	CHECK_CONTAINS(sGlsl, "texture2D(aColorMap, px_vTexCoord0.xy)");
	CHECK_NOT_CONTAINS(sGlsl, "sample(");
	CHECK_NOT_CONTAINS(sGlsl, "varying vec4 px_vPosition");
}

static void TestFragPositionUsed()
{
	// synthetic: no real file uses px_vPosition in a fragment body
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
	CHECK_NOT_CONTAINS(sGlsl, "varying vec4 px_vPosition");
}

static void TestPosteffectQuadVtx()
{
	tString sGlsl, sErr;
	CHECK(TranspileHpslToGlsl(gpsPosteffectQuadVtx, eGpuShaderType_Vertex, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "gl_Position = gl_Vertex * vec4(2.0,-2.0, 0.0, 0.0) + vec4(-1.0, 1.0, 0.0, 1.0)");
	CHECK_CONTAINS(sGlsl, "varying vec4 px_vTexCoord0");
}

static void TestVertexTexCoord1Builtin()
{
	// unverified guess; no real file references it
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

	CHECK_NOT_CONTAINS(sGlsl, "cBuffer");
	CHECK_NOT_CONTAINS(sGlsl, "cVertexArguments");

	// a_mtxModel: no fixed-function equivalent, stays a uniform
	CHECK_CONTAINS(sGlsl, "uniform mat4 a_mtxModel;");
	CHECK_CONTAINS(sGlsl, "uniform float afInvFarPlane;");
	CHECK_CONTAINS(sGlsl, "uniform vec4 avColorMul;");
	CHECK_CONTAINS(sGlsl, "uniform int alInstanceOffset;");

	CHECK_CONTAINS(sGlsl, "gl_Position = (gl_ModelViewProjectionMatrix * gl_Vertex)");
}

// #define inside a cBuffer body must survive verbatim
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

// ": N" register binding is discarded
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

// vertex inputs without a built-in fall back to plain attributes
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
	CHECK_NOT_CONTAINS(sGlsl, "attribute vec4 vtx_vTangent;");
	CHECK_CONTAINS(sGlsl, "attribute vec4 vtx_vBoneIndices;");
	CHECK_CONTAINS(sGlsl, "attribute vec4 vtx_vBoneWeight;");
	CHECK_CONTAINS(sGlsl, "gl_Vertex + gl_MultiTexCoord1 + vtx_vBoneIndices + vtx_vBoneWeight");
}

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

// non-projective shadow2D: call sites pass no .w
static void TestShadowSample()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"uniform cTexture2DCmp aShadowMap : 6;\n"
		"\n"
		"float ShadowOffsetLookup(cTexture2DCmp aTex, cVector4f avLocation, cVector2f avOffset)\n"
		"{\n"
		"	return sampleCmp(aTex, avLocation.xy + avOffset, avLocation.z);\n"
		"}\n"
		"\n"
		"void main(in cVector4f px_vPosition, out cVector4f out_vColor : 0)\n"
		"{\n"
		"	float fShadowAmount = sampleCmp(aShadowMap, px_vPosition.xy, px_vPosition.z);\n"
		"	out_vColor = cVector4f(fShadowAmount);\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "uniform sampler2DShadow aShadowMap;");
	CHECK_CONTAINS(sGlsl, "sampler2DShadow aTex");
	CHECK_CONTAINS(sGlsl, "shadow2D(aTex, vec3(avLocation.xy + avOffset, avLocation.z)).x");
	CHECK_CONTAINS(sGlsl, "shadow2D(aShadowMap, vec3(gl_FragCoord.xy, gl_FragCoord.z)).x");
	CHECK_NOT_CONTAINS(sGlsl, "sampleCmp(");
	CHECK_NOT_CONTAINS(sGlsl, "cTexture2DCmp");
}

static void TestSampleCmpRejectsWrongArgCount()
{
	tString sGlsl, sErr;
	static const char* psBadSampleCmp =
		"void main(out cVector4f px_vColor : 0)\n"
		"{\n"
		"	px_vColor = cVector4f(sampleCmp(a, b));\n"
		"}";
	CHECK(TranspileHpslToGlsl(psBadSampleCmp, eGpuShaderType_Fragment, sGlsl, sErr) == false);
	CHECK_CONTAINS(sErr, "sampleCmp()");
}

// load() -> texelFetch, needs #version 130
static void TestLoadBecomesTexelFetch()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"uniform cTexture2D aNormalDepthMap : 4;\n"
		"\n"
		"void main(in cVector4f px_vPosition, out cVector4f out_vColor : 0)\n"
		"{\n"
		"	cVector2l vMapCoords = cVector2l(gl_FragCoord.xy);\n"
		"	out_vColor = load(aNormalDepthMap, vMapCoords, 0);\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "#version 130");
	CHECK_NOT_CONTAINS(sGlsl, "#version 120");
	CHECK_CONTAINS(sGlsl, "ivec2 vMapCoords = ivec2(gl_FragCoord.xy);");
	CHECK_CONTAINS(sGlsl, "texelFetch(aNormalDepthMap, vMapCoords, 0)");
	CHECK_NOT_CONTAINS(sGlsl, "load(");
	CHECK_NOT_CONTAINS(sGlsl, "cVector2l");
}

// #version 130 only when load() is used
static void TestNoLoadKeepsVersion120()
{
	tString sGlsl, sErr;
	CHECK(TranspileHpslToGlsl(gpsNullVtx, eGpuShaderType_Vertex, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "#version 120");
	CHECK_NOT_CONTAINS(sGlsl, "#version 130");
}

static void TestLoadRejectsNonSampler2D()
{
	tString sGlsl, sErr;
	static const char* psBadLoad =
		"uniform cTextureCube aEnvMap : 0;\n"
		"\n"
		"void main(out cVector4f px_vColor : 0)\n"
		"{\n"
		"	px_vColor = load(aEnvMap, cVector2l(0, 0), 0);\n"
		"}";
	CHECK(TranspileHpslToGlsl(psBadLoad, eGpuShaderType_Fragment, sGlsl, sErr) == false);
	CHECK_CONTAINS(sErr, "aEnvMap");
}

// a_mtxUV stays a uniform: fed by name, no fixed-function equivalent
static void TestFixedFunctionMatrixSubstitution()
{
	tString sGlsl, sErr;
	static const char* psSrc =
		"cBuffer cVertexArguments\n"
		"{\n"
		"	cMatrixf a_mtxProjection;\n"
		"	cMatrixf a_mtxModelViewProjection;\n"
		"	cMatrixf a_mtxModelView;\n"
		"	cMatrixf a_mtxUV;\n"
		"	cMatrixf a_mtxModel;\n"
		"	cMatrixf a_mtxNormal;\n"
		"};\n"
		"\n"
		"void main(in cVector4f vtx_vPosition, out cVector4f px_vPosition)\n"
		"{\n"
		"	cVector4f vLocalVertexPos = cVector4f(vtx_vPosition.xyz, 1.0);\n"
		"	px_vPosition = mul(a_mtxModelViewProjection, vLocalVertexPos);\n"
		"	cMatrix3f mtxNormal = cMatrix3f(a_mtxNormal);\n"
		"	cVector4f vUv = mul(a_mtxUV, vtx_vPosition);\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Vertex, sGlsl, sErr));

	CHECK_CONTAINS(sGlsl, "gl_Position = (gl_ModelViewProjectionMatrix * vLocalVertexPos)");
	CHECK_CONTAINS(sGlsl, "mat3(gl_NormalMatrix)");
	CHECK_NOT_CONTAINS(sGlsl, "a_mtxModelViewProjection");
	CHECK_NOT_CONTAINS(sGlsl, "a_mtxNormal");
	CHECK_NOT_CONTAINS(sGlsl, "uniform mat4 a_mtxModelViewProjection");
	CHECK_NOT_CONTAINS(sGlsl, "uniform mat4 a_mtxModelView;");
	CHECK_NOT_CONTAINS(sGlsl, "uniform mat4 a_mtxProjection;");
	CHECK_NOT_CONTAINS(sGlsl, "uniform mat4 a_mtxNormal;");

	CHECK_CONTAINS(sGlsl, "uniform mat4 a_mtxUV;");
	CHECK_CONTAINS(sGlsl, "(a_mtxUV * gl_Vertex)");
	CHECK_NOT_CONTAINS(sGlsl, "gl_ModelViewMatrix");
}

static void TestMatrix3f()
{
	// not a_mtxNormal: that name is rewritten by the matrix substitution
	tString sGlsl, sErr;
	static const char* psSrc =
		"uniform cMatrixf a_mtxCustomNormal;\n"
		"\n"
		"void main(out cVector4f px_vColor : 0)\n"
		"{\n"
		"	cMatrix3f mtxNormal = cMatrix3f(a_mtxCustomNormal);\n"
		"	px_vColor = cVector4f(mtxNormal[0], 1.0);\n"
		"}";
	CHECK(TranspileHpslToGlsl(psSrc, eGpuShaderType_Fragment, sGlsl, sErr));
	CHECK_CONTAINS(sGlsl, "mat3 mtxNormal = mat3(a_mtxCustomNormal);");
	CHECK_NOT_CONTAINS(sGlsl, "cMatrix3f");
}

// trailing comment containing a comma must not split params
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
	TestShadowSample();
	TestSampleCmpRejectsWrongArgCount();
	TestLoadBecomesTexelFetch();
	TestNoLoadKeepsVersion120();
	TestLoadRejectsNonSampler2D();
	TestFixedFunctionMatrixSubstitution();

	if (gFailures == 0)
	{
		std::printf("All HpslTranspilerTests passed.\n");
		return 0;
	}
	std::fprintf(stderr, "%d HpslTranspilerTests check(s) failed.\n", gFailures);
	return 1;
}
