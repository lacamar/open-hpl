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

#include "LuxHandObject.h"

#include "LuxMap.h"
#include "RustLuxProp.h"

//////////////////////////////////////////////////////////////////////////
// XML ATTRIBUTES AS VARIABLES
//////////////////////////////////////////////////////////////////////////

class cLuxHandXmlVars
{
public:
	cLuxHandXmlVars(cXmlElement *apElem) : mpElem(apElem) {}
	tString GetVarString(const tString &asName, const tString &asDefault=""){ return mpElem->GetAttributeString(asName, asDefault); }
	bool GetVarBool(const tString &asName, bool abDefault=false){ return mpElem->GetAttributeBool(asName, abDefault); }
	float GetVarFloat(const tString &asName, float afDefault=0){ return mpElem->GetAttributeFloat(asName, afDefault); }
	int GetVarInt(const tString &asName, int alDefault=0){ return mpElem->GetAttributeInt(asName, alDefault); }
	cVector2f GetVarVector2f(const tString &asName, const cVector2f &avDefault=0){ return mpElem->GetAttributeVector2f(asName, avDefault); }
	cVector3f GetVarVector3f(const tString &asName, const cVector3f &avDefault=0){ return mpElem->GetAttributeVector3f(asName, avDefault); }
	cColor GetVarColor(const tString &asName, const cColor &aDefault=cColor(0,0)){ return mpElem->GetAttributeColor(asName, aDefault); }

private:
	cXmlElement *mpElem;
};


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxHandObject::iLuxHandObject(const tString& asName, cLuxPlayerHands *apHands)
{
	msName = asName;

	mpHands = apHands;

	m_mtxOffset = cMatrixf::Identity;
	mpMeshEntity = NULL;
	mpMesh = NULL;
}

iLuxHandObject::~iLuxHandObject()
{
	if(mpMesh) 
	{
		gpBase->mpEngine->GetResources()->GetMeshManager()->Destroy(mpMesh);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxHandObjectLoader::cLuxHandObjectLoader(const tString& asName) : cEntityLoader_Object(asName)
{
	mpHandObject = NULL;
}

//-----------------------------------------------------------------------

void cLuxHandObjectLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{

}

void cLuxHandObjectLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	///////////////////////////////
	// Load mesh

	//If hand object has not saved mesh, save it and increase user count!
	if(ohpl_ga4c_hand_adopt_mesh(mpHandObject->mpMesh != NULL))
	{
		mpMesh->IncUserCount();
		mpHandObject->mpMesh = mpMesh;
	}

	mpHandObject->mpMeshEntity = mpEntity;


	///////////////////////////////
	// Load light, billboard and particles
	mpHandObject->mvBillboards = mvBillboards;
	mpHandObject->mvParticleSystems = mvParticleSystems;
	mpHandObject->mvLights = mvLights;

	if(mpEntity)
	{
		cMatrixf mtxInvModel;
		ohpl_ga4c_hand_model_inverse(mpEntity->GetLocalMatrix().v, mtxInvModel.v);

		for(size_t i=0; i<mvBillboards.size(); ++i)
		{
			cBillboard *pBB = mvBillboards[i];
			cMatrixf mtxLocal;
			ohpl_ga4c_hand_child_local(mtxInvModel.v, pBB->GetLocalMatrix().v, mtxLocal.v);
			pBB->SetMatrix(mtxLocal);

			mpEntity->AddChild(pBB);
		}

		for(size_t i=0; i<mvLights.size(); ++i)
		{
			iLight *pLight = mvLights[i];
			cMatrixf mtxLocal;
			ohpl_ga4c_hand_child_local(mtxInvModel.v, pLight->GetLocalMatrix().v, mtxLocal.v);
			pLight->SetMatrix(mtxLocal);

			mpEntity->AddChild(pLight);
		}
	}
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool iLuxHandObject::LoadMainData(cXmlElement *apMainElem)
{
	cLuxHandXmlVars xml(apMainElem);
	OhplGa4bVars<cLuxHandXmlVars> vars(&xml);
	vars.Slot(kOhplGa4cSlot_HandModel, msModelFile);
	ohpl_ga4c_hand_load_main(vars.BaseVars());

	return true;
}

//-----------------------------------------------------------------------

void iLuxHandObject::LoadSettings(cXmlElement *apVarsElem)
{
	/////////////////////////////
	//Load base settings 
	cLuxHandXmlVars xml(apVarsElem);
	OhplGa4bVars<cLuxHandXmlVars> vars(&xml);
	vars.Slot(kOhplGa4cSlot_HandAnimIdle, msHandsAnim_Idle)
		.Slot(kOhplGa4cSlot_HandAnimDraw, msHandsAnim_Draw)
		.Slot(kOhplGa4cSlot_HandAnimHolster, msHandsAnim_Holster)
		.Slot(kOhplGa4cSlot_HandBone, msAttachBoneName);
	ohpl_ga4c_hand_load_settings(vars.BaseVars(), m_mtxOffset.v);

	/////////////////////////////
	//Load implemented vars
	LoadImplementedVars(apVarsElem);
}

//-----------------------------------------------------------------------

void iLuxHandObject::CreateEntity(cLuxMap *apMap)
{
	if(ohpl_ga4c_hand_create_needed(mpMeshEntity != NULL) == false) return;

	///////////////////////
	// Load the entity
	tString sFile = "models/player/"+msName+"/"+ msModelFile;
	apMap->GetWorld()->CreateEntity("PlayerHands", cMatrixf::Identity, sFile);
	
	///////////////////////
	// Set up the mesh
	if(mpMeshEntity)
	{
		mpMeshEntity->SetMatrix(m_mtxOffset);
		mpMeshEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster,false);
		mpMeshEntity->SetRenderFlagBit(eRenderableFlag_VisibleInReflection,false);
	}

	ImplementedCreateEntity(apMap);
}

//-----------------------------------------------------------------------

void iLuxHandObject::DestroyEntity(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();
	if(mpMeshEntity)
	{
		pWorld->DestroyMeshEntity(mpMeshEntity);
		mpMeshEntity = NULL;
	}

	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		pWorld->DestroyBillboard(mvBillboards[i]);
	}
	mvBillboards.clear();

	for(size_t i=0; i<mvParticleSystems.size(); ++i)
	{
		pWorld->DestroyParticleSystem(mvParticleSystems[i]);
	}
	mvParticleSystems.clear();

	for(size_t i=0; i<mvLights.size(); ++i)
	{
		pWorld->DestroyLight(mvLights[i]);
	}
	mvLights.clear();

	ImplementedDestroyEntity(apMap);
}

//-----------------------------------------------------------------------

void iLuxHandObject::Reset()
{
	ResetEntityContainers();

	if(mpMesh)
	{
		gpBase->mpEngine->GetResources()->GetMeshManager()->Destroy(mpMesh);
		mpMesh = NULL;
	}

	ImplementedReset();
}

//-----------------------------------------------------------------------

void iLuxHandObject::ResetEntityContainers()
{
	mpMeshEntity = NULL;
	mvBillboards.clear();
	mvParticleSystems.clear();
	mvLights.clear();
}

void iLuxHandObject::SetSetEntitiesVisible(bool abVisible)
{
	if(mpMeshEntity)
	{
		mpMeshEntity->SetVisible(abVisible);
		mpMeshEntity->SetActive(abVisible);
	}
	
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		mvBillboards[i]->SetVisible(abVisible);
	}
	
	for(size_t i=0; i<mvParticleSystems.size(); ++i)
	{
		mvParticleSystems[i]->SetVisible(abVisible);
		mvParticleSystems[i]->SetActive(abVisible);
	}
	
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		mvLights[i]->SetVisible(abVisible);
		mvLights[i]->SetActive(abVisible);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

