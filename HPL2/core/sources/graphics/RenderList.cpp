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

#include "graphics/RenderList.h"

#include "graphics/Renderable.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/Renderer.h"
#include "graphics/SubMesh.h"

#include "scene/SubMeshEntity.h"
#include "scene/Light.h"
#include "scene/FogArea.h"
#include "scene/LensFlare.h"

#include "math/Frustum.h"

#include <cstring>

namespace hpl {

	enum { eOhplRlList_Sorted = 9 };

	static_assert(eLensFlareType_MultiIris == 2 && eRenderableType_SubMesh == 1 && eRenderableType_LensFlare == 8, "rl.rs constants");
	static_assert(sizeof(cVector3f) == 12 && sizeof(cMatrixf) == 64, "rl.rs plane layout");

	// The iRenderable virtuals and fields rust/src/render/rl.rs reaches; removed with the C++ renderable subclasses.
	namespace
	{
		struct OhplRlPlane { cVector3f normal, point; cMatrixf world; };

		iRenderable* R(void *apObj){ return static_cast<iRenderable*>(apObj);}
		cLensFlare* F(void *apObj){ return static_cast<cLensFlare*>(apObj);}

		struct OhplRlHost
		{
			int (*renderer_frame)();
			int (*render_type)(void*);
			OhplH7Mat* (*material)(void*);
			unsigned (*type_flags)(void*);
			OhplRnd* (*rnd)(void*);
			void (*update_frame)(void*, float);
			bool (*update_viewport)(void*, void*, float);
			cMatrixf* (*model_matrix)(void*, void*);
			OhplBv* (*bounding_volume)(void*);
			bool (*uses_occlusion_query)(void*);
			uintptr_t (*vertex_buffer)(void*);
			void (*world_position)(void*, cVector3f*);
			bool (*plane)(void*, OhplRlPlane*);
			bool (*collides_frustum)(void*, void*);
			bool (*flare_active)(void*, int);
			void* (*flare_type)(void*, int);
		};

		const OhplRlHost gRlHost = {
			[]{ return iRenderer::GetRenderFrameCount(); },
			[](void *o){ return (int)R(o)->GetRenderType(); },
			[](void *o){ cMaterial *m = R(o)->GetMaterial(); return m ? m->Rs() : (OhplH7Mat*)NULL; },
			[](void *t){ iMaterialType *m = static_cast<iMaterialType*>(t); return (unsigned)(m->IsTranslucent() | (m->IsDecal() << 1)); },
			[](void *o){ return R(o)->Rs(); },
			[](void *o, float t){ R(o)->UpdateGraphicsForFrame(t); },
			[](void *o, void *f, float t){ return R(o)->UpdateGraphicsForViewport(static_cast<cFrustum*>(f), t); },
			[](void *o, void *f){ return R(o)->GetModelMatrix(static_cast<cFrustum*>(f)); },
			[](void *o){ return R(o)->GetBoundingVolume()->Rs(); },
			[](void *o){ return R(o)->UsesOcclusionQuery(); },
			[](void *o){ return (uintptr_t)R(o)->GetVertexBuffer(); },
			[](void *o, cVector3f *v){ *v = R(o)->GetWorldPosition(); },
			[](void *o, OhplRlPlane *p)
			{
				cSubMeshEntity *pSubEnt = static_cast<cSubMeshEntity*>(R(o));
				cSubMesh *pSubMesh = pSubEnt->GetSubMesh();
				if(pSubMesh->GetIsOneSided()==false) return false;
				p->normal = pSubMesh->GetOneSidedNormal();
				p->point = pSubMesh->GetOneSidedPoint();
				p->world = pSubEnt->GetWorldMatrix();
				return true;
			},
			[](void *o, void *f){ return R(o)->CollidesWithFrustum(static_cast<cFrustum*>(f)); },
			[](void *o, int t){ return F(o)->IsFlareActive((eLensFlareType)t); },
			[](void *o, int t){ return (void*)static_cast<iRenderable*>(F(o)->GetTypeRenderable((eLensFlareType)t)); },
		};
	}

	static_assert(sizeof(OhplRlHost) == 16 * sizeof(void*), "rl.rs Host");

	//-----------------------------------------------------------------------

	cRenderList::cRenderList() : mpRs(ohpl_rl_new())
	{
		tRenderableView *vViews[] = { &mvOcclusion, &mvSolid, &mvTrans, &mvDecal, &mvIllum };
		for(size_t i=0; i<5; ++i) vViews[i]->Rebind(ohpl_rl_list(mpRs, i));
		mvLights.Rebind(ohpl_rl_list(mpRs, 5));
		mvFogAreas.Rebind(ohpl_rl_list(mpRs, 6));
		mvLensFlares.Rebind(ohpl_rl_list(mpRs, 7));
		mvMultiIrisFlares.Rebind(ohpl_rl_list(mpRs, 8));
		for(int i=0; i<eRenderListType_LastEnum; ++i) mvSorted[i].Rebind(ohpl_rl_list(mpRs, eOhplRlList_Sorted + i));
	}

	cRenderList::~cRenderList(){ ohpl_rl_free(mpRs);}

	void cRenderList::Setup(float afFrameTime, cFrustum *apFrustum){ ohpl_rl_setup(mpRs, afFrameTime, apFrustum);}

	void cRenderList::AddObject(iRenderable *apObject){ ohpl_rl_add(mpRs, &gRlHost, apObject);}

	void cRenderList::Compile(tRenderListCompileFlag aFlags){ ohpl_rl_compile(mpRs, &gRlHost, aFlags);}

	//-----------------------------------------------------------------------

	void cRenderList::PrintAllObjects()
	{
		Log("---------------------------------\n");
		Log("------ RENDER LIST CONTENTS -----\n");

		Log("Trans Objects:\n");
		for(size_t i=0; i<mvTrans.size(); ++i)
			Log(" '%s' ViewspaceZ: %f LargeSurfacePlacement: %d Mat: '%s' RenderCount: %d\n", mvTrans[i]->GetName().c_str(),
					mvTrans[i]->GetViewSpaceZ(),
					mvTrans[i]->GetLargePlaneSurfacePlacement(),
					mvTrans[i]->GetMaterial()->GetName().c_str(),
					mvTrans[i]->GetRenderFrameCount());

		Log("Solid Objects:\n");
		for(size_t i=0; i<mvSolid.size(); ++i)
			Log(" '%s' Mat: '%s' RenderCount: %d\n", mvSolid[i]->GetName().c_str(), mvSolid[i]->GetMaterial()->GetName().c_str(), mvSolid[i]->GetRenderFrameCount());

		Log("Decal Objects:\n");
		for(size_t i=0; i<mvDecal.size(); ++i)
			Log(" '%s' Mat: '%s' RenderCount: %d\n", mvDecal[i]->GetName().c_str(), mvDecal[i]->GetMaterial()->GetName().c_str(), mvSolid[i]->GetRenderFrameCount());

		Log("Illum Objects:\n");
		for(size_t i=0; i<mvIllum.size(); ++i)
			Log(" '%s' Mat: '%s' RenderCount: %d\n", mvIllum[i]->GetName().c_str(), mvIllum[i]->GetMaterial()->GetName().c_str(), mvSolid[i]->GetRenderFrameCount());

		Log("---------------------------------\n");
	}

	//-----------------------------------------------------------------------

}
