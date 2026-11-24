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

#ifndef HPL_RENDER_LIST_H
#define HPL_RENDER_LIST_H

#include "graphics/GraphicsTypes.h"
#include "graphics/RustRender.h"
#include "graphics/RustRenderH8.h"
#include "math/MathTypes.h"
#include "system/RustPtrList.h"
#include "system/SystemTypes.h"

namespace hpl {

	//---------------------------------------------

	class iRenderable;
	class iLight;
	class cFrustum;
	class cFogArea;
	class cLensFlare;
	struct OhplRl;

	extern "C" {
	OhplRl* ohpl_rl_new();
	void ohpl_rl_free(OhplRl*);
	void ohpl_rl_setup(OhplRl*, float, cFrustum*);
	void ohpl_rl_clear(OhplRl*);
	void ohpl_rl_add(OhplRl*, const void*, iRenderable*);
	void ohpl_rl_compile(OhplRl*, const void*, unsigned);
	OhplPtrVec* ohpl_rl_list(OhplRl*, size_t);
	}

	typedef cOhplPtrList<iRenderable> tRenderableView;
	typedef cSTLIterator<iRenderable*, tRenderableView, tRenderableView::iterator> cRenderableVecIterator;

	//---------------------------------------------

	// Lists, sorting and the update gates live in rust/src/render/rl.rs; the lists hold iRenderable
	// addresses (iLight, cFogArea and cLensFlare have iRenderable as their first base).
	class cRenderList
	{
	public:
		cRenderList();
		~cRenderList();

		void Setup(float afFrameTime, cFrustum *apFrustum);

		void AddObject(iRenderable *apObject);

		void Compile(tRenderListCompileFlag aFlags);

		cRenderableVecIterator GetArrayIterator(eRenderListType aType){ return cRenderableVecIterator(&mvSorted[aType]);}

		cRenderableVecIterator GetOcclusionQueryObjectIterator(){ return cRenderableVecIterator(&mvOcclusion);}

		void Clear(){ ohpl_rl_clear(mpRs);}

		iLight* GetLight(int alIdx){ return mvLights[alIdx];}
		int GetLightNum(){ return(int)mvLights.size();}

		cFogArea* GetFogArea(int alIdx){ return mvFogAreas[alIdx];}
		int GetFogAreaNum(){ return(int)mvFogAreas.size();}

		const cOhplPtrList<cLensFlare>& GetLensFlares(){ return mvLensFlares;}
		const cOhplPtrList<cLensFlare>& GetMultiIrisFlares(){ return mvMultiIrisFlares;}

		void PrintAllObjects();

		//Temp:
		int GetSolidObjectNum(){ return (int)mvSolid.size();}

		int GetTransObjectNum(){ return (int)mvTrans.size();}

	private:
		OhplRl *mpRs;

		tRenderableView mvOcclusion, mvSolid, mvTrans, mvDecal, mvIllum, mvSorted[eRenderListType_LastEnum];
		cOhplPtrList<iLight> mvLights;
		cOhplPtrList<cFogArea> mvFogAreas;
		cOhplPtrList<cLensFlare> mvLensFlares, mvMultiIrisFlares;
	};

	//---------------------------------------------

};
#endif // HPL_RENDER_LIST_H
