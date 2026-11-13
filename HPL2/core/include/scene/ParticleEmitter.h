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

#ifndef HPL_PARTICLE_EMITTER_H
#define HPL_PARTICLE_EMITTER_H

#include "graphics/Renderable.h"
#include "scene/Entity3D.h"
#include "math/MathTypes.h"
#include "system/SystemTypes.h"
#include "graphics/GraphicsTypes.h"
#include "scene/RustParticles.h"
#include "scene/RustSceneH12.h"

namespace hpl {

	class cFrameSubImage;
	class cGraphics;
	class cResources;
	class cWorld;
	class cParticleSystem;

	//-------------------------------------------------------------------

	enum ePEType
	{
		ePEType_Normal,
		ePEType_Beam,
		ePEType_LastEnum,
	};


	//-------------------------------------------------------------------

	enum eParticleEmitterType
	{
		eParticleEmitterType_FixedPoint,
		eParticleEmitterType_DynamicPoint,
		eParticleEmitterType_Line,
		eParticleEmitterType_Axis,
		eParticleEmitterType_LastEnum,
	};

	enum eParticleEmitterCoordSystem
	{
		eParticleEmitterCoordSystem_World,
		eParticleEmitterCoordSystem_Local,
		eParticleEmitterCoordSystem_LastEnum,
	};

	//------------------------------------------

	class cPESubDivision
	{
	public:
		cVector3f mvUV[4];
	};
	
	//-------------------------------------------------------------------

	enum ePENoiseType
	{
		ePENoiseType_LowFreq,
		ePENoiseType_HighFreq,
		ePENoiseType_Both,
		ePENoiseType_None,
		ePENoiseType_LastEnum,
	};

	
	typedef struct 
	{
		float				fRelToBeamPos;
		float				fRelToBendPos;
		int					lLowFreqNoiseIdx;
		int					lHighFreqNoiseIdx;
		ePENoiseType	noiseType;
	} tBeamNoisePoint;


	//-------------------------------------------------------------------


	//////////////////////////////////////////////////////
	/////////////// PARTICLE ///////////////////////////// 
	//////////////////////////////////////////////////////

	//-------------------------------------------------------------------

	//-------------------------------------------------------------------

	//////////////////////////////////////////////////////
	/////////////// PARTICLE SYSTEM ////////////////////// 
	//////////////////////////////////////////////////////

	class iParticleEmitter :public iRenderable
	{
	public:
		iParticleEmitter(	tString asName,tMaterialVec* avMaterials,unsigned int alMaxParticles, 
							cVector3f avSize, cGraphics* apGraphics,cResources *apResources);
		virtual ~iParticleEmitter();

		void UpdateLogic(float afTimeStep);

		void Render(){}

		void SetWorld(cWorld *apWorld) { mpWorld = apWorld;}

		void SetSystem(cParticleSystem *apSystem){ mpParentSystem = apSystem;}

		virtual bool IsDead(){ return ohpl_pe_is_dead(mpRust);}
		virtual bool IsDying(){ return ohpl_pe_is_dying(mpRust);}
		virtual void Kill(){ ohpl_pe_kill(mpRust);}
		void KillInstantly(){ ohpl_pe_kill_instantly(mpRust);}

		void SetDataName(const tString &asName){msDataName = asName;}
		void SetDataSize(const cVector3f &avSize){mvDataSize = avSize;}

		int GetParticleNum(){ return (int)ohpl_pe_num_particles(mpRust);}

		//Entity implementation
		tString GetEntityType(){ return "ParticleEmitter"; }
		bool IsVisible();
		
		//Renderable implementation
		bool UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime);

		cMaterial *GetMaterial();
		iVertexBuffer* GetVertexBuffer();

		cBoundingVolume* GetBoundingVolume();

		cMatrixf* GetModelMatrix(cFrustum *apFrustum);

		int GetMatrixUpdateCount(){return GetTransformUpdateCount();}
		eRenderableType GetRenderType(){ return eRenderableType_ParticleEmitter;}

	protected:
		virtual void UpdateMotion(float afTimeStep)=0;
		
		cGraphics *mpGraphics;
		cResources *mpResources;

		tString msDataName;
		cVector3f mvDataSize;

		OhplEmitter *mpRust;
		unsigned int mlPoolSize;

		cMatrixf m_mtxTemp;

		tMaterialVec* mvMaterials;

		//Sleep count, update flags and counters, see h12b_psys.rs.
		OhplH12PeCtl mCtl;

		iVertexBuffer *mpVtxBuffer;

		cVector3f mvDirection;

		cVector3f mvRight;
		cVector3f mvForward;

		cParticleSystem *mpParentSystem;

		//Set by the particle system implementation.
		cVector2f mvDrawSize;
		cVector2f mvMaxDrawSize;

		eParticleEmitterType mDrawType;

		eParticleEmitterCoordSystem mCoordSystem;

		bool mbMultiplyRGBWithAlpha;

		cWorld *mpWorld;

		bool mbUsesDirection;

		bool mbUsePartSpin;
		bool mbUseRevolution;

		ePEType mPEType;
	};

	//-----------------------------------------------------------------

	typedef std::list<iParticleEmitter*> tParticleEmitterList;
	typedef tParticleEmitterList::iterator tParticleEmitterListIt;

	//-----------------------------------------------------------------

	//////////////////////////////////////////////////////
	/////////////// PARTICLE EMITTER DATA //////////////// 
	//////////////////////////////////////////////////////

	//-----------------------------------------------------------------

	class cResources;

	class iParticleEmitterData
	{
		friend class cParticleSystemData;
	public:
		/**
		* This inits the data needed for the particles system type
		* \param &asName name of the type
		* \param apResources 
		* \param apGraphics 
		*/
		iParticleEmitterData(const tString &asName,cResources* apResources,cGraphics *apGraphics);
		virtual ~iParticleEmitterData();

		void AddMaterial(cMaterial *apMaterial);

		const tString& GetName(){ return msName;}

		virtual iParticleEmitter* Create(tString asName, cVector3f avSize)=0;

		float GetWarmUpTime() const { return mfWarmUpTime;}
		float GetWarmUpStepsPerSec() const { return mfWarmUpStepsPerSec;}

	protected:
		cResources *mpResources;
		cGraphics *mpGraphics;

		tString msName;
		tMaterialVec mvMaterials;

		float mfWarmUpTime;
		float mfWarmUpStepsPerSec;
	};

	//-----------------------------------------------------------------

	typedef std::map<tString,iParticleEmitterData*> tParticleEmitterDataMap;
	typedef tParticleEmitterDataMap::iterator tParticleEmitterDataMapIt;

	//-----------------------------------------------------------------
};

#endif // HPL_PARTICLE_EMITTER_H

