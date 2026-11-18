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

#ifndef HPL_FOG_AREA_H
#define HPL_FOG_AREA_H

#include "graphics/GraphicsTypes.h"
#include "graphics/Renderable.h"
#include "scene/RustSL.h"

namespace hpl {
	
	//------------------------------------------

	class cCamera;
	class cFrustum;
	class iTexture;
	class cResources;
	
	//------------------------------------------

	class cFogArea : public iRenderable
	{
	public:
		cFogArea(tString asName, cResources *apResources) : iRenderable(asName), mpS(ohpl_sl_fog_new())
		{
			mbApplyTransformToBV = true;
			mBoundingVolume.SetSize(1);
		}
		virtual ~cFogArea(){ ohpl_sl_fog_free(mpS); }

		//////////////////////////////
		//Properties
		const cColor& GetColor(){ return mpS->color; }
		void SetColor(const cColor& aCol){ mpS->color = aCol; }
		float GetBrightness(){ return mpS->brightness; }
		void SetBrightness(float afX){ mpS->brightness = afX; }
		
		const cVector3f& GetSize(){ return mpS->size;}
		void SetSize(const cVector3f& avSize)
		{
			cVector3f vBvSize;
			ohpl_sl_fog_set_size(mpS, &avSize, &vBvSize);
			mBoundingVolume.SetSize(vBvSize);
			SetTransformUpdated();
		}

		void SetStart(float afX){ mpS->start = afX;}
		void SetEnd(float afX){ mpS->end = afX;}

		float GetStart(){ return mpS->start;}
		float GetEnd(){ return mpS->end;}

		void SetFalloffExp(float afX){ mpS->falloffExp = afX;}
		float GetFalloffExp(){ return mpS->falloffExp;}

		void SetShowBacksideWhenOutside(bool abX){ mpS->backsideOutside=abX; }
		void SetShowBacksideWhenInside(bool abX){ mpS->backsideInside=abX; }
		bool GetShowBacksideWhenOutside(){ return mpS->backsideOutside; }
		bool GetShowBacksideWhenInside(){ return mpS->backsideInside; }

		void SetUnderwater(bool abX){ mpS->underwater = abX; }
		bool GetUnderwater(){ return mpS->underwater; }
		void SetSkybox(bool abX){ mpS->skybox = abX; }
		bool GetSkybox(){ return mpS->skybox; }
		void SetNoise(float afStrength, float afSize, const cVector3f& avTurbulence){ mpS->noiseStrength = afStrength; mpS->noiseSize = afSize; mpS->noiseTurbulence = avTurbulence; }
		float GetNoiseStrength(){ return mpS->noiseStrength; }
		float GetNoiseSize(){ return mpS->noiseSize; }
		const cVector3f& GetNoiseTurbulence(){ return mpS->noiseTurbulence; }
		void SetLighten(bool abX){ mpS->lighten = abX; }
		bool GetLighten(){ return mpS->lighten; }
		void SetHeightBased(bool abX){ mpS->heightBased = abX; }
		bool GetHeightBased(){ return mpS->heightBased; }
		void SetExponential(bool abX){ mpS->exponential = abX; }
		bool GetExponential(){ return mpS->exponential; }
		void SetDensity(float afX){ mpS->density = afX; }
		float GetDensity(){ return mpS->density; }
		void SetHeightDensity(float afX){ mpS->heightDensity = afX; }
		float GetHeightDensity(){ return mpS->heightDensity; }
		void SetHeightHorizon(float afX){ mpS->heightHorizon = afX; }
		float GetHeightHorizon(){ return mpS->heightHorizon; }

		//////////////////////////////
		//iEntity implementation
		tString GetEntityType(){ return "cFogArea";}

		///////////////////////////////
		//Renderable implementation:
		cMaterial *GetMaterial(){ return NULL;}
		iVertexBuffer* GetVertexBuffer(){ return NULL;}

		eRenderableType GetRenderType(){ return eRenderableType_FogArea;}

		int GetMatrixUpdateCount(){ return GetTransformUpdateCount();}
		cMatrixf* GetModelMatrix(cFrustum* apFrustum){ return const_cast<cMatrixf*>(ohpl_sl_fog_model(mpS, &GetWorldMatrix())); }
		
	private:
		OhplSLFog *mpS;
	};
};
#endif // HPL_FOG_AREA_H
