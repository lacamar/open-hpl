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

#ifndef HPL_DECALCREATOR_H
#define HPL_DECALCREATOR_H

#include "math/MathTypes.h"
#include "system/SystemTypes.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/RustGx.h"

namespace hpl {

	class iLowLevelGraphics;
	class cResources;
	class cMesh;
	class cMaterial;
	class cSubMeshEntity;

	// State and logic live in rust/src/graphics/gx_decal.rs.
	class cDecalCreator
	{
	public:
		cDecalCreator(iLowLevelGraphics *apLowLevelGraphics, cResources *apResources) : mpRs(ohpl_gx_decal_new(apLowLevelGraphics, apResources)) {}
		~cDecalCreator() { ohpl_gx_decal_free(mpRs); }
		cDecalCreator(const cDecalCreator&) = delete;
		cDecalCreator& operator=(const cDecalCreator&) = delete;

		bool AddSubMesh(cSubMeshEntity* apSubMesh) { return ohpl_gx_decal_add_submesh(mpRs, apSubMesh); }
		void AddAffectedSubMesh(cSubMeshEntity* apSubMesh) { ohpl_gx_decal_add_affected(mpRs, apSubMesh); }
		void ClearMeshes() { ohpl_gx_decal_clear_meshes(mpRs); }

		void SetMaxTrianglesPerDecal(int alMaxTris) { ohpl_gx_decal_set_max_tris(mpRs, alMaxTris); }

		void SetDecalPosition(const cVector3f& avPosition) { ohpl_gx_decal_set_position(mpRs, &avPosition); }
		void SetDecalUp(const cVector3f& avUp, bool abComputeBasis=true) { ohpl_gx_decal_set_up(mpRs, &avUp, abComputeBasis); }
		void SetDecalRight(const cVector3f& avRight, bool abComputeBasis=true) { ohpl_gx_decal_set_right(mpRs, &avRight, abComputeBasis); }
		void SetDecalForward(const cVector3f& avForward, bool abComputeBasis=true) { ohpl_gx_decal_set_forward(mpRs, &avForward, abComputeBasis); }
		void SetDecalOffset(float afOffset) { ohpl_gx_decal_set_offset(mpRs, afOffset); }
		void SetDecalSize(const cVector3f& avSize) { ohpl_gx_decal_set_size(mpRs, &avSize); }
		void SetMaterial(const tString& asMat) { ohpl_gx_decal_set_material(mpRs, asMat.data(), asMat.size()); }
		void SetColor(const cColor& aCol) { ohpl_gx_decal_set_color(mpRs, &aCol); }
		void SetUVSubDivisions(const cVector2l& avSubDiv) { ohpl_gx_decal_set_sub_div(mpRs, &avSubDiv); }
		void SetCurrentSubDiv(int alSubDiv) { ohpl_gx_decal_set_current_sub_div(mpRs, alSubDiv); }

		bool IsUpdated() { return ohpl_gx_decal_is_updated(mpRs); }
		bool Compile() { return ohpl_gx_decal_compile(mpRs); }
		bool CanCreateDecal() { return ohpl_gx_decal_can_create(mpRs); }

		cMesh* CreateDecalMesh() { return static_cast<cMesh*>(ohpl_gx_decal_create_mesh(mpRs)); }

		cMaterial* GetMaterial() { return static_cast<cMaterial*>(ohpl_gx_decal_material(mpRs)); }

	private:
		OhplGxDecal *mpRs;
	};

};
#endif // HPL_DECALCREATOR_H
