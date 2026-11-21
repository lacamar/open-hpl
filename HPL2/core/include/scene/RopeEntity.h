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

#ifndef HPL_ROPE_ENTITY_H
#define HPL_ROPE_ENTITY_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"

#include "graphics/Renderable.h"
#include "scene/RustLs.h"
#include "scene/RustSceneH12.h"

namespace hpl {

	class cGraphics;
	class cResources;
	class cMaterial;
	class iVertexBuffer;
	class iPhysicsRope;

	//------------------------------------------

	class cRopeEntity : public iRenderable
	{
	public:
		cRopeEntity(const tString& asName, cResources *apResources,cGraphics *apGraphics,
					iPhysicsRope *apRope, int alMaxSegments) : iRenderable(asName),
			mpL(ohpl_ls_rope_new((uintptr_t)apResources, (uintptr_t)apGraphics, (uintptr_t)apRope, alMaxSegments))
		{
			mpE->applyBV = false;
		}
		~cRopeEntity();

		const tString& GetName(){return msName;}

		iPhysicsRope *GetPhysicsRope(){ return (iPhysicsRope*)mpL->rope;}

		void SetMaterial(cMaterial * apMaterial){ mpL->material = (uintptr_t)apMaterial;}

		void SetColor(const cColor &aColor){ ohpl_ls_rope_set_color(mpL, &aColor);}
		const cColor& GetColor(){ return mpL->color;}

		void SetMultiplyAlphaWithColor(bool abX){ mpL->multiplyAlpha = abX;}
		bool GetMultiplyAlphaWithColor(){ return mpL->multiplyAlpha;}

		void SetRadius(float afX){ mpL->radius = afX;}
		float GetRadius(){ return mpL->radius;}

		int GetMaxSegments(){ return mpL->maxSegments;}

		/**
		 * Only used when segment size > 0
		 */
		void SetLengthTileAmount(float afX){ mpL->tileAmount = afX;}
		float GetLengthTileAmount(){ return mpL->tileAmount;}

		/**
		* Only used when segment size = 0
		*/
		void SetLengthTileSize(float afX){ mpL->tileSize = afX;}
		float GetLengthTileSize(){ return mpL->tileSize;}

		/////////////////////////////////
		//Entity implementation
		tString GetEntityType(){ return "RopeEntity";}

		bool IsVisible(){ return ohpl_h12b_is_visible(&mpL->color, mbIsVisible);}

		/////////////////////////////////
		//Renderable implementations
		cMaterial *GetMaterial(){ return (cMaterial*)mpL->material;}
		iVertexBuffer* GetVertexBuffer(){return (iVertexBuffer*)mpL->vtx;}

		bool UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime){ return ohpl_ls_rope_viewport(mpL, OhplLsFrustum(apFrustum));}

		cBoundingVolume* GetBoundingVolume(){ ohpl_ls_rope_bounds(mpL, mBoundingVolume.Rs()); return &mBoundingVolume;}

		cMatrixf* GetModelMatrix(cFrustum *apFrustum){ return apFrustum==NULL ? &GetWorldMatrix() : NULL;}

		int GetMatrixUpdateCount(){ return GetTransformUpdateCount();}

		eRenderableType GetRenderType(){ return eRenderableType_Rope;}
	private:
		OhplLsRope *mpL;
	};

};
#endif // HPL_ROPE_ENTITY_H
