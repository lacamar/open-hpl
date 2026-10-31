#include "impl/GlslToVulkan.h"

#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>

using namespace hpl;

static int gFailures = 0;

static bool Check(const char *apName, const std::string& asVtx, const std::string& asFrag, bool abQuiet=false)
{
	cGlslVkProgram prog;
	if(!GlslToVulkan(asVtx, asFrag, prog))
	{
		printf("FAIL %s: rewrite: %s\n", apName, prog.msError.c_str());
		++gFailures;
		return false;
	}
	for(int s=0; s<2; ++s)
	{
		const std::string& src = s==0 ? prog.msVertex : prog.msFragment;
		if((s==0 ? asVtx : asFrag).empty()) continue;
		std::vector<unsigned int> spv;
		std::string log;
		if(!CompileGlslToSpirv(src, s==0, spv, log))
		{
			printf("FAIL %s (%s): %s\n", apName, s==0 ? "vert" : "frag", log.c_str());
			if(!abQuiet)
			{
				int n = 1;
				std::istringstream in(src);
				for(std::string l; std::getline(in, l); ) printf("%4d %s\n", n++, l.c_str());
			}
			++gFailures;
			return false;
		}
	}
	return true;
}

static std::string ReadFile(const std::string& p)
{
	std::ifstream f(p);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

int main(int argc, char **argv)
{
	const char *pVtx =
		"#version 120\n"
		"uniform mat4 a_mtxUV; // comment\n"
		"uniform vec3 avA, avB[2];\n"
		"varying vec3 gvNormal;\n"
		"attribute vec4 vtx_vBoneWeight;\n"
		"invariant gl_Position;\n"
		"void main(){ gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; gvNormal = gl_NormalMatrix * gl_Normal + avB[1];\n"
		"  gl_TexCoord[0] = a_mtxUV * gl_MultiTexCoord0; gl_FrontColor = gl_Color * vtx_vBoneWeight; }\n";
	const char *pFrag =
		"#version 120\n#extension GL_ARB_texture_rectangle : enable\n"
		"uniform sampler2D diffuseMap;\nuniform sampler2DRect aDepth;\nuniform sampler2DShadow aShadow;\n"
		"uniform vec3 avA;\nuniform float afFalloff = 0.5f;\n"
		"varying vec3 gvNormal;\n"
		"void main(){ vec4 c = texture2D(diffuseMap, gl_TexCoord[0].xy) * gl_Color;\n"
		"  c += texture2DRect(aDepth, gl_FragCoord.xy) * shadow2D(aShadow, gvNormal).x;\n"
		"  gl_FragData[0] = c * afFalloff; gl_FragData[1] = vec4(avA, 1.0); }\n";
	Check("sample", pVtx, pFrag);

	cGlslVkProgram prog;
	GlslToVulkan(pVtx, pFrag, prog);
	if(prog.mvSamplers.size() != 3 || prog.mvSamplers[1].msType != "sampler2D" || prog.mlFragOutputs != 2 ||
	   prog.mvInits.size() != 1 || prog.mvInits[0].mvValues.size() != 1 || prog.mvInits[0].mvValues[0] != 0.5f ||
	   prog.mlAttribMask != ((1u<<eGlslVkAttrib_Vertex)|(1u<<eGlslVkAttrib_Normal)|(1u<<eGlslVkAttrib_Color)|(1u<<eGlslVkAttrib_MultiTexCoord0)))
	{
		printf("FAIL sample metadata\n");
		++gFailures;
	}

	// Corpus: dir of <n>_<name>.vert/.frag dumped with OPENHPL_DUMP_SHADERS_DIR
	int lCount = 0;
	for(int a=1; a<argc; ++a)
	{
		DIR *pDir = opendir(argv[a]);
		if(!pDir) continue;
		while(dirent *e = readdir(pDir))
		{
			std::string n = e->d_name;
			bool bVtx = n.size()>5 && n.compare(n.size()-5, 5, ".vert")==0;
			bool bFrag = n.size()>5 && n.compare(n.size()-5, 5, ".frag")==0;
			if(!bVtx && !bFrag) continue;
			std::string src = ReadFile(std::string(argv[a]) + "/" + n);
			Check(n.c_str(), bVtx ? src : "", bFrag ? src : "", getenv("GLSLVK_VERBOSE") == NULL);
			++lCount;
		}
		closedir(pDir);
	}
	printf("%d corpus shaders, %d failures\n", lCount, gFailures);
	return gFailures ? 1 : 0;
}
