#ifndef HPL_RUST_SCENE_H10_H
#define HPL_RUST_SCENE_H10_H

// C ABI of rust/src/scene/ffi_h10.rs. Layouts mirror rust/src/scene/h10.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

namespace hpl {
	class cNode3D;
	class iEntity3D;

	struct OhplNodePod;

	struct OhplEntPod
	{
		cMatrixf local;
		cMatrixf world;
		uintptr_t owner;
		OhplNodePod *parentNode;
		OhplEntPod *parentEntity;
		int count;
		int uniqueId;
		int iteratorCount;
		bool transformUpdated;
		bool updateBV;
		bool applyBV;
		bool active;
		bool saved;
	};

	struct OhplNodePod
	{
		cMatrixf m_mtxLocal;
		cMatrixf m_mtxWorld;
		cMatrixf m_mtxPre;
		cMatrixf m_mtxPost;
		cQuaternion mqRotation;
		cVector3f mvScale;
		cVector3f mvTranslation;
		uintptr_t owner;
		OhplNodePod *mpParent;
		OhplEntPod *mpEntityParent;
		int customFlags;
		bool mbTransformUpdated;
		bool mbUsePre;
		bool mbUsePost;
		bool active;
		bool autoDelete;
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
		void (*ent_on_updated)(uintptr_t);
		void (*ent_callbacks)(uintptr_t);
		void (*node_delete)(uintptr_t);
	};

	static_assert(sizeof(OhplEntPod) == 176 && offsetof(OhplEntPod, owner) == 128 && offsetof(OhplEntPod, count) == 152 && offsetof(OhplEntPod, transformUpdated) == 164, "H10 entity pod layout");
	static_assert(sizeof(OhplNodePod) == 336 && offsetof(OhplNodePod, owner) == 296 && offsetof(OhplNodePod, customFlags) == 320 && offsetof(OhplNodePod, mbTransformUpdated) == 324, "H10 node pod layout");
	static_assert(sizeof(OhplH10Cam) == 468 && offsetof(OhplH10Cam, mlRotateMode) == 68, "H10 camera layout");

const OhplH10Host* OhplH10GetHost();

extern "C" {

OhplEntPod* ohpl_ent_new(uintptr_t);
void ohpl_ent_free(OhplEntPod*);
void ohpl_h10_ent_update_world(const OhplH10Host*, OhplEntPod*);
void ohpl_h10_ent_set_transform_updated(const OhplH10Host*, OhplEntPod*, bool);
void ohpl_h10_ent_set_matrix(const OhplH10Host*, OhplEntPod*, const cMatrixf*);
void ohpl_h10_ent_set_world_matrix(const OhplH10Host*, OhplEntPod*, const cMatrixf*);
void ohpl_h10_ent_set_position(const OhplH10Host*, OhplEntPod*, const cVector3f*);
void ohpl_h10_ent_set_world_position(const OhplH10Host*, OhplEntPod*, const cVector3f*);
bool ohpl_h10_ent_bv_dirty(const OhplH10Host*, OhplEntPod*);
void ohpl_ent_add_child(const OhplH10Host*, OhplEntPod*, OhplEntPod*);
void ohpl_ent_remove_child(OhplEntPod*, OhplEntPod*);
bool ohpl_ent_is_child(const OhplEntPod*, const OhplEntPod*);
size_t ohpl_ent_child_count(const OhplEntPod*);
uintptr_t ohpl_ent_child_at(const OhplEntPod*, size_t);
void ohpl_ent_add_node_child(const OhplH10Host*, OhplEntPod*, OhplNodePod*);
void ohpl_ent_remove_node_child(OhplEntPod*, OhplNodePod*);
bool ohpl_ent_is_node_child(const OhplEntPod*, const OhplNodePod*);
size_t ohpl_ent_node_child_count(const OhplEntPod*);
uintptr_t ohpl_ent_node_child_at(const OhplEntPod*, size_t);

OhplNodePod* ohpl_node_new(uintptr_t, bool);
void ohpl_node_free(const OhplH10Host*, OhplNodePod*);
bool ohpl_node_add_entity(OhplNodePod*, OhplEntPod*);
bool ohpl_node_remove_entity(OhplNodePod*, OhplEntPod*);
void ohpl_node_clear_entities(OhplNodePod*);
void ohpl_node_set_parent(OhplNodePod*, OhplNodePod*);
void ohpl_node_add_child(OhplNodePod*, OhplNodePod*);
void ohpl_node_remove_child(OhplNodePod*, OhplNodePod*);
size_t ohpl_node_child_count(const OhplNodePod*);
uintptr_t ohpl_node_child_at(const OhplNodePod*, size_t);
size_t ohpl_node_entity_count(const OhplNodePod*);
uintptr_t ohpl_node_entity_at(const OhplNodePod*, size_t);
void ohpl_node_update_entity_children(const OhplH10Host*, OhplNodePod*);
void ohpl_h10_node_update_world(const OhplH10Host*, OhplNodePod*);
void ohpl_h10_node_set_world_transform_updated(const OhplH10Host*, OhplNodePod*);
void ohpl_h10_node_set_matrix(const OhplH10Host*, OhplNodePod*, const cMatrixf*, bool);
void ohpl_h10_node_set_world_matrix(const OhplH10Host*, OhplNodePod*, const cMatrixf*);
void ohpl_h10_node_set_position(const OhplH10Host*, OhplNodePod*, const cVector3f*);
void ohpl_h10_node_set_world_position(const OhplH10Host*, OhplNodePod*, const cVector3f*);
void ohpl_h10_node_add_rotation_euler(OhplNodePod*, const cVector3f*, int);
void ohpl_h10_node_add_rotation(OhplNodePod*, const cQuaternion*);
void ohpl_h10_node_add_scale(OhplNodePod*, const cVector3f*);
void ohpl_h10_node_add_translation(OhplNodePod*, const cVector3f*);
void ohpl_h10_node_update_matrix(const OhplH10Host*, OhplNodePod*, bool);
void ohpl_h10_node_apply_anim(const OhplH10Host*, OhplNodePod*, bool, bool);

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
