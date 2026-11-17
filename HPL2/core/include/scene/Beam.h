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

#ifndef HPL_BEAM_H
#define HPL_BEAM_H

#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "system/SystemTypes.h"
#include "scene/Entity3D.h"
#include "graphics/Renderable.h"
#include "scene/RustSL.h"

namespace hpl {

	class cMaterialManager;
	class cResources;
	class cGraphics;
	class cFileSearcher;
	class iLowLevelGraphics;
	class cMaterial;
	class iVertexBuffer;

	//------------------------------------------

	class cBeam;

	class cBeamEnd : public iEntity3D
	{
	friend class cBeam;
	friend class cBeamEnd_UpdateCallback;
	public:
		cBeamEnd(const tString asName, cBeam *apBeam) : iEntity3D(asName), mpS(ohpl_sl_beam_end_new((uintptr_t)apBeam)) {}
		~cBeamEnd(){ ohpl_sl_beam_end_free(mpS); }

		void SetColor(const cColor &aColor);
		const cColor& GetColor(){ return mpS->color;}

		/////////////////////////////////
		//Entity implementation
		tString GetEntityType(){ return "BeamEnd";}

	private:
		OhplSLBeamEnd *mpS;
	};

	//------------------------------------------

	class cBeamEnd_UpdateCallback : public iEntityCallback
	{
	public:
		void OnTransformUpdate(iEntity3D * apEntity);
	};

	//------------------------------------------

	class cBeam : public iRenderable
	{
	#ifdef __GNUC__
		typedef iRenderable __super;
	#endif
	friend class cBeamEnd;
	public:
		cBeam(const tString asName, cResources *apResources,cGraphics *apGraphics);
		~cBeam();

		void SetMaterial(cMaterial * apMaterial){ mpS->material = (uintptr_t)apMaterial; }

		tString GetFileName();

		void SetSize(const cVector2f& avSize);
		cVector2f GetSize(){ return mpS->size;}

		void SetColor(const cColor &aColor);
		const cColor& GetColor(){ return mpS->color;}

		void SetTileHeight(bool abX);
		bool GetTileHeight(){ return mpS->tileHeight;}

		void SetMultiplyAlphaWithColor(bool abX){ mpS->multiplyAlpha = abX;}
		bool GetMultiplyAlphaWithColor(){ return mpS->multiplyAlpha;}

		cBeamEnd* GetEnd(){ return (cBeamEnd*)mpS->end;}

		bool LoadXMLProperties(const tString asFile);

		cVector3f GetAxis(){ return mpS->axis;}
		cVector3f GetMidPosition(){ return mpS->mid;}

		/////////////////////////////////
		//Entity implementation
		tString GetEntityType(){ return "Beam";}
		bool IsVisible();

		//Renderable implementations
		cMaterial *GetMaterial(){ return (cMaterial*)mpS->material;}
		iVertexBuffer* GetVertexBuffer(){return (iVertexBuffer*)mpS->vtx;}

		void UpdateGraphicsForFrame(float afFrameTime);

		cBoundingVolume* GetBoundingVolume();

		cMatrixf* GetModelMatrix(cFrustum *apFrustum);

		int GetMatrixUpdateCount(){ return GetTransformUpdateCount();}

		eRenderableType GetRenderType(){ return eRenderableType_Beam;}

	private:
		cBeamEnd_UpdateCallback mEndCallback;
		OhplSLBeam *mpS;
	};

};
#endif // HPL_BEAM_H
