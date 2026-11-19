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
#include "scene/RustSP.h"

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
		friend struct OhplSPEHostImpl;
		friend class cParticleSystem;
	public:
		iParticleEmitter(	tString asName,tMaterialVec* avMaterials, cGraphics* apGraphics,
							void *apData, const OhplH12PsDesc *apDesc, float afWarmUpTime, float afWarmUpStepsPerSec);
		virtual ~iParticleEmitter();

		void UpdateLogic(float afTimeStep){ ohpl_spe_update_logic(mpPe, afTimeStep);}

		void Render(){}

		void SetWorld(cWorld *apWorld) { ohpl_spe_set_world(mpPe, apWorld);}

		virtual bool IsDead(){ return ohpl_spe_is_dead(mpPe);}
		virtual bool IsDying(){ return ohpl_spe_is_dying(mpPe);}
		virtual void Kill(){ ohpl_spe_kill(mpPe);}
		void KillInstantly(){ ohpl_spe_kill_instantly(mpPe);}

		int GetParticleNum(){ return (int)ohpl_spe_num_particles(mpPe);}

		//Entity implementation
		tString GetEntityType(){ return "ParticleEmitter"; }
		bool IsVisible(){ return IsActive() && mbIsVisible;}
		
		//Renderable implementation
		bool UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime){ return ohpl_spe_update_gfx(mpPe, apFrustum);}

		cMaterial *GetMaterial(){ return static_cast<cMaterial*>(ohpl_spe_material(mpPe));}
		iVertexBuffer* GetVertexBuffer(){ return static_cast<iVertexBuffer*>(ohpl_spe_vertex_buffer(mpPe));}

		cBoundingVolume* GetBoundingVolume(){ ohpl_spe_update_bv(mpPe); return &mBoundingVolume;}

		cMatrixf* GetModelMatrix(cFrustum *apFrustum){ return const_cast<cMatrixf*>(ohpl_spe_model_matrix(mpPe, apFrustum));}

		int GetMatrixUpdateCount(){return GetTransformUpdateCount();}
		eRenderableType GetRenderType(){ return eRenderableType_ParticleEmitter;}

	private:
		OhplSPE *mpPe;
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

