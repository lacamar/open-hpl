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

#include "scene/ParticleSystem.h"

#include <memory>
#include <vector>

#include "system/String.h"

#include "scene/ParticleEmitter.h"
#include "scene/ParticleEmitter_UserData.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Renderer.h"
#include "graphics/Material.h"
#include "scene/Camera.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"

#include "resources/Resources.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"
#include "resources/MaterialManager.h"
#include "resources/ParticleManager.h"

#include "engine/Engine.h"

#include "scene/Scene.h"
#include "scene/World.h"
#include "scene/Light.h"
#include "math/Math.h"

namespace hpl {

	// Objects behind an emitter, see rust/src/scene/sp_emit.rs
	struct OhplSPEHostImpl
	{
		static iParticleEmitter* E(void *c){ return static_cast<iParticleEmitter*>(c); }
		static iVertexBuffer* V(void *p){ return static_cast<iVertexBuffer*>(p); }
		static eVertexBufferElement Elem(int32_t k)
		{
			static const eVertexBufferElement vElems[] = { eVertexBufferElement_Texture0, eVertexBufferElement_Position,
				eVertexBufferElement_Color0, eVertexBufferElement_Normal, eVertexBufferElement_Texture1Tangent };
			return vElems[k];
		}

		static void* VbCreate(void *apGfx, uint32_t alMax)
		{
			iVertexBuffer *pVb = static_cast<cGraphics*>(apGfx)->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware,
										eVertexBufferDrawType_Tri, eVertexBufferUsageType_Stream, alMax*4, alMax*6);
			pVb->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
			pVb->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
			pVb->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);
			pVb->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
			pVb->CreateElementArray(eVertexBufferElement_Texture1Tangent,eVertexBufferElementFormat_Float,4);
			return pVb;
		}
		static void VbIndex(void *p, int32_t i){ V(p)->AddIndex(i);}
		static void VbAdd(void *p, int32_t k, float x, float y, float z, float w)
		{
			if(k==2) V(p)->AddVertexColor(Elem(k), cColor(x,y,z,w));
			else if(k==4) V(p)->AddVertexVec4f(Elem(k), cVector3f(x,y,z), w);
			else V(p)->AddVertexVec3f(Elem(k), cVector3f(x,y,z));
		}
		static void VbCompile(void *p){ V(p)->Compile(0);}
		static void VbFree(void *p){ hplDelete(V(p));}
		static float* VbArray(void *p, int32_t k){ return V(p)->GetFloatArray(Elem(k));}
		static int32_t VbStride(void *p){ return V(p)->GetElementNum(eVertexBufferElement_Position);}
		static void VbSetCount(void *p, int32_t n){ V(p)->SetElementNum(n);}
		static void VbUpdate(void *p, bool abTex)
		{
			V(p)->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Color0 | (abTex ? eVertexElementFlag_Texture0 : 0), false);
		}

		static bool Active(void *c){ return E(c)->IsActive();}
		static bool SameFrame(void *c){ return iRenderer::GetRenderFrameCount() == E(c)->mlRenderFrameCount;}
		static int32_t MatrixCount(void *c){ return E(c)->GetMatrixUpdateCount();}
		static const cMatrixf* WorldMatrix(void *c){ return &E(c)->GetWorldMatrix();}
		static const cMatrixf* LocalMatrix(void *c){ return &E(c)->GetLocalMatrix();}
		static void WorldPos(void *c, cVector3f *apOut){ *apOut = E(c)->GetWorldPosition();}
		static void TransformUpdated(void *c){ E(c)->SetTransformUpdated();}
		static void BvSet(void *c, const cVector3f *apMin, const cVector3f *apMax){ E(c)->mBoundingVolume.SetLocalMinMax(*apMin, *apMax);}
		static void BvTransform(void *c, const cMatrixf *a_mtx){ E(c)->mBoundingVolume.SetTransform(*a_mtx);}
		static void Apply(void *c, int32_t alOp, bool abArg)
		{
			switch(alOp)
			{
			case kOhplH12OpVisible: E(c)->SetVisible(abArg); break;
			case kOhplH12OpCullDistance: E(c)->SetCulledByDistance(abArg); break;
			default: E(c)->SetCulledByFog(abArg); break;
			}
		}
		static const cMatrixf* PsWorldMatrix(void *p){ return &static_cast<cParticleSystem*>(p)->GetWorldMatrix();}
		static void PsCenter(void *p, cVector3f *apOut){ *apOut = static_cast<cParticleSystem*>(p)->GetBoundingVolume()->GetWorldCenter();}
		static void Frustum(const void *p, OhplSPFrustum *apOut)
		{
			cFrustum *pF = static_cast<cFrustum*>(const_cast<void*>(p));
			apOut->mbInvertsCull = pF->GetInvertsCullMode();
			apOut->mvOrigin = pF->GetOrigin();
			apOut->m_mtxView = pF->GetViewMatrix();
		}
		static void* MatAt(void *p, int32_t i)
		{
			tMaterialVec *pV = static_cast<tMaterialVec*>(p);
			return (i<0 || i>=(int32_t)pV->size()) ? NULL : (*pV)[i];
		}
		static bool MatAddBlend(void *p){ return static_cast<cMaterial*>(p)->GetBlendMode() == eMaterialBlendMode_Add;}
		static bool Collide(void *apData, void *apWorld, const cVector3f *apStart, const cVector3f *apEnd,
							cVector3f *apNormal, cVector3f *apPoint)
		{
			return static_cast<cParticleEmitterData_UserData*>(apData)->CheckCollision(*apStart, *apEnd,
						static_cast<cWorld*>(apWorld)->GetPhysicsWorld(), apNormal, apPoint);
		}

		static const OhplSPEHost* Table()
		{
			static const OhplSPEHost h = { VbCreate, VbIndex, VbAdd, VbCompile, VbFree, VbArray, VbStride, VbSetCount, VbUpdate,
				Active, SameFrame, MatrixCount, WorldMatrix, LocalMatrix, WorldPos, TransformUpdated, BvSet, BvTransform, Apply,
				PsWorldMatrix, PsCenter, Frustum, MatAt, MatAddBlend, Collide };
			return &h;
		}
	};

	iParticleEmitter::iParticleEmitter(tString asName, tMaterialVec *avMaterials, cGraphics *apGraphics,
									   void *apData, const OhplH12PsDesc *apDesc, float afWarmUpTime, float afWarmUpStepsPerSec)
		: iRenderable(asName),
		  mpPe(ohpl_spe_new(OhplSPEHostImpl::Table(), this, apGraphics, avMaterials, apData, apDesc,
							(uint32_t)avMaterials->size(), afWarmUpTime, afWarmUpStepsPerSec))
	{
		mbApplyTransformToBV = false;
		mBoundingVolume.SetSize(0);
		mBoundingVolume.SetPosition(0);
	}

	iParticleEmitter::~iParticleEmitter()
	{
		ohpl_spe_free(mpPe);
	}

	iParticleEmitterData::iParticleEmitterData(const tString &asName,cResources* apResources, cGraphics *apGraphics)
		: mpResources(apResources), mpGraphics(apGraphics), msName(asName), mfWarmUpTime(0), mfWarmUpStepsPerSec(20)
	{
	}

	iParticleEmitterData::~iParticleEmitterData()
	{
		for(int i=0;i<(int)mvMaterials.size();i++)
		{
			if(mvMaterials[i]) mpResources->GetMaterialManager()->Destroy(mvMaterials[i]);
		}
	}

	void iParticleEmitterData::AddMaterial(cMaterial *apMaterial)
	{
		mvMaterials.push_back(apMaterial);
	}

	static const char* H12NoAttr(void*, const char*){ return NULL; }

	cParticleEmitterData_UserData::cParticleEmitterData_UserData(
		const tString &asName,cResources* apResources,cGraphics *apGraphics)
		: iParticleEmitterData(asName,apResources,apGraphics), mpDesc(ohpl_h12_ps_parse(H12NoAttr, NULL))
	{
	}

	iParticleEmitter* cParticleEmitterData_UserData::Create(tString asName, cVector3f avSize)
	{
		const OhplH12PsDesc &d = D();
		iParticleEmitter *pPE = hplNew( iParticleEmitter, (asName,&mvMaterials,mpGraphics,this,&d,mfWarmUpTime,mfWarmUpStepsPerSec) );

		cMatrixf mtxOffset = cMath::MatrixRotate(d.mvAngleOffset,eEulerRotationOrder_XYZ);
		mtxOffset.SetTranslation(d.mvPosOffset);
		pPE->SetMatrix(mtxOffset);

		return pPE;
	}

	static const char* H12Attr(void *apCtx, const char *asName)
	{
		return static_cast<cXmlElement*>(apCtx)->GetAttribute(asName);
	}

	void cParticleEmitterData_UserData::LoadFromElement(cXmlElement *apElement, bool abLoadMaterials)
	{
		ohpl_h12_ps_free(mpDesc);
		mpDesc = ohpl_h12_ps_parse(H12Attr, apElement);
		const OhplH12PsDesc &d = D();

		msName = d.msName;
		mfWarmUpTime = d.mfWarmUpTime;
		mfWarmUpStepsPerSec = d.mfWarmUpStepsPerSec;

		if(abLoadMaterials)
		{
			tString sMaterial = d.msMaterial;
			if(d.mlMaterialNum <= 1)
			{
				sMaterial = cString::SetFileExt(sMaterial,"mat");
				cMaterial *pMaterial = mpResources->GetMaterialManager()->CreateMaterial(sMaterial);
				if(pMaterial) mvMaterials.push_back(pMaterial);
			}
			else
			{
				for(int i=1; i<d.mlMaterialNum+1; ++i)
				{
					tString sFileName;
					if(i>9)		sFileName = sMaterial + cString::ToString(i);
					else		sFileName = sMaterial + "0"+cString::ToString(i);

					sFileName = cString::SetFileExt(sFileName,"mat");

					cMaterial *pMaterial = mpResources->GetMaterialManager()->CreateMaterial(sFileName);
					if(pMaterial) mvMaterials.push_back(pMaterial);
				}
			}
		}

		const cVector2l vFirstSubDiv = mvMaterials.empty() ? cVector2l(0) : mvMaterials[0]->GetUVSubDivisions();
		ohpl_h12_ps_finish(mpDesc, (uint32_t)mvMaterials.size(), mvMaterials.empty() ? NULL : &vFirstSubDiv);
	}

	bool cParticleEmitterData_UserData::OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams)
	{
		if(pBody->IsActive()==false || pBody->GetCollide()==false || pBody->IsCharacter())
		{
			return true;
		}

		if(mfShortestDist > apParams->mfDist)
		{
			mbIntersected = true;
			mfShortestDist = apParams->mfDist;
			*mpIntersectPos = apParams->mvPoint;
			*mpIntersectNormal = apParams->mvNormal;
		}

		return true;
	}

	bool cParticleEmitterData_UserData::CheckCollision(const cVector3f& avStart, const cVector3f &avEnd,
														iPhysicsWorld *apPhysicsWorld, cVector3f *apNormalVec, cVector3f *apPosVec)
	{
		mfShortestDist = 99999.0f;
		mbIntersected = false;

		mpIntersectPos = apPosVec;
		mpIntersectNormal = apNormalVec;

		apPhysicsWorld->CastRay(this,avStart,avEnd,true,true,true);

		return mbIntersected;
	}

	cParticleSystemData::~cParticleSystemData()
	{
		for(unsigned i=0; i<ohpl_spd_count(mpRust); ++i) hplDelete(GetEmitterData((int)i));
		ohpl_spd_free(mpRust);
	}

	cParticleSystem* cParticleSystemData::Create(tString asName, cVector3f avSize)
	{
		const int lNum = (int)ohpl_spd_count(mpRust);
		if(!ohpl_h12b_psdata_can_create((uint32_t)lNum))
		{
			Warning("Particle system '%s' has no emitters.\n",msName.c_str());
			return NULL;
		}

		cParticleSystem *pPS = hplNew( cParticleSystem, (asName,this,mpResources,mpGraphics) );
		for(int i=0; i<lNum; ++i)
		{
			iParticleEmitter *pPE = static_cast<iParticleEmitter*>(GetEmitterData(i)->Create(asName, avSize));
			pPS->AddEmitter(pPE);
		}
		return pPS;
	}

	bool cParticleSystemData::LoadFromFile(const tWString &asFile)
	{
		SetFullPath(asFile);

		iXmlDocument* pXmlDoc = mpResources->GetLowLevel()->CreateXmlDocument();
		if(pXmlDoc->CreateFromFile(asFile)==false)
		{
			hplDelete(pXmlDoc);
			return false;
		}

		bool bRet = LoadFromElement(pXmlDoc);

		hplDelete(pXmlDoc);
		return bRet;
	}

	bool cParticleSystemData::LoadFromElement(cXmlElement* apElement)
	{
		const uint32_t lExisting = ohpl_spd_count(mpRust);
		if(apElement->GetValue()!="ParticleSystem")
			return ohpl_h12b_psdata_plan(false, NULL, 0, lExisting, NULL);

		std::vector<tString> vValues;
		cXmlNodeListIterator itV = apElement->GetChildIterator();
		while(itV.HasNext()) vValues.push_back(itV.Next()->ToElement()->GetValue());

		std::unique_ptr<bool[]> vIsEmitter(new bool[vValues.size() + 1]), vLoad(new bool[vValues.size() + 1]);
		for(size_t i=0; i<vValues.size(); ++i) vIsEmitter[i] = vValues[i]=="ParticleEmitter";

		const bool bRet = ohpl_h12b_psdata_plan(apElement->GetValue()=="ParticleSystem", vIsEmitter.get(), (uint32_t)vValues.size(),
												lExisting, vLoad.get());

		cXmlNodeListIterator itL = apElement->GetChildIterator();
		for(size_t i=0; itL.HasNext(); ++i)
		{
			cXmlElement* pElem = itL.Next()->ToElement();
			if(vLoad[i]==false) continue;

			cParticleEmitterData_UserData *pPE = hplNew( cParticleEmitterData_UserData,("",	mpResources,mpGraphics) );
			pPE->LoadFromElement(pElem);
			AddEmitterData(pPE);
		}
		return bRet;
	}

	cParticleSystem::~cParticleSystem()
	{
		if(iLight *pLight = GetLight()) pLight->RemoveParticleSystem(this);
		for(int i=0; i<GetEmitterNum(); ++i) hplDelete(GetEmitter(i));
		if(mpS->mpManager) static_cast<cParticleManager*>(mpS->mpManager)->Destroy(GetData());
		ohpl_sp_free(mpRust);
	}

	void cParticleSystem::AddEmitter(iParticleEmitter* apEmitter)
	{
		ohpl_sp_add_emitter(mpRust, apEmitter->mpPe);
		AddChild(apEmitter);
	}
}
