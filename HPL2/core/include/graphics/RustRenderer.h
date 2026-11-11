#ifndef HPL_RUST_RENDERER_H
#define HPL_RUST_RENDERER_H

// C ABI of rust/src/scene/boxtree_dyn_ffi.rs and rust/src/render/renderer_ffi.rs.
// Handles (nodes, renderables, queries, frustums) cross as uintptr_t.

#include "math/RustTypes.h"
#include "math/MathTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {

	struct OhplRnTracker;

	struct OhplRcNodeBase
	{
		cVector3f mvMin;
		cVector3f mvMax;
		cVector3f mvCenter;
		float mfRadius;
		uint32_t mlRenderFlags;
		bool mbVisibleObjects;
		bool mbNeedPropertyUpdate;
		bool mbNeedAABBUpdate;
		bool mbUsesFlagsAndVisibility;
		float mfViewDistance;
		bool mbInsideView;
		int32_t mPrevFrustumCollision;
		uintptr_t mpParent;
	};

	struct OhplBtDynState
	{
		int32_t mlSplitAxis;
		float mfSplitPlane;
		bool mbRecalculateSplit;
		bool mbRecalculateSplitAxis;
		bool mbRecalculateAABB;
		bool mbObjectMoved;
		bool mbIsSplit;
		int32_t mlIgnoreSplitCount;
		int32_t mlGarbageCollectCount;
	};

	struct OhplBtTree
	{
		int32_t mlSplitThreshold;
		float mfMaxIntersectionAmount;
		int32_t mlMaxIgnoreSplitCount;
		int32_t mlMaxGarbageCollectCount;
		int32_t mlRebuildCount;
		int32_t mlMaxRebuildCount;
	};

	struct OhplRcBv
	{
		cVector3f mvMin, mvMax, mvCenter, mvPosition;
		float mfRadius;
	};

	struct OhplRnObjCheck
	{
		bool mbVisible, mbCulledByFog, mbCulledByDistance, mbHasBV;
		uint32_t mlRenderFlags;
		float mfFade;
		cVector3f mvMin, mvMax;
	};

	struct OhplRnMatView
	{
		bool mbPresent, mbTranslucent;
		int32_t mlAlphaMode;
		uintptr_t mpZProgram, mpDiffuseTexture;
	};

	struct OhplRnFrustumData
	{
		cVector3f mvOrigin, mvForward;
		cPlanef mvPlanes[6];
	};

	struct OhplRnChecks
	{
		bool mbOcclusionPlanesActive;
		const cPlanef *mpPlanes;
		size_t mlPlaneCount;
		bool mbFogCullPlaneActive;
		cPlanef mFogCullPlane;
		bool mbDistanceCullActive;
		cVector3f mvOrigin;
		float mfMinRangeSqr, mfScreenSizeSqr, mfRandomSize, mfFadeTime, mfFadeTimeInv, mfMaxRangeSqr, mfTime;
	};

	struct OhplRnChc
	{
		uintptr_t mpFrustum;
		cMatrixf m_mtxView;
		cVector3f mvOrigin;
		uint32_t mlNeededFlags;
		int32_t mlMinObjectsBeforeOcclusion;
		int32_t mlSampleLimit;
		bool mbOnlyRenderPrevVisible;
		int32_t mlOnlyPrevVisibleFrameCount;
	};

	struct OhplRnViewCheck
	{
		cPlanef mvPlanes[6];
		uint64_t mvBeyond[6];
		uint64_t mlBeyondCount;
	};

	struct OhplRnShadowQuery
	{
		uintptr_t mpLight;
		cVector3f mvLightOrigin;
		float mfMinRadius;
		uint32_t mlCasterFlag;
		bool mbHasViewCheck;
		OhplRnViewCheck mViewCheck;
	};

	struct OhplRnFogParams
	{
		bool mbShowBacksideInside, mbShowBacksideOutside, mbInsideNearPlane;
		float mfStart, mfEnd, mfColorA, mfFalloffExp;
	};

	struct OhplRnShadowSlot
	{
		uintptr_t mpLight;
		int32_t mlFrameCount;
	};

	struct OhplRnShadowCache
	{
		uintptr_t mpLight;
		int32_t mlTransformCount;
		float mfRadius, mfFOV, mfAspect, mfNearClip;
	};

	struct OhplRnLightKey
	{
		uintptr_t mpLight;
		int32_t mlTransformCount;
		float mfRadius;
		bool mbIsSpot;
		float mfFOV, mfAspect, mfNearClip;
	};

	static_assert(sizeof(OhplRnShadowSlot) == 16 && sizeof(OhplRnShadowCache) == 32 && sizeof(OhplRnLightKey) == 32, "OhplRnShadow layout");
	static_assert(sizeof(OhplRcNodeBase) == 72 && offsetof(OhplRcNodeBase, mpParent) == 64 && offsetof(OhplRcNodeBase, mPrevFrustumCollision) == 56, "OhplRcNodeBase layout");
	static_assert(sizeof(OhplBtDynState) == 24 && sizeof(OhplBtTree) == 24, "OhplBt layout");
	static_assert(sizeof(OhplRcBv) == 52 && sizeof(OhplRnObjCheck) == 36 && sizeof(OhplRnMatView) == 24, "OhplRn view layout");
	static_assert(sizeof(cPlanef) == 16 && sizeof(OhplRnFrustumData) == 120, "OhplRnFrustumData layout");
	static_assert(sizeof(OhplRnChecks) == 88 && sizeof(OhplRnChc) == 104, "OhplRnChecks layout");
	static_assert(sizeof(OhplRnViewCheck) == 152 && sizeof(OhplRnShadowQuery) == 184 && sizeof(OhplRnFogParams) == 20, "OhplRn query layout");

	struct OhplRnHostVt
	{
		OhplRcNodeBase* (*base)(void*, uintptr_t);
		OhplBtDynState* (*dyn_state)(void*, uintptr_t);
		size_t (*child_count)(void*, uintptr_t);
		size_t (*children)(void*, uintptr_t, uintptr_t*, size_t);
		size_t (*object_count)(void*, uintptr_t);
		size_t (*objects)(void*, uintptr_t, uintptr_t*, size_t);
		void (*push_object)(void*, uintptr_t, uintptr_t);
		void (*clear_objects)(void*, uintptr_t);
		void (*remove_first_object)(void*, uintptr_t, uintptr_t);
		void (*remove_all_objects)(void*, uintptr_t, uintptr_t);
		uintptr_t (*new_child)(void*, uintptr_t);
		void (*delete_children)(void*, uintptr_t);
		void (*delete_child)(void*, uintptr_t, uintptr_t);
		void (*clear_children)(void*, uintptr_t);
		void (*push_child)(void*, uintptr_t, uintptr_t);
		void (*delete_node)(void*, uintptr_t);
		uintptr_t (*object_node)(void*, uintptr_t);
		void (*set_object_node)(void*, uintptr_t, uintptr_t);
		void (*object_bv)(void*, uintptr_t, OhplRcBv*);
		bool (*object_visible)(void*, uintptr_t);
		uint32_t (*object_render_flags)(void*, uintptr_t);
		void (*update_node)(void*, uintptr_t);
		int32_t (*collide_node)(void*, uintptr_t, uintptr_t);
		int32_t (*collide_object)(void*, uintptr_t, uintptr_t);
		bool (*object_collides_frustum)(void*, uintptr_t, uintptr_t);
		bool (*near_plane_aabb)(void*, uintptr_t, const cVector3f*, const cVector3f*);
		void (*object_check)(void*, uintptr_t, OhplRnObjCheck*);
		void (*set_object_fade)(void*, uintptr_t, float);
		void (*object_material)(void*, uintptr_t, OhplRnMatView*);
		void (*set_view_space_z)(void*, uintptr_t, float);
		float (*view_space_z)(void*, uintptr_t);
		void (*add_to_render_list)(void*, uintptr_t);
		bool (*render_object)(void*, uintptr_t);
		void (*render_node_bbox)(void*, uintptr_t, uintptr_t);
		uintptr_t (*get_query)(void*);
		void (*release_query)(void*, uintptr_t);
		bool (*query_fetch)(void*, uintptr_t);
		uint32_t (*query_samples)(void*, uintptr_t);
		void (*flush)(void*);
		void (*caster)(void*, uintptr_t);
	};

	struct OhplRnOp
	{
		uint32_t mlKind;
		int32_t mlArg;
		int32_t mlList;
		uint32_t mlIndex;
	};
	static_assert(sizeof(OhplRnOp) == 16, "OhplRnOp layout");

	// ids of rust/src/render/renderer_simple.rs
	enum eOhplRnOpKind
	{
		eOhplRnOp_DepthFunc = 0,
		eOhplRnOp_DepthTest = 1,
		eOhplRnOp_DepthWrite = 2,
		eOhplRnOp_BlendNone = 3,
		eOhplRnOp_AlphaMode = 4,
		eOhplRnOp_ChannelMode = 5,
		eOhplRnOp_TextureRangeNone = 6,
		eOhplRnOp_Program = 7,
		eOhplRnOp_ObjTexture = 8,
		eOhplRnOp_ObjMatrix = 9,
		eOhplRnOp_ObjVertexBuffer = 10,
		eOhplRnOp_ObjBlend = 11,
		eOhplRnOp_ObjUpdateGraphics = 12,
		eOhplRnOp_ObjMatrixFrustum = 13,
		eOhplRnOp_Draw = 14,
		eOhplRnOp_Callback = 15
	};

extern "C" {

int32_t ohpl_rn_shadow_map_resolution(int32_t, int32_t);
int64_t ohpl_rn_pick_shadow_map(const OhplRnShadowSlot*, size_t, uintptr_t, int32_t, bool*);
bool ohpl_rn_shadow_cache_valid(const OhplRnShadowCache*, const OhplRnLightKey*);

size_t ohpl_rn_simple_ops(const bool*, size_t, size_t, size_t, size_t, bool, bool, OhplRnOp*, size_t);

void ohpl_bt_node_base_init(OhplRcNodeBase*, bool);
void ohpl_bt_node_dyn_init(OhplBtDynState*);
void ohpl_bt_tree_init(OhplBtTree*);
void ohpl_bt_update_before_rendering(const OhplRnHostVt*, void*, uintptr_t);
void ohpl_bt_object_properties_changed(const OhplRnHostVt*, void*, uintptr_t);
void ohpl_bt_calculate_min_max(const OhplRnHostVt*, void*, uintptr_t);
void ohpl_bt_push_up_aabb_update(const OhplRnHostVt*, void*, uintptr_t);
void ohpl_bt_boxtree_remove(const OhplRnHostVt*, void*, uintptr_t, uintptr_t);
void ohpl_bt_boxtree_remove_set(const OhplRnHostVt*, void*, uintptr_t, const uintptr_t*, size_t);
void ohpl_bt_dyn_update_before_use(const OhplRnHostVt*, void*, const OhplBtTree*, uintptr_t);
uintptr_t ohpl_bt_dyn_add(const OhplRnHostVt*, void*, OhplBtTree*, uintptr_t, uintptr_t);
bool ohpl_bt_dyn_remove(const OhplRnHostVt*, void*, OhplBtTree*, uintptr_t);
void ohpl_bt_dyn_specific_update(const OhplRnHostVt*, void*, OhplBtTree*, uintptr_t, const uintptr_t*, size_t);
uintptr_t ohpl_bt_dyn_get_add_node(const OhplRnHostVt*, void*, const OhplBtTree*, uintptr_t, uintptr_t);

OhplRnTracker* ohpl_rn_tracker_new();
void ohpl_rn_tracker_free(OhplRnTracker*);
void ohpl_rn_tracker_switch(OhplRnTracker*);
void ohpl_rn_tracker_set_visible(OhplRnTracker*, uintptr_t);
bool ohpl_rn_tracker_was_visible(const OhplRnTracker*, uintptr_t);
size_t ohpl_rn_tracker_pending(const OhplRnTracker*, uintptr_t*, size_t);
void ohpl_rn_tracker_reset(OhplRnTracker*);

bool ohpl_rn_object_visible(const OhplRnHostVt*, void*, const OhplRnChecks*, uintptr_t, uint32_t);
bool ohpl_rn_node_visible(const OhplRnHostVt*, void*, const OhplRnChecks*, uintptr_t);
float ohpl_rn_distance_fade_amount(const OhplRnChecks*, float);
int32_t ohpl_rn_chc_traverse(const OhplRnHostVt*, void*, const OhplRnChecks*, OhplRnTracker*, const OhplRnChc*, const uintptr_t*, size_t);
void ohpl_rn_add_visible_to_list(const OhplRnHostVt*, void*, const OhplRnChecks*, OhplRnTracker*, uintptr_t, bool, uint32_t, uintptr_t);
void ohpl_rn_fetch_delayed(const OhplRnHostVt*, void*, OhplRnTracker*, int32_t);
bool ohpl_rn_has_delayed_queries(const OhplRnTracker*);
void ohpl_rn_render_delayed(const OhplRnHostVt*, void*, OhplRnTracker*);
bool ohpl_rn_delayed_culled(const OhplRnHostVt*, void*, OhplRnTracker*, uintptr_t, uintptr_t);
void ohpl_rn_view_check(const OhplRnFrustumData*, const OhplRnFrustumData*, OhplRnViewCheck*);
void ohpl_rn_shadow_casters(const OhplRnHostVt*, void*, const OhplRnChecks*, const OhplRnShadowQuery*, uintptr_t);
void ohpl_rn_sort_shadow_casters(const OhplRnHostVt*, void*, uintptr_t*, size_t);
bool ohpl_rn_fog_inside_near_plane(const cMatrixf*, const cVector3f*);
float ohpl_rn_fog_visibility(const OhplRnFogParams*, const cMatrixf*, const cVector3f*, const cVector3f*, const cVector3f*);
void ohpl_rn_fog_sort_order(const float*, size_t, uint32_t*);

}
}

#endif
