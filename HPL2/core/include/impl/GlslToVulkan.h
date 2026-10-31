#ifndef HPL_GLSL_TO_VULKAN_H
#define HPL_GLSL_TO_VULKAN_H

#include <string>
#include <vector>

namespace hpl {

	// Vertex attribute locations used by the rewritten shaders; the Vulkan vertex buffers bind to these.
	enum eGlslVkAttrib
	{
		eGlslVkAttrib_Vertex,
		eGlslVkAttrib_Normal,
		eGlslVkAttrib_Color,
		eGlslVkAttrib_SecondaryColor,
		eGlslVkAttrib_MultiTexCoord0,
		eGlslVkAttrib_LastEnum = eGlslVkAttrib_MultiTexCoord0 + 8,
	};

	struct cGlslVkSampler
	{
		std::string msName;
		std::string msType; // sampler2D, sampler2DShadow, samplerCube, sampler3D, sampler1D, sampler2DArray
		int mlBinding;
	};

	struct cGlslVkUniformInit
	{
		std::string msName;
		std::vector<float> mvValues;
	};

	struct cGlslVkProgram
	{
		std::string msVertex;
		std::string msFragment;
		std::vector<cGlslVkSampler> mvSamplers;
		std::vector<cGlslVkUniformInit> mvInits;
		unsigned int mlAttribMask = 0;
		int mlFragOutputs = 0;
		std::string msError;
	};

	// GLSL 1.20/1.30 compatibility-profile pair -> GLSL 4.50 for Vulkan.
	// set 0: binding 0 = std140 block "hpl_Uniforms" shared by both stages, samplers from binding 1.
	// Built-ins become hpl_* (matrices and hpl_AlphaTest live in the block).
	bool GlslToVulkan(const std::string& asVertex, const std::string& asFragment, cGlslVkProgram& aOut);

	bool CompileGlslToSpirv(const std::string& asSource, bool abVertex, std::vector<unsigned int>& avSpirv, std::string& asLog);

	const int kGlslVkFirstSamplerBinding = 1;
}

#endif
