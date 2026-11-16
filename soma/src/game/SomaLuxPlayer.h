#ifndef SOMA_LUX_PLAYER_H
#define SOMA_LUX_PLAYER_H

#include "RustSomaLuxS5.h"
#include "RustSomaLuxS5d.h"
#include "SomaLuxGame.h"
#include "SomaS5Dump.h"

#include <cstring>

class cSomaLuxPlayer;
class cSomaLuxEntity;
struct cSomaID;

class cSomaLuxPlayerState : public cSomaLuxScriptable
{
public:
	tString msName;
	OhplS5dStateBlock mB = {-1, NULL};
};

class cSomaLuxMoveState : public cSomaLuxScriptable
{
public:
	tString msName;
	OhplS5dMoveBlock mB = {-1, NULL};
};

// Rust owns the player (rust/src/somalux/s5d_player.rs); this is the shell the scripts and the engine hold
class cSomaLuxPlayer : public cSomaLuxHandler
{
public:
	cSomaLuxPlayer();
	~cSomaLuxPlayer();

	static cSomaLuxPlayer *Get() { return mpInstance; }

	void SetCamera(cCamera *apCamera) { ohpl_s5d_set_camera(apCamera); }
	cCamera *GetCamera() { return (cCamera *)ohpl_s5d_camera(); }
	iCharacterBody *GetCharacterBody() { return (iCharacterBody *)ohpl_s5d_body(); }
	void SetCharacterBody(iCharacterBody *apBody) { ohpl_s5d_set_body(apBody); }

	bool IsActive() { return ohpl_s5d_active(); }
	void SetActive(bool abX) { ohpl_s5d_set_active(abX); }

	void AddState(const tString &asName, int alId, const tString &asFile, const tString &asClass)
	{
		ohpl_s5d_add_state((const unsigned char *)asName.data(), asName.size(), alId, (const unsigned char *)asFile.data(), asFile.size(),
						   (const unsigned char *)asClass.data(), asClass.size(), false);
	}
	void AddMoveState(const tString &asName, int alId, const tString &asFile, const tString &asClass)
	{
		ohpl_s5d_add_state((const unsigned char *)asName.data(), asName.size(), alId, (const unsigned char *)asFile.data(), asFile.size(),
						   (const unsigned char *)asClass.data(), asClass.size(), true);
	}
	void ChangeState(int alId) { ohpl_s5d_change_state(alId, false); }
	void ChangeMoveState(int alId) { ohpl_s5d_change_state(alId, true); }
	cSomaLuxPlayerState *GetState() { return static_cast<cSomaLuxPlayerState *>((cSomaLuxScriptable *)ohpl_s5d_state(false)); }
	cSomaLuxMoveState *GetMoveState() { return static_cast<cSomaLuxMoveState *>((cSomaLuxScriptable *)ohpl_s5d_state(true)); }
	int StateId() { return ohpl_s5d_state_id(false); }
	int MoveStateId() { return ohpl_s5d_state_id(true); }

	// cLuxMapHandler::SetCurrentMap: after CreateWorldEntities/OnMapEnter
	void PlaceAtStart(const cVector3f &avFeetPos, float afYaw, bool abCrouching = false)
	{
		ohpl_s5d_place_at_start(&avFeetPos.x, afYaw, abCrouching);
	}

	void OnMessage(const char *apDecl) override { ohpl_s5d_message((const unsigned char *)apDecl, strlen(apDecl)); }
	void OnUpdate(float afTimeStep) override { ohpl_s5d_update(afTimeStep); }
	void OnPostUpdate(float afTimeStep) override { ohpl_s5d_post_update(afTimeStep); }
	void OnVariableUpdate(float afTimeStep) override { ohpl_s5d_variable_update(afTimeStep); }
	void OnGui(float afTimeStep) override { ohpl_s5d_gui(afTimeStep); }
	void OnMapMessage(const char *apDecl, void *apMap) override { ohpl_s5d_map_message((const unsigned char *)apDecl, strlen(apDecl), apMap); }
	void OnAction(int alAction, bool abPressed) override { ohpl_s5d_action(alAction, abPressed); }
	void OnAnalogInput(int alAnalogId, const cVector3f &avAmount) override { ohpl_s5d_analog(alAnalogId, &avAmount.x); }

	float Health() { return ohpl_s5d_health(); }
	void SetHealthRaw(float afX) { ohpl_s5d_set_health_raw(afX); }
	void SetHealth(float afX) { ohpl_s5d_set_health(afX); }
	bool IsDead() { return ohpl_s5d_is_dead(); }
	bool GiveDamage(float afAmount, int alType, float afMinHealth, const cSomaID &aSource, int alDamageID)
	{
		return ohpl_s5d_give_damage(afAmount, alType, afMinHealth, &aSource, alDamageID);
	}
	float GetVisibilityRangeMul() { return ohpl_s5d_visibility_range_mul(); }
	float GetVisibilityMaxRange() { return ohpl_s5d_visibility_max_range(); }
	void GetFOVMul(OhplS5dFade *apOut) { ohpl_s5d_fov_mul(apOut, false); }
	void SetFOVMul(const OhplS5dFade *apIn) { ohpl_s5d_set_fov_mul(apIn, false); }
	void GetAspectMul(OhplS5dFade *apOut) { ohpl_s5d_fov_mul(apOut, true); }
	void SetAspectMul(const OhplS5dFade *apIn) { ohpl_s5d_set_fov_mul(apIn, true); }

	static void RegisterNatives(asIScriptEngine *apEngine);
	static int32_t HostCall(void *, uint32_t alOp, OhplS5Io *apIo);

private:
	struct cCharBodyCallback : public iCharacterBodyCallback
	{
		void OnGravityCollide(iCharacterBody *, iPhysicsBody *, cCollideData *) override {}
		void OnHitGround(iCharacterBody *apBody, const cVector3f &avVel) override { ohpl_s5d_hit_ground(apBody, &avVel.x); }
	} mCharBodyCallback;

	static cSomaLuxPlayer *mpInstance;
};

class cSomaLuxInputHandler : public cSomaLuxHandler
{
public:
	cSomaLuxInputHandler();

	static cSomaLuxInputHandler *Get() { return mpInstance; }

	void LoadKeyConfig() { ohpl_s5_in_load_key_config(); }
	void LoadScript()
	{
		ohpl_s5_in_load_script();
		if (OhplS5DumpEnabled())
			ohpl_s5_in_dump();
	}
	void LoadUserConfig() { ohpl_s5_in_load_user_config(); }
	void UpdateInput(float afTimeStep, bool abGameInput) { ohpl_s5_in_update(abGameInput); }
	void LatchActions() { ohpl_s5_in_latch(); }
	void ClearGuiInput() { ohpl_s5_in_clear_gui(); }

	static void RegisterNatives(asIScriptEngine *apEngine) { ohpl_s5_register_input_natives(apEngine); }

private:
	static cSomaLuxInputHandler *mpInstance;
};

#endif // SOMA_LUX_PLAYER_H
