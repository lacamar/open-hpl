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

#include "resources/EngineFileLoading.h"

#include "resources/XmlDocument.h"
#include "resources/Resources.h"
#include "resources/RustResMgr.h"
#include "resources/TextureManager.h"
#include "resources/MaterialManager.h"

#include "math/Math.h"

#include "system/String.h"

#include "scene/World.h"
#include "scene/LightPoint.h"
#include "scene/LightSpot.h"
#include "scene/LightBox.h"
#include "scene/MeshEntity.h"
#include "scene/SoundEntity.h"
#include "scene/ParticleEmitter.h"
#include "scene/ParticleSystem.h"
#include "scene/BillBoard.h"
#include "scene/LensFlare.h"
#include "scene/Beam.h"
#include "scene/GuiSetEntity.h"
#include "scene/RopeEntity.h"
#include "scene/FogArea.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// DEFINES
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	#define kBeginWorldEntityLoad()		\
		tString sName = apElement->GetAttributeString("Name");
	
	#define kEndWorldEntityLoad(pEntity)		\
		SetupWorldEntity(pEntity, apElement);	\
		return pEntity;

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CREATE ENTITIES
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cFogArea* cEngineFileLoading::LoadFogArea(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld, bool abStatic)
	{
		kBeginWorldEntityLoad();

		cFogArea *pFog = apWorld->CreateFogArea(asNamePrefix+sName, abStatic);

		if(pFog)
		{
			pFog->SetColor(apElement->GetAttributeColor("Color",cColor(1,1)));
			pFog->SetCulledByFog(apElement->GetAttributeBool("CulledByFog", true));
			pFog->SetBrightness(apElement->GetAttributeFloat("Brightness", 1));
			pFog->SetStart(apElement->GetAttributeFloat("Start", 0));
			pFog->SetEnd(apElement->GetAttributeFloat("End", 0));
			pFog->SetFalloffExp(apElement->GetAttributeFloat("FalloffExp", 0));
			pFog->SetShowBacksideWhenInside(apElement->GetAttributeBool("ShownBacksideWhenInside", true));
			pFog->SetShowBacksideWhenOutside(apElement->GetAttributeBool("ShownBacksideWhenOutside", true));
			pFog->SetUnderwater(apElement->GetAttributeBool("Underwater", false));
			pFog->SetSkybox(apElement->GetAttributeBool("Skybox", false));
			pFog->SetLighten(apElement->GetAttributeBool("Lighten", false));
			pFog->SetHeightBased(apElement->GetAttributeBool("HeightBased", false));
			pFog->SetExponential(apElement->GetAttributeBool("Exponential", false));
			pFog->SetDensity(apElement->GetAttributeFloat("Density", 0.25f));
			pFog->SetHeightDensity(apElement->GetAttributeFloat("HeightDensity", 0.25f));
			pFog->SetHeightHorizon(apElement->GetAttributeFloat("HeightHorizon", 1));
			pFog->SetNoise(apElement->GetAttributeFloat("NoiseStrength", 0), apElement->GetAttributeFloat("NoiseSize", 1),
							apElement->GetAttributeVector3f("NoiseTurbulence", 0));
		}

		kEndWorldEntityLoad(pFog);
	}

	//-----------------------------------------------------------------------

	cParticleSystem* cEngineFileLoading::LoadParticleSystem(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld)
	{
		kBeginWorldEntityLoad();
		
		tString sFile = apElement->GetAttributeString("File");

		cParticleSystem *pPS = apWorld->CreateParticleSystem(asNamePrefix+sName,sFile,1);

		if(pPS)
		{
			pPS->SetColor(apElement->GetAttributeColor("Color",cColor(1,1)));
			pPS->SetBrightness(apElement->GetAttributeFloat("Brightness",1));
			pPS->SetFadeAtDistance(apElement->GetAttributeBool("FadeAtDistance", false));
			pPS->SetMinFadeDistanceStart(apElement->GetAttributeFloat("MinFadeDistanceStart"));
			pPS->SetMinFadeDistanceEnd(apElement->GetAttributeFloat("MinFadeDistanceEnd"));
			pPS->SetMaxFadeDistanceStart(apElement->GetAttributeFloat("MaxFadeDistanceStart"));
			pPS->SetMaxFadeDistanceEnd(apElement->GetAttributeFloat("MaxFadeDistanceEnd"));
			pPS->SetCulledByDistance(apElement->GetAttributeBool("CulledByDistance", true));
			pPS->SetCulledByFog(apElement->GetAttributeBool("CulledByFog", true));
		}
		
		kEndWorldEntityLoad(pPS);
	}
	
	//-----------------------------------------------------------------------

	cSoundEntity* cEngineFileLoading::LoadSound(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld)
	{
		kBeginWorldEntityLoad();
		
		tString sSoundFile = apElement->GetAttributeString("SoundEntityFile");
		bool bUseDefault = ohpl_rm_world_sound_use_default(apElement->GetAttributeBool("UseDefault"),
				apElement->GetAttribute("UseCustomProperties") ? (int)apElement->GetAttributeBool("UseCustomProperties") : -1);
		if(sSoundFile=="") return NULL;

		cSoundEntity *pSound = apWorld->CreateSoundEntity(asNamePrefix+sName,sSoundFile,false);
		if(pSound==NULL) return NULL;

		if(bUseDefault==false)
		{
			pSound->SetMinDistance(apElement->GetAttributeFloat("MinDistance"));
			pSound->SetMaxDistance(apElement->GetAttributeFloat("MaxDistance"));
			pSound->SetVolume(apElement->GetAttributeFloat("Volume"));
		}


		kEndWorldEntityLoad(pSound);
	}

	
	//-----------------------------------------------------------------------

	static eBillboardType ToBillboardType(const tString& asType)
	{
		return (eBillboardType)ohpl_rm_world_billboard_type(RM_STR(asType));
	}

	cBillboard* cEngineFileLoading::LoadBillboard(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld, cResources *apResources, bool abStatic,
													tEFL_LightBillboardConnectionList *apLightBillboardList)
	{
		kBeginWorldEntityLoad();
		
		cVector2f vSize = apElement->GetAttributeVector2f("BillboardSize");
		tString sMat = apElement->GetAttributeString("MaterialFile");
		eBillboardType bbType = ToBillboardType(apElement->GetAttributeString("BillboardType"));

		cBillboard *pBillboard = apWorld->CreateBillboard(asNamePrefix+sName,vSize,bbType,sMat, abStatic);
		if(pBillboard==NULL) return NULL;

		pBillboard->SetForwardOffset(apElement->GetAttributeFloat("BillboardOffset"));
		pBillboard->SetColor(apElement->GetAttributeColor("BillboardColor",cColor(1,1)));
		pBillboard->SetBrightness(apElement->GetAttributeFloat("Brightness",1));

		pBillboard->SetIsHalo(apElement->GetAttributeBool("IsHalo",false));
		pBillboard->SetHaloSourceSize(apElement->GetAttributeVector3f("HaloSourceSize",1));
		pBillboard->SetCulledByDistance(apElement->GetAttributeBool("CulledByDistance", true));
		pBillboard->SetCulledByFog(apElement->GetAttributeBool("CulledByFog", true));

		tString sConnectLight = apElement->GetAttributeString("ConnectLight");
		if(apLightBillboardList && sConnectLight!="")
		{
			cEFL_LightBillboardConnection lightBBConnection;
			
			lightBBConnection.msBillboardID = apElement->GetAttributeInt("ID");
			lightBBConnection.msLightName = asNamePrefix+apElement->GetAttributeString("ConnectLight");

			apLightBillboardList->push_back(lightBBConnection);
		}
		
		kEndWorldEntityLoad(pBillboard);
	}

	//-----------------------------------------------------------------------
	
	cLensFlare* cEngineFileLoading::LoadLensFlare(cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld, cResources *apResources, bool abStatic)
	{
		kBeginWorldEntityLoad();

		cLensFlare *pFlare = apWorld->CreateLensFlare(asNamePrefix+sName, 1, "", abStatic || apElement->GetAttributeBool("Static", false));
		if(pFlare==NULL) return NULL;
		pFlare->SetCulledByFog(apElement->GetAttributeBool("CulledByFog", true));

		const char *vPrefix[] = {"Anamorphic", "Flare", "MultiIris"};
		for(int i=0; i<eLensFlareType_LastEnum; ++i)
		{
			eLensFlareType type = (eLensFlareType)i;
			tString sPrefix = vPrefix[i];
			pFlare->SetFlareActive(type, apElement->GetAttributeBool("Active"+sPrefix, false));
			pFlare->SetFlareSize(type, apElement->GetAttributeVector2f("Size"+sPrefix, 0.125f));
			pFlare->SetFlareColor(type, apElement->GetAttributeColor("Color"+sPrefix, cColor(1,1)));
			tString sMat = apElement->GetAttributeString("Material"+sPrefix);
			pFlare->SetMaterial(type, sMat=="" ? NULL : apResources->GetMaterialManager()->CreateMaterial(sMat));
		}
		pFlare->SetMultiIrisSeed(apElement->GetAttributeInt("MultiIrisSeed", rand()));
		pFlare->SetMultiIrisCount(apElement->GetAttributeInt("MultiIrisCount", 16));
		cVector2f vGrid = apElement->GetAttributeVector2f("MultiIrisTextureAtlasGrid", 1);
		pFlare->SetMultiIrisTextureAtlasGrid(cVector2l((int)vGrid.x, (int)vGrid.y));
		pFlare->SetSizeChangeBasedOnDistance(cMath::Clamp(apElement->GetAttributeFloat("SizeChangeBasedOnDistance", 0), 0, 1));
		pFlare->SetUseParentMeshForOcclusion(apElement->GetAttributeBool("UseParentMeshForOcclusion", true));
		pFlare->SetShrinkWhenOccluded(apElement->GetAttributeBool("ShrinkWhenOccluded", true));
		pFlare->SetMultiplyGlareWithMultiIris(apElement->GetAttributeBool("MulGlareWithMultiIris", false));
		// HPL3 reads Outer before Inner, so OuterFOV never takes effect
		pFlare->SetOuterFieldOfView(apElement->GetAttributeFloat("OuterFOV", kPif));
		pFlare->SetInnerFieldOfView(apElement->GetAttributeFloat("InnerFOV", kPif));
		pFlare->SetGlareBrightness(apElement->GetAttributeFloat("GlareBrightness", 0.5f));
		pFlare->SetGlareFieldOfView(apElement->GetAttributeFloat("GlareFOV", 0));
		pFlare->SetGlareStareAt(apElement->GetAttributeFloat("GlareStareAt", 0.5f));
		cVector2f vRange = apElement->GetAttributeVector2f("GlareRange", 0);
		pFlare->SetGlareRange(vRange.x, vRange.y);
		vRange = apElement->GetAttributeVector2f("RangeMax", -1);
		pFlare->SetRangeMax(vRange.x, vRange.y);
		vRange = apElement->GetAttributeVector2f("RangeMin", -1);
		pFlare->SetRangeMin(vRange.x, vRange.y);
		pFlare->SetFlareSourceSize(apElement->GetAttributeVector3f("SourceSize", 0.5f));
		pFlare->SetBrightness(apElement->GetAttributeFloat("Brightness", 1));

		kEndWorldEntityLoad(pFlare);
	}

	//-----------------------------------------------------------------------

	static eShadowMapResolution ToShadowMapResolution(const tString& asType)
	{
		return (eShadowMapResolution)ohpl_rm_world_shadow_resolution(RM_STR(asType));
	}

	static eTextureAnimMode ToTextureAnimMode(const tString& asType)
	{
		return (eTextureAnimMode)ohpl_rm_mat_anim_mode(RM_STR(asType));
	}
	
	iLight* cEngineFileLoading::LoadLight(	cXmlElement* apElement, const tString& asNamePrefix, cWorld *apWorld, cResources *apResources, bool abStatic)
	{
		kBeginWorldEntityLoad();
		
		iLight *pLight = NULL;

		bool bStatic = abStatic;

		//////////////////////////
		// Box Light
		int lLightKind = ohpl_rm_world_light_kind(RM_STR(apElement->GetValue()));
		if(lLightKind == 0)
		{
			cLightBox *pLightBox = apWorld->CreateLightBox(asNamePrefix+sName, bStatic);
			pLight = pLightBox;

			pLightBox->SetSize(apElement->GetAttributeVector3f("Size", 1));
			pLightBox->SetBlendFunc((eLightBoxBlendFunc) apElement->GetAttributeInt("BlendFunc", 1));
			pLightBox->SetAmbientColorSky(apElement->GetAttributeColor("SkyColor", cColor(1,1)));
			pLightBox->SetAmbientColorGround(apElement->GetAttributeColor("GroundColor", cColor(1,1)));
			pLightBox->SetWeight(apElement->GetAttributeFloat("Weight", 1));
			pLightBox->SetBevel(apElement->GetAttributeFloat("Bevel", 0));
			pLightBox->SetFalloffPow(apElement->GetAttributeFloat("FalloffPow", 0));
			pLightBox->SetUseSphericalHarmonics(apElement->GetAttributeBool("UseSphericalHarmonics", false));
			pLightBox->SetProbeOffset(apElement->GetAttributeVector3f("ProbeOffset", 0));
			cXmlElement *pIrrElem = apElement->GetFirstElement("Irradiance");
			if(pIrrElem)
			{
				cXmlNodeListIterator setIt = pIrrElem->GetChildIterator();
				while(setIt.HasNext())
				{
					cXmlElement *pSetElem = setIt.Next()->ToElement();
					tFloatVec vValues;
					cString::GetFloatVec(pSetElem->GetAttributeString("_Text"), vValues);
					std::vector<cVector3f> vBands;
					size_t lBandCount = ohpl_rm_world_band_count(vValues.size());
					for(size_t i=0; i<lBandCount; ++i) vBands.push_back(cVector3f(vValues[i*3], vValues[i*3+1], vValues[i*3+2]));
					pLightBox->AddIrradianceSet(pSetElem->GetAttributeString("SetName"), vBands);
				}
			}
		}
		//////////////////////////
		// Spotlightt
		else if(lLightKind == 1)
		{
			cLightSpot *pLightSpot = apWorld->CreateLightSpot(asNamePrefix+sName,"", bStatic); 
			pLight = pLightSpot;

			//Frustum related
			pLightSpot->SetFOV(apElement->GetAttributeFloat("FOV", 1.0f));
			pLightSpot->SetAspect(apElement->GetAttributeFloat("Aspect", 1.0f));
			cVector3f vScale = apElement->GetAttributeVector3f("Scale", 1);
			pLightSpot->SetNearClipPlane(cMath::Max(apElement->GetAttributeFloat("NearClipPlane", 0.1f) * (vScale.x+vScale.y+vScale.z)/3.0f, 0.001f));

			//Spot fall off
			tString sSpotFalloffMap = apElement->GetAttributeString("SpotFalloffMap");
			if(sSpotFalloffMap != "")
			{
				iTexture *pFalloff = apResources->GetTextureManager()->Create1D(sSpotFalloffMap,true);
				if(pFalloff) pLightSpot->SetSpotFalloffMap(pFalloff);
			}
		}
		//////////////////////////
		// Point Light
		else if(lLightKind == 2)
		{
			cLightPoint *pLightPoint  = apWorld->CreateLightPoint(asNamePrefix+sName,"", bStatic); 
			pLight = pLightPoint;
		}
		else
		{
			Error("Unknown light type '%s'\n", apElement->GetValue().c_str());
			return NULL;
		}

		//////////////////////////
		// General properties
		eLightType lightType = pLight->GetLightType();

		//Spot and point
		if(lightType == eLightType_Point || lightType == eLightType_Spot)
		{
			//Falloff
			tString sFalloffMap = apElement->GetAttributeString("FalloffMap");
			if(sFalloffMap != "")
			{
				iTexture *pFalloff = apResources->GetTextureManager()->Create1D(sFalloffMap,true);
				if(pFalloff) pLight->SetFalloffMap(pFalloff);
			}

			//Gobo
			tString sGobo = apElement->GetAttributeString("Gobo","");
			pLight->SetGoboSpecular(apElement->GetAttributeString("GoboType","") == "Specular");
			if(sGobo  != "")
			{
				eTextureAnimMode animMode = ToTextureAnimMode(apElement->GetAttributeString("GoboAnimMode",""));
				float fAnimFrameTime = apElement->GetAttributeFloat("GoboAnimFrameTime", 1);

				iTexture *pGoboTex=NULL;
				apResources->GetTextureManager()->SetSRGB(true);
				switch(ohpl_rm_world_gobo_kind(lightType == eLightType_Spot, (int)animMode))
				{
				case 0: pGoboTex = apResources->GetTextureManager()->Create2D(sGobo,true); break;
				case 1: pGoboTex = apResources->GetTextureManager()->CreateAnim(sGobo, true, eTextureType_2D); break;
				case 2: pGoboTex = apResources->GetTextureManager()->CreateCubeMap(sGobo,true); break;
				case 3: pGoboTex = apResources->GetTextureManager()->CreateAnim(sGobo,true, eTextureType_CubeMap); break;
				}
				apResources->GetTextureManager()->SetSRGB(false);
	
				if(pGoboTex)
				{
					pLight->SetGoboTexture(pGoboTex);
					pGoboTex->SetFrameTime(fAnimFrameTime);
				}
			}
		}

		//All types
		pLight->SetCastShadows(apElement->GetAttributeBool("CastShadows", false));
		pLight->SetCulledByDistance(apElement->GetAttributeBool("CulledByDistance", true));
		pLight->SetCulledByFog(apElement->GetAttributeBool("CulledByFog", true));
		pLight->SetDiffuseColor(apElement->GetAttributeColor("DiffuseColor", cColor(1)));
		pLight->SetDefaultDiffuseColor(pLight->GetDiffuseColor());
		pLight->SetCastDiffuseLight(apElement->GetAttributeBool("CastDiffuseLight", true));
		pLight->SetCastSpecularLight(apElement->GetAttributeBool("CastSpecularLight", true));
		pLight->SetRadius(apElement->GetAttributeFloat("Radius", 1));
		pLight->SetFalloffPow(apElement->GetAttributeFloat("FalloffPow", 1));
		pLight->SetSpotFalloffPow(apElement->GetAttributeFloat("SpotFalloffPow", 1));
		pLight->SetBrightness(apElement->GetAttributeFloat("Brightness", 1));
		pLight->SetDefaultBrightness(pLight->GetBrightness());
		tString sMaskID = apElement->GetAttributeString("ConnectedLightMaskID", "4294967295");
		pLight->SetMaskID(ohpl_rm_world_light_mask_id(RM_STR(sMaskID)));

		pLight->SetShadowMapResolution( ToShadowMapResolution(apElement->GetAttributeString("ShadowResolution", "High")) );
		
		bool bShadowsAffectDynamic = apElement->GetAttributeBool("ShadowsAffectDynamic", true);
		bool bShadowsAffectStatic = apElement->GetAttributeBool("ShadowsAffectStatic", true);
		pLight->SetShadowCastersAffected(ohpl_rm_world_shadow_casters(bShadowsAffectDynamic, bShadowsAffectStatic));

		//////////////////////
		// Backwards compitabilty:
		float fDefaultFadeOn = apElement->GetAttributeFloat("FlickerOnFadeLength",0);
		float fDefaultFadeOff = apElement->GetAttributeFloat("FlickerOffFadeLength",0);

		pLight->SetFlickerActive(apElement->GetAttributeBool("FlickerActive", false));
		pLight->SetFlicker(
			apElement->GetAttributeColor("FlickerOffColor"),
			apElement->GetAttributeFloat("FlickerOffRadius"),

			apElement->GetAttributeFloat("FlickerOnMinLength"),
			apElement->GetAttributeFloat("FlickerOnMaxLength"),
			apElement->GetAttributeString("FlickerOnSound"),
			apElement->GetAttributeString("FlickerOnPS"),

			apElement->GetAttributeFloat("FlickerOffMaxLength"),
			apElement->GetAttributeFloat("FlickerOffMinLength"),
			apElement->GetAttributeString("FlickerOffSound"),
			apElement->GetAttributeString("FlickerOffPS"),

			apElement->GetAttributeBool("FlickerFade"),
			apElement->GetAttributeFloat("FlickerOnFadeMinLength", fDefaultFadeOn),
			apElement->GetAttributeFloat("FlickerOnFadeMaxLength", fDefaultFadeOn),
			
			apElement->GetAttributeFloat("FlickerOffFadeMinLength", fDefaultFadeOff),
			apElement->GetAttributeFloat("FlickerOffFadeMaxLength", fDefaultFadeOff)
			);
 

		kEndWorldEntityLoad(pLight);
	}
	

	//-----------------------------------------------------------------------

	int glDecalNumOfElements[4] = {4,3,3,4};
	eVertexBufferElement glDecalElementType[4] = {	eVertexBufferElement_Position, 
													eVertexBufferElement_Normal,
													eVertexBufferElement_Texture0,
													eVertexBufferElement_Texture1Tangent};
	cMesh* cEngineFileLoading::LoadDecalMeshHelper(cXmlElement* apElement, cGraphics* apGraphics, cResources* apResources, const tString& asName, const tString& asMaterial, const cColor& aColor)
	{
		////////////////////////////////
		//Load Vertex data
		if(apElement==NULL)return NULL;

		int lNumOfVtx = apElement->GetAttributeInt("NumVerts", 0);
		int lNumOfIdx = apElement->GetAttributeInt("NumInds", 0);

		if(lNumOfIdx <=0 || lNumOfVtx<=0)
		{
			Warning("Decal %s is missing geometry, skipping!\n", asName.c_str());
			return NULL;
		}
			
		cXmlElement *pDataArrayElem[4];
		pDataArrayElem[0] = apElement->GetFirstElement("Positions");
		pDataArrayElem[1] = apElement->GetFirstElement("Normals");
		pDataArrayElem[2] = apElement->GetFirstElement("TexCoords");
		pDataArrayElem[3] = apElement->GetFirstElement("Tangents");
		cXmlElement *pIndicesElem = apElement->GetFirstElement("Indices");

		tFloatVec vDataArrays[4];
		tIntVec vIdxArray;
		tString sSepp=" ";
		//A missing array element or short array used to read past the data.
		if(pIndicesElem==NULL)
		{
			Warning("Decal %s is missing its index array, skipping!\n", asName.c_str());
			return NULL;
		}
		for(int i=0; i<4; ++i)
		{
			if(pDataArrayElem[i]==NULL)
			{
				Warning("Decal %s is missing a vertex array, skipping!\n", asName.c_str());
				return NULL;
			}
			cString::GetFloatVec(pDataArrayElem[i]->GetAttributeString("Array"), vDataArrays[i],&sSepp);
		}
		cString::GetIntVec(pIndicesElem->GetAttributeString("Array"), vIdxArray,&sSepp);

		const float *pArrays[4] = { vDataArrays[0].data(), vDataArrays[1].data(), vDataArrays[2].data(), vDataArrays[3].data() };
		size_t lArrayLens[4] = { vDataArrays[0].size(), vDataArrays[1].size(), vDataArrays[2].size(), vDataArrays[3].size() };
		if(ohpl_rm_world_decal_vertices(pArrays, lArrayLens, (size_t)lNumOfVtx, NULL)==false || vIdxArray.size() < (size_t)lNumOfIdx)
		{
			Warning("Decal %s has too little geometry data, skipping!\n", asName.c_str());
			return NULL;
		}
		std::vector<float> vPacked((size_t)lNumOfVtx * 16);
		ohpl_rm_world_decal_vertices(pArrays, lArrayLens, (size_t)lNumOfVtx, vPacked.data());
	
		//////////////////////////////////
		// Create vertex buffer
		iVertexBuffer *pVtxBuffer = apGraphics->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Software, eVertexBufferDrawType_Tri, 
																					eVertexBufferUsageType_Static,lNumOfVtx, lNumOfIdx);

		//Create arrays	
		for(int i=0; i<4; ++i)
			pVtxBuffer->CreateElementArray(glDecalElementType[i],eVertexBufferElementFormat_Float, glDecalNumOfElements[i]);
		pVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
		
		//Copy the data!
		// TODO: This needs to be made faster so that data is loaded directly into mesh!
		for(int vtx=0; vtx<lNumOfVtx; ++vtx)
		{
			for(int i=0; i<4; ++i)
			{
				const float *pData = &vPacked[((size_t)vtx*4 + i)*4];

				if(glDecalNumOfElements[i]==2)
					pVtxBuffer->AddVertexVec3f(glDecalElementType[i], cVector3f(pData[0],pData[1],0) );
				else if(glDecalNumOfElements[i]==3)
					pVtxBuffer->AddVertexVec3f(glDecalElementType[i], cVector3f(pData[0],pData[1],pData[2]) );
				else if(glDecalNumOfElements[i]==4)
					pVtxBuffer->AddVertexVec4f(glDecalElementType[i], cVector3f(pData[0],pData[1],pData[2]),pData[3]);
			}

			pVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, aColor);
		}

		for(int i=0; i<lNumOfIdx; ++i)
			pVtxBuffer->AddIndex(vIdxArray[i]);

		//Compile
		pVtxBuffer->Compile(0);
		
		/////////////////////////
		// Create the mesh
		cMesh *pMesh = hplNew( cMesh, (asName, _W(""), apResources->GetMaterialManager(), apResources->GetAnimationManager()) );

		cSubMesh *pSubMesh = pMesh->CreateSubMesh("Main");
		
		cMaterial *pMat = apResources->GetMaterialManager()->CreateMaterial(asMaterial);
		pSubMesh->SetMaterial(pMat);
		pSubMesh->SetVertexBuffer(pVtxBuffer);
		pSubMesh->SetMaterialName(asMaterial);


		return pMesh;
	}

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cEngineFileLoading::SetupWorldEntity(iEntity3D *apEntity, cXmlElement* apElement)
	{
		if(apEntity==NULL) return;

		int lID = apElement->GetAttributeInt("ID");
		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos",0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale",1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation",0);

		cMatrixf mtxTransform = cMath::MatrixMul(cMath::MatrixRotate(vRotation, eEulerRotationOrder_XYZ),cMath::MatrixScale(vScale));
		mtxTransform.SetTranslation(vPosition);

		apEntity->SetMatrix(mtxTransform);
		apEntity->SetUniqueID(lID);
	}

    //-----------------------------------------------------------------------
}
