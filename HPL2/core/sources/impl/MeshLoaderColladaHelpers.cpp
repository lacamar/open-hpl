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

#include "impl/MeshLoaderCollada.h"
#include <cstring>

#include "system/LowLevelSystem.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "system/String.h"

#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "resources/MaterialManager.h"
#include "graphics/Material.h"

#include "graphics/Bone.h"
#include "graphics/Skeleton.h"
#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"


#include "math/Math.h"

namespace hpl {

#define GetAdress(sStr) if(sStr.length()>0 && sStr[0]=='#') sStr = cString::Sub(sStr,1);
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	cMatrixf cMeshLoaderCollada::GetMatrix(const cMatrixf &amtxA)
	{
		return amtxA;
	}

	//-----------------------------------------------------------------------

	class cTempAnimData
	{
	public:
		cTempAnimData(): mvTrans(0,0,0), mvRot(0,0,0), mvScale(1,1,1) {}

		cVector3f mvTrans;
		cVector3f mvRot;
		cVector3f mvScale;
		float mfTime;

		int mlIndex;
	};

	typedef std::vector<cTempAnimData> tTempAnimDataVec;

	typedef std::set<float> tTempTimesSet;
	typedef std::set<float>::iterator tTempTimesSetIt;

	static cTempAnimData* GetTempAnimData(float afTime, tTempAnimDataVec &avTempData)
	{
		for(size_t i=0; i<avTempData.size(); ++i)
		{
			if(avTempData[i].mfTime == afTime) return &avTempData[i];
		}

		return NULL;
	}

	/////////////////
	
	//Get times in TimeVec closes to afTime.
	static void GetAnimTimes(float afTime,float *apTimeBefore,float *apTimeAfter,tFloatVec* apTimeVec)
	{
		*apTimeBefore = -1; *apTimeAfter=-1;
		for(size_t i=0; i<apTimeVec->size(); ++i)
		{
			float fTime = (*apTimeVec)[i];
			
			if(fTime <= afTime)
			{
				*apTimeBefore = fTime;
			}
			else if(fTime >= afTime)
			{
				*apTimeAfter = fTime;
			}
		}
	}

	/////////////////

	cAnimationTrack* cMeshLoaderCollada::CreateAnimTrack(cAnimation *apAnimation , cSkeleton *apSkeleton,
														 cColladaAnimation &aAnim, cColladaScene *apScene)	
	{
		tTempAnimDataVec vTempData;
		tTempTimesSet setTempTimes;

		//Log("Animation: %s Target: %s\n",aAnim.msId.c_str(), aAnim.msTargetNode.c_str());

		//Get the node that will be animated by this track
		cColladaNode* pNode = apScene->GetNode(aAnim.msTargetNode);
		if(pNode==NULL)
		{
			Error("Couldn't find node '%s' for animation id '%s'\n",aAnim.msTargetNode.c_str(), aAnim.msId.c_str());
			return NULL;
		}

		cBone* pBone = NULL;
		
		if(apSkeleton)
		{
			pBone = apSkeleton->GetBoneByName(aAnim.msTargetNode);
			if(pBone == NULL)
			{
				Error("Couldn't find bone '%s'\n",aAnim.msTargetNode.c_str());
				return NULL;
			}
		}
		else
		{

		}
		
		/////////////////////////////////////////////////
		//Go through all sample and add the different times.
		for(size_t i=0; i< aAnim.mvSamplers.size(); i++)
		{
			cColladaSampler &Sampler = aAnim.mvSamplers[i];

			tFloatVec *pValueVec = aAnim.GetSourceVec(Sampler.msTimeArray);
			if(pValueVec == NULL){
				Error("Time array not found in sampler '%s'!\n",Sampler.msId.c_str());
				return NULL;
			}
			
			//Go through all and add to set.
			for(tFloatVecIt it = pValueVec->begin(); it != pValueVec->end(); ++it)
			{
				setTempTimes.insert(*it);			
			}
		}

		///////////////////////////////
		//Resize temp data and fill with times used.
		vTempData.resize(setTempTimes.size());
		int lCount=0;
		for(tTempTimesSetIt it= setTempTimes.begin(); it != setTempTimes.end(); ++it,++lCount)
		{
			vTempData[lCount].mfTime = *it;	
			vTempData[lCount].mlIndex = lCount;	
		}

		bool bLoadedTranslation=false;
		bool bLoadedRotation=false;
				
		/////////////////////////////////////////////////
		//Go through all samples and get data
		for(size_t i=0; i< aAnim.mvSamplers.size(); i++)
		{
			cColladaSampler &Sampler = aAnim.mvSamplers[i];

			//Log("Sampler %s\n",Sampler.msId.c_str());
			
			//////////////////////////
			//Get the times
			tFloatVec *pTimeVec = aAnim.GetSourceVec(Sampler.msTimeArray);
			if(pTimeVec == NULL){
				Error("Time array not found!\n");
				return NULL;
			}
			
			//Get the Sid of the transformation that this sampler changes.
			tString sTarget = cString::SetFileExt(cString::GetFileName(Sampler.msTarget),"");
			tString sExt = cString::ToLowerCase(cString::GetFileExt(Sampler.msTarget));

			////////////////////
			//Get the transformation and add the values accordingly
			cColladaTransform *pTrans = pNode->GetTransform(sTarget);
			if(pTrans == NULL){
				//Error("Transform '%s' not found!\n",sTarget.c_str());
				continue;
			}

			///////////////////////////
			//Get the values for the transformation
			tFloatVec *pValueVec = aAnim.GetSourceVec(Sampler.msValueArray);
			if(pValueVec == NULL){
				Error("Value array not found!\n");
				return NULL;
			}
			
			////////////////////////////////
            //Set the translation in this sampler
			if(pTrans->msType == "translate")
			{
				//Log("Loading Translation\n");
				bLoadedTranslation = true;

				for(size_t j=0; j <pTimeVec->size(); j++)
				{	
					//Check the translation type
					//If only a single axis is changes the others are set to
					//the node translation. THIS MIGHT BE WRONG!
					//TODO: Maybe the bind matrix should be here:
					cVector3f vTrans = pNode->m_mtxTransform.GetTranslation();

					//Get the temp data:
					cTempAnimData * pTempData = GetTempAnimData((*pTimeVec)[j], vTempData);
					if(pTempData==NULL){ Error("Code error at %d\n",__LINE__); return NULL;}

					// XYZ
					if(sExt=="")
					{
						pTempData->mvTrans = cVector3f((*pValueVec)[j*3 + 0],
							(*pValueVec)[j*3 +1],
							(*pValueVec)[j*3 +2]);
					}
					// X
					else if(sExt=="x")
					{
						pTempData->mvTrans = cVector3f((*pValueVec)[j],vTrans.y,vTrans.z);
					}
					// Y
					else if(sExt=="y")
					{
						pTempData->mvTrans = cVector3f(vTrans.x,(*pValueVec)[j],vTrans.z);
					}
					// Z
					else if(sExt=="z")
					{
						pTempData->mvTrans = cVector3f(vTrans.x,vTrans.y,(*pValueVec)[j]);
					}

					//Scale and rotate data
					pTempData->mvTrans = (pTempData->mvTrans);

					//What we wanna use is the relative movement of the bone.
					//The exported translation sets new local translation!.
					if(pBone)
						pTempData->mvTrans -= pBone->GetLocalTransform().GetTranslation();
				}
				
				////////////////////////////////
				//Go through all of Temp data and find times not added
				float fTimeBefore=-1, fTimeAfter=-1;
				for(size_t j=0; j<vTempData.size(); ++j)
				{
					GetAnimTimes(vTempData[j].mfTime,&fTimeBefore, &fTimeAfter, pTimeVec);
					
					//Time exists
					if(fTimeBefore == vTempData[j].mfTime){
						//Log("Sample %s time %f is in place!\n",Sampler.msId.c_str(), vTempData[j].mfTime);
						continue;
					}
					//Time between two keys
					else if(fTimeBefore>=0 && fTimeAfter>=0)
					{
						cTempAnimData *pBefore = GetTempAnimData(fTimeBefore,vTempData);
						cTempAnimData *pAfter = GetTempAnimData(fTimeAfter,vTempData);
						
						float fT = (vTempData[j].mfTime - pBefore->mfTime)/(pAfter->mfTime -  pBefore->mfTime);

						vTempData[j].mvTrans = pBefore->mvTrans*(1.0f-fT) + pAfter->mvTrans*fT;

						//Log("Interpolated sample %s time %f between %f and %f. T=%f\n",
						//						Sampler.msId.c_str(), vTempData[j].mfTime,
						//						pBefore->mfTime,pAfter->mfTime,fT);
					}
					//Time before start
					else if(fTimeBefore<0 && fTimeAfter>=0)
					{
						cTempAnimData *pAfter = GetTempAnimData(fTimeAfter,vTempData);

						vTempData[j].mvTrans = pAfter->mvTrans;

						//Log("Added before at sample %s time %f using %f\n",
						//					Sampler.msId.c_str(), vTempData[j].mfTime,
						//					pAfter->mfTime);
					}
					//Time after end
					else if(fTimeBefore>=0 && fTimeAfter<0)
					{
						cTempAnimData *pBefore = GetTempAnimData(fTimeBefore,vTempData);
						
						vTempData[j].mvTrans = pBefore->mvTrans;

						//Log("Added after at sample %s time %f using %f\n",
						//						Sampler.msId.c_str(), vTempData[j].mfTime,
						//						pBefore->mfTime);
					}
				}
			}
			//////////////////////////////////
			//Add the rotation in this sampler
			else if(pTrans->msType == "rotate")
			{
				bLoadedRotation = true;

				for(size_t j=0; j <pTimeVec->size(); j++)
				{	
					//Get the temp data:
					cTempAnimData * pTempData = GetTempAnimData((*pTimeVec)[j], vTempData);
					if(pTempData==NULL){ Error("Code error at %d\n",__LINE__); return NULL;}

					pTempData->mvRot += (cVector3f(pTrans->mvValues[0],
																  pTrans->mvValues[1],
																  pTrans->mvValues[2]) * (*pValueVec)[j]);
				}
				
				size_t lVecNum=0;
				if(pTrans->mvValues[0]>0.001f)  lVecNum =0;
				else if(pTrans->mvValues[1]>0.001f)  lVecNum =1;
				else if(pTrans->mvValues[2]>0.001f)  lVecNum =2;

				////////////////////////////////
				//Go through all of Temp data and find times not added
				float fTimeBefore=-1, fTimeAfter=-1;
				for(size_t j=0; j<vTempData.size(); ++j)
				{
					GetAnimTimes(vTempData[j].mfTime,&fTimeBefore, &fTimeAfter, pTimeVec);

					/////////////////////
					//Time exists
					if(fTimeBefore == vTempData[j].mfTime){
						//Log("Sample %s time %f is in place!\n",Sampler.msId.c_str(), vTempData[j].mfTime);
						continue;
					}
					/////////////////////
					//Time between two keys
					else if(fTimeBefore>=0 && fTimeAfter>=0)
					{
						cTempAnimData *pBefore = GetTempAnimData(fTimeBefore,vTempData);
						cTempAnimData *pAfter = GetTempAnimData(fTimeAfter,vTempData);

						float fT = (vTempData[j].mfTime - pBefore->mfTime)/(pAfter->mfTime -  pBefore->mfTime);

						vTempData[j].mvRot.v[lVecNum] = pBefore->mvRot.v[lVecNum]*(1.0f-fT) + 
														pAfter->mvRot.v[lVecNum]*fT;
						
						//Log("Interpolated sample %s time %f between %f and %f. T=%f\n",
						//	Sampler.msId.c_str(), vTempData[j].mfTime,
						//	pBefore->mfTime,pAfter->mfTime,fT);
					}
					/////////////////////
					//Time before start
					else if(fTimeBefore<0 && fTimeAfter>=0)
					{
						cTempAnimData *pAfter = GetTempAnimData(fTimeAfter,vTempData);

						vTempData[j].mvRot.v[lVecNum] = pAfter->mvRot.v[lVecNum];

						//Log("Added before at sample %s time %f using %f\n",
						//	Sampler.msId.c_str(), vTempData[j].mfTime,
						//	pAfter->mfTime);
					}
					/////////////////////
					//Time after end
					else if(fTimeBefore>=0 && fTimeAfter<0)
					{
						cTempAnimData *pBefore = GetTempAnimData(fTimeBefore,vTempData);

						vTempData[j].mvRot.v[lVecNum] = pBefore->mvRot.v[lVecNum];
					}
				}
			}
			//////////////////////////////////
			//Add the rotation and translation from a matrix in this sampler
			else if(pTrans->msType == "matrix")
			{
				bLoadedRotation = true;
				bLoadedTranslation = true;

				for(size_t j=0; j <pTimeVec->size(); j++)
				{	
					//Get the temp data:
					cTempAnimData * pTempData = GetTempAnimData((*pTimeVec)[j], vTempData);
					if(pTempData==NULL){ Error("Code error at %d\n",__LINE__); return NULL;}

					cMatrixf mtxFrame = GetMatrix(cMatrixf(&(*pValueVec)[j*16]));

					if(pBone)
					{
						cMatrixf mtxBone = pBone->GetLocalTransform();
						cMatrixf mtxInvBone = cMath::MatrixInverse(mtxBone);
						cVector3f vChange = mtxFrame.GetTranslation() - mtxBone.GetTranslation();
						mtxFrame = cMath::MatrixMul(mtxInvBone.GetRotation(), mtxFrame.GetRotation());
						mtxFrame.SetTranslation(vChange);
					}

					pTempData->mvTrans += (mtxFrame.GetTranslation());
					
					pTempData->mvRot += (cMath::Vector3ToDeg(cMath::MatrixToEulerAngles(mtxFrame, eEulerRotationOrder_XYZ)));

				}
				
				

				////////////////////////////////
				//Go through all of Temp data and find times not added
				float fTimeBefore=-1, fTimeAfter=-1;
				for(size_t j=0; j<vTempData.size(); ++j)
				{
					GetAnimTimes(vTempData[j].mfTime,&fTimeBefore, &fTimeAfter, pTimeVec);

					/////////////////////
					//Time exists
					if(fTimeBefore == vTempData[j].mfTime){
						//Log("Sample %s time %f is in place!\n",Sampler.msId.c_str(), vTempData[j].mfTime);
						continue;
					}
					/////////////////////
					//Time between two keys
					else if(fTimeBefore>=0 && fTimeAfter>=0)
					{
						cTempAnimData *pBefore = GetTempAnimData(fTimeBefore,vTempData);
						cTempAnimData *pAfter = GetTempAnimData(fTimeAfter,vTempData);

						float fT = (vTempData[j].mfTime - pBefore->mfTime)/(pAfter->mfTime -  pBefore->mfTime);

						for(size_t lVecNum=0; lVecNum < 3; ++lVecNum)
							vTempData[j].mvRot.v[lVecNum] = pBefore->mvRot.v[lVecNum]*(1.0f-fT) + 
															pAfter->mvRot.v[lVecNum]*fT;

						vTempData[j].mvTrans = pBefore->mvTrans*(1.0f-fT) + pAfter->mvTrans*fT;
					}
					/////////////////////
					//Time before start
					else if(fTimeBefore<0 && fTimeAfter>=0)
					{
						cTempAnimData *pAfter = GetTempAnimData(fTimeAfter,vTempData);

						for(size_t lVecNum=0; lVecNum < 3; ++lVecNum)
							vTempData[j].mvRot.v[lVecNum] = pAfter->mvRot.v[lVecNum];

						vTempData[j].mvTrans = pAfter->mvTrans;
					}
					/////////////////////
					//Time after end
					else if(fTimeBefore>=0 && fTimeAfter<0)
					{
						cTempAnimData *pBefore = GetTempAnimData(fTimeBefore,vTempData);

						for(size_t lVecNum=0; lVecNum < 3; ++lVecNum)
							vTempData[j].mvRot.v[lVecNum] = pBefore->mvRot.v[lVecNum];

						vTempData[j].mvTrans = pBefore->mvTrans;
					}
				}
			}
		}
		
		//Log(" Loaded trans: %d, rot: %d\n", bLoadedTranslation, bLoadedRotation);
		

		/*Log("Animation %s\n",aAnim.msName.c_str());
		for(size_t i=0; i <vTempData.size(); i++)
		{
			Log("Time: %f T: (%s) R: (%s) S: (%s)\n", vTempData[i].mfTime, 
				vTempData[i].mvTrans.ToString().c_str(),
				vTempData[i].mvRot.ToString().c_str(), 
				vTempData[i].mvScale.ToString().c_str());
		}*/

		////////////////////////////////////
		//Create new animation track
		cAnimationTrack * pTrack = apAnimation->CreateTrack(aAnim.msTargetNode,
															eAnimTransformFlag_Rotate | 
															eAnimTransformFlag_Translate);
		////////////////////////////////////
		//Iterate the temporary data and add to the track.
		for(size_t i=0; i < vTempData.size(); i++)
		{
			cKeyFrame *pFrame = pTrack->CreateKeyFrame(vTempData[i].mfTime - apScene->mfStartTime);

			//////////////////
			//Get the the node matrix offset
			cMatrixf mtxLocal = pNode->m_mtxTransform;
			cMatrixf mtxRotChange = mtxLocal.GetRotation();
			cVector3f vTransChange = mtxLocal.GetTranslation();
			if(pBone)
			{
				cMatrixf mtxBone = pBone->GetLocalTransform();
				cMatrixf mtxInvBone = cMath::MatrixInverse(mtxBone);
				
				mtxRotChange = cMath::MatrixMul(mtxLocal.GetRotation(), mtxInvBone.GetRotation());
				vTransChange = mtxLocal.GetTranslation() - mtxBone.GetTranslation();
			}			

			///////////////////////////
            //Set Translation
			if(bLoadedTranslation)
			{
				pFrame->trans = vTempData[i].mvTrans;
			}
			else
			{
				pFrame->trans = vTransChange;
			}
			
			///////////////////////////
			//Set Rotation
			if(bLoadedRotation)
			{
				//Create the quaternion from rotations.
				cVector3f vRadRot = vTempData[i].mvRot;
				vRadRot = cVector3f(cMath::ToRad(vRadRot.x),cMath::ToRad(vRadRot.y),cMath::ToRad(vRadRot.z) );
				cMatrixf mtxRot = cMath::MatrixRotate(vRadRot, eEulerRotationOrder_XYZ);
				cQuaternion qRot;
				qRot.FromRotationMatrix(mtxRot);

				pFrame->rotation = qRot;
				//pFrame->base_angles = vRadRot;
			}
			else
			{
				pFrame->rotation.FromRotationMatrix(mtxRotChange);
				//pFrame->base_angles = cMath::MatrixToEulerAngles(mtxRotChange, eEulerRotationOrder_XYZ);
			}
		}

		return pTrack;
	}

	static tString gsTemp;
	static const char* GetTabs(int alDepth)
	{
		gsTemp = "";

		for(int i=0;i<alDepth;i++)gsTemp+="\t";

		return gsTemp.c_str();
	}

	//-----------------------------------------------------------------------


	void cMeshLoaderCollada::CalcLocalMatrixAfterControllerBindMatrixRec(cBone* apBone, cMatrixf a_mtxParentGlobal, int alDepth)
	{
		//Log("%s %s \n",GetTabs(alDepth),apBone->GetName().c_str());

		cMatrixf mtxGlobal;
		//////////////////////
		//The bone is bound to the skin and has a world matrix
		if(apBone->GetValue()!=0)
		{
			mtxGlobal = apBone->GetLocalTransform();
			cMatrixf mtxInvParent = cMath::MatrixInverse(a_mtxParentGlobal);

			cMatrixf mtxLocal = cMath::MatrixMul(mtxInvParent,mtxGlobal);

			apBone->SetTransform(mtxLocal);
		}
		////////////////////////
		// Bone is not bound and has local matrix.
		else
		{
			mtxGlobal = apBone->GetWorldTransform();
		}


		cBoneIterator it = apBone->GetChildIterator();
		while(it.HasNext())
		{
			CalcLocalMatrixAfterControllerBindMatrixRec(it.Next(),mtxGlobal,alDepth+1);
		}
	}


	//-----------------------------------------------------------------------

	void cMeshLoaderCollada::CreateSkeletonBone(cColladaNode* apColladaNode, cBone* apParentBone)
	{
		cBone *pBone = apParentBone->CreateChildBone(apColladaNode->msId, apColladaNode->msSid);

		pBone->SetTransform(apColladaNode->m_mtxTransform);

		tColladaNodeListIt it = apColladaNode->mlstChildren.begin();
		for(; it != apColladaNode->mlstChildren.end();it++)
		{
			CreateSkeletonBone(*it, pBone);
		}
	}

	//-----------------------------------------------------------------------

	iVertexBuffer * cMeshLoaderCollada::CreateVertexBuffer(cColladaGeometry & aGeometry, 
		eVertexBufferUsageType aUsageType)
		//,tColladaExtraVtxListVec &vExtraVtxVec)
	{
		//tVertexVec vVertexVec;
		//tUIntVec vIndexVec;
		//SplitVertices(aGeometry,vExtraVtxVec,vVertexVec,vIndexVec);

		//Log("Creating vertex buffer for '%s'\n",aGeometry.msName.c_str());

		//TEMP: use the one in geometry directly.
		//vExtraVtxVec = aGeometry.mvExtraVtxVec;

		//Create vertex buffer and fill it
		iVertexBuffer *pVtxBuff = mpLowLevelGraphics->CreateVertexBuffer(
			eVertexBufferType_Hardware,
			eVertexBufferDrawType_Tri, aUsageType,
			(int)aGeometry.mvVertexVec.size(), (int)aGeometry.mvIndexVec.size());
		pVtxBuff->CreateElementArray( eVertexBufferElement_Position,eVertexBufferElementFormat_Float, 4);
		pVtxBuff->CreateElementArray( eVertexBufferElement_Normal,eVertexBufferElementFormat_Float, 3);
		pVtxBuff->CreateElementArray( eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float, 3);
		pVtxBuff->CreateElementArray( eVertexBufferElement_Color0,eVertexBufferElementFormat_Float, 4);
		pVtxBuff->CreateElementArray( eVertexBufferElement_Texture1Tangent,eVertexBufferElementFormat_Float, 4);
		
		pVtxBuff->ResizeArray(eVertexBufferElement_Texture1Tangent,(int)aGeometry.mvTangents.size());

		//Add vertices
		for(size_t j=0; j<aGeometry.mvVertexVec.size();j++)
		{
			pVtxBuff->AddVertexVec3f(eVertexBufferElement_Position,aGeometry.mvVertexVec[j].pos);
			pVtxBuff->AddVertexVec3f(eVertexBufferElement_Normal,aGeometry.mvVertexVec[j].norm);
			pVtxBuff->AddVertexVec3f(eVertexBufferElement_Texture0,aGeometry.mvVertexVec[j].tex);
		
			pVtxBuff->AddVertexColor(eVertexBufferElement_Color0,cColor(1,1));
		}
		
		//Add tangents
		memcpy(pVtxBuff->GetFloatArray(eVertexBufferElement_Texture1Tangent),&aGeometry.mvTangents[0], aGeometry.mvTangents.size()*sizeof(float));
		
		//Add indices
		for(size_t j=0; j<aGeometry.mvIndexVec.size();j++)
		{	
			//Flip order of indices
			size_t idx = (j/3)*3 + (2-(j%3));

			pVtxBuff->AddIndex(aGeometry.mvIndexVec[idx]);	
		}

		//Compile the vertex buffer
		pVtxBuff->Compile(0);//eVertexCompileFlag_CreateTangents);

		return pVtxBuff;
	}


	
	//-----------------------------------------------------------------------

	tString cMeshLoaderCollada::GetMaterialTextureFile(const tString &asMaterial,
		tColladaMaterialVec &avColladaMaterialVec,
		tColladaTextureVec &avColladaTextureVec,
		tColladaImageVec &avColladaImageVec)
	{
		if(asMaterial == "") return "";

		//Get the texture Id
		tString sTexId ="";
		for(size_t mat=0; mat < avColladaMaterialVec.size(); mat++)
		{
			if(avColladaMaterialVec[mat].msId == asMaterial)
			{
				sTexId = avColladaMaterialVec[mat].msTexture;
				GetAdress(sTexId);
				break;
			}
		}

		if(sTexId == "")
		{
			Warning("Material '%s' was not found!\n",asMaterial.c_str());
			return "";
		}

		//Get file Id
		tString sFileId="";
		for(size_t tex=0; tex < avColladaTextureVec.size(); tex++)
		{
			if(avColladaTextureVec[tex].msId == sTexId)
			{
				sFileId = avColladaTextureVec[tex].msImage;
				GetAdress(sFileId);
				break;
			}
		}

		if(sFileId == "")
		{
			Warning("Texture '%s' was not found!\n",sTexId.c_str());
			return "";
		}

		//Get file source
		for(size_t img=0; img < avColladaImageVec.size(); img++)
		{
			if(avColladaImageVec[img].msId == sFileId)
			{
				return cString::GetFileName(avColladaImageVec[img].msSource);
			}
		}

		Warning("Couldn't file image file id '%s'\n",sFileId.c_str());

		return "";
	}

	//-----------------------------------------------------------------------

}
