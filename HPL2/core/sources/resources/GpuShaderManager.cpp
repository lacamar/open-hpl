/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include "resources/GpuShaderManager.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "system/Platform.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/GPUShader.h"

#include "resources/FileSearcher.h"

#include <regex>
#include <cstdlib>

#ifdef WIN32
#include <io.h>
#endif

namespace hpl {

	static void ApplyHpslTextureBindings(iGpuShader* apShader, const tString& asHpslText)
	{
		static const std::regex bindRe("uniform\\s+cTexture\\w*\\s+(\\w+)\\s*:\\s*(\\d+)\\s*;");
		auto begin = std::sregex_iterator(asHpslText.begin(), asHpslText.end(), bindRe);
		for(auto it = begin; it != std::sregex_iterator(); ++it)
		{
			apShader->AddSamplerUnit((*it)[1].str(), cString::ToInt((*it)[2].str().c_str(), 0));
		}
	}

	static int gnHpslDumpCounter = 0;

	static void DumpTranspiledShaderIfRequested(const tString &asName, const tString &asGlsl)
	{
		const char *pDumpDir = getenv("OPENHPL_DUMP_HPSL_SHADERS_DIR");
		if(pDumpDir == NULL) return;

		tString sSanitizedName = asName;
		for(size_t i=0; i<sSanitizedName.size(); ++i)
		{
			if(sSanitizedName[i] == '/' || sSanitizedName[i] == '\\') sSanitizedName[i] = '_';
		}

		tString sPath = tString(pDumpDir) + "/" + cString::ToString(gnHpslDumpCounter++) + "_" + sSanitizedName + ".glsl";
		FILE *pFile = cPlatform::OpenFile(cString::To16Char(sPath), _W("wb"));
		if(pFile == NULL) return;

		fwrite(asGlsl.data(), 1, asGlsl.size(), pFile);
		fclose(pFile);
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	tHpslTranspileCallback cGpuShaderManager::mpHpslTranspileCallback = NULL;
	tStringVec cGpuShaderManager::mvGlobalDefines;

	cGpuShaderManager::cGpuShaderManager(cFileSearcher *apFileSearcher,iLowLevelGraphics *apLowLevelGraphics, 
		iLowLevelResources *apLowLevelResources,iLowLevelSystem *apLowLevelSystem)
		: iResourceManager(apFileSearcher, apLowLevelResources,apLowLevelSystem)
	{
		mpLowLevelGraphics = apLowLevelGraphics;

		mpPreprocessParser = hplNew(cPreprocessParser, () );

		mpPreprocessParser->GetEnvVarContainer()->Add("ScreenWidth",mpLowLevelGraphics->GetScreenSizeInt().x);
		mpPreprocessParser->GetEnvVarContainer()->Add("ScreenHeigth",mpLowLevelGraphics->GetScreenSizeInt().y);

		#ifdef WIN32
			mpPreprocessParser->GetEnvVarContainer()->Add("OS_Windows");
		#elif defined(__APPLE__)
			mpPreprocessParser->GetEnvVarContainer()->Add("OS_OSX");
		#elif defined(__linux__)
			mpPreprocessParser->GetEnvVarContainer()->Add("OS_Linux");
		#endif

		for(size_t i=0; i<mvGlobalDefines.size(); ++i)
			mpPreprocessParser->GetEnvVarContainer()->Add(mvGlobalDefines[i]);
	}

	cGpuShaderManager::~cGpuShaderManager()
	{
		hplDelete(mpPreprocessParser);

		DestroyAll();

		Log(" Done with Gpu programs\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cGpuShaderManager::CheckFeatureSupport()
	{
		////////////////////////////
		//Shader model variables
		if(mpLowLevelGraphics->GetCaps(eGraphicCaps_ShaderModel_2))		mpPreprocessParser->GetEnvVarContainer()->Add("ShaderModel_2");
		if(mpLowLevelGraphics->GetCaps(eGraphicCaps_ShaderModel_3))		mpPreprocessParser->GetEnvVarContainer()->Add("ShaderModel_3");
		if(mpLowLevelGraphics->GetCaps(eGraphicCaps_ShaderModel_4))		mpPreprocessParser->GetEnvVarContainer()->Add("ShaderModel_4");

		/////////////////////////
		// Test Feature support
		if(mpFileSearcher->GetFilePath("_test_array_support_frag.glsl") != _W("") &&
		   IsShaderSupported("_test_array_support_frag.glsl", eGpuShaderType_Fragment)==false)
		{
			Log("ATTENTION: System does not support const arrays in glsl!\n");
			mpPreprocessParser->GetEnvVarContainer()->Add("FeatureNotSupported_ConstArray");
		}
	}

	//-----------------------------------------------------------------------

	static const char* const gvHpslFilenameAliases[][2] = {
		{ "deferred_illumination_frag.glsl",	"deferred_illumination_solid_frag.hpsl" },
		{ "deferred_gbuffer_skybox_frag.glsl",	"deferred_skybox_frag.hpsl" },
		{ "deferred_decal_frag.glsl",			"deferred_gbuffer_decal_frag.hpsl" },
		{ "gui_vtx.glsl",						"base_vtx.hpsl" },
		{ "gui_frag.glsl",						"base_frag.hpsl" },
	};

	static const char* const gvHpslSourcePatches[][3] = {
		{ "deferred_light_frag.hpsl", "//Translucency\nuniform float afTranslucencyScale;",
		  "@ifdef BoxMask\n\tuniform cMatrixf a_mtxInvView;\n\tuniform cVector3f avMaskCenter;\n\tuniform cVector3f avMaskExtent;\n@endif\nuniform float afSpotNearClip;\n" },
		{ "deferred_light_frag.hpsl", "\tvDiffuse *= fAttenuatuion;",
		  "\t@ifdef BoxMask\n\t\tcVector3f vMaskDelta = abs((mul(a_mtxInvView, cVector4f(vPos, 1)).xyz - avMaskCenter) * 2.0 / avMaskExtent);\n"
		  "\t\tvDiffuse *= step(max(max(vMaskDelta.x, vMaskDelta.y), vMaskDelta.z), 1.0);\n\t@endif\n" },
		{ "deferred_transparent_frag.hpsl", "\t\tcVector2f avInvScreenSize;\n\t}", "\t\tcMatrixf px_mtxLightProbe;\n\t\tfloat afInvFarPlane;\n" },
	};

	static const char* const gvHpslSourceReplacements[][3] = {
		{ "deferred_illumination_solid_frag.hpsl",
		  "\t\tvIllumination.rgb *= afIlluminationMul;\n\t@endif",
		  "\t\tvIllumination.rgb *= afIlluminationMul;\n\t@else\n\t\tvIllumination.rgb *= avIlluminationMul.rgb;\n\t@endif" },
		{ "deferred_light_frag.hpsl",
		  "\t\t\tfloat fOneMinusCos = max(0.0, 1.0 - dot( vLightDir,  avLightForward));\n"
		  "\t\t\tfAttenuatuion *= pow(1.0 - sqrt(min(fOneMinusCos / afOneMinusCosHalfSpotFOV,1)), afSpotFalloffPow);",
		  "\t\t\tfAttenuatuion *= pow(max(0.0, 1.0 - distance(vProjectedUv.xy, cVector2f(0.5)) * 2.0), afSpotFalloffPow);" },
		{ "deferred_light_frag.hpsl",
		  "\t\tfAttenuatuion *= max(0, vProjectedUv.z);\n",
		  "\t\tfloat fSpotZ = (1.0 - afSpotNearClip / max(fDistance * dot(vLightDir, avLightForward), 1e-4)) / (1.0 - afSpotNearClip * afInvLightRadius);\n"
		  "\t\tfAttenuatuion *= max(0, fSpotZ) * clamp((1.0 - fSpotZ) * 128.0, 0.0, 1.0);\n" },
		{ "deferred_light_frag.hpsl",
		  "\t@ifdef GoboType_Specular\n\t\tvDiffuse = cVector3f(fGradLen / 8.0f);\n\t@endif\n",
		  "" },
		// official path: pow(neg, 16.0) is NaN and min(2.0, NaN) is 2
		{ "deferred_light_frag.hpsl",
		  "\t\tfLightTransport = fLightTransport * fLightTransport; // pow(fLightTransport, 8.0)\n"
		  "\t\tfLightTransport = fLightTransport * fLightTransport;\n"
		  "\t\tfLightTransport = fLightTransport * fLightTransport;\n"
		  "\t\tfLightTransport = fLightTransport * fLightTransport;\n",
		  "\t\tfLightTransport = fLightTransport < 0.0 ? 0.125 : 0.0;\n" },
		// official uses hardware sRGB, which leaves translucency alpha linear
		{ "deferred_light_frag.hpsl",
		  "vColorVal.rgba = GammaToLinearCorrection(vColorVal.rgba);",
		  "vColorVal.rgb = GammaToLinearCorrection(vColorVal.rgb);" },
		// and multiplies linear albedo by the vertex/ColorMul colour
		{ "deferred_gbuffer_solid_frag.hpsl", "vDiffuseColor *= px_vColor.xyz;", "vDiffuseColor *= pow(px_vColor.xyz, cVector3f(1.0 / 2.2));" },
		{ "deferred_gbuffer_solid_frag.hpsl", "vDiffuseColor *= px_vColor.xyz;", "vDiffuseColor *= pow(px_vColor.xyz, cVector3f(1.0 / 2.2));" },
		{ "deferred_gbuffer_decal_frag.hpsl", "vDiffuse * px_vColor;", "vDiffuse * cVector4f(pow(px_vColor.xyz, cVector3f(1.0 / 2.2)), px_vColor.w);" },
		{ "deferred_gbuffer_decal_frag.hpsl", "vDiffuse * px_vColor;", "vDiffuse * cVector4f(pow(px_vColor.xyz, cVector3f(1.0 / 2.2)), px_vColor.w);" },
		{ "deferred_projected_uv_frag.hpsl", "vDiffuseColor.xyz * px_vColor.xyz;", "vDiffuseColor.xyz * pow(px_vColor.xyz, cVector3f(1.0 / 2.2));" },
		{ "deferred_undergrowth_gbuffer_frag.hpsl", "vDiffuseColor *= px_vColor;", "vDiffuseColor *= cVector4f(pow(px_vColor.xyz, cVector3f(1.0 / 2.2)), px_vColor.w);" },
		// detail maps applied per blend pass, not to the composited cache
		{ "cache_terrain_diffuse_frag.hpsl", "uniform cTexture2D aAlphaMap3 : 9;",
		  "uniform cTexture2D aAlphaMap3 : 9;\n"
		  "@ifdef UseVertexPosition\n"
		  "uniform cVector3f avDetailMapScale;\nuniform cVector3f avDetailFade;\nuniform cVector3f avBaseDetailAmount;\n"
		  "uniform cVector3f avDetailAmount0;\nuniform cVector3f avDetailAmount1;\nuniform cVector3f avDetailAmount2;\nuniform cVector3f avDetailAmount3;\n"
		  "uniform cTexture2D aDetailMap0 : 10;\nuniform cTexture2D aDetailMap1 : 11;\nuniform cTexture2D aDetailMap2 : 12;\n"
		  "@endif" },
		{ "cache_terrain_diffuse_frag.hpsl", "in cVector4f px_vTexCoord0,",
		  "in cVector4f px_vTexCoord0,\n@ifdef UseVertexPosition\n\t\t  in cVector3f px_vVertexPos,\n@endif" },
		{ "cache_terrain_diffuse_frag.hpsl", "vDiffuseColor = mix(vDiffuseColor, vTexColor3.xyz, vDissolveBlend.w);",
		  "vDiffuseColor = mix(vDiffuseColor, vTexColor3.xyz, vDissolveBlend.w);\n"
		  "\t@ifdef UseVertexPosition\n"
		  "\t\t@ifdef UseBaseTexture\n\t\t\tcVector3f vDetailAmount = avBaseDetailAmount;\n\t\t@else\n\t\t\tcVector3f vDetailAmount = cVector3f(0);\n\t\t@endif\n"
		  "\t\tvDetailAmount = mix(vDetailAmount, avDetailAmount0, vDissolveBlend.x);\n"
		  "\t\tvDetailAmount = mix(vDetailAmount, avDetailAmount1, vDissolveBlend.y);\n"
		  "\t\tvDetailAmount = mix(vDetailAmount, avDetailAmount2, vDissolveBlend.z);\n"
		  "\t\tvDetailAmount = mix(vDetailAmount, avDetailAmount3, vDissolveBlend.w);\n"
		  "\t\t@ifdef UseBaseTexture\n\t\t@else\n\t\t\tvDetailAmount /= max(min(dot(vDissolveBlend, cVector4f(1.0)), 1.0), 0.0001);\n\t\t@endif\n"
		  "\t\tfloat fDetailMul = 1.0 - clamp((-px_vVertexPos.z - avDetailFade.x) / avDetailFade.y, 0.0, 1.0);\n"
		  "\t\tfloat fAmountLength = length(vDetailAmount);\n"
		  "\t\tcVector3f vNormalizedAmount = vDetailAmount / (fAmountLength + 0.0001);\n"
		  "\t\tcVector2f vWorldXZ = (vTexCoord - cVector2f(0.5)) * avDetailFade.z;\n"
		  "\t\tcVector3f vDetailColor = sqrt(pow(sample(aDetailMap0, vWorldXZ * avDetailMapScale.x).xyz, cVector3f(2.2)) * vNormalizedAmount.x +\n"
		  "\t\t\t\t\t\t\t\t\t pow(sample(aDetailMap1, vWorldXZ * avDetailMapScale.y).xyz, cVector3f(2.2)) * vNormalizedAmount.y +\n"
		  "\t\t\t\t\t\t\t\t\t pow(sample(aDetailMap2, vWorldXZ * avDetailMapScale.z).xyz, cVector3f(2.2)) * vNormalizedAmount.z);\n"
		  "\t\tvDiffuseColor *= mix(cVector3f(1.0), vDetailColor * 2.0, fDetailMul * fAmountLength);\n"
		  "\t@endif" },
		// official squares the sRGB-decoded terrain cache
		{ "cache_terrain_diffuse_frag.hpsl", "out_vColor.xyz =  vDiffuseColor.xyz;", "out_vColor.xyz = vDiffuseColor.xyz * vDiffuseColor.xyz;" },
		{ "deferred_terrain_gbuffer_frag.hpsl", "vDiffuseColor *= px_vColor;", "vDiffuseColor *= cVector4f(pow(px_vColor.xyz, cVector3f(1.0 / 2.2)), px_vColor.w);" },
		{ "deferred_light_frag.hpsl",
		  "(vSourceUV[2] - vSourceUV[0]) * fGradient).xyz;",
		  "(vSourceUV[2] - vSourceUV[0]) * fGradient).xyz;\n"
		  "\t\t\t\tvSpecular *= clamp((0.6 - max(abs(vSourceUV[0].x - 0.5), abs(vSourceUV[0].y - 0.5))) * 10.0, 0.0, 1.0);" },
		{ "deferred_transparent_frag.hpsl",
		  "\t\t@ifdef UseLinearColorSpaceCorrection\n\t\t\tvFinalColor.rgb = GammaToLinearCorrection(vFinalColor.rgb);",
		  "\t\t@ifdef UseLinearColorSpaceCorrection\n\t\t@ifdef UseSRGBDiffuseMap\n\t\t@else\n\t\t\tvFinalColor.rgb = GammaToLinearCorrection(vFinalColor.rgb);\n\t\t@endif" },
		{ "deferred_transparent_frag.hpsl",
		  "\t@endif\n\n\tcVector4f vFinalColor;",
		  "\t@else\n\t\tfloat afLightLevel = afLightLevel;\n\t@endif\n\n\tcVector4f vFinalColor;" },
	};

	static void PatchHpslSource(const tString& asFile, tString& asData)
	{
		asData.erase(std::remove(asData.begin(), asData.end(), '\r'), asData.end());
		for(size_t i=0; i<sizeof(gvHpslSourceReplacements)/sizeof(gvHpslSourceReplacements[0]); ++i)
		{
			if(asFile != gvHpslSourceReplacements[i][0]) continue;
			tString sOld = gvHpslSourceReplacements[i][1];
			size_t lPos = asData.rfind(sOld);
			if(lPos != tString::npos) asData.replace(lPos, sOld.size(), gvHpslSourceReplacements[i][2]);
			else Warning("HPSL replacement anchor not found in %s\n", asFile.c_str());
		}
		// official hardware sRGB mode decodes cube maps with GammaCorrectCubeMaps (pow 2.2)
		static const std::regex rxCubeGamma("@ifdef UseLinearColorSpaceCorrection\\s*\\n[^\\n]*GammaToLinearCorrection[^\\n]*\\n\\s*@elseif GammaCorrectCubeMaps");
		asData = std::regex_replace(asData, rxCubeGamma, "@ifdef UseLinearColorSpaceCorrection");
		for(size_t i=0; i<sizeof(gvHpslSourcePatches)/sizeof(gvHpslSourcePatches[0]); ++i)
		{
			if(asFile != gvHpslSourcePatches[i][0]) continue;
			size_t lPos = asData.rfind(gvHpslSourcePatches[i][1]);
			if(lPos != tString::npos) asData.insert(lPos, gvHpslSourcePatches[i][2]);
			else Warning("HPSL patch anchor not found in %s\n", asFile.c_str());
		}
		// debug: OPENHPL_HPSL_PATCH="file|old|new"
		const char *pPatch = getenv("OPENHPL_HPSL_PATCH");
		tString sPatch = pPatch ? pPatch : "";
		size_t lA = sPatch.find('|'), lB = sPatch.find('|', lA + 1);
		if(lB != tString::npos && asFile == sPatch.substr(0, lA))
		{
			tString sOld = sPatch.substr(lA + 1, lB - lA - 1);
			size_t lPos = asData.rfind(sOld);
			if(lPos != tString::npos) asData.replace(lPos, sOld.size(), sPatch.substr(lB + 1));
		}
	}

	static tString GetHpslFallbackName(const tString& asGlslName)
	{
		for(size_t i=0; i<sizeof(gvHpslFilenameAliases)/sizeof(gvHpslFilenameAliases[0]); ++i)
		{
			if(asGlslName == gvHpslFilenameAliases[i][0]) return gvHpslFilenameAliases[i][1];
		}
		return cString::SetFileExt(asGlslName, "hpsl");
	}

	iGpuShader* cGpuShaderManager::CreateShader(const tString& asName, eGpuShaderType aType,
												cParserVarContainer *apVarContainer)
	{
		iGpuShader* pShader;

		BeginLoad(asName);

		/////////////////////////////////////////
        // If we have a variable container do NOT add the shader as a resource!
		if(apVarContainer)
		{
			tString sFileData;
			tString sParsedOutput;

			/////////////////////////////////
			//Get file from file searcher
			bool bIsHpslFallback = false;
			tString sHpslName;
			tWString sPath = mpFileSearcher->GetFilePath(asName);
			if(sPath==_W("") && mpHpslTranspileCallback)
			{
				sHpslName = GetHpslFallbackName(asName);
				tWString sHpslPath = mpFileSearcher->GetFilePath(sHpslName);
				if(sHpslPath != _W(""))
				{
					sPath = sHpslPath;
					bIsHpslFallback = true;
				}
			}
			if(sPath==_W("")){
				Error("Couldn't find file '%s' in resources\n",asName.c_str());
				EndLoad();
				return NULL;
			}

			/////////////////////////////////
			//Load data
			unsigned int lFileSize = cPlatform::GetFileSize(sPath);

			sFileData.resize(lFileSize);
			cPlatform::CopyFileToBuffer(sPath,&sFileData[0],lFileSize);

			/////////////////////////////////
			//Parse file
			if(bIsHpslFallback)
			{
				if(apVarContainer->Get("UseDepth") != NULL)
					apVarContainer->Add("UseLinearDepth");

				apVarContainer->Add("UseExtendedArgs");
				PatchHpslSource(sHpslName, sFileData);
			}
			mpPreprocessParser->Parse(&sFileData, &sParsedOutput,apVarContainer,cString::GetFilePathW(sPath));

			tString sHpslPreTranspile;
			if(bIsHpslFallback)
			{
				tString sGlsl, sTranspileError;
				sHpslPreTranspile = sParsedOutput;
				if(mpHpslTranspileCallback(sParsedOutput, aType, sGlsl, sTranspileError)==false)
				{
					Error("Couldn't transpile HPSL shader '%s' (from '%s'): %s\n",
						  asName.c_str(), sHpslName.c_str(), sTranspileError.c_str());
					EndLoad();
					return NULL;
				}
				sParsedOutput = sGlsl;
				DumpTranspiledShaderIfRequested(asName, sGlsl);
			}

			/////////////////////////////////
			//Compile
			pShader = mpLowLevelGraphics->CreateGpuShader(asName, aType);
			pShader->SetFullPath(sPath);

			if(pShader->CreateFromString(sParsedOutput.c_str())==false)
			{
				Error("Couldn't create program '%s'\n",asName.c_str());
				hplDelete(pShader);
				EndLoad();
				return NULL;
			}

			/////////////////////////////////
			//Sampler to texture units setup, if needed
			if(bIsHpslFallback && pShader->SamplerNeedsTextureUnitSetup()) ApplyHpslTextureBindings(pShader, sHpslPreTranspile);
			if(aType == eGpuShaderType_Fragment && pShader->SamplerNeedsTextureUnitSetup())
			{
				tParseVarMap *pVarMap = mpPreprocessParser->GetParsingVarContainer()->GetMapPtr();
				tParseVarMapIt varIt = pVarMap->begin();
				for(; varIt != pVarMap->end(); ++varIt)
				{
					const tString& sVarName = varIt->first;
					const tString& sVarVal = varIt->second;
					if(sVarName == "") continue;

					tStringVec vStrings;
					tString sSepp = "_";
					cString::GetStringVec(sVarName,vStrings,&sSepp);
					if(vStrings.size()>=2 && vStrings[0]=="sampler")
					{
						int lUnit = cString::ToInt(sVarVal.c_str(), 0);

						pShader->AddSamplerUnit(vStrings[1], lUnit);
					}

				}
			}
		}
		/////////////////////////////////////////
		// Normal resource load
		else
		{
			tWString sPath;
			pShader = static_cast<iGpuShader*>(FindLoadedResource(asName,sPath));

			if(pShader==NULL && sPath!=_W(""))
			{
				pShader = mpLowLevelGraphics->CreateGpuShader(asName, aType);

				if(pShader->CreateFromFile(sPath)==false)
				{
					Error("Couldn't create program '%s'\n",asName.c_str());
					hplDelete(pShader);
					EndLoad();
					return NULL;
				}

				AddResource(pShader);
			}
			else if(pShader==NULL && sPath==_W("") && mpHpslTranspileCallback)
			{
				tString sHpslName = GetHpslFallbackName(asName);
				tWString sHpslPath = mpFileSearcher->GetFilePath(sHpslName);
				if(sHpslPath != _W(""))
				{
					tString sFileData;
					unsigned int lFileSize = cPlatform::GetFileSize(sHpslPath);
					sFileData.resize(lFileSize);
					cPlatform::CopyFileToBuffer(sHpslPath,&sFileData[0],lFileSize);

					cParserVarContainer emptyVars;
					tString sParsedOutput;
					mpPreprocessParser->Parse(&sFileData, &sParsedOutput, &emptyVars, cString::GetFilePathW(sHpslPath));

					tString sGlsl, sTranspileError;
					if(mpHpslTranspileCallback(sParsedOutput, aType, sGlsl, sTranspileError))
					{
						DumpTranspiledShaderIfRequested(asName, sGlsl);

						pShader = mpLowLevelGraphics->CreateGpuShader(asName, aType);
						pShader->SetFullPath(sHpslPath);

						if(pShader->CreateFromString(sGlsl.c_str())==false)
						{
							Error("Couldn't create program '%s' (from transpiled HPSL '%s')\n",
								  asName.c_str(), sHpslName.c_str());
							hplDelete(pShader);
							pShader = NULL;
						}
						else
						{
							if(pShader->SamplerNeedsTextureUnitSetup())
								ApplyHpslTextureBindings(pShader, sParsedOutput);

							AddResource(pShader);
						}
					}
					else
					{
						Error("Couldn't transpile HPSL shader '%s' (from '%s'): %s\n",
							  asName.c_str(), sHpslName.c_str(), sTranspileError.c_str());
					}
				}
			}

			if(pShader)pShader->IncUserCount();
			else Error("Couldn't load program '%s'\n",asName.c_str());
		}
		
		
		EndLoad();
		return pShader;
     }

	//-----------------------------------------------------------------------

	void cGpuShaderManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cGpuShaderManager::Destroy(iResourceBase* apResource)
	{
		apResource->DecUserCount();

		if(apResource->HasUsers()==false){
			RemoveResource(apResource);
			hplDelete(apResource);
		}
	}

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cGpuShaderManager::IsShaderSupported(const tString& asName, eGpuShaderType aType)
	{
		/////////////////////////////////
		//Get file from file searcher
		tWString sPath = mpFileSearcher->GetFilePath(asName);
		if(sPath==_W("")){
			Error("Couldn't find test file '%s' in resources\n",asName.c_str());
			return false;
		}

		/////////////////////////////////
		//Compile
		iGpuShader* pShader = mpLowLevelGraphics->CreateGpuShader(asName, aType);
		
		bool bRet = pShader->CreateFromFile(sPath, "main", false);
		hplDelete(pShader);
		
		return bRet;
	}
	
	//-----------------------------------------------------------------------
}
