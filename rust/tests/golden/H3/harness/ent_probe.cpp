// ent_probe ROOT LIST [FULL]: parses every .ent in LIST (paths relative to ROOT) under two builder
// configurations through ParseEntityObject and prints "<rel>\t<calls>\t<fnv64>" per file.
// The recording builder answers deterministically from its arguments; ent_golden.rs mirrors it.
#include "common.h"
#include "impl/XmlDocumentTiny.h"
#include "resources/EntityObjectBuilder.h"
#include "system/String.h"
#include <cstring>
#include <fstream>
#include <iostream>

using namespace hpl;

// NaN is printed "nan:<bits>"; the second digest drops the bits, as the cargo replay does (debug and release builds order NaN operands differently).
static std::string F(float f) { uint32_t u; memcpy(&u, &f, 4); char b[16]; snprintf(b, sizeof b, f != f ? "nan:%08x" : "%08x", u); return b; }
static std::string NoNanBits(const std::string &s)
{
	std::string out;
	for(size_t i = 0; i < s.size();)
	{
		if(s.compare(i, 4, "nan:") == 0 && i + 12 <= s.size()) { out += "nan"; i += 12; }
		else out += s[i++];
	}
	return out;
}
static std::string V(const cVector3f &v) { return F(v.x) + "," + F(v.y) + "," + F(v.z); }
static std::string M(const cMatrixf &m)
{
	std::string s;
	for(int r = 0; r < 4; ++r) for(int c = 0; c < 4; ++c) s += F(m.m[r][c]) + (r == 3 && c == 3 ? "" : ",");
	return s;
}
static uint32_t Sum(const std::string &s) { uint32_t h = 0; for(unsigned char c : s) h = h * 31 + c; return h; }
static std::string B(bool b) { return b ? "1" : "0"; }

class cRec : public iEntityObjectBuilder
{
public:
	cRec(bool abFull, std::string &asLog) : mbFull(abFull), msLog(asLog), mlNext(0), mlCalls(0) {}
	size_t mlCalls;

	void Put(const std::string &s) { msLog += s + "\n"; ++mlCalls; }
	tEob New() { return ++mlNext; }
	tEob Maybe(const tString &s, uint32_t aMod) { uint32_t h = Sum(s); return h % aMod == 0 ? 0 : New(); }

	void Error(const tString &s) override { Put("error " + s); }
	void Warning(const tString &s) override { Put("warning " + s); }
	void BeforeLoad() override { Put("before_load"); }
	void AfterLoad() override { Put("after_load"); }
	bool BeforeJoints() override { Put("before_joints"); return mbFull; }
	cEobFlags Flags() override
	{
		cEobFlags f;
		f.mbAnimations = f.mbParticleSystems = f.mbBillboards = f.mbSounds = f.mbLights = mbFull;
		f.mbSubMeshScaleIncludesModelScale = f.mbSubMeshInstanceScaleAfterRotation = f.mbSubMeshMaterials = f.mbShareBodyShapes = mbFull;
		return f;
	}

	bool CreateMesh(const tString &f, bool &abSkeleton, int &alAnimNum) override
	{
		uint32_t h = Sum(f);
		abSkeleton = h & 1; alAnimNum = (h >> 1) % 3;
		Put("create_mesh " + f + " -> " + B(abSkeleton) + " " + std::to_string(alAnimNum));
		return h % 7 != 0;
	}
	tEob CreateMeshEntity(const tString &n) override { Put("mesh_entity " + n); return New(); }
	void SetMatrix(tEob e, const cMatrixf &m) override { Put("set_matrix " + std::to_string(e) + " " + M(m)); }
	void SetShadowCaster(tEob e) override { Put("shadow_caster " + std::to_string(e)); }
	void UpdateLogic(tEob e) override { Put("update_logic " + std::to_string(e)); }
	tEob GetSubMeshEntity(const tString &n) override { Put("get_sub_mesh " + n); return Maybe(n, 4); }
	void WarnMissingSubMesh(const tString &n) override { Put("warn_missing_sub_mesh " + n); }
	cVector3f GetSubMeshModelScale(tEob s) override { Put("sub_mesh_model_scale " + std::to_string(s)); return cVector3f(1.5f, 1.0f, 0.5f); }
	void SetWorldMatrix(tEob e, const cMatrixf &m) override { Put("set_world_matrix " + std::to_string(e) + " " + M(m)); }
	void SetUniqueID(tEob e, int id) override { Put("set_unique_id " + std::to_string(e) + " " + std::to_string(id)); }
	void SetSubMeshMaterial(tEob s, const tString &f) override { Put("sub_mesh_material " + std::to_string(s) + " " + f); }

	tEob AddAnimation(const tString &f, const tString &n, float sp) override { Put("add_animation " + f + " " + n + " " + F(sp)); return Maybe(n + f, 5); }
	void SetSpecialEventTime(tEob s, float t) override { Put("special_event_time " + std::to_string(s) + " " + F(t)); }
	void AddAnimationEvent(tEob s, float t, int ty, const tString &v, const tString &n, const tString &d) override
	{ Put("anim_event " + std::to_string(s) + " " + F(t) + " " + std::to_string(ty) + " " + v + " " + n + " " + d); }

	tEob LoadWorldEntity(eEobWorldEntity k, cXmlElement *e, const tString &p) override
	{ Put("world_entity " + std::to_string((int)k) + " " + e->GetValue() + " " + p); return Maybe(p, 6); }
	cVector3f GetLocalPosition(tEob e) override { Put("local_position " + std::to_string(e)); return cVector3f(1, 2, 3); }
	void SetPosition(tEob e, const cVector3f &v) override { Put("set_position " + std::to_string(e) + " " + V(v)); }
	void LinkLightBillboards() override { Put("link_light_billboards"); }

	tEob GetBoneState(const tString &n) override { Put("bone_state " + n); return Maybe(n, 3); }
	int GetBoneStateNum() override { Put("bone_state_num"); return 3; }
	void WarnSkeletonMismatch() override { Put("warn_skeleton_mismatch"); }
	cMatrixf GetBoneWorldMatrix(tEob b) override { Put("bone_world_matrix " + std::to_string(b)); return cMatrixf::Identity; }
	tString GetBoneName(tEob b) override { Put("bone_name " + std::to_string(b)); return "bone" + std::to_string(b); }

	int GetUniqueID(tEob e) override { Put("get_unique_id " + std::to_string(e)); return (int)e * 10; }
	bool IsSubMesh(tEob e) override { Put("is_sub_mesh " + std::to_string(e)); return e & 1; }
	cMatrixf GetLocalMatrix(tEob e) override { Put("local_matrix " + std::to_string(e)); return cMatrixf::Identity; }
	cMatrixf GetWorldMatrix(tEob e) override { Put("world_matrix " + std::to_string(e)); return cMatrixf::Identity; }
	tString GetEntityName(tEob e) override { Put("entity_name " + std::to_string(e)); return "ent" + std::to_string(e); }
	void SetNodeAnimation() override { Put("set_node_animation"); }
	void AttachEntityChild(tEob p, const cMatrixf &m, tEob c) override { Put("attach_entity " + std::to_string(p) + " " + M(m) + " " + std::to_string(c)); }
	void AttachBoneChild(tEob b, const cMatrixf &m, tEob c) override { Put("attach_bone " + std::to_string(b) + " " + M(m) + " " + std::to_string(c)); }
	void AttachBoneToBody(tEob b, const cMatrixf &m, tEob o) override { Put("attach_bone_body " + std::to_string(b) + " " + M(m) + " " + std::to_string(o)); }

	tEob CreateBoxShape(const cVector3f &s, const cMatrixf &m) override { Put("box " + V(s) + " " + M(m)); return New(); }
	tEob CreateSphereShape(const cVector3f &s, const cMatrixf &m) override { Put("sphere " + V(s) + " " + M(m)); return New(); }
	tEob CreateCylinderShape(float r, float h, const cMatrixf &m) override { Put("cylinder " + F(r) + " " + F(h) + " " + M(m)); return New(); }
	tEob CreateCapsuleShape(float r, float h, const cMatrixf &m) override { Put("capsule " + F(r) + " " + F(h) + " " + M(m)); return New(); }
	tEob CreateCompoundShape(const std::vector<tEob> &v) override
	{ std::string s = "compound"; for(tEob e : v) s += " " + std::to_string(e); Put(s); return New(); }
	void IncShapeUserCount(tEob s) override { Put("inc_shape " + std::to_string(s)); }
	void DestroyShape(tEob s) override { Put("destroy_shape " + std::to_string(s)); }

	tEob CreateBody(const tString &n, tEob s) override { Put("body " + n + " " + std::to_string(s)); return New(); }
	void SetBodyProperties(tEob b, const cEobBodyProps &p) override
	{
		Put("body_props " + std::to_string(b) + " " + M(p.m_mtxMatrix) + " " + F(p.mfMass) + " " + F(p.mfAngularDamping) + " " + F(p.mfLinearDamping) + " " +
			F(p.mfBuoyancyDensityMul) + " " + F(p.mfMaxAngularSpeed) + " " + F(p.mfMaxLinearSpeed) + " " +
			B(p.mbBlocksSound) + B(p.mbCollideCharacter) + B(p.mbCollide) + B(p.mbGravity) + B(p.mbContinuousCollision) + B(p.mbPushedByCharacterGravity) +
			B(p.mbVolatile) + B(p.mbUseSurfaceEffects) + B(p.mbCanAttachCharacter) + " " + std::to_string(p.mlID));
	}
	void SetBodyMaterial(tEob b, const tString &n) override { Put("body_material " + std::to_string(b) + " " + n); }
	void RegisterBody(tEob b) override { Put("register_body " + std::to_string(b)); }

	tEob Joint(const char *k, const tString &n, const cVector3f &pv, const cVector3f &pn, tEob p, tEob c)
	{ Put(std::string("joint_") + k + " " + n + " " + V(pv) + " " + V(pn) + " " + std::to_string(p) + " " + std::to_string(c)); return New(); }
	tEob CreateJointHinge(const tString &n, const cVector3f &a, const cVector3f &b, tEob p, tEob c) override { return Joint("hinge", n, a, b, p, c); }
	tEob CreateJointBall(const tString &n, const cVector3f &a, const cVector3f &b, tEob p, tEob c) override { return Joint("ball", n, a, b, p, c); }
	tEob CreateJointSlider(const tString &n, const cVector3f &a, const cVector3f &b, tEob p, tEob c) override { return Joint("slider", n, a, b, p, c); }
	tEob CreateJointScrew(const tString &n, const cVector3f &a, const cVector3f &b, tEob p, tEob c) override { return Joint("screw", n, a, b, p, c); }
	void SetHingeLimits(tEob j, float a, float b) override { Put("hinge_limits " + std::to_string(j) + " " + F(a) + " " + F(b)); }
	void SetBallLimits(tEob j, float a, float b) override { Put("ball_limits " + std::to_string(j) + " " + F(a) + " " + F(b)); }
	void SetSliderLimits(tEob j, float a, float b) override { Put("slider_limits " + std::to_string(j) + " " + F(a) + " " + F(b)); }
	void SetScrewLimits(tEob j, float a, float b) override { Put("screw_limits " + std::to_string(j) + " " + F(a) + " " + F(b)); }
	void SetJointProperties(tEob j, const cEobJointProps &p) override
	{
		Put("joint_props " + std::to_string(j) + " " + p.msMoveSound + "|" + p.msBreakSound + "|" + p.msMaxLimitSound + "|" + p.msMinLimitSound + " " +
			F(p.mfMinMoveSpeed) + " " + F(p.mfMinMoveFreq) + " " + F(p.mfMinMoveVolume) + " " + F(p.mfMinMoveFreqSpeed) + " " + F(p.mfMaxMoveFreq) + " " +
			F(p.mfMaxMoveVolume) + " " + F(p.mfMaxMoveFreqSpeed) + " " + F(p.mfMiddleMoveSpeed) + " " + F(p.mfMiddleMoveVolume) + " " + F(p.mfBreakForce) + " " +
			F(p.mfLimitAutoSleepDist) + " " + F(p.mfMaxLimitMaxSpeed) + " " + F(p.mfMaxLimitMinSpeed) + " " + F(p.mfMinLimitMaxSpeed) + " " + F(p.mfMinLimitMinSpeed) + " " +
			B(p.mbAngularMove) + B(p.mbStickyMinLimit) + B(p.mbStickyMaxLimit) + B(p.mbBreakable) + B(p.mbLimitAutoSleep) + B(p.mbCollideBodies) + " " +
			std::to_string(p.mlLimitAutoSleepNumSteps) + " " + std::to_string(p.mlID));
	}
	void RegisterJoint(tEob j) override { Put("register_joint " + std::to_string(j)); }

	int GetAnimationStateNum() override { Put("animation_state_num"); return 2; }
	void PlayFirstAnimation() override { Put("play_first_animation"); }
	void SetSkeletonPhysicsActive() override { Put("skeleton_physics_active"); }
	void LoadUserVariables() override { Put("load_user_variables"); }

private:
	bool mbFull;
	std::string &msLog;
	tEob mlNext;
};

int main(int argc, char **argv)
{
	if(argc < 3) return 2;
	std::string root = argv[1];
	std::ifstream list(argv[2]);
	std::string rel, full;
	while(std::getline(list, rel))
	{
		std::string path = root + "/" + rel;
		cXmlDocumentTiny doc("probe");
		bool ok = doc.CreateFromFile(cString::To16Char(path));
		cXmlElement *pRoot = ok ? static_cast<cXmlElement*>(&doc) : NULL;
		if(pRoot == NULL) { printf("%s\tnoroot\n", rel.c_str()); continue; }

		std::string base = rel.substr(rel.find_last_of('/') + 1);
		std::string dir = path.substr(0, path.find_last_of('/'));
		cEobParams p;
		p.msName = base.substr(0, base.find_last_of('.'));
		p.msFileName = base;
		p.msFullPath = path;
		p.msFullPathDir = dir;
		p.m_mtxTransform = cMatrixf::Identity;
		p.mvScale = cVector3f(1, 1, 1);
		p.mbPhysicsWorld = true;

		std::string line = rel;
		for(int cfg = 0; cfg < 2; ++cfg)
		{
			std::string log;
			cRec rec(cfg == 0, log);
			p.mbPhysicsWorld = cfg == 0;
			bool done = ParseEntityObject(rec, p, pRoot);
			log += done ? "result 1\n" : "result 0\n";
			line += "\t" + std::to_string(rec.mlCalls) + "\t" + Hex(Fnv(NoNanBits(log))) + "\t" + Hex(Fnv(log));
			if(argc > 3) full += rel + "#" + std::to_string(cfg) + "\n" + log;
		}
		printf("%s\n", line.c_str());
	}
	if(argc > 3) { std::ofstream(argv[3]) << full; }
	return 0;
}
