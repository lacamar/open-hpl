#include "resources/EntityObjectBuilder.h"
#include "resources/RustEntityLoader.h"

#include "system/RustXml.h"
#include "resources/XmlDocument.h"

#include <cstring>

namespace hpl {

	namespace {

	struct cCtx
	{
		iEntityObjectBuilder *mpB;
		std::vector<cXmlElement*> mvElems;
		tString msTmp;
	};

	cCtx& C(void *apCtx){ return *static_cast<cCtx*>(apCtx); }
	iEntityObjectBuilder& B(void *apCtx){ return *C(apCtx).mpB; }

	OhplEntStr S(const tString &asStr){ OhplEntStr s = { asStr.data(), asStr.size() }; return s; }
	tString T(OhplEntStr aStr){ return tString(aStr.p, aStr.n); }
	const cMatrixf& M(const OhplEntMat *apMat){ return *reinterpret_cast<const cMatrixf*>(apMat); }
	void Out(const cMatrixf &a_mtx, OhplEntMat *apOut){ memcpy(apOut, &a_mtx, sizeof(cMatrixf)); }
	cVector3f V(OhplEntVec3 aVec){ return cVector3f(aVec.x, aVec.y, aVec.z); }
	OhplEntVec3 V(const cVector3f &avVec){ OhplEntVec3 v = { avVec.x, avVec.y, avVec.z }; return v; }

	static_assert(sizeof(cMatrixf) == sizeof(OhplEntMat), "cMatrixf layout");

	void CopyElement(OhplXmlDoc *apDoc, unsigned alId, cXmlElement *apElem, std::vector<cXmlElement*> &avElems)
	{
		if(avElems.size() <= alId) avElems.resize(alId + 1, NULL);
		avElems[alId] = apElem;

		tAttributeMap *pAttributeMap = apElem->GetAttributeMap();
		for(tAttributeMapIt it = pAttributeMap->begin(); it != pAttributeMap->end(); ++it)
			ohpl_xml_doc_set_attr(apDoc, alId, it->first.c_str(), it->first.size(), it->second.c_str(), it->second.size());

		cXmlNodeListIterator it = apElem->GetChildIterator();
		while(it.HasNext())
		{
			cXmlElement *pChild = it.Next()->ToElement();
			const tString &sName = pChild->GetValue();
			CopyElement(apDoc, ohpl_xml_doc_add_element(apDoc, alId, sName.c_str(), sName.size()), pChild, avElems);
		}
	}

	OhplEntVt MakeVt(cCtx *apCtx)
	{
		OhplEntVt vt;
		vt.ctx = apCtx;
		vt.error = [](void *c, OhplEntStr s) noexcept { B(c).Error(T(s)); };
		vt.warning = [](void *c, OhplEntStr s) noexcept { B(c).Warning(T(s)); };
		vt.before_load = [](void *c) noexcept { B(c).BeforeLoad(); };
		vt.after_load = [](void *c) noexcept { B(c).AfterLoad(); };
		vt.before_joints = [](void *c) noexcept { return B(c).BeforeJoints(); };
		vt.flags = [](void *c, OhplEntFlags *o) noexcept {
			cEobFlags f = B(c).Flags();
			o->animations = f.mbAnimations; o->particle_systems = f.mbParticleSystems; o->billboards = f.mbBillboards;
			o->sounds = f.mbSounds; o->lights = f.mbLights;
			o->sub_mesh_scale_includes_model_scale = f.mbSubMeshScaleIncludesModelScale;
			o->sub_mesh_instance_scale_after_rotation = f.mbSubMeshInstanceScaleAfterRotation;
			o->sub_mesh_materials = f.mbSubMeshMaterials;
			o->share_body_shapes = f.mbShareBodyShapes;
		};
		vt.create_mesh = [](void *c, OhplEntStr s, bool *skel, int *anim) noexcept { return B(c).CreateMesh(T(s), *skel, *anim); };
		vt.create_mesh_entity = [](void *c, OhplEntStr s) noexcept { return (OhplEntRaw)B(c).CreateMeshEntity(T(s)); };
		vt.set_matrix = [](void *c, OhplEntRaw e, const OhplEntMat *m) noexcept { B(c).SetMatrix(e, M(m)); };
		vt.set_shadow_caster = [](void *c, OhplEntRaw e) noexcept { B(c).SetShadowCaster(e); };
		vt.update_logic = [](void *c, OhplEntRaw e) noexcept { B(c).UpdateLogic(e); };
		vt.get_sub_mesh_entity = [](void *c, OhplEntStr s) noexcept { return (OhplEntRaw)B(c).GetSubMeshEntity(T(s)); };
		vt.warn_missing_sub_mesh = [](void *c, OhplEntStr s) noexcept { B(c).WarnMissingSubMesh(T(s)); };
		vt.get_sub_mesh_model_scale = [](void *c, OhplEntRaw e) noexcept { return V(B(c).GetSubMeshModelScale(e)); };
		vt.set_world_matrix = [](void *c, OhplEntRaw e, const OhplEntMat *m) noexcept { B(c).SetWorldMatrix(e, M(m)); };
		vt.set_unique_id = [](void *c, OhplEntRaw e, int id) noexcept { B(c).SetUniqueID(e, id); };
		vt.set_sub_mesh_material = [](void *c, OhplEntRaw e, OhplEntStr s) noexcept { B(c).SetSubMeshMaterial(e, T(s)); };
		vt.add_animation = [](void *c, OhplEntStr f, OhplEntStr n, float speed) noexcept { return (OhplEntRaw)B(c).AddAnimation(T(f), T(n), speed); };
		vt.set_special_event_time = [](void *c, OhplEntRaw st, float t) noexcept { B(c).SetSpecialEventTime(st, t); };
		vt.add_animation_event = [](void *c, OhplEntRaw st, float t, int ty, OhplEntStr v, OhplEntStr n, OhplEntStr d) noexcept {
			B(c).AddAnimationEvent(st, t, ty, T(v), T(n), T(d));
		};
		vt.load_world_entity = [](void *c, int kind, unsigned node, OhplEntStr prefix) noexcept {
			return (OhplEntRaw)B(c).LoadWorldEntity((eEobWorldEntity)kind, C(c).mvElems[node], T(prefix));
		};
		vt.get_local_position = [](void *c, OhplEntRaw e) noexcept { return V(B(c).GetLocalPosition(e)); };
		vt.set_position = [](void *c, OhplEntRaw e, OhplEntVec3 p) noexcept { B(c).SetPosition(e, V(p)); };
		vt.link_light_billboards = [](void *c) noexcept { B(c).LinkLightBillboards(); };
		vt.get_bone_state = [](void *c, OhplEntStr s) noexcept { return (OhplEntRaw)B(c).GetBoneState(T(s)); };
		vt.get_bone_state_num = [](void *c) noexcept { return B(c).GetBoneStateNum(); };
		vt.warn_skeleton_mismatch = [](void *c) noexcept { B(c).WarnSkeletonMismatch(); };
		vt.get_bone_world_matrix = [](void *c, OhplEntRaw b, OhplEntMat *o) noexcept { Out(B(c).GetBoneWorldMatrix(b), o); };
		vt.get_bone_name = [](void *c, OhplEntRaw b) noexcept { C(c).msTmp = B(c).GetBoneName(b); return S(C(c).msTmp); };
		vt.get_unique_id = [](void *c, OhplEntRaw e) noexcept { return B(c).GetUniqueID(e); };
		vt.is_sub_mesh = [](void *c, OhplEntRaw e) noexcept { return B(c).IsSubMesh(e); };
		vt.get_local_matrix = [](void *c, OhplEntRaw e, OhplEntMat *o) noexcept { Out(B(c).GetLocalMatrix(e), o); };
		vt.get_world_matrix = [](void *c, OhplEntRaw e, OhplEntMat *o) noexcept { Out(B(c).GetWorldMatrix(e), o); };
		vt.get_entity_name = [](void *c, OhplEntRaw e) noexcept { C(c).msTmp = B(c).GetEntityName(e); return S(C(c).msTmp); };
		vt.set_node_animation = [](void *c) noexcept { B(c).SetNodeAnimation(); };
		vt.attach_entity_child = [](void *c, OhplEntRaw p, const OhplEntMat *m, OhplEntRaw ch) noexcept { B(c).AttachEntityChild(p, M(m), ch); };
		vt.attach_bone_child = [](void *c, OhplEntRaw p, const OhplEntMat *m, OhplEntRaw ch) noexcept { B(c).AttachBoneChild(p, M(m), ch); };
		vt.attach_bone_to_body = [](void *c, OhplEntRaw p, const OhplEntMat *m, OhplEntRaw ch) noexcept { B(c).AttachBoneToBody(p, M(m), ch); };
		vt.create_box_shape = [](void *c, OhplEntVec3 s, const OhplEntMat *m) noexcept { return (OhplEntRaw)B(c).CreateBoxShape(V(s), M(m)); };
		vt.create_sphere_shape = [](void *c, OhplEntVec3 s, const OhplEntMat *m) noexcept { return (OhplEntRaw)B(c).CreateSphereShape(V(s), M(m)); };
		vt.create_cylinder_shape = [](void *c, float r, float h, const OhplEntMat *m) noexcept { return (OhplEntRaw)B(c).CreateCylinderShape(r, h, M(m)); };
		vt.create_capsule_shape = [](void *c, float r, float h, const OhplEntMat *m) noexcept { return (OhplEntRaw)B(c).CreateCapsuleShape(r, h, M(m)); };
		vt.create_compound_shape = [](void *c, const OhplEntRaw *s, size_t n) noexcept {
			return (OhplEntRaw)B(c).CreateCompoundShape(std::vector<tEob>(s, s + n));
		};
		vt.inc_shape_user_count = [](void *c, OhplEntRaw s) noexcept { B(c).IncShapeUserCount(s); };
		vt.destroy_shape = [](void *c, OhplEntRaw s) noexcept { B(c).DestroyShape(s); };
		vt.create_body = [](void *c, OhplEntStr n, OhplEntRaw s) noexcept { return (OhplEntRaw)B(c).CreateBody(T(n), s); };
		vt.set_body_properties = [](void *c, OhplEntRaw b, const OhplEntBodyProps *p) noexcept {
			cEobBodyProps o;
			memcpy(&o.m_mtxMatrix, &p->matrix, sizeof(cMatrixf));
			o.mfMass = p->mass; o.mfAngularDamping = p->angular_damping; o.mfLinearDamping = p->linear_damping;
			o.mfBuoyancyDensityMul = p->buoyancy_density_mul; o.mfMaxAngularSpeed = p->max_angular_speed; o.mfMaxLinearSpeed = p->max_linear_speed;
			o.mbBlocksSound = p->blocks_sound; o.mbCollideCharacter = p->collide_character; o.mbCollide = p->collide;
			o.mbGravity = p->gravity; o.mbContinuousCollision = p->continuous_collision;
			o.mbPushedByCharacterGravity = p->pushed_by_character_gravity; o.mbVolatile = p->volatile_;
			o.mbUseSurfaceEffects = p->use_surface_effects; o.mbCanAttachCharacter = p->can_attach_character;
			o.mlID = p->id;
			B(c).SetBodyProperties(b, o);
		};
		vt.set_body_material = [](void *c, OhplEntRaw b, OhplEntStr n) noexcept { B(c).SetBodyMaterial(b, T(n)); };
		vt.register_body = [](void *c, OhplEntRaw b) noexcept { B(c).RegisterBody(b); };
		vt.create_joint_hinge = [](void *c, OhplEntStr n, OhplEntVec3 pv, OhplEntVec3 pn, OhplEntRaw p, OhplEntRaw ch) noexcept {
			return (OhplEntRaw)B(c).CreateJointHinge(T(n), V(pv), V(pn), p, ch); };
		vt.create_joint_ball = [](void *c, OhplEntStr n, OhplEntVec3 pv, OhplEntVec3 pn, OhplEntRaw p, OhplEntRaw ch) noexcept {
			return (OhplEntRaw)B(c).CreateJointBall(T(n), V(pv), V(pn), p, ch); };
		vt.create_joint_slider = [](void *c, OhplEntStr n, OhplEntVec3 pv, OhplEntVec3 pn, OhplEntRaw p, OhplEntRaw ch) noexcept {
			return (OhplEntRaw)B(c).CreateJointSlider(T(n), V(pv), V(pn), p, ch); };
		vt.create_joint_screw = [](void *c, OhplEntStr n, OhplEntVec3 pv, OhplEntVec3 pn, OhplEntRaw p, OhplEntRaw ch) noexcept {
			return (OhplEntRaw)B(c).CreateJointScrew(T(n), V(pv), V(pn), p, ch); };
		vt.set_hinge_limits = [](void *c, OhplEntRaw j, float a, float b) noexcept { B(c).SetHingeLimits(j, a, b); };
		vt.set_ball_limits = [](void *c, OhplEntRaw j, float a, float b) noexcept { B(c).SetBallLimits(j, a, b); };
		vt.set_slider_limits = [](void *c, OhplEntRaw j, float a, float b) noexcept { B(c).SetSliderLimits(j, a, b); };
		vt.set_screw_limits = [](void *c, OhplEntRaw j, float a, float b) noexcept { B(c).SetScrewLimits(j, a, b); };
		vt.set_joint_properties = [](void *c, OhplEntRaw j, const OhplEntJointProps *p) noexcept {
			cEobJointProps o;
			o.msMoveSound = T(p->move_sound); o.msBreakSound = T(p->break_sound);
			o.msMaxLimitSound = T(p->max_limit_sound); o.msMinLimitSound = T(p->min_limit_sound);
			const float *f = p->floats;
			o.mfMinMoveSpeed = f[0]; o.mfMinMoveFreq = f[1]; o.mfMinMoveVolume = f[2]; o.mfMinMoveFreqSpeed = f[3];
			o.mfMaxMoveFreq = f[4]; o.mfMaxMoveVolume = f[5]; o.mfMaxMoveFreqSpeed = f[6]; o.mfMiddleMoveSpeed = f[7];
			o.mfMiddleMoveVolume = f[8]; o.mfBreakForce = f[9]; o.mfLimitAutoSleepDist = f[10];
			o.mfMaxLimitMaxSpeed = f[11]; o.mfMaxLimitMinSpeed = f[12]; o.mfMinLimitMaxSpeed = f[13]; o.mfMinLimitMinSpeed = f[14];
			o.mbAngularMove = p->bools[0]; o.mbStickyMinLimit = p->bools[1]; o.mbStickyMaxLimit = p->bools[2];
			o.mbBreakable = p->bools[3]; o.mbLimitAutoSleep = p->bools[4]; o.mbCollideBodies = p->bools[5];
			o.mlLimitAutoSleepNumSteps = p->ints[0]; o.mlID = p->ints[1];
			B(c).SetJointProperties(j, o);
		};
		vt.register_joint = [](void *c, OhplEntRaw j) noexcept { B(c).RegisterJoint(j); };
		vt.get_animation_state_num = [](void *c) noexcept { return B(c).GetAnimationStateNum(); };
		vt.play_first_animation = [](void *c) noexcept { B(c).PlayFirstAnimation(); };
		vt.set_skeleton_physics_active = [](void *c) noexcept { B(c).SetSkeletonPhysicsActive(); };
		vt.load_user_variables = [](void *c) noexcept { B(c).LoadUserVariables(); };
		return vt;
	}

	}

	bool ParseEntityObject(iEntityObjectBuilder &aBuilder, const cEobParams &aParams, cXmlElement *apRootElem)
	{
		cCtx ctx;
		ctx.mpB = &aBuilder;

		OhplXmlDocPtr doc;
		const tString &sRoot = apRootElem->GetValue();
		unsigned lRoot = ohpl_xml_doc_add_element(doc.p, 0, sRoot.c_str(), sRoot.size());
		CopyElement(doc.p, lRoot, apRootElem, ctx.mvElems);

		OhplEntParams params;
		params.name = S(aParams.msName);
		params.file_name = S(aParams.msFileName);
		params.full_path = S(aParams.msFullPath);
		params.full_path_dir = S(aParams.msFullPathDir);
		memcpy(&params.transform, &aParams.m_mtxTransform, sizeof(cMatrixf));
		params.scale = V(aParams.mvScale);
		params.physics_world = aParams.mbPhysicsWorld;

		OhplEntVt vt = MakeVt(&ctx);
		return ohpl_ent_parse(doc.p, lRoot, &params, &vt);
	}

};
