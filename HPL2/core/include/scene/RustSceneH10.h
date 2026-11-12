#ifndef HPL_RUST_SCENE_H10_H
#define HPL_RUST_SCENE_H10_H

// C ABI of rust/src/scene/ffi_h10.rs. Layouts mirror rust/src/scene/h10.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {
	class cNode3D;
	class iEntity3D;

	struct OhplH10EntView
	{
		cMatrixf *mpLocal;
		cMatrixf *mpWorld;
		bool *mpTransformUpdated;
		bool *mpUpdateBV;
		bool *mpApplyBV;
		int *mpCount;
		uintptr_t mlParentNode;
		uintptr_t mlParentEntity;
	};

	struct OhplH10Node
	{
		cMatrixf m_mtxLocal;
		cMatrixf m_mtxWorld;
		cMatrixf m_mtxPre;
		cMatrixf m_mtxPost;
		cQuaternion mqRotation;
		cVector3f mvScale;
		cVector3f mvTranslation;
		cNode3D *mpParent;
		iEntity3D *mpEntityParent;
		bool mbTransformUpdated;
		bool mbUsePre;
		bool mbUsePost;
	};

	struct OhplH10Cam
	{
		cVector3f mvPosition;
		float mfFOV;
		float mfAspect;
		float mfFarClipPlane;
		float mfNearClipPlane;
		cVector2f mvViewSize;
		int mlProjectionType;
		float mfPitch;
		float mfYaw;
		float mfRoll;
		float mfPitchLimitMin;
		float mfPitchLimitMax;
		float mfYawLimitMin;
		float mfYawLimitMax;
		int mlRotateMode;
		int mlMoveMode;
		cMatrixf m_mtxView;
		cMatrixf m_mtxProjection;
		cMatrixf m_mtxMove;
		cMatrixf m_mtxPrevView;
		cMatrixf m_mtxPrevProjection;
		cMatrixf m_mtxMatrixRotation;
		bool mbInfFarPlane;
		bool mbViewUpdated;
		bool mbProjectionUpdated;
		bool mbMoveUpdated;
		bool mbFrustumUpdated;
	};

	struct OhplH10Host
	{
		void (*ent_view)(uintptr_t, OhplH10EntView*);
		size_t (*ent_children)(uintptr_t, uintptr_t*, size_t);
		size_t (*ent_node_children)(uintptr_t, uintptr_t*, size_t);
		void (*ent_on_updated)(uintptr_t);
		void (*ent_callbacks)(uintptr_t);
		OhplH10Node* (*node_state)(uintptr_t);
		size_t (*node_children)(uintptr_t, uintptr_t*, size_t);
		size_t (*node_entities)(uintptr_t, uintptr_t*, size_t);
	};

	static_assert(sizeof(OhplH10EntView) == 64 && offsetof(OhplH10EntView, mlParentNode) == 48, "H10 entity view layout");
	static_assert(sizeof(OhplH10Node) == 320 && offsetof(OhplH10Node, mpParent) == 296, "H10 node layout");
	static_assert(sizeof(OhplH10Cam) == 468 && offsetof(OhplH10Cam, mlRotateMode) == 68, "H10 camera layout");

const OhplH10Host* OhplH10GetHost();

extern "C" {

void ohpl_h10_ent_update_world(const OhplH10Host*, uintptr_t);
void ohpl_h10_ent_set_transform_updated(const OhplH10Host*, uintptr_t, bool);
void ohpl_h10_ent_set_matrix(const OhplH10Host*, uintptr_t, const cMatrixf*);
void ohpl_h10_ent_set_world_matrix(const OhplH10Host*, uintptr_t, const cMatrixf*);
void ohpl_h10_ent_set_position(const OhplH10Host*, uintptr_t, const cVector3f*);
void ohpl_h10_ent_set_world_position(const OhplH10Host*, uintptr_t, const cVector3f*);
bool ohpl_h10_ent_bv_dirty(const OhplH10Host*, uintptr_t);

void ohpl_h10_node_update_world(const OhplH10Host*, uintptr_t);
void ohpl_h10_node_set_world_transform_updated(const OhplH10Host*, uintptr_t);
void ohpl_h10_node_set_matrix(const OhplH10Host*, uintptr_t, const cMatrixf*, bool);
void ohpl_h10_node_set_world_matrix(const OhplH10Host*, uintptr_t, const cMatrixf*);
void ohpl_h10_node_set_position(const OhplH10Host*, uintptr_t, const cVector3f*);
void ohpl_h10_node_set_world_position(const OhplH10Host*, uintptr_t, const cVector3f*);
void ohpl_h10_node_add_rotation_euler(OhplH10Node*, const cVector3f*, int);
void ohpl_h10_node_add_rotation(OhplH10Node*, const cQuaternion*);
void ohpl_h10_node_add_scale(OhplH10Node*, const cVector3f*);
void ohpl_h10_node_add_translation(OhplH10Node*, const cVector3f*);
void ohpl_h10_node_update_matrix(const OhplH10Host*, uintptr_t, bool);
void ohpl_h10_node_apply_anim(const OhplH10Host*, uintptr_t, bool, bool);

void ohpl_h10_cam_set_position(OhplH10Cam*, const cVector3f*);
void ohpl_h10_cam_set_pitch(OhplH10Cam*, float);
void ohpl_h10_cam_set_yaw(OhplH10Cam*, float);
void ohpl_h10_cam_set_roll(OhplH10Cam*, float);
void ohpl_h10_cam_add_pitch(OhplH10Cam*, float);
void ohpl_h10_cam_add_yaw(OhplH10Cam*, float);
void ohpl_h10_cam_add_roll(OhplH10Cam*, float);
void ohpl_h10_cam_move(OhplH10Cam*, int, float);
void ohpl_h10_cam_set_fov(OhplH10Cam*, float);
void ohpl_h10_cam_set_aspect(OhplH10Cam*, float);
void ohpl_h10_cam_set_far(OhplH10Cam*, float);
void ohpl_h10_cam_set_near(OhplH10Cam*, float);
void ohpl_h10_cam_set_projection_type(OhplH10Cam*, int);
void ohpl_h10_cam_set_ortho_view_size(OhplH10Cam*, const cVector2f*);
void ohpl_h10_cam_set_inf_far(OhplH10Cam*, bool);
void ohpl_h10_cam_set_rotate_mode(OhplH10Cam*, int);
void ohpl_h10_cam_set_move_mode(OhplH10Cam*, int);
void ohpl_h10_cam_reset_rotation(OhplH10Cam*);
void ohpl_h10_cam_set_rotation_row(OhplH10Cam*, int, const cVector3f*);
void ohpl_h10_cam_set_rotation_matrix(OhplH10Cam*, const cMatrixf*);
void ohpl_h10_cam_frustum_end(OhplH10Cam*, bool);
uint32_t ohpl_h10_cam_frustum_begin(OhplH10Cam*);
const cMatrixf* ohpl_h10_cam_matrix(OhplH10Cam*, int);
void ohpl_h10_cam_view_row(OhplH10Cam*, int, bool, cVector3f*);

}
}

#endif
