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

#include "resources/MaterialManager.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/System.h"
#include "system/Platform.h"

#include "graphics/Graphics.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/LowLevelGraphics.h"

#include "resources/TextureManager.h"
#include "resources/Resources.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"

#include "impl/XmlDocumentTiny.h"
#include "resources/RustResMgr.h"



namespace hpl {

    

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMaterialManager::cMaterialManager(cGraphics* apGraphics,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),apResources->GetLowLevelSystem())
	{
		mpGraphics = apGraphics;
		mpResources = apResources;

		mlTextureSizeDownScaleLevel =0;
		mTextureFilter = eTextureFilter_Bilinear;
		mfTextureAnisotropy = 1.0f;

	}

	cMaterialManager::~cMaterialManager()
	{
		ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_MATERIAL);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMaterial* cMaterialManager::CreateMaterial(const tString& asName)
	{
		return H3Res<cMaterial>(ohpl_h3f_material(mpRs, H3Ops(), RM_STR(asName)));
	}

	//-----------------------------------------------------------------------

	void cMaterialManager::Update(float afTimeStep)
	{
		
	}

	//-----------------------------------------------------------------------

	void cMaterialManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cMaterialManager::Destroy(iResourceBase* apResource)
	{
		ohpl_h3f_destroy_standard(mpRs, H3Ops(), apResource);
	}

	//-----------------------------------------------------------------------

	void cMaterialManager::CompileTextureLookups()
	{
		ohpl_h3f_for_each(mpRs, H3Ops(), OHPL_H3C_MAT_COMPILE_LOOKUP, 0);
	}

	//-----------------------------------------------------------------------

	void cMaterialManager::SetTextureFilter(eTextureFilter aFilter)
	{
		if(aFilter == mTextureFilter) return;
		mTextureFilter = aFilter;
		ohpl_h3f_for_each(mpRs, H3Ops(), OHPL_H3C_MAT_SET_FILTER, (int64_t)aFilter);
	}

	//-----------------------------------------------------------------------
	
	void cMaterialManager::SetTextureAnisotropy(float afX)
	{
		if(!ohpl_rm_mat_anisotropy_accepted(afX, mpGraphics->GetLowLevel()->GetCaps(eGraphicCaps_AnisotropicFiltering),
											mpGraphics->GetLowLevel()->GetCaps(eGraphicCaps_MaxAnisotropicFiltering), mfTextureAnisotropy))
			return;
		mfTextureAnisotropy = afX;
		ohpl_h3f_for_each(mpRs, H3Ops(), OHPL_H3C_MAT_SET_ANISO, H3Bits(afX));
	}

	//-----------------------------------------------------------------------

	tString cMaterialManager::GetPhysicsMaterialName(const tString& asName)
	{
		tString sRsName;
		ohpl_h3f_material_physics_name(mpRs, H3Ops(), RM_STR(asName), [](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &sRsName);
		return sRsName;
	}

	//-----------------------------------------------------------------------

	cMaterial* cMaterialManager::CreateCustomMaterial(const tString& asName, iMaterialType *apMaterialType)
	{
		cMaterial* pCustom = hplNew( cMaterial, (asName, cString::To16Char(asName), mpGraphics, mpResources, apMaterialType) );
		ohpl_h3f_material_custom(mpRs, H3Ops(), static_cast<iResourceBase*>(pCustom));
		return pCustom;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------



	void* cMaterialManager::H3Create(OhplH3Create &aCreate)
	{
		cMaterial *pMat = NULL;
		switch(aCreate.kind)
		{
		case OHPL_H3_MAT_NEW:
			return static_cast<iResourceBase*>(hplNew( cMaterial, (H3Name(aCreate.name), H3Wide(aCreate.path), mpGraphics, mpResources,
																	aCreate.i[0] ? iMaterialType::Blank() : static_cast<iMaterialType*>(aCreate.ptr)) ));
		case OHPL_H3_MAT_TYPE:
			return mpGraphics->GetMaterialType(H3Name(aCreate.name));
		case OHPL_H3_MAT_SET:
			pMat = H3Res<cMaterial>(aCreate.ptr);
			switch(aCreate.i[0])
			{
			case 0: pMat->SetDepthTest(aCreate.i[1] != 0); break;
			case 1: pMat->SetForceFullScaleTextures(aCreate.i[1] != 0); break;
			case 2: pMat->SetPhysicsMaterial(H3Name(aCreate.name)); break;
			case 3: pMat->SetUVSubDivisions(cVector2l(aCreate.i[1], aCreate.i[2])); break;
			case 4: pMat->SetBlendMode((eMaterialBlendMode)aCreate.i[1]); break;
			}
			return NULL;
		case OHPL_H3_MAT_TEXTURE:
		{
			cTextureManager *pTexMgr = mpResources->GetTextureManager();
			tString sFile = H3Name(aCreate.name);
			bool bMipMaps = aCreate.i[0] != 0;
			eTextureType type = (eTextureType)aCreate.i[1];
			iTexture *pTex = NULL;
			pTexMgr->SetSRGB(ohpl_rm_mat_texture_is_srgb(aCreate.i[3]));
			switch(aCreate.i[2])
			{
			case 0: pTex = pTexMgr->CreateAnim(sFile,bMipMaps,type,eTextureUsage_Normal,mlTextureSizeDownScaleLevel); break;
			case 1: pTex = pTexMgr->Create1D(sFile,bMipMaps,eTextureUsage_Normal,mlTextureSizeDownScaleLevel); break;
			case 2: pTex = pTexMgr->Create2D(sFile,bMipMaps,eTextureType_2D,eTextureUsage_Normal,mlTextureSizeDownScaleLevel); break;
			case 3: pTex = pTexMgr->Create3D(sFile,bMipMaps,eTextureUsage_Normal,mlTextureSizeDownScaleLevel); break;
			case 4: pTex = pTexMgr->CreateCubeMap(sFile,bMipMaps,eTextureUsage_Normal,mlTextureSizeDownScaleLevel); break;
			}
			pTexMgr->SetSRGB(false);
			return static_cast<iResourceBase*>(pTex);
		}
		case OHPL_H3_MAT_TEX_SETUP:
		{
			iTexture *pTex = static_cast<iTexture*>(static_cast<iResourceBase*>(aCreate.ptr));
			pTex->SetFrameTime(aCreate.f[0]);
			pTex->SetAnimMode((eTextureAnimMode)aCreate.i[1]);
			pTex->SetWrapSTR((eTextureWrap)aCreate.i[2]);
			pTex->SetFilter(mTextureFilter);
			pTex->SetAnisotropyDegree(mfTextureAnisotropy);
			H3Res<cMaterial>(aCreate.ptr2)->SetTexture((eMaterialTexture)aCreate.i[0], pTex);
			return NULL;
		}
		case OHPL_H3_MAT_VAR:
			if(aCreate.i[0]) mUserVars = cResourceVarsObject();
			else mUserVars.SetUserVariable(H3Name(aCreate.name), H3Name(aCreate.name2));
			return NULL;
		case OHPL_H3_MAT_VARS_APPLY:
			static_cast<iMaterialType*>(aCreate.ptr)->LoadVariables(H3Res<cMaterial>(aCreate.ptr2), &mUserVars);
			return NULL;
		case OHPL_H3_MAT_ADD_UV:
			H3Res<cMaterial>(aCreate.ptr)->AddUvAnimation((eMaterialUvAnimation)aCreate.i[0], aCreate.f[0], aCreate.f[1], (eMaterialAnimationAxis)aCreate.i[1]);
			return NULL;
		}
		return NULL;
	}

	int64_t cMaterialManager::H3Call(int alWhat, void *apObj, int64_t alArg)
	{
		if(alWhat == OHPL_H3C_MAT_TYPE_Q)
		{
			iMaterialType *pType = static_cast<iMaterialType*>(apObj);
			if(alArg == 0) return pType->IsTranslucent();
			if(alArg == 1) return pType->GetUsedTextureNum();
			return pType->GetUsedTexture((int)alArg - 2);
		}
		cMaterial *pMat = H3Res<cMaterial>(apObj);
		if(alWhat == OHPL_H3C_MAT_COMPILE)
		{
			pMat->Compile();
			return 0;
		}
		if(alWhat == OHPL_H3C_MAT_COMPILE_LOOKUP)
		{
			if(pMat->GetType()) pMat->CompileTextureLookup();
			return 0;
		}
		for(int i=0; i<eMaterialTexture_LastEnum; ++i)
		{
			iTexture *pTex = pMat->GetTexture((eMaterialTexture)i);
			if(pTex == NULL) continue;
			if(alWhat == OHPL_H3C_MAT_SET_FILTER) pTex->SetFilter((eTextureFilter)alArg);
			else if(alWhat == OHPL_H3C_MAT_SET_ANISO) pTex->SetAnisotropyDegree(H3Float(alArg));
		}
		return 0;
	}

	void cMaterialManager::H3Text(int alWhat, void *apObj, const OhplH3WStr &aPath, void (*apSink)(void*, const uint8_t*, size_t), void *apCtx)
	{
		if(alWhat == OHPL_H3C_FILE_BYTES)
		{
			// first byte: 0 not opened, 1 short read, 2 ok (as OhplXmlLoadFile reads it)
			std::string sBytes(1, '\0');
			FILE *pXmlFile = cPlatform::OpenFile(H3Wide(aPath), _W("rb"));
			if(pXmlFile != NULL)
			{
				fseek(pXmlFile, 0, SEEK_END);
				long lLength = ftell(pXmlFile);
				fseek(pXmlFile, 0, SEEK_SET);
				std::vector<char> vData(lLength > 0 ? lLength : 0);
				bool bRead = lLength > 0 && fread(vData.data(), lLength, 1, pXmlFile) == 1;
				fclose(pXmlFile);
				sBytes[0] = (lLength <= 0 || bRead) ? 2 : 1;
				sBytes.append(vData.begin(), vData.end());
			}
			apSink(apCtx, reinterpret_cast<const uint8_t*>(sBytes.data()), sBytes.size());
			return;
		}
		tString sName;
		if(apObj) sName = H3Res<cMaterial>(apObj)->GetPhysicsMaterial();
		else
		{
			tWString sPath = H3Wide(aPath);
			FILE *pFile = cPlatform::OpenFile(sPath, _W("rb"));
			if(pFile!=NULL)
			{
				fclose(pFile);
				cXmlDocumentTiny doc("");
				if(doc.CreateFromFile(sPath))
				{
					cXmlElement *pMain = doc.GetFirstElement("Main");
					if(pMain==NULL)
						Error("Main child not found in '%s'\n",sPath.c_str());
					else
						sName = cString::ToString(pMain->GetAttribute("PhysicsMaterial"),"Default");
				}
			}
		}
		apSink(apCtx, reinterpret_cast<const uint8_t*>(sName.data()), sName.size());
	}
}
