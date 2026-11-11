#include "impl/GlslToVulkan.h"
#include "impl/RustVk.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

namespace hpl {

	namespace {

		void OnError(void *apCtx, const char *apMsg, size_t alLen){ ((cGlslVkProgram*)apCtx)->msError.assign(apMsg, alLen); }

		void OnProgram(void *apCtx, const char *apVtx, size_t alVtx, const char *apFrag, size_t alFrag, unsigned alMask, int alOutputs)
		{
			cGlslVkProgram *p = (cGlslVkProgram*)apCtx;
			p->msVertex.assign(apVtx, alVtx);
			p->msFragment.assign(apFrag, alFrag);
			p->mlAttribMask |= alMask;
			p->mlFragOutputs = alOutputs;
		}

		void OnSampler(void *apCtx, const char *apName, size_t alName, const char *apType, size_t alType, int alBinding)
		{
			((cGlslVkProgram*)apCtx)->mvSamplers.push_back({std::string(apName, alName), std::string(apType, alType), alBinding});
		}

		void OnInit(void *apCtx, const char *apName, size_t alName, const float *apValues, size_t alCount)
		{
			((cGlslVkProgram*)apCtx)->mvInits.push_back({std::string(apName, alName), std::vector<float>(apValues, apValues + alCount)});
		}
	}

	bool GlslToVulkan(const std::string& asVertex, const std::string& asFragment, cGlslVkProgram& aOut)
	{
		OhplVkGlslSink sink = { &aOut, OnError, OnProgram, OnSampler, OnInit };
		return ohpl_vk_glsl_to_vulkan(asVertex.data(), asVertex.size(), asFragment.data(), asFragment.size(), &sink);
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
