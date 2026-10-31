#include "impl/GlslToVulkan.h"

#include <cstdlib>
#include <map>
#include <regex>
#include <set>
#include <sstream>

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

namespace hpl {

	namespace {

		struct cDecl
		{
			std::string msQualifier; // uniform, varying, attribute
			std::string msType;
			std::string msName;
			std::string msArray; // "[N]" or ""
			std::string msInit;
		};

		struct cStage
		{
			std::string msBody;
			std::vector<cDecl> mvDecls;
		};

		std::string StripComments(const std::string& s)
		{
			std::string out;
			out.reserve(s.size());
			for(size_t i=0; i<s.size(); ++i)
			{
				if(s[i]=='/' && i+1<s.size() && s[i+1]=='/')
				{
					while(i<s.size() && s[i]!='\n') ++i;
					out += '\n';
				}
				else if(s[i]=='/' && i+1<s.size() && s[i+1]=='*')
				{
					i += 2;
					while(i+1<s.size() && !(s[i]=='*' && s[i+1]=='/')) { if(s[i]=='\n') out += '\n'; ++i; }
					++i;
					out += ' ';
				}
				else out += s[i];
			}
			return out;
		}

		std::string Trim(const std::string& s)
		{
			size_t a = s.find_first_not_of(" \t\r\n");
			if(a == std::string::npos) return "";
			size_t b = s.find_last_not_of(" \t\r\n");
			return s.substr(a, b-a+1);
		}

		std::vector<std::string> SplitTopLevel(const std::string& s, char sep)
		{
			std::vector<std::string> v;
			int depth = 0;
			std::string cur;
			for(char c : s)
			{
				if(c=='(' || c=='[') ++depth;
				else if(c==')' || c==']') --depth;
				if(c==sep && depth==0) { v.push_back(cur); cur.clear(); }
				else cur += c;
			}
			v.push_back(cur);
			return v;
		}

		// Pulls global uniform/varying/attribute declarations out of the body.
		bool ParseStage(const std::string& asSrc, cStage& aStage, std::string& asErr)
		{
			std::string src = StripComments(asSrc);
			std::string body;
			std::istringstream lines(src);
			for(std::string line; std::getline(lines, line); )
			{
				std::string t = Trim(line);
				if(t.compare(0, 8, "#version")==0 || t.compare(0, 10, "#extension")==0) { body += '\n'; continue; }
				body += line + '\n';
			}

			static const std::regex declRe("^\\s*(?:(?:invariant|centroid|flat|smooth|noperspective|highp|mediump|lowp)\\s+)*(uniform|varying|attribute)\\s+(?:(?:highp|mediump|lowp)\\s+)?(\\w+)\\s+([\\s\\S]*)$");
			static const std::regex nameRe("^\\s*(\\w+)\\s*(\\[[^\\]]*\\])?\\s*(?:=([\\s\\S]*))?$");

			int depth = 0;
			size_t start = 0;
			for(size_t i=0; i<body.size(); ++i)
			{
				char c = body[i];
				// Preprocessor lines end statements without a ';'
				if(c=='#' && depth==0 && (i==0 || body[i-1]=='\n'))
				{
					while(i<body.size() && body[i]!='\n') ++i;
					start = i+1;
					continue;
				}
				if(c=='{') ++depth;
				else if(c=='}') { --depth; if(depth==0) start = i+1; }
				else if(c==';' && depth==0)
				{
					std::string stmt = body.substr(start, i-start);
					std::smatch m;
					if(std::regex_match(stmt, m, declRe))
					{
						for(const std::string& part : SplitTopLevel(m[3].str(), ','))
						{
							std::smatch n;
							if(!std::regex_match(part, n, nameRe)) { asErr = "can't parse declaration: " + Trim(stmt); return false; }
							aStage.mvDecls.push_back({m[1].str(), m[2].str(), n[1].str(), n[2].str(), Trim(n[3].str())});
						}
						for(size_t j=start; j<=i; ++j) if(body[j]!='\n') body[j] = ' ';
					}
					start = i+1;
				}
			}
			aStage.msBody = body;
			return true;
		}

		bool IsSampler(const std::string& asType){ return asType.compare(0, 7, "sampler")==0; }

		int LocationCount(const std::string& asType, const std::string& asArray)
		{
			int n = 1;
			if(asType=="mat2") n = 2;
			else if(asType=="mat3") n = 3;
			else if(asType=="mat4") n = 4;
			if(!asArray.empty()) n *= std::atoi(asArray.c_str()+1);
			return n;
		}

		bool Uses(const std::string& s, const std::string& asWord)
		{
			return std::regex_search(s, std::regex("\\b" + asWord + "\\b"));
		}

		void ReplaceWord(std::string& s, const std::string& asFrom, const std::string& asTo)
		{
			s = std::regex_replace(s, std::regex("\\b" + asFrom + "\\b"), asTo);
		}

		void ReplaceCall(std::string& s, const std::string& asFrom, const std::string& asTo)
		{
			s = std::regex_replace(s, std::regex("\\b" + asFrom + "\\s*\\("), asTo + "(");
		}

		struct cBuiltinUniform { const char *mpGl; const char *mpHpl; const char *mpType; };
		const cBuiltinUniform gvBuiltinUniforms[] = {
			{"gl_ModelViewProjectionMatrix", "hpl_ModelViewProjectionMatrix", "mat4"},
			{"gl_ModelViewMatrix", "hpl_ModelViewMatrix", "mat4"},
			{"gl_ProjectionMatrix", "hpl_ProjectionMatrix", "mat4"},
			{"gl_NormalMatrix", "hpl_NormalMatrix", "mat3"},
		};

		struct cBuiltinAttrib { const char *mpGl; const char *mpType; int mlLoc; };
		const cBuiltinAttrib gvBuiltinAttribs[] = {
			{"gl_Vertex", "vec4", eGlslVkAttrib_Vertex},
			{"gl_Normal", "vec3", eGlslVkAttrib_Normal},
			{"gl_Color", "vec4", eGlslVkAttrib_Color},
			{"gl_SecondaryColor", "vec4", eGlslVkAttrib_SecondaryColor},
			{"gl_MultiTexCoord0", "vec4", eGlslVkAttrib_MultiTexCoord0+0},
			{"gl_MultiTexCoord1", "vec4", eGlslVkAttrib_MultiTexCoord0+1},
			{"gl_MultiTexCoord2", "vec4", eGlslVkAttrib_MultiTexCoord0+2},
			{"gl_MultiTexCoord3", "vec4", eGlslVkAttrib_MultiTexCoord0+3},
			{"gl_MultiTexCoord4", "vec4", eGlslVkAttrib_MultiTexCoord0+4},
			{"gl_MultiTexCoord5", "vec4", eGlslVkAttrib_MultiTexCoord0+5},
			{"gl_MultiTexCoord6", "vec4", eGlslVkAttrib_MultiTexCoord0+6},
			{"gl_MultiTexCoord7", "vec4", eGlslVkAttrib_MultiTexCoord0+7},
		};

		const int kTexCoordLoc = 0, kFrontColorLoc = 8, kFrontSecondaryColorLoc = 9, kFirstUserVaryingLoc = 10;

		const char *gpHelpers[][2] = {
			{"hpl_shadow2DProj", "vec4 hpl_shadow2DProj(sampler2DShadow s, vec4 c){ return vec4(textureProj(s, c)); }\n"},
			{"hpl_shadow2D", "vec4 hpl_shadow2D(sampler2DShadow s, vec3 c){ return vec4(texture(s, c)); }\n"},
			{"hpl_textureRectProj", "vec4 hpl_textureRectProj(sampler2D s, vec3 c){ return texture(s, c.xy / (c.z * vec2(textureSize(s, 0)))); }\n"
									"vec4 hpl_textureRectProj(sampler2D s, vec4 c){ return texture(s, c.xy / (c.w * vec2(textureSize(s, 0)))); }\n"},
			{"hpl_textureRect", "vec4 hpl_textureRect(sampler2D s, vec2 c){ return texture(s, c / vec2(textureSize(s, 0))); }\n"},
			{"hpl_texelFetchRect", "vec4 hpl_texelFetchRect(sampler2D s, ivec2 p){ return texelFetch(s, p, 0); }\n"},
		};
	}

	bool GlslToVulkan(const std::string& asVertex, const std::string& asFragment, cGlslVkProgram& aOut)
	{
		cStage stages[2];
		if(!ParseStage(asVertex, stages[0], aOut.msError)) return false;
		if(!ParseStage(asFragment, stages[1], aOut.msError)) return false;

		std::vector<cDecl> vUniforms;
		std::map<std::string, size_t> mapUniforms;
		std::map<std::string, int> mapSamplerBinding;
		std::set<std::string> setRectSamplers;
		std::vector<cDecl> vVaryings;
		std::map<std::string, int> mapVaryingLoc;
		int lNextLoc = kFirstUserVaryingLoc;

		for(int s=0; s<2; ++s)
		{
			for(const cDecl& d : stages[s].mvDecls)
			{
				if(d.msQualifier == "uniform")
				{
					if(IsSampler(d.msType))
					{
						if(mapSamplerBinding.count(d.msName)) continue;
						std::string sType = d.msType == "sampler2DRect" ? "sampler2D" : d.msType == "sampler2DRectShadow" ? "sampler2DShadow" : d.msType;
						if(d.msType.find("Rect") != std::string::npos) setRectSamplers.insert(d.msName);
						int lBinding = kGlslVkFirstSamplerBinding + (int)aOut.mvSamplers.size();
						mapSamplerBinding[d.msName] = lBinding;
						aOut.mvSamplers.push_back({d.msName, sType, lBinding});
						continue;
					}
					auto it = mapUniforms.find(d.msName);
					if(it != mapUniforms.end())
					{
						if(vUniforms[it->second].msType != d.msType) { aOut.msError = "uniform type mismatch: " + d.msName; return false; }
						continue;
					}
					mapUniforms[d.msName] = vUniforms.size();
					vUniforms.push_back(d);
					if(!d.msInit.empty())
					{
						cGlslVkUniformInit init{d.msName, {}};
						static const std::regex numRe("[-+]?(\\d+\\.?\\d*|\\.\\d+)([eE][-+]?\\d+)?");
						for(auto m = std::sregex_iterator(d.msInit.begin(), d.msInit.end(), numRe); m != std::sregex_iterator(); ++m)
							init.mvValues.push_back(std::strtof(m->str().c_str(), NULL));
						aOut.mvInits.push_back(init);
					}
				}
				else if(d.msQualifier == "varying" && !mapVaryingLoc.count(d.msName))
				{
					mapVaryingLoc[d.msName] = lNextLoc;
					lNextLoc += LocationCount(d.msType, d.msArray);
					vVaryings.push_back(d);
				}
			}
		}

		std::string sBlock = "layout(std140, set=0, binding=0) uniform hpl_Uniforms\n{\n\tvec4 hpl_AlphaTest;\n";
		for(const cBuiltinUniform& u : gvBuiltinUniforms)
			if(Uses(stages[0].msBody, u.mpGl) || Uses(stages[1].msBody, u.mpGl) || (u.mpGl == std::string("gl_ModelViewProjectionMatrix") && Uses(stages[0].msBody, "ftransform")))
				sBlock += std::string("\t") + u.mpType + " " + u.mpHpl + ";\n";
		for(const cDecl& u : vUniforms)
			sBlock += "\t" + u.msType + " " + u.msName + u.msArray + ";\n";
		sBlock += "};\n";
		for(const cGlslVkSampler& smp : aOut.mvSamplers)
			sBlock += "layout(set=0, binding=" + std::to_string(smp.mlBinding) + ") uniform " + smp.msType + " " + smp.msName + ";\n";

		for(int s=0; s<2; ++s)
		{
			std::string& body = stages[s].msBody;
			bool bVtx = s==0;
			std::string sHead = "#version 450\n";

			if(bVtx)
			{
				body = std::regex_replace(body, std::regex("\\bftransform\\s*\\(\\s*\\)"), "(gl_ModelViewProjectionMatrix * gl_Vertex)");
				for(const cBuiltinAttrib& a : gvBuiltinAttribs)
				{
					if(!Uses(body, a.mpGl)) continue;
					sHead += "layout(location=" + std::to_string(a.mlLoc) + ") in " + a.mpType + " hpl_" + std::string(a.mpGl+3) + ";\n";
					ReplaceWord(body, a.mpGl, std::string("hpl_") + (a.mpGl+3));
					aOut.mlAttribMask |= 1u << a.mlLoc;
				}
				// Unbound generic attributes read GL's default current value
				for(const cDecl& d : stages[s].mvDecls)
					if(d.msQualifier == "attribute")
						sHead += "const " + d.msType + " " + d.msName + " = " + d.msType + "(vec4(0.0, 0.0, 0.0, 1.0));\n";
			}

			const char *pIO = bVtx ? "out" : "in";
			if(Uses(body, "gl_TexCoord"))
			{
				sHead += "layout(location=" + std::to_string(kTexCoordLoc) + ") " + pIO + " vec4 hpl_TexCoord[8];\n";
				ReplaceWord(body, "gl_TexCoord", "hpl_TexCoord");
			}
			const char *pColor = bVtx ? "gl_FrontColor" : "gl_Color";
			const char *pSecondary = bVtx ? "gl_FrontSecondaryColor" : "gl_SecondaryColor";
			if(Uses(body, pColor))
			{
				sHead += "layout(location=" + std::to_string(kFrontColorLoc) + ") " + pIO + " vec4 hpl_FrontColor;\n";
				ReplaceWord(body, pColor, "hpl_FrontColor");
			}
			if(Uses(body, pSecondary))
			{
				sHead += "layout(location=" + std::to_string(kFrontSecondaryColorLoc) + ") " + pIO + " vec4 hpl_FrontSecondaryColor;\n";
				ReplaceWord(body, pSecondary, "hpl_FrontSecondaryColor");
			}
			for(const cDecl& v : vVaryings)
				sHead += "layout(location=" + std::to_string(mapVaryingLoc[v.msName]) + ") " + pIO + " " + v.msType + " " + v.msName + v.msArray + ";\n";

			if(!bVtx)
			{
				int lOutputs = 0;
				if(Uses(body, "gl_FragColor")) { lOutputs = 1; ReplaceWord(body, "gl_FragColor", "hpl_FragData[0]"); }
				static const std::regex fragDataRe("\\bgl_FragData\\s*\\[\\s*(\\w+)\\s*\\]");
				for(auto m = std::sregex_iterator(body.begin(), body.end(), fragDataRe); m != std::sregex_iterator(); ++m)
				{
					std::string sIdx = (*m)[1].str();
					int lIdx = std::isdigit((unsigned char)sIdx[0]) ? std::atoi(sIdx.c_str()) : 7;
					if(lIdx+1 > lOutputs) lOutputs = lIdx+1;
				}
				ReplaceWord(body, "gl_FragData", "hpl_FragData");
				if(lOutputs == 0) lOutputs = 1;
				aOut.mlFragOutputs = lOutputs;
				sHead += "layout(location=0) out vec4 hpl_FragData[" + std::to_string(lOutputs) + "];\n";
			}

			for(const cBuiltinUniform& u : gvBuiltinUniforms) ReplaceWord(body, u.mpGl, u.mpHpl);

			for(const std::string& sRect : setRectSamplers)
			{
				body = std::regex_replace(body, std::regex("\\btexelFetch\\s*\\(\\s*" + sRect + "\\s*,"), "hpl_texelFetchRect(" + sRect + ",");
				body = std::regex_replace(body, std::regex("\\btextureSize\\s*\\(\\s*" + sRect + "\\s*\\)"), "textureSize(" + sRect + ", 0)");
			}
			static const char *vRenames[][2] = {
				{"texture1D", "texture"}, {"texture2D", "texture"}, {"texture3D", "texture"}, {"textureCube", "texture"},
				{"texture2DArray", "texture"}, {"texture1DLod", "textureLod"}, {"texture2DLod", "textureLod"},
				{"texture3DLod", "textureLod"}, {"textureCubeLod", "textureLod"}, {"texture2DProj", "textureProj"},
				{"texture2DProjLod", "textureProjLod"}, {"shadow2D", "hpl_shadow2D"}, {"shadow2DProj", "hpl_shadow2DProj"},
				{"texture2DRect", "hpl_textureRect"}, {"texture2DRectProj", "hpl_textureRectProj"},
			};
			for(auto& r : vRenames) ReplaceCall(body, r[0], r[1]);

			sHead += sBlock;
			for(auto& h : gpHelpers) if(body.find(h[0]) != std::string::npos) sHead += h[1];

			if(bVtx)
			{
				// GL clip-space z is [-w, w]; Vulkan's is [0, w]
				body = std::regex_replace(body, std::regex("\\bvoid\\s+main\\s*\\("), "void hpl_main(");
				body += "\nvoid main()\n{\n\thpl_main();\n\tgl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n}\n";
			}
			else
			{
				body = std::regex_replace(body, std::regex("\\bvoid\\s+main\\s*\\("), "void hpl_main(");
				// GL fixed-function alpha test still applies with shaders bound; x=enabled, y=eAlphaTestFunc, z=ref
				body += "\nvoid main()\n{\n\thpl_main();\n\tif(hpl_AlphaTest.x > 0.5)\n\t{\n"
						"\t\tfloat a = hpl_FragData[0].a, r = hpl_AlphaTest.z;\n\t\tint f = int(hpl_AlphaTest.y);\n"
						"\t\tbool p = f==1 ? a<r : f==2 ? a<=r : f==3 ? a>r : f==4 ? a>=r : f==5 ? a==r : f==6 ? a!=r : f==7;\n"
						"\t\tif(!p) discard;\n\t}\n}\n";
			}

			std::smatch m;
			static const std::regex glRe("\\bgl_(\\w+)");
			std::string sCheck = body;
			while(std::regex_search(sCheck, m, glRe))
			{
				static const std::set<std::string> setNative = {"Position", "FragCoord", "FragDepth", "FrontFacing", "PointCoord", "PointSize", "VertexID", "InstanceID", "ClipDistance"};
				if(!setNative.count(m[1].str())) { aOut.msError = "unsupported built-in gl_" + m[1].str(); return false; }
				sCheck = m.suffix();
			}

			(bVtx ? aOut.msVertex : aOut.msFragment) = sHead + body;
		}
		return true;
	}

	bool CompileGlslToSpirv(const std::string& asSource, bool abVertex, std::vector<unsigned int>& avSpirv, std::string& asLog)
	{
		static bool bInit = glslang::InitializeProcess();
		(void)bInit;
		EShLanguage stage = abVertex ? EShLangVertex : EShLangFragment;
		glslang::TShader shader(stage);
		const char *pSrc = asSource.c_str();
		shader.setStrings(&pSrc, 1);
		shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
		shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
		shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
		EShMessages msgs = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
		if(!shader.parse(GetDefaultResources(), 450, false, msgs))
		{
			asLog = shader.getInfoLog();
			return false;
		}
		glslang::TProgram program;
		program.addShader(&shader);
		if(!program.link(msgs))
		{
			asLog = program.getInfoLog();
			return false;
		}
		avSpirv.clear();
		glslang::GlslangToSpv(*program.getIntermediate(stage), avSpirv);
		return true;
	}
}
